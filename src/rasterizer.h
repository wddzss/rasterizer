#pragma once
#include <functional>
#include <algorithm>
#include <cmath>
#include "math_utils.h"
#include "framebuffer.h"
#include "texture.h"
#include "model.h"

// ─────────────────────────────────────────
//  Per-vertex data after vertex shader
// ─────────────────────────────────────────
struct VSOut {
    Vec4 clip_pos;    // homogeneous clip space
    Vec3 world_pos;   // for lighting
    Vec3 world_normal;
    Vec2 uv;
};

// ─────────────────────────────────────────
//  Light
// ─────────────────────────────────────────
struct Light {
    Vec3 pos;
    Vec3 color = {1,1,1};
    float intensity = 1.f;
};

// ─────────────────────────────────────────
//  Material
// ─────────────────────────────────────────
struct Material {
    Vec3  ambient  = {0.1f, 0.1f, 0.1f};
    Vec3  diffuse  = {0.8f, 0.8f, 0.8f};
    Vec3  specular = {0.5f, 0.5f, 0.5f};
    float shininess= 32.f;

    Texture* diffuse_tex  = nullptr; // optional
    Texture* specular_tex = nullptr; // optional
    Texture* normal_tex   = nullptr; // optional (not used in basic mode)
};

// ─────────────────────────────────────────
//  Shader uniforms
// ─────────────────────────────────────────
struct Uniforms {
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    Mat4 mvp;
    Mat4 model_inv_T; // for normals

    Vec3 camera_pos;
    std::vector<Light> lights;
    Material material;
    bool wireframe = false;
};

// ─────────────────────────────────────────
//  Blinn-Phong fragment shader
// ─────────────────────────────────────────
inline Vec3 blinn_phong(const Vec3& pos, const Vec3& norm, const Vec2& uv,
                         const Uniforms& uni)
{
    const Material& mat = uni.material;
    Vec3 N = norm.normalize();

    // Diffuse texture
    Vec3 base_diffuse = mat.diffuse;
    if(mat.diffuse_tex && mat.diffuse_tex->valid)
        base_diffuse = mat.diffuse_tex->sample(uv.x, uv.y);

    Vec3 base_spec = mat.specular;
    if(mat.specular_tex && mat.specular_tex->valid)
        base_spec = mat.specular_tex->sample(uv.x, uv.y);

    Vec3 color = mat.ambient * base_diffuse;

    Vec3 V = (uni.camera_pos - pos).normalize();

    for(const auto& light : uni.lights) {
        Vec3 L  = (light.pos - pos).normalize();
        Vec3 H  = (L + V).normalize();

        float diff = std::max(0.f, N.dot(L));
        float spec = std::pow(std::max(0.f, N.dot(H)), mat.shininess);

        float dist = (light.pos - pos).length();
        float att  = light.intensity / (1.f + 0.05f*dist + 0.005f*dist*dist);

        color += (base_diffuse * diff + base_spec * spec) * light.color * att;
    }
    return color;
}

// ─────────────────────────────────────────
//  Rasterizer
// ─────────────────────────────────────────
class Rasterizer {
public:
    Framebuffer& fb;
    bool back_face_cull = true;

    Rasterizer(Framebuffer& fb) : fb(fb) {}

    // ── vertex shader ──
    VSOut vertex_shader(const Vertex& vert, const Uniforms& uni) {
        VSOut out;
        Vec4 world = transform_point(uni.model, vert.pos);
        out.world_pos    = world.xyz();
        out.world_normal = transform_dir(uni.model_inv_T, vert.normal).normalize();
        out.uv           = vert.uv;
        out.clip_pos     = uni.mvp * Vec4(vert.pos, 1.f);
        return out;
    }

    // ── draw one mesh ──
    void draw_mesh(const Mesh& mesh, const Uniforms& uni) {
        for(const auto& tri : mesh.triangles) {
            VSOut v0 = vertex_shader(tri.v[0], uni);
            VSOut v1 = vertex_shader(tri.v[1], uni);
            VSOut v2 = vertex_shader(tri.v[2], uni);
            rasterize_triangle(v0, v1, v2, uni);
        }
    }

private:
    // NDC → screen
    Vec3 ndc_to_screen(const Vec3& ndc) {
        float x = (ndc.x*0.5f+0.5f)*fb.width;
        float y = (ndc.y*0.5f+0.5f)*fb.height; // Y-up NDC
        float z = ndc.z;
        return {x, y, z};
    }

