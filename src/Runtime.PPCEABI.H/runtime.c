// Helpers the compiler calls for things the CPU has no single instruction for. They are asm in the
// original Metrowerks runtime; the comment above each one is its C equivalent.

/* clang-format off */
#define save_restore_reg r11

#define SAVE_FPR(reg)           _savefpr_##reg
#define RESTORE_FPR(reg)        _restfpr_##reg
#define SAVE_GPR(reg)           _savegpr_##reg
#define RESTORE_GPR(reg)        _restgpr_##reg

#define ENTRY_SAVE_FPR(reg)     entry SAVE_FPR(reg)
#define ENTRY_RESTORE_FPR(reg)  entry RESTORE_FPR(reg)
#define ENTRY_SAVE_GPR(reg)     entry SAVE_GPR(reg)
#define ENTRY_RESTORE_GPR(reg)  entry RESTORE_GPR(reg)

void SAVE_FPR(14)(void);
void SAVE_FPR(15)(void);
void SAVE_FPR(16)(void);
void SAVE_FPR(17)(void);
void SAVE_FPR(18)(void);
void SAVE_FPR(19)(void);
void SAVE_FPR(20)(void);
void SAVE_FPR(21)(void);
void SAVE_FPR(22)(void);
void SAVE_FPR(23)(void);
void SAVE_FPR(24)(void);
void SAVE_FPR(25)(void);
void SAVE_FPR(26)(void);
void SAVE_FPR(27)(void);
void SAVE_FPR(28)(void);
void SAVE_FPR(29)(void);
void SAVE_FPR(30)(void);
void SAVE_FPR(31)(void);

void RESTORE_FPR(14)(void);
void RESTORE_FPR(15)(void);
void RESTORE_FPR(16)(void);
void RESTORE_FPR(17)(void);
void RESTORE_FPR(18)(void);
void RESTORE_FPR(19)(void);
void RESTORE_FPR(20)(void);
void RESTORE_FPR(21)(void);
void RESTORE_FPR(22)(void);
void RESTORE_FPR(23)(void);
void RESTORE_FPR(24)(void);
void RESTORE_FPR(25)(void);
void RESTORE_FPR(26)(void);
void RESTORE_FPR(27)(void);
void RESTORE_FPR(28)(void);
void RESTORE_FPR(29)(void);
void RESTORE_FPR(30)(void);
void RESTORE_FPR(31)(void);

void SAVE_GPR(14)(void);
void SAVE_GPR(15)(void);
void SAVE_GPR(16)(void);
void SAVE_GPR(17)(void);
void SAVE_GPR(18)(void);
void SAVE_GPR(19)(void);
void SAVE_GPR(20)(void);
void SAVE_GPR(21)(void);
void SAVE_GPR(22)(void);
void SAVE_GPR(23)(void);
void SAVE_GPR(24)(void);
void SAVE_GPR(25)(void);
void SAVE_GPR(26)(void);
void SAVE_GPR(27)(void);
void SAVE_GPR(28)(void);
void SAVE_GPR(29)(void);
void SAVE_GPR(30)(void);
void SAVE_GPR(31)(void);

void RESTORE_GPR(14)(void);
void RESTORE_GPR(15)(void);
void RESTORE_GPR(16)(void);
void RESTORE_GPR(17)(void);
void RESTORE_GPR(18)(void);
void RESTORE_GPR(19)(void);
void RESTORE_GPR(20)(void);
void RESTORE_GPR(21)(void);
void RESTORE_GPR(22)(void);
void RESTORE_GPR(23)(void);
void RESTORE_GPR(24)(void);
void RESTORE_GPR(25)(void);
void RESTORE_GPR(26)(void);
void RESTORE_GPR(27)(void);
void RESTORE_GPR(28)(void);
void RESTORE_GPR(29)(void);
void RESTORE_GPR(30)(void);
void RESTORE_GPR(31)(void);

static const double __constants[] = {
    0.0,
    4294967296.0, // 2^32
    2147483648.0, // 2^31
};

