/**
 * \file power_on.c
 *
 * Powerup processing related commands
 */
#include <incs.h>

#if defined _DEBUG && defined _VOS2
#include <sys/resource.h>
#endif
#include <stdio.h>

#ifdef _DEBUG
//#define EPAT_FILENAME "EP900204"
//#define CPAT_FILENAME "CPAT99325"
#endif
#include <string.h>

const char *WebLFDClientVer = VIM_FILE_PATH_LOCAL "WebLFDClient.ver";
const char *QCVer = VIM_FILE_PATH_LOCAL "Qwikcilver.ver";
const char *EDPVer = VIM_FILE_PATH_LOCAL "Wally.ver";
const char* EDPData = VIM_FILE_PATH_LOCAL "Wally.data";
const char *TID_File = VIM_FILE_PATH_LOCAL "tid";

extern VIM_ERROR_PTR txn_print_receipt_type(receipt_type_t type, transaction_t* txn);


//const char* EDPTime = VIM_FILE_PATH_LOCAL "WallyTimes.data";


VIM_DBG_SET_FILE("TM");

extern VIM_ERROR_PTR WifiStatus(VIM_BOOL SilentMode, VIM_UINT8 Usage);
extern VIM_ERROR_PTR ctls_drl_load(void);

VIM_ERROR_PTR WifiInit(void);

#ifdef _WIN32
VIM_ERROR_PTR adk_com_init(int InitBT, int InitWifi, VIM_CHAR *ErrMsg)
{
	vim_strcpy(ErrMsg, "SIM Missing");
	return VIM_ERROR_NOT_FOUND;
}
#endif

extern const char *JsonReqFile;


extern VIM_ERROR_PTR ConnectClient(void);
extern VIM_ERROR_PTR EmvCardRemoved(VIM_BOOL GenerateError);
extern const char *pceft_settings_filename;
extern VIM_BOOL ValidateConfigData( VIM_CHAR *terminal_id, VIM_CHAR *merchant_id );
extern VIM_ERROR_PTR comms_connect_internal( VIM_UINT8 host );
extern VIM_BOOL PCEftComm( VIM_UINT8 host, VIM_UINT8 attempt, VIM_BOOL showMsg );
extern VIM_NET_CONNECT_PARAMETERS sTXN_SETTINGS[];
extern VIM_BOOL Wally_BuildJSONRequestOfRegistrytoFIle(void);

void InitEMVKernel(void)
{
  static VIM_BOOL is_initialized = VIM_FALSE;
  VIM_BOOL exist=VIM_FALSE;
  const char *epat_path = VIM_FILE_PATH_LOCAL WOW_EPAT_FILENAME;

 VIM_DBG_VAR( is_initialized );

#if 0
  /* already initlized, return */
  if( is_initialized )
  {
    /* avoid re-init EMV kernel */
    return;
  }
#else
  is_initialized = VIM_FALSE;
#endif

  VIM_DBG_WARNING("--- EMV Config Contact Kernel ---");

  if( vim_file_exists( epat_path, &exist ) != VIM_ERROR_NONE )
  {
    /* return without doing anthing */
	DBG_MESSAGE( "!!!!!!!!!!! EMV KERNEL INIT FAILED - NO EPAT FILE !!!!!!!!!!!!!!!");
    return;
  }

  if( exist && !is_initialized )
  {
    if( vim_emv_init() == VIM_ERROR_NONE )
	{
		DBG_MESSAGE( "!!!!!!!!!!! EMV KERNEL SUCESSFULLY INITIALISED !!!!!!!!!!!!!!!");
		is_initialized = VIM_TRUE;
	}
  }
}
/**
 * Perform processing that must be done every time terminal is powered on.
 */

void SetPcEFTPOSPrinterParams(void)
{
	VIM_PCEFTPOS_PRINTER_PARAMETERS params;
	params.auto_print = '0';
	params.cut_receipt = '0';
	params.journal_receipt = '1';
	vim_pceftpos_set_printer_paramters(pceftpos_handle.instance, &params);
}

