// RDD 031220 Add New File for Qwikcilver GiftCard Processing 
#include <incs.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "cJSON_local.h"


//#ifndef _WIN32
// CR_RESP_FILE_PATH "flash/Prov_Resp.json"
//#else
#define CR_REQ_FILE_PATH VIM_FILE_PATH_LOCAL "Prov_Req.json"
#define CR_RESP_FILE_PATH VIM_FILE_PATH_LOCAL "Prov_Resp.json"
//#endif

//extern const char *SecurityToken;
extern VIM_ERROR_PTR swap_terminal_mode(void);


static VIM_CHAR *GetAsciiPPID(void)
{
	VIM_DES_BLOCK ppid;
	static VIM_CHAR szPPID[16 + 1];

	// PPID        
	vim_mem_clear(szPPID, VIM_SIZEOF(szPPID));
	vim_tcu_get_ppid(tcu_handle.instance_ptr, &ppid);
	bcd_to_asc(ppid.bytes, szPPID, 2 * VIM_SIZEOF(VIM_DES_BLOCK));
	return szPPID;
}


static cJSON* CR_WaitForResponse(void)
{
    cJSON* pCJResponse = NULL;
    VIM_UINT32 ulTimeoutS = 20;     // RDD - TODO - make this configurable from ????
    pCJResponse = VAA_WaitForJSONResponse(CR_RESP_FILE_PATH, ulTimeoutS, VIM_FALSE);    
    return pCJResponse;
}


static VIM_ERROR_PTR CR_ParseAS2805( const cJSON *pCJResp, char *pszAS2805Response, int iRespBuffSize )
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;

    if ( pCJResp && pszAS2805Response )
    {
        if ( cJSON_GetStringItemValue( pCJResp, "AS2805Rsp", pszAS2805Response, iRespBuffSize ) )
        {
            VIM_DBG_PRINTF1( "Got AS2805 Response. Len=%d", vim_strlen(pszAS2805Response));
        }
        else
        {
            DBG_MESSAGE( "Error: AS2805 Response not found" );
            eRet = VIM_ERROR_SYSTEM;
        }
    }
    else
    {
        eRet = VIM_ERROR_NULL;
    }
    return eRet;
}


static VIM_ERROR_PTR CR_ParseSecurityToken(const cJSON *pCJResp, char *pszSecToken, int iRespBuffSize)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;

    if (pCJResp && pszSecToken)
    {
        if (cJSON_GetStringItemValue(pCJResp, "SecToken", pszSecToken, iRespBuffSize))
        {
            VIM_DBG_PRINTF1("Got SecToken. Len=%d", vim_strlen(pszSecToken));
        }
        else
        {
            DBG_MESSAGE("SecToken not found");
            eRet = VIM_ERROR_SYSTEM;
        }
    }
    else
    {
        eRet = VIM_ERROR_NULL;
    }
    return eRet;
}




static VIM_ERROR_PTR CR_CreateSendCrAck(cJSON *pCJFileRequest, cJSON *psCJ_CR1, VIM_CHAR *SecToken, VIM_CHAR *Label)
{
    VIM_BOOL fSuccess = VIM_TRUE;
    VIM_CHAR szTID[8 + 1];
    VIM_DES_BLOCK ppid;
    VIM_CHAR szPPID[16 + 1];

    // Terminal ID        
    vim_mem_clear(szTID, VIM_SIZEOF(szTID));
    vim_mem_copy(szTID, terminal.terminal_id, 8);
    fSuccess &= VAA_AddString(psCJ_CR1, "TID", szTID, VIM_TRUE);

    // PPID        
    vim_mem_clear(szPPID, VIM_SIZEOF(szPPID));
    vim_tcu_get_ppid(tcu_handle.instance_ptr, &ppid);
    bcd_to_asc(ppid.bytes, szPPID, 2 * VIM_SIZEOF(VIM_DES_BLOCK));
    fSuccess &= VAA_AddString(psCJ_CR1, "PPID", szPPID, VIM_TRUE);
    DBG_VAR(fSuccess);

    // SecToken        
    fSuccess &= VAA_AddString(psCJ_CR1, "SecToken", SecToken, VIM_FALSE);

    DBG_VAR(fSuccess);

    DBG_STRING(Label);
    // Add the container to the file
    fSuccess &= VAA_AddItem(pCJFileRequest, Label, psCJ_CR1, VIM_TRUE);

    DBG_VAR(fSuccess);

    vim_file_delete(CR_REQ_FILE_PATH);

    if (fSuccess)
    {
        DBG_MESSAGE("Writing CR Ack API JSON file");
        if (!VAA_WriteJSONRequestFile(CR_REQ_FILE_PATH, pCJFileRequest))
        {
            DBG_MESSAGE("!!!!! Write Failed !!!!!");
            return VIM_ERROR_SYSTEM;
        }
        VAA_CheckDebug();
    }
    else
    {
        DBG_MESSAGE("!!!!! Create Failed !!!!!");
    }
    VAA_CheckDebug();
    return VIM_ERROR_NONE;
}




