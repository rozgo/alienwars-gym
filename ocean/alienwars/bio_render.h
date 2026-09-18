#ifndef ALIENWARS_BIO_RENDER_H
#define ALIENWARS_BIO_RENDER_H
/* Authored biological bodies use GPU meshes. Cosmetic articulation is driven by
 * distance travelled, not elapsed time, so stationary or paused units rest. */
static Shader aw_bio_shader;
static int aw_bio_locs[5],aw_bio_through;
static AwSVec aw_bio_previous[AW_UNITS];
static int aw_bio_pose_valid;
static float aw_bio_phase[AW_UNITS],aw_bio_speed[AW_UNITS];
static void aw_bio_draw(int asset,const AwVehicle*v,int unit,Vector3 view){
    if(!aw_bio_shader.id){
        const char*vs=AW_GLSL
            "in vec3 vertexPosition,vertexNormal;in vec4 vertexColor;in vec2 vertexTexCoord;uniform mat4 mvp,matModel,matNormal;"
            "uniform float phase,motion;uniform int specimen;out vec3 n;out vec4 c;out vec2 uv;"
            "void main(){vec3 p=vertexPosition;float w=vertexTexCoord.y;"
            "if(specimen==0){float step=sin(phase+sign(p.x)*1.57+floor((p.z+.4)*9.0)*3.14);p.z=clamp(p.z+step*.025*w*motion,-.595,.595);p.y+=max(0.0,step)*.023*w*motion;}"
            "if(specimen==1){p.y+=sin(phase+p.z*7.0)*.032*w*motion;}"
            "if(specimen==2){p.y+=sin(phase*.22+p.x*1.3)*.018*w*motion;}"
            "n=normalize((matNormal*vec4(vertexNormal,0.0)).xyz);c=vertexColor;uv=vertexTexCoord;gl_Position=mvp*vec4(p,1.0);}";
        const char*fs=AW_GLSL
            "in vec3 n;in vec4 c;in vec2 uv;uniform vec3 viewDirection;uniform int through;out vec4 finalColor;"
            "vec3 tone(vec3 x){return pow(clamp((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14),0.0,1.0),vec3(1.0/2.2));}"
            "void main(){vec3 normal=normalize(n),sun=normalize(vec3(-.55,.85,-.4)),v=normalize(viewDirection);"
            "float nl=max(0.0,dot(normal,sun)),rim=pow(1.0-max(dot(normal,v),0.0),3.0);"
            "vec3 albedo=pow(c.rgb,vec3(2.2));float spec=pow(max(dot(normal,normalize(sun+v)),0.0),uv.x>.5?22.0:58.0);"
            "vec3 lit=albedo*(mix(vec3(.36,.40,.43),vec3(.58,.61,.61),normal.y*.5+.5)+nl*vec3(.52,.49,.42));"
            "lit+=spec*vec3(.12,.14,.12)+albedo*rim*.08;if(uv.x>1.5)lit+=albedo*1.65;"
            "vec3 color=tone(lit*.77);if(through>0)color=mix(color,vec3(.40,.82,.88),.60);"
            "finalColor=vec4(color,through>0?.60:1.0);}";
        aw_bio_shader=LoadShaderFromMemory(vs,fs);
        if(!IsShaderValid(aw_bio_shader)){fprintf(stderr,"Biological shader compilation failed\n");exit(2);}
        const char*names[5]={"phase","motion","specimen","viewDirection","through"};
        for(int k=0;k<5;k++)aw_bio_locs[k]=GetShaderLocation(aw_bio_shader,names[k]);
    }
    float motion=fminf(1,aw_bio_speed[unit]*2);
    SetShaderValue(aw_bio_shader,aw_bio_locs[0],&aw_bio_phase[unit],SHADER_UNIFORM_FLOAT);
    SetShaderValue(aw_bio_shader,aw_bio_locs[1],&motion,SHADER_UNIFORM_FLOAT);
    SetShaderValue(aw_bio_shader,aw_bio_locs[2],&asset,SHADER_UNIFORM_INT);
    SetShaderValue(aw_bio_shader,aw_bio_locs[3],&view,SHADER_UNIFORM_VEC3);
    SetShaderValue(aw_bio_shader,aw_bio_locs[4],&aw_bio_through,SHADER_UNIFORM_INT);
    Matrix transform=MatrixMultiply(MatrixMultiply(MatrixRotateZ(asset==2?-v->yaw_rate*42*DEG2RAD:0),MatrixRotateX(-v->pitch)),MatrixRotateY(v->yaw));
    transform=MatrixMultiply(transform,MatrixTranslate(v->position.x,v->position.y,v->position.z));
    aw_art_material.shader=aw_bio_shader;rlDrawRenderBatchActive();DrawMesh(aw_art_mesh[asset],aw_art_material,transform);
}
#endif
