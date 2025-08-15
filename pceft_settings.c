#include <incs.h>
VIM_DBG_SET_FILE("TK");
VIM_PCEFTPOS_PARAMETERS pceftpos_settings;

extern VIM_BOOL IsPosConnected( VIM_BOOL fWaitForPos );
extern VIM_ERROR_PTR WifiSetupPwd(void);


/* For last pos command structure */
#define TAG_LAST_POS_RESPONSE_PENDING   (VIM_UINT8 *)"\xC0"
#define TAG_LAST_POS_CMD_CODE           (VIM_UINT8 *)"\xC1"
#define TAG_LAST_POS_SUB_CODE           (VIM_UINT8 *)"\xC2"

/* For pceft link layer structure */
#define TAG_PCEFT_LL_PORT               (VIM_UINT8 *)"\xC0"
#define TAG_PCEFT_LL_BAUD               (VIM_UINT8 *)"\xC1"
#define TAG_PCEFT_LL_DATA_BITS          (VIM_UINT8 *)"\xC2"
#define TAG_PCEFT_LL_STOP_BITS          (VIM_UINT8 *)"\xC3"
#define TAG_PCEFT_LL_PARITY             (VIM_UINT8 *)"\xC4"
#define TAG_PCEFT_LL_FLOW_CONTROL       (VIM_UINT8 *)"\xC5"
#define TAG_PCEFT_LL_PROTOCOL           (VIM_UINT8 *)"\xC6"

/* For dialog params structure */
#define TAG_PCEFT_DIALOG_ID             (VIM_UINT8 *)"\xC0"
#define TAG_PCEFT_DIALOG_X              (VIM_UINT8 *)"\xC1"
#define TAG_PCEFT_DIALOG_Y              (VIM_UINT8 *)"\xC2"
#define TAG_PCEFT_DIALOG_POS            (VIM_UINT8 *)"\xC3"
#define TAG_PCEFT_DIALOG_TOPMOST        (VIM_UINT8 *)"\xC4"
#define TAG_PCEFT_DIALOG_TITLE          (VIM_UINT8 *)"\xC5"

/* For printer structure */
#define TAG_PCEFT_PRINTER_CUT           (VIM_UINT8 *)"\xC0"
#define TAG_PCEFT_PRINTER_AUTO_PRINT    (VIM_UINT8 *)"\xC1"
#define TAG_PCEFT_PRINTER_JOURNAL       (VIM_UINT8 *)"\xC2"

/* For base pceft settings values */
#define TAG_PCEFT_DEVICE_CODE           (VIM_UINT8 *)"\xC0"

#define TAG_PCEFT_LINK_LAYER      (VIM_UINT8 *)"\xF4"
#define TAG_PCEFT_DIALOG          (VIM_UINT8 *)"\xF5"
#define TAG_PCEFT_PRINTER         (VIM_UINT8 *)"\xF6"

#ifdef DRIVER_PINPAD
#define PCEFT_PIPE
#endif

/*----------------------------------------------------------------------------*/
const char *pceft_settings_filename = VIM_FILE_PATH_LOCAL "pc_cfg.dat";
VIM_BOOL pceft_settings_reconfigure_busy = VIM_FALSE;
/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_UART_PARAMETERS structure */
#if 0
static VIM_MAP const pceftpos_settings_uart_map[]=
{
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PORT,VIM_UART_PARAMETERS,port,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_BAUD,VIM_UART_PARAMETERS,baud_rate,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_DATA_BITS,VIM_UART_PARAMETERS,data_bits,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PARITY,VIM_UART_PARAMETERS,parity,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_STOP_BITS,VIM_UART_PARAMETERS,stop_bits,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_FLOW_CONTROL,VIM_UART_PARAMETERS,flow_control,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_END
};
/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_PCEFTPOS_DIALOG_PARAMETERS structure */
static VIM_MAP const pceftpos_settings_dialog_map[]=
{
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_ID,VIM_PCEFTPOS_DIALOG_PARAMETERS,id,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_X,VIM_PCEFTPOS_DIALOG_PARAMETERS,x,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_Y,VIM_PCEFTPOS_DIALOG_PARAMETERS,x,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_POS,VIM_PCEFTPOS_DIALOG_PARAMETERS,pos,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_TOPMOST,VIM_PCEFTPOS_DIALOG_PARAMETERS,topmost,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DIALOG_TITLE,VIM_PCEFTPOS_DIALOG_PARAMETERS,title,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_END
};

