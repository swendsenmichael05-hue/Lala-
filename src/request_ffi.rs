use crate::core_ffi::{Context, State as PulseState, Poll, Output};
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct RequestContext {
    pub base: Context,
    pub queueable: i32,
    pub brake: i32,
}
#[repr(C)]
#[derive(Default)]
pub struct RequestState {
    pub core: PulseState,
    pub context: RequestContext,
    pub queued_at_ms: u64, pub last_attempt_ms: u64,
    pub seen: i32, pub right_was_down: i32, pub armed: i32, pub queued: i32,
    pub repeat_latched: i32, pub recovering: i32, pub saw_busy: i32, pub pending_event: i32,
}
extern "C" {
    pub fn kz_request_init(s: *mut RequestState);
    pub fn kz_request_observe(s: *mut RequestState, ctx: *const RequestContext);
    pub fn kz_request_poll(s: *mut RequestState, input: *const Poll, out: *mut Output);
    pub fn kz_request_state_size() -> u32;
}
