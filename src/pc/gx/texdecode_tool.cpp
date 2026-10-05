// Development tools around the texture codec (texdecode.h): a PNG writer, a
// walk over the textures in the channel's contents, and the two command-line
// modes that use them:
//
//   newschannel --list-textures 9
//   newschannel --list-textures 9:news_layout.arc.LZ/arc/timg
//   newschannel --dump-texture 9:TPLCommon.tpl.LZ:0 build/scratch/first.png
//   newschannel --dump-texture 7:wbf1.brfna:3 build/scratch/sheet3.png
//
// Output files are pictures of the game's assets: write them to build/ or a
// scratch directory and never commit them (docs/pc_port.md, rule R12).
//
// Files are loaded the way the game loads them: CNT for a content file, CX for
// a compressed one, ARC for the members of an archive in memory, TPLBind() for
// a palette, ut::ResFont and ut::ArchiveFont for fonts. Headers are therefore
// in host byte order when a texture is reported and the texels are untouched.

#include "texdecode.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include <nw4r/ut.h>
#include <nw4r/ut/ut_ArchiveFont.h>
#include <revolution/arc.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/gx.h>
#include <revolution/tpl.h>

#include <pc/endian.h>
#include <pc/files.h>

using namespace nw4r;

// --- PNG ---------------------------------------------------------------------

namespace {

u32 sCrcTable[256];

u32 Crc32(u32 crc, const u8* data, u32 size) {
    if (sCrcTable[1] == 0) {
        for (u32 n = 0; n < 256; n++) {
            u32 c = n;
            for (u32 k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            sCrcTable[n] = c;
        }
    }
    crc = ~crc;
    for (u32 i = 0; i < size; i++) {
        crc = sCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

void PutBE32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v >> 24);
    p[1] = static_cast<u8>(v >> 16);
    p[2] = static_cast<u8>(v >> 8);
    p[3] = static_cast<u8>(v);
}

bool WriteChunk(std::FILE* file, const char* type, const u8* data, u32 size) {
    u8 head[8];
    PutBE32(head, size);
    std::memcpy(head + 4, type, 4);
    u8 tail[4];
    PutBE32(tail, Crc32(Crc32(0, head + 4, 4), data, size));
    return std::fwrite(head, 1, 8, file) == 8 && (size == 0 || std::fwrite(data, 1, size, file) == size) &&
           std::fwrite(tail, 1, 4, file) == 4;
}

} // namespace

// The image data is a zlib stream of stored (uncompressed) deflate blocks, so
// no compression library is needed.
bool PCGXWritePNG(const char* path, const u8* rgba, u32 width, u32 height) {
    if (path == nullptr || rgba == nullptr || width == 0 || height == 0 || width > 16384 || height > 16384) {
        return false;
    }
    const u32 rowBytes = width * 4 + 1; // filter byte + pixels
    const u32 rawSize = rowBytes * height;
    const u32 blocks = (rawSize + 65534) / 65535;
    const u32 zlibSize = 2 + blocks * 5 + rawSize + 4;
    u8* zlib = static_cast<u8*>(std::malloc(zlibSize));
    if (zlib == nullptr) {
        return false;
    }

    u8* p = zlib;
    *p++ = 0x78;
    *p++ = 0x01;
    u32 adlerA = 1, adlerB = 0;
    u32 inBlock = 0;   // bytes left in the current stored block
    u32 left = rawSize; // bytes of raw data not yet written
    auto emit = [&](const u8* bytes, u32 count) {
        while (count > 0) {
            if (inBlock == 0) {
                inBlock = left < 65535 ? left : 65535;
                *p++ = left <= 65535 ? 1 : 0; // BFINAL, BTYPE = 00
                *p++ = static_cast<u8>(inBlock);
                *p++ = static_cast<u8>(inBlock >> 8);
                *p++ = static_cast<u8>(~inBlock);
                *p++ = static_cast<u8>(~inBlock >> 8);
            }
            const u32 n = count < inBlock ? count : inBlock;
            std::memcpy(p, bytes, n);
            for (u32 i = 0; i < n; i++) {
                adlerA += bytes[i];
                if (adlerA >= 65521) {
                    adlerA -= 65521;
                }
                adlerB += adlerA;
                if (adlerB >= 65521) {
                    adlerB -= 65521;
                }
            }
            p += n;
            bytes += n;
            count -= n;
            inBlock -= n;
            left -= n;
        }
    };
    static const u8 filterNone = 0;
    for (u32 y = 0; y < height; y++) {
        emit(&filterNone, 1);
        emit(rgba + y * width * 4, width * 4);
    }
    PutBE32(p, adlerB << 16 | adlerA);
    p += 4;

    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        std::free(zlib);
        return false;
    }
    static const u8 signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    u8 header[13];
    PutBE32(header, width);
    PutBE32(header + 4, height);
    header[8] = 8;  // bits per channel
    header[9] = 6;  // RGBA
    header[10] = 0; // deflate
    header[11] = 0; // adaptive filtering
    header[12] = 0; // not interlaced
    bool ok = std::fwrite(signature, 1, 8, file) == 8 && WriteChunk(file, "IHDR", header, 13) &&
              WriteChunk(file, "IDAT", zlib, static_cast<u32>(p - zlib)) && WriteChunk(file, "IEND", nullptr, 0);
    ok = std::fclose(file) == 0 && ok;
    std::free(zlib);
    return ok;
}

