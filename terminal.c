
#include "incs.h"
VIM_DBG_SET_FILE("TU");
#define TERMINAL_MAX_TLV_SIZE (10000)
#define TERMINAL_MAX_CAPK_SIZE (20000)		// RDD - we can estimate better than this...
static const char *terminal_file_name = VIM_FILE_PATH_LOCAL "TERMINAL.DAT";
static const char *capk_file_name = VIM_FILE_PATH_LOCAL "PKT";
static const char *emv_file_name = VIM_FILE_PATH_LOCAL "EPAT";
static const char *fuelcard_file_name = VIM_FILE_PATH_LOCAL "FCAT";

const VIM_TEXT LOYALTY_FILE[] = VIM_FILE_PATH_LOCAL "LOYALTY.DAT";
const VIM_TEXT STATUS_FILE[] = VIM_FILE_PATH_LOCAL "STATUS.DAT";
const VIM_TEXT STAN_FILE[] = VIM_FILE_PATH_LOCAL "STAN.DAT";
const VIM_TEXT STATS_FILE[] = VIM_FILE_PATH_LOCAL "STATS.DAT";

const VIM_TEXT CARDS_FILE[] = VIM_FILE_PATH_LOCAL "CARDS.XML";

const char *ip_config_file_name = VIM_FILE_PATH_LOCAL "IP_CONFIG.DAT";

const VIM_TEXT V400M_PM_SRC[] = VIM_FILE_PATH_LOCAL "V400m.ini";
const VIM_TEXT V400M_PM_CFG[] = VIM_FILE_PATH_CONFIG "V400m.ini";

extern VIM_SIZE mallocs;
extern VIM_SIZE frees;
extern const card_bin_list_t BlockedBinTable[];


extern VIM_TEXT const preauth_table[];

extern file_version_record_t file_status[];
extern VIM_BOOL QC_IsInstalled(VIM_BOOL fReport);


typedef struct type_of_terminal
{
  /* Name of the terminal */
	/*@null@*/VIM_CHAR *terminal_name;
  /* Type of the terminal */
  terminal_type_t type;
}terminal_type;

const struct type_of_terminal terminal_types[] =
{
  {"    I3070",   ttype_ingenico_3070},
  {"    I5100",   ttype_ingenico_5100},
  {VIM_NULL,      ttype_unknown}
};

//#define FORCE_COALESCE_LIMIT 0x400000	// RDD 220313 - P4 best to set this well above the 2mB OS limit in order to avoid 10sec hang-up during in txn

extern VIM_SIZE open_count, close_count;

#if 0
void terminal_set_reboot_time(void)
{
    VIM_RTC_TIME RebootTime, NextRebootTime;
    VIM_RTC_UNIX NextRebootTimeUnix;

    vim_rtc_get_time(&RebootTime); // RDD 180520 Trello #142
    NextRebootTime = RebootTime;

    // If outside window then put into window
    pceft_debugf("Rebooted: %d.%d ", RebootTime.hour, RebootTime.minute);

    if( RebootTime.hour > DAILY_REBOOT_HOUR )
    {
        // If reboot was out of window - reset Next Reboot back x hours + 1 day
        NextRebootTime.hour = DAILY_REBOOT_HOUR;
        NextRebootTime.minute = terminal.configuration.OffsetTime;
    }
    vim_rtc_convert_time_to_unix(&NextRebootTime, &NextRebootTimeUnix);
    NextRebootTimeUnix += SECS_IN_ONE_DAY;

    pceft_debugf_test( pceftpos_handle.instance, "Next Reboot Scheduled for %d:%d", NextRebootTime.hour, NextRebootTime.minute );

    terminal.NextRebootTime = NextRebootTimeUnix;
}

#else
void terminal_set_reboot_time(void)
{
    VIM_RTC_TIME LastRebootTime, NextRebootTime;
    VIM_RTC_UNIX NextRebootTimeUnix;
    VIM_CHAR Buffer[100];

    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    vim_rtc_get_time(&LastRebootTime); // RDD 180520 Trello #142
    vim_rtc_convert_time_to_unix(&LastRebootTime, &terminal.LastRebootTime);
    NextRebootTime = LastRebootTime;

    // If outside window then put into window
    if ((LastRebootTime.hour <= DAILY_HOUSE_KEEPING_HOUR) || (LastRebootTime.hour > DAILY_REBOOT_HOUR))
    {
        NextRebootTime.hour = DAILY_REBOOT_HOUR;
        NextRebootTime.minute = terminal.configuration.OffsetTime;

        vim_rtc_convert_time_to_unix(&NextRebootTime, &NextRebootTimeUnix);

        // If we're beyond window then next reboot will be tomorrow... if before window then wil be today
        if (LastRebootTime.hour > DAILY_REBOOT_HOUR)
        {
            NextRebootTimeUnix += SECS_IN_ONE_DAY;
            vim_sprintf(Buffer, "Reboot Time: %d.%d Set %d.%d, + one day", LastRebootTime.hour, LastRebootTime.minute, NextRebootTime.hour, NextRebootTime.minute );
        }
        else
        {
            vim_sprintf(Buffer, "Reboot Time : %d.%d out of window, second reboot today ", NextRebootTime.hour, NextRebootTime.minute);
        }
    }
    else
    {
        NextRebootTimeUnix = terminal.LastRebootTime + SECS_IN_ONE_DAY;
        vim_sprintf(Buffer, "Reboot Time : %d.%d - in window, add one day", NextRebootTime.hour, NextRebootTime.minute);
    }
    pceft_debug_test(pceftpos_handle.instance, Buffer);
    terminal.NextRebootTime = NextRebootTimeUnix;
}



#endif


// RDD 180520 Trello #142

VIM_SIZE terminal_get_secs_to_go( void )
{
    VIM_RTC_TIME CurrentTime;
    VIM_RTC_UNIX CurrentTimeUnix;
    VIM_INT32 Diff;
    VIM_SIZE SecsToGo;

    vim_rtc_get_time(&CurrentTime);
    vim_rtc_convert_time_to_unix(&CurrentTime, &CurrentTimeUnix);

    Diff = terminal.NextRebootTime - CurrentTimeUnix;

    SecsToGo = VIM_MAX(0, Diff);
    //VIM_DBG_PRINTF1("SecsToGo : %d", Diff);
    return SecsToGo;
}



VIM_SIZE terminal_secs_since_last_reboot(void)
{
    VIM_RTC_TIME CurrentTime;
    VIM_RTC_UNIX CurrentTimeUnix;

    vim_rtc_get_time(&CurrentTime);
    vim_rtc_convert_time_to_unix(&CurrentTime, &CurrentTimeUnix);
    VIM_INT32 Diff;
    VIM_SIZE SecsSinceReboot;

    Diff = CurrentTimeUnix - terminal.LastRebootTime;
    SecsSinceReboot = VIM_MAX(0, Diff);
    VIM_DBG_PRINTF1("SecsSinceLastReboot : %d", Diff);
    return(SecsSinceReboot);
}




// RDD 130618 JIRA WWBP-110 UI Files now cause reboot for checking authentication

