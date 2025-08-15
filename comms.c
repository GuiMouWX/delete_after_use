/**
 * \file comms.c
 *
 * \brief
 * functions for communicating with hosts
 *
 * \details
 * Provides communications functions common to both pos and internal comms
 *
 * \author
 * Eftsolutions Australia
 */
 
#include "incs.h"

extern VIM_ERROR_PTR WifiInit(void);
extern VIM_ERROR_PTR WifiStatus(VIM_BOOL SilentMode, VIM_UINT8 Usage);
extern VIM_BOOL BT_Available(void);
extern VIM_ERROR_PTR WifiSetConfig(VIM_CHAR* ssid, VIM_CHAR* psk, VIM_CHAR* protocol);



VIM_DBG_SET_FILE("T3");
/*----------------------------------------------------------------------------*/
VIM_NET_HANDLE comms_net_handle = {VIM_NULL};
VIM_NET_HANDLE gprs_ppp_handle = {VIM_NULL};

/* 
** RDD 190914 - ALH Project: Declare global structures for Comms Config 
** These struuctures are populated during power-up from 
** the config file: CommsCfg.xml 
*/

const char *CfgP400FileName = VIM_FILE_PATH_LOCAL "P400.xml";
const char *CfgV400FileName = VIM_FILE_PATH_LOCAL "V400m.xml";
const char *CfgP400TestFileName = VIM_FILE_PATH_LOCAL "P400T.xml";
const char *CfgV400TestFileName = VIM_FILE_PATH_LOCAL "V400mT.xml";
char CfgFileName[30] = { 0 };

//const char *ClientCertFile = "PP_TLS_CERT.pem";

#define MAX_CONNECT_TIME		"MAX_CONNECT_TIME>"
#define TRICKLE_FEED_TIME		"TRICKLE_FEED_TIME>"

#define MERCHANT_ROUTING		"MCR>"		// RDD 260318 Added

#define TMS_COMMS_TAG		"TmsComms>"
#define TXN_COMMS_TAG		"TxnComms>"

#define PRIORITY_TAG "PRIORITY>"

#define CONNECT_TIMEOUT_TAG	"CONNECT_TIMEOUT>"
#define RECEIVE_TIMEOUT_TAG	"RECEIVE_TIMEOUT>"
// TMS Params
#define _MAX_FIELD_SIZE				"MAX_FIELD_SIZE>"
#define _WINDOW_SIZE				"WINDOW_SIZE>"
#define _RETRIES					"RETRIES>"

#define START_COMMENT "<!--"
#define END_COMMENT   "-->"

#define GPRS_PARAMS_TAG		"GPRS>"
#define PPP_PARAMS_TAG		"PPP>"
#define TCP_PARAMS_TAG		"TCP>"
#define WIFI_TCP_PARAMS_TAG "WIFI_TCP>"
#define PSTN_PARAMS_TAG		"PSTN_HDLC>"
#define WIFI_PARAMS_TAG	    "WIFI>"
#define PCEFT_PARAMS_TAG	"PCEFT>"

// PPP specific Params
#define APN_TAG				"apn>"
#define USER_ID_TAG			"user_id>"
#define PASSWORD_TAG		"password>"
#define SIM_PIN_TAG			"sim_pin>"

// TCP specific Params

#define IP_ADDR_ELEMENTS		4

#define IP_ADDR1_TAG			"ip_address1>"
#define IP_ADDR2_TAG			"ip_address2>"
#define IP_ADDR3_TAG			"ip_address3>"
#define IP_ADDR4_TAG			"ip_address4>"
#define TCP_PORT_TAG			"tcp_port>"
#define CLIENT_CERT_TAG         "client_cert>"
#define SNI1_TAG                 "SNI_ODD>"		// JIRA PIRPIN-1765 Load balancing for Odds and Evens
#define SNI2_TAG                 "SNI_EVEN>"

#define DEST_URL1_TAG            "url1>"
#define DEST_URL2_TAG            "url2>"

// PSTN specific parameters
#define DIAL_PRIMARY_TAG		"PRIMARY>"
#define DIAL_SECONDARY_TAG		"SECONDARY>"
#define NII						"NII>"
#define BLIND_DIAL				"BLIND_DIAL>"
#define PABX_PREFIX				"PABX_PREFIX>"
#define HEADER_TYPE				"HEADER_TYPE>"
#define HDLC					"HDLC_SYNC>"
#define FAST_CONNECT			"FAST_CONNECT>"


// PCEFTPOS specific parameters
// If these params are set them we're required to use TCPIP connection to the client

#define PCEFT_IP_ADDR			"PCEFT_IP_ADDR>"
#define PCEFT_IP_PORT			"PCEFT_IP_PORT>"
// RS232 connection parameters 
#define PCEFT_BAUD				"PCEFT_BAUD>"
#define PCEFT_UART				"PCEFT_UART>"
// Other PCEFT Params
#define PCEFT_BANK				"PCEFT_BANK>"
#define PCEFT_HARDWARE			"PCEFT_HARDWARE>"

#define SSID_TAG                "SSID>"
#define PSK_TAG                 "PSK>"
#define KEY_MGMT_TAG            "KEY_MGMT>"


#define	TAG_DATA_LEN_MAX		10000


VIM_NET_CONNECT_PARAMETERS sTMS_SETTINGS[NUMBER_OF_SETTINGS];
VIM_NET_CONNECT_PARAMETERS sTXN_SETTINGS[NUMBER_OF_SETTINGS];


/*
static VIM_UINT8 host_connected=WBC_ACQUIRER;
*/
static VIM_UINT8 host_connected=99;

static VIM_RTC_UPTIME last_receive;
static VIM_RTC_UPTIME last_transmit;
//static VIM_BOOL dial_fallback;
#if 0

VIM_BOOL is_comms_error(VIM_ERROR_PTR error_code)
{
  if( (error_code == VIM_ERROR_NO_LINK)
    || (error_code == VIM_ERROR_MODEM)
    || (error_code == VIM_ERROR_NO_ANSWER)
    || (error_code == VIM_ERROR_DIAL_BUSY)
    || (error_code == VIM_ERROR_NO_CARRIER)
    || (error_code == VIM_ERROR_CONNECT_TIMEOUT)
    || (error_code == ERR_TRANSMISSION_FAILURE)
    || (error_code == ERR_NO_RESPONSE)
    || (error_code == ERR_INVALID_MSG)
    || (error_code == VIM_ERROR_TIMEOUT)
    || (error_code == VIM_ERROR_NO_EFT_SERVER)
    || (error_code == ERR_UNABLE_TO_GO_ONLINE_DECLINED)
    || ((error_code == ERROR_CNP) && (VIM_ERROR_NONE == vim_mem_compare(txn.cnp_error, "0108", 4)))
    )
    return VIM_TRUE;
  else
    /*
      VIM_ERROR_NO_DIAL_TONE:
      ERR_NO_LINK_SETUP_EFT_SERVER:
      ERR_NO_EFT_SERVER:    
    */
    return VIM_FALSE;
}

#endif

