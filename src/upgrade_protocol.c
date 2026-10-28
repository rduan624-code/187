#include "upgrade_protocol.h"
#include <string.h>
uint16_t up_be16(const uint8_t *p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
uint32_t up_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
void up_put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
void up_put32(uint8_t *p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v;
}
int up_decode(up_packet *p, const uint8_t *d, size_t n) {
    if (!p || !d || n < 32u || n > UP_MAX_PACKET || up_be16(d+16)!=n ||
        d[14] || d[15]!=0xf0 || up_be16(d+18)!=8 || up_be32(d+10)>999u)
        return -1;
    memset(p,0,sizeof(*p));
    p->source=up_be16(d); p->destination=up_be16(d+2); p->serial=up_be16(d+4);
    p->module=d[20]; p->processor=d[21]; p->configuration=up_be16(d+22);
    p->program=up_be16(d+24); p->end=d[26]; p->total=up_be16(d+27);
    p->sequence=up_be16(d+29); p->payload=d+31; p->payload_size=n-31;
    if (!p->source || !p->destination || !p->sequence || p->total<2 ||
        p->sequence>p->total || p->end>1 ||
        (p->end==0)!=(p->sequence==p->total)) return -1;
    return 0;
}
static int bcd(uint8_t v) { return (v>>4)<=9 && (v&15)<=9; }
static unsigned decimal(uint8_t v) { return (v>>4)*10u+(v&15); }
static int date_valid(const uint8_t *d) {
    unsigned i,year,month,day,maxday;
    static const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    for (i=0;i<7;i++) if (!bcd(d[i])) return 0;
    if (d[0]!=0x20) return 0;
    year=2000+decimal(d[1]); month=decimal(d[2]); day=decimal(d[3]);
    if (!month || month>12 || !day || decimal(d[4])>23 ||
        decimal(d[5])>59 || decimal(d[6])>59) return 0;
    maxday=days[month-1];
    if (month==2 && year%4==0 && (year%100!=0 || year%400==0)) maxday++;
    return day<=maxday;
}
int up_decode_metadata(up_metadata *m, const uint8_t *d, size_t n) {
    if (!m || !d || n!=UP_METADATA_SIZE || d[6]>9 || !bcd(d[7]) ||
        d[8] || d[23] || !date_valid(d+9) || !date_valid(d+16) || !up_be32(d+56))
        return -1;
    memcpy(m->raw,d,n); m->checksum=up_be32(d); m->program=up_be16(d+4);
    m->image_size=up_be32(d+56); return 0;
}
uint64_t up_sum(uint64_t v, const uint8_t *d, size_t n) {
    size_t i; for (i=0;i<n;i++) v+=d[i]; return v;
}
size_t up_report(uint8_t out[UP_ACK_SIZE], const up_packet *r,
                 uint16_t serial, uint32_t sec, uint32_t ms,
                 uint16_t id, uint8_t result, uint16_t detail) {
    size_t n=id==0x88 ? UP_ACK_SIZE : UP_RESULT_SIZE;
    if ((id!=0x88 && id!=0x89 && id!=0x8a) || ms>999) return 0;
    memset(out,0,UP_ACK_SIZE);
    up_put16(out,r->destination); up_put16(out+2,r->source); up_put16(out+4,serial);
    up_put32(out+6,sec); up_put32(out+10,ms); out[15]=0xf3;
    up_put16(out+16,(uint16_t)n); up_put16(out+18,id);
    out[20]=r->module; out[21]=r->processor;
    up_put16(out+22,r->configuration); up_put16(out+24,r->program);
    out[26]=result;
    if (id==0x88) up_put16(out+27,detail); else out[27]=(uint8_t)detail;
    return n;
}
