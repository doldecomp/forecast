#ifndef WCHAR_H
#define WCHAR_H

#include "size_t.h"
#include "wchar_t.h"

#ifdef __cplusplus
extern "C" {
#endif

int wcscmp(const wchar_t* lhs, const wchar_t* rhs);
wchar_t* wcsncpy(wchar_t* dst, const wchar_t* src, size_t count);

#ifdef __cplusplus
}
#endif

#endif  // WCHAR_H
