# Руководство по сборке

## Зависимости

- `arm-none-eabi-gcc` (проверено на 12.2.1 и **14.2.1 Debian 13**),
  GNU Make (генератор CMake); для `--specs=nano.specs` нужен
  `libnewlib-arm-none-eabi` и `libnewlib-dev`:
  `sudo apt install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi libnewlib-dev`
- CMake ≥ 3.20
- SDK **STM32CubeF1** в соседней директории `../STM32CubeF1`
  (или `-DSTM32CUBE_F1_PATH=...` / `$STM32CUBE_F1_PATH`) — для C8T6/CBT6
- SDK **STM32CubeC0** в `project/app/STM32CubeC0` (или `-DSTM32CUBE_C0_PATH=...` /
  `$STM32CUBE_C0_PATH`) — для C011x6/C031x6; нужны только `Drivers/` (CMSIS + LL)
- `openocd` (прошивка через ST-Link); для STM32C0 — **OpenOCD из git
  master**, в 0.12 из Debian нет `tcl/target/stm32c0x.cfg`
- `stm32flash` (UART), `stm8flash` (ST-Link для STM8) — по необходимости
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

### STM32C011F6P6 / STM32C031F6P6

Обе микросхемы используют один HAL (`hal/c0`) и один набор линкер-скриптов;
различаются SRAM и target-define: C011 — 6 КиБ, C031 — 12 КиБ. На стенде
фактически стоит **C011F6P6** (`st-info --probe` → `STM32C011xx`, sram 6144).

SDK — это репозиторий с подмодулями, поэтому одного `git clone` мало:
`Drivers/CMSIS/Device/ST/STM32C0xx` и `Drivers/STM32C0xx_HAL_Driver`
останутся пустыми, а CMake не найдёт `stm32c031xx.h`. Каталог должен
находиться там, где его видит `cmake/FindSTM32CubeC0.cmake`:
`project/app/STM32CubeC0` (рядом с `stm32_acpi-ec`), либо путь задаётся
`-DSTM32CUBE_C0_PATH=...` / `$STM32CUBE_C0_PATH`. Источник истины —
`.github/actions/setup-sdk/action.yml`.

```bash
cd project/app
git clone --depth 1 --filter=blob:none --sparse \
    https://github.com/STMicroelectronics/STM32CubeC0.git STM32CubeC0
git -C STM32CubeC0 sparse-checkout set Drivers
git -C STM32CubeC0 submodule update --init --depth 1 \
    Drivers/CMSIS/Device/ST/STM32C0xx Drivers/STM32C0xx_HAL_Driver
cd stm32_acpi-ec
cmake -B build-c011 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=C011x6
cmake --build build-c011
cmake --build build-c011 --target flash-boot     # через scripts/openocd_c0.cfg
cmake --build build-c011 --target flash-app

# Планируемая C031F6 (12 КиБ SRAM):
cmake -B build-c031 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=C031x6
```

Фактические размеры (gcc 14.2.1, обе C0-цели): `bootloader.bin` 4 232 Б,
`app.bin` 8 276 Б (debug-сборка `-DEC_DEBUG_BUILD=ON` — 9 172 Б); gate —
8 КиБ / 20 КиБ. Прямая прошивка `flash-app`
программирует только приложение; для запуска нужны валидные метаданные
(их пишет I2C OTA или `bootloader_tool`). Диагностическая сборка
загрузчика без I2C-мастера:

```bash
cmake -B build-self -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DMCU=C011x6 -DBOOT_SELFTEST=ON
cmake --build build-self --target flash-boot
```

LED: PASS — 3 вспышки, FAIL — мигание 100 мс.

**OKO.** На стенде установлен STM32F103**CBT6** (128 КиБ, слот приложения
120 КиБ); загрузчик принимает образы до 120 КиБ. Собирайте приложение с
`-DMCU=CBT6`: сборка C8T6 запускается, но читает метаданные из страниц в
конце 64 КиБ, тогда как загрузчик CBT6 пишет их в конец 128 КиБ, поэтому
`oko-ec info` показывает `img_size=0`. Рабочие образы OKO:
`output/bench/stm32/app-1.0.0.bin` и `app-1.0.1.bin` (по 2324 Б).

Результат:

