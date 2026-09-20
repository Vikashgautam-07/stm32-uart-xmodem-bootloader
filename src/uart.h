#ifndef UART_H
#define UART_H

void uart1_init(void);
void uart1_putc(char character);
char uart1_getc(void);
void uart1_puts(const char* text);

#endif 