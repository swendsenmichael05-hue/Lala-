#include "combo_lab.h"
enum { P_OFF=0,P_ARMED,P_SELECT,P_JUMP,P_AIR,P_NAIR,P_LAND,
 P_ELECTRIC,P_FINISH_PREP,P_FINISH,P_FERPS_DASH,P_FERPS_RAR,
 P_FERPS_RISE,P_FERPS_DJC,P_FERPS_NAIR,P_TECH,P_CROUCH,P_JAB,P_JAB_WAIT,P_OBSERVE };
typedef struct {
 KzComboFrame f,previous;
 KzComboConfig cfg;
 KzComboRule rules[64]; int32_t rule_count;
 KzState electric; KzOutput cached;
 uint64_t arm_ms,start_ms,phase_ms,last_hit_ms,last_poll_ms,last_epoch,arm_actor,arm_target;
 float baseline_damage,home_y,platform_y,aim_x,away;
 int32_t phase,route,di,links,polls,seen,delivered,nair_hit,air_release,
         did_djc,finish_polls,dragon_hold,observed_electric,last_tech,pulse_started,launch_logged;
 int32_t events[16],ev_read,ev_write,ev_count;
} Combo;
_Static_assert(sizeof(Combo)<=KZ_COMBO_STORAGE_BYTES,"combo storage too small");
static int has(const Combo*s,uint32_t flag){return (s->f.flags&flag)!=0;}
static int valid_float(float x){return x==x && x<=3.402823466e38F && x>=-3.402823466e38F;}
static float absf(float x){return x<0?-x:x;}
static float clampf(float x,float a,float b){return x<a?a:x>b?b:x;}
static float signf(float x){return x<0?-1.0f:1.0f;}
static int finite_frame(const KzComboFrame*f){
 const float a[]={f->x,f->y,f->vx,f->vy,f->facing,f->motion_frame,f->percent,
 f->tx,f->ty,f->tvx,f->tvy,f->target_percent,f->weight,f->stick_x,f->stick_y,
 f->target_frame,f->target_end,f->floor_y,f->target_floor_y,f->path_floor_y};
 for(unsigned i=0;i<sizeof(a)/sizeof(a[0]);++i)if(!valid_float(a[i]))return 0;
 return f->percent>=0&&f->target_percent>=0;
}
static void event(Combo*s,int e){
 if(s->ev_count==16)return;
 s->events[s->ev_write]=e;s->ev_write=(s->ev_write+1)%16;s->ev_count++;
}
static void phase(Combo*s,int p){s->phase=p;s->phase_ms=s->f.now_ms;s->polls=0;event(s,CE_PHASE);}
static void stop(Combo*s,int e){
 if(s->phase&&e)event(s,e);
 s->phase=P_OFF;s->nair_hit=0;s->observed_electric=0;s->last_poll_ms=0;kz_init(&s->electric);
}
void kzc_init(void*v,const KzComboConfig*c){
 if(!v)return;Combo*s=v;for(unsigned i=0;i<sizeof(*s);++i)((volatile unsigned char*)s)[i]=0;kz_init(&s->electric);
 s->cfg=(KzComboConfig){0,0,6,1,R_AUTO,2,95,17,14,10,11,0.14f};
 if(c)s->cfg=*c;
 s->cfg.max_links=s->cfg.max_links<1?1:s->cfg.max_links>12?12:s->cfg.max_links;
 if(s->cfg.jump_bit<0||s->cfg.jump_bit>9)s->cfg.jump_bit=2;
 if(s->cfg.route<0||s->cfg.route>5)s->cfg.route=0;
 s->cfg.finisher_percent=valid_float(s->cfg.finisher_percent)?clampf(s->cfg.finisher_percent,20,250):95;
 s->cfg.nair_x=valid_float(s->cfg.nair_x)?clampf(s->cfg.nair_x,4,30):17;
 s->cfg.nair_y=valid_float(s->cfg.nair_y)?clampf(s->cfg.nair_y,4,25):14;
 s->cfg.ferps_below=valid_float(s->cfg.ferps_below)?clampf(s->cfg.ferps_below,3,18):10;
 s->cfg.tech_lead=valid_float(s->cfg.tech_lead)?clampf(s->cfg.tech_lead,0,18):11;
 s->cfg.drift_gain=valid_float(s->cfg.drift_gain)?clampf(s->cfg.drift_gain,0.02f,0.3f):0.14f;
}
uint32_t kzc_frame_size(void){return sizeof(KzComboFrame);}
uint32_t kzc_config_size(void){return sizeof(KzComboConfig);}
uint32_t kzc_storage_used(void){return sizeof(Combo);}
int32_t kzc_active(const void*v){return v&&((const Combo*)v)->phase>P_ARMED&&((const Combo*)v)->phase!=P_OBSERVE;}
void kzc_cancel(void*v,int32_t e){if(v)stop(v,e);}
int32_t kzc_add_rule(void*v,const KzComboRule*r){
 if(!v||!r)return 0;Combo*s=v;
 if(s->rule_count>=64||r->fighter< -1||r->fighter>255||r->di< -1||r->di>2||
 r->platform< -1||r->platform>1||r->route<1||r->route>5||!valid_float(r->min_percent)||
 !valid_float(r->max_percent)||r->min_percent<0||r->max_percent<r->min_percent||r->max_percent>999)return 0;
 s->rules[s->rule_count++]=*r;return 1;
}
int32_t kzc_pop_event(void*v,int32_t*r,int32_t*d,int32_t*l,int32_t*p){
 if(!v)return 0;Combo*s=v;if(r)*r=s->route;if(d)*d=s->di;if(l)*l=s->links;if(p)*p=s->phase;
 if(!s->ev_count)return 0;int e=s->events[s->ev_read];s->ev_read=(s->ev_read+1)%16;s->ev_count--;return e;
}
static int classify_di(const Combo*s){
 float x=clampf(s->f.stick_x,-1,1)*s->away;
 return absf(x)<0.25f?0:(x<0?1:2);
}
void kzc_arm(void*v,uint64_t now){
 if(!v)return;Combo*s=v;
 if(!s->cfg.enabled||s->phase>P_ARMED||!s->seen||!has(s,C_SCOPE))return;
 if(!has(s,C_TARGET)){event(s,CE_NO_TARGET);return;}
 s->arm_ms=now;s->arm_actor=s->f.actor;s->arm_target=s->f.target;
 s->baseline_damage=s->f.target_percent;s->links=0;s->nair_hit=0;s->last_poll_ms=0;s->delivered=0;
 s->home_y=has(s,C_FLOOR)?s->f.floor_y:s->f.y;
 s->away=signf(s->f.tx-s->f.x);s->observed_electric=0;
 phase(s,P_ARMED);event(s,CE_ARMED);
}
static int elevated(const Combo*s){
 return s->cfg.ferps&&has(s,C_TFLOOR)&&has(s,C_PATH)&&
 s->f.target_floor_y>s->home_y+7&&s->f.target_floor_y<s->home_y+48&&
 absf(s->f.tx-s->f.x)<48;
}
static int choose_route(Combo*s){
 int platform=elevated(s);
 if(s->links>=s->cfg.max_links)return s->f.ty-s->f.y>22?R_UPB:R_USMASH;
 for(int i=0;i<s->rule_count;i++){
  const KzComboRule*r=&s->rules[i];
  if((r->fighter<0||r->fighter==s->f.target_kind)&&(r->di<0||r->di==s->di)&&
    (r->platform<0||r->platform==platform)&&s->f.target_percent>=r->min_percent&&
    s->f.target_percent<=r->max_percent){event(s,CE_RULE);return r->route;}
 }
 if(s->cfg.route)return s->cfg.route;
 if(platform)return R_FERPS;
 /* These are deliberately labelled calibration thresholds, not a KO formula. */
 if(s->f.target_percent>=s->cfg.finisher_percent||s->links>=s->cfg.max_links)
  return s->f.ty-s->f.y>22?R_UPB:R_USMASH;
 return R_NAIR;
}
void kzc_observe(void*v,const KzComboFrame*f){
 if(!v||!f)return;Combo*s=v;
 if(!finite_frame(f)){stop(s,CE_CANCEL);s->seen=0;return;}
 if(s->seen&&(f->epoch<s->f.epoch||f->now_ms<s->f.now_ms)){stop(s,CE_GAP);s->seen=0;}
 s->previous=s->f;s->f=*f;s->seen=1;
 if(!s->cfg.enabled)return;
 if(!has(s,C_SCOPE)||has(s,C_HURT)||has(s,C_DEAD)){stop(s,CE_CANCEL);return;}
 if(s->phase&&(s->f.actor!=s->arm_actor||s->f.target!=s->arm_target||!has(s,C_TARGET))){stop(s,CE_NO_TARGET);return;}
 if(!s->phase)return;
 if(s->phase>P_ARMED&&has(s,C_TDEAD)){stop(s,CE_TARGET_KO);return;}
 if(s->f.percent>s->previous.percent+0.01f){stop(s,CE_CANCEL);return;}
 if(s->f.target_percent+0.1f<s->previous.target_percent){stop(s,CE_CANCEL);return;}
 if(s->phase==P_ARMED&&f->now_ms-s->arm_ms>900){stop(s,CE_TIMEOUT);return;}
 if(s->phase>P_ARMED&&(f->now_ms-s->start_ms>10000||s->links>12)) {stop(s,CE_TIMEOUT);return;}
 if(absf(f->tx-f->x)>90||absf(f->ty-f->y)>85){stop(s,CE_ESCAPE);return;}
 /* Damage alone is not attribution: require P1's infliction flag AND the
    victim's attacker-entry bitmask AND a new damage increase. Shields fail. */
 int new_damage=f->target_percent>s->previous.target_percent+0.01f;
 int hit=new_damage&&has(s,C_HIT)&&has(s,C_ATTRIBUTED)&&!has(s,C_SHIELD_HIT);
 if(hit&&has(s,C_ELECTRIC)&&(s->phase==P_ARMED||s->phase==P_ELECTRIC||s->phase==P_TECH)){
  if(s->phase==P_ARMED){s->start_ms=f->now_ms;s->home_y=has(s,C_FLOOR)?f->floor_y:f->y;}
  s->last_hit_ms=f->now_ms;s->links++;s->away=signf(f->tx-f->x);
  s->di=classify_di(s);s->observed_electric=1;s->nair_hit=0;s->launch_logged=0;
  event(s,CE_HIT);event(s,CE_DI);
  if(s->cfg.observe_only)phase(s,P_OBSERVE);else phase(s,P_SELECT);
 } else if(hit&&has(s,C_NAIR)&&(s->phase==P_NAIR||s->phase==P_FERPS_NAIR)){
  s->nair_hit=1;s->last_hit_ms=f->now_ms;event(s,CE_NAIR_HIT);
 }
 if(s->phase==P_OBSERVE&&f->now_ms-s->last_hit_ms>700){stop(s,0);return;}
 if(s->phase>P_ARMED){
  if(!s->launch_logged&&!has(s,C_TSTOP)&&has(s,C_THURT)&&(absf(f->tvx)+absf(f->tvy)>0.1f)){event(s,CE_LAUNCH);s->launch_logged=1;}
  int d=classify_di(s);if(d!=s->di){s->di=d;event(s,CE_DI);}
  s->aim_x=f->tx+clampf(f->tvx*5,-12,12);
  int tech=has(s,C_TDOWN)?3:has(s,C_TROLL)?2:has(s,C_TTECH)?1:0;
  if(tech&&tech!=s->last_tech)event(s,CE_TECH);s->last_tech=tech;
  if(s->phase!=P_FINISH&&s->phase!=P_ELECTRIC&&f->now_ms-s->last_hit_ms>2600)
   stop(s,CE_ESCAPE);
 }
}
static void neutral(KzOutput*o){o->owns_input=1;o->consume_right=1;o->buttons=0;o->lx=o->ly=o->rx=o->ry=0;}
static void drift(Combo*s,KzOutput*o,float aim){
 float delta=aim-s->f.x;
 o->lx=(int32_t)(32767*clampf(delta*s->cfg.drift_gain,-1,1));
}
static void begin_electric(Combo*s){
 phase(s,P_ELECTRIC);kz_init(&s->electric);s->observed_electric=0;s->dragon_hold=0;s->pulse_started=0;
 event(s,CE_CHAIN_ELECTRIC);
}
static int electric_poll(Combo*s,const KzPoll*in,KzOutput*out){
 KzContext c={s->f.epoch,in->now_ms,s->f.actor,1,has(s,C_READY),
             has(s,C_GROUND)&&!has(s,C_STOP)&&!has(s,C_HURT),s->f.facing>0?1:-1};
 kz_observe(&s->electric,&c);
 KzPoll p=*in;p.buttons=0;
 if(!s->electric.phase&&!s->pulse_started){
  KzOutput ignored;kz_poll_fast(&s->electric,&p,&ignored);p.buttons=KZ_RIGHT;
 }
 kz_poll_fast(&s->electric,&p,out);
 if(out->event==KZ_EVENT_STARTED)s->pulse_started=1;
 if(out->event==KZ_EVENT_BAD_CLOCK||out->event==KZ_EVENT_CANCELLED){stop(s,CE_GAP);return 0;}
 out->event=0;
 if(!out->owns_input)neutral(out);
 return out->owns_input;
}
int32_t kzc_poll(void*v,const KzPoll*in,KzOutput*out){
 if(!v||!in||!out)return 0;Combo*s=v;
 *out=(KzOutput){in->buttons,in->lx,in->ly,in->rx,in->ry,0,0,0,0};
 if(!in->matching_pad)return 0;
 if(!s->cfg.enabled||!in->enabled||!in->connected||!in->training_now||!s->seen||
  in->now_ms<s->f.now_ms||in->now_ms-s->f.now_ms>80||!has(s,C_SCOPE)){
  stop(s,CE_CANCEL);return 0;
 }
 if(in->buttons&KZ_CANCEL_MASK){stop(s,CE_CANCEL);return 0;}
 if(s->phase<=P_ARMED||s->phase==P_OBSERVE)return 0;
 if(!has(s,C_TARGET)||has(s,C_HURT)||has(s,C_DEAD)){stop(s,CE_CANCEL);return 0;}
 if(s->last_poll_ms&&in->now_ms-s->last_poll_ms>100){stop(s,CE_GAP);return 0;}
 s->last_poll_ms=in->now_ms;
 int tick=!s->delivered||s->f.epoch>s->last_epoch;
 if(!tick){*out=s->cached;return out->owns_input;}
 s->last_epoch=s->f.epoch;s->delivered=1;
 neutral(out);out->phase=100+s->phase;
 if(has(s,C_STOP)){s->cached=*out;return 1;}
 if(tick)s->polls++;
 uint64_t age=in->now_ms-s->phase_ms;
 float dx=s->aim_x-s->f.x,dy=s->f.ty-s->f.y;
 uint64_t jump=UINT64_C(1)<<s->cfg.jump_bit;
 if(age>2200){stop(s,CE_TIMEOUT);return 0;}
 switch(s->phase){
 case P_SELECT:
  if(!has(s,C_READY)||!has(s,C_GROUND))break;
  if(!has(s,C_PATH)){stop(s,CE_ESCAPE);return 0;}
  s->route=choose_route(s);event(s,CE_ROUTE);
  if(s->route==R_FERPS){
   if(!elevated(s)){stop(s,CE_ESCAPE);return 0;}
   s->platform_y=s->f.target_floor_y;phase(s,P_FERPS_DASH);event(s,CE_PLATFORM);
  }else if(s->route==R_NAIR){phase(s,P_JUMP);}
  else phase(s,P_FINISH_PREP);
  break;
 case P_JUMP:
  if(has(s,C_GROUND)){out->buttons=jump;drift(s,out,s->aim_x);if(age>300){stop(s,CE_JUMP_MISSING);return 0;}}
  else {s->air_release=0;phase(s,P_AIR);event(s,CE_AIRBORNE);}
  break;
 case P_AIR:
  drift(s,out,s->aim_x);
  if(tick)s->air_release++;
  if(has(s,C_GROUND)){stop(s,CE_ESCAPE);return 0;}
  if(s->air_release>=1&&has(s,C_AIR_READY)&&absf(dx)<=s->cfg.nair_x&&
     dy<5&&dy> -s->cfg.nair_y){
   neutral(out);out->buttons=KZ_A;phase(s,P_NAIR);s->nair_hit=0;
  }
  break;
 case P_NAIR:
  drift(s,out,s->aim_x);
  if(!has(s,C_GROUND)&&s->f.vy<=0&&s->f.y>s->home_y+5)out->ly=-32767;
  if(has(s,C_GROUND)){
   if(!s->nair_hit){stop(s,CE_ESCAPE);return 0;}
   phase(s,P_LAND);
  }
  break;
 case P_LAND:
  if(!has(s,C_GROUND))break;
  if(!has(s,C_READY))break;
  if(!has(s,C_THURT)&&!has(s,C_TTECH)&&!has(s,C_TROLL)&&!has(s,C_TDOWN)){stop(s,CE_ESCAPE);return 0;}
  if(has(s,C_TTECH)||has(s,C_TROLL)||has(s,C_TDOWN)){phase(s,P_TECH);break;}
  if(absf(dx)>20||absf(dy)>22){stop(s,CE_ESCAPE);return 0;}
  begin_electric(s);
  break;
 case P_ELECTRIC:
  if(!has(s,C_GROUND)){stop(s,CE_CANCEL);return 0;}
  if(!s->pulse_started&&!has(s,C_READY))break;
  if(!s->pulse_started||s->electric.phase)electric_poll(s,in,out);
  /* Wait for attributed hit; never blindly continue after a whiff or WGF. */
  if(age>750){stop(s,CE_WRONG);return 0;}
  break;
 case P_FINISH_PREP:
  if(!has(s,C_READY))break;
  if(absf(dx)>12&&has(s,C_GROUND)&&has(s,C_PATH)){
   drift(s,out,s->aim_x);if(age>350){stop(s,CE_ESCAPE);return 0;}break;
  }
  phase(s,P_FINISH);s->finish_polls=0;s->dragon_hold=0;s->pulse_started=0;kz_init(&s->electric);
  break;
 case P_FINISH:
  if(s->route==R_DRAGON){
   if(s->dragon_hold){out->buttons=KZ_A;if(tick)s->dragon_hold++;if(s->dragon_hold>8){event(s,CE_FINISH);stop(s,0);}}
   else {electric_poll(s,in,out);if(out->phase==4)s->dragon_hold=1;}
  }else {
   if(s->finish_polls==0){out->ly=32767;out->buttons=s->route==R_UPB?(UINT64_C(1)<<1):KZ_A;}
   if(tick)s->finish_polls++;
   if(s->finish_polls>2){event(s,CE_FINISH);stop(s,0);}
  }
  break;
 case P_FERPS_DASH:
  out->lx=32767*(int32_t)signf(dx);
  if(s->polls>=2)phase(s,P_FERPS_RAR);
  break;
 case P_FERPS_RAR:
  out->lx=-32767*(int32_t)signf(dx);out->buttons=jump;
  if(!has(s,C_GROUND)){s->air_release=0;phase(s,P_FERPS_RISE);}
  else if(age>300){stop(s,CE_JUMP_MISSING);return 0;}
  break;
 case P_FERPS_RISE:
  drift(s,out,s->aim_x);if(tick)s->air_release++;
  if(has(s,C_GROUND)){stop(s,CE_ESCAPE);return 0;}
  if(s->air_release>=1&&has(s,C_AIR_READY)&&!has(s,C_DOUBLE_JUMP)&&
    (s->f.y>=s->platform_y-s->cfg.ferps_below||s->f.vy<=0)){
   neutral(out);out->buttons=jump|KZ_A;phase(s,P_FERPS_DJC);event(s,CE_PLATFORM);
  }
  break;
 case P_FERPS_DJC:
  drift(s,out,s->aim_x);
  if(has(s,C_NAIR)){phase(s,P_FERPS_NAIR);}
  else if(age>200){stop(s,CE_JUMP_MISSING);return 0;}
  break;
 case P_FERPS_NAIR:
  drift(s,out,s->aim_x);
  if(!has(s,C_GROUND)&&s->f.vy<=0&&s->f.y>s->platform_y+1)out->ly=-32767;
  if(has(s,C_GROUND)){
   if(absf(s->f.y-s->platform_y)>5){stop(s,CE_ESCAPE);return 0;}
   phase(s,P_TECH);event(s,CE_PLATFORM);
  }
  break;
 case P_TECH:
  if(!has(s,C_GROUND)||!has(s,C_READY))break;
  if(!has(s,C_PATH)||absf(dy)>12){stop(s,CE_ESCAPE);return 0;}
  if(absf(dx)>8){drift(s,out,s->aim_x);break;}
  if(has(s,C_TDOWN)){phase(s,P_CROUCH);break;}
  if(has(s,C_TTECH)||has(s,C_TROLL)){
   float remaining=s->f.target_end-s->f.target_frame;
   if(s->f.target_end>0&&remaining>s->cfg.tech_lead)break;
  }else if(!has(s,C_THURT)&&age>350){stop(s,CE_ESCAPE);return 0;}
  begin_electric(s);
  break;
 case P_CROUCH:
  out->ly=-32767;
  if(!has(s,C_TDOWN)){phase(s,P_TECH);break;}
  if(s->polls>=7){out->buttons=KZ_A;s->baseline_damage=s->f.target_percent;phase(s,P_JAB);}
  break;
 case P_JAB:
  out->ly=-32767;
  if(s->polls>=2)phase(s,P_JAB_WAIT);
  break;
 case P_JAB_WAIT:
  if(s->f.target_percent>s->baseline_damage+0.01f&&has(s,C_ATTRIBUTED)&&has(s,C_HIT)&&!has(s,C_SHIELD_HIT)&&has(s,C_READY))begin_electric(s);
  else if(age>700){stop(s,CE_ESCAPE);return 0;}
  break;
 default:stop(s,CE_CANCEL);return 0;
 }
 s->cached=*out;return 1;
}
