# LootsFinder（tools/loots）

[English](README.md) | 简体中文

用于 All-In-One Finder 地图的结构**内容**求值：判断结构内哪些箱子或方块
带有指定战利品。

本模块支持两种构建方式：

| 模式 | 产物 |
|---|---|
| **独立构建**（`cmake -S tools/loots -B build`） | 静态库 + `loots-cli` + 自测程序 |
| **作为子项目**（`add_subdirectory(tools/loots)`） | 只生成静态库 `loots`，由主程序链接 |

## 边界

本模块不决定结构**生成在哪里**。结构位置和生物群系来自[cubiomes](../cubiomes)。
这样 `loots` 不直接依赖 cubiomes，可以独立构建、测试和版本化。

| 层 | 负责内容 |
|---|---|
| `tools/cubiomes` | 结构位置、生物群系、噪声、地表高度 |
| **`tools/loots`**（本模块） | piece 图、箱子/可疑方块坐标、`LootTableSeed`、战利品表随机、附魔随机 |
| 主程序 `native/` | 只做 RPC 适配 |

## 接口

| 头文件 | 使用方 |
|---|---|
| `include/loots/loots.h` | C++ API，主程序使用；可随源码演进 |
| `include/loots/loots_capi.h` | 稳定 C ABI（`LOOTS_CAPI_VERSION`），供 CLI 和外部工具使用 |
| `include/loots/version.h` | `LOOTS_VERSION_STRING` + `LOOTS_ABI_VERSION` |

主程序会在编译期检查 `LOOTS_ABI_VERSION`，避免子模块版本过旧时静默出错。

## 独立构建

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build          # 或：./build/loots-selftest
```

独立构建选项：

- `LOOTS_BUILD_CLI`（默认 `ON`）
- `LOOTS_BUILD_TESTS`（默认 `ON`）

作为主程序子项目时，这两个选项默认都是 `OFF`。`tests/` 仅本地使用，
已加入 `.gitignore`；目录不存在时 CMake 会关闭测试目标，而不是构建失败。

## CLI

所有命令都向 stdout 输出 JSON，便于继续管道处理。

退出码：`0` 成功，`1` 运行失败，`2` 用法错误。

```sh
# 库版本 / C ABI 版本
loots-cli info

# 每个 MC 版本的能力表
loots-cli capability --mc 32

# 求值一座远古城市（箱子 + 战利品）
loots-cli ancient-city --mc 32 --seed 12345 --x 100 --z -200

# 求值一座海底神殿（海绵房）
loots-cli sponge --mc 32 --seed 12345 --x -40 --z 80

