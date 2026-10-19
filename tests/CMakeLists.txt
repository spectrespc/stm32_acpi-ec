# tests/CMakeLists.txt
#
# Модульные тесты на хосте (нативный компилятор).
# common/ не зависит от железа (кроме meta.c, для которой есть моки).

add_library(unity STATIC unity.c)
target_include_directories(unity PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- CRC ---
add_executable(test_crc
    test_crc.c
    ${CMAKE_SOURCE_DIR}/common/crc.c
)
target_include_directories(test_crc PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(test_crc PRIVATE unity)
add_test(NAME crc COMMAND test_crc)

# --- Протокол (чистый фрейминг) ---
add_executable(test_protocol
    test_protocol.c
    ${CMAKE_SOURCE_DIR}/common/protocol.c
)
target_include_directories(test_protocol PRIVATE ${CMAKE_SOURCE_DIR}/common)
target_link_libraries(test_protocol PRIVATE unity)
add_test(NAME protocol COMMAND test_protocol)

# --- Метаданные (с моками Flash и I2C) ---
add_executable(test_meta
    test_meta.c
    ${CMAKE_SOURCE_DIR}/common/meta.c
    ${CMAKE_SOURCE_DIR}/common/crc.c
    mocks/flash_mock.c
    mocks/i2c_mock.c
)
target_include_directories(test_meta PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_SOURCE_DIR}/common
)
target_link_libraries(test_meta PRIVATE unity)
add_test(NAME meta COMMAND test_meta)

# `check` собирает тесты и запускает CTest (в отличие от встроенной
# цели `test`, которая только запускает ctest и не собирает цели).
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_crc test_protocol test_meta
    COMMENT "Сборка и запуск модульных тестов (host)"
    USES_TERMINAL
)