static VIM_ERROR_PTR CR_CreateSendCRx(cJSON *pCJFileRequest, cJSON *psCJ_CR1, VIM_CHAR *Config_OTP, VIM_AS2805_MESSAGE *msg_ptr, VIM_CHAR *Label )
{
    VIM_BOOL fSuccess = VIM_TRUE;
    VIM_CHAR szAS2805_MsgAsc[1000];
    VIM_CHAR szTID[8 + 1];
    VIM_DES_BLOCK ppid;
    VIM_CHAR szPPID[16 + 1];
    transaction_t transaction;
    transaction_t *dummy_txn_ptr = &transaction;

    txn_do_init(dummy_txn_ptr);

    // Terminal ID        
    vim_mem_clear(szTID, VIM_SIZEOF(szTID));
    vim_mem_copy(szTID, terminal.terminal_id, 8);
    fSuccess &= VAA_AddString(psCJ_CR1, "TID", szTID, VIM_TRUE);

    // PPID        
    vim_mem_clear(szPPID, VIM_SIZEOF(szPPID));
    vim_tcu_get_ppid(tcu_handle.instance_ptr, &ppid);
    bcd_to_asc(ppid.bytes, szPPID, 2 * VIM_SIZEOF(VIM_DES_BLOCK));
    fSuccess &= VAA_AddString(psCJ_CR1, "PPID", szPPID, VIM_TRUE);

    // OTP   
	VIM_DBG_STRING(Config_OTP);
    fSuccess &= VAA_AddString(psCJ_CR1, "OTP", Config_OTP, VIM_FALSE);

    // RDD 161121 missing HOST_ID from new terminals until config_menrchant run.
    if( terminal.terminal_id[7] %2 )
    {
        vim_mem_copy(terminal.host_id, WOW_HOST1, vim_strlen(WOW_HOST1));
    }    
    else     
    {    
        vim_mem_copy(terminal.host_id, WOW_HOST2, vim_strlen(WOW_HOST2));        
    }
    fSuccess &= VAA_AddString(psCJ_CR1, "HostID", terminal.host_id, VIM_TRUE);

    // AS2805Req  
    vim_mem_clear(szAS2805_MsgAsc, VIM_SIZEOF(szAS2805_MsgAsc));
    hex_to_asc(msg_ptr->message, szAS2805_MsgAsc, (msg_ptr->msg_size * 2));
    fSuccess &= VAA_AddString(psCJ_CR1, "AS2805Req", szAS2805_MsgAsc, VIM_TRUE);

    // Add the container to the file
    fSuccess &= VAA_AddItem(pCJFileRequest, Label, psCJ_CR1, VIM_TRUE);

    vim_file_delete(CR_REQ_FILE_PATH);

    if (fSuccess)
    {
        DBG_MESSAGE("Writing QC Auth Challenge request file");
        if (!VAA_WriteJSONRequestFile(CR_REQ_FILE_PATH, pCJFileRequest))
        {
            DBG_MESSAGE("!!!!! Write Failed !!!!!");
            return VIM_ERROR_SYSTEM;
        }
    }
    return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR CR_ParseCRx( cJSON *pCJFileResponse, VIM_AS2805_MESSAGE *msg_ptr )
{
    VIM_ERROR_PTR err;
    VIM_CHAR szAS2805_MsgAsc[1000];

    transaction_t *dummy_txn_ptr = &txn;

    txn_do_init(dummy_txn_ptr);

    vim_mem_clear(szAS2805_MsgAsc, VIM_SIZEOF(szAS2805_MsgAsc));
    if ((err = CR_ParseAS2805(pCJFileResponse, szAS2805_MsgAsc, sizeof(szAS2805_MsgAsc))) != VIM_ERROR_NONE)
    {
        DBG_Z0("Failed to Extract As2805 Msg from json resp. VIM_ERROR = %s", err->name);
        VIM_ERROR_RETURN(err);
    }
    else
    {
        asc_to_hex(szAS2805_MsgAsc, msg_ptr->message, vim_strlen(szAS2805_MsgAsc));
        msg_ptr->msg_size = vim_strlen(szAS2805_MsgAsc) / 2;
        if ((err = msgAS2805_parse(HOST_WOW, msg_ptr->msg_type, msg_ptr, dummy_txn_ptr)) == VIM_ERROR_NONE)
        {
            if( vim_strcmp(dummy_txn_ptr->host_rc_asc, "00"))
            {
                DBG_Z0("AS2805 Msg type:%d Declined: %2.2s", msg_ptr->msg_type, dummy_txn_ptr->host_rc_asc );
                err = ERR_WOW_ERC;
            }
            else
            {
                DBG_Z0_TEST("AS2805 Msg Approved: %2.2s", dummy_txn_ptr->host_rc_asc);
            }
        }
        else
        {
            DBG_Z0_TEST("Parse AS2805 Failed. Size = %d", msg_ptr->msg_size);
        }
    }
    return err;
}


static VIM_ERROR_PTR CR_Parse_HTTP_RC(cJSON *pCJFileResponse)
{
    VIM_CHAR szResponse[3 + 1] = { 0,0,0,0 };   // HTTP response in ASCII
    VIM_ERROR_PTR err = VIM_ERROR_SYSTEM;
    VIM_SIZE http_resp = 0;

    VIM_DBG_STRING("Parse HTTP response");

    // Extract the HTTP response - thats all we get ....
    if (cJSON_GetStringItemValue(pCJFileResponse, "HTTPResp", szResponse, VIM_SIZEOF(szResponse)))
    {
        VIM_DBG_PRINTF1("Got Cr ACK Response. HTTPResp = %s ", szResponse );
        if (vim_utl_ascii_decimal_to_size(szResponse, 3, &http_resp) == VIM_ERROR_NONE)
        {
            DBG_Z0("Response File Contained HTTP Resp: %3.3d", http_resp);
            if ( http_resp < 300 )      // Any 2xx code is fine
            {
                err = VIM_ERROR_NONE;
            }
            else
            {
                VIM_CHAR TextCode[20] = { 0 };
                vim_sprintf(TextCode, "Error Code %3.3d", http_resp);
                    
                switch (http_resp)
                {
                    case 400:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "Bad Request");
                        break;
                    case 409:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "PPID Already Processed");
                        err = VIM_ERROR_ALREADY_INTIALIZED;
                        break;
                    case 401:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "Access Denied");
                        break;
                    case 404:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "OTP Mismatch\nPlease Try Again");
                        break;
                    case 500:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "Remote Server Busy\nPlease Try Again");
                        break;
                    case 503:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "Service Unavailable\nCall Helpdesk");
                        break;
                    default:
                        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, TextCode, "Remote Error (unknown)");
                        break;                
                }
                vim_event_sleep(2000);
            }
        }
        else
        {
            DBG_Z0( "System Error - can't parse HTTP resp" );
        }
    }
    else
    {
        DBG_MESSAGE("Error: HTTPResp Response not found");
    }
    VIM_ERROR_RETURN( err );
}






