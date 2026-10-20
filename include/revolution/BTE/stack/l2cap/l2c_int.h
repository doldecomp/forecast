/* Original source:
 * bluedroid <android.googlesource.com/platform/external/bluetooth/bluedroid>
 * stack/l2cap/l2c_int.h
 */

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

/* Includes changes by muff1n1634 and for this repository (KoopTheKoopa) */

/******************************************************************************
 *
 *  This file contains L2CAP internal definitions.
 *
 ******************************************************************************/

#ifndef BTE_L2CAP_INT_H
#define BTE_L2CAP_INT_H

#include "bt_target.h"
#include "bt_types.h"
#include "data_types.h"

#include "gki.h"
#include "l2c_api.h"

#define L2CAP_MIN_MTU                   48      /* Minimum acceptable MTU is 48 bytes */

/* Timeouts. Since L2CAP works off a 1-second list, all are in seconds. */
#define L2CAP_ECHO_RSP_TOUT             30      /* 30 seconds */
#define L2CAP_CHNL_CONNECT_TOUT         60      /* 60 seconds */
#define L2CAP_CHNL_CFG_TOUT             30      /* 30 seconds */
#define L2CAP_CHNL_CONNECT_TOUT_EXT     120     /* 120 seconds */
#define L2CAP_CHNL_DISCONNECT_TOUT      30      /* 30 seconds */
#define L2CAP_LINK_CONNECT_TOUT         60      /* 60 seconds */
#define L2CAP_DELAY_CHECK_SM4           2       /* 2 seconds */
#define L2CAP_LINK_DISCONNECT_TOUT      30      /* 30 seconds */
#define L2CAP_LINK_FLOW_CONTROL_TOUT    2       /* 2 seconds */
#define L2CAP_LINK_CONNECT_TOUT_EXT     120     /* 120 seconds */
#define L2CAP_LINK_ROLE_SWITCH_TOUT     10      /* 10 seconds */

#define L2CAP_SEND_CMD_OFFSET           0

