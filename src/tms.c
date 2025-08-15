#include <incs.h>
#include <tms.h>
#ifndef _WIN32
#include <cJSON.h>
#endif
#include <stdio.h>
#include <string.h>


VIM_DBG_SET_FILE("TV");


#define _WEBLFD

static VIM_CHAR WebLFDClientVersion[10 + 1] = { 0,0,0,0,0,0,0,0,0,0 };
static VIM_CHAR WebLFDServerVersion[10 + 1] = { 0,0,0,0,0,0,0,0,0,0 };

const char *JsonReqFile = VIM_FILE_PATH_LOCAL "WebLFDReq.json";
const char *JsonRespFile = VIM_FILE_PATH_LOCAL "WebLFDResp.json";

extern VIM_ERROR_PTR CreateCardTable(void);


extern VIM_SIZE NumParams(void);
extern const char* EDPData;
VIM_UINT8 attempt;


/***************************************************************************
 * Verifone LFD for Stepup host
***************************************************************************/

// RDD 070313 SPIRA 6638 : make this global so can be checked and reported to POS

// RDD 300418 JIRA WWBP-26 Now need to save the Branding Filename - get rid of the old global and replace with term.configuration.BrandFileName
//VIM_CHAR BrandFileName[20];
extern file_version_record_t file_status[];
extern const char *CfgFileName;
extern VIM_ERROR_PTR ParseRegOptions(const VIM_CHAR *data_ptr, VIM_SIZE num_params);

extern TMS_CONFIG_OPTION sTMSConfigOptions[];

extern void ResetTMS(void);

extern VIM_BOOL PCEftComm( VIM_UINT8 host, VIM_UINT8 attempt, VIM_BOOL showmsg );

extern VIM_ERROR_PTR ValidateFileName( VIM_CHAR **FoundFileName, VIM_CHAR *RootFileName, VIM_SIZE *FileVersion );
extern VIM_TMS_DOWNLOAD MapDownloadType( VIM_UINT8 file_id );
extern VIM_ERROR_PTR ValidateConfigFile( VIM_UINT8 file_id, VIM_CHAR *FileName, VIM_SIZE *FileVersion );
extern VIM_BOOL Wally_BuildJSONRequestOfRegistrytoFIle(void);
extern VIM_NET_CONNECT_PARAMETERS sTMS_SETTINGS[];

extern VIM_BOOL ValidateConfigData( VIM_CHAR *terminal_id, VIM_CHAR *merchant_id );


#define MERCHANT_ID		"@MID"					// The Merchant ID (CAIC) for this Terminal
#define PRE_AUTH_FLAG	"@Pre_Auth_Enabled"		// Enable Pre-auth functionality ( default : Preauth is enabled )
#define CCV_ENTRY		"@CCV_Entry"			// Enable CCV Entry (default : CCV is disabled )
#define RTM_BLOCK_LEN	"@MaxFieldSize"			// MaxFieldsize specified to RTM or LFD during Logon ( Max 999 as is lllvar )
#define WINDOW_SIZE		"@WindowSize"			// WindowSize specified to TRM or LFD during logon ( MaX 99 )
#define PRN_COLS		"@Prn_Columns"			// To facilitate 40 column printing ( default is 24 column printing )
#define SHORT_RECEIPT    "@SHORT_RECEIPT"         // They want on or off
#define PCEFT_DISC		"@Pceft_Disc"			// Disconnect from PCEFT Host after each txn ( not currently implemented )
#define FAST_EMV_AMOUNT "@FastEMV_Amount"		// Set a FasterEMV amount for Tag 9F02 Override on CTLS ( default is 0 ie OFF )
#define ENABLE_J8		"@Enable_J8"			// Enable/Disable J8 - currently not implemented in code
#define DEF_POLL_RESP	"@DefPollResp"			// Not currently used
#define MANPAN_PASSWORD "@ManPan_Password"		// Manual PAN entry Password override ( default "9999" )
#define CTLS_CASH		"@CTLSCASH"				// Allow CASH out on CTLS ( default is "0" so element needs to be present and set to 1 to enable )
#define COLLECTOR_ID	"@Collector_ID"			// Google Collector ID for Android Loyalty data ( default : "38012512" )
#define MERCHANT_URL	"@Merchant_URL"			// Merchant URL for Apple Loyalty data ( default : "https://www.woolworthsrewards.com.au" )
#define PASS_IDENTIFIER	"@Pass_Identifier"		// Pass Identifier for Apple Loyalty data ( "pass.com.woolworthslimited.edrcard" )
#define USE_GOOGLE_SDK	"@Use_Google_SDK"		// Use the Google supplied library rather than raw APDUs to handle Loyalty on Android devices ( default : "1" )
#define ANDROID_VAS_PRIVATE_KEY_VERSION "@Android_Key_Version"  // Android Private Key Version ( Default = "1" )
#define PCI_TOKENS_DISABLED "@PCI_TOKENS"       // 130220 DEVX Disable PCI Token and ePAN return for Third Party Terminals
#define ENABLE_BAL_RECEIPT  "@BALANCE_RECEIPT"  //  010420 v582-7 RDD Activate balance receipt option (no disabled by default)
#define CVM_OVERRIDE_LIMIT  "@CUSTOM_CVM_LIMIT"    // 020420 v582-7 RDD Added CVM Override for certain issuers if flags enabled in CPAT
#define NEW_LFD             "@NEW_LFD"           // 110520 Trello #137 Change to New LFD
#define PCI_PRE_REBOOT      "@PRE_PCI_TIME"      // 110520 Trello #142 Configure Pre-PCI reboot check
#define NEW_VAS             "@NEW_VAS"           // 280520 Trello #123 LFD Switch for VAS
#define MC_EMV_REFUND       "@MC_EMV_REFUND"    // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
#define VISA_EMV_REFUND     "@VISA_EMV_REFUND"    // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
#define EMV_REFUND_USE_CPAT "@EMV_REFUND_CPAT_PIN"
#define FORBID_XML_OPT      "@FORBID_XML_OPT"      // 280920 Trello #281 - MB wants to be able to disable extra tags if needs be
#define PRINT_COLS          "@PRINT_COLUMNS"       // 300920 Trello #239
#define FUNCTION            "@FUNCTION"
#define MAX_LFD_PACKETS     "@MAX_LFD_PACKETS"
#define QC_LOGON_ON_BOOT    "@QC_LOGON_ON_BOOT" //Configuration to attempt QC logon on startup
#define QC_RESP_TIMEOUT     "@QC_RESP_TIMEOUT"  //Timeout to wait for response from the QC application
#define QC_USERNAME         "@QC_USERNAME"      //Temporary - username used in QC authorisation. Will be replaced with provisioning project changes
#define QC_PASSWORD         "@QC_PASSWORD"      //Temporary - password used in QC authorisation. Will be replaced with provisioning project changes


#define CPAT                "@CPAT"
#define EPAT                "@EPAT"
#define TMS_AUTO            "@TMS_AUTO"
#define EGC_BIN             "@EGC_BIN"
#define HOST_ID             "@HOST_ID"
#define _VDA                 "@VDA"
#define _MDA                 "@MDA"

#define QR_WALLET               "@QR_WALLET"
#define QR_PS_TIMEOUT           "@QR_PS_TIMEOUT"
#define QR_QR_TIMEOUT           "@QR_QR_TIMEOUT"
#define QR_IDLE_SCAN_TIMEOUT    "@QR_IDLE_SCAN_TIMEOUT"
#define QR_WALLY_TIMEOUT        "@QR_WALLY_TIMEOUT"
#define QR_STORE_LOC            "@QR_STORE_LOC"
#define QR_REWARDS_POLL         "@QR_REWARDS_POLL"
#define QR_RESULTS_POLL         "@QR_RESULTS_POLL"
#define QR_POLLBUSTER           "@QR_POLLBUSTER"
#define QR_TENDER_ID            "@QR_TENDER_ID"
#define QR_TENDER_NAME          "@QR_TENDER_NAME"
#define QR_DECLINED_RCT_TEXT    "@QR_DECLINED_RCT_TEXT"
#define QR_MERCH_REF_PREFIX     "@QR_MERCH_REF_PREFIX"
#define QR_SEND_QR_TO_POS       "@QR_SEND_QR_TO_POS"
#define QR_ENABLE_WIDE_RCT      "@QR_ENABLE_WIDE_RCT"
#define QR_DISABLE_FROM_IMAGE   "@QR_DISABLE_FROM_IMAGE"

// RDD 190315 SPIRA:8453 add support for @!xCPATnnnnnn etc.
#define CONFIG_RTM_DOWNLOAD		"@!1"			// Enable Config File download via LFD but N3 from switch may overwrite ( see further explaination below )
#define CONFIG_RTM_OVERRIDE		"@!2"			// Enable Config file download via LFD and subsequently ignore switch N3 response ( see further explaination below )

#define DATA_FS			'='
#define WEB_LFD_DATA_FS ':'
#define LFD_CFG_DATA_MAX 100


// RDD 191214 add a tag to allow the terminal to distinguish between Integrated and Standalone modes
#define _VER_TAG					"VER>"
#define _MODE_TAG					"MODE>"

#define _CTLS_FIELD_CTRL			"CtlsFieldCtrl>"
#define _TIP_PROMPT					"Tip_Prompt>"
#define _TIP_MAX_PERCENT			"Tip_Max_Percent>"
#define _REFUND_PASSWORD			"Refund_Password>"
#define _CASHOUT_MAX				"Cashout_Max>"
#define _PRE_AUTH_ENABLED			"Pre_Auth_Enabled>"
#define _CCV_ENTRY					"CCV_Entry>"
#define _ENABLE_PRN_BITMAP			"Prn_Bitmap>"
#define _PRN_COLS					"Prn_Columns>"
#define _PA_BATCH_SIZE				"PA_BATCH_SIZE>"
#define _PA_BATCH_SIZE				"PA_BATCH_SIZE>"
#define _DE55						"DE55>"

/** wrap TMS_PARAMETERS */

// RDD v430 change to merge with WBC VIM
#if 0
typedef struct
{
  VIM_SIZE max_field_size;
  VIM_SIZE window_size;
  VIM_CHAR serial_number[15];
  /*@dependent@*/VIM_TMS_PARAMETERS const * tms_parameters_ptr;
}VIM_TMS_PARAMETER_WRAP;
#endif

extern VIM_ERROR_PTR tms_lfd_download_one_file
(
  VIM_NET_PTR net_ptr,
  VIM_TMS_PARAMETER_WRAP const * vim_tms_ptr,
  VIM_TEXT* file_name_ptr,
  VIM_TEXT* destination_file_name_ptr,
  VIM_FILE_HANDLE* file_handle_ptr
);

const VIM_TEXT COMMAND_FILE[] = VIM_FILE_PATH_LOCAL "REMOTECMD.DAT";

#if 0

typedef struct lfd_req_data_t
{
    VIM_CHAR szTermId[8 + 1];
    VIM_CHAR szFileBuffer[5000];
} lfd_req_data_t;

static lfd_req_data_t l_sLFDReqData;

static cJSON *pCJ_term_info;

static char szCommandsBuffer[5000];

#endif

/*----------------------------------------------------------------------------*/
static VIM_TMS_GENERAL_PROMPT_ITEM const tms_prompt_list[] =
{
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_DEFAULT, "RTM Logon\nConnecting"},
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_DOWNLOADING, "RTM Download"},
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_UPLOADING, "RTM Uploading"},
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_LOADING, "RTM Loading"},
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_PROCESSING, "RTM Processing"},
  {VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_INITIALISE, "RTM Initialising"}
};
/*----------------------------------------------------------------------------*/
void tms_save_ctls_version_to_file( void *version_number_ptr, VIM_SIZE version_size )
{
  vim_file_set(VIM_FILE_PATH_LOCAL"vvt_fmver",version_number_ptr,version_size);
}
VIM_ERROR_PTR tms_load_ctls_version_from_file( void *version_number_ptr, VIM_SIZE version_max_size, VIM_SIZE *version_size)
{
  VIM_ERROR_RETURN_ON_FAIL( vim_file_get(VIM_FILE_PATH_LOCAL"vvt_fmver",version_number_ptr,version_size,version_max_size));
  return VIM_ERROR_NONE;
}


void TMSUseLFD(void)
{
	// RDD 270422 - 'W' always uses LFD not RTM
	VIM_DBG_MESSAGE("LLLLLLLLLLLLLLLLLLLLLL USE LFD LLLLLLLLLLLLLLLLLLLLLLLLLLLL");

    vim_file_delete(WEB_TMS_PROXY_CONFIG_FILE);    
    vim_file_copy(WEB_LFD_PROXY_CONFIG_PATH, WEB_TMS_PROXY_CONFIG_FILE);

#ifdef _DEBUG
	{
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Set TMS via", " LFD ");
		vim_event_sleep(1000);
		pceft_pos_display_close();
	}
#endif
	TMS_USE_RTM = VIM_FALSE;
	pceft_debug(pceftpos_handle.instance, "--------------- TMS via LFD -----------------");
}


void TMSUseRTM(void)
{

    if (TERM_OPTION_VHQ_ONLY)
    {
        VIM_DBG_MESSAGE("RRRRRRRRRRRRRRRRRR USING VHQ RRRRRRRRRRRRRRRRRRR");
        return;
    }
    else
    {
        VIM_DBG_MESSAGE("RRRRRRRRRRRRRRRRRR USING RTM RRRRRRRRRRRRRRRRRRR");
    }
	vim_file_delete(WEB_TMS_WIFI_CONFIG_FILE);
	
    if (TERM_TEST_SIGNED)
    {
        vim_file_copy(T_WEB_RTM_WIFI_CONFIG_PATH, WEB_TMS_WIFI_CONFIG_FILE);
    }
    else
    {
        vim_file_copy(WEB_RTM_WIFI_CONFIG_PATH, WEB_TMS_WIFI_CONFIG_FILE);
    }
	vim_file_delete(WEB_TMS_PROXY_CONFIG_FILE);

    if (TERM_TEST_SIGNED)
    {
        vim_file_copy(T_WEB_RTM_PROXY_CONFIG_PATH, WEB_TMS_PROXY_CONFIG_FILE);
    }
    else
    {
        vim_file_copy(WEB_RTM_PROXY_CONFIG_PATH, WEB_TMS_PROXY_CONFIG_FILE);
    }
	if (XXX_MODE)
	{
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Set TMS via", " RTM ");
		//vim_event_sleep(2000);
		//pceft_pos_display_close();
	}
    pceft_debug(pceftpos_handle.instance, "--------------- TMS via RTM -----------------");
	TMS_USE_RTM = VIM_TRUE;
}