// double to unsigned int. fctiwz only converts to a signed int, so values from 2^31 up are moved
// down by 2^31 first and the top bit is added back:
// unsigned long __cvt_fp2unsigned(double d) {
//     if (d < 0.0) {
//         return 0;
//     }
//     if (d >= 4294967296.0) {
//         return 0xFFFFFFFF;
//     }
//     if (d < 2147483648.0) {
//         return (long)d;
//     }
//     return (long)(d - 2147483648.0) + 0x80000000;
// }
asm unsigned long __cvt_fp2unsigned(register double d) {
    nofralloc
    stwu r1, -16(r1)
    lis r4, __constants@ha
    addi r4, r4, __constants@l
    li r3, 0
    lfd f0, 0(r4)
    lfd f3, 8(r4)
    lfd f4, 16(r4)
    fcmpu cr0, f1, f0
    fcmpu cr6, f1, f3
    blt @exit
    subi r3, r3, 1
    bge cr6, @exit
    fcmpu cr7, f1, f4
    fmr f2, f1
    blt cr7, @1
    fsub f2, f1, f4
@1:
    fctiwz f2, f2
    stfd f2, 8(r1)
    lwz r3, 12(r1)
    blt cr7, @exit
    addis r3, r3, 0x8000
@exit:
    addi r1, r1, 0x10
    blr
}

// Shared prologue code for functions that keep many float registers. A function whose first saved
// register is fN calls _savefpr_N with r11 pointing just past its float save area. Each entry falls
// through to the next, so fN..f31 are saved:
// void _savefpr_N(double* end /* r11 */) {
//     for (int i = N; i <= 31; i++) {
//         end[i - 32] = f[i];
//     }
// }
asm void __save_fpr(void) {
    nofralloc
    ENTRY_SAVE_FPR(14)
        stfd f14, -144(save_restore_reg)
    ENTRY_SAVE_FPR(15)
        stfd f15, -136(save_restore_reg)
    ENTRY_SAVE_FPR(16)
        stfd f16, -128(save_restore_reg)
    ENTRY_SAVE_FPR(17)
        stfd f17, -120(save_restore_reg)
    ENTRY_SAVE_FPR(18)
        stfd f18, -112(save_restore_reg)
    ENTRY_SAVE_FPR(19)
        stfd f19, -104(save_restore_reg)
    ENTRY_SAVE_FPR(20)
        stfd f20, -96(save_restore_reg)
    ENTRY_SAVE_FPR(21)
        stfd f21, -88(save_restore_reg)
    ENTRY_SAVE_FPR(22)
        stfd f22, -80(save_restore_reg)
    ENTRY_SAVE_FPR(23)
        stfd f23, -72(save_restore_reg)
    ENTRY_SAVE_FPR(24)
        stfd f24, -64(save_restore_reg)
    ENTRY_SAVE_FPR(25)
        stfd f25, -56(save_restore_reg)
    ENTRY_SAVE_FPR(26)
        stfd f26, -48(save_restore_reg)
    ENTRY_SAVE_FPR(27)
        stfd f27, -40(save_restore_reg)
    ENTRY_SAVE_FPR(28)
        stfd f28, -32(save_restore_reg)
    ENTRY_SAVE_FPR(29)
        stfd f29, -24(save_restore_reg)
    ENTRY_SAVE_FPR(30)
        stfd f30, -16(save_restore_reg)
    ENTRY_SAVE_FPR(31)
        stfd f31, -8(save_restore_reg)
        blr
}

