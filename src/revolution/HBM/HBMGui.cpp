// RVL_SDK HBM (Home Button Menu) - gui framework and animation controllers.
//
// Second translation unit of libhbm. It starts with homebutton::GroupAnmController
// (ctor fn_8007B888, deleting dtor fn_8007B8A4, do_calc, and the virtual
// method fn_8007B9EC) and continues with the gui framework:
//
//   homebutton::FrameController     (init, initFrame)
//   homebutton::gui::Component, PaneComponent, PaneManager, Manager,
//   EventHandler (init, update, onTrig, isVisible, addComponent, getComponent,
//   dtors, setEventHandler, setAllComponentTriggerTarget, ...)
//   fn_8007BB34 / fn_8007CBB0 / fn_8007CA80 / fn_8007C610 / fn_8007C6B0
//
// Split evidence: the unit owns the GroupAnmController vtable 0x801B5320. Its
// ctor fn_8007B888 installs that table, its deleting dtor fn_8007B8A4 and its
// virtual method fn_8007B9EC are the table's entries, so ctor, dtor, vtable
// and method are one TU and the unit opens at 0x8007B888. fn_8007B9EC also
// emits this unit's own 1.0f at 0x803321E0 instead of reusing the one at
// 0x8033217C. The rest of .data
// 0x801B5320-0x801B5488 is exactly the four gui vtables (Component,
// PaneComponent, PaneManager, Manager), and the unit owns the single gui
// global in .sbss at 0x80330D68. It has no .bss and no .rodata of its own.
