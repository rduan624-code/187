/* ICD 在线升级报文：只负责读字段、检查格式、计算累加和、组装应答。
 * 本文件不操作网卡、DDR 地址或 Flash 寄存器。
 */
#include "icd_upgrade.h"
#include <string.h>
/* 读取大端 16 位整数。例如 00 1D：0x00 放高位，0x1D 放低位，结果为 29。 */
uint16_t icd_read_u16_be(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}
/* 读取大端 32 位整数。每个字节先转为 uint32_t，避免移位时符号扩展。 */
uint32_t icd_read_u32_be(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) |
           bytes[3];
}
/* 写入大端 16 位整数：高字节写前面，低字节写后面。 */
void icd_write_u16_be(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}
/* 写入大端 32 位整数。下面四行分别写四个字节。 */
void icd_write_u32_be(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24);
    bytes[1] = (uint8_t)(value >> 16);
    bytes[2] = (uint8_t)(value >> 8);
    bytes[3] = (uint8_t)value;
}
/* 输入：一份 UDP 应用报文。输出：解析后的 frame。
 * 返回 0 表示合法，-1 表示丢弃；payload 指向原始报文，不是新分配的内存。 */
int icd_parse_update(icd_update_frame_t *frame, const uint8_t *data, size_t length)
{
    /* 1. 先检查指针和实际长度，再访问 data[]，防止越界。 */
    if (frame == NULL || data == NULL)
    {
        return -1;
    }
    if (length < 32u || length > UP_MAX_PACKET)
    {
        return -1;
    }
    /* 2. 长度字段必须等于收到的字节数，不允许附加 CRC 尾巴。 */
    if (icd_read_u16_be(data + 16) != length)
    {
        return -1;
    }
    /* 3. 只接受 F0/0008，保留字段必须为零，毫秒范围为 0～999。 */
    if (data[14] != 0 || data[15] != 0xF0 || icd_read_u16_be(data + 18) != 0x0008)
    {
        return -1;
    }
    if (icd_read_u32_be(data + 10) > 999u)
    {
        return -1;
    }
    /* 4. 按协议字节偏移逐个读取字段，不直接把字节数组强转为结构体。 */
    memset(frame, 0, sizeof(*frame));
    frame->source = icd_read_u16_be(data);
    frame->destination = icd_read_u16_be(data + 2);
    frame->serial = icd_read_u16_be(data + 4);
    frame->module = data[20];
    frame->processor = data[21];
    frame->configuration = icd_read_u16_be(data + 22);
    frame->program = icd_read_u16_be(data + 24);
    frame->end = data[26];
    frame->total = icd_read_u16_be(data + 27);
    frame->sequence = icd_read_u16_be(data + 29);
    frame->payload = data + 31;
    frame->payload_size = length - 31;
    if (!frame->source || !frame->destination || !frame->sequence || frame->total < 2 ||
        frame->sequence > frame->total || frame->end > 1 ||
        (frame->end == 0 && frame->sequence != frame->total) ||
        (frame->end == 1 && frame->sequence == frame->total))
    {
        return -1;
    }
    return 0;
}
/* BCD 的一个字节存两个十进制数字：两个半字节都不能大于 9。 */
static int is_valid_bcd(uint8_t value)
{
    return (value >> 4) <= 9 && (value & 15) <= 9;
}
/* 例如 0x26：高半字节为 2，低半字节为 6，得到十进制 26。 */
static unsigned bcd_to_decimal(uint8_t value)
{
    return (value >> 4) * 10u + (value & 15);
}
/* 检查摘要内 7 字节日期：世纪、年、月、日、时、分、秒，全部为 BCD。 */
static int is_valid_date(const uint8_t *data)
{
    unsigned index, year, month, day, maxday;
    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (index = 0; index < 7; index++)
    {
        if (!is_valid_bcd(data[index]))
        {
            return 0;
        }
    }
    if (data[0] != 0x20)
    {
        return 0;
    }
    year = 2000 + bcd_to_decimal(data[1]);
    month = bcd_to_decimal(data[2]);
    day = bcd_to_decimal(data[3]);
    if (!month || month > 12 || !day || bcd_to_decimal(data[4]) > 23 ||
        bcd_to_decimal(data[5]) > 59 || bcd_to_decimal(data[6]) > 59)
    {
        return 0;
    }
    maxday = days[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
    {
        maxday++;
    }
    return day <= maxday;
}
/* 首帧只包含 60 字节摘要。检查版本/日期后保存摘要及文件长度。 */
int icd_parse_summary(icd_summary_t *summary, const uint8_t *data, size_t length)
{
    if (!summary || !data || length != UP_METADATA_SIZE || data[6] > 9 || !is_valid_bcd(data[7]) ||
        data[8] || data[23] || !is_valid_date(data + 9) || !is_valid_date(data + 16) ||
        !icd_read_u32_be(data + 56))
    {
        return -1;
    }
    memcpy(summary->raw, data, length);
    summary->checksum = icd_read_u32_be(data);
    summary->program = icd_read_u16_be(data + 4);
    summary->image_size = icd_read_u32_be(data + 56);
    return 0;
}
/* 逐字节累加。使用 64 位临时变量，调用方再检查能否放进 32 位校验字。 */
uint64_t icd_checksum_add(uint64_t value, const uint8_t *data, size_t length)
{
    size_t index;
    for (index = 0; index < length; index++)
    {
        value += data[index];
    }
    return value;
}
/* 生成完整应答：0088 为 29 字节，0089/008A 为 28 字节。
 * result 是结果；detail 在 0088 中是帧号，在 0089 中是故障码，在 008A 中是进度。 */
size_t icd_build_report(uint8_t out[UP_ACK_SIZE], const icd_update_frame_t *request,
                        uint16_t serial, uint32_t seconds, uint32_t milliseconds,
                        uint16_t message_id, uint8_t result, uint16_t detail)
{
    size_t length;
    if (message_id == 0x88)
    {
        length = UP_ACK_SIZE;
    }
    else
    {
        length = UP_RESULT_SIZE;
    }
    if ((message_id != 0x88 && message_id != 0x89 && message_id != 0x8a) || milliseconds > 999)
    {
        return 0;
    }
    memset(out, 0, UP_ACK_SIZE);
    icd_write_u16_be(out, request->destination);
    icd_write_u16_be(out + 2, request->source);
    icd_write_u16_be(out + 4, serial);
    icd_write_u32_be(out + 6, seconds);
    icd_write_u32_be(out + 10, milliseconds);
    out[15] = 0xf3;
    icd_write_u16_be(out + 16, (uint16_t)length);
    icd_write_u16_be(out + 18, message_id);
    out[20] = request->module;
    out[21] = request->processor;
    icd_write_u16_be(out + 22, request->configuration);
    icd_write_u16_be(out + 24, request->program);
    out[26] = result;
    if (message_id == 0x88)
    {
        icd_write_u16_be(out + 27, detail);
    }
    else
    {
        out[27] = (uint8_t)detail;
    }
    return length;
}
