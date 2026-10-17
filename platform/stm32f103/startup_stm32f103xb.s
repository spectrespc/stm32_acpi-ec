/* startup_stm32f103xb.s */

.syntax unified
.cpu cortex-m3
.thumb

.global g_pfnVectors
.global Default_Handler
.global Reset_Handler

/*
 * Vector table for STM32F103 medium density
 */
.section .isr_vector, "a", %progbits
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0
    .word 0
    .word 0
    .word 0
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0
    .word PendSV_Handler
    .word SysTick_Handler

    /* External interrupts */
    .word WWDG_IRQHandler
    .word PVD_IRQHandler
    .word TAMPER_IRQHandler
    .word RTC_IRQHandler
    .word FLASH_IRQHandler
    .word RCC_IRQHandler
    .word EXTI0_IRQHandler
    .word EXTI1_IRQHandler
    .word EXTI2_IRQHandler
    .word EXTI3_IRQHandler
    .word EXTI4_IRQHandler
    .word DMA1_Channel1_IRQHandler
    .word DMA1_Channel2_IRQHandler
    .word DMA1_Channel3_IRQHandler
    .word DMA1_Channel4_IRQHandler
    .word DMA1_Channel5_IRQHandler
    .word DMA1_Channel6_IRQHandler
    .word DMA1_Channel7_IRQHandler
    .word ADC1_2_IRQHandler
    .word USB_HP_CAN1_TX_IRQHandler
    .word USB_LP_CAN1_RX0_IRQHandler
    .word CAN1_RX1_IRQHandler
    .word CAN1_SCE_IRQHandler
    .word EXTI9_5_IRQHandler
    .word TIM1_BRK_IRQHandler
    .word TIM1_UP_IRQHandler
    .word TIM1_TRG_COM_IRQHandler
    .word TIM1_CC_IRQHandler
    .word TIM2_IRQHandler
    .word TIM3_IRQHandler
    .word TIM4_IRQHandler
    .word I2C1_EV_IRQHandler
    .word I2C1_ER_IRQHandler
    .word I2C2_EV_IRQHandler
    .word I2C2_ER_IRQHandler
    .word SPI1_IRQHandler
    .word SPI2_IRQHandler
    .word USART1_IRQHandler
    .word USART2_IRQHandler
    .word USART3_IRQHandler
    .word EXTI15_10_IRQHandler
    .word RTC_Alarm_IRQHandler
    .word USBWakeUp_IRQHandler

/*
 * Reset handler
 */
.section .text.Reset_Handler
.thumb_func
Reset_Handler:
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b .Lcopy_check

.Lcopy_loop:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

.Lcopy_check:
    adds r4, r0, r3
    cmp r4, r1
    bcc .Lcopy_loop

    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b .Lzero_check

.Lzero_loop:
    str r2, [r0]
    adds r0, r0, #4

.Lzero_check:
    cmp r0, r1
    bcc .Lzero_loop

    bl main
    b .

/*
 * Default handler
 */
.section .text.Default_Handler, "ax", %progbits
.thumb_func
Default_Handler:
    b .

/*
 * Weak aliases
 */
.macro def_irq_handler handler_name
    .weak \handler_name
    .thumb_set \handler_name, Default_Handler
.endm

def_irq_handler NMI_Handler
def_irq_handler HardFault_Handler
def_irq_handler MemManage_Handler
def_irq_handler BusFault_Handler
def_irq_handler UsageFault_Handler
def_irq_handler SVC_Handler
def_irq_handler DebugMon_Handler
def_irq_handler PendSV_Handler
def_irq_handler SysTick_Handler

def_irq_handler WWDG_IRQHandler
def_irq_handler PVD_IRQHandler
def_irq_handler TAMPER_IRQHandler
def_irq_handler RTC_IRQHandler
def_irq_handler FLASH_IRQHandler
def_irq_handler RCC_IRQHandler
def_irq_handler EXTI0_IRQHandler
def_irq_handler EXTI1_IRQHandler
def_irq_handler EXTI2_IRQHandler
def_irq_handler EXTI3_IRQHandler
def_irq_handler EXTI4_IRQHandler
def_irq_handler DMA1_Channel1_IRQHandler
def_irq_handler DMA1_Channel2_IRQHandler
def_irq_handler DMA1_Channel3_IRQHandler
def_irq_handler DMA1_Channel4_IRQHandler
def_irq_handler DMA1_Channel5_IRQHandler
def_irq_handler DMA1_Channel6_IRQHandler
def_irq_handler DMA1_Channel7_IRQHandler
def_irq_handler ADC1_2_IRQHandler
def_irq_handler USB_HP_CAN1_TX_IRQHandler
def_irq_handler USB_LP_CAN1_RX0_IRQHandler
def_irq_handler CAN1_RX1_IRQHandler
def_irq_handler CAN1_SCE_IRQHandler
def_irq_handler EXTI9_5_IRQHandler
def_irq_handler TIM1_BRK_IRQHandler
def_irq_handler TIM1_UP_IRQHandler
def_irq_handler TIM1_TRG_COM_IRQHandler
def_irq_handler TIM1_CC_IRQHandler
def_irq_handler TIM2_IRQHandler
def_irq_handler TIM3_IRQHandler
def_irq_handler TIM4_IRQHandler
def_irq_handler I2C1_EV_IRQHandler
def_irq_handler I2C1_ER_IRQHandler
def_irq_handler I2C2_EV_IRQHandler
def_irq_handler I2C2_ER_IRQHandler
def_irq_handler SPI1_IRQHandler
def_irq_handler SPI2_IRQHandler
def_irq_handler USART1_IRQHandler
def_irq_handler USART2_IRQHandler
def_irq_handler USART3_IRQHandler
def_irq_handler EXTI15_10_IRQHandler
def_irq_handler RTC_Alarm_IRQHandler
def_irq_handler USBWakeUp_IRQHandler
