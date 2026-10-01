#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart1_init(void);
void uart1_putc(char character);
char uart1_getc(void);
int uart1_try_getc(char *character);
int uart1_getc_timeout(char *character, uint32_t timeout_ms);
void uart1_puts(const char* text);
void uart1_disable(void);

#endif 