/*----------------------------------------------------------------------------*/
VIM_BOOL comms_internal_mode(void)
{
  //VIM_BOOL internal_supported = VIM_FALSE;
  VIM_CHAR device_code;

  if( pceftpos_handle.instance == VIM_NULL )
  {    
	  DBG_POINT;    
	  return VIM_TRUE;
  }
  DBG_POINT;    

  //VIM_DBG_VAR(terminal.standalone_parameters);
#if 1
  vim_pceftpos_get_device_code(pceftpos_handle.instance, &device_code);
  if(( terminal.standalone_parameters & STANDALONE_INTERNAL_MODEM ) || (device_code == '2') || (device_code == '3' ))
  {    
	  DBG_POINT;    
	  return VIM_TRUE;
  }
  else  
  {		
	  DBG_POINT;    
	  return VIM_FALSE;
  }
#else
	  return VIM_TRUE;
#endif
}
/*----------------------------------------------------------------------------*/
VIM_BOOL comms_connected(VIM_UINT8 host)
{
  VIM_BOOL connected;
  
  VIM_DBG_NUMBER( host );
  VIM_DBG_NUMBER( host_connected );
  if( host_connected != host )
  {
	  DBG_MESSAGE( "Comms NOT connected - NO HOST" );    
	  return VIM_FALSE;
  }
  if( comms_net_handle.instance_ptr==VIM_NULL )
  {
	  DBG_MESSAGE( "Comms NOT connected - NO INSTANCE" );        
	  return VIM_FALSE;
  }

  if( vim_net_is_connected( comms_net_handle.instance_ptr, &connected )==VIM_ERROR_NONE )
  {
    if( connected )
    {
		DBG_MESSAGE( "Comms already Connected" );
		if( gprs_ppp_handle.instance_ptr )      
		{ 
			DBG_MESSAGE( "PPP instance exists so check Network Connection too" );
			//if( vim_net_is_connected( gprs_ppp_handle.instance_ptr, &connected )==VIM_ERROR_NONE )    
            if(1)
			{
                connected = VIM_TRUE;
				DBG_MESSAGE( "Network already connected" );
				return connected;  
			}
			else
			{
				DBG_MESSAGE( "Network NOT connected" );
				return connected;  
			}
		}
    }
    else
    {
      if( gprs_ppp_handle.instance_ptr )
      { // GPRS comms, TCP not connected, close it (might be socket error)
		  DBG_MESSAGE( "DDDDDDDDDDDDDDDDDDD Network NOT connected - do full disconnect DDDDDDDDDDDDDDDDDDD" );        
		  comms_disconnect();
      }
    }
    return connected;
  }

  return VIM_FALSE;
}
/*----------------------------------------------------------------------------*/
void comms_get_phone_string
(
  VIM_UNUSED(comms_params_t *, params), 
  VIM_UNUSED(char, phone[11])
)
{
#if 0 /* TODO: */
  VIM_UINT8 i;
  
  bcd_to_asc(params->tms_phone, phone, 10);

  for (i=0; i<10; i++)
  {
    if (phone[i] < '0' || phone[i] > '9')
      phone[i] = 0;
  }
  phone[i] = 0;
#endif  
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_get_host_params( VIM_UNUSED(VIM_UINT8, host), comms_params_t *params )
{
  VIM_UINT32 i;
  vim_mem_clear(params, sizeof(*params));

  if( comms_net_handle.instance_ptr )
    if( gprs_ppp_handle.instance_ptr )
    {
      params->comms_type[0] = 4;  // GPRS. Need to rethink the way to show what header to use
#if 0
      params->argent_id = 0x9C41;
      vim_mem_copy( params->sha, "4980000006", 10 );
#endif
      params->argent_id = 0x00;
      for( i=0; i<VIM_SIZEOF(terminal.acquirers[0].AQTSENDID); i++ )
      {
        params->argent_id <<= 8;
        params->argent_id |= terminal.acquirers[0].AQTSENDID[i];
      }

      // got only 8 chars from TMS
      vim_utl_bcd_to_asc( terminal.acquirers[0].AQHOSTADDR, params->sha+2, VIM_SIZEOF(params->sha)-2 );
      params->sha[0] = '4';
      params->sha[1] = '9';
      
VIM_DBG_DATA(terminal.acquirers[0].AQHOSTADDR,4);
VIM_DBG_STRING(params->sha);
VIM_DBG_VAR(params->argent_id);
    }

  /*  TODO: add params */
 /*  *params = record.comms.data.params; */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Re-write the primary access comms table record for the host(acquirer) specified */
VIM_ERROR_PTR comms_set_host_params
(
  VIM_UNUSED(VIM_UINT8, host),
  VIM_UNUSED(comms_params_t const*, parameters_ptr)
)
{
  /*  TODO: */
/*   record.comms.data.params = *parameters_ptr; */
  /* return without errors */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

// RDD 101214 - TODO - code reduction on this !
VIM_ERROR_PTR connect_wifi(VIM_NET_CONNECT_PARAMETERS *net_conn_params)
{
    //VIM_BOOL connected = VIM_FALSE;
    //VIM_ERROR_PTR error = VIM_ERROR_NONE;
    // RDD 250914 - Point to the pre-set settings 
    //VIM_NET_CONNECT_PARAMETERS *wifi_settings = &sTXN_SETTINGS[ETHERNET_SETTINGS];

    return VIM_ERROR_NONE;
}




// RDD 101214 - TODO - code reduction on this !
VIM_ERROR_PTR connect_tcp( VIM_NET_CONNECT_PARAMETERS *net_conn_params, eCOMMS_METHODS Route)
{
	VIM_BOOL connected = VIM_FALSE;
    VIM_NET_CONNECT_PARAMETERS *gprs_tcp_settings = net_conn_params;

#ifdef _DEBUG
	if (TERM_FORCE_GPRS)
	{
		Route = GPRS;
	}
#endif
	VIM_DBG_PRINTF2("Connecting TCP... SSL=%d Route=%d", gprs_tcp_settings->interface_settings.gprs_tcp.isSSL, Route);
	pceft_debugf(pceftpos_handle.instance, "TCP Connex Connect URL=%s SNI=%s", gprs_tcp_settings->interface_settings.gprs_tcp.dest_url, gprs_tcp_settings->interface_settings.gprs_tcp.SNI);

	// RDD 050321 - Set the ppp instance to indicarte whether GPRS or not
	gprs_tcp_settings->interface_settings.gprs_tcp.ppp_instance_ptr = gprs_ppp_handle.instance_ptr;

	gprs_tcp_settings->interface_settings.gprs_tcp.Route = Route;

	VIM_DBG_VAR(gprs_tcp_settings->interface_settings.gprs_tcp.ppp_instance_ptr);

	VIM_ERROR_IGNORE( vim_net_is_connected( comms_net_handle.instance_ptr, &connected ) );	
	if( !connected )		
	{			
		// Remote must have disconnected		
		DBG_MESSAGE( "(2)TCP Connecting..." );						
		VIM_DBG_NUMBER( gprs_tcp_settings->connection_timeout );			
		VIM_DBG_NUMBER( gprs_tcp_settings->receive_timeout );			
		VIM_ERROR_RETURN_ON_FAIL( vim_net_connect_request( &comms_net_handle, gprs_tcp_settings ));    					
		DBG_MESSAGE( "(2)TCP Connected !" );
	}
	VIM_DBG_VAR( comms_net_handle.instance_ptr );
	VIM_DBG_VAR( gprs_ppp_handle.instance_ptr );
	return VIM_ERROR_NONE;
}


#if 0
VIM_ERROR_PTR connect_ppp(VIM_NET_CONNECT_PARAMETERS *net_conn_params)
{
	VIM_NET_CONNECT_PARAMETERS *gprs_ppp_settings = &sTXN_SETTINGS[HOST_PPP_SETTINGS];

	// GPRS not Ethernet
	while (!adk_com_ppp_connected())		
	{			
		DBG_MESSAGE("PPP NOT Connected !");	
		DBG_MESSAGE("PPP Re-Connecting...");			
		adk_com_init(0);		
	}
	
	DBG_MESSAGE("PPP Connected !");
}
#else

VIM_ERROR_PTR connect_ppp(VIM_NET_CONNECT_PARAMETERS *net_conn_params)
{
	VIM_NET_CONNECT_PARAMETERS *gprs_ppp_settings = &sTXN_SETTINGS[HOST_PPP_SETTINGS];

	DBG_MESSAGE("PPPPPPPPPPPPPPPPPPPPPP Connect PPP");

	if (!gprs_ppp_handle.instance_ptr)
	{
		DBG_MESSAGE("PPP NOT Connected !");
		DBG_MESSAGE("PPP Connecting...");
		VIM_ERROR_RETURN_ON_FAIL(vim_net_connect_request(&gprs_ppp_handle, gprs_ppp_settings));
		VIM_DBG_NUMBER(gprs_ppp_settings->connection_timeout);
		VIM_DBG_NUMBER(gprs_ppp_settings->receive_timeout);
		if (gprs_ppp_handle.instance_ptr != VIM_NULL)
		{
			VIM_DBG_MESSAGE("PPP Has an Instance !");
		}
	}
	else
	{
		DBG_MESSAGE("PPP Connected !");
	}
	return VIM_ERROR_NONE;
}

#endif


VIM_ERROR_PTR connect_pstn( VIM_NET_CONNECT_PARAMETERS *net_conn_params )
{  
	DBG_MESSAGE( "Connecting PSTN..." );

	vim_strcpy( net_conn_params->interface_settings.pstn_hdlc.dial.pabx, terminal.comms.pabx );
	VIM_ERROR_RETURN_ON_FAIL( vim_net_connect_request( &comms_net_handle, net_conn_params ));
	DBG_MESSAGE( "PSTN Connected !" );
	return VIM_ERROR_NONE;
}

eCOMMS_METHODS CommsType( VIM_UINT8 host, VIM_UINT8 Attempt )
{
	return ( host == HOST_WOW ) ? terminal.configuration.TXNCommsType[Attempt] : terminal.configuration.TMSCommsType[Attempt];
}


VIM_BOOL PCEftComm( VIM_UINT8 host, VIM_UINT8 Attempt, VIM_BOOL ShowMsg )
{
	eCOMMS_METHODS *eCMPtr = ( host == HOST_WOW ) ? &terminal.configuration.TXNCommsType[Attempt] : &terminal.configuration.TMSCommsType[Attempt];

#if 0
	if ((TERM_USE_4G) && ( WifiStatus(VIM_TRUE, TERM_USE_WIFI ) != VIM_ERROR_NONE) && ( IS_STANDALONE ))
	{
		pceft_debug(pceftpos_handle.instance, "Offbase standalone with Wifi down - use 4G");
		*eCMPtr = GPRS;
	}
#endif

	return ( *eCMPtr == VIA_PCEFTPOS ) ? VIM_TRUE : VIM_FALSE;
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_connect_start(VIM_UINT8 host, VIM_UINT8 Attempt )
{
  VIM_NET_CONNECT_PARAMETERS net_parameters;
  eCOMMS_METHODS Route;
  
  /*VIM_NET_CONNECT_PARAMETERS settings; */

  /* check if terminal is currently connected to another host */
  // RDD 090114 - added change so that DISC not issued before connect if not already connected
  if(( host_connected != host ) && ( host_connected != 99 ))
  {
    /* disconnect the existing connection */
   // VIM_ERROR_RETURN_ON_FAIL(comms_force_disconnect());
  }
  	
  // SET TIMEOUTS   	  
//  DBG_MESSAGE( "+++++++ USING AUS TIMEOUTS ++++++++++" );	

  pceft_debugf(pceftpos_handle.instance, "HOST:%d, Using:%d Comms Priority: %d %d %d", host, Attempt-1, terminal.configuration.TXNCommsType[0], terminal.configuration.TXNCommsType[1], terminal.configuration.TXNCommsType[2]);

  /* check if the connection to the host should be via pceftpos */
  if( PCEftComm( host, Attempt-1, VIM_TRUE ))
  //if (!comms_internal_mode())
  {
    pceft_debug_test( pceftpos_handle.instance, "Bank via PCEFT" );
	net_parameters.connection_timeout = CONNECTION_TIMEOUT; 	   
	net_parameters.receive_timeout = RECEIVE_TIMEOUT;
	net_parameters.header.type = VIM_NET_HEADER_TYPE_NONE;

    vim_mem_set(net_parameters.interface_settings.pceftpos.route.host_id,' ' ,VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.host_id));
    vim_mem_set(net_parameters.interface_settings.pceftpos.route.SHA,' ' ,VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.SHA));
    vim_mem_copy(net_parameters.interface_settings.pceftpos.route.host_id,terminal.host_id,8);
	
	if( host == HOST_XML )
	{
      /* set xml parameters */ 
	  vim_mem_set( net_parameters.interface_settings.pceftpos.route.TID, ' ', VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.TID ));
	  vim_mem_set( net_parameters.interface_settings.pceftpos.route.MID, ' ', VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.MID ));

      vim_strncpy( net_parameters.interface_settings.pceftpos.route.TID, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id));
      vim_strncpy( net_parameters.interface_settings.pceftpos.route.MID, terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id));

	  // RDD 221117 Need unique HOST ID
	  {	
		  VIM_CHAR LoyaltyHostID[8+1];
		  vim_strcpy( LoyaltyHostID, WOW_HOST_XML );
		  vim_mem_copy( net_parameters.interface_settings.pceftpos.route.host_id, LoyaltyHostID, 8 );
	  }
	  /* ITP */
      //net_parameters.interface_settings.pceftpos.route.ITP = PCEFT_BANK_CODE;
	  // RDD 310314 SPIRA:7312 - Encrypted messages with TID etc. need to go to PCEFTPOS as B3
      net_parameters.interface_settings.pceftpos.route.ITP = '2';
    }
    else
    {
      /* TID */
      vim_mem_clear(net_parameters.interface_settings.pceftpos.route.TID, VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.TID ));
      /* MID */
      vim_mem_clear(net_parameters.interface_settings.pceftpos.route.MID, VIM_SIZEOF(net_parameters.interface_settings.pceftpos.route.MID ));
      /* ITP */
	  // RDD 310314 SPIRA:7312 - Encrypted messages with TID etc. need to go to PCEFTPOS as B3