    // ── Triangle rasterization ──
    void rasterize_triangle(const VSOut& v0, const VSOut& v1, const VSOut& v2,
                            const Uniforms& uni)
    {
        // Clip-space W
        float w0 = v0.clip_pos.w;
        float w1 = v1.clip_pos.w;
        float w2 = v2.clip_pos.w;

        // Behind camera?
        if(w0<=0 || w1<=0 || w2<=0) return;

        // Perspective divide → NDC
        Vec3 ndc0 = v0.clip_pos.perspective_divide();
        Vec3 ndc1 = v1.clip_pos.perspective_divide();
        Vec3 ndc2 = v2.clip_pos.perspective_divide();

        // Frustum cull (loose)
        auto outside=[](const Vec3& n){
            return n.x<-1.1f||n.x>1.1f||n.y<-1.1f||n.y>1.1f||n.z<-1.f||n.z>1.f;
        };
        if(outside(ndc0)&&outside(ndc1)&&outside(ndc2)) return;

        // Screen coords
        Vec3 s0 = ndc_to_screen(ndc0);
        Vec3 s1 = ndc_to_screen(ndc1);
        Vec3 s2 = ndc_to_screen(ndc2);

        // Back-face culling (CCW front-face in screen space, Y-down)
        float cross2d = (s1.x-s0.x)*(s2.y-s0.y) - (s1.y-s0.y)*(s2.x-s0.x);
        // cross2d > 0 → CCW in Y-down screen → front face
        if(back_face_cull && cross2d <= 0) return;

        if(uni.wireframe) {
            draw_line(s0,s1,Color(0,255,0));
            draw_line(s1,s2,Color(0,255,0));
            draw_line(s2,s0,Color(0,255,0));
            return;
        }

        // Bounding box
        int xmin = (int)std::floor(std::min({s0.x,s1.x,s2.x}));
        int xmax = (int)std::ceil (std::max({s0.x,s1.x,s2.x}));
        int ymin = (int)std::floor(std::min({s0.y,s1.y,s2.y}));
        int ymax = (int)std::ceil (std::max({s0.y,s1.y,s2.y}));
        xmin=std::max(0,xmin); xmax=std::min(fb.width-1,xmax);
        ymin=std::max(0,ymin); ymax=std::min(fb.height-1,ymax);

        // Perspective-correct: store 1/w
        float iw0=1.f/w0, iw1=1.f/w1, iw2=1.f/w2;

        // Precompute attributes / w for perspective correction
        Vec3 wp0 = v0.world_pos  * iw0;
        Vec3 wn0 = v0.world_normal * iw0;
        Vec2 wu0 = {v0.uv.x*iw0, v0.uv.y*iw0};

        Vec3 wp1 = v1.world_pos  * iw1;
        Vec3 wn1 = v1.world_normal * iw1;
        Vec2 wu1 = {v1.uv.x*iw1, v1.uv.y*iw1};

        Vec3 wp2 = v2.world_pos  * iw2;
        Vec3 wn2 = v2.world_normal * iw2;
        Vec2 wu2 = {v2.uv.x*iw2, v2.uv.y*iw2};

        // Edge function helper
        auto edge = [](const Vec3& a, const Vec3& b, float px, float py) -> float {
            return (b.x-a.x)*(py-a.y) - (b.y-a.y)*(px-a.x);
        };
        float area = edge(s0,s1,s2.x,s2.y);
        if(std::abs(area) < 1e-6f) return;
        float inv_area = 1.f/area;

        for(int y=ymin; y<=ymax; y++) {
            for(int x=xmin; x<=xmax; x++) {
                float px=x+0.5f, py=y+0.5f;
                float l0 = edge(s1,s2,px,py) * inv_area;
                float l1 = edge(s2,s0,px,py) * inv_area;
                float l2 = 1.f - l0 - l1;
                if(l0<0||l1<0||l2<0) continue;

                // Depth (interpolated z in NDC)
                float z = l0*ndc0.z + l1*ndc1.z + l2*ndc2.z;
                // Use 1/w for depth test (larger = closer)
                float inv_w = l0*iw0 + l1*iw1 + l2*iw2;

                if(!fb.depth_test_and_write(x, y, inv_w)) continue;

                // Perspective-correct attribute recovery
                float w_corr = 1.f / inv_w;
                Vec3 pos_world = (wp0*l0 + wp1*l1 + wp2*l2) * w_corr;
                Vec3 nor_world = (wn0*l0 + wn1*l1 + wn2*l2) * w_corr;
                Vec2 uv_corr;
                uv_corr.x = (wu0.x*l0 + wu1.x*l1 + wu2.x*l2) * w_corr;
                uv_corr.y = (wu0.y*l0 + wu1.y*l1 + wu2.y*l2) * w_corr;

                // Fragment shader
                Vec3 color = blinn_phong(pos_world, nor_world, uv_corr, uni);

                // Gamma correction (linear → sRGB)
                color.x = std::pow(std::max(0.f,color.x), 1.f/2.2f);
                color.y = std::pow(std::max(0.f,color.y), 1.f/2.2f);
                color.z = std::pow(std::max(0.f,color.z), 1.f/2.2f);

                fb.set_color(x, y, Color::fromVec3(color));
            }
        }
    }

    // Bresenham line for wireframe
    void draw_line(Vec3 a, Vec3 b, Color col) {
        int x0=(int)a.x, y0=(int)a.y, x1=(int)b.x, y1=(int)b.y;
        int dx=std::abs(x1-x0), dy=std::abs(y1-y0);
        int sx=x0<x1?1:-1, sy=y0<y1?1:-1;
        int err=dx-dy;
        while(true){
            fb.set_color(x0,y0,col);
            if(x0==x1&&y0==y1) break;
            int e2=2*err;
            if(e2>-dy){err-=dy;x0+=sx;}
            if(e2< dx){err+=dx;y0+=sy;}
        }
    }
};
