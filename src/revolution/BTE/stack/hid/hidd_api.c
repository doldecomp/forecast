#include "hidd_api.h"

#include <cstring>

#include <decomp.h> // ARRAY_LENGTH

#include "bt_trace.h"
#include "bt_types.h"
#include "data_types.h"
#include "wcassert.h" // WCAssert_Line

#include "btm_api.h"
#include "gki.h"
#include "hiddefs.h"
#include "hidd_int.h"
#include "sdp_api.h"

static UINT8 const HidDevLangList[8] = {53, 6, 9, 4, 9, 9, 1, 0};


void HID_DevInit(void)
{
    memset(&hd_cb, 0, sizeof hd_cb);

    hd_cb.l2cap_ctrl_cfg.mtu_present = TRUE;
    hd_cb.l2cap_ctrl_cfg.mtu = 64;

    hd_cb.l2cap_int_cfg.mtu_present = TRUE;
    hd_cb.l2cap_int_cfg.mtu = 64;

    hd_cb.conn.timer_entry.param =
        (TIMER_PARAM_TYPE)&hidd_proc_repage_timeout;

    hd_cb.trace_level = BT_TRACE_LEVEL_NONE;
}