static VIM_ERROR_PTR CR_ProcessCRack(VIM_CHAR *SecToken, VIM_CHAR *Label, cJSON **pCJFileResponse)
{
    VIM_ERROR_PTR err;
    cJSON *pCJFileRequest = VIM_NULL;
    cJSON *psCJ_CR1 = VIM_NULL;

    if ((pCJFileRequest = cJSON_CreateObject()) == VIM_NULL)
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);

    if ((psCJ_CR1 = cJSON_CreateObject()) == VIM_NULL)
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);

    if ((err = CR_CreateSendCrAck( pCJFileRequest, psCJ_CR1, SecToken, Label )) == VIM_ERROR_NONE)
    {
        DBG_Z0_TEST(" -- Created Cr ACK -- ");
        VAA_CheckDebug();

        if ((*pCJFileResponse = CR_WaitForResponse()) != VIM_NULL)
        {
            if ((err = CR_Parse_HTTP_RC( *pCJFileResponse )) == VIM_ERROR_NONE)
            {
                DBG_Z0_TEST("Parsed CR Ack OK");
                terminal_set_status(ss_tid_configured);
                terminal_clear_status(ss_tms_configured);

                err = VIM_ERROR_NONE;
            }
            else
            {
                DBG_Z0_TEST("Failed to Parse CR Ack. VIM_ERROR = %s", err->name);
            }
        }
        else
        {
            err = VIM_ERROR_TIMEOUT;
        }
    }
    return err;
}


