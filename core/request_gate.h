#ifndef KZ_REQUEST_GATE_H
#define KZ_REQUEST_GATE_H
#include "ewgf_core.h"
enum { KZ_EVENT_BUFFERED=10, KZ_EVENT_BUFFER_EXPIRED=11,
       KZ_EVENT_BLOCKED_STATE=12, KZ_EVENT_RECOVERY_TIMEOUT=13 };
typedef struct { KzContext base; int32_t queueable, brake; } KzRequestContext;
typedef struct {
    KzState core;
    KzRequestContext context;
    uint64_t queued_at_ms, last_attempt_ms;
    int32_t seen, right_was_down, armed, queued, repeat_latched;
    int32_t recovering, saw_busy, pending_event;
} KzRequestState;
void kz_request_init(KzRequestState *s);
void kz_request_observe(KzRequestState *s, const KzRequestContext *ctx);
void kz_request_poll(KzRequestState *s, const KzPoll *in, KzOutput *out);
/* Input modes are fixed for a game session. movement_ready is supplied by the
   native supported-state classifier, not guessed from stick direction. */
void kz_request_poll_ex(KzRequestState *s, const KzPoll *in, KzOutput *out,
                        int32_t compact, int32_t movement_ready);
uint32_t kz_request_state_size(void);
#endif