#if 0
      net_parameters.interface_settings.pceftpos.route.ITP = ' ';
#else
      net_parameters.interface_settings.pceftpos.route.ITP = ' ';
#endif
    }

    /* set the interface type to pceftpos */
    net_parameters.constructor=vim_pceftpos_host_net_connect_request;
    /* pass the pceftpos instance to use for the host connection */
    if( pceftpos_handle.instance != VIM_NULL )
      net_parameters.interface_settings.pceftpos.pceftpos_ptr=pceftpos_handle.instance;
    else
      VIM_ERROR_RETURN( VIM_ERROR_NULL );
      
    //net_parameters.header.type=VIM_NET_HEADER_TYPE_NONE;
    /* start connection */
    // //DBG_MESSAGE("Connect Requested");
    //VIM_DBG_VAR("pceftpos_handle.instance");
    VIM_ERROR_RETURN_ON_FAIL(vim_net_connect_request(&comms_net_handle,&net_parameters));
  }
  else
  {	  
	  DBG_MESSAGE( "HOST via GPRS or PSTN" );
	  // RDD 071014 TODO Get Priority List for Link Layer from Config file 
	  /* set link layer */ 
	  VIM_DBG_PRINTF1( "Attempt:%d", Attempt );
	  Route = CommsType(host, Attempt - 1);
	  switch( Route )
	  {

		case PSTN:
			pceft_debug_test(pceftpos_handle.instance, "Bank via PSTN");
			VIM_ERROR_RETURN_ON_FAIL( connect_pstn(&sTXN_SETTINGS[HOST_PSTN_SETTINGS] ));    
			break;

		case GPRS:
			pceft_debug_test(pceftpos_handle.instance, "Bank via GPRS");
#ifndef _WIN32
			VIM_ERROR_RETURN_ON_FAIL(connect_ppp(&sTXN_SETTINGS[HOST_PPP_SETTINGS]));
			VIM_ERROR_RETURN_ON_FAIL(connect_tcp(&sTXN_SETTINGS[HOST_TCP_SETTINGS], Route ));
			break;
#endif

		case WIFI:
		{
			VIM_ERROR_PTR res;
#define SSID_LEN_MIN 4
			pceft_debug_test(pceftpos_handle.instance, "Bank via Wifi");
			if ((res = connect_tcp(&sTXN_SETTINGS[HOST_WIFI_SETTINGS], Route)) != VIM_ERROR_NONE)
			{
				DBG_MESSAGE("Wifi Failed. Disable Wifi to facilitate 4G");
			}
		}
		break;

		case ETHERNET:
			pceft_debug_test(pceftpos_handle.instance, "Bank via BT-ETHERNET");
			VIM_ERROR_RETURN_ON_FAIL(connect_tcp(&sTXN_SETTINGS[HOST_TCP_SETTINGS], Route));
			break;

		default:
		case RNDIS:
			pceft_debug_test(pceftpos_handle.instance, "Bank via RNDIS");
			VIM_ERROR_RETURN_ON_FAIL(connect_tcp(&sTXN_SETTINGS[HOST_TCP_SETTINGS], Route));
			break;

	  }
  }  
  return VIM_ERROR_NONE;
}

VIM_UINT8 attempt;


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_connect_internal( VIM_UINT8 host )
{
  VIM_BOOL connected = VIM_FALSE;
  VIM_SIZE connect_attempts = NUMBER_OF_CONNECT_ATTEMPTS;
  VIM_ERROR_PTR error = VIM_ERROR_NONE;

  attempt = 1;

  DBG_MESSAGE("----------------- Check Internal Connection -------------------");

  if( comms_net_handle.instance_ptr )
    VIM_ERROR_IGNORE( vim_net_is_connected( comms_net_handle.instance_ptr, &connected ) );

  if( !connected )
  {
	do	
	{
		// RDD 200722 JIRA PIRPIN-1744 : If the last connect attempt failed and we're using GPRS then probably we have a network failure - so check and reconnect as required
		if ((error != VIM_ERROR_NONE) && (terminal.configuration.TXNCommsType[1] == GPRS) && IS_V400m)
		{
			VIM_CHAR ErrMsg[100];
			vim_mem_clear(ErrMsg, VIM_SIZEOF(ErrMsg));
			adk_com_init(VIM_FALSE, VIM_FALSE, ErrMsg);
			pceft_debugf(pceftpos_handle.instance, "GPRS Network Status: ErrMsg");
		}
		/* initiate a host connection */ 
		VIM_DBG_PRINTF2("CCCCCCCCCCCC Connect Attempt: %d of %d CCCCCCCCCCCCC", attempt, connect_attempts );
		//DisplayProcessing( DISP_CONNECTING_WAIT );
		if(( error = comms_connect_start(host, attempt )) == VIM_ERROR_NONE )
		{		  			  
			/* wait for the connection to complete */	
			if(( error = vim_net_connect_wait( comms_net_handle.instance_ptr )) == VIM_ERROR_NONE )			
			{
				DBG_MESSAGE( "<<<<<<<<< CONNECTED >>>>>>>>>" );
				VIM_DBG_VAR(comms_net_handle.instance_ptr);

				// RDD 200515 added for v542-9 in order to monitor Dial up time limit			
				vim_rtc_get_uptime( &terminal.CommsUptime );			
				VIM_DBG_PRINTF1( "ConnectStart: %d", terminal.CommsUptime );

				host_connected = host;	
				terminal.stats.connection_succeses++;
				break;						
			}
			else
			{       
				DBG_MESSAGE("<<<<<<<<< CONNECTION FAILED >>>>>>>>>");
				comms_disconnect();
				VIM_DBG_ERROR(error);
				terminal.stats.connection_failures++;
				pceft_debugf(pceftpos_handle.instance, "Bank Connect Priority %d Failed", attempt);
			}
		}
		else
		{      
			if( error == VIM_ERROR_GPRS_ERROR_PPP )
			{ 				  
				DBG_POINT;
				pceft_debug(pceftpos_handle.instance, "Network Error Occurred");
				VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "GPRS Network Error", "Check SIM and Config" ));
				vim_event_sleep(5000);
				error = VIM_ERROR_MODEM;	
			}			
			VIM_DBG_ERROR(error);
            pceft_debugf(pceftpos_handle.instance, "*Bank Connect Priority %d Failed", attempt);
			comms_force_disconnect();
		}
	}
	while( attempt++ < connect_attempts );
	
  }	
  return error;
}