static VIM_ERROR_PTR tms_ready( void )
{
    //VIM_BOOL exists = VIM_FALSE;
	// RDD 100512 Added at Mitchells Request
    // Check We have Keys Loaded

#if 0
    
    vim_file_exists(CR_REQ_TOKEN, &exists);

    if(( !exists ) && ( terminal.terminal_id[0] == 'L' ))
    {
        terminal_clear_status(ss_tid_configured);
    }

#endif

	if( terminal.logon_state == initial_keys_required )
		return ERR_NO_KEYS_LOADED;

	// Check Terminal Is configured
	if( !terminal_get_status( ss_tid_configured ))
		return ERR_CONFIGURATION_REQUIRED;


	// RDD 060412 SPIRA 5188: Do not allow LFD if SAF or REVERSAL pending
	VIM_ERROR_RETURN_ON_FAIL( terminal_check_config_change_allowed( VIM_TRUE ));
	return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR tms_get_params( VIM_TMS_PARAMETERS *tms_parameters )
{

	VIM_CHAR tmp_buffer[20];
	vim_mem_clear( tmp_buffer, VIM_SIZEOF(tmp_buffer) );

	/* set terminal parameters */
	tms_parameters->retries = terminal.tms_parameters.retries;
	vim_mem_copy( tms_parameters->merchant_id, terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id));
	vim_mem_copy( tms_parameters->terminal_id, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id));
	tms_parameters->max_block_size = VIM_MAX( terminal.tms_parameters.max_block_size, 1 );
	tms_parameters->window_size = VIM_MAX( terminal.tms_parameters.window_size, 500 );
	tms_parameters->nii = WOW_TMS_HOST_NII;

	VIM_DBG_NUMBER( terminal.tms_parameters.max_block_size );
	VIM_DBG_NUMBER( terminal.tms_parameters.window_size );

	/* build the connection with the pceftpos */
	//VIM_DBG_MESSAGE( "connect to host via pceftpos " );
	if( PCEftComm( HOST_LFD, attempt, VIM_FALSE ))
	{
		DBG_MESSAGE( "RTM Use PcEftPos" );
		/* timeout */
		tms_parameters->net_parameters.connection_timeout=5000;
		tms_parameters->net_parameters.receive_timeout=45000;
		vim_mem_set(tms_parameters->net_parameters.interface_settings.pceftpos.route.host_id,' ' ,VIM_SIZEOF(tms_parameters->net_parameters.interface_settings.pceftpos.route.host_id));
		vim_mem_set(tms_parameters->net_parameters.interface_settings.pceftpos.route.SHA,' ' ,VIM_SIZEOF(tms_parameters->net_parameters.interface_settings.pceftpos.route.SHA));
		vim_mem_copy(tms_parameters->net_parameters.interface_settings.pceftpos.route.host_id, WOW_TMS_HOST_ID,8);
		tms_parameters->net_parameters.interface_settings.pceftpos.route.ITP = ' ';

		/* terminal id */
		vim_mem_set(tms_parameters->net_parameters.interface_settings.pceftpos.route.TID, ' ', VIM_SIZEOF(tms_parameters->net_parameters.interface_settings.pceftpos.route.TID));

		/* merchant id */
		vim_mem_set(tms_parameters->net_parameters.interface_settings.pceftpos.route.MID, ' ', VIM_SIZEOF(tms_parameters->net_parameters.interface_settings.pceftpos.route.MID));

		/* set the interface type to pceftpos */
		tms_parameters->net_parameters.constructor=vim_pceftpos_host_net_connect_request;

		if( pceftpos_handle.instance != VIM_NULL )
			tms_parameters->net_parameters.interface_settings.pceftpos.pceftpos_ptr=pceftpos_handle.instance;

		// RDD 211114 - header type to come from XML file
		//tms_parameters->net_parameters.header.type=VIM_NET_HEADER_TYPE_NONE;
		vim_mem_copy( tms_parameters->FileName, terminal.config_filename, VIM_SIZEOF(tms_parameters->FileName) );

		/* start connection */
		VIM_DBG_MESSAGE("Connect Requested");
	}
	else
	{
		VIM_CHAR Buffer[100];
		VIM_DBG_MESSAGE( "RTM via GPRS or PSTN" );

	  // RDD 071014 TODO Get Priority List for Link Layer from Config file
	  /* set link layer */
	  switch( CommsType( HOST_LFD, 0 ))
	  {
		default:
		case ETHERNET:
		case GPRS:
		// RDD 250914 - Point to the pre-set settings
		DBG_MESSAGE( "RTM Use TCPIP" );
		tms_parameters->net_parameters = sTMS_SETTINGS[HOST_TCP_SETTINGS];
		vim_sprintf( Buffer, "RTM TCPIP Port: %d", tms_parameters->net_parameters.interface_settings.gprs_tcp.tcp_socket );
		DBG_MESSAGE( Buffer );
		break;

		case PSTN:
		VIM_DBG_MESSAGE( "RTM via PSTN" );
		tms_parameters->net_parameters = sTMS_SETTINGS[HOST_PSTN_SETTINGS];
		// RDD 171215 added
		vim_strcpy( tms_parameters->net_parameters.interface_settings.pstn_hdlc.dial.pabx, terminal.comms.pabx );

		break;
	  }
	}
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR terminal_set_default_config( void )
{
	//VIM_UINT8 CfgFileBuffer[ CFG_FILE_MAX_SIZE ];
	//VIM_SIZE CfgFileSize = 0;
	//VIM_BOOL exists = VIM_FALSE;

	terminal_set_status( ss_no_saf_prints );
	terminal_save();
	return VIM_ERROR_NONE;
}

//////////////////
 // 110520 Trello #137 Change to New LFD
 //////////////////

#ifdef _WEBLFD

#if 0
// RDD 150513 Define a function to retreive the current filename of auxiliary package files for UI, AD(vertising) or BR(anding)
static VIM_ERROR_PTR tms_get_app_local_version(VIM_TEXT *RelToDldPtr, VIM_SIZE version_max_size)
{
    VIM_CHAR Version[20];
    VIM_CHAR Major;
    VIM_CHAR *Ptr = Version;

    vim_mem_clear(Version, VIM_SIZEOF(Version));
    vim_sprintf( Version, "%d", BUILD_NUMBER );  // eg. 82000723    
    Ptr += 5;   // Skip 82000
    Major = *Ptr;   // Normally 7    
    Ptr += 1;
    // Get into format used by version data in CFG file downoad line    
    vim_sprintf( RelToDldPtr, "%c.%s.%d", Major, Ptr, REVISION );
    VIM_DBG_PRINTF1( "PA Version: %s", RelToDldPtr );    
    return VIM_ERROR_NONE;
}

#endif

static VIM_ERROR_PTR tms_build_terminal_params( VIM_TMS_PARAMETERS *tms_parameters, VIM_CHAR *de61_buffer, VIM_SIZE *MessageLen )
{
    VIM_TEXT temp_buffer[2500];
    VIM_SIZE pos = 0;
    VIM_SIZE data_size;
    VIM_SIZE Len = *MessageLen;

    VIM_DBG_MESSAGE("-------------------- Build Terminal Params ---------------------");

    /* get terminal id */
    vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
    vim_mem_copy(temp_buffer, terminal.terminal_id, VIM_SIZEOF( terminal.terminal_id ));
    VIM_ERROR_RETURN_ON_FAIL( vim_snprintf_unterminated( de61_buffer, Len, "TID=%s\n", temp_buffer));
    pos = vim_strlen(de61_buffer);

    VIM_DBG_STRING(de61_buffer);

    /* get merchant id */
    vim_mem_clear( temp_buffer, VIM_SIZEOF( temp_buffer ));
    vim_mem_copy( temp_buffer, terminal.merchant_id, VIM_SIZEOF( terminal.merchant_id ));
    VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, Len, "MID=%s\n", temp_buffer));
    pos = vim_strlen(de61_buffer);

    VIM_DBG_STRING(de61_buffer);

    /* HW Type */
    {
        VIM_CHAR model_info[40] = { 0x00 };
        vim_terminal_get_model_info(model_info, VIM_SIZEOF(model_info));
        VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen, "HWType=%s\n", model_info));
    }
    pos = vim_strlen(de61_buffer);
    VIM_DBG_STRING(de61_buffer);

    /* Serial Number */
    {
        VIM_CHAR serial_number[30];
        vim_mem_clear( serial_number, VIM_SIZEOF( serial_number ));

        VIM_ERROR_RETURN_ON_FAIL(vim_terminal_get_serial_number( serial_number, VIM_SIZEOF(serial_number) - 1));
        VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen, "HWSN=%s\n", serial_number));
    }

	/* RDD 270422 JIRA PIRPIN-1593 report HW:SIM */
	VIM_DBG_MESSAGE("Build SIM Info");

	if( IS_V400m )
	{
		VIM_GPRS_INFO info;
		VIM_GPRS_INFO *InfoPtr = &info;

		VIM_CHAR serial_number[30];
		vim_mem_clear(serial_number, VIM_SIZEOF(serial_number));

		if( vim_gprs_get_info( &info ) == VIM_ERROR_NONE )
		{
			vim_strcpy( serial_number, InfoPtr->iccid );
		}
		else
		{
			vim_strcpy( serial_number, "Not Applicable" );
		}
		VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen, "HW:SIM=%s\n", serial_number));
	}

	else
	{
		VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen, "HW:SIM=No SIM Found\n"));
	}
	pos = vim_strlen(de61_buffer);
	VIM_DBG_STRING(de61_buffer);

    /* get application version number */	
		VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen, "ActiveSwRelease=%8d.%2d\n", BUILD_NUMBER, REVISION ));
		pos = vim_strlen(de61_buffer);

    data_size = VIM_SIZEOF(temp_buffer);
    VIM_ERROR_RETURN_ON_FAIL(vim_tms_callback_get_APP_data(0x00, (VIM_UINT8 *)temp_buffer, &data_size));
    VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(de61_buffer + pos, *MessageLen,
        "%s", temp_buffer));

    *MessageLen = vim_strlen(de61_buffer);

	VIM_DBG_STRING(de61_buffer);

    return VIM_ERROR_NONE;
}



static void JsonOpenCloseFile(VIM_CHAR **DestPtr, VIM_BOOL Open )
{
    VIM_CHAR Buff[20];
    VIM_SIZE Len;

    vim_mem_clear(Buff, VIM_SIZEOF(Buff));
    if( Open )
        vim_sprintf( Buff, "{\r\n" );
    else
        vim_sprintf( Buff, "}\r\n" );

    Len = vim_strlen(Buff);
    vim_strncat(*DestPtr, Buff, Len);
    VIM_DBG_STRING(*DestPtr);
    *DestPtr += Len;
}


static void JsonOpenCloseContainer(VIM_CHAR **DestPtr, VIM_CHAR *Key, VIM_BOOL Open, VIM_BOOL LastContainer )
{
    VIM_CHAR Buff[50];
    VIM_SIZE Len;

    vim_mem_clear(Buff, VIM_SIZEOF(Buff));
    if (Open)
        vim_sprintf(Buff, "  \"%8.8s\":\r\n  {\r\n", Key);
    else if( LastContainer )
        vim_sprintf(Buff, "  }\r\n");
    else
        vim_sprintf(Buff, "  },\r\n");

    Len = vim_strlen(Buff);
    vim_strncat(*DestPtr, Buff, Len);
//    VIM_DBG_STRING(*DestPtr);
    *DestPtr += Len;
}

// RDD - EOS had an extra CR which stuffs things up...
#define MIN_REG_ENTRY_LEN 5

VIM_ERROR_PTR ParseLine( VIM_CHAR **SrcPtr, VIM_CHAR **DestPtr )
{
    VIM_ERROR_PTR res;
    VIM_CHAR *EndPtr;
    VIM_CHAR RegEntry[50], Item[50], Data[50], NewEntry[100];
    VIM_SIZE NewEntryLen;

    vim_mem_clear(RegEntry, VIM_SIZEOF(RegEntry));
    vim_mem_clear(Item, VIM_SIZEOF(Item));
    vim_mem_clear(Data, VIM_SIZEOF(Data));
    vim_mem_clear(NewEntry, VIM_SIZEOF(NewEntry));

    if(( res = vim_strchr( *SrcPtr, 0x0a, &EndPtr )) != VIM_ERROR_NONE )
    {
        VIM_DBG_STRING(*SrcPtr);
    }
    else
    {
        *EndPtr = '\0';
        vim_strncpy(RegEntry, *SrcPtr, VIM_SIZEOF(RegEntry) - 1);
    }
    //VIM_DBG_STRING(RegEntry);

    VIM_ERROR_RETURN_ON_FAIL(vim_strchr(RegEntry, '=', &EndPtr));
    *EndPtr = '\0';
    vim_strncpy( Item, RegEntry, VIM_SIZEOF(Item)-1 );
    *SrcPtr += ( vim_strlen(Item) +1 );

    // Get rid of unwanted quotes as these cause json parse failures in client
    if (vim_strchr(Data, 0x22, &EndPtr) == VIM_ERROR_NONE)
    {
        *EndPtr = '\0';
    }

    vim_strncpy( Data, *SrcPtr, VIM_SIZEOF(Data)-1 );
    *SrcPtr += vim_strlen(Data) + 1;

    // Check to see if theres another entry, otherwise no comma...
    if(vim_strchr(*SrcPtr, 0x0a, &EndPtr) != VIM_ERROR_NONE )
    {
        vim_sprintf(NewEntry, "    \"%s\":\"%s\"\r\n", Item, Data);
    }
    else
    {
        vim_sprintf(NewEntry, "    \"%s\":\"%s\",\r\n", Item, Data);
    }
    NewEntryLen = vim_strlen(NewEntry);

    vim_strncat(*DestPtr, NewEntry, NewEntryLen);
    *DestPtr += NewEntryLen;
    return VIM_ERROR_NONE;
}


#define CMDS_ACTIVATE_BODY  "    \"ContactServer\":\"NOW\",\r\n    \"InstallFiles\":\"NOW\"\r\n"

static VIM_BOOL done;

// Set a default timeout which can be extended if eg. install is in progress
static VIM_UINT32 ulTimeout = 30000;

