#include "arith.h"
#include "time_api.h"
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#include <time.h>

#pragma exceptions on

// printf.c; <cstdio> does not declare it
int vsprintf(char* s, const char* format, va_list arg);

#define seconds_per_minute (60L)
#define seconds_per_hour (60L * seconds_per_minute)
#define seconds_per_day (24L * seconds_per_hour)

// struct tm counts years from 1900, time_t counts seconds from 1970
#define seconds_1900_to_1970 (((365 * 70UL) + 17) * 24 * 60 * 60)

static const short month_to_days[2][13] = {
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366},
};

// year counts from 1900, so (year + 1900) % 400 == 0 is year % 400 == 100
static int leap_year(int year) {
    return __msl_mod(year, 4) == 0 && (__msl_mod(year, 100) != 0 || __msl_mod(year, 400) == 100);
}

// Leap days between 1900 and the start of the given month
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
                n++;
            }
        } else if (mon <= 1) {
            n--;
        }
    }

    return n;
}

// Moves whole multiples of y from *x into *z. Returns 0 if *z overflows
static int adjust(int* x, int y, int* z) {
    div_t q;

    q = __msl_div(*x, y);
    *x = q.rem;
    return __msl_add(z, q.quot);
}

static void __time2tm(time_t inTime, struct tm* tm) {
    unsigned long years, months, days, seconds;
    int is_leap_year;

    seconds = inTime + seconds_1900_to_1970;

    if (!tm) {
        return;
    }

    tm->tm_isdst = __get_isdst();

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

        months++;
    }

    tm->tm_mon = months;
    tm->tm_mday = days + 1;

    tm->tm_hour = seconds / seconds_per_hour;
    seconds %= seconds_per_hour;

    tm->tm_min = seconds / seconds_per_minute;
    tm->tm_sec = seconds % seconds_per_minute;
}

