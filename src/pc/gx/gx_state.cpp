// GX state: the BP, CP and XF registers (see gx_internal.h).

#include "gx_internal.h"

#include <cstdlib>
#include <cstring>

#include <revolution/os.h>

PCGXState gPCGX;

namespace {

s16 SignExtend11(u32 value) {
    value &= 0x7FF;
    return static_cast<s16>((value & 0x400) ? static_cast<s32>(value) - 0x800 : static_cast<s32>(value));
}

// TX_SETMODE0 stores the minification filter in its own numbering.
const u8 kHwMinFilterToApi[8] = {GX_NEAR,   GX_NEAR_MIP_NEAR, GX_NEAR_MIP_LIN, GX_NEAR,
                                 GX_LINEAR, GX_LIN_MIP_NEAR,  GX_LIN_MIP_LIN,  GX_LINEAR};

void PutBE32(u8* p, u32 value) {
    p[0] = static_cast<u8>(value >> 24);
    p[1] = static_cast<u8>(value >> 16);
    p[2] = static_cast<u8>(value >> 8);
    p[3] = static_cast<u8>(value);
}

// Appends to the display list being recorded.
void Record(const u8* bytes, u32 count) {
    PCGXState& s = gPCGX;
    if (s.recordUsed + count > s.recordSize) {
        s.recordOverflow = true;
        return;
    }
    std::memcpy(s.recordBuffer + s.recordUsed, bytes, count);
    s.recordUsed += count;
}

void TexRegister(u32 reg, u32 data) {
    u32 unit = reg & 3;
    u32 kind = reg & 0xFC;
    if (reg >= 0xA0) {
        unit += 4;
        kind -= 0x20;
    }
    PCGXTexUnit& t = gPCGX.tex[unit];
    switch (kind) {
    case PC_BP_TX_SETMODE0:
        t.wrapS = data & 3;
        t.wrapT = (data >> 2) & 3;
        t.magFilter = (data >> 4) & 1;
        t.minFilter = kHwMinFilterToApi[(data >> 5) & 7];
        t.lodBias = static_cast<s8>((data >> 9) & 0xFF);
        t.maxAniso = (data >> 19) & 3;
        break;
    case PC_BP_TX_SETMODE1:
        t.minLod = data & 0xFF;
        t.maxLod = (data >> 8) & 0xFF;
        break;
    case PC_BP_TX_SETIMAGE0:
        t.width = static_cast<u16>((data & 0x3FF) + 1);
        t.height = static_cast<u16>(((data >> 10) & 0x3FF) + 1);
        t.format = (data >> 20) & 0xF;
        break;
    case PC_BP_TX_SETIMAGE3:
        t.image = PCGXAddressToHost(data << 5);
        break;
    case PC_BP_TX_SETTLUT: {
        // Offset into texture memory in units of 512 bytes, from 0x80000.
        // The SDK's regions: 16 palettes of 256 entries from 0xC0000, then
        // 4 of 1024 entries from 0xE0000.
        u32 offset = data & 0x3FF;
        if (offset >= 0x300) {
            t.tlutSlot = static_cast<u16>(16 + ((offset - 0x300) >> 6));
        } else if (offset >= 0x200) {
            t.tlutSlot = static_cast<u16>((offset - 0x200) >> 4);
        } else {
            t.tlutSlot = 0;
        }
        if (t.tlutSlot >= PC_GX_NUM_TLUTS) {
            t.tlutSlot = 0;
        }
        t.tlutFormat = (data >> 10) & 3;
        break;
    }
    default:
        break; // SETIMAGE1/2: where the texture is cached in texture memory
    }
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
}

// A palette load that came through the registers (a display list).
void TlutLoadRegister(u32 data) {
    u32 offset = data & 0x3FF;
    u32 count = ((data >> 10) & 0x7FF) * 16;
    u32 slot = offset >= 0x300 ? 16 + ((offset - 0x300) >> 6) : (offset >= 0x200 ? (offset - 0x200) >> 4 : 0);
    const void* source = PCGXAddressToHost(gPCGX.bp[PC_BP_TLUT_LOAD_ADDR] << 5);
    if (source == nullptr || slot >= PC_GX_NUM_TLUTS) {
        PCGXWarnOnce("GX: palette load from a register address that is not in MEM1/MEM2");
        return;
    }
    PCGXTlutSlot& t = gPCGX.tluts[slot];
    if (t.capacity < count) {
        t.data = static_cast<u8*>(std::realloc(t.data, count * 2));
        t.capacity = count;
    }
    std::memcpy(t.data, source, count * 2);
    t.count = count;
    t.hash = PCGXHashBytes(t.data, count * 2);
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
}

// An EFB copy that came through the registers (a display list).
void CopyTrigger(u32 data) {
    PCGXState& s = gPCGX;
    void* dest = const_cast<void*>(PCGXAddressToHost(s.bp[PC_BP_COPY_DST] << 5));
    bool clear = (data >> 11) & 1;
    if (dest == nullptr) {
        PCGXWarnOnce("GX: EFB copy to a register address that is not in MEM1/MEM2");
        return;
    }
    if ((data >> 14) & 1) {
        PCGXRenderCopyDisp(dest, clear);
        return;
    }
    u32 width = (s.bp[PC_BP_EFB_SRC_SIZE] & 0x3FF) + 1;
    u32 height = ((s.bp[PC_BP_EFB_SRC_SIZE] >> 10) & 0x3FF) + 1;
    bool half = (data >> 9) & 1;
    u32 format = ((data >> 4) & 7) | (((data >> 3) & 1) << 3);
    s.texCopyMipmap = half;
    s.texCopyDstWidth = static_cast<u16>(half ? width / 2 : width);
    s.texCopyDstHeight = static_cast<u16>(half ? height / 2 : height);
    // Depth copies and the copy-only formats share numbers with the plain
    // formats; the pixel format register says which EFB plane is copied.
    if ((s.bp[PC_BP_PE_CONTROL] & 7) == GX_PF_Z24) {
        format |= _GX_TF_ZTF;
    }
    s.texCopyFormat = format;
    PCGXRenderCopyTex(dest, clear);
}

} // namespace

