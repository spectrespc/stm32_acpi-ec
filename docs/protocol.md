# Спецификация I2C-протокола v3

## Транспорт

- I2C slave, 7-битный адрес `0x42` (STM32: загрузчик и приложение).
- Активная плата sc230ai работает на 100 кГц
  (`rv1106g-luckfox-pico-pro-max-sc230ai.dts`), базовый pro-max — на 400 кГц
  (`rv1106g-luckfox-pico-pro-max.dts`). Slave обязан держать обе скорости:
  STM32C031 настроен на Fast-mode 400 кГц.
- STM8-эмулятор моторов отвечает по тому же протоколу на адресе `0x39`.

## Формат кадра

```
Запрос:  [LEN_LO][LEN_HI][CMD][PAYLOAD...]
Ответ:   [LEN_LO][LEN_HI][STATUS][PAYLOAD...]
```

`LEN` — количество байт после него (`CMD`/`STATUS` + `PAYLOAD`),
little-endian. Запрос и ответ — раздельные транзакции: мастер пишет
кадр, затем читает ответ. Кадры — не более 128 байт данных.

## Команды

| Код | Имя | Payload запроса | Ответ |
|-----|-----|-----------------|-------|
| `0x01` | GET_INFO | — | 12 байт (см. ниже) |
| `0x02` | BEGIN_UPDATE | len(4) + crc(4) + version(4) | — |
| `0x03` | WRITE_DATA | offset(4) + len(1) + data(len) | — |
| `0x04` | VERIFY_UPDATE | — | — |
| `0x05` | COMMIT | — | — |
| `0x07` | GET_STATUS | — | status(1)+state(1)+res(1)+offset(4) |
| `0x08` | RESET | — | — (устройство сбрасывается) |
| `0x0E` | ENTER_UPDATE | — | — (приложение → загрузчик) |
| `0x20` | GET_POWER | — | 8 байт (см. ниже) |
| `0x21` | SET_POWER | host_state(1) + ack_flags(1) | — |
| `0x28` | GET_DEBUG | — | 60 байт (только `DEBUG_BUILD`) |
| `0x29` | DEBUG_HOLD_SCL | enable(1) | — (только `DEBUG_BUILD`) |

Команды батареи (только приложение):

| Код | Имя | Ответ |
|-----|-----|-------|
| `0x10` | GET_BATTERY_DATA | 14 байт (см. ниже) |
| `0x11` | GET_CELL_VOLTAGES | 3×LE16 (мВ) |
| `0x12` | GET_TEMPERATURE | LE16 (0.1 °C) |

`0x01` — общая команда: приложение возвращает `device_type = 2`,
загрузчик — `1`. По `device_type` хост отличает роль; `GET_POWER` и
`SET_POWER` в загрузчике возвращают `STATUS 0xFF` (ERR_UNSUPPORTED).

## Ответ GET_INFO (12 байт)

| Смещение | Поле |
|----------|------|
| 0 | device_type: 1 = загрузчик, 2 = приложение |
| 1 | версия протокола (3) |
| 2..5 | версия прошивки, LE32 |
| 6..9 | размер образа, LE32 |
| 10 | флаги: бит 0 = pending |
| 11 | доп.: приложение — число ячеек (3) |

Версия прошивки кодируется как `major<<16 | minor<<8 | patch`
(по умолчанию `0x00010000`).

## Ответ GET_POWER (8 байт)

