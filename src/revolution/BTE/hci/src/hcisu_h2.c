#include "hci.h"

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * hci/src/hci_h4.c
 */

/******************************************************************************
 *
 *  Copyright (C) 2009-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/* Includes changes by muff1n1634 and for this repository (KoopTheKoopa) */

#include <decomp.h>

#include "bt_target.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "data_types.h"
#include "gki_target.h"

#include "btu.h"
#include "gki.h"
#include "l2c_int.h"
#include "misc.h"
#include "uusb.h"

#define HCI_CMD_PREAMBLE_SIZE	3
#define HCI_ACL_PREAMBLE_SIZE	4
#define HCI_SCO_PREAMBLE_SIZE	3
#define HCI_EVT_PREAMBLE_SIZE	2

#define H4_TYPE_COMMAND			1
#define H4_TYPE_ACL_DATA		2
#define H4_TYPE_SCO_DATA		3
#define H4_TYPE_EVENT			4

#undef STREAM_TO_UINT16

// TODO: is there a fix that works for everything?
// why another cast?
#define STREAM_TO_UINT16(p, u16)								\
	do															\
	{															\
		*(u16) = 0												\
		+ /*    */((UINT16)(*((p) + 0)) << 0)					\
		+ (UINT16)((UINT16)(*((p) + 1)) << 8);					\
																\
		(p) += 2;												\
	} while (FALSE)

typedef struct
{
	HC_BT_HDR	*p_rcv_msg[3];	// 0x00
	UINT16		rcv_len[3];		// 0x0c
	UINT16		buf_size;		// 0x12
	UINT8		rcv_msg_type[3];// 0x14
	UINT8		prev_byte[3];	// 0x17
	UINT8		rcv_state[3];	// 0x1a
	char		reserved0[1];	// 0x1d
	UINT8		state;			// 0x1e
	UINT8		task_id;		// 0x1f
	UINT8		btif_task;		// 0x20
	char		reserved1[2];	// 0x21
	/* 1 byte padding */
} tHCISU_H2_CB; // size 0x24

// forgotten to be given internal linkage? these aren't used anywhere else
extern void hcisu_h2_usb_cback(UINT8, INT8);
extern UINT16 hcisu_h2_receive_msg(UINT16, tHCISU_H2_CB *p_cb);
extern void hcisu_h2_send_msg_now(tHCISU_H2_CB *cb, HC_BT_HDR *p_msg);
// NOTICE OF CHANGES: given internal linkage so the compiler inlines the single
// call in hcisu_h2_send and drops the out-of-line copy (absent from the retail
// build).
static void hcisu_h2_send_msg(tHCISU_H2_CB *p_cb, HC_BT_HDR *p_msg);

const tHCI_IF hcisu_h2 =
{
	&hcisu_h2_init,
	&hcisu_h2_open,
	&hcisu_h2_close,
	&hcisu_h2_send,
	&hcisu_h2_handle_event
};

static tHCISU_H2_CB hcisu_h2_cb;

static UINT8 const hcisu_preamble_table[] =
{
	HCI_CMD_PREAMBLE_SIZE,
	HCI_ACL_PREAMBLE_SIZE,
	HCI_SCO_PREAMBLE_SIZE,
	HCI_EVT_PREAMBLE_SIZE,
};

static UINT16 const hcisu_msg_evt_table[] =
{
	MSG_HC_TO_STACK_HCI_ERR,
	MSG_HC_TO_STACK_HCI_ACL,
	MSG_HC_TO_STACK_HCI_SCO,
	MSG_HC_TO_STACK_HCI_EVT,
};

void hcisu_h2_usb_cback(UINT8 param_1, INT8 param_2)
{
	switch (param_1)
	{
	case 0:
		break;

	case 4:
		bta_usb_close_evt(param_2);
		break;
	}
}

