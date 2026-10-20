/******************************************************************************
 *
 *  Copyright (C) 1999-2012 Broadcom Corporation
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

/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/hcic/hcicmds.c
 */

/* Includes changes by muff1n1634 and for this repository (KoopTheKoopa) */

/******************************************************************************
 *
 *  This file contains the HCI command building functions.
 *
 ******************************************************************************/

#include "hcimsgs.h"

#include <cstring>

#include <decomp.h> // ARRAY_LENGTH

#include "bt_target.h"
#include "bt_types.h"
#include "data_types.h"

#include "hcidefs.h"
#include "btu.h"

BOOLEAN btsnd_hcic_inquiry(LAP const inq_lap, UINT8 duration,
                           UINT8 response_cnt)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_INQUIRY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_INQUIRY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_INQUIRY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_INQUIRY);

    LAP_TO_STREAM(pp, inq_lap);
    UINT8_TO_STREAM(pp, duration);
    UINT8_TO_STREAM(pp, response_cnt);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_inq_cancel(void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_INQ_CANCEL)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_INQ_CANCEL;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_INQUIRY_CANCEL);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_INQ_CANCEL);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_per_inq_mode(UINT16 max_period, UINT16 min_period,
                                LAP const inq_lap, UINT8 duration,
                                UINT8 response_cnt)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_PER_INQ_MODE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_PER_INQ_MODE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_PERIODIC_INQUIRY_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_PER_INQ_MODE);

    UINT16_TO_STREAM(pp, max_period);
    UINT16_TO_STREAM(pp, min_period);
    LAP_TO_STREAM(pp, inq_lap);
    UINT8_TO_STREAM(pp, duration);
    UINT8_TO_STREAM(pp, response_cnt);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_create_conn(BD_ADDR dest, UINT16 packet_types,
                               UINT8 page_scan_rep_mode, UINT8 page_scan_mode,
                               UINT16 clock_offset, UINT8 allow_switch)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CREATE_CONN)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CREATE_CONN;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_CREATE_CONNECTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CREATE_CONN);

    BDADDR_TO_STREAM(pp, dest);
    UINT16_TO_STREAM(pp, packet_types);
    UINT8_TO_STREAM(pp, page_scan_rep_mode);
    UINT8_TO_STREAM(pp, page_scan_mode);
    UINT16_TO_STREAM(pp, clock_offset);
    UINT8_TO_STREAM(pp, allow_switch);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_disconnect(UINT16 handle, UINT8 reason)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_DISCONNECT)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_DISCONNECT;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_DISCONNECT);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_DISCONNECT);

    UINT16_TO_STREAM(pp, handle);
    UINT8_TO_STREAM(pp, reason);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_add_SCO_conn(UINT16 handle, UINT16 packet_types)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_ADD_SCO_CONN)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_ADD_SCO_CONN;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_ADD_SCO_CONNECTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_ADD_SCO_CONN);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, packet_types);

    btu_hcif_send_cmd(p);
    return TRUE;
}


void btsnd_hcic_accept_conn(void *buffer, BD_ADDR dest, UINT8 role)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_ACCEPT_CONN;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_ACCEPT_CONNECTION_REQUEST);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_ACCEPT_CONN);

    BDADDR_TO_STREAM(pp, dest);
    UINT8_TO_STREAM(pp, role);

    btu_hcif_send_cmd(p);
}

void btsnd_hcic_reject_conn(void *buffer, BD_ADDR dest, UINT8 reason)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_REJECT_CONN;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_REJECT_CONNECTION_REQUEST);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_REJECT_CONN);

    BDADDR_TO_STREAM(pp, dest);
    UINT8_TO_STREAM(pp, reason);

    btu_hcif_send_cmd(p);
}