#ifdef __cplusplus
    extern "C" {
#endif

typedef enum
{
    CST_CLOSED,
    CST_ORIG_W4_SEC_COMP,
    CST_TERM_W4_SEC_COMP,
    CST_W4_L2CAP_CONNECT_RSP,
    CST_W4_L2CA_CONNECT_RSP,
    CST_CONFIG,
    CST_OPEN,
    CST_W4_L2CAP_DISCONNECT_RSP,
    CST_W4_L2CA_DISCONNECT_RSP,
} tL2C_CHNL_STATE;

typedef enum
{
    LST_DISCONNECTED,
    LST_CONNECT_HOLDING,
    LST_CONNECTING_WAIT_SWITCH,
    LST_CONNECTING,
    LST_CONNECTED,
    LST_DISCONNECTING,
} tL2C_LINK_STATE;

enum
{
    L2CEVT_LP_CONNECT_CFM			= 0,
    L2CEVT_LP_CONNECT_CFM_NEG		= 1,
    L2CEVT_LP_DISCONNECT_IND		= 3,
    L2CEVT_LP_QOS_VIOLATION_IND		= 6,
    L2CEVT_SEC_COMP					= 7,
    L2CEVT_SEC_COMP_NEG				= 8,
    L2CEVT_L2CAP_CONNECT_REQ		= 10,
    L2CEVT_L2CAP_CONNECT_RSP		= 11,
    L2CEVT_L2CAP_CONNECT_RSP_PND	= 12,
    L2CEVT_L2CAP_CONNECT_RSP_NEG	= 13,
    L2CEVT_L2CAP_CONFIG_REQ			= 14,
    L2CEVT_L2CAP_CONFIG_RSP			= 15,
    L2CEVT_L2CAP_CONFIG_RSP_NEG		= 16,
    L2CEVT_L2CAP_DISCONNECT_REQ		= 17,
    L2CEVT_L2CAP_DISCONNECT_RSP		= 18,

    L2CEVT_L2CAP_DATA				= 19,
    L2CEVT_L2CA_CONNECT_REQ			= 20,
    L2CEVT_L2CA_CONNECT_RSP			= 21,
    L2CEVT_L2CA_CONNECT_RSP_NEG		= 22,
    L2CEVT_L2CA_CONFIG_REQ			= 23,
    L2CEVT_L2CA_CONFIG_RSP			= 24,
    L2CEVT_L2CA_CONFIG_RSP_NEG		= 25,
    L2CEVT_L2CA_DISCONNECT_REQ		= 26,
    L2CEVT_L2CA_DISCONNECT_RSP		= 27,
    L2CEVT_L2CA_DATA_WRITE			= 29,

    L2CEVT_TIMEOUT					= 30,
    L2CEVT_SEC_RE_SEND_CMD			= 31,
    L2CEVT_ACK_TIMEOUT				= 32,

#define L2CEVT_SEC_COMP		(L2CEVT_SEC_COMP + 0)
#define L2CEVT_SEC_COMP_NEG	(L2CEVT_SEC_COMP_NEG + 0)
};

enum
{
    L2CAP_ADJ_ID	= 0x03,
};

enum
{
    L2CAP_PEER_CFG_UNACCEPTABLE,
    L2CAP_PEER_CFG_OK,
    L2CAP_PEER_CFG_DISCONNECT,
};

typedef UINT8 tL2C_CCB_CFG_FLAGS;
enum
{
    IB_CFG_DONE		= 1 << 0,
    OB_CFG_DONE		= 1 << 1,
    RECONFIG_FLAG	= 1 << 2,

    CFG_DONE_MASK	= IB_CFG_DONE | OB_CFG_DONE
};

typedef struct
{
    BD_ADDR	bd_addr;		// 0x00
    UINT8	status;			// 0x06
    /* 1 byte padding */
    UINT16	psm;			// 0x08
    UINT16	l2cap_result;	// 0x0a
    UINT16	l2cap_status;	// 0x0c
    UINT16	remote_cid;		// 0x0e
} tL2C_CONN_INFO; // size 0x10

typedef struct
{
    BOOLEAN				in_use;	// 0x00
    /* 1 byte padding */
    UINT16				psm;	// 0x02
    tL2CAP_APPL_INFO	api;	// 0x04
} tL2C_RCB; // size 0x2c

typedef struct t_l2c_ccb
{
    BOOLEAN				in_use;			// 0x00
    /* 3 bytes padding */
    tL2C_CHNL_STATE		chnl_state;		// 0x04
    struct t_l2c_ccb	*p_next_ccb;	// 0x08
    struct t_l2c_ccb	*p_prev_ccb;	// 0x0c
    struct t_l2c_linkcb	*p_lcb;			// 0x10
    UINT16				local_cid;		// 0x14
    UINT16				remote_cid;		// 0x16
    TIMER_LIST_ENT		timer_entry;	// 0x18
    tL2C_RCB			*p_rcb;			// 0x30
    tL2C_CCB_CFG_FLAGS	config_done;	// 0x34
    UINT8				local_id;		// 0x35
    UINT8				remote_id;		// 0x36
    BOOLEAN				rtry_flag;		// 0x37
    UINT16				in_mtu;			// 0x38
    UINT16				out_mtu;		// 0x3a
    UINT16				our_flush_to;	// 0x3c
    /* 2 bytes padding */
    FLOW_SPEC			in_flow;		// 0x40
    FLOW_SPEC			out_flow;		// 0x58
    BUFFER_Q			xmit_hold_q;	// 0x70
} tL2C_CCB; // size 0x7c

typedef struct t_l2c_linkcb
{
    BOOLEAN				in_use;						// 0x00
    /* 3 bytes padding */
    tL2C_LINK_STATE		link_state;					// 0x04
    tL2C_CCB			*p_first_ccb;				// 0x08
    tL2C_CCB			*p_last_ccb;				// 0x0c
    TIMER_LIST_ENT		timer_entry;				// 0x10
    UINT16				handle;						// 0x28
    BD_ADDR				remote_bd_addr;				// 0x2a
    UINT8				link_role;					// 0x30
    UINT8				id;							// 0x31
    UINT16				link_flush_tout;			// 0x32
    UINT16				link_xmit_quota;			// 0x34
    UINT16				link_xmit_window;			// 0x36
    UINT16				sent_not_acked;				// 0x38
    UINT16				link_cong_start;			// 0x3a
    UINT16				link_cong_end;				// 0x3c
    UINT16				link_cong_discard;			// 0x3e
    BOOLEAN				partial_segment_being_sent;	// 0x40
    BOOLEAN				cong_sent;					// 0x41
    /* 2 bytes padding */
    BUFFER_Q			link_xmit_data_q;			// 0x44
    BT_HDR				*p_hcit_rcv_acl;			// 0x50
    tL2CA_ECHO_RSP_CB	*p_echo_rsp_cb;				// 0x54
    UINT16				idle_timeout;				// 0x58
    UINT8				acl_priority;				// 0x5a
    /* 1 byte padding */
} tL2C_LCB; // size 0x5c

typedef struct
{
    UINT8			l2cap_trace_level;				// 0x000
    UINT8			desire_role;					// 0x001
    UINT16			num_lm_acl_bufs;				// 0x002
    UINT16			controller_xmit_window;			// 0x004
    /* 2 bytes padding */
    tL2C_LCB		lcb_pool[MAX_L2CAP_LINKS];		// 0x008
    tL2C_CCB		ccb_pool[MAX_L2CAP_CHANNELS];	// 0x178
    tL2C_RCB		rcb_pool[MAX_L2CAP_CLIENTS];	// 0x650
    tL2C_CCB		*p_free_ccb_first;				// 0x7b0
    tL2C_CCB		*p_free_ccb_last;				// 0x7b4
    UINT16			num_links_active;				// 0x7b8
    UINT16			idle_timeout;					// 0x7ba
    tL2C_LCB		*p_cur_hcit_lcb;				// 0x7bc
    BUFFER_Q		rcv_hold_q;						// 0x7c0
    TIMER_LIST_ENT	rcv_hold_tle;					// 0x7cc
    UINT8			compress_pool_id;				// 0x7e4
    /* 3 bytes padding */
} tL2C_CB; // size 0x7e8

extern tL2C_CB l2cb;

void l2c_csm_execute(tL2C_CCB *p_ccb, UINT16 event, void *p_data);
void forward_peer_data(tL2C_CCB *p_ccb, BT_HDR *p_buf);

void l2c_link_hci_conn_req(BD_ADDR bd_addr);
BOOLEAN l2c_link_hci_conn_comp(UINT8 status, UINT16 handle, BD_ADDR p_bda);
void l2c_link_sec_comp(BD_ADDR p_bda, void *p_ref_data, UINT8 status);
BOOLEAN l2c_link_hci_disc_comp(UINT16 handle, UINT8 reason);
BOOLEAN l2c_link_hci_qos_violation(UINT16 handle);
void l2c_link_timeout(tL2C_LCB *p_lcb);
BOOLEAN l2c_link_send_to_lower(tL2C_LCB *p_lcb, BT_HDR *p_buf);
void l2c_link_check_send_pkts(tL2C_LCB *p_lcb,
                              tL2C_CCB *p_ccb,
                              BT_HDR *p_buf);
void l2c_link_adjust_allocation(void);
void l2c_link_process_num_completed_pkts(UINT8 *p);
/* NOTE the misspelling of process */
void l2c_link_processs_num_bufs(UINT16 num_lm_acl_bufs);
BT_HDR *l2cap_link_chk_pkt_start(BT_HDR *p_cur_buf);
BOOLEAN l2cap_link_chk_pkt_end(void);
BT_HDR *l2c_link_get_decompressed_pkt(BT_HDR *p_cur);
UINT8 l2c_link_pkts_rcvd(UINT16 *num_pkts, UINT16 *handles);
void l2c_link_role_changed(BD_ADDR bd_addr, UINT8 new_role);
void l2c_link_role_change_failed(void);
void l2c_link_segments_xmitted(BT_HDR *p_msg);
void l2c_pin_code_request(BD_ADDR bd_addr);

void l2c_init(void);
void l2c_rcv_acl_data(BT_HDR *p_msg);
void l2c_process_compress_setup_timeout(TIMER_LIST_ENT *p_tle);
void l2c_process_timeout(TIMER_LIST_ENT *p_tle);
void l2c_process_held_packets(BOOLEAN timed_out);

tL2C_LCB *l2cu_allocate_lcb(BD_ADDR p_bd_addr);
void l2cu_release_lcb(tL2C_LCB *p_lcb);
tL2C_LCB *l2cu_find_lcb_by_bd_addr(BD_ADDR p_bd_addr);
tL2C_LCB *l2cu_find_lcb_by_handle(UINT16 handle);
UINT8 l2cu_get_conn_role (tL2C_LCB *p_this_lcb);
BT_HDR *l2cu_build_header(UINT16 hci_handle, UINT16 len, UINT8 cmd, UINT8 id);
void l2cu_send_peer_cmd_reject(tL2C_LCB *p_lcb, UINT16 reason, UINT8 rem_id,
                               UINT16 p1, UINT16 p2);
void l2cu_send_peer_connect_req(tL2C_CCB *p_ccb);
void l2cu_send_peer_connect_rsp(tL2C_CCB *p_ccb, UINT16 result, UINT16 status);
void l2cu_reject_connection(tL2C_LCB *p_lcb, UINT16 remote_cid, UINT8 rem_id,
                            UINT16 result);
void l2cu_send_peer_config_req(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_send_peer_config_rsp(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_send_peer_config_rej(tL2C_CCB *p_ccb, UINT8 *p_data, UINT16 data_len,
                               UINT16 rej_len);
void l2cu_send_peer_disc_req(tL2C_CCB *p_ccb);
void l2cu_send_peer_disc_rsp(tL2C_LCB *p_lcb, UINT8 remote_id, UINT16 local_cid,
                             UINT16 remote_cid);
void l2cu_send_peer_echo_req(tL2C_LCB *p_lcb, UINT8 *p_data, UINT16 data_len);
void l2cu_send_peer_echo_rsp(tL2C_LCB *p_lcb, UINT8 id, UINT8 *p_data,
                             UINT16 data_len);
void l2cu_send_peer_info_req(tL2C_LCB *p_lcb, UINT16 info_type);
void l2cu_send_peer_info_rsp(tL2C_LCB *p_lcb, UINT8 remote_id, UINT16 info_type);
tL2C_CCB *l2cu_allocate_ccb(tL2C_LCB *p_lcb);
void l2cu_release_ccb(tL2C_CCB *p_ccb);
tL2C_CCB *l2cu_find_ccb_by_cid(tL2C_LCB *p_lcb, UINT16 local_cid);
tL2C_CCB *l2cu_find_ccb_by_remote_cid(tL2C_LCB *p_lcb, UINT16 remote_cid);
tL2C_RCB *l2cu_allocate_rcb(UINT16 psm);
void l2cu_release_rcb(tL2C_RCB *p_rcb);
tL2C_RCB *l2cu_find_rcb_by_psm(UINT16 psm);
BOOLEAN l2cu_process_peer_cfg_req(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_process_peer_cfg_rsp(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_process_our_cfg_req(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_process_our_cfg_rsp(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
void l2cu_device_reset(void);
BOOLEAN l2cu_create_conn(tL2C_LCB *p_lcb);
BOOLEAN l2cu_create_conn_after_switch(tL2C_LCB *p_lcb);
tL2C_LCB *l2cu_find_lcb_by_state(tL2C_LINK_STATE state);
BOOLEAN l2cu_lcb_disconnecting(void);
void l2cu_send_feature_req(tL2C_CCB *p_ccb);
BOOLEAN l2cu_check_feature_req(tL2C_LCB *p_lcb, UINT8 id, UINT8 *p_data,
                               UINT16 data_len);
void l2cu_check_feature_rsp(tL2C_LCB *p_lcb, UINT8 id, UINT8 *p_data,
                            UINT16 data_len);
void l2cu_stop_compression(/* void? */);
BOOLEAN l2cu_lcb_has_feature_enable(/* tL2C_LCB *p_lcb? */);
BOOLEAN l2cu_ccb_has_compression_enable(/* tL2C_CCB *p_ccb? */);
UINT32 L2CA_GetCompressionFixMemoryPoolSize(/* void? */);

#ifdef __cplusplus
    }
#endif


/* 2006 trace helper: L2CAP_TRACE(LEVEL, fmt, ...) -> L2CAP_TRACE_LEVELn(fmt, ...) */
#define L2CAP_TRACE_CAT(a, b)		L2CAP_TRACE_CAT_(a, b)
#define L2CAP_TRACE_CAT_(a, b)		a##b
#define L2CAP_TRACE_NARG(...)		L2CAP_TRACE_NARG_(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0)
#define L2CAP_TRACE_NARG_(_1, _2, _3, _4, _5, _6, _7, N, ...)	N
#define L2CAP_TRACE_DEC_1		0
#define L2CAP_TRACE_DEC_2		1
#define L2CAP_TRACE_DEC_3		2
#define L2CAP_TRACE_DEC_4		3
#define L2CAP_TRACE_DEC_5		4
#define L2CAP_TRACE_DEC_6		5
#define L2CAP_TRACE_DEC_7		6
#define L2CAP_TRACE(type_, ...)	\
    L2CAP_TRACE_CAT(L2CAP_TRACE_CAT(L2CAP_TRACE_, type_),	\
                    L2CAP_TRACE_CAT(L2CAP_TRACE_DEC_, L2CAP_TRACE_NARG(__VA_ARGS__)))(__VA_ARGS__)

#endif // BTE_L2CAP_INT_H
#if (BLE_INCLUDED == TRUE)
extern void l2cu_send_peer_ble_par_req (tL2C_LCB *p_lcb, UINT16 min_int, UINT16 max_int, UINT16 latency, UINT16 timeout);
extern void l2cu_send_peer_ble_par_rsp (tL2C_LCB *p_lcb, UINT16 reason, UINT8 rem_id);
#endif

extern BOOLEAN l2cu_initialize_fixed_ccb (tL2C_LCB *p_lcb, UINT16 fixed_cid, tL2CAP_FCR_OPTS *p_fcr);
extern void    l2cu_no_dynamic_ccbs (tL2C_LCB *p_lcb);
extern void    l2cu_process_fixed_chnl_resp (tL2C_LCB *p_lcb);


/* Functions provided for Broadcom Aware
****************************************
*/
extern BOOLEAN  l2cu_check_feature_req (tL2C_LCB *p_lcb, UINT8 id, UINT8 *p_data, UINT16 data_len);
extern void     l2cu_check_feature_rsp (tL2C_LCB *p_lcb, UINT8 id, UINT8 *p_data, UINT16 data_len);
extern void     l2cu_send_feature_req (tL2C_CCB *p_ccb);

extern tL2C_RCB *l2cu_allocate_rcb (UINT16 psm);
extern tL2C_RCB *l2cu_find_rcb_by_psm (UINT16 psm);
extern void     l2cu_release_rcb (tL2C_RCB *p_rcb);

extern UINT8    l2cu_process_peer_cfg_req (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern void     l2cu_process_peer_cfg_rsp (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern void     l2cu_process_our_cfg_req (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern void     l2cu_process_our_cfg_rsp (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);

extern void     l2cu_device_reset (void);
extern tL2C_LCB *l2cu_find_lcb_by_state (tL2C_LINK_STATE state);
extern BOOLEAN  l2cu_lcb_disconnecting (void);

extern BOOLEAN l2cu_create_conn (tL2C_LCB *p_lcb);
extern BOOLEAN l2cu_create_conn_after_switch (tL2C_LCB *p_lcb);
extern BT_HDR *l2cu_get_next_buffer_to_send (tL2C_LCB *p_lcb);
extern void    l2cu_resubmit_pending_sec_req (BD_ADDR p_bda);
extern void    l2cu_initialize_amp_ccb (tL2C_LCB *p_lcb);
extern void    l2cu_adjust_out_mps (tL2C_CCB *p_ccb);

/* Functions provided by l2c_link.c
************************************
*/
extern BOOLEAN  l2c_link_hci_conn_req (BD_ADDR bd_addr);
extern BOOLEAN  l2c_link_hci_conn_comp (UINT8 status, UINT16 handle, BD_ADDR p_bda);
extern BOOLEAN  l2c_link_hci_disc_comp (UINT16 handle, UINT8 reason);
extern BOOLEAN  l2c_link_hci_qos_violation (UINT16 handle);
extern void     l2c_link_timeout (tL2C_LCB *p_lcb);
extern void     l2c_info_timeout (tL2C_LCB *p_lcb);
extern void     l2c_link_check_send_pkts (tL2C_LCB *p_lcb, tL2C_CCB *p_ccb, BT_HDR *p_buf);
extern void     l2c_link_adjust_allocation (void);
extern void     l2c_link_process_num_completed_pkts (UINT8 *p);
extern void     l2c_link_process_num_completed_blocks (UINT8 controller_id, UINT8 *p, UINT16 evt_len);
extern void     l2c_link_processs_num_bufs (UINT16 num_lm_acl_bufs);
extern UINT8    l2c_link_pkts_rcvd (UINT16 *num_pkts, UINT16 *handles);
extern void     l2c_link_role_changed (BD_ADDR bd_addr, UINT8 new_role);
extern void     l2c_link_sec_comp (BD_ADDR p_bda, void *p_ref_data, UINT8 status);
extern void     l2c_link_segments_xmitted (BT_HDR *p_msg);
extern void     l2c_pin_code_request (BD_ADDR bd_addr);
extern void     l2c_link_adjust_chnl_allocation (void);

#if (BLE_INCLUDED == TRUE)
extern void     l2c_link_processs_ble_num_bufs (UINT16 num_lm_acl_bufs);
#endif

#if ((BTM_PWR_MGR_INCLUDED == TRUE) && L2CAP_WAKE_PARKED_LINK == TRUE)
extern BOOLEAN  l2c_link_check_power_mode ( tL2C_LCB *p_lcb );
#define L2C_LINK_CHECK_POWER_MODE(x) l2c_link_check_power_mode ((x))
#else
#define L2C_LINK_CHECK_POWER_MODE(x) (FALSE)
#endif

#if L2CAP_CONFORMANCE_TESTING == TRUE
/* Used only for conformance testing */
L2C_API extern void l2cu_set_info_rsp_mask (UINT32 mask);
#endif

/* Functions provided by l2c_csm.c
************************************
*/
extern void l2c_csm_execute (tL2C_CCB *p_ccb, UINT16 event, void *p_data);

L2C_API extern BT_HDR   *l2cap_link_chk_pkt_start(BT_HDR *);    /* Called at start of rcv to check L2CAP segmentation */
L2C_API extern BOOLEAN   l2cap_link_chk_pkt_end (void);         /* Called at end   of rcv to check L2CAP segmentation */

L2C_API extern void     l2c_enqueue_peer_data (tL2C_CCB *p_ccb, BT_HDR *p_buf);


/* Functions provided by l2c_fcr.c
************************************
*/
extern void     l2c_fcr_cleanup (tL2C_CCB *p_ccb);
extern void     l2c_fcr_proc_pdu (tL2C_CCB *p_ccb, BT_HDR *p_buf);
extern void     l2c_fcr_proc_tout (tL2C_CCB *p_ccb);
extern void     l2c_fcr_proc_ack_tout (tL2C_CCB *p_ccb);
extern void     l2c_fcr_send_S_frame (tL2C_CCB *p_ccb, UINT16 function_code, UINT16 pf_bit);
extern BT_HDR   *l2c_fcr_clone_buf (BT_HDR *p_buf, UINT16 new_offset, UINT16 no_of_bytes, UINT8 pool);
extern BOOLEAN  l2c_fcr_is_flow_controlled (tL2C_CCB *p_ccb);
extern BT_HDR   *l2c_fcr_get_next_xmit_sdu_seg (tL2C_CCB *p_ccb, UINT16 max_packet_length);
extern void     l2c_fcr_start_timer (tL2C_CCB *p_ccb);

/* Configuration negotiation */
extern UINT8    l2c_fcr_chk_chan_modes (tL2C_CCB *p_ccb);
extern BOOLEAN  l2c_fcr_adj_our_req_options (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern void     l2c_fcr_adj_our_rsp_options (tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_peer_cfg);
extern BOOLEAN  l2c_fcr_renegotiate_chan(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern UINT8    l2c_fcr_process_peer_cfg_req(tL2C_CCB *p_ccb, tL2CAP_CFG_INFO *p_cfg);
extern void     l2c_fcr_adj_monitor_retran_timeout (tL2C_CCB *p_ccb);
extern void     l2c_fcr_stop_timer (tL2C_CCB *p_ccb);

/* Functions provided by l2c_ble.c
************************************
*/
#if (BLE_INCLUDED == TRUE)
extern BOOLEAN l2cble_create_conn (tL2C_LCB *p_lcb);
extern void l2cble_process_sig_cmd (tL2C_LCB *p_lcb, UINT8 *p, UINT16 pkt_len);
extern void l2cble_conn_comp (UINT16 handle, UINT8 role, BD_ADDR bda, tBLE_ADDR_TYPE type,
                              UINT16 conn_interval, UINT16 conn_latency, UINT16 conn_timeout);

#endif

#ifdef __cplusplus
}
#endif

#endif
