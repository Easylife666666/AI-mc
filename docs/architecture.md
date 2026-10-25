# 架构

> 功能见 [功能细节](features.md)，构建见 [开发与测试](development.md)。

## 源码结构

```
src/audio      miniaudio 设备 + 程序化音效合成
src/core       数学工具 / 日志
src/game       Player（AABB 物理）、DDA 射线、Settings、Creative（物品栏规则）、Item（物品定义）
src/platform   SDL2 窗口 / GL 上下文 / 全屏 / VSync
src/render     Atlas、ChunkMesh（AO）、Frustum、SelectionBox、Shader、Sky、Screenshot
src/ui         ImGui 调试面板、HUD（准星/热键栏）、创造物品栏界面
src/world      Block、Chunk、Terrain（主世界/地狱）、Dimension、World（流式 + 线程）、
               Portal（门框判定/落点建门）、Fluid（20 Hz 流体调度）、Fire、
               Save（zlib）
```

## 线程模型

- 区块状态机：`Empty → Generating → Generated → Meshing → Meshed → Ready`
- TBB `task_group` 在 worker 上生成地形和构建网格；结果进入 `concurrent_queue`
- 主线程每帧最多上传 8 个网格（GL 上下文单线程）
- 区块用 `shared_ptr` 持有，网格任务只读；玩家编辑用 `shared_mutex` 保证数据安全
- 玩家编辑会 bump 版本号；过期的后台网格结果在 upload 时被丢弃并重新排队

## 方块更新调度

- **流体**：主线程 20 Hz 的 `FluidSim`，优先级队列 + 位置去重；`setBlock` 会自动唤醒周围
  6 格，区块加载完成时扫描流动格和岸边源头；每 tick 最多处理 512 格。
- **火焰**：`World` 内按位置记录到期 tick；20 Hz 检查寿命、下方支撑和相邻水；
  区块加载时重新登记火焰寿命。
- 区块卸载时队列中的位置会在处理时被丢弃；重新加载时由 wake 扫描恢复。

## 存档格式

`AIMC` + version + seed + 时间 + 选中格 + 快捷栏（v3 起每格保存方块与物品）+
**每个维度各自的玩家位置/朝向与编辑项**（v4），整体用 zlib（`gzFile`）压缩。

地形由 seed 确定性重建，因此只保存差异。读取端兼容 v1（无快捷栏）、v2（只有方块）和
v3（单维度）；旧档统一落到主世界，地狱从默认位置开始。

## 性能

- 流式加载半径由视距控制，主线程 GL 上传预算固定，避免卡帧。
- 网格上传每帧 8 个；流体每 tick 512 格；火焰只在有火时遍历小列表。
- 渲染顺序：不透明 → 自发光熔岩 → 半透明水 → 自发光传送门/火焰。
- 容器内 llvmpipe 的实测与 adaptive view distance 见 [开发与测试](development.md#性能测试)。
