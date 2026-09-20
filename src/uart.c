#include <stdint.h>
#include "uart.h"

#define USART1_BASE      0x40013800UL
#define RCC_BASE         0x40021000UL
#define GPIOA_BASE       0x40010800UL

#define RCC_APB2ENR    (*(volatile uint32_t *)(RCC_BASE + 0x018))
#define GPIOA_CRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x04))

#define USART1_SR      (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR      (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BR      (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1     (*(volatile uint32_t *)(USART1_BASE + 0x0c))
#define USART1_CR2     (*(volatile uint32_t *)(USART1_BASE + 0x10))
#define USART1_CR3     (*(volatile uint32_t *)(USART1_BASE + 0x14))


#define RCC_IOPAEN      (1UL << 2)
#define RCC_USARTEN     (1UL << 14)

#define USART_SR_RXNE   (1UL << 5)
#define USART_SR_TXE    (1UL << 7)

#define USART_CR1_RE    (1UL << 2)
#define USART_CR1_TE    (1UL << 3)
#define USART_CR1_UE    (1UL << 13)

void uart1_init(void)
{
    RCC_APB2ENR |= RCC_IOPAEN | RCC_USARTEN;

    GPIOA_CRH &= ~((0xFUL << 4) | (0xFUL << 8));
    GPIOA_CRH |=  ((0xBUL << 4) | (0x4UL << 8));

    USART1_BR = 0x45;

    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart1_putc(char character)
{
    while ((USART1_SR & USART_SR_TXE) == 0)
    {
    }
    USART1_DR = (uint32_t)character;
}

char uart1_getc(void)
{
    while ((USART1_SR & USART_SR_RXNE) == 0)
    {
    }

    return (char)(USART1_DR & 0xFFU);
}
void uart1_puts(const char *text)
{
    while (*text != '\0')
    {
        uart1_putc(*text);
        text++;
    }
}