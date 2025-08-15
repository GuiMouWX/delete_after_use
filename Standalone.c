
#include <incs.h>
#include <vim_apps.h>
#include <vim_multiapp.h>

#define EMV_APP_LIST_MAX	9
#define EMV_APP_LIST_TITLE	"SELECT APPLICATION"
#define TERMINAL_STATUS(x) TerminalStatusS(VIM_BOOL OtherMenu)

VIM_DBG_SET_FILE("TF");
//static VIM_UINT8 page_no = 0;
// VIM_MAIN_APPLICATION_TYPE app_type = VIM_MAIN_APPLICATION_TYPE_MASTER;

extern VIM_UINT64 original_idle_time;

extern VIM_ERROR_PTR CheckPOS( VIM_BOOL CloseOldSession );

extern PREAUTH_SAF_TABLE_T preauth_saf_table;
extern VIM_NET_HANDLE gprs_ppp_handle;

extern VIM_ERROR_PTR txn_admin_shift(void);
extern VIM_ERROR_PTR SetReasonCode(void);
extern VIM_ERROR_PTR DailyHouseKeeping(void);
extern VIM_ERROR_PTR GetSettlementDate(void);


VIM_ERROR_PTR TerminalIdleDisplay(VIM_UINT8 TxnIndex);

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


const VIM_CHAR *TerminalStatusS( VIM_BOOL OtherMenu )
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

	else if (logged_on != terminal.logon_state)      			
		status_ptr = " LOGON REQUIRED ";    		
	else if (terminal_locked == terminal.logon_state)      			
		status_ptr = " CALL HELPDESK  ";  
	else if(( IS_STANDALONE ) && (PAPER_OUT))
			status_ptr = "REPLACE PAPER ROLL";  	
	else
	{
		if( terminal.training_mode )
			status_ptr = "TRAINING MODE"; 
		else
		{
			if( OtherMenu )
				status_ptr = "Admin Menu"; 
			else
			{
				status_ptr = "Main Menu"; 
			}			
		}
	}
	return status_ptr;
}


#define PINPAD_DOCKED   0xFFFF

#define BATTERY_ICON    "BATT.pcx"

#define BATTERY_CHARGING  "charging.pcx"
#define BATTERY_00_PERCENT  "battery0.pcx"
#define BATTERY_10_PERCENT  "battery10.pcx"
#define BATTERY_20_PERCENT  "battery20.pcx"
#define BATTERY_30_PERCENT  "battery30.pcx"
#define BATTERY_40_PERCENT  "battery40.pcx"
#define BATTERY_50_PERCENT  "battery50.pcx"
#define BATTERY_60_PERCENT  "battery60.pcx"
#define BATTERY_70_PERCENT  "battery70.pcx"
#define BATTERY_80_PERCENT  "battery80.pcx"
#define BATTERY_90_PERCENT  "battery90.pcx"
#define BATTERY_100_PERCENT "battery100.pcx"

#define GPRS_ICON     "SIG.pcx"

#define GPRS_100_PERCENT  "signal100.pcx"
#define GPRS_80_PERCENT   "signal80.pcx"
#define GPRS_60_PERCENT   "signal60.pcx"
#define GPRS_40_PERCENT   "signal40.pcx"
#define GPRS_20_PERCENT   "signal20.pcx"
#define GPRS_00_PERCENT   "signal0.pcx"

#define Network_3G		  "network3G.pcx"
#define Network_GPRS	  "networkGPRS.pcx"
#define Network_PSTN	  "PSTN.pcx"
#define Network_Ethernet  "Ethernet.pcx"

#define BLANK_ICON       "blank.pcx"

/*----------------------------------------------------------------------------*/
// RDD 240314 - basically we leave the backlight level at 100 when docked.
// when removed from the dock we drop it to 80 until the battery level drops to below 50% then the backlight drops with charge level, ie 
// Charge = 50% : backlight = 80%
// Charge = 40-49% : backlight = 70%  

static VIM_PERCENT GetLevel( VIM_UINT32 charge )
{
  VIM_PERCENT SteppedLevel = ( 10 * (charge / 10 ));
  if( charge >= UNDOCKED_BATTERY_THRESHOLD_LEVEL ) 
  {
    return DEFAULT_UNDOCKED_BACKLIGHT_LEVEL; 
  }
  else 
  {
    return VIM_MIN( DEFAULT_UNDOCKED_BACKLIGHT_LEVEL, SteppedLevel );
  }
}


static const VIM_CHAR *GetBattLevelFile( VIM_UINT32 charge )
{

  if( charge < 5 ) 
    return BATTERY_00_PERCENT;
  else if(( charge >= 5 ) && ( charge < 15 ))
    return BATTERY_10_PERCENT;
  else if(( charge >= 15 ) && ( charge < 25 ))
    return BATTERY_20_PERCENT;
  else if(( charge >= 25 ) && ( charge < 35 ))
    return BATTERY_30_PERCENT;
  else if(( charge >= 35 ) && ( charge < 45 ))
    return BATTERY_40_PERCENT;
  else if(( charge >= 45 ) && ( charge < 55 ))
    return BATTERY_50_PERCENT;
  else if(( charge >= 55 ) && ( charge < 65 ))
    return BATTERY_60_PERCENT;
  else if(( charge >= 65 ) && ( charge < 75 ))
    return BATTERY_70_PERCENT;
  else if(( charge >= 75 ) && ( charge < 85 ))
    return BATTERY_80_PERCENT;
  else if(( charge >= 85 ) && ( charge < 95 ))
    return BATTERY_90_PERCENT;
  else
    return BATTERY_100_PERCENT;
}


void update_battery_level_icon()
{
  VIM_UINT32 charge;
  const VIM_CHAR *FileNamePtr;
  VIM_BOOL is_docked; 
  static VIM_RTC_UPTIME last_check_time;
  VIM_RTC_UPTIME now;

  vim_rtc_get_uptime( &now );
  if( now < last_check_time + 1000 )
    return;

  last_check_time = now;

  vim_terminal_get_dock_status( &is_docked );

  if( is_docked ) 
    FileNamePtr = BATTERY_CHARGING;
  else 
  {
    vim_terminal_get_battery_charge( &charge );
    FileNamePtr = GetBattLevelFile( charge );

	//DBG_VAR(charge);
  }
  vim_screen_set_control_image( "Battery", FileNamePtr );
}

static VIM_ERROR_PTR BacklightControlViaBatteryCheck( void )
{
//  static VIM_UINT32 charge = DEFAULT_UNDOCKED_BACKLIGHT_LEVEL;
  VIM_UINT32 new_charge;

  vim_terminal_get_battery_charge( &new_charge );

  vim_lcd_backlight_level( GetLevel( new_charge ));
  return VIM_ERROR_NONE;
}

#define SHUTTING_DOWN_WAIT_TIME_SECONDS 5

