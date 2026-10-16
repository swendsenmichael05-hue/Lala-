//! Offline training adapter. Idle legacy electrics were user-tested in v0.2.2.
//! v0.2.5 compact input and direct movement starts are experimental until tested.
mod abi;
mod core_ffi;
mod request_ffi;

use core_ffi::{Context, Output, Poll};
use request_ffi::{RequestContext, RequestState as State};
use smash::{app::{lua_bind::*, smashball, sv_information}, lib::lua_const::*, lua2cpp::L2CFighterCommon};
use smashline::{Agent, Main};
use std::{fs::{self, OpenOptions}, io::Write, sync::{atomic::{AtomicBool, Ordering}, mpsc::{sync_channel, SyncSender}, Mutex, OnceLock}, time::Instant};

const EXPECTED_VERSION: &str = "13.0.5";
const CONFIG_PATH: &str = "sd:/ultimate/kazuya_lab/config.ini";
const LOG_PATH: &str = "sd:/ultimate/kazuya_lab/runtime.log";
// Deliberately narrow initial mapping. Must be verified in diagnostics on the
// user's console; fighter entry 0 and HID ID 0 are NOT inherently identical.
const HID_ID: u32 = 0;
const FIGHTER_ENTRY: i32 = 0;
static RUNTIME: OnceLock<Runtime> = OnceLock::new();
static CANCEL_PENDING: AtomicBool = AtomicBool::new(false);

#[derive(Clone, Copy)]
struct Event {
    time: u64, kind: i32, epoch: u64, buttons: u64,
    phase: i32, status: i32, motion: u64, motion_frame: f32, x:i32, y:i32, since_start_ms:i64,
}
struct Shared { core: State, epoch: u64, status: i32, motion: u64, motion_frame: f32, last_output_epoch:u64, movement_ready:bool, started_ms:u64, attack_sent:bool, awaiting_electric:bool }
struct Runtime {
    start: Instant, enabled: bool, fast_electric:bool, fast_movement:bool, shared: Mutex<Shared>,
    logger: SyncSender<Event>, seen_pad: AtomicBool,
}
impl Runtime {
    fn now(&self) -> u64 { self.start.elapsed().as_millis().min(u64::MAX as u128) as u64 }
    fn queue(&self, s: &Shared, kind: i32, buttons: u64, phase: i32) {
        // Bounded nonblocking queue: no formatting, SD I/O, or sleeps in hooks.
        let _ = self.logger.try_send(Event { time:self.now(), kind, epoch:s.epoch,
            buttons, phase, status:s.status, motion:s.motion, motion_frame:s.motion_frame, x:0, y:0,
            since_start_ms:if s.started_ms==0 {-1} else {self.now().saturating_sub(s.started_ms).min(i64::MAX as u64) as i64} });
    }
    fn queue_output(&self, s: &Shared, out: &Output) {
        let _ = self.logger.try_send(Event { time:self.now(), kind:200, epoch:s.epoch,
            buttons:out.buttons, phase:out.phase, status:s.status, motion:s.motion,
            motion_frame:s.motion_frame, x:out.lx, y:out.ly,
            since_start_ms:if s.started_ms==0 {-1} else {self.now().saturating_sub(s.started_ms).min(i64::MAX as u64) as i64} });
    }
}
fn log_line(text: &str) {
    skyline::println!("[kazuya-lab] {}", text);
    if let Ok(mut file) = OpenOptions::new().create(true).append(true).open(LOG_PATH) {
        let _ = writeln!(file, "{}", text);
    }
}
fn config_bool(text:&str, key:&str, fallback:bool) -> bool {
    let mut result=fallback;
    for line in text.lines().map(str::trim) {
        if line.starts_with('#') || line.starts_with(';') || line.is_empty() { continue; }
        if let Some((k,v))=line.split_once('=') {
            if k.trim()==key { result=v.trim()=="true"; }
        }
    }
    result
}
unsafe fn version() -> String {
    let mut v = abi::DisplayVersion { name:[0;16] };
    abi::get_display_version(&mut v);
    let end = v.name.iter().position(|&x| x == 0).unwrap_or(v.name.len());
    String::from_utf8_lossy(&v.name[..end]).into_owned()
}
unsafe fn training_now() -> bool {
    smashball::is_training_mode() && sv_information::is_ready_go()
}