// PCI DSS rules dictate that all UI files should be autheticated - log a warning if this is not so
VIM_ERROR_PTR ValidateFileSystem(void)
{
	//VIM_CHAR *Ptr;
	VIM_TEXT current_file_name[128];// RDD - enusre that these are big enough !!!
	VIM_CHAR next_file_name[128];	// RDD - enusre that these are big enough !!!
	VIM_CHAR PrefixedPath[128 + sizeof(VIM_FILE_PATH_GLOBAL)]; // VN 120718

	vim_mem_clear(current_file_name, VIM_SIZEOF(current_file_name));
	vim_mem_clear(next_file_name, VIM_SIZEOF(next_file_name));
	vim_strcpy(current_file_name, ".");

	VIM_DBG_MESSAGE("=== Search Group1 ===");

	while (1)
	{
		VIM_CHAR *Ptr = VIM_NULL;
		VIM_BOOL Authenticated = VIM_FALSE;
		VIM_CHAR Message[50];

		vim_mem_clear(next_file_name, VIM_SIZEOF(next_file_name));
		Ptr = next_file_name;

		// Get the next filename
		if( vim_file_list_directory( VIM_FILE_PATH_GLOBAL, current_file_name, Ptr, VIM_SIZEOF(next_file_name) - 1) != VIM_ERROR_NONE )
			break;

		vim_strcpy( current_file_name, Ptr );

		vim_mem_clear(PrefixedPath, VIM_SIZEOF(PrefixedPath));
		vim_strncpy(PrefixedPath, VIM_FILE_PATH_GLOBAL, sizeof(VIM_FILE_PATH_GLOBAL));
		vim_strncat(PrefixedPath, current_file_name, sizeof(current_file_name));

		// Check the current directory listing against what we're looking for
		if(( vim_strstr( current_file_name, ".UI", &Ptr ) == VIM_ERROR_NONE )||(vim_strstr(current_file_name, ".PCX", &Ptr) == VIM_ERROR_NONE ))
		{
			if ((vim_strstr(current_file_name, ".S1G", &Ptr) == VIM_ERROR_NONE) || (vim_strstr(current_file_name, ".P7S", &Ptr) == VIM_ERROR_NONE)) {
				// Avoid .SIG and .p7s files extensions with same file name
				continue;
			}
			vim_terminal_check_file_authentication(PrefixedPath, &Authenticated );
			if (!Authenticated)
			{
				vim_sprintf(Message, "ERROR: %s Failed .p7s Authentication", current_file_name );
				pceft_debug(pceftpos_handle.instance, Message);
				// MB asked to put terminal into D mode if UI file failed authentication.
				terminal_clear_status(ss_tms_configured);

			}
		}
	}
	return VIM_ERROR_NONE;
}





VIM_BOOL IsTerminalType( const VIM_CHAR *TerminalType )
{

  VIM_CHAR *Ptr = VIM_NULL;

  // Check against required type - eg "VX680"
  //VIM_DBG_STRING(terminal.TerminalType);
  if( vim_strstr(( VIM_CHAR *)terminal.TerminalType, TerminalType, &Ptr ) == VIM_ERROR_NONE)
  {
	 // VIM_DBG_POINT;
	  return VIM_TRUE;
  }
  //VIM_DBG_POINT;
  return VIM_FALSE;
}




VIM_ERROR_PTR CheckMemory(VIM_BOOL fForceCoalesce)
{

	//	VIM_FILE_FSINFO fs_info;
	//	VIM_SIZE mem_count = 0;
	//	VIM_CHAR Text[50];
	//	VIM_SIZE file_count = open_count - close_count;

	//	VIM_SIZE unfreed = mallocs - frees;

#if 0
	VIM_ERROR_RETURN_ON_FAIL(vim_file_filesystem_info(&fs_info));
	DBG_MESSAGE("*********** Check Flash Memory ************ ");
	DBG_VAR(fs_info.space_free);
	if ((fs_info.space_free < FORCE_COALESCE_LIMIT) || (fForceCoalesce))
	{
		vim_sprintf(Text, "Free Memory: %d", fs_info.space_free);
		pceft_debug(pceftpos_handle.instance, Text);
		if (fForceCoalesce)
		{
			pceft_debug(pceftpos_handle.instance, "Manual Memory Management. Coalescing now.....");
		}
		else
		{
			DBG_MESSAGE(" !!!!! DANGER - Low Memory Detected: Coalescing..... !!!!! ");
			pceft_debug(pceftpos_handle.instance, "Low Memory Detected. Coalescing now.....");
		}
		VIM_ERROR_RETURN_ON_FAIL(vim_flash_coalesce());
		VIM_ERROR_RETURN_ON_FAIL(vim_file_filesystem_info(&fs_info));
		vim_sprintf(Text, "Free Memory: %d", fs_info.space_free);
		pceft_debug(pceftpos_handle.instance, Text);
		DBG_VAR(fs_info.space_free);
	}

	DBG_MESSAGE("*********** Check Memory ************ ");
	vim_mem_get_item_count(&mem_count);
	DBG_VAR(mem_count);
#endif
	//DBG_VAR( file_count );
	//VIM_DBG_VAR( unfreed );
	//vim_terminal_get_heap_stack_info(&MaxHeap, &Heap, &MaxStack, &Stack);
	//DBG_VAR( MaxHeap );
	//DBG_VAR( Heap );
	//DBG_VAR( MaxStack );

	return VIM_ERROR_NONE;
}


#if 0
void set_default_tms_parameters(void)
{
#ifdef _DEBUG
  terminal.tms_parameters.net_parameters.connection_timeout = 5000;
  terminal.tms_parameters.net_parameters.receive_timeout = 10000;
#else
  terminal.tms_parameters.net_parameters.connection_timeout = 30000;
  terminal.tms_parameters.net_parameters.receive_timeout = 30000;
#endif
  terminal.tms_parameters.net_parameters.constructor = vim_pstn_hdlc_connect_request;

  /*terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.blind_dial = VIM_FALSE;
  terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.fast_connect = VIM_TRUE;
  terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.pulse_dial = VIM_FALSE;*/

  terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.modulation = VIM_PSTN_MODULATION_V_22;

#if 1
  vim_strncpy(terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number, "1800664311",
                  VIM_SIZEOF(terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number));
#else
  vim_strncpy(terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number, "0299970291",
                  VIM_SIZEOF(terminal.tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number));
#endif
  /* set routing information */
  terminal.tms_parameters.net_parameters.header.type = VIM_NET_HEADER_TYPE_NONE;

  terminal.tms_parameters.net_parameters.header.parameters.tpdu.message_type = VIM_NET_HEADER_TPDU_DEFAULT_MESSAGE_TYPE;
  vim_mem_clear(terminal.tms_parameters.net_parameters.header.parameters.tpdu.source_address,
                                    VIM_SIZEOF(terminal.tms_parameters.net_parameters.header.parameters.tpdu.source_address));

  vim_mem_clear(terminal.tms_parameters.net_parameters.header.parameters.tpdu.destination_address,
                                    VIM_SIZEOF(terminal.tms_parameters.net_parameters.header.parameters.tpdu.destination_address));

  /* todo: put the correct values in the following fields */
  terminal.tms_parameters.retries = 5;

  /* test with STEPUP, NII should be set 0111, should be changed according to different hosts */
  terminal.tms_parameters.nii = 101;
  /* set LFD parameters loading status */
}
#endif


VIM_UINT8 GetRandomByte( VIM_UINT8 Max )
{
	VIM_UINT8 Rand = 0;
	vim_tcu_get_random_byte( &Rand );
	if( Rand > Max )
		Rand=Rand%Max;
	return Rand;
}



