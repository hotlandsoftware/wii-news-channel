#include <revolution/os.h>
#include <revolution/nwc24/internal/NWC24iDateParser.h>
#include <revolution/nwc24/internal/NWC24iTime.h>

// From doldecomp/ogws (src/revolution/NET/nettime.c).

BOOL NETGetUniversalCalendar(OSCalendarTime* pTime) {
    static s64 whenCached = 0;
    NWC24Date date;
    s64 universalTime;

    if (whenCached == 0 || whenCached + OSSecondsToTicks(60) < __OSGetSystemTime()) {
        NWC24iSynchronizeRtcCounter(FALSE);
        whenCached = __OSGetSystemTime();
    }

    if (NWC24iGetUniversalTime(&universalTime) >= 0 && NWC24iEpochSecondsToDate(&date, universalTime) >= 0 &&
        NWC24iDateToOSCalendarTime(pTime, &date) >= 0) {
        return TRUE;
    }

    OSTicksToCalendarTime(OSGetTime(), pTime);
    return FALSE;
}
