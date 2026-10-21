#include <nw4r/math.h>
#include <types.h>
#include <revolution/OS.h>
#include <revolution/NET/nettime.h>

extern "C" const f32 sSmoothEpsilon;
extern "C" const f32 sSmoothEpsilonNeg;
extern "C" const f32 sAngleZero;
extern "C" const f32 sAngleFull;
extern "C" const f32 sAngleHalfNeg;
extern "C" const f32 sAngleHalf;
extern "C" const f32 sIdxToFIdx;
extern "C" const f32 sHalf;

extern "C" const wchar_t sWeatherDigits[] = L"0123456789";

void WrapHour(s32* pHour) {
    *pHour %= 24;
}

void WrapHourTo24(s32* pHour) {
    *pHour %= 25;
}

f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep) {
    if (*value != target) {
        f32 diff = *value - target;

        if (__fabsf(diff) < minStep) {
            *value = target;
        } else {
            f32 absDiff = __fabsf(diff);
            f32 scaled = absDiff * rate;
            diff *= rate;

            BOOL small = scaled < sSmoothEpsilon && scaled > sSmoothEpsilonNeg;
            if (small) {
                *value = target;
            } else {
                if (scaled > maxStep) {
                    if (diff < sAngleZero)
                        maxStep = -maxStep;
                    diff = maxStep;
                } else if (scaled < minStep) {
                    if (diff < sAngleZero)
                        minStep = -minStep;
                    diff = minStep;
                }

                *value -= diff;
            }
        }
    }

    return __fabsf(*value - target);
}

f32 SmoothApproachAngle(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep) {
    f32 angle = *value;
    if (angle < sAngleZero)
        *value += sAngleFull;
    else if (angle >= sAngleFull)
        *value -= sAngleFull;

    if (target < sAngleZero)
        target += sAngleFull;
    else if (target >= sAngleFull)
        target -= sAngleFull;

    if (*value != target) {
        f32 diff = *value - target;
        if (diff < sAngleHalfNeg)
            diff += sAngleFull;
        else if (diff > sAngleHalf)
            diff -= sAngleFull;

        diff *= rate;
        f32 scaled = __fabsf(diff);

        if (scaled <= minStep) {
            *value = target;
        } else {
            if (scaled > maxStep) {
                if (diff < sAngleZero)
                    maxStep = -maxStep;
                diff = maxStep;
            }

            *value -= diff;

            angle = *value;
            if (angle < sAngleZero)
                *value += sAngleFull;
            else if (angle >= sAngleFull)
                *value -= sAngleFull;
        }
    }

    return __fabsf(*value - target);
}

f32 EaseCos(u16 t) {
    return sHalf - sHalf * nw4r::math::CosFIdx(nw4r::math::U16ToF32(t) * sIdxToFIdx);
}

extern "C" s32 FormatDigits(s32 value, s32* digits, s32 maxDigits) {
    s32 i;
    s32 count = 0;
    s32 v = value;

    while (v != 0) {
        count++;
        v /= 10;
    }

    if (count > maxDigits)
        count = maxDigits;

    i = 0;
    for (; i < count; i++) {
        digits[i] = value % 10;
        value /= 10;
    }
    for (; i < maxDigits; i++)
        digits[i] = 0;

    return count;
}

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad) {
    s32 digitBuf[10];
    s32* p;
    s32 i;

    if (digits > 10)
        digits = 10;

    if (value < 0) {
        *pBuf++ = L'-';
        value -= value * 2;
    }

    FormatDigits(value, digitBuf, digits);

    i = digits - 1;
    p = &digitBuf[i];
    for (; i >= 0; p--, i--) {
        if (!zeroPad && i != 0) {
            if (*p != 0) {
                zeroPad = TRUE;
                *pBuf++ = sWeatherDigits[*p];
            }
        } else {
            zeroPad = TRUE;
            *pBuf++ = sWeatherDigits[*p];
        }
    }

    *pBuf = 0;
    return pBuf;
}

wchar_t* FormatInt(s32 value, wchar_t* pBuf) {
    s32 hundreds;
    s32 rest;
    s32 tens;
    s32 ones;
    if (value < 0) {
        *pBuf++ = L'-';
        value -= value * 2;
    }
    hundreds = value / 100;
    rest = value % 100;
    ones = rest % 10;
    tens = rest / 10;
    if (hundreds != 0) {
        *pBuf++ = sWeatherDigits[hundreds];
        *pBuf++ = sWeatherDigits[tens];
        *pBuf++ = sWeatherDigits[ones];
    } else if (tens != 0) {
        *pBuf++ = sWeatherDigits[tens];
        *pBuf++ = sWeatherDigits[ones];
    } else {
        *pBuf++ = sWeatherDigits[ones];
    }
    *pBuf = 0;
    return pBuf;
}

static inline BOOL IsLeapYear(s32 year) {
    return year % 4 == 0;
}

static inline s32 GetDaysInMonth(s32 month, s32 year) {
    if (month == 4 || month == 6 || month == 9 || month == 11)
        return 30;
    if (month == 2) {
        if (IsLeapYear(year))
            return 29;
        return 28;
    }
    return 31;
}

void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time) {
    s32 len;
    s32 year;
    s32 month;
    s32 days;
    s32 yday;
    s32 totalDays;
    s32 hours;

    hours = minutes / 60;
    days = hours / 24;
    totalDays = days;
    year = 0;
    month = 1;
    for (;;) {
        len = IsLeapYear(year) ? 366 : 365;
        if (days < len)
            break;
        days -= len;
        year++;
    }
    yday = days;

    for (;;) {
        len = GetDaysInMonth(month, year);
        if (days < len)
            break;
        days -= len;
        month++;
    }

    time->sec = 0;
    time->min = minutes % 60;
    time->hour = hours % 24;
    time->mday = days + 1;
    time->month = month - 1;
    time->year = year + 2000;
    time->wday = (totalDays + 6) % 7;
    time->yday = yday;
    time->msec = 0;
    time->usec = 0;
}

u32 GetUniversalMinutes() {
    OSCalendarTime time;
    s32 year;
    s32 month;
    s32 days;
    s32 hour;
    s32 min;
    s32 i;
    s32 j;

    NETGetUniversalCalendar(&time);

    year = time.year;
    month = time.month + 1;
    days = time.mday;
    hour = time.hour;
    min = time.min;

    days += (year - 2000) * 365;
    for (j = 0; j < year - 2000; j += 4)
        days++;
    for (i = 1; i < month; i++) {
        if (i == 4 || i == 6 || i == 9 || i == 11) {
            days += 30;
        } else if (i == 2) {
            if (((year - 2000) & 3) == 0)
                days += 29;
            else
                days += 28;
        } else {
            days += 31;
        }
    }

    return min + hour * 60 + (days - 1) * 1440;
}
