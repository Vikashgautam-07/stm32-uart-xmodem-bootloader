#include <stdint.h>
#include "uart.h"

#define APP_ADDRESS       0x08002000UL
#define FLASH_END         0x08010000UL
#define SRAM_START        0x20000000UL
#define SRAM_END          0x20005000UL
#define SYSTICK_BASE      0xE000E010UL
#define SYSTICK_CTRL      (*(volatile uint32_t *)(SYSTICK_BASE + 0x00))
#define SYSTICK_LOAD      (*(volatile uint32_t *)(SYSTICK_BASE + 0x04))
#define SYSTICK_VALUE     (*(volatile uint32_t *)(SYSTICK_BASE + 0x08))
#define SCB_VTOR          (*(volatile uint32_t *)0xE000ED08UL)
#define SYSTICK_COUNTFLAG (1UL << 16)
#define UPDATE_COMMAND    'U'
#define HSI_CLOCK_HZ      8000000UL

/* ---- Peripheral base addresses ---- */
#define RCC_BASE        0x40021000UL
#define GPIOC_BASE      0x40011000UL

#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x10))

#define RCC_APB2ENR_IOPCEN   (1UL << 4)   /* Enable clock to GPIOC */

static int wait_for_update_command(void)
{
    SYSTICK_LOAD = HSI_CLOCK_HZ - 1UL;
    SYSTICK_VALUE = 0;
    SYSTICK_CTRL = 0x5UL;

    while ((SYSTICK_CTRL & SYSTICK_COUNTFLAG) == 0)
    {
        char character;
        if (uart1_try_getc(&character) &&
            (character == UPDATE_COMMAND || character == 'u'))
        {
            SYSTICK_CTRL = 0;
            return 1;
        }
    }

    SYSTICK_CTRL = 0;
    return 0;
}

static int application_is_valid(void)
{
    const volatile uint32_t *vectors =
        (const volatile uint32_t *)APP_ADDRESS;
    uint32_t initial_sp = vectors[0];
    uint32_t reset_handler = vectors[1];
    uint32_t reset_address = reset_handler & ~1UL;

    return initial_sp >= SRAM_START && initial_sp <= SRAM_END &&
           (initial_sp & 7UL) == 0 && (reset_handler & 1UL) != 0 &&
           reset_address >= APP_ADDRESS && reset_address < FLASH_END;
}

static void boot_application(void)
{
    const volatile uint32_t *vectors =
        (const volatile uint32_t *)APP_ADDRESS;
    uint32_t initial_sp = vectors[0];
    uint32_t reset_handler = vectors[1];

    __asm volatile("cpsid i" ::: "memory");
    SYSTICK_CTRL = 0;
    uart1_disable();
    SCB_VTOR = APP_ADDRESS;
    __asm volatile("dsb\n isb" ::: "memory");
    __asm volatile("msr msp, %0\n bx %1"
                   :
                   : "r"(initial_sp), "r"(reset_handler)
                   : "memory");
    __builtin_unreachable();
}

int main(void)
{
    /* Enable GPIOC clock. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* Configure PC13 as a 2MHz push-pull output. */
    GPIOC_CRH &= ~(0xFUL << 20);   /* clear PC13 config bits */
    GPIOC_CRH |=  (0x2UL << 20);   /* set mode=output 2MHz, cnf=push-pull */

    uart1_init();
    uart1_puts("Bootloader Ready: send U within 1 second to update\r\n");

    if (!wait_for_update_command() && application_is_valid())
    {
        boot_application();
    }

    uart1_puts("Update mode\r\n");
    while (1)
    {
        GPIOC_BSRR = (1UL << 13);
        for (volatile uint32_t delay_count = 0; delay_count < 500000UL;
             delay_count++)
        {
            __asm volatile("nop");
        }
        GPIOC_BSRR = (1UL << (13 + 16));
        for (volatile uint32_t delay_count = 0; delay_count < 500000UL;
             delay_count++)
        {
            __asm volatile("nop");
        }
    }
}