bool PCGXDumpTexture(const char* path, const void* data, u32 fmt, u32 width, u32 height, const void* tlut, u32 tlutFmt,
                     u32 tlutCount, bool hostOrder16) {
    if (width == 0 || height == 0 || width > 1024 || height > 1024) {
        return false;
    }
    u8* rgba = static_cast<u8*>(std::malloc(width * height * 4));
    if (rgba == nullptr) {
        return false;
    }
    const bool ok = PCGXDecodeTexture(data, fmt, width, height, tlut, tlutFmt, tlutCount, hostOrder16, rgba) &&
                    PCGXWritePNG(path, rgba, width, height);
    std::free(rgba);
    return ok;
}

const char* PCGXTextureFormatName(u32 fmt) {
    switch (fmt) {
    case GX_TF_I4:
        return "I4";
    case GX_TF_I8:
        return "I8";
    case GX_TF_IA4:
        return "IA4";
    case GX_TF_IA8:
        return "IA8";
    case GX_TF_RGB565:
        return "RGB565";
    case GX_TF_RGB5A3:
        return "RGB5A3";
    case GX_TF_RGBA8:
        return "RGBA8";
    case GX_TF_C4:
        return "C4";
    case GX_TF_C8:
        return "C8";
    case GX_TF_C14X2:
        return "C14X2";
    case GX_TF_CMPR:
        return "CMPR";
    case GX_TF_Z8:
        return "Z8";
    case GX_TF_Z16:
        return "Z16";
    case GX_TF_Z24X8:
        return "Z24X8";
    case GX_CTF_R4:
        return "CTF_R4";
    case GX_CTF_RA4:
        return "CTF_RA4";
    case GX_CTF_RA8:
        return "CTF_RA8";
    case GX_CTF_YUVA8:
        return "CTF_YUVA8";
    case GX_CTF_A8:
        return "CTF_A8";
    case GX_CTF_R8:
        return "CTF_R8";
    case GX_CTF_G8:
        return "CTF_G8";
    case GX_CTF_B8:
        return "CTF_B8";
    case GX_CTF_RG8:
        return "CTF_RG8";
    case GX_CTF_GB8:
        return "CTF_GB8";
    default:
        return "?";
    }
}

// --- the textures in the contents --------------------------------------------

