#pragma once
#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdint>

// ─────────────────────────────────────────
//  Vec2
// ─────────────────────────────────────────
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
    Vec2 operator+(const Vec2& o) const { return {x+o.x, y+o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x-o.x, y-o.y}; }
    Vec2 operator*(float t)       const { return {x*t,   y*t};   }
};

// ─────────────────────────────────────────
//  Vec3
// ─────────────────────────────────────────
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float t)       const { return {x*t,   y*t,   z*t};   }
    Vec3 operator/(float t)       const { return {x/t,   y/t,   z/t};   }
    Vec3 operator*(const Vec3& o) const { return {x*o.x, y*o.y, z*o.z}; } // component-wise
    Vec3& operator+=(const Vec3& o){ x+=o.x; y+=o.y; z+=o.z; return *this; }

    float dot(const Vec3& o)  const { return x*o.x + y*o.y + z*o.z; }
    Vec3  cross(const Vec3& o)const {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }
    float length()  const { return std::sqrt(x*x + y*y + z*z); }
    Vec3  normalize()const {
        float l = length();
        if (l < 1e-8f) return {0,0,0};
        return {x/l, y/l, z/l};
    }
    Vec3 clamp01() const {
        return { std::max(0.f,std::min(1.f,x)),
                 std::max(0.f,std::min(1.f,y)),
                 std::max(0.f,std::min(1.f,z)) };
    }
    // negate
    Vec3 operator-() const { return {-x,-y,-z}; }
};

inline Vec3 mix(const Vec3& a, const Vec3& b, float t) {
    return a*(1-t) + b*t;
}

// ─────────────────────────────────────────
//  Vec4
// ─────────────────────────────────────────
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
    Vec4(const Vec3& v, float w) : x(v.x), y(v.y), z(v.z), w(w) {}

    Vec3 xyz() const { return {x, y, z}; }
    // perspective divide → NDC
    Vec3 perspective_divide() const { return {x/w, y/w, z/w}; }
};

// ─────────────────────────────────────────
//  Mat4  (column-major storage)
// ─────────────────────────────────────────
struct Mat4 {
    float m[4][4]; // m[col][row]

    Mat4() { for(auto& c:m) for(auto& v:c) v=0.f; }

    static Mat4 identity() {
        Mat4 r;
        r.m[0][0]=r.m[1][1]=r.m[2][2]=r.m[3][3]=1.f;
        return r;
    }

    // matrix * column-vector
    Vec4 operator*(const Vec4& v) const {
        return {
            m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z + m[3][0]*v.w,
            m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z + m[3][1]*v.w,
            m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z + m[3][2]*v.w,
            m[0][3]*v.x + m[1][3]*v.y + m[2][3]*v.z + m[3][3]*v.w
        };
    }