VIM_ERROR_PTR terminal_set_defaults(void)
{

  vim_mem_clear(&terminal, sizeof(terminal));
  terminal.roc = 1;
  terminal.start_roc = 1;
  terminal.finish_roc = 999999l;
  terminal.file_version[0] = SOFTWARE_VERSION;
  terminal.screen_display_time = WOW_AD_SCREEN_DISPLAY_TIME;        // by default...
  terminal.tms_auto_logon = VIM_TRUE;
  terminal.configuration.prn_columns = 24;
  terminal.configuration.ForceGPRS = VIM_FALSE;

  // Set false on Power up and only true if EDP activated.
  TERM_EDP_ACTIVE = VIM_FALSE;


  if (!vim_strlen(terminal.configuration.BrandFileName))
  {
      if (terminal.terminal_id[0] == 'L')
      {
          vim_strcpy(terminal.configuration.BrandFileName, "LVE.png");
      }
  }

  // RDD 111121 Need DE47 encryption ON by default on each power up for Live Group Terminals
#ifndef _DEBUG
  //if (terminal.terminal_id[0] == 'L')
  {
      terminal.configuration.encrypt_de47 = VIM_TRUE;
      terminal.configuration.allow_disconnect = VIM_TRUE;
  }
#endif


  terminal.configuration.LFDPacketsMax = VIM_TMS_DEFAULT_MAX_PACKETS;

  // RDD 140121 - Assume WebLFD enabled by default from now on to avoid ADK upgrade by old LFD
  terminal.configuration.NewLFD = VIM_TRUE;

  //Enable background QC logon on startup by default
  TERM_QC_LOGON_ON_BOOT = VIM_TRUE;
  TERM_QC_RESP_TIMEOUT = 30; //30 second response timeout by default

  terminal.configuration.PinPadPrintflag = VIM_TRUE;

  // RDD 061217 Added for JIRA:WWBP93 - using term ID did not get a good spread between 1am and 2am so use RND value 0->59
  // RDD 140520 Trello #129 PCI Reboot sometimes happens more than once between 2-3am - ensure random value only set once !!
  if(( !terminal.configuration.OffsetTime ) || ( terminal.configuration.OffsetTime > 59 ))
    terminal.configuration.OffsetTime = GetRandomByte( 59 );

  // 110520 Trello #142 Configure Pre-PCI reboot check
  if ( !(TERM_PRE_PCI_REBOOT_CHECK )||( TERM_PRE_PCI_REBOOT_CHECK > TERM_PCI_PRE_CHECK_SECS_MAX ))
  {
      TERM_PRE_PCI_REBOOT_CHECK = TERM_DEFAULT_PCI_PRE_CHECK_SECS;
  }

  terminal.max_idle_time_seconds = WOW_BACKOFF_IDLE_TIME;

  terminal.configuration.TXNCommsType[0] = VIA_PCEFTPOS;
  terminal.configuration.TMSCommsType[0] = VIA_PCEFTPOS;

  // RDD 010415 Set a default for the max number of preauth records
  //terminal.configuration.PreauthBatchSize = PREAUTH_SAF_RECORD_COUNT;

  // RDD 161115 v547-3 also ensure that SAF Batch size has a default value
  terminal.configuration.efb_max_txn_count = PREAUTH_SAF_RECORD_COUNT;

  /* set host id */
  // RDD 231116 - need to set host ID from TermCfg.xml
  //vim_mem_set(terminal.host_id, 0, VIM_SIZEOF(terminal.host_id));
  //vim_mem_copy(terminal.host_id, WOW_HOST2, 8);
  //set_default_tms_parameters();

  terminal_set_qr_defaults();

  vim_strcpy( terminal.ecg_bin, DEFAULT_EGC_BIN );
  return VIM_ERROR_NONE;
}

void terminal_set_qr_defaults(void)
{
	//Set sensible Everyday pay defaults
	//TERM_QR_WALLET            = DEFAULT_QR_WALLET;
	TERM_QR_UPDATEPAY_TIMEOUT = DEFAULT_QR_UPDATEPAY_TIMEOUT;
	TERM_QR_REVERSALTIMER	  = DEFAULT_QR_REVERSALTIMER;
	//TERM_QR_PS_TIMEOUT        = DEFAULT_QR_PS_TIMEOUT;
	//TERM_QR_QR_TIMEOUT        = DEFAULT_QR_QR_TIMEOUT;
	TERM_QR_IDLE_SCAN_TIMEOUT = DEFAULT_QR_IDLE_SCAN_TIMEOUT;
	//TERM_QR_WALLY_TIMEOUT     = DEFAULT_QR_WALLY_TIMEOUT;
	TERM_QR_REWARDS_POLL      = DEFAULT_QR_REWARDS_POLL;
	TERM_QR_RESULTS_POLL      = DEFAULT_QR_RESULTS_POLL;
	//TERM_QR_POLLBUSTER        = DEFAULT_QR_POLLBUSTER;
	//TERM_QR_SEND_QR_TO_POS    = DEFAULT_QR_SEND_QR_TO_POS;
	TERM_QR_SEND_QR_TO_POS    = IS_SCO ? VIM_TRUE : VIM_FALSE;

	//TERM_QR_ENABLE_WIDE_RCT   = DEFAULT_QR_ENABLE_WIDE_RCT;
	//TERM_QR_DISABLE_FROM_IMAGE = DEFAULT_QR_DISABLE_FROM_IMAGE;
	//vim_mem_clear(TERM_QR_LOCATION,          sizeof(TERM_QR_LOCATION));
	vim_mem_clear(TERM_QR_TENDER_ID,         sizeof(TERM_QR_TENDER_ID));
	vim_mem_clear(TERM_QR_TENDER_NAME,       sizeof(TERM_QR_TENDER_NAME));
	vim_mem_clear(TERM_QR_DECLINED_RCT_TEXT, sizeof(TERM_QR_DECLINED_RCT_TEXT));
	vim_mem_clear(TERM_QR_MERCH_REF_PREFIX,  sizeof(TERM_QR_MERCH_REF_PREFIX));
	//vim_strncpy(TERM_QR_LOCATION,          DEFAULT_QR_LOCATION,          sizeof(TERM_QR_LOCATION)    - 1);
	vim_strncpy(TERM_QR_TENDER_ID,         DEFAULT_QR_TENDER_ID,         sizeof(TERM_QR_TENDER_ID)   - 1);
	vim_strncpy(TERM_QR_TENDER_NAME,       DEFAULT_QR_TENDER_NAME,       sizeof(TERM_QR_TENDER_NAME) - 1);
	vim_strncpy(TERM_QR_DECLINED_RCT_TEXT, DEFAULT_QR_DECLINED_RCT_TEXT, sizeof(TERM_QR_DECLINED_RCT_TEXT) - 1);
	vim_strncpy(TERM_QR_MERCH_REF_PREFIX,  DEFAULT_QR_MERCH_REF_PREFIX,  sizeof(TERM_QR_MERCH_REF_PREFIX   - 1));
}

