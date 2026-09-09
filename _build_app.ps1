# Build App images (equivalent to Debug_APP config), outputs app_a.bin / app_b.bin
$ErrorActionPreference = "Stop"

$PROJ  = "C:/Users/10608/Documents/NXP_S32_3.4/demo_s32k144"
$TOOL  = "E:/NXP/S32DS.3.4/S32DS/build_tools/gcc_b1620/gcc-6.3-arm32-eabi/bin"
$SDKX  = "E:/NXP/S32DS.3.4/S32DS/software/S32SDK_S32K1XX_RTM_4.0.2"
$SYSR  = "E:/NXP/S32DS.3.4/S32DS/build_tools/gcc_b1620/gcc-6.3-arm32-eabi/arm-none-eabi/newlib"
$OUT   = "$PROJ/_build_app"

New-Item -ItemType Directory -Force -Path $OUT | Out-Null

$gcc   = "$TOOL/arm-none-eabi-gcc.exe"
$objcopy = "$TOOL/arm-none-eabi-objcopy.exe"

$INC = @(
  "-I$PROJ/SDK/rtos/osif",
  "-I$PROJ/SDK/platform/drivers/inc",
  "-I$PROJ/SDK/platform/drivers/src/lpuart",
  "-I$PROJ/src",
  "-I$PROJ/board",
  "-I$SDKX/platform/devices/S32K144/include",
  "-I$SDKX/platform/devices/S32K144/startup",
  "-I$SDKX/platform/devices/S32K144",
  "-I$SDKX/platform/devices/common",
  "-I$SDKX/platform/devices",
  "-I$SDKX/platform/drivers/inc",
  "-I$SDKX/platform/drivers/src/clock/S32K1xx"
)

$CFLAGS = @(
  "-std=c99",
  "-DCPU_S32K144HFT0VLLT",
  "-DDEV_ERROR_DETECT",
  "-DBUILD_APP",
  "-O1", "-g3", "-Wall", "-c", "-fmessage-length=0",
  "-ffunction-sections", "-fdata-sections",
  "-mcpu=cortex-m4", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16",
  "-specs=nano.specs", "-specs=nosys.specs",
  "--sysroot=$SYSR"
) + $INC

$ASFLAGS = @(
  "-DCPU_S32K144HFT0VLLT",
  "-DSTART_FROM_FLASH",
  "-x", "assembler-with-cpp",
  "-c",
  "-mcpu=cortex-m4", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16",
  "--sysroot=$SYSR"
) + $INC

$CSRC = @(
  "SDK/platform/devices/S32K144/startup/system_S32K144.c",
  "SDK/platform/devices/startup.c",
  "SDK/platform/drivers/src/clock/S32K1xx/clock_S32K1xx.c",
  "SDK/platform/drivers/src/edma/edma_driver.c",
  "SDK/platform/drivers/src/edma/edma_hw_access.c",
  "SDK/platform/drivers/src/edma/edma_irq.c",
  "SDK/platform/drivers/src/flash/flash_driver.c",
  "SDK/platform/drivers/src/flexcan/flexcan_driver.c",
  "SDK/platform/drivers/src/flexcan/flexcan_hw_access.c",
  "SDK/platform/drivers/src/flexcan/flexcan_irq.c",
  "SDK/platform/drivers/src/interrupt/interrupt_manager.c",
  "SDK/platform/drivers/src/lpuart/lpuart_driver.c",
  "SDK/platform/drivers/src/lpuart/lpuart_hw_access.c",
  "SDK/platform/drivers/src/lpuart/lpuart_irq.c",
  "SDK/platform/drivers/src/pins/pins_driver.c",
  "SDK/platform/drivers/src/pins/pins_port_hw_access.c",
  "SDK/rtos/osif/osif_baremetal.c",
  "board/clock_config.c",
  "board/peripherals_flash_1.c",
  "board/peripherals_flexcan_config_1.c",
  "board/peripherals_lpuart_1.c",
  "board/peripherals_osif_1.c",
  "board/pin_mux.c",
  "src/app.c",
  "src/can.c",
  "src/flash_app.c",
  "src/led.c",
  "src/main.c",
  "src/partition.c",
  "src/uart.c",
  "src/uds.c"
)

$objs = @()
foreach ($s in $CSRC) {
  $name = [IO.Path]::GetFileNameWithoutExtension($s)
  $obj  = "$OUT/$name.o"
  Write-Host "CC  $s"
  & $gcc @CFLAGS "-o" $obj "$PROJ/$s"
  if ($LASTEXITCODE -ne 0) { Write-Error "compile failed: $s"; exit 1 }
  $objs += $obj
}

# 启动汇编
$objStartup = "$OUT/startup_S32K144.o"
Write-Host "AS  startup_S32K144.S"
& $gcc @ASFLAGS "-o" $objStartup "$PROJ/Project_Settings/Startup_Code/startup_S32K144.S"
if ($LASTEXITCODE -ne 0) { Write-Error "assemble failed"; exit 1 }
$objs += $objStartup

# Dual-image link: same source, different link base (A/B dual-slot)
$variants = @(
  @{ name = "app_a"; ld = "S32K144_64_app_a.ld"; bin = "app_a.bin" },
  @{ name = "app_b"; ld = "S32K144_64_app_b.ld"; bin = "app_b.bin" }
)

foreach ($v in $variants) {
  $elf = "$OUT/demo_s32k144_$($v.name).elf"
  $linkArgs = @(
    "-T", "$PROJ/Project_Settings/Linker_Files/$($v.ld)",
    "-Wl,-Map,$OUT/demo_s32k144_$($v.name).map",
    "-Xlinker", "--gc-sections",
    "-n",
    "-mcpu=cortex-m4", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16",
    "-specs=nano.specs", "-specs=nosys.specs",
    "--sysroot=$SYSR"
  ) + $objs
  Write-Host "LD  $($v.name) -> $($v.bin)"
  & $gcc @linkArgs "-o" $elf
  if ($LASTEXITCODE -ne 0) { Write-Error "link failed: $($v.name)"; exit 1 }

  $bin = "$PROJ/$($v.bin)"
  Write-Host "OBJCOPY -> $($v.bin)"
  & $objcopy -O binary $elf $bin
  if ($LASTEXITCODE -ne 0) { Write-Error "objcopy failed: $($v.name)"; exit 1 }
}

Write-Host "OK: app_a.bin / app_b.bin generated"
