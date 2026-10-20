#include "hidd_int.h"

#include <cstring>

#include "bt_trace.h"
#include "bt_types.h"
#include "data_types.h"

#include "btm_api.h" // BTM_SetQoS
#include "btm_int.h"
#include "gki.h" // GKI_freebuf
#include "hid_conn.h"
#include "hidd_api.h"
#include "hiddefs.h"
#include "l2c_api.h"

char hidd_conn_trace_originate[0x18] = "HID - Originate started";
char hidd_conn_trace_originate_failed[0x18] = "HID - Originate failed";





tHID_STATUS hidd_conn_initiate(void)
{
    tHID_CONN *p_hcon = &hd_cb.conn;

    HIDD_TRACE(EVENT, hidd_conn_trace_originate);

    p_hcon->ctrl_cid = 0;
    p_hcon->intr_cid = 0;
    p_hcon->conn_flags = HID_CONN_FLAGS_IS_ORIG;

    if ((p_hcon->ctrl_cid = L2CA_ConnectReq(17, hd_cb.host_addr)) == 0)
    {
        HIDD_TRACE(WARNING, hidd_conn_trace_originate_failed);
        return HID_ERR_L2CAP_FAILED;
    }
    else
    {
        p_hcon->conn_state = HID_CONN_STATE_CONNECTING_CTRL;
        return HID_SUCCESS;
    }
}












