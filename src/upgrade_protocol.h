#ifndef UPGRADE_PROTOCOL_H
#define UPGRADE_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#define UP_HEADER_SIZE 20u
#define UP_METADATA_SIZE 60u
#define UP_MAX_PAYLOAD 1024u
#define UP_MAX_PACKET (31u + UP_MAX_PAYLOAD)
#define UP_ACK_SIZE 29u
#define UP_RESULT_SIZE 28u
#define UP_PROCESSOR_A 0x71u

typedef struct {
    uint16_t source, destination, serial;
    uint8_t module, processor, end;
    uint16_t configuration, program, total, sequence;
    const uint8_t *payload;
    size_t payload_size;
} up_packet;
typedef struct {
    uint32_t checksum, image_size;
    uint16_t program;
    uint8_t raw[UP_METADATA_SIZE];
} up_metadata;
uint16_t up_be16(const uint8_t *p);
uint32_t up_be32(const uint8_t *p);
void up_put16(uint8_t *p, uint16_t value);
void up_put32(uint8_t *p, uint32_t value);
/* Returns zero only for a complete, structurally valid F0/0008 datagram. */
int up_decode(up_packet *p, const uint8_t *data, size_t length);
int up_decode_metadata(up_metadata *m, const uint8_t *data, size_t length);
/* Exact byte sum, not CRC; caller decides whether it fits the 32-bit wire field. */
uint64_t up_sum(uint64_t initial, const uint8_t *data, size_t length);
size_t up_report(uint8_t out[UP_ACK_SIZE], const up_packet *request,
                 uint16_t serial, uint32_t seconds, uint32_t milliseconds,
                 uint16_t id, uint8_t result, uint16_t detail);
#endif
