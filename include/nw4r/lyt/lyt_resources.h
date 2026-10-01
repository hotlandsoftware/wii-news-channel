#ifndef NW4R_LYT_RESOURCES_H
#define NW4R_LYT_RESOURCES_H

#include <types.h>
#include <revolution/gx.h>
#include <nw4r/lyt/lyt_types.h>

namespace nw4r {
namespace lyt {

class ResourceAccessor;

struct InflationLRTB {
    f32 l; // at 0x0
    f32 r; // at 0x4
    f32 t; // at 0x8
    f32 b; // at 0xC
};

struct WindowFrameSize {
    f32 l; // at 0x0
    f32 r; // at 0x4
    f32 t; // at 0x8
    f32 b; // at 0xC
};

class MaterialResourceNum {
public:
    u8 GetTexMapNum() const { return detail::GetBits<>(bits, 0, 4); }
    u8 GetTexSRTNum() const { return detail::GetBits<>(bits, 4, 4); }
    u8 GetTexCoordGenNum() const { return detail::GetBits<>(bits, 8, 4); }
    bool HasTevSwapTable() const { return detail::TestBit<>(bits, 12); }
    u8 GetIndTexSRTNum() const { return detail::GetBits<>(bits, 13, 2); }
    u8 GetIndTexStageNum() const { return detail::GetBits<>(bits, 15, 3); }
    u8 GetTevStageNum() const { return detail::GetBits<>(bits, 18, 5); }
    bool HasAlphaCompare() const { return detail::TestBit<>(bits, 23); }
    bool HasBlendMode() const { return detail::TestBit<>(bits, 24); }
    u8 GetChanCtrlNum() const { return detail::GetBits<>(bits, 25, 1); }
    u8 GetMatColNum() const { return detail::GetBits<>(bits, 27, 1); }

private:
    u32 bits; // at 0x0
};

namespace res {

struct BinaryFileHeader {
    char signature[4]; // at 0x0
    u16 byteOrder;     // at 0x4
    u16 version;       // at 0x6
    u32 fileSize;      // at 0x8
    u16 headerSize;    // at 0xC
    u16 dataBlocks;    // at 0xE
};

struct DataBlockHeader {
    char kind[4]; // at 0x0
    u32 size;     // at 0x4
};

struct StepKey {
    f32 frame;   // at 0x0
    u16 value;   // at 0x4
    u16 padding; // at 0x6
};

struct HermiteKey {
    f32 frame; // at 0x0
    f32 value; // at 0x4
    f32 slope; // at 0x8
};

struct AnimationInfo {
    u32 kind;       // at 0x0
    u8 num;         // at 0x4
    u8 padding[3];  // at 0x5

    static const u32 ANIM_INFO_PANE_PAIN_SRT = 'RLPA';
    static const u32 ANIM_INFO_PANE_VERTEX_COLOR = 'RLVC';
    static const u32 ANIM_INFO_PANE_VISIBILITY = 'RLVI';
    static const u32 ANIM_INFO_MATERIAL_COLOR = 'RLMC';
    static const u32 ANIM_INFO_MATERIAL_TEXTURE_PATTERN = 'RLTP';
    static const u32 ANIM_INFO_MATERIAL_TEXTURE_SRT = 'RLTS';
    static const u32 ANIM_INFO_MATERIAL_IND_TEX_SRT = 'RLIM';
};

struct AnimationTarget {
    u8 id;          // at 0x0
    u8 target;      // at 0x1
    u8 curveType;   // at 0x2
    u8 padding1;    // at 0x3
    u16 keyNum;     // at 0x4
    u8 padding2[2]; // at 0x6
    u32 keysOffset; // at 0x8
};

struct AnimationBlock {
    DataBlockHeader blockHeader; // at 0x00
    u16 frameSize;               // at 0x08
    u8 loop;                     // at 0x0A
    u8 padding1;                 // at 0x0B
    u16 fileNum;                 // at 0x0C
    u16 animContNum;             // at 0x0E
    u32 animContOffsetsOffset;   // at 0x10
};

struct AnimationContent {
    enum {
        ACType_Pane = 0,
        ACType_Material,
        ACType_Max
    };

