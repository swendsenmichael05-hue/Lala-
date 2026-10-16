#![allow(dead_code)]
#[path="../src/action_gate.rs"] mod action_gate;
#[path="../src/gate_patch.rs"] mod gate_patch;
#[path="../src/core_ffi.rs"] mod core_ffi;
#[path="../src/request_ffi.rs"] mod request_ffi;
use gate_patch::{Facts, Gate, ElectricEntry, trigger_buttons, RIGHT};
use core_ffi::{Context, Poll, Output};
use request_ffi::{RequestState, RequestContext};
fn idle() -> Facts {Facts {actor:7,training:true,grounded:true,idle:true,fast_movement:true,..Facts::default()}}
fn air() -> Facts {Facts {grounded:false,idle:false,nair_air:true,..idle()}}
fn landing() -> Facts {Facts {idle:false,nair_landing:true,..idle()}}
fn dash() -> Facts {Facts {idle:false,movement:true,direct_movement:true,..idle()}}
fn electric() -> Facts {Facts {idle:false,electric_recovery:true,ewgf:true,attack_sent:true,..idle()}}
struct Harness {g:Gate,s:RequestState,p:Poll,epoch:u64,d:gate_patch::Decision,fast:bool}
impl Harness {
    fn new() -> Self {
        let mut h=Self {g:Gate::default(),s:RequestState::default(),p:Poll {connected:1,matching_pad:1,training_now:1,enabled:1,now_ms:100,..Poll::default()},epoch:0,d:Default::default(),fast:true};
        unsafe {request_ffi::kz_request_init(&mut h.s);}
        h.observe(idle());h.poll(0);h
    }
    fn observe(&mut self,f:Facts) {
        self.epoch+=1;self.p.now_ms+=16;
        self.d=self.g.observe(f,self.fast && f.electric_recovery);
        let c=RequestContext {base:Context {epoch:self.epoch,now_ms:self.p.now_ms,actor:f.actor,permitted:self.d.permitted as i32,actionable:self.d.actionable as i32,can_continue:self.d.can_continue as i32,facing:1},queueable:self.d.queueable as i32,brake:self.d.brake as i32};
        unsafe {request_ffi::kz_request_observe(&mut self.s,&c);}
    }
    fn poll(&mut self,raw:u64) -> Output {
        let scope=self.p.enabled!=0&&self.p.training_now!=0&&self.p.connected!=0&&self.p.matching_pad!=0&&self.d.permitted;
        self.p.buttons=trigger_buttons(raw,scope);
        let mut o=Output::default();
        unsafe {request_ffi::kz_request_poll_ex(&mut self.s,&self.p,&mut o,1,self.d.movement_ready as i32);}
        o
    }
    fn complete(&mut self) {
        self.observe(dash());assert_eq!(self.poll(RIGHT).phase,3);
        self.observe(dash());assert_eq!(self.poll(RIGHT).phase,4);
        self.observe(electric());assert_eq!(self.poll(RIGHT).phase,5);
        self.observe(electric());assert_eq!(self.poll(RIGHT).event,2);
    }
}
#[test] fn pending_nair_survives_ground_before_landing_status() {
    let mut h=Harness::new();h.observe(air());assert_eq!(h.poll(RIGHT).event,10);
    h.observe(Facts {grounded:true,native_cancel:true,..air()});
    let o=h.poll(RIGHT);assert_eq!(o.owns_input,0);assert_eq!(h.s.queued,1);assert_ne!(o.event,3);
    h.observe(landing());assert_eq!(h.poll(RIGHT).owns_input,0);assert_eq!(h.s.queued,1);
    h.observe(Facts {native_cancel:true,..landing()});assert_eq!(h.poll(0).event,1);
    h.complete();
}
#[test] fn pending_nair_survives_landing_status_before_ground() {
    let mut h=Harness::new();h.observe(air());h.poll(RIGHT);
    h.observe(Facts {grounded:false,native_cancel:true,..landing()});
    let o=h.poll(0);assert_eq!(o.owns_input,0);assert_eq!(h.s.queued,1);
    h.observe(landing());assert_eq!(h.poll(0).owns_input,0);
    h.observe(Facts {native_cancel:true,..landing()});assert_eq!(h.poll(0).event,1);
}
#[test] fn released_air_request_keeps_original_expiry() {
    let mut h=Harness::new();h.observe(air());h.poll(RIGHT);
    for _ in 0..70 {h.observe(Facts {grounded:true,..air()});assert_eq!(h.poll(0).owns_input,0);}
    assert_eq!(h.s.queued,0);h.observe(Facts {native_cancel:true,..landing()});assert_eq!(h.poll(0).owns_input,0);
}
#[test] fn unrelated_aerial_still_cancels() {
    let mut h=Harness::new();h.observe(air());h.poll(RIGHT);
    h.observe(Facts {nair_air:false,..air()});h.poll(RIGHT);assert_eq!(h.s.queued,0);
}
#[test] fn diagonal_right_down_starts_and_survives_pulse() {
    let mut h=Harness::new();assert_eq!(h.poll(RIGHT|(1<<15)).event,1);
    h.observe(dash());let o=h.poll(RIGHT|(1<<15));assert_eq!(o.phase,3);assert_eq!(o.buttons,0);
}
#[test] fn diagonal_right_up_can_buffer_in_nair() {
    let mut h=Harness::new();h.observe(air());assert_eq!(h.poll(RIGHT|(1<<13)).event,10);assert_eq!(h.s.queued,1);
}
#[test] fn diagonal_does_not_hide_manual_attack() {
    let mut h=Harness::new();h.observe(air());h.poll(RIGHT);
    h.poll(RIGHT|(1<<15)|1);assert_eq!(h.s.queued,0);
}
#[test] fn standalone_down_and_left_keep_manual_priority() {
    assert_eq!(trigger_buttons(1<<15,true),1<<15);
    assert_eq!(trigger_buttons(RIGHT|(1<<12),true),RIGHT|(1<<12));
    let mut h=Harness::new();h.observe(landing());h.poll(RIGHT);h.poll(1<<15);assert_eq!(h.s.queued,0);
}
#[test] fn diagonal_disabled_scope_is_byte_preserving() {
    for scope in [false,true] {for low in 0..65536u64 {
        let out=trigger_buttons(low,scope);
        if !scope||low&RIGHT==0 {assert_eq!(low,out);} else {assert_eq!(out,low&!((1<<13)|(1<<15)));}
    }}
}
#[test] fn repeat_waits_for_native_permission_not_animation_end() {
    let mut h=Harness::new();assert_eq!(h.poll(RIGHT).event,1);h.complete();
    for _ in 0..5 {h.observe(electric());assert_eq!(h.poll(RIGHT).owns_input,0);}
    h.observe(Facts {native_cancel:true,..electric()});assert_eq!(h.poll(RIGHT).event,1);
}
#[test] fn legacy_recovery_option_still_waits_for_idle() {
    let mut h=Harness::new();h.fast=false;h.poll(RIGHT);h.complete();
    h.observe(Facts {native_cancel:true,..electric()});assert_eq!(h.poll(RIGHT).owns_input,0);
    h.observe(idle());assert_eq!(h.poll(RIGHT).event,1);
}
#[test] fn hitlag_blocks_recovery_start() {
    let mut h=Harness::new();h.poll(RIGHT);h.complete();
    h.observe(Facts {native_cancel:true,stopped:true,..electric()});assert_eq!(h.poll(RIGHT).owns_input,0);
    h.observe(Facts {native_cancel:true,..electric()});assert_eq!(h.poll(RIGHT).event,1);
}
#[test] fn native_permission_does_not_allow_unrelated_attack() {
    let mut h=Harness::new();h.observe(Facts {idle:false,native_cancel:true,..idle()});assert_eq!(h.poll(RIGHT).owns_input,0);
}
#[test] fn old_electric_cannot_confirm_new_attempt() {
    let mut e=ElectricEntry::default();e.start(true,40.0);
    for f in 41..50 {assert!(!e.observe(true,f as f32,true));}
    assert!(!e.observe(false,0.0,false));assert!(e.observe(true,0.0,true));
}
#[test] fn entry_can_be_detected_by_new_motion_start() {
    let mut e=ElectricEntry::default();e.start(true,40.0);
    assert!(!e.observe(true,41.0,false));assert!(e.observe(true,0.0,true));
}
#[test] fn entry_needs_attack_and_clear_removes_old_confirmation() {
    let mut e=ElectricEntry::default();e.start(false,0.0);
    assert!(!e.observe(true,0.0,false));assert!(e.observe(true,1.0,true));
    e.clear();assert!(!e.observe(true,2.0,true));
}
#[test] fn nair_normal_fall_ground_snapshot_preserves_request() {
    let mut h=Harness::new();h.observe(air());h.poll(RIGHT);
    h.observe(Facts {nair_air:false,normal_fall:true,..air()});h.poll(0);
    h.observe(Facts {nair_air:false,normal_fall:true,grounded:true,..air()});
    assert_eq!(h.poll(0).owns_input,0);assert_eq!(h.s.queued,1);
    h.observe(Facts {native_cancel:true,..landing()});assert_eq!(h.poll(0).event,1);
}
