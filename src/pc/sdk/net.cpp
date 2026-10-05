// NET: universal time. (The error-code and CRC functions of NET are the SDK's
// own source, compiled natively: pc/ported/sdk_net.txt.)
//
// On the console universal time is the clock (local time) plus the difference
// WiiConnect24 learnt from its server. Here it is the game's clock converted
// with the host's time zone: the host's UTC, or what `--date` set
// (src/pc/sdk/os_time.cpp). The news files carry times in minutes of this
// clock, and the game picks the file of the current universal hour.

#include <revolution/net.h>

#include <ctime>

#include <pc/os.h>
#include <revolution/os.h>

extern "C" {

BOOL NETGetUniversalCalendar(OSCalendarTime* calendar) {
    u32 microseconds = 0;
    time_t seconds = static_cast<time_t>(PCOSGetUnixTime(&microseconds));
    struct tm utc;
    gmtime_r(&seconds, &utc);

    calendar->sec = utc.tm_sec;
    calendar->min = utc.tm_min;
    calendar->hour = utc.tm_hour;
    calendar->mday = utc.tm_mday;
    calendar->mon = utc.tm_mon;
    calendar->year = utc.tm_year + 1900;
    calendar->wday = utc.tm_wday;
    calendar->yday = utc.tm_yday;
    calendar->msec = static_cast<int>(microseconds / 1000);
    calendar->usec = static_cast<int>(microseconds % 1000);
    return TRUE;
}

} // extern "C"
