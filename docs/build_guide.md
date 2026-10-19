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
export BOOTLOADER_TOOL=/путь/к/bootloader_tool
export I2C_DEVICE=/dev/i2c-7
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build --target flash-app-i2c
```

Полное стирание Flash: `cmake --build build --target erase-boot`.

Пути по умолчанию переопределяются на этапе **конфигурации**: либо
через переменные окружения `BOOTLOADER_TOOL` / `I2C_DEVICE`, либо через
`-D` при вызове `cmake` (при повторной конфигурации `-D` имеет
приоритет над закэшированным значением; env читается только при
первой конфигурации чистого каталога):

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

## Линтинг и статический анализ

Цели добавляются только в нативной сборке и только если инструмент найден
в `PATH` (`clang-format`, `cppcheck`); иначе цель молча не создаётся.

```bash
cmake -B build_host

cmake --build build_host --target format        # clang-format -i (стиль .clang-format)
cmake --build build_host --target format-check  # clang-format --dry-run --Werror
cmake --build build_host --target cppcheck      # cppcheck --error-exitcode=1
```

Единый стиль задаётся файлом `.clang-format` в корне (LLVM + Allman,
отступ 4, лимит 100). Версия clang-format закреплена на **18.x** во всех
трёх путях проверки (pre-commit `v18.1.8`, CI `clang-format-18`, локальная
цель CMake) — разные мажорные версии форматируют по-разному.
`cppcheck` запускается двумя проходами (загрузчик `-DBOOT_BUILD` и
приложение), чтобы проверить обе ветки `hal/`. Точечные исключения
оформляются inline-комментарием `/* cppcheck-suppress <id> */`.

## Pre-commit

```bash
pip install pre-commit
pre-commit install          # один раз
pre-commit run --all-files  # ручной прогон по всем файлам
```

Хуки: clang-format (`--dry-run --Werror`), удаление висячих пробелов,
проверка конца файла, валидация YAML, лимит размера файлов и запрет
прямого коммита в `main` (`SKIP=no-commit-to-branch` для обхода).

## CI/CD (GitHub Actions)

| Workflow | Триггер | Что делает |
|----------|---------|------------|
| `.github/workflows/ci.yml` | push/PR в `main` | format-check, cppcheck, сборка C8T6+CBT6, host-тесты |
| `.github/workflows/release.yml` | тег `v*.*.*` | сборка обоих вариантов + `bootloader_tool` → GitHub Release |
| `.github/workflows/flash.yml` | `workflow_dispatch` | сборка, прошивка boot (ST-Link) и app (I2C OTA, retry 3×), fallback через ST-Link |

SDK `STM32CubeF1` в CI кэшируется и клонируется в
`${{ github.workspace }}/../STM32CubeF1` (эквивалент `../STM32CubeF1`);
настройка SDK и тулчейна вынесена в composite action
`.github/actions/setup-sdk` (единый источник версии SDK и ключа кэша).
Релизные артефакты именуются с вариантом MCU (`bootloader_C8T6.elf`,
`app_CBT6.bin`, …), чтобы варианты не перезаписывали друг друга.

Локальный релиз:

```bash
./scripts/tag_release.sh 1.0.1 "Краткое описание"   # 5 проверок → тег → релиз
./scripts/push_changes.sh "область: описание"       # проверки → коммит → пуш
```

`flash.yml` выполняется на self-hosted runner (метки `self-hosted`,
`stm32`), которому нужны `arm-none-eabi-gcc`, `openocd`, `cmake`,
`git`, ST-Link v2, доступ к I2C-шине (`dialout`/`plugdev`) и SDK
`STM32CubeF1` в `_work/stm32_acpi-ec/STM32CubeF1`.

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