BOOLEAN btsnd_hcic_link_key_req_reply(BD_ADDR bd_addr, LINK_KEY link_key)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_LINK_KEY_REQ_REPLY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_LINK_KEY_REQ_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_LINK_KEY_REQUEST_REPLY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_LINK_KEY_REQ_REPLY);

    BDADDR_TO_STREAM(pp, bd_addr);
    ARRAY16_TO_STREAM(pp, link_key);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_link_key_neg_reply(BD_ADDR bd_addr)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_LINK_KEY_NEG_REPLY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_LINK_KEY_NEG_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_LINK_KEY_REQUEST_NEG_REPLY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_LINK_KEY_NEG_REPLY);

    BDADDR_TO_STREAM(pp, bd_addr);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_pin_code_req_reply(BD_ADDR bd_addr, UINT8 pin_code_len,
                                      PIN_CODE pin_code)
{
    BT_HDR *p;
    UINT8 *pp;
    int i;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_PIN_CODE_REQ_REPLY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_PIN_CODE_REQ_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_PIN_CODE_REQUEST_REPLY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_PIN_CODE_REQ_REPLY);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, pin_code_len);

    for (i = 0; i < pin_code_len; i++)
        *pp++ = *pin_code++;

    for (; i < PIN_CODE_LEN; i++)
        *pp++ = 0;

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_pin_code_neg_reply(BD_ADDR bd_addr)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_PIN_CODE_NEG_REPLY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_PIN_CODE_NEG_REPLY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_PIN_CODE_REQUEST_NEG_REPLY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_PIN_CODE_NEG_REPLY);

    BDADDR_TO_STREAM(pp, bd_addr);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_change_conn_type(UINT16 handle, UINT16 packet_types)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CHANGE_CONN_TYPE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CHANGE_CONN_TYPE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_CHANGE_CONN_PACKET_TYPE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CHANGE_CONN_TYPE);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, packet_types);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_auth_request(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_AUTHENTICATION_REQUESTED);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_set_conn_encrypt(UINT16 handle, BOOLEAN enable)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_SET_CONN_ENCRYPT)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_CONN_ENCRYPT;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_SET_CONN_ENCRYPTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_SET_CONN_ENCRYPT);

    UINT16_TO_STREAM(pp, handle);
    UINT8_TO_STREAM(pp, enable);

    btu_hcif_send_cmd(p);
    return TRUE;
}



BOOLEAN btsnd_hcic_rmt_name_req(BD_ADDR bd_addr, UINT8 page_scan_rep_mode,
                                UINT8 page_scan_mode, UINT16 clock_offset)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_RMT_NAME_REQ)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_RMT_NAME_REQ;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_RMT_NAME_REQUEST);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_RMT_NAME_REQ);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, page_scan_rep_mode);
    UINT8_TO_STREAM(pp, page_scan_mode);
    UINT16_TO_STREAM(pp, clock_offset);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_rmt_name_req_cancel(BD_ADDR bd_addr)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_RMT_NAME_REQ_CANCEL)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_RMT_NAME_REQ_CANCEL;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_RMT_NAME_REQUEST_CANCEL);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_RMT_NAME_REQ_CANCEL);

    BDADDR_TO_STREAM(pp, bd_addr);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_rmt_features_req(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_RMT_FEATURES);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_rmt_ver_req(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_RMT_VERSION_INFO);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_read_rmt_clk_offset(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_RMT_CLOCK_OFFSET);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_setup_esco_conn(UINT16 handle, UINT32 tx_bw, UINT32 rx_bw,
                                   UINT16 max_latency, UINT16 voice,
                                   UINT8 retrans_effort, UINT16 packet_types)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_SETUP_ESCO)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SETUP_ESCO;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_SETUP_ESCO_CONNECTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_SETUP_ESCO);

    UINT16_TO_STREAM(pp, handle);
    UINT32_TO_STREAM(pp, tx_bw);
    UINT32_TO_STREAM(pp, rx_bw);
    UINT16_TO_STREAM(pp, max_latency);
    UINT16_TO_STREAM(pp, voice);
    UINT8_TO_STREAM(pp, retrans_effort);
    UINT16_TO_STREAM(pp, packet_types);

    btu_hcif_send_cmd(p);
    return TRUE;
}

