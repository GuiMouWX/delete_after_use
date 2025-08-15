
//  ***************  RDD 031220 Add New File for Table Based TMS Options Processing ****************

#include <incs.h>
#include <tms.h>

#ifndef _WIN32
#include <cJSON.h>
#endif

extern VIM_ERROR_PTR GetElementData(const VIM_CHAR *ElementPtr, const VIM_CHAR *SearchString, VIM_CHAR *DataPtr, VIM_SIZE *DataLen, VIM_SIZE MaxSize);

extern VIM_ERROR_PTR vim_xml_test_parse_buffer(VIM_CHAR *Input, VIM_CHAR *Output, VIM_SIZE *num_params);
extern VIM_ERROR_PTR ProcessBrandingParams(VIM_CHAR **ParamsPtr, VIM_BOOL *pb_flag);
extern VIM_ERROR_PTR tms_vhq_contact(VIM_INT16 *ResultPtr);

extern VIM_ERROR_PTR vim_xml_test_parse_buffer(VIM_CHAR *Input, VIM_CHAR *Output, VIM_SIZE *num_params);
void Wally_BuildJSONRequestOfRegistrytoFIle(void);


extern const VIM_TEXT EDPData[];

static VIM_SIZE FunctionMenu = 0;
static VIM_SIZE Covid19CVMLimit = 0;

const VIM_TEXT VHQ_Params[] = VIM_FILE_PATH_LOCAL "vhq_params.xml";



// RDD : Create a Table of all possible Parameters