    // matrix * matrix
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for(int c=0;c<4;c++)
            for(int row=0;row<4;row++) {
                float s=0;
                for(int k=0;k<4;k++) s += m[k][row]*o.m[c][k];
                r.m[c][row]=s;
            }
        return r;
    }

    // ── builders ──

    static Mat4 translate(float tx, float ty, float tz) {
        Mat4 r = identity();
        r.m[3][0]=tx; r.m[3][1]=ty; r.m[3][2]=tz;
        return r;
    }
    static Mat4 scale(float sx, float sy, float sz) {
        Mat4 r = identity();
        r.m[0][0]=sx; r.m[1][1]=sy; r.m[2][2]=sz;
        return r;
    }
    static Mat4 rotateX(float a) {
        Mat4 r = identity();
        r.m[1][1]= std::cos(a); r.m[2][1]=-std::sin(a);
        r.m[1][2]= std::sin(a); r.m[2][2]= std::cos(a);
        return r;
    }
    static Mat4 rotateY(float a) {
        Mat4 r = identity();
        r.m[0][0]= std::cos(a); r.m[2][0]= std::sin(a);
        r.m[0][2]=-std::sin(a); r.m[2][2]= std::cos(a);
        return r;
    }
    static Mat4 rotateZ(float a) {
        Mat4 r = identity();
        r.m[0][0]= std::cos(a); r.m[1][0]=-std::sin(a);
        r.m[0][1]= std::sin(a); r.m[1][1]= std::cos(a);
        return r;
    }

    // lookAt (right-hand)
    static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = (center - eye).normalize();
        Vec3 r2 = f.cross(up).normalize();
        Vec3 u2 = r2.cross(f);
        Mat4 m = identity();
        m.m[0][0]=r2.x; m.m[1][0]=r2.y; m.m[2][0]=r2.z;
        m.m[0][1]=u2.x; m.m[1][1]=u2.y; m.m[2][1]=u2.z;
        m.m[0][2]=-f.x; m.m[1][2]=-f.y; m.m[2][2]=-f.z;
        m.m[3][0]=-r2.dot(eye);
        m.m[3][1]=-u2.dot(eye);
        m.m[3][2]= f.dot(eye);
        return m;
    }

    // perspective projection (right-hand, depth [-1,1])
    static Mat4 perspective(float fovY, float aspect, float zNear, float zFar) {
        float tanHalf = std::tan(fovY * 0.5f);
        Mat4 r;
        r.m[0][0] = 1.f / (aspect * tanHalf);
        r.m[1][1] = 1.f / tanHalf;
        r.m[2][2] = -(zFar + zNear) / (zFar - zNear);
        r.m[3][2] = -(2.f * zFar * zNear) / (zFar - zNear);
        r.m[2][3] = -1.f;
        r.m[3][3] = 0.f;
        return r;
    }

    Mat4 normal_matrix() const {
        // 提取 3x3 子矩阵
        float inv[3][3];
        float a00 = m[0][0], a01 = m[1][0], a02 = m[2][0];
        float a10 = m[0][1], a11 = m[1][1], a12 = m[2][1];
        float a20 = m[0][2], a21 = m[1][2], a22 = m[2][2];
        
        // 计算行列式
        float det = a00*(a11*a22 - a12*a21) 
                - a01*(a10*a22 - a12*a20)
                + a02*(a10*a21 - a11*a20);
        
        if (det == 0) return identity();
        float inv_det = 1.0f / det;
        
        // 计算伴随矩阵并转置（逆的转置）
        inv[0][0] = (a11*a22 - a12*a21) * inv_det;
        inv[0][1] = -(a10*a22 - a12*a20) * inv_det;
        inv[0][2] = (a10*a21 - a11*a20) * inv_det;
        inv[1][0] = -(a01*a22 - a02*a21) * inv_det;
        inv[1][1] = (a00*a22 - a02*a20) * inv_det;
        inv[1][2] = -(a00*a21 - a01*a20) * inv_det;
        inv[2][0] = (a01*a12 - a02*a11) * inv_det;
        inv[2][1] = -(a00*a12 - a02*a10) * inv_det;
        inv[2][2] = (a00*a11 - a01*a10) * inv_det;
        
        Mat4 r = identity();
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                r.m[i][j] = inv[j][i];  // 已经是逆的转置
        return r;
    }

    // normal matrix = transpose of inverse of upper-left 3x3
    Mat4 transpose3x3() const {
        Mat4 r = identity();
        for(int i=0;i<3;i++)
            for(int j=0;j<3;j++)
                r.m[i][j]=m[j][i];
        return r;
    }
};

// transform Vec3 as point (w=1)
inline Vec4 transform_point(const Mat4& m, const Vec3& v) {
    return m * Vec4(v, 1.f);
}
// transform Vec3 as direction (w=0)
inline Vec3 transform_dir(const Mat4& m, const Vec3& v) {
    Vec4 r = m * Vec4(v, 0.f);
    return r.xyz();
}

// ─────────────────────────────────────────
//  Color
// ─────────────────────────────────────────
struct Color {
    uint8_t r, g, b, a;
    Color() : r(0),g(0),b(0),a(255) {}
    Color(uint8_t cr,uint8_t cg,uint8_t cb,uint8_t ca=255):r(cr),g(cg),b(cb),a(ca){}
    // from [0,1] float
    static Color fromFloat(float r,float g,float b) {
        return Color(
            (uint8_t)(std::min(1.f,r)*255),
            (uint8_t)(std::min(1.f,g)*255),
            (uint8_t)(std::min(1.f,b)*255)
        );
    }
    static Color fromVec3(Vec3 v) {
        v = v.clamp01();
        return fromFloat(v.x, v.y, v.z);
    }
    Vec3 toFloat() const {
        return {r/255.f, g/255.f, b/255.f};
    }
};