VIM_ERROR_PTR terminal_save(void)
{
  VIM_SIZE buffer_size;
  VIM_UINT8 buffer[TERMINAL_MAX_TLV_SIZE];

  DBG_MESSAGE( "******* SAVE TERMINAL STRUCTURE TO FILE *******" );
  VIM_ERROR_RETURN_ON_FAIL(terminal_serialize(&terminal, buffer, &buffer_size,VIM_SIZEOF(buffer)));

  VIM_ERROR_RETURN_ON_FAIL(vim_file_set(terminal_file_name,buffer,buffer_size));

  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR wow_fcat_load(void)
{
	VIM_TLV_LIST tlv;
	VIM_SIZE buffer_size;
	VIM_UINT8 buffer[TERMINAL_MAX_TLV_SIZE];
	VIM_BOOL exists;

	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( fuelcard_file_name, &exists ));

    // Read the file into the buffer
    VIM_ERROR_RETURN_ON_FAIL( vim_file_get( fuelcard_file_name, buffer, &buffer_size, sizeof(buffer)));

	// create a container
	VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tlv, buffer, buffer_size, buffer_size, VIM_FALSE ));

	// deseialise the fuelcard configuration
	VIM_ERROR_RETURN_ON_FAIL( deserialise_fuelcard_configuration( &fuelcard_data, &tlv ));

	return VIM_ERROR_NONE;
}





/*
	RDD 311011 - WBC picked up CAPK data via LFD. WOW picks this up via file download (like NAB).
	During Power up we read the PKT file into the terminal structure for use in EMV txns. Basically
	the DO-WHILE loop reads AID related data and increases the table INDEX until an error occurs.
	This gives us the correct number of Scheme Public Keys
*/
VIM_ERROR_PTR wow_capk_load(void)
{
	VIM_TLV_LIST tlv;
	VIM_SIZE buffer_size;
	VIM_UINT8 buffer[TERMINAL_MAX_CAPK_SIZE];
	VIM_BOOL exists;

	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( capk_file_name, &exists ));

    // Read the file into the buffer
    VIM_ERROR_RETURN_ON_FAIL( vim_file_get( capk_file_name, buffer, &buffer_size, sizeof(buffer)));
	// create a container

	VIM_DBG_NUMBER( buffer_size );

	// RDD - TODO there appears to be a VIM related TLV decoding problem as the validation fails fairly early in the file
	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open( &tlv, buffer, buffer_size, buffer_size, VIM_FALSE));

	// deseialise the individual keys for each AID
	VIM_ERROR_RETURN_ON_FAIL( capk_deserialise( &tlv ));

	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR wow_emv_params_load(void)
{
	VIM_TLV_LIST tlv;
	VIM_SIZE buffer_size;
	VIM_UINT8 buffer[TERMINAL_MAX_TLV_SIZE];
	VIM_BOOL exists;

	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( emv_file_name, &exists ));

	// RDD 010513 - exit if the files doesn't exist
	if( !exists ) return VIM_ERROR_NOT_FOUND;

    // Read the file into the buffer
	buffer_size = VIM_SIZEOF(buffer);
	DBG_VAR( buffer_size );
    VIM_ERROR_RETURN_ON_FAIL( vim_file_get( emv_file_name, buffer, &buffer_size, VIM_SIZEOF(buffer)));

	DBG_MESSAGE( "$$$$$$$$$$$$$ OPEN FILE TO WRITE XXX (below) BYTES - MAX, ACTUAL $$$$$$$$$$$$$$" );
	DBG_MESSAGE( emv_file_name );
	DBG_VAR( buffer_size );
	// create a container

	// RDD - TODO there appears to be a VIM related TLV decoding problem as the validation fails fairly early in the file
	VIM_ERROR_IGNORE(vim_tlv_open( &tlv, buffer, buffer_size, buffer_size, VIM_FALSE));

	// deseialise the default parameters for the terminal
	VIM_ERROR_RETURN_ON_FAIL( deserialise_emv_default_configuration( &tlv ));

	// deseialise the EMV parameters for each AID
	VIM_ERROR_RETURN_ON_FAIL( deserialise_emv_aid_configuration( &tlv ));

	DBG_MESSAGE( "!!!!!!!!!!!!!! READ NEW EMV_PARAMETERS FROM FILE !!!!!!!!!!!!!!!!");
	return VIM_ERROR_NONE;
}


/* static VIM_UINT8 AppVersion[] = { 0x00,0x00,0x00 }; */
/* static VIM_UINT8 AppVersion[] = { 0x46,0x15,0x03 }; */
// static VIM_UINT8 AppVersion[] = { 0x03,0x15,0x17 };

VIM_BOOL CheckFiles( VIM_BOOL fClearVersions )
{
    VIM_UINT8 n;
	VIM_BOOL AllFilesValid = VIM_TRUE;
	VIM_TEXT new_name[30];
	VIM_FILE_HANDLE file_handle;
	VIM_UINT8 *ptr = (VIM_UINT8 *)&file_status[0];
	//DBG_MESSAGE( "!!!!!!!!!!!!!! ERASE FILE VERSION NUMBERS !!!!!!!!!!!!!!!!");

	// RDD 130618 JIRA WWBP-
    // RDD 170221 - not relevent for P400 and can cause issues
	//ValidateFileSystem();

    /* clear the files structure */
	if( fClearVersions )
	{
		for( n=0; n<WOW_NUMBER_OF_FILES; n++ )
		{
			vim_mem_clear( ptr, VIM_SIZEOF(file_version_record_t));
			ptr += VIM_SIZEOF(file_version_record_t);
		}
		/* restore the default file names*/
		vim_strcpy( file_status[CPAT_FILE_IDX].FileName, WOW_CPAT_FILENAME );
		vim_strcpy( file_status[PKT_FILE_IDX].FileName,  WOW_PKT_FILENAME);
		vim_strcpy( file_status[EPAT_FILE_IDX].FileName, WOW_EPAT_FILENAME );
		vim_strcpy( file_status[FCAT_FILE_IDX].FileName, WOW_FCAT_FILENAME );
		//pceft_debug_test( pceftpos_handle.instance, "Filenames copied" );
	}

    for( n=CPAT_FILE_IDX; n<= FCAT_FILE_IDX; n++ )
    {
		if( n==EPUMP_FILE_IDX ) continue;	// RDD - no Epump file for initial version
		if(( WOW_NZ_PINPAD )&&( n==FCAT_FILE_IDX )) continue;	// RDD 070912 P3: Appears not to be an FCAT file for NZ PinPad
		if(( ALH_PINPAD )&&( n==FCAT_FILE_IDX )) continue;		// RDD 161014 ALH SPIRA:8197 FCAT Not Required for ALH

		vim_strcpy( new_name, VIM_FILE_PATH_LOCAL );
        vim_strcat( new_name, file_status[n].FileName );

		//pceft_debug_test( pceftpos_handle.instance, file_status[n].FileName );

		if( vim_file_open( &file_handle, new_name, VIM_FALSE, VIM_TRUE, VIM_FALSE ) != VIM_ERROR_NONE )
		{
			terminal.file_version[n] = WOW_INVALID_FILE_VER;
			// We can't open the file, therefore it has been deleted and we need to download a new one
			// RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
			terminal.last_logon_state = terminal.logon_state;
			// RDD 020115 SPIRA:8277 If previous state was RSA then need to stay in RSA not promote the state to logon required
#if 0
			terminal.logon_state = logon_required;
#else
			if( terminal.logon_state == logged_on )
				terminal.logon_state = logon_required;
#endif
			// RDD 240413 - default ALL files to LFD config on Power-up. If nothing on LFD then will log on to switch.
			file_status[n].host_type = LFD_CONFIG;

			AllFilesValid = VIM_FALSE;
			pceft_debug( pceftpos_handle.instance, new_name );
			pceft_debug( pceftpos_handle.instance, "Failed Open" );

			// if the file cannot be opened then we return FALSE
			// RDD 140513 - surely if the file can't be opened then we shold always return FALSE
#if 0
			if( !fClearVersions )
				return VIM_FALSE;
#endif
		}
		else
		{
			// RDD 150513 - TODO really need to get the versions from the files themselves
			vim_file_close( file_handle.instance );
			bin_to_bcd( terminal.file_version[n], file_status[n].ped_version.bytes, WOW_FILE_VER_BCD_LEN*2 );

			//pceft_debug_test( pceftpos_handle.instance, new_name );
			//pceft_debug_test( pceftpos_handle.instance, "Opened OK" );
		}
    }

//    bin_to_bcd( APP_VERSION, (VIM_UINT8 *)&file_status[APP_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN*2);
//    vim_mem_copy( (VIM_UINT8 *)&file_status[APP_FILE_IDX].ped_version, AppVersion, WOW_FILE_VER_BCD_LEN);
    terminal.file_version[APP_FILE_IDX] = SOFTWARE_VERSION;
    bin_to_bcd( terminal.file_version[APP_FILE_IDX], file_status[0].ped_version.bytes, WOW_FILE_VER_BCD_LEN*2 );
	return AllFilesValid;
}


