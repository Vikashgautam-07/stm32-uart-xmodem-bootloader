#include <stdint.h>
#include "uart.h"
#include "xmodem.h"

#define XMODEM_SOH             0x01U
#define XMODEM_EOT             0x04U
#define XMODEM_ACK             0x06U
#define XMODEM_NAK             0x15U
#define XMODEM_CAN             0x18U
#define XMODEM_PACKET_SIZE     128U
#define XMODEM_MAX_RETRIES     60U
#define XMODEM_BYTE_TIMEOUT_MS 1000UL

static int read_byte(uint8_t *value, uint32_t timeout_ms)
{
    char character;
    if (!uart1_getc_timeout(&character, timeout_ms))
    {
        return 0;
    }

    *value = (uint8_t)character;
    return 1;
}

static int read_packet(uint8_t *packet_number,
                       uint8_t *packet_complement,
                       uint8_t payload[XMODEM_PACKET_SIZE],
                       uint8_t *checksum)
{
    if (!read_byte(packet_number, XMODEM_BYTE_TIMEOUT_MS) ||
        !read_byte(packet_complement, XMODEM_BYTE_TIMEOUT_MS))
    {
        return 0;
    }

    for (uint32_t index = 0; index < XMODEM_PACKET_SIZE; index++)
    {
        if (!read_byte(&payload[index], XMODEM_BYTE_TIMEOUT_MS))
        {
            return 0;
        }
    }

    return read_byte(checksum, XMODEM_BYTE_TIMEOUT_MS);
}

static void cancel_transfer(void)
{
    uart1_putc((char)XMODEM_CAN);
    uart1_putc((char)XMODEM_CAN);
}

int xmodem_receive(xmodem_packet_writer_t write_packet,
                   uint32_t capacity,
                   uint32_t *received_size)
{
    uint8_t expected_packet = 1U;
    uint32_t retries = 0;
    uint8_t payload[XMODEM_PACKET_SIZE];

    if (write_packet == 0 || received_size == 0 ||
        capacity < XMODEM_PACKET_SIZE)
    {
        return -1;
    }

    *received_size = 0;
    uart1_putc((char)XMODEM_NAK);

    while (retries < XMODEM_MAX_RETRIES)
    {
        uint8_t marker;
        if (!read_byte(&marker, XMODEM_BYTE_TIMEOUT_MS))
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        if (marker == XMODEM_EOT)
        {
            uart1_putc((char)XMODEM_ACK);
            return 0;
        }

        if (marker == XMODEM_CAN)
        {
            uint8_t second_cancel;
            if (read_byte(&second_cancel, 100UL) &&
                second_cancel == XMODEM_CAN)
            {
                uart1_putc((char)XMODEM_ACK);
                return -2;
            }
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        if (marker != XMODEM_SOH)
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        uint8_t packet_number;
        uint8_t packet_complement;
        uint8_t checksum;
        if (!read_packet(&packet_number, &packet_complement,
                         payload, &checksum))
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        uint8_t calculated_checksum = 0;
        for (uint32_t index = 0; index < XMODEM_PACKET_SIZE; index++)
        {
            calculated_checksum =
                (uint8_t)(calculated_checksum + payload[index]);
        }

        if ((uint8_t)(packet_number + packet_complement) != 0xFFU ||
            calculated_checksum != checksum)
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        if (packet_number == (uint8_t)(expected_packet - 1U))
        {
            uart1_putc((char)XMODEM_ACK);
            retries = 0;
            continue;
        }

        if (packet_number != expected_packet ||
            *received_size > capacity - XMODEM_PACKET_SIZE)
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        if (write_packet(*received_size, payload, XMODEM_PACKET_SIZE) != 0)
        {
            cancel_transfer();
            return -3;
        }

        *received_size += XMODEM_PACKET_SIZE;
        expected_packet++;
        retries = 0;
        uart1_putc((char)XMODEM_ACK);
    }

    cancel_transfer();
    return -4;
}