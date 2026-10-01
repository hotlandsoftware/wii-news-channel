#include <revolution/os/OSTime.h>
#include <revolution/os.h>
#define USEC_MAX 1000
#define MSEC_MAX 1000
#define MONTH_MAX 12
#define WEEK_DAY_MAX 7
#define YEAR_DAY_MAX 365

#define SECS_IN_MIN 60
#define SECS_IN_HOUR (SECS_IN_MIN * 60)
#define SECS_IN_DAY (SECS_IN_HOUR * 24)
#define SECS_IN_YEAR (SECS_IN_DAY * 365)

#define BIAS 0xB2575


static s32 YearDays[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
static s32 LeapYearDays[] = { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 };

asm OSTime OSGetTime(void) {
    nofralloc

loop:
    mftbu r3
    mftb r4
    mftbu r5
    cmpw r3, r5
    bne loop
    blr
}

asm OSTick OSGetTick(void) {
    nofralloc
    mftb r3
    blr
}

OSTime __OSGetSystemTime(void) {
    BOOL en;
    OSTime* time = (OSTime*)(0x800030D8);
    OSTime res;

    en = OSDisableInterrupts();
    res = *time + OSGetTime();
    OSRestoreInterrupts(en);
    return res;
}

OSTime __OSTimeToSystemTime(OSTime time) {
    BOOL en;
    OSTime* os_time = (OSTime*)(0x800030D8);
    OSTime res;

    en = OSDisableInterrupts();
    res = *os_time + time;
    OSRestoreInterrupts(en);
    return res;
}

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

static void GetDates(s32 days, OSCalendarTime* cal) NO_INLINE {
    s32 year;
    s32 totalDays;
    s32* p_days;
    s32 month;

    cal->wday = (days + 6) % WEEK_DAY_MAX;

    // WTF??
    for (year = days / YEAR_DAY_MAX;
         days < (totalDays = year * YEAR_DAY_MAX + GetLeapDays(year)); year--) {
        ;
    }
    days -= totalDays;
    cal->year = year;
    cal->yday = days;

    p_days = IsLeapYear(year) ? LeapYearDays : YearDays;
    for (month = MONTH_MAX; days < p_days[--month];) {
        ;
    }
    cal->mon = month;
    cal->mday = days - p_days[month] + 1;
}

void OSTicksToCalendarTime(s64 ticks, OSCalendarTime* cal) {
    s32 days, secs;
    s64 d;

    d = ticks % OSSecondsToTicks(1);
    if (d < 0) {
        d += OSSecondsToTicks(1);
    }

    cal->usec = OSTicksToMicroseconds(d) % USEC_MAX;
    cal->msec = OSTicksToMilliseconds(d) % MSEC_MAX;
    ticks -= d;

    days = (OSTicksToSeconds(ticks) / SECS_IN_DAY) + BIAS;
    secs = OSTicksToSeconds(ticks) % SECS_IN_DAY;
    if (secs < 0) {
        days -= 1;
        secs += SECS_IN_DAY;
    }

    GetDates(days, cal);
    cal->hour = secs / 60 / 60;
    cal->min = secs / 60 % 60;
    cal->sec = secs % 60;
}

s64 OSCalendarTimeToTicks(const OSCalendarTime* cal) {
    s64 seconds;
    s32 month;
    s32 ovMon;
    s32 year;

    ovMon = cal->mon / MONTH_MAX;
    month = cal->mon - (ovMon * MONTH_MAX);

    if (month < 0) {
        month += MONTH_MAX;
        ovMon--;
    }

    year = cal->year + ovMon;

    // clang-format off
    seconds = (s64)SECS_IN_YEAR * year +
              (s64)SECS_IN_DAY * (cal->mday + GetLeapDays(year) + GetYearDays(year, month) - 1) +
              (s64)SECS_IN_HOUR * cal->hour +
              (s64)SECS_IN_MIN * cal->min +
              cal->sec -
              (s64)0xEB1E1BF80ULL;
    // clang-format on

    return OSSecondsToTicks(seconds) + OSMillisecondsToTicks((s64)cal->msec) +
           OSMicrosecondsToTicks((s64)cal->usec);
}
