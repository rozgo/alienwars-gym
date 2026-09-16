#ifndef ALIENWARS_UNIT_VISIBILITY_H
#define ALIENWARS_UNIT_VISIBILITY_H
/* Unit-only underlay. Terrain depth, reflections, shadow textures, sensors and
 * collision are untouched. Visible fragments are covered by the normal pass. */
static Shader aw_unit_xray;
static void aw_units_through_begin(void){
    if(!aw_unit_xray.id){
#ifdef PLATFORM_WEB
#define AW_XRAY_GLSL "#version 300 es\nprecision highp float;\n"
#else
#define AW_XRAY_GLSL "#version 330\n"
#endif
        const char*vertex=AW_XRAY_GLSL
            "in vec3 vertexPosition;in vec3 vertexNormal;in vec4 vertexColor;in vec2 vertexTexCoord;uniform mat4 mvp;out vec3 n;out vec4 c;out vec2 uv;"
            "void main(){n=vertexNormal;c=vertexColor;uv=vertexTexCoord;gl_Position=mvp*vec4(vertexPosition,1.0);}";
        const char*fragment=AW_XRAY_GLSL
            "in vec3 n;in vec4 c;in vec2 uv;uniform sampler2D texture0;out vec4 finalColor;"
            "void main(){vec3 albedo=(c*texture(texture0,uv)).rgb;float light=.65+.35*max(dot(normalize(n),normalize(vec3(-.55,.85,-.4))),0.0);"
            "finalColor=vec4(mix(albedo,vec3(.40,.82,.88),.60)*light,.60);}";
        aw_unit_xray=LoadShaderFromMemory(vertex,fragment);
#undef AW_XRAY_GLSL
    }
    rlDrawRenderBatchActive();rlDisableDepthTest();rlDisableDepthMask();BeginShaderMode(aw_unit_xray);
}
static void aw_units_through_end(void){EndShaderMode();rlDrawRenderBatchActive();rlEnableDepthMask();rlEnableDepthTest();}
#endif