unsafe extern "C" fn kazuya_frame(fighter: &mut L2CFighterCommon) {
    let Some(rt) = RUNTIME.get() else { return; };
    let module = fighter.module_accessor;
    if module.is_null() { return; }
    let entry = WorkModule::get_int(module, *FIGHTER_INSTANCE_WORK_ID_INT_ENTRY_ID);
    if entry != FIGHTER_ENTRY { return; }
    // All game API calls happen before obtaining the core mutex.
    let training = training_now();
    let grounded = StatusModule::situation_kind(module) == *SITUATION_KIND_GROUND;
    let stopped = StopModule::is_stop(module);
    let status = StatusModule::status_kind(module);
    let actionable = status == *FIGHTER_STATUS_KIND_WAIT;
    let movement = [*FIGHTER_STATUS_KIND_WALK, *FIGHTER_STATUS_KIND_DASH,
        *FIGHTER_STATUS_KIND_RUN, *FIGHTER_STATUS_KIND_RUN_BRAKE,
        *FIGHTER_STATUS_KIND_TURN, *FIGHTER_STATUS_KIND_TURN_DASH,
        *FIGHTER_STATUS_KIND_TURN_RUN, *FIGHTER_STATUS_KIND_TURN_RUN_BRAKE,
        *FIGHTER_DEMON_STATUS_KIND_DASH_BACK, *FIGHTER_DEMON_STATUS_KIND_TURN_AUTO,
        *FIGHTER_DEMON_STATUS_KIND_WALK_BACK, *FIGHTER_DEMON_STATUS_KIND_WALK_BRAKE_BACK,
        *FIGHTER_DEMON_STATUS_KIND_TURN_RUN_BACK].contains(&status);
    // Only non-turning locomotion can take the experimental direct-start path.
    // This sends controller inputs; it never forces or enables a game status.
    let movement_ready = rt.fast_movement && training && grounded && !stopped &&
        [*FIGHTER_STATUS_KIND_WALK, *FIGHTER_STATUS_KIND_DASH,
        *FIGHTER_STATUS_KIND_RUN, *FIGHTER_STATUS_KIND_RUN_BRAKE,
        *FIGHTER_DEMON_STATUS_KIND_DASH_BACK, *FIGHTER_DEMON_STATUS_KIND_WALK_BACK,
        *FIGHTER_DEMON_STATUS_KIND_WALK_BRAKE_BACK].contains(&status);
    let electric_recovery = status == *FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2F ||
        status == *FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2 ||
        status == *FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP;
    let queueable = actionable || movement || electric_recovery;
    let lr = PostureModule::lr(module);
    let facing = if lr > 0.5 { 1 } else if lr < -0.5 { -1 } else { 0 };
    let motion = MotionModule::motion_kind(module);
    let motion_frame = MotionModule::frame(module);
    let Ok(mut shared) = rt.shared.try_lock() else {
        CANCEL_PENDING.store(true, Ordering::Release); return;
    };
    if CANCEL_PENDING.swap(false, Ordering::AcqRel) { request_ffi::kz_request_init(&mut shared.core); }
    shared.epoch = shared.epoch.wrapping_add(1);
    let context = Context { epoch:shared.epoch, now_ms:rt.now(), actor:module as usize as u64,
        permitted:(training && grounded) as i32,
        actionable:(training && grounded && !stopped && actionable) as i32,
        can_continue:(training && grounded && !stopped && queueable) as i32, facing };
    shared.status = status; shared.motion = motion; shared.motion_frame = motion_frame;
    shared.movement_ready=movement_ready;
    request_ffi::kz_request_observe(&mut shared.core, &RequestContext {
        base:context, queueable:queueable as i32, brake:movement as i32 });
    if shared.awaiting_electric && shared.attack_sent && training &&
       status == *FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2F {
        rt.queue(&shared,300,0,0);
        shared.awaiting_electric=false;
    }
    if !training || !grounded || !queueable ||
       (shared.started_ms>0 && rt.now().saturating_sub(shared.started_ms)>500) {
        shared.awaiting_electric=false;
    }
}

#[skyline::hook(replace = abi::get_gc_state)]
unsafe fn gc_state_hook(out: *mut abi::PadPrefix, id: *const u32) {
    // The caller owns a complete native GC buffer. Never allocate PadPrefix and
    // call the getter into it: the SDK may write a larger GC-specific suffix.
    original!()(out, id);
    if out.is_null() || id.is_null() || *id != HID_ID { return; }
    let Some(rt) = RUNTIME.get() else { return; };
    let allowed = training_now();
    let Ok(mut shared) = rt.shared.try_lock() else {
        CANCEL_PENDING.store(true, Ordering::Release); return;
    };
    if CANCEL_PENDING.swap(false, Ordering::AcqRel) { request_ffi::kz_request_init(&mut shared.core); }
    if !rt.seen_pad.swap(true, Ordering::AcqRel) { rt.queue(&shared, 100, (*out).buttons, 0); }
    let input = Poll { buttons:(*out).buttons, lx:(*out).lx, ly:(*out).ly,
        rx:(*out).rx, ry:(*out).ry, connected:(((*out).attributes & 1) != 0) as i32,
        matching_pad:1, training_now:allowed as i32, enabled:rt.enabled as i32, now_ms:rt.now() };
    let mut output = Output::default();
    let direct=shared.movement_ready as i32;
    request_ffi::kz_request_poll_ex(&mut shared.core, &input, &mut output,
                                  rt.fast_electric as i32, direct);
    if output.event==1 {
        shared.started_ms=input.now_ms;
        shared.attack_sent=false; shared.awaiting_electric=true;
    }
    if output.owns_input!=0 && output.phase==4 { shared.attack_sent=true; }
    if output.event==3 || output.event==5 { shared.awaiting_electric=false; }
    if output.event != 0 { rt.queue(&shared, output.event, input.buttons, output.phase); }
    if output.owns_input != 0 && shared.last_output_epoch != shared.epoch {
        rt.queue_output(&shared, &output); shared.last_output_epoch=shared.epoch;
    }
    // Only modified fields are written; sampling number, native attributes,
    // padding, and GC analog-trigger suffix remain untouched.
    if output.owns_input != 0 || output.consume_right != 0 {
        (*out).buttons=output.buttons;
        (*out).lx=output.lx; (*out).ly=output.ly;
        (*out).rx=output.rx; (*out).ry=output.ry;
    }
}

