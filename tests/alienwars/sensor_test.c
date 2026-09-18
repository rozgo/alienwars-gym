#include <assert.h>
#include <time.h>
#include <stdlib.h>
static int forbid_alloc;
static void* checked_malloc(size_t n){assert(!forbid_alloc);return malloc(n);}
static void* checked_calloc(size_t n,size_t size){assert(!forbid_alloc);return calloc(n,size);}
static void* checked_realloc(void*p,size_t n){assert(!forbid_alloc);return realloc(p,n);}
#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#include "ocean/alienwars/sensors.h"
#include "ocean/alienwars/volume.h"
#include "ocean/alienwars/patrols.h"
static AwMap m,before;
static AwSensors sensors,copy;
static AwSensorUnit sensor_units[AW_SENSOR_UNITS],copy_units[AW_SENSOR_UNITS];
static AwRayWorld rays;
typedef struct {AwSVec a,b,c;} Triangle;
static Triangle triangles[180000];static int triangle_count;
static AwSVec world_point(AwVolumePoint v){return (AwSVec){v.x*2,v.q*.75f-1.2f,v.z*2};}
static void triangle(void*ctx,AwVolumePoint a,AwVolumePoint b,AwVolumePoint c){
    (void)ctx;assert(triangle_count<180000);triangles[triangle_count++]=(Triangle){world_point(a),world_point(b),world_point(c)};
}
static AwSVec cross(AwSVec a,AwSVec b){return (AwSVec){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static float mesh_hit(AwSVec o,AwSVec d,float range){
    for(int i=0;i<triangle_count;i++){
        Triangle*t=&triangles[i];AwSVec e1=aw_sv_add(t->b,aw_sv_scale(t->a,-1)),e2=aw_sv_add(t->c,aw_sv_scale(t->a,-1)),p=cross(d,e2);
        float det=aw_sv_dot(e1,p);if(fabsf(det)<1e-8f)continue;
        AwSVec s=aw_sv_add(o,aw_sv_scale(t->a,-1));float u=aw_sv_dot(s,p)/det;if(u<-.00001f||u>1.00001f)continue;
        AwSVec q=cross(s,e1);float v=aw_sv_dot(d,q)/det;if(v<-.00001f||u+v>1.00001f)continue;
        float distance=aw_sv_dot(e2,q)/det;if(distance>=0&&distance<range)range=distance;
    }return range;
}
static void flat(int q){memset(&m,0,sizeof(m));memset(m.blend,255,sizeof(m.blend));for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],q);}
static void passage(int q){int prev=-1;for(int x=8;x<=16;x++){int n=aw_cave_node(&m,x,12,q);assert(n>=0);m.cave[n].profile=1;if(prev>=0)assert(aw_cave_link(&m,prev,n));prev=n;}}
static int mesh_checks;
static void compare_mesh(AwSVec origin,int count){
    for(int i=0;i<count;i++){
        AwSVec d=i==0?(AwSVec){0,1,0}:i==1?(AwSVec){0,-1,0}:i==2?(AwSVec){1,0,0}:i==3?(AwSVec){-1,0,0}:
            aw_sv_normal((AwSVec){(int)(aw_hash(i*3)%2001)-1000,(int)(aw_hash(i*3+1)%2001)-1000,(int)(aw_hash(i*3+2)%2001)-1000});
        float expected=mesh_hit(origin,d,7);AwSensorHit hit=aw_ray_terrain(&m,&rays,origin,d,7,0);
        if(fabsf(expected-hit.distance)>.002f){fprintf(stderr,"ray %d origin %.3f %.3f %.3f dir %.5f %.5f %.5f mesh %.6f field %.6f kind %d\n",i,origin.x,origin.y,origin.z,d.x,d.y,d.z,expected,hit.distance,hit.kind);abort();}
        mesh_checks++;
    }
}
static void build_reference(void){triangle_count=0;for(int z=7;z<=18;z++)for(int x=7;x<=18;x++)aw_volume_cell(&m,z*64+x,triangle,NULL);aw_ray_world_init(&rays,&m);}
static void ray_tests(void){
    flat(4);aw_bathymetry_build(&m);aw_ray_world_init(&rays,&m);
    AwSensorHit hit=aw_ray_terrain(&m,&rays,(AwSVec){25,9,25},(AwSVec){0,-1,0},20,0);
    assert(hit.kind==AW_HIT_TERRAIN&&fabsf(hit.distance-7.2f)<.001f);
    assert(aw_ray_terrain(&m,&rays,(AwSVec){25,9,25},(AwSVec){1,0,0},10,0).kind==AW_HIT_NONE);
    assert(aw_ray_terrain(&m,&rays,(AwSVec){159,9,25},(AwSVec){1,0,0},10,0).kind==AW_HIT_BOUNDARY);
    /* Sea-surface returns for optical sensors; sonar sees the deep shelf. */
    hit=aw_ray_terrain(&m,&rays,(AwSVec){-20,2,25},(AwSVec){0,-1,0},20,1);
    assert(hit.kind==AW_HIT_WATER&&fabsf(hit.distance-2.12f)<.001f);
    hit=aw_ray_terrain(&m,&rays,(AwSVec){-20,-.6f,25},(AwSVec){0,-1,0},20,1);
    assert(hit.kind==AW_HIT_TERRAIN&&fabsf(hit.distance-9.6f)<.001f);
    hit=aw_ray_terrain(&m,&rays,(AwSVec){-4,-.6f,25},(AwSVec){0,-1,0},20,1);
    /* At two tiles from this straight coast, smoothstep(1/8) * 12
     * quantizes to .52 q of depression: world y=-1.59, range=.99. */
    assert(hit.kind==AW_HIT_TERRAIN&&fabsf(hit.distance-(AW_VERSION>=13?.99f:3.6f))<.001f);
#if AW_VERSION >= 13
    /* A curved shelf around one coastal anchor, with the mandatory submerged
     * domain border. The whole seven-unit ray sphere stays outside the land. */
    flat(0);for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++)
        if(aw_corner_vertex(c,k)==12*65+3)m.cells[c].q[k]=4;
    aw_bathymetry_build(&m);aw_ray_world_init(&rays,&m);
    triangle_count=0;
    for(int z=7;z<=18;z++)for(int x=-10;x<0;x++){
        AwVolumePoint p[4]={{x,aw_ocean_bed_q(&m,x,z),z},{x+1,aw_ocean_bed_q(&m,x+1,z),z},
            {x+1,aw_ocean_bed_q(&m,x+1,z+1),z+1},{x,aw_ocean_bed_q(&m,x,z+1),z+1}};
        triangle(NULL,p[0],p[1],p[2]);triangle(NULL,p[0],p[2],p[3]);
    }
    compare_mesh((AwSVec){-10,-3,25},256);