void CompleteTxn( VIM_BOOL PowerFail )
{
  VIM_CHAR pos_command_code=0x00;
  VIM_CHAR pos_sub_code=0x00;
  VIM_CHAR dev_code=0x00;
  VIM_BOOL pending_flag=VIM_FALSE;

  VIM_DBG_MESSAGE("Complete Transaction");

  /* load last pos command transaction */
  vim_pceftpos_get_last_cmd( &pending_flag, &pos_command_code, &pos_sub_code, &dev_code );

    //This doesn't reliably return the previous in progress command (ignores Query cards)
    if (terminal_get_status(ss_edp_query_powerfail))
    {
        //Override command with J8
        pceft_debug(pceftpos_handle.instance, "Forcing powerfail for interrupted EDP Query card");
        pos_command_code = 'J';
        pos_sub_code = '8';
        pending_flag = VIM_TRUE;
    }

  //if( transaction_get_status( &txn, st_incomplete ) || pending_flag == VIM_TRUE )

  if( terminal_get_status( ss_incomplete ) || pending_flag == VIM_TRUE )		// RDD 161215 SPIRA:8855
  {

      DBG_MESSAGE( "<<<< INCOMPLETE TXN LOADED or PENDING FLAG >>>>" );

      //terminal.initator = it_pceftpos;


      //if( transaction_get_status( &txn, st_incomplete ) )

      if( terminal_get_status( ss_incomplete )) 	// RDD 161215 SPIRA:8855
      {
			  if( PowerFail )
			  {
				// RDD 080212 - Bugfix to report Q5 to POS after power fail so it can print the power fail receipt
				txn.error = ERR_POWER_FAIL;
				vim_strcpy( txn.host_rc_asc_backup, txn.host_rc_asc );
				vim_strcpy( txn.host_rc_asc, "Q5" );
				txn_duplicate_txn_save();
				// RDD 080212 - End Bugfix
			  }
			  // DBG_MESSAGE( "<<<< INCOMPLETE TXN LOADED >>>>" );
			  /* finalisation of previous transaction */

			  if( reversal_present() )
			  {
				  DBG_MESSAGE( "<<<< REVERSAL ALREADY SET >>>>" );
			  }
			  else if (transaction_get_status(&txn, st_message_sent))
			  {
				  DBG_MESSAGE( "!!!! SET REVERSAL NEW STAN !!!!");
				  DBG_VAR( terminal.stan );
				  reversal_set( &txn, VIM_FALSE);
			  }
			  else
			  {
				  DBG_MESSAGE( "<<<< NO REVERSAL NEEDED >>>>" );
			  }
			  // RDD 211211 - removed this line as was calling SAF send on power up. Not sure why it was there anyway.
			  // RDD 010312 - actually need this to print receipt and display POS stuff relating to Q5 Power fail. Just remove the SAF Processing in this case
			  if( PowerFail )
			  {
				  VIM_ERROR_PTR res;					
				  DBG_MESSAGE( "<<<< POWER FAIL RUN FINALISATION >>>>" );
#if 1
				// RDD 100523 JIRA PIRPIN-2376 - set device code for txn as restored from previous txn as value will be '0' from status and prevents power fail receipt print
				SetPcEFTPOSPrinterParams();
				vim_pceftpos_set_device_code(pceftpos_handle.instance, txn.pceftpos_device_code);

				display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
				if ((res = txn_print_receipt_type(receipt_duplicate, &txn)) != VIM_ERROR_NONE)
				{
					pceft_debug_error(pceftpos_handle.instance, "POS Receipt Print Error:", res);
				}
#endif
			    pceft_pos_display_close(); // 5100 doesn't do a clear display on power up but BD says send it anyway...
			  }
			  else
			  {
				   DBG_MESSAGE( "<<<< CLEARING TXN STATUS >>>>" );
				  //transaction_clear_status( &txn, st_incomplete );
				  terminal.trickle_feed = VIM_FALSE;

				  terminal.idle_time_count = 0;
				  vim_rtc_get_uptime(&terminal.idle_start_time);	// RDD 210312 reset the IDLE timer so this does not cause an IDLE time SAF upload immediatly after a transaction
			  }
		  }
		  else
		  {
			  DBG_MESSAGE( "<<<< TXN WAS COMPLETE >>>>" );
		  }

		  if( PowerFail )
		  {
			/* send a GENPOS transaction response command in case power fail */

			  txn.error = ERR_POWER_FAIL;
			  vim_strcpy( txn.host_rc_asc, "Q5" );

			DBG_MESSAGE( "<<<< SEND POWER FAIL >>>>" );
			pceft_send_pos_power_fail( ERR_POWER_FAIL, pos_command_code, pos_sub_code );
			vim_pceftpos_update_last_cmd( VIM_FALSE );
		  }
  }

  DBG_MESSAGE("Powerfail complete, clearing EDP PF bit");
  terminal_clear_status(ss_edp_query_powerfail);
}

#define LFD_LATEST_CFG_SHA1_FILE    LFD_FILES_PATH "WebLFDLatestCfgSha1.bin"


void DeleteFiles( VIM_BOOL DeleteCustomerFiles )
{
	pceft_debug(pceftpos_handle.instance, "S/W Version changed so clear config files");

	//vim_file_delete("~/terminal.dat");
	//vim_file_delete("~/saftot.bin");
	//vim_file_delete("~/saf.bin");

	vim_mem_clear(terminal.configuration.CPAT_Filename, VIM_SIZEOF(terminal.configuration.CPAT_Filename));
	vim_mem_clear(terminal.configuration.EPAT_Filename, VIM_SIZEOF(terminal.configuration.EPAT_Filename));
	vim_mem_clear(terminal.configuration.PKT_Filename, VIM_SIZEOF(terminal.configuration.PKT_Filename));

	if( DeleteCustomerFiles )
	{
		vim_file_delete("~/CPAT");
		vim_file_delete("~/EPAT");
		vim_file_delete("~/PKT");
		vim_file_delete("~/FCAT");
	}
}


VIM_BOOL NewSoftwareVersion(void)
{
#if 1
	if( terminal.sfw_minor != REVISION )
	{
		terminal.sfw_minor = REVISION;
		terminal.sfw_version = BUILD_NUMBER;
		return VIM_TRUE;
	}
#endif
	if( terminal.sfw_version != BUILD_NUMBER )
	{
        terminal.sfw_minor = REVISION;
		terminal.sfw_version = BUILD_NUMBER;
		return VIM_TRUE;
	}
	return  VIM_FALSE;
}

