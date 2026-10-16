#include "request_gate.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#define CHECK(x) do { if (!(x)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static KzRequestState s;
static KzRequestContext c;
static KzPoll p;
static KzOutput o;
static unsigned tests;
static void done(const char *name){++tests;printf("PASS %02u %s\n",tests,name);}
static void poll(uint64_t buttons){p.buttons=buttons;kz_request_poll(&s,&p,&o);}
static void observe(int idle,int movement,int queueable,int steps){
    c.base.epoch+=(uint64_t)steps;c.base.now_ms+=16;p.now_ms=c.base.now_ms;
    c.base.actionable=idle;c.brake=movement;c.queueable=queueable;
    c.base.can_continue=queueable;kz_request_observe(&s,&c);
}
static void reset(void){
    kz_request_init(&s);c=(KzRequestContext){{1,100,0x1234,1,1,1,1},1,0};
    p=(KzPoll){0,1234,456,222,333,1,1,1,1,100};
    kz_request_observe(&s,&c);poll(0);
}
static void start(void){poll(KZ_RIGHT);CHECK(o.event==1&&o.phase==1);}
static void complete(int held){
    for(int phase=2;phase<=5;phase++){
        observe(0,phase<5,1,phase==2?2:1);poll(held?KZ_RIGHT:0);
        CHECK(o.phase==phase&&o.owns_input);
    }
    observe(0,0,1,1);poll(held?KZ_RIGHT:0);CHECK(o.event==2&&!o.owns_input);
}
static void physical(void){CHECK(o.buttons==p.buttons&&o.lx==p.lx&&o.ly==p.ly&&o.rx==p.rx&&o.ry==p.ry);CHECK(!o.owns_input&&!o.consume_right);}
int main(void){
    _Static_assert(sizeof(KzState)==96,"Pulse ABI");
    _Static_assert(sizeof(KzRequestContext)==48,"Request context ABI");
    _Static_assert(sizeof(KzRequestState)==192,"Request state ABI");
    CHECK(kz_request_state_size()==192);
    for(int lr=-1;lr<=1;lr+=2){
        reset();c.base.facing=lr;kz_request_observe(&s,&c);start();CHECK(o.lx==32767*lr);
        observe(0,1,1,2);poll(KZ_RIGHT);CHECK(o.phase==2&&!o.lx&&!o.ly);
        observe(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==3&&!o.lx&&o.ly==-32767);
        observe(0,1,1,1);poll(KZ_RIGHT);CHECK(o.phase==4&&o.lx==23170*lr&&o.buttons==KZ_A);
        observe(0,0,1,2);poll(KZ_RIGHT);CHECK(o.phase==5&&!o.buttons);
        observe(0,0,1,1);poll(KZ_RIGHT);CHECK(o.event==2&&!o.owns_input);
        done(lr<0?"left-facing original electric pulse unchanged":"right-facing original electric pulse unchanged");
    }
    reset();observe(0,1,1,1);poll(KZ_RIGHT);CHECK(o.event==10&&o.phase==0&&!o.lx&&!o.ly&&o.consume_right);
    for(int i=0;i<10;i++){observe(0,1,1,1);poll(0);CHECK(o.phase==0&&!o.lx);}
    observe(1,0,1,1);poll(0);CHECK(o.event==1&&o.phase==1);done("tap after dash buffers neutral stop until WAIT");
    reset();observe(0,1,1,1);poll(KZ_RIGHT);
    for(int i=0;i<30;i++){observe(0,1,1,1);poll(0);}
    CHECK(!s.queued&&!o.owns_input&&o.lx==p.lx);observe(1,0,1,1);poll(0);CHECK(!o.owns_input);done("released dash request expires without delayed attack");
    reset();observe(0,1,1,1);poll(KZ_RIGHT);
    for(int i=0;i<35;i++){observe(0,1,1,1);poll(KZ_RIGHT);CHECK(!o.phase);}
    observe(1,0,1,1);poll(KZ_RIGHT);CHECK(o.event==1);done("held request survives long natural movement stop");
    reset();start();complete(1);
    for(int i=0;i<30;i++){observe(0,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input&&!o.phase);}
    observe(1,0,1,1);poll(KZ_RIGHT);CHECK(o.event==1);done("hold repeats only after observed recovery to WAIT");
    complete(1);observe(0,0,1,1);poll(0);observe(1,0,1,1);poll(0);CHECK(!o.owns_input);done("release stops future held repeats");
    reset();start();complete(0);
    poll(KZ_RIGHT);CHECK(o.event==10&&!o.owns_input);poll(0);
    for(int i=0;i<5;i++){observe(0,0,1,1);poll(0);}
    observe(1,0,1,1);poll(0);CHECK(o.event==1);done("tap during electric recovery queues one next attempt");
    reset();start();complete(0);
    for(int i=0;i<10;i++){observe(0,0,1,1);poll(KZ_RIGHT);poll(0);CHECK(s.queued==1);}
    observe(1,0,1,1);poll(0);CHECK(o.event==1);complete(0);observe(1,0,1,1);poll(0);CHECK(!o.owns_input);done("spam coalesces into one queued attempt not a backlog");
    reset();start();complete(0);poll(KZ_RIGHT);poll(0);
    for(int i=0;i<30;i++){observe(0,0,1,1);poll(0);}
    observe(1,0,1,1);poll(0);CHECK(!o.owns_input);done("stale tap during recovery expires");
    reset();observe(0,0,0,1);poll(KZ_RIGHT);CHECK(o.event==12&&!o.owns_input&&!(o.buttons&KZ_RIGHT));
    observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);poll(0);start();done("busy unapproved status blocks and consumes taunt");
    reset();start();poll(KZ_RIGHT|KZ_A);CHECK(!o.owns_input&&o.buttons==KZ_A&&!s.repeat_latched);
    observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);done("manual attack cancels and held Right cannot rearm");
    for(int bit=0;bit<16;bit++){
        if(bit==14)continue;
        reset();observe(0,1,1,1);poll(KZ_RIGHT);poll(KZ_RIGHT|(UINT64_C(1)<<bit));
        CHECK(!o.owns_input&&!s.queued&&o.buttons==(UINT64_C(1)<<bit));
    }done("all manual buttons cancel queued movement without taunt");
    reset();start();p.training_now=0;poll(KZ_RIGHT);physical();CHECK(!s.repeat_latched&&!s.queued);
    p.training_now=1;observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);done("leaving training clears requests and requires release");
    reset();start();p.connected=0;poll(KZ_RIGHT);physical();CHECK(!s.core.phase);done("disconnect cancels and passes raw state through");
    reset();start();p.matching_pad=0;poll(KZ_A);physical();CHECK(s.core.phase==1);done("other pad is never modified and cannot cancel");
    reset();start();p.enabled=0;poll(KZ_RIGHT);physical();CHECK(!s.core.phase);done("disabling cancels active request");
    reset();p.enabled=0;poll(0);poll(KZ_RIGHT);physical();CHECK(o.event==4);done("diagnostics retain original raw D-pad report");
    reset();start();c.base.actor++;observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input&&!s.queued);done("fighter identity change cancels");
    reset();start();c.base.epoch=0;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(!o.owns_input);done("backwards observation cancels");
    reset();observe(0,1,1,1);poll(KZ_RIGHT);p.now_ms=c.base.now_ms+81;poll(0);physical();CHECK(!s.queued);done("stale callback cannot keep a queue or neutralization alive");
    reset();start();c.base.permitted=0;observe(0,0,1,1);poll(KZ_RIGHT);physical();CHECK(!s.repeat_latched);done("airborne or revoked context cancels");
    reset();start();complete(1);
    for(int i=0;i<5;i++){c.base.epoch++;c.base.now_ms+=16;p.now_ms=c.base.now_ms;c.base.can_continue=0;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(!o.owns_input&&s.repeat_latched);}
    observe(0,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);observe(1,0,1,1);poll(KZ_RIGHT);CHECK(o.event==1);done("own hitlag while recovering does not inject or discard hold");
    reset();start();c.base.can_continue=0;c.base.epoch++;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(!o.owns_input&&!s.repeat_latched);done("stop during active pulse cancels safely");
    reset();start();for(int i=0;i<80;i++){poll(KZ_RIGHT);CHECK(o.phase==1);}done("many HID polls in one callback cannot advance electric");
    reset();start();for(int i=0;i<12;i++){observe(0,1,1,1);if(i<4)continue;break;}
    poll(KZ_RIGHT);CHECK(o.phase==2);done("extra callbacks do not skip output steps");
    reset();start();c.base.epoch+=7;c.base.now_ms+=81;p.now_ms=c.base.now_ms;kz_request_observe(&s,&c);poll(KZ_RIGHT);
    CHECK(o.event==5&&!o.owns_input&&!s.repeat_latched);done("80ms pulse watchdog retained");
    reset();start();complete(1);
    for(int i=0;i<165;i++){observe(0,0,1,1);poll(KZ_RIGHT);}
    CHECK(!s.repeat_latched&&!s.recovering);observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);done("bounded recovery watchdog prevents hidden indefinite retry");
    reset();start();complete(1);observe(0,1,1,1);poll(KZ_RIGHT);
    CHECK(o.owns_input&&o.phase==0&&!o.lx&&!o.ly);observe(1,0,1,1);poll(KZ_RIGHT);CHECK(o.event==1);
    done("held movement after recovery neutralizes until WAIT then repeats");
    reset();c.base.facing=0;kz_request_observe(&s,&c);poll(KZ_RIGHT);CHECK(!o.owns_input&&!(o.buttons&KZ_RIGHT));done("unknown facing never injects or taunts");
    reset();observe(0,1,1,1);poll(KZ_RIGHT);poll(0);c.base.facing=-1;observe(1,0,1,1);poll(0);CHECK(o.phase==1&&o.lx==-32767);done("buffer captures facing at execution not the dash tap");
    reset();start();for(int i=0;i<5;i++){observe(1,0,1,1);poll(KZ_RIGHT);}
    for(int i=0;i<20;i++){observe(1,0,1,1);poll(KZ_RIGHT);CHECK(!o.owns_input);}done("must observe busy then WAIT before repeat");
    kz_request_init(&s);c=(KzRequestContext){{1,100,0x1234,1,1,1,1},1,0};kz_request_observe(&s,&c);p=(KzPoll){KZ_RIGHT,0,0,0,0,1,1,1,1,100};poll(KZ_RIGHT);CHECK(!o.owns_input);done("held trigger on entry requires release first");
    reset();kz_request_init(NULL);kz_request_observe(NULL,&c);kz_request_observe(&s,NULL);kz_request_poll(NULL,&p,&o);kz_request_poll(&s,NULL,&o);kz_request_poll(&s,&p,NULL);done("null API arguments tolerated");
    reset();
    uint32_t rng=0x19ab7341;
    for(unsigned i=0;i<100000;i++){
        rng=rng*1664525u+1013904223u;
        unsigned mode=(rng>>16)%12;
        observe(mode<3,mode==3,mode<7,1);
        c.base.permitted=mode!=9;kz_request_observe(&s,&c);
        p.enabled=mode!=10;p.training_now=mode!=11;p.connected=mode!=8;p.matching_pad=mode!=7;
        uint64_t buttons=mode<5?KZ_RIGHT:((uint64_t)rng&0xFFFF);
        poll(buttons);
        int allowed=p.enabled&&p.training_now&&p.connected&&p.matching_pad&&s.context.base.permitted;
        if(!allowed)physical();
        CHECK(o.phase>=0&&o.phase<=5);
        if(o.owns_input){CHECK(allowed&&c.queueable&&c.base.can_continue);CHECK(o.buttons==0||o.buttons==KZ_A);}
        if(allowed)CHECK(!(o.buttons&KZ_RIGHT));
        CHECK(s.queued==0||s.queued==1);
    }
    done("100000 randomized request safety invariants");
    printf("RESULT: %u request cases passed; synthetic tests, not console proof.\n",tests);
    printf("ABI: RequestContext=%zu RequestState=%zu\n",sizeof(KzRequestContext),sizeof(KzRequestState));
}