VIM_BOOL cancel_shutdown()
{
  VIM_TEXT secs_remaining[5];
  VIM_ERROR_PTR res;
  VIM_KBD_CODE key_code;
  VIM_UINT8 loops = SHUTTING_DOWN_WAIT_TIME_SECONDS;

  while( loops-- )
  {
    vim_sound( VIM_TRUE );          
    vim_sprintf( secs_remaining, "%d", loops );
    display_screen( DISPLAY_SCREEN_SHUTTING_DOWN, VIM_PCEFTPOS_KEY_NONE, secs_remaining );
 		res = vim_screen_kbd_or_touch_read( &key_code, 1000, VIM_FALSE );
    if( res != VIM_ERROR_TIMEOUT )
      return VIM_TRUE;
  }
  return VIM_FALSE;
}

VIM_ERROR_PTR BatteryPowerManagement( VIM_RTC_UPTIME uptime_now )
{
  VIM_BOOL is_docked; 
  static VIM_BOOL was_docked;
  static VIM_RTC_UPTIME uptime_since_docked;

  vim_terminal_get_dock_status( &is_docked );

  if( is_docked ) 
  {    
	  // RDD 240314 - If docked set backlight to max and continually reset start timers	  
	  DBG_POINT;
	  vim_lcd_set_backlight( 100 );
	  vim_rtc_get_uptime( &uptime_since_docked );
    
	  if( !was_docked )
	  {			
		  // Disconnect from GPRS
		  //comms_force_disconnect( );
		  //terminal.configuration.TXNCommsType[0] = ETHERNET;
		  //DBG_MESSAGE( "!!!!!!!!! Set Comms to Ethernet !!!!!!!!!!" );
		  was_docked = VIM_TRUE;
	  }
  }
  else
  {   
	  if( was_docked )
	  {		
		  //DBG_MESSAGE( "!!!!!!!!! Set Comms to GPRS !!!!!!!!!!" );
		  //terminal.configuration.TXNCommsType[0] = GPRS;
		  //comms_force_disconnect( );
		  vim_lcd_backlight_level( 80 );
		  was_docked = VIM_FALSE;
	  }
  }

  // RDD 240314 - adaptive control of backlight via battery level  
  BacklightControlViaBatteryCheck( );

  // RDD 240314 - check the power-off timer - will cause more trouble than its worth IMO  
  if( terminal.power_off_timer_minutes )    
  {           
      if( 60000 * terminal.power_off_timer_minutes <= ( uptime_now - uptime_since_docked ))
      { 
        if( cancel_shutdown() )
        {
          terminal.idle_time_count = 0;
          vim_rtc_get_uptime( &uptime_since_docked );
        }
        else
        {
          pceft_debug( pceftpos_handle.instance, "Shutting Down due to Power Off Timer for undocked PinPad" );
          vim_event_sleep(1000);
          vim_terminal_shutdown();
        }
      }    
  }  

  if( terminal.backlight_off_timer )
  {            
	  if( 1000 * terminal.backlight_off_timer <= ( uptime_now - uptime_since_docked ) )                
	  {                   
		  vim_lcd_set_backlight( 5 );                
	  }
  }
  return VIM_ERROR_NONE;
}



#if 0

    if( VIM_ERROR_NONE != vim_gprs_get_info( &gprs_info ) )
    {
      vim_strcpy( gprs_info.imei, "N/A" );
      vim_strcpy( gprs_info.iccid, "N/A" );
      gprs_info.con_network = ' ';
    }

    if( gprs_info.con_network == '1' || gprs_info.con_network == '2' )
      con_network = '2';
    else if( gprs_info.con_network == '3' || gprs_info.con_network == '4' || gprs_info.con_network == '5' )
      con_network = '3';
    vim_snprintf( block4, VIM_SIZEOF(block4), 
        "4. GPRS = %s:%s\n" \
        "   %s %s\n" \
        "   IMEI: %s\n" \
        "   ICCID %s\n" \
        "   %cG, signal: %d%%%%\n",
                                terminal.acquirers[0].AQIPTRANS.IPADDR, ip_port,
                                terminal.acquirers[0].AQGPRSAPN, sha,
                                gprs_info.imei,
                                gprs_info.iccid,
                                con_network, gprs_info.signal_level_percent );
#endif


VIM_TEXT *current_icon_file_name = BLANK_ICON;  	
VIM_TEXT *network_icon_file_name = BLANK_ICON;

