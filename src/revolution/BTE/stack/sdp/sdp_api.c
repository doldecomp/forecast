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
 *  this file contains SDP interface functions
 *
 ******************************************************************************/

#include <cstring>

#include "bt_target.h"
#include "gki.h"
#include "l2cdefs.h"
#include "hcidefs.h"
#include "hcimsgs.h"

#include "sdp_api.h"
#include "sdpint.h"
#include "btu.h"



/**********************************************************************
**   C L I E N T    F U N C T I O N    P R O T O T Y P E S            *
***********************************************************************/

/*******************************************************************************
**
** Function         SDP_InitDiscoveryDb
**
** Description      This function is called to initialize a discovery database.
**
** Parameters:      p_db        - (input) address of an area of memory where the
**                                        discovery database is managed.
**                  len         - (input) size (in bytes) of the memory
**                                  NOTE: This must be larger than sizeof(tSDP_DISCOVERY_DB)
**                  num_uuid    - (input) number of UUID filters applied
**                  p_uuid_list - (input) list of UUID filters
**                  num_attr    - (input) number of attribute filters applied
**                  p_attr_list - (input) list of attribute filters
**
**
** Returns          BOOLEAN
**                          TRUE if successful
**                          FALSE if one or more parameters are bad
**
*******************************************************************************/
BOOLEAN SDP_InitDiscoveryDb (tSDP_DISCOVERY_DB *p_db, UINT32 len, UINT16 num_uuid,
                             tSDP_UUID *p_uuid_list, UINT16 num_attr, UINT16 *p_attr_list)
{
#if SDP_CLIENT_ENABLED == TRUE
    UINT16  xx;

    /* verify the parameters */
    if (p_db == NULL || (sizeof (tSDP_DISCOVERY_DB) > len) ||
        num_attr > SDP_MAX_ATTR_FILTERS || num_uuid > SDP_MAX_UUID_FILTERS)
    {
        /* The retail .data pool pads this string by one byte. */
        SDP_TRACE_ERROR4("SDP_InitDiscoveryDb Illegal param: p_db 0x%x, len %d, num_uuid %d, num_attr %d\0",
                         (UINT32)p_db, len, num_uuid, num_attr);

        return(FALSE);
    }

    memset (p_db, 0, (size_t)len);

    p_db->mem_size = len - sizeof (tSDP_DISCOVERY_DB);
    p_db->mem_free = p_db->mem_size;
    p_db->p_first_rec = NULL;
    p_db->p_free_mem = (UINT8 *)(p_db + 1);

    for (xx = 0; xx < num_uuid; xx++)
        p_db->uuid_filters[xx] = *p_uuid_list++;

    p_db->num_uuid_filters = num_uuid;

    for (xx = 0; xx < num_attr; xx++)
        p_db->attr_filters[xx] = *p_attr_list++;

    /* sort attributes */
    sdpu_sort_attr_list( num_attr, p_db );

    p_db->num_attr_filters = num_attr;
#endif
    return(TRUE);
}






/*******************************************************************************
**
** Function         SDP_ServiceSearchRequest
**
** Description      This function queries an SDP server for information.
**
** Returns          TRUE if discovery started, FALSE if failed.
**
*******************************************************************************/
BOOLEAN SDP_ServiceSearchRequest (UINT8 *p_bd_addr, tSDP_DISCOVERY_DB *p_db,
                                  tSDP_DISC_CMPL_CB *p_cb)
{
#if SDP_CLIENT_ENABLED == TRUE
    tCONN_CB     *p_ccb;

    /* Specific BD address */
    p_ccb = sdp_conn_originate (p_bd_addr);

    if (!p_ccb)
        return(FALSE);

    p_ccb->disc_state = SDP_DISC_WAIT_CONN;
    p_ccb->p_db       = p_db;
    p_ccb->p_cb       = p_cb;

    return(TRUE);
#else
    return(FALSE);
#endif
}


