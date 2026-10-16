//! Offline v0.2.7: nair touchdown continuity, directional-trigger priority,
//! and native electric recovery readiness. The C input pulse is unchanged.
mod abi;
mod core_ffi;
mod request_ffi;
mod action_gate;
mod gate_patch;

use core_ffi::{Context, Output, Poll};
use request_ffi::{RequestContext, RequestState as State};
use gate_patch::{Facts, Gate, Decision, ElectricEntry, trigger_buttons, RIGHT};
use smash::{app::{lua_bind::*, smashball, sv_information}, lib::lua_const::*, lua2cpp::L2CFighterCommon};
use smashline::{Agent, Main};
use std::{fs::{self, OpenOptions}, io::Write, sync::{atomic::{AtomicBool, Ordering}, mpsc::{sync_channel, SyncSender}, Mutex, OnceLock}, time::Instant};

const EXPECTED_VERSION:&str="13.0.5";
const CONFIG_PATH:&str="sd:/ultimate/kazuya_lab/config.ini";
const LOG_PATH:&str="sd:/ultimate/kazuya_lab/runtime.log";
const HID_ID:u32=0;
const FIGHTER_ENTRY:i32=0;
static RUNTIME:OnceLock<Runtime>=OnceLock::new();
static CANCEL_PENDING:AtomicBool=AtomicBool::new(false);
#[derive(Clone,Copy)]
struct Event {
    time:u64,kind:i32,epoch:u64,buttons:u64,phase:i32,status:i32,
    motion:u64,motion_frame:f32,x:i32,y:i32,since_start_ms:i64,
    request_wait_ms:i64,queued_age_ms:i64,grounded:bool,native_cancel:bool,
    decision:Decision,recovering:bool,
}
struct Shared {
    core:State,gate:Gate,decision:Decision,entry:ElectricEntry,epoch:u64,status:i32,
    motion:u64,motion_frame:f32,last_output_epoch:u64,started_ms:u64,
    attack_sent:bool,awaiting_electric:bool,grounded:bool,native_cancel:bool,
    pending_request_ms:Option<u64>,request_wait_ms:i64,last_raw_buttons:u64,
    recovery_cancel_ready:bool,
}
struct Runtime {
    start:Instant,enabled:bool,fast_electric:bool,fast_movement:bool,
    fast_recovery:bool,diagonal_priority:bool,shared:Mutex<Shared>,
    logger:SyncSender<Event>,seen_pad:AtomicBool,
}
impl Runtime {
    fn now(&self)->u64 {self.start.elapsed().as_millis().min(u64::MAX as u128) as u64}
    fn queue(&self,s:&Shared,kind:i32,buttons:u64,phase:i32,x:i32,y:i32) {
        let now=self.now();
        let age=|t:u64|now.saturating_sub(t).min(i64::MAX as u64) as i64;
        let _=self.logger.try_send(Event {time:now,kind,epoch:s.epoch,buttons,phase,
            status:s.status,motion:s.motion,motion_frame:s.motion_frame,x,y,
            since_start_ms:if s.started_ms==0 {-1} else {age(s.started_ms)},
            request_wait_ms:if kind==10 {0} else {s.request_wait_ms},
            queued_age_ms:if s.core.queued!=0 {age(s.core.queued_at_ms)} else {-1},
            grounded:s.grounded,native_cancel:s.native_cancel,decision:s.decision,
            recovering:s.core.recovering!=0});
    }
}
fn log_line(text:&str) {
    skyline::println!("[kazuya-lab] {}",text);
    if let Ok(mut f)=OpenOptions::new().create(true).append(true).open(LOG_PATH) {
        let _=writeln!(f,"{}",text);
    }
}
fn config_bool(text:&str,key:&str,fallback:bool)->bool {
    let mut value=fallback;
    for line in text.lines().map(str::trim) {
        if line.starts_with('#')||line.starts_with(';')||line.is_empty() {continue;}
        if let Some((k,v))=line.split_once('=') {if k.trim()==key {value=v.trim()=="true";}}
    }
    value
}
unsafe fn version()->String {
    let mut v=abi::DisplayVersion {name:[0;16]};abi::get_display_version(&mut v);
    let end=v.name.iter().position(|&x|x==0).unwrap_or(v.name.len());
    String::from_utf8_lossy(&v.name[..end]).into_owned()
}
unsafe fn training_now()->bool {smashball::is_training_mode()&&sv_information::is_ready_go()}
fn reset_shared(s:&mut Shared) {
    unsafe {request_ffi::kz_request_init(&mut s.core);}
    s.gate=Gate::default();s.decision=Decision::default();s.entry.clear();
    s.attack_sent=false;s.awaiting_electric=false;s.pending_request_ms=None;
    s.request_wait_ms=-1;s.recovery_cancel_ready=false;
}
unsafe extern "C" fn kazuya_frame(fighter:&mut L2CFighterCommon) {
    let Some(rt)=RUNTIME.get() else {return;};
    let module=fighter.module_accessor;if module.is_null() {return;}
    if WorkModule::get_int(module,*FIGHTER_INSTANCE_WORK_ID_INT_ENTRY_ID)!=FIGHTER_ENTRY {return;}
    let training=training_now();
    let grounded=StatusModule::situation_kind(module)==*SITUATION_KIND_GROUND;
    let stopped=StopModule::is_stop(module);
    let status=StatusModule::status_kind(module);
    let motion=MotionModule::motion_kind(module);
    let motion_frame=MotionModule::frame(module);
    let native_cancel=CancelModule::is_enable_cancel(module);
    let idle=status==*FIGHTER_STATUS_KIND_WAIT;
    let movement=[*FIGHTER_STATUS_KIND_WALK,*FIGHTER_STATUS_KIND_DASH,
        *FIGHTER_STATUS_KIND_RUN,*FIGHTER_STATUS_KIND_RUN_BRAKE,
        *FIGHTER_STATUS_KIND_TURN,*FIGHTER_STATUS_KIND_TURN_DASH,
        *FIGHTER_STATUS_KIND_TURN_RUN,*FIGHTER_STATUS_KIND_TURN_RUN_BRAKE,
        *FIGHTER_DEMON_STATUS_KIND_DASH_BACK,*FIGHTER_DEMON_STATUS_KIND_TURN_AUTO,
        *FIGHTER_DEMON_STATUS_KIND_WALK_BACK,*FIGHTER_DEMON_STATUS_KIND_WALK_BRAKE_BACK,
        *FIGHTER_DEMON_STATUS_KIND_TURN_RUN_BACK].contains(&status);
    let direct_movement=[*FIGHTER_STATUS_KIND_WALK,*FIGHTER_STATUS_KIND_DASH,
        *FIGHTER_STATUS_KIND_RUN,*FIGHTER_STATUS_KIND_RUN_BRAKE,
        *FIGHTER_DEMON_STATUS_KIND_DASH_BACK,*FIGHTER_DEMON_STATUS_KIND_WALK_BACK,
        *FIGHTER_DEMON_STATUS_KIND_WALK_BRAKE_BACK].contains(&status);
    let ewgf=status==*FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2F;
    let wgf=status==*FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2;
    let electric_recovery=ewgf||wgf||status==*FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP;
    let nair_motion=motion==smash::hash40("attack_air_n");
    let nair_air=status==*FIGHTER_STATUS_KIND_ATTACK_AIR&&nair_motion;
    let landing_status=status==*FIGHTER_STATUS_KIND_LANDING_ATTACK_AIR;
    let landing_motion=motion==smash::hash40("landing_air_n");
    // Landing status may be visible before its motion is updated. This bridge
    // retains a known nair request but never grants cancel using the old motion.
    let nair_landing=landing_status&&(landing_motion||nair_motion);
    let gate_cancel=native_cancel&&!(landing_status&&nair_motion);
    let normal_fall=status==*FIGHTER_STATUS_KIND_FALL||status==*FIGHTER_STATUS_KIND_FALL_AERIAL;
    let normal_landing=status==*FIGHTER_STATUS_KIND_LANDING||status==*FIGHTER_STATUS_KIND_LANDING_LIGHT;
    let recovery_cancel_ready=rt.fast_recovery&&training&&grounded&&!stopped&&
        (ewgf||wgf)&&native_cancel;
    let lr=PostureModule::lr(module);
    let facing=if lr>0.5 {1} else if lr< -0.5 {-1} else {0};
    let Ok(mut s)=rt.shared.try_lock() else {CANCEL_PENDING.store(true,Ordering::Release);return;};
    if CANCEL_PENDING.swap(false,Ordering::AcqRel) {reset_shared(&mut s);}
    let previous=s.decision;
    let previous_recovery_ready=s.recovery_cancel_ready;
    let attack_sent=s.attack_sent;
    let new_electric=s.entry.observe(ewgf,motion_frame,attack_sent);
    let decision=s.gate.observe(Facts {actor:module as usize as u64,training,grounded,
        stopped,idle,movement,direct_movement,electric_recovery,ewgf,attack_sent:new_electric,
        nair_air,nair_landing,normal_fall,normal_landing,native_cancel:gate_cancel,
        fast_movement:rt.fast_movement},rt.fast_recovery&&(ewgf||wgf));
    s.decision=decision;s.grounded=grounded;s.native_cancel=native_cancel;
    s.recovery_cancel_ready=recovery_cancel_ready;s.epoch=s.epoch.wrapping_add(1);
    let context=Context {epoch:s.epoch,now_ms:rt.now(),actor:module as usize as u64,
        permitted:decision.permitted as i32,actionable:decision.actionable as i32,
        can_continue:decision.can_continue as i32,facing};
    s.status=status;s.motion=motion;s.motion_frame=motion_frame;
    request_ffi::kz_request_observe(&mut s.core,&RequestContext {base:context,
        queueable:decision.queueable as i32,brake:decision.brake as i32});
    if decision.recovered_to_movement&&!previous.recovered_to_movement {rt.queue(&s,302,0,0,0,0);}
    if decision.landing_cancel_ready&&!previous.landing_cancel_ready {rt.queue(&s,301,0,0,0,0);}
    if recovery_cancel_ready&&!previous_recovery_ready {rt.queue(&s,305,0,0,0,0);}
    if s.awaiting_electric&&attack_sent&&training&&new_electric {
        rt.queue(&s,300,0,0,0,0);s.awaiting_electric=false;
    }
    let non_electric=wgf||[*FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_1,
        *FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_2,*FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_3,
        *FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_4,*FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_5,
        *FIGHTER_DEMON_STATUS_KIND_ATTACK_STAND_6].contains(&status);
    if s.awaiting_electric&&attack_sent&&training&&non_electric {
        rt.queue(&s,304,0,0,0,0);s.awaiting_electric=false;
    }
    if !training||!grounded||!decision.queueable||
       (s.started_ms>0&&rt.now().saturating_sub(s.started_ms)>500) {
        s.awaiting_electric=false;
        if !ewgf {s.attack_sent=false;s.entry.clear();}
    }
}
#[skyline::hook(replace = abi::get_gc_state)]
unsafe fn gc_state_hook(out:*mut abi::PadPrefix,id:*const u32) {
    original!()(out,id);
    if out.is_null()||id.is_null()||*id!=HID_ID {return;}
    let Some(rt)=RUNTIME.get() else {return;};
    let allowed=training_now();
    let Ok(mut s)=rt.shared.try_lock() else {CANCEL_PENDING.store(true,Ordering::Release);return;};
    if CANCEL_PENDING.swap(false,Ordering::AcqRel) {reset_shared(&mut s);}
    if !rt.seen_pad.swap(true,Ordering::AcqRel) {rt.queue(&s,100,(*out).buttons,0,0,0);}
    let raw_buttons=(*out).buttons;
    let now=rt.now();
    let fresh=s.core.seen!=0&&now>=s.core.context.base.now_ms&&now-s.core.context.base.now_ms<=80;
    let scope=rt.enabled&&allowed&&((*out).attributes&1)!=0&&fresh&&s.core.context.base.permitted!=0;
    let buttons=trigger_buttons(raw_buttons,scope&&rt.diagonal_priority);
    if buttons!=raw_buttons&&raw_buttons!=s.last_raw_buttons {rt.queue(&s,303,raw_buttons,0,0,0);}
    s.last_raw_buttons=raw_buttons;
    let input=Poll {buttons,lx:(*out).lx,ly:(*out).ly,rx:(*out).rx,ry:(*out).ry,
        connected:(((*out).attributes&1)!=0) as i32,matching_pad:1,training_now:allowed as i32,
        enabled:rt.enabled as i32,now_ms:now};
    let fresh_press=(buttons&RIGHT)!=0&&s.core.right_was_down==0;
    let direct=s.decision.movement_ready as i32;
    let mut output=Output::default();
    request_ffi::kz_request_poll_ex(&mut s.core,&input,&mut output,rt.fast_electric as i32,direct);
    if output.event==10&&fresh_press {s.pending_request_ms=Some(now);}
    if output.event==1 {
        s.request_wait_ms=if fresh_press {0} else {
            s.pending_request_ms.map(|t|now.saturating_sub(t).min(i64::MAX as u64) as i64).unwrap_or(-1)
        };
        s.pending_request_ms=None;s.started_ms=now;
        s.gate.reset_attempt();s.attack_sent=false;s.awaiting_electric=true;
        let was_electric=s.status==*FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2F;
        let frame=s.motion_frame;s.entry.start(was_electric,frame);
    }
    if output.owns_input!=0&&output.phase==4 {s.attack_sent=true;}
    if matches!(output.event,3|5|11|12|13) {
        s.pending_request_ms=None;
        if matches!(output.event,3|5|12|13) {
            s.awaiting_electric=false;s.attack_sent=false;s.gate.reset_attempt();s.entry.clear();
        }
    }
    if output.event!=0 {rt.queue(&s,output.event,raw_buttons,output.phase,0,0);}
    if output.owns_input!=0&&s.last_output_epoch!=s.epoch {
        rt.queue(&s,200,output.buttons,output.phase,output.lx,output.ly);s.last_output_epoch=s.epoch;
    }
    // Native sampling number, attributes, padding and GC suffix stay untouched.
    if output.owns_input!=0||output.consume_right!=0 {
        (*out).buttons=output.buttons;(*out).lx=output.lx;(*out).ly=output.ly;
        (*out).rx=output.rx;(*out).ry=output.ry;
    }
}
#[skyline::main(name = "kazuya_training_lab")]
pub fn main() {
    let _=fs::create_dir_all("sd:/ultimate/kazuya_lab");
    let detected=unsafe {version()};
    log_line(&format!("v0.2.7 INPUT GATE PATCH | detected Smash {} | expected {}",detected,EXPECTED_VERSION));
    if detected!=EXPECTED_VERSION {log_line("VERSION MISMATCH: no hook or fighter callback installed.");return;}
    if unsafe {request_ffi::kz_request_state_size()} as usize!=std::mem::size_of::<State>()||
       std::mem::size_of::<abi::PadPrefix>()!=40 {log_line("ABI SIZE MISMATCH: no hook installed.");return;}
    let config=fs::read_to_string(CONFIG_PATH).unwrap_or_default();
    let enabled=config_bool(&config,"enable_macro",false);
    let fast_electric=config_bool(&config,"fast_electric",true);
    let fast_movement=config_bool(&config,"fast_movement",true);
    let fast_recovery=config_bool(&config,"fast_recovery",true);
    let diagonal_priority=config_bool(&config,"right_diagonal_priority",true);
    let (tx,rx)=sync_channel::<Event>(128);
    if std::thread::Builder::new().name("kazuya-lab-log".into()).stack_size(128*1024).spawn(move || {
        while let Ok(e)=rx.recv() {
            let name=match e.kind {
                1=>"INPUT_SEQUENCE_STARTED",2=>"INPUT_SEQUENCE_FINISHED_NOT_HIT_CONFIRMED",
                3=>"CANCELLED",4=>"DIAGNOSTIC_RIGHT_PRESS",5=>"CLOCK_OR_POLL_GAP",
                10=>"REQUEST_BUFFERED",11=>"REQUEST_EXPIRED",12=>"REQUEST_BLOCKED_STATE",
                13=>"RECOVERY_WAIT_TIMEOUT",100=>"GC_HID_ID_0_SEEN",200=>"OUTPUT_FRAME",
                300=>"EWGF_STATE_OBSERVED_NOT_HIT_CONFIRMED",
                301=>"NAIR_LANDING_NATIVE_CANCEL_READY",302=>"EWGF_RECOVERY_EXIT_TO_MOVEMENT",
                303=>"DIAGONAL_TRIGGER_NORMALIZED",304=>"NON_ELECTRIC_ATTACK_OBSERVED",
                305=>"ELECTRIC_NATIVE_CANCEL_READY",_=>"OTHER",
            };
            log_line(&format!("t={} event={} epoch={} buttons={:#x} phase={} status={} motion={:#x} motion_frame={:.2} output_x={} output_y={} since_start_ms={} request_wait_ms={} queued_age_ms={} grounded={} native_cancel={} nair_buffer={} ready={} recovering={}",
                e.time,name,e.epoch,e.buttons,e.phase,e.status,e.motion,e.motion_frame,e.x,e.y,
                e.since_start_ms,e.request_wait_ms,e.queued_age_ms,e.grounded,e.native_cancel,
                e.decision.nair_buffer,e.decision.actionable||e.decision.movement_ready,e.recovering));
        }
    }).is_err() {log_line("LOG THREAD FAILED: no hook installed.");return;}
    let mut core=State::default();unsafe {request_ffi::kz_request_init(&mut core);}
    if RUNTIME.set(Runtime {start:Instant::now(),enabled,fast_electric,fast_movement,fast_recovery,diagonal_priority,
        shared:Mutex::new(Shared {core,gate:Gate::default(),decision:Decision::default(),entry:ElectricEntry::default(),epoch:0,
            status:0,motion:0,motion_frame:0.0,last_output_epoch:0,started_ms:0,attack_sent:false,
            awaiting_electric:false,grounded:false,native_cancel:false,pending_request_ms:None,
            request_wait_ms:-1,last_raw_buttons:0,recovery_cancel_ready:false}),logger:tx,seen_pad:AtomicBool::new(false)}).is_err() {return;}
    log_line(&format!("FAST_ELECTRIC={}; FAST_MOVEMENT={}; FAST_RECOVERY={}; RIGHT_DIAGONAL_PRIORITY={}.",fast_electric,fast_movement,fast_recovery,diagonal_priority));
    log_line("NAIR_TOUCHDOWN_BRIDGE=ENABLED; pulse unchanged; no airborne injection; tap expiry=1000ms.");
    log_line("Recovery starts use existing native cancel permission, never enable_cancel, forced status or faster animations.");
    log_line("Known non-electric outcomes need console testing; input completion is not move or hit confirmation.");
    log_line("Release A/jump before Right; manual attack/shield/jump cancels. A queued tap may execute within 1000ms.");
    log_line(if enabled {"EXPERIMENTAL INPUT ENABLED; OFFLINE TRAINING ONLY; P1/ID0."} else {"DIAGNOSTICS ONLY; no macro input changes."});
    skyline::install_hooks!(gc_state_hook);
    Agent::new("demon").on_line(Main,kazuya_frame).install();
}
