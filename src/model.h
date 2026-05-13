#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "math_utils.h"

struct Vertex {
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
};

struct Triangle {
    Vertex v[3];
};

struct Mesh {
    std::vector<Triangle> triangles;
    std::string name;
};

// ─────────────────────────────────────────
//  OBJ Loader
//  Supports: v, vt, vn, f (triangles and quads)
// ─────────────────────────────────────────
inline Mesh load_obj(const std::string& path) {
    Mesh mesh;
    mesh.name = path;

    std::vector<Vec3> positions;
    std::vector<Vec2> texcoords;
    std::vector<Vec3> normals;

    std::ifstream file(path);
    if(!file) {
        std::cerr << "[OBJ] Cannot open: " << path << "\n";
        return mesh;
    }

    std::string line;
    while(std::getline(file, line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if(token == "v") {
            float x,y,z; ss>>x>>y>>z;
            positions.push_back({x,y,z});
        } else if(token == "vt") {
            float u,v; ss>>u>>v;
            texcoords.push_back({u,v});
        } else if(token == "vn") {
            float x,y,z; ss>>x>>y>>z;
            normals.push_back({x,y,z});
        } else if(token == "f") {
            // Each vertex: idx_pos[/idx_uv[/idx_norm]]
            struct FaceVert { int pi=-1, ti=-1, ni=-1; };
            auto parse_fv = [](const std::string& s) -> FaceVert {
                FaceVert fv;
                std::istringstream ss2(s);
                std::string part;
                int k=0;
                while(std::getline(ss2, part, '/')) {
                    if(!part.empty()) {
                        int idx = std::stoi(part);
                        if(idx<0) { /* relative: handled below */ }
                        if(k==0) fv.pi=idx;
                        else if(k==1) fv.ti=idx;
                        else fv.ni=idx;
                    }
                    k++;
                }
                return fv;
            };

            std::vector<FaceVert> face;
            std::string tok;
            while(ss >> tok) face.push_back(parse_fv(tok));

            // Fan triangulation for polygons
            auto make_vertex = [&](const FaceVert& fv) -> Vertex {
                Vertex vert;
                // OBJ is 1-indexed, negative = relative
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

            for(int i=1;i+1<(int)face.size();i++) {
                Triangle tri;
                tri.v[0] = make_vertex(face[0]);
                tri.v[1] = make_vertex(face[i]);
                tri.v[2] = make_vertex(face[i+1]);
                mesh.triangles.push_back(tri);
            }
        }
    }

    // Compute flat normals for faces that have no vertex normals
    for(auto& tri : mesh.triangles) {
        bool missing = (tri.v[0].normal.length() < 1e-6f);
        if(missing) {
            Vec3 e1 = tri.v[1].pos - tri.v[0].pos;
            Vec3 e2 = tri.v[2].pos - tri.v[0].pos;
            Vec3 n  = e1.cross(e2).normalize();
            tri.v[0].normal = tri.v[1].normal = tri.v[2].normal = n;
        }
    }

    // Auto-generate UV if missing (spherical mapping)
    for(auto& tri : mesh.triangles) {
        for(int k=0;k<3;k++) {
            if(tri.v[k].uv.x == 0 && tri.v[k].uv.y == 0) {
                Vec3 n = tri.v[k].normal.normalize();
                float u = 0.5f + std::atan2(n.z, n.x) / (2.f * 3.14159265f);
                float v = 0.5f - std::asin(n.y) / 3.14159265f;
                tri.v[k].uv = {u, v};
            }
        }
    }

    std::cout << "[OBJ] Loaded: " << path
              << "  triangles=" << mesh.triangles.size() << "\n";
    return mesh;
}

// ─────────────────────────────────────────
//  Built-in procedural meshes
// ─────────────────────────────────────────

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
    // 6 faces
    struct Face { Vec3 n; Vec3 right; Vec3 up; };
    Face faces[6]={
        {{ 0, 0, 1},{1,0,0},{0,1,0}},  // front  +Z
        {{ 0, 0,-1},{-1,0,0},{0,1,0}}, // back   -Z
        {{ 1, 0, 0},{0,0,-1},{0,1,0}}, // right  +X
        {{-1, 0, 0},{0,0,1},{0,1,0}},  // left   -X
        {{ 0, 1, 0},{1,0,0},{0,0,-1}}, // top    +Y
        {{ 0,-1, 0},{1,0,0},{0,0,1}},  // bottom -Y
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
