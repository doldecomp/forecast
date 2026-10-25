#ifndef TIME_API_H
#define TIME_API_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform hooks (OS/__start.c, time_zone.c)
clock_t __get_clock(void);
time_t __get_time(void);
int __to_gm_time(time_t* time);
long __get_isdst(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_API_H
