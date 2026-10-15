#include "ewgf_core.h"
#define LEASE_MS UINT64_C(80)
static void stop(KzState *s, int event) {
    if (s->phase) s->pending_event=event;
    s->phase=KZ_IDLE; s->delivered=0; s->armed=0;
}
void kz_init(KzState *s) {
    KzState zero={0}; *s=zero; s->facing=1; s->initialized=1;
}
uint32_t kz_state_size(void) { return (uint32_t)sizeof(KzState); }
void kz_observe(KzState *s, const KzContext *ctx) {
    if (!s || !ctx) return;
    if (!s->initialized) kz_init(s);
    int reset=!s->seen_context || ctx->actor != s->context.actor;
    if (s->seen_context && ctx->epoch <= s->context.epoch) {
        if (ctx->epoch == s->context.epoch && ctx->actor == s->context.actor) {
            /* Duplicate callbacks cannot advance the sequence, but a revoked
               permission must still cancel immediately. */
            if (!ctx->permitted || !ctx->can_continue) {
                s->context.permitted=0; stop(s,KZ_EVENT_CANCELLED);
            }
            return;
        }
        stop(s,KZ_EVENT_BAD_CLOCK); reset=1;
    }
    if (reset) {
        stop(s,KZ_EVENT_CANCELLED); s->armed=0;
    } else if (s->phase) {
        if (ctx->epoch != s->context.epoch+1 || !s->delivered)
            stop(s,KZ_EVENT_BAD_CLOCK);
        else if (!ctx->permitted || !ctx->can_continue)
            stop(s,KZ_EVENT_CANCELLED);
        else if (s->phase == KZ_RELEASE) {
            s->phase=KZ_IDLE; s->delivered=0;
            s->pending_event=KZ_EVENT_INPUT_COMPLETE;
        } else {
            ++s->phase; s->delivered=0;
        }
    }
    s->context=*ctx; s->seen_context=1;
    if (!ctx->permitted) stop(s,KZ_EVENT_CANCELLED);
}
void kz_poll(KzState *s, const KzPoll *in, KzOutput *out) {
    if (!s || !in || !out) return;
    *out=(KzOutput){in->buttons,in->lx,in->ly,in->rx,in->ry,0,0,0,0};
    if (!s->initialized) kz_init(s);
    /* Never consume another controller's edge, or mutate its input. */
    if (!in->matching_pad) return;
    int right=(in->buttons & KZ_RIGHT)!=0;
    int rising=right && !s->right_was_down;
    s->right_was_down=right;
    int fresh=s->seen_context && in->now_ms>=s->context.now_ms &&
              in->now_ms-s->context.now_ms<=LEASE_MS;
    int allowed=in->training_now && in->connected && fresh && s->context.permitted;
    if (!allowed) {
        stop(s,KZ_EVENT_CANCELLED); s->armed=0;
        s->suppress_until_release=0;
        out->event=s->pending_event; s->pending_event=0;
        return;
    }
    if (!right) { s->armed=1; s->suppress_until_release=0; }
    if ((in->buttons & KZ_CANCEL_MASK)!=0) {
        stop(s,KZ_EVENT_CANCELLED);
        /* A + D-pad right is not a macro trigger. Manual buttons win. */
        s->armed=0;
    } else if (rising && s->armed && !s->phase && s->context.actionable &&
               (s->context.facing==1 || s->context.facing==-1)) {
        s->armed=0;
        if (in->enabled) {
            s->phase=KZ_FORWARD; s->facing=s->context.facing;
            s->delivered=0; s->suppress_until_release=1;
            s->pending_event=KZ_EVENT_STARTED;
        } else s->pending_event=KZ_EVENT_DIAGNOSTIC;
    }
    if (!in->enabled && s->phase) stop(s,KZ_EVENT_CANCELLED);
    if (in->enabled && s->suppress_until_release) {
        out->buttons &= ~KZ_RIGHT; out->consume_right=1;
    }
    if (s->phase) {
        /* Do not merge real stick values into a command-input pulse. */
        out->owns_input=1; out->consume_right=1;
        out->buttons=0; out->lx=0; out->ly=0; out->rx=0; out->ry=0;
        out->phase=s->phase;
        if (s->phase==KZ_FORWARD) out->lx=32767*s->facing;
        if (s->phase==KZ_DOWN) out->ly=-32767;
        if (s->phase==KZ_DIAGONAL_ATTACK) {
            out->lx=23170*s->facing; out->ly=-23170; out->buttons=KZ_A;
        }
        s->delivered=1; s->last_poll_epoch=s->context.epoch;
    }
    out->event=s->pending_event; s->pending_event=0;
}