//#ifdef _DEBUG
#if 0
static VIM_ERROR_PTR wow_test_ip_internal
(
  /*@out@*/VIM_NET_HANDLE *ethernet_ip_handle_ptr
)
{
  VIM_NET_CONNECT_PARAMETERS ip_settings;

  /* IP parameters to be set */
  //VIM_UINT8 ip_address[4]= {192, 168, 31, 75};
  VIM_UINT8 ip_address[4]= {192, 168, 31, 130};
  VIM_UINT8 gtway_addr[4]= {192, 168, 31, 1};
  VIM_UINT8 sbnet_mask[4]= {255, 255, 255, 0};
  VIM_UINT8 dns_addr1[4] = {208, 67, 222, 220};
  VIM_UINT8 dns_addr2[4] = {208, 67, 222, 222};
  VIM_BOOL is_connected;

  /* configure the ethernet ip interface */
  ip_settings.constructor=vim_ethernet_ip_connect_request;
  /* set communication timeouts */
  ip_settings.connection_timeout=0;
  ip_settings.receive_timeout=0;
  ip_settings.header.type=VIM_NET_HEADER_TYPE_NONE;

  /* initialise ip settings */
  vim_mem_clear(ip_settings.interface_settings.ethernet_ip.static_ip_address,VIM_SIZEOF(ip_settings.interface_settings.ethernet_ip.static_ip_address));
  vim_mem_clear(ip_settings.interface_settings.ethernet_ip.netmask,VIM_SIZEOF(ip_settings.interface_settings.ethernet_ip.netmask));
  vim_mem_clear(ip_settings.interface_settings.ethernet_ip.gateway_ip_address,VIM_SIZEOF(ip_settings.interface_settings.ethernet_ip.gateway_ip_address));
  vim_mem_clear(ip_settings.interface_settings.ethernet_ip.dns1_ip_address,VIM_SIZEOF(ip_settings.interface_settings.ethernet_ip.dns1_ip_address));
  vim_mem_clear(ip_settings.interface_settings.ethernet_ip.dns2_ip_address,VIM_SIZEOF(ip_settings.interface_settings.ethernet_ip.dns2_ip_address));

#if 0  /*if we use DHCP*/
    ip_settings.interface_settings.ethernet_ip.use_DHCP=VIM_TRUE;

#else
    /*if we dont use DHCP*/
    ip_settings.interface_settings.ethernet_ip.use_DHCP=VIM_FALSE;
    vim_mem_copy(ip_settings.interface_settings.ethernet_ip.static_ip_address,ip_address,4);
    vim_mem_copy(ip_settings.interface_settings.ethernet_ip.netmask,sbnet_mask,4);
    vim_mem_copy(ip_settings.interface_settings.ethernet_ip.gateway_ip_address,gtway_addr,4);
    /* we will not copy dns addresses */
    vim_mem_copy(ip_settings.interface_settings.ethernet_ip.dns1_ip_address, dns_addr1, 4);
    vim_mem_copy(ip_settings.interface_settings.ethernet_ip.dns2_ip_address, dns_addr2, 4);
#endif

  /* bring up the ethernet ip interface */
  VIM_ERROR_RETURN_ON_FAIL(vim_net_connect_request(ethernet_ip_handle_ptr,&ip_settings));
  /* make sure the interface was started correctly */

  /* make sure the interface was started correctly */
  VIM_ERROR_RETURN_ON_NULL(ethernet_ip_handle_ptr->instance_ptr);

   //VIM_DBG_MESSAGE("TEST ETHERNET IP: Connection established");

  /* Check the IP connection, is it set properly or not */
  VIM_ERROR_RETURN_ON_FAIL(vim_net_is_connected(ethernet_ip_handle_ptr->instance_ptr, &is_connected));
  if (!is_connected)
  {
     //VIM_DBG_MESSAGE("TEST ETHERNET IP: Is not connected");
    VIM_ERROR_RETURN(VIM_ERROR_OS);
  }

   //VIM_DBG_MESSAGE("TEST ETHERNET TCP: TEST COMPLETED SUCCESSFULLY");
  /* return without error */
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
#if 0
static VIM_ERROR_PTR wow_ethernet_test_ip(void)
{
  VIM_ERROR_PTR error1;
  VIM_ERROR_PTR error2;
  VIM_NET_HANDLE ethernet_ip_handle_ptr;

  error1=wow_test_ip_internal(&ethernet_ip_handle_ptr);
  error2=vim_net_disconnect(ethernet_ip_handle_ptr.instance_ptr);
  VIM_ERROR_RETURN_ON_FAIL(error1);
  VIM_ERROR_RETURN_ON_FAIL(error2);
  /* return without error */
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR SecuritySetup( VIM_BOOL unused )
{
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_new( &tcu_handle ));
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_set_sponsor( tcu_handle.instance_ptr, SPONSOR ));
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_set_acquirer( tcu_handle.instance_ptr,ACQUIRER ));
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_set_merchant( tcu_handle.instance_ptr,"0" ));

	//Check if we have keys loaded and if not sit there and load them
	VIM_ERROR_RETURN_ON_FAIL( keyLoad() );
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR CtlsInit( void )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_UINT8 Level;
	// Check to ensure that all the config files are present


#ifdef _WIN32
	return VIM_ERROR_NONE;
