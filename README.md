# 软件光栅化渲染器
## 图形学与虚拟现实课程大作业

---

## 功能特性

| 功能 | 说明 |
|------|------|
| MVP变换 | 模型/视图/投影矩阵，完整坐标变换流水线 |
| 背面剔除 | 屏幕空间叉积检测，剔除背向三角形 |
| Z-buffer | 透视正确深度测试，基于1/w插值 |
| 透视正确插值 | 所有属性（法线/UV/世界坐标）均做透视除法校正 |
| Blinn-Phong光照 | 环境光+漫反射+镜面反射，支持多光源+距离衰减 |
| 纹理映射 | 双线性插值采样，支持TGA纹理加载 |
| Gamma校正 | 线性空间渲染→sRGB输出 |
| OBJ加载 | 支持v/vt/vn/f（三角面和多边形自动三角化） |
| 内置几何体 | 球体、立方体、平面（程序化生成） |
| 程序化纹理 | 棋盘格、UV网格（无需外部纹理文件） |
| 多物体场景 | 4个物体+3个光源的完整场景 |
| TGA输出 | 纯C++实现，无第三方库依赖 |
| 动画序列 | 可渲染多帧相机旋转动画 |
| X11预览 | Linux下可编译窗口实时预览 |

---

## 编译环境

- **语言**: C++17
- **编译器**: g++ 7.0+（或 clang++ 5.0+）
- **依赖**: 无第三方库（可选 X11 用于窗口预览）
- **平台**: Linux / Windows（WSL）/ macOS

---

## 编译方法

### Linux / 虚拟机（推荐）

```bash
# 克隆/解压项目后进入目录
cd rasterizer

# 编译（自动检测X11）
make

# 或手动编译（无X11依赖）
g++ -std=c++17 -O2 -o rasterizer main.cpp
```

### Windows（MinGW/MSYS2）

```bash
g++ -std=c++17 -O2 -o rasterizer.exe main.cpp
```

### macOS

```bash
g++ -std=c++17 -O2 -o rasterizer main.cpp
```

---

## 运行方法

```bash
# 1. 创建输出目录
mkdir -p output textures

# 2. 单帧渲染（最快）
./rasterizer

# 3. 指定分辨率
./rasterizer --width 1280 --height 720

# 4. 渲染36帧旋转动画
./rasterizer --width 800 --height 600 --frames 36

# 5. 加载自定义OBJ模型
./rasterizer --obj models/your_model.obj

# 6. Linux X11窗口预览
./rasterizer --window

# 7. 查看帮助
./rasterizer --help
```

---

## 输出文件

- `output/frame_NNNN.tga` — 渲染帧（TGA格式，可用 GIMP/IrfanView/任意图片查看器打开）
- `output/depth_last.tga` — 最后一帧的深度缓冲可视化

### 查看 TGA 文件

```bash
# Linux：安装 eog 或 gimp
eog output/frame_0000.tga
gimp output/frame_0000.tga

# 或转换为PNG（需要ImageMagick）
convert output/frame_0000.tga output/frame_0000.png

# 或用Python/PIL批量转换
python3 convert_png.py
```

---

## 纹理说明

程序内置程序化纹理，无需准备纹理文件即可运行。

若需使用自定义纹理：
1. 将纹理文件转换为**未压缩TGA格式**（RGB 24位，GIMP可导出）
2. 放置到 `textures/` 目录：
   - `textures/diffuse.tga`  — 漫反射贴图
   - `textures/specular.tga` — 镜面反射贴图

---

## 代码结构

```
rasterizer/
├── main.cpp              # 主程序：场景定义、相机、渲染循环
├── Makefile              # 编译脚本
├── src/
│   ├── math_utils.h      # Vec2/Vec3/Vec4/Mat4/Color 数学库
│   ├── framebuffer.h     # 帧缓冲 + Z缓冲 + TGA输出
│   ├── texture.h         # 纹理加载（TGA）+ 双线性采样
│   ├── model.h           # OBJ加载 + 内置几何体
│   └── rasterizer.h      # 核心光栅化器：顶点着色器/三角形扫描/片段着色
├── textures/             # 纹理文件目录
├── models/               # OBJ模型目录
└── output/               # 渲染输出目录
```

---

## 渲染管线流程

```
顶点数据 (Vertex)
    ↓
顶点着色器 (vertex_shader)
  · 模型变换: pos_world = Model * pos_local
  · 法线变换: norm_world = Model_inv_T * norm_local
  · 投影变换: clip_pos = MVP * pos_local
    ↓
背面剔除 (Back-face culling)
  · 屏幕空间叉积: cross2d > 0 → 正面
    ↓
光栅化 (Rasterize)
  · NDC → 屏幕坐标
  · 包围盒遍历
  · 重心坐标测试 (Barycentric)
    ↓
Z-buffer 深度测试
  · 存储 1/w，值越大越近
    ↓
透视正确插值 (Perspective-correct)
  · attr_corr = (Σ attr_i/w_i * λ_i) / (Σ 1/w_i * λ_i)
    ↓
片段着色器 = Blinn-Phong
  · 漫反射纹理采样（双线性）
  · 环境光 + Σ 光源(漫反射 + 镜面反射)
  · 距离衰减: att = I / (1 + k1*d + k2*d²)
    ↓
Gamma校正 (linear → sRGB)
  · out = pow(color, 1/2.2)
    ↓
写入帧缓冲 → TGA输出
```

---

## 自己独立设计的部分（代码标注）

以下为本人独立设计实现（在代码中已用注释标注 `/* 自设计 */`）：

1. **透视正确插值** (`rasterizer.h` rasterize_triangle函数)
   - 基于1/w的属性插值，保证纹理映射在透视投影下正确
   
2. **多光源Blinn-Phong着色** (`rasterizer.h` blinn_phong函数)
   - 3个异色光源（主光/补光/轮廓光），距离二次衰减模型
   
3. **程序化纹理生成** (`texture.h` make_checkerboard / make_uv_grid)
   - 棋盘格和UV网格，无需外部资源
   
4. **多物体场景设计** (`main.cpp` render_scene)
   - 4个物体（OBJ/球/立方体/平面），相机环绕轨道动画
   
5. **TGA文件读写** (`framebuffer.h` write_tga / `texture.h` load_tga)
   - 纯C++实现，支持压缩(RLE)和非压缩格式
