#include <types.h>

#pragma section data_type ".rodata"
#pragma section sdata_type ".sdata2"

extern "C" {

// OS stubs
void OSReport(const char*, ...);
void OSPanic(const char*, int, const char*, ...);
s32 RSOListInit(void*);

struct RSOSymbol {
    u32 hash;
    u32 offset;
    u32 sectionIdx;
};
RSOSymbol* RSOFindExportSymbol(void*, const char*);

struct RSOSection {
    u32 addr;
    u32 size;
};
struct RSOModule {
    u8 pad[0x0C];
    RSOSection* sections;
};

// Functions referenced in the table
void ARCClose(void);
void ARCConvertPathToEntrynum(void);
void ARCEntrynumIsDir(void);
void ARCFastOpen(void);
void ARCGetLength(void);
void ARCGetStartAddrInMem(void);
void ARCGetStartOffset(void);
void ARCInitHandle(void);
void ARCOpen(void);
void ARCReadDir(void);
void DVDClose(void);
void DVDCloseDir(void);
void DVDConvertPathToEntrynum(void);
void DVDEntrynumIsDir(void);
void DVDFastOpen(void);
void DVDOpenDir(void);
void DVDReadDir(void);
void DVDReadPrio(void);
void NANDClose(void);
void NANDCreate(void);
void NANDCreateDir(void);
void NANDDelete(void);
void NANDGetCurrentDir(void);
void NANDGetLength(void);
void NANDGetStatus(void);
void NANDGetType(void);
void NANDOpen(void);
void NANDRead(void);
void NANDReadDir(void);
void NANDSafeClose(void);
void NANDSafeOpen(void);
void NANDSeek(void);
void NANDWrite(void);
void GetCurrentCalendarTime__FP14OSCalendarTime(void);
void OSCalendarTimeToTicks(void);
void OSGetTime(void);
void OSRegisterVersion(void);
void OSYieldThread(void);
void SCGetLanguage(void);
extern u8 wwwFontList[];
void contentCloseNAND(void);
void contentConvertPathToEntrynumNAND(void);
void contentFastOpenNAND(void);
void contentGetLengthNAND(void);
void contentInitHandleNAND(void);
void contentOpenDirNAND(void);
void contentReadNAND(void);
void contentReleaseHandleNAND(void);
void contentSeekNAND(void);
void __OSGetSystemTime(void);
extern u8 _SDA2_BASE_[];
extern u8 _SDA_BASE_[];
void __construct_array(void);
void __construct_new_array(void);
void __cvt_fp2unsigned(void);
void __cvt_sll_dbl(void);
void __destroy_arr(void);
void __div2i(void);
extern double __double_huge;
extern u8 __files[];
void __mod2i(void);
void __ptmf_scall(void);
void __setjmp(void);
void __shl2i(void);
#undef __va_arg
void* __va_arg(void*, int);
extern u8 _current_locale[];
void abort(void);
int abs(int);
void acos(void);
void asin(void);
void atan(void);
void atan2(void);
void atoi(void);
void atol(void);
void bsearch(void);
void ceil(void);
void clock(void);
void cos(void);
void exit(int);
void exp(void);
void floor(void);
void fmod(void);
void fprintf(void);
void getenv(void);
void itoa(void);
void localtime(void);
void log(void);
void longjmp(void);
void memchr(void);
void memcmp(void);
void memcpy(void);
void memmove(void);
void memset(void);
void mktime(void);
void pow(void);
void printf(void);
void qsort(void);
void rand(void);
void sin(void);
void snprintf(void);
void sprintf(void);
void sqrt(void);
void srand(void);
void sscanf(void);
void strcat(void);
void strchr(void);
void strcmp(void);
void strcpy(void);
void strcspn(void);
void strftime(void);
void stricmp(void);
void strlen(void);
void strncat(void);
void strncmp(void);
void strncpy(void);
void strncasecmp(void);
void strpbrk(void);
void strrchr(void);
void strspn(void);
void strstr(void);
void strtol(void);
void strtoul(void);
void tan(void);
void vprintf(void);
void vsnprintf(void);
void vsprintf(void);
}

struct RSOImportEntry {
    const char* name;
    void* func;
};