PCGXTevOrder PCGXGetTevOrder(u32 stage) {
    u32 reg = gPCGX.bp[PC_BP_RAS1_TREF0 + stage / 2];
    if (stage & 1) {
        reg >>= 12;
    }
    PCGXTevOrder order;
    order.texMap = reg & 7;
    order.texCoord = (reg >> 3) & 7;
    order.texEnable = ((reg >> 6) & 1) != 0;
    order.channel = (reg >> 7) & 7;
    return order;
}

const void* PCGXAddressToHost(u32 address) {
    if (address == 0) {
        return nullptr;
    }
    // The OS backend maps MEM1 and MEM2 at the console's addresses when the
    // host leaves them free (docs/pc_port.md, section 11). Only then does an
    // address in a register mean anything.
    u32 physical = address & 0x3FFFFFFF;
    u32 mem1Hi = reinterpret_cast<u32>(OSGetMEM1ArenaHi());
    u32 mem2Hi = reinterpret_cast<u32>(OSGetMEM2ArenaHi());
    if (physical < 0x01800000 && (mem1Hi >> 28) == 0x8 && (physical | 0x80000000) < mem1Hi) {
        return reinterpret_cast<const void*>(physical | 0x80000000);
    }
    if (physical >= 0x10000000 && physical < 0x14000000 && (mem2Hi >> 28) == 0x9 && (physical | 0x80000000) < mem2Hi) {
        return reinterpret_cast<const void*>(physical | 0x80000000);
    }
    return nullptr;
}

