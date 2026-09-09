/*
 * Startup file for STM32F103C8T6 (Cortex-M3)
 * Defines the vector table and the Reset_Handler that runs first.
 */

.syntax unified
.cpu cortex-m3
.thumb

/* Symbols provided by the linker script */
.word _estack
.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

/* ===================== Vector Table ===================== */
.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack              /* Initial stack pointer (top of RAM) */
    .word Reset_Handler        /* Reset handler: entry point after reset */
    .word NMI_Handler
    .word HardFault_Handler
    .word MemManage_Handler
    .word BusFault_Handler
    .word UsageFault_Handler
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word 0                    /* Reserved */
    .word SVC_Handler
    .word DebugMon_Handler
    .word 0                    /* Reserved */
    .word PendSV_Handler
    .word SysTick_Handler
    /* Peripheral interrupts would follow here — omitted for this minimal bring-up */

.size g_pfnVectors, .-g_pfnVectors

/* ===================== Reset Handler ===================== */
.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    /* Copy .data section from flash to RAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
copy_data_loop:
    cmp r1, r2
    bge copy_data_done
    ldr r3, [r0], #4
    str r3, [r1], #4
    b copy_data_loop
copy_data_done:

    /* Zero out .bss section */
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
zero_bss_loop:
    cmp r1, r2
    bge zero_bss_done
    str r3, [r1], #4
    b zero_bss_loop
zero_bss_done:

    /* Jump to main() */
    bl main

    /* main() should never return, but trap here if it does */
infinite_loop:
    b infinite_loop
.size Reset_Handler, .-Reset_Handler

/* ===================== Default Handlers ===================== */
/* Any interrupt not explicitly defined falls through to an infinite loop,
   so a fault is obvious (stuck) rather than silently jumping to garbage. */
.section .text.Default_Handler, "ax", %progbits
Default_Handler:
    b Default_Handler

.macro def_irq_handler handler_name
    .weak \handler_name
    .set \handler_name, Default_Handler
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