#define MIN_CFG_FILE_LEN 6

VIM_ERROR_PTR wow_load_files( void )
{
	VIM_ERROR_PTR res=VIM_ERROR_NONE;

#if 1
	{
		VIM_CHAR PathName[30] = { 0 };
		if ((vim_file_is_present(terminal.configuration.CPAT_Filename))&&( vim_strlen(terminal.configuration.CPAT_Filename) >= MIN_CFG_FILE_LEN ))
		{
			vim_sprintf(PathName, "%s%s", VIM_FILE_PATH_LOCAL, terminal.configuration.CPAT_Filename);
			VIM_DBG_PRINTF1("-------- Process Hard CPAT :%s ---------", terminal.configuration.CPAT_Filename);
			if (ProcessConfigFile(PathName, VIM_FILE_PATH_LOCAL "CPAT", CPAT_FILE_IDX) == VIM_ERROR_NONE)
			{
#if 0
				if (CpatLoadTable() == VIM_ERROR_NONE)
				{
					pceft_debugf(pceftpos_handle.instance, "Local CPAT[%s] Processed OK", terminal.configuration.CPAT_Filename);
				}
				else
				{
					pceft_debugf(pceftpos_handle.instance, "!! Local CPAT[%s] Processing Failure", terminal.configuration.CPAT_Filename);
				}
#endif
			}
			else
			{
				pceft_debugf(pceftpos_handle.instance, "!! Local CPAT[%s] Parsing Error", terminal.configuration.CPAT_Filename);
			}
		}

		if ((vim_file_is_present(terminal.configuration.EPAT_Filename)) && (vim_strlen(terminal.configuration.EPAT_Filename) >= MIN_CFG_FILE_LEN))			
		{
			vim_sprintf(PathName, "%s%s", VIM_FILE_PATH_LOCAL, terminal.configuration.EPAT_Filename);
			if (ProcessConfigFile(PathName, VIM_FILE_PATH_LOCAL "EPAT", EPAT_FILE_IDX) == VIM_ERROR_NONE)
			{
#if 0
				if (wow_capk_load() == VIM_ERROR_NONE)
				{
					pceft_debugf(pceftpos_handle.instance, "Local EPAT[%s] Processed OK", terminal.configuration.EPAT_Filename);
				}
				else
				{
					pceft_debugf(pceftpos_handle.instance, "!! Local EPAT[%s] Processing Failure", terminal.configuration.EPAT_Filename);
				}
#endif
			}
			else
			{
				pceft_debugf(pceftpos_handle.instance, "!! Local EPAT[%s] Parsing Error", terminal.configuration.EPAT_Filename);
			}
		}
		if ((vim_file_is_present(terminal.configuration.PKT_Filename)) && (vim_strlen(terminal.configuration.PKT_Filename) >= MIN_CFG_FILE_LEN))
		{
			vim_sprintf(PathName, "%s%s", VIM_FILE_PATH_LOCAL, terminal.configuration.PKT_Filename);
			VIM_DBG_PRINTF1("-------- Process Hard PKT :%s ---------", terminal.configuration.PKT_Filename);
			VIM_ERROR_RETURN_ON_FAIL(ProcessConfigFile(PathName, VIM_FILE_PATH_LOCAL "PKT", PKT_FILE_IDX));
			//wow_capk_load();
		}
	}
#endif

	if( CheckFiles( VIM_TRUE ) )
	{
		DBG_MESSAGE( "Reading Config File Data");
		// RDD 200412 Ignore errors on file load so deletion of one file does not cause the others not to load on power up
		res = wow_capk_load();
		if( res != VIM_ERROR_NONE ) VIM_DBG_ERROR( res );
		res = wow_emv_params_load();

        // RDD 101019 JIRA WWBP - 662
        //ctls_drl_load();

		if( res != VIM_ERROR_NONE ) VIM_DBG_ERROR( res );
		if( !WOW_NZ_PINPAD )
		{
			res = wow_fcat_load();	// FCAT file is only for AU PinPads
			if( res != VIM_ERROR_NONE ) VIM_DBG_ERROR( res );
		}
		res = CpatLoadTable();
		if( res != VIM_ERROR_NONE ) VIM_DBG_ERROR( res );
	}
	return ( res == VIM_ERROR_NONE ) ? res : VIM_ERROR_NOT_FOUND;
}

extern card_bin_name_item_t CardNameTable[];

// RDD 021220 - Todo Add Protection agaist missing or mal-formatted card files
VIM_ERROR_PTR CreateCardTable(void)
{
    VIM_CHAR CardTableBuffer[10000];   // Allow room for max size of 100 entries
    VIM_SIZE TableLen=(VIM_SIZEOF(card_bin_name_item_t))*CARD_INDEX_MAX, Index, Entry=0;
	VIM_SIZE TableSize=(VIM_SIZEOF(card_bin_list_t))*CARD_INDEX_MAX;
	VIM_CHAR *Ptr = CardTableBuffer;
	VIM_CHAR* CardFilePtr = TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "CARDS.XML");

	card_bin_list_t* BlockedBinPtr = (card_bin_list_t*)&BlockedBinTable[0];
	VIM_CHAR* Ptr2;

	vim_mem_clear((void *)BlockedBinTable, TableSize);

    vim_mem_clear(CardTableBuffer, VIM_SIZEOF(CardTableBuffer));
    vim_mem_clear(CardNameTable, TableLen);

    VIM_ERROR_IGNORE( vim_file_get(CardFilePtr, (void *)CardTableBuffer, &TableLen, VIM_SIZEOF(CardTableBuffer)));
    // Push Ptr to end of line
    xml_parse_tag((VIM_CHAR *)Ptr, terminal.configuration.CARDS_Version, "VER>", _TEXT);
    VIM_DBG_STRING(terminal.configuration.CARDS_Version);
                
    for( Index = 0; Index < CARD_INDEX_MAX; Index++)
    {
        // RDD 020321 JIRA PIRPIN XXXX report CARDS.XML and RPAT.XML versions to TMS.
        vim_strstr(Ptr, "<ENTRY>", &Ptr);
        xml_parse_tag((VIM_CHAR *)Ptr, &Entry, "INDEX>", NUMBER_32_BIT );
        xml_parse_tag((VIM_CHAR *)Ptr, &CardNameTable[Entry].bin, "PCEFTBIN>", NUMBER_32_BIT );
        xml_parse_tag((VIM_CHAR *)Ptr, &CardNameTable[Entry].card_name[0], "DESCR>", _TEXT );

        // Push Ptr to end of line
        vim_strstr(Ptr, "</ENTRY>", &Ptr);
    }
	Ptr = CardTableBuffer;
	if( vim_strstr( Ptr, "<INDEX>79<", &Ptr2) == VIM_ERROR_NONE )
	{
		VIM_ERROR_PTR ret;		
		do	
		{
			if (( ret = vim_strstr(Ptr2, "<BLOCKED>", &Ptr2)) == VIM_ERROR_NONE)
			{
				ret = xml_parse_tag((VIM_CHAR*)Ptr2, BlockedBinPtr++, "BLOCKED>", _TEXT);
				Ptr2++;
			}
		} while (ret == VIM_ERROR_NONE);
	}
	return VIM_ERROR_NONE;
}


