# История изменений

## [Unreleased]

### Added

- **Освобождение прижатой SCL по таймеру (F1).** Аппаратный SCL-low
  timeout `I2C_TIMEOUTR` (~15 мс, `TIMEOUTA=350`, `TIDLE=0`) в
  `i2c_hw_config()`: при `I2C_ISR_TIMEOUT` ISR сбрасывает `rx_count`/
  `rx_frame_done` и делает `PE=0/1`, отпуская линию. Страховка —
  программный bus-watchdog (`app/src/bus_watchdog.c`,
  `POWER_BUS_HANG_MS=15`): при `PE=1` и отсутствии прогресса
  `tx_pos`/`rx_count` дольше порога вызывает `i2c_recover()`. Оба порога
  ниже таймаута `rk3x-i2c` (~20–30 мс).
- **Отладочный слой I2C (`-DEC_DEBUG_BUILD=ON`).** Счётчики ADDR/TXIS/
  STOP/NACK/error/TIMEOUT, `bus_hang_ms_max`, `irq_masked_us_max`,
  `paused_ms_max`, снимки `ISR`/`CR1` на ADDR(read) и зависании; зеркало
  в `PWR->BKP2R/BKP3R` (переживает `NRST`). Команды `0x28 GET_DEBUG`
  (60 байт) и `0x29 DEBUG_HOLD_SCL` (эталонное удержание SCL); в обычной
  сборке отвечают `0xFF`.
- **Хостовый ретрай записи:** `i2c_transport_write_read_timed()` повторяет
  неудачную запись (`I2C_WRITE_RETRIES=2`, с probe адреса); первый кадр
  после перезагрузки RV1106 — именно запись.
- **`bench/ec_bus_soak.py`:** soak-матрица reboot/read-window/detect/
  hold-scl с отчётом в `output/bench/`.
- **C0-цель: STM32C011F6P6 (стенд, 6 КиБ SRAM) и STM32C031F6P6
  (12 КиБ SRAM).** Ветки `MCU=C011x6`/`MCU=C031x6`: `cmake/stm32c0.cmake`,
  `cmake/FindSTM32CubeC0.cmake`, `platform/stm32c0/` (startup CubeC0 +
  общие линкер-скрипты 8/20 КиБ с `RAM_LENGTH`/`ECFLAG_ADDR`), карта пинов
  в `docs/hardware.md`. Фактический стенд определяется как `STM32C011xx`
  (flash 32768, pagesize 2048, sram 6144). Цели C8T6/CBT6 не изменены.
- **FSM кнопки и питания** (`common/power_fsm.c/.h`): short/long/very-long/
  double, graceful-запросы SLEEP/SHUTDOWN/REBOOT с таймаутом 5 с,
  power-cycle с `T_off`, boot-fail 3→ERROR, тишина хоста → SUSPENDED,
  idle → Stop, LED-коды, backup-регистры PWR. Хостовые тесты —
  `tests/test_power_fsm.c` (28 кейсов).
- **Протокол v3**: `CMD_GET_POWER 0x20` (8 байт состояния) и
  `CMD_SET_POWER 0x21`; `PROTO_VER=3`. Команды `0x01…0x0E` и `0x10…0x12`
  совместимы с proto-2 хостами.
- **HAL C0** (`hal/c0/`): SYSCLK 12 МГц, I2C1 от HSIKER 48 МГц
  (slave 400 кГц) с `WUPEN`,
  Flash 2 КиБ/double-word, Stop с пробуждением кнопкой/адресом I2C,
  PWR_EN/PWR_GOOD/HOST_WAKE, backup-регистры.
- **Загрузчик C0**: `BOOT_SELFTEST` (erase/program/read контрольной
  страницы с LED-индикацией), граница SP по семейству, `i2c_deinit()`.
- `scripts/openocd_c0.cfg` — ST-Link SWD + `target/stm32c0x.cfg` для
  OpenOCD master.
- Живая проверка C011: прошиты bootloader/app, записана валидная
  `MetaConfig`, приложение запускается из загрузчика, FSM в OFF,
  `PWR_EN=0`, I2C `PE=0`, backup-регистры сохраняются.

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

### Fixed

- **I2C1 тактировался от PCLK, а не от HSIKER.** `RCC->CCIPR.I2C1SEL=00`
  (PCLK) в Stop не тактирует периферию, поэтому `WUPEN` не будил ядро по
  адресу; кроме того, `TIMINGR` и `tTIMEOUT` считались от 12 МГц вместо
  48 МГц (аппаратный timeout ~60 мс вместо 15). Исправлено на
  `I2C1SEL=10` (HSIKER, 48 МГц) в `hal/c0/clock.c`.
- **`i2c_pause()`/`i2c_resume()` теперь работают и в приложении, а не
  только в загрузчике.** `PE=0` + маска `I2C1_IRQn` на время операции с
  Flash; инвариант «Flash XOR живой I2C» не зависит от порядка вызовов в
  `main()`. `i2c_resume()` заново конфигурирует периферию и не включает
  её, если хост обесточен.