namespace {

void* ToolAlloc(MEMAllocator*, u32 size) {
    void* p = nullptr;
    return posix_memalign(&p, 32, size ? size : 32) == 0 ? p : nullptr;
}
void ToolFree(MEMAllocator*, void* block) {
    std::free(block);
}
const MEMAllocatorFunc sToolAllocFuncs = {ToolAlloc, ToolFree};
MEMAllocator sToolAllocator = {&sToolAllocFuncs, nullptr, 0, 0};

// ut::ResFont and ut::ArchiveFont keep the glyph block to themselves.
struct ResFontSheets : ut::ResFont {
    const ut::FontTextureGlyph* Glyphs() const { return GetFINF() != nullptr ? GetFINF()->pGlyph : nullptr; }
};
struct ArchiveFontSheets : ut::ArchiveFont {
    const ut::FontTextureGlyph* Glyphs() const { return GetFINF() != nullptr ? GetFINF()->pGlyph : nullptr; }
};

struct Walk {
    const char* filter; // path to visit (a file, a directory or an archive); "" for everything
    s32 index;          // texture of the file to visit, or -1 for all
    PCGXAssetTextureFunc func;
    void* user;
    s32 content; // index of the content being walked
    s32 visited;
    bool stop;
};

// Is `path` on the way to the filter, or below it?
bool Wanted(const Walk& walk, const char* path) {
    const size_t filterLength = std::strlen(walk.filter);
    const size_t pathLength = std::strlen(path);
    if (filterLength == 0 || pathLength == 0) {
        return true;
    }
    if (pathLength <= filterLength) {
        return strncasecmp(path, walk.filter, pathLength) == 0 &&
               (walk.filter[pathLength] == '\0' || walk.filter[pathLength] == '/');
    }
    return strncasecmp(path, walk.filter, filterLength) == 0 && path[filterLength] == '/';
}

// Is `path` the filter itself or below it? (Textures are only reported there.)
bool Selected(const Walk& walk, const char* path) {
    const size_t filterLength = std::strlen(walk.filter);
    return filterLength == 0 || (strncasecmp(path, walk.filter, filterLength) == 0 &&
                                 (path[filterLength] == '\0' || path[filterLength] == '/'));
}

void Report(Walk& walk, PCGXAssetTexture& texture) {
    if (walk.stop || (walk.index >= 0 && static_cast<u32>(walk.index) != texture.index)) {
        return;
    }
    walk.visited++;
    texture.content = static_cast<u32>(walk.content);
    if (!walk.func(&texture, walk.user)) {
        walk.stop = true;
    }
}

bool HasSuffix(const char* name, const char* suffix) {
    const size_t n = std::strlen(name);
    const size_t s = std::strlen(suffix);
    return n >= s && strcasecmp(name + n - s, suffix) == 0;
}

const char* BaseName(const char* path) {
    const char* slash = std::strrchr(path, '/');
    return slash != nullptr ? slash + 1 : path;
}

void VisitArchive(Walk& walk, const char* path, ARCHandle* handle, CNTHandle* content);

void VisitTPL(Walk& walk, const char* path, void* data, u32 size) {
    const u8* begin = static_cast<const u8*>(data);
    const u8* end = begin + size;
    TPLPalette* palette = static_cast<TPLPalette*>(data);
    if (size < sizeof(TPLPalette) || palette->numDescriptors == 0 || palette->numDescriptors > 4096) {
        return;
    }
    TPLBind(palette);
    for (u32 i = 0; i < palette->numDescriptors && !walk.stop; i++) {
        const TPLDescriptor* descriptor = TPLGet(palette, i);
        const TPLHeader* header = descriptor->textureHeader;
        const u8* texels = header != nullptr ? reinterpret_cast<const u8*>(header->data) : nullptr;
        if (texels == nullptr || texels < begin || texels >= end) {
            continue;
        }
        PCGXAssetTexture texture = {};
        std::snprintf(texture.path, sizeof(texture.path), "%s", path);
        texture.kind = "TPL";
        texture.index = i;
        texture.count = palette->numDescriptors;
        texture.data = texels;
        texture.dataSize = static_cast<u32>(end - texels);
        texture.fmt = header->format;
        texture.width = header->width;
        texture.height = header->height;
        texture.wrapS = header->wrapS;
        texture.wrapT = header->wrapT;
        texture.minFilter = header->minFilter;
        texture.magFilter = header->magFilter;
        texture.levels = header->maxLOD > header->minLOD ? header->maxLOD - header->minLOD + 1u : 1u;
        const TPLClutHeader* clut = descriptor->CLUTHeader;
        if (clut != nullptr && reinterpret_cast<const u8*>(clut->data) >= begin &&
            reinterpret_cast<const u8*>(clut->data) + clut->numEntries * 2 <= end) {
            texture.tlut = clut->data;
            texture.tlutFmt = clut->format;
            texture.tlutCount = clut->numEntries;
        }
        Report(walk, texture);
    }
}

void VisitSheets(Walk& walk, const char* path, const char* kind, const ut::FontTextureGlyph* glyphs) {
    if (glyphs == nullptr || glyphs->sheetImage == nullptr) {
        return;
    }
    for (u32 i = 0; i < glyphs->sheetNum && !walk.stop; i++) {
        PCGXAssetTexture texture = {};
        std::snprintf(texture.path, sizeof(texture.path), "%s", path);
        texture.kind = kind;
        texture.index = i;
        texture.count = glyphs->sheetNum;
        texture.data = glyphs->sheetImage + i * glyphs->sheetSize;
        texture.dataSize = glyphs->sheetSize;
        texture.fmt = glyphs->sheetFormat & 0x7FFF;
        texture.width = glyphs->sheetWidth;
        texture.height = glyphs->sheetHeight;
        texture.wrapS = texture.wrapT = GX_CLAMP;
        texture.minFilter = texture.magFilter = GX_LINEAR;
        texture.levels = 1;
        Report(walk, texture);
    }
}

void VisitArchiveFont(Walk& walk, const char* path, void* data) {
    static const char all[] = ""; // ut::ArchiveFont::LOAD_GLYPH_ALL
    const u32 need = ut::ArchiveFont::GetRequireBufferSize(data, all);
    if (need == 0 || need > 64u * 1024 * 1024) {
        return;
    }
    void* buffer = ToolAlloc(nullptr, need);
    if (buffer == nullptr) {
        return;
    }
    ArchiveFontSheets font;
    if (font.Construct(buffer, need, data, all)) {
        VisitSheets(walk, path, "RFNA", font.Glyphs());
        font.Destroy();
    }
    std::free(buffer);
}

// A complete, uncompressed file in memory. `data` is converted in place.
void VisitData(Walk& walk, const char* path, void* data, u32 size) {
    PCEndianFixFile(data, size);
    const char* format = PCEndianIdentify(data, size);
    if (format == nullptr) {
        return;
    }
    if (std::strcmp(format, "U8") == 0) {
        ARCHandle handle;
        if (ARCInitHandle(data, &handle)) {
            VisitArchive(walk, path, &handle, nullptr);
        }
    } else if (!Selected(walk, path)) {
        return;
    } else if (std::strcmp(format, "TPL") == 0) {
        VisitTPL(walk, path, data, size);
    } else if (std::strcmp(format, "RFNT") == 0) {
        ResFontSheets font;
        if (font.SetResource(data)) {
            VisitSheets(walk, path, "RFNT", font.Glyphs());
        }
    } else if (std::strcmp(format, "RFNA") == 0) {
        VisitArchiveFont(walk, path, data);
    }
}

// A complete file in memory, compressed or not. A compressed file is reported
// under its own name ("TPLCommon.tpl.LZ"), which is the name the game asks for.
void VisitFile(Walk& walk, const char* path, void* data, u32 size) {
    if (walk.stop || data == nullptr || size < 16) {
        return;
    }
    const char* name = BaseName(path);
    const u8 type = *static_cast<const u8*>(data) & 0xF0;
    const bool lz = (HasSuffix(name, ".LZ") || strncasecmp(name, "LZ77", 4) == 0) && type == 0x10;
    const bool huffman = strncasecmp(name, "Huf8", 4) == 0 && type == 0x20;
    if (!lz && !huffman) {
        VisitData(walk, path, data, size);
        return;
    }
    const u32 length = CXGetUncompressedSize(data);
    if (length < 16 || length > 64u * 1024 * 1024) {
        return;
    }
    void* buffer = ToolAlloc(nullptr, length);
    if (buffer == nullptr) {
        return;
    }
    if (lz) {
        CXUncompressLZ(data, buffer);
    } else {
        CXUncompressHuffman(data, buffer);
    }
    VisitData(walk, path, buffer, length);
    std::free(buffer);
}

// Could this file hold textures? Only such files are read from a content.
bool Interesting(const char* name) {
    static const char* const suffixes[] = {".tpl", ".brfnt", ".brfna", ".arc", ".LZ"};
    for (const char* suffix : suffixes) {
        if (HasSuffix(name, suffix)) {
            return true;
        }
    }
    return strncasecmp(name, "LZ77", 4) == 0 || strncasecmp(name, "Huf8", 4) == 0;
}

void VisitDirectory(Walk& walk, const char* path, const char* arcPath, ARCHandle* handle, CNTHandle* content) {
    ARCDir dir;
    if (!ARCOpenDir(handle, arcPath, &dir)) {
        return;
    }
    ARCDirEntry entry;
    while (!walk.stop && ARCReadDir(&dir, &entry)) {
        char childPath[256];
        char childArcPath[256];
        std::snprintf(childPath, sizeof(childPath), "%s%s%s", path, path[0] != '\0' ? "/" : "", entry.name);
        std::snprintf(childArcPath, sizeof(childArcPath), "%s/%s", std::strcmp(arcPath, "/") == 0 ? "" : arcPath,
                      entry.name);
        if (!Wanted(walk, childPath)) {
            continue;
        }
        if (entry.isDir) {
            VisitDirectory(walk, childPath, childArcPath, handle, content);
            continue;
        }
        if (!Interesting(entry.name)) {
            continue;
        }
        if (content != nullptr) {
            // A content file: only the node table is in memory.
            CNTFileInfo info;
            if (contentFastOpenNAND(content, static_cast<s32>(entry.entryNum), &info) != 0) {
                continue;
            }
            const u32 length = contentGetLengthNAND(&info);
            const u32 rounded = (length + 31) & ~31u;
            void* buffer = length != 0 ? ToolAlloc(nullptr, rounded) : nullptr;
            if (buffer != nullptr && contentReadNAND(&info, buffer, rounded, 0) > 0) {
                VisitFile(walk, childPath, buffer, length);
            }
            std::free(buffer);
            contentCloseNAND(&info);
        } else {
            ARCFileInfo info;
            if (!ARCFastOpen(handle, static_cast<s32>(entry.entryNum), &info)) {
                continue;
            }
            VisitFile(walk, childPath, ARCGetStartAddrInMem(&info), ARCGetLength(&info));
            ARCClose(&info);
        }
    }
    ARCCloseDir(&dir);
}

void VisitArchive(Walk& walk, const char* path, ARCHandle* handle, CNTHandle* content) {
    VisitDirectory(walk, path, "/", handle, content);
}

struct Spec {
    s32 content; // -1: every content
    char path[256];
    s32 index;
};

// CONTENT[:PATH[:INDEX]], or "all".
bool ParseSpec(const char* text, Spec* spec) {
    spec->content = -1;
    spec->path[0] = '\0';
    spec->index = -1;
    if (text == nullptr || std::strcmp(text, "all") == 0) {
        return text != nullptr;
    }
    char* end;
    const long content = std::strtol(text, &end, 10);
    if (end == text || content < 0 || content > 255 || (*end != '\0' && *end != ':')) {
        return false;
    }
    spec->content = static_cast<s32>(content);
    if (*end == '\0') {
        return true;
    }
    const char* path = end + 1;
    while (*path == '/') {
        path++;
    }
    const char* colon = std::strrchr(path, ':');
    size_t length = std::strlen(path);
    if (colon != nullptr) {
        const long index = std::strtol(colon + 1, &end, 10);
        if (end == colon + 1 || *end != '\0' || index < 0) {
            return false;
        }
        spec->index = static_cast<s32>(index);
        length = static_cast<size_t>(colon - path);
    }
    while (length > 0 && path[length - 1] == '/') {
        length--;
    }
    if (length >= sizeof(spec->path)) {
        return false;
    }
    std::memcpy(spec->path, path, length);
    spec->path[length] = '\0';
    return true;
}

} // namespace