static VIM_ERROR_PTR CR_Process_AS2805( VIM_CHAR *Config_OTP, VIM_AS2805_MESSAGE_TYPE msg_type, VIM_CHAR *Label, cJSON **pCJFileResponse )
{
    VIM_ERROR_PTR err = VIM_ERROR_SYSTEM;
    VIM_AS2805_MESSAGE msg = { 0 };
    cJSON *pCJFileRequest = VIM_NULL;
    cJSON *psCJ_CR1 = VIM_NULL;

    if ((pCJFileRequest = cJSON_CreateObject()) == VIM_NULL)
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);

    if ((psCJ_CR1 = cJSON_CreateObject()) != VIM_NULL)
    {
        vim_mem_clear(msg.message, VIM_SIZEOF(msg.message));
#ifndef _WIN32
        vim_file_delete(CR_RESP_FILE_PATH);
#endif
        if ((err = msgAS2805_create(HOST_WOW, &msg, msg_type)) != VIM_ERROR_NONE)
        {
            DBG_Z0("Failed to Create AS2805 Msg. VIM_ERROR = %s", err->name);
        }
        else if ((err = CR_CreateSendCRx(pCJFileRequest, psCJ_CR1, Config_OTP, &msg, Label)) == VIM_ERROR_NONE)
        {
            DBG_Z0("Sent AS2805 Msg type %d", msg.msg_type );

            if ((*pCJFileResponse = CR_WaitForResponse()) != VIM_NULL)
            {
                if ((err = CR_Parse_HTTP_RC(*pCJFileResponse)) == VIM_ERROR_NONE)
                {
                    DBG_Z0("HTTP RC = 2xx");
                    if ((err = CR_ParseCRx(*pCJFileResponse, &msg)) == VIM_ERROR_NONE)
                    {
                        DBG_Z0("Parsed AS2805 Msg. Size = %d", msg.msg_size);
                        err = VIM_ERROR_NONE;
                    }
                    else
                    {
                        // RDD 220921 Probably don;t want to delete the token in case they provison sucessfuly and then for some reasomn try again unsucesfully.
                        //vim_file_delete(CR_REQ_TOKEN);
                    }
                }
                else
                {
                    if (err == VIM_ERROR_ALREADY_INTIALIZED)
                    {
                        DBG_Z0("CDS Already Provisioned - Exit NO ERROR");
                    }
                    //vim_file_delete(CR_REQ_TOKEN);
                    DBG_Z0("Failed to Parse AS2805 Msg. VIM_ERROR = %s", err->name );
                }
            }
            else
            {
                err = VIM_ERROR_TIMEOUT;
            }
        }
        else
        {
            DBG_Z0("Failed to Send AS2805 Msg type %d", msg.msg_type);
        }
    }
    //FREE_JSON(psCJ_CR1);
    //FREE_JSON(pCJFileResponse);
    //FREE_JSON(pCJFileRequest);
    VIM_ERROR_RETURN( err );
}




