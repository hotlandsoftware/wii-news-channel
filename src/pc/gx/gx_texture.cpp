// Texture cache: GX texture images as OpenGL textures.
//
// The hardware reads texels from main memory when it draws (through a small
// cache that GXInvalidateTexAll() empties). Here an image is decoded once
// into an OpenGL texture and found again by what identifies it: the pointer,
// size, format, number of mipmap levels, byte order and, for colour-index
// formats, the palette that was loaded with GXLoadTlut().
//
// The application may write new texels into the same buffer (a decoded JPEG,
// an EFB copy, a font sheet that is decompressed on demand). To notice, every
// entry keeps a checksum of the encoded data, which is compared the first
// time the entry is used after anything that starts a new "generation": the
// end of a frame, GXInvalidateTexAll(), an EFB copy, PCGXInvalidateTexture().

#include "gx_internal.h"

#include <cstdlib>
#include <cstring>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>

#include "texdecode.h"

namespace {

struct Entry {
    const void* image;
    u16 width, height;
    u8 format;
    u8 levels;
    u8 hostOrder;
    u8 tlutFormat;
    u32 tlutHash;
    u32 dataSize;
    u32 dataHash;
    GLuint texture;
    u32 lastUsedFrame;
    u32 checkedGeneration;
};

enum { kMaxEntries = 1024, kMaxHostOrder = 1024, kUnusedFrames = 600 };

Entry* sEntries;
u32 sNumEntries;
u32 sGeneration = 1;
u32 sFrame;

const void* sHostOrder[kMaxHostOrder];
u32 sNumHostOrder;

u8* sScratch;
u32 sScratchSize;

bool IsColorIndex(u32 format) {
    return format == GX_TF_C4 || format == GX_TF_C8 || format == GX_TF_C14X2;
}

u32 LevelSize(u32 size, u32 level) {
    size >>= level;
    return size ? size : 1;
}

u32 EncodedSize(const Entry& e) {
    u32 total = 0;
    for (u32 level = 0; level < e.levels; level++) {
        total += PCGXTextureDataSize(e.format, LevelSize(e.width, level), LevelSize(e.height, level));
    }
    return total;
}

// `tlut`: the palette of a colour-index texture, as loaded into its slot.
void Upload(Entry& e, const PCGXTlutSlot* tlut) {
    glBindTexture(GL_TEXTURE_2D, e.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const u8* data = static_cast<const u8*>(e.image);
    for (u32 level = 0; level < e.levels; level++) {
        u32 w = LevelSize(e.width, level), h = LevelSize(e.height, level);
        u32 bytes = w * h * 4;
        if (bytes > sScratchSize) {
            sScratch = static_cast<u8*>(std::realloc(sScratch, bytes));
            sScratchSize = bytes;
        }
        bool ok = PCGXDecodeTexture(data, e.format, w, h, tlut ? tlut->data : nullptr, e.tlutFormat,
                                    tlut ? tlut->count : 0, e.hostOrder != 0, sScratch);
        if (!ok) {
            PCGXWarnOnce("GX: cannot decode texture format 0x%X (%ux%u); drawn magenta", e.format, w, h);
            for (u32 i = 0; i < w * h; i++) {
                sScratch[i * 4 + 0] = 255;
                sScratch[i * 4 + 1] = 0;
                sScratch[i * 4 + 2] = 255;
                sScratch[i * 4 + 3] = 255;
            }
        }
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), GL_RGBA8, static_cast<GLsizei>(w), static_cast<GLsizei>(h),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, sScratch);
        data += PCGXTextureDataSize(e.format, w, h);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, e.levels - 1);
    gPCGX.stats.textures++;
}

void Remove(u32 index) {
    glDeleteTextures(1, &sEntries[index].texture);
    sEntries[index] = sEntries[--sNumEntries];
}

} // namespace

u32 PCGXHashBytes(const void* data, u32 size) {
    // FNV-1a over 32-bit words, then the tail bytes.
    const u8* p = static_cast<const u8*>(data);
    u32 hash = 2166136261u;
    u32 words = size / 4;
    for (u32 i = 0; i < words; i++) {
        u32 w;
        std::memcpy(&w, p + i * 4, 4);
        hash = (hash ^ w) * 16777619u;
        hash ^= hash >> 15;
    }
    for (u32 i = words * 4; i < size; i++) {
        hash = (hash ^ p[i]) * 16777619u;
    }
    return hash;
}

bool PCGXTextureIsHostOrder(const void* image) {
    for (u32 i = 0; i < sNumHostOrder; i++) {
        if (sHostOrder[i] == image) {
            return true;
        }
    }
    return false;
}