UINT16 hcisu_h2_receive_msg(UINT16 param_1, tHCISU_H2_CB *p_cb)
{
	UINT16 bytes_read = 0;
	UINT8 byte;
	UINT16 msg_len;
	UINT16 len;
	UINT8 msg_received;
	UINT16 type;

	if (p_cb->state != 2)
		return 0;

	type = param_1 + 1;

	while (TRUE)
	{
		if (UUSB_Read(type, &byte, 1) == 0)
			break;

		++bytes_read;
		msg_received = FALSE;

		switch (p_cb->rcv_state[param_1])
		{
		case 0:
			if (type == 1)
			{
				p_cb->p_rcv_msg[param_1] = GKI_getpoolbuf(GKI_POOL_ID_2);
				p_cb->rcv_msg_type[param_1] = 4;
			}
			else if (type == 2)
			{
				p_cb->p_rcv_msg[param_1] = GKI_getpoolbuf(GKI_POOL_ID_3);
				p_cb->rcv_msg_type[param_1] = 2;
			}
			else if (type == 3)
			{
				p_cb->p_rcv_msg[param_1] = GKI_getpoolbuf(GKI_POOL_ID_1);
				p_cb->rcv_msg_type[param_1] = 3;
			}
			else
			{
				break;
			}

			if (p_cb->p_rcv_msg[param_1])
			{
				p_cb->p_rcv_msg[param_1]->len = 0;
				p_cb->p_rcv_msg[param_1]->event =
					hcisu_msg_evt_table[p_cb->rcv_msg_type[param_1] - 1];
				p_cb->p_rcv_msg[param_1]->offset = 0;
				p_cb->rcv_state[param_1] = 1;
			}
			else
			{
				BT_TRACE(HCI, ERROR,
				         "HCIS: Unable to allocate buffer for incoming HCI "
				         "message.");

				p_cb->rcv_state[param_1] = 2;
			}

			p_cb->rcv_len[param_1] =
				hcisu_preamble_table[p_cb->rcv_msg_type[param_1] - 1];

			/* fallthrough */

		case 1:
			*((UINT8 *)(p_cb->p_rcv_msg[param_1] + 1)
			  + p_cb->p_rcv_msg[param_1]->len++) = byte;

			--p_cb->rcv_len[param_1];

			if (p_cb->rcv_len[param_1] == 0)
			{
				msg_len = byte;

				if (p_cb->rcv_msg_type[param_1] == 2)
				{
					msg_len = (msg_len << 8) + p_cb->prev_byte[param_1];

					if ((p_cb->p_rcv_msg[param_1] =
					         (HC_BT_HDR *)l2cap_link_chk_pkt_start(
								 (BT_HDR *)p_cb->p_rcv_msg[param_1]))
					    == 0)
					{
						p_cb->rcv_len[param_1] = msg_len;

						if (msg_len == 0)
							p_cb->rcv_state[param_1] = 0;
						else
							p_cb->rcv_state[param_1] = 4;

						break;
					}
				}

				p_cb->rcv_len[param_1] = msg_len;

				if (sizeof(HC_BT_HDR)
				        + hcisu_preamble_table[p_cb->rcv_msg_type[param_1] - 1]
				        + msg_len
				    > GKI_get_buf_size(p_cb->p_rcv_msg[param_1]))
				{
					GKI_freebuf(p_cb->p_rcv_msg[param_1]);
					p_cb->p_rcv_msg[param_1] = NULL;
					p_cb->rcv_state[param_1] = 4;

					BT_TRACE(HCI, ERROR,
					         "HCIS: Invalid length for incoming HCI message.");

					break;
				}

				if (msg_len != 0)
				{
					p_cb->rcv_state[param_1] = 3;
				}
				else
				{
					msg_received = TRUE;
					p_cb->rcv_state[param_1] = 0;
				}
			}
			else
			{
				p_cb->prev_byte[param_1] = byte;
			}

			break;

		case 2:
			--p_cb->rcv_len[param_1];

			if (p_cb->rcv_len[param_1] == 0)
			{
				msg_len = byte;

				if (p_cb->rcv_msg_type[param_1] == 2)
					msg_len = (msg_len << 8) + p_cb->prev_byte[param_1];

				p_cb->rcv_len[param_1] = msg_len;
				p_cb->rcv_state[param_1] = 4;
			}
			else
			{
				p_cb->prev_byte[param_1] = byte;
			}

			break;

		case 3:
			*((UINT8 *)(p_cb->p_rcv_msg[param_1] + 1)
			  + p_cb->p_rcv_msg[param_1]->len++) = byte;

			--p_cb->rcv_len[param_1];

			len = UUSB_Read(param_1 + 1,
			                (UINT8 *)(p_cb->p_rcv_msg[param_1] + 1)
			                    + p_cb->p_rcv_msg[param_1]->len,
			                p_cb->rcv_len[param_1]);

			p_cb->p_rcv_msg[param_1]->len += len;
			p_cb->rcv_len[param_1] -= len;
			bytes_read += len;

			if (p_cb->rcv_len[param_1] == 0)
			{
				if (p_cb->rcv_msg_type[param_1] == 2
				    && l2cap_link_chk_pkt_end() == 0)
				{
					p_cb->rcv_state[param_1] = 0;
				}
				else
				{
					msg_received = TRUE;
					p_cb->rcv_state[param_1] = 0;
				}
			}

			break;

		case 4:
			--p_cb->rcv_len[param_1];

			if (p_cb->rcv_len[param_1] == 0)
				p_cb->rcv_state[param_1] = 0;

			break;
		}

		if (msg_received)
		{
			GKI_send_msg(hcisu_h2_cb.task_id, 0, p_cb->p_rcv_msg[param_1]);
			p_cb->p_rcv_msg[param_1] = NULL;
		}
	}

	return bytes_read;
}

