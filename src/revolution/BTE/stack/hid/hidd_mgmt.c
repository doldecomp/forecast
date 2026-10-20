#include "hidd_int.h"

#include <cstring>

#include "bt_trace.h"
#include "bt_types.h"
#include "data_types.h"

#include "btu.h"
#include "hiddefs.h"
#include "hidd_api.h"

static tHID_STATUS hidd_no_conn_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_connecting_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_connected_proc_evt(UINT8 event, void *p_data);
static tHID_STATUS hidd_disc_ing_proc_evt(UINT8 event, void *p_data);


tHID_DEV_CTB hd_cb;



char hidd_mgmt_trace_repage[0x20] = "hidd_proc_repage_timeout";

void hidd_proc_repage_timeout(void)
{
    HIDD_TRACE(DEBUG, hidd_mgmt_trace_repage);

    ++hd_cb.conn_tries;

    if (hidd_conn_initiate())
    {
        if (hd_cb.conn_tries > 15)
        {
            hd_cb.dev_state = 0;
            (*hd_cb.callback)(1, 0, NULL);
        }
        else
        {
            btu_start_timer(&hd_cb.conn.timer_entry, 22, 1);
        }
    }
    else
    {
        (*hd_cb.callback)(2, hd_cb.conn_tries, NULL);
    }
}




