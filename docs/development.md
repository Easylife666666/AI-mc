# 开发与测试

> 功能说明见 [功能细节](features.md)，架构见 [架构](architecture.md)。

## 依赖

Arch Linux：

```bash
sudo pacman -S --needed pkgconf cmake ninja ccache gdb \
  sdl2 glew glm stb spdlog fmt nlohmann-json gtest \
  miniaudio zlib
```

Dear ImGui 与 FastNoiseLite 已 vendored 在 `third_party/`，可用
`scripts/fetch_thirdparty.sh` 重新下载。

## 构建与运行

```bash
cmake -S . -B build -G Ninja
cmake --build build -j$(nproc)

./build/aimc                         # 默认 1280x720，打开设置面板
./build/aimc --view 8 --seed 42 --new
./build/aimc --benchmark 10 --no-vsync
./build/aimc --shot shot.png --hidden --width 1280 --height 720 --no-hud
```

使用 ccache 的可选写法：

```bash
CCACHE_DIR=$PWD/.ccache cmake --build build -j$(nproc)
```

## 启动参数

```
--view N / --radius N     视距（区块），会覆盖 settings.json
--seed N --new            新世界
--load PATH --save PATH   指定存档
--time T --day-length N   时间 0..1 / 一天秒数（0 冻结）
--benchmark N             跑 N 秒输出平均帧率
--shot PATH --hidden      无头截图
--inventory --tab NAME    截图时打开物品栏（building / natural / nether / tools / search）
--no-vsync --no-hud --no-audio --verbose
```

截图/benchmark 建议在临时目录运行，避免把测试用的 `settings.json` 写进工作区：

```bash
mkdir -p /tmp/aimc-shot && cd /tmp/aimc-shot
/home/you/AI-mc/build/aimc --load /path/to/save.aimc --shot shot.png --hidden --no-hud
```

## 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接跑
./build/aimc_tests
```

当前 **68 个用例**，覆盖：

- 区块索引、负数坐标除法
- 地形确定性与高度范围、树木生成、地狱地形
- 面剔除/AO 网格、水/熔岩/传送门/火焰独立缓冲、图集 UV、视锥剔除
- DDA 射线（含传送门方块可命中）
- 流式生成 + 方块编辑
- 存档 v1–v4 兼容与设置往返
- 物品栏分类/搜索/滚动边界/热键栏放置与光标堆叠规则、物品堆叠上限
- 传送门框判定/点燃/落点建门、逐格破坏后残块仍可用
- 维度切换与编辑隔离、8:1 坐标换算
- 流体等级映射与高度、水/岩浆扩散与下落及干涸、岩浆主世界/地狱扩散距离、流动方块网格
- 地狱水蒸发、水岩反应（黑曜石/圆石）
- 打火石火焰的放置/寿命/水与支撑熄灭/网格
- 出水入水跳跃判定

## 性能测试

- 容器内是 **llvmpipe 软件渲染**：1280×720、视距 3 约 14 fps（天空全屏 shader 是主要开销）。
- 有硬件加速时（例如 Intel HD 620）预期 1080p 60fps；用 `--benchmark` 实测：

```bash
./build/aimc --benchmark 10 --no-vsync
```

- 设置面板打开 “adaptive view distance” 后，帧时间 > 22ms 会自动降视距（最低 3），
  < 13ms 时回升（最高 10）。