static VIM_ERROR_PTR CR_PerformAuthChallenge(VIM_CHAR *Config_OTP)
{
    VIM_ERROR_PTR err = VIM_ERROR_SYSTEM;
    cJSON *pCJFileResponse = VIM_NULL;
    VIM_CHAR szSecToken[100];
    VIM_CHAR *pszSecToken = szSecToken;
    VIM_SIZE Len = 0;

    vim_mem_clear(szSecToken, VIM_SIZEOF(szSecToken));

    vim_file_get(SECURITY_TOKEN_PATH, pszSecToken, &Len, VIM_SIZEOF(szSecToken));

    // RDD 100323 JIRA PIRPIN-2177 - send renew request if we already have a sec token
    if (Len >= 16 )
    {
        VIM_DBG_NUMBER(Len);
        vim_file_delete(CR_RESP_FILE_PATH);
        if ((err = CR_ProcessCRack(pszSecToken, "Credentials-renew-request", &pCJFileResponse)) == VIM_ERROR_NONE)
        {
            VAA_CheckDebug();
            DBG_Z0("Processed Cr Renew - Database Reset OK");
        }
        else
        {
            VAA_CheckDebug();
            DBG_Z0("Processed Cr Renew - Database Reset Error");
        }
        return err;
    }
    else
    {
        VIM_DBG_MESSAGE("SecToken Missing - run CDS");
    }

    // Process the 191 
    VIM_ERROR_RETURN_ON_FAIL(CR_Process_AS2805(Config_OTP, MESSAGE_WOW_800_NMIC_191_M, "Credentials-request1", &pCJFileResponse));
    VAA_CheckDebug();
    DBG_Z0("Processed 191 - OK !!");

    // Process the 192
    VIM_ERROR_RETURN_ON_FAIL(CR_Process_AS2805(Config_OTP, MESSAGE_WOW_800_NMIC_192, "Credentials-request2", &pCJFileResponse));
    VAA_CheckDebug();
    DBG_Z0("Processed 192 - OK !!");

    if ((err = CR_ParseSecurityToken(pCJFileResponse, pszSecToken, VIM_SIZEOF(szSecToken))) != VIM_ERROR_NONE)
    {
        DBG_Z0("Failed to Extract SecurityToken from json resp. VIM_ERROR = %s", err->name);
        VIM_ERROR_RETURN(err);
    }
    else
    {
        DBG_Z0("Got Security Token - OK !!");
        VIM_DBG_STRING(pszSecToken);

        if(( err = CR_ProcessCRack(pszSecToken, "Credentials-acknowledge", &pCJFileResponse )) == VIM_ERROR_NONE)
        {
            VAA_CheckDebug();
            DBG_Z0( "Processed Cr ACK - Provisioning completed sucessfully" );
            vim_file_set(SECURITY_TOKEN_PATH, pszSecToken, vim_strlen(pszSecToken) );
        }
        else
        {
            DBG_Z0("Failed to Process CR Ack. VIM_ERROR = %s", err->name);
        }
    }
    return err;
}



static VIM_ERROR_PTR config_OTP(void)
{
    VIM_ERROR_PTR res;
    //VIM_SIZE Len = 0;
    kbd_options_t options;
    VIM_CHAR *Ptr = TERM_CONFIG_OTP;

#ifdef _DEBUG
    terminal_set_status(ss_wow_basic_test_mode);
#endif

    // make a copy of the BCD data
    vim_mem_clear(Ptr, VIM_SIZEOF(TERM_CONFIG_OTP));

    options.timeout = ENTRY_TIMEOUT*3;      // Was a bit short 
	// RDD 020522 JIRA PIRPIN-1598 : TID that starts with 'A' should not prompt for OTP. 
	// options.min = OTP_LEN;
	options.min = 0;
	options.max = OTP_LEN;
    options.CheckEvents = VIM_FALSE;
    options.entry_options = KBD_OPT_DISP_ORIG| KBD_OPT_PASSWORD| KBD_OPT_BEEP_ON_CLR;

    if ((res = display_data_entry(Ptr, &options, DISP_PCD_ENTRY, VIM_PCEFTPOS_KEY_CANCEL, "OTP Entry", "", "", "")) == VIM_ERROR_NONE)
    {
        VIM_DBG_STRING(Ptr);
        DisplayProcessing(DISP_PROCESSING_WAIT);
        res = CR_PerformAuthChallenge(Ptr);
    }
    return res;
}



static VIM_BOOL CheckResult(VIM_ERROR_PTR res)
{
    if ((res != VIM_ERROR_SYSTEM )&&( res != ERR_WOW_ERC ))
        return VIM_TRUE;
    else
        return VIM_FALSE;
}

