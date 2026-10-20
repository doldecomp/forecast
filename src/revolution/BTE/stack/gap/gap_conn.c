/******************************************************************************
 *
 *  Copyright (C) 2009-2013 Broadcom Corporation
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
 * stack/gap/gap_conn.c
 */

/* Includes changes by muff1n1634 and for this repository (KoopTheKoopa) */

/******************************************************************************
 *
 *  This file contains the GAP connection functions.
 *
 ******************************************************************************/

#include "gap_int.h"

#include <stddef.h> // NULL
#include <cstring>

#include "bt_target.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "data_types.h"

#include "btm_api.h"
#include "btu.h"
#include "gap_api.h"
#include "gki.h"
#include "l2cdefs.h"
#include "l2c_api.h"
#include "profiles_api.h"

char gap_conn_trace_separator[] = "*******";
char gap_conn_trace_unexpected_addr[] = "WARNING: GAP Conn Indication for Unexpected Bd Addr...Disconnecting";
char gap_conn_trace_connect_ind[] = "GAP_CONN - Rcvd L2CAP conn ind, CID: 0x%x";
char gap_conn_trace_disconnect_ind[] = "GAP_CONN - Rcvd L2CAP disc, CID: 0x%x";
char gap_conn_trace_congestion_ind[0x38] = "GAP_CONN - Rcvd L2CAP Is Congested (%d), CID: 0x%x";



static void gap_connect_ind(BD_ADDR bd_addr, UINT16 l2cap_cid, UINT16 psm,
                            UINT8 l2cap_id);
static void gap_connect_cfm(UINT16 l2cap_cid, UINT16 result);
static void gap_config_ind(UINT16 l2cap_cid, tL2CAP_CFG_INFO *p_cfg);
static void gap_config_cfm(UINT16 l2cap_cid, tL2CAP_CFG_INFO *p_cfg);
static void gap_disconnect_ind(UINT16 l2cap_cid, BOOLEAN ack_needed);
static void gap_data_ind(UINT16 l2cap_cid, BT_HDR *p_msg);
static void gap_congestion_ind(UINT16 lcid, BOOLEAN is_congested);
static tGAP_CCB *gap_find_ccb_by_cid(UINT16 cid);
static tGAP_CCB *gap_find_ccb_by_handle(UINT16 handle);
static tGAP_CCB *gap_allocate_ccb(void);
static void gap_release_ccb(tGAP_CCB *p_ccb);

void gap_conn_init(void)
{
    gap_cb.conn.reg_info.pL2CA_ConnectInd_Cb		= &gap_connect_ind;
    gap_cb.conn.reg_info.pL2CA_ConnectCfm_Cb		= &gap_connect_cfm;
    gap_cb.conn.reg_info.pL2CA_ConnectPnd_Cb		= NULL;
    gap_cb.conn.reg_info.pL2CA_ConfigInd_Cb			= &gap_config_ind;
    gap_cb.conn.reg_info.pL2CA_ConfigCfm_Cb			= &gap_config_cfm;
    gap_cb.conn.reg_info.pL2CA_DisconnectInd_Cb		= &gap_disconnect_ind;
    gap_cb.conn.reg_info.pL2CA_DisconnectCfm_Cb		= NULL;
    gap_cb.conn.reg_info.pL2CA_QoSViolationInd_Cb	= NULL;
    gap_cb.conn.reg_info.pL2CA_DataInd_Cb			= &gap_data_ind;
    gap_cb.conn.reg_info.pL2CA_CongestionStatus_Cb	= &gap_congestion_ind;
}

