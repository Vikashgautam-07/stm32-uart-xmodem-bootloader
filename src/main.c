#include <stdint.h>
#include "uart.h"
#include "xmodem.h"

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

/* Flash Registers */
#define FLASH_BASE        0x40022000UL
#define FLASH_KEYR        (*(volatile uint32_t *)(FLASH_BASE + 0x04))
#define FLASH_SR          (*(volatile uint32_t *)(FLASH_BASE + 0x0C))
#define FLASH_CR          (*(volatile uint32_t *)(FLASH_BASE + 0x10))
#define FLASH_AR          (*(volatile uint32_t *)(FLASH_BASE + 0x14))

#define FLASH_SR_BSY      (1UL << 0)   /* Busy */
#define FLASH_SR_PGERR    (1UL << 2)   /* Programming Error */
#define FLASH_SR_WRPRTERR (1UL << 4)   /* Write Protection Error */
#define FLASH_SR_EOP      (1UL << 5)   /* End of Operation */

#define FLASH_CR_PG       (1UL << 0)   /* Programming */
#define FLASH_CR_PER      (1UL << 1)   /* Page Erase */
#define FLASH_CR_MER      (1UL << 2)   /* Mass Erase */
#define FLASH_CR_STRT     (1UL << 6)   /* Start Programming */
#define FLASH_CR_LOCK      (1UL << 7)   /* Lock */

#define FLASH_KEY1        0x45670123UL
#define FLASH_KEY2        0xCDEF89ABUL
#define FLASH_PAGE_SIZE   1024UL
#define FLASH_WAIT_LIMIT  8000000UL

static int flash_unlock(void)
{
    if ((FLASH_CR & FLASH_CR_LOCK) == 0)
    {
        return 0;
    }

    FLASH_KEYR = FLASH_KEY1;
    FLASH_KEYR = FLASH_KEY2;
    return (FLASH_CR & FLASH_CR_LOCK) == 0 ? 0 : -1;
}

static void flash_lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

static int flash_wait_busy(void)
{
    uint32_t timeout = FLASH_WAIT_LIMIT;
    while ((FLASH_SR & FLASH_SR_BSY) && timeout > 0)
    {
        timeout--;
    }
    return (FLASH_SR & FLASH_SR_BSY) == 0 ? 0 : -1;
}

static int flash_check_errors(void)
{
    uint32_t status = FLASH_SR;

    if (status & FLASH_SR_PGERR)
    {
        return -1;
    }
    if (status & FLASH_SR_WRPRTERR)
    {
        return -2;
    }
    if ((status & FLASH_SR_EOP) == 0)
    {
        return -3;
    }
    return 0;
}

static int flash_erase_page(uint32_t page_address)
{
    if (page_address < APP_ADDRESS ||
        page_address > FLASH_END - FLASH_PAGE_SIZE ||
        (page_address % FLASH_PAGE_SIZE) != 0)
    {
        return -1;
    }

    if (flash_wait_busy() != 0 || flash_unlock() != 0)
    {
        flash_lock();
        return -2;
    }

    FLASH_SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;
    FLASH_CR = FLASH_CR_PER;
    FLASH_AR = page_address;
    FLASH_CR |= FLASH_CR_STRT;

    int result = flash_wait_busy();
    if (result == 0)
    {
        result = flash_check_errors();
    }
    FLASH_CR &= ~FLASH_CR_PER;
    flash_lock();

    if (result != 0)
    {
        return result;
    }

    const volatile uint16_t *page = (const volatile uint16_t *)page_address;
    for (uint32_t index = 0; index < FLASH_PAGE_SIZE / sizeof(uint16_t); index++)
    {
        if (page[index] != 0xFFFFU)
        {
            return -4;
        }
    }

    return 0;
}

static int flash_program_halfword(uint32_t address, uint16_t value)
{
    if (address < APP_ADDRESS || address > FLASH_END - sizeof(uint16_t) ||
        (address & 1UL) != 0)
    {
        return -1;
    }

    if (flash_wait_busy() != 0 || flash_unlock() != 0)
    {
        flash_lock();
        return -2;
    }

    FLASH_SR = FLASH_SR_PGERR | FLASH_SR_WRPRTERR | FLASH_SR_EOP;
    FLASH_CR = FLASH_CR_PG;
    *(volatile uint16_t *)address = value;

    int result = flash_wait_busy();
    if (result == 0)
    {
        result = flash_check_errors();
    }
    FLASH_CR &= ~FLASH_CR_PG;
    flash_lock();

    if (result != 0)
    {
        return result;
    }

    return *(const volatile uint16_t *)address == value ? 0 : -4;
}

static int flash_write_packet(uint32_t offset,
                              const uint8_t *data,
                              uint32_t length)
{
    uint32_t application_size = FLASH_END - APP_ADDRESS;
    uint32_t written = 0;

    if (offset > application_size || length > application_size - offset)
    {
        return -1;
    }

    while (written < length)
    {
        uint32_t address = APP_ADDRESS + offset + written;
        uint32_t page_address = address & ~(FLASH_PAGE_SIZE - 1UL);
        uint32_t page_offset = address - page_address;
        uint32_t chunk = FLASH_PAGE_SIZE - page_offset;

        if (chunk > length - written)
        {
            chunk = length - written;
        }
        if ((chunk & 1UL) != 0)
        {
            return -1;
        }

        if (page_offset == 0 && flash_erase_page(page_address) != 0)
        {
            return -2;
        }

        for (uint32_t index = 0; index < chunk; index += 2)
        {
            uint16_t value = (uint16_t)data[written + index] |
                             ((uint16_t)data[written + index + 1] << 8);
            if (flash_program_halfword(address + index, value) != 0)
            {
                return -3;
            }
        }

        written += chunk;
    }

    return 0;
}

static int wait_for_update_command(void)
{
    for (uint32_t elapsed_seconds = 0; elapsed_seconds < 5; elapsed_seconds++)
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
    uart1_puts("Bootloader Ready: send U within 5 seconds to update\r\n");

    if (!wait_for_update_command() && application_is_valid())
    {
        boot_application();
    }

    uart1_puts("Update mode\r\n");
    uart1_puts("Send XMODEM checksum transfer\r\n");
    uint32_t image_size;
    int transfer_result = xmodem_receive(flash_write_packet,
                                         FLASH_END - APP_ADDRESS,
                                         &image_size);
    if (transfer_result == 0 && image_size != 0 && application_is_valid())
    {
        uart1_puts("Update verified; starting application\r\n");
        boot_application();
    }
    uart1_puts("Update failed or application is invalid\r\n");

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
