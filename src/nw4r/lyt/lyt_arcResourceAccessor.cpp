#include <nw4r/lyt/lyt_arcResourceAccessor.h>

#include <revolution/arc.h>
#include <string.h>

extern "C" int stricmp(const char* a, const char* b);

namespace {

s32 FindNameResource(ARCHandle* pArcHandle, const char* resName) {
    s32 entryNum = -1;

    ARCDir dir;
    ARCOpenDir(pArcHandle, ".", &dir);

    ARCDirEntry dirEntry;
    while (ARCReadDir(&dir, &dirEntry)) {
        if (dirEntry.isDir != 0) {
            ARCChangeDir(pArcHandle, dirEntry.name);
            entryNum = FindNameResource(pArcHandle, resName);
            ARCChangeDir(pArcHandle, "..");

            if (entryNum != -1) {
                break;
            }
        } else if (stricmp(resName, dirEntry.name) == 0) {
            entryNum = dirEntry.entryNum;
            break;
        }
    }

    ARCCloseDir(&dir);
    return entryNum;
}

void* GetResourceSub(ARCHandle* pArcHandle, const char* resRootDir, u32 resType, const char* name,
                     u32* pSize) {
    s32 entryNum = -1;

    if (ARCConvertPathToEntrynum(pArcHandle, resRootDir) != -1 && ARCChangeDir(pArcHandle, resRootDir)) {
        if (resType == 0) {
            entryNum = FindNameResource(pArcHandle, name);
        } else {
            char resTypeStr[5];
            resTypeStr[0] = resType >> 24;
            resTypeStr[1] = resType >> 16;
            resTypeStr[2] = resType >> 8;
            resTypeStr[3] = resType;
            resTypeStr[4] = '\0';

            if (ARCConvertPathToEntrynum(pArcHandle, resTypeStr) != -1 &&
                ARCChangeDir(pArcHandle, resTypeStr)) {
                entryNum = ARCConvertPathToEntrynum(pArcHandle, name);
                ARCChangeDir(pArcHandle, "..");
            }
        }

        ARCChangeDir(pArcHandle, "..");
    }

    if (entryNum != -1) {
        ARCFileInfo arcFileInfo;
        ARCFastOpen(pArcHandle, entryNum, &arcFileInfo);

        void* resPtr = ARCGetStartAddrInMem(&arcFileInfo);
        if (pSize) {
            *pSize = ARCGetLength(&arcFileInfo);
        }

        ARCClose(&arcFileInfo);
        return resPtr;
    }

    return NULL;
}

} // namespace

namespace nw4r {
namespace lyt {

ut::Font* detail::FindFont(FontRefLinkList* pFontRefList, const char* name) {
    for (FontRefLinkList::Iterator it = pFontRefList->GetBeginIter(); it != pFontRefList->GetEndIter();
         it++) {
        if (strcmp(name, it->GetFontName()) == 0) {
            return it->GetFont();
        }
    }
    return NULL;
}

ArcResourceAccessor::ArcResourceAccessor() : mArcBuf(NULL) {}

bool ArcResourceAccessor::Attach(void* archiveStart, const char* resourceRootDirectory) {
    BOOL bSuccess = ARCInitHandle(archiveStart, &mArcHandle);
    if (!bSuccess) {
        return false;
    }

    mArcBuf = archiveStart;

    strncpy(mResRootDir, resourceRootDirectory, ARRAY_SIZE(mResRootDir) - 1);
    mResRootDir[ARRAY_SIZE(mResRootDir) - 1] = '\0';

    return true;
}

void* ArcResourceAccessor::Detach() {
    void* buf = mArcBuf;
    mArcBuf = NULL;
    return buf;
}

void* ArcResourceAccessor::GetResource(u32 resType, const char* name, u32* pSize) {
    return GetResourceSub(&mArcHandle, mResRootDir, resType, name, pSize);
}

ut::Font* ArcResourceAccessor::GetFont(const char* name) {
    return detail::FindFont(&mFontList, name);
}

} // namespace lyt
} // namespace nw4r