void hcisu_h2_send_msg_now(tHCISU_H2_CB *cb, HC_BT_HDR *p_msg)
{
	UINT8 type = 0;
	UINT16 handle;
	UINT16 bytes_to_send;
	UINT8 *p = (UINT8 *)(p_msg + 1) + p_msg->offset;
	UINT16 bytes_sent;
	UINT8 num;

	if (p_msg->event == MSG_STACK_TO_HC_HCI_ACL)
	{
		type = H4_TYPE_ACL_DATA;
		num = 2;
	}
	else if (p_msg->event == MSG_STACK_TO_HC_HCI_SCO)
	{
		type = H4_TYPE_SCO_DATA;
		num = 3;
	}
	else if (p_msg->event == MSG_STACK_TO_HC_HCI_CMD)
	{
		type = H4_TYPE_COMMAND;
		num = 0;
	}

	if (p_msg->event == MSG_STACK_TO_HC_HCI_ACL
	    && p_msg->len > btu_cb.hcit_acl_pkt_size)
	{
		STREAM_TO_UINT16(p, &handle);

		handle = (handle & 0xcfff) | 0x1000;

		while (p_msg->len > btu_cb.hcit_acl_pkt_size)
		{
			bytes_to_send = btu_cb.hcit_acl_pkt_size;
			bytes_sent = UUSB_Write(num, p, bytes_to_send, p_msg);

			p_msg->offset += btu_cb.hcit_acl_data_size;
			p_msg->len -= btu_cb.hcit_acl_data_size;

			p = (UINT8 *)(p_msg + 1) + p_msg->offset;

			UINT16_TO_STREAM(p, handle);

			// NOTICE OF CHANGES: braced, because this repo's UINT16_TO_STREAM
			// is a compound statement rather than a do/while.
			if (p_msg->len > btu_cb.hcit_acl_pkt_size)
			{
				UINT16_TO_STREAM(p, btu_cb.hcit_acl_data_size);
			}
			else
			{
				UINT16_TO_STREAM(p, p_msg->len - HCI_ACL_PREAMBLE_SIZE);
			}

			if (p_msg->layer_specific)
			{
				if (--p_msg->layer_specific == 0)
				{
					p_msg->event = MSG_HC_TO_STACK_L2C_SEG_XMIT;

					GKI_send_msg(hcisu_h2_cb.task_id, 0, p_msg);

					return;
				}
			}
		}
	}

	bytes_to_send = p_msg->len;
	bytes_sent = UUSB_Write(num, p, bytes_to_send, p_msg);

	GKI_freebuf(p_msg);

	(void)cb;
}

static void hcisu_h2_send_msg(tHCISU_H2_CB *p_cb, HC_BT_HDR *p_msg)
{
	hcisu_h2_send_msg_now(p_cb, p_msg);
}

void hcisu_h2_init(UINT8 param_1, UINT8 param_2, UINT16 param_3)
{
	hcisu_h2_cb.state = 0;

	hcisu_h2_cb.rcv_state[0] = 0;
	hcisu_h2_cb.rcv_state[1] = 0;
	hcisu_h2_cb.rcv_state[2] = 0;

	hcisu_h2_cb.task_id = param_1;
	hcisu_h2_cb.btif_task = param_2;
	hcisu_h2_cb.buf_size = param_3;
}

BOOLEAN hcisu_h2_open(tHCI_CFG *_p_cfg)
{
	tHCI_CFG *p_cfg = _p_cfg;
	tUUSB uusb;

	uusb.mbox = 0;
	uusb.vid = p_cfg->vid;
	uusb.pid = p_cfg->pid;
	uusb.task_id = hcisu_h2_cb.task_id;

	hcisu_h2_cb.state = 2;

	UUSB_Register(&uusb);
	UUSB_Open(&uusb, &hcisu_h2_usb_cback);

	return TRUE;
}

void hcisu_h2_close(void)
{
	hcisu_h2_cb.state = 0;

	UUSB_Close();
	UUSB_Unregister();
}

BOOLEAN hcisu_h2_send(HC_BT_HDR *p_msg)
{
	hcisu_h2_send_msg(&hcisu_h2_cb, p_msg);

	return TRUE;
}

int hcisu_h2_handle_event(UINT16 len)
{
	UINT16 bytes_read;

	bytes_read = hcisu_h2_receive_msg(
		len - sizeof(HC_BT_HDR) - hcisu_h2_cb.buf_size, &hcisu_h2_cb);

	return 0;
}