VIM_ERROR_PTR SetGPRSIcons( void )
{
	VIM_GPRS_INFO gprs_info;  
	static VIM_RTC_UPTIME last_check_time;  
	VIM_RTC_UPTIME now;

	vim_rtc_get_uptime(&now);  
//	DBG_POINT;
	// 10 seconds interval for GPRS signal update    
	if( now < last_check_time + 10000L )  
		return VIM_ERROR_NONE;
	last_check_time = now;
	
	//DBG_POINT;
	if( vim_gprs_get_info( &gprs_info ) == VIM_ERROR_NONE )    
	{
		//DBG_POINT;
		if( gprs_info.signal_level_percent > 80 )        		
			current_icon_file_name = GPRS_100_PERCENT;      			
		else if( gprs_info.signal_level_percent > 60 )        				
			current_icon_file_name = GPRS_80_PERCENT;      			
		else if( gprs_info.signal_level_percent > 40 )        				
			current_icon_file_name = GPRS_60_PERCENT;      			
		else if( gprs_info.signal_level_percent > 20 )        				
			current_icon_file_name = GPRS_40_PERCENT;      	
		else if( gprs_info.signal_level_percent > 0 )        				
			current_icon_file_name = GPRS_20_PERCENT;      			
		else        				
			current_icon_file_name = GPRS_00_PERCENT;		
	}
	//DBG_STRING( current_icon_file_name );	  		
	//VIM_DBG_VAR( gprs_info.con_network );	  		
	if( gprs_info.con_network >= '3' )	  		
		network_icon_file_name = Network_3G;	  		
	else if( gprs_info.con_network == '2' )		  
		network_icon_file_name = Network_GPRS;		
	else 
		network_icon_file_name = BLANK_ICON;
	//DBG_STRING( network_icon_file_name );
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR update_signal_level_icon( void )
{
  static VIM_BOOL Docked;
    
  VIM_NET_CE_INFO Handles;

  // RDD - if already set to PSTN then leave this setting as it was done deliberatly
  Docked = IsDocked();
  //DBG_VAR( Docked );
  vim_net_nwif_get_handles( &Handles );	
  //DBG_VAR( Handles.ETHHandle.Connected );
  //DBG_VAR( Handles.ETHHandle.Handle );
  //DBG_VAR( Handles.GPRSHandle.Connected );
  //DBG_VAR( Handles.GPRSHandle.Handle );

  if(( terminal.configuration.TXNCommsType[0] == PSTN ) && Docked )
  {
	  current_icon_file_name = BLANK_ICON;  	
	  network_icon_file_name = Network_PSTN;	  
  }
  else if(( Handles.ETHHandle.Connected ) && ( Handles.ETHHandle.Handle >=0 ) && ( Docked ))
  {
	  terminal.configuration.TXNCommsType[0] = ETHERNET;	
	  terminal.configuration.TMSCommsType[0] = ETHERNET;				
	  //DBG_MESSAGE( "Ethernet Connected - Use Ethernet as PRIMARY" );
	  network_icon_file_name = Network_Ethernet;
	  current_icon_file_name = BLANK_ICON;  	
  }
  else if(( Handles.GPRSHandle.Connected )&&( Handles.GPRSHandle.Handle >=0 ))	  
  {		 
	  terminal.configuration.TXNCommsType[0] = GPRS;		  	
	  terminal.configuration.TMSCommsType[0] = GPRS;					  		  
	  //DBG_MESSAGE( "No Ethernet, but GPRS OK - Changing Primary Comms to GPRS" );	 
	  SetGPRSIcons( );
  }	  
  //DBG_STRING( current_icon_file_name );
  //DBG_STRING( network_icon_file_name );
  vim_screen_set_control_image( "Signal", current_icon_file_name );
  vim_screen_set_control_image( "Network", network_icon_file_name );
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

VIM_ERROR_PTR display_status_s( VIM_CHAR *status_string_ptr, VIM_CHAR *logon_status_string )
{		  
	// Battery Icon is only for VX680 or VX690
	if( IsTerminalType( "VX820" ))
		vim_screen_set_control_image( "Battery", BLANK_ICON );
	else
		update_battery_level_icon();

	// Network and Signal ICONS are for Non-PcEftpos mode ( lite or full standalone )
	if( 0 )		
	{			
		vim_screen_set_control_image( "Signal", BLANK_ICON );  
		vim_screen_set_control_image( "Network", BLANK_ICON ); 
	}	
	else
	{		  
		update_signal_level_icon();  
	}

	if( IS_STANDALONE )
		terminal.IdleScreen = DISP_IDLE;
	else
		terminal.IdleScreen = DISP_IDLE_PCEFT;	

	VIM_ERROR_RETURN_ON_FAIL( display_screen( terminal.IdleScreen, VIM_PCEFTPOS_KEY_NONE, status_string_ptr, logon_status_string ));	  
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_reversal_saf_plus(void)
{
  VIM_TEXT saf_rev_status[3+1];
  VIM_BOOL flag = VIM_FALSE;

  saf_rev_status[0] = ' ';
  saf_rev_status[1] = ' ';
  saf_rev_status[2] = ' ';
  saf_rev_status[3] = 0x00;

  if( reversal_present() )
  {
    saf_rev_status[0] = '+';
    flag = VIM_TRUE;
  }
  if (saf_totals.fields.saf_count)
  {
    saf_rev_status[1] = '*';
    flag = VIM_TRUE;
  }

  if( !terminal_get_status( ss_no_saf_prints ))
  {
	if (saf_totals.fields.saf_print_count)
	{
	  saf_rev_status[2] = '#';
		flag = VIM_TRUE;
	}
  }

  if( flag )
  {
		if (use_vim_screen_functions)
		{
			return vim_screen_display_terminal("Status", saf_rev_status);
		}
		else
		{
			VIM_GRX_VECTOR temp_grx_vector;
			VIM_ERROR_RETURN_ON_FAIL(vim_lcd_set_font(&vim_grx_font_ascii_6x10_bold));
			VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_size_in_pixels(&temp_grx_vector));
			//VIM_ERROR_RETURN_ON_FAIL(vim_lcd_put_text_at_pixel(vim_grx_vector(temp_grx_vector.x-18,0),saf_rev_status));
			VIM_ERROR_RETURN_ON_FAIL(vim_lcd_put_text_at_pixel(vim_grx_vector(temp_grx_vector.x-240,0),saf_rev_status));
		}
  }
  
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/




VIM_ERROR_PTR is_ready_s(void)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE; 

	if( IS_STANDALONE )
	{
		if(( res = vim_prn_get_status()) != VIM_ERROR_NONE )
		{
			DBG_MESSAGE( "<<< PRINTER ERROR >>>" );
#ifndef _DEBUG
			return res;  
#endif
		}
	}
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

static VIM_ERROR_PTR txn_settlement( void )
{
  /* Init the Txn structure */

  settle.type = tt_settle;

  VIM_ERROR_RETURN_ON_FAIL(GetSettlementDate());

  if( settle_terminal() != VIM_ERROR_NONE )
  {	  				
	  display_result( settle.error, settle.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);	  				
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR txn_presettlement( void )
{
  /* Init the Txn structure */
  settle.type = tt_presettle;
  settle_terminal();
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR txn_lastsettlement( void )
{
  /* Init the Txn structure */
  settle.type = tt_last_settle;
  VIM_ERROR_RETURN_ON_FAIL(settle_terminal());
  /* return without error */
  return VIM_ERROR_NONE;
}



#define MAX_STANDALONE_TXN_TYPES 20
#define TXN_LABEL_LEN			 20+1

VIM_BOOL MrchTxnTypeIsEnabled(txn_type_t t_type)
{
	switch (t_type)
	{
	case tt_refund:
		DBG_POINT;
		return (TERM_REFUND_ENABLE);

	case tt_cashout:
		DBG_POINT;
		return(TERM_CASHOUT_MAX > 0);

	case tt_completion:
	case tt_checkout:
		DBG_POINT;
		return(TERM_CHECKOUT_ENABLE & TERM_ALLOW_PREAUTH);

		// RDD 010322 JIRA PIRPIN-1449
	case tt_preauth_enquiry: 
		return(TERM_ALLOW_PREAUTH);

	case tt_preauth:
		DBG_POINT;
		return(TERM_ALLOW_PREAUTH);

	case tt_moto:
		DBG_POINT;
		return(TERM_ALLOW_MOTO_ECOM);

	case tt_moto_refund:
		DBG_POINT;
		return(TERM_ALLOW_MOTO_ECOM & TERM_REFUND_ENABLE);

	default:
		DBG_POINT;
		break;
	}
	return VIM_TRUE;
}




VIM_ERROR_PTR ProcessMenuItem( txn_type_t txn_type )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	txn_init();
	txn.type = txn_type;

	switch( txn.type )
	{
		case tt_settle:
		res = txn_settlement();		
		break;

		case tt_presettle:
		res = txn_presettlement();		
		break;

		case tt_last_settle:
			res = txn_lastsettlement();
			break;

		case tt_logon:
		if( terminal.logon_state == logged_on ) 		
			terminal.logon_state = logon_required;	

		// RDD 121114 SPIRA:8248
		vim_pceftpos_callback_generic_initialisation( terminal.logon_state );
		//logon_request( VIM_TRUE );									
		//logon_finalisation( VIM_FALSE );		
		break;

		case tt_reprint_receipt:
		res = txn_admin_copy();
		break;

		case tt_shift_totals:
		res = txn_admin_shift();		
		break;
	
		case tt_tms_logon:
		res = tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE, VIM_FALSE, web_then_vim );
		break;

		case tt_rsa_logon:
			terminal.logon_state = rsa_logon;
		//logon_request( VIM_TRUE );									
		//logon_finalisation( VIM_FALSE );

		// RDD 121114 SPIRA:8248
		vim_pceftpos_callback_generic_initialisation( terminal.logon_state );
		break;

		case tt_main_menu:
			TerminalIdleDisplay(0);
			return VIM_ERROR_NONE;;

		case tt_moto:
		case tt_moto_refund:
			transaction_set_status(&txn, st_moto_txn);
			// intentional drop through...

		default:
		res = txn_process();
	}
	if (res != VIM_ERROR_NONE )
	{
		//display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY );
	}
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR PurchaseTxn(void)
{
    VIM_ERROR_RETURN( ProcessMenuItem(tt_sale));
}

VIM_ERROR_PTR RefundTxn(void)
{
    VIM_ERROR_RETURN( ProcessMenuItem(tt_refund));
}

VIM_ERROR_PTR CashOutTxn(void)
{
    VIM_ERROR_RETURN( ProcessMenuItem(tt_cashout));
}

VIM_ERROR_PTR MoToSaleTxn(void)
{
    VIM_ERROR_RETURN( ProcessMenuItem(tt_moto));
}

VIM_ERROR_PTR MoToRefundTxn(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_moto_refund));
}


S_MENU MoToMenu[2] = {
    { "MoTo Sale", MoToSaleTxn },
    { "MoTo Refund", MoToRefundTxn }
};

VIM_ERROR_PTR MoToMenuRun(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(MoToMenu, 2, "MoTo Menu", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
	return res;
}



S_MENU EFTMenu[4] = {
    { "Purchase", PurchaseTxn },
    { "Cash Out", CashOutTxn },
	{ "Refund", RefundTxn },
	{ "MOTO", MoToMenuRun }
};


VIM_ERROR_PTR EFTMenuRun(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(EFTMenu, 4, "EFT Menu", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
	return res;
}



VIM_ERROR_PTR Loyalty(void)
{
    txn.card_state = card_capture_picc_apdu;
    // RDD 291217 Wipe previous saved loyalty number for XML reporting purposes
    vim_file_delete(LOYALTY_FILE);
	VIM_ERROR_RETURN(ProcessMenuItem(tt_query_card));
}


VIM_ERROR_PTR Activate(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_gift_card_activation));
}


VIM_ERROR_PTR Redeem(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_sale));
}


VIM_ERROR_PTR Balance(void)
{
    transaction_set_status(&txn, st_giftcard_s_type);           // New flag for S-type gift cards
	VIM_ERROR_RETURN(ProcessMenuItem(tt_balance));
}

S_MENU GiftCardMenu[3] = {
    { "Activation", Activate },
    { "Redemption", Redeem },
    { "Balance", Balance }
};



VIM_ERROR_PTR GiftCard(void)
{
    VIM_ERROR_PTR res;
    
    while ((res = MenuRun(GiftCardMenu, 3, "Gift Card Menu", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE)
    {
        DBG_POINT;
        txn_init();
    }
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
	return res;
}



VIM_ERROR_PTR PreAuth(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_preauth));
}

VIM_ERROR_PTR Completion(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_checkout));
}

