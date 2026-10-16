#include "request_gate.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static KzRequestState s;
static KzRequestContext c;
static KzPoll p;
static KzOutput o;
static unsigned count;
static int fast, moving;
static void poll(uint64_t buttons){p.buttons=buttons;kz_request_poll_ex(&s,&p,&o,fast,moving);}
static void step(int idle,int brake,int allowed,int jump){
 c.base.epoch+=jump;c.base.now_ms+=16;p.now_ms=c.base.now_ms;
 c.base.actionable=idle;c.brake=brake;c.queueable=allowed;
 c.base.can_continue=allowed;kz_request_observe(&s,&c);
}
static void init(int compact,int direct,int lr){
 kz_request_init(&s);fast=compact;moving=direct;
 c=(KzRequestContext){{1,100,0x1234,1,1,1,lr},1,0};
 p=(KzPoll){0,1234,-456,789,-321,1,1,1,1,100};
 kz_request_observe(&s,&c);poll(0);
}
static void done(const char *msg){printf("SPEED PASS %02u %s\n",++count,msg);}
static void physical(void){CHECK(o.buttons==p.buttons&&o.lx==p.lx&&o.ly==p.ly&&o.rx==p.rx&&o.ry==p.ry&&!o.owns_input&&!o.consume_right);}
static void finish(int held){
 if(!fast){step(0,1,1,2);poll(held?KZ_RIGHT:0);CHECK(o.phase==2);}
 step(0,1,1,2);poll(held?KZ_RIGHT:0);CHECK(o.phase==3&&o.ly==-32767);
 step(0,1,1,1);poll(held?KZ_RIGHT:0);CHECK(o.phase==4&&o.buttons==KZ_A);
 step(0,0,1,2);poll(held?KZ_RIGHT:0);CHECK(o.phase==5&&o.buttons==0);
 step(0,0,1,1);poll(held?KZ_RIGHT:0);CHECK(o.event==2&&!o.owns_input);
}
int main(void){
 _Static_assert(sizeof(KzState)==96,"pulse ABI unchanged");
 _Static_assert(sizeof(KzRequestState)==192,"request ABI unchanged");
 for(int lr=-1;lr<=1;lr+=2){
  init(1,0,lr);poll(KZ_RIGHT);CHECK(o.phase==1&&o.event==1&&o.lx==32767*lr);
  step(0,1,1,2);poll(KZ_RIGHT);CHECK(o.phase==3&&o.lx==0&&o.ly==-32767);
  step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==4&&o.buttons==KZ_A&&o.lx==23170*lr&&o.ly==-23170);
  CHECK(p.now_ms-s.last_attempt_ms==32);
  step(0,0,1,2);poll(KZ_RIGHT);CHECK(o.phase==5&&!o.buttons);
  step(0,0,1,1);poll(KZ_RIGHT);CHECK(o.event==2);
  done(lr<0?"left compact input order and one-poll attack pulse":"right compact input order and one-poll attack pulse");
 }
 init(0,0,1);poll(KZ_RIGHT);step(0,1,1,2);poll(KZ_RIGHT);CHECK(o.phase==2);
 step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==3);step(0,1,1,1);poll(KZ_RIGHT);
 CHECK(o.phase==4&&p.now_ms-s.last_attempt_ms==48);done("legacy path retains neutral: 48ms vs compact 32ms in a 16ms fixture");
 init(1,0,1);poll(KZ_RIGHT);for(int i=0;i<100;i++){poll(KZ_RIGHT);CHECK(o.phase==1);}done("no same-observation step compression");
 init(1,0,1);poll(KZ_RIGHT);for(int i=0;i<3;i++)step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==3);done("extra observations cannot skip down");
 init(1,1,1);step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==1&&o.event==1);CHECK(s.context.base.actionable==0);done("direct moving start without writing an idle context to request gate");
 init(1,0,1);step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==0&&o.event==10&&!o.lx);done("disabled direct mode retains neutral braking");
 init(1,1,1);step(0,0,1,1);poll(KZ_RIGHT);CHECK(o.phase==0&&!o.owns_input);done("movement hint cannot start from an attack or nonmovement state");
 init(1,0,1);step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.event==10);poll(0);moving=1;step(0,1,1,1);poll(0);CHECK(o.event==1);done("buffer starts at first permitted movement observation");
 init(1,1,1);step(0,1,1,1);poll(KZ_RIGHT);finish(1);
 for(int i=0;i<8;i++){step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==0);}
 step(1,0,1,1);poll(KZ_RIGHT);CHECK(o.event==1);done("fast movement does not bypass repeat recovery WAIT gate");
 finish(1);poll(0);step(1,0,1,1);poll(0);CHECK(!o.owns_input);done("release stops continuous repeats");
 init(1,1,1);poll(KZ_RIGHT);finish(0);poll(KZ_RIGHT);poll(0);step(1,0,1,1);poll(0);CHECK(o.event==1);done("single pending tap still executes after recovery");
 init(1,1,1);poll(KZ_RIGHT);poll(KZ_RIGHT|KZ_A);CHECK(o.buttons==KZ_A&&!o.owns_input&&!s.repeat_latched);done("manual attack cancellation still wins");
 init(1,1,1);step(0,1,1,1);p.training_now=0;poll(KZ_RIGHT);physical();done("outside training is unchanged");
 init(1,1,1);step(0,1,1,1);p.enabled=0;poll(KZ_RIGHT);physical();done("disabled config never accelerates input");
 init(1,1,1);step(0,1,1,1);p.matching_pad=0;poll(KZ_RIGHT);physical();done("other controller untouched");
 init(1,1,1);step(0,1,1,1);p.connected=0;poll(KZ_RIGHT);physical();done("disconnected input untouched");
 init(1,1,1);step(0,1,1,1);p.now_ms+=81;poll(KZ_RIGHT);physical();done("stale context cannot direct-start");
 init(1,1,0);step(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==0&&!o.lx&&!o.ly&&!o.buttons);done("unknown facing only neutralizes; cannot start an electric");
 init(1,1,1);step(0,1,0,1);poll(KZ_RIGHT);CHECK(!o.owns_input&&!s.queued);done("unsupported status rejects direct hint");
 init(1,1,1);poll(KZ_RIGHT);c.base.now_ms+=81;c.base.epoch+=2;p.now_ms=c.base.now_ms;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(o.event==5&&!o.owns_input);done("80ms per-phase watchdog remains");
 init(1,1,1);poll(KZ_RIGHT);c.base.can_continue=0;c.base.epoch++;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(!o.owns_input);done("hitstop during active pulse cancels");
 init(1,1,1);poll(KZ_RIGHT);c.base.permitted=0;step(0,1,1,1);poll(KZ_RIGHT);physical();done("airborne/revoked scope cancels");
 init(1,1,1);poll(KZ_RIGHT);c.base.actor++;step(0,1,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input&&!s.repeat_latched);done("fighter identity change cancels");
 uint32_t rng=0x825763ab;
 init(1,1,1);
 for(int i=0;i<100000;i++){
  rng=rng*1664525u+1013904223u;
  unsigned mode=(rng>>16)%12;
  step(mode<3,mode==3,mode<7,1);
  c.base.permitted=mode!=9;kz_request_observe(&s,&c);
  p.enabled=mode!=10;p.training_now=mode!=11;p.connected=mode!=8;p.matching_pad=mode!=7;
  poll(mode<5?KZ_RIGHT:((uint64_t)rng&0xFFFF));
  int scope=p.enabled&&p.training_now&&p.connected&&p.matching_pad&&c.base.permitted;
  if(!scope)physical();
  CHECK(o.phase!=2&&o.phase>=0&&o.phase<=5);
  if(o.owns_input){CHECK(scope&&c.queueable&&c.base.can_continue);CHECK(o.buttons==0||o.buttons==KZ_A);}
 }
 done("100000 compact-mode randomized safety checks");
 printf("SPEED RESULT: %u cases passed; synthetic input timing, not Switch proof.\n",count);
}
