#!/bin/bash
# Build script for Board1_SensorNode
# Auto-generated from Makefile - compiles all sources and links

set -e

PROJECT_DIR="d:/Desktop/电气组实习/任务五：STM32信号检测系统设计/stm32-signal-detection/Board1_SensorNode"
cd "$PROJECT_DIR"

GCC="D:/10 2021.10/bin/arm-none-eabi-gcc"
OBJCOPY="D:/10 2021.10/bin/arm-none-eabi-objcopy"
SIZE="D:/10 2021.10/bin/arm-none-eabi-size"

BUILD_DIR="build"
TARGET="Board1_SensorNode"

# Common flags
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

# Create build directory
mkdir -p "$BUILD_DIR"

# C sources (from Makefile)
C_SOURCES=(
"Core/Src/main.c"
"Core/Src/key.c"
"Core/Src/threshold.c"
"Core/Src/can_command.c"
"Core/Src/rgb_modes.c"
"Core/Src/dht11.c"
"Core/Src/rgb_led.c"
"Core/Src/adc_dma.c"
"Core/Src/w25q64.c"
"Core/Src/can_protocol.c"
"Core/Src/system_status.c"
"Core/Src/freertos.c"
"Core/Src/stm32f1xx_it.c"
"Core/Src/stm32f1xx_hal_msp.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c"
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
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_can.c"
"Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_spi.c"
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

# Compile all C sources
echo ""
echo "[1/3] Compiling C sources..."
for src in "${C_SOURCES[@]}"; do
    obj="$BUILD_DIR/$(basename "${src%.c}.o")"
    OBJECTS+=("$obj")
    echo "  CC  $src"
    "$GCC" -c $CFLAGS "$src" -o "$obj" 2>&1
done

# Assemble startup file
echo ""
echo "[2/3] Assembling startup..."
ASM_SRC="startup_stm32f103xb.s"
ASM_OBJ="$BUILD_DIR/startup_stm32f103xb.o"
OBJECTS+=("$ASM_OBJ")
echo "  AS  $ASM_SRC"
"$GCC" -x assembler-with-cpp -c $CFLAGS "$ASM_SRC" -o "$ASM_OBJ" 2>&1

# Link
echo ""
echo "[3/3] Linking..."
echo "  LD  $TARGET.elf"
"$GCC" "${OBJECTS[@]}" $LDFLAGS -o "$BUILD_DIR/$TARGET.elf" 2>&1

echo ""
"$SIZE" "$BUILD_DIR/$TARGET.elf"

# Create hex
echo ""
echo "Creating hex..."
"$OBJCOPY" -O ihex "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.hex"
echo "  HEX $BUILD_DIR/$TARGET.hex"

# Create bin
"$OBJCOPY" -O binary -S "$BUILD_DIR/$TARGET.elf" "$BUILD_DIR/$TARGET.bin"
echo "  BIN $BUILD_DIR/$TARGET.bin"

echo ""
echo "============================================"
echo "  BUILD SUCCESS"
echo "  hex:  $BUILD_DIR/$TARGET.hex"
echo "  bin:  $BUILD_DIR/$TARGET.bin"
echo "  elf:  $BUILD_DIR/$TARGET.elf"
echo "============================================"
