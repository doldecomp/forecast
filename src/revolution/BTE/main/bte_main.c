/******************************************************************************
 *
 *  NOTICE OF CHANGES
 *  2026/10/20:
 *      - Reconstruct the RVL BTE bring-up and shutdown helpers from the
 *        retail binary (BTA_Init, BTA_CleanUp, BTUInterruptHandler,
 *        bta_usb_close_evt).
 *
 *  Compile with REVOLUTION defined to include these changes.
 *
 ******************************************************************************/

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

/******************************************************************************
 *
 *  Filename:      bte_main.c
 *
 *  Description:   Contains BTE core stack initialization and shutdown code
 *
 ******************************************************************************/

#include <revolution/OS.h>

#include "data_types.h"

#include "bte.h"
#include "bte_common.h"
#include "btu.h"
#include "gki.h"
#include "hci.h"
#include "misc.h"
#include "rvl/uusb.h"

#ifdef __cplusplus
extern "C" {
#endif

/* HCI transport configuration (Broadcom USB vendor/product ids). */
static tHCI_CFG s_hci_cfg[2] = {{0x0A5C, 0x2101}, {0x0000, 0x0000}};

/* Fiber stack used to run the BTU task, and the alarm that switches to it. */
static UINT32 s_btu_stack[0x400];
static OSAlarm s_btu_alarm;

/* Shutdown callback handed to BTA_CleanUp. */
static BTA_CleanUpCallback s_cleanup_cb;

/* Target mode the board is running in (see bte.h). */
volatile UINT8 bte_target_mode;

/* Provided by bte_hcisu.c */
extern tHCI_CFG* p_hcisu_cfg;
extern tHCI_IF* p_hcisu_if;

/* Provided by btu_task.c */
extern void btu_task_init(void);
extern void btu_task_msg_handler(void);

/******************************************************************************
**
** Function         BTUInterruptHandler
**
** Description      Periodic alarm handler: switches into the BTU fiber.
**
** Returns          None
**
******************************************************************************/
void BTUInterruptHandler(void)
{
    OSSwitchFiber((void*)btu_task_msg_handler,
                  (void*)((UINT8*)s_btu_stack + sizeof(s_btu_stack)));
}

/******************************************************************************
**
** Function         BTA_Init
**
** Description      BTE MAIN API - Entry point for BTE chip/stack initialization
**
** Returns          Status of the operation
**
******************************************************************************/
tBTA_STATUS BTA_Init(void)
{
    s_cleanup_cb = NULL;

    GKI_init();
    GKI_enable();
    BTE_Init();

    p_hcisu_if = (tHCI_IF*)&hcisu_h2;
    p_hcisu_cfg = s_hci_cfg;
    bte_hcisu_task(0);

    while (wait4hci)
    {
        GKI_delay(100);
    }

    btu_task_init();

    OSCreateAlarm(&s_btu_alarm);
    OSSetPeriodicAlarm(&s_btu_alarm, OSGetTime(), OS_MSEC_TO_TICKS(2),
                       (OSAlarmHandler)BTUInterruptHandler);

    GKI_run(0);

    while (!BTA_DmIsDeviceUp())
    {
        GKI_delay(2000);
    }

    return BTA_SUCCESS;
}

/******************************************************************************
**
** Function         BTA_CleanUp
**
** Description      BTE MAIN API - Shutdown code for BTE chip/stack
**
** Returns          None
**
******************************************************************************/
void BTA_CleanUp(BTA_CleanUpCallback p_cb)
{
    s_cleanup_cb = p_cb;

    OSCancelAlarm(&s_btu_alarm);
    bte_hcisu_close();
    GKI_shutdown();
}

/******************************************************************************
**
** Function         bta_usb_close_evt
**
** Description      USB transport closed: report the result to the BTA user.
**
** Returns          None
**
******************************************************************************/
void bta_usb_close_evt(INT8 param_1)
{
    if (s_cleanup_cb == NULL)
        return;

    if (param_1 >= 0)
        (*s_cleanup_cb)(BTA_SUCCESS);
    else
        (*s_cleanup_cb)(BTA_FAILURE);
}

#ifdef __cplusplus
}
#endif