#endif
    flat(24);passage(-6);passage(6);assert(aw_cave_index(&m));build_reference();
    compare_mesh((AwSVec){25,-4.95f,25},96);compare_mesh((AwSVec){25,4.05f,25},96);
    /* Surface breach must let an upward beam escape to max range. */
    for(int c=0;c<AW_CELLS;c++)aw_flat(&m.cells[c],8);build_reference();compare_mesh((AwSVec){25,4.05f,25},96);
    assert(aw_ray_terrain(&m,&rays,(AwSVec){25,4.05f,25},(AwSVec){0,1,0},7,0).kind==AW_HIT_NONE);
    flat(0);m.bridge_count=1;m.bridges[0]=(AwBridge){8,12,1,0,10,20,20,22};assert(aw_bridge_index(&m));build_reference();
    compare_mesh((AwSVec){25,13,25},96);compare_mesh((AwSVec){25,17,25},96);
    hit=aw_ray_terrain(&m,&rays,(AwSVec){25,13,25},(AwSVec){0,1,0},7,0);assert(hit.kind==AW_HIT_TERRAIN&&hit.distance<3);
    /* Oblique graded surfaces and exact lattice boundaries. */
    flat(0);for(int c=0;c<AW_CELLS;c++)for(int k=0;k<4;k++){int v=aw_corner_vertex(c,k);m.cells[c].q[k]=aw_clamp(v%65+v/65-14,0,40);}
    build_reference();compare_mesh((AwSVec){25,11,25},128);compare_mesh((AwSVec){24,11,24},96);
}
static void contract_tests(void){
    flat(4);aw_sensors_init(&sensors,&m,3,sensor_units);
    for(int i=0;i<3;i++){aw_sensor_equip(&sensors,i,0,.7f);sensors.units[i].pose.position=(AwSVec){20+i*5,1.8f,25};}
    AwSensorConfig sonar=aw_sensor_default(AW_SENSOR_SONAR,0);sonar.enabled=1;assert(aw_sensor_attach(&sensors,0,AW_SENSOR_SONAR,sonar));
    before=m;forbid_alloc=1;aw_sensors_step(&sensors,&m,1.0f/60);forbid_alloc=0;
    assert(sensors.units[0].reading[AW_SENSOR_LIDAR].valid&&sensors.units[0].reading[AW_SENSOR_CAMERA].valid);
    assert(!sensors.units[0].reading[AW_SENSOR_SONAR].valid&&sensors.units[0].reading[AW_SENSOR_RF].peers[1].detected);
    AwSensorHit hit=aw_sensor_cast(&sensors,&m,0,(AwSVec){20,2.3f,25},(AwSVec){1,0,0},24,0);
    assert(hit.kind==AW_HIT_UNIT&&hit.entity==1&&fabsf(hit.distance-4.3f)<.001f);
    AwSensorReading saved=sensors.units[0].reading[AW_SENSOR_LIDAR];
    sensors.units[0].pose.position.x+=.1f;aw_sensors_step(&sensors,&m,.01f);
    assert(!memcmp(&saved,&sensors.units[0].reading[AW_SENSOR_LIDAR],sizeof(saved)));
    assert(fabsf(sensors.units[0].odometry.delta.x-.1f)<.0001f&&fabsf(sensors.units[0].odometry.velocity.x-10)<.001f);
    sensors.units[0].previous.yaw=AW_SENSOR_PI-.01f;sensors.units[0].pose.yaw=-AW_SENSOR_PI+.01f;
    aw_sensors_step(&sensors,&m,.01f);assert(fabsf(sensors.units[0].odometry.angle_delta.y-.02f)<.0001f);
    sensors.units[0].pose.position.x=70;aw_sensor_teleport(&sensors,0);aw_sensors_step(&sensors,&m,.01f);assert(sensors.units[0].odometry.distance==0&&aw_sv_length(sensors.units[0].odometry.velocity)==0);
    AwSensorConfig disabled=sensors.units[0].config[0];disabled.enabled=0;assert(aw_sensor_attach(&sensors,0,0,disabled));aw_sensors_step(&sensors,&m,.01f);
    for(int i=13;i<13+AW_SENSOR_SLOT_OBS;i++)assert(sensors.observations[0][i]==0);
    disabled.range=NAN;assert(!aw_sensor_attach(&sensors,0,0,disabled));assert(!aw_sensor_attach(&sensors,17,0,sonar));
    sensors.units[2].pose.position=(AwSVec){-20,-.12f,25};sensors.units[2].layer=1;
    sonar=aw_sensor_default(AW_SENSOR_SONAR,1);assert(aw_sensor_attach(&sensors,2,AW_SENSOR_SONAR,sonar));
    aw_sensors_step(&sensors,&m,.01f);assert(sensors.units[2].reading[AW_SENSOR_SONAR].valid);
    int seabed=0;for(int i=0;i<24;i++)seabed+=sensors.units[2].reading[AW_SENSOR_SONAR].beams[i].hit.kind==AW_HIT_TERRAIN;assert(seabed>10);
    copy=sensors;memcpy(copy_units,sensor_units,sizeof(sensor_units));copy.units=copy_units;forbid_alloc=1;
    for(int step=0;step<180;step++){aw_sensors_step(&sensors,&m,1.0f/60);aw_sensors_step(&copy,&m,1.0f/60);}
    forbid_alloc=0;assert(!memcmp(sensor_units,copy_units,sizeof(sensor_units)));
    copy.units=sensors.units;assert(!memcmp(&sensors,&copy,sizeof(sensors))&&!memcmp(&m,&before,sizeof(m)));
    for(int i=0;i<3;i++)for(int k=0;k<AW_SENSOR_OBS;k++)assert(isfinite(sensors.observations[i][k])&&fabsf(sensors.observations[i][k])<=1.00001f);
    /* Nonzero mount translations and rotations compose in the body's frame. */
    AwSensorConfig mounted=aw_sensor_default(AW_SENSOR_LIDAR,0);mounted.mount.position=(AwSVec){0,1,2};mounted.mount.yaw=AW_SENSOR_PI*.5f;
    sensors.units[0].pose=(AwSensorPose){.position={20,1.8f,25},.yaw=AW_SENSOR_PI*.5f};
    assert(aw_sensor_attach(&sensors,0,AW_SENSOR_LIDAR,mounted));aw_sensor_sample(&sensors,&m,0,AW_SENSOR_LIDAR);
    AwSensorReading*scan=&sensors.units[0].reading[0];assert(fabsf(scan->pose.position.x-22)<.001f&&fabsf(scan->pose.position.y-2.8f)<.001f);
    assert(fabsf(scan->beams[16].direction.x)<.001f&&fabsf(scan->beams[16].direction.z+1)<.001f);
    copy=sensors;memcpy(copy_units,sensor_units,sizeof(sensor_units));aw_sensors_step(&sensors,&m,NAN);aw_sensors_step(&sensors,&m,0);
    assert(!memcmp(&copy,&sensors,sizeof(sensors))&&!memcmp(copy_units,sensor_units,sizeof(sensor_units)));
    sensors.units[2].active=0;aw_sensors_step(&sensors,&m,.01f);for(int k=0;k<AW_SENSOR_OBS;k++)assert(sensors.observations[2][k]==0);
    /* Occluding terrain attenuates RF; disabled peers do not transmit. */
    sensors.units[0].pose.position=(AwSVec){20,1.8f,25};sensors.units[1].pose.position=(AwSVec){30,1.8f,25};
    sensors.units[0].pose.yaw=0;aw_sensor_sample(&sensors,&m,0,AW_SENSOR_RF);float open=sensors.units[0].reading[AW_SENSOR_RF].peers[1].strength;assert(open>0);
    for(int z=0;z<64;z++)for(int x=12;x<14;x++)aw_flat(&m.cells[z*64+x],20);aw_ray_world_init(&sensors.rays,&m);
    aw_sensor_sample(&sensors,&m,0,AW_SENSOR_RF);assert(sensors.units[0].reading[AW_SENSOR_RF].peers[1].strength<open);
    sensors.units[1].config[AW_SENSOR_RF].enabled=0;aw_sensor_sample(&sensors,&m,0,AW_SENSOR_RF);assert(!sensors.units[0].reading[AW_SENSOR_RF].peers[1].detected);
}
static AwSensorPose frames[600][AW_UNITS];
static void benchmark(void){
    static AwPatrols patrols;double total=0;uint64_t queries=0,samples=0;int finite=0;
    for(int seed=0;seed<3;seed++){
        assert(aw_generate(&m,seed+71)&&aw_patrol_build(&m,&patrols));before=m;aw_sensors_init(&sensors,&m,AW_UNITS,sensor_units);aw_sensor_equip(&sensors,0,0,.65f);
        for(int i=0;i<AW_PATROLS;i++)aw_sensor_equip(&sensors,i+1,patrols.units[i].layer,1);
        for(int f=0;f<600;f++){
            int c=m.cave_hubs[0];AwCaveNode*n=&m.cave[c];frames[f][0]=(AwSensorPose){.position={(n->x+.5f)*2,(n->q+1)*.75f-1.2f,(n->z+.5f)*2},.yaw=f*.01f};
            for(int i=0;i<AW_PATROLS;i++){AwPatrol*p=&patrols.units[i];AwPatrolPoint a=aw_patrol_position(&m,p,p->progress+f*p->speed/60),b=aw_patrol_position(&m,p,p->progress+f*p->speed/60+.05f);
                frames[f][i+1]=(AwSensorPose){.position={a.x*2,a.q*.75f-1.2f,a.z*2},.yaw=atan2f(b.x-a.x,b.z-a.z)};
            }
        }
        double start=(double)clock()/CLOCKS_PER_SEC;forbid_alloc=1;
        for(int f=0;f<600;f++){
            for(int i=0;i<AW_UNITS;i++)sensors.units[i].pose=frames[f][i];aw_sensors_step(&sensors,&m,1.0f/60);
            for(int i=0;i<AW_UNITS;i++)for(int k=0;k<AW_SENSOR_OBS;k++)assert(isfinite(sensors.observations[i][k]));
        }forbid_alloc=0;total+=(double)clock()/CLOCKS_PER_SEC-start;queries+=sensors.ray_queries;samples+=sensors.samples;finite++;
        assert(!memcmp(&m,&before,sizeof(m)));
    }
    printf("SENSOR_BENCH worlds=%d units=12 steps=1800 simulation_seconds=30 cpu_ms=%.3f ms_per_world_step=%.4f rays=%llu samples=%llu state_bytes=%zu obs_floats=%d no_step_alloc=PASS map_unchanged=PASS\n",finite,total*1000,total*1000/1800,(unsigned long long)queries,(unsigned long long)samples,sizeof(AwSensors)+sizeof(sensor_units),AW_SENSOR_OBS);
}
int main(int argc,char**argv){
    if(argc>1&&!strcmp(argv[1],"--bench")){benchmark();return 0;}
    ray_tests();contract_tests();
    printf("SENSOR_TEST version=1 mesh_rays=%d caves/roofs/bridges/slopes=PASS sea/sonar=PASS unit_hits=PASS mounts=PASS RF=PASS odometry/reset=PASS cadence/cache=PASS finite_fixed_obs=%d no_step_alloc=PASS deterministic=PASS map_unchanged=PASS\n",mesh_checks,AW_SENSOR_OBS);
}
