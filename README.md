# AI-mc

一个用 **C++20 + OpenGL 3.3** 从零搭的体素沙盒（Minecraft 风格），不依赖任何游戏引擎。窗口/输入用 SDL2，ImGui 做调试面板，FastNoiseLite 生成地形。

当前进度：**M0 + M1 完成** —— 可自由飞行的无限风格地形（有限世界，M2 转流式），程序化贴图，面剔除网格，雾，帧率面板。

![M1 截图](docs/m1.png)

## 依赖

Arch Linux：

```bash
sudo pacman -S --needed pkgconf cmake ninja ccache gdb \
  sdl2 glew glm stb spdlog fmt gtest
```

Dear ImGui 与 FastNoiseLite 已 vendored 在 `third_party/`（可用 `scripts/fetch_thirdparty.sh` 重新下载）。

## 构建

```bash
cmake -S . -B build -G Ninja
cmake --build build          # 可加 -j$(nproc)
```

## 运行

```bash
./build/aimc                  # 1280x720，自由飞行
./build/aimc --radius 6       # 更大的世界
./build/aimc --seed 42        # 换地形
```

控制：`WASD` 移动，`Space / Ctrl` 上升下降，`Shift` 加速，`F1` 开关调试面板，`Esc` 退出。

截图（无头，隐藏窗口）：

```bash
./build/aimc --shot shot.png --hidden --width 960 --height 540
```

性能测量（建议关 vsync）：

```bash
./build/aimc --benchmark 5 --no-vsync
```

全部参数见 `./build/aimc --help`。

## 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接 ./build/aimc_tests
```

现有 10 个用例：区块索引、负数坐标除法、地形确定性/高度范围、面剔除（单方块 6 面、相邻方块 10 面、不透明邻居剔除）、图集 UV、世界方块查询。

## 目录

```
src/core       数学工具、日志
src/platform   SDL2 窗口与 GL 上下文
src/render     shader、程序化图集、区块网格、相机、截图
src/world      方块表、16x64x16 区块、噪声地形、世界与绘制提交
src/ui         ImGui 调试面板
tests          gtest
third_party    Dear ImGui 1.92.4、FastNoiseLite
```

## 实现要点

- 区块 `16 x 64 x 16`，`uint8` 方块 id，世界坐标直接烘进顶点，**每个区块一次 draw call**。
- 网格构建只发生暴露面：同种方块相邻剔除（水-水），不透明邻居剔除。
- 纹理是**代码生成的 4x4 / 16px 图集**，零美术资源；UV 带半像素内缩防渗色。
- 地形：FastNoiseLite OpenSimplex2 + 4 层 fBm，海平面 20，沙滩/草地/水。
- 雾 + 天空色清屏，遮住有限世界边缘（M2 会换成流式加载）。
- 相机为自由飞行，M3 会换成带 AABB 碰撞的玩家。

## 已知限制（后续里程碑）

- 没有树/植被/生物群系（M2 地形特征）
- 水是不透明的，没有半透明排序（M4）
- 世界是有限块状区域，没有流式加载与视锥剔除（M2）
- 没有顶点 AO、没有 mipmap（M2/M4）
- 没有存档（M5）

## 路线图

- [x] M0 窗口 + 主循环 + ImGui + 截图
- [x] M1 区块网格 + 程序化图集 + 自由相机 + 雾
- [ ] M2 流式区块加载 + TBB 后台网格构建 + 视锥剔除 + 树
- [ ] M3 玩家物理 / 碰撞 / DDA 拾取 / 破坏放置
- [ ] M4 顶点 AO + 天空昼夜 + 音频
- [ ] M5 存档（zlib）+ 设置面板 + 性能自适应
