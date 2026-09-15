#ifndef ALIENWARS_NAV_API_H
#define ALIENWARS_NAV_API_H
#include <stdint.h>
#define AW_NAV_INPUTS 621
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AwNav AwNav;
typedef struct {
    uint32_t map_seed,episode_seed;
    int maps,kind,limit,controller;
} AwNavConfig;
typedef struct {
    float reward,total_reward,success,fall,timeout,contacts,steps,kind,invalid_actions;
    uint32_t map_seed,map_hash,start,goal;
} AwNavResult;
AwNav *aw_nav_create(AwNavConfig config,uint32_t instance);
void aw_nav_delete(AwNav *nav);
void aw_nav_restart(AwNav *nav,float *observations);
AwNavResult aw_nav_tick(AwNav *nav,const float *actions,float *observations);
void aw_nav_render(AwNav *nav);
void aw_nav_render_close(void);
void aw_nav_controller(AwNav *nav,int mode);
void aw_nav_prediction(AwNav *nav,float value,const float *logits,const char *label);
int aw_nav_view_paused(void);
int aw_nav_view_reset(void);
#ifdef __cplusplus
}
#endif
#endif