void btsnd_hcic_accept_esco_conn(void *buffer, BD_ADDR bd_addr, UINT32 tx_bw,
                                 UINT32 rx_bw, UINT16 max_latency,
                                 UINT16 content_fmt, UINT8 retrans_effort,
                                 UINT16 packet_types)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_ACCEPT_ESCO;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_ACCEPT_ESCO_CONNECTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_ACCEPT_ESCO);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT32_TO_STREAM(pp, tx_bw);
    UINT32_TO_STREAM(pp, rx_bw);
    UINT16_TO_STREAM(pp, max_latency);
    UINT16_TO_STREAM(pp, content_fmt);
    UINT8_TO_STREAM(pp, retrans_effort);
    UINT16_TO_STREAM(pp, packet_types);

    btu_hcif_send_cmd(p);
}

void btsnd_hcic_reject_esco_conn(void *buffer, BD_ADDR bd_addr, UINT8 reason)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_REJECT_ESCO;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_REJECT_ESCO_CONNECTION);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_REJECT_ESCO);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, reason);

    btu_hcif_send_cmd(p);
}

BOOLEAN btsnd_hcic_hold_mode(void *buffer, UINT16 handle,
                             UINT16 max_hold_period, UINT16 min_hold_period)
{
    BT_HDR *p;
    UINT8 *pp;

    // ???
    if (!buffer
        && (buffer = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_HOLD_MODE)) == NULL)
    {
        return FALSE;
    }

    p = buffer;
    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_HOLD_MODE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_HOLD_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_HOLD_MODE);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, max_hold_period);
    UINT16_TO_STREAM(pp, min_hold_period);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_sniff_mode(void *buffer, UINT16 handle,
                              UINT16 max_sniff_period, UINT16 min_sniff_period,
                              UINT16 sniff_attempt, UINT16 sniff_timeout)
{
    BT_HDR *p;
    UINT8 *pp;

    if (!buffer
        && (buffer = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_HOLD_MODE)) == NULL)
    {
        return FALSE;
    }

    p = buffer;
    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SNIFF_MODE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_SNIFF_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_SNIFF_MODE);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, max_sniff_period);
    UINT16_TO_STREAM(pp, min_sniff_period);
    UINT16_TO_STREAM(pp, sniff_attempt);
    UINT16_TO_STREAM(pp, sniff_timeout);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_exit_sniff_mode(void *buffer, UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if (!buffer
        && (buffer = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_HOLD_MODE)) == NULL)
    {
        return FALSE;
    }

    p = buffer;
    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_EXIT_SNIFF_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_park_mode(void *buffer, UINT16 handle,
                             UINT16 beacon_max_interval,
                             UINT16 beacon_min_interval)
{
    BT_HDR *p;
    UINT8 *pp;

    if (!buffer
        && (buffer = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_HOLD_MODE)) == NULL)
    {
        return FALSE;
    }

    p = buffer;
    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_PARK_MODE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_PARK_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_PARK_MODE);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, beacon_max_interval);
    UINT16_TO_STREAM(pp, beacon_min_interval);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_exit_park_mode(void *buffer, UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if (!buffer
        && (buffer = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_HOLD_MODE)) == NULL)
    {
        return FALSE;
    }

    p = buffer;
    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_EXIT_PARK_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}



BOOLEAN btsnd_hcic_switch_role(BD_ADDR bd_addr, UINT8 role)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_SWITCH_ROLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SWITCH_ROLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_SWITCH_ROLE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_SWITCH_ROLE);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, role);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_write_policy_set(UINT16 handle, UINT16 settings)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_WRITE_POLICY_SET)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_POLICY_SET;
    p->offset = 0;
    UINT16_TO_STREAM(pp, HCI_WRITE_POLICY_SETTINGS);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_POLICY_SET);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, settings);

    btu_hcif_send_cmd(p);
    return TRUE;
}






