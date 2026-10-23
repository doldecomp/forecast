// RVL_SDK HBM (Home Button Menu) - base and menu state machine.
//
// This is the first translation unit of libhbm. It owns the version string
// "<< RVL_SDK - HBM release build: May 16 2007 ... >>" at .data 0x801B4730,
// the C entry points and the homebutton::HomeButton implementation:
//
//   HBMCreate / HBMDelete / HBMInit / HBMCalc / HBMDraw / HBMGetSelectBtnNum
//   HBMSetAdjustFlag / HBMStartBlackOut / HBMCreateSound / HBMDeleteSound /
//   HBMUpdateSound
//   homebutton::HomeButton (ctor, set_text, calc_battery, updateTrigPane,
//   startLeftEvent, reset_btn, getPaneNo, onEvent) and its sound/GX helpers
//   (fn_80072354 ... fn_80076024, fn_8007699C, fn_800777A8, fn_80079448,
//   fn_8007B374 initgx, fn_8007B52C, fn_8007B68C, fn_8007B7B0 and the empty
//   virtual stubs fn_8007B870-fn_8007B884)
//
// Split evidence: the unit ends where the GroupAnmController class starts
// (see HBMGui.cpp). Its .data block is 0x801B4730-0x801B5320 (the version
// string, the pane/animation name tables and the HomeButton vtable
// 0x801B52E0), its .rodata 0x80193DC8-0x80194078 is the two tables only
// HomeButton code uses, its .bss 0x80288D60-0x80288D98 holds the sound
// archive/player, its .sdata2 pool 0x80332168-0x803321E0 is consumed before
// the gui unit emits its own 1.0f at 0x803321E0 (MWCC emits one pool per TU),
// and its .sbss is the single HBM global at 0x80330D60.
// No .ctors: the HBM objects have no static constructors.
