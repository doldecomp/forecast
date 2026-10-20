/******************************************************************************
 *
 *  NOTICE OF CHANGES
 *  2026/10/20:
 *      - Replace the 2012 Bluedroid BTU task loop with the RVL version
 *        reconstructed from the retail binary. RVL runs the BTU handler
 *        from a fiber (see bte_main.c) and keeps the 2012-only helpers
 *        (quick timers, timer/event registration) out of the image.
 *
 *  Compile with REVOLUTION defined to include these changes.
 *
 ******************************************************************************/

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

/******************************************************************************
 *
 *  This file contains the main Bluetooth Upper Layer processing loop.
 *  The Broadcom implementations of L2CAP RFCOMM, SDP and the BTIf run as one
 *  GKI task. This btu_task switches between them.
 *
 ******************************************************************************/

#include <revolution/OS.h>

#include "bt_target.h"

#include "bt_types.h"
#include "btu.h"
#include "gki.h"

#include "bta_sys.h"

#include "l2c_int.h"
#include "sdpint.h"

#include "btm_int.h"

#include "hidh_int.h"

#include "port_ext.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Set by bta_sys_init() and used to rate-limit the BTU timers. */
#define BTU_TASK_TICK_LIMIT 500

typedef void (tUSER_TIMEOUT_FUNC)(TIMER_LIST_ENT* p_tle);

/* Control block for the BTU task. */
tBTU_CB btu_cb;

/* Number of times the handler was entered while it was still running. */
static UINT8 s_btu_task_reentry = 1;

/* TRUE when no BTU handler is running (retail: 4-byte flag). */
static UINT32 s_btu_task_idle = 1;

/* Counts BTU handler invocations and the last tick the timers ran on. */
static UINT32 s_btu_last_timer_tick;
static UINT32 s_btu_tick_count;

/*******************************************************************************
**
** Function         btu_task_init
**
** Description      Initialize the core stack control blocks and the BTU
**                  task state. Called from BTA_Init before the task runs.
**
** Returns          void
**
*******************************************************************************/
void btu_task_init(void)
{
    s_btu_task_reentry = 1;
    s_btu_task_idle = 1;
    s_btu_tick_count = 0;
    s_btu_last_timer_tick = 0;

    btu_init_core();
    BTE_InitStack();
    bta_sys_init();
}

