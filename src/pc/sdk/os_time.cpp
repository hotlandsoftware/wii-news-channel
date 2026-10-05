// OS time: OSGetTime, OSGetTick, calendar conversion.
//
// The Wii's time base counts OS_TIMER_CLOCK ticks per second (a quarter of the
// bus clock: 60,750,000 Hz) and OSGetTime() is the number of ticks since
// 2000-01-01 00:00 of the console's clock, which is local time (the console
// has no time zone). Here the value is the host's local time when the clock is
// first read, plus the monotonic time elapsed since then, so it never jumps.
//
// PCOSSetClock() (`--date`, $NEWSCHANNEL_DATE) replaces the wall clock with a
// given instant for reproducible runs: the game's clock starts there and keeps
// running. PCOSGetUnixTime() is the same clock as universal time, for the
// backends that need UTC (NETGetUniversalCalendar, NWC24).
//
// The calendar functions are the SDK's (src/revolution/OS/OSTime.c).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <pc/os.h>

#include "os_internal.h"

namespace {

const s64 kNanosecondsPerSecond = 1000000000LL;
// Seconds from 1970-01-01 to 2000-01-01.
const s64 kUnixToOSEpoch = 946684800LL;

pthread_once_t sOnce = PTHREAD_ONCE_INIT;
struct timespec sAnchor; // CLOCK_MONOTONIC at the first read
s64 sBaseTicks;          // OS time at the first read
s64 sUtcOffset;          // seconds: local time minus universal time, at the first read
bool sInitialized;
bool sClockSet;          // PCOSSetClock() was called, or $NEWSCHANNEL_DATE is set
s64 sClockUnix;          // its value: seconds since 1970-01-01 UTC

s64 TimerClock() {
    return static_cast<s64>(OS_TIMER_CLOCK);
}

s64 NanosecondsToTicks(s64 nsec) {
    s64 clock = TimerClock();
    return (nsec / kNanosecondsPerSecond) * clock + (nsec % kNanosecondsPerSecond) * clock / kNanosecondsPerSecond;
}

void InitTime() {
    struct timespec wall;
    clock_gettime(CLOCK_REALTIME, &wall);
    clock_gettime(CLOCK_MONOTONIC, &sAnchor);

    s64 unixSeconds = static_cast<s64>(wall.tv_sec);
    if (!sClockSet) {
        const char* env = std::getenv("NEWSCHANNEL_DATE");
        if (env != nullptr && env[0] != '\0') {
            if (PCOSParseDate(env, &sClockUnix)) {
                sClockSet = true;
            } else {
                std::fprintf(stderr, "NEWSCHANNEL_DATE: cannot read '%s' (YYYY-MM-DDTHH:MM[:SS][Z])\n", env);
            }
        }
    }
    if (sClockSet) {
        unixSeconds = sClockUnix;
        wall.tv_nsec = 0;
    }

    struct tm local;
    time_t seconds = static_cast<time_t>(unixSeconds);
    localtime_r(&seconds, &local);

    sUtcOffset = local.tm_gmtoff;
    s64 localSeconds = unixSeconds + sUtcOffset - kUnixToOSEpoch;
    sBaseTicks = localSeconds * TimerClock() + NanosecondsToTicks(wall.tv_nsec);
    sInitialized = true;
}

} // namespace

// "YYYY-MM-DDTHH:MM[:SS]" (a space instead of the T is accepted): local time,
// or universal time with a trailing Z.
BOOL PCOSParseDate(const char* text, s64* unixSeconds) {
    int year, month, day, hour, minute, second = 0;
    char sep = 0;
    int used = 0;
    if (text == nullptr ||
        std::sscanf(text, "%d-%d-%d%c%d:%d%n", &year, &month, &day, &sep, &hour, &minute, &used) != 6 ||
        (sep != 'T' && sep != 't' && sep != ' ')) {
        return FALSE;
    }
    const char* rest = text + used;
    if (*rest == ':') {
        int n = 0;
        if (std::sscanf(rest, ":%d%n", &second, &n) != 1) {
            return FALSE;
        }
        rest += n;
    }
    bool utc = false;
    if (*rest == 'Z' || *rest == 'z') {
        utc = true;
        rest++;
    }
    // The upper limit is the 32-bit time_t of this build.
    if (*rest != '\0' || year < 2000 || year > 2037 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 ||
        hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        return FALSE;
    }
    struct tm when;
    std::memset(&when, 0, sizeof(when));
    when.tm_year = year - 1900;
    when.tm_mon = month - 1;
    when.tm_mday = day;
    when.tm_hour = hour;
    when.tm_min = minute;
    when.tm_sec = second;
    when.tm_isdst = -1;
    time_t result = utc ? timegm(&when) : mktime(&when);
    if (result == static_cast<time_t>(-1)) {
        return FALSE;
    }
    *unixSeconds = static_cast<s64>(result);
    return TRUE;
}

// Sets the clock. Meant to be called before the game starts: alarms and sleeps
// that are already waiting are not adjusted.
void PCOSSetClock(s64 unixSeconds) {
    pthread_once(&sOnce, InitTime);
    sClockSet = true;
    sClockUnix = unixSeconds;
    InitTime(); // start again from the new instant
}

// The game's clock as universal time: seconds since 1970-01-01 UTC, and the
// microseconds of the current second.
s64 PCOSGetUnixTime(u32* microseconds) {
    s64 clock = TimerClock();
    s64 ticks = OSGetTime();
    if (microseconds != nullptr) {
        *microseconds = static_cast<u32>((ticks % clock) * 1000000 / clock);
    }
    return ticks / clock + kUnixToOSEpoch - sUtcOffset;
}