void SetProdOrTest(void)
{
	VIM_CHAR *Ptr = VIM_NULL;
	VIM_TEXT OS_sponsor[50];

	TERM_TEST_SIGNED = VIM_FALSE;
	vim_mem_clear(OS_sponsor, VIM_SIZEOF(OS_sponsor));
	vim_terminal_get_OS_info(VIM_NULL, 0, VIM_NULL, 0, OS_sponsor, VIM_SIZEOF(OS_sponsor));

	VIM_DBG_PRINTF1("SSSSSSSSSSSSSSSSSSS Check Sponsor : %s SSSSSSSSSSSSSSSSSSSSS", OS_sponsor);

	// Look for the word "Test in the Sponsor name to determine a test-signed terminal 
	if (vim_strstr(OS_sponsor, "Test", &Ptr) == VIM_ERROR_NONE)
	{
		TERM_TEST_SIGNED = VIM_TRUE;
		pceft_debugf_test(pceftpos_handle.instance, "TID:%8.8s (Test Signed)", terminal.terminal_id);
	}
	else
	{
		pceft_debugf_test(pceftpos_handle.instance, "TID:%8.8s (Prod Signed)", terminal.terminal_id);
	}
}


VIM_ERROR_PTR terminal_load( VIM_BOOL *IsVirgin )
{
  VIM_SIZE buffer_size;
  VIM_UINT8 buffer[TERMINAL_MAX_TLV_SIZE];
  VIM_BOOL exists;
  init.error = VIM_ERROR_NONE;
  *IsVirgin = VIM_FALSE;
  /* set default values for terminal structure */
  VIM_ERROR_RETURN_ON_FAIL( terminal_set_defaults() );


  // RDD 230516 SPIRA:8958 - pick up the original status file immediatly after clearing the terminal.status
  VIM_ERROR_IGNORE( vim_file_get( STATUS_FILE, (void *)&terminal.status, &buffer_size, VIM_SIZEOF(terminal.status) ) );

  // RDD 011222 ensure this is only set for a single power cycle
  terminal_clear_status(ss_wow_enhanced_test_mode);


  /* check if there is a saved terminal configuration file */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(terminal_file_name,&exists));

  //VIM_ERROR_RETURN_ON_FAIL( CreateCardTable());

  if( !exists )
  {
	  // RDD 010813 - V418 Reset Statistics if no terminal file
	  DBG_MESSAGE( "No Terminal file - reset all stats" );
	  terminal_reset_statistics( VIM_NULL );
	  *IsVirgin = VIM_TRUE;
  }
  else
  {
	  /* Read the file*/
	  VIM_ERROR_RETURN_ON_FAIL(vim_file_get(terminal_file_name, buffer, &buffer_size, sizeof(buffer)));

	  /* conver file from TLV to C structure */
	  VIM_ERROR_RETURN_ON_FAIL(terminal_deserialize(&terminal, buffer, buffer_size));
#ifdef _WIN32
	  TERM_VDA = 0;
	  TERM_MDA = 0;
#endif

	  // RDD 191214 - note that terminal_load will now configure PcEFT comms if integrated mode is selected
	  SetProdOrTest();

	  vim_terminal_get_part_number(terminal.TerminalType, VIM_SIZEOF(terminal.TerminalType));

	  // RDD 140122 Make a note of the power-up mode as we change this temporalily sometimes for printing off base
	  terminal.original_mode = terminal.mode;
	  VIM_DBG_STRING(terminal.TerminalType);

	  //DBG_VAR(terminal.logon_state);

	  // RDD 060814 ALH - retrieve the shift totals
	  shift_totals_load();
  }

  VIM_ERROR_RETURN_ON_FAIL(CreateCardTable());


#ifdef _WIN32
//  terminal.configuration.efb_credit_limit = 100;
//  terminal.configuration.efb_debit_limit = 100;
//  vim_strcpy( terminal.configuration.merchant_name, "Merch Name From Switch" );
//  vim_strcpy( terminal.configuration.merchant_address, "Address From Switch " );
#endif

  //VIM_DBG_MESSAGE("MMMMMMMMMMMMMM Migrate Power Management Config MMMMMMMMMMMMMM");  
  //VIM_DBG_ERROR( vim_file_copy( V400M_PM_SRC, V400M_PM_CFG ));
		

  if ( IS_STANDALONE )
  {
      TermSetStandaloneMode();
  }
  else
  {  
      TermSetIntegratedMode();
  }

  // RDD 060814 ALH - retrieve the shift totals	  
  shift_totals_load();

  // RDD 180914 ALH - retrieve the Termcfg.xml Comms Configuration Parameters
  comms_config_load(VIM_TRUE);

  // RDD 180814 added for ALH
  preauth_load_table();

  // Load the saf_totals file
  retrieve_saf_totals();

  // Read the stats file into the terminal structure
  terminal_read_statistics( &terminal.stats );

//#ifndef _WIN32
  terminal.configuration.contactless_enable = VIM_TRUE;
  terminal.initator = it_pinpad;

  // RDD 200912 Pick up the (new)  and STAN and read the contents into the terminal.status word
  // if the file doesn't exist then the status hasn't been changed yet
  // RDD 250513 - note DO NOT DO TERMINAL SET BEFORE READING THE FILE CONTENTS !!
  VIM_ERROR_IGNORE( vim_file_get( STATUS_FILE, (void *)&terminal.status, &buffer_size, VIM_SIZEOF(terminal.status) ) );
  VIM_ERROR_IGNORE( vim_file_get( STAN_FILE, (void *)&terminal.stan, &buffer_size, VIM_SIZEOF(terminal.stan) ) );

  // RDD 070316 v560 - use our own config files rather than the WOW ones
#if 1
  if( wow_load_files() != VIM_ERROR_NONE )
  {
	  // pceft_debug( pceftpos_handle.instance, "Unable to load config files" );
	// If theres an error loading any of the WOW files reset the logon state to update the files unless already set to rsa logon
	if( terminal.logon_state > file_update_required )
	{
		// RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
		terminal.last_logon_state = terminal.logon_state;
		terminal.logon_state = file_update_required;
	}
  }
#endif

  // RDD 170412 removed to save on file writes
  //terminal_save();
	//   pceft_debug( pceftpos_handle.instance, "Config files loaded OK" );
  return VIM_ERROR_NONE;
}
/**
 * \brief
 * Checks if configuration change allowed
 *
 * \details
 * If a reversal or saf is outstanding configuration change is not allowed.
 *
 * \return
 * VIM_ERROR_NONE - command processed successfully - ERROR MSG if not
 */
