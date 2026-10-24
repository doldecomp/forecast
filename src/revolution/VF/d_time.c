#include <revolution/OS.h>
#include <revolution/VF.h>

void dTM_GetNowTime(struct PF_SYS_DATE* sdate, struct PF_SYS_TIME* stime) {
    VFSysTimeStampCallback getTimeCallBack;
    struct VFSysTime time;
    struct OSCalendarTime CalenderTime;

    getTimeCallBack = VFSysGetTimeStampCallback();

    sdate->sys_year = 0;
    sdate->sys_month = 0;
    sdate->sys_day = 0;
    stime->sys_hour = 0;
    stime->sys_min = 0;
    stime->sys_sec = 0;

    if (getTimeCallBack != NULL) {
        getTimeCallBack(&time);
        sdate->sys_year = time.year;
        sdate->sys_month = time.month;
        sdate->sys_day = time.day;
        stime->sys_hour = time.hour;
        stime->sys_min = time.min;
        stime->sys_sec = time.sec;
    } else {
        OSTicksToCalendarTime(OSGetTime(), &CalenderTime);
        sdate->sys_year = CalenderTime.year;
        sdate->sys_month = CalenderTime.month + 1;
        sdate->sys_day = CalenderTime.mday;
        stime->sys_hour = CalenderTime.hour;
        stime->sys_min = CalenderTime.min;
        stime->sys_sec = CalenderTime.sec;
    }
}
