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
    u8 replaced;          // the replacer wants this texture (part of the key)
    u8 replacementLevels; // levels of the replacement that was made; 0: the image itself was uploaded
    u32 tlutHash;
    u32 dataSize;
    u32 dataHash;
    GLuint texture;
    u32 lastUsedFrame;
    u32 checkedGeneration;
};

PCGXTextureReplacer sReplacer;
bool sHaveReplacer;

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

// Halves a picture of `channels` bytes per pixel in place (2x2 averages,
// rounded to nearest). Width and height are even.
void HalveInPlace(u8* pixels, u32 width, u32 height, u32 channels) {
    const u32 w = width / 2, h = height / 2;
    for (u32 y = 0; y < h; y++) {
        const u8* a = pixels + (y * 2) * width * channels;
        const u8* b = a + width * channels;
        u8* out = pixels + y * w * channels;
        for (u32 x = 0; x < w * channels; x++) {
            const u32 from = (x / channels) * 2 * channels + x % channels;
            out[x] = static_cast<u8>((a[from] + a[from + channels] + b[from] + b[from + channels] + 2) >> 2);
        }
    }
}

// Asks the replacer for a larger picture of the entry's image, whose decoded
// texels are in sScratch, and uploads it with the levels between its size and
// the image's. False: there is none, upload the image.
bool UploadReplacement(Entry& e) {
    static GLint maxSize;
    if (maxSize == 0) {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    }
    const u32 largest = e.width > e.height ? e.width : e.height;
    u32 maxScale = 8;
    while (maxScale > 1 && largest * maxScale > static_cast<u32>(maxSize)) {
        maxScale /= 2;
    }
    if (maxScale < 2) {
        return false;
    }
    u32 scale = 0, channels = 0;
    u8* pixels = sReplacer.make(e.image, e.format, e.width, e.height, sScratch, maxScale, &scale, &channels, sReplacer.user);
    if (pixels == nullptr) {
        return false;
    }
    if ((scale != 2 && scale != 4 && scale != 8) || scale > maxScale || (channels != 1 && channels != 2 && channels != 4)) {
        PCGXWarnOnce("GX: replacement texture with scale %u and %u channels refused", scale, channels);
        std::free(pixels);
        return false;
    }
    static const GLint kInternal[5] = {0, GL_R8, GL_RG8, 0, GL_RGBA8};
    static const GLenum kFormat[5] = {0, GL_RED, GL_RG, 0, GL_RGBA};
    // What a texel of the source format reads as: I, I, I, I or I, I, I, A.
    static const GLint kSwizzle[5][4] = {{},
                                         {GL_RED, GL_RED, GL_RED, GL_RED},
                                         {GL_RED, GL_RED, GL_RED, GL_GREEN},
                                         {},
                                         {GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA}};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, kSwizzle[channels]);
    u32 w = e.width * scale, h = e.height * scale;
    u32 level = 0;
    for (;; level++) {
        glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), kInternal[channels], static_cast<GLsizei>(w),
                     static_cast<GLsizei>(h), 0, kFormat[channels], GL_UNSIGNED_BYTE, pixels);
        if (w == e.width) {
            break;
        }
        HalveInPlace(pixels, w, h, channels);
        w /= 2;
        h /= 2;
    }
    std::free(pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(level));
    e.replacementLevels = static_cast<u8>(level + 1);
    gPCGX.stats.replacements++;
    return true;
}

