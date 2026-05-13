#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include "math_utils.h"

class Texture {
public:
    int width = 0, height = 0;
    std::vector<uint8_t> data; // RGB, row-major top-to-bottom
    bool valid = false;

    Texture() {}

    // Load uncompressed TGA (RGB or RGBA → stored as RGB)
    bool load_tga(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if(!f) { return false; }

        uint8_t hdr[18];
        f.read(reinterpret_cast<char*>(hdr), 18);

        int id_len  = hdr[0];
        int cm_type = hdr[1]; // 0=no colormap
        int img_type= hdr[2]; // 2=uncompressed rgb
        width   = hdr[12] | (hdr[13]<<8);
        height  = hdr[14] | (hdr[15]<<8);
        int bpp = hdr[16];    // 24 or 32
        int origin = (hdr[17]>>4)&1; // 0=bottom-left, 1=top-left

        if((img_type!=2 && img_type!=10) || (bpp!=24&&bpp!=32)) {
            // Unsupported, try anyway
        }

        // skip image ID
        f.seekg(id_len, std::ios::cur);

        int channels = bpp/8;
        int npix = width * height;
        std::vector<uint8_t> raw(npix * channels);

        if(img_type == 2) { // uncompressed
            f.read(reinterpret_cast<char*>(raw.data()), raw.size());
        } else if(img_type == 10) { // RLE
            int idx=0;
            while(idx < npix) {
                uint8_t rep; f.read(reinterpret_cast<char*>(&rep),1);
                int count = (rep&0x7F)+1;
                if(rep & 0x80) { // run-length packet
                    uint8_t pix[4]; f.read(reinterpret_cast<char*>(pix),channels);
                    for(int i=0;i<count&&idx<npix;i++,idx++)
                        memcpy(&raw[idx*channels], pix, channels);
                } else {         // raw packet
                    f.read(reinterpret_cast<char*>(&raw[idx*channels]),count*channels);
                    idx+=count;
                }
            }
        }

        // Convert BGR(A) → RGB, flip if bottom-left origin
        data.resize(npix * 3);
        for(int y=0;y<height;y++) {
            int sy = (origin==0) ? (height-1-y) : y; // flip if bottom-left
            for(int x=0;x<width;x++) {
                int si = (sy*width+x)*channels;
                int di = (y*width+x)*3;
                data[di+0] = raw[si+2]; // R
                data[di+1] = raw[si+1]; // G
                data[di+2] = raw[si+0]; // B
            }
        }
        valid = true;
        return true;
    }

    // Generate a checkerboard procedural texture
    void make_checkerboard(int w, int h, Vec3 col_a={1,1,1}, Vec3 col_b={0,0,0}, int tiles=8) {
        width=w; height=h;
        data.resize(w*h*3);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            bool black = ((x*tiles/w)^(y*tiles/h))&1;
            Vec3 c = black ? col_b : col_a;
            int i=(y*w+x)*3;
            data[i+0]=(uint8_t)(c.x*255);
            data[i+1]=(uint8_t)(c.y*255);
            data[i+2]=(uint8_t)(c.z*255);
        }
        valid=true;
    }

    // Generate UV grid texture
    void make_uv_grid(int w, int h) {
        width=w; height=h;
        data.resize(w*h*3);
        for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
            float u=(float)x/w, v=(float)y/h;
            // grid lines
            bool gx = (x%(w/8))<2;
            bool gy = (y%(h/8))<2;
            Vec3 c;
            if(gx||gy) c={0.1f,0.1f,0.1f};
            else c={u, v, 0.5f};
            int i=(y*w+x)*3;
            data[i+0]=(uint8_t)(c.x*255);
            data[i+1]=(uint8_t)(c.y*255);
            data[i+2]=(uint8_t)(c.z*255);
        }
        valid=true;
    }

    // Bilinear sample, UV in [0,1] with repeat wrap
    Vec3 sample(float u, float v) const {
        if(!valid || data.empty()) return {1,0,1}; // magenta = missing
        // repeat
        u = u - std::floor(u);
        v = v - std::floor(v);
        float px = u*(width-1);
        float py = v*(height-1);
        int x0=(int)px, y0=(int)py;
        int x1=std::min(x0+1,width-1);
        int y1=std::min(y0+1,height-1);
        float tx=px-x0, ty=py-y0;

        auto fetch=[&](int x,int y)->Vec3{
            int i=(y*width+x)*3;
            return {data[i]/255.f, data[i+1]/255.f, data[i+2]/255.f};
        };
        Vec3 c00=fetch(x0,y0), c10=fetch(x1,y0);
        Vec3 c01=fetch(x0,y1), c11=fetch(x1,y1);
        Vec3 c0 = c00*(1-tx)+c10*tx;
        Vec3 c1 = c01*(1-tx)+c11*tx;
        return c0*(1-ty)+c1*ty;
    }

    // Nearest neighbour (fast)
    Vec3 sample_nearest(float u, float v) const {
        if(!valid||data.empty()) return {1,0,1};
        u=u-std::floor(u); v=v-std::floor(v);
        int x=(int)(u*width)  %width;
        int y=(int)(v*height) %height;
        int i=(y*width+x)*3;
        return {data[i]/255.f, data[i+1]/255.f, data[i+2]/255.f};
    }
};