VIM_ERROR_PTR PreAuthEnq(void)
{
    VIM_ERROR_RETURN( ProcessMenuItem(tt_preauth_enquiry));
}



S_MENU HotelMenu[3] = {
    { "Pre Auth", PreAuth },
    { "Completion", Completion },
    { "PA Enquiry", PreAuthEnq }
};


VIM_ERROR_PTR Hotel(void)
{
    VIM_ERROR_PTR res;
    
    while ((res = MenuRun(HotelMenu, 3, "Check In/Check Out", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE)
    {
        DBG_POINT;
        txn_init();
    }
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
	return res;
}




VIM_ERROR_PTR Reprint(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_reprint_receipt));
}

VIM_ERROR_PTR ShiftTotals(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_shift_totals));
}

VIM_ERROR_PTR PreSettle(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_presettle));
}

VIM_ERROR_PTR Settle(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_settle));
}

VIM_ERROR_PTR Logon(void)
{
	VIM_ERROR_RETURN( ProcessMenuItem(tt_logon));
}

VIM_ERROR_PTR TMSLogon(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_tms_logon));
}

VIM_ERROR_PTR RSALogon(void)
{
	VIM_ERROR_RETURN(ProcessMenuItem(tt_rsa_logon));
}





S_MENU AdminMenu[7] = {
    { "Reprint Receipt", Reprint },
	{ "Settle Totals", Settle },
	{ "Pre-Settle", PreSettle },
	{ "Shift Totals", ShiftTotals },
    { "EFT Logon", Logon },
	{ "RSA Logon", RSALogon },
	{ "TMS Logon", TMSLogon }
};



VIM_ERROR_PTR Admin(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(AdminMenu, VIM_LENGTH_OF(AdminMenu), "Admin", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE)
    {
        txn_init();
    }
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
	return res;
}

#if 0
void AliPay(void)
{
    txn.original_type = tt_alipay;
    txn.card_state = card_capture_picc_apdu;
    // RDD 291217 Wipe previous saved loyalty number for XML reporting purposes
    vim_file_delete(LOYALTY_FILE);
    ProcessMenuItem(tt_query_card);
}
#endif


#define LIST_MAX 6





S_MENU MainMenu0[5] = {
	{ "EFT", EFTMenuRun },
	{ "Gift Cards", GiftCard },
	{ "Loyalty", Loyalty },
	{ "Hotel", Hotel },
	{ "Admin", Admin }
};


S_MENU MainMenu[4] = {
	{ "Purchase", PurchaseTxn },
	{ "Cash Out", CashOutTxn },
	{ "Refund", RefundTxn },
	{ "MOTO", MoToMenuRun }
};


#if 1