/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_PCEFTPOS_PRINTER_PARAMETERS structure */
static VIM_MAP const pceftpos_settings_printer_map[]=
{
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_PRINTER_AUTO_PRINT,VIM_PCEFTPOS_PRINTER_PARAMETERS,auto_print,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_PRINTER_CUT,VIM_PCEFTPOS_PRINTER_PARAMETERS,cut_receipt,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_PRINTER_JOURNAL,VIM_PCEFTPOS_PRINTER_PARAMETERS,journal_receipt,&vim_map_accessor_char_string,VIM_NULL),
  VIM_MAP_DEFINE_END
};

/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_PCEFTPOS_PARAMETERS structure */

static VIM_MAP pceftpos_settings_map[]=
{
  VIM_MAP_DEFINE_SUB_MAP(TAG_PCEFT_LINK_LAYER,VIM_PCEFTPOS_PARAMETERS,uart_parameters,pceftpos_settings_uart_map),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PROTOCOL,VIM_PCEFTPOS_PARAMETERS,protocol,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_SUB_MAP(TAG_PCEFT_DIALOG,VIM_PCEFTPOS_PARAMETERS,dialog,pceftpos_settings_dialog_map),
  VIM_MAP_DEFINE_SUB_MAP(TAG_PCEFT_PRINTER,VIM_PCEFTPOS_PARAMETERS,printer,pceftpos_settings_printer_map),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DEVICE_CODE,VIM_PCEFTPOS_PARAMETERS,device_code,&vim_map_accessor_char_string,VIM_NULL),    
  VIM_MAP_DEFINE_END
};
#endif
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceftpos_settings_set_defaults(VIM_PCEFTPOS_PARAMETERS *settings, VIM_BOOL AllSettings)
{
  /* set default physical layer parameters of the pceftpos connection */
  /* set the default serial port for this terminal type */
	DBG_MESSAGE( "Setting Default Values for PCEFT Interface" );

    settings->uart_parameters.uart_over_usb = VIM_UART_OVER_USB_NO;
    vim_pceftpos_get_port(&settings->uart_parameters.port);

#ifdef _WIN32  
  settings->uart_parameters.port = 1;
#endif

  if( settings->uart_parameters.port == 9 )
      settings->uart_parameters.uart_over_usb = VIM_UART_OVER_USB_YES;

	VIM_DBG_PRINTF2( "Port: %d (USB:%d)", settings->uart_parameters.port, settings->uart_parameters.uart_over_usb );

  /* set the default baud rate */
#ifdef _WIN32

    if( AllSettings )
		settings->uart_parameters.baud_rate = VIM_UART_BAUD_115200;
#else
	if( AllSettings )
		settings->uart_parameters.baud_rate = VIM_UART_BAUD_38400;
#endif
  /* set the default data bits */
  settings->uart_parameters.data_bits = VIM_UART_DATA_BITS_8;
  /* set the default stop bits */
  settings->uart_parameters.stop_bits = VIM_UART_STOP_BITS_1;
  /* set the default parity */
  settings->uart_parameters.parity = VIM_UART_PARITY_NONE;
  /* set the default flow control */
  settings->uart_parameters.flow_control = VIM_UART_FLOW_CONTROL_NONE;

  /* set the default data link layer parameters of the pceftpos connection */
  settings->protocol = VIM_PCEFTPOS_PROTOCOL_STX_VLI_ETX_CRC;
  /* set the default pceftpos dialog parameters  */
  vim_mem_set(&settings->dialog, ' ', sizeof(settings->dialog));

  /* set the default pceftpos printer parameters  */
  settings->printer.auto_print = '0';
  settings->printer.cut_receipt = '0';
  settings->printer.journal_receipt = '1';

  /* set the device code */ 
                
  // RDD 100523 JIRA PIRPIN-2376
  settings->device_code = '1';
  vim_pceftpos_set_device_code(pceftpos_handle.instance, settings->device_code);


  /* set the bank code */  
  if( AllSettings )	
	  settings->bank_code = PCEFT_BANK_CODE;

  /* set the hardware code */
  settings->hardware_code= '4';

  // 220220 DEVX - set defaults for TCPIP
#ifdef _WIN32
 // settings->protocol = VIM_PCEFTPOS_PROTOCOL_TCP;
  vim_strcpy(settings->tcp_parameters.hostname, "192.168.000.032");
  settings->tcp_parameters.tcp_port = 2020;
#else
  vim_strcpy(settings->tcp_parameters.hostname, "192168000001");
  settings->tcp_parameters.tcp_port = 2012;
#endif
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_settings_save(void)
{
    VIM_DBG_PRINTF2("HostID:%s Port:%d", pceftpos_settings.tcp_parameters.hostname, pceftpos_settings.tcp_parameters.tcp_port);
    if ( vim_file_set(pceft_settings_filename, (const void *)&pceftpos_settings, VIM_SIZEOF(pceftpos_settings)) != VIM_ERROR_NONE)
    {
        /* return without error */
        VIM_DBG_MESSAGE("!!!! PCEFT Config File Save Error !!!!");
        pceft_configure(VIM_FALSE);
    }
    else
    {
        VIM_DBG_MESSAGE("!!!! PCEFT Config File Save OK !!!!");
    }  
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_settings_reconfigure( VIM_BOOL CloseOldSession )
{ 
	// function is not re-entrant, using mutex (pceft_settings_reconfigure_busy)
	VIM_ERROR_PTR res;  
	VIM_RTC_UPTIME time_now, time_end;
  
	VIM_DBG_MESSAGE( " pceft_settings_reconfigure ---------------------------" );

	// wait for other instance to exit  
	vim_rtc_get_uptime( &time_now );  
	time_end = time_now + 10000L;  // safety timer
  
	while( pceft_settings_reconfigure_busy && time_now < time_end )
	{
		VIM_DBG_MESSAGE( " pceft_settings_reconfigure: waiting for other instance to exit...");    
		vim_event_sleep( 50 );    
		vim_rtc_get_uptime( &time_now );  
	}  
	pceft_settings_reconfigure_busy = VIM_TRUE;
  
	/* check if there is already a pceftpos instance */
	if (pceftpos_handle.instance != VIM_NULL)    
		pceft_close();
    
    // Can't have a sleep here as task will be run and task may not exist
	//vim_event_sleep(500);

	// RDD 080118 - if ComPort error then warn user and set defaults, then if still not working offer menu to fix
    if(( res = vim_pceftpos_new(&pceftpos_handle, &pceftpos_settings)) == VIM_ERROR_NONE )
	{	
#ifndef _WIN32
        if (TERM_NEW_VAS)
        {
            mal_nfc_GetCommsFd(pceft_settings_is_tcp(), &pceftpos_handle, &comms_fd);
            VIM_DBG_PRINTF1("comms_fd = %d", comms_fd);
        }
#endif
		//VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_connected_flag( pceftpos_handle.instance ) );  

		//VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_connected_flag( pceftpos_handle.instance ) );  

	}  
	else  
	{
		VIM_DBG_MESSAGE( " vim_pceftpos_new: Failed");	  			  
	}

	pceft_settings_reconfigure_busy = VIM_FALSE;
	return res;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_close(void)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE; 

  // Do not nullify instance until delete is done (may have some time with BT)
  if( pceftpos_handle.instance != VIM_NULL )
  { 	  
    res = vim_pceftpos_delete( pceftpos_handle.instance );
	  if( res == VIM_ERROR_NONE )
	  {
      pceftpos_handle.instance = VIM_NULL;
DBG_MESSAGE( "CLOSING PCEFT SESSION: Success" );
	  }
	  else
	  {
DBG_MESSAGE( "CLOSING PCEFT SESSION: Failed");
	  }
  }
  else
  {
DBG_MESSAGE( "CLOSING PCEFT SESSION: Nothing to Close");
  }

  return res;
  //return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_settings_load_only(void)
{  
	VIM_SIZE buffer_size; 
	VIM_PCEFTPOS_PARAMETERS temp_settings;
	// Get the file (if it exists) to overwrite the defaults with updated settings
	VIM_ERROR_RETURN_ON_FAIL(pceftpos_settings_set_defaults(&pceftpos_settings, VIM_TRUE));
	if( vim_file_get( pceft_settings_filename, (void *)&temp_settings, &buffer_size, VIM_SIZEOF(pceftpos_settings) ) != VIM_ERROR_NONE )
	{
        // RDD 301214 added - ensure that something is loaded by setting defaults first
        VIM_DBG_MESSAGE( "PCEFT_CFG File Read Error: Use Default Settings" );
		VIM_ERROR_RETURN_ON_FAIL( VIM_ERROR_NOT_FOUND );  
	}
	else
	{
        pceftpos_settings = temp_settings;
        VIM_DBG_PRINTF2("Using pc_cfg.dat settings: protocol=%d port=%d", pceftpos_settings.protocol, pceftpos_settings.tcp_parameters.tcp_port);
    }

    VIM_DBG_STRING(pceftpos_settings.tcp_parameters.hostname);


	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_UINT8 pceft_get_port_number(void)
{
  return pceftpos_settings.uart_parameters.port;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
VIM_UINT8 pceft_settings_get_port(void)
{
  return pceftpos_settings.uart_parameters.port;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/


VIM_ERROR_PTR pceft_set_USB(void)
{
    pceftpos_settings.uart_parameters.port = 9;
    pceftpos_settings.uart_parameters.uart_over_usb = VIM_UART_OVER_USB_YES;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR pceft_set_RS232(void)
{
#ifdef _WIN32
    pceftpos_settings.uart_parameters.port = 2;	// RDD 311214 - usually set COM0COM to port2
#else
    pceftpos_settings.uart_parameters.port = 1;
#endif
    pceftpos_settings.uart_parameters.uart_over_usb = VIM_UART_OVER_USB_NO;

    return VIM_ERROR_USER_BACK;
}


// RDD 040221 JIRA PIRPIN-999 Luigi says please only set to USB because service team keep setting ot RS232 by mistake
#ifdef _WIN32
S_MENU ConfigPortMenu[] = { { "USB", pceft_set_USB },{ "RS232", pceft_set_RS232 } };
#else
S_MENU ConfigPortMenu[] = { { "USB", pceft_set_USB },{ "RS232", pceft_set_USB } };
#endif

VIM_ERROR_PTR pceft_config_port(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(ConfigPortMenu, VIM_LENGTH_OF(ConfigPortMenu), "PCEFT UART Port", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
#define WEBLFD_LOGON_URL "http://weblfd.linkly.com.au/api/v1/logon"
#define WEBLFD_FILES_URL "http://weblfd.linkly.com.au/files"
#define WEBLFD_PROXY_PORT "5000"


/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR ConfigureWebLFDProxyRoute(VIM_CHAR* ProxyIP)
{
    VIM_CHAR Buffer[500] = { 0 };

    vim_sprintf(Buffer, "%s\n%s\n%s\n%s\n", WEBLFD_LOGON_URL, WEBLFD_FILES_URL, ProxyIP, WEBLFD_PROXY_PORT);
    VIM_ERROR_RETURN_ON_FAIL( vim_file_set( WEB_LFD_PROXY_CONFIG_PATH, Buffer, vim_strlen( Buffer )));
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR pceft_config_tcp_hostname(void)
{
    //VIM_CHAR tcp_hostname[VIM_SIZEOF(pceftpos_settings.tcp_parameters.hostname)];
    VIM_CHAR Port[4 + 1];
    kbd_options_t options;
    VIM_CHAR HostNameDigits[20+1];

    vim_mem_clear( HostNameDigits, VIM_SIZEOF( HostNameDigits ));

    options.timeout = ENTRY_TIMEOUT;
    options.min = 12;    // DEVX 190320 192.168.000.001 - dots are not included only digits
    options.max = 12;
#ifndef _VOS2
    options.entry_options = KBD_OPT_IP | KBD_OPT_ALLOW_BLANK | KBD_OPT_DISP_ORIG;
#else
    options.entry_options = KBD_OPT_ALLOW_BLANK | KBD_OPT_DISP_ORIG;
#endif
    options.CheckEvents = VIM_FALSE;

    VIM_DBG_STRING(pceftpos_settings.tcp_parameters.hostname);
#ifdef _nnWIN32
    vim_strcpy(HostNameDigits, pceftpos_settings.tcp_parameters.hostname);
    // RDD 240320 - for VOS2 remove delimeters as not supported by gui
#else
    {
        VIM_SIZE n, Len = vim_strlen(pceftpos_settings.tcp_parameters.hostname);
        VIM_CHAR *Ptr = pceftpos_settings.tcp_parameters.hostname;
        for( n = 0; n < Len; Ptr++ )
        {
            if (*Ptr != '.')
                HostNameDigits[n++] = *Ptr;
        }
    }
#endif
    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL( display_data_entry( HostNameDigits, &options, DISP_TCP_HOSTNAME, VIM_PCEFTPOS_KEY_NONE, 0 ));
    VIM_DBG_STRING(HostNameDigits);

#ifndef _WIN32
    {
        vim_mem_clear(pceftpos_settings.tcp_parameters.hostname, VIM_SIZEOF(pceftpos_settings.tcp_parameters.hostname));
        // Add the dots as none are returned in P400

        vim_mem_copy( &pceftpos_settings.tcp_parameters.hostname[0], &HostNameDigits[0], 3 );
        pceftpos_settings.tcp_parameters.hostname[3] = '.';
        vim_mem_copy(&pceftpos_settings.tcp_parameters.hostname[4], &HostNameDigits[3], 3 );
        pceftpos_settings.tcp_parameters.hostname[7] = '.';
        vim_mem_copy(&pceftpos_settings.tcp_parameters.hostname[8], &HostNameDigits[6], 3 );
        pceftpos_settings.tcp_parameters.hostname[11] = '.';
        vim_mem_copy(&pceftpos_settings.tcp_parameters.hostname[12], &HostNameDigits[9], 3 );
    }
#else
    vim_strcpy(pceftpos_settings.tcp_parameters.hostname, HostNameDigits);
#endif
    VIM_DBG_STRING(pceftpos_settings.tcp_parameters.hostname);

    //display_screen(DISP_TCP_HOSTNAME, 0, pceftpos_settings.tcp_parameters.hostname ? pceftpos_settings.tcp_parameters.hostname : " ");
    //VIM_ERROR_RETURN_ON_FAIL(data_entry(pceftpos_settings.tcp_parameters.hostname, &options));

    // 180220 DEVX Add option to change TCPIP Port 
    if (pceftpos_settings.tcp_parameters.tcp_port <= 9999)
    {
        vim_sprintf(Port, "%4.4d", pceftpos_settings.tcp_parameters.tcp_port);
    }
    else
    {
        vim_mem_clear(Port, VIM_SIZEOF(Port));
    }

    options.min = 4;
    options.max = 4;
    options.entry_options = KBD_OPT_DISP_ORIG;

    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(Port, &options, DISP_TCP_PORT, VIM_PCEFTPOS_KEY_NONE, 0));

    pceftpos_settings.tcp_parameters.tcp_port = asc_to_bin(Port, 4);
    VIM_DBG_NUMBER(pceftpos_settings.tcp_parameters.tcp_port);
    pceftpos_settings.protocol = VIM_PCEFTPOS_PROTOCOL_TCP;

#if 0
    // RDD 211022 Adding Wifi Initialisation from this point if not alreadty setup via TMS
    if(IS_V400m)
    {
        if (config_get_yes_no("Use Wifi?", "Wifi Set as Primary"))
        {
            if( WifiRunFromParams() != VIM_ERROR_NONE )
            {
                if (config_get_yes_no("Wifi requires config\nConfigure now?", "Run Wifi Config"))
                {
                    vim_event_sleep(1000);
                    WifiSetupPwd();
                }
            }
            ConfigureWebLFDProxyRoute(pceftpos_settings.tcp_parameters.hostname);
        }
    }
    return VIM_ERROR_USER_BACK;
#endif
    return VIM_ERROR_NONE;
}

VIM_ERROR_PTR pceft_set_ETX(void)
{
    pceftpos_settings.protocol = VIM_PCEFTPOS_PROTOCOL_STX_DLE_ETX_LRC;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR pceft_set_VLI(void)
{
    pceftpos_settings.protocol = VIM_PCEFTPOS_PROTOCOL_STX_VLI_ETX_CRC;
    return VIM_ERROR_USER_BACK;
}


S_MENU ConfigProtocolMenu[] = {
    { "STX/VLI/CRC", pceft_set_VLI },
    { "STX/ETX/LRC", pceft_set_ETX },
    { "TCPIP", pceft_config_tcp_hostname }
};


VIM_ERROR_PTR pceft_config_protocol(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(ConfigProtocolMenu, VIM_LENGTH_OF(ConfigProtocolMenu), "PCEFT Protocol", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
    return res;
}


VIM_ERROR_PTR pceft_set_9600(void)
{
	pceftpos_settings.uart_parameters.baud_rate = VIM_UART_BAUD_9600;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR pceft_set_38400(void)
{
	pceftpos_settings.uart_parameters.baud_rate = VIM_UART_BAUD_38400;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR pceft_set_115200(void)
{
	pceftpos_settings.uart_parameters.baud_rate = VIM_UART_BAUD_115200;
    return VIM_ERROR_USER_BACK;
}


S_MENU ConfigBaudMenu[] =	{{ "9600", pceft_set_9600 },
						    {  "38400", pceft_set_38400 },
							{  "115200", pceft_set_115200 }};

VIM_ERROR_PTR pceft_config_baud_rate(void)
{						
	VIM_ERROR_PTR res;
	while(( res = MenuRun( ConfigBaudMenu, VIM_LENGTH_OF(ConfigBaudMenu), "PCEFT Baud Rate", VIM_FALSE, VIM_TRUE )) == VIM_ERROR_NONE );
	if( res == VIM_ERROR_USER_BACK )			
		res = VIM_ERROR_NONE;
	return res;
}





S_MENU ConfigPCEFTMenu[] = { {    "Comms Type ", pceft_config_protocol },
                                { "Baud Rate  ", pceft_config_baud_rate },
                                { "Serial Port"  , pceft_config_port } };


VIM_ERROR_PTR pceft_configure(VIM_BOOL fUseDefaultSettings)
{			
	while( MenuRun( ConfigPCEFTMenu, VIM_LENGTH_OF(ConfigPCEFTMenu), "PCEFT Comms", VIM_FALSE, VIM_TRUE ) == VIM_ERROR_NONE );		
     return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_settings_get_conf_string( VIM_TEXT* conf_string, VIM_SIZE len )
{
  VIM_ERROR_RETURN_ON_NULL( conf_string );

  conf_string[0] = 0x00;
  
  if( pceftpos_settings.protocol == VIM_PCEFTPOS_PROTOCOL_TCP )
  {
    vim_snprintf( conf_string, len, "%s:%d", pceftpos_settings.tcp_parameters.hostname, pceftpos_settings.tcp_parameters.tcp_port );
    return VIM_ERROR_NONE;
  }

  if( pceftpos_settings.uart_parameters.port == 2 )
  {
    vim_snprintf( conf_string, len, "USB");
    return VIM_ERROR_NONE;
  }

  if( pceftpos_settings.uart_parameters.port == 1 )
  {
    vim_snprintf( conf_string, len, "RS232, %s, %d", 
                  pceftpos_settings.protocol == VIM_PCEFTPOS_PROTOCOL_STX_VLI_ETX_CRC ? "VLI":"STX/ETX", 
                  pceftpos_settings.uart_parameters.baud_rate );
    return VIM_ERROR_NONE;
  }

  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_BOOL pceft_settings_is_tcp(void)
{
    // RDD 051118 return tcp
    VIM_BOOL RetVal = (pceftpos_settings.protocol == VIM_PCEFTPOS_PROTOCOL_TCP) ? VIM_TRUE : VIM_FALSE;
    return RetVal;
}
