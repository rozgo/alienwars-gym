#ifndef ALIENWARS_UNIT_LIGHTING_H
#define ALIENWARS_UNIT_LIGHTING_H
/* Primitives carry real normals. Gentle directional fill separates hull faces
 * without borrowing the terrain's texture/cutaway shader or shadow buffers. */
static Shader aw_unit_lighting;
static void aw_units_lit_begin(void){
    if(!aw_unit_lighting.id){
#ifdef PLATFORM_WEB
#define AW_UNIT_GLSL "#version 300 es\nprecision highp float;\n"
#else
#define AW_UNIT_GLSL "#version 330\n"
#endif
        const char*vs=AW_UNIT_GLSL
            "in vec3 vertexPosition;in vec3 vertexNormal;in vec4 vertexColor;in vec2 vertexTexCoord;uniform mat4 mvp;out vec3 n;out vec4 c;out vec2 uv;"
            "void main(){n=vertexNormal;c=vertexColor;uv=vertexTexCoord;gl_Position=mvp*vec4(vertexPosition,1.0);}";
        const char*fs=AW_UNIT_GLSL
            "in vec3 n;in vec4 c;in vec2 uv;uniform sampler2D texture0;out vec4 finalColor;"
            "void main(){vec3 normal=normalize(n);float light=.64+.36*max(dot(normal,normalize(vec3(-.55,.85,-.4))),0.0);"
            "vec4 albedo=c*texture(texture0,uv);finalColor=vec4(albedo.rgb*light,albedo.a);}";
        aw_unit_lighting=LoadShaderFromMemory(vs,fs);
#undef AW_UNIT_GLSL
    }BeginShaderMode(aw_unit_lighting);
}
static void aw_units_lit_end(void){EndShaderMode();}
#endif
