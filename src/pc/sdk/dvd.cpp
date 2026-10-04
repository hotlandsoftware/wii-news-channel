// DVD: the disc drive. A channel has no disc.
//
// The game code makes no DVD calls. NW4R does, in code paths the News Channel
// does not take with a disc file: ut::DvdFileStream / snd::DvdSoundArchive
// (the game opens its sound archive from memory) and the drive-status poll in
// snd::AxManager::Update() and snd::StrmPlayer.
//
// So this is the minimal faithful behaviour of a drive with no disc file
// system mounted: every path lookup fails, nothing can be opened or read, and
// the drive reports "idle, no error" (DVD_STATE_END) so that the sound system
// does not think a disc error is in progress.
//
// If a later milestone wants loose files (for example a replacement sound
// archive), map DVDConvertPathToEntrynum/DVDFastOpen/DVDReadPrio to a host
// directory here; nothing else needs to change.

#include <revolution/dvd.h>

extern "C" {

// Declared by src/nw4r/ut/ut_DvdFileStream.cpp, not by <revolution/dvd.h>.
BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback);

s32 DVDConvertPathToEntrynum(const char* path) {
    (void)path;
    return -1; // not found
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    (void)entrynum;
    (void)fileInfo;
    return FALSE;
}

BOOL DVDOpen(const char* path, DVDFileInfo* fileInfo) {
    (void)path;
    (void)fileInfo;
    return FALSE;
}

BOOL DVDClose(DVDFileInfo* fileInfo) {
    (void)fileInfo;
    return TRUE;
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    (void)fileInfo;
    (void)addr;
    (void)length;
    (void)offset;
    (void)prio;
    return -1; // DVD_RESULT_FATAL_ERROR: no file can be open
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback,
                      s32 prio) {
    (void)fileInfo;
    (void)addr;
    (void)length;
    (void)offset;
    (void)callback;
    (void)prio;
    return FALSE; // the request was not queued; the callback is not called
}

s32 DVDCancel(DVDCommandBlock* block) {
    (void)block;
    return 0; // nothing in progress
}

BOOL DVDCancelAsync(DVDCommandBlock* block, DVDCBCallback callback) {
    // Nothing is ever in progress: the cancel completes at once.
    if (callback != nullptr) {
        callback(0, block);
    }
    return TRUE;
}

s32 DVDGetDriveStatus(void) {
    return DVD_STATE_END;
}

} // extern "C"
