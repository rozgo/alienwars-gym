#ifndef AW_SHARED_API_H
#define AW_SHARED_API_H
#ifdef __cplusplus
extern "C" {
#endif
#define AW_SHARED_AGENTS 12
#define AW_SHARED_OBS 645
typedef struct AwSharedTask AwSharedTask;
AwSharedTask*aw_shared_create(int maps,unsigned seed,unsigned instance,int curriculum);
void aw_shared_destroy(AwSharedTask*task);
int aw_shared_family(int unit);
void aw_shared_reset(AwSharedTask*task);
void aw_shared_action(AwSharedTask*task,int unit,const float*action);
void aw_shared_step(AwSharedTask*task);
void aw_shared_read(AwSharedTask*task,int unit,float*obs,float*reward,int*terminal,int*event,int*arrived,int*contacts,int*blocked);
void aw_shared_reference(AwSharedTask*task,int unit,float*actions,int random);
void aw_shared_mask(AwSharedTask*task,int unit,unsigned char*mask);
#ifdef __cplusplus
}
#endif
#endif
