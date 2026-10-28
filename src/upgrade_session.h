#ifndef UPGRADE_SESSION_H
#define UPGRADE_SESSION_H
#include "upgrade_protocol.h"
#define UP_REPORT_QUEUE 16u
#define UP_RECEIVE_TIMEOUT_MS 30000u
#define UP_BURN_TIMEOUT_MS 120000u

typedef struct { uint32_t address; uint16_t port; } up_peer;
typedef enum { UP_IDLE, UP_RECEIVING, UP_PREPARING, UP_WAIT_BURN, UP_BURNING, UP_DONE, UP_FAILED } up_state;
typedef struct {
    uint8_t enabled;
    uint16_t configuration, program;
} up_target;
typedef struct {
    uint16_t local_uuid;
    up_target targets[4]; /* index 0..3 = A/B/C/D (0x71..0x74) */
    uint8_t module;
    uint8_t *image;
    size_t capacity;
    void *context;
    /* send: zero means copied/accepted; nonzero retains the queued report. */
    int (*send)(void *, up_peer, const uint8_t *, size_t);
    /* Optional UTC provider. Without it, timestamps are zero (not fabricated UTC). */
    void (*time)(void *, uint32_t *seconds, uint32_t *milliseconds);
    /* All hardware hooks are nonblocking, target-aware, and called only by poll.
       prepare: start staging/checking the target (local A or remote B/C/D).
       prepare_step: 0 busy, 1 ready with image/metadata checked and capacity reserved,
         -1 failed; fault 1..4 is ICD 0089, zero means unspecified/reserved.
       For B/C/D this stage includes AXI/Aurora transfer and remote validation.
       begin: start persistent programming only AFTER 0089 was sent.
       step: -1 failure, 0 busy, 1 success with image AND metadata verified durable.
       A FIFO write/DMA completion is NOT remote success.
       abort: synchronously stop/release outstanding work for this target.
       No pointers may be retained after abort or successful step completion. */
    int (*prepare)(void *, uint8_t target, const up_metadata *, const uint8_t *, size_t);
    int (*prepare_step)(void *, uint8_t target, uint8_t *fault);
    int (*begin)(void *, uint8_t target);
    int (*step)(void *, uint8_t target, uint8_t *progress);
    void (*abort)(void *, uint8_t target);
} up_config;
typedef struct { up_peer peer; uint8_t data[UP_ACK_SIZE]; size_t length; } up_pending;
typedef struct {
    up_config config;
    up_state state;
    up_peer peer;
    up_packet request;
    up_metadata metadata;
    uint16_t last_sequence, report_serial;
    size_t received;
    uint64_t sum;
    uint32_t last_activity, burn_started;
    uint8_t prepare_started;
    uint8_t load_ok, load_fault, burn_result, progress, have_burn_result;
    up_pending pending[UP_REPORT_QUEUE];
    unsigned head, count;
} up_session;
int up_init(up_session *s, const up_config *config);
/* 0 processed, -1 rejected/dropped without response. Only configured targets 0x71..0x74 are accepted; one active session total. */
int up_receive(up_session *s, up_peer peer, const uint8_t *data, size_t length, uint32_t now);
void up_poll(up_session *s, uint32_t now);
#endif