VIM_ERROR_PTR terminal_check_config_change_allowed( VIM_BOOL DisplayMsg )
{
    VIM_SIZE QC_SAF_Count = 0;
	if( reversal_present() )
	{
		// VN 26/02/18 should not got to 'D' state if a reversal is present
		terminal_set_status(ss_tms_configured);
		pceft_debug(pceftpos_handle.instance, "EFT Reversal Present");

		DBG_MESSAGE("ss_tms_configured SET reversal present");

		//DBG_MESSAGE( "<<<<<<<< CONFIG CHANGE DENIED: REVERSAL PRESENT >>>>>>>>>>>" );
		if( DisplayMsg )
		{
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_CANCELLED_PENDING, VIM_PCEFTPOS_KEY_NONE, "Reversal Pending" ));
			vim_kbd_sound_invalid_key();
			vim_event_sleep( 1500 );

			// RDD 030413 - make sure that this flag is cleared if we have to clear a reversal first ! //BD This looks to be in wrong place ????? should be outside if???
#if 0		// RDD 160713 - actually I don't think that this should be here at all !
			terminal_set_status( ss_tms_configured );
		   DBG_MESSAGE( "ss_tms_configured SET reversal present" );
#endif
		}
		return ERR_REVERSAL_PENDING;
	}
	// RDD - BD says don't let SAF prints prevent config - new request from WOW

    // RDD 160221 - Prevent Lane change if QC SAF pending as this can really mess things up
    {
        VIM_SIZE Reversal = 0;
        VIM_SIZE Advice = 0;

		if( QC_IsInstalled(VIM_FALSE) )
		{
			QC_GetSAFCount(&QC_SAF_Count, &Reversal, &Advice);
		}
		if (Reversal)
        {
            pceft_debug(pceftpos_handle.instance, "No Lane change as QC SAF in old lane");
            return ERR_REVERSAL_PENDING;
        }
    }
    if (QC_SAF_Count)
    {
        pceft_debug( pceftpos_handle.instance, "No Lane change as QC SAF in old lane" );
    }
    if( saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count ) || QC_SAF_Count )
	{
		//DBG_MESSAGE( "<<<<<<<< CONFIG CHANGE DENIED: SAF PRESENT >>>>>>>>>>>" );
		if( DisplayMsg )
		{
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_CANCELLED_PENDING, VIM_PCEFTPOS_KEY_NONE, "SAF Pending" ));
			vim_kbd_sound_invalid_key();
			vim_event_sleep( 1500 );
		}
		return ERR_SAF_PENDING;
	}

	return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_set_status(VIM_UINT32 status)
{
	// Check to see if the status is actually already set
	if( !( terminal.status & status ) )
	{
		// status has changed so update local copy and save the file
		terminal.status |= status;
		VIM_ERROR_RETURN_ON_FAIL( vim_file_set( STATUS_FILE, (const void *)&terminal.status, VIM_SIZEOF(terminal.status) ) );
	}
	return VIM_ERROR_NONE;
}