/*******************************************************************************
**
** Function         SDP_ServiceSearchAttributeRequest
**
** Description      This function queries an SDP server for information.
**
**                  The difference between this API function and the function
**                  SDP_ServiceSearchRequest is that this one does a
**                  combined ServiceSearchAttributeRequest SDP function.
**                  (This is for Unplug Testing)
**
** Returns          TRUE if discovery started, FALSE if failed.
**
*******************************************************************************/
BOOLEAN SDP_ServiceSearchAttributeRequest (UINT8 *p_bd_addr, tSDP_DISCOVERY_DB *p_db,
                                           tSDP_DISC_CMPL_CB *p_cb)
{
#if SDP_CLIENT_ENABLED == TRUE
    tCONN_CB     *p_ccb;

    /* Specific BD address */
    p_ccb = sdp_conn_originate (p_bd_addr);

    if (!p_ccb)
        return(FALSE);

    p_ccb->disc_state = SDP_DISC_WAIT_CONN;
    p_ccb->p_db       = p_db;
    p_ccb->p_cb       = p_cb;

    p_ccb->is_attr_search = TRUE;

    return(TRUE);
#else
    return(FALSE);
#endif
}



/*******************************************************************************
**
** Function         SDP_FindAttributeInRec
**
** Description      This function searches an SDP discovery record for a specific
**                  attribute.
**
** Returns          Pointer to matching attribute entry, or NULL
**
*******************************************************************************/
tSDP_DISC_ATTR *SDP_FindAttributeInRec (tSDP_DISC_REC *p_rec, UINT16 attr_id)
{
#if SDP_CLIENT_ENABLED == TRUE
    tSDP_DISC_ATTR  *p_attr;

    p_attr = p_rec->p_first_attr;
    while (p_attr)
    {
        if (p_attr->attr_id == attr_id)
            return(p_attr);

        p_attr = p_attr->p_next_attr;
    }
#endif
    /* If here, no matching attribute found */
    return(NULL);
}


/*******************************************************************************
**
** Function         SDP_FindServiceInDb
**
** Description      This function queries an SDP database for a specific service.
**                  If the p_start_rec pointer is NULL, it looks from the beginning
**                  of the database, else it continues from the next record after
**                  p_start_rec.
**
** Returns          Pointer to record containing service class, or NULL
**
*******************************************************************************/
tSDP_DISC_REC *SDP_FindServiceInDb (tSDP_DISCOVERY_DB *p_db, UINT16 service_uuid, tSDP_DISC_REC *p_start_rec)
{
#if SDP_CLIENT_ENABLED == TRUE
    /* RVL version: no "any UUID" (service_uuid == 0) match, no HDP or
       Toyota G Block workarounds and no debug printfs. */
    tSDP_DISC_REC   *p_rec;
    tSDP_DISC_ATTR  *p_attr, *p_sattr;

    if (!p_start_rec)
        p_rec = p_db->p_first_rec;
    else
        p_rec = p_start_rec->p_next_rec;

    while (p_rec)
    {
        p_attr = p_rec->p_first_attr;
        while (p_attr)
        {
            if ((p_attr->attr_id == ATTR_ID_SERVICE_CLASS_ID_LIST)
                && (SDP_DISC_ATTR_TYPE(p_attr->attr_len_type) == DATA_ELE_SEQ_DESC_TYPE))
            {
                for (p_sattr = p_attr->attr_value.v.p_sub_attr; p_sattr; p_sattr = p_sattr->p_next_attr)
                {
                    if ((SDP_DISC_ATTR_TYPE(p_sattr->attr_len_type) == UUID_DESC_TYPE)
                        && (SDP_DISC_ATTR_LEN(p_sattr->attr_len_type) == 2)
                        && (p_sattr->attr_value.v.u16 == service_uuid))
                    {
                        return(p_rec);
                    }
                }
                break;
            }
            else if (p_attr->attr_id == ATTR_ID_SERVICE_ID)
            {
                if ((SDP_DISC_ATTR_TYPE(p_attr->attr_len_type) == UUID_DESC_TYPE)
                    && (SDP_DISC_ATTR_LEN(p_attr->attr_len_type) == 2)
                    && (p_attr->attr_value.v.u16 == service_uuid))
                    return(p_rec);
            }

            p_attr = p_attr->p_next_attr;
        }

        p_rec = p_rec->p_next_rec;
    }
#endif
    /* If here, no matching UUID found */
    return(NULL);
}