static void gap_connect_ind(BD_ADDR bd_addr, UINT16 l2cap_cid, UINT16 psm,
                            UINT8 l2cap_id)
{
    UINT16 xx;
    tGAP_CCB *p_ccb;

    /* explicitly post-increment */
    for (xx = 0, p_ccb = gap_cb.conn.ccb_pool; xx < GAP_MAX_CONNECTIONS;
         xx++, p_ccb++)
    {
        if (p_ccb->con_state == GAP_CCB_STATE_LISTENING && p_ccb->psm == psm
            && (!p_ccb->rem_addr_specified
                || memcmp(bd_addr, p_ccb->rem_dev_address, BD_ADDR_LEN) == 0))
        {
            break;
        }
    }

    if (xx == GAP_MAX_CONNECTIONS)
    {
        GAP_TRACE(WARNING, gap_conn_trace_separator);
        GAP_TRACE(WARNING, gap_conn_trace_unexpected_addr);
        GAP_TRACE(WARNING, gap_conn_trace_separator);

        L2CA_DisconnectReq(l2cap_cid);
        return;
    }

    p_ccb->con_state = GAP_CCB_STATE_CFG_SETUP;

    memcpy(p_ccb->rem_dev_address, bd_addr, BD_ADDR_LEN);
    p_ccb->connection_id = l2cap_cid;

    L2CA_ConnectRsp(bd_addr, l2cap_id, l2cap_cid, L2CAP_CONN_OK, L2CAP_CONN_OK);

    GAP_TRACE(EVENT, gap_conn_trace_connect_ind,
              p_ccb->connection_id);

    L2CA_ConfigReq(l2cap_cid, &p_ccb->cfg);
}

static void gap_connect_cfm(UINT16 l2cap_cid, UINT16 result)
{
    tGAP_CCB *p_ccb = gap_find_ccb_by_cid(l2cap_cid);

    if (!p_ccb)
        return;

    if (result == L2CAP_CONN_OK
        && p_ccb->con_state == GAP_CCB_STATE_CONN_SETUP)
    {
        p_ccb->con_state = GAP_CCB_STATE_CFG_SETUP;

        L2CA_ConfigReq(l2cap_cid, &p_ccb->cfg);
    }
    else
    {
        if (p_ccb->p_callback)
            (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_CLOSED);

        gap_release_ccb(p_ccb);
    }
}

static void gap_config_ind(UINT16 l2cap_cid, tL2CAP_CFG_INFO *p_cfg)
{
    tGAP_CCB *p_ccb = gap_find_ccb_by_cid(l2cap_cid);

    if (!p_ccb)
        return;

    if (!p_cfg->mtu_present || p_cfg->mtu > L2CAP_MTU_SIZE)
        p_ccb->rem_mtu_size = L2CAP_MTU_SIZE;
    else
        p_ccb->rem_mtu_size = p_cfg->mtu;

    p_cfg->flush_to_present = FALSE;
    p_cfg->mtu_present = FALSE;
    p_cfg->result = L2CAP_CFG_OK;

    L2CA_ConfigRsp(l2cap_cid, p_cfg);

    p_ccb->con_flags |= GAP_CCB_FLAGS_THEIR_CFG_DONE;

    if (p_ccb->con_flags & GAP_CCB_FLAGS_OUR_CFG_DONE)
    {
        p_ccb->con_state = GAP_CCB_STATE_CONNECTED;

        (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_OPENED);
    }
}

static void gap_config_cfm(UINT16 l2cap_cid, tL2CAP_CFG_INFO *p_cfg)
{
    tGAP_CCB *p_ccb = gap_find_ccb_by_cid(l2cap_cid);

    if (!p_ccb)
        return;

    if (p_cfg->result == L2CAP_CFG_OK)
    {
        p_ccb->con_flags |= GAP_CCB_FLAGS_OUR_CFG_DONE;

        if (p_ccb->con_flags & GAP_CCB_FLAGS_THEIR_CFG_DONE)
        {
            p_ccb->con_state = GAP_CCB_STATE_CONNECTED;

            (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_OPENED);
        }
    }
    else
    {
        (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_CLOSED);
        gap_release_ccb(p_ccb);
    }
}

