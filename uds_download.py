"""
S32K144 Bootloader UDS 下载工具
读取 app.bin，生成完整的 UDS CAN 报文序列
CAN ID: 0x7E0 (请求), 0x123 (响应)
"""

import struct
import zlib
import sys
import os

# ==================== 配置 ====================
BIN_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "app.bin")
CAN_REQUEST_ID = 0x7E0
CAN_RESPONSE_ID = 0x123
APP_START_ADDR = 0x00010000
BLOCK_SIZE = 4  # 每块数据字节数（与固件实现一致）

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

    # 2. 请求下载
    frames.append(("RequestDownload", make_request_download(APP_START_ADDR, bin_size)))

    # 3. 传输数据
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

    # 4. 退出传输
    frames.append(("RequestTransferExit", make_uds_frame(0x37)))

    # 5. CRC32 校验
    frames.append(("WriteDID_CRC32", make_write_did_crc(crc32)))

    # 6. ECU 复位
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