/*******************************************************************************
**
** Function         SDP_FindServiceUUIDInDb
**
** Description      This function queries an SDP database for a specific service.
**                  If the p_start_rec pointer is NULL, it looks from the beginning
**                  of the database, else it continues from the next record after
**                  p_start_rec.
**
** NOTE             the only difference between this function and the previous function
**                  "SDP_FindServiceInDb()" is that this function takes a tBT_UUID input
**
** Returns          Pointer to record containing service class, or NULL
**
*******************************************************************************/
tSDP_DISC_REC *SDP_FindServiceUUIDInDb (tSDP_DISCOVERY_DB *p_db, tBT_UUID *p_uuid, tSDP_DISC_REC *p_start_rec)
{
#if SDP_CLIENT_ENABLED == TRUE
    tSDP_DISC_REC   *p_rec;
    tSDP_DISC_ATTR  *p_attr, *p_sattr;


    if (!p_start_rec)
        p_rec = p_db->p_first_rec;
    else
        p_rec = p_start_rec->p_next_rec;

    while (p_rec)
    {
        p_attr = p_rec->p_first_attr;
        while (p_attr)
        {
            if ((p_attr->attr_id == ATTR_ID_SERVICE_CLASS_ID_LIST)
                && (SDP_DISC_ATTR_TYPE(p_attr->attr_len_type) == DATA_ELE_SEQ_DESC_TYPE))
            {
                for (p_sattr = p_attr->attr_value.v.p_sub_attr; p_sattr; p_sattr = p_sattr->p_next_attr)
                {
                    if (SDP_DISC_ATTR_TYPE(p_sattr->attr_len_type) == UUID_DESC_TYPE)
                    {
                        if (sdpu_compare_uuid_with_attr (p_uuid, p_sattr))
                            return(p_rec);
                    }
                }
                break;
            }
            else if (p_attr->attr_id == ATTR_ID_SERVICE_ID)
            {
                if (SDP_DISC_ATTR_TYPE(p_attr->attr_len_type) == UUID_DESC_TYPE )
                {
                    if (sdpu_compare_uuid_with_attr (p_uuid, p_attr))
                        return(p_rec);
                }
            }

            p_attr = p_attr->p_next_attr;
        }

        p_rec = p_rec->p_next_rec;
    }
#endif  /* CLIENT_ENABLED == TRUE */
    /* If here, no matching UUID found */
    return(NULL);
}


/*******************************************************************************
**                   Device Identification (DI) Server Functions
*******************************************************************************/

