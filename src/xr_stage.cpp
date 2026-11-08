#include "xr_stage.h"
#include "xr_gl_state.h"
#ifdef MCU_QUEST
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <openxr/openxr_platform.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace mcu {
namespace {
constexpr float pi = 3.14159265358979323846f;
void xrCheck(XrResult result, const char *what) {
    if (XR_FAILED(result))
        throw std::runtime_error(std::string(what) + " (OpenXR " + std::to_string(result) + ")");
}
struct V3 {
    float x, y, z;
};
V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 mul(V3 a, float v) { return {a.x * v, a.y * v, a.z * v}; }
struct Matrix {
    float v[16]{};
    static Matrix identity() {
        Matrix m;
        m.v[0] = m.v[5] = m.v[10] = m.v[15] = 1;
        return m;
    }
};
Matrix product(const Matrix &a, const Matrix &b) {
    Matrix r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                r.v[c * 4 + row] += a.v[k * 4 + row] * b.v[c * 4 + k];
    return r;
}
Matrix poseMatrix(XrPosef p, float scale = 1) {
    auto q = p.orientation;
    Matrix m = Matrix::identity();
    m.v[0] = 1 - 2 * (q.y * q.y + q.z * q.z);
    m.v[1] = 2 * (q.x * q.y + q.z * q.w);
    m.v[2] = 2 * (q.x * q.z - q.y * q.w);
    m.v[4] = 2 * (q.x * q.y - q.z * q.w);
    m.v[5] = 1 - 2 * (q.x * q.x + q.z * q.z);
    m.v[6] = 2 * (q.y * q.z + q.x * q.w);
    m.v[8] = 2 * (q.x * q.z + q.y * q.w);
    m.v[9] = 2 * (q.y * q.z - q.x * q.w);
    m.v[10] = 1 - 2 * (q.x * q.x + q.y * q.y);
    for (int i = 0; i < 12; ++i)
        m.v[i] *= scale;
    m.v[12] = p.position.x;
    m.v[13] = p.position.y;
    m.v[14] = p.position.z;
    return m;
}
Matrix viewMatrix(XrPosef p) {
    Matrix r = poseMatrix(
        {{-p.orientation.x, -p.orientation.y, -p.orientation.z, p.orientation.w}, {0, 0, 0}});
    r.v[12] = -(r.v[0] * p.position.x + r.v[4] * p.position.y + r.v[8] * p.position.z);
    r.v[13] = -(r.v[1] * p.position.x + r.v[5] * p.position.y + r.v[9] * p.position.z);
    r.v[14] = -(r.v[2] * p.position.x + r.v[6] * p.position.y + r.v[10] * p.position.z);
    return r;
}
Matrix projection(XrFovf f) {
    float l = std::tan(f.angleLeft), r = std::tan(f.angleRight), u = std::tan(f.angleUp),
          d = std::tan(f.angleDown);
    constexpr float n = .025f, far = 2000.f;
    Matrix m;
    m.v[0] = 2 / (r - l);
    m.v[5] = 2 / (u - d);
    m.v[8] = (r + l) / (r - l);
    m.v[9] = (u + d) / (u - d);
    m.v[10] = -(far + n) / (far - n);
    m.v[11] = -1;
    m.v[14] = -2 * far * n / (far - n);
    return m;
}
Matrix placement(V3 p, V3 scale = {1, 1, 1}, float pitch = 0) {
    Matrix m = poseMatrix({{std::sin(pitch / 2), 0, 0, std::cos(pitch / 2)}, {p.x, p.y, p.z}});
    for (int row = 0; row < 3; ++row) {
        m.v[row] *= scale.x;
        m.v[4 + row] *= scale.y;
        m.v[8 + row] *= scale.z;
    }
    return m;
}
V3 point(const Matrix &m, V3 p, bool normal = false) {
    return {m.v[0] * p.x + m.v[4] * p.y + m.v[8] * p.z + (normal ? 0 : m.v[12]),
            m.v[1] * p.x + m.v[5] * p.y + m.v[9] * p.z + (normal ? 0 : m.v[13]),
            m.v[2] * p.x + m.v[6] * p.y + m.v[10] * p.z + (normal ? 0 : m.v[14])};
}
// SDL caches its GL state. Restore every binding/state touched by this renderer.
struct Vertex {
    V3 position, normal;
    float u, v;
    std::array<float, 4> color;
    float finish;
};
using Vertices = std::vector<Vertex>;
using Color = std::array<float, 4>;
constexpr Color ink{.018f, .029f, .053f, 1}, blue{.025f, .12f, .29f, 1}, cyan{.07f, .73f, .88f, 1},
    gold{.98f, .55f, .12f, 1}, metal{.36f, .43f, .52f, 1}, white{.78f, .84f, .89f, 1};
void quad(Vertices &out, const Matrix &m, V3 a, V3 b, V3 c, V3 d, V3 n, Color color,
          float finish = .5f) {
    Vertex v[4]{{point(m, a), point(m, n, true), 0, 1, color, finish},
                {point(m, b), point(m, n, true), 1, 1, color, finish},
                {point(m, c), point(m, n, true), 1, 0, color, finish},
                {point(m, d), point(m, n, true), 0, 0, color, finish}};
    for (int i : {0, 1, 2, 0, 2, 3})
        out.push_back(v[i]);
}
void box(Vertices &o, V3 center, V3 size, Color col, float finish = .5f,
         const Matrix &parent = Matrix::identity()) {
    auto m = product(parent, placement(center));
    float x = size.x / 2, y = size.y / 2, z = size.z / 2;
    quad(o, m, {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}, {0, 0, 1}, col, finish);
    quad(o, m, {x, -y, -z}, {-x, -y, -z}, {-x, y, -z}, {x, y, -z}, {0, 0, -1}, col, finish);
    quad(o, m, {-x, -y, -z}, {-x, -y, z}, {-x, y, z}, {-x, y, -z}, {-1, 0, 0}, col, finish);
    quad(o, m, {x, -y, z}, {x, -y, -z}, {x, y, -z}, {x, y, z}, {1, 0, 0}, col, finish);
    quad(o, m, {-x, y, z}, {x, y, z}, {x, y, -z}, {-x, y, -z}, {0, 1, 0}, col, finish);
    quad(o, m, {-x, -y, -z}, {x, -y, -z}, {x, -y, z}, {-x, -y, z}, {0, -1, 0}, col, finish);
}
void bevel(Vertices &o, V3 center, V3 size, Color color, float radius, float finish = .5f) {
    // An eight-sided extrusion gives the enclosure real chamfer highlights.
    float x = size.x / 2, y = size.y / 2, z = size.z / 2,
          r = std::min({radius, x * .45f, y * .45f});
    std::array<V3, 8> p{{{-x + r, -y, z},
                         {x - r, -y, z},
                         {x, -y + r, z},
                         {x, y - r, z},
                         {x - r, y, z},
                         {-x + r, y, z},
                         {-x, y - r, z},
                         {-x, -y + r, z}}};
    auto m = placement(center);
    for (int i = 0; i < 8; ++i) {
        int j = (i + 1) % 8;
        V3 n{p[j].y - p[i].y, p[i].x - p[j].x, 0};
        float len = std::hypot(n.x, n.y);
        n = mul(n, 1 / len);
        quad(o, m, {p[i].x, p[i].y, -z}, {p[j].x, p[j].y, -z}, p[j], p[i], n, color, finish);
        quad(o, m, {0, 0, z}, p[i], p[j], {0, 0, z}, {0, 0, 1}, color, finish);
        quad(o, m, {0, 0, -z}, {p[j].x, p[j].y, -z}, {p[i].x, p[i].y, -z}, {0, 0, -z}, {0, 0, -1},
             color, finish);
    }
}
void cylinder(Vertices &o, V3 center, float radius, float depth, Color color,
              const Matrix &parent = Matrix::identity()) {
    auto m = product(parent, placement(center));
    for (int i = 0; i < 20; ++i) {
        float a = i * 2 * pi / 20, b = (i + 1) * 2 * pi / 20;
        V3 p{radius * std::cos(a), radius * std::sin(a), depth / 2},
            q{radius * std::cos(b), radius * std::sin(b), depth / 2};
        quad(o, m, {0, 0, depth / 2}, p, q, {0, 0, depth / 2}, {0, 0, 1}, color, .12f);
        quad(o, m, {p.x, p.y, -depth / 2}, {q.x, q.y, -depth / 2}, q, p,
             {std::cos((a + b) / 2), std::sin((a + b) / 2), 0}, color, .15f);
    }
}
void legend(Vertices &o, const Matrix &m, V3 at, float pixel, const char *text) {
    // Original geometric lettering, batched into the furniture mesh.
    for (const char *p = text; *p; ++p, at.x += pixel * 6) {
        std::array<unsigned, 5> cols{};
        switch (*p) {
        case 'S':
            cols = {0x46, 0x49, 0x49, 0x49, 0x31};
            break;
        case 'C':
            cols = {0x3e, 0x41, 0x41, 0x41, 0x22};
            break;
        case 'U':
            cols = {0x3f, 0x40, 0x40, 0x40, 0x3f};
            break;
        case 'Y':
            cols = {0x07, 0x08, 0x70, 0x08, 0x07};
            break;
        case 'M':
            cols = {0x7f, 0x02, 0x04, 0x02, 0x7f};
            break;
        case 'E':
            cols = {0x7f, 0x49, 0x49, 0x49, 0x41};
            break;
        case 'N':
            cols = {0x7f, 0x02, 0x04, 0x08, 0x7f};
            break;
        default:
            break;
        }
        for (int col = 0; col < 5; ++col)
            for (int row = 0; row < 7; ++row)
                if (cols[col] & (1u << row)) {
                    float x = at.x + col * pixel, y = at.y + (6 - row) * pixel;
                    quad(o, m, {x, y, at.z}, {x + pixel * .84f, y, at.z},
                         {x + pixel * .84f, y + pixel * .84f, at.z}, {x, y + pixel * .84f, at.z},
                         {0, 0, 1}, cyan, -1);
                }
    }
}
GLuint compile(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]{};
        glGetShaderInfoLog(shader, sizeof log, nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("XR scene shader: ") + log);
    }
    return shader;
}
const char *vertexSource = R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv;
layout(location=3) in vec4 color;
layout(location=4) in float finish;
uniform mat4 vp,model;
uniform highp int kind;
out vec3 worldPosition,worldNormal;
out vec2 texcoord;
out vec4 tint;
out float roughness;
uniform float voxelCurveAngle,voxelDepthScale;
void main(){
 vec3 pos=position,n=normal;
 if(voxelCurveAngle>.00001){
  float theta=pos.x*voxelCurveAngle,radius=1.0/voxelCurveAngle;
  float sn=sin(theta),cs=cos(theta),z=pos.z*voxelDepthScale;
  pos=vec3(sn*(radius-z),pos.y,radius-cs*(radius-z));
  n=vec3(cs*n.x-sn*n.z,n.y,sn*n.x+cs*n.z);
 }
 vec4 p=model*vec4(pos,1);worldPosition=p.xyz;worldNormal=kind==7?vec3(0,0,1):normalize(mat3(model)*n);texcoord=uv;tint=color;roughness=finish;gl_Position=vp*p;
}
)GLSL";
const char *fragmentSource = R"GLSL(#version 300 es
precision highp float;
in vec3 worldPosition,worldNormal;
in vec2 texcoord;
in vec4 tint;
in float roughness;
uniform sampler2D image;
uniform vec3 eye;
uniform highp int kind;
uniform int crt,maskMode;
uniform vec2 raster;
uniform vec4 nativeHole;
uniform bool clipNative;
uniform float intensity,glassStrength,surroundBrightness;
uniform bool flipImage,srgbTarget;
uniform bool efficientShader;
out vec4 result;
vec3 displayColor(vec2 uv){return texture(image,vec2(uv.x,flipImage?1.0-uv.y:uv.y)).rgb;}
void main(){
 vec3 c=tint.rgb;float a=tint.a;
 if(kind==1||kind==2||kind==3){
  vec2 uv=texcoord;
  if(kind==3&&clipNative&&all(greaterThanEqual(uv,nativeHole.xy))&&all(lessThan(uv,nativeHole.xy+nativeHole.zw)))discard;
  vec4 sampleColor=texture(image,vec2(uv.x,flipImage?1.0-uv.y:uv.y));
  a=sampleColor.a;
  if(a<.002)discard;
  c=(kind==1||kind==3)?sampleColor.rgb/max(a,.001):sampleColor.rgb;
  if(kind==2&&crt>0){
   vec3 n=normalize(worldNormal);vec3 v=normalize(eye-worldPosition);float facing=max(dot(n,v),0.0);
   float phase=fract(uv.y*raster.y+.75)*2.0-1.0;
   float wave=4.0*phase*(1.0-abs(phase));
   wave=.225*(wave*abs(wave)-wave)+wave;
   float scan=.5+.5*(efficientShader?wave:cos(uv.y*raster.y*6.2831853));
   float scanAA=1.0-smoothstep(.5,1.25,fwidth(uv.y*raster.y));
   float strength=intensity*(crt==1?.19:crt==2?.32:.43);
   c*=1.0-strength*scan*scanAA;
   if(maskMode>0){float lane=mod(floor(uv.x*raster.x*3.0),3.0);vec3 mask=lane<1.0?vec3(1,.68,.68):lane<2.0?vec3(.68,1,.68):vec3(.68,.68,1);
    float aa=1.0-smoothstep(.45,1.5,fwidth(uv.x*raster.x*3.0));c*=mix(vec3(1),mask,aa*intensity*(maskMode==1?.38:.65));}
   vec2 edge=uv*(1.0-uv);float vignette=pow(clamp(edge.x*edge.y*16.0,0.0,1.0),.12);
   c*=mix(1.0,vignette,intensity);vec3 glow=max(c-vec3(.65),vec3(0));c+=glow*sqrt(glow)*.12*intensity;
   float rim=1.0-facing;c+=vec3(.015,.025,.04)*rim*rim*rim*glassStrength;
  }
  if(kind==3)c*=surroundBrightness;
 }else if(kind==6){
  vec2 uv=vec2(fract(texcoord.x*4.0),texcoord.y);
  float fade=smoothstep(0.0,.28,uv.y)*smoothstep(0.0,.28,1.0-uv.y);
  c=mix(vec3(.008,.013,.025),displayColor(uv)*surroundBrightness,fade);
 }else if(kind==4){
  vec3 n=normalize(worldNormal);vec3 v=normalize(eye-worldPosition);float rim=1.0-max(dot(n,v),0.0);
  float square=rim*rim;float fresnel=square*square;c=vec3(.2,.47,.65);a=glassStrength*(.015+.14*fresnel);
 }else if(kind==5){
  vec2 cell=worldPosition.xz*.8;vec2 grid=abs(fract(cell-.5)-.5)/max(fwidth(cell),vec2(.001));
  float line=1.0-min(min(grid.x,grid.y),1.0);float fade=exp(-length(worldPosition.xz-eye.xz)*.055);
  c=vec3(.014,.022,.036)+vec3(.025,.095,.12)*line*fade;
 }else if(kind==7){
  // Voxel face lighting is cached with geometry; preserve its palette and
  // avoid per-fragment CRT, specular and rim work on standalone headsets.
  c=tint.rgb;
 }else{
  vec3 n=normalize(worldNormal);vec3 v=normalize(eye-worldPosition);float facing=max(dot(n,v),0.0);
  vec3 l=normalize(vec3(-.45,.85,.65));float diffuse=max(dot(n,l),0.0);
  float spec=pow(max(dot(reflect(-l,n),v),0.0),mix(100.0,12.0,roughness));
  float edge=1.0-facing;float square=edge*edge;float rim=square*square;
  c*=.3+.7*diffuse;c+=vec3(.7,.84,1)*spec*(1.0-roughness)*.7;
  c+=vec3(.035,.10,.14)*rim;
  if(roughness<0.0)c=tint.rgb*1.2;
 }
 // Color constants and cartridge texels are authored in display space.
 // An sRGB framebuffer performs the final encode, so linearize exactly once.
 if(srgbTarget)c=pow(max(c,vec3(0)),vec3(2.2));
 result=vec4(c*a,a);
}
)GLSL";
struct Mesh {
    GLuint vao = 0, buffer = 0;
    GLsizei count = 0;
    void upload(const Vertices &vertices, bool streaming = false) {
        if (!vao) {
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &buffer);
        }
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                     streaming ? GL_STREAM_DRAW : GL_STATIC_DRAW);
        const int sizes[]{3, 3, 2, 4, 1};
        const size_t offsets[]{offsetof(Vertex, position), offsetof(Vertex, normal),
                               offsetof(Vertex, u), offsetof(Vertex, color),
                               offsetof(Vertex, finish)};
        for (int i = 0; i < 5; ++i) {
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, sizes[i], GL_FLOAT, GL_FALSE, sizeof(Vertex),
                                  reinterpret_cast<void *>(offsets[i]));
        }
        count = GLsizei(vertices.size());
    }
    void draw() const {
        if (count) {
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, count);
        }
    }
    void clear() {
        if (buffer)
            glDeleteBuffers(1, &buffer);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        buffer = vao = 0;
        count = 0;
    }
};
// Voxel faces already have palette lighting. Stream only position and RGBA;
// ordinary scene meshes keep their normals, UVs and material parameters.
struct VolumeVertex {
    V3 position;
    std::array<uint8_t, 4> color;
};
static_assert(sizeof(VolumeVertex) == 16);
using VolumeVertices = std::vector<VolumeVertex>;
struct VolumeStream {
    GLuint vao = 0, buffer = 0;
    GLsizei count = 0;
    void upload(const VolumeVertices &vertices) {
        count = GLsizei(vertices.size());
        if (!count)
            return;
        if (!vao) {
            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &buffer);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VolumeVertex), nullptr);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VolumeVertex),
                                  reinterpret_cast<void *>(offsetof(VolumeVertex, color)));
        } else
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VolumeVertex), vertices.data(),
                     GL_STREAM_DRAW);
    }
    void draw() const {
        if (count) {
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, count);
        }
    }
    void clear() {
        if (buffer)
            glDeleteBuffers(1, &buffer);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        buffer = vao = 0;
        count = 0;
    }
};
} // namespace

