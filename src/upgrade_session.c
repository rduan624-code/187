#include "upgrade_session.h"
#include <string.h>
static int same_peer(up_peer a, up_peer b) { return a.address==b.address && a.port==b.port; }
static int same_target(const up_packet *a,const up_packet *b) {
    return a->source==b->source && a->destination==b->destination &&
        a->module==b->module && a->processor==b->processor &&
        a->configuration==b->configuration && a->program==b->program && a->total==b->total;
}
static void report(up_session *s, up_peer peer,const up_packet *r,
                   uint16_t id,uint8_t result,uint16_t detail) {
    uint32_t sec=0,ms=0;
    up_pending *p=&s->pending[(s->head+s->count)%UP_REPORT_QUEUE];
    /* All callers reserve queue slots before changing session state. */
    if (s->config.time) s->config.time(s->config.context,&sec,&ms);
    if (ms>999) { sec=0; ms=0; }
    if (++s->report_serial==0) s->report_serial=1;
    p->peer=peer;
    p->length=up_report(p->data,r,s->report_serial,sec,ms,id,result,detail);
    s->count++;
}
static void load_failure(up_session *s,uint8_t code) {
    s->state=UP_FAILED; s->load_ok=0; s->load_fault=code;
    report(s,s->peer,&s->request,0x89,0,code);
}
static void burn_failure(up_session *s) {
    s->state=UP_FAILED; s->have_burn_result=1; s->burn_result=0; s->progress=0;
    report(s,s->peer,&s->request,0x8a,0,0);
}
static void reset_transfer(up_session *s) {
    s->state=UP_IDLE; s->last_sequence=0; s->received=0; s->sum=0;
    s->prepare_started=0; s->load_ok=0; s->load_fault=0; s->have_burn_result=0; s->progress=0;
}
int up_init(up_session *s,const up_config *c) {
    if (!s || !c || !c->local_uuid || !c->image || !c->capacity || !c->send ||
        c->capacity>UINT32_MAX) return -1;
    memset(s,0,sizeof(*s)); s->config=*c; return 0;
}
int up_receive(up_session *s,up_peer peer,const uint8_t *data,size_t length,uint32_t now) {
    up_packet p;
    up_metadata meta;
    const up_target *target;
    if (!s || up_decode(&p,data,length) || p.destination!=s->config.local_uuid ||
        p.module!=s->config.module || p.processor<0x71 || p.processor>0x74 ||
        s->count>UP_REPORT_QUEUE-3) return -1;
    target=&s->config.targets[p.processor-0x71];
    if (!target->enabled || p.configuration!=target->configuration || p.program!=target->program)
        return -1;
    /* Permit the next distinct upgrade after terminal reports have left the queue.
       An identical first frame remains a retransmission during retention time. */
    if ((s->state==UP_DONE || s->state==UP_FAILED) && s->count==0 && p.sequence==1 &&
        up_decode_metadata(&meta,p.payload,p.payload_size)==0 &&
        (!same_peer(s->peer,peer) || !same_target(&s->request,&p) ||
         memcmp(meta.raw,s->metadata.raw,UP_METADATA_SIZE)!=0))
        reset_transfer(s);
    if (s->state!=UP_IDLE && (!same_peer(s->peer,peer) || !same_target(&s->request,&p)))
        return -1; /* A different sender cannot take over an active transfer. */
    if (s->state==UP_IDLE) {
        if (p.sequence!=1) { report(s,peer,&p,0x88,0,0); return 0; }
        if (up_decode_metadata(&meta,p.payload,p.payload_size)) {
            report(s,peer,&p,0x88,0,0); return 0;
        }
        reset_transfer(s); s->peer=peer; s->request=p;
        s->request.payload=NULL; s->request.payload_size=0;
        s->metadata=meta; s->last_activity=now;
        s->last_sequence=1; s->state=UP_RECEIVING;
        report(s,peer,&p,0x88,1,1);
        if (meta.program!=p.program) { load_failure(s,4); return 0; }
        if (meta.image_size>s->config.capacity) { load_failure(s,1); return 0; }
        /* Nonempty chunks <=1024; total includes the metadata frame. */
        if ((uint64_t)meta.image_size>(uint64_t)(p.total-1)*UP_MAX_PAYLOAD ||
            meta.image_size<(uint32_t)(p.total-1)) { load_failure(s,3); return 0; }
        s->sum=up_sum(0,meta.raw+4,UP_METADATA_SIZE-4);
        return 0;
    }
    if (s->state!=UP_RECEIVING || p.sequence!=(uint32_t)s->last_sequence+1) {
        /* ICD: all non-contiguous sequences fail, returning the latest accepted one.
           Repeating a terminal frame also replays results, never repeats a burn. */
        report(s,peer,&p,0x88,0,s->last_sequence);
        if ((s->state==UP_DONE || s->state==UP_FAILED) && p.sequence==s->last_sequence) {
            report(s,peer,&s->request,0x89,s->load_ok,s->load_fault);
            if (s->have_burn_result)
                report(s,peer,&s->request,0x8a,s->burn_result,s->progress);
        }
        return 0;
    }
    if (p.payload_size>s->metadata.image_size-s->received) {
        report(s,peer,&p,0x88,0,s->last_sequence); load_failure(s,3); return 0;
    }
    memcpy(s->config.image+s->received,p.payload,p.payload_size);
    s->sum=up_sum(s->sum,p.payload,p.payload_size);
    s->received+=p.payload_size; s->last_sequence=p.sequence; s->last_activity=now;
    report(s,peer,&p,0x88,1,p.sequence);
    if (p.end==0) {
        /* Exact sum: overflow of the 32-bit transmitted checksum is rejected.
           Do not silently select modulo arithmetic without an ICD agreement. */
        if (s->received!=s->metadata.image_size || s->sum>UINT32_MAX ||
            s->sum!=s->metadata.checksum) { load_failure(s,3); return 0; }
        s->state=UP_PREPARING;
        s->prepare_started=0;
        s->burn_started=now;
    }
    return 0;
}
void up_poll(up_session *s,uint32_t now) {
    int result;
    uint8_t progress, fault=0;
    uint8_t target;
    if (!s) return;
    target=s->request.processor;
    /* Bounded queue; preserve order and retry a local send failure next poll. */
    while (s->count) {
        up_pending *p=&s->pending[s->head];
        if (s->config.send(s->config.context,p->peer,p->data,p->length)) break;
        s->head=(s->head+1)%UP_REPORT_QUEUE; s->count--;
    }
    /* Timeout must still stop a backend if the network is blocked. */
    if ((s->state==UP_BURNING || s->state==UP_PREPARING) &&
        (uint32_t)(now-s->burn_started)>=UP_BURN_TIMEOUT_MS) {
        if (s->config.abort) s->config.abort(s->config.context,target);
        /* Keep one slot available while burning, including under backpressure. */
        if (s->state==UP_PREPARING) load_failure(s,0); else burn_failure(s);
        s->last_activity=now; return;
    }
    if (s->state!=UP_PREPARING && s->state!=UP_BURNING && s->state!=UP_IDLE &&
        (uint32_t)(now-s->last_activity)>=UP_RECEIVE_TIMEOUT_MS) {
        if (s->state==UP_WAIT_BURN && s->config.abort)
            s->config.abort(s->config.context,target);
        s->head=0; s->count=0; reset_transfer(s); return;
    }
    if (s->count) return;
    if (s->state==UP_PREPARING) {
        if (!s->prepare_started) {
            if (!s->config.prepare || !s->config.prepare_step || !s->config.abort ||
                !s->config.begin || !s->config.step ||
                s->config.prepare(s->config.context,target,&s->metadata,s->config.image,s->received)) {
                if (s->config.abort) s->config.abort(s->config.context,target);
                load_failure(s,0); s->last_activity=now; return;
            }
            s->prepare_started=1; return;
        }
        result=s->config.prepare_step(s->config.context,target,&fault);
        if (result<0 || result>1 || fault>4) {
            s->config.abort(s->config.context,target);
            load_failure(s,fault<=4 ? fault : 0); s->last_activity=now;
        } else if (result==1) {
            s->load_ok=1; s->state=UP_WAIT_BURN; s->last_activity=now;
            report(s,s->peer,&s->request,0x89,1,0);
        }
        return;
    }
    if (s->state==UP_WAIT_BURN) {
        if (!s->config.begin || !s->config.step || !s->config.abort ||
            s->config.begin(s->config.context,target)) {
            /* Missing backend is a real burn failure, not a simulated success. */
            if (s->config.abort) s->config.abort(s->config.context,target);
            burn_failure(s); s->last_activity=now; return;
        }
        s->burn_started=now; s->state=UP_BURNING;
        report(s,s->peer,&s->request,0x8a,2,0); return;
    }
    if (s->state!=UP_BURNING) return;
    progress=s->progress;
    result=s->config.step(s->config.context,target,&progress);
    if (result<0 || result>1 || progress>4 || (result==0 && progress>3) || progress<s->progress) {
        s->config.abort(s->config.context,target); burn_failure(s); s->last_activity=now;
    } else if (result==1) {
        s->state=UP_DONE; s->have_burn_result=1; s->burn_result=1; s->progress=4;
        s->last_activity=now; report(s,s->peer,&s->request,0x8a,1,4);
    } else if (progress!=s->progress) {
        s->progress=progress; report(s,s->peer,&s->request,0x8a,2,progress);
    }
}