// The matching epilogue code:
// void _restfpr_N(double* end /* r11 */) {
//     for (int i = N; i <= 31; i++) {
//         f[i] = end[i - 32];
//     }
// }
asm void __restore_fpr(void) {
    nofralloc
    ENTRY_RESTORE_FPR(14)
        lfd f14, -144(save_restore_reg)
    ENTRY_RESTORE_FPR(15)
        lfd f15, -136(save_restore_reg)
    ENTRY_RESTORE_FPR(16)
        lfd f16, -128(save_restore_reg)
    ENTRY_RESTORE_FPR(17)
        lfd f17, -120(save_restore_reg)
    ENTRY_RESTORE_FPR(18)
        lfd f18, -112(save_restore_reg)
    ENTRY_RESTORE_FPR(19)
        lfd f19, -104(save_restore_reg)
    ENTRY_RESTORE_FPR(20)
        lfd f20, -96(save_restore_reg)
    ENTRY_RESTORE_FPR(21)
        lfd f21, -88(save_restore_reg)
    ENTRY_RESTORE_FPR(22)
        lfd f22, -80(save_restore_reg)
    ENTRY_RESTORE_FPR(23)
        lfd f23, -72(save_restore_reg)
    ENTRY_RESTORE_FPR(24)
        lfd f24, -64(save_restore_reg)
    ENTRY_RESTORE_FPR(25)
        lfd f25, -56(save_restore_reg)
    ENTRY_RESTORE_FPR(26)
        lfd f26, -48(save_restore_reg)
    ENTRY_RESTORE_FPR(27)
        lfd f27, -40(save_restore_reg)
    ENTRY_RESTORE_FPR(28)
        lfd f28, -32(save_restore_reg)
    ENTRY_RESTORE_FPR(29)
        lfd f29, -24(save_restore_reg)
    ENTRY_RESTORE_FPR(30)
        lfd f30, -16(save_restore_reg)
    ENTRY_RESTORE_FPR(31)
        lfd f31, -8(save_restore_reg)
        blr
}

// The same for the integer registers rN..r31:
// void _savegpr_N(unsigned long* end /* r11 */) {
//     for (int i = N; i <= 31; i++) {
//         end[i - 32] = r[i];
//     }
// }
asm void __save_gpr(void) {
    nofralloc
    ENTRY_SAVE_GPR(14)
        stw r14, -72(save_restore_reg)
    ENTRY_SAVE_GPR(15)
        stw r15, -68(save_restore_reg)
    ENTRY_SAVE_GPR(16)
        stw r16, -64(save_restore_reg)
    ENTRY_SAVE_GPR(17)
        stw r17, -60(save_restore_reg)
    ENTRY_SAVE_GPR(18)
        stw r18, -56(save_restore_reg)
    ENTRY_SAVE_GPR(19)
        stw r19, -52(save_restore_reg)
    ENTRY_SAVE_GPR(20)
        stw r20, -48(save_restore_reg)
    ENTRY_SAVE_GPR(21)
        stw r21, -44(save_restore_reg)
    ENTRY_SAVE_GPR(22)
        stw r22, -40(save_restore_reg)
    ENTRY_SAVE_GPR(23)
        stw r23, -36(save_restore_reg)
    ENTRY_SAVE_GPR(24)
        stw r24, -32(save_restore_reg)
    ENTRY_SAVE_GPR(25)
        stw r25, -28(save_restore_reg)
    ENTRY_SAVE_GPR(26)
        stw r26, -24(save_restore_reg)
    ENTRY_SAVE_GPR(27)
        stw r27, -20(save_restore_reg)
    ENTRY_SAVE_GPR(28)
        stw r28, -16(save_restore_reg)
    ENTRY_SAVE_GPR(29)
        stw r29, -12(save_restore_reg)
    ENTRY_SAVE_GPR(30)
        stw r30, -8(save_restore_reg)
    ENTRY_SAVE_GPR(31)
        stw r31, -4(save_restore_reg)
        blr
}

// void _restgpr_N(unsigned long* end /* r11 */) {
//     for (int i = N; i <= 31; i++) {
//         r[i] = end[i - 32];
//     }
// }
asm void __restore_gpr(void) {
    nofralloc
    ENTRY_RESTORE_GPR(14)
        lwz r14, -72(save_restore_reg)
    ENTRY_RESTORE_GPR(15)
        lwz r15, -68(save_restore_reg)
    ENTRY_RESTORE_GPR(16)
        lwz r16, -64(save_restore_reg)
    ENTRY_RESTORE_GPR(17)
        lwz r17, -60(save_restore_reg)
    ENTRY_RESTORE_GPR(18)
        lwz r18, -56(save_restore_reg)
    ENTRY_RESTORE_GPR(19)
        lwz r19, -52(save_restore_reg)
    ENTRY_RESTORE_GPR(20)
        lwz r20, -48(save_restore_reg)
    ENTRY_RESTORE_GPR(21)
        lwz r21, -44(save_restore_reg)
    ENTRY_RESTORE_GPR(22)
        lwz r22, -40(save_restore_reg)
    ENTRY_RESTORE_GPR(23)
        lwz r23, -36(save_restore_reg)
    ENTRY_RESTORE_GPR(24)
        lwz r24, -32(save_restore_reg)
    ENTRY_RESTORE_GPR(25)
        lwz r25, -28(save_restore_reg)
    ENTRY_RESTORE_GPR(26)
        lwz r26, -24(save_restore_reg)
    ENTRY_RESTORE_GPR(27)
        lwz r27, -20(save_restore_reg)
    ENTRY_RESTORE_GPR(28)
        lwz r28, -16(save_restore_reg)
    ENTRY_RESTORE_GPR(29)
        lwz r29, -12(save_restore_reg)
    ENTRY_RESTORE_GPR(30)
        lwz r30, -8(save_restore_reg)
    ENTRY_RESTORE_GPR(31)
        lwz r31, -4(save_restore_reg)
        blr
}