/*******************************************************************************
**
** Function         SDP_SetLocalDiRecord
**
** Description      This function adds a DI record to the local SDP database.
**
**
**
** Returns          Returns SDP_SUCCESS if record added successfully, else error
**
*******************************************************************************/
UINT16 SDP_SetLocalDiRecord( tSDP_DI_RECORD *p_device_info, UINT32 *p_handle )
{
#if SDP_SERVER_ENABLED == TRUE
    UINT16  result = SDP_SUCCESS;
    UINT32  handle;
    UINT16  di_uuid = UUID_SERVCLASS_PNP_INFORMATION;
    UINT16  di_specid = BLUETOOTH_DI_SPECIFICATION;
    UINT8   temp_u16[2];
    UINT8   *p_temp;

    *p_handle = 0;
    if ( p_device_info == NULL )
        return SDP_ILLEGAL_PARAMETER;

    /* if record is to be primary record, get handle to replace old primary */
    if ( p_device_info->primary_record == TRUE && sdp_cb.server_db.di_primary_handle )
        handle = sdp_cb.server_db.di_primary_handle;
    else
    {
        if ( (handle = SDP_CreateRecord()) == 0 )
            return SDP_NO_RESOURCES;
    }

    *p_handle = handle;

    /* build the SDP entry */
    /* Add the UUID to the Service Class ID List */
    if ((SDP_AddServiceClassIdList(handle, 1, &di_uuid)) == FALSE)
        result = SDP_DI_REG_FAILED;

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        p_temp = temp_u16;
        UINT16_TO_BE_STREAM(p_temp, di_specid);
        if ( !(SDP_AddAttribute(handle, ATTR_ID_SPECIFICATION_ID,
                                UINT_DESC_TYPE, sizeof(di_specid),
                                temp_u16)) )
            result = SDP_DI_REG_FAILED;
    }

    /* optional - if string is null, do not add attribute */
    if ( result == SDP_SUCCESS )
    {
        if ( p_device_info->client_executable_url[0] != '\0' )
        {
            if ( !((strlen(p_device_info->client_executable_url)+1 <= SDP_MAX_ATTR_LEN) &&
                   SDP_AddAttribute(handle, ATTR_ID_CLIENT_EXE_URL, URL_DESC_TYPE,
                                    (UINT32)(strlen(p_device_info->client_executable_url)+1),
                                    (UINT8 *)p_device_info->client_executable_url)) )
                result = SDP_DI_REG_FAILED;
        }
    }

    /* optional - if string is null, do not add attribute */
    if ( result == SDP_SUCCESS )
    {
        if ( p_device_info->service_description[0] != '\0' )
        {
            if ( !((strlen(p_device_info->service_description)+1 <= SDP_MAX_ATTR_LEN) &&
                   SDP_AddAttribute(handle, ATTR_ID_SERVICE_DESCRIPTION,
                                    TEXT_STR_DESC_TYPE,
                                    (UINT32)(strlen(p_device_info->service_description)+1),
                                    (UINT8 *)p_device_info->service_description)) )
                result = SDP_DI_REG_FAILED;
        }
    }

    /* optional - if string is null, do not add attribute */
    if ( result == SDP_SUCCESS )
    {
        if ( p_device_info->documentation_url[0] != '\0' )
        {
            if ( !((strlen(p_device_info->documentation_url)+1 <= SDP_MAX_ATTR_LEN) &&
                   SDP_AddAttribute(handle, ATTR_ID_DOCUMENTATION_URL, URL_DESC_TYPE,
                                    (UINT32)(strlen(p_device_info->documentation_url)+1),
                                    (UINT8 *)p_device_info->documentation_url)) )
                result = SDP_DI_REG_FAILED;
        }
    }

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        p_temp = temp_u16;
        UINT16_TO_BE_STREAM(p_temp, p_device_info->vendor);
        if ( !(SDP_AddAttribute(handle, ATTR_ID_VENDOR_ID, UINT_DESC_TYPE,
                                sizeof(p_device_info->vendor), temp_u16)) )
            result = SDP_DI_REG_FAILED;
    }

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        p_temp = temp_u16;
        UINT16_TO_BE_STREAM (p_temp, p_device_info->product);
        if ( !(SDP_AddAttribute(handle, ATTR_ID_PRODUCT_ID,
                                UINT_DESC_TYPE, sizeof(p_device_info->product), temp_u16)) )
            result = SDP_DI_REG_FAILED;
    }

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        p_temp = temp_u16;
        UINT16_TO_BE_STREAM (p_temp, p_device_info->version);
        if ( !(SDP_AddAttribute(handle, ATTR_ID_PRODUCT_VERSION, UINT_DESC_TYPE,
                                sizeof(p_device_info->version), temp_u16)) )
            result = SDP_DI_REG_FAILED;
    }

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        if ( !(SDP_AddAttribute(handle, ATTR_ID_PRIMARY_RECORD,
                                BOOLEAN_DESC_TYPE, 1, &p_device_info->primary_record)) )
            result = SDP_DI_REG_FAILED;
    }

    /* mandatory */
    if ( result == SDP_SUCCESS)
    {
        p_temp = temp_u16;
        UINT16_TO_BE_STREAM(p_temp, p_device_info->vendor_id_source);
        if ( !(SDP_AddAttribute(handle, ATTR_ID_VENDOR_ID_SOURCE, UINT_DESC_TYPE,
                                sizeof(p_device_info->vendor_id_source), temp_u16)) )
            result = SDP_DI_REG_FAILED;
    }

    if ( result != SDP_SUCCESS )
        SDP_DeleteRecord( handle );
    else if (p_device_info->primary_record == TRUE)
        sdp_cb.server_db.di_primary_handle = handle;

    return result;
