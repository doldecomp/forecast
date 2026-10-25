#include <revolution/IPC/ipcHardware.h>
#include <revolution/OS.h>
#include <revolution/VI.h>

/**
 * The video encoder sits on an I2C bus that is driven by hand through two
 * Hollywood GPIO pins, GPIO_AVE_SCL and GPIO_AVE_SDA.
 */
#define GPIO_SCL_SHIFT 14
#define GPIO_SDA_SHIFT 15

// When zero, the SDA line is inverted
static volatile u32 __i2c_ident_flag = 1;
static volatile u32 __i2c_ident_first = 0;

void WaitMicroTime(s32 usec) {
    OSTime start = __OSGetSystemTime();

    while (OS_TICKS_TO_USEC(__OSGetSystemTime() - start) < usec) {
        ;
    }
}

static inline void VICheckI2C(void) {
    __i2c_ident_flag = 1;
}

static inline BOOL __VISetSCL(u32 value) {
    u32 reg;
    value &= 1;

    reg = IPC_HW_REGS[IPC_GPIO1BOUT];
    reg &= ~GPIO_AVE_SCL;
    reg = (u32)(reg | (value << GPIO_SCL_SHIFT));
    IPC_HW_REGS[IPC_GPIO1BOUT] = reg;
    return TRUE;
}

static inline BOOL __VISetSDA(u32 value) {
    u32 reg;
    value &= 1;

    reg = IPC_HW_REGS[IPC_GPIO1BOUT];
    reg &= ~GPIO_AVE_SDA;
    reg = (u32)(reg | (value << GPIO_SDA_SHIFT));
    IPC_HW_REGS[IPC_GPIO1BOUT] = reg;
    return TRUE;
}

static inline u32 __VIGetSDA(void) {
    u32 reg = IPC_HW_REGS[IPC_GPIO1BIN];
    return (u32)((reg >> GPIO_SDA_SHIFT) & 1);
}

// Takes SCL as an output; SDA becomes an output (1) or an input (0)
static inline void __VIOpenI2C(u32 dir) {
    u32 reg = IPC_HW_REGS[IPC_GPIO1BDIR];
    reg = (u32)(reg & ~GPIO_AVE_SDA);
    reg = (u32)(reg | GPIO_AVE_SCL | (dir << GPIO_SDA_SHIFT));
    IPC_HW_REGS[IPC_GPIO1BDIR] = reg;
}

static inline int wait4ClkHigh(void) {
    WaitMicroTime(2);
    return 1;
}

static s32 sendSlaveAddr(u8 slaveAddr) {
    int i;

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    WaitMicroTime(2);
    __VISetSCL(0);

    for (i = 0; i < 8; i++) {
        if (slaveAddr & 0x80) {
            if (0 == __i2c_ident_flag) {
                __VISetSDA(0);
            } else {
                __VISetSDA(1);
            }
        } else {
            if (0 == __i2c_ident_flag) {
                __VISetSDA(1);
            } else {
                __VISetSDA(0);
            }
        }

        WaitMicroTime(2);
        __VISetSCL(1);

        if (!wait4ClkHigh()) {
            return 0;
        }

        __VISetSCL(0);
        slaveAddr <<= 1;
    }

    // Acknowledge
    __VIOpenI2C(0);
    WaitMicroTime(2);
    __VISetSCL(1);

    if (!wait4ClkHigh()) {
        return 0;
    }

    if (1 == __i2c_ident_flag) {
        if (__VIGetSDA() != 0) {
            return 0;
        }
    }

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    __VIOpenI2C(1);
    __VISetSCL(0);

    return 1;
}

s32 __VISendI2CData(u8 slaveAddr, u8* data, s32 size) {
    int i;
    u8 byte;
    BOOL enabled;

    if (__i2c_ident_first == 0) {
        VICheckI2C();
        __i2c_ident_first = 1;
    }

    enabled = OSDisableInterrupts();

    // Start condition
    __VIOpenI2C(1);
    __VISetSCL(1);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(0);
    } else {
        __VISetSDA(1);
    }

    WaitMicroTime(2);
    WaitMicroTime(2);

    if (!sendSlaveAddr(slaveAddr)) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    __VIOpenI2C(1);

    while (size) {
        byte = *data++;

        for (i = 0; i < 8; i++) {
            if (byte & 0x80) {
                if (0 == __i2c_ident_flag) {
                    __VISetSDA(0);
                } else {
                    __VISetSDA(1);
                }
            } else {
                if (0 == __i2c_ident_flag) {
                    __VISetSDA(1);
                } else {
                    __VISetSDA(0);
                }
            }

            WaitMicroTime(2);
            __VISetSCL(1);

            if (!wait4ClkHigh()) {
                OSRestoreInterrupts(enabled);
                return 0;
            }

            __VISetSCL(0);
            byte <<= 1;
        }

        // Acknowledge
        __VIOpenI2C(0);
        WaitMicroTime(2);
        __VISetSCL(1);

        if (!wait4ClkHigh()) {
            OSRestoreInterrupts(enabled);
            return 0;
        }

        if (1 == __i2c_ident_flag) {
            if (__VIGetSDA() != 0) {
                OSRestoreInterrupts(enabled);
                return 0;
            }
        }

        if (0 == __i2c_ident_flag) {
            __VISetSDA(1);
        } else {
            __VISetSDA(0);
        }

        __VIOpenI2C(1);
        __VISetSCL(0);
        size--;
    }

    // Stop condition
    __VIOpenI2C(1);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(1);
    } else {
        __VISetSDA(0);
    }

    WaitMicroTime(2);
    __VISetSCL(1);
    WaitMicroTime(2);

    if (0 == __i2c_ident_flag) {
        __VISetSDA(0);
    } else {
        __VISetSDA(1);
    }

    OSRestoreInterrupts(enabled);
    return 1;
}