// 64-bit integer helpers, called for long long arithmetic. Each 64-bit value is split across a
// register pair, high word first (a in r3:r4, b in r5:r6, result in r3:r4). cntlz64(x) below is
// the number of leading zero bits in x (64 for 0).

// unsigned long long __div2u(unsigned long long a, unsigned long long b) {
//     int a_bits = 64 - cntlz64(a);
//     int b_bits = 64 - cntlz64(b);
//     if (a_bits < b_bits) {
//         return 0; // a < b
//     }
//     // Shift-and-subtract long division, one quotient bit per step. The asm skips the first
//     // b_bits - 1 steps (they can't produce a 1) by starting with rem = a >> (a_bits - b_bits + 1).
//     unsigned long long rem = 0;
//     unsigned long long quot = 0;
//     for (int i = a_bits - 1; i >= 0; i--) {
//         rem = (rem << 1) | ((a >> i) & 1);
//         quot <<= 1;
//         if (rem >= b) {
//             rem -= b;
//             quot |= 1;
//         }
//     }
//     return quot;
// }
asm void __div2u(void) {
    nofralloc
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne @1
    addi r0, r9, 0x20
@1:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne @2
    addi r9, r10, 0x20
@2:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt @8
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt @3
    srw r8, r3, r7
    li r7, 0x0
    b @4
@3:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
@4:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt @5
    slw r3, r4, r9
    li r4, 0x0
    b @6
@5:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
@6:
    li r10, -0x1
    addic r7, r7, 0x0
@7:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt @7b
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
@7b:
    bdnz @7
    adde r4, r4, r4
    adde r3, r3, r3
    blr
@8:
    li r4, 0x0
    li r3, 0x0
    blr
}

// long long __div2i(long long a, long long b) {
//     unsigned long long quot = __div2u(a < 0 ? -a : a, b < 0 ? -b : b); // the asm repeats __div2u's body
//     return (a < 0) != (b < 0) ? -(long long)quot : quot;
// }
asm void __div2i(void) {
    nofralloc
    stwu r1, -0x10(r1)
    clrrwi. r9, r3, 31
    beq @1
    subfic r4, r4, 0x0
    subfze r3, r3
@1:
    stw r9, 0x8(r1)
    clrrwi. r10, r5, 31
    beq @2
    subfic r6, r6, 0x0
    subfze r5, r5
@2:
    stw r10, 0xc(r1)
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne @3
    addi r0, r9, 0x20
@3:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne @4
    addi r9, r10, 0x20
@4:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt @11
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt @5
    srw r8, r3, r7
    li r7, 0x0
    b @6
@5:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
@6:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt @7
    slw r3, r4, r9
    li r4, 0x0
    b @8
@7:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
@8:
    li r10, -0x1
    addic r7, r7, 0x0
@9:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt @9b
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
@9b:
    bdnz @9
    adde r4, r4, r4
    adde r3, r3, r3
    lwz r9, 0x8(r1)
    lwz r10, 0xc(r1)
    xor. r7, r9, r10
    beq @10
    cmpwi r9, 0x0
    subfic r4, r4, 0x0
    subfze r3, r3
@10:
    b @12
@11:
    li r4, 0x0
    li r3, 0x0
@12:
    addi r1, r1, 0x10
    blr
}

