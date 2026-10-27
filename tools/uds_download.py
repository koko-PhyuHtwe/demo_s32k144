"""
S32K144 Bootloader UDS 下载工具（A/B 双区架构）
读取 app_a.bin 作为示例，生成完整的 UDS CAN 报文序列
CAN ID: 0x7E0 (请求), 0x7E8 (响应)   ← ISO 14229-2 物理寻址约定

A/B 双区说明：
  - Bootloader 会忽略 0x34 请求里的地址，自动写入非激活槽
  - 新版固件永远写到非激活槽，CRC 校验通过后才原子切换激活槽
  - 因此升级中途断电不会破坏正在运行的固件（可回滚）
  - 镜像按链接地址区分：app_a.bin(0x10000) / app_b.bin(0x48000)，
    由 CANoe 脚本通过 0x22 F19A 读激活槽后自动选镜像。
    本脚本是静态参考工具（无法实时查询槽位），默认演示 app_a.bin。

0x27 安全访问说明：
  - 固件 0x34 要求先通过 0x27 seed-key 解锁，否则返回 NRC=0x33
  - 种子由 ECU 动态下发（基于毫秒 tick），无法在生成阶段预先计算密钥
  - 因此本脚本生成的 0x27 0x02 帧为占位帧，仅展示报文结构；
    密钥必须按 key = (seed + 0x1111) & 0xFFFF 现场计算后替换
  - 需要全自动刷写请使用 CANoe CAPL 脚本 uds_download.can
    （其 on message 0x7E8 回调里已实现动态 seed-key 交换）
"""

import struct
import zlib
import sys
import os

# ==================== 配置 ====================
# 示例镜像：A/B 双区下由 CANoe 实时选镜像，本工具默认演示 Slot A 镜像
BIN_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "app_a.bin")
CAN_REQUEST_ID = 0x7E0    # UDS 请求 ID（ISO 14229-2）
CAN_RESPONSE_ID = 0x7E8   # UDS 响应 ID（ISO 14229-2），本脚本仅用于打印提示
# 下载目标地址：A/B 双区下 Bootloader 忽略此值，自动写入非激活槽，仅作占位
APP_START_ADDR = 0x00010000
BLOCK_SIZE = 4  # 每块数据字节数（与固件实现一致）

# 0x27 安全访问密钥算法（与固件 uds.c 一致）
SECURITY_KEY_OFFSET = 0x1111
# 占位种子：真实种子由 ECU 动态下发，生成阶段只能用占位值
SECURITY_SEED_PLACEHOLDER = 0x1234

# ==================== CRC32 ====================
def calc_crc32(data):
    """与固件 FlashApp_CalcCRC32 一致：CRC-32/ISO-HDLC"""
    return zlib.crc32(data) & 0xFFFFFFFF

# ==================== UDS 帧构造 ====================
def make_uds_frame(sid, sub_func=None, data_bytes=None):
    """构造单帧 UDS 请求（PCI 自动计算）"""
    payload_len = 1  # SID
    if sub_func is not None:
        payload_len += 1
    if data_bytes:
        payload_len += len(data_bytes)

    pci = payload_len & 0x0F
    frame = [pci, sid]

    if sub_func is not None:
        frame.append(sub_func)
    if data_bytes:
        frame.extend(data_bytes)

    # 填充到 8 字节
    while len(frame) < 8:
        frame.append(0x00)

    return frame[:8]

def make_request_download(addr, size):
    """0x34 请求下载：4字节地址 + 2字节大小"""
    data = [
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF,
        (size >> 8) & 0xFF,
        size & 0xFF,
    ]
    return make_uds_frame(0x34, data_bytes=data)

def make_transfer_data(block_num, data_bytes):
    """0x36 传输数据：块序号 + 数据（最多4字节）"""
    frame = make_uds_frame(0x36, data_bytes=[block_num] + list(data_bytes))
    return frame

def make_write_did_crc(crc32_val):
    """0x2E 写 DID 0xFF01：CRC32 校验"""
    data = [
        0xFF, 0x01,  # DID
        (crc32_val >> 24) & 0xFF,
        (crc32_val >> 16) & 0xFF,
        (crc32_val >> 8) & 0xFF,
        crc32_val & 0xFF,
    ]
    return make_uds_frame(0x2E, data_bytes=data)