static VIM_ERROR_PTR parseResp( VIM_CHAR *JsonBuffer, VIM_BOOL *Completed, VIM_CHAR *ERC, VIM_BOOL SilentMode )
{
    cJSON *pResp, *pRes, *pLogon, *pAct, *pAct1, *pComp, *pERC, *pVer, *pStrError;
    VIM_CHAR *pszPrint = VIM_NULL;
    VIM_CHAR *Ptr = VIM_NULL;
    VIM_CHAR Buffer[100];
    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    *Completed = VIM_FALSE;     // by default...

    if ((pResp = cJSON_Parse(JsonBuffer)) != VIM_NULL)
    {
        VIM_DBG_POINT;
        VIM_CHAR TID[8 + 1];
        vim_mem_clear( TID, VIM_SIZEOF( TID ));
        vim_mem_copy(TID, terminal.terminal_id, 8);
        VIM_DBG_STRING(TID);
        VIM_BOOL InstallInProgress = VIM_FALSE;

        if(( pLogon = cJSON_DetachItemFromObject(pResp, TID)) != VIM_NULL)
        {
            cJSON *pBrand, *pLFDServerVer, *pOthers;
            VIM_DBG_POINT;

            // RDD 070121 JIRA PIRPIN-972 - write WebLFD version to file
            if ((pLFDServerVer = cJSON_GetObjectItem(pLogon, "webLfdVersion")) != VIM_NULL)
            {
                VIM_CHAR *Ptr = VIM_NULL;
                if ((pszPrint = cJSON_Print(pLFDServerVer)) != VIM_NULL)
                {
                    VIM_DBG_POINT;
                    if (vim_strstr(pszPrint, "\"", &Ptr) == VIM_ERROR_NONE)
                    {
                        VIM_CHAR *Ptr = pszPrint;
                        VIM_CHAR *Ptr2 = VIM_NULL;

                        if (vim_strchr( Ptr, '"', &Ptr2 ) == VIM_ERROR_NONE)
                        {
                            VIM_DBG_PRINTF1("LFDVersion = %s ", WebLFDServerVersion);
                            *Ptr2 = 0;
                        }
                        VIM_DBG_PRINTF1("webLFDServerVer = %s ", pszPrint);
                        vim_strcpy(WebLFDServerVersion, ++Ptr );
                    }
                    FREE_MALLOC(pszPrint);
                }
            }


            if ((pBrand = cJSON_GetObjectItem( pLogon, "BRANDING" )) != VIM_NULL)
            {
                VIM_CHAR *Ptr = VIM_NULL;
                if ((pszPrint = cJSON_Print(pBrand)) != VIM_NULL)
                {
                    VIM_DBG_POINT;
                    if (vim_strstr(pszPrint, "\"", &Ptr) == VIM_ERROR_NONE)
                    {
                        VIM_SIZE FileSize = 0;
                        VIM_FILE_HANDLE file_handle;
                        VIM_CHAR *Ptr = pszPrint;
                        VIM_CHAR *Ptr2 = VIM_NULL;
                        VIM_CHAR Brand[3 + 1] = { 0,0,0,0 };
                        VIM_DBG_PRINTF1("Branding = %s ", pszPrint);
                        vim_strncpy(Brand, ++Ptr, 3);
                        if (vim_file_open(&file_handle, EDPData, VIM_TRUE, VIM_TRUE, VIM_TRUE) == VIM_ERROR_NONE)
                        {
                            FileSize = VIM_SIZEOF(Brand);
                            vim_file_write(file_handle.instance, Brand, FileSize);
                            vim_file_close(file_handle.instance);
                            pceft_debug(pceftpos_handle.instance, "Branding added to wally.data file");
                        }
                        else {
                            pceft_debug(pceftpos_handle.instance, "Branding failed to wally.data file");
                        }

                        // RDD 020413 - Changes for handling extra params in branding string
                        //ProcessBrandingParams( &Ptr, &pb_flag);
                        vim_sprintf(terminal.configuration.BrandFileName, "%s.png", Brand );
                        VIM_DBG_STRING(terminal.configuration.BrandFileName);

                        if( vim_strstr( Ptr, "XXX", &Ptr2) == VIM_ERROR_NONE)
                        {
                            terminal_set_status(ss_wow_basic_test_mode);                            
							pceft_debug(pceftpos_handle.instance, "Set XXX");
						}
                        else                        
                        {   
							if (terminal_get_status(ss_wow_basic_test_mode))
							{
								pceft_debug(pceftpos_handle.instance, "Clear XXX");
							}
                            terminal_clear_status(ss_wow_basic_test_mode);
                        }                        
                        terminal_save();               
                    }
                    VIM_DBG_POINT;
                    FREE_MALLOC(pszPrint);
                }
                VIM_DBG_POINT;
            }
            // RDD 120321 - need to run this even if nothing in others as will set defaults 
            if ((pOthers = cJSON_GetObjectItem(pLogon, "others")) != VIM_NULL)
            {
                //VIM_NET_PTR net_ptr = VIM_NULL; // Not need for WebLFD
                pszPrint = cJSON_Print(pOthers);
                VIM_DBG_PRINTF1( "Others = %s ", pszPrint );                
                if (!done)
                {
                    ParseRegOptions( (VIM_CHAR *)pszPrint, NumParams() );
                    done = VIM_TRUE;
                }
                FREE_MALLOC(pszPrint);
            }
            cJSON_Delete( pLogon );
        }

        if ((pRes = cJSON_DetachItemFromObject(pResp, "RESULTS")) != VIM_NULL)
        {
            VIM_DBG_POINT;
            if ((pERC = cJSON_GetObjectItem(pRes, "ERC")) != VIM_NULL)
            {
                VIM_CHAR *Ptr = VIM_NULL;
                if ((pszPrint = cJSON_Print(pERC)) != VIM_NULL)
                {
                    VIM_DBG_POINT;
                    if( vim_strstr( pszPrint, "\"", &Ptr ) == VIM_ERROR_NONE )
                    {
                        vim_strncpy( ERC, ++Ptr, 2 );
                        VIM_DBG_PRINTF1( "ERC = %s ", pszPrint );

                        if( vim_strncmp( ERC, "00", 2 ) != 0)
                        {
                            VIM_CHAR Buffer2[100];
                            pceft_debugf_test( pceftpos_handle.instance, "ERC = %s", pszPrint );

                            if ((pStrError = cJSON_GetObjectItem(pRes, "StrError")) != VIM_NULL)
                            {
                                //VIM_CHAR *Ptr2 = VIM_NULL;
                                if ((pszPrint = cJSON_Print(pStrError)) != VIM_NULL)
                                {
                                    VIM_DBG_POINT;
                                    if ( vim_strlen(pszPrint) > 2 )     // Always get empty quotes ... "" so exclude these from reporting and error generation
                                    {
                                        vim_sprintf(Buffer2, "WebLFD Client StrErr: %s", pszPrint);
                                        pceft_debug(pceftpos_handle.instance, Buffer2);
                                        vim_event_sleep(2000);
                                        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
                                    }
                                }
                                VIM_DBG_POINT;
                            }
                            VIM_DBG_POINT;
                        }
                        VIM_DBG_POINT;
                    }
                    VIM_DBG_POINT;
                    FREE_MALLOC(pszPrint);
                }
                VIM_DBG_POINT;
            }

            VIM_DBG_POINT;

            if ((pVer = cJSON_GetObjectItem(pRes, "LFDVersion")) != VIM_NULL)
            {
                VIM_CHAR *Ptr = VIM_NULL;
                if ((pszPrint = cJSON_Print(pVer)) != VIM_NULL)
                {
                    VIM_DBG_POINT;
                    if (vim_strstr(pszPrint, "\"", &Ptr) == VIM_ERROR_NONE)
                    {
                        vim_sprintf(WebLFDClientVersion, "v%s", ++Ptr);
                        if (vim_strchr(WebLFDClientVersion, '"', &Ptr) == VIM_ERROR_NONE)
                        {
                            *Ptr = 0;
                            VIM_DBG_PRINTF1("LFDVersion = %s ", WebLFDClientVersion);
                        }
                        VIM_DBG_PRINTF1("LFDVersion = %s ", WebLFDClientVersion);
                    }
                    VIM_DBG_POINT;
                    FREE_MALLOC(pszPrint);
                }
                VIM_DBG_POINT;

            }


            // Get the "Action" contents
            VIM_DBG_POINT;
            if ((pAct = cJSON_GetObjectItem(pRes, "Action")) != VIM_NULL)
            {
                VIM_DBG_POINT;
                if ((pszPrint = cJSON_Print(pAct)) != VIM_NULL)
                {
                    VIM_DBG_STRING(pszPrint);
                    //pceft_debug( pceftpos_handle.instance, pszPrint );
                    VIM_DBG_POINT;
                    vim_sprintf(Buffer, "%s", pszPrint);
                    pszPrint = VIM_NULL;

                    // MB says lets send the WebLFD Actions as Z0 debug
                    pceft_debug( pceftpos_handle.instance, Buffer );
                    FREE_MALLOC(pszPrint);
                }
                VIM_DBG_POINT;
            }

            // RDD - WebLFD client can set Activation Pending before downloading the file for activation - don't reboot after a .CFG download
            if (((pAct1 = cJSON_GetObjectItem(pRes, "ActivationPending")) != VIM_NULL) && ( vim_strstr( Buffer, "dl.", &Ptr ) == VIM_ERROR_NONE ))
            {
                VIM_DBG_MESSAGE( "----------------- Activation Pending -------------------" );

                if ((pszPrint = cJSON_Print(pAct1)) != VIM_NULL)
                {
                    VIM_DBG_STRING(pszPrint);
#if 1
                    if (!SilentMode)
                    {
                        static VIM_BOOL Set;
                        VIM_CHAR *Ptr = VIM_NULL;
                        VIM_DBG_PRINTF1("Activation Pending = %s ", pszPrint);
                        if( vim_strchr( pszPrint, '1', &Ptr ) == VIM_ERROR_NONE )
                        {
                            // RDD 230321 - Ensure Z0 debuf generated by WebLFD is always displayed
                            VAA_CheckDebug();

                            InstallInProgress = VIM_TRUE;
                            if( Set != InstallInProgress )
                            {
                                display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "S/W Install\nIn Progress", "Please Wait");
                                // extend the timeout to 10 mins
                                vim_event_sleep(5000);      // Some installs can be real quick so ensure stays on display
                                ulTimeout = 600000; // 10 minutes
                                Set = InstallInProgress;
                            }
                        }
                        VIM_DBG_POINT;
                    }
#endif
                    // MB says lets send the WebLFD Actions as Z0 debug
                   // pceft_debug( pceftpos_handle.instance, Buffer );
                    
                    FREE_MALLOC(pszPrint);
                }
                VIM_DBG_POINT;
            }

            VIM_DBG_POINT;

            // Get the "Completed" contents
            if(( pComp = cJSON_GetObjectItem( pRes, "Completed" )) != VIM_NULL )
            {
                VIM_DBG_POINT;
                VIM_CHAR *pszPrint;
                if(( pszPrint = cJSON_PrintUnformatted( pComp )) != VIM_NULL )
                {
                    VIM_CHAR *Ptr = VIM_NULL;
                    VIM_DBG_PRINTF1("Completed = %s ", pszPrint);
                    if (vim_strchr(pszPrint, '1', &Ptr) == VIM_ERROR_NONE)
                    {
                        *Completed = VIM_TRUE;
                        //terminal_set_status(ss_tms_configured);
                        if (InstallInProgress)
                        {
                            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "S/W Install Success", "Rebooting Now...");
                            terminal_set_status(ss_tms_configured);
                            // ADK Install can take a while ( > 4 mins ).... so don't exit for 10 mins - WebLFDClient will reboot
                            //pceft_debug(pceftpos_handle.instance, "WebLFD SW Install Error Timeout:Reboot from Application");

                            // RDD 230321 - Ensure Z0 debuf generated by WebLFD is always displayed
                            VAA_CheckDebug();
                            pceft_pos_display_close();
                            vim_event_sleep(1000);
                            vim_terminal_reboot();
                        }
                    }
                    FREE_MALLOC(pszPrint);
                }
            }
            cJSON_Delete(pRes);
        }
        VIM_DBG_POINT;
        cJSON_Delete(pResp);
    }
    return VIM_ERROR_NONE;
}



static VIM_CHAR *GetScheduledBody(void)
{
    static VIM_CHAR Buffer[200];
    VIM_CHAR DateTimeString[50];
    VIM_RTC_UNIX UnixTime;
    VIM_RTC_TIME CurrentTime, ScheduledTime;
    VIM_UINT8 lfd_hour = DAILY_HOUSE_KEEPING_HOUR;
    VIM_UINT8 lfd_mins = terminal.configuration.OffsetTime;
    //VIM_UINT8 lfd_days;

    vim_mem_clear(DateTimeString, VIM_SIZEOF(DateTimeString));
    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    vim_rtc_get_time(&CurrentTime);
    vim_rtc_get_unix_date(&UnixTime);

    if ((CurrentTime.hour > lfd_hour) || ((CurrentTime.hour == lfd_hour) && (CurrentTime.minute > lfd_mins)))
    {
        // Next LFD is tomorrow
        UnixTime += 86400;
    }
    // Convert unix back to normal time and set hrs, mins etc.
    vim_rtc_convert_unix_to_time(UnixTime, &ScheduledTime);
    ScheduledTime.hour = lfd_hour;
    ScheduledTime.minute = lfd_mins;
    ScheduledTime.second = 0;

    // format: “DDMMYY HHMMSS”
    vim_sprintf( DateTimeString, "%2.2d%2.2d%2.2d %2.2d%2.2d%2.2d", ScheduledTime.day, ScheduledTime.month, ScheduledTime.year%2000, lfd_hour, lfd_mins, ScheduledTime.second );
    vim_sprintf(Buffer, "    \"ContactServer\":\"%s\",\r\n    \"InstallFiles\":\"%s\"\r\n", DateTimeString, DateTimeString);
    return Buffer;
}



static VIM_ERROR_PTR CreateJsonRequestFile( VIM_CHAR *temp_buffer, VIM_SIZE data_size, VIM_BOOL Activate )
{
    VIM_CHAR JsonBuffer[2000];
    VIM_CHAR SrcBuffer[2000];
    VIM_CHAR *SrcPtr = SrcBuffer;
    VIM_CHAR *DestPtr = JsonBuffer;
    VIM_ERROR_PTR res;

    vim_mem_clear(SrcBuffer, VIM_SIZEOF(SrcBuffer));
    vim_mem_clear(JsonBuffer, VIM_SIZEOF(JsonBuffer));

    vim_mem_copy(SrcBuffer, temp_buffer, data_size);

    // Open Bracket for file
    JsonOpenCloseFile(&DestPtr, VIM_TRUE);
    JsonOpenCloseContainer(&DestPtr, terminal.terminal_id, VIM_TRUE, VIM_FALSE);

    // Create Logon body
    do
    {
        res = ParseLine(&SrcPtr, &DestPtr);
    } while (res == VIM_ERROR_NONE);

    JsonOpenCloseContainer(&DestPtr, terminal.terminal_id, VIM_FALSE, VIM_FALSE);

    // "COMMANDS" - container if we want to activate LFD Now
    if (1)
    {
        JsonOpenCloseContainer(&DestPtr, "COMMANDS", VIM_TRUE, VIM_TRUE);
        {
            VIM_CHAR CmdsBuff[200];
            VIM_SIZE Len;
            vim_mem_clear(CmdsBuff, VIM_SIZEOF(CmdsBuff));

            // TODO - Add support and test scheduled downloads
            //if( Activate )
            if(1)
                vim_strcpy(CmdsBuff, CMDS_ACTIVATE_BODY);
            else
            {
                vim_strcpy(CmdsBuff, GetScheduledBody());
            }
            Len = vim_strlen(CmdsBuff);
            vim_strncat(DestPtr, CmdsBuff, Len);
            DestPtr += Len;
        }
        JsonOpenCloseContainer(&DestPtr, "COMMANDS", VIM_FALSE, VIM_TRUE);
    }
    JsonOpenCloseFile(&DestPtr, VIM_FALSE);
    vim_file_delete(JsonReqFile);
    VIM_DBG_STRING(JsonBuffer);
    VIM_ERROR_RETURN_ON_FAIL( vim_file_set( JsonReqFile, (const void *)JsonBuffer, vim_strlen(JsonBuffer) ));

    VIM_ERROR_RETURN( VIM_ERROR_NONE );
}


