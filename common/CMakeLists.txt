# common/CMakeLists.txt
#
# Общая логика, не зависящая от цели: CRC, метаданные, фрейминг
# протокола, общие типы и конечный автомат питания. Компилируется один
# раз и линкуется в оба образа; неиспользуемые секции удаляются
# --gc-sections.

add_library(common STATIC
    crc.c
    meta.c
    protocol.c
    power_fsm.c
)

ec_configure(common)
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/${EC_HAL_DIR}
    ${CMAKE_SOURCE_DIR}/app/inc
)