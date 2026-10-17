# scripts/gdb_debug.gdb
#
# Сценарий GDB для отладки приложения через загрузчик
# (OpenOCD должен быть запущен: openocd -f scripts/openocd.cfg).

target remote :3333

# 1. Сброс и halt
monitor reset halt

# 2. Загружаем прошивку в слот приложения (0x08002000)
load

# 3. Breakpoint на main
break main

# 4. Сброс БЕЗ halt — MCU перезагружается,
#    загрузчик находит прошивку и переходит в неё
monitor reset

# 5. Продолжаем — остановимся в main прошивки
continue