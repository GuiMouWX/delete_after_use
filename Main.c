
#include <incs.h>
#include <vim_apps.h>
#include <vim_multiapp.h>
#ifndef _WIN32
#include <mal_nfc.h>
#endif

#define EMV_APP_LIST_MAX	9
#define EMV_APP_LIST_TITLE	"SELECT APPLICATION"
#define TERMINAL_STATUS(x) TerminalStatus(VIM_BOOL OtherMenu)

VIM_DBG_SET_FILE("TF");
//static VIM_UINT8 page_no = 0;
// VIM_MAIN_APPLICATION_TYPE app_type = VIM_MAIN_APPLICATION_TYPE_MASTER;

VIM_UINT64 original_idle_time = 180;
//VIM_RTC_UPTIME TempStatusDisplay;
#ifdef STANDALONE
extern VIM_BOOL adk_com_ppp_connected(void);
#endif
extern const card_bin_name_item_t CardNameTable[];

extern VIM_CHAR BrandFileName[];
extern PREAUTH_SAF_TABLE_T preauth_saf_table;
extern VIM_NET_HANDLE gprs_ppp_handle;
extern VIM_ERROR_PTR GetSettlementDate(void);
extern VIM_ERROR_PTR gprs_get_info( VIM_GPRS_INFO *info, VIM_BOOL use_last_known );
extern VIM_TEXT const preauth_saf_file_batch[];
extern VIM_TEXT const preauth_table[];
extern VIM_SIZE SlaveModeTimeout;
extern VIM_ERROR_PTR vim_pceftpos_callback_generic_initialisation(logon_state_t set_logon_state);
extern VIM_ERROR_PTR TerminalIdleDisplay(VIM_UINT8 TxnIndex);
extern VIM_BOOL IsSlaveMode(void);

extern VIM_BOOL CheckHello(VIM_PCEFTPOS_PTR self_ptr);


extern void slave_loop(    /** [in] Self PCEFTPOS Ptr */
    VIM_PCEFTPOS_PTR self_ptr,
    /** [in] Bank type of application */
    VIM_UINT8 bankcode,
    /** [in] Pointer to a uart instance connected to PCEFTPOS */
    VIM_UART_PTR uart_ptr,
    /** [in] Pointer to the UART parameters for the given UART PTR */
    VIM_UART_PARAMETERS const * uart_parameters,
    /** [in] Pointer to the protocol used to connect to PCEFTPOS */
    VIM_UART_PROTOCOL const * protocol_ptr,
    /** [in] Pointer to slave request message */
    void const * req,
    /** [in] Number of bytes in the request message */
    VIM_SIZE reqlen,
    /** Slave mode timeout - set up by enter slave mode command */
    VIM_SIZE *Timeout

);


//VIM_ERROR_PTR IdleDisplay(void);
VIM_ERROR_PTR display_idle(void);



/*----------------------------------------------------------------------------*/
static VIM_TLV_DICTIONARY_ENTRY const application_tag_dictionary[]=
{
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_TYPE),
//  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_INCOMPLETE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_HOST_RC_ASC),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_AMT_PURCHASE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_AMT_REFUND),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_AMT_CASH),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_AMT_TOTAL),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_ACCOUNT),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_TIME),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_BATCH_NO),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_ROC_NO),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_ERROR),

  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_STAN),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_AUTH_NO),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_OFFLINE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_SETTLE_DATE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_MSG_SENT),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_PIN_ENTERED),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_MSG_RECEIVED),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_PIN_BLOCK),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_EMV_DATA_SIZE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_EMV_DATA_BUFFER),
  VIM_TLV_DICTIONARY_ITEM(TAG_TXN_STATUS),
  VIM_TLV_DICTIONARY_ITEM(TAG_CARD_TYPE),
  VIM_TLV_DICTIONARY_ITEM(TAG_TRACK_1_RESULT),
  VIM_TLV_DICTIONARY_ITEM(TAG_TRACK_1),
  VIM_TLV_DICTIONARY_ITEM(TAG_TRACK_2_RESULT),
  VIM_TLV_DICTIONARY_ITEM(TAG_TRACK_2),
  VIM_TLV_DICTIONARY_ITEM(TAG_PAN),
  VIM_TLV_DICTIONARY_ITEM(TAG_EXPIRY_MM),
  VIM_TLV_DICTIONARY_ITEM(TAG_EXPIRY_YY),
  VIM_TLV_DICTIONARY_ITEM(TAG_4DBC),
  VIM_TLV_DICTIONARY_ITEM(TAG_NO_CCV_REASON),
  VIM_TLV_DICTIONARY_ITEM(TAG_MANUAL_REASON),
  VIM_TLV_DICTIONARY_ITEM(TAG_RECURRENCE_TYPE),
  VIM_TLV_DICTIONARY_ITEM(TAG_EMV_FALLBACK),
  VIM_TLV_DICTIONARY_ITEM(TAG_TERM_STAN),
  VIM_TLV_DICTIONARY_ITEM(TAG_TERM_SFW_VERSION),
  /*VIM_TLV_DICTIONARY_ITEM(TAG_LOGON_STATE),*/
  VIM_TLV_DICTIONARY_ITEM(TAG_TERM_CONFIGURED),
  //VIM_TLV_DICTIONARY_ITEM(TAG_TERM_BATCH_NUMBER),
  VIM_TLV_DICTIONARY_ITEM(TAG_TERM_SEQ_NUMBER),
  VIM_TLV_DICTIONARY_ITEM(TAG_EFB_TIMER_PENDING),
  VIM_TLV_DICTIONARY_ITEM(TAG_EFB_TIMER),
  VIM_TLV_DICTIONARY_ITEM(TAG_DUPLICATE_PRESENT),
  VIM_TLV_DICTIONARY_ITEM(TAG_TRAINING_MODE),
  VIM_TLV_DICTIONARY_ITEM(TAG_CATID),
  VIM_TLV_DICTIONARY_ITEM(TAG_CAIC),
  VIM_TLV_DICTIONARY_ITEM(TAG_FLOOR_LIMITS),
  VIM_TLV_DICTIONARY_ITEM(TAG_SAF_UPLOAD),
  VIM_TLV_DICTIONARY_ITEM(TAG_CARD_ACCEPTANCE),
  VIM_TLV_DICTIONARY_ITEM(TAG_KEY_BEEP_ON),
  VIM_TLV_DICTIONARY_ITEM(TAG_PULSE_DIAL),
  VIM_TLV_DICTIONARY_ITEM(TAG_BLIND_DIAL),
  VIM_TLV_DICTIONARY_ITEM(TAG_PABX),
  VIM_TLV_DICTIONARY_ITEM(TAG_ENTRY_TIMEOUT),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_CARD_MISREADS),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_CONNECTION_FAILS),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_RESPONSE_TIMEOUTS),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_LINE_FAILS),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_DIAL_ATTEMPTS),
  VIM_TLV_DICTIONARY_ITEM(TAG_LAST_ACTIVITY),
  VIM_TLV_DICTIONARY_ITEM(TAG_IDLE_SCREEN),
  VIM_TLV_DICTIONARY_ITEM(TAG_PREFERRED_VERSION),
  VIM_TLV_DICTIONARY_ITEM(TAG_DLL_SCHEDULED),
  VIM_TLV_DICTIONARY_ITEM(TAG_TERMINAL_STATUS),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_RID),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_LIMIT_DOMESTIC),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_LIMIT_INTERNATIONAL),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_LIMIT_CONTACTLESS_TRANSACTION),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_LIMIT_CONTACTLESS_FLOOR),
  VIM_TLV_DICTIONARY_ITEM(TAG_ICC_LIMIT_CONTACTLESS_CVM),
  VIM_TLV_DICTIONARY_ITEM(TAG_STATS_CHIP_MISREADS),
  VIM_TLV_DICTIONARY_ITEM(TAG_CARD_CONTAINER),
  VIM_TLV_DICTIONARY_ITEM(TAG_MERCHANT_CONTAINER),
  VIM_TLV_DICTIONARY_ITEM(TAG_MERCHANT_ICC_LIMITS),
  VIM_TLV_DICTIONARY_ITEM(TAG_MERCHANT_ICC_LIMIT_RECORD)
};


const VIM_CHAR *TerminalStatus( VIM_BOOL OtherMenu )
{
	static const VIM_CHAR *status_ptr;

	if (initial_keys_required == terminal.logon_state)
		status_ptr = " KEYS REQUIRED  ";
	else if (!terminal_get_status( ss_tid_configured ))
		status_ptr = "CONFIG REQUIRED ";
	// RDD 170722 JIRA PIRPIN-1747
#if 0
	else if (!terminal_get_status( ss_tms_configured ))
		status_ptr = "  TMS REQUIRED  ";
#endif

	// JIRA PIRPIN-1477 : Logon required after 76 response in standalone
	else if ((logged_on != terminal.logon_state)&&(session_key_logon != terminal.logon_state))
		status_ptr = " LOGON REQUIRED ";
	else if (terminal_locked == terminal.logon_state)
		status_ptr = " CALL HELPDESK  ";
	else if( terminal_get_status( ss_offline_test_mode))
		status_ptr = "OFFLINE MODE";

	else
	{
#if 0
		if( terminal.training_mode )
#else
		// 010818 RDD JIRA WWBP-290 - no training mode idle display in integrated
		if((terminal.training_mode) && ( IS_STANDALONE ))
#endif
			status_ptr = "TRAINING MODE";
		else if (IS_STANDALONE && OtherMenu )
		{
				status_ptr = "OTHER MENU";
		}
		// RDD 180718 JIRA WWBP-225
		else if (IS_STANDALONE || (IS_INTEGRATED_V400M))
		{
			if (PAPER_OUT)
				status_ptr = "REPLACE PAPER";
			else if ((PRN_BATTERY_ERROR) && ( IsDocked() == VIM_FALSE ))
				status_ptr = "BATTERY LOW";
			else
				status_ptr = READY_STRING;
		}
		else
		{
			status_ptr = READY_STRING;
		}
	}
	return status_ptr;
}

// RDD JIRA WWBP-289 100818
VIM_BOOL IsIdle(void)
{
	if ( !vim_strcmp( TerminalStatus(VIM_FALSE), READY_STRING ))
		return VIM_TRUE;
	return VIM_FALSE;
	//return( vim_strstr( TerminalStatus( VIM_FALSE ), "RY", &Ptr ) == VIM_ERROR_NONE) ? VIM_TRUE : VIM_FALSE;
}



