// OS time: OSGetTime, OSGetTick, calendar conversion.
//
// The Wii's time base counts OS_TIMER_CLOCK ticks per second (a quarter of the
// bus clock: 60,750,000 Hz) and OSGetTime() is the number of ticks since
// 2000-01-01 00:00 of the console's clock, which is local time (the console
// has no time zone). Here the value is the host's local time when the clock is
// first read, plus the monotonic time elapsed since then, so it never jumps.
//
// The calendar functions are the SDK's (src/revolution/OS/OSTime.c).

#include <ctime>

#include "os_internal.h"

namespace {

const s64 kNanosecondsPerSecond = 1000000000LL;
// Seconds from 1970-01-01 to 2000-01-01.
const s64 kUnixToOSEpoch = 946684800LL;

pthread_once_t sOnce = PTHREAD_ONCE_INIT;
struct timespec sAnchor; // CLOCK_MONOTONIC at the first read
s64 sBaseTicks;          // OS time at the first read

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

    struct tm local;
    time_t seconds = wall.tv_sec;
    localtime_r(&seconds, &local);

    s64 localSeconds = static_cast<s64>(wall.tv_sec) + local.tm_gmtoff - kUnixToOSEpoch;
    sBaseTicks = localSeconds * TimerClock() + NanosecondsToTicks(wall.tv_nsec);
}

} // namespace

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
