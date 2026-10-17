# common/CMakeLists.txt
#
# Общая логика, не зависящая от цели: CRC, метаданные, фрейминг
# протокола, общие типы. Компилируется один раз и линкуется в оба
# образа; неиспользуемые секции удаляются --gc-sections.

add_library(common STATIC
    crc.c
    meta.c
    protocol.c
)

stm32f103_configure(common)
target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal
)