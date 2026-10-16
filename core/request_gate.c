#include "request_gate.h"
#define CONTEXT_MS UINT64_C(80)
#define BUFFER_MS UINT64_C(1000)
#define RECOVERY_MS UINT64_C(2500)
#define STICK_DIRECTION_MASK (UINT64_C(0xFF) << 16)

static void clear_request(KzRequestState *s, int event) {
    if (event && (s->core.phase || s->queued || s->repeat_latched || s->recovering))
        s->pending_event=event;
    kz_init(&s->core);
    s->queued=0; s->repeat_latched=0; s->recovering=0;
    s->saw_busy=0; s->armed=0;
}
void kz_request_init(KzRequestState *s) {
    if (!s) return;
    KzRequestState zero={0}; *s=zero; kz_init(&s->core);
}
uint32_t kz_request_state_size(void) { return (uint32_t)sizeof(KzRequestState); }
void kz_request_observe(KzRequestState *s, const KzRequestContext *ctx) {
    if (!s || !ctx) return;
    int changed=!s->seen || ctx->base.actor!=s->context.base.actor;
    int backward=s->seen && (ctx->base.epoch<s->context.base.epoch ||
                            ctx->base.now_ms<s->context.base.now_ms);
    if (changed || backward) clear_request(s,backward?KZ_EVENT_BAD_CLOCK:KZ_EVENT_CANCELLED);
    int duplicate=!changed && ctx->base.epoch==s->context.base.epoch;
    int revoked=duplicate && (!s->context.base.permitted || !s->context.queueable);
    s->context=*ctx; s->seen=1;
    if (revoked) { s->context.base.permitted=0; s->context.queueable=0; }
    if (!s->context.base.permitted || !s->context.queueable)
        clear_request(s,KZ_EVENT_CANCELLED);
    int active=s->core.phase!=KZ_IDLE;
    kz_observe(&s->core,&s->context.base);
    if (active && !s->core.phase) clear_request(s,KZ_EVENT_CANCELLED);
    if (s->recovering) {
        if (!s->context.base.actionable) s->saw_busy=1;
        /* Preserve normal recovery: only native WAIT unlocks a held repeat,
           even when the initial request may start from movement. */
        if (s->saw_busy && s->context.base.actionable && !s->core.phase)
            s->recovering=0;
    }
}
static void pulse(KzState *s, const KzPoll *in, KzOutput *out, int compact) {
    if (compact) kz_poll_fast(s,in,out); else kz_poll(s,in,out);
}
void kz_request_poll_ex(KzRequestState *s, const KzPoll *in, KzOutput *out,
                        int32_t compact, int32_t movement_ready) {
    if (!s || !in || !out) return;
    *out=(KzOutput){in->buttons,in->lx,in->ly,in->rx,in->ry,0,0,0,0};
    if (!in->matching_pad) return;
    int right=(in->buttons & KZ_RIGHT)!=0;
    int rising=right && !s->right_was_down;
    s->right_was_down=right;
    int fresh=s->seen && in->now_ms>=s->context.base.now_ms &&
              in->now_ms-s->context.base.now_ms<=CONTEXT_MS;
    int scope=in->training_now && in->connected && fresh && s->context.base.permitted;
    if (!in->enabled) {
        if (s->recovering || s->queued || s->repeat_latched || s->core.phase)
            clear_request(s,0);
        s->armed=0; s->pending_event=0;
        kz_observe(&s->core,&s->context.base);
        kz_poll(&s->core,in,out);
        return;
    }
    if (!scope) {
        clear_request(s,KZ_EVENT_CANCELLED);
        out->event=s->pending_event; s->pending_event=0; return;
    }
    out->buttons &= ~KZ_RIGHT; out->consume_right=right;
    if ((in->buttons & KZ_CANCEL_MASK)!=0 || !s->context.queueable) {
        clear_request(s,KZ_EVENT_CANCELLED);
        if (rising && !s->pending_event) s->pending_event=KZ_EVENT_BLOCKED_STATE;
        out->event=s->pending_event; s->pending_event=0; return;
    }
    if (!right) { s->armed=1; s->repeat_latched=0; }
    if (rising && s->armed) {
        s->armed=0; s->queued=1; s->queued_at_ms=in->now_ms;
        s->repeat_latched=1;
        if (s->core.phase || s->recovering || !s->context.base.actionable)
            s->pending_event=KZ_EVENT_BUFFERED;
    }
    if (s->queued && (in->now_ms<s->queued_at_ms || in->now_ms-s->queued_at_ms>BUFFER_MS)) {
        s->queued=0;
        if (!s->repeat_latched) s->pending_event=KZ_EVENT_BUFFER_EXPIRED;
    }
    if (s->recovering && (in->now_ms<s->last_attempt_ms ||
                          in->now_ms-s->last_attempt_ms>RECOVERY_MS))
        clear_request(s,KZ_EVENT_RECOVERY_TIMEOUT);
    int ready=s->context.base.actionable || (movement_ready && s->context.brake);
    if (s->core.phase) {
        KzPoll physical=*in; physical.buttons &= ~KZ_RIGHT;
        pulse(&s->core,&physical,out,compact);
        if (out->event==KZ_EVENT_BAD_CLOCK || out->event==KZ_EVENT_CANCELLED) {
            int reason=out->event; clear_request(s,0); s->pending_event=reason;
        }
    } else if (!s->recovering && (s->queued || (right && s->repeat_latched)) &&
               ready && s->context.base.can_continue &&
               (s->context.base.facing==1 || s->context.base.facing==-1)) {
        KzPoll command=*in; KzOutput ignored;
        KzContext start=s->context.base;
        /* This changes our request predicate, NOT Smash status/animation.
           Input commands remain subject to the game's native parser. */
        start.actionable=1;
        kz_init(&s->core); kz_observe(&s->core,&start);
        command.buttons=0; pulse(&s->core,&command,&ignored,compact);
        command.buttons=KZ_RIGHT; pulse(&s->core,&command,out,compact);
        if (out->event==KZ_EVENT_STARTED) {
            s->queued=0; s->recovering=1; s->saw_busy=0;
            s->last_attempt_ms=in->now_ms; s->pending_event=0;
        }
    } else if ((s->queued || (right && s->repeat_latched)) &&
               s->context.brake && s->context.base.can_continue) {
        out->lx=0; out->ly=0; out->rx=0; out->ry=0;
        out->buttons &= ~STICK_DIRECTION_MASK;
        out->owns_input=1; out->consume_right=1; out->phase=0;
    }
    if (right) { out->buttons &= ~KZ_RIGHT; out->consume_right=1; }
    if (!out->event) { out->event=s->pending_event; s->pending_event=0; }
}
/* Legacy entry point retains all v0.2.4 behavior for regression tests. */
void kz_request_poll(KzRequestState *s, const KzPoll *in, KzOutput *out) {
    kz_request_poll_ex(s,in,out,0,0);
}