s32 PCGXForEachAssetTexture(const char* specText, PCGXAssetTextureFunc func, void* user) {
    Spec spec;
    if (func == nullptr || !ParseSpec(specText, &spec)) {
        return -1;
    }
    Walk walk = {spec.path, spec.index, func, user, 0, 0, false};
    bool opened = false;
    CNTInit();
    const s32 first = spec.content >= 0 ? spec.content : 0;
    const s32 last = spec.content >= 0 ? spec.content : 15;
    for (s32 number = first; number <= last && !walk.stop; number++) {
        if (!PCContentExists(number)) {
            continue;
        }
        // A content that is not a U8 archive (11) is refused by CNT with a
        // message; ask first when walking everything.
        if (spec.content < 0) {
            char file[600];
            std::snprintf(file, sizeof(file), "%s/%02d.app", PCGetContentsDir(), number);
            u8 magic[4] = {};
            std::FILE* probe = std::fopen(file, "rb");
            const bool archive = probe != nullptr && std::fread(magic, 1, 4, probe) == 4 &&
                                 PCReadBE32(magic) == 0x55AA382D;
            if (probe != nullptr) {
                std::fclose(probe);
            }
            if (!archive) {
                continue;
            }
        }
        CNTHandle content;
        std::memset(&content, 0, sizeof(content));
        if (contentInitHandleNAND(number, &content, &sToolAllocator) != 0) {
            continue;
        }
        opened = true;
        walk.content = number;
        VisitArchive(walk, "", &content.arcHandle, &content);
        contentReleaseHandleNAND(&content);
    }
    return opened ? walk.visited : -1;
}

