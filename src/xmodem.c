#include <stdint.h>
#include "uart.h"
#include "xmodem.h"

#define XMODEM_SOH             0x01U
#define XMODEM_STX             0x02U
#define XMODEM_EOT             0x04U
#define XMODEM_ACK             0x06U
#define XMODEM_NAK             0x15U
#define XMODEM_CAN             0x18U
#define XMODEM_C               0x43U
#define XMODEM_BLOCK_SIZE      128U
#define XMODEM_1K_BLOCK_SIZE   1024U
#define XMODEM_MAX_RETRIES     60U
#define XMODEM_BYTE_TIMEOUT_MS 10000UL

static uint16_t crc16_xmodem(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0U;

    for (uint32_t index = 0; index < length; index++)
    {
        crc ^= (uint16_t)data[index] << 8U;
        for (uint32_t bit = 0; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else
            {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

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
                       uint8_t *payload,
                       uint32_t payload_size,
                       uint8_t *checksum,
                       uint16_t *crc_value,
                       int use_crc)
{
    if (!read_byte(packet_number, XMODEM_BYTE_TIMEOUT_MS))
    {
        return 0;
    }

    if (!read_byte(packet_complement, XMODEM_BYTE_TIMEOUT_MS))
    {
        return 0;
    }

    for (uint32_t index = 0; index < payload_size; index++)
    {
        if (!read_byte(&payload[index], XMODEM_BYTE_TIMEOUT_MS))
        {
            return 0;
        }
    }

    if (use_crc)
    {
        uint8_t crc_high;
        uint8_t crc_low;
        if (!read_byte(&crc_high, XMODEM_BYTE_TIMEOUT_MS))
        {
            return 0;
        }

        if (!read_byte(&crc_low, XMODEM_BYTE_TIMEOUT_MS))
        {
            return 0;
        }

        *crc_value = ((uint16_t)crc_high << 8U) | crc_low;
        *checksum = 0U;
        return 1;
    }

    *crc_value = 0U;
    if (!read_byte(checksum, XMODEM_BYTE_TIMEOUT_MS))
    {
        return 0;
    }

    return 1;
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
    uint8_t payload[XMODEM_1K_BLOCK_SIZE];
    int use_crc = 1;

    if (write_packet == 0 || received_size == 0 ||
        capacity < XMODEM_BLOCK_SIZE)
    {
        return -1;
    }

    *received_size = 0;
    uart1_putc((char)XMODEM_C);

    while (retries < XMODEM_MAX_RETRIES)
    {
        uint8_t marker;
        if (!read_byte(&marker, XMODEM_BYTE_TIMEOUT_MS))
        {
            retries++;
            uart1_putc((char)(use_crc ? XMODEM_C : XMODEM_NAK));
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
            uart1_putc((char)(use_crc ? XMODEM_C : XMODEM_NAK));
            continue;
        }

        if (marker == XMODEM_C)
        {
            use_crc = 1;
            retries = 0;
            continue;
        }

        if (marker == XMODEM_NAK)
        {
            use_crc = 0;
            retries = 0;
            continue;
        }

        uint32_t packet_size;
        if (marker == XMODEM_SOH)
        {
            packet_size = XMODEM_BLOCK_SIZE;
        }
        else if (marker == XMODEM_STX)
        {
            packet_size = XMODEM_1K_BLOCK_SIZE;
        }
        else
        {
            retries++;
            uart1_putc((char)(use_crc ? XMODEM_C : XMODEM_NAK));
            continue;
        }

        uint8_t packet_number;
        uint8_t packet_complement;
        uint8_t checksum = 0U;
        uint16_t packet_crc = 0U;
        if (!read_packet(&packet_number, &packet_complement,
                         payload, packet_size, &checksum, &packet_crc,
                         use_crc))
        {
            retries++;
            uart1_putc((char)XMODEM_NAK);
            continue;
        }

        uint8_t calculated_checksum = 0U;
        for (uint32_t index = 0; index < packet_size; index++)
        {
            calculated_checksum =
                (uint8_t)(calculated_checksum + payload[index]);
        }

        uint16_t calculated_crc = crc16_xmodem(payload, packet_size);
        if ((uint8_t)(packet_number + packet_complement) != 0xFFU)
        {
            retries++;
            uart1_putc((char)(use_crc ? XMODEM_C : XMODEM_NAK));
            continue;
        }

        if (use_crc)
        {
            if (calculated_crc != packet_crc)
            {
                retries++;
                uart1_putc((char)XMODEM_NAK);
                continue;
            }
        }
        else
        {
            if (calculated_checksum != checksum)
            {
                retries++;
                uart1_putc((char)XMODEM_NAK);
                continue;
            }
        }

        if (packet_number == (uint8_t)(expected_packet - 1U))
        {
            uart1_putc((char)XMODEM_ACK);
            retries = 0;
            continue;
        }

        if (packet_number != expected_packet ||
            packet_size > capacity ||
            *received_size > capacity - packet_size)
        {
            retries++;
            uart1_putc((char)(use_crc ? XMODEM_C : XMODEM_NAK));
            continue;
        }

        if (write_packet(*received_size, payload, packet_size) != 0)
        {
            cancel_transfer();
            return -3;
        }

        *received_size += packet_size;
        expected_packet++;
        retries = 0;
        uart1_putc((char)XMODEM_ACK);
    }

    cancel_transfer();
    return -4;
}