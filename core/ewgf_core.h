#ifndef KZ_EWGF_CORE_H
#define KZ_EWGF_CORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* No game memory or API assumptions live in this layer. */
enum { KZ_IDLE=0, KZ_FORWARD=1, KZ_NEUTRAL=2, KZ_DOWN=3, KZ_DIAGONAL_ATTACK=4, KZ_RELEASE=5 };
enum { KZ_EVENT_NONE=0, KZ_EVENT_STARTED=1, KZ_EVENT_INPUT_COMPLETE=2,
       KZ_EVENT_CANCELLED=3, KZ_EVENT_DIAGNOSTIC=4, KZ_EVENT_BAD_CLOCK=5 };
typedef struct {
    uint64_t epoch, now_ms, actor;
    int32_t permitted, actionable, can_continue, facing;
} KzContext;
typedef struct {
    uint64_t buttons;
    int32_t lx, ly, rx, ry, connected, matching_pad, training_now, enabled;
    uint64_t now_ms;
} KzPoll;
typedef struct {
    uint64_t buttons;
    int32_t lx, ly, rx, ry, owns_input, consume_right, event, phase;
} KzOutput;
typedef struct {
    KzContext context;
    uint64_t last_poll_epoch, phase_started_ms;
    int32_t phase, facing, delivered, right_was_down, suppress_until_release;
    int32_t seen_context, armed, pending_event, initialized;
} KzState;
#define KZ_RIGHT (UINT64_C(1) << 14)
#define KZ_A UINT64_C(1)
/* Physical non-D-pad buttons, plus other D-pad directions, abort the pulse.
   These are Horizon Npad button bits, not Smash logical action flags. */
#define KZ_CANCEL_MASK (UINT64_C(0xFFFF) & ~(KZ_RIGHT))
void kz_init(KzState *s);
void kz_observe(KzState *s, const KzContext *ctx);
void kz_poll(KzState *s, const KzPoll *in, KzOutput *out);
void kz_poll_fast(KzState *s, const KzPoll *in, KzOutput *out);
uint32_t kz_state_size(void);
#ifdef __cplusplus
}
#endif
#endif
