#include "ewgf_core.h"
#define LEASE_MS UINT64_C(80)
#define PHASE_TIMEOUT_MS UINT64_C(80)

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
    int changed=!s->seen_context || ctx->actor!=s->context.actor;
    if (changed) {
        stop(s,KZ_EVENT_CANCELLED); s->armed=0;
    } else if (ctx->epoch<s->context.epoch || ctx->now_ms<s->context.now_ms) {
        stop(s,KZ_EVENT_BAD_CLOCK);
    }
    /* The callback count is NOT a hardware/input-frame clock. In the user's
       v0.2 log it advanced twice before the next GC poll (85 -> 87, 17ms).
       Observation updates safety/context only. Never advance an input here. */
    int duplicate=!changed && ctx->epoch==s->context.epoch;
    int revoked=duplicate && (!s->context.permitted || !s->context.can_continue);
    s->context=*ctx; s->seen_context=1;
    if (revoked) { s->context.permitted=0; s->context.can_continue=0; }
    if (!s->context.permitted || !s->context.can_continue)
        stop(s,KZ_EVENT_CANCELLED);
}

void kz_poll(KzState *s, const KzPoll *in, KzOutput *out) {
    if (!s || !in || !out) return;
    *out=(KzOutput){in->buttons,in->lx,in->ly,in->rx,in->ry,0,0,0,0};
    if (!s->initialized) kz_init(s);
    if (!in->matching_pad) return;
    int right=(in->buttons & KZ_RIGHT)!=0;
    int rising=right && !s->right_was_down;
    s->right_was_down=right;
    int fresh=s->seen_context && in->now_ms>=s->context.now_ms &&
              in->now_ms-s->context.now_ms<=LEASE_MS;
    int allowed=in->training_now && in->connected && fresh &&
                s->context.permitted && s->context.can_continue;
    if (!allowed) {
        stop(s,KZ_EVENT_CANCELLED); s->armed=0;
        s->suppress_until_release=0;
        out->event=s->pending_event; s->pending_event=0;
        return;
    }
    if (!right) { s->armed=1; s->suppress_until_release=0; }
    int was_active=s->phase!=KZ_IDLE;
    if ((in->buttons & KZ_CANCEL_MASK)!=0) {
        stop(s,KZ_EVENT_CANCELLED); s->armed=0;
    } else if (was_active) {
        if (!in->enabled) stop(s,KZ_EVENT_CANCELLED);
        else if (in->now_ms<s->phase_started_ms ||
                 in->now_ms-s->phase_started_ms>PHASE_TIMEOUT_MS) {
            /* A real long stall still cancels. Repeated polls or callbacks
               cannot continually renew this per-output deadline. */
            stop(s,KZ_EVENT_BAD_CLOCK);
        } else if (s->delivered && s->context.epoch>s->last_poll_epoch) {
            /* One new delivered phase at most per poll, after at least one
               new fighter observation. Extra observations cannot skip an
               unpolled phase. Repeated polls in one epoch repeat its output. */
            if (s->phase==KZ_RELEASE) stop(s,KZ_EVENT_INPUT_COMPLETE);
            else { ++s->phase; s->delivered=0; s->phase_started_ms=in->now_ms; }
        }
    } else if (rising && s->armed && s->context.actionable &&
               (s->context.facing==1 || s->context.facing==-1)) {
        s->armed=0;
        if (in->enabled) {
            s->phase=KZ_FORWARD; s->facing=s->context.facing;
            s->delivered=0; s->suppress_until_release=1;
            s->phase_started_ms=in->now_ms;
            s->pending_event=KZ_EVENT_STARTED;
        } else s->pending_event=KZ_EVENT_DIAGNOSTIC;
    }
    if (in->enabled && s->suppress_until_release) {
        out->buttons &= ~KZ_RIGHT; out->consume_right=1;
    }
    if (s->phase) {
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