VIM_ERROR_PTR SetupLFDReq(VIM_TMS_PARAMETERS *tms_parameters, VIM_BOOL Activate )
{
    VIM_CHAR temp_buffer[2000];
    VIM_SIZE data_size = VIM_SIZEOF(temp_buffer);

    vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));

    if (Activate)
    {
        vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
        VIM_ERROR_RETURN_ON_FAIL(tms_build_terminal_params(tms_parameters, temp_buffer, &data_size));
    }
#ifndef _WIN32
    // Ensure that the last response file is deleted prior to sending the next request
    vim_file_delete(JsonRespFile);
#endif

    CreateJsonRequestFile( temp_buffer, data_size, Activate );

    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR ParseLFDResp( VIM_TMS_PARAMETERS *tms_parameters, VIM_BOOL SilentMode )
{
    //VIM_CHAR * JsonBuffer = l_sLFDReqData;
    VIM_CHAR JsonBuffer[3000];
    VIM_SIZE FileSize;
    VIM_BOOL exists, FileUpdated = VIM_FALSE;
    //VIM_CHAR *Ptr, *Ptr2 = VIM_NULL;
    VIM_RTC_UPTIME now, end;
    VIM_BOOL Completed = VIM_FALSE;
    VIM_CHAR RespCode[2 + 1] = { 0,0,0 };
    cJSON_Hooks *DefaultDMA = VIM_NULL;
    VIM_UINT64 SavedTimeOut = ulTimeout;

    vim_rtc_get_uptime(&now);
    end = now + ulTimeout;

    vim_file_modified(JsonRespFile, &FileUpdated);

    VIM_DBG_MESSAGE("--------------------- Init cJSON DMA ----------------------");
    vim_event_sleep(100);

    // RDD 160620 - if Verifone cJSON lib follows standard SRC then NULL will allocate standard hooks, malloc, free etc.
    cJSON_InitHooks(DefaultDMA);

    VIM_DBG_MESSAGE("--------------------- Init cJSON DMA OK ----------------------");

    do
    {
        FileUpdated = VIM_FALSE;

        // RDD 230321 - Ensure Z1 debug generated by WebLFD is always displayed
        VAA_CheckDebug();

        if (ulTimeout != SavedTimeOut)
        {
            pceft_debug(pceftpos_handle.instance, "WebLFD Timeout Extended due to install");
            end += ulTimeout;
            SavedTimeOut = ulTimeout;
        }
        vim_rtc_get_uptime(&now);
        vim_file_modified( JsonRespFile, &FileUpdated );
        exists = VIM_FALSE;
        VIM_DBG_VAR(FileUpdated);
        if (1)
        {
            // Grab the file contents
            //VIM_DBG_MESSAGE("--------------------- New LFD Response File Update  ----------------------");
            vim_file_exists( JsonRespFile, &exists );
            if (!exists)
            {
               // VIM_DBG_MESSAGE("!!!!!!!!!!!!!! No response File !!!!!!!!!!!!!");
                vim_event_sleep(25);
                continue;
            }
            else
            {
                 VIM_DBG_MESSAGE("Picked up WebLFD Response File");
                 // RDD JIRA PIRPIN-1640 - issue with 82000 timeout on s/w installs - allow wait for second resp file write 
				if (!SilentMode)
				{
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Processing Response\n", "Please Wait");
					vim_event_sleep(5000);
				}
                VIM_ERROR_WARN_ON_FAIL( vim_file_get(JsonRespFile, (void *)JsonBuffer, &FileSize, VIM_SIZEOF(JsonBuffer)));
            }
            VIM_DBG_MESSAGE("--------------------- New LFD JSON Parsing  ----------------------");
            VIM_DBG_MESSAGE(JsonBuffer);

            vim_event_sleep(100);
            VIM_ERROR_RETURN_ON_FAIL( parseResp(JsonBuffer, &Completed, RespCode, SilentMode ));

            if (Completed)
            {
                DBG_MESSAGE(" ------ COMPLETED !!! ----------");
                if (!SilentMode)
                {
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Web TMS Session\n", "Completed");
                    vim_event_sleep(1000);
                }
                // MB says lets send the WebLFD Actions as Z0 debug
                vim_strcpy(tms_parameters->response_code, RespCode);
                VIM_DBG_PRINTF1("--------------------- New LFD JSON Completed : %s ----------------------", RespCode );

                if( vim_strncmp( RespCode, "00", 2 ))
                {
                    pceft_debug_test(pceftpos_handle.instance, "WebLFD Error:");
                    pceft_debug_test(pceftpos_handle.instance, RespCode);
                    if (!vim_strncmp(RespCode, "WB", 2))
                    {
                        pceft_debug(pceftpos_handle.instance, "WebLFD-Check Server Running:");
                    }
                    else if (!vim_strncmp(RespCode, "X0", 2))
                    {
                        pceft_debug(pceftpos_handle.instance, "WebLFD-Check Proxy Running:");
                    }
                    else if (!vim_strncmp(RespCode, "W1", 2))
                    {
                        pceft_debug(pceftpos_handle.instance, "WebLFD-Check Server Files Path and Registry Settings");
                    }
                    return ERR_WOW_ERC;
                }
                else
                {
                    return VIM_ERROR_NONE;
                }
            }
            else
            {
                DBG_MESSAGE(" ------ NOT COMPLETED !!! ----------");
            }
        }
        // Goto sleep
        vim_terminal_wait(10);

    } while (now < end);

    // RDD 171220- if X0 here then probably no Proxy or RNDIS not working
    pceft_debug_test(pceftpos_handle.instance, "WebLFD No response from Applet. Maybe not present");
    vim_strcpy(tms_parameters->response_code, "X0");
    return ERR_HOST_RECV_TIMEOUT;
}
#endif


// RDD 030413 re-written for Phase4
//#define _OFFLINESW

#define DECLINED_INVALID "12"




VIM_BOOL AppVersionMismatch(void)
{
    VIM_CHAR AppFile[50], AppVer[50], AppRev[50];
    VIM_CHAR FileBuffer[1000];
    VIM_SIZE FileLen = 0;
    VIM_CHAR *Ptr = VIM_NULL;

    vim_mem_clear(AppVer, VIM_SIZEOF(AppVer));
    vim_mem_clear(AppRev, VIM_SIZEOF(AppRev));
    vim_mem_clear(FileBuffer, VIM_SIZEOF(FileBuffer));

    vim_sprintf(AppVer, "%d", BUILD_NUMBER);
    vim_sprintf(AppRev, "%d", REVISION);

    vim_sprintf(AppFile, "dl.82000.%c.%c%c.%c", AppVer[5], AppVer[6], AppVer[7], AppRev[0]);
    if( vim_file_get(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_HOST_VERSION_CFG, FileBuffer, &FileLen, VIM_SIZEOF(FileBuffer)) != VIM_ERROR_NONE)
    {
        pceft_debug(pceftpos_handle.instance, "Can't read last loaded .CFG");
        return VIM_TRUE;
    }
    VIM_DBG_STRING(FileBuffer);
    if( vim_strstr(FileBuffer, AppFile, &Ptr) != VIM_ERROR_NONE)
    {
        pceft_debug(pceftpos_handle.instance, "AppVer mismatch with last loaded .CFG" );
        return VIM_TRUE;
    }
    return VIM_FALSE;
}



void TMSCheckForReset(VIM_CHAR *ResponseCode, VIM_ERROR_PTR error )
{
    VIM_CHAR *Ptr = VIM_NULL;
    VIM_BOOL ResetReqd = VIM_FALSE;

    VIM_DBG_STRING(ResponseCode);
    if( vim_strstr( ResponseCode, DECLINED_INVALID, &Ptr ) == VIM_ERROR_NONE )
    {
        pceft_debug(pceftpos_handle.instance, "TMS Reset Due to Declined Invalid");
        ResetReqd = VIM_TRUE;
    }
    else if (error == VIM_ERROR_TMS_MAC)
    {
        pceft_debug(pceftpos_handle.instance, "TMS Reset Due to SHA1 Error");
        ResetReqd = VIM_TRUE;
    }
    else if (AppVersionMismatch())
    {
        pceft_debug(pceftpos_handle.instance, "TMS Reset Due to App Ver mismatch or .CFG error");
        ResetReqd = VIM_TRUE;
    }
    if (ResetReqd)
    {
        ResetTMS();
    }
}


VIM_ERROR_PTR web_lfd_contact(VIM_TMS_PARAMETERS *tms_parameters, VIM_BOOL SilentMode )
{
    VIM_ERROR_PTR error = VIM_ERROR_NONE;

    // Exit if not enabled.
    if (!TERM_NEW_LFD)
    {
        pceft_debug(pceftpos_handle.instance, "weblfd not enabled");
        //if( terminal.terminal_id[0] != 'L' )        
         //   return VIM_ERROR_NOT_FOUND;
        pceft_debug(pceftpos_handle.instance, "live override");
    }

    // Reset here
    done = VIM_FALSE;

    VIM_ERROR_RETURN_ON_FAIL(SetupLFDReq( tms_parameters, VIM_TRUE));
        if (!SilentMode)
        {
			if (TMS_VIA_LFD)
			{
				pceft_debug(pceftpos_handle.instance, "Web LFD Session - Start");
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Web LFD Session\n", "Please Wait...");
			}
			else
			{
				pceft_debug(pceftpos_handle.instance, "Web RTM Session - Start");
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Web RTM Session\n", "Please Wait...");
			}

        }

    if ((error = ParseLFDResp( tms_parameters, SilentMode )) != VIM_ERROR_NONE)
        {            
            pceft_debug(pceftpos_handle.instance, "WebTMS Failed, Run OldLFD:");
        }
        else
        {
            terminal_set_status(ss_tms_configured);
        }

        // Get rid of the Activate
    //VIM_ERROR_RETURN_ON_FAIL(SetupLFDReq(tms_parameters, VIM_FALSE));

        //terminal_set_status(ss_tms_configured);
    vim_mem_copy(init.logon_response, tms_parameters->response_code, VIM_SIZEOF(tms_parameters->response_code));

        // TODO - Fix this as shouldn't have to go in here but does...
    vim_mem_copy(txn.host_rc_asc, tms_parameters->response_code, VIM_SIZEOF(tms_parameters->response_code));
        if( !SilentMode )
            display_result( error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );

            return error;
    }