BOOLEAN btsnd_hcic_reset(void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_RESET)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_RESET;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_RESET);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_RESET);

    btu_hcif_send_cmd(p);
    return TRUE;
}

void btsnd_hcic_set_event_filter(void *buffer, UINT8 filt_type,
                                 UINT8 filt_cond_type, UINT8 *filt_cond,
                                 UINT8 filt_cond_len)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_SET_EVENT_FILTER);

    if (filt_type)
    {
        p->len = (UINT16)(HCIC_PREAMBLE_SIZE + 2 + filt_cond_len);
        UINT8_TO_STREAM(pp, (UINT8)(2 + filt_cond_len));

        UINT8_TO_STREAM(pp, filt_type);
        UINT8_TO_STREAM(pp, filt_cond_type);

        if (filt_cond_type == HCI_FILTER_COND_DEVICE_CLASS)
        {
            DEVCLASS_TO_STREAM(pp, filt_cond);
            filt_cond += DEV_CLASS_LEN;
            DEVCLASS_TO_STREAM(pp, filt_cond);
            filt_cond += DEV_CLASS_LEN;

            filt_cond_len -= (2 * DEV_CLASS_LEN);
        }
        else if (filt_cond_type == HCI_FILTER_COND_BD_ADDR)
        {
            BDADDR_TO_STREAM(pp, filt_cond);
            filt_cond += BD_ADDR_LEN;

            filt_cond_len -= BD_ADDR_LEN;
        }

        if (filt_cond_len)
            ARRAY_TO_STREAM(pp, filt_cond, filt_cond_len);
    }
    else
    {
        p->len = (UINT16)(HCIC_PREAMBLE_SIZE + 1);
        UINT8_TO_STREAM(pp, 1);

        UINT8_TO_STREAM(pp, filt_type);
    }

    btu_hcif_send_cmd(p);
}



BOOLEAN btsnd_hcic_write_pin_type(UINT8 type)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_WRITE_PARAM1)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_PIN_TYPE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, type);

    btu_hcif_send_cmd(p);
    return TRUE;
}


void btsnd_hcic_read_stored_key(void *buffer, BD_ADDR bd_addr,
                                BOOLEAN read_all_flag)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_STORED_KEY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_STORED_LINK_KEY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_READ_STORED_KEY);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, read_all_flag);

    btu_hcif_send_cmd(p);
}

void btsnd_hcic_write_stored_key(void *buffer, UINT8 num_keys, BD_ADDR *bd_addr,
                                 LINK_KEY *link_key)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);
    int j;

    p->len = HCIC_PREAMBLE_SIZE + 1 + (num_keys * (BD_ADDR_LEN + LINK_KEY_LEN));
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_STORED_LINK_KEY);
    UINT8_TO_STREAM(pp, p->len - HCIC_PREAMBLE_SIZE);

    if (num_keys > HCI_MAX_NUM_OF_LINK_KEYS_PER_CMD)
        num_keys = HCI_MAX_NUM_OF_LINK_KEYS_PER_CMD;

    UINT8_TO_STREAM(pp, num_keys);

    for (j = 0; j < num_keys; j++)
    {
        BDADDR_TO_STREAM(pp, bd_addr[j]);
        ARRAY16_TO_STREAM(pp, link_key[j]);
    }

    btu_hcif_send_cmd(p);
}

BOOLEAN btsnd_hcic_delete_stored_key(BD_ADDR bd_addr, BOOLEAN delete_all_flag)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_DELETE_STORED_KEY)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_DELETE_STORED_KEY;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_DELETE_STORED_LINK_KEY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_DELETE_STORED_KEY);

    BDADDR_TO_STREAM(pp, bd_addr);
    UINT8_TO_STREAM(pp, delete_all_flag);

    btu_hcif_send_cmd(p);
    return TRUE;
}

