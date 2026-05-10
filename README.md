# Screen-Space Fluid Rendering with Curvature Flow

基于屏幕空间的流体渲染系统，使用曲率流（Curvature Flow）进行流体表面平滑。本项目参照论文 *Screen Space Fluid Rendering with Curvature Flow* 实现，作为 CS7GV3 课程作业。

## 渲染管线

本系统采用纯屏幕空间的流体渲染管线，无需三维网格重建：

| 阶段 | 说明 |
|------|------|
| **深度渲染 (Depth Pass)** | 将粒子以球体点精灵形式渲染到深度缓冲区 |
| **厚度渲染 (Thickness Pass)** | 使用累加混合计算流体的厚度图 |
| **表面平滑 (Smoothing)** | 对深度图进行迭代平滑，三种可选算法 |
| **噪声细节 (Noise Pass)** | 使用 Simplex 噪声为表面添加亚粒子级细节 |
| **合成渲染 (Composite Pass)** | 最终合成：折射、透明度、菲涅尔、高光、泡沫 |

### 平滑算法对比

| 算法 | 描述 | 特点 |
|------|------|------|
| **曲率流 (Curvature Flow)** | 基于曲率的各向异性扩散 | 保留流体体积，边缘保持好，需调节迭代步长 |
| **高斯模糊 (Gaussian Blur)** | 标准 5×5 高斯核卷积 | 简单快速，但会模糊边缘细节 |
| **双边滤波 (Bilateral Blur)** | 空间距离 + 深度差异联合加权 | 边缘保持优于高斯模糊 |

## 功能特性

- 粒子物理模拟（重力、碰撞、反弹）
- 三种表面平滑算法实时切换
- 屏幕空间折射效果
- 基于比尔-朗伯定律的透明度吸收
- 菲涅尔效应与高光反射
- 泡沫效果（基于粒子叠加噪声）
- HDR 环境贴图天空盒
- ImGui 实时参数调节面板

## 构建与运行

### 环境要求

- Windows 10/11
- Visual Studio 2019/2022
- OpenGL 3.3 及以上
- 支持 GLSL 330 的 GPU

### 依赖库

| 库 | 用途 |
|---|---|
| GLFW | 窗口管理与输入 |
| GLAD | OpenGL 函数加载 |
| GLM | 数学运算 |
| Assimp | 模型加载 |
| stb_image | 纹理加载 |
| Dear ImGui | 调试界面 |

### 构建步骤

1. 使用 Visual Studio 打开 `fluidRendering.sln`
2. 确保所有依赖库路径正确配置
3. 选择 `x64 | Debug` 配置
4. 生成解决方案并运行

### 资源文件

运行时需要以下资源文件在项目根目录：

- `story_studio_01_4k.hdr` — HDR 环境贴图（天空盒）
- `waters-pipe.fbx` — 水管模型
- `water-pipe.gltf` — 水管模型（备用）
- `edur681_banded_slate.glb` — 石板模型
- `textures/T_DefaultMaterial_BaseColor.png` — 水管基础色贴图
- `textures/T_DefaultMaterial_Metallic.png` — 金属度贴图
- `textures/T_DefaultMaterial_Roughness.png` — 粗糙度贴图
- `textures/T_DefaultMaterial_AO.png` — 环境光遮蔽贴图
- `glfw3.dll` — GLFW 运行时库

## 操作控制

| 按键 | 功能 |
|------|------|
| WASD | 摄像机移动 |
| 鼠标 | 视角旋转 |
| Tab | 切换鼠标锁定/释放 |
| Space | 手动发射粒子 |
| Esc | 退出程序 |

右侧 ImGui 面板提供实时参数调节。

## 参考

- [Screen Space Fluid Rendering with Curvature Flow](1507149.1507164.pdf) — 本项目的核心参考论文
- CS7GV3_A5_Report_Kehan.pdf — 课程项目报告

## 许可

本项目仅用于教育目的。