int vcmvAbs(int x);

extern const char sEmptyStr[8];

const RSOImportEntry sRsoImports[] = {
    {"ARCClose", (void*)ARCClose},
    {"ARCConvertPathToEntrynum", (void*)ARCConvertPathToEntrynum},
    {"ARCEntrynumIsDir", (void*)ARCEntrynumIsDir},
    {"ARCFastOpen", (void*)ARCFastOpen},
    {"ARCGetLength", (void*)ARCGetLength},
    {"ARCGetStartAddrInMem", (void*)ARCGetStartAddrInMem},
    {"ARCGetStartOffset", (void*)ARCGetStartOffset},
    {"ARCInitHandle", (void*)ARCInitHandle},
    {"ARCOpen", (void*)ARCOpen},
    {"ARCReadDir", (void*)ARCReadDir},
    {"DVDClose", (void*)DVDClose},
    {"DVDCloseDir", (void*)DVDCloseDir},
    {"DVDConvertPathToEntrynum", (void*)DVDConvertPathToEntrynum},
    {"DVDEntrynumIsDir", (void*)DVDEntrynumIsDir},
    {"DVDFastOpen", (void*)DVDFastOpen},
    {"DVDOpenDir", (void*)DVDOpenDir},
    {"DVDReadDir", (void*)DVDReadDir},
    {"DVDReadPrio", (void*)DVDReadPrio},
    {"NANDClose", (void*)NANDClose},
    {"NANDCreate", (void*)NANDCreate},
    {"NANDCreateDir", (void*)NANDCreateDir},
    {"NANDDelete", (void*)NANDDelete},
    {"NANDGetCurrentDir", (void*)NANDGetCurrentDir},
    {"NANDGetLength", (void*)NANDGetLength},
    {"NANDGetStatus", (void*)NANDGetStatus},
    {"NANDGetType", (void*)NANDGetType},
    {"NANDOpen", (void*)NANDOpen},
    {"NANDRead", (void*)NANDRead},
    {"NANDReadDir", (void*)NANDReadDir},
    {"NANDSafeClose", (void*)NANDSafeClose},
    {"NANDSafeOpen", (void*)NANDSafeOpen},
    {"NANDSeek", (void*)NANDSeek},
    {"NANDWrite", (void*)NANDWrite},
    {"NETGetUniversalCalendar", (void*)GetCurrentCalendarTime__FP14OSCalendarTime},
    {"OSCalendarTimeToTicks", (void*)OSCalendarTimeToTicks},
    {"OSGetTime", (void*)OSGetTime},
    {"OSPanic", (void*)OSPanic},
    {"OSRegisterVersion", (void*)OSRegisterVersion},
    {"OSReport", (void*)OSReport},
    {"OSYieldThread", (void*)OSYieldThread},
    {"SCGetLanguage", (void*)SCGetLanguage},
    {"WWW_FONT_FILE_DATA_TABLE__", (void*)wwwFontList},
    {"contentCloseNAND", (void*)contentCloseNAND},
    {"contentConvertPathToEntrynumNAND", (void*)contentConvertPathToEntrynumNAND},
    {"contentFastOpenNAND", (void*)contentFastOpenNAND},
    {"contentGetLengthNAND", (void*)contentGetLengthNAND},
    {"contentInitHandleNAND", (void*)contentInitHandleNAND},
    {"contentOpenDirNAND", (void*)contentOpenDirNAND},
    {"contentReadNAND", (void*)contentReadNAND},
    {"contentReleaseHandleNAND", (void*)contentReleaseHandleNAND},
    {"contentSeekNAND", (void*)contentSeekNAND},
    {"__OSGetSystemTime", (void*)__OSGetSystemTime},
    {"_SDA2_BASE_", (void*)_SDA2_BASE_},
    {"_SDA_BASE_", (void*)_SDA_BASE_},
    {"__construct_array", (void*)__construct_array},
    {"__construct_new_array", (void*)__construct_new_array},
    {"__cvt_fp2unsigned", (void*)__cvt_fp2unsigned},
    {"__cvt_sll_dbl", (void*)__cvt_sll_dbl},
    {"__destroy_arr", (void*)__destroy_arr},
    {"__div2i", (void*)__div2i},
    {"__double_huge", (void*)&__double_huge},
    {"__files", (void*)__files},
    {"__mod2i", (void*)__mod2i},
    {"__ptmf_scall", (void*)__ptmf_scall},
    {"__setjmp", (void*)__setjmp},
    {"__shl2i", (void*)__shl2i},
    {"__va_arg", (void*)__va_arg},
    {"_current_locale", (void*)_current_locale},
    {"abort", (void*)abort},
    {"abs", (void*)vcmvAbs},
    {"acos", (void*)acos},
    {"asin", (void*)asin},
    {"atan", (void*)atan},
    {"atan2", (void*)atan2},
    {"atoi", (void*)atoi},
    {"atol", (void*)atol},
    {"bsearch", (void*)bsearch},
    {"ceil", (void*)ceil},
    {"clock", (void*)clock},
    {"cos", (void*)cos},
    {"exit", (void*)exit},
    {"exp", (void*)exp},
    {"floor", (void*)floor},
    {"fmod", (void*)fmod},
    {"fprintf", (void*)fprintf},
    {"getenv", (void*)getenv},
    {"itoa", (void*)itoa},
    {"labs", (void*)abs},
    {"localtime", (void*)localtime},
    {"log", (void*)log},
    {"longjmp", (void*)longjmp},
    {"memchr", (void*)memchr},
    {"memcmp", (void*)memcmp},
    {"memcpy", (void*)memcpy},
    {"memmove", (void*)memmove},
    {"memset", (void*)memset},
    {"mktime", (void*)mktime},
    {"pow", (void*)pow},
    {"printf", (void*)printf},
    {"qsort", (void*)qsort},
    {"rand", (void*)rand},
    {"sin", (void*)sin},
    {"snprintf", (void*)snprintf},
    {"sprintf", (void*)sprintf},
    {"sqrt", (void*)sqrt},
    {"srand", (void*)srand},
    {"sscanf", (void*)sscanf},
    {"strcat", (void*)strcat},
    {"strchr", (void*)strchr},
    {"strcmp", (void*)strcmp},
    {"strcpy", (void*)strcpy},
    {"strcspn", (void*)strcspn},
    {"strftime", (void*)strftime},
    {"stricmp", (void*)stricmp},
    {"strlen", (void*)strlen},
    {"strncat", (void*)strncat},
    {"strncmp", (void*)strncmp},
    {"strncpy", (void*)strncpy},
    {"strnicmp", (void*)strncasecmp},
    {"strpbrk", (void*)strpbrk},
    {"strrchr", (void*)strrchr},
    {"strspn", (void*)strspn},
    {"strstr", (void*)strstr},
    {"strtol", (void*)strtol},
    {"strtoul", (void*)strtoul},
    {"tan", (void*)tan},
    {"vprintf", (void*)vprintf},
    {"vsnprintf", (void*)vsnprintf},
    {"vsprintf", (void*)vsprintf},
    {sEmptyStr, 0},
};

#pragma explicit_zero_data on
const char sEmptyStr[8] = "";
#pragma explicit_zero_data off

#pragma push
#pragma force_active on
static const char sRsoErrors[] =
    "!!!!!Prolog!!!!!\n\0"
    "RSO Module : call constructor (%08x)\n\0"
    "!!!!!Epilog!!!!!\n\0"
    "RSO Module : call destructor (%08x)\n\0"
    "\n[Error]: Unlinked function was called.\n\0"
    "RSOLinkInit ERROR!\n\0\0\0\0";
#pragma pop

int vcmvAbs(int x) { return abs(x); }

void vcmvLinkRSO(void* param1) {
    RSOModule* new_var;

    if (RSOListInit(new_var = (RSOModule*)param1) == 0) {
        OSReport(&sRsoErrors[0x98]);
    }

    for (const RSOImportEntry* ptr = sRsoImports; ptr->func != 0; ptr++) {
        RSOSymbol* temp_r3 = RSOFindExportSymbol(new_var, ptr->name);

        if (temp_r3 != 0) {
            temp_r3->offset = ((u32)ptr->func) - new_var->sections[temp_r3->sectionIdx].addr;
        }
    }
}
