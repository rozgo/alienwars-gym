#ifndef ALIENWARS_ART_H
#define ALIENWARS_ART_H
/* Viewer-only authored meshes. No assets or animation enter the RL world.
 * AWM1: little-endian vertex count, then position/normal/material/weight/color.
 * Meshes are loaded once, reused across world resets and freed before GL exits. */
enum {AW_ART_SCOUT,AW_ART_SKIFF,AW_ART_WING,AW_ART_NURSERY,AW_ART_ROVER,
    AW_ART_HAULER,AW_ART_PATROL_BOAT,AW_ART_CUTTER,AW_ART_QUAD,AW_ART_TRANSPORT,
    AW_ART_RECON_SUB,AW_ART_PATROL_SUB,AW_ART_HEAVY_SUB,AW_ART_COUNT};
static Mesh aw_art_mesh[AW_ART_COUNT];
static Material aw_art_material;
static int aw_art_ready;
static void aw_art_init(void){
    if(aw_art_ready)return;
    const char*names[AW_ART_COUNT]={"scout","skiff","wing","nursery","rover","hauler",
        "patrol_boat","cutter","quad","transport","recon_sub","patrol_sub","heavy_sub"};
    for(int k=0;k<AW_ART_COUNT;k++){
        int size=0;unsigned char*data=LoadFileData(TextFormat("resources/alienwars/art/%s.awm",names[k]),&size);uint32_t count=0;
        if(data&&size>=8)memcpy(&count,data+4,4);
        if(!data||size<8||memcmp(data,"AWM1",4)||count<3||count%3||count>1000000||(size_t)size!=8+(size_t)count*36){fprintf(stderr,"Invalid art asset: %s\n",names[k]);exit(2);}
        Mesh*m=&aw_art_mesh[k];m->vertexCount=count;m->triangleCount=count/3;
        m->vertices=MemAlloc(count*3*sizeof(float));m->normals=MemAlloc(count*3*sizeof(float));m->texcoords=MemAlloc(count*2*sizeof(float));m->colors=MemAlloc(count*4);
        if(!m->vertices||!m->normals||!m->texcoords||!m->colors){fprintf(stderr,"Art allocation failed\n");exit(2);}
        for(uint32_t i=0;i<count;i++){
            const unsigned char*p=data+8+i*36;
            memcpy(m->vertices+i*3,p,12);memcpy(m->normals+i*3,p+12,12);memcpy(m->texcoords+i*2,p+24,8);memcpy(m->colors+i*4,p+32,4);
        }
        UnloadFileData(data);UploadMesh(m,false);
    }
    aw_art_material=LoadMaterialDefault();aw_art_ready=1;
}
static void aw_art_close(void){
    if(!aw_art_ready)return;for(int i=0;i<AW_ART_COUNT;i++)UnloadMesh(aw_art_mesh[i]);
    MemFree(aw_art_material.maps);aw_art_ready=0;
}
static void aw_art_nursery(AwBuilder*b,Vector3 p,int side){
    aw_art_init();const Mesh*m=&aw_art_mesh[AW_ART_NURSERY];aw_reserve(b,m->vertexCount);
    for(int i=0;i<m->vertexCount;i++){
        int n=b->count++;Vector3 v={m->vertices[i*3],m->vertices[i*3+1],m->vertices[i*3+2]};
        b->positions[n]=Vector3Add(p,v);b->normals[n]=(Vector3){m->normals[i*3],m->normals[i*3+1],m->normals[i*3+2]};
        Color c={m->colors[i*4],m->colors[i*4+1],m->colors[i*4+2],255};
        int emissive=m->texcoords[i*2]>1.5f;
        if(emissive)c=side?(Color){223,158,85,255}:(Color){88,211,186,255};
        b->colors[n]=c;b->uv[n]=(Vector2){AW_CELLS-1,emissive?16:15};
    }
}
#endif
