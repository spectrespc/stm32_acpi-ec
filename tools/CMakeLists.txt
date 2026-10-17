# tools/CMakeLists.txt
#
# Хостовые утилиты. Собираются только нативным компилятором
# (защита от кросс-сборки: инструменты не должны попасть в toolchain).

if(CMAKE_CROSSCOMPILING)
    message(STATUS "tools/: пропущено при кросс-сборке")
    return()
endif()

add_subdirectory(bootloader_tool)