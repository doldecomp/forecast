.include "macros.inc"

.data
.balign 4

.obj sHciControllerError, global
.string "Ctlr H/w error event"
.endobj sHciControllerError

.obj sHciControllerErrorPad, global
.zero 3
.endobj sHciControllerErrorPad

.obj sHciEventMismatch, global
.string "Event mismatch opcode=%X cmd opcode=%X"
.endobj sHciEventMismatch

.obj sHciEventMismatchPad, global
.zero 1
.endobj sHciEventMismatchPad

.obj sHciCommandTimeout, global
.string "Cmd timeout; no cmd in queue"
.endobj sHciCommandTimeout

.obj sHciCommandTimeoutPad, global
.zero 3
.endobj sHciCommandTimeoutPad

.obj sHciCommandTimeoutOpcode, global
.string "BTU HCI command timeout - cmd opcode = 0x%02x"
.endobj sHciCommandTimeoutOpcode

.obj sHciCommandTimeoutOpcodePad, global
.zero 2
.endobj sHciCommandTimeoutOpcodePad