- **Stop не удерживает линию под маской прерываний (H4).** Перед
  `__disable_irq()` и после него проверяются `ADDR`/`TXIS`/`TXIE`:
  при незавершённой сделке EC не уходит в Stop, а кадр отдаётся ISR.
  `dbg_irq_masked_us_max` измеряется по `SysTick->VAL`.
- **Транзакция, разбудившая EC, не обрывается.** После `WFI` CR1 не
  трогается, если идёт ADDR/TXIS/TXIE; иначе сброс `TXIE` оставлял slave
  в растяжке до watchdog-а.
- **`flash-app` для C011:** условие OpenOCD было `MCU STREQUAL
  "C031x6"`, из-за чего C011 прошивался конфигом `stm32f1x`; теперь
  `MCU MATCHES "^C0"`.
- **Векторы C011/C031 проверены (H3).** Таблицы `IRQn` в
  `stm32c011xx.h` и `stm32c031xx.h` совпадают (`I2C1_IRQn=23`,
  `EXTI0_1_IRQn=5`); в собранном `app.elf` слово вектора по смещению
  `(16+23)*4 = 0x9C` от `0x08002000` содержит `0x08002d79`
  (`I2C1_IRQHandler | 1`), EXTI0 — `0x0800320d`. Вывод `nm`/`objdump`
  сохранён в `output/bench/ec_vectors_c011.txt`. Отдельный
  `startup_stm32c011xx.s` не нужен.
- **Линковка ELF для ST-Link:** добавлен `-Wl,--nmagic`. Без него GNU ld
  (binutils 2.45) включал ELF-заголовки в первый `PT_LOAD`, и `flash-app`
  затирал загрузчик (на C031 — ELF-магия по `0x08000000`), а `flash-boot`
  мог писать ниже Flash. Проверено `readelf -lW` и прошивкой.
- FSM: `PWR_GOOD=0` теперь принудительно даёт OFF даже при `pwr_en=1` в
  backup; вход в Stop перепроверяет кнопку/I2C под маскированными
  прерываниями; `WUPEN` пишется отдельно после `PE`.
- Валидация SP образа: верхняя граница `EC_RAM_TOP - 16` и выравнивание 8.
- **Stop на C011 не спал:** pending-прерывание SysTick сразу выводило ядро
  из WFI. `power_enter_stop()` теперь вызывает `system_stop_systick()` и
  сбрасывает `PENDSTCLR` перед WFI; после пробуждения SysTick
  восстанавливается. Проверено на стенде: `SLEEPDEEP=1`, PC на WFI,
  `PWR_EN=0`, I2C `PE=0`.
- **C0 I2C: два дефекта чтения slave.**
  1. `TXIE` был включён с инициализации, ISR до адреса писал `0xFF` в
     TXDR — первый байт ответа уходил впереди `LEN`.
  2. После последнего байта кадра `TXIE` выключался; при чтении хостом
     большего буфера slave с `NOSTRETCH=0` **навсегда удерживал SCL** —
     вся шина умирала (наблюдалось после OTA/`oko-ec`).
  Исправлено: `TXIE` включается только на ADDR чтения, до STOP остаётся
  включённым и отдаёт `0xFF`-filler; на STOPF/NACK/ошибке выполняется
  `i2c_hw_config()` (PE=0/1 + TIMINGR/OAR1/CR1), что очищает TXDR и
  отпускает линию. `power_enter_stop()` не спит при `BUSY`/`TXIE`.
  Прошивка собрана (`app.bin` 7 628 Б), OTA-скрипт —
  `output/bench/stm32/c011/ota_over_i2c.sh`.
- **WUPEN в рабочем режиме держал SCL.** На C011 установленный `WUPEN`
  при `PE=1` вне Stop прижимал SCL к нулю и убивал всю шину. Теперь
  WUPEN включается только непосредственно перед `WFI` (`i2c_wakeup_arm`)
  и снимается после пробуждения. Проверено на стенде: `oko-ec info`
  (`proto=3`, 7628 Б), `battery`, `GET_POWER` (AWAKE, PWR_EN|PWR_GOOD),
  повторное чтение после 5 с (Stop → wake по адресу), daemon стабилен.

### Changed

- `hal/` разделён на `hal/include` (общие заголовки), `hal/f1` и `hal/c0`;
  выбор — CMake-переменная `EC_HAL_DIR`.
- `meta.c` пишет метаданные буфером (32 байта, дополнение `0xFF`), чтобы
  поддержать double-word programming C031 и полуслова F103.
- Флаг OTA — линкер-символ `__ec_update_flag` вместо константы
  `UPDATE_FLAG_ADDR` (`0x20002FFC` на C031, `0x20004FFC` на F103).
- CI: матрица дополнена `C011x6`/`C031x6`, cppcheck запускается для F1 и C0,
  `setup-sdk` клонирует STM32CubeC0; `flash.yml` поддерживает обе C0-цели.

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
