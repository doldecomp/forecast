#ifndef BTE_UUSB_H
#define BTE_UUSB_H

#include <decomp.h>

#include "data_types.h"

#ifdef __cplusplus
	extern "C" {
#endif

typedef enum
{
	UUSB_OP_NONE,
} tUUSB_IOCTL_OP;

typedef void tUUSB_CBACK(UINT8, INT8);

typedef struct
{
	char	reserved0[4];
	UINT16	vid;
	UINT16	pid;
	UINT16	task_id;
	char	reserved1[1];
	char	mbox;
	char	reserved2[1]; // for stack
} tUUSB;

extern UINT32 volatile wait4hci;

/* NOTICE OF CHANGES: uusb_set_trace_state is internal to uusb_ppc.c in this
 * build (only reached from UUSB_Register, where the compiler inlines it). */
UINT8 uusb_get_trace_state(void);

void UUSB_Register(tUUSB *uusb);
void UUSB_Open(tUUSB *uusb, tUUSB_CBACK *cback);
UINT16 UUSB_Read(UINT8, void *, UINT16);
void UUSB_ReadBuf(/* ... */);
int UUSB_WriteBuf(/* ... */);
UINT16 UUSB_Write(UINT8, void *p_data, UINT16 len, void *);
void UUSB_Ioctl(tUUSB_IOCTL_OP op, void *p_data);
void UUSB_Close(void);
void UUSB_Unregister(void);
int UUSB_Feature(void);

#ifdef __cplusplus
	}
#endif

#endif // BTE_UUSB_H
