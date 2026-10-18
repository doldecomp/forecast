#include "bte.h"

#include <decomp.h> // int

#include "data_types.h" // UINT16
#include "gki_target.h" // BTU_TASK

#include "btu.h" // BTU_HCI_RCV_MBOX
#include "gki.h"
#include "hci.h"

// NOTICE OF CHANGES: MWCC emits uninitialized .sbss objects in reverse
// declaration order, so p_hcisu_cfg is declared before p_hcisu_if to
// reproduce the retail .sbss layout (uusb_ppc.c's variables come first).
tHCI_CFG *p_hcisu_cfg;
tHCI_IF *p_hcisu_if;

void bte_hcisu_send(HC_BT_HDR *p_msg, UINT16 event)
{
	p_msg->event = event;

	if (p_hcisu_if)
		(*p_hcisu_if->send)(p_msg);
	else
		GKI_freebuf(p_msg);
}

void bte_hcisu_task(int param_1)
{
	if (p_hcisu_if)
	{
		if (p_hcisu_if->init)
			(*p_hcisu_if->init)(2, 1, 0x800);

		if (p_hcisu_if->open)
			(*p_hcisu_if->open)(p_hcisu_cfg);
	}
}

void bte_hcisu_close(void)
{
	if (p_hcisu_if)
	{
		if (p_hcisu_if->close)
			(*p_hcisu_if->close)();
	}
}

void bta_ci_hci_msg_handler(void *p_data)
{
	HC_BT_HDR *p_msg = p_data;

	GKI_send_msg(BTU_TASK, BTU_HCI_RCV_MBOX, p_msg);
}
