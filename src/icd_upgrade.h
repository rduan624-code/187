#ifndef ICD_UPGRADE_H
#define ICD_UPGRADE_H
#include <stddef.h>
#include <stdint.h>
#define UP_HEADER_SIZE 20u
#define UP_METADATA_SIZE 60u
#define UP_MAX_PAYLOAD 1024u
#define UP_MAX_PACKET (31u + UP_MAX_PAYLOAD)
#define UP_ACK_SIZE 29u
#define UP_RESULT_SIZE 28u
#define UP_PROCESSOR_A 0x71u

typedef struct
{
    uint16_t source;        /* 源 UUID */
    uint16_t destination;   /* 目的 UUID */
    uint16_t serial;        /* 报文流水号，不是文件帧号 */
    uint8_t module;         /* 模块 MARK */
    uint8_t processor;      /* 0x71/72/73/74 对应 A/B/C/D */
    uint8_t end;            /* 0=结束帧，1=中间帧 */
    uint16_t configuration; /* 配置项编号 */
    uint16_t program;       /* 程序文件编号 */
    uint16_t total;         /* 总帧数，包含摘要帧 */
    uint16_t sequence;      /* 文件帧号，从 1 开始 */
    const uint8_t *payload; /* 指向载荷，不能在原报文释放后继续使用 */
    size_t payload_size;
} icd_update_frame_t;
typedef struct
{
    uint32_t checksum;   /* 首四字节传来的期望累加和 */
    uint32_t image_size; /* 纯程序文件长度，不包括摘要 */
    uint16_t program;
    uint8_t raw[UP_METADATA_SIZE];
} icd_summary_t;
uint16_t icd_read_u16_be(const uint8_t *p);
uint32_t icd_read_u32_be(const uint8_t *p);
void icd_write_u16_be(uint8_t *p, uint16_t value);
void icd_write_u32_be(uint8_t *p, uint32_t value);
/* 解析成功返回 0；字段/长度不符合 F0/0008 格式时返回 -1。 */
int icd_parse_update(icd_update_frame_t *p, const uint8_t *data, size_t length);
int icd_parse_summary(icd_summary_t *m, const uint8_t *data, size_t length);
/* 精确字节累加和。不是 CRC，调用方还要检查结果能否放入 32 位。 */
uint64_t icd_checksum_add(uint64_t initial, const uint8_t *data, size_t length);
size_t icd_build_report(uint8_t out[UP_ACK_SIZE], const icd_update_frame_t *request,
                        uint16_t serial, uint32_t seconds, uint32_t milliseconds, uint16_t id,
                        uint8_t result, uint16_t detail);
#endif
