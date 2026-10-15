/* Synthetic scheduler regressions. They do NOT emulate Smash's command parser. */
#include "ewgf_core.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)
static KzState s;
static KzContext c;
static KzPoll p;
static KzOutput o;
static unsigned tests;
static void done(const char *name) { printf("TIMING PASS %02u %s\n",++tests,name); }
static void poll(void) { kz_poll(&s,&p,&o); }
static void observe(unsigned count, unsigned ms) {
    c.now_ms+=ms; p.now_ms=c.now_ms;
    for(unsigned j=0;j<count;j++) { c.epoch++; kz_observe(&s,&c); }
}
static void start(uint64_t epoch, uint64_t ms, int facing) {
    kz_init(&s); c=(KzContext){epoch,ms,0x1234,1,1,1,facing}; kz_observe(&s,&c);
    p=(KzPoll){0,42,24,7,8,1,1,1,1,ms}; poll();
    p.buttons=KZ_RIGHT; poll(); CHECK(o.phase==KZ_FORWARD && o.event==KZ_EVENT_STARTED);
}
static void phase(int expected) {
    CHECK(o.phase==expected && o.owns_input && o.consume_right);
    CHECK(o.buttons==(expected==KZ_DIAGONAL_ATTACK?KZ_A:0));
    CHECK(o.lx==(expected==KZ_FORWARD?32767*c.facing:expected==KZ_DIAGONAL_ATTACK?23170*c.facing:0));
    CHECK(o.ly==(expected==KZ_DOWN?-32767:expected==KZ_DIAGONAL_ATTACK?-23170:0));
}
int main(void) {
    const unsigned epochs[]={85,129,176,426,493,595,670,795,983,1034,1080,1125,1166,1452,1522,2266,2342};
    const unsigned delta_ms[]={17,17,17,12,17,16,17,17,12,17,17,17,17,12,16,12,16};
    for(unsigned i=0;i<sizeof(epochs)/sizeof(*epochs);i++) {
        for(int facing=-1;facing<=1;facing+=2) {
            start(epochs[i],1000,facing); phase(KZ_FORWARD);
            c.actionable=0; observe(2,delta_ms[i]); poll(); phase(KZ_NEUTRAL); CHECK(o.event==0);
            for(int k=KZ_DOWN;k<=KZ_RELEASE;k++) { observe(2,17); poll(); phase(k); CHECK(o.event==0); }
            observe(2,17); poll(); CHECK(o.event==KZ_EVENT_INPUT_COMPLETE && !o.owns_input);
        }
    }
    done("17 reported epoch-gap/delay pairs, both facings: all five outputs delivered");
    start(1,100,1); observe(20,1); CHECK(s.phase==KZ_FORWARD && s.delivered);
    poll(); phase(KZ_NEUTRAL); done("20 callbacks without an input poll do not skip outputs");
    for(unsigned n=1;n<=16;n++) {
        start(1,100,1);
        for(int k=2;k<=5;k++) { observe(n,16); poll(); phase(k); }
        observe(n,16); poll(); CHECK(o.event==KZ_EVENT_INPUT_COMPLETE);
    }
    done("batches of 1 to 16 callbacks preserve phase order");
    start(1,100,1);
    for(int k=1;k<=5;k++) {
        if(k>1) { observe(2,16); poll(); }
        phase(k);
        for(int repeat=0;repeat<100;repeat++) { poll(); phase(k); CHECK(o.event==0); }
    }
    done("100 same-epoch polls per phase do not consume later phases");
    start(1,100,1); observe(2,81); poll();
    CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input);
    done("fresh fighter context does not hide an 81ms input-output stall");
    start(1,100,1); observe(2,80); poll(); phase(KZ_NEUTRAL);
    done("80ms phase-watchdog boundary is inclusive");
    start(1,100,1);
    for(int i=1;i<=8;i++) { c.now_ms+=10; p.now_ms=c.now_ms; kz_observe(&s,&c); poll(); phase(KZ_FORWARD); }
    c.now_ms++; p.now_ms=c.now_ms; kz_observe(&s,&c); poll();
    CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input);
    done("same-epoch observations and repeated polls cannot renew the deadline");
    start(1,100,1); observe(2,17); p.buttons=KZ_A; poll();
    CHECK(o.event==KZ_EVENT_CANCELLED && !o.owns_input && o.buttons==KZ_A && o.lx==42);
    done("manual input still cancels after duplicate observations");
    start(1,100,1); c.permitted=0; kz_observe(&s,&c); c.permitted=1; kz_observe(&s,&c); poll();
    CHECK(o.event==KZ_EVENT_CANCELLED && !o.owns_input && o.buttons==KZ_RIGHT);
    done("permission revocation cannot be undone in the same epoch");
    start(1,100,1); c.now_ms=99; c.epoch++; kz_observe(&s,&c); p.now_ms=100; poll();
    CHECK(o.event==KZ_EVENT_BAD_CLOCK && !o.owns_input);
    done("backward callback timestamp cancels instead of progressing");
    start(1,100,1); observe(2,16); poll(); phase(KZ_NEUTRAL);
    p.enabled=0; observe(2,16); poll(); CHECK(!o.owns_input && !o.consume_right && o.buttons==KZ_RIGHT);
    done("disabled configuration never returns automated inputs");
    start(1,100,1); observe(2,16); poll(); phase(KZ_NEUTRAL);
    p.training_now=0; observe(2,16); poll(); CHECK(!o.owns_input && !o.consume_right && o.buttons==KZ_RIGHT);
    done("leaving training restores the original input immediately");
    printf("TIMING RESULT: %u cases passed; command parsing, game frame consumption, and EWGF timing remain unverified.\n",tests);
    return 0;
}
