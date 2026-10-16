#ifndef KZ_COMBO_LAB_H
#define KZ_COMBO_LAB_H
#include "ewgf_core.h"
/* ABI of read-only native snapshots. Flags describe observations, not permission
   to alter Smash's state. No opponent input/damage/status writes are performed. */
enum {
 C_SCOPE=1u<<0,C_GROUND=1u<<1,C_STOP=1u<<2,C_READY=1u<<3,
 C_ELECTRIC=1u<<4,C_NAIR=1u<<5,C_HURT=1u<<6,C_DEAD=1u<<7,
 C_JUMP=1u<<8,C_AIR_READY=1u<<9,C_HIT=1u<<10,C_SHIELD_HIT=1u<<11,
 C_TARGET=1u<<12,C_TGROUND=1u<<13,C_THURT=1u<<14,C_TSTOP=1u<<15,
 C_TDEAD=1u<<16,C_TTECH=1u<<17,C_TROLL=1u<<18,C_TDOWN=1u<<19,
 C_FLOOR=1u<<20,C_TFLOOR=1u<<21,C_PATH=1u<<22,C_ATTRIBUTED=1u<<23,
 C_DOUBLE_JUMP=1u<<24
};
typedef struct {
 uint64_t epoch,now_ms,actor,target;
 uint32_t flags; int32_t target_kind,self_status,target_status;
 float x,y,vx,vy,facing,motion_frame,percent;
 float tx,ty,tvx,tvy,target_percent,weight,stick_x,stick_y;
 float target_frame,target_end,floor_y,target_floor_y,path_floor_y;
} KzComboFrame;
enum { R_AUTO=0,R_NAIR=1,R_USMASH=2,R_UPB=3,R_FERPS=4,R_DRAGON=5 };
typedef struct {
 int32_t enabled,observe_only,max_links,ferps,route,jump_bit;
 float finisher_percent,nair_x,nair_y,ferps_below,tech_lead,drift_gain;
} KzComboConfig;
/* Custom rows are calibration candidates, never claims of a true combo.
   -1 fighter/DI/platform means any. DI: 0 neutral,1 in,2 out.
   platform: 0 flat,1 elevated. Rows are evaluated in file order. */
typedef struct {
 int32_t fighter,di,platform,route;
 float min_percent,max_percent;
} KzComboRule;
enum {
 CE_ARMED=400,CE_HIT=401,CE_ROUTE=402,CE_DI=403,CE_PHASE=404,
 CE_NAIR_HIT=405,CE_CHAIN_ELECTRIC=406,CE_PLATFORM=407,CE_TECH=408,
 CE_CANCEL=409,CE_TIMEOUT=410,CE_ESCAPE=411,CE_NO_TARGET=412,
 CE_TARGET_KO=413,CE_WRONG=414,CE_AIRBORNE=415,CE_FINISH=416,
 CE_GAP=417,CE_RULE=418,CE_JUMP_MISSING=419,CE_LAUNCH=420
};
#define KZ_COMBO_STORAGE_BYTES 16384u
void kzc_init(void *storage,const KzComboConfig *config);
void kzc_observe(void *storage,const KzComboFrame *frame);
void kzc_arm(void *storage,uint64_t now_ms);
void kzc_cancel(void *storage,int32_t reason);
int32_t kzc_active(const void *storage);
int32_t kzc_poll(void *storage,const KzPoll *input,KzOutput *out);
int32_t kzc_pop_event(void *storage,int32_t *route,int32_t *di,int32_t *links,int32_t *phase);
int32_t kzc_add_rule(void *storage,const KzComboRule *rule);
uint32_t kzc_frame_size(void);
uint32_t kzc_config_size(void);
uint32_t kzc_storage_used(void);
#endif
