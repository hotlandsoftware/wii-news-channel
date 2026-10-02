#include <types.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef long time_t;
typedef unsigned long clock_t;

struct tm {
    int tm_sec;   // 0x00
    int tm_min;   // 0x04
    int tm_hour;  // 0x08
    int tm_mday;  // 0x0C
    int tm_mon;   // 0x10
    int tm_year;  // 0x14
    int tm_wday;  // 0x18
    int tm_yday;  // 0x1C
    int tm_isdst; // 0x20
};

#define ULONG_MAX 0xFFFFFFFFUL

#define seconds_per_minute (60L)
#define seconds_per_hour (60L * seconds_per_minute)
#define seconds_per_day (24L * seconds_per_hour)

/* Seconds from 1900-01-01 (struct tm) to 1970-01-01 (time_t). */
#define seconds_1900_to_1970 (2208988800UL)

extern clock_t __get_clock(void);
extern time_t __get_time(void);
extern int __to_gm_time(time_t* time);
extern int __isdst(void);

extern int __msl_add(int* x, int y);
extern int __msl_ladd(long* x, long y);
extern int __msl_mul(int* x, int y);
extern div_t __msl_div(int x, int y);
extern int __msl_mod(int x, int y);


static const short month_to_days[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366},
};

static char* extract_name(const char* names, int n) {
    static char name_buf[60];
    const char* p;
    int i;
    size_t len;

    for (i = 0; i <= n; i++) {
        p = strchr(names, '|');

        if (i == n) {
            if (p == NULL) {
                strcpy(name_buf, names);
            } else {
                len = p - names;
                strncpy(name_buf, names, len);
                name_buf[len] = '\0';
            }
            break;
        }

        names = p + 1;
    }

    return name_buf;
}

static struct tm MSL_GMTIME_BUFF;
static struct tm MSL_LOCALTIME_BUFF;

static int leap_year(int year) {
    return __msl_mod(year, 4) == 0 && (__msl_mod(year, 100) != 0 || __msl_mod(year, 400) == 100);
}

static int leap_days(int year, int mon) {
    int n;
    div_t q;

    q = div(year, 4);
    n = q.quot;

    q = div(year, 100);
    n -= q.quot;

    if (year < 100) {
        q = __msl_div(year + 899, 1000);
        n += q.quot;
    } else {
        q = __msl_div(year - 100, 1000);
        n += q.quot + 1;
    }

    if (leap_year(year)) {
        if (year < 0) {
            if (mon > 1) {
                ++n;
            }
        } else if (mon <= 1) {
            --n;
        }
    }

    return n;
}

static void __time2tm(time_t inTime, struct tm* tm) {
    unsigned long years, months, days, seconds;
    int is_leap_year;

    seconds = inTime + seconds_1900_to_1970;

    if (!tm) {
        return;
    }

    tm->tm_isdst = __isdst();

    days = seconds / seconds_per_day;
    seconds %= seconds_per_day;

    tm->tm_wday = (days + 1) % 7;

    years = 0;

    for (;;) {
        unsigned long days_this_year = leap_year(years) ? 366 : 365;

        if (days < days_this_year) {
            break;
        }

        days -= days_this_year;
        years++;
    }

    tm->tm_year = years;
    tm->tm_yday = days;

    months = 0;

    is_leap_year = leap_year(years);

    for (;;) {
        unsigned long days_thru_this_month = month_to_days[is_leap_year][months + 1];

        if (days < days_thru_this_month) {
            days -= month_to_days[is_leap_year][months];
            break;
        }

        ++months;
    }

    tm->tm_mon = months;
    tm->tm_mday = days + 1;

    tm->tm_hour = seconds / seconds_per_hour;
    seconds -= tm->tm_hour * seconds_per_hour;

    tm->tm_min = seconds / seconds_per_minute;
    tm->tm_sec = seconds - tm->tm_min * seconds_per_minute;
}

