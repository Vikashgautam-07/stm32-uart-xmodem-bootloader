/*
 * Minimal bare-metal firmware for STM32F103C8T6.
 * Blinks the onboard PC13 LED using direct register access (no HAL).
 *
 * Note: on many Blue Pill clones PC13 is active-LOW (LED on when pin is 0).
 * Your board's photo doesn't show a dedicated LED pin labeled, so if you
 * don't see anything blink, it may not have one wired to PC13 -- but the
 * important part of this exercise is confirming the register writes/timing
 * actually execute, which we can verify via GDB even without a visible LED.
 */

#include <stdint.h>

#define DELAY 400000
/* ---- Peripheral base addresses (from the STM32F103 reference manual) ---- */
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
    /* 1. Enable the clock to GPIO port C (peripherals are clock-gated off by default) */
    RCC_APB2ENR |= RCC_APB2ENR_IOPCEN;

    /* 2. Configure PC13 as general purpose output, push-pull, 2MHz
     *    CRH controls pins 8-15; each pin has 4 config bits (CNF+MODE).
     *    PC13 occupies bits [23:20] of CRH.
     *    MODE = 10 (output, 2MHz), CNF = 00 (push-pull) -> value 0b0010 = 0x2
     */
    GPIOC_CRH &= ~(0xFUL << 20);   /* clear PC13 config bits */
    GPIOC_CRH |=  (0x2UL << 20);   /* set mode=output 2MHz, cnf=push-pull */

    while (1) {
        GPIOC_BSRR = (1UL << 13);  /* set PC13 */
        delay(DELAY);
        GPIOC_BSRR = (1UL << (13 + 16));  /* reset PC13 */
        delay(DELAY);
    }
}