TMS_CONFIG_OPTION sTMSConfigOptions[] = { 

    // RDD 140923 JIRA PIRPIN_2712 Countdown -> Woolworths
    {"@NZ_WOW", _FlagOpt, &TERM_NZ_WOW, VIM_SIZEOF(TERM_NZ_WOW), NUMBER_8_BIT, "0"},

    {"@USE_DE38", _FlagOpt, &TERM_USE_DE38, VIM_SIZEOF(TERM_USE_DE38), NUMBER_8_BIT, "0"},
    {"@Cashout_Max", _6DigitLimitOpt, &TERM_CASHOUT_MAX, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "999999" },
    {"@USE_RNDIS", _FlagOpt, &TERM_USE_RNDIS, VIM_SIZEOF(TERM_USE_RNDIS), NUMBER_8_BIT, "0"},
    {"@QR_WALLET",_FlagOpt, &TERM_QR_WALLET, VIM_SIZEOF(VIM_UINT8), FLAG, "0"},

    {"@CPAT", _FileOpt, &TERM_CPAT_FILENAME, VIM_SIZEOF(TERM_CPAT_FILENAME), _TEXT, "0" },
    {"@EPAT", _FileOpt, &TERM_EPAT_FILENAME, VIM_SIZEOF(TERM_EPAT_FILENAME), _TEXT, "0" },
    {"@PKT", _FileOpt, &TERM_PKT_FILENAME, VIM_SIZEOF(TERM_PKT_FILENAME), _TEXT, "0" },

    // RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS
    {"@ENABLE_TFB", _FlagOpt, &TERM_TECHNICAL_FALLBACK_ENABLE, VIM_SIZEOF(TERM_TECHNICAL_FALLBACK_ENABLE), FLAG, "1" },

	// Surcharge
	{"@SC_POINTS_VISA",_StringOpt, &TERM_SC_POINTS_VISA, VIM_SIZEOF(TERM_SC_POINTS_VISA), _TEXT, "0" },
	{"@SC_POINTS_MC",_StringOpt, &TERM_SC_POINTS_MC, VIM_SIZEOF(TERM_SC_POINTS_MC), _TEXT, "0" },
	{"@SC_POINTS_AMEX",_StringOpt, &TERM_SC_POINTS_AMEX, VIM_SIZEOF(TERM_SC_POINTS_AMEX),_TEXT, "0" },
	{"@SC_POINTS_UPI",_StringOpt, &TERM_SC_POINTS_UPI, VIM_SIZEOF(TERM_SC_POINTS_UPI),_TEXT, "0" },
	{"@SC_POINTS_DINERS",_StringOpt, &TERM_SC_POINTS_DINERS, VIM_SIZEOF(TERM_SC_POINTS_DINERS),_TEXT, "0" },
	{"@SC_POINTS_EFTPOS",_StringOpt, &TERM_SC_POINTS_EFTPOS, VIM_SIZEOF(TERM_SC_POINTS_EFTPOS), _TEXT, "0" },
	{"@SC_POINTS_JCB",_StringOpt, &TERM_SC_POINTS_JCB, VIM_SIZEOF(TERM_SC_POINTS_JCB),_TEXT, "0" },
	{"@SC_ON_TIP",_FlagOpt, &TERM_SC_ON_TIP, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
	{"@SC_SUBTRACT_CASHOUT",_FlagOpt, &TERM_SC_EXCLUDE_CASH, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },


    // RDD 150922 Added new params for EDP stuff
    {"@QR_REVERSAL_TIMER",_6DigitLimitOpt, &TERM_QR_REVERSALTIMER,  VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "3" },                    // Default from CPAT
    {"@QR_UPDATE_PAY_TIMEOUT",_6DigitLimitOpt, &TERM_QR_UPDATEPAY_TIMEOUT,  VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "30" },            // Default from CPAT

	// Branding - added for VHQ
	{"@BRANDING", _StringOpt, &TERM_VHQ_BRANDING, VIM_SIZEOF(TERM_VHQ_BRANDING), _TEXT, "0" },
	{"@VHQ_ONLY", _FlagOpt, &TERM_OPTION_VHQ_ONLY, VIM_SIZEOF(TERM_OPTION_VHQ_ONLY), FLAG, "0" },


	// Manpan password
	{"@ManPan_Password",_12DigitLimitOpt, &TERM_MANPAN_PASSWORD, VIM_SIZEOF(VIM_UINT32), NUMBER_64_BIT, "0" },   // Manual entry Password - default = DISABLED

	// Wifi Config via RTM
	{"@USE_WIFI", _2DigitLimitOpt, &TERM_USE_WIFI, VIM_SIZEOF(TERM_USE_WIFI), NUMBER_8_BIT, "0" },
    //{"@WIFI_SSID",_StringOpt, &TERM_WIFI_SSID, VIM_SIZEOF(TERM_WIFI_SSID), _TEXT, "0" },
    //{"@WIFI_PWD",_StringOpt, &TERM_WIFI_PWD, VIM_SIZEOF(TERM_WIFI_PWD), _TEXT, "0" },
    //{"@WIFI_PROT",_StringOpt, &TERM_WIFI_PROT, VIM_SIZEOF(TERM_WIFI_PROT), _TEXT, "0" },


	// don't allow permanent session - disconnect after each (0200) transaction
    {"@ALLOW_DISCONNECT", _FlagOpt, &TERM_OPTION_PCEFT_DISCONNECT_ENABLED, VIM_SIZEOF(TERM_OPTION_PCEFT_DISCONNECT_ENABLED), FLAG, "1" },

    // RDD 030123 JIRA PIRPIN-2113 Move Host IDs to become RTM Paramters
    {"@SNI_ODD",_StringOpt, &TERM_SNI_ODD, VIM_SIZEOF(TERM_SNI_ODD), _TEXT, "0" },
    {"@SNI_EVEN",_StringOpt, &TERM_SNI_EVEN, VIM_SIZEOF(TERM_SNI_EVEN), _TEXT, "0" },
    {"@HOST_URL",_StringOpt, &TERM_HOST_URL, VIM_SIZEOF(TERM_HOST_URL), _TEXT, "0" },


	// Surcharge
	{"@SC_POINTS_VISA",_StringOpt, &TERM_SC_POINTS_VISA, VIM_SIZEOF(TERM_SC_POINTS_VISA), _TEXT, "0" },
	{"@SC_POINTS_MC",_StringOpt, &TERM_SC_POINTS_MC, VIM_SIZEOF(TERM_SC_POINTS_MC), _TEXT, "0" },
	{"@SC_POINTS_AMEX",_StringOpt, &TERM_SC_POINTS_AMEX, VIM_SIZEOF(TERM_SC_POINTS_AMEX),_TEXT, "0" },
	{"@SC_POINTS_UPI",_StringOpt, &TERM_SC_POINTS_UPI, VIM_SIZEOF(TERM_SC_POINTS_UPI),_TEXT, "0" },
	{"@SC_POINTS_DINERS",_StringOpt, &TERM_SC_POINTS_DINERS, VIM_SIZEOF(TERM_SC_POINTS_DINERS),_TEXT, "0" },
	{"@SC_POINTS_EFTPOS",_StringOpt, &TERM_SC_POINTS_EFTPOS, VIM_SIZEOF(TERM_SC_POINTS_EFTPOS), _TEXT, "0" },
	{"@SC_POINTS_JCB",_StringOpt, &TERM_SC_POINTS_JCB, VIM_SIZEOF(TERM_SC_POINTS_JCB),_TEXT, "0" },
	{"@SC_ON_TIP",_FlagOpt, &TERM_SC_ON_TIP, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
	{"@SC_SUBTRACT_CASHOUT",_FlagOpt, &TERM_SC_EXCLUDE_CASH, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },

    // DE47 Encryption
    // RDD 211021 Enable DE47 encryption by default for v728 +
    //{"@ENCRYPT_DE47", _FlagOpt, &TERM_ENCRYPT_DE47, VIM_SIZEOF(TERM_ENCRYPT_DE47), FLAG, "1" },
    // HOST Disconnect after each transaction
    //{"@HOST_DISCONNECT",_FlagOpt, &TERM_OPTION_PCEFT_DISCONNECT_ENABLED, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },


    // Standin Management
    {"@SAF_TXNS_MAX",_6DigitLimitOpt, &TERM_MAX_SAFS,  VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "199"},                    // Default from CPAT
    {"@SAF_TOTAL_LIMIT",_12DigitLimitOpt, &TERM_SAF_TOTAL_LIMIT, VIM_SIZEOF(VIM_UINT64), NUMBER_64_BIT, "200000" },
    {"@SAF_TXN_CREDIT_LIMIT", _6DigitLimitOpt, &TERM_EFB_CREDIT_LIMIT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "90"},   // Default from CPAT
    {"@SAF_TXN_DEBIT_LIMIT", _6DigitLimitOpt, &TERM_EFB_DEBIT_LIMIT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "200"},    // Default from CPAT

    // Refund Management
    {"@REFUND_DAILY_LIMIT",_6DigitLimitOpt, &TERM_REFUND_DAILY_LIMIT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "999999" },
    {"@REFUND_TXN_LIMIT",_6DigitLimitOpt, &TERM_REFUND_TXN_LIMIT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "999999" },
    {"@REFUND_PASSWORD", _PasswordOpt, &TERM_REFUND_PASSWORD, VIM_SIZEOF(TERM_REFUND_PASSWORD), _TEXT, "0" },

    // MOTO
    {"@ALLOW_MOTO", _FlagOpt, &TERM_ALLOW_MOTO_ECOM, VIM_SIZEOF(TERM_ALLOW_MOTO_ECOM), FLAG, "0" },
    {"@MOTO_PASSWORD", _PasswordOpt, &TERM_MOTO_PASSWORD, VIM_SIZEOF(TERM_MOTO_PASSWORD), _TEXT, "0" },
    {"@CCV_Entry", _StringOpt, &TERM_CCV_ENTRY, VIM_SIZEOF(TERM_CCV_ENTRY), _CHAR, "0" },

    // Tipping
    {"@ALLOW_TIP", _2DigitLimitOpt, &TERM_ALLOW_TIP, VIM_SIZEOF(TERM_ALLOW_TIP), NUMBER_8_BIT, "0" },
    {"@TIP_MAX_PERCENT", _2DigitLimitOpt, &TERM_TIP_MAX_PERCENT, VIM_SIZEOF(TERM_TIP_MAX_PERCENT), NUMBER_8_BIT, "0" },


    // Disable schemes
    {"@AMEX_DISABLE", _FlagOpt, &TERM_AMEX_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG,"0" },
    {"@DINERS_DISABLE", _FlagOpt, &TERM_DINERS_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
    {"@UPI_DISABLE", _FlagOpt, &TERM_UPI_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
    {"@JCB_DISABLE", _FlagOpt, &TERM_JCB_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
    {"@QC_GIFT_DISABLE", _FlagOpt, &TERM_QC_GIFT_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },		// RDD 030123 Set Default to "0" ( not disabled ) for safety
    {"@WEX_GIFT_DISABLE", _FlagOpt, &TERM_WEX_GIFT_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "1" },     // WEX effectively obsolete now
    {"@QPS_DISABLE", _FlagOpt, &TERM_QPS_DISABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },

    // Merchant Routing
    {"@MCR", _FlagOpt, &TERM_USE_MERCHANT_ROUTING, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },
    {"@MCR_MC_HIGH",_6DigitLimitOpt, &TERM_MCR_MC_HIGH,  VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "0" },
    {"@MCR_VISA_HIGH",_6DigitLimitOpt, &TERM_MCR_VISA_HIGH, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "0" },
    {"@MCR_MC_LOW",_6DigitLimitOpt, &TERM_MCR_MC_LOW,  VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "0" },
    {"@MCR_VISA_LOW",_6DigitLimitOpt, &TERM_MCR_VISA_LOW, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "0" },

	// EGC BIN ( Electronic Gift Card keyed entry ) 
	{"@EGC_BIN",_StringOpt, &terminal.ecg_bin, VIM_SIZEOF(terminal.ecg_bin)-1, _TEXT, "628000555" },

    // Original Parameters
    {"@PCI_TOKENS",_FlagOpt, &TERM_USE_PCI_TOKENS, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },                  // Define a non-Woolworths Group Terminal      
    {"@NO_GIFT_ON_EFT",_FlagOpt, &TERM_NO_EFT_GIFT, VIM_SIZEOF(VIM_UINT8), FLAG, "1" },                 // Throw an error if Gift Card swiped on EFT ( no WWG tag )      
    {"@RETURNS_NEW_FLOW", _FlagOpt, &TERM_RETURNS_CARD_NEW_FLOW, VIM_SIZEOF(TERM_RETURNS_CARD_NEW_FLOW), FLAG, "0" },


    {"@BALANCE_RECEIPT",_FlagOpt, &TERM_ALLOW_BALANCE_RECEIPT, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },      // Allow Balance receipt
  
    {"@CTLSCASH",_FlagOpt, &TERM_CTLS_CASHOUT, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },                      // Allow Cashout on CTLS

    // Enable Optional Transaction Types
    {"@Pre_Auth_Enabled",_FlagOpt, &TERM_ALLOW_PREAUTH, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },            // Allow Pre-auth
    {"@CHECKOUT_ENABLED",_FlagOpt, &TERM_CHECKOUT_ENABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },          // Allow Check-out transaction
    {"@VOID_ENABLED",_FlagOpt, &TERM_VOID_ENABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },                  // Allow Void
    {"@REFUND_ENABLED",_FlagOpt, &TERM_REFUND_ENABLE, VIM_SIZEOF(VIM_UINT8), FLAG, "1" },              // Allow Refund

    // Enable Short Receipt ( requires pronting on POS using 50 col paper )
    {"@SHORT_RECEIPT",_FlagOpt, &TERM_SHORT_RECEIPT, VIM_SIZEOF(VIM_UINT8), FLAG, "0" },               // Allow Refund
    
    // DE55 in EMV refunds
    {"@MC_EMV_REFUND",_FlagOpt, &ENABLE_MC_EMV_REFUNDS, VIM_SIZEOF(ENABLE_MC_EMV_REFUNDS), FLAG, "0" },    // Allow MC EMV Refund
    {"@VISA_EMV_REFUND",_FlagOpt, &ENABLE_VISA_EMV_REFUNDS, VIM_SIZEOF(ENABLE_VISA_EMV_REFUNDS), FLAG, "0" },    // Allow MC EMV Refund
    {"@EMV_REFUND_CPAT_PIN",_FlagOpt, &EMV_REFUND_CPAT_OVERRIDE, VIM_SIZEOF(EMV_REFUND_CPAT_OVERRIDE), FLAG, "1" },    // Allow MC EMV Refund

	
	// RDD 290322 JIRA PIRPIN-1521 
	{ "@TXN_RESULT_DISPLAY_TIME", _3DigitLimitOpt, &TERM_DISPLAY_RESULT_SECS, VIM_SIZEOF(VIM_UINT8), NUMBER_8_BIT, "2" },

    // Reboot control 
    {"@PRE_PCI_TIME",_6DigitLimitOpt, &TERM_PRE_PCI_REBOOT_CHECK, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "600" },

    // COVID19 Special CVM limit - $999 limit
    {"@CUSTOM_CVM_LIMIT",_3DigitLimitOpt, &Covid19CVMLimit , VIM_SIZEOF(Covid19CVMLimit), NUMBER_32_BIT, "0" },

    // New LFD - RDD 081021 agreed with MB to change this back to ON by default
    {"@NEW_LFD",_FlagOpt, &TERM_NEW_LFD, VIM_SIZEOF(TERM_NEW_LFD), FLAG, "1" },
    // New VAS
    {"@NEW_VAS",_FlagOpt, &TERM_NEW_VAS, VIM_SIZEOF(TERM_NEW_VAS), FLAG, "1" },

    // Function menu access
    {"@FUNCTION",_6DigitLimitOpt, &FunctionMenu, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "0" },   // Activate function menu remotely
     // Disable extra XML Tags
    {"@FORBID_XML_OPT",_FlagOpt, &TERM_FORBID_XML_OPT, VIM_SIZEOF(TERM_FORBID_XML_OPT), FLAG, "0" },

  
    // EDP Parameters
	// RDD 180722 JIRA PIRPIN-1749 adjust defaults to reflect current prod values
    {"@QR_PS_TIMEOUT",_6DigitLimitOpt, &TERM_QR_PS_TIMEOUT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "86400" },
    {"@QR_QR_TIMEOUT",_6DigitLimitOpt, &TERM_QR_QR_TIMEOUT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "270" },
    {"@QR_IDLE_SCAN_TIMEOUT",_6DigitLimitOpt, &TERM_QR_IDLE_SCAN_TIMEOUT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "30" },
    {"@QR_WALLY_TIMEOUT",_6DigitLimitOpt, &TERM_QR_WALLY_TIMEOUT, VIM_SIZEOF(VIM_UINT32), NUMBER_32_BIT, "31" },
    {"@QR_STORE_LOC", _StringOpt, &TERM_QR_LOCATION, VIM_SIZEOF(TERM_QR_LOCATION), _TEXT, DEFAULT_QR_LOCATION },
    {"@QR_REWARDS_POLL", _2DigitLimitOpt, &TERM_QR_REWARDS_POLL, VIM_SIZEOF(VIM_UINT8), NUMBER_8_BIT, "0" },
    {"@QR_RESULTS_POLL", _2DigitLimitOpt, &TERM_QR_RESULTS_POLL, VIM_SIZEOF(VIM_UINT8), NUMBER_8_BIT, "0" },
    {"@QR_POLLBUSTER",_FlagOpt, &TERM_QR_POLLBUSTER, VIM_SIZEOF(VIM_UINT8), FLAG, "1" },
    {"@QR_TENDER_ID", _StringOpt, &TERM_QR_TENDER_ID, VIM_SIZEOF(TERM_QR_TENDER_ID), _TEXT, DEFAULT_QR_TENDER_ID },
    {"@QR_TENDER_NAME", _StringOpt, &TERM_QR_TENDER_NAME, VIM_SIZEOF(TERM_QR_TENDER_NAME), _TEXT, DEFAULT_QR_TENDER_NAME },
    {"@QR_DECLINED_RCT_TEXT", _StringOpt, &TERM_QR_DECLINED_RCT_TEXT, VIM_SIZEOF(TERM_QR_DECLINED_RCT_TEXT), _TEXT, DEFAULT_QR_DECLINED_RCT_TEXT },
    {"@QR_MERCH_REF_PREFIX", _StringOpt, &TERM_QR_MERCH_REF_PREFIX, VIM_SIZEOF(TERM_QR_MERCH_REF_PREFIX), _TEXT, DEFAULT_QR_MERCH_REF_PREFIX },
    {"@QR_SEND_QR_TO_POS",_FlagOpt, &TERM_QR_SEND_QR_TO_POS, VIM_SIZEOF(TERM_QR_SEND_QR_TO_POS), FLAG, "1" },
    {"@QR_ENABLE_WIDE_RCT",_FlagOpt, &TERM_QR_ENABLE_WIDE_RCT, VIM_SIZEOF(TERM_QR_ENABLE_WIDE_RCT), FLAG, "1" },
    {"@QR_DISABLE_FROM_IMAGE",_FlagOpt, &TERM_QR_DISABLE_FROM_IMAGE, VIM_SIZEOF(TERM_QR_DISABLE_FROM_IMAGE), FLAG, "0" }
};

VIM_SIZE NumParams(void)
{
	return VIM_LENGTH_OF(sTMSConfigOptions);
}


static VIM_BOOL fChkFile(VIM_CHAR *psFileName)
{
    VIM_CHAR TempFileName[50];
    VIM_BOOL Exists = VIM_FALSE;

    vim_mem_clear(TempFileName, sizeof(TempFileName));
    vim_sprintf(TempFileName, "~/%s", psFileName);

    vim_file_exists(TempFileName, &Exists);
    return Exists;
}


static void ReportValue( const VIM_CHAR *psRegStrVal, void *pvDestPtr, eDEST_DATA_FORMAT eDestFormat)
{
    switch (eDestFormat)
    {
    default:
    case _TEXT:
    case _CHAR:
    {
        VIM_CHAR *DestPtr = (VIM_CHAR *)pvDestPtr;
        pceft_debugf_test(pceftpos_handle.instance, "%s = %s", psRegStrVal, DestPtr);
    }
    break;

    case FLAG:
    case NUMBER_8_BIT:
    {
        VIM_UINT8 *DestPtr = (VIM_UINT8 *)pvDestPtr;
        pceft_debugf_test(pceftpos_handle.instance, "%s = %d", psRegStrVal, *DestPtr);
    }
    break;

    case NUMBER_16_BIT:
    {
        VIM_UINT16 *DestPtr = (VIM_UINT16 *)pvDestPtr;
        pceft_debugf_test(pceftpos_handle.instance, "%s = %d", psRegStrVal, *DestPtr);
    }
    break;

    case NUMBER_32_BIT:
    {
        VIM_UINT32 *DestPtr = (VIM_UINT32 *)pvDestPtr;
        pceft_debugf_test(pceftpos_handle.instance, "%s = %d", psRegStrVal, *DestPtr);
        break;
    }
    break;

    case NUMBER_64_BIT:
    {
        VIM_UINT64 *DestPtr = (VIM_UINT64 *)pvDestPtr;
        pceft_debugf_test(pceftpos_handle.instance, "%s = %lu", psRegStrVal, *DestPtr);
    }
    break;

    }
}


static VIM_BOOL IsNumericString(VIM_CHAR *LFDCfgData)
{
    VIM_SIZE n, len = vim_strlen(LFDCfgData);
    VIM_CHAR *Ptr = LFDCfgData;
    for ( n = 0; n < len; n++, Ptr++ )
    {
        if (!VIM_ISDIGIT(*Ptr))
            return VIM_FALSE;
    }
    return VIM_TRUE;
}



static VIM_ERROR_PTR ValidateOption(VIM_CHAR *LFDCfgData, eTMSOptions_t OptionType)
{
    VIM_ERROR_PTR res = VIM_ERROR_OVERFLOW;

    switch (OptionType)
    {
        case _FileOpt:
            if (vim_strlen(LFDCfgData) > BRAND_FILENAME_MAX_LEN)
                return res;
            break;

        case _PasswordOpt:
            if (vim_strlen(LFDCfgData) > WOW_MAX_PASSWORD_LEN )
                return res;
            break;

        case _FlagOpt:  // allow multiple ways of handling flags
            break;

        case _2DigitLimitOpt:
            if( !IsNumericString(LFDCfgData) || vim_strlen(LFDCfgData) > 2 )
                return res;
            break;

        case _3DigitLimitOpt:
            if (!IsNumericString(LFDCfgData) || vim_strlen(LFDCfgData) > 3)
                return res;
            break;

        case _6DigitLimitOpt:
            if (!IsNumericString(LFDCfgData) || vim_strlen(LFDCfgData) > 6)
                return res;
            break;

        case _12DigitLimitOpt:
            if (!IsNumericString(LFDCfgData) || vim_strlen(LFDCfgData) > 12)
                return res;
            break;

        default:
            if( vim_strlen(LFDCfgData) > MAX_PARAM_LEN )
                return res;
            break;
    }
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR ParseConfigFile(const VIM_CHAR *Type, VIM_CHAR *Filename)
{
    // RDD JIRA PIRPIN-1069 : Ensure load is done immediatly.
    if (!vim_strcmp(Type, "@CPAT"))
    {
        VIM_ERROR_RETURN_ON_FAIL(ProcessConfigFile(Filename, VIM_FILE_PATH_LOCAL "CPAT", CPAT_FILE_IDX));
        CpatLoadTable();
    }
    else if (!vim_strcmp(Type, "@EPAT"))
    {
        VIM_ERROR_RETURN_ON_FAIL(ProcessConfigFile(Filename, VIM_FILE_PATH_LOCAL "EPAT", EPAT_FILE_IDX));
        wow_emv_params_load();
    }
    else if (!vim_strcmp(Type, "@PKT"))
    {
        VIM_ERROR_RETURN_ON_FAIL(ProcessConfigFile(Filename, VIM_FILE_PATH_LOCAL "PKT", PKT_FILE_IDX));
        wow_capk_load();
    }
    return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR ParseRegOpt( const VIM_CHAR *psDataStr, const VIM_CHAR *psRegStrVal, eTMSOptions_t OptionType, void *pvDestPtr, VIM_SIZE DestLen, eDEST_DATA_FORMAT eDestFormat, const void **pvDefaultValue  )
{
    VIM_CHAR LFDCfgData[LFD_CFG_DATA_MAX + 1];
    VIM_SIZE DataLen = 0;
    //VIM_CHAR* Ptr = VIM_NULL;

	//if( vim_strstr( psDataStr, psRegStrVal, &Ptr ) != VIM_ERROR_NONE)
	//	return VIM_ERROR_NOT_FOUND;

    // If not NULL then ensure default is set ( usually zero but not always )
    // RDD 240223 JIRA-PIRPIN 2240 this cause issues when default zero params were changed to 1
    //if ((pvDefaultValue != VIM_NULL))
    if(1)
    {
        // Put the default value into the destination so will get written over if the value has been sent down.
        parse_data( (VIM_CHAR *)pvDestPtr, (VIM_CHAR *)*pvDefaultValue, eDestFormat);

        VIM_ERROR_RETURN_ON_FAIL(GetElementData(psDataStr, psRegStrVal, LFDCfgData, &DataLen, VIM_SIZEOF(LFDCfgData)));

        // Ensure space for NULL allowed in Strings
		
        if ((eDestFormat == _TEXT) && (DataLen >= DestLen))		
        {		
            pceft_debugf(pceftpos_handle.instance, "%s too long for %s", LFDCfgData, psRegStrVal);			
            VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );			
        }
			
        // ensure the data is within limits		
        VIM_ERROR_RETURN_ON_FAIL(ValidateOption(LFDCfgData, OptionType));

        parse_data(pvDestPtr, LFDCfgData, eDestFormat);		
        if (OptionType == _FileOpt)		
        {		
            if (!fChkFile(LFDCfgData))			
            {			
                pceft_debugf(pceftpos_handle.instance, "[%s] File Not Found", LFDCfgData);				
            }			
            else			
            {			
                VIM_CHAR DestFile[20] = { 0 };				
                vim_sprintf(DestFile, "%s%s", VIM_FILE_PATH_LOCAL, LFDCfgData);				
                ParseConfigFile(psRegStrVal, DestFile);				
            }			
        }
		ReportValue(psRegStrVal, pvDestPtr, eDestFormat);
	}
    return VIM_ERROR_NONE;
}


extern void Wally_UpdateConfig(void);
extern VIM_ERROR_PTR CreateCardTable(void);


VIM_BOOL valid_cpat_file(VIM_CHAR* cpat_filename)
{
    VIM_BOOL valid = VIM_FALSE;
    if (vim_strlen(cpat_filename) >= MIN_CFG_FILE_LEN)
    {
        valid = vim_file_is_present(cpat_filename);
    }
    return valid;
}

VIM_ERROR_PTR ParseRegOptions( const VIM_CHAR *data_ptr, VIM_SIZE num_params )
{
	VIM_SIZE item = 0;
    TMS_CONFIG_OPTION *RegItemPtr = sTMSConfigOptions;

    // RDD 250523 JIRA PIRPIN-2384 Force Logon Required if Generic CPAT was deactivated by TMS
    VIM_BOOL UsingCPATFileAfter, UsingCPATFileBefore = valid_cpat_file(TERM_CPAT_FILENAME);

    VIM_DBG_STRING(data_ptr);

    FunctionMenu = 0;
    
    for (item = 0; item < num_params; item++, RegItemPtr++ )
    {
		if( ParseRegOpt( data_ptr, RegItemPtr->ItemName, RegItemPtr->OptionType, RegItemPtr->pvDestPtr, RegItemPtr->DestLen, RegItemPtr->eDestFormat, &RegItemPtr->pvDefaultValue ) != VIM_ERROR_NONE )
		{
            pceft_debugf(pceftpos_handle.instance, "%s uses default of %s", RegItemPtr->ItemName, RegItemPtr->pvDefaultValue ? RegItemPtr->pvDefaultValue : "NULL");
            //continue;
		}
		else
		{
            //pceft_debugf(pceftpos_handle.instance, "Registry Item : %s set OK", RegItemPtr->ItemName);
		}
    }

    // RDD 250523 JIRA PIRPIN-2384 Force Logon Required if Generic CPAT was deactivated by TMS
    UsingCPATFileAfter = valid_cpat_file(TERM_CPAT_FILENAME);
    if (UsingCPATFileBefore && !UsingCPATFileAfter)
    {
        terminal.logon_state = logon_required;
    }

	// OK to here
	//return VIM_ERROR_NONE;
	// If Wifi Priority changed then rejig the comms setup etc. 
	CommsSetup(VIM_TRUE);

    // RDD 040123 Branding now affects card table - QA will change branding so assume card table needs top change too
    CreateCardTable();

    // Add Wally stuff
    Wally_UpdateConfig();
    pceft_debugf(pceftpos_handle.instance, "Using Card Table from: %s", TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "CARDS.XML"));


#ifdef _WIN32
	TERM_MDA = 0;
	TERM_VDA = 0;
#endif

    TERM_PRN_COLS = TERM_SHORT_RECEIPT ? _50_COL_PRN : _24_COL_PRN;
    if (Covid19CVMLimit * 100 != TERM_COVID19_CVM_OVERRIDE)
    {
        // Convert whole dollars to cents and reset CTLS if value has changed
        TERM_COVID19_CVM_OVERRIDE = 100 * Covid19CVMLimit;
        terminal_set_status(ss_wow_reload_ctls_params);
    }
	// OK to here
	//return VIM_ERROR_NONE;

	// Set QR Dynamic Opts
    if (TERM_QR_WALLET)
    {
        TERM_QR_SEND_QR_TO_POS = IS_SCO ? VIM_TRUE : VIM_FALSE;
        vim_strncpy(TERM_QR_LOCATION, terminal.configuration.merchant_name, VIM_SIZEOF(terminal.configuration.merchant_name) - 1);
        pceft_debugf(pceftpos_handle.instance, "QR_SEND_QR_TO_POS [modified to %d] QR_LOCATION [modified to %s]", TERM_QR_SEND_QR_TO_POS, TERM_QR_LOCATION);
        //SetQROpts();
    }
    terminal_save();

    if (FunctionMenu)
    {
        pceft_debugf(pceftpos_handle.instance, "Running function %d", FunctionMenu );
        vim_event_sleep(200);
        RunFunctionMenu(FunctionMenu, VIM_TRUE, "");
    }

    return VIM_ERROR_NONE;
}


void bin_to_asc_s(VIM_UINT64 num, char* asc, VIM_SIZE digits)
{
	VIM_CHAR Buffer[20];
	VIM_CHAR *Ptr = Buffer;
	VIM_SIZE len, count = 0;

	vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
	bin_to_asc(num, Buffer, digits);
	len = vim_strlen(Buffer);
	// skip any leading zeros
	while ((*Ptr++ == '0') && (count++ < len));
	vim_strcpy(asc, (vim_strlen(Ptr)) ? Ptr : "000");
}

VIM_ERROR_PTR DeParseRegOpt(VIM_CHAR *psDestStrPtr, void *pvPtr, eDEST_DATA_FORMAT eDestFormat)
{
	void *pvSrcPtr = pvPtr;
	switch (eDestFormat)
	{
		default:
		case _TEXT:	
		case _CHAR:
			vim_strcpy( psDestStrPtr, pvSrcPtr );
			break;

		case FLAG:
			//bin_to_asc(*(VIM_UINT8 *)pvSrcPtr, psDestStrPtr, 1);	// '0' or '1'
			*psDestStrPtr =  !(*(VIM_UINT8 *)pvSrcPtr) ? '0' : '1';	// '0' or '1'
			*(++psDestStrPtr) = 0;
			break;

		case NUMBER_8_BIT:
			bin_to_asc_s(*(VIM_UINT8 *)pvSrcPtr, psDestStrPtr, 3 );	// "000" - "255"
			break;

		case NUMBER_16_BIT:
			bin_to_asc_s(*(VIM_UINT16 *)pvSrcPtr, psDestStrPtr, 5);	// "00000" - "65535"
			break;

		case NUMBER_32_BIT:
			bin_to_asc_s(*(VIM_UINT32 *)pvSrcPtr, psDestStrPtr, 10 );	// 10 digits max
			break;

		case NUMBER_64_BIT:
			bin_to_asc_s( *(VIM_UINT64 *)pvSrcPtr, psDestStrPtr, 12 ); // Never more than 12 
			break;
	}
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR tms_vhq_callback_get_app_parameters(vim_tms_vhq_app_parameter_t *app_parameters_ptr, VIM_SIZE *num)
{
	// RDD - 1st param is the TID the rest are parsed from the standard array which cover all TMS methods
	vim_tms_vhq_app_parameter_t *vhq_index = app_parameters_ptr;
	//TMS_CONFIG_OPTION *rtm_index = sTMSConfigOptions;
	// truncate number of params to 10 until its all working
#if 0
	VIM_SIZE n, count = VIM_LENGTH_OF( sTMSConfigOptions ) ;
#else
	VIM_SIZE count = 1;
#endif

	pceft_debugf_test(pceftpos_handle.instance, "tms_vhq_callback_get_app_parameters input count=%d", count);

	vim_strcpy( vhq_index->parameterName, "DeviceId");
	vim_strncpy( vhq_index->parameterValue, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id));
#if 0
	//pceft_debugf(pceftpos_handle.instance, "VHQ Requested Device ID. Returned:%s", app_parameters_ptr->parameterValue);
	vhq_index ++;
	for( n = 1; n <= count; n++, vhq_index++, rtm_index++ )
	{
		VIM_DBG_PRINTF1("DeParsing Parameter %d", count);
		// Copy over the name but skip the leading '@'
		vim_strcpy( vhq_index->parameterName, &rtm_index->ItemName[1] );
		DeParseRegOpt( vhq_index->parameterValue, rtm_index->pvDestPtr, rtm_index->eDestFormat );
		VIM_DBG_PRINTF2("VHQ_PARAM_NAME:%s VHQ_PARAM_VALUE:%s", vhq_index->parameterName, vhq_index->parameterValue);
	}
#endif
	VIM_DBG_PRINTF1("tms_vhq_callback_get_app_parameters  output count=", count);
	pceft_debugf_test(pceftpos_handle.instance, "VHQ Requested %s. Returned:%s", app_parameters_ptr->parameterName, app_parameters_ptr->parameterValue);

	*num = count+1;	// Allow for TID

	return VIM_ERROR_NONE;
}

#define MAX_FILESIZE 4096 // RDD currently 590 bytes but allow for a lot of expansion TODO add limit/overflow checking and error handling

VIM_ERROR_PTR tms_vhq_callback_app_parameters_updated(void)
{
	VIM_CHAR vhq_params_buffer[MAX_FILESIZE];
	VIM_CHAR rtm_params_buffer[MAX_FILESIZE];
	VIM_SIZE num_params, max_params = VIM_LENGTH_OF(sTMSConfigOptions);
	VIM_SIZE FileSize = 0;

	vim_mem_clear(vhq_params_buffer, VIM_SIZEOF(vhq_params_buffer)); 
	vim_mem_clear(rtm_params_buffer, VIM_SIZEOF(rtm_params_buffer));

	if( vim_file_is_present( VHQ_Params ))	
	{
		VIM_ERROR_RETURN_ON_FAIL(vim_file_get_size(VHQ_Params, VIM_NULL, &FileSize));
		pceft_debugf( pceftpos_handle.instance, "tms_vhq_callback_app_parameters_updated : VHQ_Params filesize: %d bytes [max=%d]", FileSize, MAX_FILESIZE);

		if (FileSize > MAX_FILESIZE)
		{
			pceft_debugf(pceftpos_handle.instance, "tms_vhq_callback_app_parameters EXIT due to lack of buffer space" );
			VIM_ERROR_RETURN(VIM_ERROR_OVERFLOW);
		}

		VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( VHQ_Params, vhq_params_buffer, FileSize, 0));
		// Convert from XML to JSON format to use standard parser into database
		VIM_ERROR_RETURN_ON_FAIL( vim_xml_test_parse_buffer(vhq_params_buffer, rtm_params_buffer, &num_params ));
		VIM_DBG_STRING(TERM_VHQ_BRANDING);
		// Load the params into the database
		pceft_debugf(pceftpos_handle.instance, "VHQ_Params : Parsing %d (VHQ) Params of possible %d", num_params, max_params );
	
		DBG_POINT;

		if (ParseRegOptions(rtm_params_buffer, max_params ) == VIM_ERROR_NONE)
		{
			VIM_BOOL pb_flag = VIM_FALSE;
			VIM_CHAR *Ptr = &terminal.configuration.BrandingString[0];
			pceft_debugf(pceftpos_handle.instance, "VHQ_Params : Branding = %s", TERM_VHQ_BRANDING);
			pceft_debug_test( pceftpos_handle.instance, "VHQ_Params Parsed sucessfully into Terminal Configuration file" );
			if( vim_strlen( TERM_VHQ_BRANDING ))
			{
				VIM_DBG_PRINTF1("Process VHQ Branding String: %s", TERM_VHQ_BRANDING);
				ProcessBrandingParams( &Ptr, &pb_flag );
			}
		}
		else
		{
			pceft_debug_test(pceftpos_handle.instance, "VHQ_Params Parsing Failed");
		}
	}
	else
	{
		VIM_DBG_MESSAGE("tms_vhq_callback_app_parameters_updated : VHQ_Params file_missing!! ");
	}
	DBG_POINT;
	return VIM_ERROR_NONE;
}