// --- command line ------------------------------------------------------------

namespace {

struct ListState {
    u32 total;
    u32 perFormat[64];
    u32 failed;
};

bool ListOne(const PCGXAssetTexture* texture, void* user) {
    ListState* state = static_cast<ListState*>(user);
    state->total++;
    state->perFormat[texture->fmt & 63]++;
    static const char* const wraps[] = {"clamp", "repeat", "mirror"};
    std::printf("%u:%-54s %3u/%-3u %4ux%-4u %-7s", texture->content, texture->path, texture->index, texture->count, texture->width,
                texture->height, PCGXTextureFormatName(texture->fmt));
    if (texture->tlut != nullptr) {
        static const char* const tlutNames[] = {"IA8", "RGB565", "RGB5A3"};
        std::printf(" palette %s x%u", texture->tlutFmt < 3 ? tlutNames[texture->tlutFmt] : "?", texture->tlutCount);
    }
    static const char* const filters[] = {"near", "linear", "near_mip_near", "lin_mip_near", "near_mip_lin",
                                          "lin_mip_lin"};
    std::printf(" %s/%s %s/%s", texture->wrapS < 3 ? wraps[texture->wrapS] : "?",
                texture->wrapT < 3 ? wraps[texture->wrapT] : "?",
                texture->minFilter < 6 ? filters[texture->minFilter] : "?",
                texture->magFilter < 6 ? filters[texture->magFilter] : "?");
    // All levels of a texture with mipmaps, one after the other.
    u32 bytes = 0;
    u32 width = texture->width, height = texture->height;
    for (u32 level = 0; level < texture->levels; level++) {
        bytes += PCGXTextureDataSize(texture->fmt, width, height);
        width = width > 1 ? width >> 1 : 1;
        height = height > 1 ? height >> 1 : 1;
    }
    if (texture->levels > 1) {
        std::printf(" %u levels", texture->levels);
    }
    if (bytes > texture->dataSize) {
        std::printf(" (image runs past the end of the file)");
        state->failed++;
    }
    std::printf("\n");
    return true;
}

struct DumpState {
    const char* outPath;
    bool ok;
};

bool DumpOne(const PCGXAssetTexture* texture, void* user) {
    DumpState* state = static_cast<DumpState*>(user);
    state->ok = PCGXDumpTexture(state->outPath, texture->data, texture->fmt, texture->width, texture->height,
                                texture->tlut, texture->tlutFmt, texture->tlutCount, false);
    std::printf("%u:%s [%u of %u]: %ux%u %s -> %s%s\n", texture->content, texture->path, texture->index, texture->count, texture->width,
                texture->height, PCGXTextureFormatName(texture->fmt), state->outPath, state->ok ? "" : " FAILED");
    return false; // the first match only
}

} // namespace