| Байт | Поле | Значения |
|---|---|---|
| 0 | `host_state` | 0 OFF, 1 BOOTING, 2 AWAKE, 3 SUSPENDING, 4 SUSPENDED, 5 SHUTTING_DOWN, 6 REBOOTING, 7 ERROR |
| 1 | `flags` | b0 `PWR_EN`, b1 `PWR_GOOD`, b2 хост сообщал состояние, b3 таймаут связи, b4 boot-fail, b5 LSE ok |
| 2 | `power_request` | 0 нет, 1 SLEEP, 2 SHUTDOWN, 3 REBOOT (EC → хост) |
| 3 | `last_event` | 0 нет, 1 short, 2 long, 3 very-long, 4 double, 5 long в выключенном |
| 4 | `event_count` | счётчик распознанных нажатий (uint8, wrap) |
| 5 | `reset_cause` | bitmask: b0 POR, b1 PIN, b2 SFT, b3 IWDG, b4 WWDG, b5 BOR, b6 LPWR |
| 6 | `last_wake_src` | 0 нет, 1 кнопка (EXTI), 2 адрес I2C (`WUPEN`), 3 LPTIM (резерв), 4 сброс |
| 7 | `wake_count` | число пробуждений из Stop с момента загрузки (uint8, wrap) |

На STM32C031 нет LPTIM, а RTC не имеет wakeup-таймера, поэтому
`last_wake_src=3` не выдаётся, `flags.b5` всегда 0. На STM32C031 также
нет флага BOR — `reset_cause.b5` всегда 0.

## CMD_SET_POWER (0x21)

Payload 2 байта:

| Байт | Поле |
|---|---|
| 0 | `host_state` (пишет хост) |
| 1 | `ack_flags`: b0 = `power_request` принят |

Ответ — только `STATUS`.

Правила:

- Хост обязан записывать `host_state` при старте демона (AWAKE), перед
  `echo mem` (SUSPENDING), после resume (AWAKE), перед shutdown
  (SHUTTING_DOWN), перед `reboot` (REBOOTING).
- EC считает состояние, сообщённое хостом, приоритетным. При отсутствии
  кадров дольше 3 с EC сам помечает SUSPENDED (`flags.b3=1`); первый
  валидный кадр возвращает AWAKE.
- `last_wake_src`/`wake_count` — единственный способ подтвердить
  пробуждение по адресу I2C до подключения шины: поля читаются через GDB
  (`p g_last_wake_src`-эквивалент в драйвере) и через `GET_POWER`.
- После `CMD_RESET` (0x08) и OTA `host_state` и `pwr_en_wanted`
  восстанавливаются из backup-регистров; `flags.b2` остаётся 0 до первого
  `SET_POWER`.
- Короткое нажатие в AWAKE при хосте без v3 (нет `SET_POWER`) — no-op:
  EC ждёт ACK 5 с, затем возвращается в AWAKE, питание не трогает.

## Ответ GET_BATTERY_DATA (14 байт)

| Смещение | Поле |
|----------|------|
| 0..1 | напряжение батареи, мВ |
| 2..3 | ток, мА (знаковый) |
| 4..5 | температура, 0.1 °C (знаковая) |
| 6 | SOC, % |
| 7 | флаги статуса |
| 8..13 | напряжения ячеек 0..2, мВ |

## Статусы (`STATUS`)

| Код | Имя | Значение |
|-----|-----|----------|
| `0x00` | OK | успех |
| `0x01` | ERR_CMD | неизвестная команда |
| `0x02` | ERR_PARAM | неверный параметр |
| `0x03` | ERR_STATE | недопустимое состояние |
| `0x05` | ERR_FLASH | ошибка Flash |
| `0x06` | ERR_CRC | CRC не совпал |
| `0x07` | ERR_SEQ | нарушена последовательность offset |
| `0x08` | ERR_LEN | неверная длина кадра |
| `0xFF` | ERR_UNSUPPORTED | команда не поддерживается этим образом (power-команды в загрузчике) |

## Процесс обновления

1. `BEGIN_UPDATE` — задаёт размер, CRC32 и версию образа.
2. `WRITE_DATA` — данные передаются по `CHUNK_SIZE` (32 байта), строго
   по возрастанию `offset` (0, 32, 64, ...); на границе страницы Flash
   страница стирается автоматически: **1 KB для F103, 2 KB для C031**.
   На C031 данные программируются double-word по 8 байт (чанк 32 байта =
   4 double-word).