# 求值一座海底遗迹（可疑沙 / 嗅探兽蛋）
loots-cli ocean-ruin --mc 35 --seed 12345 --x 10592 --z -2080
```

`--mc` 使用 cubiomes 的 `MC_*` 数字 id：

- `23` = 1.19.2
- `25` = 1.20.6
- `29` = 1.21.5
- `32` = 1.21.11
- `35` = 26.3

`--flags` 可选，用于大生物群系等，默认 `0`。

示例：

```text
$ loots-cli ancient-city --mc 32 --seed 12345 --x 100 --z -200
{
  "x": 100, "z": -200, "chunkX": 6, "chunkZ": -13,
  "exists": false, "pieces": 0,
  "bounds": { "minX": 0, "minY": 0, "minZ": 0, "maxX": 0, "maxY": 0, "maxZ": 0 },
  "chestCount": 0,
  "chests": [
  ]
}
```

旧命令名 `underwater-ruin` 仍作为 `ocean-ruin` 的兼容别名保留。

## 目录结构

```text
cubiomes/                    子模块（仅独立构建使用）
include/loots/               公共 C++ / C ABI / 版本头
data/                        生成的原版数据头，全部参与编译
src/core/                    rng、transform、version、C ABI
src/loot/                    战利品表回放
src/structures/              piece 提取（pieces_c.*、structure_pieces）
src/jigsaw/                  jigsaw 装配核心
src/ancient_city/            远古城市箱子
src/bastion/                 堡垒遗迹
src/monument/                海底神殿海绵房
src/outpost/                 掠夺者前哨站 + 悦灵 feature
src/woodland_mansion/        林地府邸房间 / 箱子
src/ocean_ruin/              海底遗迹箱子 / 可疑沙
src/trail_ruins/             古迹废墟 processor + 可疑砂砾
src/shipwreck/               沉船
src/desert_jungle/           沙漠神殿 + 丛林神殿
src/stronghold/              要塞 1.18 模拟器（从 third_party 移入）
src/cli/                     loots-cli
tests/                       本地自测（已 gitignore）
```

源码目录按结构分组；公共头文件在 `include/loots/`，参与编译的实现都在
`src/` 下。

## Cubiomes

独立构建会编译根目录的 `cubiomes/` 子模块：

```sh
git submodule update --init --recursive
```

作为主程序子项目时，本模块**不会**读取或编译本地 `cubiomes/` 子模块；
主程序使用公共的 `tools/cubiomes` 头文件和已有的 cubiomes 目标。
All-In-One 发行包采用的就是这种方式。

## 生成数据

参与编译的数据头已经提交，保证新克隆无需 Minecraft jar 也能构建。
`data/end_city_layouts.*` 只是参考转储，没有编译单元包含，已 gitignore。

| 文件 | 来源 | 是否提交 |
|---|---|---|
| `data/loot_tables.hpp` | 客户端 jar `loot_table/**` | 是 |
| `data/loot_tables_26_3.hpp` | 26.3 战利品表版本段 | 是 |
| `data/chest_layouts.hpp` | 结构 `.nbt` 模板 | 是 |
| `data/jigsaw_templates.hpp` | 结构模板 + template pool | 是 |
| `data/enchantments.hpp` | 附魔表 | 是 |
| `data/mansion_templates.hpp` | 林地府邸模板 | 是 |
| `data/trail_ruins*.hpp` | 古迹废墟模板 | 是 |
| `data/ocean_blocks.hpp` | 海底遗迹方块候选 | 是 |
| `data/end_city_layouts.*` | 末地城参考转储 | 否 |

重新生成（提取工具仅本地使用，已 gitignore）：

```sh
python tools/extract_loot_tables.py --out data
python tools/extract_chest_layouts.py --structure-dir <structure/> --out data
python tools/extract_chest_layouts.py --jar %APPDATA%/.minecraft/versions/26.3/26.3.jar --out data
```

用导出的 `loot_table/` 目录交叉检查生成表：

```sh
python tools/verify_py/verify_loot_tables.py
```

## 当前状态

本模块内置远古城市的 26.3 布局和精确地表高度，因此
`ancientCityChests()` 和地图支持独立构建使用。
仍会报告 `exists = false`。

| 能力 | C++ 库 | Java 26.3 查询 |
|---|---|---|
| 海底神殿海绵房 | 已完成 | 已完成 |
| 远古城市（监守/幽静/迅捷潜行/附魔金） | 内置布局完成 | 已完成 |
| 掠夺者前哨站悦灵 | 提供原版高度时回放，缺高度标记未判定 | 已完成 |
| 海底遗迹嗅探兽蛋 | 26.3 单 piece 精确；多 piece 高度近似 | 已完成 |
| 林地府邸尖顶模板 | 1.19.4–26.3 坐标一致；1.19.2 无模板/箱子 | 已完成 |
| 古迹废墟 4 种纹饰 | 提供原版高度时回放，缺高度只返回轮廓 | 已完成 |
| 堡垒遗迹猪鼻/下界合金/外框 | 箱子精确；方形外框由主程序绘制 | 已完成 |
| 沙漠神殿沙丘/附魔金 | 已完成（仅提示） | 已完成 |
| 丛林神殿荒野纹饰 | 已完成（仅提示） | 已完成 |
| 下界要塞肋骨纹饰 | 已完成 | 已完成 |
| 末地城尖塔纹饰 | 已完成 | 已完成 |
| 掠夺者前哨站哨兵纹饰 | 已完成（仅提示） | 已完成 |
| 沉船海岸纹饰 | 已完成（仅提示） | 已完成 |
| 要塞图书馆眼眸纹饰 | 已完成（精确 X/Z） | 已完成 |
| 要塞末地门房间 | 已完成（`strongholdPortalAt` 精确 X/Z/Y） | 已完成 |

调用方必须容忍 `exists = false` 和空结果。Java 26.3 后端是 oracle，
不会链接进便携版主程序。

## 版本

`ancientCityTier()` 使用 cubiomes 的 `MC_*` 数字 id。主程序构建会在
`native/map_service.cpp` 中静态断言这些 id 与真实枚举一致；cubiomes
重排版本号时会直接编译失败，而不是静默错档。

## 许可证

MIT，见 [LICENSE](LICENSE)。

主程序 All-In-One Finder 使用 GPL-3.0-only；MIT 代码可以并入 GPL 项目，
因此把本库链接进主程序没有问题。
