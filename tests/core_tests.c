#include "ewgf_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static KzState s;
static KzContext c;
static KzPoll p;
static KzOutput o;
static unsigned tests;
static void reset(void) {
    kz_init(&s);
    c=(KzContext){1,100,0x1234,1,1,1,1}; kz_observe(&s,&c);
    p=(KzPoll){0,123,-456,789,-321,1,1,1,1,100};
    kz_poll(&s,&p,&o);
}
static void poll_buttons(uint64_t buttons) { p.buttons=buttons; kz_poll(&s,&p,&o); }
static void trigger(void) { poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_STARTED); CHECK(o.phase==KZ_FORWARD); }
static void next(void) { c.epoch++; c.now_ms+=16; p.now_ms=c.now_ms; kz_observe(&s,&c); }
static void same_physical(void) {
    CHECK(o.buttons==p.buttons && o.lx==p.lx && o.ly==p.ly && o.rx==p.rx && o.ry==p.ry);
    CHECK(!o.owns_input && !o.consume_right);
}
static void case_done(const char *name) { tests++; printf("PASS %02u %s\n",tests,name); }
static void basic_sequence(int facing) {
    reset(); c.facing=facing; next(); trigger();
    CHECK(o.lx==32767*facing && o.ly==0 && o.buttons==0);
    next(); poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_NEUTRAL && !o.lx && !o.ly && !o.buttons);
    next(); poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_DOWN && o.lx==0 && o.ly==-32767);
    next(); poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_DIAGONAL_ATTACK);
    CHECK(o.lx==23170*facing && o.ly==-23170 && o.buttons==KZ_A);
    next(); poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_RELEASE && !o.lx && !o.ly && !o.buttons);
    next(); poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_INPUT_COMPLETE && !o.owns_input);
    CHECK(o.consume_right && !(o.buttons&KZ_RIGHT));
    case_done(facing==1?"right-facing complete pulse":"left-facing complete pulse");
}
static uint64_t rndstate=0x923943ad;
static uint32_t rnd(void) { rndstate^=rndstate<<13; rndstate^=rndstate>>7; rndstate^=rndstate<<17; return (uint32_t)rndstate; }
int main(void) {
    _Static_assert(sizeof(KzContext)==40,"Context ABI");
    _Static_assert(sizeof(KzPoll)==48,"Poll ABI");
    _Static_assert(sizeof(KzOutput)==40,"Output ABI");
    _Static_assert(sizeof(KzState)==88,"State ABI");
    _Static_assert(offsetof(KzPoll,now_ms)==40,"Poll offset");
    basic_sequence(1); basic_sequence(-1);
    reset(); trigger(); for(int i=0;i<100;i++){ poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_FORWARD && o.event==0); }
    case_done("100 HID polls cannot advance a frame");
    reset(); trigger(); kz_observe(&s,&c); poll_buttons(KZ_RIGHT); CHECK(o.phase==KZ_FORWARD);
    case_done("duplicate observer epoch cannot advance");
    reset(); trigger(); next(); next(); poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input);
    case_done("missing phase delivery cancels");
    reset(); trigger(); c.epoch+=2; c.now_ms+=32; p.now_ms=c.now_ms; kz_observe(&s,&c); poll_buttons(KZ_RIGHT);
    CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input); case_done("skipped callback epoch cancels");
    reset(); trigger(); c.epoch=0; kz_observe(&s,&c); poll_buttons(KZ_RIGHT);
    CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input); case_done("backwards epoch cancels");
    reset(); trigger(); c.actor++; next(); poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_CANCELLED && !o.owns_input);
    case_done("fighter identity change cancels");
    reset(); trigger(); c.facing=-1; next(); poll_buttons(KZ_RIGHT); next(); poll_buttons(KZ_RIGHT); next(); poll_buttons(KZ_RIGHT);
    CHECK(o.lx==23170); case_done("facing captured at initial press");
    reset(); p.enabled=0; poll_buttons(KZ_RIGHT); same_physical(); CHECK(o.event==KZ_EVENT_DIAGNOSTIC);
    case_done("diagnostics never inject or consume taunt");
    reset(); p.training_now=0; poll_buttons(KZ_RIGHT); same_physical(); case_done("outside training never injects");
    reset(); trigger(); p.training_now=0; poll_buttons(KZ_RIGHT); same_physical(); CHECK(o.event==KZ_EVENT_CANCELLED);
    case_done("leaving training cancels active sequence");
    reset(); p.connected=0; poll_buttons(KZ_RIGHT); same_physical(); case_done("disconnected pad never starts");
    reset(); trigger(); p.connected=0; poll_buttons(KZ_RIGHT); same_physical(); CHECK(!s.phase);
    case_done("disconnect cancels active sequence");
    reset(); p.matching_pad=0; poll_buttons(KZ_RIGHT); same_physical(); CHECK(!s.right_was_down);
    case_done("other pad input and edge are untouched");
    reset(); trigger(); p.matching_pad=0; poll_buttons(KZ_A); same_physical(); CHECK(s.phase==KZ_FORWARD);
    case_done("another pad cannot cancel selected pad");
    reset(); p.now_ms=181; poll_buttons(KZ_RIGHT); same_physical(); case_done("stale context cannot start");
    reset(); trigger(); p.now_ms=181; poll_buttons(KZ_RIGHT); same_physical(); CHECK(o.event==KZ_EVENT_CANCELLED);
    case_done("stale context cancels in progress");
    reset(); p.now_ms=180; trigger(); case_done("80 millisecond lease boundary is inclusive");
    reset(); p.now_ms=99; poll_buttons(KZ_RIGHT); same_physical(); case_done("backwards monotonic timestamp rejected");
    reset(); c.permitted=0; next(); poll_buttons(KZ_RIGHT); same_physical(); case_done("revoked context prevents start");
    reset(); trigger(); c.permitted=0; kz_observe(&s,&c); poll_buttons(KZ_RIGHT); same_physical();
    case_done("permission revoked even on duplicate epoch");
    reset(); trigger(); c.can_continue=0; next(); poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_CANCELLED && !o.owns_input);
    case_done("cannot-continue cancels existing command");
    reset(); c.actionable=0; next(); poll_buttons(KZ_RIGHT); same_physical(); case_done("busy fighter cannot start");
    reset(); c.facing=0; next(); poll_buttons(KZ_RIGHT); same_physical(); case_done("unknown facing cannot start");
    reset(); poll_buttons(KZ_RIGHT|KZ_A); same_physical(); case_done("manual A plus trigger never starts");
    reset(); trigger(); poll_buttons(KZ_A); CHECK(o.buttons==KZ_A && !o.owns_input && o.event==KZ_EVENT_CANCELLED);
    CHECK(o.lx==p.lx); case_done("manual attack immediately wins and cancels");
    reset(); trigger(); p.enabled=0; poll_buttons(KZ_RIGHT); CHECK(o.event==KZ_EVENT_CANCELLED); same_physical();
    case_done("disabling active command restores physical input");
    reset(); trigger(); for(int i=0;i<5;i++){ next(); poll_buttons(KZ_RIGHT); }
    for(int i=0;i<30;i++){ next(); poll_buttons(KZ_RIGHT); CHECK(!o.owns_input && o.event==0); }
    case_done("held D-pad never retriggers");
    poll_buttons(0); CHECK(!o.consume_right); trigger(); case_done("release and fresh press rearms");
    kz_init(&s); c=(KzContext){1,100,0x1234,1,1,1,1}; kz_observe(&s,&c);
    p=(KzPoll){KZ_RIGHT,0,0,0,0,1,1,1,1,100}; kz_poll(&s,&p,&o); CHECK(!o.owns_input);
    case_done("held D-pad on entry requires neutral first");
    reset(); trigger(); poll_buttons(0); CHECK(o.owns_input && o.phase==KZ_FORWARD);
    case_done("brief D-pad tap completes current command");
    reset(); trigger(); next(); poll_buttons(KZ_RIGHT); next(); poll_buttons(KZ_RIGHT); next(); poll_buttons(KZ_RIGHT);
    CHECK(o.buttons==KZ_A); next(); poll_buttons(KZ_RIGHT); CHECK(o.buttons==0);
    case_done("A pulse is exactly one observed epoch");
    reset(); trigger(); for(int i=0;i<5;i++){next();poll_buttons(KZ_RIGHT);} poll_buttons(0); same_physical();
    case_done("physical sticks restored after release");
    reset(); trigger(); CHECK(o.rx==0 && o.ry==0); case_done("C-stick not merged into command");
    reset(); kz_poll(NULL,&p,&o); kz_poll(&s,NULL,&o); kz_poll(&s,&p,NULL); kz_observe(NULL,&c); kz_observe(&s,NULL);
    case_done("null API arguments are ignored safely");
    for(unsigned bit=0;bit<16;bit++) {
        if(bit==14) continue;
        reset(); trigger(); poll_buttons(UINT64_C(1)<<bit);
        CHECK(!o.owns_input && o.buttons==p.buttons);
    }
    case_done("all 15 low-bit manual buttons cancel");
    reset();
    for(unsigned i=0;i<100000;i++) {
        if(rnd()%3==0) { c.epoch+=(rnd()%20==0?2:1); c.now_ms+=16; c.permitted=(rnd()%7!=0); c.actionable=(rnd()%5!=0); c.can_continue=(rnd()%9!=0); kz_observe(&s,&c); }
        p=(KzPoll){rnd()&0xFFFF,(int32_t)(rnd()%65535)-32767,(int32_t)(rnd()%65535)-32767,10,20,
            (rnd()%20!=0),(rnd()%8!=0),(rnd()%9!=0),(rnd()%3!=0),c.now_ms+(rnd()%100)};
        kz_poll(&s,&p,&o);
        int allowed=p.matching_pad && p.connected && p.training_now && s.context.permitted && p.now_ms>=s.context.now_ms && p.now_ms-s.context.now_ms<=80;
        if(!allowed || !p.enabled) same_physical();
        CHECK(o.phase>=0 && o.phase<=5); CHECK(o.lx>=-32767 && o.lx<=32767); CHECK(o.ly>=-32767 && o.ly<=32767);
        if(o.owns_input) { CHECK(allowed && p.enabled); CHECK(o.buttons==0 || o.buttons==KZ_A); }
    }
    case_done("100000 deterministic randomized invariant steps");
    printf("RESULT: %u cases passed. C engine only; no Switch integration or EWGF-hit proof.\n",tests);
    printf("ABI: Context=%zu Poll=%zu Output=%zu State=%zu\n",sizeof(KzContext),sizeof(KzPoll),sizeof(KzOutput),sizeof(KzState));
    return 0;
}