struct XrStageRenderer::Impl {
    XrSession session = XR_NULL_HANDLE;
    struct Eye {
        XrSwapchain chain = XR_NULL_HANDLE;
        int width = 0, height = 0;
        std::vector<XrSwapchainImageOpenGLESKHR> images;
        GLuint depth = 0;
    };
    std::array<Eye, 2> eyes{};
    std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    GLuint program = 0, framebuffer = 0, nativeTexture = 0, demoTexture = 0, backdropTexture = 0;
    int nativeW = 0, nativeH = 0, eyeDetail = -1;
    std::array<XrViewConfigurationView, 2> eyeSizes{};
    int64_t eyeFormat = GL_RGBA8;
    std::vector<uint32_t> rgbaScratch;
    bool backdropAllocated = false;
    uint64_t remoteHash = 0, insetHash = 0;
    Mesh remoteMesh, insetFurniture, insetScreen, cube;
    Mesh volumeHud;
    VolumeStream volumeMesh, volumeAlpha;
    std::vector<std::array<VolumeVertex, 6>> volumeAlphaQuads;
    std::vector<std::pair<float, size_t>> volumeAlphaOrder;
    VolumeVertices volumeVertices, volumeAlphaVertices;
    GLuint volumeHudImage = 0;
    int volumeHudW = 0, volumeHudH = 0;
    std::array<uint64_t, 7> volumeLayout{};
    int imageUniform = -1, crtUniform = -1, maskUniform = -1, rasterUniform = -1,
        intensityUniform = -1, glassUniform = -1, brightnessUniform = -1, srgbUniform = -1,
        flipUniform = -1, holeUniform = -1, clipUniform = -1, efficientUniform = -1;
    uint64_t nativeVersion = ~uint64_t(0), backdropVersion = ~uint64_t(0), frameCount = 0,
             meshHash = 0;
    bool srgb = false, foveationAvailable = false;
    PFN_xrCreateFoveationProfileFB createFoveation = nullptr;
    PFN_xrDestroyFoveationProfileFB destroyFoveation = nullptr;
    PFN_xrUpdateSwapchainFB updateFoveation = nullptr;
    std::array<XrFoveationProfileFB, 3> foveationProfiles{};
    int foveationLevel = -1;
    Mesh furniture, screen, glass, plain, surroundings;
    Matrix screenLocal = Matrix::identity(), glassLocal = Matrix::identity();
    int vp = -1, model = -1, kind = -1, eyePosition = -1;
    int voxelCurveUniform = -1, voxelDepthUniform = -1;
    static float physicalVoxelWidth(const Settings &s, int mode, const VoxelCamera &camera) {
        return mode == 4   ? s.questCabinetWidth * .01f
               : mode == 6 ? s.questWallHeight * .01f * camera.width / std::max(1.f, camera.height)
                           : s.questWidth * .01f;
    }
    void initialise(XrInstance instance, XrSystemId system, XrSession xrSession, int detail,
                    bool enableFoveation) {
        GlState restore;
        session = xrSession;
        if (enableFoveation) {
            auto load = [&](const char *name, auto &function) {
                return XR_SUCCEEDED(xrGetInstanceProcAddr(
                    instance, name, reinterpret_cast<PFN_xrVoidFunction *>(&function)));
            };
            foveationAvailable = load("xrCreateFoveationProfileFB", createFoveation) &&
                                 load("xrDestroyFoveationProfileFB", destroyFoveation) &&
                                 load("xrUpdateSwapchainFB", updateFoveation);
            if (foveationAvailable)
                for (int i = 0; i < 3; ++i) {
                    XrFoveationLevelProfileCreateInfoFB level{
                        XR_TYPE_FOVEATION_LEVEL_PROFILE_CREATE_INFO_FB};
                    level.level = std::array<XrFoveationLevelFB, 3>{
                        XR_FOVEATION_LEVEL_NONE_FB, XR_FOVEATION_LEVEL_LOW_FB,
                        XR_FOVEATION_LEVEL_MEDIUM_FB}[i];
                    XrFoveationProfileCreateInfoFB profile{
                        XR_TYPE_FOVEATION_PROFILE_CREATE_INFO_FB};
                    profile.next = &level;
                    if (XR_FAILED(createFoveation(session, &profile, &foveationProfiles[i]))) {
                        foveationAvailable = false;
                        break;
                    }
                }
        }
        SDL_Log("MCU XR optional peripheral foveation: %s",
                foveationAvailable ? "available" : "ordinary-rendering fallback");
        GLuint vert = compile(GL_VERTEX_SHADER, vertexSource),
               frag = compile(GL_FRAGMENT_SHADER, fragmentSource);
        program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);
        glDeleteShader(vert);
        glDeleteShader(frag);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048]{};
            glGetProgramInfoLog(program, sizeof log, nullptr, log);
            throw std::runtime_error(std::string("XR scene link: ") + log);
        }
        vp = glGetUniformLocation(program, "vp");
        model = glGetUniformLocation(program, "model");
        voxelCurveUniform = glGetUniformLocation(program, "voxelCurveAngle");
        voxelDepthUniform = glGetUniformLocation(program, "voxelDepthScale");
        kind = glGetUniformLocation(program, "kind");
        eyePosition = glGetUniformLocation(program, "eye");
        imageUniform = glGetUniformLocation(program, "image");
        crtUniform = glGetUniformLocation(program, "crt");
        maskUniform = glGetUniformLocation(program, "maskMode");
        rasterUniform = glGetUniformLocation(program, "raster");
        intensityUniform = glGetUniformLocation(program, "intensity");
        glassUniform = glGetUniformLocation(program, "glassStrength");
        brightnessUniform = glGetUniformLocation(program, "surroundBrightness");
        srgbUniform = glGetUniformLocation(program, "srgbTarget");
        flipUniform = glGetUniformLocation(program, "flipImage");
        holeUniform = glGetUniformLocation(program, "nativeHole");
        clipUniform = glGetUniformLocation(program, "clipNative");
        efficientUniform = glGetUniformLocation(program, "efficientShader");
        glGenFramebuffers(1, &framebuffer);
        uint32_t count = 0;
        xrCheck(xrEnumerateViewConfigurationViews(instance, system,
                                                  XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                                                  &count, nullptr),
                "Read stereo views");
        if (count != 2)
            throw std::runtime_error("XR presentation requires two stereo views");
        std::array<XrViewConfigurationView, 2> sizes{
            {{XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        xrCheck(xrEnumerateViewConfigurationViews(instance, system,
                                                  XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 2,
                                                  &count, sizes.data()),
                "Read stereo view sizes");
        xrCheck(xrEnumerateSwapchainFormats(session, 0, &count, nullptr), "Read scene formats");
        std::vector<int64_t> formats(count);
        xrCheck(xrEnumerateSwapchainFormats(session, count, &count, formats.data()),
                "Read scene formats");
        srgb = std::find(formats.begin(), formats.end(), GL_SRGB8_ALPHA8) != formats.end();
        if (!srgb && std::find(formats.begin(), formats.end(), GL_RGBA8) == formats.end())
            throw std::runtime_error("No supported stereo color format");
        eyeSizes = sizes;
        eyeFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        configureEyes(detail);
        Vertices q;
        quad(q, Matrix::identity(), {-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0},
             {0, 0, 1}, white);
        plain.upload(q);
        Vertices unitCube;
        box(unitCube, {0, 0, 0}, {1, 1, 1}, cyan, .28f);
        cube.upload(unitCube);
        Vertices panorama;
        for (int i = 0; i < 64; ++i) {
            float u = i / 64.f, v = (i + 1) / 64.f;
            auto pt = [](float t, float y) {
                return V3{std::sin(t * 2 * pi), y, std::cos(t * 2 * pi)};
            };
            size_t start = panorama.size();
            quad(panorama, Matrix::identity(), pt(u, -1), pt(v, -1), pt(v, 1), pt(u, 1), {0, 0, 1},
                 white);
            const float coords[6][2]{{u, 1}, {v, 1}, {v, 0}, {u, 1}, {v, 0}, {u, 0}};
            for (int j = 0; j < 6; ++j) {
                panorama[start + j].u = coords[j][0];
                panorama[start + j].v = coords[j][1];
            }
        }
        surroundings.upload(panorama);
        std::vector<uint32_t> demo(512 * 512);
        for (int y = 0; y < 512; ++y)
            for (int x = 0; x < 512; ++x) {
                float u = x / 511.f, v = y / 511.f;
                int ring = std::abs(std::hypot(u - .5f, v - .45f) - .25f) < .014f;
                int grid = x % 32 < 2 || y % 32 < 2;
                int r = ring ? 255 : 10 + int(12 * u),
                    g = ring ? 171 : 24 + int(32 * (1 - v)) + grid * 10,
                    b = ring ? 42 : 44 + int(38 * u) + grid * 16;
                // Bytes are RGBA; the pattern is original and contains no cartridge art.
                demo[y * 512 + x] =
                    uint32_t(255u << 24) | uint32_t(b << 16) | uint32_t(g << 8) | uint32_t(r);
            }
        glGenTextures(1, &demoTexture);
        glBindTexture(GL_TEXTURE_2D, demoTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     demo.data());
        textureParameters();
        SDL_Log("MCU XR stereo framebuffer=%u", framebuffer);
        SDL_Log("MCU XR stereo ready: %dx%d per eye; independent menu quad", eyes[0].width,
                eyes[0].height);
    }
    void configureEyes(int detail) {
        detail = std::clamp(detail, 0, 2);
        if (eyeDetail == detail)
            return;
        eyeDetail = detail;
        foveationLevel = -1;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        uint32_t count = 0;
        for (int i = 0; i < 2; ++i) {
            if (eyes[i].chain)
                xrDestroySwapchain(eyes[i].chain);
            if (eyes[i].depth)
                glDeleteRenderbuffers(1, &eyes[i].depth);
            eyes[i] = {};
            auto &e = eyes[i];
            // A bounded eye buffer preserves 120 Hz headroom; mural textures have
            // a separate detail budget and sharp options stay in a quad layer.
            float scale = std::min(1.f, float(std::array<int, 3>{1024, 1280, 1600}[detail]) /
                                            std::max(eyeSizes[i].recommendedImageRectWidth,
                                                     eyeSizes[i].recommendedImageRectHeight));
            e.width = std::max(256, int(eyeSizes[i].recommendedImageRectWidth * scale));
            e.height = std::max(256, int(eyeSizes[i].recommendedImageRectHeight * scale));
            XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            XrSwapchainCreateInfoFoveationFB foveation{XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB};
            foveation.flags = XR_SWAPCHAIN_CREATE_FOVEATION_SCALED_BIN_BIT_FB;
            if (foveationAvailable)
                info.next = &foveation;
            info.usageFlags =
                XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            info.format = eyeFormat;
            info.sampleCount = 1;
            info.width = e.width;
            info.height = e.height;
            info.faceCount = info.arraySize = info.mipCount = 1;
            XrResult created = xrCreateSwapchain(session, &info, &e.chain);
            if (XR_FAILED(created) && info.next) {
                info.next = nullptr;
                foveationAvailable = false;
                created = xrCreateSwapchain(session, &info, &e.chain);
            }
            xrCheck(created, "Create stereo swapchain");
            xrCheck(xrEnumerateSwapchainImages(e.chain, 0, &count, nullptr), "Read stereo images");
            e.images.assign(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});
            xrCheck(xrEnumerateSwapchainImages(
                        e.chain, count, &count,
                        reinterpret_cast<XrSwapchainImageBaseHeader *>(e.images.data())),
                    "Read stereo images");
            glGenRenderbuffers(1, &e.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.width, e.height);
        }
    }
    static void textureParameters() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    GLuint native(const XrStageContent &c) {
        if (!c.loaded || !c.native || c.native->empty())
            return demoTexture;
        if (!nativeTexture)
            glGenTextures(1, &nativeTexture);
        glBindTexture(GL_TEXTURE_2D, nativeTexture);
        if (nativeW != c.nativeWidth || nativeH != c.nativeHeight || nativeVersion != c.version) {
            // Core uses 0xAARRGGBB. Convert to portable RGBA bytes explicitly.
            rgbaScratch.resize(c.native->size());
            auto &rgba = rgbaScratch;
            for (size_t i = 0; i < rgba.size(); ++i) {
                uint32_t p = (*c.native)[i];
                rgba[i] = (p & 0xff00ff00) | ((p >> 16) & 255) | ((p & 255) << 16);
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            if (nativeW != c.nativeWidth || nativeH != c.nativeHeight)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, c.nativeWidth, c.nativeHeight, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, rgba.data());
            else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, c.nativeWidth, c.nativeHeight, GL_RGBA,
                                GL_UNSIGNED_BYTE, rgba.data());
            nativeW = c.nativeWidth;
            nativeH = c.nativeHeight;
            nativeVersion = c.version;
            textureParameters();
        }
        return nativeTexture;
    }
    GLuint backdrop(const XrStageContent &c) {
        if (!c.loaded || !c.backdrop)
            return demoTexture;
        if (!backdropTexture)
            glGenTextures(1, &backdropTexture);
        glBindTexture(GL_TEXTURE_2D, backdropTexture);
        if (backdropVersion != c.version) {
            rgbaScratch.resize(c.backdrop->size());
            auto &rgba = rgbaScratch;
            for (size_t i = 0; i < rgba.size(); ++i) {
                uint32_t p = (*c.backdrop)[i];
                rgba[i] = (p & 0xff00ff00) | ((p >> 16) & 255) | ((p & 255) << 16);
            }
            if (!backdropAllocated) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 56, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             rgba.data());
                backdropAllocated = true;
            } else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 56, GL_RGBA, GL_UNSIGNED_BYTE,
                                rgba.data());
            textureParameters();
            backdropVersion = c.version;
        }
        return backdropTexture;
    }
    void crtGeometry(Vertices &f, Vertices &p, const Settings &s, int nw, int nh, bool inset) {
        float w = inset ? 1.f : s.questTvWidth * .01f, h = w * nh / std::max(1, nw),
              y = inset ? 0.f : h / 2 + .72f;
        Color shell = s.questTvModel == 0   ? Color{.56f, .59f, .63f, 1}
                      : s.questTvModel == 1 ? Color{.10f, .09f, .08f, 1}
                                            : Color{.22f, .25f, .28f, 1};
        // 4:3 consumer proportions, deep rear housing, original geometry; no brand assets.
        bevel(f, {0, y, -w * .28f}, {w * 1.40f, h * 1.43f, w * .86f}, shell, w * .045f, .48f);
        bevel(f, {0, y, -w * .68f}, {w * .83f, h * .86f, w * .22f}, ink, w * .04f, .65f);
        for (int side : {-1, 1}) {
            for (int vent = 0; vent < 13; ++vent)
                box(f, {side * w * .627f, y + (vent - 6) * h * .047f, w * .153f},
                    {w * .083f, h * .011f, w * .008f}, ink, .8f);
            if (s.questTvModel == 2) {
                box(f, {side * w * .72f, y, w * .07f}, {w * .035f, h * .72f, w * .035f}, metal,
                    .3f);
                for (int end : {-1, 1})
                    box(f, {side * w * .69f, y + end * h * .35f, 0},
                        {w * .10f, w * .035f, w * .20f}, metal, .3f);
            }
        }
        for (int vent = 0; vent < 12; ++vent)
            box(f, {(vent - 5.5f) * w * .045f, y + h * .72f, -w * .24f},
                {w * .016f, w * .006f, w * .34f}, ink, .7f);
        bevel(f, {0, y, w * .087f}, {w * 1.10f, h * 1.13f, w * .14f}, ink, w * .045f, .58f);
        if (!inset)
            box(f, {0, .34f, -.16f}, {w * .80f, .60f, w * .48f}, ink, .6f);
        if (!inset)
            for (int i : {-1, 1})
                box(f, {i * w * .34f, .025f, -.13f}, {.07f, .05f, w * .53f}, metal, .18f);
        for (int i = 0; i < 15; ++i)
            box(f, {(i - 7) * w * .031f, y - h * .605f, w * .158f}, {w * .014f, h * .035f, .006f},
                metal, .4f);
        cylinder(f, {w * .45f, y - h * .60f, w * .165f}, w * .026f, .025f, metal);
        box(f, {-w * .45f, y - h * .60f, w * .167f}, {.02f, .012f, .008f}, cyan, -1);
        legend(f, Matrix::identity(), {-w * .46f, y - h * .63f, w * .168f}, w / 260.f, "Y MENU");
        // Curved glass is geometry, so stereo highlights follow head motion.
        float bulge =
            !inset && s.questCrtCoplanar && s.questSurround ? 0.f : w * s.questCrtCurve * .0006f;
        for (int iy = 0; iy < 16; ++iy)
            for (int ix = 0; ix < 28; ++ix) {
                auto v = [&](int xx, int yy) {
                    float u = xx / 28.f, v = yy / 16.f, x = (u - .5f) * w, py = (v - .5f) * h;
                    return V3{x, y + py,
                              w * .16f + bulge * (1 - ((u - .5f) * 2) * ((u - .5f) * 2)) *
                                             (1 - ((v - .5f) * 2) * ((v - .5f) * 2))};
                };
                V3 a = v(ix, iy), b = v(ix + 1, iy), c = v(ix + 1, iy + 1), d = v(ix, iy + 1);
                size_t start = p.size();
                quad(p, Matrix::identity(), a, b, c, d, {0, 0, 1}, white);
                float u0 = ix / 28.f, u1 = (ix + 1) / 28.f, v0 = 1 - iy / 16.f,
                      v1 = 1 - (iy + 1) / 16.f;
                const float coords[6][2]{{u0, v0}, {u1, v0}, {u1, v1},
                                         {u0, v0}, {u1, v1}, {u0, v1}};
                for (int j = 0; j < 6; ++j) {
                    p[start + j].u = coords[j][0];
                    p[start + j].v = coords[j][1];
                }
            }
    }
    void controlsGeometry(const XrSpatialControls &c, const Settings &s, int nw, int nh) {
        uint64_t key = 0;
        if (c.visible && c.buttons) {
            key = 1469598103934665603ull;
            for (auto b : *c.buttons) {
                key ^= b.action;
                key *= 1099511628211ull;
            }
            for (auto id : c.hover) {
                key ^= uint32_t(id + 2);
                key *= 1099511628211ull;
            }
            for (auto id : c.pressed) {
                key ^= uint32_t(id + 2);
                key *= 1099511628211ull;
            }
            for (auto active : c.active) {
                key ^= unsigned(active);
                key *= 1099511628211ull;
            }
            if (key != remoteHash) {
                Vertices v;
                bevel(v, {0, 0, -.025f}, {spatialWidth + .04f, spatialHeight + .04f, .075f}, ink,
                      .035f, .55f);
                bevel(v, {0, .355f, .015f}, {.74f, .075f, .035f}, metal, .025f, .35f);
                for (size_t i = 0; i < c.buttons->size(); ++i) {
                    auto b = (*c.buttons)[i];
                    bool down = c.pressed[0] == int(i) || c.pressed[1] == int(i);
                    bool hot = c.hover[0] == int(i) || c.hover[1] == int(i);
                    bevel(v, {b.x, b.y, down ? .018f : .029f}, {b.width, b.height, .025f},
                          down                                 ? gold
                          : hot                                ? cyan
                          : i < c.active.size() && c.active[i] ? Color{.04f, .28f, .20f, 1}
                                                               : blue,
                          .012f, .42f);
                }
                remoteMesh.upload(v);
                remoteHash = key;
            }
        }
        uint64_t ik =
            uint64_t(nw) * 65537 + nh + s.questTvModel * 999999 + s.questCrtCurve * 111111;
        if (c.nativeVisible && ik != insetHash) {
            Vertices f, p;
            crtGeometry(f, p, s, nw, nh, true);
            insetFurniture.upload(f);
            insetScreen.upload(p);
            insetHash = ik;
        }
    }
    void geometry(const Settings &s, int mode, float ratio, int nw, int nh) {
        uint64_t hash = 1469598103934665603ull;
        for (int v : {mode, int(ratio * 10000), nw, nh, s.questCabinetWidth, s.questCabinetHeight,
                      s.questCabinetTilt, s.questTvWidth, s.questCrtCurve, s.questTvModel,
                      int(s.questCrtCoplanar), int(s.questSurround), s.questCurve, int(s.help)}) {
            hash ^= uint32_t(v);
            hash *= 1099511628211ull;
        }
        if (hash == meshHash)
            return;
        meshHash = hash;
        Vertices f, p, g;
        if (mode == 4) {
            float w = s.questCabinetWidth * .01f, d = w / std::clamp(ratio, .25f, 2.0f),
                  tilt = s.questCabinetTilt * pi / 180;
            float front = s.questCabinetHeight * .01f, center = front + std::sin(tilt) * d / 2;
            auto table = placement({0, center, 0}, {1, 1, 1}, -pi / 2 + tilt);
            screenLocal = product(table, placement({0, 0, .012f}, {w, d, 1}));
            glassLocal = product(table, placement({0, 0, .046f}, {w, d, 1}));
            for (int side : {-1, 1}) {
                box(f, {side * (w / 2 + .065f), 0, -.05f}, {.13f, d + .32f, .24f}, blue, .3f,
                    table);
                box(f, {side * (w / 2 + .065f), 0, .094f}, {.10f, d + .32f, .035f}, metal, .12f,
                    table);
                box(f, {side * (w / 2 + .019f), 0, .116f}, {.014f, d + .18f, .014f}, cyan, -1,
                    table);
                for (int end : {-1, 1}) {
                    float z = end * (d / 2 - .15f),
                          height = std::max(.25f, center - z * std::sin(tilt) - .15f);
                    box(f, {side * (w / 2 + .015f), height / 2, z}, {.055f, height, .055f}, metal,
                        .18f);
                    box(f, {side * (w / 2 + .015f), .025f, z}, {.14f, .05f, .14f}, ink, .7f);
                }
                auto buttonPose = product(
                    table, placement({side * (w / 2 + .14f), -d / 2 + .20f, -.01f}, {1, 1, 1}, 0));
                // Flipper buttons sit on the side rails, distinct from virtual controls.
                box(f, {side * (w / 2 + .14f), -d / 2 + .20f, .08f}, {.035f, .13f, .06f}, gold,
                    .18f, table);
                (void)buttonPose;
            }
            box(f, {0, 0, -.11f}, {w + .12f, d + .22f, .12f}, ink, .7f, table);
            box(f, {0, -d / 2 - .09f, .035f}, {w + .26f, .18f, .15f}, blue, .3f, table);
            box(f, {0, -d / 2 - .19f, .08f}, {w + .30f, .04f, .11f}, metal, .12f, table);
            legend(f, table, {-w * .44f, -d / 2 - .15f, .114f}, w / 100.f, "MCU");
            legend(f, table, {w * .18f, -d / 2 - .12f, .114f}, w / 260.f, "Y MENU");
            box(f, {0, d / 2 + .07f, .07f}, {w + .26f, .14f, .16f}, metal, .18f, table);
            float backY = center + d / 2 * std::sin(tilt) + .58f;
            bevel(f, {0, backY, -d / 2 - .13f}, {w + .30f, .85f, .24f}, blue, .07f, .28f);
            box(f, {0, backY, -d / 2 + .002f}, {w + .10f, .015f, .015f}, cyan, -1);
            for (int i = 0; i < 9; ++i)
                box(f, {(i - 4) * w * .085f, backY - .30f, -d / 2 + .003f},
                    {w * .045f, .035f, .018f}, i % 2 ? gold : cyan, -1);
            // A real native score display in the backglass, independent of the board.
            quad(p, table, {-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0}, {-.5f, .5f, 0},
                 {0, 0, 1}, white);
        } else if (mode == 5) {
            crtGeometry(f, p, s, nw, nh, false);
            screenLocal = glassLocal = Matrix::identity();
        } else {
            float h = s.questWallHeight * .01f, w = h * ratio, angle = s.questCurve * pi / 180,
                  radius = w / std::max(.1f, angle);
            for (int i = 0; i < 96; ++i) {
                auto pnt = [&](float t, float y) {
                    float a = (t - .5f) * angle;
                    return V3{std::sin(a) * radius, y, radius * (1 - std::cos(a))};
                };
                size_t start = p.size();
                float u = i / 96.f, v = (i + 1) / 96.f;
                quad(p, Matrix::identity(), pnt(u, 0), pnt(v, 0), pnt(v, h), pnt(u, h), {0, 0, 1},
                     white);
                const float coords[6][2]{{u, 1}, {v, 1}, {v, 0}, {u, 1}, {v, 0}, {u, 0}};
                for (int j = 0; j < 6; ++j) {
                    p[start + j].u = coords[j][0];
                    p[start + j].v = coords[j][1];
                }
            }
            screenLocal = Matrix::identity();
            glassLocal = Matrix::identity();
        }
        // Pinball's board is the shared unit plane; its model includes the slope.
        if (mode == 4) {
            p.clear();
            quad(p, Matrix::identity(), {-.5f, -.5f, 0}, {.5f, -.5f, 0}, {.5f, .5f, 0},
                 {-.5f, .5f, 0}, {0, 0, 1}, white);
        }
        if (mode == 6 && s.help) {
            float w = s.questWallHeight * .01f * ratio, angle = s.questCurve * pi / 180,
                  radius = w / angle;
            XrPosef badge{{0, -std::sin(angle / 4), 0, std::cos(angle / 4)},
                          {std::sin(angle / 2) * radius, .06f, radius * (1 - std::cos(angle / 2))}};
            auto m = poseMatrix(badge);
            box(f, {-.24f, .055f, .006f}, {.56f, .13f, .01f}, ink, .8f, m);
            legend(f, m, {-.46f, .022f, .013f}, .009f, "Y MENU");
        }
        furniture.upload(f);
        screen.upload(p);
        glass.upload(g);
    }
    template <class Stream>
    void draw(const Stream &mesh, const Matrix &m, int shaderKind, GLuint texture = 0) {
        glUniformMatrix4fv(model, 1, GL_FALSE, m.v);
        glUniform1i(kind, shaderKind);
        glBindTexture(GL_TEXTURE_2D, texture ? texture : demoTexture);
        mesh.draw();
    }
    const XrCompositionLayerProjection *render(XrSpace space, XrTime time, XrPosef origin,
                                               const Settings &s, int mode,
                                               const XrStageContent &content,
                                               const XrSpatialControls &controls, bool ar,
                                               const std::string &capture, float resolutionScale) {
        std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
        locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime = time;
        locate.space = space;
        XrViewState state{XR_TYPE_VIEW_STATE};
        uint32_t count = 0;
        xrCheck(xrLocateViews(session, &locate, &state, 2, &count, views.data()),
                "Locate stereo eyes");
        if (count != 2 ||
            (state.viewStateFlags &
             (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT)) !=
                (XR_VIEW_STATE_POSITION_VALID_BIT | XR_VIEW_STATE_ORIENTATION_VALID_BIT))
            return nullptr;
        // The presenter owns the complete SDL/OpenXR state guard; avoid a nested
        // glGet* round trip on each eye frame.
        configureEyes(s.questEyeDetail);
        int peripheral = s.questFoveation && s.adaptiveResolution
                             ? (s.projectionEffects >= 3                        ? 2
                                : s.projectionEffects >= 1 || s.performanceMode ? 1
                                                                                : 0)
                             : 0;
        if (foveationAvailable && peripheral != foveationLevel) {
            bool accepted = true;
            XrSwapchainStateFoveationFB state{XR_TYPE_SWAPCHAIN_STATE_FOVEATION_FB};
            state.profile = foveationProfiles[peripheral];
            for (auto &eye : eyes)
                if (XR_FAILED(updateFoveation(
                        eye.chain, reinterpret_cast<XrSwapchainStateBaseHeaderFB *>(&state))))
                    accepted = false;
            if (accepted)
                foveationLevel = peripheral;
            else {
                state.profile = foveationProfiles[0];
                for (auto &eye : eyes)
                    updateFoveation(eye.chain,
                                    reinterpret_cast<XrSwapchainStateBaseHeaderFB *>(&state));
                foveationAvailable = false;
                SDL_Log("MCU XR peripheral detail unavailable; ordinary adaptive rendering remains "
                        "active");
            }
        }
        glUseProgram(program);
        glUniform1f(voxelCurveUniform, 0);
        glUniform1f(voxelDepthUniform, 1);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(imageUniform, 0);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);
        glUniform1i(crtUniform, s.questCrt);
        glUniform1i(maskUniform, s.questCrtMask);
        glUniform2f(rasterUniform, content.nativeWidth, content.nativeHeight);
        glUniform1f(intensityUniform, s.questCrtStrength * .01f);
        glUniform1f(glassUniform, s.questGlass * .01f);
        glUniform1f(brightnessUniform, s.questSurroundBrightness * .01f);
        glUniform1i(srgbUniform, srgb);
        glUniform1i(flipUniform, s.questFlip);
        glUniform1i(efficientUniform, s.questShader == 0);
        GLuint nativeImage = native(content), expanded = demoTexture;
        bool volumetric = content.volume && content.volume->ready;
        std::array<uint64_t, 7> volumeKey{
            {volumetric ? content.volume->version() : 0, uint64_t(mode), uint64_t(s.questWidth),
             uint64_t(s.questCabinetWidth), uint64_t(content.nativeWidth),
             uint64_t(content.nativeHeight), uint64_t(s.questWallHeight)}};
        if (volumetric && volumeLayout != volumeKey) {
            Vertices hud;
            volumeVertices.clear();
            volumeAlphaQuads.clear();
            const auto &scene = *content.volume;
            const auto &camera = scene.camera;
            float unit = 1 / std::max(1.f, camera.width);
            auto pt = [&](VoxelPoint p) {
                return V3{(p.x - camera.x - camera.width / 2) * unit,
                          -(p.y - camera.y - camera.height / 2) * unit, p.z * unit};
            };
            for (const auto &f : scene.mesh.faces) {
                std::array<uint8_t, 4> col{uint8_t(f.color >> 16), uint8_t(f.color >> 8),
                                           uint8_t(f.color), uint8_t(f.color >> 24)};
                if (!col[3])
                    continue;
                // Y is inverted once when entering the room. Reverse winding
                // so face normals and hardware back-face culling still agree.
                auto a = pt(f.points[0]), b = pt(f.points[3]), c = pt(f.points[2]),
                     d = pt(f.points[1]);
                const std::array<VolumeVertex, 6> vertices{
                    {{a, col}, {b, col}, {c, col}, {a, col}, {c, col}, {d, col}}};
                if (col[3] == 255)
                    volumeVertices.insert(volumeVertices.end(), vertices.begin(), vertices.end());
                else
                    volumeAlphaQuads.push_back(vertices);
            }
            float physicalWidth = std::max(.5f, physicalVoxelWidth(s, mode, camera));
            float hudScale = std::min(1.f, 1.2f / physicalWidth);
            for (auto strip : scene.hudAreas) {
                if (strip.w <= 0 || strip.h <= 0)
                    continue;
                float x0 = (float(strip.x) / content.nativeWidth - .5f) * hudScale;
                float x1 = x0 + float(strip.w) / content.nativeWidth * hudScale;
                float hh = float(strip.h) / content.nativeWidth * hudScale;
                float y0 = strip.y == 0 ? camera.height / camera.width * .5f + .025f + hh
                           : strip.y + strip.h == content.nativeHeight
                               ? -camera.height / camera.width * .5f - .025f
                               : .15f;
                float y1 = y0 - hh, front = camera.fitDepth * unit + .002f;
                size_t start = hud.size();
                quad(hud, Matrix::identity(), {x0, y0, front}, {x1, y0, front}, {x1, y1, front},
                     {x0, y1, front}, {0, 0, 1}, {1, 1, 1, 1});
                float u0 = float(strip.x) / content.nativeWidth,
                      u1 = float(strip.x + strip.w) / content.nativeWidth;
                float v0 = float(strip.y) / content.nativeHeight,
                      v1 = float(strip.y + strip.h) / content.nativeHeight;
                const float uv[6][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v0}, {u1, v1}, {u0, v1}};
                for (int j = 0; j < 6; ++j) {
                    hud[start + j].u = uv[j][0];
                    hud[start + j].v = uv[j][1];
                }
            }
            volumeMesh.upload(volumeVertices);
            volumeHud.upload(hud);
            if (!volumeHudImage)
                glGenTextures(1, &volumeHudImage);
            glBindTexture(GL_TEXTURE_2D, volumeHudImage);
            rgbaScratch.resize(scene.hudPixels.size());
            for (size_t i = 0; i < rgbaScratch.size(); ++i) {
                uint32_t p = scene.hudPixels[i];
                rgbaScratch[i] = (p & 0xff00ff00) | ((p >> 16) & 255) | ((p & 255) << 16);
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            if (volumeHudW != content.nativeWidth || volumeHudH != content.nativeHeight)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, content.nativeWidth, content.nativeHeight,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaScratch.data());
            else
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, content.nativeWidth, content.nativeHeight,
                                GL_RGBA, GL_UNSIGNED_BYTE, rgbaScratch.data());
            volumeHudW = content.nativeWidth;
            volumeHudH = content.nativeHeight;
            textureParameters();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            volumeLayout = volumeKey;
        }
        GLuint backdropImage =
            !ar && mode == 5 && s.questWorldBackdrop ? backdrop(content) : demoTexture;
        bool bound = false;
        if (content.expanded) {
            float w = 0, h = 0;
            if (SDL_GL_BindTexture(content.expanded, &w, &h) == 0) {
                GLint id = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
                expanded = GLuint(id);
                bound = true;
            }
        }
        float ratio = content.loaded && content.scene.valid
                          ? float(content.coverageWidth / content.coverageHeight)
                          : .5f;
        if (mode != 4 && !content.loaded)
            ratio = 16.f / 9;
        if (mode >= 4)
            geometry(s, mode, ratio, content.nativeWidth, content.nativeHeight);
        controlsGeometry(controls, s, content.nativeWidth, content.nativeHeight);
        GLuint labelsImage = 0;
        if (controls.visible && controls.labels) {
            float tw = 0, th = 0;
            if (SDL_GL_BindTexture(controls.labels, &tw, &th) == 0) {
                GLint id = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &id);
                labelsImage = GLuint(id);
            }
        }
        auto hole =
            coplanarViewport(content.left, content.top, content.coverageWidth,
                             content.coverageHeight, content.scene.cameraX, content.scene.cameraY,
                             content.nativeWidth, content.nativeHeight);
        glUniform4f(holeUniform, hole.x, hole.y, hole.width, hole.height);
        glUniform1i(clipUniform, 0);
        Matrix root = poseMatrix(origin, s.questScale * .01f);
        for (int i = 0; i < 2; ++i) {
            auto &e = eyes[i];
            uint32_t index = 0;
            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xrCheck(xrAcquireSwapchainImage(e.chain, &acquire, &index), "Acquire stereo image");
            XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            wait.timeout = XR_INFINITE_DURATION;
            xrCheck(xrWaitSwapchainImage(e.chain, &wait), "Wait stereo image");
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   e.images[index].image, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      e.depth);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                xrReleaseSwapchainImage(e.chain, &release);
                if (foveationAvailable) {
                    foveationAvailable = false;
                    eyeDetail = -1;
                    configureEyes(s.questEyeDetail);
                    SDL_Log("MCU XR foveated framebuffer rejected; rebuilt ordinary eye images");
                    return nullptr;
                }
                throw std::runtime_error("Incomplete XR stereo framebuffer");
            }
            float scale =
                std::isfinite(resolutionScale) ? std::clamp(resolutionScale, .375f, 1.f) : 1.f;
            int renderWidth = std::max(1, int(e.width * scale));
            int renderHeight = std::max(1, int(e.height * scale));
            int renderX = (e.width - renderWidth) / 2, renderY = (e.height - renderHeight) / 2;
            glViewport(renderX, renderY, renderWidth, renderHeight);
            glEnable(GL_SCISSOR_TEST);
            glScissor(renderX, renderY, renderWidth, renderHeight);
            glClearColor(ar ? 0 : .008f, ar ? 0 : .013f, ar ? 0 : .025f, ar ? 0 : 1);
            glDepthMask(GL_TRUE);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            auto vpMatrix = product(projection(views[i].fov), viewMatrix(views[i].pose));
            glUniformMatrix4fv(vp, 1, GL_FALSE, vpMatrix.v);
            glUniform3f(eyePosition, views[i].pose.position.x, views[i].pose.position.y,
                        views[i].pose.position.z);
            if (!ar && mode == 5 && s.questWorldBackdrop && content.loaded) {
                glDepthMask(GL_FALSE);
                XrPosef backdropPose{origin.orientation, views[i].pose.position};
                draw(surroundings,
                     product(poseMatrix(backdropPose), placement({0, 0, 0}, {500, 300, 500})), 6,
                     backdropImage);
                glDepthMask(GL_TRUE);
            }
            if (!ar && s.questEnvironment && !(volumetric && mode == 0)) {
                auto ground =
                    product(poseMatrix(origin), placement({0, -.01f, 0}, {1000, 1000, 1}, -pi / 2));
                draw(plain, ground, 5);
            }
            // The surrounding expanded level follows the native camera. The CRT
            // face always keeps the unmodified cartridge viewport and aspect.
            if (volumetric) {
                const auto &camera = content.volume->camera;
                float width = physicalVoxelWidth(s, mode, camera);
                float lift = camera.fitDepth * width / std::max(1.f, camera.width) + .025f;
                float tilt = mode == 4 ? s.questCabinetTilt * pi / 180 : 0;
                float center =
                    mode == 4 ? s.questCabinetHeight * .01f +
                                    std::sin(tilt) * (width / std::clamp(ratio, .25f, 2.f)) * .5f
                              : 0;
                V3 position = mode == 3   ? V3{0, lift + s.questWallGap * .01f, 0}
                              : mode == 4 ? V3{0, center + lift, 0}
                                          : V3{0, 1.3f + s.questHeight * .01f, .025f};
                Matrix neutralModel =
                    product(poseMatrix(origin, s.questScale * .01f),
                            placement(position, {width, width, width},
                                      mode == 3 || mode == 4 ? -pi / 2 + tilt : 0));
                const auto &orbit = content.volumeView;
                float yaw = orbit.yaw * pi / 180, pitch = orbit.pitch * pi / 180;
                Matrix volumeModel =
                    product(neutralModel,
                            product(poseMatrix({{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)},
                                                {orbit.panX, -orbit.panY * camera.height /
                                                                  std::max(1.f, camera.width),
                                                 0}}),
                                    placement({0, 0, 0}, {1, 1, 1}, pitch)));
                float curveAngle =
                    mode == 6 ? std::clamp(float(s.questCurve), 10.f, 250.f) * pi / 180 : 0;
                float curveDepth = curveAngle > 0
                                       ? std::min(1.f, .45f / (curveAngle * camera.fitDepth /
                                                               std::max(1.f, camera.width)))
                                       : 1;
                glUniform1f(voxelCurveUniform, curveAngle);
                glUniform1f(voxelDepthUniform, curveDepth);
                if (mode == 4)
                    draw(furniture, root, 0);
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                glFrontFace(GL_CCW);
                draw(volumeMesh, volumeModel, 7);
                volumeAlphaOrder.clear();
                volumeAlphaVertices.clear();
                for (size_t j = 0; j < volumeAlphaQuads.size(); ++j) {
                    const auto &q = volumeAlphaQuads[j];
                    V3 center{(q[0].position.x + q[2].position.x) * .5f,
                              (q[0].position.y + q[2].position.y) * .5f,
                              (q[0].position.z + q[2].position.z) * .5f};
                    auto warped =
                        curveVoxelPoint({center.x, center.y, center.z}, curveAngle, curveDepth);
                    auto p = point(volumeModel, {warped.x, warped.y, warped.z});
                    auto e = views[i].pose.position;
                    float distance = (p.x - e.x) * (p.x - e.x) + (p.y - e.y) * (p.y - e.y) +
                                     (p.z - e.z) * (p.z - e.z);
                    volumeAlphaOrder.emplace_back(distance, j);
                }
                std::sort(volumeAlphaOrder.begin(), volumeAlphaOrder.end(), [](auto a, auto b) {
                    return a.first == b.first ? a.second < b.second : a.first > b.first;
                });
                for (auto entry : volumeAlphaOrder) {
                    auto &q = volumeAlphaQuads[entry.second];
                    volumeAlphaVertices.insert(volumeAlphaVertices.end(), q.begin(), q.end());
                }
                if (!volumeAlphaVertices.empty()) {
                    volumeAlpha.upload(volumeAlphaVertices);
                    glDepthMask(GL_FALSE);
                    draw(volumeAlpha, volumeModel, 7);
                    glDepthMask(GL_TRUE);
                }
                glDisable(GL_CULL_FACE);
                draw(volumeHud, neutralModel, 1, volumeHudImage);
                glUniform1f(voxelCurveUniform, 0);
                glUniform1f(voxelDepthUniform, 1);
            }
            if (!volumetric && mode < 4 && content.loaded) {
                float width = s.questWidth * .01f;
                float aspect = content.scene.valid
                                   ? float(content.coverageWidth / content.coverageHeight)
                                   : float(content.nativeWidth) / std::max(1, content.nativeHeight);
                draw(plain,
                     product(root, placement({0, 1.3f + s.questHeight * .01f, .025f},
                                             {width, width / std::max(.1f, aspect), 1})),
                     1, bound ? expanded : nativeImage);
            }
            if (!volumetric && mode == 5 && s.questSurround && content.loaded &&
                content.scene.valid && bound) {
                float unit = s.questTvWidth * .01f / std::max(1, content.nativeWidth),
                      h = s.questTvWidth * .01f * content.nativeHeight /
                          std::max(1, content.nativeWidth),
                      y = h / 2 + .72f;
                float x = (content.left + content.coverageWidth / 2 - content.scene.cameraX -
                           content.nativeWidth / 2) *
                          unit;
                float py = y + (content.scene.cameraY + content.nativeHeight / 2 - content.top -
                                content.coverageHeight / 2) *
                                   unit;
                glUniform1i(clipUniform, s.questCrtCoplanar);
                draw(plain,
                     product(root,
                             placement({x, py, s.questCrtCoplanar ? s.questTvWidth * .0016f : -.7f},
                                       {float(content.coverageWidth) * unit,
                                        float(content.coverageHeight) * unit, 1})),
                     3, expanded);
            }
            glUniform1i(clipUniform, 0);
            if (mode >= 4 && !volumetric) {
                draw(furniture, root, 0);
                draw(screen, product(root, screenLocal), mode == 5 ? 2 : 1,
                     mode == 5 ? nativeImage : expanded);
            }
            if (controls.nativeVisible) {
                auto mini = poseMatrix(controls.nativePose, s.questInsetWidth * .01f);
                draw(insetFurniture, mini, 0);
                draw(insetScreen, mini, 2, nativeImage);
                draw(cube, product(mini, placement({0, -.48f, .1f}, {.52f, .035f, .035f})), 0);
            }
            for (int hand = 0; hand < 2; ++hand) {
                if (controls.fingerVisible[hand]) {
                    auto p = controls.fingerTips[hand];
                    draw(cube, placement({p.x, p.y, p.z}, {.011f, .011f, .011f}), 0);
                }
                if (controls.rayLength[hand] > 0) {
                    float distance = controls.rayLength[hand];
                    auto ray = poseMatrix(controls.rays[hand]);
                    draw(cube,
                         product(ray, placement({0, 0, -distance / 2}, {.0015f, .0015f, distance})),
                         0);
                    draw(cube, product(ray, placement({0, 0, -distance}, {.009f, .009f, .009f})),
                         0);
                }
            }
            if (controls.visible) {
                auto remote = poseMatrix(controls.pose, s.questRemoteScale * .01f);
                draw(remoteMesh, remote, 0);
                if (labelsImage) {
                    glDepthMask(GL_FALSE);
                    draw(
                        plain,
                        product(remote, placement({0, 0, .051f}, {spatialWidth, spatialHeight, 1})),
                        1, labelsImage);
                    glDepthMask(GL_TRUE);
                }
                if (controls.buttons)
                    for (auto b : *controls.buttons)
                        if (b.action == GameStick)
                            draw(cube,
                                 product(remote, placement({b.x + controls.stick.x * .18f,
                                                            b.y + controls.stick.y * .18f, .075f},
                                                           {.075f, .075f, .055f})),
                                 0);
            }
            if (mode == 4 && !volumetric) {
                float w = s.questCabinetWidth * .01f, d = w / std::clamp(ratio, .25f, 2.f),
                      center = s.questCabinetHeight * .01f +
                               std::sin(s.questCabinetTilt * pi / 180) * d / 2;
                float backY = center + d / 2 * std::sin(s.questCabinetTilt * pi / 180) + .58f;
                float backH = std::min(.55f, w * .84f * content.nativeHeight /
                                                 std::max(1, content.nativeWidth));
                draw(plain,
                     product(root, placement({0, backY + .065f, -d / 2 + .013f},
                                             {backH * content.nativeWidth /
                                                  std::max(1, content.nativeHeight),
                                              backH, 1})),
                     2, nativeImage);
                if (s.questGlass) {
                    glDepthMask(GL_FALSE);
                    draw(plain, product(root, glassLocal), 4);
                    glDepthMask(GL_TRUE);
                }
            }
            if (!capture.empty() && i == 0) {
                auto surface = SDL_CreateRGBSurfaceWithFormat(0, renderWidth, renderHeight, 32,
                                                              SDL_PIXELFORMAT_RGBA32);
                if (surface) {
                    glPixelStorei(GL_PACK_ALIGNMENT, 4);
                    glReadPixels(renderX, renderY, renderWidth, renderHeight, GL_RGBA,
                                 GL_UNSIGNED_BYTE, surface->pixels);
                    auto *pixels = static_cast<uint8_t *>(surface->pixels);
                    std::vector<uint8_t> row(surface->pitch);
                    for (int y = 0; y < renderHeight / 2; ++y) {
                        auto *a = pixels + y * surface->pitch,
                             *b = pixels + (renderHeight - y - 1) * surface->pitch;
                        std::memcpy(row.data(), a, row.size());
                        std::memcpy(a, b, row.size());
                        std::memcpy(b, row.data(), row.size());
                    }
                    if (SDL_SaveBMP(surface, capture.c_str()) == 0)
                        SDL_Log("MCU XR menu scene captured: %dx%d; ROM loaded=%d", renderWidth,
                                renderHeight, content.loaded);
                    SDL_FreeSurface(surface);
                }
            }
            glInvalidateFramebuffer(GL_FRAMEBUFFER, 1,
                                    std::array<GLenum, 1>{GL_DEPTH_ATTACHMENT}.data());
            glFlush();
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrCheck(xrReleaseSwapchainImage(e.chain, &release), "Release stereo image");
            auto &p = projectionViews[i];
            p = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            p.pose = views[i].pose;
            p.fov = views[i].fov;
            p.subImage.swapchain = e.chain;
            p.subImage.imageRect = {{renderX, renderY}, {renderWidth, renderHeight}};
            glDisable(GL_SCISSOR_TEST);
        }
        if (labelsImage)
            SDL_GL_UnbindTexture(controls.labels);
        if (bound)
            SDL_GL_UnbindTexture(content.expanded);
        layer.space = space;
        layer.layerFlags = ar ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT : 0;
        layer.viewCount = 2;
        layer.views = projectionViews.data();
        ++frameCount;
        return &layer;
    }
    ~Impl() {
        remoteMesh.clear();
        insetFurniture.clear();
        insetScreen.clear();
        cube.clear();
        furniture.clear();
        screen.clear();
        glass.clear();
        plain.clear();
        surroundings.clear();
        if (program)
            glDeleteProgram(program);
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        volumeMesh.clear();
        volumeAlpha.clear();
        volumeHud.clear();
        if (volumeHudImage)
            glDeleteTextures(1, &volumeHudImage);
        if (nativeTexture)
            glDeleteTextures(1, &nativeTexture);
        if (demoTexture)
            glDeleteTextures(1, &demoTexture);
        if (backdropTexture)
            glDeleteTextures(1, &backdropTexture);
        for (auto &e : eyes) {
            if (e.depth)
                glDeleteRenderbuffers(1, &e.depth);
            if (e.chain)
                xrDestroySwapchain(e.chain);
        }
        if (destroyFoveation)
            for (auto profile : foveationProfiles)
                if (profile)
                    destroyFoveation(profile);
    }
};
XrStageRenderer::XrStageRenderer() : impl(std::make_unique<Impl>()) {}
XrStageRenderer::~XrStageRenderer() = default;
void XrStageRenderer::initialise(XrInstance i, XrSystemId s, XrSession session, int detail,
                                 bool foveation) {
    impl->initialise(i, s, session, detail, foveation);
}
const XrCompositionLayerProjection *
XrStageRenderer::render(XrSpace space, XrTime time, XrPosef origin, const Settings &s, int mode,
                        const XrStageContent &c, const XrSpatialControls &controls, bool ar,
                        const std::string &capture, float resolutionScale) {
    return impl->render(space, time, origin, s, mode, c, controls, ar, capture, resolutionScale);
}
uint64_t XrStageRenderer::frames() const { return impl->frameCount; }
} // namespace mcu
#endif