3. `VERIFY_UPDATE` — загрузчик считает CRC32 образа и сверяет.
4. `COMMIT` — записываются метаданные (`VALID | PENDING`).
5. `RESET` — сброс; загрузчик запускает новый образ, приложение
   подтверждает его (`PENDING` снимается).

### Задержки, ретраи и пробуждение

- Стирание страницы C031 занимает ~22 мс; хост выдерживает
  `I2C_WRITE_READ_DELAY_US` = 100 мс, что перекрывает паузу `i2c_pause()`.
- Первый кадр после выхода EC из Stop может получить NACK: EC просыпается
  по адресу, но не успевает обслужить кадр. `i2c_transport_write_read_timed()`
  повторяет **запись** (`I2C_WRITE_RETRIES=2`, с probe адреса) и чтение
  (`I2C_READ_RETRIES`), поэтому один потерянный кадр не фатален.
- Если мастер исчез посреди чтения, EC удерживает SCL не дольше
  ~10-15 мс: аппаратный `TIMEOUTR` (~10 мс) или bus-watchdog (15 мс)
  переконфигурируют I2C.
  Хост увидит один `-ETIMEDOUT`; следующий кадр проходит без сброса EC.

## Отладочные команды (только `DEBUG_BUILD`)

Сборка `-DEC_DEBUG_BUILD=ON` добавляет `CMD_GET_DEBUG 0x28` и
`CMD_DEBUG_HOLD_SCL 0x29`; в обычной прошивке обе отвечают
`STATUS 0xFF` (`ST_ERR_UNSUPPORTED`).

`0x28` — 60 байт, LE:

| Смещение | Поле |
|---|---|
| 0 | `addr_events` u32 |
| 4 | `read_addr_events` u32 |
| 8 | `txis_events` u32 |
| 12 | `stop_fires` u32 |
| 16 | `nack_fires` u32 |
| 20 | `err_fires` u32 |
| 24 | `timeout_fires` u32 |
| 28 | `bus_hang_events` u32 |
| 32 | `bus_hang_ms_max` u32 |
| 36 | `irq_masked_us_max` u32 |
| 40 | `paused_ms_max` u32 |
| 44 | `isr_at_read_addr` u16 |
| 46 | `cr1_at_read_addr` u16 |
| 48 | `hang_isr` u16 |
| 50 | `hang_cr1` u16 |
| 52 | `BKP2R` (16-битная упаковка ISR/CR1, см. ниже) u32 |
| 56 | `BKP3R` (16-битная упаковка счётчиков, см. ниже) u32 |

BKP2R (16 бит): 0 TXE, 1 TXIS, 2 RXNE, 3 ADDR, 4 NACKF, 5 STOPF,
6 BERR, 7 ARLO, 8 OVR, 9 TIMEOUT, 10 BUSY, 11 DIR, 12 PE, 13 TXIE,
14 WUPEN, 15 «зеркало валидно». BKP3R (16 бит): 0..3 `addr_events`,
4..7 `read_addr_events`, 8..11 `bus_hang_events`, 12 `timeout_fires != 0`,
13 `hold_scl`, 14 `paused`, 15 «зеркало валидно». На STM32C0 backup-
регистры 16-битные (RM0490: `BKP[15:0]`), поэтому 32-битные поля ответа
содержат только младшие 16 бит.

`0x29` с payload `[1]` заставляет следующий `ADDR(read)` не заполнять
`TXDR` — эталонное зависание для проверки таймерного освобождения; `[0]`
выключает режим. Backup-регистры переживают `NRST`, поэтому BKP2R/BKP3R
читаются после сброса, когда RAM-счётчики уже потеряны.

## Вход в режим обновления из приложения

Команда `0x0E`: приложение отвечает `OK`, пишет магическое число
`0x55AA55AA` в `__ec_update_flag` (линкер-символ: `0x20004FFC` для F103,
`0x20002FFC` для C031) и сбрасывается. Загрузчик видит флаг и остаётся в
режиме OTA.