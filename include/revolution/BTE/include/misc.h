#ifndef BTE_MISC_H
#define BTE_MISC_H

// Declarations that haven't been given a proper home yet

#include "data_types.h"
#include "hci.h"

#ifdef __cplusplus
	extern "C" {
#endif

// bte_hcisu.c

void bte_hcisu_send(HC_BT_HDR *p_msg, UINT16 event);
void bte_hcisu_task(int);
void bte_hcisu_close(void);
void bta_ci_hci_msg_handler(void *p_data);

// hcisu_h2.c

void bta_usb_close_evt(INT8);

#ifdef __cplusplus
	}
#endif

#endif // BTE_MISC_H
