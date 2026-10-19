# История изменений

## [Unreleased]

### Added

- **CI/CD на GitHub Actions**: `ci.yml` (push/PR — format-check, cppcheck,
  сборка C8T6+CBT6, host-тесты), `release.yml` (тег `v*.*.*` — релизные
  образы + GitHub Release), `flash.yml` (self-hosted — ST-Link и I2C OTA
  с retry 3× и аварийным fallback).
- **Код-стайл**: `.clang-format` (LLVM + Allman) и цели CMake
  `format` / `format-check`. Версия clang-format закреплена на 18.x во
  всех путях проверки (pre-commit, CI, локальная цель).
- **Статический анализ**: цель CMake `cppcheck` (`--error-exitcode=1`),
  два прохода — загрузчик (`-DBOOT_BUILD`) и приложение.
- Релизные артефакты именуются с вариантом MCU
  (`bootloader_C8T6.elf`, `app_CBT6.bin`, …), чтобы варианты не
  перезаписывали друг друга в GitHub Release.
- **Pre-commit**: `.pre-commit-config.yaml` (clang-format, пробелы, конец
  файла, YAML, защита ветки `main`).
- **Скрипты**: `scripts/tag_release.sh` (pre-tag проверки + тег) и
  `scripts/push_changes.sh` (проверки → коммит → пуш).
- Параметры прошивки `BOOTLOADER_TOOL` / `I2C_DEVICE` читаются из env.

### Changed (мажорное изменение — реструктуризация)

- Переход на **CMake** (кросс-сборка прошивок + нативная сборка tools/tests).
- Единый источник правды в `common/`: CRC, `MetaConfig`, фрейминг протокола,
  общие типы. `MetaConfig` теперь определён один раз и проверяется
  `_Static_assert` (размер 28 байт и смещения полей).
- `hal/` объединяет `board`, `flash`, `i2c`, `system`; различающийся код —
  под `BOOT_BUILD`.
- Новая структура: `platform/`, `hal/`, `common/`, `boot/`, `app/`,
  `tools/`, `tests/`, `scripts/`, `docs/`.
- SDK STM32CubeF1 вынесен в соседнюю директорию `../STM32CubeF1`
  (больше не внутри проекта).
- `protocol.c` разделён: чистый фрейминг — в `common/protocol.{h,c}`,
  диспетчер загрузчика — в `boot/src/request.c`.
- Хостовая утилита разделена на `i2c_transport`, `crc_host` и `main`;
  поддержка STM8 (`motor`) сохранена.
- Версия прошивки генерируется из `git describe`
  (`common/version.h.in` → `build/generated/version.h`).
- Добавлены модульные тесты (CRC, метаданные, протокол) и моки HAL.
- Весь исходный код приведён к единому стилю `.clang-format`
  (изменения только форматирования — `.bin` остаются байт-идентичными).
- Выходные `.elf` теперь кладутся в корень сборки с расширением `.elf`
  (`build/bootloader.elf`, `build/app.elf`) — как документировано в README.

### Removed

- Каталоги `bootloader/`, `firmware/`, `bootloader-tool/` (код перенесён).
- Prebuilt-бинарники утилиты из репозитория (собираются из исходников).

### Fixed

- I2C slave (загрузчик и приложение) не подтверждал собственный адрес:
  `i2c_init()` устанавливал `ACK` до `PE`, а на STM32F103 запись `ACK` при
  `PE=0` игнорируется. Теперь `PE` включается первым, затем `ACK`.
  Проверено на железе (STM32F103CBT6): OTA-прошивка приложения по I2C
  проходит полностью (write → verify → commit → запуск приложения).

### Notes

- Бинарная идентичность `.bin` не является целью: объединение TU и
  выделение `commands.c` меняют раскладку секций и кодогенерацию при
  неизменном поведении. Эквивалентность подтверждается размерами,
  поведением и хостовыми тестами.