/*******************************************************************************
**
** Function         btu_task_msg_handler
**
** Description      Process the BTU mailboxes and the expired BTU timers.
**                  Runs on the BTU fiber.
**
** Returns          void
**
*******************************************************************************/
void btu_task_msg_handler(void)
{
    BT_HDR* p_msg;
    TIMER_LIST_ENT* p_tle;
    UINT16 event;
    UINT16 mask;
    UINT8 i;
    BOOLEAN handled = FALSE;
    BOOLEAN found;

    /* Sampling the tick counter matches the retail handler. */
    OSGetTime();

    s_btu_tick_count++;

    GKI_disable();

    if (s_btu_task_idle)
    {
        s_btu_task_idle = FALSE;
        s_btu_task_reentry = 1;
    }
    else
    {
        s_btu_task_reentry++;
        GKI_enable();
        return;
    }

    GKI_enable();

    mask = TASK_MBOX_0_EVT_MASK | TASK_MBOX_2_EVT_MASK;

    if (s_btu_tick_count > s_btu_last_timer_tick + BTU_TASK_TICK_LIMIT)
    {
        mask |= TIMER_0_EVT_MASK | TIMER_1_EVT_MASK;
        s_btu_last_timer_tick = s_btu_tick_count;
    }

    while (!handled)
    {
        handled = TRUE;

        if (mask & TASK_MBOX_0_EVT_MASK)
        {
            while ((p_msg = (BT_HDR*)GKI_read_mbox(BTU_HCI_RCV_MBOX)) != NULL)
            {
                handled = FALSE;

                event = (UINT16)(p_msg->event & BT_EVT_MASK);

                switch (event)
                {
                    case BT_EVT_TO_BTU_HCI_ACL:
                        l2c_rcv_acl_data(p_msg);
                        break;

                    case BT_EVT_TO_BTU_L2C_SEG_XMIT:
                        l2c_link_segments_xmitted(p_msg);
                        break;

                    case BT_EVT_TO_BTU_HCI_SCO:
                        btm_route_sco_data(p_msg);
                        break;

                    case BT_EVT_TO_BTU_HCI_EVT:
                        btu_hcif_process_event(p_msg);
                        GKI_freebuf(p_msg);
                        break;

                    case BT_EVT_TO_BTU_HCI_CMD:
                        btu_hcif_send_cmd(p_msg);
                        break;

                    default:
                        i = 0;
                        found = FALSE;

                        for (; !found && i < BTU_MAX_REG_EVENT; i++)
                        {
                            if (btu_cb.event_reg[i].event_cb == NULL)
                                continue;

                            if (event == btu_cb.event_reg[i].event_range)
                            {
                                if (btu_cb.event_reg[i].event_cb)
                                {
                                    btu_cb.event_reg[i].event_cb(p_msg);
                                    found = TRUE;
                                }
                            }
                        }

                        if (found == FALSE)
                            GKI_freebuf(p_msg);

                        break;
                }
            }
        }

        if (mask & TIMER_0_EVT_MASK)
        {
            GKI_update_timer_list(&btu_cb.timer_queue, 1);
            mask &= ~TIMER_0_EVT_MASK;

            while (btu_cb.timer_queue.p_first != NULL &&
                   btu_cb.timer_queue.p_first->ticks == 0)
            {
                p_tle = btu_cb.timer_queue.p_first;

                handled = FALSE;
                GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);

                switch (p_tle->event)
                {
                    case BTU_TTYPE_BTM_DEV_CTL:
                        btm_dev_timeout(p_tle);
                        break;

                    case BTU_TTYPE_BTM_ACL:
                        btm_acl_timeout(p_tle);
                        break;

                    case BTU_TTYPE_L2CAP_LINK:
                    case BTU_TTYPE_L2CAP_CHNL:
                    case BTU_TTYPE_L2CAP_HOLD:
                        l2c_process_timeout(p_tle);
                        break;

                    case BTU_TTYPE_SDP:
                        sdp_conn_timeout((tCONN_CB*)p_tle->param);
                        break;

                    case BTU_TTYPE_BTM_RMT_NAME:
                        btm_inq_rmt_name_failed();
                        break;

                    case 8:
                        /* RVL discovery database timeout */
                        btm_discovery_db_timeout();
                        break;

                    case BTU_TTYPE_RFCOMM_MFC:
                    case BTU_TTYPE_RFCOMM_PORT:
                        rfcomm_process_timeout(p_tle);
                        break;

                    case BTU_TTYPE_BTU_CMD_CMPL:
                        btu_hcif_cmd_timeout();
                        break;

                    case BTU_TTYPE_HID_HOST_REPAGE_TO:
                        hidh_proc_repage_timeout(p_tle);
                        break;

                    case BTU_TTYPE_USER_FUNC:
                        (*((tUSER_TIMEOUT_FUNC*)p_tle->param))(p_tle);
                        break;

                    default:
                        i = 0;
                        found = FALSE;

                        for (; !found && i < BTU_MAX_REG_TIMER; i++)
                        {
                            if (btu_cb.timer_reg[i].timer_cb == NULL)
                                continue;

                            if (btu_cb.timer_reg[i].p_tle == p_tle)
                            {
                                btu_cb.timer_reg[i].timer_cb(p_tle);
                                found = TRUE;
                            }
                        }

                        break;
                }
            }
        }

        if (mask & TASK_MBOX_2_EVT_MASK)
        {
            while ((p_msg = (BT_HDR*)GKI_read_mbox(TASK_MBOX_2)) != NULL)
            {
                handled = FALSE;
                bta_sys_event(p_msg);
            }
        }

        if (mask & TIMER_1_EVT_MASK)
        {
            mask &= ~TIMER_1_EVT_MASK;
            bta_sys_timer_update();
        }

        /* Retail checks an exit event bit that nothing sets in this build. */
        if (mask & 0x8000)
            break;
    }

    s_btu_task_idle = TRUE;
}

/*******************************************************************************
**
** Function         btu_start_timer
**
** Description      Start a BTU timer.
**
** Returns          void
**
*******************************************************************************/
void btu_start_timer(TIMER_LIST_ENT* p_tle, UINT16 type, UINT32 timeout)
{
    GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);

    p_tle->event = type;
    p_tle->ticks = timeout;

    GKI_add_to_timer_list(&btu_cb.timer_queue, p_tle);
}

/*******************************************************************************
**
** Function         btu_stop_timer
**
** Description      Stop a BTU timer.
**
** Returns          void
**
*******************************************************************************/
void btu_stop_timer(TIMER_LIST_ENT* p_tle)
{
    GKI_remove_from_timer_list(&btu_cb.timer_queue, p_tle);
}

#ifdef __cplusplus
}
#endif