/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_clear_status(VIM_UINT32 status)
{
	// Check to see if the status is actually already clear
	if( terminal.status & status )
	{
		// status has changed so update local copy and save the file
		terminal.status &= ~status;
		VIM_ERROR_RETURN_ON_FAIL( vim_file_set( STATUS_FILE, (const void *)&terminal.status, VIM_SIZEOF(terminal.status) ) );
	}
	return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */
VIM_BOOL terminal_get_status(VIM_UINT32 status)
{
  return (0 != (terminal.status & status));
}




/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_set_statistics( statistics_info_t *stats_s_ptr )
{
	statistic_info_u stats_u;
	vim_mem_copy( stats_u.byte_buf, (statistic_info_u *)stats_s_ptr, VIM_SIZEOF(statistics_info_t)) ;
	VIM_ERROR_RETURN_ON_FAIL( vim_file_set( STATS_FILE, (const void *)&stats_u.byte_buf, VIM_SIZEOF(statistics_info_t) ) );
	return VIM_ERROR_NONE;
}


/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_read_statistics( statistics_info_t *stats_s_ptr )
{
	VIM_SIZE fileSize = 0;
	statistic_info_u stats_u;
	VIM_ERROR_RETURN_ON_FAIL( vim_file_get( STATS_FILE, (void *)stats_u.byte_buf, &fileSize, VIM_SIZEOF(statistics_info_t) ));
	DBG_VAR( fileSize );
	vim_mem_copy( (statistic_info_u *)stats_s_ptr, stats_u.byte_buf, VIM_SIZEOF(statistics_info_t)) ;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR terminal_debug_statistics( void *Ptr )
{
	VIM_CHAR Buffer[1000];
	vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );
	tms_GetStatsString( Buffer );
	pceft_debug( pceftpos_handle.instance, Buffer );
	return VIM_ERROR_NONE;
}


/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_reset_statistics( void *Ptr )
{
	//VIM_SIZE fileSize = 0;
	//statistic_info_u stats_u;
	vim_mem_clear( (void *)&terminal.stats, VIM_SIZEOF( statistics_info_t ) );

	VIM_ERROR_RETURN_ON_FAIL( vim_file_set( STATS_FILE, (void *)&terminal.stats, VIM_SIZEOF(terminal.stats) ));
	return VIM_ERROR_NONE;
}


/* ------------------------------------------------------------- */
VIM_ERROR_PTR terminal_increment_roc(void)
{
  /* Increment RRN */
  terminal.roc++;

  if ((terminal.roc >= terminal.finish_roc) || (terminal.roc >= 999999l))
    terminal.roc = terminal.start_roc;

  terminal_save();

  return VIM_ERROR_NONE;
}
/* ------------------------------------------------------------- */
terminal_type_t get_terminal_type(void)
{
  VIM_TEXT part_number[15];
  terminal_type_t type = ttype_unknown;
  VIM_UINT8 index = 0;

  vim_terminal_get_part_number(part_number, VIM_SIZEOF(part_number));

  do
  {
    if (terminal_types[index].terminal_name == VIM_NULL)
      break;

    if (VIM_ERROR_NONE == vim_mem_compare(part_number,
                                          terminal_types[index].terminal_name,
                                          vim_strlen(terminal_types[index].terminal_name)))
    {
      type = terminal_types[index].type;
      break;
    }
  }
  while (VIM_NULL != terminal_types[++index].terminal_name);

  return type;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR terminal_ip_config_save( VIM_NET_ETH_IP_CONFIG_PARAMETERS *ip_params )
{
VIM_DBG_VAR(ip_params->DHCP);
	VIM_ERROR_RETURN_ON_FAIL( vim_file_set( ip_config_file_name, (const void *)ip_params, VIM_SIZEOF(VIM_NET_ETH_IP_CONFIG_PARAMETERS) ));
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR terminal_ip_config_load( VIM_NET_ETH_IP_CONFIG_PARAMETERS *ip_params )
{
  VIM_SIZE size;
  VIM_ERROR_RETURN_ON_NULL(ip_params);
	VIM_ERROR_RETURN_ON_FAIL( vim_file_get( ip_config_file_name, (void *)ip_params, &size, VIM_SIZEOF(VIM_NET_ETH_IP_CONFIG_PARAMETERS) ));
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR terminal_eth_ip_config_set(void)
{
VIM_NET_ETH_IP_CONFIG_PARAMETERS ip_params;

VIM_DBG_MESSAGE("terminal_eth_ip_config_set");
  vim_mem_clear( &ip_params, VIM_SIZEOF(ip_params) );
  ip_params.DHCP = 1; // set default to DHCP and try to load saved settings
  VIM_ERROR_IGNORE( terminal_ip_config_load( &ip_params ) );
  VIM_ERROR_RETURN_ON_FAIL( vim_net_nwif_ethernet_ip_restart( &ip_params ) );
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR gprs_get_info( VIM_GPRS_INFO *info, VIM_BOOL use_last_known )
{
  VIM_ERROR_PTR error;

VIM_DBG_MESSAGE("gprs_get_info");

  vim_mem_clear( info, sizeof (VIM_GPRS_INFO) );

#if 0 // RDD 031117 - TODO For WOW
  if( terminal.acquirers[0].AQGPRSAPN[0] != 0 ) // Use Acquirer APN
    vim_strncpy( info->APN, terminal.acquirers[0].AQGPRSAPN, VIM_MIN( VIM_SIZEOF(terminal.acquirers[0].AQGPRSAPN), VIM_SIZEOF(info->APN)) );
  else // Use TMS APN as WBC is not be avil at this time
    vim_strncpy( info->APN, terminal.tms_gprs_apn, VIM_SIZEOF(info->APN) );
#endif

  error = vim_gprs_get_info( info );
  return error;
}

// Need this to control Z0 debug logs in VIM layer based on XXX mode
VIM_BOOL vim_terminal_is_in_basic_test_mode() {
	return terminal_get_status(ss_wow_basic_test_mode) ? VIM_TRUE : VIM_FALSE;
}

#define VRK_GOOD_STATUS 3

const char *apple_key_db = VIM_FILE_PATH_LOCAL "apple_key.db";
const char *android_key_db = VIM_FILE_PATH_LOCAL "vwi_key.db";

VIM_ERROR_PTR TerminalDBStatus(VIM_BOOL Verbose)
{

    VIM_CHAR serial_number[30];

    vim_mem_clear(serial_number, VIM_SIZEOF(serial_number));

    vim_terminal_get_serial_number(serial_number, VIM_SIZEOF(serial_number) - 1);

    if ((!vim_file_is_present(apple_key_db) || !vim_file_is_present(android_key_db)))
    {
        pceft_debug(pceftpos_handle.instance, "Error detecting Loyalty db files" );
        pceft_debug(pceftpos_handle.instance, serial_number);

        if (Verbose)
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "!! FATAL ERROR !!\nNo VAS db Files", serial_number);
            vim_event_sleep(10000);
        }
        return VIM_ERROR_NOT_FOUND;
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Loyalty db Files detected OK");
    }
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR TerminalVRKStatus(VIM_BOOL Verbose)
{
    VIM_ERROR_PTR res = VIM_ERROR_NONE;
    VIM_UINT8 VrkStatus = 0;
    VIM_CHAR Buffer[100];
    VIM_CHAR serial_number[30];

    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
    vim_mem_clear(serial_number, VIM_SIZEOF(serial_number));

    vim_terminal_get_serial_number(serial_number, VIM_SIZEOF(serial_number) - 1);
    res = vim_terminal_get_vrk_status(&VrkStatus);

    VIM_DBG_VAR(VrkStatus);

    if (res != VIM_ERROR_NONE)
    {
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Error Reading VRK Status", "");
    }

    else if( VrkStatus != VRK_GOOD_STATUS )
    {
        vim_sprintf( Buffer, "VRK Status Error : %d", VrkStatus );

        pceft_debug(pceftpos_handle.instance, Buffer);
        pceft_debug(pceftpos_handle.instance, serial_number);

        if( Verbose )
        {
            display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer, serial_number );
            vim_event_sleep(10000);
        }
    }
    else if (Verbose)
    {
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "VRK Status OK", serial_number);
        vim_event_sleep(1000);
    }

    return VIM_ERROR_NONE;
}


void terminal_init_vas( VIM_BOOL UseNewVAS, VIM_BOOL Verbose )
// RDD 170719 - don't proceed if ADK Init Fails and display nothing if it's OK as P400 takes long enough to power up as it is
{
    VIM_ERROR_PTR res;
#ifndef _WIN32
    if ((res = mal_nfc_Init()) != VIM_ERROR_NONE)
    {
        if (Verbose)
        {
            display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "ADK Init Failed(M)", "Call Help Desk");
            vim_event_sleep(10000);
        }
        pceft_debug(pceftpos_handle.instance, "ADK Init Failed(MAL)");
    } 
    else
    {
        pceft_debug(pceftpos_handle.instance, "Using MAL VAS");
    }
#endif
}

// RDD - this size for good files determined from Production
#define VSS_MIN_SIZE 68

VIM_CHAR *terminal_vas_log_status(void)
{
    static VIM_CHAR LogData[50];
    VIM_SIZE VSSLogFileSize = 0;

    vim_mem_clear(LogData, VIM_SIZEOF(LogData));
    vim_file_get_size(VIM_FILE_PATH_LOGS "vss_nfc.log", VIM_NULL, &VSSLogFileSize);
    if (VSSLogFileSize < VSS_MIN_SIZE)
    {
        vim_sprintf(LogData, "vss_nfc.log FAIL (%d)", VSSLogFileSize);
    }
    else
    {
        vim_sprintf(LogData, "vss_nfc.log PASS (%d)", VSSLogFileSize);
    }
    return LogData;
}

VIM_BOOL IsStandalone(void)
{
	return (IS_STANDALONE);
}

VIM_CHAR *TermGetBrandedFile(const VIM_CHAR *Path, const VIM_CHAR *DefaultImage)
{
	static VIM_CHAR ImageFilename[30];

	VIM_DBG_PRINTF3(" TermGetBrandedFile Brand =%s Path=%s Image=%s", TERM_BRAND, Path, DefaultImage);

	vim_mem_clear(ImageFilename, VIM_SIZEOF(ImageFilename));

	// Setup with Path (if supplied)
	if (vim_strlen(Path))
		vim_strcpy( ImageFilename, Path );

	// Add Branding ( if exists )
	if (vim_strlen(TERM_BRAND))
	{
		VIM_CHAR TempFilename[30] = { 0 };
		vim_strcpy( TempFilename, ImageFilename );

		VIM_DBG_STRING(TempFilename);

		vim_strncat( TempFilename, TERM_BRAND, WOW_BRAND_LEN);
		vim_strcat( TempFilename, DefaultImage );

		VIM_DBG_STRING(TempFilename);

		if (vim_file_is_present(TempFilename))
		{
			vim_strcpy( ImageFilename, TempFilename );
			VIM_DBG_STRING(ImageFilename);
		}
		else		
		{
			vim_strcat( ImageFilename, DefaultImage);
			VIM_DBG_STRING(ImageFilename);
		}
	}
	else
	{
		VIM_DBG_MESSAGE("No Brand file - use default image");
		vim_strcpy(ImageFilename, DefaultImage);
	}
	//pceft_debugf_test(pceftpos_handle.instance, "Branded Files Using:%s", ImageFilename);
	return ImageFilename;
}


VIM_SIZE terminal_get_lane(VIM_CHAR *term_id)
{
	terminal.LaneNumber = asc_to_bin(&terminal.terminal_id[5], 3);
	VIM_DBG_NUMBER(terminal.LaneNumber);
	return terminal.LaneNumber;
}





