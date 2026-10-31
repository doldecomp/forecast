typedef struct PTMF {
    long this_delta; // at 0x0
    long v_offset;   // at 0x4
    union {
        void* f_addr;
        long ve_offset;
    } f_data;        // at 0x8
} PTMF;

const PTMF __ptmf_null = { 0, 0, 0 };

// Pointer-to-member-function helpers, called by the compiler for `if (pmf)`, `pmf1 == pmf2` and
// `(obj->*pmf)(...)`. They are asm in the original Metrowerks runtime; the comment above each one
// is its C equivalent.

/* clang-format off */
// long __ptmf_test(PTMF* ptmf) {
//     return ptmf->this_delta != 0 || ptmf->v_offset != 0 || ptmf->f_data.f_addr != 0;
// }
asm long __ptmf_test(register PTMF* ptmf) {
    nofralloc
    lwz r5, PTMF.this_delta(r3)
    lwz r6, PTMF.v_offset(r3)
    lwz r7, PTMF.f_data(r3)
    li r3, 1
    cmpwi cr0, r5, 0
    cmpwi cr6, r6, 0
    cmpwi cr7, r7, 0
    bnelr cr0
    bnelr cr6
    bnelr cr7
    li r3, 0
    blr
}

// Returns 0 when the two are equal:
// long __ptmf_cmpr(PTMF* ptmf1, PTMF* ptmf2) {
//     return ptmf1->this_delta != ptmf2->this_delta || ptmf1->v_offset != ptmf2->v_offset ||
//            ptmf1->f_data.f_addr != ptmf2->f_data.f_addr;
// }
asm long __ptmf_cmpr(register PTMF* ptmf1, register PTMF* ptmf2) {
    nofralloc
    lwz r5, PTMF.this_delta(r3)
    lwz r6, PTMF.this_delta(r4)
    lwz r7, PTMF.v_offset(r3)
    lwz r8, PTMF.v_offset(r4)
    lwz r9, PTMF.f_data(r3)
    lwz r10, PTMF.f_data(r4)
    li r3, 1
    cmpw cr0, r5, r6
    cmpw cr6, r7, r8
    cmpw cr7, r9, r10
    bnelr cr0
    bnelr cr6
    bnelr cr7
    li r3, 0
    blr
}

// Calls (obj->*ptmf)(args...). The PTMF comes in r12, which is not an argument register, so the
// arguments pass through untouched (only obj is adjusted). It jumps to the member function instead
// of calling it, so the member function returns straight to the caller:
// R __ptmf_scall(Class* obj, Args... args, PTMF* ptmf /* r12 */) {
//     char* self = (char*)obj + ptmf->this_delta;
//     void* func;
//     if (ptmf->v_offset < 0) {
//         func = ptmf->f_data.f_addr;                              // non-virtual
//     } else {
//         char* vtable = *(char**)(self + ptmf->f_data.ve_offset); // virtual: look it up
//         func = *(void**)(vtable + ptmf->v_offset);
//     }
//     return ((R (*)(void*, Args...))func)(self, args...);
// }
asm void __ptmf_scall(...) {
    nofralloc
    lwz r0, PTMF.this_delta(r12)
    lwz r11, PTMF.v_offset(r12)
    lwz r12, PTMF.f_data(r12)
    add r3, r3, r0
    cmpwi cr0, r11, 0
    blt cr0, @1
    lwzx r12, r3, r12
    lwzx r12, r12, r11
@1:
    mtctr r12
    bctr
}

// Same as __ptmf_scall, for member functions that return a struct: r3 holds the hidden
// return-value pointer, so obj is the second argument (r4).
asm void __ptmf_scall4(...) {
    nofralloc
    lwz r0, PTMF.this_delta(r12)
    lwz r11, PTMF.v_offset(r12)
    lwz r12, PTMF.f_data(r12)
    add r4, r4, r0
    cmpwi cr0, r11, 0
    blt cr0, @1
    lwzx r12, r4, r12
    lwzx r12, r12, r11
@1:
    mtctr r12
    bctr
}
/* clang-format on */
