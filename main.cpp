/*
 * =====================================================================
 *  软件光栅化渲染器  (Software Rasterizer)
 *  课程：图形学与虚拟现实  22CS32055
 *  作者：[你的姓名] [学号]
 *
 *  功能：
 *   - 纯CPU软件光栅化，无GPU依赖
 *   - MVP 矩阵变换（模型/视图/投影）
 *   - 背面剔除 + Z-buffer深度测试
 *   - 透视正确插值（Perspective-correct interpolation）
 *   - Blinn-Phong 光照模型（多光源）
 *   - 纹理映射（双线性插值）+ TGA纹理加载
 *   - Gamma校正
 *   - OBJ模型加载 + 内置几何体（球、立方体、平面）
 *   - 输出 TGA 图像
 *   - 可选：X11窗口预览（Linux）
 * =====================================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <cstdint>

#include "src/math_utils.h"
#include "src/framebuffer.h"
#include "src/texture.h"
#include "src/model.h"
#include "src/rasterizer.h"

// ──────────────────────────────────────────────────────────────────────
//  Optional X11 display (compile with -DUSE_X11 -lX11 if available)
// ──────────────────────────────────────────────────────────────────────
#ifdef USE_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>

static void show_x11(const Framebuffer& fb, const std::string& title) {
    Display* dpy = XOpenDisplay(nullptr);
    if(!dpy) { std::cerr << "Cannot open X display\n"; return; }
    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy,scr),
        0, 0, fb.width, fb.height, 0,
        BlackPixel(dpy,scr), BlackPixel(dpy,scr));
    XStoreName(dpy, win, title.c_str());
    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(dpy, win);

    // Build XImage
    Visual* vis = DefaultVisual(dpy,scr);
    int depth = DefaultDepth(dpy,scr);
    std::vector<uint32_t> pixels(fb.width*fb.height);
    for(int y=0;y<fb.height;y++) for(int x=0;x<fb.width;x++){
        const Color& c=fb.color_buf[y*fb.width+x];
        pixels[(fb.height-1-y)*fb.width+x] =
            ((uint32_t)c.r<<16)|((uint32_t)c.g<<8)|c.b;
    }
    XImage* img = XCreateImage(dpy,vis,depth,ZPixmap,0,
        reinterpret_cast<char*>(pixels.data()),
        fb.width,fb.height,32,0);
    GC gc = XCreateGC(dpy,win,0,nullptr);

    XEvent ev;
    bool running=true;
    while(running){
        XNextEvent(dpy,&ev);
        if(ev.type==Expose)
            XPutImage(dpy,win,gc,img,0,0,0,0,fb.width,fb.height);
        if(ev.type==KeyPress||ev.type==ButtonPress) running=false;
    }
    img->data=nullptr; // we own the pixel buffer
    XDestroyImage(img);
    XCloseDisplay(dpy);
}
#endif

// ──────────────────────────────────────────────────────────────────────
//  Helper: build Uniforms from camera/scene settings
// ──────────────────────────────────────────────────────────────────────
static Uniforms make_uniforms(int w, int h,
    Vec3 cam_pos, Vec3 cam_target,
    const Mat4& model,
    const Material& mat,
    const std::vector<Light>& lights)
{
    Uniforms uni;
    uni.model      = model;
    uni.view       = Mat4::lookAt(cam_pos, cam_target, {0,1,0});
    uni.projection = Mat4::perspective(
        45.f * 3.14159265f/180.f,  // fovY
        (float)w/h,                 // aspect
        0.1f, 100.f);               // near, far
    uni.mvp        = uni.projection * uni.view * uni.model;
    uni.model_inv_T= uni.model.transpose3x3(); // simplified: works for uniform scale
    uni.camera_pos = cam_pos;
    uni.lights     = lights;
    uni.material   = mat;
    return uni;
}

// ──────────────────────────────────────────────────────────────────────
//  Render one frame of the scene
// ──────────────────────────────────────────────────────────────────────
static void render_scene(Framebuffer& fb,
                         float yaw_deg,     // camera orbit angle
                         const std::string& obj_path)
{
    fb.clear(Color(20, 20, 35)); 

    Rasterizer rast(fb);

    // ── Camera ──
    float yaw = yaw_deg * 3.14159265f / 180.f;
    float cam_r = 5.5f, cam_h = 2.5f;
    Vec3 cam_pos = { cam_r*std::sin(yaw), cam_h, cam_r*std::cos(yaw) };
    Vec3 cam_target = {0, 0.5f, 0};

    // ── Lights (多光源) ──
    std::vector<Light> lights = {
        { {4, 6,  5}, {1.0f, 0.95f, 0.9f}, 1.5f },
        { {-3, 4, -4}, {0.4f, 0.6f, 1.0f}, 0.8f },
        { {0, 1, 3}, {0.9f, 0.5f, 0.3f}, 0.5f },
    };

    // 加载材质纹理的静态缓存
    static std::unordered_map<std::string, Texture> tex_cache;

        if(!obj_path.empty()) {
        Mesh mesh = load_obj(obj_path);
        if(mesh.triangles.empty()) mesh = make_sphere(48,48,1.f);
        
        // 按材质分组绘制
        std::vector<int> mat_ids;
        for(const auto& tri : mesh.triangles) {
            if(tri.material_id >= 0 && 
               std::find(mat_ids.begin(), mat_ids.end(), tri.material_id) == mat_ids.end()) {
                mat_ids.push_back(tri.material_id);
            }
        }
        
        // 如果没有材质，添加一个默认的
        if(mat_ids.empty()) mat_ids.push_back(-1);
        
        for(int mat_id : mat_ids) {
            Material mat;
            
            if(mat_id >= 0 && mat_id < (int)mesh.materials.size()) {
                const auto& mtl = mesh.materials[mat_id];
                mat.ambient   = mtl.ambient;
                mat.diffuse   = mtl.diffuse;
                mat.specular  = mtl.specular;
                mat.shininess = mtl.shininess;
                
                // 加载漫反射纹理
                if(!mtl.diffuse_map.empty()) {
                    std::string tex_path = obj_path.substr(0, obj_path.find_last_of("/\\") + 1) + mtl.diffuse_map;
                    auto it = tex_cache.find(tex_path);
                    if(it != tex_cache.end()) {
                        mat.diffuse_tex = &it->second;
                    } else {
                        Texture& tex = tex_cache[tex_path];
                        if(tex.load_tga(tex_path)) {
                            mat.diffuse_tex = &tex;
                            std::cout << "Loaded texture: " << tex_path << "\n";
                        }
                    }
                }
            } else {
                // 默认材质
                mat.ambient   = {0.15f, 0.15f, 0.15f};
                mat.diffuse   = {0.8f, 0.7f, 0.6f};
                mat.specular  = {0.5f, 0.5f, 0.5f};
                mat.shininess = 32.f;
            }
            
            // 收集该材质的三角形
            std::vector<Triangle> mat_tris;
            for(const auto& tri : mesh.triangles) {
                if(tri.material_id == mat_id) {
                    mat_tris.push_back(tri);
                }
            }
            
            if(mat_tris.empty()) continue;
            
            Mesh sub_mesh;
            sub_mesh.triangles = mat_tris;
            sub_mesh.name = mesh.name;
            
            Mat4 model = Mat4::rotateY(yaw * 0.2f) * Mat4::translate(0,0.5f,0);
            auto uni = make_uniforms(fb.width,fb.height,cam_pos,cam_target,
                                     model, mat, lights);
            rast.draw_mesh(sub_mesh, uni);
        }
    }

    

    // // ─────────────────────────────────────────
    // //  Object 1: OBJ model (or sphere fallback)
    // // ─────────────────────────────────────────
    // {
    //     Mesh mesh;
    //     static Texture diff_tex, spec_tex;
    //     static bool tex_loaded = false;

    //     if(!tex_loaded) {
    //         // Try to load textures; fall back to procedural
    //         if(!diff_tex.load_tga("textures/diffuse.tga"))
    //             diff_tex.make_checkerboard(512,512,{0.9f,0.7f,0.4f},{0.3f,0.2f,0.1f},8);
    //         if(!spec_tex.load_tga("textures/specular.tga"))
    //             spec_tex.make_checkerboard(512,512,{0.9f,0.9f,0.9f},{0.1f,0.1f,0.1f},8);
    //         tex_loaded=true;
    //     }

    //     if(!obj_path.empty()) {
    //         mesh = load_obj(obj_path);
    //         if(mesh.triangles.empty()) mesh = make_sphere(48,48,1.f);
    //     } else {
    //         mesh = make_sphere(48,48,1.f);
    //     }

    //     Material mat;
    //     mat.ambient   = {0.05f,0.05f,0.05f};
    //     mat.diffuse   = {0.8f,0.8f,0.8f};
    //     mat.specular  = {0.6f,0.6f,0.6f};
    //     mat.shininess = 64.f;
    //     mat.diffuse_tex  = &diff_tex;
    //     mat.specular_tex = &spec_tex;

    //     // Slowly rotate the model
    //     Mat4 model = Mat4::rotateY(yaw * 0.3f) * Mat4::translate(0,0.8f,0);
    //     auto uni = make_uniforms(fb.width,fb.height,cam_pos,cam_target,
    //                              model,mat,lights);
    //     rast.draw_mesh(mesh, uni);
    // }


    if(obj_path.empty()){
    // ─────────────────────────────────────────
    //  Object 2: Cube (左边)
    // ─────────────────────────────────────────
    {
        static Texture cube_tex;
        static bool loaded=false;
        if(!loaded){
            cube_tex.make_uv_grid(256,256);
            loaded=true;
        }
        Mesh cube = make_cube(0.8f);
        Material mat;
        mat.ambient={0.05f,0.05f,0.1f};
        mat.diffuse={0.3f,0.5f,0.9f};
        mat.specular={0.8f,0.8f,1.f};
        mat.shininess=128.f;
        mat.diffuse_tex=&cube_tex;

        Mat4 model = Mat4::translate(-1.8f, 0.4f, 0.3f)
                   * Mat4::rotateY(yaw*0.5f + 0.5f)
                   * Mat4::rotateX(0.3f);
        auto uni=make_uniforms(fb.width,fb.height,cam_pos,cam_target,model,mat,lights);
        rast.draw_mesh(cube,uni);
    }

    // ─────────────────────────────────────────
    //  Object 3: Small metallic sphere (右边)
    // ─────────────────────────────────────────
    {
        Mesh sphere = make_sphere(32,32,0.6f);
        Material mat;
        mat.ambient  = {0.02f,0.04f,0.02f};
        mat.diffuse  = {0.1f,0.4f,0.1f};
        mat.specular = {0.9f,0.95f,0.9f};
        mat.shininess= 256.f;

        Mat4 model = Mat4::translate(1.8f, 0.6f, -0.2f);
        auto uni=make_uniforms(fb.width,fb.height,cam_pos,cam_target,model,mat,lights);
        rast.draw_mesh(sphere,uni);
    }

    // ─────────────────────────────────────────
    //  Object 4: Ground plane
    // ─────────────────────────────────────────
    {
        static Texture floor_tex;
        static bool loaded=false;
        if(!loaded){
            floor_tex.make_checkerboard(256,256,{0.9f,0.9f,0.95f},{0.3f,0.3f,0.4f},16);
            loaded=true;
        }
        Mesh plane = make_plane(6.f, 8);
        Material mat;
        mat.ambient  = {0.05f,0.05f,0.05f};
        mat.diffuse  = {0.8f,0.8f,0.85f};
        mat.specular = {0.2f,0.2f,0.2f};
        mat.shininess= 16.f;
        mat.diffuse_tex=&floor_tex;

        Mat4 model = Mat4::identity();
        auto uni=make_uniforms(fb.width,fb.height,cam_pos,cam_target,model,mat,lights);
        rast.draw_mesh(plane,uni);
    }
}
}

// ──────────────────────────────────────────────────────────────────────
//  main
// ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // ── Parse arguments ──
    int   width     = 800;
    int   height    = 600;
    int   frames    = 1;         // how many frames to render (animation)
    float yaw_start = 30.f;
    std::string obj_path;
    std::string out_dir = "output";
    bool  show_window = false;

    for(int i=1;i<argc;i++){
        std::string a=argv[i];
        if(a=="--width"   && i+1<argc) width     = std::stoi(argv[++i]);
        else if(a=="--height"  && i+1<argc) height    = std::stoi(argv[++i]);
        else if(a=="--frames"  && i+1<argc) frames    = std::stoi(argv[++i]);
        else if(a=="--yaw"     && i+1<argc) yaw_start = std::stof(argv[++i]);
        else if(a=="--obj"     && i+1<argc) obj_path  = argv[++i];
        else if(a=="--outdir"  && i+1<argc) out_dir   = argv[++i];
        else if(a=="--window") { show_window = true; std::cout << "Window mode ON\n"; }
        else if(a=="--help"){
            std::cout <<
"软件光栅化渲染器\n"
"用法: rasterizer [选项]\n"
"  --width  N      图像宽度 (默认800)\n"
"  --height N      图像高度 (默认600)\n"
"  --frames N      渲染帧数 (默认1，>1生成动画序列)\n"
"  --yaw    F      起始相机偏航角(度，默认30)\n"
"  --obj    PATH   加载OBJ模型文件\n"
"  --outdir DIR    输出目录 (默认output)\n"
"  --window        (Linux+X11) 渲染后弹出窗口预览\n";
            return 0;
        }
    }

    std::cout << "====================================\n";
    std::cout << " 软件光栅化渲染器  Software Rasterizer\n";
    std::cout << "====================================\n";
    std::cout << " 分辨率: " << width << "x" << height << "\n";
    std::cout << " 帧数  : " << frames << "\n";
    if(!obj_path.empty()) std::cout << " OBJ   : " << obj_path << "\n";
    std::cout << "------------------------------------\n";

    Framebuffer fb(width, height);

    for(int f=0; f<frames; f++){
        float yaw = yaw_start + f*(360.f/frames);
        render_scene(fb, yaw, obj_path);

        // Build output filename
        std::ostringstream ss;
        ss << out_dir << "/frame_"
           << std::setw(4) << std::setfill('0') << f
           << ".tga";
        std::string fname = ss.str();
        fb.save_tga(fname);
        std::cout << " [" << f+1 << "/" << frames << "] 已保存: " << fname << "\n";
    }

    // Also save depth map of last frame
    fb.save_depth_tga(out_dir + "/depth_last.tga");
    std::cout << " 深度图: " << out_dir << "/depth_last.tga\n";

    std::cout << "------------------------------------\n";
    std::cout << " 渲染完成！\n";

#ifdef USE_X11
    if(show_window) {
        std::cout << " 打开X11窗口预览（按任意键关闭）...\n";
        show_x11(fb, "Software Rasterizer");
    }
#endif

    return 0;
}
