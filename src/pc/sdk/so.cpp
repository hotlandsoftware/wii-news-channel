// SO: the socket library, as far as the game uses it -- starting the network.
// NCD: the connection profiles of the Wii's Internet settings.
//
// The game starts the network before it asks WiiConnect24 to download
// (CWiiConnect24::execDownload: SOInit, SOStartup). On PC the "link" is the
// news source (src/pc/news/pc_news.h): SOStartup() succeeds when the source can
// be reached, and fails with SO_ERR_LINK_UP_TIMEOUT otherwise, which the SDK's
// NETGetStartupErrorCode() (compiled natively) turns into error 51099, "unable
// to connect to the Internet". No socket is ever opened here; an HTTP news
// source would use libcurl, not SO.

#include <revolution/so.h>

#include <revolution/ncd.h>

#include "../news/pc_news.h"

namespace {

bool sInitialized;
bool sStarted;

} // namespace

extern "C" {

int SOInit(const SOLibraryConfig* /*config*/) {
    sInitialized = true;
    return SO_SUCCESS;
}

int SOFinish(void) {
    sInitialized = false;
    sStarted = false;
    return SO_SUCCESS;
}

// Brings the network interface up and waits for an address.
int SOStartup(void) {
    if (!PCNewsGetSource()->available()) {
        return SO_ERR_LINK_UP_TIMEOUT;
    }
    sStarted = true;
    return SO_SUCCESS;
}

int SOStartupEx(int /*timeout*/) {
    return SOStartup();
}

int SOCleanup(void) {
    sStarted = false;
    return SO_SUCCESS;
}

// The enabled connection profiles, for NETGetStartupErrorCode(): none, which
// selects the "no link" codes.
s32 NCDiGetEnabledConfigList(u32* enabled, u32* wireless, u32* wired) {
    *enabled = 0;
    *wireless = 0;
    *wired = 0;
    return 0;
}

} // extern "C"