/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR comms_connect(VIM_UINT8 host)
{  
	if (!comms_connected(host))  
	{			  
		return comms_connect_internal(host);	  
	}  
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_transmit_internal
(
  void const* source_ptr,
  VIM_SIZE source_size
)
{
#if 0 
  VIM_UINT8 buffer[1000];
  VIM_SIZE header_size;
  comms_params_t comms_parameters;
#endif
  VIM_ERROR_PTR error;
  
#if 0 // RDD 141014 - should use VIM NET setup to build the header as it already has all the info needed
  /* get the communcations paremetrs for the selected host */
  VIM_ERROR_RETURN_ON_FAIL(comms_get_host_params(host_connected, &comms_parameters));
  
  /* generate the message header */
  VIM_ERROR_RETURN_ON_FAIL(header_set(&comms_parameters,buffer,source_size,&header_size,VIM_SIZEOF(buffer)));

  /* ensure that the size of the header and the source message will fit inside the buffer */
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(header_size+source_size,VIM_SIZEOF(buffer));

  /* append the message after the header */
  vim_mem_copy(&buffer[header_size],source_ptr,source_size);
 
  VIM_DBG_LABEL_DATA("PINPAD==>HOST",source_ptr, source_size);

  /* send the message to the current network connection */

  error = vim_net_send(comms_net_handle.instance_ptr,buffer,header_size+source_size);
#else
 //if( txn.type != tt_logon )
  // RDD 210515 MB requested this to be across the board
	//DisplayProcessing( DISP_SENDING_WAIT );
  DBG_MESSAGE("SSSSSSSSSSSSSSS Sending SSSSSSSSSSSSSSSSS");
	error = vim_net_send(comms_net_handle.instance_ptr, source_ptr, source_size);
#endif
 
///////////////// TEST TEST TEST //////////////
#if 0
	if( !PCEftComm( HOST_WOW )) 
		vim_event_sleep(250);
#endif
///////////////// TEST TEST TEST //////////////

  if( error == VIM_ERROR_TIMEOUT )
       error = ERR_NO_RESPONSE;

  VIM_ERROR_RETURN_ON_FAIL( error );

  vim_rtc_get_uptime(&last_transmit);

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

// RDD 121012 re-written
// RDD 25121 JIRA PIRPIN-1318
VIM_ERROR_PTR comms_transmit( VIM_SIZE host, VIM_AS2805_MESSAGE *message_ptr )
{
	VIM_ERROR_PTR res;  
	void const* source_ptr = message_ptr->message;
	VIM_SIZE source_size = message_ptr->msg_size;

	//if( message_ptr->msg_type != MESSAGE_WOW_300_NMIC_151 )
	//		DisplayCommsState( message_ptr->msg_type, "Sending" );
	
	if(( res = comms_transmit_internal( source_ptr, source_size )) != VIM_ERROR_NONE )  
	{	
		VIM_DBG_ERROR(res);
		if (res == VIM_ERROR_POS_CANCEL)
		{
			terminal.abort_transaction = VIM_TRUE;
		}
		comms_force_disconnect( );

		// RDD 300922 JIRA PIRPIN-1917 Add a reconnect to facilitate broken connections when in permanent session mode		
		if (res == VIM_ERROR_TCP_CREATE_SOCKET)
		{
			pceft_debug(pceftpos_handle.instance, "remote was disconnected- retry");
			// RDD - this error is onl;y returned when no data has actually been sent, so safe to re-connect and send.
			if ((res = comms_connect(host)) == VIM_ERROR_NONE)
			{
				if ((res = comms_transmit_internal(source_ptr, source_size)) != VIM_ERROR_NONE)
				{
					comms_force_disconnect();
					VIM_ERROR_RETURN(res);
				}
			}
		}
		else
		{
			VIM_ERROR_RETURN(res);
		}
	} 

	// RDD 061021 - Add some Z debug if Not routed via EFT Client    
	if (PCEftComm(host, attempt, VIM_FALSE) == VIM_FALSE)        
	{            
		VIM_CHAR AscBuffer[PCEFT_MAX_HOST_RESP_LEN * 2];        
		vim_mem_clear(AscBuffer, VIM_SIZEOF(AscBuffer));        
		hex_to_asc(message_ptr->message, AscBuffer, 2 * message_ptr->msg_size);        
		pceft_debug(pceftpos_handle.instance, AscBuffer);        
	}
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_receive( VIM_AS2805_MESSAGE *message_ptr, VIM_SIZE destination_max_size )
{  
	VIM_ERROR_PTR error;  
	void *destination_ptr = message_ptr->message;
	//VIM_SIZE destination_size = message_ptr->msg_size;
	VIM_UINT8 buffer[10000];  // RDD 240515 - ensure that this temprary buffer is big enough to handle any message
	VIM_SIZE MESSAGE_size;  
	VIM_SIZE header_size; 
	comms_params_t comms_parameters;

	/* get the communcations paremetrs for the selected host */  
	VIM_ERROR_RETURN_ON_FAIL( comms_get_host_params(host_connected, &comms_parameters ));
  
	/* get a message  */  
	  
	// RDD 210515 Now they want to display comms state for logons too

	DisplayCommsState( message_ptr->msg_type, "Receiving" );

	//DisplayProcessing( DISP_RECEIVING_WAIT );
	DBG_MESSAGE("RRRRRRRRRRRRRRRRRRRRRRRR Receiving RRRRRRRRRRRRRRRRRRRRRRRRRRRR");
	if(( error = vim_net_receive( comms_net_handle.instance_ptr, buffer, &MESSAGE_size, VIM_SIZEOF(buffer))) == VIM_ERROR_TIMEOUT )
	{
		terminal.stats.response_timeouts++;		  	    
		error = ERR_NO_RESPONSE;  
	}
	else
	{
		terminal.stats.responses_received++;		  	    
	}
	DBG_VAR( MESSAGE_size );

  if( error != VIM_ERROR_NONE )
  {
	  // RDD 251014 - don't disconnect PPP if there was a remote disconnection at the TCP layer
	if( error == VIM_ERROR_DISCONNECTED )
	{
		DBG_MESSAGE( "Remote Forced a Disconnect !! " );
		terminal.abort_transaction = VIM_TRUE;
		return VIM_ERROR_TIMEOUT;
	}

  	if( error != VIM_ERROR_POS_CANCEL )
  		comms_force_disconnect();  
    else      
  		comms_disconnect( );  
  }
	// RDD 100413 P4 added for stats
	//terminal.stats.response_percentage_fail = ( 100*terminal.stats.response_timeouts )/terminal.stats.responses_received+terminal.stats.response_timeouts ;

	if( error == VIM_ERROR_POS_CANCEL )
	{
		terminal.abort_transaction = VIM_TRUE;
	}
    VIM_ERROR_RETURN_ON_FAIL(error);
  
	/* ensure that the header does not contain errors */  
	VIM_ERROR_RETURN_ON_FAIL( header_validate( &comms_parameters, buffer, MESSAGE_size,error ));
  
	/* get the size of the header */  
	VIM_ERROR_RETURN_ON_FAIL( header_get_size( &comms_parameters, &header_size ));
  
	/* ensure that the caller supplied a large enough buffer */  
	VIM_ERROR_RETURN_ON_SIZE_OVERFLOW( MESSAGE_size,destination_max_size+header_size );
  
	/* return message content to the caller */  
	vim_mem_copy( destination_ptr, &buffer[header_size], MESSAGE_size-header_size );
  
	/* return the message size to the caller */ 
	message_ptr->msg_size = MESSAGE_size-header_size;
	//*destination_size_ptr=MESSAGE_size-header_size;
  
	//VIM_DBG_LABEL_DATA("HOST==>PINPAD",destination_ptr, *destination_size_ptr);
    
	vim_rtc_get_uptime(&last_receive);
  
	return error;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_force_disconnect(void)
{
  VIM_NET_PTR instance_ptr;
  VIM_ERROR_PTR error1 = VIM_ERROR_NONE;

  DBG_MESSAGE("DDDDDDDDDDDDDDDDDDDD Forced disconnect DDDDDDDDDDDDDDDDDDDDDDD");
//#if 0	// RDD 271014 - never close the socket or network connection - we need to allow a quick reversal
  if( comms_net_handle.instance_ptr )
  {
    instance_ptr=comms_net_handle.instance_ptr;
    comms_net_handle.instance_ptr=VIM_NULL;
    host_connected = 99; /* clear host connected */
	DisplayProcessing( DISP_DISCONNECTING_WAIT );	
	error1 = vim_net_disconnect(instance_ptr);	
	VIM_DBG_POINT;
  }
  else
  {
	DBG_MESSAGE("Nothing to Disconnect");
  }

#if 0	// RDD 271014 - never close the socket or network connection - we need to allow a quick reversal
  if( gprs_ppp_handle.instance_ptr!=VIM_NULL )
  { // close GPRS ppp
    instance_ptr = gprs_ppp_handle.instance_ptr;
    gprs_ppp_handle.instance_ptr = VIM_NULL;
    VIM_ERROR_RETURN_ON_FAIL(vim_net_disconnect(instance_ptr));
  }
#endif

  VIM_ERROR_RETURN_ON_FAIL(error1);

  // RDD 200515 - looks like we disconnected sucessfully so reset the comms timer
  terminal.CommsUptime = 0;

  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR comms_disconnect(void)
{
  VIM_NET_PTR instance_ptr;

  if(CommsType(HOST_WOW, 0) == VIA_PCEFTPOS)
  {
      DBG_MESSAGE("Comms is via PcEFTPOS : Leave connection open");
      return VIM_ERROR_NONE;
  }

  if (comms_net_handle.instance_ptr!=VIM_NULL)
  {
	  DBG_MESSAGE("comms_disconnect");  
	  instance_ptr=comms_net_handle.instance_ptr;    
	  comms_net_handle.instance_ptr=VIM_NULL;
	  if( CommsType( HOST_WOW, 0 ) == PSTN )  
	  {		
		  // RDD 111215 Need to do this in the background
		  //DisplayProcessing( DISP_DISCONNECTING_WAIT );
		  // RDD 200515 - looks like we disconnected sucessfully so reset the comms timer	  	  
		  terminal.CommsUptime = 0;
	  }

      DBG_MESSAGE("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD Disconnet DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD");
      VIM_ERROR_RETURN_ON_FAIL(vim_net_disconnect(instance_ptr));
  }


  if( gprs_ppp_handle.instance_ptr != VIM_NULL )
  {
	  DBG_MESSAGE("leave GPRS connection open");
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Get turnaround time for last send/receive pair in hundreths of seconds. Will return 0 if there was no response to the last message sent */
VIM_ERROR_PTR comms_get_last_turnaround_time(VIM_UINT32 *turnaround)
{
  if (last_receive > last_transmit)
    *turnaround = (last_receive - last_transmit)/10;
  else
    *turnaround = 0;

  return VIM_ERROR_NONE;
}

#define START_COMMENT "<!--"
#define END_COMMENT   "-->"


#if 0
VIM_ERROR_PTR RemoveComments( VIM_CHAR *buffer )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR TagStr[50000];
	VIM_CHAR *StartPtr = TagStr, *EndPtr = VIM_NULL;
	VIM_SIZE TagDataLen = 0;
	VIM_BOOL CommentFound = VIM_FALSE;

	vim_mem_clear( TagStr, VIM_SIZEOF( TagStr ));

	// Make a copy of the whole container
	vim_strcpy( TagStr, buffer );

	while( res == VIM_ERROR_NONE )
	{
		if(( res = vim_strstr( StartPtr, START_COMMENT, &StartPtr )) == VIM_ERROR_NONE )
		{
			// found a comment
//			DBG_STRING( TagStr );
			CommentFound = VIM_TRUE;
			if(( res = vim_strstr( TagStr, END_COMMENT, &EndPtr )) == VIM_ERROR_NONE )
			{
				// Set endPtr to the very end of the comment
				EndPtr += vim_strlen( END_COMMENT );
				
				if(( TagDataLen = ( EndPtr - StartPtr )) > TAG_DATA_LEN_MAX )
				{
					DBG_MESSAGE( " Invalid Comment - too long ! ");
					return VIM_ERROR_MISMATCH;
				}					
				vim_strcpy( StartPtr, EndPtr );
			}
		}
	}

	if( CommentFound )
	{
		//DBG_STRING( TagStr );
		vim_strcpy( buffer, TagStr );
		//TagDataLen = vim_strlen( buffer );
		//VIM_ERROR_RETURN_ON_FAIL( vim_file_set( CfgFileName1, buffer, TagDataLen ));
	}
	return VIM_ERROR_NONE;
}
#else
VIM_ERROR_PTR RemoveComments(VIM_CHAR* buffer)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_SIZE FileSize = vim_strlen( buffer ) + 1;
	void* Ptr = VIM_NULL;
	VIM_CHAR* StartPtr = VIM_NULL, * EndPtr = VIM_NULL;
	VIM_CHAR* TagStr = VIM_NULL;
	VIM_SIZE TagDataLen = 0;
	VIM_BOOL CommentFound = VIM_FALSE;

	VIM_ERROR_RETURN_ON_FAIL(vim_malloc(&Ptr, FileSize));

	TagStr = (VIM_CHAR*)Ptr;

	vim_mem_clear(TagStr, FileSize );

	// Make a copy of the whole container
	vim_mem_copy(TagStr, buffer, FileSize-1 );
	StartPtr = TagStr;

	while (res == VIM_ERROR_NONE)
	{
		if ((res = vim_strstr(StartPtr, START_COMMENT, &StartPtr)) == VIM_ERROR_NONE)
		{
			// found a comment
//			DBG_STRING( TagStr );
			CommentFound = VIM_TRUE;
			if ((res = vim_strstr(TagStr, END_COMMENT, &EndPtr)) == VIM_ERROR_NONE)
			{
				// Set endPtr to the very end of the comment
				EndPtr += vim_strlen(END_COMMENT);

				if ((TagDataLen = (EndPtr - StartPtr)) > TAG_DATA_LEN_MAX)
				{
					DBG_MESSAGE(" Invalid Comment - too long ! ");
					vim_free(Ptr);
					return VIM_ERROR_MISMATCH;
				}
				vim_strcpy(StartPtr, EndPtr);
			}
		}
	}

	if (CommentFound)
	{
		//DBG_STRING( TagStr );
		vim_strcpy(buffer, TagStr);
		//TagDataLen = vim_strlen( buffer );
		//VIM_ERROR_RETURN_ON_FAIL( vim_file_set( CfgFileName1, buffer, TagDataLen ));
	}
	vim_free(Ptr);
	return VIM_ERROR_NONE;
}
#endif

VIM_ERROR_PTR xml_get_tag_end(VIM_CHAR *Container, const VIM_CHAR *Tag, VIM_CHAR *TagDataBuffer, VIM_CHAR **EndPtr)
{
	VIM_CHAR *StartPtr = Container;
	VIM_CHAR TagStr[500];
	VIM_SIZE TagDataLen = 0;

	vim_mem_clear( TagStr, VIM_SIZEOF(TagStr) );

	vim_strcpy( TagStr, Tag );

	if( vim_strstr( StartPtr, TagStr, &StartPtr ) == VIM_ERROR_NONE )
	{
		StartPtr += vim_strlen( TagStr );
		vim_strcpy( TagStr, TAG_END );
		vim_strcat( TagStr, Tag );
		if( vim_strstr( StartPtr, TagStr, EndPtr ) == VIM_ERROR_NONE )
		{
			if(( TagDataLen = ( *EndPtr - StartPtr )) > TAG_DATA_LEN_MAX )
				return VIM_ERROR_MISMATCH;
			*EndPtr += vim_strlen( TagStr );

			vim_strncpy( TagDataBuffer, StartPtr, TagDataLen );
			// RDD 120215 added facility to remove comments from XML strings
			RemoveComments( TagDataBuffer );
			return VIM_ERROR_NONE;
		}
	}	
	VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
}




VIM_ERROR_PTR xml_get_tag( VIM_CHAR *Container, const VIM_CHAR *Tag, VIM_CHAR *TagDataBuffer )
{
	VIM_CHAR *StartPtr = Container, *EndPtr = VIM_NULL;
	VIM_CHAR TagStr[10000];
	VIM_SIZE TagDataLen = 0;

	vim_mem_clear( TagStr, VIM_SIZEOF(TagStr) );

	vim_strcpy( TagStr, Tag );

	if( vim_strstr( StartPtr, TagStr, &StartPtr ) == VIM_ERROR_NONE )
	{
		StartPtr += vim_strlen( TagStr );
		vim_strcpy( TagStr, TAG_END );
		vim_strcat( TagStr, Tag );
		if( vim_strstr( StartPtr, TagStr, &EndPtr ) == VIM_ERROR_NONE )
		{
			if(( TagDataLen = ( EndPtr - StartPtr )) > VIM_SIZEOF(TagStr) )
				return VIM_ERROR_MISMATCH;

			vim_strncpy( TagDataBuffer, StartPtr, TagDataLen );
			// RDD 120215 added facility to remove comments from XML strings
            //RemoveComments(TagDataBuffer);
            return VIM_ERROR_NONE;
		}
	}	
	return VIM_ERROR_NOT_FOUND;
}



VIM_ERROR_PTR xml_find_next(VIM_CHAR **StartPtr, const VIM_CHAR *Tag, VIM_CHAR *Container)
{
	VIM_ERROR_RETURN_ON_FAIL( xml_get_tag( *StartPtr, Tag, Container )); 
	*StartPtr += vim_strlen( Container );
	return VIM_ERROR_NONE;
}


void parse_data(VIM_CHAR *Dest, VIM_CHAR *TagBuffer, eDEST_DATA_FORMAT DataFormat)
{
    VIM_DBG_PRINTF2( "Source Data: [%s] Format [%d]", TagBuffer, (VIM_SIZE)DataFormat );

    switch (DataFormat)
    {
    case _CHAR:			// convert to VIM_CHAR
    {
        VIM_CHAR *DestPtr = (VIM_CHAR *)Dest;
        *DestPtr = *TagBuffer;
        VIM_DBG_NUMBER(*DestPtr);
    }
    break;

    case NUMBER_8_BIT:			// convert to UINT8
    {
        VIM_UINT8 *DestPtr = (VIM_UINT8 *)Dest;
        *DestPtr = (VIM_UINT8)asc_to_bin(TagBuffer, vim_strlen(TagBuffer));
        VIM_DBG_NUMBER(*DestPtr);
    }
    break;

    case NUMBER_16_BIT:			// convert to UINT16
    {
        VIM_UINT16 *DestPtr = (VIM_UINT16 *)Dest;
        VIM_DBG_STRING(TagBuffer);
        VIM_DBG_NUMBER(vim_strlen(TagBuffer));
        *DestPtr = (VIM_UINT16)asc_to_bin(TagBuffer, vim_strlen(TagBuffer));
        VIM_DBG_NUMBER(*DestPtr);
    }
    break;

    case NUMBER_32_BIT:		// Convert to UINT32
    {
        VIM_UINT32 *DestPtr = (VIM_UINT32 *)Dest;
        *DestPtr = (VIM_UINT32)asc_to_bin(TagBuffer, vim_strlen(TagBuffer));
        VIM_DBG_NUMBER(*DestPtr);
    }
    break;

    case NUMBER_64_BIT:		// Convert to UINT64
    {
        VIM_UINT64 *DestPtr = (VIM_UINT64 *)Dest;
        *DestPtr = asc_to_bin(TagBuffer, vim_strlen(TagBuffer));
        VIM_DBG_NUMBER(*DestPtr);
    }
    break;

    case IP_ADDRESS:		// convert to array of 4 x UINT8 
    {
        VIM_UINT8 *DestPtr = (VIM_UINT8 *)Dest;
        VIM_CHAR *StartPtr = TagBuffer;// , *EndPtr;
        VIM_SIZE n;

        for (n = 0; n < IP_ADDR_ELEMENTS; n++)
        {
            VIM_CHAR AdrElement[3 + 1];
            VIM_CHAR *EndPtr = StartPtr;
            vim_mem_clear(AdrElement, VIM_SIZEOF(AdrElement));
			DBG_POINT;

            while (VIM_ISDIGIT(*EndPtr))
            {
                EndPtr += 1;
            }
			DBG_POINT;
			vim_mem_copy(AdrElement, StartPtr, (VIM_SIZE)(EndPtr - StartPtr));
			DBG_POINT;
            *DestPtr++ = asc_to_bin(AdrElement, VIM_MIN(vim_strlen(AdrElement), VIM_SIZEOF(AdrElement) - 1));
			DBG_POINT;
            StartPtr = ++EndPtr;
        }
    }
	DBG_POINT;
    break;

    case FLAG:				// Y or N or "true" -> VIM_TRUE or VIM_FALSE
    {
        //VIM_CHAR *Ptr;
        VIM_BOOL *DestPtr = (VIM_BOOL *)Dest;
        *DestPtr = ((*TagBuffer == 'Y') || (*TagBuffer == '1')) ? VIM_TRUE : VIM_FALSE;
        VIM_DBG_VAR(*DestPtr);
    }
    break;

    default:
    case _TEXT:				// just copy over
        vim_strcpy(Dest, TagBuffer);
        break;
    }
}


VIM_ERROR_PTR xml_parse_tag(VIM_CHAR *Src, void *Dest, void *Tag, eDEST_DATA_FORMAT DataFormat)
{
	VIM_CHAR TagBuffer[1000];
	//VIM_SIZE n;

	vim_mem_clear( TagBuffer, VIM_SIZEOF(TagBuffer) );

	if( xml_get_tag( Src, Tag, TagBuffer  ) == VIM_ERROR_NONE )
	{
        parse_data(Dest, TagBuffer, DataFormat);		
	}
	else
		return VIM_ERROR_NOT_FOUND;
	
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR comms_load_common_settings( VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetPtr )
{
	VIM_SIZE n;			
	VIM_NET_CONNECT_PARAMETERS *NetParamsPtr = NetPtr;

	for( n=0; n<NUMBER_OF_SETTINGS; n++, NetParamsPtr++ )
	{
#if 0
		// Load the Connect and Receive Timeouts 
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->connection_timeout, CONNECT_TIMEOUT_TAG, NUMBER_32_BIT );
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->receive_timeout, RECEIVE_TIMEOUT_TAG, NUMBER_32_BIT );
		// RDD 281014 Make the XML file overrule anything else on power up
		//if( terminal.tms_parameters.max_block_size == 0 )
			xml_parse_tag( TagBuffer, (void *)&terminal.tms_parameters.max_block_size, _MAX_FIELD_SIZE, NUMBER_32_BIT );		
			VIM_DBG_NUMBER( terminal.tms_parameters.max_block_size );
		//if( terminal.tms_parameters.window_size == 0 )
			xml_parse_tag( TagBuffer, (void *)&terminal.tms_parameters.window_size, _WINDOW_SIZE, NUMBER_32_BIT );		
			VIM_DBG_NUMBER( terminal.tms_parameters.window_size );
#endif
	
		// Hard Code the Fixed Header Parameters 
		NetParamsPtr->header.parameters.tpdu.source_address[0] = 0x00;	
		NetParamsPtr->header.parameters.tpdu.source_address[1] = 0x00;	
		NetParamsPtr->header.parameters.tpdu.destination_address[0] = terminal.acquirers[txn.acquirer_index].nii[1];	
		NetParamsPtr->header.parameters.tpdu.destination_address[1] = terminal.acquirers[txn.acquirer_index].nii[0];
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR comms_load_ppp_settings( VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetParamsPtr )
{
	VIM_SIZE n;			
				
	if( 1 )	
	{			
		// Point to the correct member for GPRS PPP		
		for( n=0; n<HOST_PPP_SETTINGS; n++, NetParamsPtr++ );		
		NetParamsPtr->constructor = vim_gprs_ppp_connect;
		//NetParamsPtr->header.type=VIM_NET_HEADER_TYPE_PCEFT_TPDU;	

		// RDD 201114 set up individual timeouts for each connection type
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->connection_timeout, CONNECT_TIMEOUT_TAG, NUMBER_32_BIT );
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->receive_timeout, RECEIVE_TIMEOUT_TAG, NUMBER_32_BIT);

		xml_parse_tag( TagBuffer, (void *)NetParamsPtr->interface_settings.gprs_ppp.apn, APN_TAG, _TEXT );		
		xml_parse_tag( TagBuffer, (void *)NetParamsPtr->interface_settings.gprs_ppp.user_id, USER_ID_TAG, _TEXT );		
		xml_parse_tag( TagBuffer, (void *)NetParamsPtr->interface_settings.gprs_ppp.password, PASSWORD_TAG, _TEXT );		
		xml_parse_tag( TagBuffer, (void *)NetParamsPtr->interface_settings.gprs_ppp.sim_pin, SIM_PIN_TAG, _TEXT );					
	}
	return VIM_ERROR_NONE;
}

extern VIM_PCEFTPOS_PARAMETERS pceftpos_settings;
extern const char *pceft_settings_filename;

// RS232 connection parameters 
// Other PCEFT Params

// Don't get these from TermCFG.xml
VIM_ERROR_PTR comms_load_pceft_settings( VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetParamsPtr )
{
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR comms_load_wifi_settings(VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetParamsPtr)
{
    VIM_SIZE n;
    //VIM_CHAR *ptr;

    // Point to the correct member for GPRS TCP	and ETHERNET
    if (1)
    {
        for (n = 0; n < HOST_WIFI_SETTINGS; n++, NetParamsPtr++);

        // RDD 201114 set up individual timeouts for each connection type
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.ethernet_ip.SSID, SSID_TAG, _TEXT );
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.ethernet_ip.PSK, PSK_TAG, _TEXT);
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.ethernet_ip.KeyMgmt, KEY_MGMT_TAG, _TEXT);

        NetParamsPtr->constructor = vim_gprs_tcp_connect_request;
    }
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR comms_load_tcp_settings( VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetParamsPtr, VIM_BOOL wifi, VIM_BOOL Standalone )
{
	VIM_SIZE n;	
    VIM_CHAR *ptr;
    CommsParams tcp_settings = wifi ? HOST_WIFI_SETTINGS : HOST_TCP_SETTINGS;

	VIM_DBG_MESSAGE("!!!!!!!!!!!!! Load TCP Settings !!!!!!!!!!!!!!!!!!!");
	VIM_DBG_VAR(wifi);

	// Point to the correct member for GPRS TCP	and ETHERNET
	if( 1 )
	{
		for( n=0; n< tcp_settings; n++, NetParamsPtr++ );

		// RDD 201114 set up individual timeouts for each connection type
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->connection_timeout, CONNECT_TIMEOUT_TAG, NUMBER_32_BIT );
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->receive_timeout, RECEIVE_TIMEOUT_TAG, NUMBER_32_BIT );

        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.ConnTimeout, CONNECT_TIMEOUT_TAG, NUMBER_32_BIT);
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.ReceiveTimeout, RECEIVE_TIMEOUT_TAG, NUMBER_32_BIT);

		NetParamsPtr->constructor = vim_gprs_tcp_connect_request;

		// Header Type
		xml_parse_tag( TagBuffer, (VIM_NET_HEADER_TYPE *)&NetParamsPtr->header.type, HEADER_TYPE, NUMBER_8_BIT );								
		//NetParamsPtr->header.type=VIM_NET_HEADER_TYPE_PCEFT_TPDU;	

		NetParamsPtr->header.parameters.tpdu.message_type=VIM_NET_HEADER_TPDU_DEFAULT_MESSAGE_TYPE;	
		// RDD 061021 added

		DBG_VAR(Standalone)

		if (vim_strlen(TERM_HOST_URL) > MIN_URL_LEN )
		{
			DBG_POINT;
			vim_strcpy(NetParamsPtr->interface_settings.gprs_tcp.dest_url, TERM_HOST_URL);
			if( !Standalone )
				pceft_debugf(pceftpos_handle.instance, "Set Host URL from TMS: %s", TERM_HOST_URL);
			else
				DBG_POINT;
		}
		else
		{
			DBG_POINT;
			xml_parse_tag(TagBuffer, (void*)NetParamsPtr->interface_settings.gprs_tcp.dest_url, DEST_URL1_TAG, _TEXT);
			if( !Standalone )
				pceft_debugf(pceftpos_handle.instance, "Set Host URL from XML: %s", NetParamsPtr->interface_settings.gprs_tcp.dest_url);
			else
				DBG_POINT;
		}

		// RDD 091214 - write the order of these according to the WOW rules ie. Odd terminals 1234 Even terminals 2143
					// RDD 030123 odd
		xml_parse_tag(TagBuffer, (void*)NetParamsPtr->interface_settings.gprs_tcp.ip_address[0], IP_ADDR1_TAG, IP_ADDRESS);
		//xml_parse_tag(TagBuffer, (void*)NetParamsPtr->interface_settings.gprs_tcp.ip_address[1], IP_ADDR2_TAG, IP_ADDRESS);
		DBG_POINT;


		if( terminal.terminal_id[7]%2 )
		{
			DBG_POINT;
			if( vim_strlen(TERM_SNI_ODD) > MIN_SNI_LEN )
			{
				DBG_POINT;
				vim_strcpy(NetParamsPtr->interface_settings.gprs_tcp.SNI, TERM_SNI_ODD);
				if (!Standalone)
					pceft_debugf(pceftpos_handle.instance, "Set Odd SNI to TMS Value: %s", NetParamsPtr->interface_settings.gprs_tcp.SNI);
				else
					DBG_POINT;
			}
			else
			{
				DBG_POINT;

				xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.SNI, SNI1_TAG, _TEXT);
				if (!Standalone)
					pceft_debugf(pceftpos_handle.instance, "Set Odd SNI to default value: %s", NetParamsPtr->interface_settings.gprs_tcp.SNI);
				else
					DBG_POINT;
			}
		}
		else
		{

			DBG_POINT;
			if ( vim_strlen( TERM_SNI_EVEN) > MIN_SNI_LEN )
			{
				DBG_POINT;
				vim_strcpy(NetParamsPtr->interface_settings.gprs_tcp.SNI, TERM_SNI_EVEN);
				if (!Standalone)
					pceft_debugf(pceftpos_handle.instance, "Set Even SNI to TMS Value: %s", NetParamsPtr->interface_settings.gprs_tcp.SNI);
				else
					DBG_POINT;
			}
			else
			{
				DBG_POINT;
				// RDD 030123 JIRA PIRPIN-2113 Get SNI from TMS going forwards	
				xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.SNI, SNI2_TAG, _TEXT);
				if (!Standalone)
					pceft_debugf(pceftpos_handle.instance, "Set Even SNI to default value: %s", NetParamsPtr->interface_settings.gprs_tcp.SNI);
				else
					DBG_POINT;
			}
		}
		DBG_POINT;
		xml_parse_tag( TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.tcp_socket, TCP_PORT_TAG, NUMBER_16_BIT );	
        xml_parse_tag(TagBuffer, (void *)&NetParamsPtr->interface_settings.gprs_tcp.ssl_cert_filename, CLIENT_CERT_TAG, _TEXT);
		DBG_POINT;

		NetParamsPtr->interface_settings.gprs_tcp.Route = WIFI;
		DBG_POINT;

        // RDD todo Get this from termcfg.xml but use presence of cert filename for now to indicate secure connection
        if(( vim_strlen(NetParamsPtr->interface_settings.gprs_tcp.ssl_cert_filename)||(vim_strstr(NetParamsPtr->interface_settings.gprs_tcp.dest_url, "https", &ptr )== VIM_ERROR_NONE )))
        {
            NetParamsPtr->interface_settings.gprs_tcp.isSSL = VIM_TRUE;
            // RDD 190121 special setting to use ADK Com
            NetParamsPtr->interface_settings.gprs_tcp.UseUDP = VIM_TRUE;
        }
        else
        {
            NetParamsPtr->interface_settings.gprs_tcp.isSSL = VIM_FALSE;
            // RDD 190121 special seeting to use ADK Com
            NetParamsPtr->interface_settings.gprs_tcp.UseUDP = VIM_FALSE;
        }
	}
	return VIM_ERROR_NONE;
}


