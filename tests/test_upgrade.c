#include "upgrade_app.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    upgrade_app_t session;
    uint8_t image[4096], wire[UP_MAX_PACKET], meta[60], content[2048];
    uint8_t output[128][UP_ACK_SIZE];
    size_t lengths[128];
    upgrade_peer_t destinations[128];
    unsigned sent, starts, steps, aborts, prepares;
    uint8_t target;
    int prepare_busy;
    int blocked, validation, begin_error, step_result;
    uint8_t progress;
} fixture;
static const upgrade_peer_t peer={0x01020304u,10000};
static int send_data(void *v,upgrade_peer_t p,const uint8_t *d,size_t n) {
    fixture *f=v;
    if (f->blocked) return -1;
    assert(f->sent<128); memcpy(f->output[f->sent],d,n);
    f->destinations[f->sent]=p; f->lengths[f->sent++]=n; return 0;
}
static int prepare(void *v,uint8_t target,const icd_summary_t *m,const uint8_t *d,size_t n) {
    fixture *f=v; f->prepares++; f->target=target;
    assert(n==sizeof(f->content)); assert(m->image_size==n);
    assert(memcmp(d,f->content,n)==0); return 0;
}
static int prepare_step(void *v,uint8_t target,uint8_t *fault) {
    fixture *f=v; assert(target==f->target);
    if (f->prepare_busy) return 0;
    *fault=(uint8_t)f->validation; return f->validation ? -1 : 1;
}
static int begin(void *v,uint8_t target) {
    fixture *f=v; assert(target==f->target); f->starts++; return f->begin_error;
}
static int step(void *v,uint8_t target,uint8_t *p) {
    fixture *f=v; assert(target==f->target); f->steps++; *p=f->progress; return f->step_result;
}
static void abort_burn(void *v,uint8_t target) { (void)target; ((fixture *)v)->aborts++; }
static void init(fixture *f) {
    unsigned i;
    upgrade_config_t c;
    static const uint8_t date[7]={0x20,0x26,0x10,0x28,0x12,0x34,0x56};
    memset(f,0,sizeof(*f)); memset(&c,0,sizeof(c));
    c.local_uuid=0x360a; c.module=0x36; /* TEST ADDRESSES, not board defaults. */
    for(i=0;i<4;i++) c.targets[i]=(upgrade_target_t){1,0x4b,0x4b0a};
    c.image=f->image; c.capacity=sizeof(f->image);
    c.context=f; c.send=send_data; c.prepare=prepare; c.prepare_step=prepare_step; c.begin=begin; c.step=step; c.abort=abort_burn;
    assert(upgrade_app_init(&f->session,&c)==0);
    for(i=0;i<sizeof(f->content);i++) f->content[i]=(uint8_t)i;
    icd_write_u16_be(f->meta+4,0x4b0a); f->meta[6]=2; f->meta[7]=1;
    memcpy(f->meta+9,date,7); memcpy(f->meta+16,date,7);
    memcpy(f->meta+24,"A.bin",5); icd_write_u32_be(f->meta+56,sizeof(f->content));
    icd_write_u32_be(f->meta,(uint32_t)icd_checksum_add(icd_checksum_add(0,f->meta+4,56),f->content,sizeof(f->content)));
}
static size_t packet(fixture *f,unsigned seq,const uint8_t *payload,size_t n) {
    uint8_t *w=f->wire;
    memset(w,0,sizeof(f->wire)); icd_write_u16_be(w,0x1001); icd_write_u16_be(w+2,0x360a);
    icd_write_u16_be(w+4,(uint16_t)(100+seq)); icd_write_u32_be(w+6,123456789); icd_write_u32_be(w+10,999);
    w[15]=0xf0; icd_write_u16_be(w+16,(uint16_t)(31+n)); icd_write_u16_be(w+18,8);
    w[20]=0x36; w[21]=0x71; icd_write_u16_be(w+22,0x4b); icd_write_u16_be(w+24,0x4b0a);
    w[26]=(uint8_t)(seq==3 ? 0 : 1); icd_write_u16_be(w+27,3); icd_write_u16_be(w+29,(uint16_t)seq);
    memcpy(w+31,payload,n); return 31+n;
}
static void receive(fixture *f,unsigned seq) {
    size_t n=seq==1 ? packet(f,1,f->meta,60) : packet(f,seq,f->content+(seq-2)*1024,1024);
    assert(upgrade_app_on_datagram(&f->session,peer,f->wire,n,seq*10)==0);
}
static void load(fixture *f) {
    receive(f,1); receive(f,2); receive(f,3);
    assert(f->session.received==2048);
}
static void start_burn(fixture *f) {
    upgrade_app_poll(&f->session,40); upgrade_app_poll(&f->session,41); upgrade_app_poll(&f->session,42);
}
static void check_report(fixture *f,unsigned i,uint16_t id,uint8_t result,unsigned detail) {
    const uint8_t *d=f->output[i];
    assert(i<f->sent); assert(icd_read_u16_be(d)==0x360a); assert(icd_read_u16_be(d+2)==0x1001);
    assert(icd_read_u16_be(d+4)!=0); assert(d[14]==0 && d[15]==0xf3);
    assert(icd_read_u16_be(d+16)==f->lengths[i]); assert(icd_read_u16_be(d+18)==id);
    assert(d[20]==0x36 && d[21]==0x71 && icd_read_u16_be(d+22)==0x4b && icd_read_u16_be(d+24)==0x4b0a);
    assert(d[26]==result); assert(f->lengths[i]==(id==0x88 ? 29u : 28u));
    assert((id==0x88 ? icd_read_u16_be(d+27) : d[27])==detail);
    assert(f->destinations[i].address==peer.address && f->destinations[i].port==peer.port);
}
static void test_protocol(void) {
    fixture f; icd_update_frame_t p; icd_summary_t m; size_t n,i;
    uint8_t report[UP_ACK_SIZE];
    static const uint8_t expected[29]={
        0x36,0x0a,0x10,0x01,0x12,0x34,0x01,0x02,0x03,0x04,0,0,0x03,0xe7,
        0,0xf3,0,0x1d,0,0x88,0x36,0x71,0,0x4b,0x4b,0x0a,1,0,1};
    init(&f); n=packet(&f,1,f.meta,60); assert(n==91);
    assert(icd_parse_update(&p,f.wire,n)==0); assert(p.payload_size==60);
    assert(icd_build_report(report,&p,0x1234,0x01020304,999,0x88,1,1)==29);
    assert(memcmp(report,expected,29)==0);
    for(i=0;i<n;i++) assert(icd_parse_update(&p,f.wire,i)!=0);
    assert(icd_parse_update(&p,f.wire,n+2)!=0); /* No trailing CRC accepted. */
    f.wire[15]=0xf3; assert(icd_parse_update(&p,f.wire,n)!=0); f.wire[15]=0xf0;
    f.wire[26]=0; assert(icd_parse_update(&p,f.wire,n)!=0); f.wire[26]=1;
    icd_write_u32_be(f.wire+10,1000); assert(icd_parse_update(&p,f.wire,n)!=0);
    assert(icd_parse_summary(&m,f.meta,60)==0);
    f.meta[11]=0x13; assert(icd_parse_summary(&m,f.meta,60)!=0); /* month */
    assert(icd_checksum_add(UINT32_MAX,(const uint8_t *)"\1",1)==UINT64_C(4294967296));
}
static void test_success(void) {
    fixture f; init(&f); load(&f);
    assert(f.starts==0); assert(f.session.state==UP_PREPARING);
    start_burn(&f); assert(f.starts==1);
    check_report(&f,0,0x88,1,1); check_report(&f,1,0x88,1,2);
    check_report(&f,2,0x88,1,3); check_report(&f,3,0x89,1,0);
    upgrade_app_poll(&f.session,50); check_report(&f,4,0x8a,2,0);
    f.progress=1; upgrade_app_poll(&f.session,60); upgrade_app_poll(&f.session,70);
    check_report(&f,5,0x8a,2,1);
    f.step_result=1; upgrade_app_poll(&f.session,80); upgrade_app_poll(&f.session,90);
    check_report(&f,6,0x8a,1,4); assert(f.session.state==UP_DONE);
    receive(&f,3); upgrade_app_poll(&f.session,100);
    check_report(&f,7,0x88,0,3); check_report(&f,8,0x89,1,0); check_report(&f,9,0x8a,1,4);
    assert(f.starts==1); /* terminal retransmission must not program twice */
}
static void test_order_and_peer(void) {
    fixture f; size_t n; upgrade_peer_t stranger=peer; init(&f);
    receive(&f,2); upgrade_app_poll(&f.session,0); check_report(&f,0,0x88,0,0);
    receive(&f,1); receive(&f,3); receive(&f,1);
    upgrade_app_poll(&f.session,40); check_report(&f,2,0x88,0,1); check_report(&f,3,0x88,0,1);
    assert(f.session.received==0);
    n=packet(&f,2,f.content,1024); stranger.address++;
    assert(upgrade_app_on_datagram(&f.session,stranger,f.wire,n,50)==-1);
    f.wire[21]=0x75; assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,50)==-1);
    f.wire[21]=0x71; f.wire[20]++; assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,50)==-1);
    receive(&f,2); receive(&f,2); assert(f.session.received==1024);
    receive(&f,3); assert(f.session.state==UP_PREPARING);
}
static void test_invalid_images(void) {
    fixture f; size_t n;
    init(&f); f.meta[0]^=1; load(&f); start_burn(&f);
    check_report(&f,3,0x89,0,3); assert(f.starts==0);
    init(&f); icd_write_u16_be(f.meta+4,0x1234); receive(&f,1); upgrade_app_poll(&f.session,20);
    check_report(&f,1,0x89,0,4);
    init(&f); f.session.config.capacity=100; receive(&f,1); upgrade_app_poll(&f.session,20);
    check_report(&f,1,0x89,0,1);
    init(&f); receive(&f,1); receive(&f,2); n=packet(&f,3,f.content+1024,1000);
    assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,30)==0); upgrade_app_poll(&f.session,40);
    check_report(&f,3,0x89,0,3); assert(f.starts==0);
    init(&f); receive(&f,1); f.session.metadata.image_size=1000;
    receive(&f,2); assert(f.session.received==0); upgrade_app_poll(&f.session,40);
    check_report(&f,1,0x88,0,1); check_report(&f,2,0x89,0,3);
    init(&f); receive(&f,1); icd_write_u32_be(f.meta+56,1); /* cannot replace accepted metadata */
    assert(f.session.metadata.image_size==2048);
}
static void test_backend_failures(void) {
    fixture f;
    init(&f); f.validation=1; load(&f); start_burn(&f);
    check_report(&f,3,0x89,0,1); assert(f.starts==0);
    init(&f); f.session.config.prepare=NULL; load(&f); start_burn(&f);
    check_report(&f,3,0x89,0,0); assert(f.starts==0);
    init(&f); f.begin_error=-1; load(&f); start_burn(&f); upgrade_app_poll(&f.session,50);
    check_report(&f,4,0x8a,0,0); assert(f.aborts==1);
    init(&f); f.step_result=-1; load(&f); start_burn(&f);
    upgrade_app_poll(&f.session,50); upgrade_app_poll(&f.session,60); check_report(&f,5,0x8a,0,0);
    init(&f); f.progress=4; load(&f); start_burn(&f);
    upgrade_app_poll(&f.session,50); upgrade_app_poll(&f.session,60); check_report(&f,5,0x8a,0,0);
}
static void test_backpressure_and_timeout(void) {
    fixture f; unsigned i; size_t before;
    init(&f); f.blocked=1; load(&f); start_burn(&f);
    assert(f.sent==0 && f.starts==0); f.blocked=0; start_burn(&f);
    assert(f.starts==1); check_report(&f,0,0x88,1,1);
    f.blocked=1; upgrade_app_poll(&f.session,50+UP_BURN_TIMEOUT_MS);
    assert(f.aborts==1 && f.session.state==UP_FAILED); f.blocked=0;
    upgrade_app_poll(&f.session,51+UP_BURN_TIMEOUT_MS); check_report(&f,5,0x8a,0,0);
    init(&f); receive(&f,1); upgrade_app_poll(&f.session,10+UP_RECEIVE_TIMEOUT_MS);
    assert(f.session.state==UP_IDLE); receive(&f,1); assert(f.session.last_sequence==1);
    init(&f); receive(&f,1); f.blocked=1;
    for(i=0;i<30;i++) {
        size_t n=packet(&f,1,f.meta,60); (void)upgrade_app_on_datagram(&f.session,peer,f.wire,n,20);
    }
    assert(f.session.count<=UP_REPORT_QUEUE); before=f.session.received;
    { size_t n=packet(&f,2,f.content,1024); assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,30)==-1); }
    assert(f.session.received==before); f.blocked=0; upgrade_app_poll(&f.session,40);
    receive(&f,2); assert(f.session.received==1024);
    /* Millisecond counter wraparound. */
    init(&f); receive(&f,1); f.session.last_activity=UINT32_MAX-10;
    upgrade_app_poll(&f.session,UP_RECEIVE_TIMEOUT_MS); assert(f.session.state==UP_IDLE);
}
static void test_target_routes(void) {
    fixture f; unsigned target,seq; size_t n;
    for(target=0x71;target<=0x74;target++) {
        init(&f);
        for(seq=1;seq<=3;seq++) {
            n=seq==1 ? packet(&f,1,f.meta,60) : packet(&f,seq,f.content+(seq-2)*1024,1024);
            f.wire[21]=(uint8_t)target;
            assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,seq*10)==0);
        }
        f.prepare_busy=1; start_burn(&f);
        assert(f.target==target && f.starts==0 && f.sent==3);
        /* An ACK from staging cannot be mistaken for a burn result. */
        f.prepare_busy=0; upgrade_app_poll(&f.session,50); upgrade_app_poll(&f.session,51);
        assert(f.starts==1 && f.session.state==UP_BURNING);
        assert(f.output[3][21]==target && icd_read_u16_be(f.output[3]+18)==0x89);
        n=packet(&f,1,f.meta,60); f.wire[21]=(uint8_t)(target==0x71 ? 0x72 : 0x71);
        assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,52)==-1);
        f.step_result=1; upgrade_app_poll(&f.session,60); upgrade_app_poll(&f.session,61);
        assert(f.session.state==UP_DONE && f.starts==1);
        assert(f.output[f.sent-1][21]==target && f.output[f.sent-1][26]==1);
    }
    /* A completed A transfer must not block a subsequent B transfer. */
    init(&f); load(&f); start_burn(&f); f.step_result=1;
    upgrade_app_poll(&f.session,50); upgrade_app_poll(&f.session,51);
    n=packet(&f,1,f.meta,60); f.wire[21]=0x72;
    assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,60)==0);
    assert(f.session.state==UP_RECEIVING && f.session.request.processor==0x72);
    init(&f); f.prepare_busy=1; load(&f); upgrade_app_poll(&f.session,40);
    upgrade_app_poll(&f.session,30+UP_BURN_TIMEOUT_MS);
    assert(f.session.state==UP_FAILED && f.starts==0 && f.aborts==1);
    init(&f); f.session.config.targets[1].enabled=0;
    n=packet(&f,1,f.meta,60); f.wire[21]=0x72;
    assert(upgrade_app_on_datagram(&f.session,peer,f.wire,n,0)==-1);
}
static void test_decoder_random_inputs(void) {
    uint8_t bytes[UP_MAX_PACKET+4]; uint32_t rng=1; unsigned i,j; icd_update_frame_t p;
    for(i=0;i<10000;i++) {
        size_t n=i%sizeof(bytes);
        for(j=0;j<n;j++) { rng=rng*1664525u+1013904223u; bytes[j]=(uint8_t)(rng>>24); }
        (void)icd_parse_update(&p,bytes,n);
    }
}
int main(void) {
    test_protocol(); test_success(); test_order_and_peer(); test_invalid_images();
    test_backend_failures(); test_backpressure_and_timeout(); test_target_routes(); test_decoder_random_inputs();
    puts("PASS: 8 test groups (wire format, success, ordering/targets, image validation, backend failures, backpressure/timeouts, four target routes, 10000 malformed inputs)");
    return 0;
}
