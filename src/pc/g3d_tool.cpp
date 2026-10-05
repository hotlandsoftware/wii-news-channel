// `newschannel --view-model CONTENT:PATH`: the globe without the news.
//
// In the game the globe is only reachable after a news file has loaded. This
// development mode shows it on its own, with as little code of its own as
// possible:
//
//   - the game's SystemInit() starts everything (heaps, VI, GX, g3d, the
//     news scene with its Globe, Camera and GlobeDots objects);
//   - the model file is read like EarthLoadThread() in d_scene.cpp reads it
//     (CNT, the streaming LZ decompressor, then the byte-order converter) and
//     handed to the game's Model class, as Scene::Execute() does;
//   - every frame runs the calls MainScreen makes for the globe (Globe::Calc,
//     CalcPoles, ApplyCamera, UpdateLights, CalcScene) and the globe part of
//     NewsScene::Draw() (GlobeDots::Draw, Globe::Draw) between the frame setup
//     and the display copy of SystemDraw().
//
// So the model is drawn by nw4r::g3d (ScnRoot, ScnMdlSimple) exactly as in
// the game; what is missing is everything else the news scene draws.
//
//   newschannel --view-model 8:earth.brres.LZ --no-window --frames 20 ...
//       --screenshot 15 --screenshot-dir build/shots
//   newschannel --view-model 8:earth.brres.LZ            (window; spins slowly)
//
// Options: --view-rot LAT,LON (degrees), --view-zoom LEVEL (0 nearest to 9),
// --view-tilt LEVEL (0 to 10, 5 is level), --view-spin DEGREES per frame.
// Screenshots show the game's assets: never commit them.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <news/Camera.h>
#include <news/Globe.h>
#include <news/GlobeDots.h>
#include <news/Model.h>
#include <news/System.h>
#include <nw4r/g3d.h>
#include <revolution/cnt.h>
#include <revolution/cx.h>
#include <revolution/gx.h>
#include <revolution/mem.h>
#include <revolution/os.h>
#include <revolution/vi.h>

#include <pc/endian.h>
#include <pc/os.h>

#include "gx/pc_gx.h"
#include "pc_config.h"
#include "pc_g3d_tool.h"
#include "pc_video.h"

using namespace nw4r;

// The game's own objects (d_scene.cpp, d_s_news.cpp, System.cpp).
extern Globe* gGlobe;
extern Model* gEarthModel;
extern s32 sEarthFadeAlpha;
extern GlobeDots* sGlobeRenderer;
extern CNTHandle gContentHandles[10];
extern bool gVIBlackPending;
extern MEMHeapHandle sAppHeap;

namespace {

// EarthLoadThread() of d_scene.cpp, on this thread: the file in pieces through
// the streaming decompressor. A file that is not compressed is read as it is.
void* LoadModelFile(u32 content, const char* path, u32* size) {
    if (content < 6 || content > 11) {
        std::fprintf(stderr, "view-model: the game opens contents 6 to 11 only\n");
        return nullptr;
    }
    CNTHandle* handle = &gContentHandles[content - 2];
    CNTFileInfo info;
    if (contentOpenNAND(handle, path, &info) != 0) {
        std::fprintf(stderr, "view-model: no file '%s' in content %u\n", path, content);
        return nullptr;
    }
    const u32 fileSize = OSRoundUp32B(contentGetLengthNAND(&info));
    u8 header[32] ATTRIBUTE_ALIGN(32);
    if (contentReadNAND(&info, header, sizeof(header), 0) <= 0) {
        contentCloseNAND(&info);
        return nullptr;
    }
    const bool compressed = (header[0] & 0xF0) == 0x10;
    const u32 dataSize = compressed ? CXGetUncompressedSize(header) : fileSize;
    // The scene's own heap, as LoadEarth() does
    void* data = MEMAllocFromExpHeapEx(sAppHeap, OSRoundUp32B(dataSize), -32);
    const u32 chunkSize = 0x10000;
    void* chunk = MEMAllocFromExpHeapEx(sAppHeap, chunkSize, -32);
    if (data == nullptr || chunk == nullptr) {
        std::fprintf(stderr, "view-model: no memory for %u bytes\n", dataSize);
        contentCloseNAND(&info);
        return nullptr;
    }

    CXUncompContextLZ context;
    CXInitUncompContextLZ(&context, data);
    bool ok = true;
    for (u32 offset = 0; ok && offset < fileSize; offset += chunkSize) {
        const u32 length = fileSize - offset < chunkSize ? fileSize - offset : chunkSize;
        if (contentReadNAND(&info, chunk, length, offset) <= 0) {
            ok = false;
        } else if (compressed) {
            CXReadUncompLZ(&context, chunk, length);
        } else {
            std::memcpy(static_cast<u8*>(data) + offset, chunk, length);
        }
    }
    contentCloseNAND(&info);
    MEMFreeToExpHeap(sAppHeap, chunk);
    if (!ok || (compressed && (context.destCount > 0 || context.headerSize != 0))) {
        std::fprintf(stderr, "view-model: reading '%s' failed\n", path);
        return nullptr;
    }
    *size = dataSize;
    return data;
}

// The frame of SystemDraw() (System.cpp) with only the globe in it.
void DrawFrame() {
    GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
    GXInvalidateVtxCache();
    GXInvalidateTexAll();

    // NewsScene::Draw(): the dots behind the globe, then the globe
    if (sGlobeRenderer != nullptr && gCursorTpl != nullptr) {
        sGlobeRenderer->UpdateAlpha(gGlobe->mOffsetX, gGlobe->mOffsetY);
        sGlobeRenderer->Draw();
    }
    gGlobe->Draw();

    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetColorUpdate(GX_TRUE);
    GXCopyDisp(gCurXfb, GX_TRUE);
    GXDrawDone();
    VISetNextFrameBuffer(gCurXfb);
    if (gVIBlackPending) {
        VISetBlack(FALSE);
        gVIBlackPending = false;
    }
    VIFlush();
    VIWaitForRetrace();
    gCurXfb = gCurXfb == gXfb1 ? gXfb2 : gXfb1;
}

} // namespace

