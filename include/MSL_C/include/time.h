#ifndef TIME_H
#define TIME_H

#include "size_t.h"
#include <__internal/__NULL.h>

#ifdef __cplusplus
extern "C" {
#endif

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

clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm* timeptr);
time_t time(time_t* timer);
char* asctime(const struct tm* timeptr);
char* ctime(const time_t* timer);
struct tm* gmtime(const time_t* timer);
struct tm* localtime(const time_t* timer);
size_t strftime(char* str, size_t max_size, const char* format_str, const struct tm* timeptr);

#ifdef __cplusplus
}
#endif

#endif // TIME_H