void PCGXLoadBP(u32 value) {
    PCGXState& s = gPCGX;
    u32 reg = value >> 24;
    u32 data = ((s.bp[reg] & ~s.bpMask) | (value & s.bpMask)) & 0xFFFFFF;
    s.stats.bpWrites++;

    // The mask applies to one write.
    if (reg == PC_BP_MASK) {
        s.bpMask = value & 0xFFFFFF;
        return;
    }
    s.bpMask = 0xFFFFFF;
    s.bp[reg] = data;

    if (reg >= PC_BP_TEV_REG_RA0 && reg < PC_BP_TEV_REG_RA0 + 8) {
        // RA and BG halves of a TEV colour register or a konst colour (bit 23).
        u32 index = (reg - PC_BP_TEV_REG_RA0) >> 1;
        s16* color = (data & 0x800000) ? s.tevKonst[index] : s.tevColor[index];
        if (reg & 1) {
            color[2] = SignExtend11(data);
            color[1] = SignExtend11(data >> 12);
        } else {
            color[0] = SignExtend11(data);
            color[3] = SignExtend11(data >> 12);
        }
        s.dirty |= PC_GX_DIRTY_UNIFORMS;
        return;
    }
    if ((reg >= 0x80 && reg < 0x9C) || (reg >= 0xA0 && reg < 0xBC)) {
        TexRegister(reg, data);
        return;
    }
    switch (reg) {
    case PC_BP_TLUT_LOAD:
        TlutLoadRegister(data);
        break;
    case PC_BP_COPY_TRIGGER:
        CopyTrigger(data);
        break;
    case PC_BP_SCISSOR_TL:
    case PC_BP_SCISSOR_BR:
    case PC_BP_SCISSOR_OFFSET:
        s.dirty |= PC_GX_DIRTY_RASTER;
        break;
    case PC_BP_ZMODE:
    case PC_BP_CMODE0:
    case PC_BP_CMODE1:
    case PC_BP_PE_CONTROL:
        // Destination alpha and the pixel format also select a program variant.
        s.dirty |= PC_GX_DIRTY_PIXEL | PC_GX_DIRTY_PROGRAM | PC_GX_DIRTY_UNIFORMS;
        break;
    default:
        s.dirty |= PC_GX_DIRTY_ALL;
        break;
    }
}

void PCGXLoadCP(u32 reg, u32 value) {
    PCGXState& s = gPCGX;
    s.stats.cpWrites++;
    u32 index = reg & 0xF;
    switch (reg & 0xF0) {
    case PC_CP_MATINDEX_A:
        s.cpMatIndexA = value;
        break;
    case PC_CP_MATINDEX_B:
        s.cpMatIndexB = value;
        break;
    case PC_CP_VCD_LO:
        s.vcdLo = value;
        break;
    case PC_CP_VCD_HI:
        s.vcdHi = value;
        break;
    case PC_CP_VAT_A:
        s.vatA[index & 7] = value;
        break;
    case PC_CP_VAT_B:
        s.vatB[index & 7] = value;
        break;
    case PC_CP_VAT_C:
        s.vatC[index & 7] = value;
        break;
    case PC_CP_ARRAY_BASE:
        s.arrays[index].base = static_cast<const u8*>(PCGXAddressToHost(value));
        break;
    case PC_CP_ARRAY_STRIDE:
        s.arrays[index].stride = value & 0xFF;
        break;
    default:
        break;
    }
}

void PCGXLoadXF(u32 address, u32 count, const u32* words) {
    PCGXState& s = gPCGX;
    s.stats.xfWrites++;
    for (u32 i = 0; i < count; i++) {
        u32 a = address + i;
        if (a < PC_XF_SIZE) {
            s.xf[a] = words[i];
        }
    }
}

void PCGXWriteBP(u32 reg, u32 value24) {
    u32 value = (reg << 24) | (value24 & 0xFFFFFF);
    if (gPCGX.recordBuffer != nullptr) {
        u8 bytes[5];
        bytes[0] = 0x61;
        PutBE32(bytes + 1, value);
        Record(bytes, 5);
        return;
    }
    PCGXLoadBP(value);
}

void PCGXWriteCP(u32 reg, u32 value) {
    if (gPCGX.recordBuffer != nullptr) {
        u8 bytes[6];
        bytes[0] = 0x08;
        bytes[1] = static_cast<u8>(reg);
        PutBE32(bytes + 2, value);
        Record(bytes, 6);
        return;
    }
    PCGXLoadCP(reg, value);
}