// `tlut`: the palette of a colour-index texture, as loaded into its slot.
void Upload(Entry& e, const PCGXTlutSlot* tlut) {
    if (e.replaced) {
        // A fresh texture object: a replacement has its own format and
        // channel order, and the image that follows it must not inherit them.
        glDeleteTextures(1, &e.texture);
        glGenTextures(1, &e.texture);
        e.replacementLevels = 0;
    }
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
        if (ok && e.replaced && UploadReplacement(e)) {
            gPCGX.stats.textures++;
            return;
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

// Kept EFB copies (gx_internal.h). None exist unless the EFB is scaled.
struct Copy {
    const void* image;
    u16 width, height; // as a GX texture; 0 until PCGXCopyTextureEnd()
    u8 format;
    GLuint texture;
    u32 dataSize;
    u32 dataHash;
    u32 checkedGeneration;
    u32 lastUsedFrame;
};

enum { kMaxCopies = 8 };

Copy sCopies[kMaxCopies];
u32 sNumCopies;

void RemoveCopy(u32 index) {
    glDeleteTextures(1, &sCopies[index].texture);
    sCopies[index] = sCopies[--sNumCopies];
}

// The kept copy for a texture unit's image, if the buffer still holds what
// the copy wrote.
GLuint CopyForUnit(const PCGXTexUnit& unit) {
    for (u32 i = 0; i < sNumCopies; i++) {
        Copy& c = sCopies[i];
        if (c.image != unit.image) {
            continue;
        }
        if (c.width != unit.width || c.height != unit.height || c.format != unit.format) {
            return 0; // used as another texture: only the texels can say what it is
        }
        if (c.checkedGeneration != sGeneration) {
            c.checkedGeneration = sGeneration;
            if (PCGXHashBytes(c.image, c.dataSize) != c.dataHash) {
                RemoveCopy(i); // the game wrote into its buffer
                return 0;
            }
        }
        c.lastUsedFrame = sFrame;
        return c.texture;
    }
    return 0;
}

} // namespace

u32 PCGXCopyTextureBegin(const void* image, u32 width, u32 height) {
    Copy* copy = nullptr;
    for (u32 i = 0; i < sNumCopies; i++) {
        if (sCopies[i].image == image) {
            copy = &sCopies[i];
        }
    }
    if (copy == nullptr) {
        if (sNumCopies == kMaxCopies) {
            u32 oldest = 0;
            for (u32 i = 1; i < sNumCopies; i++) {
                if (sCopies[i].lastUsedFrame < sCopies[oldest].lastUsedFrame) {
                    oldest = i;
                }
            }
            RemoveCopy(oldest);
        }
        copy = &sCopies[sNumCopies++];
        std::memset(copy, 0, sizeof(*copy));
        copy->image = image;
        glGenTextures(1, &copy->texture);
    }
    copy->width = copy->height = 0; // not usable until the buffer is written
    copy->lastUsedFrame = sFrame;
    glBindTexture(GL_TEXTURE_2D, copy->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    return copy->texture;
}

void PCGXCopyTextureEnd(const void* image, u32 texWidth, u32 texHeight, u32 format) {
    for (u32 i = 0; i < sNumCopies; i++) {
        Copy& c = sCopies[i];
        if (c.image == image) {
            c.width = static_cast<u16>(texWidth);
            c.height = static_cast<u16>(texHeight);
            c.format = static_cast<u8>(format);
            c.dataSize = PCGXTextureDataSize(format, texWidth, texHeight);
            c.dataHash = PCGXHashBytes(image, c.dataSize);
            c.checkedGeneration = sGeneration;
            return;
        }
    }
}

void PCGXCopyTextureDrop(const void* image) {
    for (u32 i = 0; i < sNumCopies; i++) {
        if (sCopies[i].image == image) {
            RemoveCopy(i);
            return;
        }
    }
}

void PCGXCopyTextureDropAll() {
    while (sNumCopies != 0) {
        RemoveCopy(sNumCopies - 1);
    }
    PCGXTextureNewGeneration();
}

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

void PCGXSetTextureReplacer(const PCGXTextureReplacer* replacer) {
    sHaveReplacer = replacer != nullptr && replacer->wants != nullptr && replacer->make != nullptr;
    if (sHaveReplacer) {
        sReplacer = *replacer;
    }
    PCGXTextureNewGeneration();
}

f32 PCGXTextureReplacementLodBias() {
    return sHaveReplacer ? sReplacer.lodBias : 0.0f;
}

u32 PCGXTextureForUnit(u32 unitIndex, bool* mipmapped, u32* replacementLevels) {
    const PCGXTexUnit& unit = gPCGX.tex[unitIndex];
    *mipmapped = false;
    *replacementLevels = 0;
    if (unit.image == nullptr || unit.width == 0 || unit.height == 0) {
        return 0;
    }
    if (sNumCopies != 0) {
        GLuint copy = CopyForUnit(unit);
        if (copy != 0) {
            return copy;
        }
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
    // A replacement texture (pc_gx.h). The fragment shader has to take the
    // texture's size from the game and not from OpenGL, which it does in the
    // variant for a scaled or multisampled frame buffer only.
    if (sHaveReplacer && key.levels == 1 && !IsColorIndex(unit.format) && PCGXRenderEnhancedSampling() &&
        sReplacer.wants(unit.image, unit.format, unit.width, unit.height, sReplacer.user)) {
        key.replaced = 1;
    }
    const PCGXTlutSlot* tlut = IsColorIndex(unit.format) ? &gPCGX.tluts[unit.tlutSlot] : nullptr;

    if (sEntries == nullptr) {
        sEntries = static_cast<Entry*>(std::calloc(kMaxEntries, sizeof(Entry)));
    }

    Entry* found = nullptr;
    for (u32 i = 0; i < sNumEntries; i++) {
        Entry& e = sEntries[i];
        if (e.image == key.image && e.width == key.width && e.height == key.height && e.format == key.format &&
            e.levels == key.levels && e.hostOrder == key.hostOrder && e.tlutFormat == key.tlutFormat &&
            e.tlutHash == key.tlutHash && e.replaced == key.replaced) {
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
        *replacementLevels = found->replacementLevels;
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
    *replacementLevels = e.replacementLevels;
    return e.texture;
}
