#ifndef AW_LOCAL_API_H
#define AW_LOCAL_API_H
#ifdef __cplusplus
extern "C" {
#endif
#define AW_LOCAL_OBS 96
typedef struct AwLocalTask AwLocalTask;
AwLocalTask*aw_local_create(int family,int maps,unsigned seed,unsigned instance,int difficulty);
void aw_local_destroy(AwLocalTask*t);
void aw_local_restart_task(AwLocalTask*t,float*obs);
void aw_local_tick_task(AwLocalTask*t,const float*actions,float*obs,float*reward,int*terminal,int*success,int*contacts);
void aw_local_baseline_task(AwLocalTask*t,float*actions,int mode);
#ifdef __cplusplus
}
#endif
#endif