VIM_ERROR_PTR vim_lfd_contact(VIM_TMS_PARAMETERS *tms_parameters, VIM_TMS_DOWNLOAD download_type, VIM_BOOL SilentMode)
{
    VIM_ERROR_PTR error;
    VIM_INT16 Attempts = 1;

        VIM_DBG_ERROR( display_screen( terminal.IdleScreen, VIM_PCEFTPOS_KEY_NONE, "Contact TMS", "" ));

ClearCommand:			//BD Re-entry point if COMMAND needs to be cleared

    do
	{
		// download  parameters from a host
		VIM_DBG_NUMBER( Attempts );
        // RDD 080720 Trello #170
       // TMSCheckForReset(tms_parameters->response_code, error);

        error = vim_tms_lfd_start(download_type, tms_parameters);

          
        if( error == VIM_ERROR_TMS_NO_FILES || error == VIM_ERROR_TMS_UNAVAILABLE )
        {
			// All downloading finished and LFD host is updated
			error = VIM_ERROR_NONE;
		}

        // RDD 090819 JIRA WWBP-680
        if (error == VIM_ERROR_OS)
        {
            VIM_CHAR buffer[100];
            vim_mem_clear(buffer, VIM_SIZEOF(buffer));
            vim_tms_get_secins_status(buffer);
            DBG_STRING(buffer);
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "TMS Install Failed:\n", buffer );
            vim_event_wait(VIM_NULL, 0, 5000);
        }

		// RDD 080115 need more specific errors for comms errors espacially for PSTN
		if( error == VIM_ERROR_TIMEOUT )
			error = ERR_HOST_RECV_TIMEOUT;

		// RDD 151115 SPIRA:8841 Return W1 errors from file retrieval and report problem the N3 is from the original logon
		Attempts -= 1;
		if( error == VIM_ERROR_TMS_FILESYSTEM )
		{
            display_result(ERR_WOW_ERC, tms_parameters->response_code, "", "", VIM_PCEFTPOS_KEY_NONE, 2000);
			pceft_debug_error( pceftpos_handle.instance, "RTM Error:", error );
			// VN 230718 JIRA WWBP-57 MO returned instead of 'W1' for 'Request File not Found'
            vim_mem_copy(init.logon_response, tms_parameters->response_code, VIM_SIZEOF(tms_parameters->response_code));
			VIM_ERROR_RETURN( error );
		}

#if 0
		else if( DoRetries && Attempts )
		{
			display_result( error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
		}
#endif

    } while ((Attempts > 0) && (error != VIM_ERROR_NONE));

	// close display in case
	pceft_pos_display_close();

   // set LFD configured flag
	if(( error == VIM_ERROR_NONE ) || ( error == VIM_ERROR_TMS_RECONNECT ))
	{
		vim_strncpy( init.logon_response, "00", 2 );
		if( error == VIM_ERROR_NONE )
		{
			terminal_set_status( ss_tms_configured );
			DBG_MESSAGE( "ss_tms_configured SET tms contact" );
		}
	}
	else
	{
		VIM_DBG_ERROR( error );
        if ((vim_strlen(tms_parameters->response_code)) && (vim_strncmp(tms_parameters->response_code, "N3", 2)) && (vim_strncmp(tms_parameters->response_code, "00", 2)))
		{
			// RDD 070912 Show some sort of error if LFD fails
            vim_mem_copy(init.logon_response, tms_parameters->response_code, VIM_SIZEOF(tms_parameters->response_code));
			error = ERR_WOW_ERC;
            display_result(error, tms_parameters->response_code, "", "", VIM_PCEFTPOS_KEY_NONE, 2000);
			pceft_debug_error( pceftpos_handle.instance, "RTM Error:", error );
		}
        else if (vim_strlen(tms_parameters->response_code))
		{
            display_result(error, tms_parameters->response_code, "", "", VIM_PCEFTPOS_KEY_NONE, 2000);
		}
		else
		{
			display_result( error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
		}
		VIM_ERROR_RETURN( error );
	}

	if( terminal_get_status( ss_lfd_remote_command_executed ) )
	{
//		terminal_clear_status( ss_tms_configured );				//BD Moved higher to set LFD needed after TMS contact
		DBG_MESSAGE( "GOTO tms contact due remote command" );
		goto ClearCommand;
	}
	/* return without error */

    // RDD 030420 v582-7 check to see if we need to re-init CTLS reader
    if( terminal_get_status( ss_wow_reload_ctls_params ))
    {
        CtlsInit();
    }

	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR tms_contact( VIM_TMS_DOWNLOAD download_type, VIM_BOOL SilentMode, tms_type_t tms_type )
{
    VIM_ERROR_PTR error = VIM_ERROR_SYSTEM;
	VIM_TMS_PARAMETERS tms_parameters;

	if (TID_INVALID)
	{
		pceft_debug(pceftpos_handle.instance, "Invalid TID - Skip Power up RTM");
		return VIM_ERROR_NONE;
	}

	// RDD 170722 VHQ - lock against reboots etc.
	terminal_set_status(ss_locked);

//ClearCommand:			//BD Re-entry point if COMMAND needs to be cleared

	// RDD 230812 - Revert to the branding screen when finished or if error
	terminal.screen_id = 0;

    // RDD 100321 - noticed getting Q5 after reboot during s/w installation
    terminal_clear_status(ss_incomplete);	

	vim_mem_clear( &tms_parameters, sizeof( tms_parameters ));

	// RDD 080618 JIRA WWBP-112 - PCI encrypt card data - ensure SAF deleted before changing key !!
	// RDD 210818 JIRA WWBP-315 - Woolies want to reject TMS if SAF pending - Now agreed to send SAFs (MB)
    // Reversal includes sending any repeat SAF

	// RDD 260722 JIRA PIRPIN-1752
	if (TERM_OPTION_VHQ_ONLY)
	{
		return tms_vhq_heartbeat(SilentMode);
	}


	// RDD 290422 - don't allow testers to get in a mess by setting unrealistic configs
	if ((IS_V400m)||( IS_R10_POS == VIM_FALSE ))
	{
		tms_type = web_lfd;
	}

#ifdef _WIN32
	tms_type = vim_lfd;
#endif

    error = reversal_send( VIM_TRUE );
    if( error == VIM_ERROR_NOT_FOUND )
		  error = VIM_ERROR_NONE;

    // RDD 230212 SPIRA4824 - abort SAF if error
    VIM_ERROR_RETURN_ON_FAIL(error);

	/* Check for trickle feeding 220s (if required - depends on caller )  */
	if( saf_totals.fields.saf_count )
	{	
		DBG_MESSAGE("<<<<<<<<< SAFs to Send.... Sending SAF >>>>>>>>>>>>>");
		terminal_clear_status(ss_locked);
		VIM_ERROR_RETURN_ON_FAIL( saf_send( VIM_FALSE, MAX_BATCH_SIZE, VIM_FALSE, VIM_FALSE ));
	}

	// Return if not ready for TMS
	VIM_ERROR_RETURN_ON_FAIL( tms_ready( ) );

	// Return if params can't be set for some reason
	VIM_ERROR_RETURN_ON_FAIL( tms_get_params( &tms_parameters ) );
	terminal_set_status(ss_locked);

    VIM_DBG_DATA(tms_parameters.merchant_id, 15);

    //////////////////
    // 110520 Trello #137 Change to New LFD
    // RDD 030221 - updated to upport multiple lfd paths
    //////////////////
#ifndef _WIN32
    if (terminal.terminal_id[0] == 'L')
    {
        error = web_lfd_contact(&tms_parameters, VIM_FALSE);
    }
    else
#endif
    {
        switch (tms_type)
        {
            // web lfd only
        case web_lfd:
			VIM_DBG_MESSAGE("WEB LFD ONLY");
            // WebLFD not supported on W32 yet. 
            error = web_lfd_contact(&tms_parameters, SilentMode);
            break;

            // vim_lfd only
        case vim_lfd:
			VIM_DBG_MESSAGE("VIM LFD ONLY");
			error = vim_lfd_contact(&tms_parameters, download_type, SilentMode);
            break;

            // web then vim if web fails
        case web_then_vim:
        default:
			VIM_DBG_MESSAGE("WEB LFD then VIM LFD");
#ifndef _WIN32
            VIM_ERROR_RETURN_ON_PASS(web_lfd_contact(&tms_parameters, SilentMode));
#endif
            error = vim_lfd_contact(&tms_parameters, download_type, SilentMode);
            break;
        }

    }
	terminal_clear_status(ss_locked);
    VIM_ERROR_RETURN(error);   
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_callback_parameters_get_versions
(
  VIM_TLV_LIST *tlv_parameters_ptr
)
{
  VIM_ERROR_RETURN_ON_NULL(tlv_parameters_ptr);
  VIM_ERROR_RETURN(VIM_ERROR_NOT_IMPLEMENTED);
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR tms_get_prompt_title
(
  /**[out] ptr of prompt_title */
  VIM_TEXT *prompt_title_ptr,
  /**[in] max length of prompt title */
  VIM_SIZE prompt_title_max_length,
  /**[in] type of language */
  VIM_TMS_DISPLAY_LANGUAGE language_type,
  /**[in[ type of prompt */
  VIM_TMS_PROMPT_TYPE prompt_type
)
{
  VIM_SIZE prompt_index;

  for( prompt_index=0; prompt_index<VIM_LENGTH_OF( tms_prompt_list); prompt_index++ )
  {
    if( tms_prompt_list[prompt_index].language_type == language_type &&
        tms_prompt_list[prompt_index].prompt_type == prompt_type )
    {
        vim_strncpy( prompt_title_ptr, (VIM_TEXT *)tms_prompt_list[prompt_index].prompt_content,prompt_title_max_length);
        break;
    }
  }

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_callback_display_general_prompt
(
  /**[in] type of language */
  VIM_TMS_DISPLAY_LANGUAGE language_type,
  /**[in[ type of prompt */
  VIM_TMS_PROMPT_TYPE prompt_type,
  /**[in] specific prompt content */
  VIM_TEXT const * prompt_content
)
{
  VIM_TEXT prompt_title[100];
  vim_mem_set( prompt_title, 0x00, VIM_SIZEOF( prompt_title ));

  VIM_ERROR_RETURN_ON_FAIL( tms_get_prompt_title( prompt_title, VIM_SIZEOF(prompt_title)-1, language_type, prompt_type));
#if 0
  if( vim_strlen( prompt_title ) < 20 && vim_strlen( (VIM_TEXT *)prompt_content ) < 20 )
  {
    VIM_TEXT tmp_buffer[64];
    vim_snprintf( tmp_buffer, VIM_SIZEOF(tmp_buffer), "%s\n%s",prompt_title,prompt_content );
    VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, tmp_buffer, ""));
    VIM_ERROR_RETURN_ON_FAIL( vim_event_sleep(700));
  }
#else	// RDD 291211 - make to look like 5100 LFD UI
    //vim_sprintf( prompt_title, "%s\n%s", prompt_title, prompt_content );
    VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, language_type, prompt_title, "" ));
#endif

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_callback_software_downloading_display
(
  VIM_TEXT const * filename_ptr,
  VIM_SIZE number_size,
  VIM_SIZE total_size
)
{
  VIM_TEXT prompt[50+1];
  VIM_TEXT prompt_title[100];

  if (total_size > terminal.configuration.LFDPacketsMax)
  {
      VIM_DBG_MESSAGE( "!!!!!!!!!!!!!!!!!! Exceeds Max Packets - Disconnect !!!!!!!!!!!!!!!!!!!")
      comms_disconnect();
  }

  vim_mem_set( prompt_title, 0x00, VIM_SIZEOF( prompt_title ));

  VIM_ERROR_RETURN_ON_FAIL( tms_get_prompt_title( prompt_title, VIM_SIZEOF(prompt_title)-1, VIM_TMS_LANGUAGE_ENGLISH, VIM_TMS_PROMPT_DOWNLOADING));

  if( total_size == 0 )
    return VIM_ERROR_OVERFLOW;

 // update_battery_level_icon();

#if 0
  if( total_size < 100 )
  {
    VIM_ERROR_RETURN_ON_NULL(filename_ptr);
    VIM_ERROR_RETURN_ON_FAIL( vim_sprintf( progress_bar, "%d%%", number_size*100/total_size ));
    vim_sprintf( prompt, "%s %s", filename_ptr, progress_bar );
	VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, prompt_title, prompt ));
  }
  else
  {
    VIM_SIZE unit;
    unit = total_size/100;
    if( number_size% unit == 0 || number_size == total_size || number_size == 1 )
    {
      vim_mem_set( progress_bar, 0x00, VIM_SIZEOF( progress_bar ));
      if( total_size == 0 )
        return VIM_ERROR_OVERFLOW;
      VIM_ERROR_RETURN_ON_FAIL( vim_sprintf( progress_bar, "%d%%", number_size*100/total_size ));
      vim_sprintf( prompt, "%s %s", filename_ptr, progress_bar );
	  VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, prompt_title, prompt ));
	}
  }
#else
  VIM_ERROR_RETURN_ON_NULL( filename_ptr );
  {
      VIM_UINT8 percentage = number_size * 100 / total_size;
      vim_sprintf(prompt, "%s\n---- %2d%% ----", filename_ptr, percentage);

      VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, prompt_title, prompt));

      if( percentage == 100 )
      {
          vim_event_sleep(100);
          pceft_pos_display_close();
      }
  }
#endif
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_stepup_callback_get_file_zipped_flag
(
  /**[out] flag of file zipped */
  VIM_BOOL *zipped_flag_ptr
)
{
  *zipped_flag_ptr = VIM_TRUE;

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR tms_generate_ctls_version_number
(
  const VIM_UINT8* version_buffer,
  VIM_SIZE version_size,
  /*@out@*/VIM_TEXT *version_number,
  VIM_SIZE version_number_max_size
)
{
  VIM_UINT8 fm_number[5];
  VIM_UINT8 tmp_ver_buffer[30];
  VIM_SIZE cnt, pos;

  /* set default version number, we do like this coz the reader is there
     and we are not able to parse the string */
  vim_snprintf( version_number, version_number_max_size, "VC000000" );

  /* convert version buffer to to AN */
  vim_mem_clear( tmp_ver_buffer, VIM_SIZEOF(tmp_ver_buffer));
  pos=0;
  for( cnt=0; cnt<version_size; cnt++ )
  {
    if(  ('0' <= version_buffer[cnt] && version_buffer[cnt] <= '9') ||
         ('a' <= version_buffer[cnt] && version_buffer[cnt] <= 'z') ||
         ('A' <= version_buffer[cnt] && version_buffer[cnt] <= 'Z'))
    {
      tmp_ver_buffer[pos++] = version_buffer[cnt];
    }
  }

  VIM_ERROR_RETURN_ON_UNDERFLOW(pos,6);
  /* build the contactless firmware file */
  vim_mem_copy( fm_number, tmp_ver_buffer + pos - 6, 6 );


  vim_mem_clear( version_number, version_number_max_size);
  vim_snprintf( version_number, version_number_max_size, "VC%6.6s", fm_number);

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_stepup_callback_get_other_download_files
(
  /**[in,out] TLV list for files */
  VIM_TLV_LIST *tlv_version_list,
  /**[in,out] buffer of TLV structure */
  VIM_UINT8* tlv_buffer_ptr,
  /**[in,out] buffer max size */
  VIM_SIZE *tlv_buffer_max_size
)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_UINT8 version_buffer[30];
  VIM_SIZE version_size=0;

  vim_mem_clear( version_buffer, VIM_SIZEOF(version_buffer));
  VIM_ERROR_RETURN_ON_NULL( tlv_buffer_ptr );
  if( picc_handle.instance != VIM_NULL )
  {
    res = vim_picc_emv_get_firmware_version(picc_handle.instance,version_buffer,VIM_SIZEOF( version_buffer),&version_size);
    if( res == VIM_ERROR_TIMEOUT )
    {
      /* close contactless reader */
	  DBG_POINT;

	  // RDD 290914 changed
	  //picc_close(VIM_TRUE);
	  picc_close(VIM_FALSE);

      /* reset CTLS reader */
	  // RDD 200716 v560-4 - this only works on vx810 - legacy code so get rid
      //vim_picc_vivotech_reader_restart();
      /* open contactless reader */
	  // RDD 200716 - get firmare version opens and closes reader
      //VIM_ERROR_RETURN_ON_FAIL( picc_open(VIM_TRUE,VIM_FALSE));
      /* read firm ware again */
      res = vim_picc_emv_get_firmware_version(picc_handle.instance,version_buffer,VIM_SIZEOF( version_buffer),&version_size);
    }
    if( res != VIM_ERROR_NONE )
    {
		VIM_DBG_WARNING( "CTLS READ FW VER FAILED !!!" );
		VIM_DBG_ERROR( res );
      /* try to read from file */
      res = tms_load_ctls_version_from_file(version_buffer,VIM_SIZEOF(version_buffer),&version_size);
    }
    else
    {
      /* store fmver number */
      tms_save_ctls_version_to_file(version_buffer, version_size);
    }

  }
  else
  {
    /* try to read from file */
    res = tms_load_ctls_version_from_file(version_buffer,VIM_SIZEOF(version_buffer),&version_size);
  }

  if( res == VIM_ERROR_NONE )
  {
    VIM_CHAR contactless_FM_file[20];
    VIM_TEXT version_number[20];
    //VIM_DBG_DATA( version_buffer, version_size );
    tms_generate_ctls_version_number(version_buffer,version_size,version_number,VIM_SIZEOF( version_number ));
    vim_mem_clear( contactless_FM_file, VIM_SIZEOF(contactless_FM_file));
    vim_snprintf( contactless_FM_file, VIM_SIZEOF(contactless_FM_file), "0\\%s", version_number );
    VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(tlv_version_list->total_length + vim_strlen(contactless_FM_file), *tlv_buffer_max_size );

    /* add file */
    {
      /* add this file into tlv_software_list */
      VIM_TLV_LIST tlv_fm_list;
      VIM_TEXT tlv_fm_buffer[200];
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create(&tlv_fm_list,tlv_fm_buffer,VIM_SIZEOF(tlv_fm_buffer)));
       //VIM_DBG_MESSAGE( contactless_FM_file );
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tlv_fm_list, "\xdf\x87\x01", contactless_FM_file,  vim_strlen(contactless_FM_file)));
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes( tlv_version_list, "\xff\x87\x01", tlv_fm_buffer, tlv_fm_list.total_length));
    }
  }

#ifdef __WXMSW__
{
  /* add this file into tlv_software_list */
  VIM_TLV_LIST tlv_fm_list;
  VIM_TEXT tlv_fm_buffer[200];
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create(&tlv_fm_list,tlv_fm_buffer,VIM_SIZEOF(tlv_fm_buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tlv_fm_list, "\xdf\x87\x01", "0\\QT820015",  10));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes( tlv_version_list, "\xff\x87\x01", tlv_fm_buffer, tlv_fm_list.total_length));
}

{
  /* add this file into tlv_software_list */
  VIM_TLV_LIST tlv_fm_list;
  VIM_TEXT tlv_fm_buffer[200];
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create(&tlv_fm_list,tlv_fm_buffer,VIM_SIZEOF(tlv_fm_buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tlv_fm_list, "\xdf\x87\x01", "0\\VC10400D",  10));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes( tlv_version_list, "\xff\x87\x01", tlv_fm_buffer, tlv_fm_list.total_length));
}

#endif
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_BOOL is_logon_time_due( void )
{
  /* TODO: */
  return VIM_FALSE;
}