static int __tm2time(struct tm* tm, time_t* time) {
    long days;
    unsigned long seconds, day_secs;
    div_t q1, q2, q3, q4;

    if (!tm || !time) {
        return 0;
    }

    --tm->tm_mday;

    q1 = __msl_div(tm->tm_sec, 60);
    tm->tm_sec = q1.rem;
    if (!__msl_add(&tm->tm_min, q1.quot)) {
        goto no_exit;
    }

    q2 = __msl_div(tm->tm_min, 60);
    tm->tm_min = q2.rem;
    if (!__msl_add(&tm->tm_hour, q2.quot)) {
        goto no_exit;
    }

    q3 = __msl_div(tm->tm_hour, 24);
    tm->tm_hour = q3.rem;
    if (!__msl_add(&tm->tm_mday, q3.quot)) {
        goto no_exit;
    }

    q4 = __msl_div(tm->tm_mon, 12);
    tm->tm_mon = q4.rem;
    if (!__msl_add(&tm->tm_year, q4.quot)) {
        goto no_exit;
    }

    days = tm->tm_year;

    if (!__msl_mul((int*)&days, 365)) {
        goto no_exit;
    }

    if (!__msl_ladd(&days, leap_days(tm->tm_year, tm->tm_mon))) {
        goto no_exit;
    }

    if (!__msl_ladd(&days, month_to_days[0][tm->tm_mon])) {
        goto no_exit;
    }

    if (!__msl_ladd(&days, tm->tm_mday)) {
        goto no_exit;
    }

    if (days < 0 || (unsigned long)days > (ULONG_MAX / seconds_per_day)) {
        goto no_exit;
    }

    day_secs = days * seconds_per_day;

    seconds = (tm->tm_hour * seconds_per_hour) + (tm->tm_min * seconds_per_minute) + tm->tm_sec;

    if (seconds > ULONG_MAX - day_secs) {
        goto no_exit;
    }

    seconds += day_secs;

    *time = seconds - seconds_1900_to_1970;

    __time2tm(seconds - seconds_1900_to_1970, tm);

    return 1;

no_exit:
    return 0;
}

clock_t clock(void) {
    return __get_clock();
}

double difftime(time_t time1, time_t time0) {
    if (time1 >= time0) {
        return (double)(time1 - time0);
    } else {
        return -(double)(time0 - time1);
    }
}

time_t mktime(struct tm* timeptr) {
    struct tm tm = *timeptr;
    time_t time;

    if (!__tm2time(&tm, &time)) {
        return (time_t)-1;
    }

    *timeptr = tm;

    return time;
}

time_t time(time_t* timer) {
    time_t time = __get_time();

    if (timer) {
        *timer = time;
    }

    return time;
}

static void asciitime(struct tm tm, char* str) {
    if (mktime(&tm) == (time_t)-1) {
        sprintf(str, "xxx xxx xx xx:xx:xx xxxx\n");
        return;
    }

    sprintf(str, "%s %s%3d %.2d:%.2d:%.2d %d\n", str, str, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);
}

char* asctime(const struct tm* tm) {
    struct tm temp_tm;

    temp_tm = *tm;
    asciitime(temp_tm, (char*)&MSL_GMTIME_BUFF);

    return (char*)&MSL_GMTIME_BUFF;
}