VIM_ERROR_PTR IconMenu(void)
{
	// Index contains the MID for the Image
	//static VIM_SIZE ItemIndex = 0;
	//VIM_SIZE SelectedIndex = 0;
	//VIM_CHAR index[LIST_MAX][20];
	//VIM_CHAR *indexList[LIST_MAX];
	//VIM_SIZE r;
	VIM_ERROR_PTR res;
	while ((res = MenuRun(MainMenu, VIM_LENGTH_OF(MainMenu), "Main Menu", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
	return res;
}


VIM_ERROR_PTR TerminalIdleDisplay(VIM_UINT8 TxnIndex)
{
	transaction_t transaction;
	txn_type_t StandaloneTxnList[] = { tt_sale, tt_cashout, tt_refund, tt_preauth, tt_checkout, tt_preauth_enquiry, tt_moto, tt_moto_refund, tt_other };
	txn_type_t OtherMenuTxnList[] = { tt_reprint_receipt, tt_settle, tt_presettle, tt_shift_totals, tt_logon, tt_rsa_logon, tt_tms_logon, tt_main_menu };
	txn_type_t EnabledTxnList[MAX_STANDALONE_TXN_TYPES];
	//txn_type_t SeletedTxn = tt_none;
	VIM_SIZE SelectedIndex = 0xFFFF;
	VIM_SIZE i, EnabledTxns = 0;
	/* static */ VIM_SIZE ItemIndex = 0; // RDD - make this static to preserve the last menu list that was run
	//VIM_ERROR_PTR res;
	VIM_CHAR labels[TXN_LABEL_LEN][MAX_STANDALONE_TXN_TYPES];
	VIM_CHAR *items[MAX_STANDALONE_TXN_TYPES];
	//VIM_CHAR status_ptr[25];
	VIM_BOOL OtherMenu = VIM_FALSE;
	//VIM_BOOL IsReady = VIM_FALSE;

	vim_mem_clear(labels, TXN_LABEL_LEN * MAX_STANDALONE_TXN_TYPES);
	vim_mem_clear(EnabledTxnList, VIM_SIZEOF(EnabledTxnList));

	if( !is_integrated_mode() )
	{
		if (PAPER_OUT)
		{
			IdleDisplay();
			return VIM_ERROR_NONE;
		}

		// RDD - Get the text descriptors for each enabled transaction
		for (i = 0; i < VIM_LENGTH_OF(StandaloneTxnList); i++)
		{
			transaction.type = StandaloneTxnList[i];
			transaction.status = 0;

			if (MrchTxnTypeIsEnabled(transaction.type))
			{
				get_txn_type_string(&transaction, &labels[EnabledTxns][0], TXN_LABEL_LEN, VIM_FALSE);
				EnabledTxnList[EnabledTxns] = transaction.type;
				items[EnabledTxns] = &labels[EnabledTxns][0];
				// RDD 281014 - UI change req. don't display anything for tt_none
				//if( vim_strlen(items[EnabledTxns]))
				EnabledTxns += 1;
			}
		}
	}
	if (!TxnIndex)
	{
#ifdef _VOS2
		VIM_ERROR_RETURN_ON_FAIL(display_menu_select_zero_based(TerminalStatusS(0), ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, EnabledTxns, 10000, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE));
		SelectedIndex -= 1;
#else
		VIM_ERROR_RETURN_ON_FAIL(display_menu_select(TerminalStatusS(0), &ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, EnabledTxns, 10000, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE));
#endif
		VIM_DBG_NUMBER(SelectedIndex);
		// Happy hack hack
#ifdef _WIN32
		if (SelectedIndex == 10)
#else
		if (SelectedIndex == 9)
#endif
		{
			DBG_POINT;
			TxnIndex = 10;
		}
	}
	else
	{
		SelectedIndex = TxnIndex - 1;
	}

	txn_init();
	if ((TxnIndex == 10) || (EnabledTxnList[SelectedIndex] == tt_other) || OtherMenu)
	{
		DBG_POINT;
		// RDD - Get the text descriptors for each enabled transaction
		vim_mem_clear(labels, TXN_LABEL_LEN * MAX_STANDALONE_TXN_TYPES);
		EnabledTxns = 0;
		for (i = 0; i < VIM_LENGTH_OF(OtherMenuTxnList); i++)
		{
			transaction.type = OtherMenuTxnList[i];
			transaction.status = 0;
			ItemIndex = 0;

			get_txn_type_string(&transaction, &labels[EnabledTxns][0], TXN_LABEL_LEN, VIM_FALSE);
			EnabledTxnList[EnabledTxns] = transaction.type;
			// Items is an array of pointers
			items[i] = &labels[EnabledTxns][0];
			EnabledTxns += 1;
		}

#ifdef _VOS2
		DBG_POINT;
		VIM_ERROR_RETURN_ON_FAIL(display_menu_select_zero_based(TerminalStatusS(1), ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, EnabledTxns, 10000, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE));
		SelectedIndex -= 1;
#else
		VIM_ERROR_RETURN_ON_FAIL(display_menu_select(TerminalStatusS(1), &ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, EnabledTxns, 10000, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE));
#endif
		VIM_ERROR_RETURN_ON_FAIL(ProcessMenuItem(EnabledTxnList[SelectedIndex]));
	}

	// Don't allow cashout to be selected if max is zero or not present
	else if ((EnabledTxnList[SelectedIndex] == tt_cashout) && (!terminal.configuration.cashout_max))
	{
		DBG_POINT;
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "", "Cash Out\nDisabled");
		vim_event_wait(VIM_NULL, 0, 2000);
		return VIM_ERROR_NONE;
	}
	else
	{
		DBG_POINT;
		VIM_ERROR_RETURN_ON_FAIL(ProcessMenuItem(EnabledTxnList[SelectedIndex]));
	}
	return VIM_ERROR_NONE;
}






#else

VIM_ERROR_PTR IconMenu(void)
{
	// Index contains the MID for the Image
	static VIM_SIZE ItemIndex = 0;
	VIM_SIZE SelectedIndex = 0;
	VIM_CHAR index[LIST_MAX][20];
	VIM_CHAR *indexList[LIST_MAX];
	VIM_SIZE r;

#ifdef _WIN32
	VIM_ERROR_PTR res;
	while ((res = MenuRun( MainMenu0, VIM_LENGTH_OF(MainMenu0), "Main Menu", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
	return res;

	//    display_menu_select("Main Menu", &ItemIndex, &SelectedIndex, &indexList[0], LIST_MAX, ENTRY_TIMEOUT, VIM_TRUE, VIM_TRUE, VIM_FALSE, VIM_FALSE);
	//    SelectedIndex += 1; // ADK EMV expect one-based index
#else
	for (r = 0; r < LIST_MAX; r++)
	{
		vim_mem_clear(index[r], VIM_SIZEOF(index[r]));
		vim_sprintf(index[r], "img\\%2.2d.png", r);
		indexList[r] = &index[r][0];
		VIM_DBG_STRING(index[r]);
		VIM_DBG_STRING(indexList[0]);
	}

	slave_adkgui_menu_icon("Main Menu", &ItemIndex, &SelectedIndex, &indexList[0], LIST_MAX, ENTRY_TIMEOUT, VIM_TRUE, VIM_FALSE, VIM_FALSE, "I_Menu.html", VIM_NULL, 0);
#endif
	VIM_DBG_NUMBER(SelectedIndex);
	txn_init();

	switch (SelectedIndex)
	{
	case 1:
		EFTMenuRun();
		break;

	case 2:
		GiftCard();
		break;

	case 3:
		Loyalty();
		break;

	case 4:
		Hotel();
		break;

	case 5:
		Admin();
		break;

	case 6:
		//AliPay();
		break;

	default:
		break;
	}
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR TerminalIdleDisplay( VIM_UINT8 TxnIndex )
{
    if (!TxnIndex)
        IconMenu();
    else
    {
        // Hotkey Transactions
        switch (TxnIndex)
        {
        case 1:
            ProcessMenuItem(tt_sale);
            break;

        default:
            vim_sound(VIM_TRUE);
            break;
        }
    }
    return VIM_ERROR_NONE;
}

#endif


static VIM_BOOL TimeSet = VIM_FALSE;




/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR display_idle_lock( VIM_SEMAPHORE_TOKEN_HANDLE* lcd_lock_ptr )
{
  VIM_TEXT status_string[50];
  VIM_TEXT logon_status_string[50];
  VIM_BOOL rotate_screens = VIM_FALSE;
  static VIM_RTC_TIME current_time, saved_time;
  VIM_TEXT *status_string_ptr = status_string;
  //VIM_SIZE rev = REVISION;

  //DBG_POINT;
	 

  vim_sprintf( status_string, "v%d.%d\n%s", BUILD_NUMBER, REVISION, TerminalStatusS(0) );

  if( terminal_get_status( ss_offline_test_mode ) )
  {
	vim_strcat( status_string, "\nOFFLINE MODE" );
	terminal.allow_ads = VIM_FALSE;
  }

  vim_sprintf( logon_status_string, "" );
  txn.error = VIM_ERROR_NONE;
 
  /* lock the display (prevent updates)*/
  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_lock_update(lcd_lock_ptr));

 
  if( rotate_screens == VIM_FALSE )
  {
	  VIM_ERROR_RETURN_ON_FAIL( display_status_s( status_string, logon_status_string ) );
#ifdef _DEBUG        
		  /* display "+" and/or "*" in corner to denote non-empty SAF or REVERSAL pending */
		  display_reversal_saf_plus();
#endif
  }
  // If the status indicates that the terminal is ready for transactions (normal operating mode) then run the Ad Screens
  else
  {
	  VIM_CHAR Message[50];
	  vim_mem_clear( Message, VIM_SIZEOF(Message) );	  
	  vim_rtc_get_time(&current_time);
	  if( current_time.second/terminal.screen_display_time != saved_time.second/terminal.screen_display_time )
	  {
		  VIM_ERROR_PTR res;
		  TimeSet = VIM_FALSE;
		  if( terminal.screen_id > WOW_MAX_SCREENS ) 
		  {
			  terminal_clear_status( ss_report_branding_status );			
			  terminal.screen_id = 0; 
		  }

		  switch( terminal.screen_id )		
		  {			  
			case 0:
			if((  res = display_screen( DISP_BRANDING, VIM_PCEFTPOS_KEY_NONE )) != VIM_ERROR_NONE )
			{
				status_string_ptr = status_string;	  
				VIM_ERROR_RETURN_ON_FAIL( display_status_s( status_string_ptr, logon_status_string ) );
#ifdef _DEBUG
		        /* display "+" and/or "*" in corner to denote non-empty SAF or REVERSAL pending */
			    display_reversal_saf_plus();
#endif
			}
			else
			{
				terminal.screen_id +=1;
			}
			break;
		
			default:		
					//DBG_POINT;

				CheckMemory( VIM_FALSE );
#if 0
				if( terminal.screen_id == MAX_NUMBER_OF_ADS )
				{
					DBG_MESSAGE( "--------- MAX ADS REACHED ---------" );
					DBG_VAR( terminal.screen_id );
					terminal.screen_id = 0;
					break;
				}
#endif
			
			if(( res = display_screen( DISP_GENERIC_00+(terminal.screen_id-1), VIM_PCEFTPOS_KEY_NONE)) != VIM_ERROR_NONE )
			{
				// we reached the end of the advertising	
				//DBG_POINT;

				CheckMemory( VIM_FALSE );

				if( terminal_get_status( ss_report_branding_status ))
				{
					vim_sprintf( Message, "Advert %d Not Displayed", terminal.screen_id );
					terminal_clear_status( ss_report_branding_status );			
					pceft_debug_error(pceftpos_handle.instance, Message, res );
				}
					
				terminal.screen_id = 0;

				TimeSet = VIM_TRUE;					  
			}
			else	
			{
				// Halve the display time if the number of ads exceeds 5
					//DBG_POINT;

				CheckMemory( VIM_FALSE );
				if( terminal.screen_id++ > (MAX_NUMBER_OF_ADS/2) )
					terminal.screen_display_time = WOW_AD_SCREEN_DISPLAY_TIME/2;

				if( terminal_get_status( ss_report_branding_status ))
				{
					vim_sprintf( Message, "Advert %d Displayed OK", (terminal.screen_id)-1 );
					pceft_debug( pceftpos_handle.instance, Message );
				}
			}
			break;
		 }
	  }	
	  if( TimeSet == VIM_FALSE )
	  {
		saved_time = current_time;
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





VIM_ERROR_PTR display_idleS(void)
{
  VIM_SEMAPHORE_TOKEN_HANDLE lcd_lock;

  VIM_ERROR_PTR error;
  //VIM_TMS_DOWNLOAD scheduled_type;
  //VIM_BOOL need_tms;

  VIM_DBG_POINT;

  // RDD 090212 - bugfix : Backlight sometimes drops so make 100% in IDLE
  vim_lcd_set_backlight( 100 );

  {	
	  terminal.IdleScreen = ( terminal.IdleScreen == DISP_IDLE ) ? DISP_IDLE2 : DISP_IDLE;
  }
  /* display idle screen */
  error=display_idle_lock(&lcd_lock);
  /* ensure that screen is unlocked */
  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));
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

  if (VIM_ERROR_NONE == is_ready_s())
  {
    if( terminal.logon_state != logged_on )
    {
	  if( terminal.logon_state == file_update_required )
	  {
		  // if file update was interrupted then return to normal logon as version number won't have been updated
		  terminal.logon_state = logon_required;
	  }
      /* check logon status */
    }
  }
  
  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR handle_key_press_s(VIM_KBD_CODE key_code)
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

  switch(key_code)
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
			 VIM_ERROR_RETURN_ON_FAIL( TerminalIdleDisplay(key_code - '0'));
		 }
		break;

	 case VIM_KBD_CODE_KEY0:
	  if( IS_STANDALONE )
			VIM_ERROR_RETURN_ON_FAIL( TerminalIdleDisplay( 10 ));
		//VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());
		break;

	 case VIM_KBD_CODE_FEED:   // Paper Feed 
      // receipt_paper_feed();
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
          vim_mag_flush(mag_handle.instance_ptr);
        }
        VIM_ERROR_RETURN_ON_FAIL(error);
      }
        break;

     case VIM_KBD_CODE_OK:
	  if( IS_STANDALONE )
			VIM_ERROR_RETURN_ON_FAIL( TerminalIdleDisplay(0) );
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
		  if( txn.type == tt_preswipe )
		  break;
     default:
       /*VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());*/
  break;
  }
  
  display_idleS();
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_kbd
(
  VIM_KBD_CODE key_code
)
{
#if 0
    if (terminal_get_status(ss_locked))
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_kbd_insert_key(key_code));
      if (password_compare(DISP_IDLE_LOCKED,  terminal.configuration.password, "  ", VIM_FALSE) == VIM_ERROR_NONE)
      {
        VIM_ERROR_RETURN_ON_FAIL(terminal_clear_status(ss_locked));
        //VIM_ERROR_RETURN_ON_FAIL(terminal_save());
      }
    }
    else
#endif
    {
      /*VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());*/
      /* process key press */
      VIM_ERROR_RETURN(handle_key_press_s(key_code));
    }


  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR main_event_uart(void)
{
	//DBG_MESSAGE( "~~~~~~~~ UART Event Occurred ~~~~~~~~~" );
	if( pceftpos_handle.instance != VIM_NULL )
    {
		vim_pceftpos_process_pending_message_at_idle(pceftpos_handle.instance);
    }
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR ReversalAndSAFProcessingS( VIM_SIZE SAFSendCount, VIM_BOOL SendReversal, VIM_BOOL TerminalDisplay, VIM_BOOL DownloadFiles )
{
  VIM_ERROR_PTR error = VIM_ERROR_NONE;
  VIM_BOOL execute_logon = VIM_FALSE;

  init.error = VIM_ERROR_NONE;
  //VIM_EVENT_FLAG_PTR pceftpos_connected_ptr;

  if( !terminal_get_status( ss_tid_configured ))
    return ERR_CONFIGURATION_REQUIRED;     

  // RDD - put back for target when we have keys
  if( terminal.logon_state == initial_keys_required )
    return ERR_NO_KEYS_LOADED;

  // DBG_MESSAGE( "<<<<<<<< REVERSAL/SAF/LOGON PROCESSING >>>>>>>>>>");

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

#if 1 // RDD 240412: I put this in on BD req. now taking it out again as it's not in the spec
  // RDD 260412 SPIRA 5367 - no embedded RSA logon 
  if(( terminal.logon_state == rsa_logon ) && ( txn.type != tt_logon ))
  {
	  return ERR_NOT_LOGGED_ON;  
  }
#endif

  if( terminal.logon_state != logged_on )
  {
	 execute_logon = VIM_TRUE;
	 // Do a logon request in the background
	 logon_request( DownloadFiles );

	 // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error	  	
	 if(( init.error == VIM_ERROR_NO_LINK ) || ( init.error == ERR_NO_RESPONSE ) || ( init.error == VIM_ERROR_NO_EFT_SERVER ) || ( init.error == VIM_ERROR_MODEM ) || ( init.error == VIM_ERROR_CONNECT_TIMEOUT ))
	 {
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
  // RDD 230212 SPIRA4824 - abort SAF if error
  VIM_ERROR_RETURN_ON_FAIL(error);

  /* Check for trickle feeding 220s (if required - depends on caller )  */
  		
  if(( saf_totals.fields.saf_count )&&( SAFSendCount ))
  {
	  // DBG_MESSAGE("<<<<<<<<< SAFs to Send.... Sending SAF >>>>>>>>>>>>>");
	  VIM_ERROR_RETURN_ON_FAIL( saf_send( VIM_FALSE, SAFSendCount, VIM_FALSE, VIM_FALSE ));
  }           

  // RDD 290312 : SPIRA 5168 - All PinPad intiated downloads should be interruptable
  if( terminal.initator == it_pceftpos )
	vim_pceftpos_enable_cancel_on_message(pceftpos_handle.instance, VIM_FALSE);
  return error;
}



VIM_ERROR_PTR PostTxnProcessingS( void )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	pceft_pos_display_close();


	// RDD 110615 SPIRA:8753 Contactless Issues when reader not fully closed after a txn.		
	if( txn.type != tt_none )		
	{			
	
		// RDD 251013 Can still have active stuff in txn struct when this is called from IDLE so need to wipe this	  		
		txn_init();
	}

	if( terminal.trickle_feed )
	{
		VIM_RTC_UPTIME trickle_feed_start_time, trickle_feed_timer;
		//DBG_MESSAGE( "!!! TRICKLE FEED PROCESSING !!!");
		vim_rtc_get_uptime(&trickle_feed_start_time);
		vim_rtc_get_uptime(&trickle_feed_timer );	

		terminal.trickle_feed = VIM_FALSE;
		//while((( trickle_feed_timer - trickle_feed_start_time ) < WOW_TRICKLE_FEED_TIME_SLOT ) && (( saf_totals.fields.saf_count )||( reversal_present() )))
		if(1)
		{

			// attempt to trickle feed one transaction or a signature rejection reversal
			if(( res = ReversalAndSAFProcessingS( TRICKLE_FEED_COUNT, VIM_TRUE, VIM_TRUE, VIM_TRUE )) != VIM_ERROR_NONE )
			{
				VIM_DBG_ERROR(res);
				//terminal.allow_ads = VIM_FALSE;
			}
			vim_rtc_get_uptime(&trickle_feed_timer );
		}
		terminal.idle_time_count = 0;
	    vim_rtc_get_uptime( &terminal.idle_start_time );

		// piggy back has completed sucessfully so allow ads to be displayed
		//terminal.allow_ads = VIM_TRUE;
		terminal.screen_id = 0;			
	}
	// RDD 300812 - Switch ads on after the first txn, NOT after first trickle feed
#if 0
	if( txn.type != tt_query_card )
		terminal.allow_ads = VIM_TRUE;
#endif
	// RDD 230215 SPIRA:8425 In standalone ensure that TCPIP Connection is closed after trickle feed has been completed
	if( IS_STANDALONE )
	{
		comms_disconnect();
	}   
	return res;
}


static void UpdateTerminalFlags( void )
{
	VIM_BOOL new_saf_status = saf_full(VIM_FALSE);
	VIM_BOOL old_saf_status = terminal_get_status(ss_saf_full);

	if( new_saf_status != old_saf_status )
	{
		// Saf status has changed so need to reset CTLS floot limits
		if( new_saf_status )
			terminal_set_status( ss_saf_full );
		else
			terminal_clear_status( ss_saf_full );
		terminal_set_status(ss_reset_ctls_floor_limits);
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





VIM_ERROR_PTR IdleTimeProcessingS( void )
{
	VIM_RTC_UPTIME idle_now_time;
	VIM_RTC_TIME time;
	VIM_SIZE ready_timeout_seconds = 1;
	VIM_SIZE ready_long_timeout_seconds = IS_STANDALONE ? 2 : 2;
	VIM_BOOL CardPresent = VIM_FALSE;

#ifdef _DEBUG
	terminal.max_idle_time_seconds = 20;
#endif
	/* get timer */
	vim_rtc_get_time( &time );

	vim_rtc_get_uptime(&idle_now_time);
	terminal.idle_time_count = (idle_now_time - terminal.idle_start_time)/1000;

	UpdateTerminalFlags( );

	// RDD 310714 added for ALH - if card in slot run purchase directly
	if( IS_STANDALONE )
	{	
		// RDD 251014 - check the PPP connection and reconnect if not already connected
		VIM_BOOL ppp_connected = VIM_FALSE;

		if( CommsType( HOST_WOW, 0 ) == GPRS )
		{
			vim_net_is_connected(gprs_ppp_handle.instance_ptr,&ppp_connected);
			if( !ppp_connected )
			{
				comms_connect_internal( HOST_WOW );
				DBG_MESSAGE( "Network Reconnection In progress..." );
				vim_event_sleep(500);
			}
		}

		vim_icc_check_card_slot( &CardPresent );		
		if(( CardPresent ) && ( !IS_INTEGRATED ))  		  	
		{    	  							
			VIM_DBG_MESSAGE( "Card Docked during Idle" );				
			VIM_ERROR_IGNORE( TerminalIdleDisplay( 1 ) );  		  			
		}			
		// If the terminal needs config or TMS or isn't logged on then go straight to Standalone IDLE
	}

#if 1
	// IDLE TIME PROCESSING: Logon then Reversal then ALL outstanding SAF
	if(( terminal.idle_time_count >= ready_timeout_seconds )||( txn.type == tt_none ))
	{

		//DBG_POINT;
		if( txn.type == tt_none )
		{

			//DBG_POINT;
			if( terminal.idle_time_count >= ready_long_timeout_seconds )
			{
				terminal.allow_ads = VIM_TRUE;
			}
		}
		else
		{
			terminal.allow_ads = VIM_TRUE;
		}
#ifdef nnVX690
		if( IS_STANDALONE )
		{
			VIM_UINT32 new_charge;
			// RDD - add an audible warning if tghe battery gets less than 10%		
			vim_terminal_get_battery_charge( &new_charge );
			if( new_charge <= 10 )	
			{									
				vim_sound( VIM_TRUE );          			
				vim_event_sleep( 100 );			
				vim_sound( VIM_TRUE );          			
	`			vim_event_sleep( 100 );			
				vim_sound( VIM_TRUE );          		
			}
		}
#endif
		display_idleS();  
	}
	else
	{
		// Keep the result (eg APPROVED) display from the last txn for xx seconds in IDLE
		terminal.allow_ads = VIM_FALSE;
	}
#endif
	//DailyHouseKeeping();
	if( terminal.idle_time_count >= terminal.max_idle_time_seconds )      
	{
		// RDD 300413: The Daily Housekeeping Offset is designated by the lane number % 60 (mins). So x 60 to get secs
		//VIM_BOOL ForceCoalesce = terminal_get_status( ss_daily_housekeeping_reqd ) ? VIM_TRUE : VIM_FALSE;
		VIM_SIZE DailyHouseKeepingOffsetMins =  ( asc_to_bin( &terminal.terminal_id[6], 2 ) % 60 );
		
		DBG_MESSAGE( "Regular Houskeeping" );
		// RDD 220323 added as was not being run every 3 mins as specced. 
		ReversalAndSAFProcessing(TRICKLE_FEED_COUNT, VIM_TRUE, VIM_FALSE, VIM_TRUE);

		// RDD 311014 Kill training mode after 3mins
		terminal.training_mode = VIM_FALSE;

		// RDD 110615 SPIRA:8753 Contactless Issues when reader not fully closed after a txn.
#if 0	// RDD SPIRA:8380 - fiddling around with the reader status...
		if( IsDocked() )
		{
			if( !terminal_get_status( ss_ctls_reader_open ))
			{
				if( picc_open(VIM_FALSE,VIM_FALSE) != VIM_ERROR_NONE )				
				{
					  pceft_debug( pceftpos_handle.instance, "CTLS ERROR: re-open reader failed" );
					  DBG_POINT;
					  picc_close(VIM_FALSE);							
				}
			}
		}
		else
		{
			// If not docked then close the CTLS field to save power
			if( terminal_get_status( ss_ctls_reader_open ))
			{
				picc_close( VIM_TRUE );
			}
		}
		if(( terminal_get_status( ss_ctls_reader_fatal_error ) || ( terminal_get_status( ss_wow_reload_ctls_params ))))
		{			
			terminal_set_status(ss_wow_reload_ctls_params);
			pceft_debug(pceftpos_handle.instance, "Reboot Due to Ctls failure" );		
			vim_event_sleep( 1000 );
			pceft_pos_display_close();	  
			VIM_ERROR_RETURN_ON_FAIL( vim_terminal_reboot () );  
		}
#endif

		// RDD 280814 - check the preauth table to see if the preauth stuff is empty and needs deleting.
		preauth_save_table( &preauth_saf_table );
		display_idleS();  
		if( !terminal_get_status( ss_ctls_reader_open ))			
		{
			DBG_MESSAGE( "!!!!!!!!!!!! CONTACTLESS READER IS CLOSED !!!!!!!!!!!!!!!!" );
			// RDD 270513 SPIRA:6749 
#if 0
			if( CtlsReaderEnable( VIM_TRUE ) != VIM_ERROR_NONE )
			{							
				DBG_MESSAGE( "!!!!!!!!!!!! CONTACTLESS READER IS REALLY DEAD !!!!!!!!!!!!!!!!" );
			}
#else

			// RDD 280513 - can't send this as if PinPad is idle after ctls has been de-activated (eg after manual entry or cash out) then this gets sent
			// out erroneously
			//pceft_debug( pceftpos_handle.instance, "Contactless Reader Unavailabile");
#endif
		}	

		//if(!(time.minute%5))
		if(( time.hour == 1 ) && ( time.minute >= DailyHouseKeepingOffsetMins ))
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
		terminal.allow_ads = VIM_TRUE;
		terminal.allow_idle = VIM_TRUE;

		// RDD 130413 - whether the display has already been closed or not is checked inside the fn.
		pceft_pos_display_close();

		terminal_set_statistics( &terminal.stats );

		// RDD 291211 added LFD Schedule Checking
#if 0
		if( tms_scheduled_logon() )
		{
		  tms_contact( );
		}
#endif
		//vim_pceftpos_enable_cancel_on_message(pceftpos_handle.instance, VIM_TRUE);
#if 0
		if(( terminal_get_status( ss_auto_test_mode ) && ( terminal_get_status( ss_wow_basic_test_mode ))))
		{
			terminal_set_status(ss_wow_reload_ctls_params);			      
			vim_terminal_reboot () ;
		}
#endif   

		if( !CheckFiles( VIM_FALSE ) )
		{
			DBG_MESSAGE( "Check Files Failed" );
			terminal.logon_state = logon_required;
		}

		if( terminal.max_idle_time_seconds != WOW_BACKOFF_IDLE_TIME )
		{
			original_idle_time = terminal.max_idle_time_seconds;
		}
		terminal.trickle_feed = VIM_TRUE;
		return( PostTxnProcessingS() );
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

VIM_ERROR_PTR main_event_handler_s
(
  VIM_MAIN_EVENT_DATA const* event_data_ptr,
  VIM_MAIN_RETURN_DATA* return_data_ptr,
  VIM_BOOL* is_handled_ptr
)
{
  //VIM_ERROR_PTR res = VIM_ERROR_NONE;

	VIM_BOOL icc_removed = VIM_FALSE;

  *is_handled_ptr=VIM_TRUE;
  return_data_ptr->type = event_data_ptr->type;
  /*VIM_DBG_VAR(event_data_ptr->type);*/

  //DBG_VAR( terminal.abort_transaction )
  switch(event_data_ptr->type)
  {

    case VIM_MAIN_EVENT_IDLE:
		terminal.initator = it_pinpad; 	  

		// RDD 270913 SUN SPIRA:6860 Read unused card event		
		if( icc_handle.instance_ptr != VIM_NULL )		
			vim_icc_is_removed(icc_handle.instance_ptr, &icc_removed );

	/* RDD 210312 : SPIRA 5115 Always Piggyback pending 101 logons  */
		if(( terminal.trickle_feed ) && ( terminal.logon_state == session_key_logon ))	  
		{
		  // RDD 251013 Can still have active stuff in txn struct when this is called from IDLE so need to wipe this	 
		  txn_init();

		  ReversalAndSAFProcessingS( SEND_NO_SAF, VIM_FALSE, VIM_TRUE, VIM_TRUE );	
		  terminal.trickle_feed = VIM_FALSE;	  
		}
	  
	  				  
		// RDD 030412 - reset IDLE start time from end of post-txn processing	  
		IdleTimeProcessingS( );
	  
		// RDD 230212 added for deleting stale card data stored for completion		
		MaintainCardBuffer();
	  
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
	  DBG_POINT;
      VIM_ERROR_RETURN_ON_FAIL(main_event_kbd(event_data_ptr->data.kbd.key_code));
      vim_rtc_get_uptime(&terminal.idle_start_time);
      break;

    case VIM_MAIN_EVENT_UART:
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
    
    default:
		// RDD 270913 SUN SPIRA:6860 Read unused card event		
		if( icc_handle.instance_ptr != VIM_NULL )		
			vim_icc_is_removed(icc_handle.instance_ptr, &icc_removed );
      break;
  }
  //VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&terminal.last_activity));
  
  return VIM_ERROR_NONE;
}