#define DummyMID "6110006000L0000"

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_terminal_id(void)
{
    kbd_options_t options;
    VIM_CHAR date_buf[10];
    //VIM_CHAR init_tid[8 + 1];
    //VIM_TEXT temp_id[VIM_SIZEOF(terminal.terminal_id) + 1];
    VIM_ERROR_PTR res;

    vim_mem_clear(date_buf, sizeof(date_buf));

    vim_mem_copy(date_buf, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id));

    options.timeout = ENTRY_TIMEOUT*10;
    options.min = VIM_SIZEOF(terminal.terminal_id);
    options.max = VIM_SIZEOF(terminal.terminal_id);
    options.entry_options = KBD_OPT_DISP_ORIG;
    options.CheckEvents = VIM_FALSE;

    // RDD 160115 SPIRA:8334 - check SAFs etc. when changing TID from kbd
    //VIM_ERROR_RETURN_ON_FAIL(terminal_check_config_change_allowed(VIM_TRUE));

#ifdef _WIN32
    display_screen(DISP_TERMINAL_CONFIG_TERMINAL_ID, VIM_PCEFTPOS_KEY_NONE);
    if ((res = data_entry(date_buf, &options)) != VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN(res);
    }
#else
    // RDD 201218 Create a generic function to handle display and data entry in all platforms
    if ((res = display_data_entry(date_buf, &options, DISP_TERMINAL_CONFIG_TERMINAL_ID, VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN(res);
    }
    
#endif
    VIM_DBG_STRING(date_buf);

#if 0
	// RDD - TODO - add Alpha entry support in HTML
#ifdef _DEBUG
    if( terminal.terminal_id[0] != 'W' )
    {
       terminal.terminal_id[0] = 'W';
    }
#else
    if (terminal.terminal_id[0] != 'L')
    {
        terminal.terminal_id[0] = 'L';
    }
#endif
#endif

    // Setup a dummy MID for some reason 
    //vim_mem_copy(terminal.merchant_id, DummyMID, 15);

    VIM_DBG_STRING(date_buf);
    if (VIM_ERROR_NONE != vim_mem_compare(terminal.terminal_id, date_buf, VIM_SIZEOF(terminal.terminal_id)))
    {
#if 0
        DBG_POINT;
        if (reversal_present())
            VIM_ERROR_RETURN( ERR_REVERSAL_PENDING );

        if (saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count))
           VIM_ERROR_RETURN( ERR_SAF_PENDING );

        VIM_ERROR_RETURN_ON_FAIL(terminal_check_config_change_allowed(VIM_TRUE));
#endif
        DBG_POINT;
        terminal_set_status(ss_tid_configured);

        shift_totals_reset(0);        
        saf_destroy_batch();        
        terminal_clear_status(ss_tms_configured);
        
        terminal.logon_state = rsa_logon;

            //preauth_batch_delete();
            //txn_delete(VIM_TRUE);
            //txn_delete(VIM_FALSE);
            //delete_card_last_track2();
        
        pceft_debug(pceftpos_handle.instance, "Updated Terminal ID");
        vim_mem_copy(terminal.terminal_id, date_buf, VIM_SIZEOF(terminal.terminal_id));
        vim_mem_copy(terminal.tms_parameters.terminal_id, date_buf, VIM_SIZEOF(terminal.terminal_id));

        // RDD 091214 - reload the comms config as stuff like IP address order can change eith even and odd TIDs
        //comms_config_load();
    }
    terminal_set_status(ss_tid_configured);

    // Do not allow to proceed to change TID if Timeout, User Cancel etc. 
    VIM_ERROR_RETURN_ON_FAIL(res);

    return VIM_ERROR_USER_BACK;
}

#if 1
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_merchant_id(void)
{
    kbd_options_t options;
    VIM_CHAR date_buf[16];

    vim_mem_clear(date_buf, sizeof(date_buf));

    options.timeout = ENTRY_TIMEOUT*10;
    options.min = 15;
    options.max = 15;
    //options.entry_options = KBD_OPT_DISP_ORIG;
    options.entry_options = KBD_OPT_DISP_ORIG | KBD_OPT_ALPHA | KBD_OPT_ALLOW_BLANK;
    options.CheckEvents = VIM_FALSE;

    vim_mem_copy(date_buf, terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id));

    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(date_buf, &options, DISP_TERMINAL_CONFIG_MERCHANT_ID, VIM_PCEFTPOS_KEY_NONE, ""));

    if (VIM_ERROR_NONE != vim_mem_compare(terminal.merchant_id, date_buf, vim_strlen(date_buf)))
    {
#if 1
        if (reversal_present())
            return ERR_REVERSAL_PENDING;

        if (saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count))
            return ERR_SAF_PENDING;
