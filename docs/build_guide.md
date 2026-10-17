# Руководство по сборке

## Зависимости

- `arm-none-eabi-gcc` 12.2.1 (проверено), GNU Make (генератор CMake)
- CMake ≥ 3.20
- SDK **STM32CubeF1** в соседней директории `../STM32CubeF1`
  (или путь через `-DSTM32CUBE_F1_PATH=...` / `$STM32CUBE_F1_PATH`)
- `openocd` (прошивка через ST-Link), `stm32flash` (UART), `stm8flash`
  (ST-Link для STM8) — по необходимости
- Нативный GCC — для `tools/` и `tests/`

CMake использует только заголовки SDK (CMSIS Core/Device + HAL/LL Inc);
исходники HAL не компилируются.

## Сборка прошивок

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build
```

Выбор MCU (по умолчанию C8T6):

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=CBT6
cmake --build build
```

Результат:

```
build/bootloader.elf|hex|bin
build/app.elf|hex|bin
build/bootloader.map, build/app.map
```

Версия прошивки берётся из `git describe --tags` (иначе 1.0.0 →
`0x00010000`) и генерируется в `build/generated/version.h`.

## Прошивка

```bash
# Загрузчик (ST-Link v2)
cmake --build build --target flash-boot

# Приложение напрямую (ST-Link v2)
cmake --build build --target flash-app

# Приложение через I2C-загрузчик (нужен bootloader_tool)
cmake --build build --target flash-app-i2c -DBOOTLOADER_TOOL=/путь/к/bootloader_tool
```

Полное стирание Flash: `cmake --build build --target erase-boot`.

Пути по умолчанию (переопределяются кэш-переменными):

- `OPENOCD_EXECUTABLE` = `openocd`
- `OPENOCD_SCRIPTS` = `/usr/share/openocd/scripts`
- `I2C_DEVICE` = `/dev/i2c-7`
- `BOOTLOADER_TOOL` = `bootloader_tool`

## Хостовые утилиты

```bash
cmake -B build_host
cmake --build build_host
#→ build_host/tools/bootloader_tool/bootloader_tool
```

`tools/` пропускается при кросс-сборке (`if(CMAKE_CROSSCOMPILING)`).

## Утилита прошивки

```bash
bootloader_tool ec /dev/i2c-7 info
bootloader_tool ec /dev/i2c-7 status
bootloader_tool ec /dev/i2c-7 flash build/app.bin 0x00010000
bootloader_tool ec /dev/i2c-7 enter-update
bootloader_tool ec /dev/i2c-7 reset
bootloader_tool motor /dev/i2c-7 flash motor.ihx 0x00010000
bootloader_tool motor stlink motor.ihx --verify
```

## Тесты

```bash
cmake -B build_host
cmake --build build_host --target check          # собрать и запустить тесты

# либо после обычной сборки:
cmake --build build_host
ctest --test-dir build_host --output-on-failure
# встроенная цель test только запускает ctest (без сборки):
cmake --build build_host --target test
```

## Отладка

```bash
openocd -f scripts/openocd.cfg          # терминал 1
arm-none-eabi-gdb build/app.elf -x scripts/gdb_debug.gdb  # терминал 2
```

## Прошивка через UART (системный загрузчик)

```bash
scripts/flash_stm32.sh <ПОРТ> <СКОРОСТЬ> <ФАЙЛ>
scripts/flash_stm32.sh /dev/ttyCH343USB0 115200 build/app.bin
```