void btsnd_hcic_change_name(void *buffer, BD_NAME name)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);
    UINT16 len = strlen((char *)name) + 1;

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CHANGE_NAME;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_CHANGE_LOCAL_NAME);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CHANGE_NAME);

    ARRAY_TO_STREAM(pp, name, len);

    btu_hcif_send_cmd(p);
}





void btsnd_hcic_write_page_tout(void *buffer, UINT16 timeout)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM2;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_PAGE_TOUT);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM2);

    UINT16_TO_STREAM(pp, timeout);

    btu_hcif_send_cmd(p);
}


void btsnd_hcic_write_scan_enable(void *buffer, UINT8 flag)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_SCAN_ENABLE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, flag);

    btu_hcif_send_cmd(p);
}


void btsnd_hcic_write_pagescan_cfg(void *buffer, UINT16 interval, UINT16 window)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PAGESCAN_CFG;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_PAGESCAN_CFG);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PAGESCAN_CFG);

    UINT16_TO_STREAM(pp, interval);
    UINT16_TO_STREAM(pp, window);

    btu_hcif_send_cmd(p);
}


void btsnd_hcic_write_inqscan_cfg(void *buffer, UINT16 interval, UINT16 window)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_INQSCAN_CFG;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_INQUIRYSCAN_CFG);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_INQSCAN_CFG);

    UINT16_TO_STREAM(pp, interval);
    UINT16_TO_STREAM(pp, window);

    btu_hcif_send_cmd(p);
}


BOOLEAN btsnd_hcic_write_auth_enable(UINT8 flag)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_WRITE_PARAM1)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_AUTHENTICATION_ENABLE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, flag);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_write_encr_mode(UINT8 mode)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_WRITE_PARAM1)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_ENCRYPTION_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, mode);

    btu_hcif_send_cmd(p);
    return TRUE;
}


void btsnd_hcic_write_dev_class(void *buffer, DEV_CLASS dev_class)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM3;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_CLASS_OF_DEVICE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM3);

    DEVCLASS_TO_STREAM(pp, dev_class);

    btu_hcif_send_cmd(p);
}




void btsnd_hcic_write_auto_flush_tout(void *buffer, UINT16 handle, UINT16 tout)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_AUTO_FLUSH_TOUT;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_AUTO_FLUSH_TOUT);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_AUTO_FLUSH_TOUT);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, tout);

    btu_hcif_send_cmd(p);
}









BOOLEAN btsnd_hcic_set_host_buf_size(UINT16 acl_len, UINT8 sco_len,
                                     UINT16 acl_num, UINT16 sco_num)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_SET_HOST_BUF_SIZE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_HOST_BUF_SIZE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_HOST_BUFFER_SIZE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_SET_HOST_BUF_SIZE);

    UINT16_TO_STREAM(pp, acl_len);
    UINT8_TO_STREAM(pp, sco_len);
    UINT16_TO_STREAM(pp, acl_num);
    UINT16_TO_STREAM(pp, sco_num);

    btu_hcif_send_cmd(p);
    return TRUE;
}



BOOLEAN btsnd_hcic_write_link_super_tout(UINT16 handle, UINT16 timeout)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_WRITE_LINK_SUPER_TOUT)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_LINK_SUPER_TOUT;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_LINK_SUPER_TOUT);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_LINK_SUPER_TOUT);

    UINT16_TO_STREAM(pp, handle);
    UINT16_TO_STREAM(pp, timeout);

    btu_hcif_send_cmd(p);
    return TRUE;
}



void btsnd_hcic_write_cur_iac_lap(void *buffer, UINT8 num_cur_iac,
                                  LAP * const iac_lap)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);
    int i;

    p->len = HCIC_PREAMBLE_SIZE + 1 + (LAP_LEN * num_cur_iac);
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_CURRENT_IAC_LAP);
    UINT8_TO_STREAM(pp, p->len - HCIC_PREAMBLE_SIZE);

    UINT8_TO_STREAM(pp, num_cur_iac);

    for (i = 0; i < num_cur_iac; i++)
        LAP_TO_STREAM(pp, iac_lap[i]);

    btu_hcif_send_cmd(p);
}