static void gap_disconnect_ind(UINT16 l2cap_cid, BOOLEAN ack_needed)
{
    tGAP_CCB *p_ccb;

    GAP_TRACE(EVENT, gap_conn_trace_disconnect_ind, l2cap_cid);

    if ((p_ccb = gap_find_ccb_by_cid(l2cap_cid)) == NULL)
        return;

    if (ack_needed)
        L2CA_DisconnectRsp(l2cap_cid);

    (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_CLOSED);
    gap_release_ccb(p_ccb);
}

static void gap_data_ind(UINT16 l2cap_cid, BT_HDR *p_msg)
{
    tGAP_CCB *p_ccb = gap_find_ccb_by_cid(l2cap_cid);

    if (!p_ccb)
    {
        GKI_freebuf(p_msg);
        return;
    }

    if (p_ccb->con_state == GAP_CCB_STATE_CONNECTED)
    {
        GKI_enqueue(&p_ccb->rx_queue, p_msg);

        (*p_ccb->p_callback)(p_ccb->gap_handle, GAP_EVT_CONN_DATA_AVAIL);
    }
    else
    {
        GKI_freebuf(p_msg);
    }
}

static void gap_congestion_ind(UINT16 lcid, BOOLEAN is_congested)
{
    tGAP_CCB *p_ccb;
    UINT16 event;

    GAP_TRACE(EVENT, gap_conn_trace_congestion_ind,
              is_congested, lcid);

    if ((p_ccb = gap_find_ccb_by_cid(lcid)) == NULL)
        return;

    event = is_congested ? GAP_EVT_CONN_CONGESTED : GAP_EVT_CONN_UNCONGESTED;
    (*p_ccb->p_callback)(p_ccb->gap_handle, event);
}

static tGAP_CCB *gap_find_ccb_by_cid(UINT16 cid)
{
    UINT16 xx;
    tGAP_CCB *p_ccb;

    /* explicitly post-increment */
    for (xx = 0, p_ccb = gap_cb.conn.ccb_pool; xx < GAP_MAX_CONNECTIONS;
         xx++, p_ccb++)
    {
        if (p_ccb->con_state != GAP_CCB_STATE_IDLE
            && p_ccb->connection_id == cid)
        {
            return p_ccb;
        }
    }

    return NULL;
}

static tGAP_CCB *gap_find_ccb_by_handle(UINT16 handle)
{
    tGAP_CCB *p_ccb;

    if (handle < GAP_MAX_CONNECTIONS)
    {
        p_ccb = &gap_cb.conn.ccb_pool[handle];

        if (p_ccb->con_state != GAP_CCB_STATE_IDLE)
            return p_ccb;
    }

    return NULL;
}

static tGAP_CCB *gap_allocate_ccb(void)
{
    UINT16 xx;
    tGAP_CCB *p_ccb;

    /* explicitly post-increment */
    for (xx = 0, p_ccb = gap_cb.conn.ccb_pool; xx < GAP_MAX_CONNECTIONS;
         xx++, p_ccb++)
    {
        if (p_ccb->con_state == GAP_CCB_STATE_IDLE)
        {
            memset(p_ccb, 0, sizeof *p_ccb);

            p_ccb->gap_handle = xx;
            p_ccb->rem_mtu_size = L2CAP_MTU_SIZE;

            return p_ccb;
        }
    }

    return NULL;
}

static void gap_release_ccb(tGAP_CCB *p_ccb)
{
    UINT16 xx;
    UINT16 psm = p_ccb->psm;

    while (p_ccb->rx_queue.p_first)
        GKI_freebuf(GKI_dequeue(&p_ccb->rx_queue));

    p_ccb->con_state = GAP_CCB_STATE_IDLE;

    /* explicitly post-increment */
    for (xx = 0, p_ccb = gap_cb.conn.ccb_pool; xx < GAP_MAX_CONNECTIONS;
         xx++, p_ccb++)
    {
        if (p_ccb->con_state != GAP_CCB_STATE_IDLE && p_ccb->psm == psm)
            return;
    }

    L2CA_Deregister(psm);
}
