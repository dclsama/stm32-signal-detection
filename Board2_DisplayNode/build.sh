#!/bin/bash
# Build script for Board2_DisplayNode
set -e

PROJECT_DIR="d:/Desktop/电气组实习/任务五：STM32信号检测系统设计/stm32-signal-detection/Board2_DisplayNode"
cd "$PROJECT_DIR"

GCC="D:/10 2021.10/bin/arm-none-eabi-gcc"
OBJCOPY="D:/10 2021.10/bin/arm-none-eabi-objcopy"
SIZE="D:/10 2021.10/bin/arm-none-eabi-size"

BUILD_DIR="build"
TARGET="Board2_DisplayNode"

CFLAGS="-mcpu=cortex-m3 -mthumb"
CFLAGS="$CFLAGS -DUSE_HAL_DRIVER -DSTM32F103xB"
CFLAGS="$CFLAGS -ICore/Inc"
CFLAGS="$CFLAGS -IDrivers/STM32F1xx_HAL_Driver/Inc"
CFLAGS="$CFLAGS -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy"
CFLAGS="$CFLAGS -IMiddlewares/Third_Party/FreeRTOS/Source/include"
CFLAGS="$CFLAGS -IMiddlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
CFLAGS="$CFLAGS -IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3"
CFLAGS="$CFLAGS -IDrivers/CMSIS/Device/ST/STM32F1xx/Include"
CFLAGS="$CFLAGS -IDrivers/CMSIS/Include"
CFLAGS="$CFLAGS -Og -Wall -fdata-sections -ffunction-sections -g -gdwarf-2"

LDFLAGS="-mcpu=cortex-m3 -mthumb -specs=nano.specs -TSTM32F103XX_FLASH.ld -lc -lm -lnosys -Wl,-Map=$BUILD_DIR/$TARGET.map,--cref -Wl,--gc-sections"

echo "============================================"
echo "  Building $TARGET"
echo "============================================"

mkdir -p "$BUILD_DIR"

C_SOURCES=(
"Core/Src/main.c"
"Core/Src/oled.c"
"Core/Src/can_receiver.c"
"Core/Src/can_protocol.c"
"Core/Src/system_status.c"
"Core/Src/freertos.c"
"Core/Src/stm32f1xx_it.c"
"Core/Src/stm32f1xx_hal_msp.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_i2c.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c"
"Core/Src/system_stm32f1xx.c"
"Middlewares/Third_Party/FreeRTOS/Source/croutine.c"
"Middlewares/Third_Party/FreeRTOS/Source/event_groups.c"
"Middlewares/Third_Party/FreeRTOS/Source/list.c"
"Middlewares/Third_Party/FreeRTOS/Source/queue.c"
"Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c"
"Middlewares/Third_Party/FreeRTOS/Source/tasks.c"
"Middlewares/Third_Party/FreeRTOS/Source/timers.c"
"Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c"
"Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c"
"Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c"
"Core/Src/sysmem.c"
"Core/Src/syscalls.c"
)

OBJECTS=()

echo ""
echo "[1/3] Compiling C sources..."
for src in "${C_SOURCES[@]}"; do
    obj="$BUILD_DIR/$(basename "${src%.c}.o")"
    OBJECTS+=("$obj")
    echo "  CC  $src"
    "$GCC" -c $CFLAGS "$src" -o "$obj" 2>&1
done

echo ""
echo "[2/3] Assembling startup..."
ASM_OBJ="$BUILD_DIR/startup_stm32f103xb.o"
OBJECTS+=("$ASM_OBJ")
echo "  AS  startup_stm32f103xb.s"
"$GCC" -x assembler-with-cpp -c $CFLAGS "startup_stm32f103xb.s" -o "$ASM_OBJ" 2>&1

echo ""
echo "[3/3] Linking..."
echo "  LD  $TARGET.elf"
"$GCC" "${OBJECTS[@]}" $LDFLAGS -o "$BUILD_DIR/$TARGET.elf" 2>&1

echo ""
"$SIZE" "$BUILD_DIR/$TARGET.elf"

echo ""
echo "Creating hex..."
"$OBJCOPY" -O ihex "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.hex"
echo "  HEX $BUILD_DIR/$TARGET.hex"
"$OBJCOPY" -O binary -S "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.bin"
echo "  BIN $BUILD_DIR/$TARGET.bin"

echo ""
echo "============================================"
echo "  BUILD SUCCESS"
echo "  hex:  $BUILD_DIR/$TARGET.hex"
echo "============================================"
