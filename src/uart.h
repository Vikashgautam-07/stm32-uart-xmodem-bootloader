#ifndef UART_H
#define UART_H

void uart1_init(void);
void uart1_putc(char character);
char uart1_getc(void);
int uart1_try_getc(char *character);
void uart1_puts(const char* text);
void uart1_disable(void);

#endif 