#else   /* SDP_SERVER_ENABLED is FALSE */
    return SDP_DI_REG_FAILED;
#endif  /* if SDP_SERVER_ENABLED */
}

/*******************************************************************************
**
** Function         SDP_GetLocalDiRecord
**
** Description      This function adds a DI record to the local SDP database.
**
**                  Fills in the device information of the record
**                  p_handle - if p_handle == 0, the primary record is returned
**
** Returns          Returns SDP_SUCCESS if record exists, else error
**
*******************************************************************************/
UINT16 SDP_GetLocalDiRecord(tSDP_DI_GET_RECORD *p_device_info, UINT32 *p_handle )
{
    UINT16 result = SDP_NO_DI_RECORD_FOUND;

#if SDP_SERVER_ENABLED == TRUE
    tSDP_RECORD     *p_rec;
    tSDP_ATTRIBUTE  *p_attr;
    UINT8           *p_temp;
    INT32            templen;

    if (*p_handle == 0)
        *p_handle = sdp_cb.server_db.di_primary_handle;

    if ((p_rec = sdp_db_find_record(*p_handle)) != NULL)
    {
        memset(p_device_info, 0, sizeof(tSDP_DI_RECORD));

        result = SDP_SUCCESS;

        /* Retrieve the Specification ID */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_SPECIFICATION_ID,
                                              ATTR_ID_SPECIFICATION_ID)) != NULL)
        {
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_UINT16 (p_device_info->spec_id, p_temp);
        }

        /* Retrieve the Vendor ID */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_VENDOR_ID,
                                              ATTR_ID_VENDOR_ID)) != NULL)
        {
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_UINT16 (p_device_info->rec.vendor, p_temp);
        }

        /* Retrieve the Product ID */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_PRODUCT_ID,
                                              ATTR_ID_PRODUCT_ID)) != NULL)
        {
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_UINT16 (p_device_info->rec.product, p_temp);
        }

        /* Retrieve the Version ID */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_PRODUCT_VERSION,
                                              ATTR_ID_PRODUCT_VERSION)) != NULL)
        {
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_UINT16 (p_device_info->rec.version, p_temp);
        }

        /* Retrieve the Vendor ID Source ID */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_VENDOR_ID_SOURCE,
                                              ATTR_ID_VENDOR_ID_SOURCE)) != NULL)
        {
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_UINT16 (p_device_info->rec.vendor_id_source, p_temp);
        }

        /* Retrieve the Primary Record */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_PRIMARY_RECORD,
                                              ATTR_ID_PRIMARY_RECORD)) != NULL)
        {
            p_device_info->rec.primary_record = *p_attr->value_ptr;
        }

        /* Retrieve the Client Executable URL */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_CLIENT_EXE_URL,
                                              ATTR_ID_CLIENT_EXE_URL)) != NULL)
        {
            templen = (INT32)((p_attr->len < SDP_MAX_ATTR_LEN) ? p_attr->len : SDP_MAX_ATTR_LEN);
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_ARRAY (p_temp, p_device_info->rec.client_executable_url, templen);
        }

        /* Retrieve the Service Description */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_SERVICE_DESCRIPTION,
                                              ATTR_ID_SERVICE_DESCRIPTION)) != NULL)
        {
            templen = (INT32)((p_attr->len < SDP_MAX_ATTR_LEN) ? p_attr->len : SDP_MAX_ATTR_LEN);
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_ARRAY (p_temp, p_device_info->rec.service_description, templen);
        }

        /* Retrieve the Documentation URL */
        if ((p_attr = sdp_db_find_attr_in_rec(p_rec, ATTR_ID_DOCUMENTATION_URL,
                                              ATTR_ID_DOCUMENTATION_URL)) != NULL)
        {
            templen = (INT32)((p_attr->len < SDP_MAX_ATTR_LEN) ? p_attr->len : SDP_MAX_ATTR_LEN);
            p_temp = p_attr->value_ptr;
            BE_STREAM_TO_ARRAY (p_temp, p_device_info->rec.documentation_url, templen);
        }
    }
    else
        *p_handle = 0;
