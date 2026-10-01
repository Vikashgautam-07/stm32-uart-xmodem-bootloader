#include <stdint.h>
#include "uart.h"

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018UL)
#define GPIOC_CRH   (*(volatile uint32_t *)0x40011004UL)
#define GPIOC_BSRR  (*(volatile uint32_t *)0x40011010UL)
#define RCC_IOPCEN  (1UL << 4)

static void delay(volatile uint32_t count)
{
    while (count--)
    {
        __asm volatile("nop");
    }
}

int main(void)
{
    RCC_APB2ENR |= RCC_IOPCEN;
    GPIOC_CRH = (GPIOC_CRH & ~(0xFUL << 20)) | (0x2UL << 20);

    uart1_init();
    uart1_puts("Application started at 0x08002000\r\n");

    while (1)
    {
        GPIOC_BSRR = 1UL << (13 + 16);
        delay(500000UL);
        GPIOC_BSRR = 1UL << 13;
        delay(500000UL);
    }
}