#endif
        VIM_ERROR_RETURN_ON_FAIL(terminal_check_config_change_allowed(VIM_TRUE));

        if (terminal_get_status(ss_tid_configured))
        {
            shift_totals_reset(0);
            saf_destroy_batch();
            //preauth_batch_delete();
            //txn_delete(VIM_TRUE);
            //txn_delete(VIM_FALSE);
            //delete_card_last_track2();
        }

        vim_mem_copy(terminal.merchant_id, date_buf, vim_strlen(date_buf));
        vim_mem_copy(terminal.tms_parameters.merchant_id, date_buf, vim_strlen(date_buf));

        // RDD TODO TMS
        //tms_delete_param_files();

#if 0
        terminal.configured = VIM_TRUE;
#endif
        // RDD
        terminal_clear_status(ss_tms_configured);

        //remove_settle_date();

        terminal_save();
    }

    return VIM_ERROR_USER_BACK;
}

#endif


VIM_ERROR_PTR ConfigureTerminal(void)
{
    VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;

    do
    {
        if ((res = config_terminal_id()) == VIM_ERROR_USER_BACK)
        {
            //if ((res = config_merchant_id()) == VIM_ERROR_USER_BACK)
			if (IS_V400m)
			{
				// Agreed with MB that this was best spot to configure Standalone or Integrated
				VIM_ERROR_RETURN_ON_FAIL(swap_terminal_mode());
			}
			
			// Change of TID may mean change in comms methods etc.		
			CommsSetup(VIM_TRUE);

			// RDD 210622 JIRA PIRPIN-1644 May need MID for standalone
			if (IS_STANDALONE)
			{
				config_merchant_id();
			}
                
			if((res = config_OTP()) != VIM_ERROR_NONE)            
			{
                    display_result(res, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
                    if (res == VIM_ERROR_ALREADY_INTIALIZED)
                    {
                        pceft_debug_test(pceftpos_handle.instance, "Already Provisioned");
                        terminal_set_status(ss_tid_configured);
                        return VIM_ERROR_NONE;
                    }
					else
					{
						terminal_clear_status(ss_tid_configured);
					}
                    break;
                    //vim_file_delete(CR_REQ_TOKEN);                
			}                
			else                
			{                    
				display_result(res, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);                
			}      
        }

    } while (!CheckResult(res));

    terminal_clear_status(ss_tms_configured);
    terminal.logon_state = rsa_logon;
    return VIM_ERROR_NONE;
}


// RDD 150322 JIRA PIRPIN-1484 No OTP CDS
VIM_ERROR_PTR PostCDSValidation(VIM_BOOL Force)
{
	VIM_CHAR AscPPID[16 + 1];
    VIM_ERROR_PTR Error = VIM_ERROR_NONE; 
	VIM_BOOL Exists =	vim_file_is_present(SECURITY_TOKEN);
	// Only run CDS if no security token.
    VIM_DBG_VAR( Exists );
	if (( !Exists )||( Force ))
	{
        DBG_POINT;

		// Use last 12 bytes of PPID as OTP
		vim_mem_clear(AscPPID, VIM_SIZEOF(AscPPID));
		vim_strcpy(AscPPID, GetAsciiPPID());
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Running Auto-CDS", "");
        if( !Exists )
		    pceft_debug(pceftpos_handle.instance, "Security Token Missing - run auto CDS");
        if( Force )
            pceft_debug(pceftpos_handle.instance, "Certificate near Expiry - run auto CDS");
        DBG_POINT;
		//if ((Error = CR_PerformAuthChallenge(&AscPPID[4])) != VIM_ERROR_NONE)
		if ((Error = CR_PerformAuthChallenge("")) != VIM_ERROR_NONE)
		{
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "CDS Failed", Error->name );
			pceft_debug_error(pceftpos_handle.instance, "CDS Failed", Error );
			//terminal_clear_status(ss_tid_configured);
		}
		else
		{
			pceft_debug_error(pceftpos_handle.instance, "CDS Success", Error);
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "CDS Success", "Running RTM");
		}
		vim_event_sleep(1000);
	}
	else
	{
		pceft_debug(pceftpos_handle.instance, "No need to run CDS");
	}

	VIM_ERROR_RETURN( Error );
}