#endif

    return result;
}


/*******************************************************************************
**
** Function         SDP_SetTraceLevel
**
** Description      This function sets the trace level for SDP. If called with
**                  a value of 0xFF, it simply reads the current trace level.
**
** Returns          the new (current) trace level
**
*******************************************************************************/
UINT8 SDP_SetTraceLevel (UINT8 new_level)
{
    if (new_level != 0xFF)
        sdp_cb.trace_level = new_level;

    return(sdp_cb.trace_level);
}

#if SDP_FOR_JV_INCLUDED == TRUE
/*******************************************************************************
**
** Function         SDP_ConnOpen
**
** Description      This function creates a connection to the SDP server on the
**                  given device.
**
** Returns          0, if failed to initiate connection. Otherwise, the handle.
**
*******************************************************************************/
UINT32 SDP_ConnOpen (UINT8 *p_bd_addr, tSDP_DISC_RES_CB *p_rcb,
                     tSDP_DISC_CMPL_CB *p_cb)
{
#if SDP_CLIENT_ENABLED == TRUE
    tCONN_CB    *p_ccb;
    UINT32      idx = 0;

    if (!p_cb || !p_rcb)
        return(idx);

    /* Specific BD address */
    p_ccb = sdp_conn_originate (p_bd_addr);

    if (!p_ccb)
        return(idx);

    p_ccb->disc_state = SDP_DISC_WAIT_CONN;
    p_ccb->p_db       = (tSDP_DISCOVERY_DB *)p_rcb;
    p_ccb->p_cb       = p_cb;

    p_ccb->is_attr_search = SDP_IS_PASS_THRU;

    idx = (UINT32)(p_ccb - sdp_cb.ccb);
    return(UINT32)(idx + 1);
#else
    return(0);
#endif
}

/*******************************************************************************
**
** Function         SDP_WriteData
**
** Description      This function sends data to the connected SDP server.
**
** Returns          TRUE if data is sent, FALSE if failed.
**
*******************************************************************************/
BOOLEAN SDP_WriteData (UINT32 handle, BT_HDR  *p_msg)
{
#if SDP_CLIENT_ENABLED == TRUE
    tCONN_CB    *p_ccb = NULL;

    if (p_msg && (handle > 0) && (handle <= SDP_MAX_CONNECTIONS) )
    {
        p_ccb = &sdp_cb.ccb[handle - 1];
        if ( (p_ccb->con_state == SDP_STATE_CONNECTED) &&
             (p_ccb->con_flags & SDP_FLAGS_IS_ORIG) )
        {
            /* Start inactivity timer */
            btu_start_timer (&p_ccb->timer_entry, BTU_TTYPE_SDP, SDP_INACT_TIMEOUT);
            L2CA_DataWrite (p_ccb->connection_id, p_msg);
            return TRUE;
        }
    }
#endif
    return FALSE;
}

/*******************************************************************************
**
** Function         SDP_ConnClose
**
** Description      This function is called to close a SDP connection.
**
** Parameters:      handle      - Handle of the connection returned by SDP_ConnOpen
**
** Returns          TRUE if connection is closed, FALSE if failed to find the handle.
**
*******************************************************************************/
BOOLEAN SDP_ConnClose (UINT32 handle)
{
#if SDP_CLIENT_ENABLED == TRUE
    tCONN_CB    *p_ccb = NULL;

    if (handle > 0 && handle <= SDP_MAX_CONNECTIONS)
    {
        p_ccb = &sdp_cb.ccb[handle - 1];
        sdp_disconnect (p_ccb, SDP_SUCCESS);
        return TRUE;
    }
#endif
    return FALSE;
}
#endif