int PCGXListTexturesMain(const char* spec) {
    ListState state = {};
    const s32 count = PCGXForEachAssetTexture(spec, ListOne, &state);
    if (count < 0) {
        std::fprintf(stderr, "--list-textures: bad argument '%s' or no contents in '%s'\n", spec, PCGetContentsDir());
        std::fprintf(stderr, "  use CONTENT[:PATH[:INDEX]] (for example 9, 9:TPLCommon.tpl.LZ) or 'all'\n");
        return 2;
    }
    std::printf("%u textures", state.total);
    for (u32 fmt = 0; fmt < 64; fmt++) {
        if (state.perFormat[fmt] != 0) {
            std::printf(", %u %s", state.perFormat[fmt], PCGXTextureFormatName(fmt));
        }
    }
    std::printf("\n");
    return state.failed == 0 ? 0 : 1;
}

int PCGXDumpTextureMain(const char* spec, const char* outPath) {
    DumpState state = {outPath, false};
    const s32 count = PCGXForEachAssetTexture(spec, DumpOne, &state);
    if (count < 0) {
        std::fprintf(stderr, "--dump-texture: bad argument '%s' or no contents in '%s'\n", spec, PCGetContentsDir());
        std::fprintf(stderr, "  use CONTENT:PATH[:INDEX], for example 9:TPLCommon.tpl.LZ:0\n");
        return 2;
    }
    if (count == 0) {
        std::fprintf(stderr, "--dump-texture: no texture matches '%s' (try --list-textures)\n", spec);
        return 1;
    }
    return state.ok ? 0 : 1;
}
