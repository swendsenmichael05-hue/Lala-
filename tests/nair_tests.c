#include "request_gate.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"NAIR FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static KzRequestState s;
static KzRequestContext c;
static KzPoll p;
static KzOutput o;
static unsigned n;
static void done(const char *label){printf("NAIR PASS %02u %s\n",++n,label);}
static void poll(uint64_t buttons){p.buttons=buttons;kz_request_poll_ex(&s,&p,&o,1,0);}
static void step(int permitted,int ready,int can_continue,int queueable,int brake){
 c.base.epoch++;c.base.now_ms+=16;p.now_ms=c.base.now_ms;
 c.base.permitted=permitted;c.base.actionable=ready;c.base.can_continue=can_continue;
 c.queueable=queueable;c.brake=brake;kz_request_observe(&s,&c);
}
static void reset(void){
 kz_request_init(&s);c=(KzRequestContext){{1,100,0x1234,1,1,1,1},1,0};
 p=(KzPoll){0,13000,-20000,123,456,1,1,1,1,100};
 kz_request_observe(&s,&c);poll(0);
}
static void axes_unchanged(void){CHECK(!o.owns_input&&o.lx==p.lx&&o.ly==p.ly&&o.rx==p.rx&&o.ry==p.ry);}
static void request_in_nair(void){step(1,0,0,1,0);poll(KZ_RIGHT);CHECK(o.event==KZ_EVENT_BUFFERED&&s.queued);axes_unchanged();}
int main(void){
 reset();request_in_nair();CHECK(!(o.buttons&KZ_RIGHT));done("airborne nair tap buffers without automated sticks or attack");
 for(int i=0;i<10;i++){step(1,0,0,1,0);poll(0);CHECK(s.queued&&!o.phase);axes_unchanged();}done("air drift and fast fall preserved while waiting");
 for(int i=0;i<9;i++){step(1,0,0,1,0);poll(0);CHECK(s.queued);axes_unchanged();}done("landing recovery preserves the request without premature input");
 step(1,1,1,1,0);poll(0);CHECK(o.event==KZ_EVENT_STARTED&&o.phase==1);done("native landing permission starts buffered request immediately");
 step(1,0,1,1,1);poll(0);CHECK(o.phase==3&&!o.lx&&o.ly==-32767);
 step(1,0,1,1,1);poll(0);CHECK(o.phase==4&&o.buttons==KZ_A);
 step(1,0,1,1,0);poll(0);CHECK(o.phase==5&&!o.buttons);
 step(1,0,1,1,0);poll(0);CHECK(o.event==KZ_EVENT_INPUT_COMPLETE);done("landing request uses unchanged compact pulse");
 reset();request_in_nair();for(int i=0;i<70;i++){step(1,0,0,1,0);poll(0);}CHECK(!s.queued);step(1,1,1,1,0);poll(0);CHECK(!o.owns_input);done("released aerial tap expires after bounded window");
 reset();request_in_nair();poll(0);poll(KZ_A);CHECK(!s.queued&&o.buttons==KZ_A);axes_unchanged();done("manual attack cancels aerial request");
 reset();request_in_nair();poll(0);poll(UINT64_C(4));CHECK(!s.queued&&o.buttons==4);axes_unchanged();done("manual jump cancels aerial request");
 reset();request_in_nair();step(1,0,0,0,0);poll(0);CHECK(!s.queued);step(1,1,1,1,0);poll(0);CHECK(!o.owns_input);done("unsupported action interrupts nair chain");
 reset();request_in_nair();p.training_now=0;poll(KZ_RIGHT);CHECK(!s.queued&&o.buttons==KZ_RIGHT);axes_unchanged();done("leaving training clears the airborne queue");
 reset();request_in_nair();p.connected=0;poll(0);CHECK(!s.queued);axes_unchanged();done("disconnect clears airborne queue");
 reset();request_in_nair();p.enabled=0;poll(KZ_RIGHT);CHECK(!s.queued&&o.buttons==KZ_RIGHT);axes_unchanged();done("disabled config passes through inputs");
 reset();request_in_nair();p.matching_pad=0;poll(KZ_RIGHT|KZ_A);CHECK(s.queued&&o.buttons==(KZ_RIGHT|KZ_A));axes_unchanged();done("unrelated controller cannot alter queued nair");
 reset();request_in_nair();p.now_ms=c.base.now_ms+81;poll(0);CHECK(!s.queued);axes_unchanged();done("stale airborne context cancels");
 reset();request_in_nair();c.base.facing=-1;step(1,1,1,1,0);poll(0);CHECK(o.phase==1&&o.lx==-32767);done("landing uses current facing rather than airborne request facing");
 reset();request_in_nair();for(int i=0;i<8;i++){step(1,0,0,1,0);poll(KZ_RIGHT);CHECK(!o.owns_input&&s.repeat_latched);}step(1,1,1,1,0);poll(KZ_RIGHT);CHECK(o.event==1);done("held Right survives nair and landing before first attempt");
 printf("NAIR RESULT: %u synthetic integration cases passed; no console proof.\n",n);
}