s64 PCOSTicksToNanoseconds(OSTime ticks) {
    s64 clock = TimerClock();
    return (ticks / clock) * kNanosecondsPerSecond + (ticks % clock) * kNanosecondsPerSecond / clock;
}

void PCOSTimeToMonotonic(OSTime time, struct timespec* out) {
    pthread_once(&sOnce, InitTime);
    s64 nsec = PCOSTicksToNanoseconds(time - sBaseTicks) + sAnchor.tv_nsec;
    s64 sec = static_cast<s64>(sAnchor.tv_sec) + nsec / kNanosecondsPerSecond;
    nsec %= kNanosecondsPerSecond;
    if (nsec < 0) {
        nsec += kNanosecondsPerSecond;
        sec--;
    }
    if (sec < 0) {
        sec = 0;
        nsec = 0;
    }
    if (sec > 0x7FFFFFFF) {
        sec = 0x7FFFFFFF; // time_t is 32 bits here
    }
    out->tv_sec = static_cast<time_t>(sec);
    out->tv_nsec = static_cast<long>(nsec);
}

extern "C" {

OSTime OSCalendarTimeToTicks(const OSCalendarTime* cal);

OSTime OSGetTime(void) {
    pthread_once(&sOnce, InitTime);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    s64 nsec = (static_cast<s64>(now.tv_sec) - sAnchor.tv_sec) * kNanosecondsPerSecond + (now.tv_nsec - sAnchor.tv_nsec);
    return sBaseTicks + NanosecondsToTicks(nsec);
}

// The low word of the time base, as on the Wii.
OSTick OSGetTick(void) {
    return static_cast<OSTick>(OSGetTime());
}

// On the Wii the system time is the time base plus a boot offset; here the
// time base already is the system time.
OSTime __OSGetSystemTime(void) {
    return OSGetTime();
}

OSTime __OSTimeToSystemTime(OSTime time) {
    return time;
}

// --- calendar (as in src/revolution/OS/OSTime.c) ------------------------------

#define USEC_MAX 1000
#define MSEC_MAX 1000
#define MONTH_MAX 12
#define WEEK_DAY_MAX 7
#define YEAR_DAY_MAX 365
#define SECS_IN_MIN 60
#define SECS_IN_HOUR (SECS_IN_MIN * 60)
#define SECS_IN_DAY (SECS_IN_HOUR * 24)
#define SECS_IN_YEAR (SECS_IN_DAY * 365)
// Days from 0000-01-01 to 2000-01-01
#define BIAS 0xB2575

static const s32 YearDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static const s32 LeapYearDays[] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

static BOOL IsLeapYear(s32 year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static s32 GetYearDays(s32 year, s32 mon) {
    return (IsLeapYear(year) ? LeapYearDays : YearDays)[mon];
}

static s32 GetLeapDays(s32 year) {
    if (year < 1) {
        return 0;
    }
    return (year + 3) / 4 - (year - 1) / 100 + (year - 1) / 400;
}

static void GetDates(s32 days, OSCalendarTime* cal) {
    s32 year;
    s32 totalDays;
    const s32* p_days;
    s32 month;

    cal->wday = (days + 6) % WEEK_DAY_MAX;

    for (year = days / YEAR_DAY_MAX; days < (totalDays = year * YEAR_DAY_MAX + GetLeapDays(year)); year--) {
    }

    days -= totalDays;
    cal->year = year;
    cal->yday = days;

    p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
    for (month = MONTH_MAX; days < p_days[--month];) {
    }

    cal->mon = month;
    cal->mday = days - p_days[month] + 1;
}

void OSTicksToCalendarTime(OSTime ticks, OSCalendarTime* cal) {
    s32 days, secs;
    s64 d;
    s64 clock = TimerClock();

    d = ticks % clock;
    if (d < 0) {
        d += clock;
    }

    cal->usec = static_cast<int>((d * 8) / (clock / 125000) % USEC_MAX);
    cal->msec = static_cast<int>(d / (clock / 1000) % MSEC_MAX);

    ticks -= d;
    days = static_cast<s32>((ticks / clock) / SECS_IN_DAY) + BIAS;
    secs = static_cast<s32>((ticks / clock) % SECS_IN_DAY);

    if (secs < 0) {
        days -= 1;
        secs += SECS_IN_DAY;
    }

    GetDates(days, cal);
    cal->hour = secs / 60 / 60;
    cal->min = secs / 60 % 60;
    cal->sec = secs % 60;
}

OSTime OSCalendarTimeToTicks(const OSCalendarTime* cal) {
    s64 seconds;
    s32 month;
    s32 ovMon;
    s32 year;
    s64 clock = TimerClock();

    ovMon = cal->mon / MONTH_MAX;
    month = cal->mon - (ovMon * MONTH_MAX);

    if (month < 0) {
        month += MONTH_MAX;
        ovMon--;
    }

    year = cal->year + ovMon;

    seconds = static_cast<s64>(SECS_IN_YEAR) * year +
              static_cast<s64>(SECS_IN_DAY) * (cal->mday + GetLeapDays(year) + GetYearDays(year, month) - 1) +
              static_cast<s64>(SECS_IN_HOUR) * cal->hour + static_cast<s64>(SECS_IN_MIN) * cal->min + cal->sec -
              static_cast<s64>(0xEB1E1BF80ULL);

    return seconds * clock + static_cast<s64>(cal->msec) * (clock / 1000) +
           (static_cast<s64>(cal->usec) * (clock / 125000)) / 8;
}

} // extern "C"
