/* stm32c031_app.ld
 *
 * STM32C0: приложение 20 KB @ 0x08002000 (страницы 4..13).
 * Метаданные ping-pong — две последние страницы по 2 KB:
 * 0x08007000 (Meta 0) и 0x08007800 (Meta 1).
 */

MEMORY
{
    FLASH   (rx)  : ORIGIN = FLASH_ORIGIN, LENGTH = FLASH_LENGTH

    /* Верхние 16 байт RAM зарезервированы под флаг входа в OTA
     * (__ec_update_flag = 0x20002FFC) — см. скрипт загрузчика. */
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
        _flash_start = .;
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
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
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    /DISCARD/ :
    {
        libc.a(*)
        libm.a(*)
        libgcc.a(*)
    }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}