#endif

    vim_file_delete(VIM_FILE_PATH_LOCAL"EMV_CTLS_Apps_SchemeSpecific.xml");


	if( CheckFiles( VIM_FALSE ))
    {
			// RDD 240812 : Get the status to determine whether to reload params or not
			VIM_BOOL ReloadParams = terminal_get_status(ss_wow_reload_ctls_params);

			DBG_MESSAGE( "<<<< INIT CTLS >>>>" );

#ifndef _WIN32
			ReloadParams = VIM_TRUE;
#endif

#if 1
			Level = ReloadParams ? PICC_FULL_RELOAD : PICC_OPEN_ONLY;
#else
			Level = ReloadParams ? PICC_FULL_RELOAD : PICC_MESSAGES_ONLY;
#endif

			// RDD 110912 SPIRA:???? Sometimes the CTLS param load appears to fail for some reason so try again.
			if(( res = CtlsReaderEnable( Level )) == VIM_ERROR_NONE )
			{
				if( ReloadParams )
					pceft_debug(pceftpos_handle.instance, "INIT CTLS SUCEEDED" );
				terminal_clear_status(ss_wow_reload_ctls_params);
				// RDD 291014 added
				terminal_clear_status( ss_ctls_reader_fatal_error );
			}
			else
			{
				//VIM_TEXT text_buffer[256];
				VIM_TEXT debug_buffer[256];
				VIM_SIZE debug_size=0;

				vim_mem_clear( debug_buffer, VIM_SIZEOF( debug_buffer ));

				DBG_MESSAGE( "<<<< INIT CTLS (2nd TRY) >>>>" );

				pceft_debug(pceftpos_handle.instance, "INIT CTLS FAILED - TRY AGAIN !" );
				//vim_picc_emv_debug_vivotech_transaction_data (text_buffer, debug_buffer, &debug_size);
				if( debug_size )
					vim_pceftpos_debug_error( pceftpos_handle.instance, debug_buffer, res );

				if(( res = CtlsReaderEnable( Level )) == VIM_ERROR_NONE )
				{
					pceft_debug(pceftpos_handle.instance, "2nd INIT CTLS SUCEEDED" );
					terminal_clear_status(ss_wow_reload_ctls_params);
					// RDD 291014 added
					terminal_clear_status( ss_ctls_reader_fatal_error );
				}
				else
				{
					vim_mem_clear( debug_buffer, VIM_SIZEOF( debug_buffer ));
					pceft_debug(pceftpos_handle.instance, "INIT CTLS FAILED TWICE - DISABLE READER" );
					//vim_picc_emv_debug_vivotech_transaction_data (text_buffer, debug_buffer, &debug_size);
					if( debug_size )
						vim_pceftpos_debug_error( pceftpos_handle.instance, debug_buffer, res );

					DBG_MESSAGE( "<<<< INIT CTLS 2nd TRY FAILED >>>>" );
					// RDD - this seems to be set on by default so if the init failed ensure it's cleared

					terminal_set_status( ss_ctls_reader_fatal_error );
					terminal_set_status(ss_wow_reload_ctls_params);
				}
			}
    }
    else
    {
        VIM_KBD_CODE key;

        // DBG_MESSAGE( "===== PICC ERROR : NO EPAT FILE =====" );

        display_screen( DISP_CONTACTLESS_NOT_READY, VIM_PCEFTPOS_KEY_NONE );

        pceft_debug_error( pceftpos_handle.instance, "Power On Ctls init Fail: Config files(s) missing", VIM_ERROR_SYSTEM );
        vim_kbd_read(&key, TIMEOUT_INACTIVITY);
        terminal_set_status(ss_wow_reload_ctls_params);
        picc_close(VIM_TRUE);
    }

    // RDD 251019 - move from above as we should always have the reader closedon startup.
    picc_close(VIM_TRUE);
    pceft_pos_display_close();
	return res;
}