void CheckConfig( VIM_SIZE Timeout )
{
    VIM_KBD_CODE key_code;

    //VIM_DBG_MESSAGE("---------------- Check Kbd -----------------");
    // RDD 230819 JIRA WWBP-684 Use function key when not connected to register
    // kill the task for the time being otherwise we'll never read a key if one is pressed

    if (pceft_settings_is_tcp())
    {
        // vim_pceftpos_pause(pceftpos_handle.instance);
    }

    if (vim_kbd_read(&key_code, Timeout) == VIM_ERROR_NONE)
    {
        VIM_DBG_VAR(key_code);

        if ((key_code == VIM_KBD_CODE_FUNC) || (key_code == VIM_KBD_CODE_HASH))
        {
            DBG_MESSAGE("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF config Menu FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
            // RDD 250918 -  kill the task as we may need to re-configure the PcEFT Interface Settings and TCPIP connection takes most of the proc power
            //vim_pceftpos_delete(pceftpos_handle.instance);
#ifndef _WIN32
			if (IS_V400m) {
				//hideStatusBar();
			}
#endif
            // RDD 220121 - JIRA PIRPIN-983 Config menu access during txn can allow fraud
            if (transaction_get_status(&txn, st_cpat_check_completed) == VIM_FALSE)
            {
                config_functions(0);
            }
            else
            {
#ifdef STANDALONE
                if (IS_STANDALONE)
                {
                    TerminalIdleDisplay(0);
                }
#endif
                //pceft_settings_reconfigure(VIM_TRUE);
            }
        }
    }
    if (pceft_settings_is_tcp())
    {
        // vim_pceftpos_resume(pceftpos_handle.instance);
        vim_event_sleep(100);   // Give the task a chance to run and see if we got connected
    }
}


static void DisplayClientStatus(VIM_BOOL PosConnected, VIM_BOOL NeverConnected)
{
    VIM_CHAR buffer[150];
    static VIM_BOOL WasConnected;
    terminal.allow_ads = PosConnected;

    if (PosConnected)
    {
            
       // DBG_MESSAGE("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF config Menu FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        CheckConfig(50);
      
        if (PosConnected != WasConnected)
        {
            vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Register Connected");
            if (XXX_MODE)
            {
                VIM_CHAR pceft_mode[30];
                vim_mem_clear(pceft_mode, VIM_SIZEOF(pceft_mode));
                pceft_settings_get_conf_string(pceft_mode, VIM_SIZEOF(pceft_mode));
                vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s\n%s", BUILD_NUMBER, REVISION, "Register Connected", pceft_mode );
            }
            display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
            if (XXX_MODE) vim_event_sleep(3000);
            vim_event_sleep(2000);
            WasConnected = PosConnected;
        }
        //DBG_MESSAGE("<<<< POS CONNECTED >>>>");
    }
    else
    {
        static VIM_SIZE count;
        DBG_MESSAGE("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF config Menu FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

        CheckConfig(1000);

        if (NeverConnected)
        {
            if( PCEFT_TCPIP )
            {
                if (count++ % 5 < 4 )
                    vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register");
                else
                {
                    vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register\n(TCPIP Error)");
                    pceft_settings_reconfigure(VIM_TRUE);
                    return;
                }
            }
            else
            {
                vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register");
            }
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
		}
        else if (!(count++ % 10))
        {
            // Was once connected since power up
            if (pceft_settings_is_tcp())
            {
                vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register\nTCPIP Disconnect");
            }
            else
            {
                vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register");
            }
            // Try to Restart only ocassionally in case cable was pulled and replaced
			if( pceft_settings_reconfigure(VIM_TRUE) != VIM_ERROR_NONE )
			{
				display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
				pceft_debug(pceftpos_handle.instance, "Interface failure");
			}
			else
			{
				display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
				pceft_debug(pceftpos_handle.instance, "Client Reconnect");
			}
        }
    }
}




VIM_BOOL IsPosConnected(VIM_BOOL fWaitForPos)
{
    //VIM_BOOL pceftpos_connected = VIM_FALSE;
    static VIM_BOOL PosConnected;
    static VIM_BOOL NeverConnected;
    static VIM_BOOL AlreadyRun;
	//static VIM_BOOL OnBase;

	//VIM_DBG_MESSAGE("Is POS Connected");
    //VIM_DBG_ERROR(VIM_ERROR_TIMEOUT);
    if (!AlreadyRun)
    {
        NeverConnected = VIM_TRUE;
        AlreadyRun = VIM_TRUE;
    }

    do
    {
        if (IS_STANDALONE)
        {
            return VIM_FALSE;
        }
        else if (VIM_NULL == pceftpos_handle.instance)
        {
            // Connect failed
            DBG_MESSAGE("<<<< ERROR!!! PCEFTPOS MODDULE NOT INITILIASED >>>>");
            DisplayClientStatus(VIM_FALSE, NeverConnected);
            PosConnected = VIM_FALSE;
            vim_event_sleep(2000);
            pceft_settings_reconfigure(VIM_FALSE);
        }
        else
        {
            // RDD 080312: SPIRA 5029 Attempt to connect to POS needs to timeout after about 10 seconds (like 5100)
            // RDD 040420 May get connected back for ages after cable disconnected because

            if (pceft_is_connected())
            {
                NeverConnected = VIM_FALSE;
                PosConnected = VIM_TRUE;

#if 1
                // Aux check when connected to ensure cable hasn't been removed
#ifndef _WIN32
               //if( CheckHello( pceftpos_handle.instance ) /* && pceft_settings_is_tcp()*/)
				if(1)
#else
			   if( CheckHello( pceftpos_handle.instance ) /* && pceft_settings_is_tcp()*/)
#endif
                {
                    PosConnected = VIM_TRUE;
                   // DBG_POINT;
                }
                else
                {
                    PosConnected = VIM_FALSE;
                    VIM_DBG_MESSAGE("----- HELLO TIMEOUT ------");
                }
#endif
            }
            else
            {
				DBG_POINT;
                // Looks like we just got disconnected like someone removed the cable....
                if (PosConnected)
                {
                    VIM_DBG_WARNING("Clear Trickle Feed Flag !!");
                    terminal.trickle_feed = VIM_FALSE;
                    VIM_DBG_MESSAGE("Just Got Disconnected....");
                    NeverConnected = VIM_FALSE;

					// When Terminal removed from base and direct Eth active, BT PAN may not always take over
					if ((IS_V400m) && (IS_INTEGRATED))
					{
						pceft_settings_reconfigure(VIM_TRUE);
					}
                }

                DBG_POINT;
                PosConnected = VIM_FALSE;

            }
            DisplayClientStatus(PosConnected, NeverConnected);
        }
    }

    // RDD 160714 v430 SPIRA:7398 - make this infinite so we can't ever have a situation where terminal is moved and transacts on old TID

    //while( --count );
    while ((!PosConnected) && (fWaitForPos));
    return PosConnected;
}




// 101018 RDD JIRA WWBP-331 Idle Display Delay after Cancel
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR main_display_idle_screen( VIM_TEXT *text1, VIM_TEXT *text2 )
{
    IdleDisplay();
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_power_on(void)
{
  VIM_ERROR_PTR error;
  static VIM_BOOL is_initialized=VIM_FALSE;

  if (is_initialized)
  {
    return VIM_ERROR_NONE;
  }

  /*setup an application tag dictionary */
  vim_tlv_dictionary_add( application_tag_dictionary,VIM_LENGTH_OF(application_tag_dictionary));

  /* start the file system*/
  vim_file_initialise("WOW");

    /* init sound driver */
  vim_sound_initalize();

  /* start the display */
  display_initialize();

  error = VIM_ERROR_NONE;

  error = power_up_routine();
  is_initialized=VIM_TRUE;
  return error;
}


/*----------------------------------------------------------------------------*/

//static VIM_BOOL IconsSet;

VIM_ERROR_PTR display_status( VIM_CHAR *status_string_ptr, VIM_CHAR *logon_status_string )
{
	VIM_CHAR uStat[40];
	VIM_CHAR tid[VIM_SIZEOF( terminal.terminal_id )+1];

	vim_mem_clear( tid, VIM_SIZEOF( tid ));
	vim_mem_copy( tid, terminal.terminal_id, VIM_SIZEOF( terminal.terminal_id ));
	vim_sprintf( uStat, "%s\n%s", status_string_ptr, tid );

#ifndef _WIN32
	if (IS_V400m) {
		//hideStatusBar();
	}
#endif

	VIM_ERROR_RETURN_ON_FAIL(display_screen(terminal.IdleScreen, VIM_PCEFTPOS_KEY_NONE, uStat, logon_status_string));
	return VIM_ERROR_NONE;
}




/*----------------------------------------------------------------------------*/
void set_operation_initiator(terminal_initator_t initator)
{
  terminal.initator = initator;
}

terminal_initator_t get_operation_initiator(void)
{
  return (terminal.initator);
}



VIM_ERROR_PTR is_ready(void)
{
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;

	if( !terminal_get_status( ss_tid_configured ))
	{
		return ERR_CONFIGURATION_REQUIRED;
	}

	if( terminal.terminal_id[0] == ' ' || terminal.terminal_id == 0x00 )
	{
		terminal_clear_status( ss_tid_configured );
		return ERR_CONFIGURATION_REQUIRED;
	}
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR txn_settlement( void )
{
  /* Init the Txn structure */

  settle.type = tt_settle;

  VIM_ERROR_RETURN_ON_FAIL( GetSettlementDate( ));

  if( settle_terminal() != VIM_ERROR_NONE )
  {
	  display_result( settle.error, settle.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR txn_presettlement( void )
{
  /* Init the Txn structure */
  settle.type = tt_presettle;
  settle_terminal();
  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/*static VIM_ERROR_PTR txn_lastsettlement( void )
{
  //Init the Txn structure
  settle.type = tt_last_settle;
  VIM_ERROR_RETURN_ON_FAIL(settle_terminal());
  // return without error
  return VIM_ERROR_NONE;
}*/



#define MAX_STANDALONE_TXN_TYPES 20
#define TXN_LABEL_LEN			 20+1




/*----------------------------------------------------------------------------*/
// RDD JIRA WWBP-293 Reprint PinPad Receipt for VX690
#if 0
static VIM_ERROR_PTR txn_admin_copy( void )
#else
VIM_ERROR_PTR txn_admin_copy(void)
#endif
{
  VIM_ERROR_PTR ret;

  if (VIM_ERROR_NONE != (ret = is_ready()))
  {
    if (VIM_MAIN_APPLICATION_TYPE_MASTER != app_type)
      display_result(ret, txn.host_rc_asc, "", transaction_get_training_mode() ? STR_TRAINING_MODE : "", 0,TWO_SECOND_DELAY);
    return ret;
  }
  if(( ret = txn_duplicate_txn_read( VIM_TRUE )) == VIM_ERROR_NONE )
  {
	  //if(( ret = PrintReceipts( &txn_duplicate, VIM_TRUE )) != VIM_ERROR_NONE )
	  //  display_result( ret, "", "", "", VIM_PCEFTPOS_KEY_NONE, __2_SECONDS__ );
	  VIM_ERROR_RETURN_ON_FAIL( print_duplicate_receipt( &txn_duplicate ));
  }

  /* return without error */
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR txn_admin_shift( void )
{
  if (VIM_ERROR_NONE != (txn.error = is_ready()))
  {
    if (VIM_MAIN_APPLICATION_TYPE_MASTER != app_type)
      display_result(txn.error, txn.host_rc_asc, "", transaction_get_training_mode() ? STR_TRAINING_MODE : "", 0,TWO_SECOND_DELAY);
    return txn.error;
  }
  /* Init the Txn structure */
  txn_init();
  txn.type = tt_shift_totals;
  VIM_ERROR_RETURN_ON_FAIL( shift_totals_process( VIM_FALSE ));
  /* return without error */
  return VIM_ERROR_NONE;
}



static VIM_BOOL TimeSet = VIM_FALSE;

VIM_ERROR_PTR GetTerminalStatus( VIM_CHAR *logon_status_string )
{
	VIM_ERROR_PTR res = VIM_ERROR_NOT_FOUND;

	// RDD 280718 JIRA WWBP-226 - ensure empty by default
	vim_strcpy(logon_status_string, "");

	if( !terminal_get_status( ss_tid_configured ))
		vim_strcpy( logon_status_string, "C" );
	// RDD 170722 JIRA PIRPIN-1747
#if 0
	else if( !terminal_get_status( ss_tms_configured ))
		vim_strcpy( logon_status_string, "D" );
#endif
	else if( initial_keys_required == terminal.logon_state )
		vim_strcpy( logon_status_string, "K" );

	else if( rsa_logon == terminal.logon_state )
		vim_strcpy( logon_status_string, "R" );

	else if(( terminal.logon_state != logged_on ) && ( terminal.logon_state != session_key_logon ))
	{
		TimeSet = VIM_FALSE;
		terminal.screen_id = 0;
		vim_strcpy( logon_status_string, "W" );
	}
	else
		res = VIM_ERROR_NONE;

	return res;
}



VIM_ERROR_PTR AddXMLEvent(VIM_CHAR *Buffer, VIM_RTC_UPTIME EndTime, VIM_RTC_UPTIME StartTime, VIM_CHAR *Label)
{
    VIM_CHAR TagBuff[100];
    VIM_CHAR EndLabel[50];
    VIM_SIZE Interval = 0;

    VIM_DBG_PRINTF1("%s", Label);
    VIM_DBG_VAR(EndTime);
    VIM_DBG_VAR(StartTime);

    vim_sprintf(EndLabel, "</%s>", Label);

    if ((EndTime) && (EndTime > StartTime))
    {
        Interval = ((VIM_UINT64)EndTime - (VIM_UINT64)StartTime);
        VIM_DBG_NUMBER(Interval);

        vim_sprintf(TagBuff, "<%s>%d", Label, Interval);
        vim_strcat(TagBuff, EndLabel);
        vim_strcat(Buffer, TagBuff);
    }
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR BuildXMLEvents(u_TxnEventCheck *uEventData, VIM_CHAR *Buffer, VIM_BOOL Loyalty)
{

    VIM_DBG_MESSAGE("<<<<<<<<<<<<<< EVENT PROCESSING >>>>>>>>>>>>>>>>");

    vim_strcat(Buffer, "<EVENTS>");

    AddXMLEvent(Buffer, uEventData->Fields.Prompt, uEventData->Fields.POSReq, "PRESENT_DEVICE");
    AddXMLEvent(Buffer, uEventData->Fields.Presented, uEventData->Fields.POSReq, "DEVICE_PRESENTED");

    if (!Loyalty)
    {
        // App Selection start - end
        AddXMLEvent(Buffer, uEventData->Fields.AppReq, uEventData->Fields.POSReq, "SELECT_APP");
        AddXMLEvent(Buffer, uEventData->Fields.AppResp, uEventData->Fields.POSReq, "APP_SELECTED");

        // Acc Selection start - end
        AddXMLEvent(Buffer, uEventData->Fields.AccReq, uEventData->Fields.POSReq, "SELECT_ACC");
        AddXMLEvent(Buffer, uEventData->Fields.AccResp, uEventData->Fields.POSReq, "ACC_SELECTED");

        // CVM proc start - end
        AddXMLEvent(Buffer, uEventData->Fields.CVMReq, uEventData->Fields.POSReq, "CVM_PROMPT");
        AddXMLEvent(Buffer, uEventData->Fields.CVMResp, uEventData->Fields.POSReq, "CVM_ENTERED");

        // Msg send - receive
        AddXMLEvent(Buffer, uEventData->Fields.MsgTx, uEventData->Fields.POSReq, "MSG_SENT");
        AddXMLEvent(Buffer, uEventData->Fields.MsgRx, uEventData->Fields.POSReq, "MSG_RECEIVED");
    }

    AddXMLEvent(Buffer, uEventData->Fields.POSResp, uEventData->Fields.POSReq, "POS_RESP_SENT");

    vim_strcat(Buffer, "</EVENTS>");
    return VIM_ERROR_NONE;
}





VIM_ERROR_PTR BuildXMLLoyaltyReport(transaction_t *tran, VIM_CHAR *Buffer)
{

	VIM_CHAR TagBuff[2000];
	VIM_CHAR DataBuff[2000];
    //VIM_SIZE DataLen = 0;

    vim_strcat(Buffer, "<TXN><TYPE>Loyalty</TYPE>");

    VIM_DBG_MESSAGE("----------- XML - Loyalty Report -------------");

    // Transaction Container Header
    if (tran->session_type == VIM_ICC_SESSION_APPLE_VAS)
        vim_strcpy(DataBuff, "AppleVAS");
    else if (tran->session_type == VIM_ICC_SESSION_ANDROID_VAS)
        vim_strcpy(DataBuff, "Android");
    else
    {
        vim_strcpy(DataBuff, "Unknown");
    }

    // Add the loyalty data
    vim_sprintf(TagBuff, "<WALLET_TYPE>%s</WALLET_TYPE>", DataBuff);
    vim_strcat(Buffer, TagBuff);

    // Add unique tap id for Android.
    // 19/04/2018 Vyshakh - Uncomment as requested
#if 1
    vim_mem_clear(DataBuff, VIM_SIZEOF(DataBuff));
    vim_mem_clear(TagBuff, VIM_SIZEOF(TagBuff));

    if (tran->session_type == VIM_ICC_SESSION_ANDROID_VAS) {
        VIM_UINT8 i;
        for (i = 0; i < VIM_SIZEOF(tran->vas_android_tap_id); i++) {
            vim_sprintf(&DataBuff[i * 2], "%02X", tran->vas_android_tap_id[i]);
        }

        vim_sprintf(TagBuff, "<UNIQUE_TAP_ID>%s</UNIQUE_TAP_ID>", DataBuff);
        vim_strcat(Buffer, TagBuff);
    }
    else if (tran->session_type == VIM_ICC_SESSION_APPLE_VAS) {
        VIM_UINT8 i;
        for (i = 0; i < VIM_SIZEOF(tran->vas_apple_mob_token); i++) {
            vim_sprintf(&DataBuff[i * 2], "%02X", tran->vas_apple_mob_token[i]);
        }
        vim_sprintf(TagBuff, "<MOBILE_TOKEN>%s</MOBILE_TOKEN>", DataBuff);
        vim_strcat(Buffer, TagBuff);
    }

    vim_mem_clear(tran->vas_android_tap_id, VIM_SIZEOF(tran->vas_android_tap_id));
    vim_mem_clear(tran->vas_apple_mob_token, VIM_SIZEOF(tran->vas_apple_mob_token));
#endif
    // Add Events : Present Card Time, Card Presented Time, Response Sent Time 
    BuildXMLEvents(&tran->uEventDeltas, Buffer, VIM_TRUE);

    // Add the trailer
    vim_strcat(Buffer, "</TXN>");
    return VIM_ERROR_NONE;
}

VIM_ERROR_PTR GetAID(transaction_t *tran, VIM_CHAR *DataBuff)
{
    VIM_TLV_LIST tags_list;
    VIM_UINT8 AID[128];
    VIM_UINT8 tlv_buffer[256];
    const VIM_TAG_ID tags[] = { VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_TAG_EMV_AID_TERMINAL };

    vim_mem_clear(AID, VIM_SIZEOF(AID));
    vim_mem_clear(tlv_buffer, VIM_SIZEOF(tlv_buffer));

    vim_tlv_create(&tags_list, tlv_buffer, sizeof(tlv_buffer));
    vim_picc_emv_get_tags(picc_handle.instance, (VIM_TLV_TAG_PTR *)tags, sizeof(tags) / sizeof(tags[0]), &tags_list);

    if (vim_tlv_search(&tags_list, VIM_TAG_EMV_DEDICATED_FILE_NAME) == VIM_ERROR_NONE)
    {
        vim_mem_copy(AID, tags_list.current_item.value, tags_list.current_item.length);
    }
    else if (vim_tlv_search(&tags_list, VIM_TAG_EMV_AID_TERMINAL) == VIM_ERROR_NONE)
    {
        vim_mem_copy(AID, tags_list.current_item.value, tags_list.current_item.length);
    }
    if (tags_list.current_item.length)
        hex_to_asc(AID, DataBuff, 2 * tags_list.current_item.length);
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR EmvGetFormFactor(transaction_t *tran, VIM_CHAR *FFAscii_Ptr)
{
    VIM_TLV_LIST tags_list;
    VIM_UINT8 FF_Buff[512];

    vim_mem_clear(FF_Buff, VIM_SIZEOF(FF_Buff));
    if (tran->emv_data.data_size)
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&tags_list, tran->emv_data.buffer, tran->emv_data.data_size, VIM_SIZEOF(tran->emv_data.buffer), VIM_FALSE));
        if (vim_tlv_search(&tags_list, VIM_TAG_EMV_FORM_FACTOR) == VIM_ERROR_NONE)
        {
            vim_tlv_get_bytes(&tags_list, FF_Buff, VIM_SIZEOF(FF_Buff));
            hex_to_asc(FF_Buff, FFAscii_Ptr, 2 * tags_list.current_item.length);
        }
    }
    return VIM_ERROR_NONE;
}




VIM_ERROR_PTR BuildXMLTranReport(transaction_t *tran, VIM_CHAR *Buffer)
{
	VIM_CHAR TagBuff[2000];
	VIM_CHAR DataBuff[2000];
    //VIM_CHAR Tid[8+1] = { 0,0,0,0,0,0,0,0,0 };
    //VIM_SIZE DataLen = 0;
    VIM_UINT32 CpatIndex = tran->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

    VIM_DBG_MESSAGE("----------- XML - Transaction Report -------------");

    vim_strcat(Buffer, "<TXN><TYPE>Payment</TYPE>");

    // Add the Txn POS ref
    vim_sprintf(TagBuff, "<TXN_ID>%s</TXN_ID>", tran->pos_reference);
    vim_strcat(Buffer, TagBuff);

    // Get Details of the tran type
    get_txn_type_string(&txn, DataBuff, VIM_SIZEOF(DataBuff), VIM_FALSE);
    vim_sprintf(TagBuff, "<TXN_TYPE>%s</TXN_TYPE>", DataBuff);
    vim_strcat(Buffer, TagBuff);

    // Get the details of the Payment
    vim_strcpy(DataBuff, CardNameTable[CardNameIndex].card_name);
    vim_sprintf(TagBuff, "<CARD_TYPE>%s</CARD_TYPE>", DataBuff);
    vim_strcat(Buffer, TagBuff);

    // Get the details of the Amount
    vim_sprintf(TagBuff, "<AMOUNT>%s</AMOUNT>", AmountString(tran->amount_total));
    vim_strcat(Buffer, TagBuff);

    // Get AID
    vim_mem_clear(DataBuff, VIM_SIZEOF(DataBuff));
    GetAID(tran, DataBuff);
    if (vim_strlen(DataBuff))
    {
        vim_sprintf(TagBuff, "<AID>%s</AID>", DataBuff);
        vim_strcat(Buffer, TagBuff);
    }

    // Get FFI
    vim_mem_clear(DataBuff, VIM_SIZEOF(DataBuff));
    EmvGetFormFactor(tran, DataBuff);
    if (vim_strlen(DataBuff))
    {
        vim_sprintf(TagBuff, "<FFI>%s</FFI>", DataBuff);
        vim_strcat(Buffer, TagBuff);
    }

    VIM_DBG_STRING(Buffer);

    // Trello #281 add new info to aid splunk queries
    if( !TERM_FORBID_XML_OPT )
    {
        vim_sprintf( TagBuff, "<SW_STC>%s</SW_STC>", txn.cnp_error );
        vim_strcat( Buffer, TagBuff );
        vim_sprintf( TagBuff, "<SW_IEM>%3.3d</SW_IEM>", txn.sw_iem );
        vim_strcat( Buffer, TagBuff );
        vim_sprintf( TagBuff, "<SW_FEM>%3.3d</SW_FEM>", txn.sw_fem );
        vim_strcat( Buffer, TagBuff );
    }


    // Add Events : Present Card Time, Card Presented Time, Response Sent Time 
    BuildXMLEvents(&tran->uEventDeltas, Buffer, VIM_FALSE);

    vim_strcat(Buffer, "</TXN>");
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR BuildXMLHeader(transaction_t *tran, VIM_CHAR *Buffer)
{
	VIM_CHAR TagBuff[1000];
	VIM_CHAR DataBuff[500];
    VIM_CHAR Tid[8 + 1] = { 0,0,0,0,0,0,0,0,0 };
    VIM_SIZE DataLen = 0;
    //VIM_UINT32 CpatIndex = tran->card_name_index;
    //VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
    vim_mem_copy(Tid, terminal.terminal_id, 8);

    vim_strcpy(Buffer, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
    vim_strcat(Buffer, "<WOW_DIGITAL_WALLET>");

    // Add the TID
    vim_sprintf(TagBuff, "<TID>%s</TID>", Tid);
    vim_strcat(Buffer, TagBuff);

    // Get the txn time (time of arrival of J8/M )
    VIM_ERROR_RETURN_ON_FAIL(vim_sprintf(DataBuff, "%02d/%02d/%02d %02d:%02d:%02d:%03d", tran->time.day, tran->time.month, tran->time.year % 2000, tran->time.hour, tran->time.minute, tran->time.second, tran->time.millisecond));
    vim_sprintf(TagBuff, "<DATE_TIME>%s</DATE_TIME>", DataBuff);
    vim_strcat(Buffer, TagBuff);

    // Get the Loyalty number
    DataLen = VIM_SIZEOF(DataBuff);
    RetrieveLoyaltyNumber(DataBuff, &DataLen);
    if( DataLen )
    {
        vim_sprintf( TagBuff, "<LOYALTY_NUMBER>%s</LOYALTY_NUMBER>", DataBuff );
        vim_strcat( Buffer, TagBuff );
        return VIM_ERROR_NONE;
    }
    else
    {
        return VIM_ERROR_NOT_FOUND;
    }
}



VIM_ERROR_PTR BuildXMLReport(transaction_t *tran /*, VIM_BOOL Loyalty*/)
{
    VIM_CHAR Buffer[2000];
    VIM_BOOL Exists = VIM_FALSE;
    VIM_ERROR_PTR res = VIM_ERROR_NONE;

    // No report if no loyalty number
    //vim_file_exists( LOYALTY_FILE, &Exists );

    // Add Basic header + start time and loyalty number
    // RDD 270521 - exit if no loyalty number as empty report can mess up J8+redemption etc.
    res = BuildXMLHeader(tran, Buffer);

    // Add Wallet Type
    // VN JIRA WWBP-322 Intermittent incomplete Digital Wallet XML containers
    // Issue was caused because a STATUS was trigerred just after loyalty tap and it was served before reaching reporting
    // Tran Report for all Tapped Cards
    if ((tran->type != tt_query_card) && (tran->card.type != card_none)) {
        VIM_DBG_MESSAGE("----------- XML - Transaction present -------------");
        BuildXMLTranReport(tran, Buffer);
        // RDD 160321 - v724-0 File has been used up so delete it.
        //vim_file_delete(LOYALTY_FILE);
        // RDD 230621 JIRA PIRPIN-1092 
        pceft_debug(pceftpos_handle.instance, Buffer);
    }
    // RDD 270521 - exit if no loyalty number as empty report can mess up J8+redemption etc.
    else if ( res == VIM_ERROR_NONE )
    {
        vim_file_exists(LOYALTY_FILE, &Exists);
        if (tran->type == tt_query_card && Exists) {
            VIM_DBG_MESSAGE("----------- XML - Transaction not present -------------");
            BuildXMLLoyaltyReport(tran, Buffer);
            // Trailer
            vim_strcat(Buffer, "</WOW_DIGITAL_WALLET>");

            pceft_debug(pceftpos_handle.instance, Buffer);
        }
    }
    // RDD 160321 - v724-0 File has been used up so delete it.
    vim_file_delete(LOYALTY_FILE);
    return VIM_ERROR_NONE;
}





/*
RDD 300418 JIRA WWBP-26 Rejig Adverts to include Banner specific advertising
Flow is as follows:
Check to see if there is branding.
If no branding then Exit.

If branding then check to see if Branding Ad00 is present.
If present then display only branding Ad

If not present then display default (non branded) advertising
*/

#ifdef _WIN32

VIM_ERROR_PTR DisplayAdverts(VIM_UINT8 ScreenID)
{
    VIM_ERROR_PTR res = VIM_ERROR_NONE;
    static VIM_BOOL BrandingAds;
    VIM_CHAR AdfileName[20];
    VIM_BOOL AdPresent = VIM_FALSE;
    VIM_CHAR Brand[3 + 1] = { 0,0,0,0 };
    VIM_CHAR TempFileName[20];

    if (Wally_HasRewardsId())
    {
        return VIM_ERROR_NOT_FOUND;
    }

	terminal_clear_status(ss_locked);

    // Set up default Ad file to display by using non-specific (existing Advert file format)
    vim_sprintf(AdfileName, "ADVERT%2.2d.pcx", ScreenID);

    // First check we actually have Branding
    if (!vim_strlen(TERM_BRAND))
    {
        // No brand therefore no advertising.
        return VIM_ERROR_NOT_FOUND;
    }
    vim_mem_clear(TempFileName, VIM_SIZEOF(TempFileName));
    //Ptr += vim_strlen(VIM_FILE_PATH_LOCAL);
    vim_mem_copy(Brand, TERM_BRAND, WOW_BRAND_LEN);

    // Build the Advert file format as specified in JIRA WWBP-26
    vim_sprintf(TempFileName, "%3sAD%2.2d.PCX", Brand, ScreenID);

    if (!ScreenID)
    {
        BrandingAds = vim_file_is_present(TempFileName) ? VIM_TRUE : VIM_FALSE;
    }

    // See if there is actually a Bran specific ad for current index
    // RDD 300418 - if no brand specific Ad then default to generic Ad for that index

    if (BrandingAds)
    {
        if (vim_file_is_present(TempFileName))
        {
            if ((res = display_screen_cust_pcx(DISP_BRANDING, "Label", TempFileName, VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE)
            {
                pceft_debug(pceftpos_handle.instance, "FailB");
                terminal.screen_id = 0;
            }
        }
        else
            terminal.screen_id = 0;
    }
    else
    {
        if (vim_file_is_present(AdfileName))
        {
            if ((res = display_screen_cust_pcx(DISP_BRANDING, "Label", AdfileName, VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE)
            {
                terminal.screen_id = 0;
            }
            //pceft_debug(pceftpos_handle.instance, "OK");
        }
        else
        {
            terminal.screen_id = 0;
        }
    }
    return  VIM_ERROR_NONE;
}



#else

VIM_ERROR_PTR DisplayAdverts(VIM_UINT8 ScreenID)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	static VIM_BOOL BrandingAds;
	VIM_CHAR AdfileName[20];
	//VIM_BOOL AdPresent = VIM_FALSE;
	VIM_CHAR Brand[3 + 1] = { 0,0,0,0 };
	VIM_CHAR TempFileName[20];
	VIM_CHAR *Ptr = terminal.configuration.BrandFileName;
    VIM_UINT8 id_screen = DISP_IMAGES;
    VIM_CHAR buf_format[16];
    VIM_UINT8 format_len;

	if (Wally_HasRewardsId())
	{
		return VIM_ERROR_NOT_FOUND;
	}

    //VIM_DBG_STRING(Ptr);

	// Set up default Ad file to display by using non-specific (existing Advert file format)

    display_get_image_format(buf_format, &format_len);
    vim_sprintf(AdfileName, "ADVERT%2.2d.", ScreenID);
    vim_strncat(AdfileName, buf_format, format_len);

	// First check we actually have Branding
    //VIM_DBG_STRING(terminal.configuration.BrandFileName);

	if (!vim_strlen(terminal.configuration.BrandFileName))
	{
		// No brand therefore no advertising.
		return VIM_ERROR_NOT_FOUND;
	}
	vim_mem_clear(TempFileName, VIM_SIZEOF(TempFileName));
	//Ptr += vim_strlen(VIM_FILE_PATH_LOCAL);
	vim_mem_copy(Brand, Ptr, WOW_BRAND_LEN);

	// Build the Advert file format as specified in JIRA WWBP-26
    //VIM_DBG_WARNING(Ptr);

    vim_sprintf(TempFileName, "%3sAD%2.2d.png", Brand, ScreenID);
	//pceft_debugf_test(pceftpos_handle.instance, "Try:%s", TempFileName);

    id_screen = DISP_IMAGES;

    if (!ScreenID)
    {
        BrandingAds = vim_file_is_present(TempFileName) ? VIM_TRUE : VIM_FALSE;
    }

	// RDD 300418 - if no brand specific Ad then default to generic Ad for that index
	if (BrandingAds)
	{
		if (vim_file_is_present(TempFileName))
		{
			if ((res = display_screen_cust_pcx(id_screen, "Label", TempFileName, VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE)
			{
				pceft_debugf(pceftpos_handle.instance, "Failed to Display: TempFileName");
				terminal.screen_id = 0;
			}
			else
			{
				//pceft_debugf_test(pceftpos_handle.instance, "Displayed: TempFileName");
			}
		}
		else
		{
			//pceft_debug_test(pceftpos_handle.instance, "No more Ads");
			terminal.screen_id = 0;
		}
	}
	else if(vim_file_is_present(AdfileName))
	{
		if ((res = display_screen_cust_pcx(id_screen, "Label", AdfileName, VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE)
		{
			pceft_debugf(pceftpos_handle.instance, "Failed to Display: %s", AdfileName );
			terminal.screen_id = 0;
		}
		else
		{
			//pceft_debugf_test(pceftpos_handle.instance, "Displayed: AdfileName");
		}
	}
	else
	{
		//pceft_debug_test(pceftpos_handle.instance, "No more Ads");
		terminal.screen_id = 0;
	}
	return res;
}

#endif

VIM_ERROR_PTR IdleDisplay(void)
{
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_TEXT logon_status_string[50];
	VIM_TEXT status_string[50];
	VIM_TEXT *status_string_ptr = status_string;
	//VIM_SIZE rev = REVISION;
	VIM_CHAR Message[50];
	//VIM_BOOL OfflineMode;
	vim_mem_clear(Message, VIM_SIZEOF(Message));

   // VIM_DBG_WARNING("IIIIIIIIIIIIIIIIIIIIIIIII IDLE DISPLAY IIIIIIIIIIIIIIIIIIIIIIIII");

	//OfflineMode = terminal_get_status(ss_offline_test_mode) ? VIM_TRUE : VIM_FALSE;

	// RDD 140312 - Basically the terminal is ready for action so rotate the ad screens
	GetTerminalStatus(logon_status_string);

    // RDD 010519 JIRA WWBP-611 this was being called when VX690 off base prior to 'm' response being sent out. Provided free goods as overrides any error
    // txn.error = VIM_ERROR_NONE;

	vim_sprintf(status_string, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, TerminalStatus(0));
	//DBG_POINT;

	if( IsIdle() )
	{
		// RDD 160323 added to show background handling tasks (eg SAF handling) in VAAs
		VAA_CheckDebug();

		//DBG_POINT;
		if (Wally_HasRewardsId())
		{
            display_screen_cust_pcx(DISP_IMAGES, "Label", EDP_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
/*
            VIM_BOOL default_event = VIM_FALSE;
            VIM_EVENT_FLAG_PTR pceftpos_event_ptr = &default_event;
            VIM_EVENT_FLAG_PTR event_list[1];
            display_screen_cust_pcx(DISP_IMAGES_NEW, "Label", EDP_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
            vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceftpos_event_ptr);
            event_list[0] = pceftpos_event_ptr;
            VIM_KBD_CODE key;
            while (vim_adkgui_kbd_touch_or_other_read(&key, 30000, event_list,1) == VIM_ERROR_NONE)  {
                if (key == VIM_KBD_CODE_CANCEL)
                {
                    vim_kbd_sound();
                    display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, "QrRemovingEDP.gif");
                    if (Wally_HasPaymentSessionId()) {
                        Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_TRUE);
                        display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, "QrEDPRemoved.png");
                    }
                    else {
                        pceft_debug(pceftpos_handle.instance, "Payment Session ID not FOund");
                    }

                }
                if (*event_list )
                {
                    break;
                }
            }
            */
		}
		//else if(( IS_INTEGRATED_V400M ) && ( !OfflineMode ))
		else if (0)		// Need this for icons (when ready )
		{
		// RDD 100818 JIRA WWBP-290

			display_screen_cust_pcx( DISP_IDLE, "label", terminal.configuration.BrandFileName, VIM_PCEFTPOS_KEY_NONE );
		}
		else
		{
		    //VIM_DBG_WARNING( terminal.configuration.BrandFileName );
			{
				VIM_SIZE charge = 0;
			
				if( IS_V400m )
					vim_terminal_get_battery_charge(&charge);

				if ((charge < 25)&&( !IsDocked() ))
				{
					VIM_CHAR Buffer[200] = { 0 };
					vim_sprintf(Buffer, "Battery Low: %d%%", charge);
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer, "Please Return To Base");
					vim_sound(VIM_TRUE);
					vim_event_sleep(500);
					vim_sound(VIM_TRUE);
					vim_event_sleep(500);
					vim_sound(VIM_TRUE);
					vim_event_sleep(500);
					vim_sound(VIM_TRUE);
					vim_event_sleep(3500);
				}
			}


            // RDD 291019 JIRA WWBP-755 : Branding Broken in P400 - if no branding display IDLE status
#ifdef _WIN32
            if (display_screen_cust_pcx(DISP_BRANDING, "Label", terminal.configuration.BrandFileName, VIM_PCEFTPOS_KEY_NONE) != VIM_ERROR_NONE)
#else
			if ((IS_V400m) && (terminal.logon_state == logged_on))
			{
				showStatusBar();
			}
            if (display_screen_cust_pcx(DISP_IMAGES, "Image", terminal.configuration.BrandFileName, VIM_PCEFTPOS_KEY_NONE) != VIM_ERROR_NONE)
#endif
            {
                VIM_DBG_WARNING(" ------------- BRANDING ERROR --------------");
                display_status(status_string_ptr, logon_status_string);
            }
        }
		//VIM_ERROR_RETURN_ON_FAIL( display_status( "", "" ));
	}
	else  // Not ready
	{
		status_string_ptr = status_string;
        VIM_ERROR_RETURN_ON_FAIL(display_status(status_string_ptr, logon_status_string));
    }

	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

void DoRebootTimed(void)
{
    // RDD - get rid of any txn data so we don't see on powerup.
    txn_init();
    TERM_PRE_PCI_REBOOT_CHECK = TERM_DEFAULT_REBOOT_SECS;
    terminal_save();
    pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 3 --");
    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 3 --");
    vim_event_sleep(1000);
    pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 2 --");
    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 2 --");
    vim_event_sleep(1000);
    pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 1 --");
    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 1 --");
    vim_event_sleep(950);
    pceft_debug(pceftpos_handle.instance, "PCI Idle Reboot\nRebooting Now..");
    vim_event_sleep(50);
    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "--  Rebooting...  --", "");
    vim_terminal_reboot();

}

void CheckPCIReboot(VIM_BOOL Verbose)
{
    VIM_CHAR Buffer[250];
    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
    VIM_RTC_TIME CurrentTime;
    VIM_RTC_UNIX CurrentTimeUnix;

    vim_rtc_get_time(&CurrentTime);
    vim_rtc_convert_time_to_unix(&CurrentTime, &CurrentTimeUnix);
    CurrentTimeUnix += SECS_IN_ONE_DAY;
    vim_rtc_convert_unix_to_time(CurrentTimeUnix, &CurrentTime);

    vim_sprintf(Buffer, "SecToReboot:%d Pre-Reboot Window:%d", terminal_get_secs_to_go(), TERM_PRE_PCI_REBOOT_CHECK);
   // pceft_debug_test( pceftpos_handle.instance, Buffer );

    if ((terminal_get_secs_to_go() < (TERM_PRE_PCI_REBOOT_CHECK / 10)))
    {
        pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 3 --");
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 3 --");
        vim_event_sleep(1000);
        pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 2 --");
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 2 --");
        vim_event_sleep(1000);
        pceft_debug_test(pceftpos_handle.instance, "-- Nightly Reboot in 1 --");
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "-- Nightly Reboot --", "-- in 1 --");
        vim_event_sleep(950);
        pceft_debug(pceftpos_handle.instance, "PCI Idle Reboot\nRebooting Now..");

        vim_terminal_set_pci_reboot_time(&CurrentTime, VIM_TRUE);
        vim_event_sleep(50);
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "--  Rebooting...  --", "");
        vim_terminal_reboot();
    }
    else if ((terminal_get_secs_to_go() < (TERM_PRE_PCI_REBOOT_CHECK)))
    {
        if( config_get_yes_no_timeout( "Nightly Reboot Due Soon\n<YES> To Delay 5 mins", "Reboot Delayed\nBy 5 mins", 30000, VIM_FALSE ) == VIM_FALSE )
        {
            pceft_debug(pceftpos_handle.instance, "PCI Idle Reboot\nRebooting Now..");                
            vim_terminal_set_pci_reboot_time(&CurrentTime, VIM_TRUE);
            vim_event_sleep(50);
            vim_terminal_reboot();
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "PCI Idle Reboot\nDelayed by Operator");
            TERM_PRE_PCI_REBOOT_CHECK += 300;
        }
    }
}



static VIM_ERROR_PTR display_idle_lock( VIM_SEMAPHORE_TOKEN_HANDLE* lcd_lock_ptr )
{
  VIM_BOOL rotate_screens = VIM_FALSE;
 // static VIM_RTC_TIME current_time, saved_time;
  VIM_TEXT logon_status_string[50];
  VIM_RTC_UPTIME uptime;
  static VIM_SIZE Secs, LastSecs;

  /* lock the display (prevent updates)*/
  //VIM_ERROR_RETURN_ON_FAIL(vim_lcd_lock_update(lcd_lock_ptr));

  // DEVX 031219 Don't update display with Ads etc. if slave mode in progress instead run the slave mode for event checking etc.
  //DBG_MESSAGE("Display Idle Lock ");
  //DBG_POINT;
  if( IsSlaveMode() == VIM_TRUE )
  {
      //VIM_DBG_MESSAGE("----------- SSSSS NO IDLE DISPLAY SSSSS --------------");
      slave_loop(pceftpos_handle.instance, 0, VIM_NULL, VIM_NULL, VIM_NULL, VIM_NULL, 0, &SlaveModeTimeout);
      return VIM_ERROR_NONE;
  }



  if (IsPosConnected(VIM_TRUE))
  {
      // RDD 140312 - Basically the terminal is ready for action so rotate the ad screens
      if ((GetTerminalStatus(logon_status_string) == VIM_ERROR_NONE) && (terminal.allow_ads))
          rotate_screens = VIM_TRUE;
      else
          rotate_screens = VIM_FALSE;
  }
  else if (IS_STANDALONE)
  {
	  //DBG_POINT;
      rotate_screens = VIM_TRUE;
  }


  // RDD 181018 - catch all to ensure that no Ads if not logged on ( eg session key logon reqd. )
  if (terminal.logon_state == session_key_logon)
  {
      // logon_request(VIM_FALSE);
      IdleDisplay();
      return VIM_ERROR_NONE;
  }


  // If the status indicates that the terminal is ready for transactions (normal operating mode) then run the Ad Screens

		vim_rtc_get_uptime(&uptime);

		// Convert to secs
		Secs = uptime / 1000;
#if 0
  if ((Secs % 3 == 0) && (TERM_AUTO_TMS))
  {
      VIM_DBG_MESSAGE("Run TMS Auto ");
      tms_contact(VIM_TMS_DOWNLOAD_SOFTWARE, VIM_TRUE, web_lfd);
  }
#endif

  // RDD 260718 Ensure that if DockStatus changes then we go directly to screen update.
  if ((Secs != LastSecs) && (Secs%terminal.screen_display_time == 0))

  {
      TimeSet = VIM_FALSE;
      LastSecs = Secs;

      // RDD -donna says check PCI reboot very frequently - should be every 10 secs here normally
      if (TERM_PRE_PCI_REBOOT_CHECK)
      {
          CheckPCIReboot(VIM_FALSE);
      }

		  if( terminal.screen_id > WOW_MAX_SCREENS )
		  {
			  terminal_clear_status( ss_report_branding_status );
			  terminal.screen_id = 0;
		  }

		  //DBG_POINT;
#ifndef _WIN32
		  if((IS_V400m)&&( terminal.logon_state == logged_on ))
		{
			showStatusBar();
		}
#endif		  
	  if (rotate_screens)
	  {
		  //DBG_POINT;

		  switch( terminal.screen_id )
		  {
			case 0:
				//DBG_POINT;
				IdleDisplay();
				terminal.screen_id +=1;
			break;

			default:
				// RDD 031018 - Run Idle display here is no brand setup.
				if (DisplayAdverts(terminal.screen_id - 1) != VIM_ERROR_NONE)
				{
					IdleDisplay();
				}
				if (terminal.screen_id != 0)
					terminal.screen_id += 1;

			break;

		 }
	  }
	  else
          IdleDisplay();

	  if( TimeSet == VIM_FALSE )
	  {
		//saved_time = current_time;
		TimeSet = VIM_TRUE;
	  }
  }
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
#if 0
static VIM_ERROR_PTR auto_tms_download(void)
{
  /* check the time */
  VIM_RTC_TIME current_time;
  VIM_TEXT  temp_buffer[5];
  VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&current_time));
  vim_mem_clear( temp_buffer, VIM_SIZEOF( temp_buffer ));
  vim_snprintf( temp_buffer, VIM_SIZEOF( temp_buffer ), "%02d%02d",current_time.hour,  current_time.minute );
  /*VIM_DBG_PRINTF1( "current time:%s", temp_buffer );*/

  if( vim_mem_compare(temp_buffer,terminal.tms_auto_download_settings.download_time,4) == VIM_ERROR_NONE )
  {
    if( terminal.tms_auto_download_settings.download_type == 'P' )
      VIM_ERROR_RETURN_ON_FAIL(tms_download_parameters());
    if( terminal.tms_auto_download_settings.download_type == 'F' )
      VIM_ERROR_RETURN_ON_FAIL(tms_download_parameters_and_software());
    /*
    terminal.tms_auto_download_settings.status = VIM_TRUE;
    terminal_save();*/
  }


  return VIM_ERROR_NONE;
}
#endif



VIM_BOOL is_standalone_mode(void)
{
  if( st_standalone == terminal.mode )
  {
	  terminal.initator = it_pinpad;
	  return VIM_TRUE;
  }
  return VIM_FALSE;
}

VIM_BOOL is_integrated_mode(void)
{
	VIM_DBG_VAR(terminal.mode);
  return (st_integrated == terminal.mode);
}

VIM_ERROR_PTR display_idle(void)
{
  VIM_SEMAPHORE_TOKEN_HANDLE lcd_lock;

  VIM_ERROR_PTR error;
  //VIM_TMS_DOWNLOAD scheduled_type;
  //VIM_BOOL need_tms;

  // RDD 090212 - bugfix : Backlight sometimes drops so make 100% in IDLE
  //vim_lcd_set_backlight( 100 );
  //DBG_POINT;
#if 0
  if (TempStatusDisplay)
  {
	  terminal.IdleScreen = DISP_IDLE;
  }
  else
#endif
  {
	  terminal.IdleScreen = terminal.allow_ads ? DISP_IDLE_PCEFT : DISP_IDLE;
  }

#if 0
  if( IS_INTEGRATED )
  {
	  if (IS_INTEGRATED_V400M && PRINT_ON_TERMINAL && !IsDocked() )
	  {
		  terminal.IdleScreen = DISP_IDLE;
		  //VIM_CHAR buffer[100];
		  //vim_sprintf((VIM_CHAR *)buffer, "Ver: %d.%d", SOFTWARE_VERSION, REVISION);
		  //main_display_idle_screen((VIM_CHAR *)buffer, "");
	  }
	  else
	  {
		  terminal.IdleScreen = DISP_IDLE_PCEFT;
	  }
  }
  else
  {
		//terminal.IdleScreen = ( terminal.IdleScreen == DISP_IDLE2 ) ? DISP_IDLE : DISP_IDLE2;
		terminal.IdleScreen = DISP_IDLE;
  }
#endif

  /* display idle screen */
  error=display_idle_lock(&lcd_lock);
  /* ensure that screen is unlocked */
  //VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));
  /* catch and return any untreated errors */

  // RDD 170113 SPIRA:6491 - MB Says make this 10 secs only then return to ads
#if 0
  if(( terminal.allow_ads ) == VIM_FALSE )
  {
	  // RDD 200213 - this is blocking for the kbd so should be used really. Better just clear the
	pceft_pos_command_check( __10_SECONDS__ );
	terminal.allow_ads = VIM_TRUE;
  }
#endif

  VIM_ERROR_RETURN_ON_FAIL(error);
  VIM_ERROR_RETURN_ON_FAIL(is_ready());

    if( terminal.logon_state != logged_on )
    {
	  if( terminal.logon_state == file_update_required )
	  {
		  // if file update was interrupted then return to normal logon as version number won't have been updated
		  terminal.logon_state = logon_required;
	  }
      /* check logon status */
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

#if 1

VIM_ERROR_PTR handle_key_press(VIM_KBD_CODE key_code)
{
	/*display test code -----------------*

	char *items[EMV_APP_LIST_MAX];
	VIM_SIZE max;
	VIM_SIZE aid_count;

	// VIM_BOOL res;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_KBD_CODE key_pressed;
	char InputData[20];
	kbd_options_t options;

	options.timeout = ENTRY_TIMEOUT;
	options.min = 20;
	options.max = 20;
	options.entry_options = KBD_OPT_DISP_ORIG;

	vim_mem_clear(InputData, VIM_SIZEOF(InputData));
	*/

	DBG_POINT;
#ifndef _WIN32
	if (IS_V400m) {
		//hideStatusBar();
	}
#endif
	switch (key_code)
	{
	case VIM_KBD_CODE_KEY1:
	case VIM_KBD_CODE_KEY2:
	case VIM_KBD_CODE_KEY3:
	case VIM_KBD_CODE_KEY4:
	case VIM_KBD_CODE_KEY5:
	case VIM_KBD_CODE_KEY6:
	case VIM_KBD_CODE_KEY7:
	case VIM_KBD_CODE_KEY8:
	case VIM_KBD_CODE_KEY9:
		if (IS_STANDALONE)
		{
			TerminalIdleDisplay(key_code - '0');
			DBG_POINT;
			IdleDisplay();
		}
		break;

	case VIM_KBD_CODE_KEY0:
		if (IS_STANDALONE)
		{
			TerminalIdleDisplay(10);
			DBG_POINT;
			IdleDisplay();

		}
		//VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());
		break;

	case VIM_KBD_CODE_FEED:   // Paper Feed 
		//receipt_paper_feed();
		break;

	case VIM_KBD_CODE_INFO:
	case VIM_KBD_CODE_FUNC:
	case VIM_KBD_CODE_HASH:
	{
		VIM_ERROR_PTR error;

		// RDD 160412 SPIRA:5316 Z DEBUG for Function key presses
		pceft_debug(pceftpos_handle.instance, "Function Key pressed");
		error = config_functions(0);
		txn.type = tt_none;				//BD 27-06-2013
		pceft_debug(pceftpos_handle.instance, "Function processing complete");

		//VIM_DBG_ERROR(error);
		if (error == VIM_ERROR_USER_CANCEL || error == VIM_ERROR_USER_BACK)
		{
			vim_mag_flush(mag_handle.instance_ptr);
		}
		VIM_ERROR_RETURN_ON_FAIL(error);
	}
	break;

	case VIM_KBD_CODE_OK:
		if (IS_STANDALONE)
		{
			TerminalIdleDisplay(0);
			DBG_POINT;
			IdleDisplay();
		}
		break;

	case VIM_KBD_CODE_CLR:
		break;

	case VIM_KBD_CODE_SOFT1:
		break;

	case VIM_KBD_CODE_SOFT2:
		break;

	case VIM_KBD_CODE_SOFT3:
		break;

	case VIM_KBD_CODE_CANCEL:
		if (txn.type == tt_preswipe)
			break;
	default:
		/*VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());*/
		break;
	}

	display_idle();
	return VIM_ERROR_NONE;
}

#else

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR handle_key_press(VIM_KBD_CODE key_code)
{

    DBG_POINT;
  switch(key_code)
  {

     case VIM_KBD_CODE_OK:
      if (IS_STANDALONE)
          TerminalIdleDisplay(0);
      break;

     case VIM_KBD_CODE_INFO:
     case VIM_KBD_CODE_FUNC:
     case VIM_KBD_CODE_HASH:
     {
        VIM_ERROR_PTR error;

		// RDD 160412 SPIRA:5316 Z DEBUG for Function key presses
		pceft_debug(pceftpos_handle.instance, "Function Key pressed" );
        VIM_DBG_MESSAGE("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF - Run Config Functions FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

        error = config_functions(0);
	    txn.type = tt_none;				//BD 27-06-2013
		pceft_debug(pceftpos_handle.instance, "Function processing complete" );

        //VIM_DBG_ERROR(error);
        if( error == VIM_ERROR_USER_CANCEL || error == VIM_ERROR_USER_BACK )
        {
            VIM_DBG_POINT;
          vim_mag_flush(mag_handle.instance_ptr);
        }
        VIM_ERROR_RETURN_ON_FAIL(error);
      }
      break;

     default:
         DBG_VAR(key_code);
         if (IS_STANDALONE)
             TerminalIdleDisplay( key_code - 0x30 );
         break;
  }

  display_idle();
  return VIM_ERROR_NONE;
}

#endif

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_kbd
(
  VIM_KBD_CODE key_code
)
{
    DBG_POINT;
#if 0
    if (terminal_get_status(ss_locked))
    {
        DBG_POINT;
        VIM_ERROR_RETURN_ON_FAIL(vim_kbd_insert_key(key_code));
        DBG_POINT;

        if (password_compare(DISP_IDLE_LOCKED,  terminal.configuration.password, "  ", VIM_FALSE) == VIM_ERROR_NONE)
        {
            DBG_POINT;        
            VIM_ERROR_RETURN_ON_FAIL(terminal_clear_status(ss_locked));
            //VIM_ERROR_RETURN_ON_FAIL(terminal_save());
        }
    }
    else
#endif
    {
      /*VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());*/
      /* process key press */
      VIM_ERROR_IGNORE(handle_key_press(key_code));
    }

	// RDD 210618 WWBP-226 BWS VX690 Display - key press when docked goes to battery/status display
	//if( IS_INTEGRATED )
	//	VIM_ERROR_IGNORE(display_screen( DISP_BRANDING, VIM_PCEFTPOS_KEY_NONE));

  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_uart(void)
{
	//DBG_MESSAGE( "~~~~~~~~ UART Event Occurred ~~~~~~~~~" );
#ifndef _WIN32
	if (IS_V400m) {
		//hideStatusBar();
	}
#endif
	if( pceftpos_handle.instance != VIM_NULL )
    {
		vim_pceftpos_process_pending_message_at_idle( pceftpos_handle.instance );
    }
	return( VIM_ERROR_NONE );
}

/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR ReversalAndSAFProcessing( VIM_SIZE SAFSendCount, VIM_BOOL SendReversal, VIM_BOOL TerminalDisplay, VIM_BOOL DownloadFiles )
{
  VIM_ERROR_PTR error = VIM_ERROR_NONE;
  VIM_BOOL execute_logon = VIM_FALSE;

  init.error = VIM_ERROR_NONE;
  //VIM_EVENT_FLAG_PTR pceftpos_connected_ptr;



  if (!terminal_get_status(ss_tid_configured)) {
	  VIM_DBG_PRINTF1("ReversalAndSAFProcessing: terminal_get_status error: terminal.status is %d", terminal.status);
	  return ERR_CONFIGURATION_REQUIRED;
  }

  // RDD - put back for target when we have keys
  if( terminal.logon_state == initial_keys_required )
    return ERR_NO_KEYS_LOADED;

  pceft_debug_test( pceftpos_handle.instance, "* BEGIN SAF PROCESSING *" );


  // RDD 290312 : SPIRA 5168 - All PinPad intiated downloads should be interruptable
  if( terminal.initator == it_pceftpos )
	vim_pceftpos_enable_cancel_on_message(pceftpos_handle.instance, VIM_TRUE);

  // RDD 170722 JIRA PIRPIN-1747
#if 0

  // RDD 230412 - reject logon if embedded and LFD pending
   //DBG_MESSAGE( "Idle Check ss_tms_configured" );
  if( !terminal_get_status( ss_tms_configured ) )
  {
	   DBG_MESSAGE( "Idle Check ss_tms_configured NOT SET" );
	  if( DownloadFiles == VIM_FALSE )
		return ERR_NOT_TMS_ON;
	  else
	  {
	     DBG_MESSAGE( "Idle Check ss_tms_configured trying contact" );
		//VIM_ERROR_RETURN_ON_FAIL( tms_contact(VIM_TMS_DOWNLOAD_SOFTWARE) );
	  }
  }
#endif
  // RDD 260412 SPIRA 5367 - no embedded RSA logon
  //if(( terminal.logon_state == rsa_logon ) && ( txn.type != tt_logon ))
  // RDD 190122 JIRA PIRPIN-1404 : Allow background RSA Logon for LIVE terminals
  if(( terminal.logon_state == rsa_logon ) && ( txn.type != tt_logon ) && ( !LIVE_PINPAD ))
  {
	  return ERR_NOT_LOGGED_ON;
  }

  if( terminal.logon_state != logged_on )
  {
	 execute_logon = VIM_TRUE;
	 // Do a logon request in the background
	 logon_request( DownloadFiles );

	 // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
	 if(( init.error == VIM_ERROR_NO_LINK ) || ( init.error == ERR_NO_RESPONSE ) || ( init.error == VIM_ERROR_NO_EFT_SERVER ) || ( init.error == VIM_ERROR_MODEM ) || ( init.error == VIM_ERROR_CONNECT_TIMEOUT ))
	 {
         // RDD 181018 JIRA WWBP-297 Don't do this for session key logon
         if( terminal.logon_state == logon_required )
		    terminal.logon_state = terminal.last_logon_state;
	 }

	 if(( init.error != ERR_LOGON_SUCCESSFUL ) && ( init.error != VIM_ERROR_NONE ))
	 {
		 // If the logon fails reset the IDLE timer to 10 mins so we're not attempting freq. background logons
		 terminal.max_idle_time_seconds = WOW_BACKOFF_IDLE_TIME;
		 //terminal.allow_ads = VIM_TRUE;
		 return init.error;
	 }
	 else
	 {
		terminal.max_idle_time_seconds = original_idle_time;
	 }
  }


  /* Reversal Processing - RDD 160312 : Always send reversals after a sucessful logon */
  if(( SendReversal )||( execute_logon ))
  {
	  // Reversal includes sending any repeat SAF
	  error = reversal_send( TerminalDisplay );
	  if( error == VIM_ERROR_NOT_FOUND )
		  error = VIM_ERROR_NONE;
  }

  // JIRA PIRPIN_1917 ensure disconnect post SAF as this is deliberately blocked internally due to another JIRA ticket.
  if ((TERM_OPTION_PCEFT_DISCONNECT_ENABLED) && (error != VIM_ERROR_NONE))
  {
	  VIM_DBG_MESSAGE("DISCONNECT REVERSAL SEND ERROR");
	  comms_disconnect();
  }
  // RDD 230212 SPIRA4824 - abort SAF if error
  VIM_ERROR_RETURN_ON_FAIL(error);

  /* Check for trickle feeding 220s (if required - depends on caller )  */

  //DBG_MESSAGE("<<<<<<<<< Check SAF count in IDLE >>>>>>>>>>>>>");
  if(( saf_totals.fields.saf_count )&&( SAFSendCount ))
  {
	  DBG_MESSAGE("<<<<<<<<< SAFs to Send.... Sending SAF >>>>>>>>>>>>>");
	  error = saf_send( VIM_FALSE, SAFSendCount, VIM_FALSE, VIM_FALSE );
  }

  // RDD 290312 : SPIRA 5168 - All PinPad intiated downloads should be interruptable
  // RDD 300922 - this is out of date. We never should be able to interrupt a SAF upload. 
  //if( terminal.initator == it_pceftpos )
  //	vim_pceftpos_enable_cancel_on_message(pceftpos_handle.instance, VIM_FALSE);
  if (( TERM_OPTION_PCEFT_DISCONNECT_ENABLED )&&( SAFSendCount ))
  {
	  VIM_DBG_MESSAGE("DISCONNECT SAF SENT");
	  comms_disconnect();
  }

  //pceft_debug_test( pceftpos_handle.instance, "* END SAF PROCESSING *" );
  return error;
}


VIM_ERROR_PTR PostTxnProcessing( VIM_BOOL DisplayMsg )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

 //   VIM_DBG_WARNING("Post Txn Processing");

	//ConditionalDisconnect( );

	//DBG_POINT;
	if( terminal.trickle_feed )
	{
		//VIM_SIZE SafCount=0, SafPrintCount=0;
		VIM_RTC_UPTIME trickle_feed_start_time, trickle_feed_timer;
		//DBG_MESSAGE( "!!! TRICKLE FEED PROCESSING !!!");
		vim_rtc_get_uptime( &trickle_feed_start_time );
		vim_rtc_get_uptime( &trickle_feed_timer );

		// RDD 230315 ensure that all PICC txns are cancelled after we're done
		if( card_picc == txn.card.type )
		{
			res = vim_picc_emv_cancel_transaction( picc_handle.instance );
			VIM_DBG_ERROR( res );
		}

		//VIM_DBG_WARNING( "Clear Trickle Feed Flag !!" );

		terminal.trickle_feed = VIM_FALSE;
		// RDD 200515 Reintroduce the concept of timed trickle feeds to upload more than one txn if theres time

		//do
		if( 1 )
		{
			//DBG_POINT;
			// attempt to trickle feed one transaction or a signature rejection reversal

			// RDD 251013 Can still have active stuff in txn struct when this is called from IDLE so need to wipe this
			txn_init();
			//DBG_MESSAGE( "!!!!!!!!!!!!!!! SEND A SAF or a REVERSAL !!!!!!!!!!!!!!");
			// RDD 210217 - remove display as this cuts out some error displays from the TXN
			if(( res = ReversalAndSAFProcessing( TRICKLE_FEED_COUNT, VIM_TRUE, DisplayMsg, VIM_TRUE )) != VIM_ERROR_NONE )
			{
				VIM_ERROR_RETURN_ON_FAIL( res );
				//VIM_DBG_ERROR(res);
				//terminal.allow_ads = VIM_FALSE;
			}
			vim_rtc_get_uptime( &trickle_feed_timer );
			//VIM_DBG_NUMBER( trickle_feed_timer - trickle_feed_start_time );
			//VIM_DBG_NUMBER( terminal.configuration.TrickleFeedTime );
			//VIM_DBG_NUMBER( saf_pending_count( &SafCount, &SafPrintCount ));
		}
		//while((( trickle_feed_timer - trickle_feed_start_time ) < ( terminal.configuration.TrickleFeedTime )) && saf_pending_count( &SafCount, &SafPrintCount ));

		//DBG_POINT;

		////////////// TEST TEST TEST ////////////
		saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count );
		////////////// TEST TEST TEST ////////////

		//VIM_DBG_NUMBER( saf_totals.fields.saf_count );

		//terminal.idle_time_count = 0;
	    //vim_rtc_get_uptime( &terminal.idle_start_time );

		// piggy back has completed sucessfully so allow ads to be displayed
		//terminal.allow_ads = VIM_TRUE;
		//terminal.screen_id = 0;
	}
	// RDD 300812 - Switch ads on after the first txn, NOT after first trickle feed

    //comms_disconnect();

	return res;
}


static void UpdateTerminalFlags( void )
{
	VIM_BOOL new_saf_status = saf_full(VIM_FALSE);
	VIM_BOOL old_saf_status = terminal_get_status(ss_saf_full);
	static VIM_BOOL already_run;

	if(( new_saf_status != old_saf_status )||(already_run ))
	{
		// Saf status has changed so need to reset CTLS floor limits
		if( new_saf_status )
			terminal_set_status( ss_saf_full );
		else
			terminal_clear_status( ss_saf_full );
		terminal_set_status(ss_reset_ctls_floor_limits);
		DBG_MESSAGE(" !!!!!!!!!!!!!! FLOOR LIMITS RESET !!!!!!!!!!!!!!!" );
		already_run = VIM_TRUE;
	}
#if 0	// RDD 040714 - this hangs the terminal if no reader sw installed
	if(( terminal_get_status(ss_wow_reload_ctls_params)) && ( terminal.logon_state == logged_on ))
	{
		// RDD 171013 SPIRA:6961 v419 BD say to continue normal opearion if fatal reader error.
		if( !terminal_get_status(ss_ctls_reader_fatal_error) )
		{
			InitEMVKernel();
			CtlsInit( );
		}
		else
		{
			vim_event_sleep( 100 );
		}
	}
#endif
}



#define SECS_IN_ONE_DAY 86400

VIM_BOOL ContactSwitch( void )
{
  VIM_ERROR_PTR ret = VIM_ERROR_NONE;
  VIM_RTC_TIME CurrentTime;// , LastTxnTime;
  VIM_RTC_UNIX CurrentUnix, LastTxnUnix;
  VIM_CHAR Buffer[200];

  if(( ret = txn_duplicate_txn_read( VIM_FALSE )) == VIM_ERROR_NONE )
  {
	  // check to see if transacted
	  if( txn_duplicate.time.year == 0 )
		  return VIM_TRUE;
	  else
	  {
		  VIM_RTC_UNIX TimeDiff;

		  vim_rtc_get_time( &CurrentTime );
		  vim_rtc_convert_time_to_unix( &txn_duplicate.time, &LastTxnUnix );
		  vim_rtc_get_unix_date( &CurrentUnix );
		  TimeDiff = CurrentUnix - LastTxnUnix;
		  vim_sprintf( Buffer, "Secs Since Last Txn: %d", TimeDiff );
		  pceft_debug( pceftpos_handle.instance, Buffer  );

		  if( TimeDiff > SECS_IN_ONE_DAY)
		  {
			  pceft_debug_test( pceftpos_handle.instance, "JIRA: WWBP-93 Forced Logon" );
			  return VIM_TRUE;
		  }
	  }
	  // Normaly Operating Terminal - ie. has transacted in last 24hrs
	  return VIM_FALSE;
  }
  return VIM_TRUE;
}



// RDD 300413 - introduced for Phase 4
VIM_ERROR_PTR DailyHouseKeeping( void )
{
	//VIM_SIZE Expired=0, Completed=0;
	pceft_debug( pceftpos_handle.instance, "Daily Houskeeping" );

	// RDD 010515 SPIRA:8611 Need to purge preauth files to get rid of completed/expired records
	//PurgePreauthFiles( &Completed, &Expired );

	CheckMemory( VIM_TRUE );
	// Read the stats file into the terminal structure
	terminal_debug_statistics( VIM_NULL );
	vim_event_sleep(1000);
	// RDD 291014 SPIRA:8212 Contact TMS on a daily basis
	// RDD 080618 JIRA WWBP-112 - PCI encrypt card data - ensure SAF deleted before changing key !!
	tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE, VIM_TRUE, web_then_vim );

	// RDD 301214 SPIRA:8280 Logon automatically during daily houekeeping for standalone terminals
	// RDD 091117 JIRA:WWBP-22 add overnight logon for integrated

	if( ContactSwitch( ))
	{
		if( terminal.logon_state == logged_on )
			terminal.logon_state = logon_required;
		logon_request( VIM_TRUE );
	}

	// RDD 120618 We need to ensure that both SAF and Card Buffer are empty before changing the batch key
	card_destroy_batch();
	// VN JIRA WWBP-333 Saf tran caused due to base losing power - causes trickle feed process to stop funtioning
	// The key was changed on a TMS logon
#if 0
	// RDD 080618 JIRA WWBP-112 - PCI encrypt card data - ensure SAF deleted before changing key !!
	vim_tcu_generate_new_batch_key();
#endif

    // RDD 140520 Trello #129 We don't need this because the P400 reboots every night anyway.
#if 0
	if(( terminal_get_status( ss_ctls_reader_fatal_error ) || ( terminal_get_status( ss_wow_reload_ctls_params ))))
	{
		terminal_set_status(ss_wow_reload_ctls_params);
		pceft_debug(pceftpos_handle.instance, "Housekeeping Reboot Due to ctls failure" );
		pceft_pos_display_close();
        vim_event_sleep(1000);
		VIM_ERROR_RETURN_ON_FAIL( vim_terminal_reboot () );
	}
#endif
	return VIM_ERROR_NONE;
}

#if 0
VIM_BOOL DelayReboot(VIM_SIZE secs_to_go)
{
    // RDD 200520 Decided to reboot automatically after all.
    return VIM_FALSE;
    if (config_get_yes_no_timeout("Reboot Required.\nDelay by 3 mins?", "Requesting Delay..", 10000, VIM_FALSE))
    {
        if (pci_reboot_delay_available(&secs_to_go))
        {
            pceft_debug(pceftpos_handle.instance, "Reboot delayed by user: 3 mins");
            return VIM_TRUE;
        }
        else
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Delay Unavailable", "Rebooting now...");
        }
    }
    else
    {
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Delay Declined", "Rebooting now...");
    }
    vim_event_sleep(1000);
    return VIM_FALSE;
}
#endif


VIM_ERROR_PTR IdleTimeProcessing( void )
{
	VIM_RTC_UPTIME idle_now_time;
	VIM_RTC_TIME time;
	//VIM_SIZE ready_timeout_seconds = 0;
	VIM_SIZE ready_long_timeout_seconds = 0;
	//VIM_BOOL CardPresent = VIM_FALSE;
	VIM_ERROR_PTR ret = VIM_ERROR_NONE;
    //static VIM_BOOL sent;
	//VIM_SIZE Expired = 0, Completed = 0;

    //VIM_DBG_WARNING("Idle time Processing");

    // RDD 160321 - extend the timeout to avoid unwanted IDLE display between J8 Gift Query and completion
    //if((txn.card_state == card_capture_new_gift)&&( txn.type == tt_query_card ))
    //{
    //    ready_timeout_seconds = 5;
    //}

	/* get timer */
	vim_rtc_get_time( &time );

	//DBG_VAR( terminal.trickle_feed );
	vim_rtc_get_uptime(&idle_now_time);
	terminal.idle_time_count = (idle_now_time - terminal.idle_start_time)/1000;
    UpdateTerminalFlags();

	// IDLE TIME PROCESSING: Logon then Reversal then ALL outstanding SAF
	//if(( terminal.idle_time_count >= ready_timeout_seconds )||( txn.type == tt_none ))
	if(1)
	{
        //DBG_POINT;
        // RDD 270521 JIRA  PIRPIN-1078 - no X0 post J8
        if(( txn.type != tt_query_card )&&( txn.type != tt_none )&&(!terminal_get_status(ss_pos_window_closed)))
            pceft_pos_display_close();
        //sent = VIM_FALSE;

        if (txn.type == tt_none)
        {
            if (IS_STANDALONE)
            {
                //adk_com_ppp_connected();        
            }
			//DBG_POINT;
			if( terminal.idle_time_count >= ready_long_timeout_seconds )
			{
				// RDD 190517 SPIRA:9231 Improve Cancel Speed for J8 via Status
				terminal.abort_transaction = VIM_FALSE;
				if( !IS_INTEGRATED_V400M )
				terminal.allow_ads = VIM_TRUE;
			}
			else
			{
				// RDD 190517 SPIRA:9231 Improve Cancel Speed for J8 via Status
				if( terminal.abort_transaction )
				{
					terminal.abort_transaction = VIM_FALSE;
					terminal.allow_ads = VIM_TRUE;
				}
				else
				{
					return VIM_ERROR_NONE;
				}
			}
		}
		else
		{
			terminal.allow_ads = VIM_TRUE;
		}
		display_idle();

        // RDD - don't want this here !!!
		//QC_CheckForAuthChallengeRequest();
		//VAA_CheckDebug();
	}
	else
	{
		// Keep the result (eg APPROVED) display from the last txn for xx seconds in IDLE
		terminal.allow_ads = VIM_FALSE;
	}
#ifdef _DEBUG
	//terminal.max_idle_time_seconds = 20;
#endif
	if( terminal.idle_time_count >= terminal.max_idle_time_seconds )
	{
		VIM_INT16 RamUsedKB = 0;
		terminal.idle_time_count = 0;

		vim_prn_get_ram_used(&RamUsedKB);

		VIM_DBG_PRINTF1( "!!!! RAM USED = %d Kb !!!!", RamUsedKB );

		// RDD 300413: The Daily Housekeeping Offset is designated by the lane number % 60 (mins). So x 60 to get secs
		//VIM_BOOL ForceCoalesce = terminal_get_status( ss_daily_housekeeping_reqd ) ? VIM_TRUE : VIM_FALSE;
#if 0
		VIM_SIZE DailyHouseKeepingOffsetMins =  ( asc_to_bin( &terminal.terminal_id[6], 2 ));
#else
		// RDD 061217 Added for JIRA:WWBP93 - using term ID did not get a good spread between 1am and 2am so use RND value 0->59
		VIM_SIZE DailyHouseKeepingOffsetMins = terminal.configuration.OffsetTime;
#endif

		terminal.idle_start_time = idle_now_time;
		// RDD 220323 added as was not being run every 3 mins as specced. 
		ReversalAndSAFProcessing(TRICKLE_FEED_COUNT, VIM_TRUE, VIM_FALSE, VIM_TRUE);


		// RDD 061116 v561-5 Move this to daily in case its causing pilot "freeze"
		// RDD 230212 added for deleting stale card data stored for completion	- Phase6 check every 3 minutes
		MaintainCardBuffer();

        // RDD 140520 Trello #129 We don't need this because the P400 reboots every night anyway.
		// RDD 250116 SPIRA:8774 Need to reboot overnight if error is M0 system error due to issues with EMV GENAC1 getting killed
#if 0
		if( terminal.configuration.ctls_reset & CTLS_ONCE_OFF_REBOOT )
		{
			pceft_debug( pceftpos_handle.instance, "M0 Caused Idle Reboot" );
			pceft_pos_display_close();
			VIM_ERROR_RETURN_ON_FAIL( vim_terminal_reboot () );
		}


#endif

		if( terminal.max_idle_time_seconds != WOW_BACKOFF_IDLE_TIME )
		{
			original_idle_time = terminal.max_idle_time_seconds;
		}

        // RDD 200217 SPIRA:9206 Restore Daily housekeeping - particularly overnight contact to RTM/LFD
		if(( time.hour == DAILY_HOUSE_KEEPING_HOUR) && ( time.minute >= DailyHouseKeepingOffsetMins ))
		{
			if( terminal_get_status( ss_daily_housekeeping_reqd ))
			{
				terminal_clear_status( ss_daily_housekeeping_reqd );
				DailyHouseKeeping();
			}
		}
		else
		{
			// Set the flag ready for next time
			if( !terminal_get_status( ss_daily_housekeeping_reqd ) )
				terminal_set_status( ss_daily_housekeeping_reqd );
		}


		// RDD 300315 No obtain no trickle feed we need to set txn.error_backup
		//if( txn.error_backup == VIM_ERROR_NONE )
		//	terminal.trickle_feed = VIM_TRUE;

		// RDD 050615 SPIRA:8749 - reset Comms timer so that we can send SAFs from IDLE
		vim_rtc_get_uptime( &terminal.CommsUptime );
		//VIM_DBG_PRINTF1( "ConnectStart: %d", terminal.CommsUptime );
		//terminal.trickle_feed = VIM_TRUE;

		//DBG_VAR( terminal.trickle_feed );

		//DBG_MESSAGE( "!!!!!!!!!!!! DO SAFS ETC CALLED FROM IDLE !!!!!!!!!!!!!!!!" );
		if (set_mem_log) {
			pceft_debug(pceftpos_handle.instance, "Mem log before sending SAF in Idle");
			ValidateMemory(VIM_FALSE);
		}
		//ret =  PostTxnProcessing( VIM_TRUE );

		if (set_mem_log) {
			pceft_debug(pceftpos_handle.instance, "Mem log after sending SAF in Idle");
			ValidateMemory(VIM_FALSE);
		}
		return ret;
	}
	else
	{
		static VIM_UINT8 Count1, Count2;
		Count1 = terminal.max_idle_time_seconds - terminal.idle_time_count;
		if( Count1 != Count2 )
		{
			Count2 = Count1;
			//DBG_VAR( Count1 );
		}
	}
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_handler
(
  VIM_MAIN_EVENT_DATA const* event_data_ptr,
  VIM_MAIN_RETURN_DATA* return_data_ptr,
  VIM_BOOL* is_handled_ptr
)
{
  //VIM_ERROR_PTR res = VIM_ERROR_NONE;

	VIM_BOOL icc_removed = VIM_FALSE;

#ifdef _VOS2
    VIM_ERROR_RETURN_ON_FAIL(InitGUI());
#endif

  *is_handled_ptr=VIM_TRUE;
  return_data_ptr->type = event_data_ptr->type;
  /*VIM_DBG_VAR(event_data_ptr->type);*/

  //DBG_VAR( terminal.abort_transaction )
  switch(event_data_ptr->type)
  {

    case VIM_MAIN_EVENT_IDLE:
		terminal.initator = it_pinpad;

        // RDD 270913 SUN SPIRA:6860 Read unused card event
        //if( icc_handle.instance_ptr != VIM_NULL )
        //    vim_icc_is_removed(icc_handle.instance_ptr, &icc_removed );

		// RDD 220312
		if( IS_INTEGRATED )	  
		{
		    terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855
		}

		/* RDD 210312 : SPIRA 5115 Always Piggyback pending 101 logons  */
		if(( terminal.trickle_feed ) && ( terminal.logon_state == session_key_logon ))
		{
		  // RDD 251013 Can still have active stuff in txn struct when this is called from IDLE so need to wipe this
		  txn_init();
		  DBG_POINT;
		  ReversalAndSAFProcessing( SEND_NO_SAF, VIM_FALSE, VIM_TRUE, VIM_TRUE );
		  terminal.trickle_feed = VIM_FALSE;
		}

		//DBG_VAR( terminal.trickle_feed );

		// RDD 020212 - Trickle feed processing moved to immediatly after txn finished
		PostTxnProcessing( VIM_TRUE );

		// RDD 030412 - reset IDLE start time from end of post-txn processing
		IdleTimeProcessing( );

		// RDD 230212 added for deleting stale card data stored for completion
		//MaintainCardBuffer();

		//display_idle();
		terminal.initator = it_pceftpos;
		break;

    case VIM_MAIN_EVENT_POWER_ON:
      /* call the user defined power up routine */
      main_event_power_on();
	  terminal.idle_time_count = 0;
      vim_rtc_get_uptime(&terminal.idle_start_time);
      break;

    case VIM_MAIN_EVENT_KBD:
      /* reset idle timer */
      terminal.idle_time_count = 0;
      vim_rtc_get_uptime(&terminal.idle_start_time);
 
      // No keypress processing if slave mode
      if( IsSlaveMode() == VIM_FALSE )
      {
          VIM_ERROR_RETURN_ON_FAIL(main_event_kbd(event_data_ptr->data.kbd.key_code));
          vim_rtc_get_uptime(&terminal.idle_start_time);
      }
      break;

    case VIM_MAIN_EVENT_UART:
       // VIM_DBG_MESSAGE("-------------- UART ( or TCPIP ) RX DATA EVENT ---------------");
      VIM_ERROR_RETURN_ON_FAIL(main_event_uart());
      break;

    case VIM_MAIN_EVENT_MAN:
      txn_init();
      txn.card.type = card_manual;
      txn.card.data.manual.pan_length = event_data_ptr->data.man_check.pan.length;
     //VIM_DBG_VAR(txn.card.data.manual.pan_length);
      vim_mem_copy(txn.card.data.manual.pan, event_data_ptr->data.man_check.pan.buffer, VIM_MIN(VIM_PAN_BUFFER_SIZE, txn.card.data.manual.pan_length));
      vim_mem_copy(txn.card.data.manual.expiry_mm, event_data_ptr->data.man.expiry.month, VIM_SIZEOF(txn.card.data.manual.expiry_mm));
      vim_mem_copy(txn.card.data.manual.expiry_yy, event_data_ptr->data.man.expiry.year, VIM_SIZEOF(txn.card.data.manual.expiry_yy));
      txn_process();
      break;

	case VIM_MAIN_EVENT_MAG:
		vim_mag_new( &mag_handle );
		HandleSwipedPan( &txn.card );
		transaction_set_status( &txn, st_pos_sent_pci_pan );
		txn.type = tt_sale;
		txn_process();
		break;

    case VIM_MAIN_EVENT_ICC:
        txn_init();
        txn.card.type = card_chip;
#ifdef STANDALONE
        TerminalIdleDisplay(0);
#endif
        break;

    default:
		// RDD 270913 SUN SPIRA:6860 Read unused card event
		if( icc_handle.instance_ptr != VIM_NULL )
			vim_icc_is_removed(icc_handle.instance_ptr, &icc_removed );
      break;
  }
  //VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&terminal.last_activity));

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_main_callback_setup(void)
{
  VIM_MULTIAPP_CONFIG config;

  config.application_id = APP_NUMBER;
  vim_mem_copy(config.application_name, "WOW", VIM_SIZEOF("WOW"));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_main_callback
(
  VIM_MAIN_EVENT_DATA const* event_data_ptr,
  VIM_MAIN_RETURN_DATA * return_data_ptr,
  VIM_BOOL* is_handled_ptr
)
{
  return main_event_handler(event_data_ptr,return_data_ptr,is_handled_ptr);
}
/*----------------------------------------------------------------------------*/