char* ctime(const time_t* timer) {
    char* str = (char*)&MSL_GMTIME_BUFF;

    sprintf(str, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", str, str, 0, 0, 0, 0, (int)*timer);

    return str;
}

struct tm* gmtime(const time_t* timer) {
    time_t time;

    if (!timer) {
        return NULL;
    }

    time = *timer;

    if (!__to_gm_time(&time)) {
        return NULL;
    }

    __time2tm(time, &MSL_GMTIME_BUFF);
    MSL_GMTIME_BUFF.tm_isdst = 0;

    return &MSL_GMTIME_BUFF;
}

struct tm* localtime(const time_t* timer) {
    if (!timer) {
        MSL_LOCALTIME_BUFF.tm_sec = 0;
        MSL_LOCALTIME_BUFF.tm_min = 0;
        MSL_LOCALTIME_BUFF.tm_hour = 0;
        MSL_LOCALTIME_BUFF.tm_mday = 1;
        MSL_LOCALTIME_BUFF.tm_mon = 0;
        MSL_LOCALTIME_BUFF.tm_year = 0;
        MSL_LOCALTIME_BUFF.tm_wday = 1;
        MSL_LOCALTIME_BUFF.tm_yday = 0;
        MSL_LOCALTIME_BUFF.tm_isdst = -1;
        MSL_LOCALTIME_BUFF.tm_isdst = __isdst();
        return &MSL_LOCALTIME_BUFF;
    }

    __time2tm(*timer, &MSL_LOCALTIME_BUFF);
    MSL_LOCALTIME_BUFF.tm_isdst = __isdst();

    return &MSL_LOCALTIME_BUFF;
}

static int emit(char* str, size_t size, size_t* max_size, const char* format_str, ...) {
    va_list args;
    va_start(args, format_str);

    if (size > *max_size) {
        return 0;
    }

    *max_size -= size;

    return vsprintf(str, format_str, args);
}

static time_t ISO8601NewYear(int year) {
    struct tm ts0;
    short StartMday[7] = {2, 3, 4, 29, 30, 31, 1};

    ts0.tm_sec = 0;
    ts0.tm_min = 0;
    ts0.tm_hour = 0;
    ts0.tm_mon = 0;
    ts0.tm_isdst = -1;
    ts0.tm_mday = 0;
    ts0.tm_wday = 7;
    ts0.tm_year = year;

    while ((ts0.tm_wday != 0) && (ts0.tm_mday <= 7)) {
        ++ts0.tm_mday;
        mktime(&ts0);
    }

    ts0.tm_mday = StartMday[ts0.tm_mday - 1];

    if (ts0.tm_mday >= 29) {
        ts0.tm_mon = 11;
        ts0.tm_year--;
    }

    return mktime(&ts0);
}

static int ISO8601Week(const struct tm* tmptr, int* WYear) {
    struct tm Localtm = *tmptr;
    time_t timer;
    struct {
        int year;
        time_t NewYear;
    } WY[3], *p;

    WY[2].year = tmptr->tm_year - 1;
    WY[2].NewYear = ISO8601NewYear(WY[2].year);
    WY[1].year = tmptr->tm_year;
    WY[1].NewYear = ISO8601NewYear(WY[1].year);
    WY[0].year = tmptr->tm_year + 1;
    WY[0].NewYear = ISO8601NewYear(WY[0].year);

    timer = mktime(&Localtm);

    if (WY[2].NewYear <= timer && timer < WY[1].NewYear) {
        p = &WY[2];
    } else if (WY[1].NewYear <= timer && timer < WY[0].NewYear) {
        p = &WY[1];
    } else {
        p = &WY[0];
    }

    *WYear = p->year;

    return (int)(difftime(timer, p->NewYear) / 86400.0 / 7.0) + 1;
}

size_t strftime(char* str, size_t max_size, const char* format_str, const struct tm* timeptr) {
    struct tm tm;
    const struct tm default_tm = {0, 0, 0, 1, 0, 0, 1, 0, -1};
    size_t num_chars;
    size_t chars_written;
    size_t space_remaining;
    const char* format_ptr;
    const char* curr_format;
    int n;
    int ISO8601Year;
    char temp_string[32];
    struct __locale* locale = &_current_locale;

    if ((space_remaining = --max_size) <= 0) {
        return 0;
    }

    tm = default_tm;

    if (timeptr) {
        tm = *timeptr;

        if (mktime(&tm) == (time_t)-1) {
            tm = default_tm;
        }
    }

    format_ptr = format_str;
    chars_written = 0;

    while (*format_ptr) {
        if (!(curr_format = strchr(format_ptr, '%'))) {
            if ((num_chars = strlen(format_ptr)) != 0) {
                if (num_chars <= space_remaining) {
                    memcpy(str, format_ptr, num_chars);
                    chars_written += num_chars;
                    str += num_chars;
                    space_remaining -= num_chars;
                } else {
                    return 0;
                }
            }

            break;
        }

        if ((num_chars = curr_format - format_ptr) != 0) {
            if (num_chars <= space_remaining) {
                memcpy(str, format_ptr, num_chars);
                chars_written += num_chars;
                str += num_chars;
                space_remaining -= num_chars;
            } else {
                return 0;
            }
        }

        format_ptr = curr_format;

        if ((*(format_ptr + 1) == 'E') || (*(format_ptr + 1) == 'O')) {
            ++format_ptr;
        }

        switch (*(format_ptr + 1)) {
        case 'a':
            strcpy(temp_string, extract_name(locale->time_cmpt_ptr->Day_Names, tm.tm_wday * 2));
            num_chars = emit(str, strlen(temp_string), &space_remaining, "%s", temp_string);
            break;

        case 'A':
            strcpy(temp_string, extract_name(locale->time_cmpt_ptr->Day_Names, tm.tm_wday * 2 + 1));
            num_chars = emit(str, strlen(temp_string), &space_remaining, "%s", temp_string);
            break;

        case 'b':
        case 'h':
            num_chars = emit(str, 3, &space_remaining, "%.3s", extract_name(locale->time_cmpt_ptr->MonthNames, tm.tm_mon * 2));
            break;

        case 'B':
            strcpy(temp_string, extract_name(locale->time_cmpt_ptr->MonthNames, tm.tm_mon * 2 + 1));
            num_chars = emit(str, strlen(temp_string), &space_remaining, "%s", temp_string);
            break;

        case 'c':
            num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->DateTime_Format, &tm);
            space_remaining -= num_chars;
            break;

        case 'd':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_mday);
            break;

        case 'D':
            num_chars = strftime(str, space_remaining + 1, "%m/%d/%y", &tm);
            break;

        case 'e':
            num_chars = emit(str, 2, &space_remaining, "%2d", tm.tm_mday);
            break;

        case 'F':
            num_chars = emit(str, 10, &space_remaining, "%.4d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
            break;

        case 'g':
            ISO8601Week(timeptr, &ISO8601Year);
            num_chars = emit(str, 2, &space_remaining, "%.2d", ISO8601Year % 100);
            break;

        case 'G':
            ISO8601Week(timeptr, &ISO8601Year);
            num_chars = emit(str, 4, &space_remaining, "%.4d", ISO8601Year + 1900);
            break;

        case 'H':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_hour);
            break;

        case 'I':
            num_chars = emit(str, 2, &space_remaining, "%.2d", (n = tm.tm_hour % 12) != 0 ? n : 12);
            break;

        case 'j':
            num_chars = emit(str, 3, &space_remaining, "%.3d", tm.tm_yday + 1);
            break;

        case 'm':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_mon + 1);
            break;

        case 'M':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_min);
            break;

        case 'n':
            num_chars = emit(str, 2, &space_remaining, "\n");
            break;

        case 'p': {
            char* name = extract_name(locale->time_cmpt_ptr->am_pm, tm.tm_hour >= 12);
            num_chars = emit(str, strlen(name), &space_remaining, "%s", name);
            break;
        }

        case 'r':
            num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->Twelve_hr_format, &tm);
            space_remaining -= num_chars;
            break;

        case 'R':
            num_chars = strftime(str, space_remaining + 1, "%H:%M", &tm);
            space_remaining -= num_chars;
            break;

        case 'S':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_sec);
            break;

        case 't':
            num_chars = emit(str, 2, &space_remaining, "\t");
            break;

        case 'T':
            num_chars = strftime(str, space_remaining + 1, "%H:%M:%S", &tm);
            space_remaining -= num_chars;
            break;

        case 'u':
            if (tm.tm_wday == 0) {
                num_chars = emit(str, 1, &space_remaining, "7");
            } else {
                num_chars = emit(str, 1, &space_remaining, "%.1d", tm.tm_wday);
            }
            break;

        case 'U':
            n = tm.tm_yday;
            n -= __msl_mod(tm.tm_wday, 7);
            num_chars = emit(str, 2, &space_remaining, "%.2d", (n < 0) ? 0 : n / 7 + 1);
            break;

        case 'V':
            num_chars = emit(str, 2, &space_remaining, "%.2d", ISO8601Week(timeptr, &ISO8601Year));
            break;

        case 'w':
            num_chars = emit(str, 1, &space_remaining, "%.1d", tm.tm_wday);
            break;

        case 'W':
            n = tm.tm_yday;
            n -= __msl_mod(tm.tm_wday - 1, 7);
            num_chars = emit(str, 2, &space_remaining, "%.2d", (n < 0) ? 0 : n / 7 + 1);
            break;

        case 'x':
            num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->Date_Format, &tm);
            space_remaining -= num_chars;
            break;

        case 'X':
            num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->Time_Format, &tm);
            space_remaining -= num_chars;
            break;

        case 'C':
        case 'y':
            num_chars = emit(str, 2, &space_remaining, "%.2d", tm.tm_year % 100);
            break;

        case 'Y':
            num_chars = emit(str, 4, &space_remaining, "%.4d", tm.tm_year + 1900);
            break;

        case 'z': {
            time_t local, utc, now;
            struct tm localtm;
            struct tm* utctmptr;
            double diff, diffmins, diffhours;

            now = time(NULL);
            utc = now;
            utctmptr = gmtime(&utc);

            if (utctmptr == NULL) {
                num_chars = emit(str, 4, &space_remaining, "0000");
            } else {
                localtm = *localtime(&now);
                local = mktime(&localtm);
                utc = mktime(utctmptr);
                diff = difftime(local, utc);
                diffmins = diff / 60;
                diffhours = (int)diff / 3600;
                num_chars = emit(str, 5, &space_remaining, "%+03.0f%02.0f", diffhours, (double)__abs((int)(diffmins - 60 * diffhours)));
            }
            break;
        }

        case 'Z':
            if (*locale->time_cmpt_ptr->TimeZone == '\0') {
                num_chars = 0;
                *str = '\0';
            } else {
                num_chars = emit(str, strlen(locale->time_cmpt_ptr->TimeZone), &space_remaining, "%s",
                                 locale->time_cmpt_ptr->TimeZone);
            }
            break;

        case '%':
            num_chars = emit(str, 2, &space_remaining, "%%", *(format_ptr + 1));
            break;

        default:
            num_chars = emit(str, 2, &space_remaining, "%%%c", *(format_ptr + 1));
            break;
        }

        if (!num_chars) {
            return 0;
        }

        chars_written += num_chars;
        str += num_chars;
        format_ptr += 2;
    }

    *str = '\0';

    if (max_size < chars_written) {
        return 0;
    }

    return chars_written;
}