VIM_ERROR_PTR LoadFileToBuffer( VIM_CHAR *FileName, void *FileBuffer, VIM_SIZE *BufferSize )
{
	VIM_BOOL exists = VIM_FALSE;

	vim_mem_clear( FileBuffer, *BufferSize );
	VIM_ERROR_IGNORE( vim_file_exists( FileName, &exists ));

	if( !exists )
	{
		return VIM_ERROR_NOT_FOUND;
	}

	// Read the file into a buffer
	VIM_ERROR_RETURN_ON_FAIL( vim_file_get( FileName, FileBuffer, BufferSize, *BufferSize ));
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR get_file_version( const VIM_CHAR *FilePath, VIM_CHAR *dest, VIM_SIZE Max )
{
    VIM_BOOL FilePresent = VIM_FALSE;
    VIM_SIZE Len = 0;
    vim_file_exists( FilePath, &FilePresent);
    VIM_DBG_VAR(FilePresent);
    if( FilePresent )
    {
        VIM_ERROR_RETURN_ON_FAIL( vim_file_get( FilePath, dest, &Len, Max ));
    }
    else
    {
        VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
    }
    VIM_DBG_PRINTF2( "%s Ver=%s", FilePath, dest );
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR InitComms( void )
{
	VIM_CHAR buffer[100];
	vim_sprintf( (VIM_CHAR *)buffer, "v%8d.%2d", BUILD_NUMBER, REVISION );

	DBG_MESSAGE( "Init Comms" );
	// If either TMS or TXN Host is via GPRS, make a network connection on Power up
	// RDD 101214 - MB says its OK to assume VX680 will always have a SIM
#ifndef _DEBUG
	if(( !IsTerminalType( "VX820" )  && ( !vim_adkgui_is_active() )))
	{
		VIM_DBG_ERROR( display_screen( terminal.IdleScreen, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "MOBILE DATA INIT" ));
		comms_connect_internal( HOST_WOW );
	}
#endif
	// RDD 191214 - basically init the Sync MODEM any time we're doing init comms
	return VIM_ERROR_NONE;
}

#if 0
static void RLimit_setup( void )
{
    int iRet;
    struct rlimit sRLimit = { 0 };

    iRet = getrlimit( RLIMIT_CORE, &sRLimit );
    if ( iRet < 0 )
    {
        DBG_MESSAGE( "getrlimit failed!" );
        //VIM_DBG_VAR( errno );
    }
    else
    {
        DBG_MESSAGE( "getrlimit success - attempting setrlimit..." );
        VIM_DBG_VAR( sRLimit.rlim_cur );
        VIM_DBG_VAR( sRLimit.rlim_max );

        sRLimit.rlim_cur = RLIM_INFINITY;
        sRLimit.rlim_max = RLIM_INFINITY;
        iRet = setrlimit( RLIMIT_CORE, &sRLimit );
        if ( iRet < 0 )
        {
            DBG_MESSAGE( "setrlimit failed!" );
            //VIM_DBG_VAR( errno );
        }
        else
        {
            DBG_MESSAGE( "setrlimit success!" );
        }
    }
}
#endif



VIM_ERROR_PTR power_up_routine(void)
{
	VIM_BOOL AppVersionChanged = VIM_FALSE;
	VIM_CHAR buffer[200];
	VIM_BOOL IsVirgin = VIM_FALSE;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
#ifdef _DEBUG
	VIM_BOOL isP400 = VIM_FALSE;
#endif

	// RDD 121022 start with a fresh logfile after each reboot + ensure no debug data in prod terminals.
	vim_file_delete(VIM_FILE_PATH_LOGS "debug_file");

	if (terminal_get_status(ss_wow_basic_test_mode))
	{
		//RLimit_setup();
	}

	vim_terminal_initialize("WOW");
	txn.acquirer_index = 0;   /* RDD - only single acquirer for WOW */

	vim_terminal_get_part_number(terminal.TerminalType, VIM_SIZEOF(terminal.TerminalType));
	VIM_DBG_STRING(terminal.TerminalType);

	VIM_DBG_POINT;
#ifdef _DEBUG
	isP400 = IsTerminalType("P400");
	VIM_DBG_VAR(isP400);
#endif

	DBG_POINT;
	//display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");

	/* open security session */
	// 101018 RDD JIRA WWBP-336 - Special message if Crypto Script incorrect/missing

	// RDD 081014 - need to do this to determine comms method from the default file
	vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d", BUILD_NUMBER, REVISION);

	if ((res = SecuritySetup(VIM_FALSE)) != VIM_ERROR_NONE)
	{
		if (res == VIM_ERROR_NOT_FOUND)
		{
			DBG_MESSAGE("VSS FILE NOT FOUND");
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "VSS FILE NOT FOUND");
			vim_event_sleep(10000);
		}
		DBG_MESSAGE("CRYPTO INIT FAIL");

		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "CRYPTO INIT ERROR");
		vim_event_sleep(10000);
	}

	// RDD 021222 JIRA PIRPIN-2116 fix BMX load in factory.
	terminal_init_vas( TERM_NEW_VAS, VIM_TRUE );


	DBG_MESSAGE("Checking Config");


	terminal_load(&IsVirgin);
	terminal.configuration.NewVAS = VIM_TRUE;

	DBG_STRING(terminal.terminal_id);
	DBG_STRING(terminal.merchant_id);

	extern VIM_BOOL WifiRequired(void);


	/////////////////////// HERE - WORK!! ////////////////////////
	// RDD 130322 - apply to integrated V400m too.
	//if(( IS_V400m) && ( IS_STANDALONE ))
	if (IS_V400m)
	{
		VIM_CHAR ErrMsg[100] = { 0 };
		VIM_DBG_MESSAGE("VVVVVVVVVVVVVVVVVVVV V400m VVVVVVVVVVVVVVVVVVVVV");
		adk_com_init(VIM_TRUE, WifiRequired(), ErrMsg);		
	}
	else
	{
		
		if (WifiRequired()) {
			ADKcomInit();
		}

		pceft_settings_load_only();
		// RDD 270422 v733-0 : set a new LFD Parameter to route all EFT payments via RNDIS
	}
	//CommsSetup(VIM_TRUE);
	DBG_STRING(terminal.terminal_id);
	//UpdateTopIcons();
	//vim_event_sleep(1000);

	// RDD - just get the terminal type a second time because terminal load wipes it !
	vim_terminal_get_part_number(terminal.TerminalType, VIM_SIZEOF(terminal.TerminalType));
	VIM_DBG_STRING(terminal.TerminalType);

	VIM_DBG_NUMBER(terminal_get_status(ss_tms_configured));

	DBG_STRING(terminal.terminal_id);
	DBG_STRING(terminal.merchant_id);
	DBG_MESSAGE("POWER ON ROUTINE");

	if (IsVirgin)
	{
		VIM_SIZE n;
		DBG_MESSAGE("**** New Terminal ****");
		vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "New Terminal");
		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
		vim_event_sleep(2000);

		config_display_software_version(VIM_NULL);
		// Allow timne for the ADK ver to be seen
		for (n = 0; n < 10; n++)
		{
			vim_sound(VIM_TRUE);
			vim_event_sleep(1000);
		}
		// RDD 281214 SPIRA:8296 Ensure that the Shift totals file is created in a new terminal
		shift_totals_reset(0);
		vim_strcpy(terminal.configuration.BrandFileName, DEFAULT_BRAND_FILE);

		// RDD 300523 ensure Batch Key created in new terminal
		if (vim_tcu_generate_new_batch_key() != VIM_ERROR_NONE)
		{
			VIM_DBG_MESSAGE("Gen new Batch Key failed");
		}
		else
		{
			VIM_DBG_MESSAGE("Gen new Batch Key OK");
		}
	}
	else
	{
		VIM_TEXT adk_version[50] = { 0 };
		VIM_TEXT qc_version[50] = { 0 };
		VIM_TEXT wlfd_version[50] = { 0 };
		VIM_TEXT edp_version[50] = { 0 };

		vim_terminal_get_EOS_info(adk_version, VIM_SIZEOF(adk_version));

		if (get_file_version(WebLFDClientVer, wlfd_version, VIM_SIZEOF(wlfd_version)) != VIM_ERROR_NONE)
		{
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "WebLFDClient Not Present", "Disable WebLFD");
			vim_strcpy(wlfd_version, "N/A");
			vim_event_sleep(5000);
			TERM_NEW_LFD = VIM_FALSE;
		}
