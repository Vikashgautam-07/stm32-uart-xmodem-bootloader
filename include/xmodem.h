#ifndef XMODEM_H
#define XMODEM_H

#include <stdint.h>

typedef int (*xmodem_packet_writer_t)(uint32_t offset,
                                      const uint8_t *data,
                                      uint32_t length);

int xmodem_receive(xmodem_packet_writer_t write_packet,
                   uint32_t capacity,
                   uint32_t *received_size);

#endif