    char name[20];  // at 0x00
    u8 num;         // at 0x14
    u8 type;        // at 0x15
    u8 padding[2];  // at 0x16
};

struct Texture {
    u32 nameStrOffset; // at 0x0
    u8 type;           // at 0x4
    u8 padding[3];     // at 0x5
};

struct Material {
    char name[20];                        // at 0x00
    GXColorS10 tevCols[TEVCOLOR_MAX];     // at 0x14
    GXColor tevKCols[GX_MAX_KCOLOR];      // at 0x2C
    MaterialResourceNum resNum;           // at 0x3C
};

struct TexMap {
    u16 texIdx; // at 0x0
    u8 wrapS;   // at 0x2
    u8 wrapT;   // at 0x3
};

static const u32 FILE_HEADER_SIGNATURE_ANIMATION = 'RLAN';
static const u32 FILE_HEADER_SIGNATURE_LAYOUT = 'RLYT';

static const u32 OBJECT_SIGNATURE_LAYOUT = 'lyt1';
static const u32 OBJECT_SIGNATURE_FONT_LIST = 'fnl1';
static const u32 OBJECT_SIGNATURE_MATERIAL_LIST = 'mat1';
static const u32 OBJECT_SIGNATURE_TEXTURE_LIST = 'txl1';
static const u32 OBJECT_SIGNATURE_PANE = 'pan1';
static const u32 OBJECT_SIGNATURE_PANE_CHILD_START = 'pas1';
static const u32 OBJECT_SIGNATURE_PANE_CHILD_END = 'pae1';
static const u32 OBJECT_SIGNATURE_PICTURE = 'pic1';
static const u32 OBJECT_SIGNATURE_BOUNDING = 'bnd1';
static const u32 OBJECT_SIGNATURE_WINDOW = 'wnd1';
static const u32 OBJECT_SIGNATURE_TEXT_BOX = 'txt1';
static const u32 OBJECT_SIGNATURE_GROUP = 'grp1';
static const u32 OBJECT_SIGNATURE_GROUP_CHILD_START = 'grs1';
static const u32 OBJECT_SIGNATURE_GROUP_CHILD_END = 'gre1';
static const u32 OBJECT_SIGNATURE_PANE_ANIM = 'pai1';

struct Pane {
    DataBlockHeader blockHeader; // at 0x00
    u8 flag;                     // at 0x08
    u8 basePosition;             // at 0x09
    u8 alpha;                    // at 0x0A
    u8 padding;                  // at 0x0B
    char name[16];               // at 0x0C
    char userData[8];            // at 0x1C
    math::VEC3 translate;        // at 0x24
    math::VEC3 rotate;           // at 0x30
    math::VEC2 scale;            // at 0x3C
    Size size;                   // at 0x44
};

struct Bounding : Pane {};

struct Picture : Pane {
    u32 vtxCols[VERTEXCOLOR_MAX]; // at 0x4C
    u16 materialIdx;              // at 0x5C
    u8 texCoordNum;               // at 0x5E
    u8 padding[1];                // at 0x5F
};

struct Font {
    u32 nameStrOffset; // at 0x0
    u8 type;           // at 0x4
    u8 padding[3];     // at 0x5
};

struct TextBox : Pane {
    u16 textBufBytes;              // at 0x4C
    u16 textStrBytes;              // at 0x4E
    u16 materialIdx;               // at 0x50
    u16 fontIdx;                   // at 0x52
    u8 textPosition;               // at 0x54
    u8 padding[3];                 // at 0x55
    u32 textStrOffset;             // at 0x58
    u32 textCols[TEXTCOLOR_MAX];   // at 0x5C
    Size fontSize;                 // at 0x64
    f32 charSpace;                 // at 0x6C
    f32 lineSpace;                 // at 0x70
};

struct WindowFrame {
    u16 materialIdx; // at 0x0
    u8 textureFlip;  // at 0x2
    u8 padding1;     // at 0x3
};

struct WindowContent {
    u32 vtxCols[VERTEXCOLOR_MAX]; // at 0x00
    u16 materialIdx;              // at 0x10
    u8 texCoordNum;               // at 0x12
    u8 padding[1];                // at 0x13
};

struct Window : Pane {
    InflationLRTB inflation;     // at 0x4C
    u8 frameNum;                 // at 0x5C
    u8 padding1;                 // at 0x5D
    u8 padding2;                 // at 0x5E
    u8 padding3;                 // at 0x5F
    u32 contentOffset;           // at 0x60
    u32 frameOffsetTableOffset;  // at 0x64
};

struct Group {
    DataBlockHeader blockHeader; // at 0x00
    char name[16];               // at 0x08
    u16 paneNum;                 // at 0x18
    u8 padding[2];               // at 0x1A
};

struct Layout {
    DataBlockHeader blockHeader; // at 0x00
    u8 originType;               // at 0x08
    u8 padding[3];               // at 0x09
    Size layoutSize;             // at 0x0C
};

struct TextureList {
    DataBlockHeader blockHeader; // at 0x0
    u16 texNum;                  // at 0x8
    u8 padding[2];               // at 0xA
};

struct FontList {
    DataBlockHeader blockHeader; // at 0x0
    u16 fontNum;                 // at 0x8
    u8 padding[2];               // at 0xA
};

struct MaterialList {
    DataBlockHeader blockHeader; // at 0x0
    u16 materialNum;             // at 0x8
    u8 padding[2];               // at 0xA
};

} // namespace res

struct ResBlockSet {
    const res::TextureList* pTextureList;   // at 0x0
    const res::FontList* pFontList;         // at 0x4
    const res::MaterialList* pMaterialList; // at 0x8
    ResourceAccessor* pResAccessor;         // at 0xC
};

} // namespace lyt
} // namespace nw4r

#endif