#if 0
static VIM_BOOL PinPadFaultStatus( void )
{
	if((( terminal.stats.card_misreads > 10 ) && ( terminal.stats.card_read_percentage_fail > 20 )) ||
		(( terminal.stats.chip_misreads > 10 ) && ( terminal.stats.chip_read_percentage_fail > 10 )) ||
		(( terminal.stats.contactless_fallbacks > 10 ) && ( terminal.stats.contactless_read_percentage_fail > 10 )) ||
		(( terminal.stats.contactless_declines > 10 ) && ( terminal.stats.contactless_read_percentage_fail > 10 )) ||
		(( terminal.stats.connection_failures > 10 ) && ( terminal.stats.connection_percentage_fail > 10 )) ||
		(( terminal.stats.response_timeouts > 10 ) && ( terminal.stats.response_percentage_fail > 10 )))
	{
		return VIM_TRUE;
	}
	else
	{
		return VIM_FALSE;
	}
}
#endif

VIM_ERROR_PTR tms_GetStatsString( VIM_CHAR *dest_data_ptr )
{
	VIM_CHAR temp_buffer[100];

	//  vim_sprintf( temp_buffer, "Z:MAG BAD=%lu\n", terminal.stats.card_misreads );
	//  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:MAG GOOD=%lu\n", terminal.stats.card_good_reads );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	//  vim_sprintf( temp_buffer, "Z:MAG FAIL RATE=%lu\n", terminal.stats.card_read_percentage_fail );
	//  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:CHIP BAD=%lu\n", terminal.stats.chip_misreads );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:CHIP GOOD=%lu\n", terminal.stats.chip_good_reads );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:CHIP FAIL RATE=%lu\n", terminal.stats.chip_read_percentage_fail );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  // RDD 171013 SPIRA:6961 If the reader is dead then better report it
	//  if( terminal_get_status(ss_ctls_reader_fatal_error))
	//  {
	//	  vim_sprintf( temp_buffer, "Z:CTLS <<<FIRMWARE FATAL ERROR!!!>>>\n" );
	//  }
	//  else
	  {
		  VIM_SIZE good = terminal.stats.contactless_declines+terminal.stats.contactless_approves;
		  VIM_SIZE bad = terminal.stats.contactless_tap_fails + terminal.stats.contactless_collisions + terminal.stats.contactless_fallbacks + terminal.stats.bad_data;
		  VIM_SIZE total = bad + good;
          VIM_SIZE safs, saf_prints;
         
          vim_sprintf( temp_buffer, "Z:SAFS=%d\n", saf_pending_count( &safs, &saf_prints ));
          vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);

		  vim_sprintf( temp_buffer, "Z:CTLS FBKS=%lu\n", terminal.stats.contactless_fallbacks );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

		  vim_sprintf( temp_buffer, "Z:CTLS CARD ERR=%lu\n", terminal.stats.bad_data );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

		  // RDD 250315 SPIRA 8513
		  vim_sprintf( temp_buffer, "Z:CTLS BAD TAP=%lu\n", terminal.stats.contactless_tap_fails  );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

		  vim_sprintf( temp_buffer, "Z:CTLS COLLISIONS=%lu\n", terminal.stats.contactless_collisions  );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

		  vim_sprintf( temp_buffer, "Z:CTLS GOOD TAP=%lu\n", good );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

		  terminal.stats.contactless_read_percentage_fail = ( total ) ? (( bad * 100 ) / total ) : 0;

		  vim_sprintf( temp_buffer, "Z:CTLS FAIL RATE=%lu\n", terminal.stats.contactless_read_percentage_fail );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
	  }

		    // RDD 100413 - added for P4

	  vim_sprintf( temp_buffer, "Z:COMMS CONN FAILS=%lu\n", terminal.stats.connection_failures );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS CONN GOOD=%lu\n", terminal.stats.connection_succeses );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP TIMEOUTS=%lu\n", terminal.stats.response_timeouts );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP GOOD=%lu\n", terminal.stats.responses_received );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP TIMEOUT RATE=%lu\n", terminal.stats.response_percentage_fail );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP AVE TIME (mS)=%lu\n", terminal.stats.AverageResponseTime );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND0:0-1s)=%lu\n", terminal.stats.ResponseTimes[0] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND1:1-2s)=%lu\n", terminal.stats.ResponseTimes[1] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND2:2-4s)=%lu\n", terminal.stats.ResponseTimes[2] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND3:4-6s)=%lu\n", terminal.stats.ResponseTimes[3] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND4:6-10s)=%lu\n", terminal.stats.ResponseTimes[4] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

	  vim_sprintf( temp_buffer, "Z:COMMS RESP (BAND5:>10)=%lu\n", terminal.stats.ResponseTimes[5] );
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );


	  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
/**
 * Check if we are in the current dll window and dll flag is turned on
*/
VIM_BOOL tms_scheduled_logon(void)
{
  return is_logon_time_due();
}


// RDD - to add new TMS upload params follow the format exactly. The '=' between descriptor and data is critical for the data to parse into json format correctly

static VIM_ERROR_PTR build_LFD_parameters(
  VIM_UINT8* dest_data_ptr,
  VIM_SIZE max_size,
  VIM_SIZE* data_size )
{
  VIM_TEXT temp_buffer[1000];
  //VIM_CHAR part_number[50];
  //VIM_GPRS_INFO gprs_info;

  *data_size = 0;
  vim_mem_clear( dest_data_ptr, max_size );

  /* PPID */
  if( tcu_handle.instance_ptr != VIM_NULL )
  {
    VIM_DES_BLOCK ppid;
    if( vim_tcu_get_ppid (tcu_handle.instance_ptr, &ppid) == VIM_ERROR_NONE )
    {
		VIM_CHAR buffer2[100];
		vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
		vim_mem_clear( buffer2, VIM_SIZEOF(buffer2));
		VIM_ERROR_RETURN_ON_FAIL(vim_utl_hex_to_ascii (temp_buffer, &ppid, 16));
		VIM_ERROR_RETURN_ON_FAIL(vim_sprintf( (VIM_CHAR *)buffer2, "PPID=%s\n", temp_buffer ));
		vim_strcat( (VIM_CHAR *)dest_data_ptr, buffer2 );
     }
  }
  /* CPAT */
  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:CPAT=%06d\n", terminal.file_version[CPAT_FILE_IDX] ));
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
  /* EPAT */
  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:EPAT=%06d\n", terminal.file_version[EPAT_FILE_IDX] ));
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
  /* FCAT */
  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:FCAT=%06d\n", terminal.file_version[FCAT_FILE_IDX] ));
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
  /* PKT */
  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:PKT=%06d\n", terminal.file_version[PKT_FILE_IDX]));
  vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);

  // RDD 020821 JIRA PIRPIN 1014 Report CARDS and RPAT Ver
  // CARDS.XML
  vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:CARDS.XML=%s\n", terminal.configuration.CARDS_Version ));
  vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);
  // RPAT.XML

  XMLProductLimitExceeded( VIM_NULL, VIM_NULL );
  vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "CFG:RPAT.XML=%s\n", terminal.configuration.RPAT_Version));
  vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);

  // RDD WWBP-205 - Add flag for Enabling/disabling On device Printing
  if (terminal.configuration.PinPadPrintflag)
  {
	  vim_strcat((VIM_CHAR *)dest_data_ptr, "CFG:TerminalPrinting=Y\n");
  }
  else
  {
	  vim_strcat((VIM_CHAR *)dest_data_ptr, "CFG:TerminalPrinting=N\n");
  }

  /* Fault Status   BD */
  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
  if( terminal_get_status( ss_ctls_reader_fatal_error ))
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, "FaultStatus=CTLS Faulty\n" );
  else
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, "FaultStatus=None\n" );

	  // RDD 100413 - added for P4
  /* OS Version */
  {
    VIM_TEXT OS_name[50];
    VIM_TEXT OS_version[50];
    vim_mem_clear( OS_name, VIM_SIZEOF(OS_name));
    vim_mem_clear( OS_version, VIM_SIZEOF(OS_version));
    vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
    VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_OS_info (OS_name, VIM_SIZEOF(OS_name), OS_version, VIM_SIZEOF(OS_version), VIM_NULL, 0));
    VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "SW:OSVersion=%s\n", OS_name+1 ));
    vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
  }


  /* EOS Version */
  {
    VIM_TEXT EOS_version[50];
    VIM_CHAR *Ptr;
    vim_mem_clear( EOS_version, VIM_SIZEOF(EOS_version));
    vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
    VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_EOS_info (EOS_version, VIM_SIZEOF(EOS_version)));

    // RDD 200520 - check to see if theres a CR already as this stuffs up .json generation
    if (vim_strchr(EOS_version, 0x0a, &Ptr) == VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "SW:EOSVersion=%s", EOS_version));
    }
    else
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "SW:EOSVersion=%s", EOS_version));
    }
    vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
  }

  /* CTLS Version */

  // RDD 210618 JIRA WWBP-215 sometimes no CTLS ver in reg. probabaly reader was closed at the time
#ifndef _WIN32
  picc_open(PICC_OPEN_ONLY, VIM_FALSE);
#endif
  if( 1 )
  {
    VIM_TEXT CTLS_version[150];
    VIM_SIZE ret_size=0;
    vim_mem_clear( CTLS_version, VIM_SIZEOF(CTLS_version));
    vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
    if(( vim_picc_emv_get_firmware_version (picc_handle.instance, CTLS_version, VIM_SIZEOF(CTLS_version), &ret_size)) == VIM_ERROR_NONE )
	{
        vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "SW:CTLSVersion=%s\n", CTLS_version );
	}
	else
	{
		vim_snprintf_unterminated( temp_buffer, VIM_SIZEOF(temp_buffer), "SW:CTLSVersion=UNKNOWN\n" );
	}
    VIM_DBG_MESSAGE("------------- CTLS Version : ------------");
    VIM_DBG_STRING(temp_buffer);
    vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
    VIM_DBG_STRING((const VIM_CHAR*)dest_data_ptr);
  }
  picc_close(VIM_TRUE);


  // RDD 100323 JIRA PIRPIN-2177 Client Certificate management - upload expiry info to RTM etc.
#if 0
  if (vim_file_is_present(CLIENT_CERT_PATH))
  {
      VIM_SIZE ValidDays = 0;
      vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
      vim_x509Validate(VIM_NULL, CLIENT_CERT, &ValidDays);

      VIM_DBG_NUMBER(ValidDays);


      vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "SW:Client Cert Exp days=%d\n", ValidDays);
      VIM_DBG_STRING(temp_buffer);
      vim_strcat((VIM_CHAR*)dest_data_ptr, temp_buffer);
      VIM_DBG_STRING((const VIM_CHAR*)dest_data_ptr);
  }
#endif


  vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));

  // RDD 070121 JIRA PIRPIN-972
  if (!vim_strlen(WebLFDClientVersion))
  {
      vim_strcpy(WebLFDClientVersion, "UNKNOWN");
  }
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "SW:WebLFDClientVer=%s\n", WebLFDClientVersion ));
  VIM_DBG_MESSAGE("------------- WebLFD Client Version : ------------");
  VIM_DBG_STRING(temp_buffer);
  vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);
  VIM_DBG_STRING((const VIM_CHAR*)dest_data_ptr);

  vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));

  if (!vim_strlen(WebLFDServerVersion))
  {
      vim_strcpy(WebLFDServerVersion, "UNKNOWN");
  }
  else
  {
      VIM_CHAR *Ptr = WebLFDServerVersion;
      VIM_CHAR *Ptr2 = VIM_NULL;

      if (vim_strchr(Ptr, '"', &Ptr2) == VIM_ERROR_NONE)
      {
          VIM_DBG_PRINTF1("LFDVersion = %s ", WebLFDServerVersion);
          *Ptr2 = 0;
      }
  }
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf_unterminated(temp_buffer, VIM_SIZEOF(temp_buffer), "SW:WebLFDServerVer=%s\n", WebLFDServerVersion));
  VIM_DBG_MESSAGE("------------- WebLFD Server Version : ------------");
  VIM_DBG_STRING(temp_buffer);
  vim_strcat((VIM_CHAR *)dest_data_ptr, temp_buffer);
  VIM_DBG_STRING((const VIM_CHAR*)dest_data_ptr);


  vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));

#if 1   // RDD - TODO - FIX this 
  // Current Application Version
  vim_sprintf( temp_buffer, "SW:APP_CURRENT=V%d.%1.1d\n", BUILD_NUMBER, REVISION );
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

  // Previous Application Version
  vim_sprintf( temp_buffer, "SW:APP_PREVIOUS=V%d.%1.1d\n", terminal.sfw_prev_version, terminal.sfw_prev_minor );
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );
#endif

  // Date and Time Application was last updated
  vim_sprintf( temp_buffer, "SW:LAST CHANGED=%02d-%02d-%02d %02d:%02d\n", terminal.last_activity.day, terminal.last_activity.month, terminal.last_activity.year%2000, terminal.last_activity.hour, terminal.last_activity.minute );
  vim_strcat( (VIM_CHAR *)dest_data_ptr, temp_buffer );

  // RDD 240413 - added update to BRANDING in LFD
#if 1
  if( terminal_get_status( ss_lfd_remote_command_executed ) )
  {
	  VIM_CHAR BrandingFileData[MAX_BRANDING_STRING_LEN];
	  VIM_SIZE file_size = 0;

	  // Update Branding String (will have stripped commands so they don't get repeated after every TMS access..
	  VIM_ERROR_IGNORE( vim_file_get( COMMAND_FILE, (void *)BrandingFileData, &file_size, VIM_SIZEOF(BrandingFileData) ) );
	  if( file_size )
	  {
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, BrandingFileData );
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, "\n" );
		  // RDD 090814 added for LFD delete any Remote COMMAND data
		  vim_strcat( (VIM_CHAR *)dest_data_ptr, "COMMAND=\n" );
	  }
	  terminal_clear_status( ss_lfd_remote_command_executed );
  }
#else
  // RDD 170513 - now we have a dedicated command string so just clear this
  if( terminal_get_status( ss_lfd_remote_command_executed ) )
  {
	  vim_strcat( (VIM_CHAR *)dest_data_ptr, "COMMAND=\n" );
	  terminal_clear_status( ss_lfd_remote_command_executed );
  }