void PCGXSetTextureHostOrder(const void* image, bool hostOrder) {
    for (u32 i = 0; i < sNumHostOrder; i++) {
        if (sHostOrder[i] == image) {
            if (!hostOrder) {
                sHostOrder[i] = sHostOrder[--sNumHostOrder];
                PCGXTextureNewGeneration();
            }
            return;
        }
    }
    if (hostOrder) {
        if (sNumHostOrder == kMaxHostOrder) {
            PCGXWarnOnce("GX: too many host-order texture buffers");
            return;
        }
        sHostOrder[sNumHostOrder++] = image;
        PCGXTextureNewGeneration();
    }
}

void PCGXInvalidateTexture(const void* image) {
    (void)image; // a new generation re-checks every texture that is used again
    PCGXTextureNewGeneration();
}

void PCGXTextureNewGeneration() {
    sGeneration++;
    gPCGX.dirty |= PC_GX_DIRTY_TEXTURES;
}

void PCGXTextureFrameEnd() {
    sFrame++;
    if (!PCGXRenderAvailable()) {
        return;
    }
    for (u32 i = 0; i < sNumEntries;) {
        if (sFrame - sEntries[i].lastUsedFrame > kUnusedFrames) {
            Remove(i);
        } else {
            i++;
        }
    }
    PCGXTextureNewGeneration();
}

u32 PCGXTextureForUnit(u32 unitIndex, bool* mipmapped) {
    const PCGXTexUnit& unit = gPCGX.tex[unitIndex];
    *mipmapped = false;
    if (unit.image == nullptr || unit.width == 0 || unit.height == 0) {
        return 0;
    }

    Entry key;
    std::memset(&key, 0, sizeof(key));
    key.image = unit.image;
    key.width = unit.width;
    key.height = unit.height;
    key.format = unit.format;
    key.levels = 1;
    if (unit.minFilter >= GX_NEAR_MIP_NEAR && unit.maxLod > 0) {
        // Levels up to the largest level of detail the sampler may use.
        u32 levels = (unit.maxLod + 15) / 16 + 1;
        u32 largest = unit.width > unit.height ? unit.width : unit.height;
        u32 possible = 1;
        while ((largest >> possible) != 0) {
            possible++;
        }
        key.levels = static_cast<u8>(levels < possible ? levels : possible);
    }
    key.hostOrder = PCGXTextureIsHostOrder(unit.image);
    if (IsColorIndex(unit.format)) {
        const PCGXTlutSlot& tlut = gPCGX.tluts[unit.tlutSlot];
        key.tlutFormat = unit.tlutFormat;
        key.tlutHash = tlut.hash ^ (tlut.count * 2654435761u);
    }
    *mipmapped = key.levels > 1;
    const PCGXTlutSlot* tlut = IsColorIndex(unit.format) ? &gPCGX.tluts[unit.tlutSlot] : nullptr;

    if (sEntries == nullptr) {
        sEntries = static_cast<Entry*>(std::calloc(kMaxEntries, sizeof(Entry)));
    }

    Entry* found = nullptr;
    for (u32 i = 0; i < sNumEntries; i++) {
        Entry& e = sEntries[i];
        if (e.image == key.image && e.width == key.width && e.height == key.height && e.format == key.format &&
            e.levels == key.levels && e.hostOrder == key.hostOrder && e.tlutFormat == key.tlutFormat &&
            e.tlutHash == key.tlutHash) {
            found = &e;
            break;
        }
    }

    if (found != nullptr) {
        found->lastUsedFrame = sFrame;
        if (found->checkedGeneration != sGeneration) {
            found->checkedGeneration = sGeneration;
            u32 hash = PCGXHashBytes(found->image, found->dataSize);
            if (hash != found->dataHash) {
                found->dataHash = hash;
                Upload(*found, tlut);
            }
        }
        return found->texture;
    }

    if (sNumEntries == kMaxEntries) {
        // Full: drop the entry that was not used for the longest time.
        u32 oldest = 0;
        for (u32 i = 1; i < sNumEntries; i++) {
            if (sEntries[i].lastUsedFrame < sEntries[oldest].lastUsedFrame) {
                oldest = i;
            }
        }
        Remove(oldest);
    }
    Entry& e = sEntries[sNumEntries++];
    e = key;
    e.dataSize = EncodedSize(e);
    e.dataHash = PCGXHashBytes(e.image, e.dataSize);
    e.lastUsedFrame = sFrame;
    e.checkedGeneration = sGeneration;
    glGenTextures(1, &e.texture);
    Upload(e, tlut);
    return e.texture;
}
