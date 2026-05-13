// model.h - 完整替换
#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include "math_utils.h"

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
};

struct Triangle {
    Vertex v[3];
    int material_id = -1;  // 材质ID
};

// MTL 材质
struct MtlMaterial {
    std::string name;
    Vec3 ambient  = {0.2f, 0.2f, 0.2f};
    Vec3 diffuse  = {0.8f, 0.8f, 0.8f};
    Vec3 specular = {0.5f, 0.5f, 0.5f};
    float shininess = 32.0f;
    float alpha = 1.0f;
    std::string diffuse_map;
    std::string specular_map;
    std::string normal_map;
};

struct Mesh {
    std::vector<Triangle> triangles;
    std::vector<MtlMaterial> materials;
    std::string name;
};

// ─────────────────────────────────────────
//  MTL Loader
// ─────────────────────────────────────────
inline std::vector<MtlMaterial> load_mtl(const std::string& path) {
    std::vector<MtlMaterial> materials;
    std::ifstream file(path);
    if(!file) {
        std::cerr << "[MTL] Cannot open: " << path << "\n";
        return materials;
    }
    
    MtlMaterial current;
    bool has_current = false;
    std::string line;
    
    while(std::getline(file, line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        
        if(token == "newmtl") {
            if(has_current) materials.push_back(current);
            current = MtlMaterial();
            ss >> current.name;
            has_current = true;
        }
        else if(token == "Ka") {
            float r,g,b; ss >> r >> g >> b;
            current.ambient = {r,g,b};
        }
        else if(token == "Kd") {
            float r,g,b; ss >> r >> g >> b;
            current.diffuse = {r,g,b};
        }
        else if(token == "Ks") {
            float r,g,b; ss >> r >> g >> b;
            current.specular = {r,g,b};
        }
        else if(token == "Ns") {
            ss >> current.shininess;
        }
        else if(token == "d" || token == "Tr") {
            ss >> current.alpha;
        }
        else if(token == "map_Kd") {
            ss >> current.diffuse_map;
        }
        else if(token == "map_Ks") {
            ss >> current.specular_map;
        }
        else if(token == "map_Bump" || token == "map_bump" || token == "norm") {
            ss >> current.normal_map;
        }
    }
    if(has_current) materials.push_back(current);
    
    std::cout << "[MTL] Loaded " << materials.size() << " materials from " << path << "\n";
    return materials;
}

// ─────────────────────────────────────────
//  OBJ Loader with MTL support
// ─────────────────────────────────────────
inline Mesh load_obj(const std::string& path) {
    Mesh mesh;
    mesh.name = path;
    
    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;
    std::vector<int> material_ids;
    
    // 找对应的 MTL 文件
    std::string mtl_path;
    std::string base_dir = path.substr(0, path.find_last_of("/\\") + 1);
    
    std::ifstream file(path);
    if(!file) {
        std::cerr << "[OBJ] Cannot open: " << path << "\n";
        return mesh;
    }
    
    // 当前材质
    int current_material = -1;
    
    // 临时存储面数据
    struct FaceVert { int pi=-1, ti=-1, ni=-1; };
    std::vector<std::vector<FaceVert>> face_vertices;
    int face_material = -1;
    
    std::string line;
    while(std::getline(file, line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;
        
        if(token == "mtllib") {
            std::string mtl_filename;
            ss >> mtl_filename;
            mtl_path = base_dir + mtl_filename;
            mesh.materials = load_mtl(mtl_path);
        }
        else if(token == "usemtl") {
            // 先提交之前的面
            if(!face_vertices.empty()) {
                for(auto& face : face_vertices) {
                    for(int i=1; i+1<(int)face.size(); i++) {
                        Triangle tri;
                        auto make_vertex = [&](const FaceVert& fv) -> Vertex {
                            Vertex vert;
                            int pi = fv.pi > 0 ? fv.pi-1 : (int)positions.size()+fv.pi;
                            if(pi>=0 && pi<(int)positions.size()) vert.pos = positions[pi];
                            if(fv.ti != -1) {
                                int ti = fv.ti > 0 ? fv.ti-1 : (int)texcoords.size()+fv.ti;
                                if(ti>=0 && ti<(int)texcoords.size()) vert.uv = texcoords[ti];
                            }
                            if(fv.ni != -1) {
                                int ni = fv.ni > 0 ? fv.ni-1 : (int)normals.size()+fv.ni;
                                if(ni>=0 && ni<(int)normals.size()) vert.normal = normals[ni];
                            }
                            return vert;
                        };
                        tri.v[0] = make_vertex(face[0]);
                        tri.v[1] = make_vertex(face[i]);
                        tri.v[2] = make_vertex(face[i+1]);
                        tri.material_id = face_material;
                        mesh.triangles.push_back(tri);
                    }
                }
                face_vertices.clear();
            }
            
            std::string mtl_name;
            ss >> mtl_name;
            face_material = -1;
            for(size_t i=0; i<mesh.materials.size(); i++) {
                if(mesh.materials[i].name == mtl_name) {
                    face_material = (int)i;
                    break;
                }
            }
        }
        else if(token == "v") {
            float x,y,z; ss>>x>>y>>z;
            positions.push_back({x,y,z});
        }
        else if(token == "vt") {
            float u,v; ss>>u>>v;
            texcoords.push_back({u, 1.0f - v});  // flip V for OpenGL style
        }
        else if(token == "vn") {
            float x,y,z; ss>>x>>y>>z;
            normals.push_back({x,y,z});
        }
        else if(token == "f") {
            auto parse_fv = [](const std::string& s) -> FaceVert {
                FaceVert fv;
                std::istringstream ss2(s);
                std::string part;
                int k=0;
                while(std::getline(ss2, part, '/')) {
                    if(!part.empty()) {
                        int idx = std::stoi(part);
                        if(k==0) fv.pi = idx;
                        else if(k==1) fv.ti = idx;
                        else if(k==2) fv.ni = idx;
                    }
                    k++;
                }
                return fv;
            };
            
            std::vector<FaceVert> face;
            std::string tok;
            while(ss >> tok) face.push_back(parse_fv(tok));
            face_vertices.push_back(face);
        }
    }
    
    // 处理最后一批面
    if(!face_vertices.empty()) {
        for(auto& face : face_vertices) {
            for(int i=1; i+1<(int)face.size(); i++) {
                Triangle tri;
                auto make_vertex = [&](const FaceVert& fv) -> Vertex {
                    Vertex vert;
                    int pi = fv.pi > 0 ? fv.pi-1 : (int)positions.size()+fv.pi;
                    if(pi>=0 && pi<(int)positions.size()) vert.pos = positions[pi];
                    if(fv.ti != -1) {
                        int ti = fv.ti > 0 ? fv.ti-1 : (int)texcoords.size()+fv.ti;
                        if(ti>=0 && ti<(int)texcoords.size()) vert.uv = texcoords[ti];
                    }
                    if(fv.ni != -1) {
                        int ni = fv.ni > 0 ? fv.ni-1 : (int)normals.size()+fv.ni;
                        if(ni>=0 && ni<(int)normals.size()) vert.normal = normals[ni];
                    }
                    return vert;
                };
                tri.v[0] = make_vertex(face[0]);
                tri.v[1] = make_vertex(face[i]);
                tri.v[2] = make_vertex(face[i+1]);
                tri.material_id = face_material;
                mesh.triangles.push_back(tri);
            }
        }
    }
    
    // 计算缺失的法线
    for(auto& tri : mesh.triangles) {
        bool missing = (tri.v[0].normal.length() < 1e-6f) ||
                       (tri.v[1].normal.length() < 1e-6f) ||
                       (tri.v[2].normal.length() < 1e-6f);
        if(missing) {
            Vec3 e1 = tri.v[1].pos - tri.v[0].pos;
            Vec3 e2 = tri.v[2].pos - tri.v[0].pos;
            Vec3 n  = e1.cross(e2).normalize();
            tri.v[0].normal = tri.v[1].normal = tri.v[2].normal = n;
        }
    }
    
    // 自动生成 UV
    for(auto& tri : mesh.triangles) {
        for(int k=0;k<3;k++) {
            if(tri.v[k].uv.x == 0 && tri.v[k].uv.y == 0 && 
               tri.v[k].uv.x == tri.v[k].uv.y) {
                Vec3 n = tri.v[k].normal.normalize();
                float u = 0.5f + std::atan2(n.z, n.x) / (2.f * 3.14159265f);
                float v = 0.5f - std::asin(n.y) / 3.14159265f;
                tri.v[k].uv = {u, v};
            }
        }
    }
    
    std::cout << "[OBJ] Loaded: " << path
              << "  triangles=" << mesh.triangles.size()
              << "  materials=" << mesh.materials.size() << "\n";
    return mesh;
}

// ── 以下为内置几何体函数 (保持不变) ──
inline Mesh make_sphere(int stacks=32, int slices=32, float radius=1.f) {
    Mesh mesh; mesh.name="sphere";
    const float PI = 3.14159265f;
    for(int i=0;i<stacks;i++) {
        float phi0 = PI*(float)i/stacks - PI/2;
        float phi1 = PI*(float)(i+1)/stacks - PI/2;
        for(int j=0;j<slices;j++) {
            float th0 = 2*PI*(float)j/slices;
            float th1 = 2*PI*(float)(j+1)/slices;
            auto vtx=[&](float th,float phi)->Vertex{
                Vertex v;
                v.normal = {std::cos(phi)*std::cos(th),
                            std::sin(phi),
                            std::cos(phi)*std::sin(th)};
                v.pos    = v.normal * radius;
                v.uv     = {th/(2*PI), (phi+PI/2)/PI};
                return v;
            };
            auto p00=vtx(th0,phi0),p10=vtx(th1,phi0),
                 p01=vtx(th0,phi1),p11=vtx(th1,phi1);
            Triangle t1,t2;
            t1.v[0]=p00; t1.v[1]=p10; t1.v[2]=p11;
            t2.v[0]=p00; t2.v[1]=p11; t2.v[2]=p01;
            mesh.triangles.push_back(t1);
            mesh.triangles.push_back(t2);
        }
    }
    return mesh;
}

inline Mesh make_cube(float size=1.f) {
    Mesh mesh; mesh.name="cube";
    float h=size/2;
    struct Face { Vec3 n; Vec3 right; Vec3 up; };
    Face faces[6]={
        {{ 0, 0, 1},{1,0,0},{0,1,0}},
        {{ 0, 0,-1},{-1,0,0},{0,1,0}},
        {{ 1, 0, 0},{0,0,-1},{0,1,0}},
        {{-1, 0, 0},{0,0,1},{0,1,0}},
        {{ 0, 1, 0},{1,0,0},{0,0,-1}},
        {{ 0,-1, 0},{1,0,0},{0,0,1}},
    };
    Vec2 uvs[4]={{0,0},{1,0},{1,1},{0,1}};
    for(auto& f:faces){
        Vec3 ctr = f.n*h;
        Vec3 p[4];
        p[0]=ctr+(-f.right-f.up)*h;
        p[1]=ctr+( f.right-f.up)*h;
        p[2]=ctr+( f.right+f.up)*h;
        p[3]=ctr+(-f.right+f.up)*h;
        auto mk=[&](int i)->Vertex{
            Vertex v; v.pos=p[i]; v.normal=f.n; v.uv=uvs[i]; return v;
        };
        Triangle t1,t2;
        t1.v[0]=mk(0);t1.v[1]=mk(1);t1.v[2]=mk(2);
        t2.v[0]=mk(0);t2.v[1]=mk(2);t2.v[2]=mk(3);
        mesh.triangles.push_back(t1);
        mesh.triangles.push_back(t2);
    }
    return mesh;
}

inline Mesh make_plane(float size=5.f, int divs=1) {
    Mesh mesh; mesh.name="plane";
    float step=size*2/divs;
    for(int i=0;i<divs;i++) for(int j=0;j<divs;j++){
        float x0=-size+j*step, x1=x0+step;
        float z0=-size+i*step, z1=z0+step;
        float u0=(float)j/divs, u1=(float)(j+1)/divs;
        float v0=(float)i/divs, v1=(float)(i+1)/divs;
        Vec3 n={0,1,0};
        auto mk=[&](float x,float z,float u,float v)->Vertex{
            Vertex vt; vt.pos={x,0,z}; vt.normal=n; vt.uv={u,v}; return vt;
        };
        Triangle t1,t2;
        t1.v[0]=mk(x0,z0,u0,v0); t1.v[1]=mk(x1,z0,u1,v0); t1.v[2]=mk(x1,z1,u1,v1);
        t2.v[0]=mk(x0,z0,u0,v0); t2.v[1]=mk(x1,z1,u1,v1); t2.v[2]=mk(x0,z1,u0,v1);
        mesh.triangles.push_back(t1);
        mesh.triangles.push_back(t2);
    }
    return mesh;
}