// a % b: the __div2u loop, returning a when a_bits < b_bits and rem at the end:
// unsigned long long __mod2u(unsigned long long a, unsigned long long b);
asm void __mod2u(void) {
    nofralloc
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne @1
    addi r0, r9, 0x20
@1:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne @2
    addi r9, r10, 0x20
@2:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt @8
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt @3
    srw r8, r3, r7
    li r7, 0x0
    b @4
@3:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
@4:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt @5
    slw r3, r4, r9
    li r4, 0x0
    b @6
@5:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
@6:
    li r10, -0x1
    addic r7, r7, 0x0
@7:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt @7b
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
@7b:
    bdnz @7
    mr r4, r8
    mr r3, r7
    blr
@8:
    blr
}

// The remainder takes the sign of a:
// long long __mod2i(long long a, long long b) {
//     unsigned long long rem = __mod2u(a < 0 ? -a : a, b < 0 ? -b : b); // the asm repeats __mod2u's body
//     return a < 0 ? -(long long)rem : rem;
// }
asm void __mod2i(void) {
    nofralloc
    cmpwi cr7, r3, 0x0
    bge cr7, @1
    subfic r4, r4, 0x0
    subfze r3, r3
@1:
    cmpwi r5, 0x0
    bge @2
    subfic r6, r6, 0x0
    subfze r5, r5
@2:
    cmpwi r3, 0x0
    cntlzw r0, r3
    cntlzw r9, r4
    bne @3
    addi r0, r9, 0x20
@3:
    cmpwi r5, 0x0
    cntlzw r9, r5
    cntlzw r10, r6
    bne @4
    addi r9, r10, 0x20
@4:
    cmpw r0, r9
    subfic r10, r0, 0x40
    bgt @10
    addi r9, r9, 0x1
    subfic r9, r9, 0x40
    add r0, r0, r9
    subf r9, r9, r10
    mtctr r9
    cmpwi r9, 0x20
    subi r7, r9, 0x20
    blt @5
    srw r8, r3, r7
    li r7, 0x0
    b @6
@5:
    srw r8, r4, r9
    subfic r7, r9, 0x20
    slw r7, r3, r7
    or r8, r8, r7
    srw r7, r3, r9
@6:
    cmpwi r0, 0x20
    subic r9, r0, 0x20
    blt @7
    slw r3, r4, r9
    li r4, 0x0
    b @8
@7:
    slw r3, r3, r0
    subfic r9, r0, 0x20
    srw r9, r4, r9
    or r3, r3, r9
    slw r4, r4, r0
@8:
    li r10, -0x1
    addic r7, r7, 0x0
@9:
    adde r4, r4, r4
    adde r3, r3, r3
    adde r8, r8, r8
    adde r7, r7, r7
    subfc r0, r6, r8
    subfe. r9, r5, r7
    blt @9b
    mr r8, r0
    mr r7, r9
    addic r0, r10, 0x1
@9b:
    bdnz @9
    mr r4, r8
    mr r3, r7
@10:
    bge cr7, @11
    subfic r4, r4, 0x0
    subfze r3, r3
@11:
    blr
}

// x << n for n = 0..63, on the two 32-bit halves (n in r5). slw/srw give 0 for shift counts of
// 32..63, and a negative count is read as its low 6 bits, which lands there too. That covers
// n < 32 and n >= 32 without a branch:
// long long __shl2i(long long x, int n) {
//     unsigned long hi = x >> 32;
//     unsigned long lo = x;
//     hi = (hi << n) | (lo >> (32 - n)) | (lo << (n - 32));
//     lo = lo << n;
//     return (long long)hi << 32 | lo;
// }
asm void __shl2i(void) {
    nofralloc
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    slw r3, r3, r5
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r5
    blr
}