int PCViewModelMain(const PCViewModelOptions* options) {
    const char* colon = std::strchr(options->spec, ':');
    char* end = nullptr;
    const unsigned long content = std::strtoul(options->spec, &end, 10);
    if (colon == nullptr || end != colon || colon[1] == '\0') {
        std::fprintf(stderr, "view-model: use CONTENT:PATH, for example 8:earth.brres.LZ\n");
        return 2;
    }

    // --frames belongs to this loop, not to the VI backend's frame limit.
    PCConfig* config = PCGetConfig();
    const s32 frames = config->maxFrames;
    config->maxFrames = 0;

    // The game's start-up, with its news scene (and so its Globe).
    SystemInit();
    if (gGlobe == nullptr || gFatalError) {
        std::fprintf(stderr, "view-model: the game did not create its news scene\n");
        return 1;
    }

    u32 size = 0;
    void* data = LoadModelFile(static_cast<u32>(content), colon + 1, &size);
    if (data == nullptr) {
        return 1;
    }
    // Scene::Execute(): the file is complete now, so its byte order can be
    // converted; then the model.
    if (PCEndianFixFile(data, size) != PC_ENDIAN_SWAPPED || !PCEndianIsHostOrder(data, 4)) {
        std::fprintf(stderr, "view-model: '%s' is not a resource file that can be converted\n", colon + 1);
        return 1;
    }
    gEarthModel = new (-32) Model(data);
    if (gEarthModel == nullptr || gEarthModel->GetScnMdl() == nullptr) {
        std::fprintf(stderr, "view-model: no model in '%s'\n", colon + 1);
        return 1;
    }
    g3d::ResMdl mdl = gEarthModel->GetResMdl();
    std::printf("view-model: %s: %u nodes, %u materials, %u shapes\n", colon + 1, mdl.GetResNodeNumEntries(),
                mdl.GetResMatNumEntries(), mdl.GetResShpNumEntries());

    // What the news scene does before it shows the globe (ShowGlobe in
    // d_s_news.cpp: Init, SetTiltNow; OnHomeMenuClose: Reset for the light).
    math::VEC3 rot(options->latitude, options->longitude, 0.0f);
    gGlobe->Init(&rot, options->zoom);
    gGlobe->Reset(&rot);
    gGlobe->SetTiltNow(options->tilt);
    gGlobe->SetTwist(0.0f);
    sEarthFadeAlpha = 0; // Scene::Execute() fades the globe in; show it at once
    if (sGlobeRenderer != nullptr) {
        sGlobeRenderer->ResetAlpha();
    }

    for (s32 frame = 0; frames <= 0 || frame < frames; frame++) {
        g3d::G3dReset(); // Scene::Execute()

        gGlobe->mCamera->mTargetRot.y += options->spin;

        // MainScreen's globe update
        gGlobe->Calc();
        gGlobe->CalcPoles();
        gGlobe->ApplyCamera();
        gGlobe->UpdateLights();
        gGlobe->CalcScene();

        if (frame == 0) {
            std::printf("view-model: first frame is shown at retrace %u\n", PCGXCurrentFrame());
            std::fflush(stdout);
        }
        DrawFrame();
    }

    const PCGXStats* stats = PCGXGetStats();
    std::printf("view-model: %d frames; GX: %u primitives, %u vertices, %u display lists, %u bad FIFO bytes, "
                "%u programs, %u textures\n",
                frames, stats->draws, stats->vertices, stats->displayLists, stats->badCommands, stats->programs,
                stats->textures);
    return stats->badCommands == 0 ? 0 : 1;
}
