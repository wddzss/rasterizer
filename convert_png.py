#!/usr/bin/env python3
"""
将 output/ 目录下的所有 TGA 文件转换为 PNG（需要 Pillow）
安装: pip install Pillow
用法: python3 convert_png.py
"""
import os, sys

try:
    from PIL import Image
except ImportError:
    print("请先安装 Pillow: pip install Pillow  或  pip3 install Pillow")
    sys.exit(1)

def read_tga(path):
    with open(path, 'rb') as f:
        hdr = f.read(18)
        id_len  = hdr[0]
        img_type= hdr[2]
        w = hdr[12] | (hdr[13] << 8)
        h = hdr[14] | (hdr[15] << 8)
        bpp = hdr[16]
        origin = (hdr[17] >> 4) & 1   # 1 = top-left

        f.seek(id_len, 1)
        ch = bpp // 8
        raw = bytearray(f.read(w * h * ch))

    img = Image.new('RGB', (w, h))
    pixels = []
    for y in range(h):
        sy = y if origin == 1 else (h - 1 - y)
        for x in range(w):
            i = (sy * w + x) * ch
            b, g, r = raw[i], raw[i+1], raw[i+2]
            pixels.append((r, g, b))
    img.putdata(pixels)
    return img

out_dir = 'output'
if not os.path.isdir(out_dir):
    print(f"目录 '{out_dir}' 不存在，请先运行渲染器")
    sys.exit(1)

tgas = sorted(f for f in os.listdir(out_dir) if f.endswith('.tga'))
if not tgas:
    print("没有找到 TGA 文件")
    sys.exit(0)

print(f"找到 {len(tgas)} 个 TGA 文件，开始转换...")
for name in tgas:
    src = os.path.join(out_dir, name)
    dst = os.path.join(out_dir, name.replace('.tga', '.png'))
    img = read_tga(src)
    img.save(dst)
    print(f"  {src} → {dst}")

# 如果有多帧，生成 GIF
frame_files = sorted(
    [os.path.join(out_dir, f) for f in os.listdir(out_dir)
     if f.startswith('frame_') and f.endswith('.tga')]
)
if len(frame_files) > 1:
    frames = [read_tga(f) for f in frame_files]
    gif_path = os.path.join(out_dir, 'animation.gif')
    frames[0].save(gif_path, save_all=True, append_images=frames[1:],
                   duration=80, loop=0)
    print(f"\n动画 GIF 已保存: {gif_path}")

print("\n转换完成！")
