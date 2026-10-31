// setjmp/longjmp. They read and write the registers directly, so the comments are pseudocode with
// the registers written as variables. The jump buffer:
// struct __jmp_buf {
//     void* lr;              // 0x0   return address of the __setjmp call
//     unsigned long cr;      // 0x4
//     void* sp;              // 0x8   r1
//     void* r2;              // 0xC
//     unsigned long unused;  // 0x10  not saved
//     unsigned long gpr[19]; // 0x14  r13..r31
//     struct {
//         double d;          // fN as a double
//         float ps[2];       // fN as a pair of singles (ps0, ps1)
//     } fpr[18];             // 0x60  f14..f31
//     double fpscr;          // 0x180
// };

/* clang-format off */
// int __setjmp(__jmp_buf* env) {
//     env->lr = LR;
//     env->cr = CR;
//     env->sp = r1;
//     env->r2 = r2;
//     for (int i = 13; i <= 31; i++) {
//         env->gpr[i - 13] = r[i];
//     }
//     for (int i = 14; i <= 31; i++) {
//         env->fpr[i - 14].d = f[i];
//         env->fpr[i - 14].ps[0] = ps0(f[i]);
//         env->fpr[i - 14].ps[1] = ps1(f[i]);
//     }
//     env->fpscr = FPSCR;
//     return 0;
// }
asm int __setjmp(register void* env) {
    nofralloc
    mflr r5
    mfcr r6
    stw r5, 0x0(r3)
    stw r6, 0x4(r3)
    stw r1, 0x8(r3)
    stw r2, 0xc(r3)
    stmw r13, 0x14(r3)
    mffs f0
    stfd f14, 0x60(r3)
    addi r4, r3, 0x68
    psq_stx f14, r0, r4, 0, 0
    stfd f15, 0x70(r3)
    addi r4, r3, 0x78
    psq_stx f15, r0, r4, 0, 0
    stfd f16, 0x80(r3)
    addi r4, r3, 0x88
    psq_stx f16, r0, r4, 0, 0
    stfd f17, 0x90(r3)
    addi r4, r3, 0x98
    psq_stx f17, r0, r4, 0, 0
    stfd f18, 0xa0(r3)
    addi r4, r3, 0xa8
    psq_stx f18, r0, r4, 0, 0
    stfd f19, 0xb0(r3)
    addi r4, r3, 0xb8
    psq_stx f19, r0, r4, 0, 0
    stfd f20, 0xc0(r3)
    addi r4, r3, 0xc8
    psq_stx f20, r0, r4, 0, 0
    stfd f21, 0xd0(r3)
    addi r4, r3, 0xd8
    psq_stx f21, r0, r4, 0, 0
    stfd f22, 0xe0(r3)
    addi r4, r3, 0xe8
    psq_stx f22, r0, r4, 0, 0
    stfd f23, 0xf0(r3)
    addi r4, r3, 0xf8
    psq_stx f23, r0, r4, 0, 0
    stfd f24, 0x100(r3)
    addi r4, r3, 0x108
    psq_stx f24, r0, r4, 0, 0
    stfd f25, 0x110(r3)
    addi r4, r3, 0x118
    psq_stx f25, r0, r4, 0, 0
    stfd f26, 0x120(r3)
    addi r4, r3, 0x128
    psq_stx f26, r0, r4, 0, 0
    stfd f27, 0x130(r3)
    addi r4, r3, 0x138
    psq_stx f27, r0, r4, 0, 0
    stfd f28, 0x140(r3)
    addi r4, r3, 0x148
    psq_stx f28, r0, r4, 0, 0
    stfd f29, 0x150(r3)
    addi r4, r3, 0x158
    psq_stx f29, r0, r4, 0, 0
    stfd f30, 0x160(r3)
    addi r4, r3, 0x168
    psq_stx f30, r0, r4, 0, 0
    stfd f31, 0x170(r3)
    addi r4, r3, 0x178
    psq_stx f31, r0, r4, 0, 0
    stfd f0, 0x180(r3)
    li r3, 0
    blr
}

// Restores everything __setjmp saved, so that __setjmp call returns a second time, now with val
// (or 1 if val is 0):
// void longjmp(__jmp_buf* env, int val) {
//     LR = env->lr;
//     CR = env->cr;
//     r1 = env->sp;
//     r2 = env->r2;
//     for (int i = 13; i <= 31; i++) {
//         r[i] = env->gpr[i - 13];
//     }
//     for (int i = 14; i <= 31; i++) {
//         f[i] = env->fpr[i - 14].d;
//         ps0(f[i]) = env->fpr[i - 14].ps[0];
//         ps1(f[i]) = env->fpr[i - 14].ps[1];
//     }
//     FPSCR = env->fpscr;
//     return val != 0 ? val : 1; // to env->lr
// }
asm void longjmp(register void* env, register int val) {
    nofralloc
    lwz r5, 0x0(r3)
    lwz r6, 0x4(r3)
    mtlr r5
    mtcrf 255, r6
    lwz r1, 0x8(r3)
    lwz r2, 0xc(r3)
    lmw r13, 0x14(r3)
    lfd f14, 0x60(r3)
    addi r7, r3, 0x68
    psq_lx f14, r0, r7, 0, 0
    lfd f15, 0x70(r3)
    addi r7, r3, 0x78
    psq_lx f15, r0, r7, 0, 0
    lfd f16, 0x80(r3)
    addi r7, r3, 0x88
    psq_lx f16, r0, r7, 0, 0
    lfd f17, 0x90(r3)
    addi r7, r3, 0x98
    psq_lx f17, r0, r7, 0, 0
    lfd f18, 0xa0(r3)
    addi r7, r3, 0xa8
    psq_lx f18, r0, r7, 0, 0
    lfd f19, 0xb0(r3)
    addi r7, r3, 0xb8
    psq_lx f19, r0, r7, 0, 0
    lfd f20, 0xc0(r3)
    addi r7, r3, 0xc8
    psq_lx f20, r0, r7, 0, 0
    lfd f21, 0xd0(r3)
    addi r7, r3, 0xd8
    psq_lx f21, r0, r7, 0, 0
    lfd f22, 0xe0(r3)
    addi r7, r3, 0xe8
    psq_lx f22, r0, r7, 0, 0
    lfd f23, 0xf0(r3)
    addi r7, r3, 0xf8
    psq_lx f23, r0, r7, 0, 0
    lfd f24, 0x100(r3)
    addi r7, r3, 0x108
    psq_lx f24, r0, r7, 0, 0
    lfd f25, 0x110(r3)
    addi r7, r3, 0x118
    psq_lx f25, r0, r7, 0, 0
    lfd f26, 0x120(r3)
    addi r7, r3, 0x128
    psq_lx f26, r0, r7, 0, 0
    lfd f27, 0x130(r3)
    addi r7, r3, 0x138
    psq_lx f27, r0, r7, 0, 0
    lfd f28, 0x140(r3)
    addi r7, r3, 0x148
    psq_lx f28, r0, r7, 0, 0
    lfd f29, 0x150(r3)
    addi r7, r3, 0x158
    psq_lx f29, r0, r7, 0, 0
    lfd f30, 0x160(r3)
    addi r7, r3, 0x168
    psq_lx f30, r0, r7, 0, 0
    lfd f31, 0x170(r3)
    addi r7, r3, 0x178
    psq_lx f31, r0, r7, 0, 0
    lfd f0, 0x180(r3)
    cmpwi r4, 0
    mr r3, r4
    mtfsf 255, f0
    bnelr
    li r3, 1
    blr
}
/* clang-format on */
