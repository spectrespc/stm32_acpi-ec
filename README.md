# stm32_acpi-ec

Эмулятор контроллера батареи (ACPI EC) на **STM32F103** и **STM32C0**
с обновлением прошивки по **I2C (OTA)**. Состоит из загрузчика, приложения
и хостовой утилиты прошивки. На STM32C0 (C011/C031) приложение дополнительно
управляет питанием хоста и сном EC (кнопка power, `PWR_EN`, Stop).

## Возможности

- Загрузчик 8 KB с валидацией образа (SP/PC/размер + CRC32) и
  подтверждением запуска (ping-pong метаданные).
- Приложение: I2C slave `0x42`, эмуляция 3S Li-ion, команды батареи.
- OTA-протокол v3: `GET_POWER`/`SET_POWER` (см. [docs/protocol.md](docs/protocol.md)).
- Четыре цели: STM32F103**C8T6** (64 KB), **CBT6** (128 KB),
  STM32C011**F6P6** (32 KB Flash / 6 KB SRAM) и STM32C031**F6P6**
  (32 KB / 12 KB), TSSOP20.
- FSM кнопки питания на C0 (C011/C031): сон/wake/reset/выключение/включение,
  Stop с пробуждением кнопкой и адресом I2C.
- Модульные тесты `common/` и FSM на хосте.

## Аппаратные требования

- STM32F103C8T6 / CBT6, STM32C011F6P6 (стенд) или STM32C031F6P6
  (TSSOP20).
- ST-Link v2 + **OpenOCD из git master** (в OpenOCD 0.12 нет C0; для F103
  подходит любой свежий OpenOCD). Конфигурация C0 — `scripts/openocd_c0.cfg`.
- I2C-адаптер хоста (RV1106 / RPi / CP2112) для OTA.
- Светодиод: PB12 (F103) / PA4 (C011/C031); питание хоста: PA1.
- Карта пинов C0 и требования к обвязке — [docs/hardware.md](docs/hardware.md).

## Быстрый старт

```bash
# SDK STM32CubeF1 должен лежать в ../STM32CubeF1
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=C8T6
cmake --build build

# C0: нужен ../STM32CubeC0 и OpenOCD master
cmake -B build-c011 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DMCU=C011x6
cmake --build build-c031

# Прошивка (C0-цели используют scripts/openocd_c0.cfg)
cmake --build build-c031 --target flash-boot
cmake --build build-c031 --target flash-app
```

Результаты сборки: `build/bootloader.{elf,hex,bin}`, `build/app.{elf,hex,bin}`.

Выбор MCU: `-DMCU=CBT6` (128 KB), `-DMCU=C011x6` (32 KB/6 KB)
или `-DMCU=C031x6` (32 KB/12 KB).

## Сборка утилиты и тестов

```bash
cmake -B build_host
cmake --build build_host --target check
```

## CI/CD, линтинг и прошивка

Пайплайн автоматизации на **GitHub Actions**:

| Workflow | Триггер | Назначение |
|----------|---------|------------|
| `.github/workflows/ci.yml` | push/PR в `main` | clang-format, cppcheck, сборка C8T6+CBT6+C011x6+C031x6, тесты |
| `.github/workflows/release.yml` | тег `v*.*.*` | релизные образы + GitHub Release |
| `.github/workflows/flash.yml` | `workflow_dispatch` | прошивка на self-hosted runner (ST-Link + I2C OTA) |

Локальные проверки (нативная сборка):

```bash
cmake -B build_host
cmake --build build_host --target format        # автоформатирование
cmake --build build_host --target format-check  # проверка стиля
cmake --build build_host --target cppcheck      # статический анализ
```

Pre-commit-хуки (формат, пробелы, YAML, защита `main`):

```bash
pip install pre-commit
pre-commit install
```

Скрипты разработчика:

```bash
./scripts/push_changes.sh "boot: описание"      # проверки → коммит → пуш
./scripts/tag_release.sh 1.0.1 "Описание"       # полная проверка → тег → релиз
```

Прошивка на self-hosted runner:

```bash
gh workflow run flash.yml -f mcu=C011x6 -f flash_bootloader=true
```

## Структура

```
cmake/       toolchain, настройки MCU, FindSTM32CubeF1/C0
platform/    startup + линкер-скрипты (stm32f103, stm32c0)
hal/include  общие board/flash/i2c/system/power
hal/f1,hal/c0 HAL семейства (пины, такты, flash, Stop)
common/      crc, meta, protocol, types, power_fsm, version
boot/        загрузчик
app/         приложение (батарея, команды)
tools/       bootloader_tool (нативный GCC)
tests/       модульные тесты (host)
scripts/     прошивка, OpenOCD, GDB, генерация версии
docs/        архитектура, протокол, карта памяти, сборка
```

## Документация

- [Архитектура](docs/architecture.md)
- [Протокол v3](docs/protocol.md)
- [Карта памяти](docs/flash_map.md)
- [Руководство по сборке](docs/build_guide.md)
- [Карта пинов C0](docs/hardware.md)
- [История изменений](docs/changelog.md)

## Известное ограничение

Короткое нажатие в AWAKE реализует сон только с хостом, который
поддерживает протокол v3 и отправляет `SET_POWER`. С текущим `oko-ec`
(proto 2) это осознанный no-op: EC ждёт ACK 5 с и остаётся в AWAKE.

## Лицензия

См. `LICENSE` (при наличии).
