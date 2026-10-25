# AI-mc

> [!NOTE]
> 本项目由 **DeepSeek-V4.1-Flash** 模型与 **DeepSeek Harness** 智能体协作完成。
> Built with the **DeepSeek-V4.1-Flash** model and the **DeepSeek Harness** agent.

一个用 **C++20 + OpenGL 3.3** 从零搭的体素沙盒（Minecraft 风格），不依赖游戏引擎。
SDL2 负责窗口/输入，Dear ImGui 做调试 UI，FastNoiseLite 生成地形，miniaudio 做程序化音效，
zlib 压缩存档。

![主世界远景](docs/m2.png)

**状态**：M0–M10 完成，当前 **68 个测试**通过。流式世界、后台网格、玩家物理、创造物品栏、
传送门/地狱、流动水/岩浆、火焰都可玩。

## 亮点

- `16 x 64 x 16` 区块流式加载，TBB 后台生成/建网格，主线程只做 GL 上传
- 程序化 8×8 图集、顶点 AO、视锥剔除、昼夜/星空、半透明水/传送门
- 创造模式物品栏 + 物品系统 + 打火石 + 黑曜石传送门 + 可往返的地狱
- 水/岩浆 20 Hz 流动：下落、扩散、1/8 格高度、地狱蒸发、水岩反应
- AABB 物理、游泳/飞行、DDA 破坏/放置/取色、zlib 分维度存档

## 快速开始

Arch Linux：

```bash
sudo pacman -S --needed pkgconf cmake ninja ccache gdb \
  sdl2 glew glm stb spdlog fmt nlohmann-json gtest miniaudio zlib
cmake -S . -B build -G Ninja
cmake --build build -j$(nproc)
./build/aimc
```

常用操作：

- `WASD` 移动（双击 `W` 奔跑）、鼠标视角
- 左/右键 破坏/放置，中键取色，`E` 打开创造物品栏
- 手持打火石右键点燃传送门或地面火焰，左键点水源可移除源头
- `F1` 设置、`F5`/`F9` 存档/读档、`Esc` 释放鼠标/退出

启动时自动读取 `save.aimc`，`--new` 开新世界。完整按键、启动参数、截图和 benchmark 见
[操作与按键](docs/controls.md) 与 [开发与测试](docs/development.md)。

## 文档

- [功能细节](docs/features.md) — 世界、玩法、传送门/地狱、流体、火焰、存档
- [操作与按键](docs/controls.md) — 完整按键表
- [开发与测试](docs/development.md) — 依赖、构建、CLI、测试、性能
- [架构与存档格式](docs/architecture.md) — 源码结构、线程模型、方块更新、性能
- [已知限制与路线图](docs/roadmap.md)
- [截图](docs/screenshots.md)

## 许可证

MIT，见 [LICENSE](LICENSE)。`third_party/` 中的 Dear ImGui 与 FastNoiseLite 遵循各自许可；
SDL2、GLEW、GLM、miniaudio、spdlog、zlib 等系统库遵循各自许可。
