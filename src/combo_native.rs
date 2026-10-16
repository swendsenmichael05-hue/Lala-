//! Opt-in, offline 1v1 experimental combo controller. Game data is READ ONLY;
//! player actions use the same GC HID report as the existing electric adapter.
//! A mapped stick read is NOT a claim of finalized DI, a true combo, or a KO.
use crate::core_ffi::{Poll,Output};
use smash::{app::{self,lua_bind::*,sv_battle_object,utility},lib::lua_const::*,phx::Vector2f};
use std::{fs::{self,OpenOptions},io::Write,sync::{Mutex,OnceLock,atomic::{AtomicBool,AtomicUsize,Ordering},mpsc::{sync_channel,SyncSender}},time::Instant};
const LOG:&str="sd:/ultimate/kazuya_lab/combo.log";
const RULES:&str="sd:/ultimate/kazuya_lab/combo_rules.csv";
const SCOPE:u32=1<<0;const GROUND:u32=1<<1;const STOP:u32=1<<2;const READY:u32=1<<3;
const ELECTRIC:u32=1<<4;const NAIR:u32=1<<5;const HURT:u32=1<<6;const DEAD:u32=1<<7;
const JUMP:u32=1<<8;const AIR_READY:u32=1<<9;const HIT:u32=1<<10;const SHIELD_HIT:u32=1<<11;
const TARGET:u32=1<<12;const TGROUND:u32=1<<13;const THURT:u32=1<<14;const TSTOP:u32=1<<15;
const TDEAD:u32=1<<16;const TTECH:u32=1<<17;const TROLL:u32=1<<18;const TDOWN:u32=1<<19;
const FLOOR:u32=1<<20;const TFLOOR:u32=1<<21;const PATH:u32=1<<22;const ATTRIBUTED:u32=1<<23;
const DOUBLE_JUMP:u32=1<<24;
#[repr(C)]
#[derive(Clone,Copy,Default)]
pub struct Frame {
 pub epoch:u64,pub now_ms:u64,pub actor:u64,pub target:u64,
 pub flags:u32,pub target_kind:i32,pub self_status:i32,pub target_status:i32,
 pub x:f32,pub y:f32,pub vx:f32,pub vy:f32,pub facing:f32,pub motion_frame:f32,pub percent:f32,
 pub tx:f32,pub ty:f32,pub tvx:f32,pub tvy:f32,pub target_percent:f32,pub weight:f32,pub stick_x:f32,pub stick_y:f32,
 pub target_frame:f32,pub target_end:f32,pub floor_y:f32,pub target_floor_y:f32,pub path_floor_y:f32,
}
#[repr(C)]
#[derive(Clone,Copy)]
struct Config {
 enabled:i32,observe_only:i32,max_links:i32,ferps:i32,route:i32,jump_bit:i32,
 finisher_percent:f32,nair_x:f32,nair_y:f32,ferps_below:f32,tech_lead:f32,drift_gain:f32,
}
#[repr(C)]
struct Rule {fighter:i32,di:i32,platform:i32,route:i32,min_percent:f32,max_percent:f32}
#[repr(C,align(8))]
struct Storage([u64;2048]);
extern "C" {
 fn kzc_init(s:*mut Storage,c:*const Config);
 fn kzc_observe(s:*mut Storage,f:*const Frame);
 fn kzc_arm(s:*mut Storage,now:u64);
 fn kzc_cancel(s:*mut Storage,reason:i32);
 fn kzc_active(s:*const Storage)->i32;
 fn kzc_poll(s:*mut Storage,p:*const Poll,o:*mut Output)->i32;
 fn kzc_pop_event(s:*mut Storage,r:*mut i32,d:*mut i32,l:*mut i32,p:*mut i32)->i32;
 fn kzc_add_rule(s:*mut Storage,r:*const Rule)->i32;
 fn kzc_frame_size()->u32;
 fn kzc_config_size()->u32;
 fn kzc_storage_used()->u32;
}
struct Shared {storage:Storage,frame:Frame,epoch:u64}
#[derive(Clone,Copy)]
struct Event {frame:Frame,event:i32,route:i32,di:i32,links:i32,phase:i32}
struct Runtime {start:Instant,state:Mutex<Shared>,logger:SyncSender<Event>}
static RUNTIME:OnceLock<Runtime>=OnceLock::new();
static MANAGER:AtomicUsize=AtomicUsize::new(0);
static ACTIVE:AtomicBool=AtomicBool::new(false);
static CANCEL:AtomicBool=AtomicBool::new(false);
fn log(s:&str){if let Ok(mut f)=OpenOptions::new().create(true).append(true).open(LOG){let _=writeln!(f,"{}",s);}}
fn setting<'a>(text:&'a str,key:&str)->Option<&'a str>{
 text.lines().map(str::trim).filter(|l|!l.starts_with('#')&&!l.starts_with(';'))
 .filter_map(|l|l.split_once('=')).filter(|(k,_)|k.trim()==key).map(|(_,v)|v.trim()).last()
}
fn b(text:&str,k:&str,d:bool)->bool{setting(text,k).map(|v|v=="true").unwrap_or(d)}
fn n(text:&str,k:&str,d:f32)->f32{setting(text,k).and_then(|v|v.parse::<f32>().ok()).filter(|v|v.is_finite()).unwrap_or(d)}
fn route(v:&str)->Option<i32>{match v {"auto"=>Some(0),"nair"=>Some(1),"usmash"=>Some(2),"upb"=>Some(3),"ferps"=>Some(4),"dragon"=>Some(5),_=>None}}
fn pump(rt:&Runtime,s:&mut Shared){unsafe{
 loop{let(mut r,mut d,mut l,mut p)=(0,0,0,0);let e=kzc_pop_event(&mut s.storage,&mut r,&mut d,&mut l,&mut p);if e==0{break;}
 let _=rt.logger.try_send(Event{frame:s.frame,event:e,route:r,di:d,links:l,phase:p});}
 ACTIVE.store(kzc_active(&s.storage)!=0,Ordering::Release);
}}
pub fn active()->bool{ACTIVE.load(Ordering::Acquire)}
fn now(rt:&Runtime)->u64{rt.start.elapsed().as_millis().min(u64::MAX as u128) as u64}
pub fn init(text:&str,base_enabled:bool){
 let assist=b(text,"combo_assist",false);let observer=b(text,"combo_observe",false);
 if !base_enabled||!(assist||observer){return;}
 log("v0.3.0 COMBO LAB | experimental offline 1v1 | no universal zero-to-death claim");
 let c=Config{enabled:1,observe_only:(!assist) as i32,max_links:n(text,"combo_max_links",6.) as i32,
 ferps:b(text,"combo_ferps",true) as i32,route:route(setting(text,"combo_route").unwrap_or("auto")).unwrap_or(0),
 jump_bit:n(text,"combo_jump_bit",2.) as i32,finisher_percent:n(text,"combo_finisher_percent",95.),
 nair_x:n(text,"combo_nair_x",17.),nair_y:n(text,"combo_nair_y",14.),ferps_below:n(text,"combo_ferps_below",10.),
 tech_lead:n(text,"combo_tech_lead",11.),drift_gain:n(text,"combo_drift_gain",0.14)};
 unsafe{
  if kzc_frame_size() as usize!=std::mem::size_of::<Frame>()||kzc_config_size() as usize!=std::mem::size_of::<Config>()||kzc_storage_used() as usize>std::mem::size_of::<Storage>(){
   log("COMBO ABI MISMATCH: combo module disabled; original EWGF remains available");return;
  }
  let mut address:usize=0;
  skyline::nn::ro::LookupSymbol(&mut address as *mut usize,b"_ZN3lib9SingletonIN3app14FighterManagerEE9instance_E\0".as_ptr());
  if address==0 {log("FIGHTER_MANAGER_NOT_FOUND: combo module disabled");return;}
  MANAGER.store(address,Ordering::Release);
 }
 let mut storage=Storage([0;2048]);unsafe{kzc_init(&mut storage,&c);}
 let mut loaded=0;
 if let Ok(rows)=fs::read_to_string(RULES){
  for (i,line) in rows.lines().enumerate(){
   let line=line.trim();if line.is_empty()||line.starts_with('#'){continue;}
   let a:Vec<_>=line.split(',').map(str::trim).collect();
   let parsed=(||->Option<Rule>{if a.len()!=6{return None;}Some(Rule{fighter:a[0].parse().ok()?,
    min_percent:a[1].parse().ok()?,max_percent:a[2].parse().ok()?,di:a[3].parse().ok()?,platform:a[4].parse().ok()?,route:route(a[5])?})})();
   if let Some(r)=parsed {if unsafe{kzc_add_rule(&mut storage,&r)}!=0{loaded+=1;continue;}}
   log(&format!("RULE_REJECTED line={}; expected fighter,min,max,di,platform,route",i+1));
  }
 }
 let(tx,rx)=sync_channel::<Event>(256);
 if std::thread::Builder::new().name("kazuya-combo-log".into()).stack_size(128*1024).spawn(move||{
  while let Ok(e)=rx.recv(){
   let name=match e.event{400=>"COMBO_ARMED",401=>"ATTRIBUTED_EWGF_HIT",402=>"COMBO_ROUTE_SELECTED",403=>"TARGET_STICK_CLASS_CHANGED",
    404=>"COMBO_PHASE",405=>"ATTRIBUTED_NAIR_HIT",406=>"COMBO_ELECTRIC_REQUEST",407=>"PLATFORM_SEQUENCE",408=>"TECH_OPTION_OBSERVED",
    409=>"COMBO_CANCELLED",410=>"COMBO_TIMEOUT",411=>"COMBO_ESCAPE_OR_GEOMETRY_STOP",412=>"COMBO_TARGET_LOST",413=>"TARGET_KO_STATE_OBSERVED",
    414=>"FOLLOWUP_NOT_CONFIRMED",415=>"JUMP_AIRBORNE_OBSERVED",416=>"FINISHER_INPUT_COMPLETE_NOT_KO_CONFIRMED",417=>"COMBO_POLL_GAP",
    418=>"CUSTOM_RULE_SELECTED",419=>"JUMP_OR_NAIR_NOT_OBSERVED",420=>"TARGET_LAUNCH_MOTION_OBSERVED",_=>"OTHER"};
   let f=e.frame;
   log(&format!("t={} event={} route={} di={} links={} phase={} flags={:#x} target={} kind={} status={}/{} self_xy={:.2},{:.2} target_xy={:.2},{:.2} target_velocity={:.3},{:.3} target_stick={:.3},{:.3} percent={:.2} weight={:.2} target_motion_frame={:.2} target_end={:.2} floor={:.2} target_floor={:.2}",
    f.now_ms,name,e.route,e.di,e.links,e.phase,f.flags,f.target,f.target_kind,f.self_status,f.target_status,f.x,f.y,f.tx,f.ty,f.tvx,f.tvy,f.stick_x,f.stick_y,f.target_percent,f.weight,f.target_frame,f.target_end,f.floor_y,f.target_floor_y));
  }
 }).is_err(){log("COMBO_LOGGER_FAILED: combo module disabled");return;}
 if RUNTIME.set(Runtime{start:Instant::now(),state:Mutex::new(Shared{storage,frame:Frame::default(),epoch:0}),logger:tx}).is_err(){return;}
 log(&format!("COMBO_ASSIST={}; OBSERVE_ONLY={}; custom_rows={}; jump_HID_bit={}; A=attack B=special. Thresholds are uncalibrated route candidates.",assist,!assist,loaded,c.jump_bit));
 log("Only P1 Kazuya vs one P2 fighter. Hit attribution needs collision-hit AND damage increase AND victim attacker mask. Target stick is sampled input, not proof of finalized DI. No target is modified.");
}
unsafe fn hurt(status:i32)->bool{
 (*FIGHTER_STATUS_KIND_DAMAGE..=*FIGHTER_STATUS_KIND_DAMAGE_FALL).contains(&status)||
 // Use upstream status names; DAMAGE_PARALYZE variants do not exist.
 [*FIGHTER_STATUS_KIND_BIND,*FIGHTER_STATUS_KIND_DAMAGE_SLEEP_START,
 *FIGHTER_STATUS_KIND_DAMAGE_SLEEP_FALL,*FIGHTER_STATUS_KIND_DAMAGE_SLEEP_END,
 *FIGHTER_STATUS_KIND_CAPTURE_PULLED,*FIGHTER_STATUS_KIND_CAPTURE_WAIT,*FIGHTER_STATUS_KIND_CAPTURE_DAMAGE,
 *FIGHTER_STATUS_KIND_FURAFURA,*FIGHTER_STATUS_KIND_DAMAGE_SLEEP].contains(&status)
}
unsafe fn dead(status:i32)->bool{[*FIGHTER_STATUS_KIND_DEAD,*FIGHTER_STATUS_KIND_STANDBY,*FIGHTER_STATUS_KIND_REBIRTH,*FIGHTER_STATUS_KIND_ENTRY].contains(&status)}
unsafe fn accessor(mgr:*mut app::FighterManager,id:i32)->Option<*mut app::BattleObjectModuleAccessor>{
 let entry=FighterManager::get_fighter_entry(mgr,app::FighterEntryID(id)) as *mut app::FighterEntry;
 if entry.is_null(){return None;}
 let id=FighterEntry::current_fighter_id(entry) as u32;
 if !sv_battle_object::is_active(id){return None;}
 let p=sv_battle_object::module_accessor(id);
 if p.is_null(){None}else{Some(p)}
}
unsafe fn floor(module:*mut app::BattleObjectModuleAccessor,x:f32,y:f32)->Option<f32>{
 let mut hit=Vector2f{x:0.,y:0.};
 if GroundModule::ray_check_hit_pos(module,&Vector2f{x,y:y+2.},&Vector2f{x:0.,y:-180.},&mut hit,true)&&hit.y.is_finite(){Some(hit.y)}else{None}
}
pub unsafe fn observe(module:*mut app::BattleObjectModuleAccessor){
 let Some(rt)=RUNTIME.get()else{return;};if module.is_null(){return;}
 let mut f=Frame::default();f.now_ms=now(rt);f.actor=module as usize as u64;
 let training=app::smashball::is_training_mode()&&app::sv_information::is_ready_go();
 if training{
  f.flags|=SCOPE;f.self_status=StatusModule::status_kind(module);
  let grounded=StatusModule::situation_kind(module)==*SITUATION_KIND_GROUND;
  let stopped=StopModule::is_stop(module);let cancel=CancelModule::is_enable_cancel(module);
  let status=f.self_status;let motion=MotionModule::motion_kind(module);
  let moving=[*FIGHTER_STATUS_KIND_WAIT,*FIGHTER_STATUS_KIND_WALK,*FIGHTER_STATUS_KIND_DASH,*FIGHTER_STATUS_KIND_RUN,
   *FIGHTER_STATUS_KIND_RUN_BRAKE,*FIGHTER_STATUS_KIND_SQUAT_WAIT,*FIGHTER_DEMON_STATUS_KIND_DASH_BACK,*FIGHTER_DEMON_STATUS_KIND_WALK_BACK].contains(&status);
  let air_ready=[*FIGHTER_STATUS_KIND_JUMP,*FIGHTER_STATUS_KIND_JUMP_AERIAL,*FIGHTER_STATUS_KIND_FALL,*FIGHTER_STATUS_KIND_FALL_AERIAL].contains(&status)||cancel;
  if grounded{f.flags|=GROUND;}if stopped{f.flags|=STOP;}
  if grounded&&!stopped&&(moving||cancel){f.flags|=READY;}
  if !grounded&&!stopped&&air_ready{f.flags|=AIR_READY;}
  if status==*FIGHTER_DEMON_STATUS_KIND_ATTACK_STEP_2F{f.flags|=ELECTRIC;}
  if status==*FIGHTER_STATUS_KIND_ATTACK_AIR&&motion==smash::hash40("attack_air_n"){f.flags|=NAIR;}
  if hurt(status){f.flags|=HURT;}if dead(status){f.flags|=DEAD;}
  if status==*FIGHTER_STATUS_KIND_JUMP||status==*FIGHTER_STATUS_KIND_JUMP_AERIAL{f.flags|=JUMP;}
  if WorkModule::get_int(module,*FIGHTER_INSTANCE_WORK_ID_INT_JUMP_COUNT)>=2{f.flags|=DOUBLE_JUMP;}
  if AttackModule::is_infliction_status(module,*COLLISION_KIND_MASK_HIT){f.flags|=HIT;}
  if AttackModule::is_infliction_status(module,*COLLISION_KIND_MASK_SHIELD){f.flags|=SHIELD_HIT;}
  f.x=PostureModule::pos_x(module);f.y=PostureModule::pos_y(module);f.facing=PostureModule::lr(module);
  f.vx=KineticModule::get_sum_speed_x(module,*KINETIC_ENERGY_RESERVE_ATTRIBUTE_MAIN);
  f.vy=KineticModule::get_sum_speed_y(module,*KINETIC_ENERGY_RESERVE_ATTRIBUTE_MAIN);
  f.motion_frame=MotionModule::frame(module);f.percent=DamageModule::damage(module,0);
  if let Some(y)=floor(module,f.x,f.y){f.floor_y=y;f.flags|=FLOOR;}
  let addr=MANAGER.load(Ordering::Acquire);
  if addr!=0{
   let mgr=*(addr as *const *mut app::FighterManager);
   if !mgr.is_null(){
    let mut extra=false;for id in 2..8{if accessor(mgr,id).is_some(){extra=true;break;}}
    if !extra{if let Some(t)=accessor(mgr,1){
     f.flags|=TARGET;f.target=t as usize as u64;f.target_kind=utility::get_kind(&mut *t);
     let st=StatusModule::status_kind(t);f.target_status=st;
     if StatusModule::situation_kind(t)==*SITUATION_KIND_GROUND{f.flags|=TGROUND;}
     if StopModule::is_stop(t){f.flags|=TSTOP;}if hurt(st){f.flags|=THURT;}if dead(st){f.flags|=TDEAD;}
     if st==*FIGHTER_STATUS_KIND_PASSIVE{f.flags|=TTECH;}
     if st==*FIGHTER_STATUS_KIND_PASSIVE_FB||st==*FIGHTER_STATUS_KIND_DOWN_STAND_FB{f.flags|=TROLL;}
     if st==*FIGHTER_STATUS_KIND_DOWN||st==*FIGHTER_STATUS_KIND_DOWN_WAIT{f.flags|=TDOWN;}
     if WorkModule::get_int(t,*FIGHTER_INSTANCE_WORK_ID_INT_SUCCEED_ATTACKER_ENTRY_ID)&1!=0{f.flags|=ATTRIBUTED;}
     f.tx=PostureModule::pos_x(t);f.ty=PostureModule::pos_y(t);
     f.tvx=KineticModule::get_sum_speed_x(t,*KINETIC_ENERGY_RESERVE_ATTRIBUTE_MAIN);
     f.tvy=KineticModule::get_sum_speed_y(t,*KINETIC_ENERGY_RESERVE_ATTRIBUTE_MAIN);
     f.stick_x=ControlModule::get_stick_x(t);f.stick_y=ControlModule::get_stick_y(t);
     f.target_percent=DamageModule::damage(t,0);f.weight=WorkModule::get_param_float(t,smash::hash40("weight"),0);
     f.target_frame=MotionModule::frame(t);f.target_end=MotionModule::end_frame(t);
     if let Some(y)=floor(module,f.tx,f.ty){f.target_floor_y=y;f.flags|=TFLOOR;}
     let aim=f.tx+(f.tvx*5.).clamp(-12.,12.);
     if let Some(y)=floor(module,aim,f.y.max(f.ty)){f.path_floor_y=y;f.flags|=PATH;}
    }}
   }
  }
 }
 let Ok(mut s)=rt.state.try_lock()else{CANCEL.store(true,Ordering::Release);return;};
 if CANCEL.swap(false,Ordering::AcqRel){kzc_cancel(&mut s.storage,409);}
 s.epoch=s.epoch.wrapping_add(1);f.epoch=s.epoch;s.frame=f;
 kzc_observe(&mut s.storage,&f);pump(rt,&mut s);
}
pub fn arm(){let Some(rt)=RUNTIME.get()else{return;};let Ok(mut s)=rt.state.try_lock()else{CANCEL.store(true,Ordering::Release);return;};unsafe{if CANCEL.swap(false,Ordering::AcqRel){kzc_cancel(&mut s.storage,409);}kzc_arm(&mut s.storage,now(rt));}pump(rt,&mut s);}
pub fn override_input(input:&Poll,out:&mut Output)->bool{
 let Some(rt)=RUNTIME.get()else{return false;};
 let Ok(mut s)=rt.state.try_lock()else{CANCEL.store(true,Ordering::Release);return false;};
 if CANCEL.swap(false,Ordering::AcqRel){unsafe{kzc_cancel(&mut s.storage,409);}}
 let mut p=*input;p.now_ms=now(rt);
 let handled=unsafe{kzc_poll(&mut s.storage,&p,out)}!=0;
 pump(rt,&mut s);handled
}

pub fn cancel() { CANCEL.store(true,Ordering::Release); ACTIVE.store(false,Ordering::Release); }
