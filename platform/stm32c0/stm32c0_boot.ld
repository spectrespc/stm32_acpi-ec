/* stm32c031_boot.ld
 *
 * STM32C0 (C011F6: 32 KB Flash, 6 KB SRAM; C031F6: 32 KB Flash, 12 KB SRAM),
 * страница Flash 2 KB. RAM_LENGTH/ECFLAG_ADDR задаются CMake по MCU.
 * Загрузчик занимает первые 4 страницы (8 KB) @ 0x08000000.
 */

MEMORY
{
    FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = FLASH_LEN

    /* Верхние 16 байт RAM (0x20002FF0..0x20002FFF) зарезервированы
     * под флаг входа в режим обновления и НЕ входят ни в стек, ни в
     * кучу. Символ __ec_update_flag лежит в последних 4 байтах. */
    RAM     (rwx) : ORIGIN = 0x20000000, LENGTH = RAM_LENGTH
    ECFLAG  (rw)  : ORIGIN = ECFLAG_ADDR, LENGTH = 4
}

PROVIDE(__ec_update_flag = ORIGIN(ECFLAG));

_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)

        KEEP(*(.init))
        KEEP(*(.fini))

        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + 0x200;
        . = ALIGN(8);
    } > RAM

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 :
    {
        *(.ARM.attributes)
    }
}