#ifndef _WIN32
		if (QC_IsInstalled(VIM_TRUE) != VIM_TRUE)
		{
			pceft_debug(pceftpos_handle.instance, "QC VAA Not present in this terminal");
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Qwikcilver Not Present", "Disable Qwikcilver");
			vim_strcpy(qc_version, "N/A");
			vim_event_sleep(5000);
		}
		else
		{
			get_file_version(QCVer, qc_version, VIM_SIZEOF(qc_version));
		}

		get_file_version(EDPVer, edp_version, VIM_SIZEOF(edp_version));
#endif

		vim_sprintf((VIM_CHAR *)buffer, "EFT:%8d.%2d\nADK:%s\nWLFD:%s QC:%s EDP:%s", BUILD_NUMBER, REVISION, adk_version, wlfd_version, qc_version, edp_version);
		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
		/// UDP Debug will appear from here...
		vim_event_sleep(1000);
	}

	terminal.file_version[0] = SOFTWARE_VERSION;
	// Config files are all deleted if version has changed

	if ((AppVersionChanged = NewSoftwareVersion()) == VIM_TRUE)
	{
		//terminal_set_status(ss_wow_reload_ctls_params);
		DeleteFiles(VIM_FALSE);

		// RDD 080922 JIRA PIRPIN-1806 Don't allow Key roll on SW change
		//VIM_DBG_WARNING("Generate New Batch Key !!!!!");
		//vim_tcu_generate_new_batch_key();

		// RDD 221114 SPIRA:8256
		terminal_clear_status(ss_tms_configured);
		VIM_DBG_MESSAGE(" New SW VER - need TMS");
	}
	DBG_POINT;

	// RDD 050412 SPIRA:5188 SET RSA If Software Version has changed
	// RDD 280812 For Z debug to tell us that s/w has been updated move this to the end of power up sequence

	// Load up the last saved transaction - always.
	//VIM_ERROR_RETURN_ON_FAIL( txn_load() );

	//CheckSecurity(VIM_TRUE);

	// RDD 060819 JIRA WWBP-673 Stay forever waiting for client else problems ensue...
	if (!IS_STANDALONE)
	{
		// RDD 100523 JIRA PIRPIN-2376 Missing Power Fail receipt
		VIM_ERROR_IGNORE(txn_load());
		VIM_DBG_VAR(txn.pceftpos_device_code);

		// ***************************************************
		ConnectClient();
		// ***************************************************

		if (((FUEL_PINPAD || ALH_PINPAD)) && (TID_INVALID == VIM_FALSE))
		{
			PostCDSValidation(VIM_FALSE);
		}
		else if ((IS_R10_POS == VIM_FALSE) && (TID_INVALID))
		{
			ConfigureTerminal();
		}
	}
	else
	{
		if (TID_INVALID)
		{
			// Invalid TID for Standalone - just set config required and continue.
			terminal_clear_status(ss_tid_configured);
		}
	}

    DBG_POINT;

    pceft_debug(pceftpos_handle.instance, buffer);

    // Kill any parial preswipe on power up
    txn.preswipe_status = no_preswipe;

    /* set a number of auto settlement attempts */
    terminal.settle_retry = terminal.configuration.auto_settle_retry;
    terminal.auto_settle_day = 0;
    terminal.screen_id = 0;		// RDD 140312 reset the screen id to the divisional default on power up

    //VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&terminal.last_activity));

    if (reversal_present())
    {
        terminal.reversal_count = 1;
    }

    // wait for heartbeat from POS - except when using GPRS
    // RDD 290814 - some of this stuff is causing problems in standalone mode...

    terminal.initator = it_pinpad;

#if 1 // RDD 160216 moved here from much lower down to privide correct response to power on status
    // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
    //terminal.last_logon_state = terminal.logon_state;
    terminal.logon_state = terminal.logon_state < logon_required ? rsa_logon : logon_required;