// RDD 071014 added for ALH
VIM_ERROR_PTR SetPriority( VIM_CHAR *TagBuffer, eCOMMS_METHODS *CommsTypes )
{
//	VIM_CHAR ParamsBuffer[1000];
	VIM_CHAR MethodsList[MAX_COMMS_METHODS*2];
	VIM_CHAR *MethodPtr = MethodsList;
	VIM_UINT8 n;
	eCOMMS_METHODS *CommsTypePtr = CommsTypes;

	VIM_ERROR_RETURN_ON_FAIL( xml_parse_tag( TagBuffer, MethodsList, PRIORITY_TAG, _TEXT ));
	
	// RDD - if there are actually any types in the tag then parse them into the config below 
	for( n=0; n<MAX_COMMS_METHODS; n++, CommsTypePtr++, MethodPtr++ )
	{
		*CommsTypePtr = VIA_PCEFTPOS;	// By default	

		// continue if no contents
		if( !*MethodPtr ) continue;

		// The designation letter in the XML config file gets mapped to an eCOMMS_METHODS 
		switch( *MethodPtr )
		{
			case 'G': *CommsTypePtr = GPRS; break;
			case 'P': *CommsTypePtr = PSTN; break;
			case 'E': *CommsTypePtr = ETHERNET; break;
			case 'W': *CommsTypePtr = WIFI; break;
			case 'R': *CommsTypePtr = RNDIS; break;
			case 'D':
			default: *CommsTypePtr = VIA_PCEFTPOS; break;
		}
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR comms_load_settings( VIM_CHAR *TagBuffer, VIM_NET_CONNECT_PARAMETERS *NetPtr, VIM_BOOL Standalone )
{
	VIM_CHAR ParamsBuffer[4000];
	VIM_NET_CONNECT_PARAMETERS *NetParamsPtr = NetPtr;
	
	VIM_ERROR_RETURN_ON_FAIL( comms_load_common_settings( TagBuffer, NetPtr ));

	if( xml_get_tag( (VIM_CHAR *)TagBuffer, PCEFT_PARAMS_TAG, ParamsBuffer ) == VIM_ERROR_NONE )
	{
		VIM_ERROR_RETURN_ON_FAIL( comms_load_pceft_settings( ParamsBuffer, NetParamsPtr ));
	}

	if( xml_get_tag( (VIM_CHAR *)TagBuffer, GPRS_PARAMS_TAG, ParamsBuffer ) == VIM_ERROR_NONE )
	{		
		// Load the GPRS PPP Settings if present		
		if( xml_get_tag( (VIM_CHAR *)TagBuffer, PPP_PARAMS_TAG, ParamsBuffer ) == VIM_ERROR_NONE )
		{
			VIM_ERROR_RETURN_ON_FAIL( comms_load_ppp_settings( ParamsBuffer, NetParamsPtr ));
		}
	}

    // Load the TCP Settings if present
    if (xml_get_tag((VIM_CHAR *)TagBuffer, WIFI_PARAMS_TAG, ParamsBuffer) == VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN_ON_FAIL(comms_load_wifi_settings(ParamsBuffer, NetParamsPtr));
        // TODO add PSTN and ETHERNET Parameters	
    }

    // Load the TCP Settings if present
    if (xml_get_tag((VIM_CHAR *)TagBuffer, WIFI_TCP_PARAMS_TAG, ParamsBuffer) == VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN_ON_FAIL(comms_load_tcp_settings( ParamsBuffer, NetParamsPtr, VIM_TRUE, Standalone ));
        // TODO add PSTN and ETHERNET Parameters	
    }

	// Load the TCP Settings if present
	if( xml_get_tag( (VIM_CHAR *)TagBuffer, TCP_PARAMS_TAG, ParamsBuffer ) == VIM_ERROR_NONE )		
	{					
		VIM_ERROR_RETURN_ON_FAIL( comms_load_tcp_settings( ParamsBuffer, NetParamsPtr, VIM_FALSE, Standalone ));			
		// TODO add PSTN and ETHERNET Parameters	
	}
	
	// Load the PSTN Settings if present
	if( xml_get_tag( (VIM_CHAR *)TagBuffer, PSTN_PARAMS_TAG, ParamsBuffer ) == VIM_ERROR_NONE )	
	{			
		//VIM_ERROR_RETURN_ON_FAIL( comms_load_pstn_settings( ParamsBuffer, NetParamsPtr ));		
	}
	
	// TODO add ETHERNET Parameters if different to GPRS/TCP ( should be same really )

	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR comms_config_load( VIM_BOOL Standalone )
{
    VIM_UINT8 CfgFileBuffer[CFG_FILE_MAX_SIZE];
    VIM_SIZE CfgFileSize = 0;
    VIM_BOOL exists = VIM_FALSE;
    VIM_CHAR TagBuffer[5000];
    VIM_CHAR *TagPtr = TagBuffer;

    vim_mem_clear(CfgFileBuffer, CFG_FILE_MAX_SIZE);

	if (IS_P400)
	{
#ifdef _DEBUG
		if (1)
#else
		if (TERM_TEST_SIGNED)
#endif
		{
			vim_strcpy(CfgFileName, CfgP400TestFileName);
		}
		else
		{
			vim_strcpy(CfgFileName, CfgP400FileName);
		}
	}
	else // assume v400m
	{
#ifdef _DEBUG
		if(1)
#else
		if (TERM_TEST_SIGNED)
#endif
		{
			vim_strcpy(CfgFileName, CfgV400TestFileName);
		}
		else
		{
			vim_strcpy(CfgFileName, CfgV400FileName);
		}
	}
	pceft_debugf_test(pceftpos_handle.instance, "Comms Config Using %s", CfgFileName);

    VIM_ERROR_IGNORE(vim_file_exists(CfgFileName, &exists));

    if (!exists)
    {
        VIM_KBD_CODE key_code;
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Missing Termcfg.xml", "Call Helpdesk");
        VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read(&key_code, 10000));

        DBG_MESSAGE("!!!! Comms Config file not found !!!!");
        terminal.configuration.TXNCommsType[0] = VIA_PCEFTPOS;
        terminal.configuration.TMSCommsType[0] = VIA_PCEFTPOS;
        return VIM_ERROR_NOT_FOUND;
    }
	else
	{
		pceft_debugf_test(pceftpos_handle.instance, "Loading CFG file: %s", CfgFileName);
	}

    // Read the file into a buffer 
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get(CfgFileName, CfgFileBuffer, &CfgFileSize, CFG_FILE_MAX_SIZE));

    DBG_MESSAGE("OK Config File Opened... ");

    // SPIRA:8673 Introduce Max Comms Connect time for dial up
    terminal.configuration.MaxCommsUptime = INFINITE_TIMEOUT;
    xml_parse_tag((VIM_CHAR *)CfgFileBuffer, (void *)&terminal.configuration.MaxCommsUptime, MAX_CONNECT_TIME, NUMBER_32_BIT);

    // RDD - make trickle feed time 1sec by default - this will allow 1 SAF to get up to the switch
    terminal.configuration.TrickleFeedTime = WOW_TRICKLE_FEED_TIME_SLOT;
    xml_parse_tag((VIM_CHAR *)CfgFileBuffer, (void *)&terminal.configuration.TrickleFeedTime, TRICKLE_FEED_TIME, NUMBER_32_BIT);

    // RDD 260318 Added for MCR support - Get flag from TERMCFG.XML
    //terminal.configuration.use_mer_routing = VIM_FALSE;
    //xml_parse_tag((VIM_CHAR *)CfgFileBuffer, (void *)&terminal.configuration.use_mer_routing, MERCHANT_ROUTING, FLAG);

	// RDD 030123 TMS Settings no longer picked up from xml file
#if 0
	// Detect the correct section and Load the TMS settings first
	if( xml_get_tag( (VIM_CHAR *)CfgFileBuffer, TMS_COMMS_TAG, TagPtr ) == VIM_ERROR_NONE )
	{
        // RDD 240320 - used for standalone only. for integrated just use pc_cfg file

		VIM_ERROR_RETURN_ON_FAIL( SetPriority( TagPtr, terminal.configuration.TMSCommsType ));
		VIM_ERROR_RETURN_ON_FAIL( comms_load_settings( TagPtr, sTMS_SETTINGS ));

		// RDD 281014 Make the XML file overrule anything else on power up
		//if( terminal.tms_parameters.max_block_size == 0 )
		xml_parse_tag( TagPtr, (void *)&terminal.tms_parameters.max_block_size, _MAX_FIELD_SIZE, NUMBER_32_BIT );				
		VIM_DBG_NUMBER( terminal.tms_parameters.max_block_size );
		//if( terminal.tms_parameters.window_size == 0 )		
		xml_parse_tag( TagPtr, (void *)&terminal.tms_parameters.window_size, _WINDOW_SIZE, NUMBER_32_BIT );					
		VIM_DBG_NUMBER( terminal.tms_parameters.window_size );	
		terminal.tms_parameters.retries = 2;
		xml_parse_tag( TagPtr, (void *)&terminal.tms_parameters.retries, _RETRIES, NUMBER_8_BIT );					
		VIM_DBG_NUMBER( terminal.tms_parameters.retries );	

		DBG_MESSAGE( "OK TMS Comms Settings Loaded " );
	}
#endif
	
#if 1
	if( xml_get_tag( (VIM_CHAR *)CfgFileBuffer, TXN_COMMS_TAG, TagPtr ) == VIM_ERROR_NONE )
	{
		VIM_ERROR_RETURN_ON_FAIL( SetPriority( TagPtr, terminal.configuration.TXNCommsType ));
		VIM_ERROR_RETURN_ON_FAIL( comms_load_settings( TagPtr, sTXN_SETTINGS, Standalone ));
	}  	
	DBG_MESSAGE( "OK TXN Comms Settings Loaded " );
#endif

	return VIM_ERROR_NONE;
}


// Callback from vim_gprs.c
VIM_ERROR_PTR vim_drv_nwif_gprs_connect_request(VIM_NET_INTERFACE_HANDLE_PTR handle_ptr, VIM_NET_INTERFACE_CONNECT_PARAMETERS_PTR parameters_ptr)
{
#ifndef _WIN32
	VIM_ERROR_RETURN( adk_gprs_connect_request( handle_ptr,  parameters_ptr));
#else
	return VIM_ERROR_NONE;
#endif
}

VIM_ERROR_PTR SetCommsDefaultSettingsV400m(VIM_BOOL WiFiAvailable )
{
	VIM_ERROR_PTR res = VIM_ERROR_NOT_FOUND;

#ifdef _WIN32
	terminal.configuration.use_wifi = 2;
#endif

	pceft_debug(pceftpos_handle.instance, "Set Default comms V400m");

	if ( WiFiAvailable )
	{
		pceft_debug(pceftpos_handle.instance, "Set Default WiFi");
		terminal.configuration.TXNCommsType[0] = WIFI;
		terminal.configuration.TXNCommsType[1] = WIFI;
		terminal.configuration.TXNCommsType[2] = WIFI;
		if (BT_Available())
		{
			terminal.configuration.TXNCommsType[1] = ETHERNET;
		}
		if (TERM_USE_4G)
		{
			terminal.configuration.TXNCommsType[2] = GPRS;
		}
		res = VIM_ERROR_NONE;
	}
	else if ((TERM_USE_BT)&&(BT_Available()))
	{
		pceft_debug(pceftpos_handle.instance, "Set Default BT");
		terminal.configuration.TXNCommsType[0] = ETHERNET;
		terminal.configuration.TXNCommsType[1] = ETHERNET;
		terminal.configuration.TXNCommsType[2] = ETHERNET;
		if (TERM_USE_4G)
		{
			terminal.configuration.TXNCommsType[1] = GPRS;
		}
		res = VIM_ERROR_NONE;
	}
	else if( TERM_USE_4G )
	{
		pceft_debug(pceftpos_handle.instance, "Set Default 4G");
		terminal.configuration.TXNCommsType[0] = GPRS;
		terminal.configuration.TXNCommsType[1] = GPRS;
		terminal.configuration.TXNCommsType[2] = GPRS;
		if (BT_Available())
		{
			terminal.configuration.TXNCommsType[1] = ETHERNET;
		}

		res = VIM_ERROR_NONE;
	}
	return res;
}

VIM_ERROR_PTR SetCommsDefaultSettingsP400(VIM_BOOL WiFiAvailable)
{
	VIM_ERROR_PTR res = VIM_ERROR_NOT_FOUND;

	pceft_debug(pceftpos_handle.instance, "Set Default comms P400");

#ifdef _WIN32
	terminal.configuration.TXNCommsType[0] = WIFI;
	terminal.configuration.TXNCommsType[1] = WIFI;
	terminal.configuration.TXNCommsType[2] = WIFI;
	res = VIM_ERROR_NONE;
#else
	if((WiFiAvailable)&&( IS_R10_POS == VIM_FALSE ))
	{
		pceft_debug(pceftpos_handle.instance, "Set Default WiFi");
		terminal.configuration.TXNCommsType[0] = WIFI;
		terminal.configuration.TXNCommsType[1] = WIFI;
		terminal.configuration.TXNCommsType[2] = WIFI;
		res = VIM_ERROR_NONE;
	}
	else 
	{
		pceft_debug(pceftpos_handle.instance, "Set Default RNDIS");
		terminal.configuration.TXNCommsType[0] = RNDIS;
		terminal.configuration.TXNCommsType[1] = RNDIS;
		terminal.configuration.TXNCommsType[2] = RNDIS;
		res = VIM_ERROR_NONE;
	}
#endif
	return res;
}

extern VIM_TEXT const WifiPWDCfg[];

VIM_BOOL WifiRequired(void)
{
	if (TERM_USE_WIFI)
		return VIM_TRUE;
	// If there is an encrypted Wifi password in the system then assume wifi is required
	//if (vim_file_is_present(WifiPWDCfg))
	//	return VIM_TRUE;
	return VIM_FALSE;
}





extern VIM_ERROR_PTR WifiSetup(void);


VIM_ERROR_PTR CommsSetup(VIM_BOOL Verbose)
{
	VIM_ERROR_PTR res;
	VIM_CHAR ErrMsg[100];
	VIM_CHAR buffer[100];
	VIM_BOOL WifiAvailable = VIM_FALSE;
	//extern VIM_PCEFTPOS_PARAMETERS pceftpos_settings;

	TERM_USE_BT = VIM_FALSE;
	TERM_USE_4G = VIM_FALSE;

	// RDD 030123 Need to call this to parse correct SNI Tag (if used)
	comms_config_load(IS_STANDALONE);

	pceft_debug(pceftpos_handle.instance, "Checking Comms Routes post TMS update");
	
	// RDD add Fuel to R10 compatible list so we can route comms via PcEFTPOS
	if( IS_R10_POS || FUEL_PINPAD || (ALH_PINPAD && IS_P400))
	{
		pceft_debug(pceftpos_handle.instance, "R10 POS P400");
		VIM_DBG_VAR(TERM_USE_RNDIS);
		if(( TERM_USE_RNDIS )||(IS_STANDALONE))
		{
			VIM_DBG_MESSAGE("PPPPPPPPPPPPP P400 via RNDIS PPPPPPPPPPPPPP");
#ifdef _WIN32
			terminal.configuration.TXNCommsType[0] = WIFI;
			terminal.configuration.TXNCommsType[1] = WIFI;
			terminal.configuration.TXNCommsType[2] = WIFI;
#else
			pceft_debug_test(pceftpos_handle.instance, "TMS Says use RNDIS as default route to HOST");
			terminal.configuration.TXNCommsType[0] = RNDIS;
			terminal.configuration.TXNCommsType[1] = RNDIS;
			terminal.configuration.TXNCommsType[2] = RNDIS;
#endif
			// RDD 280422 - Remove this comment to add Wifi capability to Internal P400 banners
			//if (!TERM_USE_WIFI)
			pceft_debug(pceftpos_handle.instance, "TMS Says use RNDIS Proxy as default route to Connex");
		}
		else
		{
			pceft_debug_test(pceftpos_handle.instance, "TMS Says use EFT Server as default route to HOST");
			//terminal.configuration.allow_disconnect = VIM_TRUE;

			VIM_DBG_MESSAGE("PPPPPPPPPPPPP P400 via VIA_PCEFTPOS PPPPPPPPPPPPPP");
			terminal.configuration.TXNCommsType[0] = VIA_PCEFTPOS;
			terminal.configuration.TXNCommsType[1] = VIA_PCEFTPOS;
			terminal.configuration.TXNCommsType[2] = VIA_PCEFTPOS;
		}
		// Use LFD for all R10 TMS systems
		// RDD 080822 Kill the current connection
		comms_disconnect();
		if(FUEL_PINPAD)
		{
			TMSUseRTM();
		}
		else
		{
			TMSUseLFD();
		}
		return VIM_ERROR_NONE;
	}
	else
	{
		// Use RTM for everything else 
		DBG_POINT;
		TMSUseRTM();
	}

	if (Verbose)
	{
		vim_sprintf((VIM_CHAR*)buffer, "v%d.%d\n%s", BUILD_NUMBER, REVISION, "Configuring Network...\nPlease Wait");
		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR*)buffer, "");
	}
	VIM_DBG_MESSAGE("Running Com Init ..... ");

	vim_mem_clear(ErrMsg, VIM_SIZEOF(ErrMsg));
	//TERM_OPTION_PCEFT_DISCONNECT_ENABLED = VIM_TRUE;

	if (WifiRequired())
	{
		VIM_DBG_MESSAGE("Init Wifi");
		WifiInit();

		if (WifiStatus(VIM_TRUE, 1) == VIM_ERROR_NOT_FOUND)
		{
			if ((TERM_USE_WIFI_PRIMARY)||(TERM_USE_WIFI_BACKUP))
			{
				vim_sprintf((VIM_CHAR*)buffer, "v%d.%d\n%s", BUILD_NUMBER, REVISION, "TMS->WiFi Primary\nBut WiFi not Available");
				if (Verbose)
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, buffer, ""));
					vim_event_sleep(2000);
					WifiSetup();
				}
			}
			else
			{
				vim_sprintf((VIM_CHAR*)buffer, "v%d.%d\n%s", BUILD_NUMBER, REVISION, "TMS->WiFi Not required\nWiFi not Available");
			}

			pceft_debug(pceftpos_handle.instance, buffer);
			if (Verbose)
				vim_event_sleep(2000);

			// No Wifi so need to correct.
			TERM_USE_WIFI = VIM_FALSE;
		}

		else
		{
			WifiAvailable = VIM_TRUE;
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "WiFi Available", "" );
		}
	}
	vim_event_sleep(1000);


	if (IS_V400m)
	{
		pceft_debug_test(pceftpos_handle.instance, "V400m");
		if ((res = adk_com_init(VIM_TRUE, VIM_FALSE, ErrMsg)) != VIM_ERROR_NONE)
		{
			if ((Verbose)&&( IS_STANDALONE ))
			{
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "4G Not Enabled", ErrMsg);
				pceft_debugf_test(pceftpos_handle.instance, "4G Not Enabled: %s", ErrMsg);
			}
		}
		else // 4G Is available - but do we want to use it ?
		{
			TERM_USE_4G = VIM_TRUE;
			if (Verbose)
			{
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "4G Available", ErrMsg);
			}
			pceft_debugf(pceftpos_handle.instance, "4G Available: %s", ErrMsg);
		}
		if (Verbose)
			vim_event_sleep(1000);

		if (BT_Available())
		{
			TERM_USE_BT = VIM_TRUE;
			pceft_debug(pceftpos_handle.instance, "BT Ethernet Available");
		}
		//vim_event_sleep(1000);

		if( SetCommsDefaultSettingsV400m(WifiAvailable) != VIM_ERROR_NONE)
		{
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "No Comms Available", "Call Helpdesk");
			vim_event_sleep(30000);
		}

		// Decision Time...
		if (TERM_USE_WIFI_PRIMARY)
		{
			if (TERM_USE_BT)
			{
				terminal.configuration.TXNCommsType[1] = ETHERNET;
			}
			else if (TERM_USE_4G)
			{
				terminal.configuration.TXNCommsType[1] = GPRS;
			}

			if (TERM_USE_4G)
			{
				terminal.configuration.TXNCommsType[2] = GPRS;
			}
		}
		else if (TERM_USE_WIFI_BACKUP)
		{
			if (TERM_USE_BT)
			{
				terminal.configuration.TXNCommsType[0] = ETHERNET;
			}
			else if (TERM_USE_4G)
			{
				terminal.configuration.TXNCommsType[0] = GPRS;
			}
			if (TERM_USE_4G)
			{
				terminal.configuration.TXNCommsType[2] = GPRS;
			}
		}
		else if (TERM_USE_BT)
		{
			if (TERM_USE_4G)
			{
				terminal.configuration.TXNCommsType[1] = GPRS;
			}
		}
	}
	else 
	{
		// set default for P400
		SetCommsDefaultSettingsP400(WifiAvailable);
		
		if( TERM_USE_WIFI_PRIMARY )
		{
			terminal.configuration.TXNCommsType[0] = WIFI;	// Wifi as primary TMS needs to be > 1
			terminal.configuration.TXNCommsType[1] = RNDIS;	// Wifi as primary TMS needs to be > 1
			terminal.configuration.TXNCommsType[2] = RNDIS;	// Wifi as primary TMS needs to be > 1
		}
		else if( TERM_USE_WIFI_BACKUP )
		{
			terminal.configuration.TXNCommsType[0] = RNDIS;	// Wifi as primary TMS needs to be > 1
			terminal.configuration.TXNCommsType[1] = WIFI;	// Wifi as primary TMS needs to be > 1
			terminal.configuration.TXNCommsType[2] = WIFI;	// Wifi as primary TMS needs to be > 1
		}
	}
	vim_sprintf(buffer, "Comms Priorities: %d %d %d", terminal.configuration.TXNCommsType[0], terminal.configuration.TXNCommsType[1], terminal.configuration.TXNCommsType[2]);
	pceft_debug( pceftpos_handle.instance, buffer );
	return VIM_ERROR_NONE;
}