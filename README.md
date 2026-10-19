# stm32_acpi-ec

Эмулятор контроллера батареи (ACPI EC) на **STM32F103** с обновлением
прошивки по **I2C (OTA)**. Состоит из загрузчика, приложения и хостовой
утилиты прошивки.

## Возможности

- Загрузчик 8 KB с валидацией образа (SP/PC/размер + CRC32) и
  подтверждением запуска (ping-pong метаданные).
- Приложение: I2C slave `0x42`, эмуляция 3S Li-ion, команды батареи.
- OTA-протокол v2 (см. [docs/protocol.md](docs/protocol.md)).
- Единая прошивка для STM32F103**C8T6** (64 KB) и **CBT6** (128 KB).
- Модульные тесты `common/` на хосте.

## Аппаратные требования

- STM32F103C8T6 / CBT6
- ST-Link v2 (OpenOCD) для прямой прошивки
- I2C-адаптер хоста (RV1106 / RPi / CP2112) для OTA
- Светодиод на PB12; питание host-устройства на PA1 (загрузчик)

## Быстрый старт

```bash
# SDK STM32CubeF1 должен лежать в ../STM32CubeF1

cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake
cmake --build build

# Прошивка
cmake --build build --target flash-boot      # загрузчик (ST-Link)
cmake --build build --target flash-app       # приложение (ST-Link)
```

Результаты сборки: `build/bootloader.{elf,hex,bin}`, `build/app.{elf,hex,bin}`.

Выбор MCU: добавьте `-DMCU=CBT6`.

## Сборка утилиты и тестов

```bash
cmake -B build_host
cmake --build build_host --target check
```

## CI/CD, линтинг и прошивка

Пайплайн автоматизации на **GitHub Actions**:

| Workflow | Триггер | Назначение |
|----------|---------|------------|
| `.github/workflows/ci.yml` | push/PR в `main` | clang-format, cppcheck, сборка C8T6+CBT6, тесты |
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
gh workflow run flash.yml -f mcu=C8T6 -f flash_bootloader=false
```

## Структура

```
cmake/       toolchain, настройки MCU, FindSTM32CubeF1
platform/    startup + линкер-скрипты (один экземпляр)
hal/         board, flash, i2c, system (по BOOT_BUILD)
common/      crc, meta, protocol, types, version
boot/        загрузчик
app/         приложение (батарея, команды)
tools/       bootloader_tool (нативный GCC)
tests/       модульные тесты (host)
scripts/     прошивка, OpenOCD, GDB, генерация версии
docs/        архитектура, протокол, карта памяти, сборка
```

## Документация

- [Архитектура](docs/architecture.md)
- [Протокол v2](docs/protocol.md)
- [Карта памяти](docs/flash_map.md)
- [Руководство по сборке](docs/build_guide.md)
- [История изменений](docs/changelog.md)

## Лицензия

См. `LICENSE` (при наличии).