```
build/bootloader.elf|hex|bin
build/app.elf|hex|bin
build/app-<версия>.bin|hex      # копии с версией в имени
build/bootloader.map, build/app.map
```

`app.bin`/`app.hex` остаются алиасами для ST-Link и
`flash-app-i2c`; в WebUI `/api/flash` нужно загружать имя с версией
(`app-1.0.1.bin`) — сервер берёт версию из имени файла.

Версия прошивки берётся из `git describe --tags` (иначе 1.0.0 →
`0x00010000`) и генерируется в `build/generated/version.h`. Версию можно
задать на этапе конфигурации:

```bash
cmake -B build-101 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
      -DFW_VERSION_STRING=1.0.1     # FW_VERSION = 0x00010001
```

`FW_VERSION_STRING` — кэш-переменная. В вендоренном дереве без `.git`
(как `project/app/stm32_acpi-ec` в OKO) значение из `-D` используется
напрямую; в git-чекауте с тегами `v*.*.*` версия из `git describe` имеет
приоритет. Проверка: `grep FW_VERSION build-101/generated/version.h`.

## OpenOCD для STM32C0

OpenOCD master собирается один раз; артефакт не коммитится
(`output/bench/openocd-master/`):

```bash
git clone https://github.com/openocd-org/openocd.git
cd openocd
git submodule update --init --depth 1 jimtcl   # libjaylink не нужен без J-Link
./bootstrap
./configure --prefix=$PWD/install --enable-stlink --disable-jlink \
            --disable-cmsis-dap --disable-ftdi --enable-internal-jimtcl
make -j
```

Если в системе нет `libusb-1.0-0-dev`, поставьте его (Debian:
`sudo apt install libusb-1.0-0-dev autoconf automake libtool pkg-config texinfo`).
Проверка:

```bash
openocd -s $PWD/tcl -f project/app/stm32_acpi-ec/scripts/openocd_c0.cfg \
        -c "init; targets; flash info 0; shutdown"
```

Ожидается Cortex-M0+ C0 (C011 или C031) и 32 КиБ Flash. ST-Link должен быть
прописан в udev (обычно `/etc/udev/rules.d/60-openocd.rules`, группа
`plugdev`), иначе `libusb_open() failed with LIBUSB_ERROR_ACCESS`.
`transport select swd` требует прошивки ST-Link примерно 2.37+; старый
адаптер обновляется STM32CubeProgrammer. Fallback без OpenOCD —
`STM32CubeProgrammer_CLI`.

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
bootloader_tool ec /dev/i2c-7 flash build/app-1.0.0.bin 0x00010000
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
| `.github/workflows/ci.yml` | push/PR в `main` | format-check, cppcheck (F1 и C0), сборка C8T6+CBT6+C011x6+C031x6, host-тесты |
| `.github/workflows/release.yml` | тег `v*.*.*` | сборка обоих вариантов + `bootloader_tool` → GitHub Release |
| `.github/workflows/flash.yml` | `workflow_dispatch` | сборка, прошивка boot (ST-Link) и app (I2C OTA, retry 3×), fallback через ST-Link |

SDK `STM32CubeF1` и `STM32CubeC0` в CI кэшируются и клонируются в
`${{ github.workspace }}/../STM32CubeF1` и `../STM32CubeC0`;
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
`git`, ST-Link v2, доступ к I2C-шине и USB (`dialout`/`plugdev`) и SDK
`STM32CubeF1`/`STM32CubeC0` в `_work/stm32_acpi-ec/`; для C0 — OpenOCD
master с `tcl/target/stm32c0x.cfg`.

## Отладка

```bash
openocd -f scripts/openocd.cfg          # F103, терминал 1
arm-none-eabi-gdb build/app.elf -x scripts/gdb_debug.gdb  # терминал 2

# C0 (OpenOCD master, путь к tcl — из вашей сборки)
openocd -s <openocd>/tcl -f scripts/openocd_c0.cfg
arm-none-eabi-gdb build-c031/app.elf -ex "target extended-remote :3333"
```

## Прошивка через UART (системный загрузчик)

```bash
scripts/flash_stm32.sh <ПОРТ> <СКОРОСТЬ> <ФАЙЛ>
scripts/flash_stm32.sh /dev/ttyCH343USB0 115200 build/app.bin
```
