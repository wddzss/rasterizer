#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include "math_utils.h"

// ─────────────────────────────────────────
//  TGA writer (uncompressed RGB/RGBA)
// ─────────────────────────────────────────
inline void write_tga(const std::string& path,
                      const uint8_t* data,
                      int w, int h, int channels = 3)
{
    std::ofstream f(path, std::ios::binary);
    if(!f) throw std::runtime_error("Cannot open file: " + path);

    uint8_t hdr[18] = {};
    hdr[2]  = 2;                          // uncompressed true-color
    hdr[12] = w & 0xFF;
    hdr[13] = (w >> 8) & 0xFF;
    hdr[14] = h & 0xFF;
    hdr[15] = (h >> 8) & 0xFF;
    hdr[16] = channels * 8;               // bits per pixel
    hdr[17] = 0x20;                       // top-left origin

    f.write(reinterpret_cast<char*>(hdr), 18);

    // TGA stores BGR(A)
    std::vector<uint8_t> row(w * channels);
    for(int y = 0; y < h; y++) {
        const uint8_t* src = data + y * w * channels;
        for(int x = 0; x < w; x++) {
            int si = x * channels;
            int di = x * channels;
            if(channels >= 3) {
                row[di+0] = src[si+2]; // B
                row[di+1] = src[si+1]; // G
                row[di+2] = src[si+0]; // R
                if(channels == 4) row[di+3] = src[si+3];
            } else {
                row[di] = src[si];
            }
        }
        f.write(reinterpret_cast<char*>(row.data()), row.size());
    }
}

// ─────────────────────────────────────────
//  Framebuffer + Z-buffer
// ─────────────────────────────────────────
class Framebuffer {
public:
    int width, height;
    std::vector<Color>  color_buf;
    std::vector<float>  depth_buf; // stores 1/w (perspective-correct)

    Framebuffer(int w, int h) : width(w), height(h),
        color_buf(w*h, Color(30,30,30)),
        depth_buf(w*h, -1e30f)           // -inf: nothing written yet
    {}

    void clear(Color bg = Color(30,30,30)) {
        std::fill(color_buf.begin(), color_buf.end(), bg);
        std::fill(depth_buf.begin(), depth_buf.end(), -1e30f);
    }

    // depth: we store 1/w from clip space; larger = closer to camera
    bool depth_test_and_write(int x, int y, float depth) {
        if(x<0||x>=width||y<0||y>=height) return false;
        int idx = y*width + x;
        if(depth > depth_buf[idx]) {
            depth_buf[idx] = depth;
            return true;
        }
        return false;
    }

    void set_color(int x, int y, Color c) {
        if(x<0||x>=width||y<0||y>=height) return;
        color_buf[y*width + x] = c;
    }

    // Save color buffer as TGA
    void save_tga(const std::string& path) const {
        std::vector<uint8_t> raw(width*height*3);
        for(int i=0;i<width*height;i++){
            raw[i*3+0] = color_buf[i].r;
            raw[i*3+1] = color_buf[i].g;
            raw[i*3+2] = color_buf[i].b;
        }
        write_tga(path, raw.data(), width, height, 3);
    }

    // Save depth as greyscale TGA for debugging
    void save_depth_tga(const std::string& path) const {
        float mn=1e30f, mx=-1e30f;
        for(auto v:depth_buf) { mn=std::min(mn,v); mx=std::max(mx,v); }
        std::vector<uint8_t> raw(width*height*3);
        float range = mx-mn; if(range<1e-6f) range=1.f;
        for(int i=0;i<width*height;i++){
            uint8_t g = (uint8_t)((depth_buf[i]-mn)/range*255);
            raw[i*3]=raw[i*3+1]=raw[i*3+2]=g;
        }
        write_tga(path, raw.data(), width, height, 3);
    }
};
