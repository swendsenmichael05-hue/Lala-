#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Context {
    pub epoch:u64, pub now_ms:u64, pub actor:u64,
    pub permitted:i32, pub actionable:i32, pub can_continue:i32, pub facing:i32,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Poll {
    pub buttons:u64,
    pub lx:i32, pub ly:i32, pub rx:i32, pub ry:i32,
    pub connected:i32, pub matching_pad:i32, pub training_now:i32, pub enabled:i32,
    pub now_ms:u64,
}
#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct Output {
    pub buttons:u64,
    pub lx:i32, pub ly:i32, pub rx:i32, pub ry:i32,
    pub owns_input:i32, pub consume_right:i32, pub event:i32, pub phase:i32,
}
#[repr(C)]
#[derive(Default)]
pub struct State {
    pub context:Context, pub last_poll_epoch:u64,
    pub phase:i32, pub facing:i32, pub delivered:i32, pub right_was_down:i32,
    pub suppress_until_release:i32, pub seen_context:i32, pub armed:i32,
    pub pending_event:i32, pub initialized:i32,
}
extern "C" {
    pub fn kz_init(s:*mut State);
    pub fn kz_observe(s:*mut State, ctx:*const Context);
    pub fn kz_poll(s:*mut State, input:*const Poll, out:*mut Output);
    pub fn kz_state_size()->u32;
}