#endif
    // RDD 060715 don't want power fail recorded unless txn was incomplete before failure
  // txn.error = ERR_POWER_FAIL;
  // vim_strcpy( txn.host_rc_asc_backup, txn.host_rc_asc );
  // vim_strcpy( txn.host_rc_asc, "Q5" );

    DBG_STRING(terminal.terminal_id);
    DBG_STRING(terminal.merchant_id);

    /* clear struct error pointer */
    txn.error = VIM_NULL;
    txn.error_backup = VIM_NULL;
    //transaction_clear_status( &txn, st_incomplete );
    //terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855

    /* idle timer start from here */
    if (IS_INTEGRATED)
    {
        terminal.idle_time_count = 0;
        terminal.allow_idle = VIM_TRUE;
        terminal.allow_ads = VIM_TRUE;
        vim_rtc_get_uptime(&terminal.idle_start_time);
    }
    // RDD 010818 - noticed SAF prints were getting accumulated in VX690 for some reason.
    //  else   //BRD Clear SAF prints for Standalone 190115
    {
        terminal_set_status(ss_no_saf_prints);
        //DeleteSafPrints(); - RDD 210719 no longer needed
    }

    /* set transaction intiator */
    terminal.initator = it_pinpad;
    /* init EMV  */

    InitEMVKernel();

    /* check card removed */
    emv_remove_card("");
    DBG_POINT;
    vim_mem_clear(&picc_handle, VIM_SIZEOF(picc_handle));
    DBG_VAR(terminal.logon_state);


    if (terminal_get_status(ss_tid_configured))
    {
        // RDD 070912 P3: don't want any txn remains if we a TMS in case we need to display result
		VIM_CHAR tid[8 + 1];
		vim_mem_clear(tid, VIM_SIZEOF(tid));
		vim_mem_copy(tid, terminal.terminal_id, 8);
        DBG_POINT;
		// Save the TID for other Apps to use
		vim_file_set_bytes(TID_File, tid, VIM_SIZEOF(tid), 0);
		// RDD 170722 JIRA PIRPIN-1747
#if 0
		if (!terminal_get_status(ss_tms_configured))
		{
			txn_init();				
			terminal.initator = it_pinpad;
		}
#endif
    }

    // RDD 280812 For Z debug to tell us that s/w has been updated move this to near the end of power up sequence
    DBG_VAR(terminal.logon_state);

    if ((AppVersionChanged) || (IsVirgin))
    {
        VIM_CHAR Buffer[100];
        DBG_POINT;

        if (IsVirgin)
        {
            terminal.sfw_prev_version = 0;
            terminal.sfw_prev_minor = 0;
        }
 
        terminal.sfw_version = BUILD_NUMBER;
        terminal.sfw_minor = REVISION;
        DBG_VAR(terminal.logon_state);

        // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
        terminal.last_logon_state = terminal.logon_state;
        terminal.logon_state = rsa_logon;
        DBG_POINT;
        vim_sprintf(Buffer, "APP SW CHANGED FROM: V%d.%d TO V%d.%d", terminal.sfw_prev_version, terminal.sfw_prev_minor, terminal.sfw_version, terminal.sfw_minor);
        pceft_debug(pceftpos_handle.instance, Buffer);
		terminal.sfw_prev_version = terminal.sfw_version;
		terminal.sfw_prev_minor = terminal.sfw_minor;

        // RDD 100413 - Save the time when this occurred
        vim_rtc_get_time(&terminal.last_activity);
        DBG_POINT;

        saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);

        pceft_debug(pceftpos_handle.instance, "************************");
        vim_sprintf(Buffer, "DELETING %6.6d SAF PRINTS", saf_totals.fields.saf_print_count);
        pceft_debug(pceftpos_handle.instance, Buffer);
        vim_sprintf(Buffer, "DELETING %6.6d SAF TXNS  ", saf_totals.fields.saf_count);
        pceft_debug(pceftpos_handle.instance, Buffer);
        pceft_debug(pceftpos_handle.instance, "************************");

        // RDD 280812 SPIRA:5963 Only delete SAF Prints if Application S/W has been changed
        // If no SAF or reversal pending, saf prints will be lost as the whole SAF is being deleted here
		// 
		// RDD 080922 JIRA PIRPIN-1806 Don't allow Key roll on SW change if SAFs in batch
		// RDD 010623 ensure PAs not present before rolling batch key
		if (!saf_totals.fields.saf_count)
		{
			VIM_SIZE PendingCount = 0, PendingTotal = 0;
			preauth_get_pending(&PendingCount, &PendingTotal, VIM_FALSE);

			if (!PendingCount)
			{
				saf_destroy_batch();
			}
		}

        // RDD 240812: Also reload contactless params if the s/w version has changed
        terminal_set_status(ss_wow_reload_ctls_params);
        saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);
        DBG_POINT;

        terminal_save();
        DBG_POINT;
    }

    // RDD 170912 - Delete this file as it causes "N3" for NZ if present from previous Australian configs
    DBG_MESSAGE("######## CHECKING COUNTRY #########");

    if (WOW_NZ_PINPAD)
    {
        // RDD Set pre-insert mode for NZ to allow One card to be kept in the PinPad between transactions
        DBG_MESSAGE("NZNZNZNZ <<< NEW ZEALAND LANE DETECTED >>> NZNZNZNZ");
        //terminal_set_status(ss_wow_pre_insert_mode);
        DBG_MESSAGE("Pre-Insert mode set");
        vim_file_delete("~/FCAT");
        DBG_MESSAGE("FCAT deleted and files reset");
    }

    // RDD 270312 SPIRA 5189: Power failure when Logon file Upload in progress
    DBG_VAR(terminal.logon_state);

#if 0 // RDD 150216 - need to move this up to provide correct response to intial status message
    // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
    //terminal.last_logon_state = terminal.logon_state;
    terminal.logon_state = terminal.logon_state < logon_required ? rsa_logon : logon_required;
#endif
    //txn.amount_total = 0;
    //txn.account = account_unknown_any;

    // Logon on power-up to save time
    //logon_request( VIM_TRUE );

    // RDD 161013 SPIRA:6802 DG Says NOT to keep last logon state if power cycle
    terminal.last_logon_state = terminal.logon_state;

    // RDD 010513 - at BD request set !1 mode by default on power up so LFD will always be invoked before first logon.
    // If there is no !1 for this Lane then future logons won't go to LFD
  //BD  terminal_set_status( ss_lfd_then_switch_config ); //BD This NOT what I want it should behave like Phase 3 if NOT present

    DBG_VAR(terminal.logon_state);

    // RDD 110413 - make always do init on power cycle
    terminal_clear_status(ss_ctls_reader_open);

#if 0
    // RDD 060819 JIRA WWBP-673 Stay forever waiting for client else problems ensue...
    if (IS_INTEGRATED)
    {
        ConnectClient();
    }
    else
    {
        terminal_set_status( ss_tid_configured );
        terminal_set_status( ss_tms_configured );            
    }
