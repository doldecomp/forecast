// RVL_SDK HBM (Home Button Menu) - controller and remote speaker.
//
// Third translation unit of libhbm:
//
//   homebutton::Controller (wpadConnectCallback, wpadExtensionCallback,
//   soundOnCallback, initCallback, clearCallback, setKpad, getInfoAsync,
//   startMotor, stopMotor, isPlayingSoundId, and the WPAD/kpad helpers
//   0x8007D1FC ... 0x8007DDA0)
//   homebutton::RemoteSpk (Start, Connect, Play, isPlaying, isPlayingId,
//   isPlayReady and the helpers 0x8007DE00 ... 0x8007E2A8)
//   fn_8007E2A8 - the menu-result bridge the vcmv (Opera) code calls to enter
//   the manual/black-out states; it plays the HBM sound ids through fn_80072354
//   fn_8007E378 - empty stub
//
// Split evidence: the .sdata2 pool restarts with its own 1.0f at 0x803321F8
// (the gui unit's pool stopped at 0x803321F0), .data 0x801B5488 holds the
// controller table {fn_8007DE00}, .bss 0x80288D98-0x80288F28 holds the WPAD
// status blocks, and .sbss 0x80330D70-0x80330DA0 holds the controller, kpad
// and menu-result globals (the first of them is used from 0x8007D1FC on).
// It has no .rodata.