// long long to double, built from the IEEE-754 bits (x in r3:r4, result in f1):
// double __cvt_sll_dbl(long long x) {
//     unsigned long long sign = 0;
//     unsigned long long m = x;
//     if (x < 0) {
//         sign = 1ULL << 63;
//         m = -m;
//     }
//     if (m == 0) {
//         return 0.0;
//     }
//     int shift = cntlz64(m);
//     m <<= shift;                       // bit 63 is now the leading 1
//     int exp = 1023 + 63 - shift;
//     unsigned long dropped = m & 0x7FF; // the 11 bits that don't fit in the 52-bit mantissa
//     if (dropped > 0x400 || (dropped == 0x400 && (m & 0x800))) { // round to nearest, ties to even
//         m += 0x800;
//         if (m < 0x800) {
//             exp++;                     // the carry ran out of bit 63
//         }
//     }
//     unsigned long long bits = sign | (unsigned long long)exp << 52 | ((m >> 11) & 0x000FFFFFFFFFFFFF);
//     return *(double*)&bits;
// }
asm void __cvt_sll_dbl(void) {
    nofralloc
    stwu r1, -0x10(r1)
    clrrwi. r5, r3, 31
    beq @1
    subfic r4, r4, 0x0
    subfze r3, r3
@1:
    or. r7, r3, r4
    li r6, 0x0
    beq @4
    cntlzw r7, r3
    cntlzw r8, r4
    extlwi r9, r7, 5, 26
    srawi r9, r9, 31
    and r9, r9, r8
    add r7, r7, r9
    subfic r8, r7, 0x20
    subic r9, r7, 0x20
    slw r3, r3, r7
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r7
    subf r6, r7, r6
    clrlwi r7, r4, 21
    cmpwi r7, 0x400
    addi r6, r6, 0x43e
    blt @3
    bgt @2
    rlwinm. r7, r4, 0, 20, 20
    beq @3
@2:
    addic r4, r4, 0x800
    addze r3, r3
    addze r6, r6
@3:
    rotrwi r4, r4, 11
    rlwimi r4, r3, 21, 0, 10
    extrwi r3, r3, 20, 1
    slwi r6, r6, 20
    or r3, r6, r3
    or r3, r5, r3
@4:
    stw r3, 0x8(r1)
    stw r4, 0xc(r1)
    lfd f1, 0x8(r1)
    addi r1, r1, 0x10
    blr
}

// double to unsigned long long, from the IEEE-754 bits (d in f1, result in r3:r4):
// unsigned long long __cvt_dbl_ull(double d) {
//     unsigned long long bits = *(unsigned long long*)&d;
//     int exp = (bits >> 52) & 0x7FF;
//     if (exp < 1023 || (bits >> 63)) {
//         return 0;                                            // |d| < 1, or negative
//     }
//     unsigned long long m = (bits & 0x000FFFFFFFFFFFFF) | (1ULL << 52); // add the leading 1
//     int shift = exp - (1023 + 52);
//     if (shift < 0) {
//         return m >> -shift;                                  // drop the fraction bits
//     }
//     if (shift > 11) {
//         return 0xFFFFFFFFFFFFFFFF;                           // 2^64 or more (or inf/NaN)
//     }
//     return m << shift;
// }
asm void __cvt_dbl_ull(void) {
    nofralloc
    stwu r1, -0x10(r1)
    stfd f1, 0x8(r1)
    lwz r3, 0x8(r1)
    lwz r4, 0xc(r1)
    extrwi r5, r3, 11, 1
    cmplwi r5, 0x3ff
    bge @2
@1:
    li r3, 0x0
    li r4, 0x0
    b @5
@2:
    clrrwi. r6, r3, 31
    bne @1
    clrlwi r3, r3, 12
    oris r3, r3, 0x10
    subi r5, r5, 0x433
    cmpwi r5, 0x0
    bge @3
    neg r5, r5
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    srw r4, r4, r5
    slw r10, r3, r8
    or r4, r4, r10
    srw r10, r3, r9
    or r4, r4, r10
    srw r3, r3, r5
    b @5
@3:
    cmpwi r5, 0xb
    ble+ @4
    li r3, -0x1
    li r4, -0x1
    b @5
@4:
    subfic r8, r5, 0x20
    subic r9, r5, 0x20
    slw r3, r3, r5
    srw r10, r4, r8
    or r3, r3, r10
    slw r10, r4, r9
    or r3, r3, r10
    slw r4, r4, r5
@5:
    addi r1, r1, 0x10
    blr
}
/* clang-format on */