static int __tm2time(struct tm* tm, time_t* time) {
    long days;
    unsigned long seconds, day_secs;

    if (!tm || !time) {
        return 0;
    }

    --tm->tm_mday;

    if (!adjust(&tm->tm_sec, 60, &tm->tm_min)) {
        goto no_exit;
    }

    if (!adjust(&tm->tm_min, 60, &tm->tm_hour)) {
        goto no_exit;
    }

    if (!adjust(&tm->tm_hour, 24, &tm->tm_mday)) {
        goto no_exit;
    }

    if (!adjust(&tm->tm_mon, 12, &tm->tm_year)) {
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

// Returns the n-th entry of a '|'-separated name list
static char* extract_name(const char* names, int n) {
    static char name[60];
    int i;
    const char* end;
    const char* start = names;

    for (i = 0; i <= n; i++) {
        end = strchr(start, '|');

        if (i == n) {
            if (end == NULL) {
                strcpy(name, start);
            } else {
                strncpy(name, start, end - start);
                name[end - start] = '\0';
            }
            break;
        }

        start = end + 1;
    }

    return name;
}

static void asciitime(struct tm tm, char* str) {
    char day_name[32];
    char month_name[32];

    if (mktime(&tm) == (time_t)-1) {
        sprintf(str, "xxx xxx xx xx:xx:xx xxxx\n");
        return;
    }

    strcpy(day_name, extract_name(_current_locale.time_cmpt_ptr->Day_Names, tm.tm_wday * 2));
    strcpy(month_name, extract_name(_current_locale.time_cmpt_ptr->MonthNames, tm.tm_mon * 2));

    if (strlen(day_name) == 3 && strlen(month_name) == 3) {
        sprintf(str, "%s %s%3d %.2d:%.2d:%.2d %d\n", day_name, month_name, tm.tm_mday, tm.tm_hour, tm.tm_min,
                tm.tm_sec, tm.tm_year + 1900);
    } else {
        sprintf(str, "%.3s %.3s %2d %.2d:%.2d:%.2d %4d\n", day_name, month_name, tm.tm_mday, tm.tm_hour,
                tm.tm_min, tm.tm_sec, tm.tm_year + 1900);
    }
}

static void clear_tm(struct tm* tm) {
    tm->tm_sec = 0;
    tm->tm_min = 0;
    tm->tm_hour = 0;
    tm->tm_mday = 1;
    tm->tm_mon = 0;
    tm->tm_year = 0;
    tm->tm_wday = 1;
    tm->tm_yday = 0;
    tm->tm_isdst = -1;
}

clock_t clock(void) {
    return __get_clock();
}

// difftime and asctime are not linked into the channel, but the original object had them out of line:
// difftime puts the int-to-double constant first in .sdata2 and asctime puts its strings first in the
// string pool. Exceptions are off for them so they add no unwind entries next to the linked functions
#pragma exceptions off
double difftime(time_t time1, time_t time0) {
    if (time1 >= time0) {
        return (double)(time1 - time0);
    } else {
        return -(double)(time0 - time1);
    }
}
#pragma exceptions on

time_t mktime(struct tm* timeptr) {
    struct tm tm = *timeptr;
    time_t time;

    if (!__tm2time(&tm, &time)) {
        return (time_t)-1;
    }

    *timeptr = tm;

    return time;
}

// time and gmtime are only used inlined in strftime, so no out-of-line copies are emitted
inline time_t time(time_t* timer) {
    time_t time = __get_time();

    if (timer) {
        *timer = time;
    }

    return time;
}

static struct tm gmtime_tm;
static struct tm localtime_tm;

// The result shares gmtime's buffer. Being the first use of that buffer, asctime also places it
// between extract_name's and localtime's buffers in .bss
#pragma exceptions off
char* asctime(const struct tm* tm) {
    asciitime(*tm, (char*)&gmtime_tm);
    return (char*)&gmtime_tm;
}
#pragma exceptions on

inline struct tm* gmtime(const time_t* timer) {
    time_t time;

    if (!timer) {
        clear_tm(&gmtime_tm);
        return &gmtime_tm;
    }

    time = *timer;

    if (!__to_gm_time(&time)) {
        return NULL;
    }

    __time2tm(time, &gmtime_tm);
    gmtime_tm.tm_isdst = 0;

    return &gmtime_tm;
}

struct tm* localtime(const time_t* timer) {
    if (!timer) {
        clear_tm(&localtime_tm);
        localtime_tm.tm_isdst = __get_isdst();
        return &localtime_tm;
    }

    __time2tm(*timer, &localtime_tm);
    localtime_tm.tm_isdst = __get_isdst();

    return &localtime_tm;
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

// Week number for %U (week starts on Sunday) and %W (week starts on Monday)
static int week_num(const struct tm* tm, int starting_day) {
    int days = tm->tm_yday;

    days -= __msl_mod(tm->tm_wday - starting_day, 7);

    if (days < 0) {
        return 0;
    }

    return days / 7 + 1;
}

// Start of ISO 8601 week 1: the Monday of the week that contains January 4
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

    while (ts0.tm_wday != 0 && ts0.tm_mday <= 7) {
        ts0.tm_mday++;
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
    struct WeekYear {
        int Year;
        time_t Start;
    };
    struct WeekYear LastYear, ThisYear, NextYear;
    struct WeekYear* WyPtr;
    struct tm Localtm = *tmptr;
    double Days;
    time_t GivenTimeVal;

    LastYear.Year = tmptr->tm_year - 1;
    LastYear.Start = ISO8601NewYear(LastYear.Year);
    ThisYear.Year = tmptr->tm_year;
    ThisYear.Start = ISO8601NewYear(ThisYear.Year);
    NextYear.Year = tmptr->tm_year + 1;
    NextYear.Start = ISO8601NewYear(NextYear.Year);

    GivenTimeVal = mktime(&Localtm);

    if (LastYear.Start <= GivenTimeVal && GivenTimeVal < ThisYear.Start) {
        WyPtr = &LastYear;
    } else if (ThisYear.Start <= GivenTimeVal && GivenTimeVal < NextYear.Start) {
        WyPtr = &ThisYear;
    } else {
        WyPtr = &NextYear;
    }

    *WYear = WyPtr->Year;
    Days = difftime(GivenTimeVal, WyPtr->Start) / 86400;
    return (int)(Days / 7) + 1;
}

size_t strftime(char* str, size_t max_size, const char* format_str, const struct tm* timeptr) {
    struct tm tm;
    struct tm default_tm = {0, 0, 0, 1, 0, 0, 1, 0, -1};
    char name[32];
    size_t num_chars, chars_written, space_remaining;
    struct __locale* locale = &_current_locale;
    const char* format_ptr;
    const char* curr_format;
    int n, ISO8601Year;

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

        // Skip the E and O modifiers
        if (format_ptr[1] == 'E' || format_ptr[1] == 'O') {
            format_ptr++;
        }

        switch (*++format_ptr) {
            case 'a':
                strcpy(name, extract_name(locale->time_cmpt_ptr->Day_Names, tm.tm_wday * 2));
                num_chars = emit(str, strlen(name), &space_remaining, "%s", name);
                break;

            case 'A':
                strcpy(name, extract_name(locale->time_cmpt_ptr->Day_Names, tm.tm_wday * 2 + 1));
                num_chars = emit(str, strlen(name), &space_remaining, "%s", name);
                break;

            case 'b':
            case 'h':
                num_chars = emit(str, 3, &space_remaining, "%.3s",
                                 extract_name(locale->time_cmpt_ptr->MonthNames, tm.tm_mon * 2));
                break;

            case 'B':
                strcpy(name, extract_name(locale->time_cmpt_ptr->MonthNames, tm.tm_mon * 2 + 1));
                num_chars = emit(str, strlen(name), &space_remaining, "%s", name);
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
                num_chars = emit(str, 10, &space_remaining, "%.4d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1,
                                 tm.tm_mday);
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
                char* am_pm = extract_name(locale->time_cmpt_ptr->am_pm, tm.tm_hour < 12 ? 0 : 1);
                num_chars = emit(str, strlen(am_pm), &space_remaining, "%s", am_pm);
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
                num_chars = emit(str, 2, &space_remaining, "%.2d", week_num(&tm, 0));
                break;

            case 'V':
                num_chars = emit(str, 2, &space_remaining, "%.2d", ISO8601Week(timeptr, &ISO8601Year));
                break;

            case 'w':
                num_chars = emit(str, 1, &space_remaining, "%.1d", tm.tm_wday);
                break;

            case 'W':
                num_chars = emit(str, 2, &space_remaining, "%.2d", week_num(&tm, 1));
                break;

            case 'x':
                num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->Date_Format, &tm);
                space_remaining -= num_chars;
                break;

            case 'X':
                num_chars = strftime(str, space_remaining + 1, locale->time_cmpt_ptr->Time_Format, &tm);
                space_remaining -= num_chars;
                break;

            case 'y':
            case 'C':
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
                utctmptr = gmtime(&now);

                if (utctmptr == NULL) {
                    num_chars = emit(str, 4, &space_remaining, "0000");
                } else {
                    localtm = *localtime(&now);
                    local = mktime(&localtm);
                    utc = mktime(utctmptr);
                    diff = difftime(local, utc);
                    diffmins = diff / 60;
                    diffhours = (int)diff / 3600;
                    num_chars = emit(str, 5, &space_remaining, "%+03.0f%02.0f", diffhours,
                                     (double)__abs((int)(diffmins - 60 * diffhours)));
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
                num_chars = emit(str, 2, &space_remaining, "%%", *format_ptr);
                break;

            default:
                num_chars = emit(str, 2, &space_remaining, "%%%c", *format_ptr);
                break;
        }

        if (!num_chars) {
            return 0;
        }

        chars_written += num_chars;
        str += num_chars;
        ++format_ptr;
    }

    *str = '\0';

    if (max_size < chars_written) {
        return 0;
    }

    return chars_written;
}