#endif

    // RDD 251019 JIRA WWBP-689 Q5 sent to GCM Logon after power fail kills normal logon with Q5 response to GCM config.
    txn_init();
    vim_mem_copy(init.logon_response, "00", VIM_SIZEOF(init.logon_response));

    // RDD 120917 SPIRA:9241 - LFD resume on poweer up if partial download of new files
    // RDD 071217 - this line causes terminal to lose its 'D' state if it was in that state befroe power cycle !
    // RDD 190819 JIRA WWBP-673 Only do version check if LFD NOT already required ( eg. by config change )
    VIM_ERROR_WARN_ON_FAIL(vim_tms_build_app_filename(BUILD_NUMBER, REVISION));
    DBG_POINT;

    // RDD 040321 -DON't DELETE THIS AS BREAKS OLD LFD !!
    if (!terminal_get_status(ss_config_changed))
    {
#ifndef _WIN32
        vim_tms_lfd_clear_donloading_status(VIM_TRUE);
        //terminal_set_status(ss_tms_configured);
#endif

    }

    // RDD 140121 - remove power up LFD checks as we can't afford multi line downloads for old LFD in case ADK upgrade

#if 1	// RDD v560 -move this to last because of TMS issues with downgrading etc.
    DBG_POINT;

#ifdef _DEBUG

	terminal.logon_state = logged_on;
	//terminal_set_status(ss_wow_basic_test_mode);
#endif

	//CommsSetup(VIM_TRUE);

  if( CheckFiles( VIM_FALSE ))
  {
	  DBG_MESSAGE( "Init Ctls" );
	  // RDD 040715 SPIRA:8774 Set control for CTLS reset
	  if( terminal.configuration.ctls_reset & CTLS_RESET_POWER_UP )
	  {
		  // RDD 040715 for emergencies only !
		  terminal_set_status( ss_wow_reload_ctls_params );
	  }
	  DBG_MESSAGE( "Init Ctls" );
	  CtlsInit();
  }
  else
  {
	  // RDD 151014 Kill Contactless as there are invalid files
	  DBG_MESSAGE( "Config Files Missing - disable reader" );
	  terminal_set_status( ss_ctls_reader_fatal_error );
  }
#endif
  DBG_POINT;

  // RDD 290520 Trello #123 - option to use either new or old vas modules
  // RDD 021222 JIRA PIRPIN-2116 fix BMX load in factory.
  //terminal_init_vas( TERM_NEW_VAS, VIM_TRUE );
DBG_POINT;

terminal_save();
txn_save();

// RDD 211212 - start off with a clean slate now for idle processing.
txn.type = tt_none;
#ifdef _DEBUG
// terminal_set_status( ss_wow_basic_test_mode );
#endif

terminal_set_reboot_time();

// RDD 100818 JIRA WWBP-290 DG says offline mode should be cleared on power up
terminal_clear_status(ss_offline_test_mode);

TerminalVRKStatus(VIM_FALSE);

#ifdef _DEBUG
//terminal.last_logon_state = terminal.logon_state = logged_on;
#endif

#if 0 // RDD 230421 - move this all to run post switch logon as I can see that the QC app hasn't always built the 0600 request by the time we get here...
 // RDD - always logon to QC and feedback result.
DBG_MESSAGE("Attempting QC Logon at boot");


if (QC_IsInstalled(VIM_TRUE) == VIM_TRUE)
{
	// RDD 060819 JIRA WWBP-673 Stay forever waiting for client else problems ensue...  
	if ((res = QC_CheckForAuthChallengeRequest()) != VIM_ERROR_NONE)
	{
		pceft_debug_error(pceftpos_handle.instance, "Qwikcilver CMS Fail:", res);
	}
	else
	{
		if (QC_Logon(VIM_TRUE, VIM_FALSE) == VIM_ERROR_NONE)
		{
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Qwikcilver Logon", "Success");
			vim_event_sleep(2000);
		}
		else
		{
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Qwikcilver Logon", "Failed");
			pceft_debug(pceftpos_handle.instance, "QC logon: Fail");
			vim_event_sleep(5000);
		}
	}
}
else
{
	pceft_debug(pceftpos_handle.instance, "QC App Not Present");
}

#endif


// RDD 290523 added to restore CPAT filename etc. if upgrade S/W 

if( IS_R10_POS )
{
	// RDD 230623 JIRA_2446 RNDIS set as default link on WW PinPads if power up TMS fails.
	CommsSetup(VIM_TRUE);

	if ((res = tms_contact(VIM_TMS_LOGON_ONLY, VIM_TRUE, web_then_vim)) != VIM_ERROR_NONE)
	{
		pceft_debug(pceftpos_handle.instance, "Unable to Logon to WebLFD");
	}
}
else
{
	if ((ALH_PINPAD || FUEL_PINPAD || OOL_PINPAD) && IS_P400) {
		CommsSetup(VIM_TRUE);
	}
	// CDS / renew etc. if needed	
	CheckSecurity(VIM_TRUE);

	if ((res = tms_contact(VIM_TMS_LOGON_ONLY, VIM_TRUE, web_lfd)) != VIM_ERROR_NONE)
	{
		pceft_debug(pceftpos_handle.instance, "Unable to Logon to WebLFD");
	}
	// Open VHQ callbacks for non-wow banners
	tms_vhq_open();
}


VAA_CheckDebug();
pceft_debug(pceftpos_handle.instance, terminal_vas_log_status());
pceft_debugf(pceftpos_handle.instance, "Using Card Table from: %s", TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "CARDS.XML"));


IdleDisplay();
vim_file_delete(JsonReqFile);

terminal.original_mode = terminal.mode;

//WifiInit();
// RDD 010322 - Auto Logon for Standalone
{
	VIM_SIZE Completed = 0, Expired = 0;
	PurgePreauthFiles(&Completed, &Expired);
}

if (IS_STANDALONE)
{
	ReversalAndSAFProcessing(SEND_NO_SAF, VIM_FALSE, VIM_TRUE, VIM_FALSE);
}

 return VIM_ERROR_NONE;
}