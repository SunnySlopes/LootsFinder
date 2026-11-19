/* Ported from tools/geode/src/mineshaft_gen/mineshaft_gen.h (GPLv3). */
/*
 * mineshaft_gen.h �?废弃矿井(Mineshaft)生成器库接口
 *
 * �?1.21.11 (mojmap) 反编译源�?1:1 复刻:
 *   net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
 *   net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure
 *   net.minecraft.world.level.levelgen.structure.BoundingBox
 *   net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder
 *   net.minecraft.world.level.levelgen.WorldgenRandom / LegacyRandomSource
 *
 * 版本覆盖: 1.21.11 / 26.1.2 / 26.2 / 26.3
 * ---------------------------------------------------------------
 * 已逐行核对四个版本�?MineshaftPieces.java �?MineshaftStructure.java:
 *
 *   26.1.2  MineshaftPieces.java �?1.21.11 **逐字节相�?*
 *   26.2    �?EntityType -> EntityTypes �?API 改名;
 *           RNG 调用序列与生成逻辑不变
 *   26.3    MineshaftPieces �?builder 参数位置前移(random 仍紧随其�?,
 *           RNG 调用序列 (nextInt(100) / nextInt(3)+2 / nextInt(4) /
 *           nextInt(3) / nextInt(23)) �?1.21.11 完全一�?
 *           MineshaftStructure.java **逐字节相�?*
 *
 * 因此四个版本共用同一套生成规则。版本枚举存在的意义�? 让调用方记录
 * "按哪个版本的源码核对�?, 并让 ms_defaults_for_version() 给出该版�? * 正确�?seaLevel / minY 默认值�? */

/*
 * Version coverage (verified against the real jars): MineshaftPieces and MineshaftStructure
 * are logically identical from 1.18.2 through 1.21.11; the 1.21.4+ decompiler output only
 * splits helpers that 1.18-1.21.1 inlined. MsVersion therefore records the comparison
 * baseline, while ms_version_min_y()/ms_version_sea_level() are the same for every 1.18+
 * version. The exact chest replay is used for mc 22 (1.18.2) and newer.
 */
#ifndef MINESHAFT_GEN_H_
#define MINESHAFT_GEN_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 生成器内部容量上�?piece �?。原版深度上�?8, 实践中远达不到这个数�?*/
#define MS_MAX_PIECES 4096

/* ------------------------------------------------------------------------- */
/* 版本                                                                      */
/* ------------------------------------------------------------------------- */

typedef enum {
    MS_VERSION_1_21_11 = 0,
    MS_VERSION_26_1_2  = 1,
    MS_VERSION_26_2    = 2,
    MS_VERSION_26_3    = 3,
    MS_VERSION_COUNT   = 4
} MsVersion;

/** 版本�?("1.21.11" / "26.1.2" / "26.2" / "26.3")�?*/
const char *ms_version_name(MsVersion v);

/** 从字符串解析版本; 失败返回 -1�?*/
int ms_version_parse(const char *text);

/** 该版本的世界最�?Y (1.18+ 均为 -64)�?*/
int ms_version_min_y(MsVersion v);

/** 该版本的海平�?(1.18+ 均为 63)�?*/
int ms_version_sea_level(MsVersion v);

/**
 * 该区块是否被放置为废弃矿井（placement 层，不是 piece 生成）�? *
 * ms_generate() 只负�?piece 生成，对�?vanilla
 * MineshaftStructure.findGenerationPoint；是否生成由外层�? * RandomSpreadStructurePlacement（spacing=1, separation=0,
 * frequency=0.004, legacy_type_3）决定。调用方必须先过这道门，
 * 否则会把每个区块都当成有矿井�? *
 * 判定与本仓库 tools/cubiomes �?getMineshafts() 逐位一致，因此
 * 地图标记与结构覆盖用的是同一套结论�? */
int ms_is_feature_chunk(uint64_t seed, int32_t chunkX, int32_t chunkZ);

/* ------------------------------------------------------------------------- */
/* 方向 / 类型 / 标志                                                        */
/* ------------------------------------------------------------------------- */

enum { MS_DIR_NORTH = 0, MS_DIR_SOUTH = 1, MS_DIR_WEST = 2, MS_DIR_EAST = 3 };

enum {
    MS_PT_ROOM = 0,
    MS_PT_CORRIDOR = 1,
    MS_PT_CROSSING = 2,
    MS_PT_STAIRS = 3,
    MS_PT_ENTRANCE = 4
};

#define MS_FLAG_RAILS    0x01 /* 走廊带铁�?*/
#define MS_FLAG_SPIDER   0x02 /* 走廊有洞穴蜘蛛刷怪笼 */
#define MS_FLAG_TWOFLOOR 0x04 /* 十字为双�?*/

/** 矿井材质类型(决定原木/木板/栅栏方块, 也影响垂直定�?�?*/
enum { MS_TYPE_NORMAL = 0, MS_TYPE_MESA = 1 };

/* ------------------------------------------------------------------------- */
/* 数据结构                                                                  */
/* ------------------------------------------------------------------------- */

/** 整型闭区间盒, 与源�?BoundingBox 同语义�?*/
typedef struct {
    int32_t minX, minY, minZ, maxX, maxY, maxZ;
} MsBox;

/** 一个生成的 piece, 与源�?StructurePiece 的盒/元信息对应�?*/
typedef struct {
    int type;      /* MS_PT_* */
    int orient;    /* MS_DIR_* (起点房间�?-1) */
    int genDepth;  /* 生成深度 (起点房间�?0) */
    int flags;     /* MS_FLAG_* 组合 */
    int sections;  /* 走廊段数(其他类型�?0) */
    MsBox bb;      /* 世界坐标包围�?已含垂直偏移 dy) */
} MsPiece;