#[skyline::main(name = "kazuya_training_lab")]
pub fn main() {
    let _ = fs::create_dir_all("sd:/ultimate/kazuya_lab");
    let detected = unsafe { version() };
    log_line(&format!("v0.2.5 FAST INPUT PATCH | detected Smash {} | expected {}", detected, EXPECTED_VERSION));
    if detected != EXPECTED_VERSION {
        log_line("VERSION MISMATCH: no hook or fighter callback installed."); return;
    }
    if unsafe { request_ffi::kz_request_state_size() } as usize != std::mem::size_of::<State>() ||
       std::mem::size_of::<abi::PadPrefix>() != 40 {
        log_line("ABI SIZE MISMATCH: no hook installed."); return;
    }
    let config=fs::read_to_string(CONFIG_PATH).unwrap_or_default();
    let enabled=config_bool(&config,"enable_macro",false);
    let fast_electric=config_bool(&config,"fast_electric",true);
    let fast_movement=config_bool(&config,"fast_movement",true);
    let (tx, rx) = sync_channel::<Event>(128);
    if std::thread::Builder::new().name("kazuya-lab-log".into()).stack_size(128*1024).spawn(move || {
        while let Ok(e) = rx.recv() {
            let name = match e.kind {
                1=>"INPUT_SEQUENCE_STARTED", 2=>"INPUT_SEQUENCE_FINISHED_NOT_HIT_CONFIRMED",
                3=>"CANCELLED", 4=>"DIAGNOSTIC_RIGHT_PRESS", 5=>"CLOCK_OR_POLL_GAP",
                10=>"REQUEST_BUFFERED", 11=>"REQUEST_EXPIRED",
                12=>"REQUEST_BLOCKED_STATE", 13=>"RECOVERY_WAIT_TIMEOUT",
                100=>"GC_HID_ID_0_SEEN", 200=>"OUTPUT_FRAME",
                300=>"EWGF_STATE_OBSERVED_NOT_HIT_CONFIRMED", _=>"OTHER",
            };
            log_line(&format!("t={} event={} epoch={} buttons={:#x} phase={} status={} motion={:#x} motion_frame={:.2} output_x={} output_y={} since_start_ms={}",
                e.time,name,e.epoch,e.buttons,e.phase,e.status,e.motion,e.motion_frame,e.x,e.y,e.since_start_ms));
        }
    }).is_err() { log_line("LOG THREAD FAILED: no hook installed."); return; }
    let mut core = State::default();
    unsafe { request_ffi::kz_request_init(&mut core); }
    if RUNTIME.set(Runtime { start:Instant::now(), enabled, fast_electric, fast_movement,
        shared:Mutex::new(Shared { core,epoch:0,status:0,motion:0,motion_frame:0.0,last_output_epoch:0, movement_ready:false, started_ms:0, attack_sent:false, awaiting_electric:false }),
        logger:tx, seen_pad:AtomicBool::new(false) }).is_err() { return; }
    log_line(&format!("FAST_ELECTRIC={}; FAST_MOVEMENT={}; compact phases=1,3,4,5; legacy phases=1,2,3,4,5.",fast_electric,fast_movement));
    log_line("SCHEDULER=DELIVERY_HANDSHAKE; watchdog=80ms; tap buffer=1000ms; repeat still waits for native WAIT.");
    log_line("Direct movement commands and compact timing require console validation; no forced cancels or animation changes.");
    log_line("A queued tap may execute within 1000ms; manual attack/shield/jump cancels pending requests.");
    log_line(if enabled { "EXPERIMENTAL INPUT ENABLED; OFFLINE TRAINING ONLY; P1/ID0 assumption must be verified." }
        else { "DIAGNOSTICS ONLY; no macro input changes. Leave enable_macro=false for the first test." });
    skyline::install_hooks!(gc_state_hook);
    Agent::new("demon").on_line(Main, kazuya_frame).install();
}
