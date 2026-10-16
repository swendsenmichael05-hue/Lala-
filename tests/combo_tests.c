#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../core/combo_lab.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Combo s;
static KzComboFrame f;
static KzComboConfig cfg;
static KzPoll in;
static KzOutput out;
static int count;
static void done(const char*n){printf("PASS %02d %s\n",++count,n);}
static void observe(void){f.epoch++;f.now_ms+=16;in.now_ms=f.now_ms;kzc_observe(&s,&f);}
static int poll(uint64_t b){in.buttons=b;return kzc_poll(&s,&in,&out);}
static void reset(void){
 cfg=(KzComboConfig){1,0,6,1,0,2,95,17,14,10,11,0.14f};kzc_init(&s,&cfg);
 f=(KzComboFrame){.epoch=1,.now_ms=100,.actor=1,.target=2,
 .flags=C_SCOPE|C_GROUND|C_READY|C_TARGET|C_FLOOR|C_TFLOOR|C_PATH,
 .target_kind=0,.tx=7,.ty=8,.facing=1,.weight=100,.target_end=26};
 in=(KzPoll){.enabled=1,.connected=1,.matching_pad=1,.training_now=1,.now_ms=100};
 kzc_observe(&s,&f);
}
static void arm(void){kzc_arm(&s,f.now_ms);CHECK(s.phase==P_ARMED);}
static void hit(void){f.flags|=C_ELECTRIC|C_HIT|C_ATTRIBUTED|C_THURT;f.flags&=~C_READY;f.target_percent+=17.4f;observe();}
static void ready(void){f.flags|=C_READY;observe();}
static int take_event(int e){int x,found=0;while((x=kzc_pop_event(&s,0,0,0,0)))if(x==e)found=1;return found;}
static void to_air(void){
 ready();CHECK(poll(0));CHECK(s.phase==P_JUMP);observe();CHECK(poll(0));CHECK(out.buttons==(1u<<2));
 f.flags&=~(C_GROUND|C_READY|C_ELECTRIC|C_HIT);f.flags|=C_JUMP|C_AIR_READY;f.y=10;f.ty=10;observe();poll(0);CHECK(s.phase==P_AIR);
}
int main(void){
 CHECK(sizeof(KzComboFrame)==128&&sizeof(KzComboConfig)==48&&sizeof(KzComboRule)==24);
 CHECK(kzc_frame_size()==128&&kzc_config_size()==48&&kzc_storage_used()<=KZ_COMBO_STORAGE_BYTES);done("C/native ABI and bounded storage");
 reset();cfg.enabled=0;kzc_init(&s,&cfg);kzc_observe(&s,&f);kzc_arm(&s,100);CHECK(!s.phase&&!poll(KZ_RIGHT));done("disabled module preserves original input");
 reset();arm();CHECK(!poll(KZ_RIGHT));CHECK(!kzc_active(&s));done("press arms only and leaves existing electric pulse alone");
 reset();arm();f.flags|=C_ELECTRIC;observe();CHECK(s.phase==P_ARMED);done("electric animation without contact cannot start followup");
 reset();arm();f.flags|=C_ELECTRIC|C_HIT;f.target_percent=17;observe();CHECK(s.phase==P_ARMED);done("damage without attacker attribution does not trigger");
 reset();arm();f.flags|=C_ELECTRIC|C_ATTRIBUTED;f.target_percent=17;observe();CHECK(s.phase==P_ARMED);done("attacker mask without own collision hit cannot trigger");
 reset();arm();f.flags|=C_ELECTRIC|C_HIT|C_ATTRIBUTED|C_SHIELD_HIT;f.target_percent=17;observe();CHECK(s.phase==P_ARMED);done("shield contact does not start followup");
 reset();arm();f.flags|=C_HIT|C_ATTRIBUTED;f.target_percent=17;observe();CHECK(s.phase==P_ARMED);done("non-electric damage is not an EWGF starter");
 reset();arm();hit();CHECK(kzc_active(&s)&&s.links==1&&s.phase==P_SELECT);CHECK(take_event(CE_HIT));done("new attributed EWGF damage activates one followup");
 observe();CHECK(s.links==1);done("sticky infliction cannot count the same hit twice");
 reset();cfg.observe_only=1;kzc_init(&s,&cfg);kzc_observe(&s,&f);arm();hit();CHECK(s.phase==P_OBSERVE&&!kzc_active(&s)&&!poll(KZ_RIGHT));f.tvx=1;observe();CHECK(take_event(420));done("observer reports hit and motion but never owns input");
 reset();arm();hit();CHECK(poll(0)&&out.buttons==0&&s.phase==P_SELECT);done("normal recovery is respected before jump");
 ready();f.flags|=C_STOP;observe();poll(0);CHECK(out.buttons==0&&s.phase==P_SELECT);done("hitstop prevents phase execution");
 reset();arm();hit();f.stick_x=-1;observe();CHECK(s.di==1);f.stick_x=1;observe();CHECK(s.di==2);f.stick_x=0;observe();CHECK(s.di==0);done("live in out neutral stick classification");
 reset();f.tx=-7;f.facing=-1;kzc_observe(&s,&f);arm();hit();f.stick_x=1;observe();CHECK(s.di==1);f.stick_x=-1;observe();CHECK(s.di==2);done("DI classification is mirrored for left-side targets");
 reset();arm();hit();ready();poll(0);CHECK(s.route==R_NAIR);done("low percent flat-ground route selects nair family");
 reset();f.target_percent=110;kzc_observe(&s,&f);arm();hit();ready();poll(0);CHECK(s.route==R_USMASH);done("configured high-percent candidate selects up smash");
 reset();f.target_percent=110;f.ty=30;kzc_observe(&s,&f);arm();hit();ready();poll(0);CHECK(s.route==R_UPB);done("high target vertical finisher chooses up special");
 reset();f.target_floor_y=27;f.ty=32;kzc_observe(&s,&f);arm();hit();ready();poll(0);CHECK(s.route==R_FERPS&&s.phase==P_FERPS_DASH);done("observed elevated support selects platform approach");
 reset();KzComboRule r={.fighter=0,.di=-1,.platform=-1,.route=R_DRAGON,.min_percent=0,.max_percent=999};CHECK(kzc_add_rule(&s,&r));arm();hit();ready();poll(0);CHECK(s.route==R_DRAGON);done("custom per-fighter route override");
 reset();r.fighter=1;CHECK(kzc_add_rule(&s,&r));arm();hit();ready();poll(0);CHECK(s.route==R_NAIR);done("unmatched character does not inherit custom rule");
 reset();r.fighter=-1;r.min_percent=120;CHECK(kzc_add_rule(&s,&r));arm();hit();ready();poll(0);CHECK(s.route==R_NAIR);done("custom percent condition is enforced");
 reset();r.min_percent=0;r.di=2;r.platform=1;CHECK(kzc_add_rule(&s,&r));arm();hit();ready();poll(0);CHECK(s.route==R_NAIR);done("custom DI and platform conditions are enforced");
 reset();r=(KzComboRule){-1,-1,-1,R_NAIR,0,999};for(int i=0;i<64;i++)CHECK(kzc_add_rule(&s,&r));CHECK(!kzc_add_rule(&s,&r));r.min_percent=NAN;CHECK(!kzc_add_rule(&s,&r));done("rule count and malformed values are bounded");
 reset();arm();hit();to_air();f.y=15;f.ty=10;observe();poll(0);CHECK(out.buttons==KZ_A&&out.lx==0&&out.ly==0&&s.phase==P_NAIR);done("jump followup uses neutral attack pulse when in range");
 KzOutput cached=out;for(int i=0;i<40;i++){poll(0);CHECK(out.buttons==cached.buttons&&s.phase==P_NAIR);}done("duplicate HID polls cannot consume a new script step");
 f.flags&=~C_JUMP;f.flags|=C_NAIR|C_HIT|C_ATTRIBUTED;f.target_percent+=11;f.vy=-1;observe();poll(0);CHECK(s.nair_hit&&out.ly==-32767);done("nair contact and descending fast-fall observation");
 f.flags|=C_GROUND;f.flags&=~(C_NAIR|C_HIT|C_AIR_READY);f.y=0;observe();poll(0);CHECK(s.phase==P_LAND);ready();poll(0);CHECK(s.phase==P_ELECTRIC);observe();poll(0);CHECK(out.phase==1&&out.lx==32767);done("confirmed nair landing begins original compact EWGF engine");
 for(int p=3;p<=5;p++){f.flags&=~C_READY;observe();poll(0);CHECK(out.phase==p);}done("followup electric preserves forward down diagonal release outputs");
 f.flags|=C_ELECTRIC|C_HIT|C_ATTRIBUTED;f.target_percent+=17;observe();CHECK(s.links==2&&s.phase==P_SELECT);done("next attributed electric loops through route selection");
 reset();arm();hit();to_air();f.y=15;observe();poll(0);f.flags|=C_GROUND;f.flags&=~C_HIT;observe();CHECK(!poll(0)&&!kzc_active(&s));done("whiffed nair cannot invent an electric followup");
 for(int bit=0;bit<16;bit++){if(bit==14)continue;reset();arm();hit();CHECK(!poll(KZ_RIGHT|(UINT64_C(1)<<bit))&&!kzc_active(&s));}done("all manual competing low buttons abort active combo");
 reset();arm();hit();in.matching_pad=0;CHECK(!poll(KZ_A)&&kzc_active(&s));done("other pad is untouched and cannot cancel player-one combo");
 reset();arm();hit();in.connected=0;CHECK(!poll(KZ_RIGHT)&&!kzc_active(&s));done("disconnection cancels combo ownership");
 reset();arm();hit();in.training_now=0;CHECK(!poll(KZ_RIGHT)&&!kzc_active(&s));done("leaving training cancels immediately");
 reset();arm();hit();in.now_ms=f.now_ms+81;CHECK(!poll(KZ_RIGHT)&&!kzc_active(&s));done("stale fighter snapshots cannot sustain input ownership");
 reset();arm();hit();f.actor++;observe();CHECK(!kzc_active(&s));done("player identity change clears combo");
 reset();arm();hit();f.target++;observe();CHECK(!kzc_active(&s));done("target replacement or transformation clears combo");
 reset();arm();hit();f.flags&=~C_TARGET;observe();CHECK(!kzc_active(&s));done("missing or ambiguous opponent clears combo");
 reset();arm();hit();f.percent+=1;observe();CHECK(!kzc_active(&s));done("taking damage aborts even before damage animation is observed");
 reset();arm();hit();f.target_percent=0;observe();CHECK(!kzc_active(&s));done("training reset damage decrease clears route");
 reset();arm();hit();f.tx=200;observe();CHECK(!kzc_active(&s));done("out-of-range chase is rejected");
 reset();arm();hit();f.x=NAN;observe();CHECK(!kzc_active(&s));done("nonfinite native input fails closed");
 reset();arm();hit();f.flags&=~C_PATH;ready();CHECK(!poll(0)&&!kzc_active(&s));done("missing support ray prevents starting a stage-edge chase");
 reset();arm();hit();f.flags|=C_TDEAD;observe();CHECK(!kzc_active(&s)&&take_event(CE_TARGET_KO));done("KO state terminates route without claiming causality");
 reset();arm();f.now_ms+=901;kzc_observe(&s,&f);CHECK(!s.phase);done("unconfirmed starter expires");
 reset();arm();hit();s.links=6;ready();poll(0);CHECK(s.route!=R_NAIR);done("max links imposes finisher instead of infinite loop");
 reset();arm();hit();ready();poll(0);observe();poll(0);f.now_ms+=101;f.epoch++;kzc_observe(&s,&f);in.now_ms=f.now_ms;CHECK(!poll(0));done("long delivery gap aborts script");
 reset();arm();hit();s.phase=P_TECH;s.phase_ms=f.now_ms;f.flags|=C_READY|C_TTECH|C_TGROUND;f.target_frame=5;f.target_end=26;f.ty=0;observe();poll(0);CHECK(s.phase==P_TECH);f.target_frame=16;observe();poll(0);CHECK(s.phase==P_ELECTRIC);done("tech punish waits for configured end-of-animation approach window");
 reset();arm();hit();s.phase=P_TECH;s.phase_ms=f.now_ms;f.flags|=C_READY|C_TDOWN|C_TGROUND;f.ty=0;observe();poll(0);CHECK(s.phase==P_CROUCH);for(int i=0;i<6;i++){observe();poll(0);CHECK(out.ly==-32767&&!out.buttons);}observe();poll(0);CHECK(out.buttons==KZ_A&&s.phase==P_JAB);done("missed-tech branch crouches before attempting crouch jab");
 reset();cfg.route=R_FERPS;kzc_init(&s,&cfg);kzc_observe(&s,&f);arm();hit();ready();CHECK(!poll(0)&&!kzc_active(&s));done("forced Ferps route still requires elevated support");
 reset();f.ty=32;f.target_floor_y=27;kzc_observe(&s,&f);arm();hit();ready();poll(0);observe();poll(0);CHECK(out.lx>0);observe();poll(0);CHECK(s.phase==P_FERPS_RAR);observe();poll(0);CHECK(out.lx<0&&out.buttons==(1u<<2));done("Ferps candidate emits forward setup then reverse jump");
 f.flags&=~(C_GROUND|C_READY|C_ELECTRIC|C_HIT);f.flags|=C_AIR_READY|C_JUMP;f.y=5;observe();poll(0);CHECK(s.phase==P_FERPS_RISE);f.y=19;observe();poll(0);CHECK(out.buttons==(KZ_A|(1u<<2))&&out.lx==0&&s.phase==P_FERPS_DJC);done("delayed double-jump neutral-air uses sampled platform height");
 f.flags|=C_NAIR|C_DOUBLE_JUMP;f.flags&=~C_JUMP;observe();poll(0);CHECK(s.phase==P_FERPS_NAIR);f.y=31;f.vy=-1;observe();poll(0);CHECK(out.ly==-32767);f.y=27;f.flags|=C_GROUND|C_READY|C_TTECH|C_TGROUND;f.ty=27;observe();poll(0);CHECK(s.phase==P_TECH);done("observed platform landing enters tech-chase branch");
 reset();cfg.route=R_USMASH;kzc_init(&s,&cfg);kzc_observe(&s,&f);arm();hit();ready();poll(0);observe();poll(0);observe();poll(0);CHECK(out.buttons==KZ_A&&out.ly==32767);observe();poll(0);CHECK(!out.buttons);done("up-smash finisher emits fresh up-attack then release");
 reset();cfg.route=R_UPB;kzc_init(&s,&cfg);kzc_observe(&s,&f);arm();hit();ready();poll(0);observe();poll(0);observe();poll(0);CHECK(out.buttons==2&&out.ly==32767);done("up-special finisher emits up and B");
 reset();cfg.route=R_DRAGON;kzc_init(&s,&cfg);kzc_observe(&s,&f);arm();hit();ready();poll(0);observe();poll(0);observe();poll(0);CHECK(out.phase==1);observe();poll(0);CHECK(out.phase==3);observe();poll(0);CHECK(out.phase==4&&out.buttons==1);observe();poll(0);CHECK(out.buttons==1);done("Dragon candidate retains A after compact command rather than inventing a move-state write");
 reset();cfg.max_links=999;cfg.nair_x=NAN;cfg.jump_bit=99;kzc_init(&s,&cfg);CHECK(s.cfg.max_links==12&&s.cfg.nair_x==17&&s.cfg.jump_bit==2);done("configuration limits sanitize invalid numeric tuning");
 reset();arm();hit();s.phase=P_LAND;s.phase_ms=f.now_ms;f.flags=C_SCOPE|C_TARGET|C_GROUND|C_READY|C_TROLL|C_PATH;f.ty=0;observe();poll(0);CHECK(s.phase==P_TECH);done("landing branch preserves observed tech roll rather than treating it as escape");
 reset();f.flags&=~C_TARGET;observe();kzc_arm(&s,f.now_ms);CHECK(!s.phase&&take_event(CE_NO_TARGET));done("arming with no unique opponent produces a diagnostic without ownership");
 reset();arm();hit();s.phase=P_JAB_WAIT;s.phase_ms=f.now_ms;s.baseline_damage=f.target_percent;f.flags=C_SCOPE|C_TARGET|C_GROUND|C_READY|C_ATTRIBUTED;f.target_percent+=5;observe();poll(0);CHECK(s.phase==P_JAB_WAIT);done("missed-tech jab damage alone cannot confirm a followup");
 reset();uint32_t rng=0x125cc91;for(int i=0;i<100000;i++){
  rng=rng*1664525u+1013904223u;
  if(!s.phase){kzc_arm(&s,f.now_ms);}
  f.flags=C_SCOPE|C_TARGET|C_GROUND|C_READY|C_PATH|C_FLOOR|C_TFLOOR;
  if(rng&1){f.flags|=C_ELECTRIC|C_HIT|C_ATTRIBUTED;f.target_percent+=0.1f;}
  if(rng&2)f.flags|=C_STOP;
  if((rng&31)==3)f.flags&=~C_SCOPE;
  if((rng&31)==4)f.flags&=~C_TARGET;
  if((rng&31)==5)f.flags|=C_HURT;
  f.target_percent=fmodf(f.target_percent,900);observe();
  uint64_t buttons=(rng&7)==0?KZ_A:KZ_RIGHT;
  int own=poll(buttons);
  if(own){CHECK((f.flags&C_SCOPE)&&(f.flags&C_TARGET)&&!(f.flags&C_HURT));CHECK(!(out.buttons&KZ_RIGHT));CHECK(out.lx>=-32767&&out.lx<=32767);}
  CHECK(s.ev_count<=16&&s.links<=13&&s.rule_count<=64);
 }
 done("100000 randomized ownership, cancellation, queue and output-bound checks");
 kzc_init(NULL,NULL);kzc_observe(NULL,&f);kzc_observe(&s,NULL);CHECK(!kzc_poll(NULL,&in,&out));CHECK(!kzc_add_rule(NULL,NULL));done("null ABI arguments fail safely");
 printf("RESULT: %d synthetic combo cases passed; no console or matchup validation implied.\n",count);
}
