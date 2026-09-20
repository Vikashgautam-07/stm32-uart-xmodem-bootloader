#include <stdint.h>
#include "uart.h"

#define DELAY 500000
/* ---- Peripheral base addresses ---- */
#define RCC_BASE        0x40021000UL
#define GPIOC_BASE      0x40011000UL

#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18))
#define GPIOC_CRH       (*(volatile uint32_t *)(GPIOC_BASE + 0x04))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x0C))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x10))

#define RCC_APB2ENR_IOPCEN   (1UL << 4)   /* Enable clock to GPIOC */

static void delay(volatile uint32_t count)
{
    while (count--) {
        __asm__("nop");
    }
}

int main(void)
{
    /* Enable GPIOC clock. */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* Configure PC13 as a 2MHz push-pull output. */
    GPIOC_CRH &= ~(0xFUL << 20);   /* clear PC13 config bits */
    GPIOC_CRH |=  (0x2UL << 20);   /* set mode=output 2MHz, cnf=push-pull */

    uart1_init();
    uart1_puts("Bootloader Ready\r\n");

    while (1) {
        char received = uart1_getc();
        uart1_putc(received);
        GPIOC_BSRR = (1UL << 13);  /* set PC13 */
        delay(DELAY);
        GPIOC_BSRR = (1UL << (13 + 16));  /* reset PC13 */
        delay(DELAY);
    }
}