# ==================== 主流程 ====================
def main():
    if not os.path.exists(BIN_FILE):
        print(f"ERROR: {BIN_FILE} not found!")
        sys.exit(1)

    # 读取 bin 文件
    with open(BIN_FILE, "rb") as f:
        bin_data = f.read()

    bin_size = len(bin_data)
    crc32 = calc_crc32(bin_data)

    # 0x34 的 size 字段只有 2 字节（最大 65535），CANoe 侧也只读 64KB，超限直接报错
    if bin_size > 0xFFFF:
        print(f"ERROR: {os.path.basename(BIN_FILE)} too large: {bin_size} bytes (max 65535)!")
        sys.exit(1)

    print(f"File: {BIN_FILE}")
    print(f"Size: {bin_size} bytes (0x{bin_size:X})")
    print(f"CRC32: 0x{crc32:08X}")
    print(f"Blocks: {bin_size // BLOCK_SIZE} (4 bytes each)")
    print()

    # 计算需要的 TransferData 块数
    num_blocks = (bin_size + BLOCK_SIZE - 1) // BLOCK_SIZE
    last_block_bytes = bin_size % BLOCK_SIZE
    if last_block_bytes == 0:
        last_block_bytes = BLOCK_SIZE

    # ==================== 生成所有报文 ====================
    frames = []

    # 1. 启动扩展会话
    frames.append(("StartSession", make_uds_frame(0x10, sub_func=0x03)))

    # 2. 0x27 安全访问：请求种子（帧结构固定，种子由 ECU 动态下发）
    frames.append(("SecurityAccess_ReqSeed", make_uds_frame(0x27, sub_func=0x01)))

    # 3. 0x27 安全访问：发送密钥（占位帧！须用 ECU 实际返回的种子重算）
    placeholder_key = (SECURITY_SEED_PLACEHOLDER + SECURITY_KEY_OFFSET) & 0xFFFF
    frames.append(("SecurityAccess_SendKey[PLACEHOLDER]",
                   make_uds_frame(0x27, sub_func=0x02,
                                  data_bytes=[(placeholder_key >> 8) & 0xFF,
                                              placeholder_key & 0xFF])))

    # 4. 请求下载
    frames.append(("RequestDownload", make_request_download(APP_START_ADDR, bin_size)))

    # 5. 传输数据
    for i in range(num_blocks):
        offset = i * BLOCK_SIZE
        if i == num_blocks - 1:
            # 最后一块
            block_data = bin_data[offset:offset + last_block_bytes]
            block_num = (i + 1) & 0xFF  # 块序号从1开始，0xFF后绕回1
            # 如果最后一块不足 BLOCK_SIZE，需要调整 PCI
            actual_len = len(block_data)
            frame = [actual_len + 2, 0x36, block_num]  # PCI + SID + BlockNum
            frame.extend(block_data)
            while len(frame) < 8:
                frame.append(0x00)
            frames.append((f"TransferData[{block_num}]", frame[:8]))
        else:
            block_data = bin_data[offset:offset + BLOCK_SIZE]
            block_num = (i + 1) & 0xFF
            frames.append((f"TransferData[{block_num}]", make_transfer_data(block_num, block_data)))

    # 6. 退出传输
    frames.append(("RequestTransferExit", make_uds_frame(0x37)))

    # 7. CRC32 校验
    frames.append(("WriteDID_CRC32", make_write_did_crc(crc32)))

    # 8. ECU 复位
    frames.append(("ECUReset", make_uds_frame(0x11, sub_func=0x01)))

    # ==================== 打印所有报文 ====================
    print("=" * 70)
    print(f"{'Step':<25s} {'CAN ID':>8s}  {'DLC':>3s}  Data")
    print("=" * 70)

    for name, frame in frames:
        data_str = " ".join(f"{b:02X}" for b in frame)
        print(f"{name:<25s} 0x{CAN_REQUEST_ID:03X}     8   {data_str}")

    print("=" * 70)
    print(f"\nTotal frames: {len(frames)}")
    print(f"CRC32 (for 0x2E WriteDID): 0x{crc32:08X}")

    print("\n" + "!" * 70)
    print("! WARNING: SecurityAccess_SendKey 为占位帧（seed 由 ECU 动态下发）。")
    print(f"! 实际使用时请用 ECU 返回的 seed 计算 key = (seed + 0x{SECURITY_KEY_OFFSET:X}) & 0xFFFF 替换。")
    print("! 全自动刷写请用 CANoe 脚本 uds_download.can（已实现动态 seed-key 交换）。")
    print("!" * 70)

    # ==================== 导出为 CSV 格式（方便 CANoe 导入） ====================
    csv_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "uds_frames.csv")
    with open(csv_file, "w") as f:
        f.write("Message Name,Channel,ID,Length,Data Bytes\n")
        for name, frame in frames:
            data_str = " ".join(f"{b:02X}" for b in frame)
            f.write(f"{name},CAN,0x{CAN_REQUEST_ID:03X},8,{data_str}\n")
    print(f"\nCSV 导出: {csv_file}")

if __name__ == "__main__":
    main()