/** 随机数状�?(LegacyRandomSource / java.util.Random �?48-bit LCG)�?*/
typedef struct {
    uint64_t seed;  /* �?48 位有�?*/
    int64_t  count; /* 已取数次�? 仅统计用 */
} MsRng;

/** 生成选项。传 NULL �?ms_generate() 则用 normal 类型的默认值�?*/
typedef struct {
    int version;      /* MsVersion, 用于记录与默认值推�?*/
    int type;         /* MS_TYPE_NORMAL / MS_TYPE_MESA */
    int32_t seaLevel; /* 海平�?默认 63) */
    int32_t minY;     /* 维度最�?y(默认 -64) */
    /**
     * mesa 专用: 结构中心处的地表高度�?     *
     * 原版�?chunkGenerator.getBaseHeight() 取真实地形高�? 本库不建地形,
     * 因此必须由调用方提供�?*MS_TYPE_NORMAL 忽略此字�?*�?     */
    int32_t mesaSurface;
} MsGenOpts;

/** 生成统计�?*/
typedef struct {
    int pieceCount;     /* 生成并保留的 piece �?*/
    int entranceCount;  /* 房间入口盒数(不参与碰撞检�? */
    int overflow;       /* 1 = 超出 MS_MAX_PIECES 被截�?*/
    int32_t dy;         /* 施加的垂直偏�?*/
    MsBox structureBox; /* 全部 piece 的并�?已含 dy) */
} MsGenResult;

/** 生成器工作区。容量固�? 调用方可栈上或堆上分�?�?400 KB)�?*/
typedef struct {
    MsRng rng;

    MsPiece pieces[MS_MAX_PIECES];
    int     count;

    /* 起点房间�?childEntranceBoxes: 不参与碰撞检�? 仅用于挖入口 */
    MsPiece entrances[MS_MAX_PIECES];
    int     entranceCount;

    int overflow;

    /* 起点房间�?恒等�?pieces[0].bb), 方便调用方直接取�?*/
    MsBox startRoom;

    MsGenResult result;
} MineshaftGen;

/* ------------------------------------------------------------------------- */
/* 生成                                                                      */
/* ------------------------------------------------------------------------- */

/**
 * 生成一座废弃矿井�? *
 * 对应源码 MineshaftStructure.findGenerationPoint -> generatePiecesAndAdjust:
 * 播种 -> 丢弃一�?nextDouble -> 起点房间 -> 递归拼接 -> 垂直定位�? *
 * @param gen     输出工作�? 调用方分�? * @param seed    世界种子
 * @param chunkX  区块 X
 * @param chunkZ  区块 Z
 * @param opts    选项, 可传 NULL(等价�?normal + 默认 seaLevel/minY)
 * @return 生成�?piece �? 0 表示该位置没有矿�? -1 表示参数非法
 *
 * 生成后可直接�?
 *   gen->pieces[0..count)  piece �?depth-first 顺序, pieces[0] 恒为房间)
 *   gen->entrances[0..entranceCount)  入口�? *   gen->result.structureBox          整体并集
 */
int ms_generate(MineshaftGen *gen, uint64_t seed, int32_t chunkX, int32_t chunkZ,
                const MsGenOpts *opts);

/**
 * 轻量查询: 只关�?这个区块有没有矿井、范围多�?时用这个�? *
 * 内部同样走完整生成路�? 但不要求调用方持�?400 KB �?MineshaftGen�? *
 * @param outBox     输出并集�?�?NULL)
 * @param outPieces  输出 piece �?�?NULL)
 * @return 1 = 有矿�? 0 = 没有, -1 = 参数非法
 */
int ms_probe(uint64_t seed, int32_t chunkX, int32_t chunkZ, const MsGenOpts *opts,
             MsBox *outBox, int *outPieces);

/* ------------------------------------------------------------------------- */
/* 辅助                                                                      */
/* ------------------------------------------------------------------------- */

const char *ms_piece_type_name(int type);
const char *ms_dir_name(int orient);

int32_t ms_box_xspan(const MsBox *b);
int32_t ms_box_yspan(const MsBox *b);
int32_t ms_box_zspan(const MsBox *b);

/** 两盒相交判定(六轴闭区�? 与源�?BoundingBox.intersects 一�?�?*/
int ms_box_intersects(const MsBox *a, const MsBox *b);

/**
 * �?piece 的盒型是否落在源码公式允许的集合内�? *
 * 这是移植正确性的核心自检口径: (xs, ys, zs) 必须能由源码里的构造公式产生�? * 返回 1 = 合法�? */
int ms_span_is_valid(int type, int32_t xs, int32_t ys, int32_t zs);

/* ------------------------------------------------------------------------- */
/* 随机�?(暴露给需要与 Java 逐位对拍的调用方)                               */
/* ------------------------------------------------------------------------- */

/**
 * java.util.Random �?nextInt(bound)�? *
 * 与库内部生成用的是同一�?48-bit LCG �? 因此外部可以按任意顺序推进它�? * 复现 Java 的行为。`gen` 只需是可用的 MineshaftGen(不必�?ms_generate)�? */
int32_t ms_rng_next_int(MineshaftGen *gen, int32_t bound);

/**
 * setLargeFeatureSeed(seed, chunkX, chunkZ)�? *
 * 这是结构生成开始前的播种步�? setSeed(seed) -> 取两�?nextLong 作为
 * xScale/zScale -> result = cx*xScale ^ cz*zScale ^ seed -> setSeed(result)�? */
void ms_rng_set_large_feature_seed(MineshaftGen *gen, uint64_t seed,
                                   int32_t chunkX, int32_t chunkZ);

#ifdef __cplusplus
}
#endif

#endif /* MINESHAFT_GEN_H_ */