void PCGXWriteXF(u32 address, u32 count, const u32* words) {
    if (gPCGX.recordBuffer != nullptr) {
        // At most 16 words per command.
        while (count > 0) {
            u32 n = count > 16 ? 16 : count;
            u8 bytes[5 + 16 * 4];
            bytes[0] = 0x10;
            PutBE32(bytes + 1, ((n - 1) << 16) | address);
            for (u32 i = 0; i < n; i++) {
                PutBE32(bytes + 5 + i * 4, words[i]);
            }
            Record(bytes, 5 + n * 4);
            address += n;
            words += n;
            count -= n;
        }
        return;
    }
    PCGXLoadXF(address, count, words);
}

PCGXViewport PCGXGetViewport() {
    // XF holds the scale and the centre, with the rasteriser's bias of 342.
    f32 sx = PCGXXFFloat(PC_XF_VIEWPORT + 0);
    f32 sy = PCGXXFFloat(PC_XF_VIEWPORT + 1);
    f32 sz = PCGXXFFloat(PC_XF_VIEWPORT + 2);
    f32 ox = PCGXXFFloat(PC_XF_VIEWPORT + 3);
    f32 oy = PCGXXFFloat(PC_XF_VIEWPORT + 4);
    f32 oz = PCGXXFFloat(PC_XF_VIEWPORT + 5);

    u32 offset = gPCGX.bp[PC_BP_SCISSOR_OFFSET];
    f32 offsetX = static_cast<f32>(static_cast<s32>((offset & 0x3FF) << 1) - 342);
    f32 offsetY = static_cast<f32>(static_cast<s32>(((offset >> 10) & 0x3FF) << 1) - 342);

    PCGXViewport v;
    v.width = sx * 2.0f;
    v.height = sy * -2.0f;
    v.left = ox - 342.0f - sx - offsetX;
    v.top = oy - 342.0f + sy - offsetY;
    v.farZ = oz / 16777215.0f;
    v.nearZ = (oz - sz) / 16777215.0f;
    return v;
}

void PCGXGetScissorRect(int* left, int* top, int* width, int* height) {
    u32 tl = gPCGX.bp[PC_BP_SCISSOR_TL];
    u32 br = gPCGX.bp[PC_BP_SCISSOR_BR];
    u32 offset = gPCGX.bp[PC_BP_SCISSOR_OFFSET];
    int offsetX = static_cast<int>((offset & 0x3FF) << 1) - 342;
    int offsetY = static_cast<int>(((offset >> 10) & 0x3FF) << 1) - 342;

    int y0 = static_cast<int>(tl & 0x7FF) - 342 - offsetY;
    int x0 = static_cast<int>((tl >> 12) & 0x7FF) - 342 - offsetX;
    int y1 = static_cast<int>(br & 0x7FF) - 342 - offsetY + 1;
    int x1 = static_cast<int>((br >> 12) & 0x7FF) - 342 - offsetX + 1;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PC_GX_EFB_WIDTH) x1 = PC_GX_EFB_WIDTH;
    if (y1 > PC_GX_EFB_HEIGHT) y1 = PC_GX_EFB_HEIGHT;
    *left = x0;
    *top = y0;
    *width = x1 > x0 ? x1 - x0 : 0;
    *height = y1 > y0 ? y1 - y0 : 0;
}

void PCGXResetState() {
    PCGXState& s = gPCGX;
    // Keep what was allocated.
    PCGXTlutSlot tluts[PC_GX_NUM_TLUTS];
    std::memcpy(tluts, s.tluts, sizeof(tluts));
    PCGXStats stats = s.stats;
    std::memset(&s, 0, sizeof(s));
    std::memcpy(s.tluts, tluts, sizeof(tluts));
    s.stats = stats;
    for (PCGXTlutSlot& t : s.tluts) {
        t.count = 0;
        t.hash = 0;
    }
    s.bpMask = 0xFFFFFF;
    s.zScale = 16777215.0f;
    s.dirty = PC_GX_DIRTY_ALL;
    PCGXFifoReset();
}