BOOLEAN btsnd_hcic_read_local_ver(void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_READ_CMD)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_LOCAL_VERSION_INFO);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd(p);
    return TRUE;
}


BOOLEAN btsnd_hcic_read_local_features(void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_READ_CMD)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_LOCAL_FEATURES);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd(p);
    return TRUE;
}


void btsnd_hcic_read_buffer_size(void *buffer)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_BUFFER_SIZE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd(p);
}


BOOLEAN btsnd_hcic_read_bd_addr(void)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_READ_CMD)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_READ_CMD;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_BD_ADDR);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_READ_CMD);

    btu_hcif_send_cmd(p);
    return TRUE;
}



BOOLEAN btsnd_hcic_get_link_quality(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_GET_LINK_QUALITY);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}

BOOLEAN btsnd_hcic_read_rssi(UINT16 handle)
{
    BT_HDR *p;
    UINT8 *pp;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_CMD_HANDLE)) == NULL)
        return FALSE;

    pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_CMD_HANDLE;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_READ_RSSI);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_CMD_HANDLE);

    UINT16_TO_STREAM(pp, handle);

    btu_hcif_send_cmd(p);
    return TRUE;
}






BOOLEAN btsnd_hcic_set_afh_channels(UINT8 first, UINT8 last)
{
    BT_HDR *p;
    UINT8  *pp;
    UINT8  channels[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F};
    int    i;

    if ((p = HCI_GET_CMD_BUF(HCIC_PARAM_SIZE_SET_AFH_CHANNELS)) == NULL)
        return (FALSE);

    pp = (UINT8 *)(p + 1);

    p->len    = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_SET_AFH_CHANNELS;
    p->offset = 0;

    UINT16_TO_STREAM (pp, HCI_SET_AFH_CHANNELS);
    UINT8_TO_STREAM  (pp, HCIC_PARAM_SIZE_SET_AFH_CHANNELS);

    /* Just make sure that caller did not exceed 79 Bluetooth channels */
    if ((first <= last) && (last <= 78))
    {
        for (i = first; i <= last; i++)
        {
            int byte_offset = i / 8;
            int bit_offset  = i % 8;
            channels[byte_offset] &= ~(1 << bit_offset);
        }
    }
    for (i = 0; i < 10; i++)
        *pp++ = channels[i];

    btu_hcif_send_cmd (p);
    return (TRUE);
}





void btsnd_hcic_write_inqscan_type(void *buffer, UINT8 type)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_INQSCAN_TYPE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, type);

    btu_hcif_send_cmd(p);
}


void btsnd_hcic_write_inquiry_mode(void *buffer, UINT8 mode)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_INQUIRY_MODE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, mode);

    btu_hcif_send_cmd(p);
}


void btsnd_hcic_write_pagescan_type(void *buffer, UINT8 type)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + HCIC_PARAM_SIZE_WRITE_PARAM1;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_WRITE_PAGESCAN_TYPE);
    UINT8_TO_STREAM(pp, HCIC_PARAM_SIZE_WRITE_PARAM1);

    UINT8_TO_STREAM(pp, type);

    btu_hcif_send_cmd(p);
}

void btsnd_hcic_vendor_spec_cmd(void *buffer, UINT16 opcode, UINT8 len,
                                UINT8 *p_data)
{
    BT_HDR *p = buffer;
    UINT8 *pp = (UINT8 *)(p + 1);

    p->len = HCIC_PREAMBLE_SIZE + len;
    p->offset = 0;

    UINT16_TO_STREAM(pp, HCI_GROUP_VENDOR_SPECIFIC | opcode);
    UINT8_TO_STREAM(pp, len);
    ARRAY_TO_STREAM(pp, p_data, len);

    btu_hcif_send_cmd(p);
}



__declspec(section ".rodata") const UINT8 hcicmds_rodata_pad[4] = {0};