#endif
  // Pin Pad fault Status
  if( /*PinPadFaultStatus()*/1 )
  {
	tms_GetStatsString(( VIM_CHAR *)dest_data_ptr );
  }
  /* return */
  VIM_ERROR_RETURN_ON_FALSE( vim_strlen((VIM_CHAR *)dest_data_ptr)  <= max_size, VIM_ERROR_OVERFLOW );
  *data_size = vim_strlen((VIM_CHAR *)dest_data_ptr);

 // VIM_DBG_STRING(dest_data_ptr);

  return VIM_ERROR_NONE;

}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR tms_get_CTLS_version_number
(
  VIM_UINT8* dest_data_ptr,
  VIM_SIZE* data_size
)
{
  //VIM_BOOL to_close_CTLS=VIM_FALSE;
  VIM_ERROR_PTR error=VIM_ERROR_NONE;
  VIM_SIZE max_buffer_size = *data_size;

  /* init */
  *data_size = 0;
  vim_mem_clear( dest_data_ptr, max_buffer_size );

#if 0
  if( picc_handle.instance == VIM_NULL )
  {
    /* try to re-open CTLS reader */
    picc_port_configure (VIM_FALSE);
    to_close_CTLS=VIM_TRUE;
  }
#else
  picc_open( PICC_OPEN_ONLY, VIM_FALSE );
#endif

  /* TODO: no handle, return with error ?? */
  if( picc_handle.instance == VIM_NULL )
    return VIM_ERROR_NOT_FOUND;
  /* get version */
  error = vim_picc_emv_get_firmware_version (picc_handle.instance, dest_data_ptr, max_buffer_size, data_size);
  /* close reader in case */
  picc_close( VIM_TRUE );
  /* check error */
  VIM_ERROR_RETURN_ON_FAIL( error );

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tms_callback_get_APP_data
(
  /** [in] */
  VIM_UINT8 data_type,
  /** [out] */
  VIM_UINT8* dest_data_ptr,
  /** [in, out] */
  VIM_SIZE* data_size
)
{
  VIM_SIZE string_size = *data_size;
  //VIM_ERROR_PTR error;

  vim_mem_clear( dest_data_ptr, string_size );
  switch( data_type )
  {
    case 0x00:
      VIM_ERROR_RETURN_ON_FAIL(build_LFD_parameters( dest_data_ptr, string_size, data_size ));
      break;
    case 0x01:
        vim_snprintf_unterminated((VIM_CHAR *)dest_data_ptr, string_size - 1, "%8d.%2d", BUILD_NUMBER, REVISION) ;
      break;
    case 0x02:
      VIM_ERROR_RETURN_ON_FAIL( tms_get_CTLS_version_number(dest_data_ptr, data_size));
      break;
    default:
      *data_size = 0;
      break;
  }

  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR tms_download_one_file( VIM_CHAR *FileName, VIM_NET_PTR net_ptr )
{
	VIM_FILE_HANDLE tmp_file_handle;
	VIM_TMS_PARAMETERS * parameters = VIM_NULL;
	VIM_TMS_PARAMETER_WRAP vim_tms;
	VIM_ERROR_PTR error;
	VIM_CHAR PathName[30];

	vim_mem_clear( PathName, VIM_SIZEOF(PathName) );

	VIM_ERROR_RETURN_ON_FAIL( tms_get_params( parameters ) );

#ifndef _WIN32
	vim_strcat( FileName, ".ZIP" );
#endif

	/* initialize vim_tms_ptr */
	VIM_ERROR_RETURN_ON_NULL( parameters );
	vim_tms.tms_parameters_ptr = parameters;
	vim_mem_set( vim_tms.serial_number, 0x00, VIM_SIZEOF( vim_tms.serial_number));
	VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_serial_number( vim_tms.serial_number, VIM_SIZEOF(vim_tms.serial_number)-1));

	vim_mem_clear( PathName, VIM_SIZEOF( PathName ) );
	vim_strcpy( PathName, VIM_FILE_PATH_LOCAL );
	vim_strcat( PathName, FileName );

	if(( error = tms_lfd_download_one_file( net_ptr, &vim_tms, FileName, PathName, &tmp_file_handle )) != VIM_ERROR_NONE )
	{
		/* close file handle */
		VIM_DBG_WARNING( "File download Failed !!!" );
		vim_file_close( tmp_file_handle.instance );
		vim_pceftpos_debug_error( pceftpos_handle.instance, "File download Failed !!!", error );
		VIM_ERROR_RETURN(error);
	}

	if(( error = vim_tms_unzip_file( FileName )) != VIM_ERROR_NONE )
	{
		VIM_CHAR DebugBuffer[100];
		vim_sprintf( DebugBuffer, "ERROR: Unzip Failed - %s", FileName );
		vim_pceftpos_debug_error( pceftpos_handle.instance, DebugBuffer, error );
		VIM_ERROR_RETURN(error);
	}

    /* check error */
	return VIM_ERROR_NONE;
}


#define BRANDING_STRING "BRANDING="
#define COMMAND_STRING  "COMMAND="


// RDD 020413 Implemented for WOW Phase4

#define MAX_CONFIG_STRING_LEN 10

static VIM_ERROR_PTR ProcessConfig( VIM_CHAR *ConfigPtr, VIM_NET_PTR net_ptr, VIM_BOOL *Updated )
{
	//VIM_ERROR_PTR error = VIM_ERROR_NONE;
	VIM_UINT8 file_id;
	VIM_CHAR *FileNamePtr, *TempStringPtr;
	VIM_SIZE FileVersion = 0;
	VIM_CHAR TempString[MAX_CONFIG_STRING_LEN+1];	// Max contents "CPAT123456"
	//VIM_ERROR_PTR error;

	DBG_MESSAGE( " Process Config " );
	*Updated = VIM_FALSE;
	vim_mem_clear( TempString, VIM_SIZEOF( TempString ) );
	vim_mem_clear( terminal.config_filename, VIM_SIZEOF(terminal.config_filename) );

	// RDD 260315 - need to check files in case they were deleted via the congig menu etc.
	CheckFiles( VIM_FALSE );

	for( file_id=CPAT_FILE_IDX; file_id<=FCAT_FILE_IDX; file_id++ )
	{
		file_status[file_id].host_type = SWITCH_CONFIG;

		if( file_id == EPUMP_FILE_IDX ) continue;	// Don't need the EPUMP file
		if(( file_id == FCAT_FILE_IDX )&&( WOW_NZ_PINPAD )) continue;	// Don't need the FCAT file in New Zealand
		if(( ALH_PINPAD )&&( file_id==FCAT_FILE_IDX )) continue;		// RDD 161014 ALH SPIRA:8197 FCAT Not Required for ALH

		FileVersion = 0;
		DBG_STRING( file_status[file_id].FileName );
		// 'continue' here if the Version is the same as the current version
		{
			//VIM_SIZE var = bcd_to_bin ( (VIM_UINT8 *)&file_status[file_id].ped_version, WOW_FILE_VER_BCD_LEN*2 );
			//VIM_DBG_NUMBER( var );
		}

		VIM_DBG_NUMBER( terminal.file_version[file_id] );

		if(( vim_strstr( ConfigPtr, file_status[file_id].FileName, &FileNamePtr ) == VIM_ERROR_NONE ) && FileNamePtr != VIM_NULL )
		{
			// RDD 140416 Fix length
			VIM_SIZE PrefixLen = vim_strlen( file_status[file_id].FileName );
			VIM_DBG_PRINTF1( "CFG Download Request: %s", file_status[file_id].FileName );
			// Copy over 10 bytes - any extra will be ignored eg. "PKT123456!" '!' will be ignored
			vim_mem_clear( TempString, VIM_SIZEOF(TempString) );
			vim_strncpy( TempString, FileNamePtr, PrefixLen + 6 );
			TempStringPtr = TempString;

			// If the version is non-zero then we may need to get this file from LFD
			if(( ValidateFileName( &TempStringPtr, file_status[file_id].FileName, &FileVersion )) != VIM_ERROR_NONE )
			{
				VIM_DBG_PRINTF1( "Invalid Filename: %s - No Dwnld", TempStringPtr );
				continue;
			}
			VIM_DBG_PRINTF1( "Current Version: %d", terminal.file_version[file_id] );
			if( FileVersion == terminal.file_version[file_id] )
			{
				DBG_MESSAGE( "TMS config File is same as current Ver - No Dwnld" );
				continue;
			}
			VIM_DBG_PRINTF1( "Downloading %d ...", TempStringPtr );

			// The version must be different so copy the filename into the global buffer for the request, this gets copied into TMS pararms later.
			vim_strcpy( terminal.config_filename, TempStringPtr );
			DBG_STRING( terminal.config_filename );
			DBG_STRING( TempStringPtr );

			// RDD 101116 v561-3 If theres an error ( eg. TMS can't find the requested file then EXIT !!!
#if 0
			if( tms_download_one_file( terminal.config_filename, net_ptr ) != VIM_ERROR_NONE ) continue;
#else
			VIM_ERROR_RETURN_ON_FAIL( tms_download_one_file( terminal.config_filename, net_ptr ) );
#endif
			DBG_STRING( terminal.config_filename );
			vim_strcpy( terminal.config_filename, TempStringPtr );
			DBG_STRING( terminal.config_filename );

		}
		else	// No specific version number listed in the BRANDING string - poll for generic file if there is an update pending
		{
			VIM_CHAR DebugBuffer[150];
			// RDD - tricky bit - if there is no file specified then download if we're in mode !1 AND the initial logon said get the file
			// OR if there is no file at all - ie. version is all zeros
			if((( file_status[file_id].UpdateRequired ) && ( terminal_get_status(ss_lfd_then_switch_config )))||( !terminal.file_version[file_id] ))
			{
				DBG_STRING( "NVNVNVNVNV Getting Generic file NVNVNVNVNV");
				vim_strcpy( terminal.config_filename, file_status[file_id].FileName );
				vim_sprintf( DebugBuffer, "Poll for Generic File: %s", terminal.config_filename );
				// Try to get the file - if it fails continue

#if 0
				if(( error = tms_download_one_file( terminal.config_filename, net_ptr )) != VIM_ERROR_NONE )
				{
					if( error == VIM_ERROR_TMS_FILESYSTEM )
					{
						error = ERR_WOW_ERC;
						vim_sprintf( DebugBuffer, "LFD Download Failed: %s", terminal.config_filename );
						continue;
					}
				}
#else
				// RDD 101116 v561-3 Return Error if file download failed
				VIM_ERROR_RETURN_ON_FAIL( tms_download_one_file( terminal.config_filename, net_ptr ));
#endif
				pceft_debug_test( pceftpos_handle.instance, DebugBuffer );
			}

			else
			{
				vim_sprintf( DebugBuffer, "Validate RTM Config file: %s", terminal.config_filename );
				pceft_debug_test( pceftpos_handle.instance, DebugBuffer );
				continue;
			}
		}
		DBG_POINT;
		DBG_STRING( terminal.config_filename );
		DBG_VAR( FileVersion );
		// Validate the downloaded file
		// RDD 101116 v561-3 need logon to fail if error occurs otherwise things get stuffed up !
		VIM_ERROR_RETURN_ON_FAIL( ValidateConfigFile( file_id, terminal.config_filename, &FileVersion ));
		{
			DBG_MESSAGE( " $$$$$$$$$$$$$$$$$$$$$$$ VALIDATED $$$$$$$$$$$$$$$$$$$$$$$$$$");

			VIM_DBG_STRING( file_status[file_id].FileName );

			file_status[file_id].host_type = LFD_CONFIG;
			*Updated = VIM_TRUE;

			// Load parameters from config file into global data - gets the real file versions
			VIM_ERROR_RETURN_ON_FAIL(file_download_activate( file_id, VIM_TRUE ));

			VIM_DBG_NUMBER( FileVersion );
			if(( file_id == EPAT_FILE_IDX )||( file_id == PKT_FILE_IDX ))
			{
				// Re-init reader/ kernel
				terminal_set_status(ss_wow_reload_ctls_params);
			}
			// TODO - find out why the version number gets wiped by the activation process
			if(( file_id == CPAT_FILE_IDX ) && ( !terminal.file_version[file_id] ))
			{
				VIM_DBG_NUMBER( FileVersion );
				terminal.file_version[file_id] = FileVersion;
				bin_to_bcd( terminal.file_version[file_id], file_status[file_id].ped_version.bytes, WOW_FILE_VER_BCD_LEN*2 );
			}

			file_status[file_id].UpdateRequired = VIM_FALSE;
			terminal_save();
			continue;
		}

	}
	// RDD 101116 V561-3 noticed that CTLS init wasn't performed immediatly after !2
	if( terminal_get_status( ss_wow_reload_ctls_params ))
		CtlsInit( );

	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR ProcessBrandingParams( VIM_CHAR **ParamsPtr, VIM_BOOL *pb_flag )
{
	// Set ptr to point to the 3 character brand
	VIM_CHAR TempString[MAX_BRANDING_STRING_LEN];
	VIM_CHAR TempFileName[50];
	VIM_CHAR *Ptr = *ParamsPtr, *EndPtr = Ptr;
	VIM_SIZE Len = vim_strlen( Ptr );
    VIM_CHAR buf_format[16];
    VIM_UINT8 format_len;
    //VIM_SIZE FileSize = 0;
    //VIM_FILE_HANDLE file_handle;
	DBG_VAR( Len );

	if( Len > MAX_BRANDING_STRING_LEN )
	{
		VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
	}

	// Clear the TempString and copy over the first
	vim_mem_clear( TempString, VIM_SIZEOF( TempString ) );
	vim_mem_clear( TempFileName, VIM_SIZEOF(TempFileName) );
	vim_strcpy( TempString, Ptr );

	// Point to the start of the actual branding format: XXXYYYZZZ.. :	where XXX=brand file name, YYY=division, ZZZ.. =operating mode(s)
	Ptr+=vim_strlen(BRANDING_STRING);

	// Create the brand filename
    display_get_image_format(buf_format, &format_len);
    vim_sprintf( TempFileName, "%3.3s.", Ptr );
    vim_strncat(TempFileName, buf_format, format_len);

	// RDD 070313 SPIRA 6638 - ensure that ui file not present returns different error to its embedded pcx file not present
	{
		//VIM_BOOL exist = VIM_FALSE;
		VIM_CHAR Message[100];
		vim_mem_clear( Message, VIM_SIZEOF(Message) );

        VIM_DBG_WARNING("Branding Stuff:");
        if (!vim_file_is_present(TempFileName))
        {
            VIM_DBG_PRINTF1("BrandFile: %s", TempFileName);
            // if the filename doesn't exist then report an error to the POS and continue to use the last Branding file (if any)
			vim_sprintf( Message, "Branding Error: %s not present. Using WPY.png", &TempFileName[2] );
            pceft_debug( pceftpos_handle.instance, Message );
            VIM_DBG_WARNING( " ---------------- Set Branding to NoBrand ---------------" );
            vim_strcpy(terminal.configuration.BrandFileName, DEFAULT_BRAND_FILE);
        }
		else
		{
			pceft_debugf(pceftpos_handle.instance, "Old Branding: %s", terminal.configuration.BrandFileName);
			vim_strcpy(terminal.configuration.BrandFileName, TempFileName);
			pceft_debugf(pceftpos_handle.instance, "New Branding: %s", terminal.configuration.BrandFileName);
            VIM_DBG_PRINTF1("BrandFile: %s", TempFileName);
		}
	}
	// RDD 070313 SPIRA 6638 - report branding via Z debug to allow config controller to check that its working correctly
	terminal_set_status( ss_report_branding_status );

	// Set PinPad Optional Test Modes
	if( ( vim_strstr( Ptr, WOW_TEST_MODE, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		terminal_set_status(ss_wow_basic_test_mode);
		DBG_MESSAGE( "!!!! XXX MODE SET !!!! " );
	}
    else
    {
        terminal_clear_status(ss_wow_basic_test_mode);
        DBG_MESSAGE("!!!! XXX MODE CLEARED !!!! ");
    }

	// Check to see if there are post-branding parameters and truncate the string accordingly
	if(( vim_strchr( Ptr, FS_LFD_BRANDING, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		// field separator detected so there are post-brading parameters set
		*pb_flag = VIM_TRUE;
		*ParamsPtr = ++EndPtr;
	}
	// RDD 040518 added
	terminal_save();

	// If Post-branding parames were detected shift the input pointer to the relevent spot
	return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR ProcessRemoteCommand( VIM_CHAR *pbPtr )
{
	VIM_CHAR *EndPtr;
	VIM_CHAR Password[10];
	VIM_SIZE Len=0;

	vim_mem_clear( Password, VIM_SIZEOF( Password ) );
	if( ( vim_strchr( pbPtr, FS_LFD_FUNCTION, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		VIM_CHAR DisplayString[50];

		vim_mem_clear( DisplayString, VIM_SIZEOF( DisplayString ) );
		pbPtr = ++EndPtr;
		Len = 0;
		while( VIM_ISDIGIT( *pbPtr ))
		{
			Password[Len++] = *pbPtr;
			pbPtr++;
		}

		if( vim_strlen( pbPtr ) < VIM_SIZEOF(DisplayString) )
		{
			vim_strcpy( DisplayString, pbPtr );
		}

		// RDD 110413 - allow display for these functions to be sent to POS - may need to change this...
		terminal.initator = it_pceftpos;
		//terminal.initator = it_pinpad;

		terminal_set_status( ss_lfd_remote_command_executed );

		// RDD 250513 - SPIRA:6747 Need to do TMS again to clear the command string
//BD		terminal_clear_status( ss_tms_configured );						//BD done higher based on ss_lfd_remote_command_executed
//BD		   DBG_MESSAGE( "ss_tms_configured CLEARED pre RunFunc" );
		RunFunctionMenu( asc_to_bin( Password, Len ), VIM_TRUE, DisplayString );
	}
	return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR ProcessPostBrandingParams( VIM_CHAR *ptr, VIM_NET_PTR net_ptr )
{
	VIM_CHAR *pbPtr = ptr;
	VIM_CHAR *EndPtr = ptr;
	VIM_BOOL Updated = VIM_FALSE;
	VIM_SIZE Len=0;

	// RDD 030513 added for P4 - value of 0 denotes delete remaining SAF prints and don't store any more
	if( vim_strstr( pbPtr, SFP, &EndPtr ) == VIM_ERROR_NONE )
	{
			// RDD - beware of mal-formatted strings
			EndPtr += WOW_PAD_STRING_TAG_LEN;
			if( *EndPtr == '0' )
			{
				terminal_set_status( ss_no_saf_prints );
				DeleteSafPrints();
			}
			else if( *EndPtr == '1' )
				terminal_clear_status( ss_no_saf_prints );
			// else - leave it as it was
	}

	// Check to see if there is a terminal config string and truncate the string accordingly
	if( ( vim_strchr( pbPtr, FS_LFD_CONFIG, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		// If there was a file update and the flag is set to ignore what the switch tells us then set the internal flag
		if( *++EndPtr == WOW_LFD_CONFIG_ONLY )
		{
			terminal_set_status( ss_lfd_only_config );
		}
		else if( *EndPtr == WOW_LFD_THEN_SWITCH )
		{
			terminal_set_status( ss_lfd_then_switch_config );
		}
		ProcessConfig( EndPtr, net_ptr, &Updated );
	}

	if( ( vim_strchr( pbPtr, FS_LFD_EGC, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		// reset the length after removing the training flags

		pbPtr = ++EndPtr;
		Len = 0;
		while( VIM_ISDIGIT( *pbPtr ))
		{
			Len++;
			pbPtr++;
		}

		// Copy over New EGC Bin
		if(( Len >= CARD_MIN_BIN_LEN )&& ( Len <= CARD_MAX_BIN_LEN ))
		{
			// if the new value is different then load and save
			if( vim_strncmp( pbPtr, terminal.ecg_bin, Len ) )
			{
				vim_mem_clear( terminal.ecg_bin, VIM_SIZEOF( terminal.ecg_bin ));
				vim_strncpy( terminal.ecg_bin, EndPtr, Len );
			}
		}
	}

	// RDD 270714 SPIRA:8018 If Remote command not working (old DLL) - use the old method from within the branding
	ProcessRemoteCommand( pbPtr );
    terminal_save();
    return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR SaveBrandingData( VIM_CHAR *pbPtr )
{
	VIM_CHAR BrandingFileData[MAX_BRANDING_STRING_LEN];
	VIM_CHAR *EndPtr, *Ptr;

	vim_mem_clear( BrandingFileData, VIM_SIZEOF( BrandingFileData ) );
	vim_strcpy( BrandingFileData, pbPtr );

	if( ( vim_strchr( BrandingFileData, FS_LFD_FUNCTION, &EndPtr ) == VIM_ERROR_NONE ) && EndPtr != VIM_NULL )
	{
		Ptr = EndPtr;
		while( VIM_ISALPHA( *++Ptr ));
		vim_strcpy( EndPtr, Ptr );
	}

	VIM_ERROR_RETURN_ON_FAIL( vim_file_set( COMMAND_FILE, (const void *)BrandingFileData, VIM_SIZEOF(BrandingFileData) ) );

	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR GetElementData(const VIM_CHAR *ElementPtr, const VIM_CHAR *SearchString, VIM_CHAR *DataPtr, VIM_SIZE *DataLen, VIM_SIZE MaxSize)
{
    VIM_CHAR *Ptr;
    //VIM_BOOL CfgFilesUpdated;

    vim_mem_clear(DataPtr, MaxSize);
    if (vim_strstr((VIM_CHAR *)ElementPtr, SearchString, &Ptr) == VIM_ERROR_NONE && Ptr != VIM_NULL)
    {
        VIM_CHAR *Ptr2;
      //  DBG_POINT;
        if (*(Ptr - 1) == '"')
        {
            // WebLFD eg. "@NEW_VAS": "1",
       //     VIM_DBG_STRING(Ptr);
            if (vim_strchr(Ptr, WEB_LFD_DATA_FS, &Ptr2) == VIM_ERROR_NONE && Ptr2 != VIM_NULL)
            {
                VIM_CHAR *Ptr3 = VIM_NULL;
                //VIM_DBG_STRING(Ptr2);
                // Skip space and quotes to point to data
                Ptr2 += 3;
                // Find end of data and terminate
                if (vim_strchr(Ptr2, '"', &Ptr3) == VIM_ERROR_NONE && Ptr3 != VIM_NULL)
                {
                    // Found terminator
                   // VIM_DBG_STRING(Ptr3);
                   // VIM_DBG_STRING(Ptr2);

                    *DataLen = Ptr3 - Ptr2;
                    //VIM_DBG_NUMBER(*DataLen);
                    if (*DataLen && *DataLen < 100)
                    {
                        vim_strncpy(DataPtr, Ptr2, *DataLen);
                        //VIM_DBG_STRING(DataPtr);
                        return VIM_ERROR_NONE;
                    }
                }
            }
        }

        // Found this key string but does it have any data ?
        else if (vim_strchr(Ptr, DATA_FS, &Ptr2) == VIM_ERROR_NONE && Ptr != VIM_NULL)
        {
            // OldLFD eg. @NEW_VAS = 1
            ++Ptr2;
            if ((*DataLen = vim_strlinelen(Ptr2)) > 0)
            {
                vim_strncpy(DataPtr, Ptr2, *DataLen);
                return VIM_ERROR_NONE;
            }
        }
    }
    return VIM_ERROR_NOT_FOUND;
}

#if 0
static VIM_BOOL fCheckFile(VIM_CHAR *psFileName)
{
    VIM_CHAR TempFileName[50];
    VIM_BOOL Exists = VIM_FALSE;

    vim_mem_clear(TempFileName, sizeof(TempFileName));
    vim_sprintf(TempFileName, "~/%s", psFileName);

    vim_file_exists(TempFileName, &Exists);
    return Exists;
}
#endif





static VIM_ERROR_PTR set_APP_parameters
(
  const VIM_UINT8* DataPtr,
  VIM_SIZE data_size,
  VIM_NET_PTR net_ptr
)
{

  VIM_CHAR ParamsBuffer[2000];
  VIM_CHAR* data_ptr = ParamsBuffer;
  VIM_TEXT temp_buffer[100];
  VIM_CHAR* ptr;
  VIM_SIZE start_index=0, current_index=0;

  // RDD 240223 JIRA PIRPIN-2240 make a copy buffer to avoid corrupt vim files
  vim_mem_clear(ParamsBuffer, VIM_SIZEOF(ParamsBuffer));
  vim_mem_copy(ParamsBuffer, DataPtr, data_size);

  // RDD 030512 SPIRA:5405 Set Test mode
  //terminal_clear_status(ss_wow_pre_insert_mode);
  //terminal_clear_status(ss_wow_basic_test_mode);

  // RDD 230513 SPIRA:
  vim_strcpy( terminal.ecg_bin, DEFAULT_EGC_BIN );

  // RDD 240413 - set to download config file from only switch by default

  // PHASE6
  //terminal_clear_status( ss_lfd_then_switch_config );
  //terminal_clear_status( ss_lfd_only_config );

#if 1	// RDD 270714 doens't work here as we need the sub-string
if (vim_strstr((VIM_CHAR*)data_ptr, COMMAND_STRING, &ptr) == VIM_ERROR_NONE && ptr != VIM_NULL)
{
    DBG_MESSAGE("Found Command String");
    ProcessRemoteCommand(ptr);
}
#endif

// RDD 010814 Process the new ALH LFD Configuration Parameters
VIM_ERROR_RETURN_ON_FAIL(ParseRegOptions((VIM_CHAR*)data_ptr, NumParams()));

DBG_MESSAGE("========= In Set App Params =========");
while (current_index < data_size)
{
    if (data_ptr[current_index] == 0x0a)
    {
        VIM_BOOL pb_flag = VIM_FALSE;
        ptr = VIM_NULL;
        /* find one record */
        vim_mem_clear(temp_buffer, VIM_SIZEOF(temp_buffer));
        vim_mem_copy(temp_buffer, data_ptr + start_index, VIM_MIN(current_index - start_index, VIM_SIZEOF(temp_buffer) - 1));
        /* get file name */
        DBG_DATA(temp_buffer, VIM_MIN(current_index - start_index, VIM_SIZEOF(temp_buffer) - 1));

#if 1

        if (vim_strstr(temp_buffer, BRANDING_STRING, &ptr) == VIM_ERROR_NONE && ptr != VIM_NULL)
        {
            // RDD 240413 cleverly process the Branding string minus any command data and store it in a file
            SaveBrandingData(ptr);
            VIM_DBG_PRINTF1("New Branding: %s", ptr);

            // RDD 020413 - Changes for handling extra params in branding string
            VIM_ERROR_RETURN_ON_FAIL(ProcessBrandingParams(&ptr, &pb_flag));
            // RDD 040123 Branding now affects card table - QA will change branding so assume card table needs top change too
            CreateCardTable();
            pceft_debugf(pceftpos_handle.instance, "Using Card Table from: %s", TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "CARDS.XML"));

        }
#endif
        // RDD 020413 - check to see if there is any post-branding data (follows FS char)
        if (pb_flag)
        {
            VIM_ERROR_RETURN_ON_FAIL(ProcessPostBrandingParams(ptr, net_ptr));
        }

        /* next */
        start_index = ++current_index;
    }
    else
        current_index++;
}

// RDD 101116 V561-3 noticed that CTLS init wasn't performed immediatly after !2
if (terminal_get_status(ss_wow_reload_ctls_params))
CtlsInit();

terminal_save();
return VIM_ERROR_NONE;
}

VIM_ERROR_PTR vim_tms_callback_set_APP_data
(
    VIM_UINT8 data_type,
    VIM_UINT8 const* data_value_ptr,
    VIM_SIZE data_size,
    VIM_NET_PTR net_ptr
)
{
    //VIM_DBG_DATA( data_value_ptr, data_size );
    switch (data_type)
    {
    case 0x00:
        VIM_ERROR_RETURN_ON_FAIL(set_APP_parameters(data_value_ptr, data_size, net_ptr));
        break;
    case 0x01:
        if (data_value_ptr[0] == '1')
        {
            // RDD 301115 comment this out as can cause unwanted blocking of terminal
            //terminal_clear_status( ss_tms_configured );
        }
        else if (data_value_ptr[0] == '0')
        {
            //terminal_set_status( ss_tms_configured );
            DBG_MESSAGE("ss_tms_configured SET tms contact");		//BD These seem to be in conflict with line 190 ???
        }
        break;
    default:
        break;
    }
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR vim_tms_callback_pceft_debug(VIM_CHAR* DebugText)
{
    //VIM_ERROR_RETURN_ON_FAIL( pceft_debug( pceftpos_handle.instance, DebugText ));
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR tms_vhq_callback_get_app_state(vim_vhq_app_state_t* app_state)
{
    VIM_DBG_MESSAGE("VHQ Driver Called : tms_vhq_callback_get_app_state");
    *app_state = terminal_get_status(ss_locked) ? vim_vhq_app_state_busy : vim_vhq_app_state_free;
    if(*app_state == vim_vhq_app_state_busy)
    {
        pceft_debug_test(pceftpos_handle.instance, "* vhq call delayed *");
    }    
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR tms_vhq_open(void)
{
	VIM_CHAR AgentStatus[100];
	vim_mem_clear(AgentStatus, VIM_SIZEOF(AgentStatus));

	terminal_clear_status(ss_locked);
	VIM_ERROR_RETURN(vim_tms_vhq_open(APPNAME, AgentStatus));
}

VIM_ERROR_PTR tms_vhq_open_v(void)
{
	VIM_CHAR AgentStatus[100];
	vim_mem_clear(AgentStatus, VIM_SIZEOF(AgentStatus));

	vim_tms_vhq_open(APPNAME, AgentStatus);

	display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, AgentStatus);
	vim_event_sleep(5000);
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR tms_vhq_contact(VIM_INT16 *ResultPtr)
{
	VIM_ERROR_RETURN( vim_tms_vhq_contact_server( 10000, ResultPtr ));
}


VIM_ERROR_PTR tms_vhq_heartbeat( VIM_BOOL SilentMode )
{
	VIM_INT16 HeartbeatResult = -1;
	VIM_CHAR Buffer[100];
	vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

	pceft_debug(pceftpos_handle.instance, "VHQ Contacting Server, Please Wait...");
	if (!SilentMode)
	{
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "VHQ\nContacting Server\n", "Please Wait...");
	}
	if (tms_vhq_contact(&HeartbeatResult) == VIM_ERROR_NONE)
	{
		vim_sprintf(Buffer, "VHQ Success [%d]", HeartbeatResult);
	}
	else
	{
		vim_sprintf(Buffer, "VHQ Result Code:[%d]", HeartbeatResult);
	}
	if (!SilentMode)
	{
		vim_event_sleep(1000);
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Completed\n", Buffer );
		vim_event_sleep(2000);
	}
	pceft_debugf(pceftpos_handle.instance, "TMS Initiated VHQ Heartbeat. Result: %s", Buffer);
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR tms_vhq_heartbeat_v(void)
{
	return tms_vhq_heartbeat(VIM_FALSE);
}



VIM_ERROR_PTR tms_vhq_decommission(void)
{
	VIM_ERROR_RETURN(vim_tms_vhq_close());
}







