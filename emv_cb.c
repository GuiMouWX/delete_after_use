#include <incs.h>

extern txn_state_t txn_sale_amount_entry(void);
extern VIM_ERROR_PTR send_query_response(void);
extern VIM_ERROR_PTR vim_pceftpos_callback_after_query_card(VIM_CHAR sub_code);
extern VIM_BOOL TxnWasApproved( VIM_CHAR *RC ) ;
extern VIM_BOOL ApplySurcharge(transaction_t *transaction, VIM_BOOL Apply);

extern void get_acc_type_string_short( account_t account_type, VIM_CHAR *out_string );
extern void SetupPinEntryParams( VIM_TCU_PIN_ENTRY_PARAMETERS *param, txn_pin_entry_type *pin_entry_type );
extern void SetPinEntryType( txn_pin_entry_type *pin_entry_type  );

extern VIM_ERROR_PTR EmvCardRemoved(VIM_BOOL GenerateError);

extern VIM_CHAR type[];
extern VIM_PCEFTPOS_PARAMETERS pceftpos_settings;
// RDD 241013 SPIRA:6987 Check that the amount returned by the reader matches the amount sent to it 
extern VIM_ERROR_PTR CheckTagAmt( VIM_TLV_LIST *tags_list, VIM_BOOL fAbort, transaction_t *transaction );

#define EMV_GET_BOOL_FROM_BCD(character)  (((int)character == 'Y')? VIM_TRUE : VIM_FALSE)

VIM_RTC_UPTIME LastPinEntryHeartBeat;

/* Offline CVM related to pin verification status flag, to enable pin status display only once */
static VIM_BOOL skip_pin_status_display;
extern VIM_BOOL IsPosConnected( VIM_BOOL fWaitForPos );

// RDD Trello #69 and #199 : If selected label mentions debit then assign default account at this point.    
extern VIM_BOOL CheckDebitLabel(VIM_CHAR *Label);

extern VIM_ERROR_PTR icc_is_removed(VIM_ICC_PTR instance, VIM_BOOL * is_removed);


#if 0
#define CHEQUE_ACCOUNT          1
#define SAVINGS_ACCOUNT         2
#define CREDIT_ACCOUNT          4
#define HOST_CHEQUE_ACCOUNT     0
#define HOST_SAVINGS_ACCOUNT    1
#define HOST_CREDIT_ACCOUNT     2
#else
#define CHEQUE_ACCOUNT          0x20
#define SAVINGS_ACCOUNT			0x10
#define CREDIT_ACCOUNT          0x30
#define HOST_CHEQUE_ACCOUNT     0
#define HOST_SAVINGS_ACCOUNT    1
#define HOST_CREDIT_ACCOUNT     2
#endif

VIM_UINT8 tvr_predial_mask[] = { 0x00,          
                                0x80 | 0x40 | 0x20,         /* (diff app versions, expired app, app not effective) */
                                0x00,               
                                0x80 | 0x40 | 0x20 | 0x10,  /* (floor limit, LCOL exceeded, UCOL exceeded, Random online selection) */
                                0x00 };

/* Hardcode The Termianl Info */
/* Transaction Category Code : Retail/All other transactions = 'R' = 0x52  */
const unsigned char transaction_category_code = 0x52;
/* Point of Service Entry Mode (POSEM) 051: EMV smart card in an EMV capable terminal */
const unsigned char POSEM = 0x51;
/* Terminal Country Code Default country : Aus, currency : AUD$ */
//const unsigned char terminal_country_code[] = "\x00\x36";
/* Terminal Capabilities */
//const unsigned char terminal_capabilities[] = "\xE0\xF0\xC8";
/* Additional Terminal Capabilities */
//const unsigned char additional_terminal_capabilities[] = "\xF0\x00\xF0\xA0\x01";
/* Terminal Type */
const unsigned char terminal_type = 0x22;
/* Merchant Category Code */
const unsigned char merchant_category_code = 0x00; /*  TODO: not sure which value should be used */
/* Transaction Currency Code */
//const unsigned char transaction_currency_code[] = "\x00\x36";
/* Transaction Ref Currency Code */
//const unsigned char transaction_ref_currency_code[] = "\x00\x36";

/* Global: current ICC record */
/*table_record_t icc_record;*/
/* Global Tags to read for printing (include Offline Declined) */

//WOW_PRINT_TAGS_LEN
// RDD - If you add or remove tags here also change the const: WOW_PRINT_TAGS_LEN

const VIM_TAG_ID tags_to_print[] = 
{
  VIM_TAG_EMV_ISSUER_COUNTRY_CODE,			// RDD 220316 added for V560
  VIM_TAG_EMV_AMOUNT_AUTHORISED_NUMERIC,
  //VIM_TAG_EMV_CARDHOLDER_NAME,
  //VIM_TAG_EMV_CARDHOLDER_NAME_EXTENDED,
  VIM_TAG_EMV_APPLICATION_TRANSACTION_COUNTER,
  //VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE,
  VIM_TAG_EMV_APPLICATION_INTERCHANGE_PROFILE,
  VIM_TAG_EMV_TERMINAL_CAPABILITIES,
  VIM_TAG_EMV_TRANSACTION_CURRENCY_CODE,
  VIM_TAG_EMV_TERMINAL_COUNTRY_CODE,
  VIM_TAG_EMV_APPLICATION_PAN,
  VIM_TAG_EMV_APPLICATION_PAN_SEQUENCE_NUMBER,
  VIM_TAG_EMV_AMOUNT_OTHER_NUMERIC,
  VIM_TAG_EMV_APPLICATION_PREFERRED_NAME,
  VIM_TAG_EMV_TSI,
  VIM_TAG_EMV_TERMINAL_TYPE,
  VIM_TAG_EMV_CRYPTOGRAM,
  VIM_TAG_EMV_TERMINAL_IDENTIFICATION,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DEFAULT,
  VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_ONLINE,
  VIM_TAG_EMV_ADDITIONAL_TERM_CAPABILITIES,
  VIM_TAG_EMV_ISSUER_COUNTRY_CODE,
  VIM_TAG_EMV_DEDICATED_FILE_NAME,
  VIM_TAG_EMV_TRANSACTION_DATE,
  VIM_TAG_EMV_TRANSACTION_TIME,
  VIM_TAG_EMV_TRANSACTION_TYPE,
  VIM_TAG_EMV_UNPREDICTABLE_NUMBER,
  VIM_TAG_EMV_ISSUER_APPLICATION_DATA,
  VIM_TAG_EMV_ISSUER_AUTHENTICATION_DATA,
  VIM_TAG_EMV_TVR,
  VIM_TAG_EMV_APPLICATION_EFFECTIVE_DATE,
  VIM_TAG_EMV_APPLICATION_VERSION_NUMBER_ICC,		// RDD 150714 v430 added SPIRA:7861
  VIM_TAG_EMV_APPLICATION_VERSION_NUMBER_TERMINAL,
  VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE,
  VIM_TAG_EMV_RESPONSE_CODE,
  VIM_TAG_EMV_CARDHOLDER_VERIFICATION_RESULTS,
  VIM_TAG_EMV_AID_ICC,
  VIM_TAG_EMV_APPLICATION_LABEL,  
  VIM_TAG_EMV_ISSUER_CODE_TABLE_INDEX, 
  VIM_TAG_EMV_TERMINAL_FLOOR_LIMIT,
  VIM_TAG_EMV_FORM_FACTOR,					// RDD 140714 v430 SPIRA:7565 - only returned by Paypass3 Mobile Phone Payments etc.
  VIM_TAG_EMV_APPLICATION_CURRENCY_CODE,	// RDD 140714 v430 SPIRA:7565 - only returned by Paypass3 Cards and Mobile Phone Payments etc.
  VIM_TAG_EMV_CARD_TRANSACTION_QUALIFIERS,	// RDD 240716 added for v560-6
  VIM_TAG_EMV_UPI_ADDITIONAL_PROCESSING,
  VIM_TAG_EMV_VISA_TERMINAL_TRANSACTION_QUALIFIER,	
  VIM_TAG_EMV_ADDITIONAL_TERM_CAPABILITIES ,		// RDD 300118 Added at Donnas request
#ifdef _DEBUG	// RDD - handy to see these in debug version...
  //VIM_TAG_EMV_CARD_TRANSACTION_QUALIFIERS,
  VIM_TAG_EMV_ADDITIONAL_TERM_CAPABILITIES ,
  VIM_TAG_EMV_APPLICATION_USAGE_CONTROL, 
  VIM_TAG_EMV_CVM_LIST,
  VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE,
#endif
    ""
};

const VIM_TAG_ID tags_to_print_txn[] = 
{
  VIM_TAG_EMV_TSI,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_ONLINE,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DEFAULT,
  VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA,
  VIM_TAG_EMV_APPLICATION_TRANSACTION_COUNTER,
  // RDD 090118 removed at Donnas' request because of some old SPIRA issue
  //VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE,		
  VIM_TAG_EMV_APPLICATION_INTERCHANGE_PROFILE,
  VIM_TAG_EMV_ISSUER_COUNTRY_CODE,
  VIM_TAG_EMV_ISSUER_APPLICATION_DATA,
  VIM_TAG_EMV_CARDHOLDER_VERIFICATION_RESULTS,
  VIM_TAG_EMV_FORM_FACTOR,					// RDD 140714 v430 SPIRA:7565 - only returned by Paypass3 Mobile Phone Payments etc.
  VIM_TAG_EMV_RESPONSE_CODE,
#ifdef _DEBUG	// RDD - handy to see these in debug version...
  //VIM_TAG_EMV_CARD_TRANSACTION_QUALIFIERS,
  VIM_TAG_EMV_ADDITIONAL_TERM_CAPABILITIES ,
  VIM_TAG_EMV_APPLICATION_USAGE_CONTROL, 
  VIM_TAG_EMV_CVM_LIST,
#endif
    "",
};



const VIM_TAG_ID tags_to_display[] = 
{	 	
	VIM_TAG_EMV_APPLICATION_LABEL,
	VIM_TAG_EMV_APPLICATION_PREFERRED_NAME,
	VIM_TAG_EMV_APPLICATION_PAN_SEQUENCE_NUMBER,	// RDD 060812 SPIRA Need this tag for Partial EMV
	VIM_TAG_EMV_ISSUER_CODE_TABLE_INDEX,
	VIM_TAG_EMV_ISSUER_APPLICATION_DATA,			// RDD 140912 P3 Added for Rimu Loyalty Query Card (CTLS)
	RIMU_FCI_IDD_TAG								// RDD 170912 P3 Added for Rimu Loyalty Query Card (CONTACT)
};

const VIM_TAG_ID tags_to_update[] = 
{
  VIM_TAG_EMV_TVR,
  VIM_TAG_EMV_TSI,
  VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA,
  VIM_TAG_EMV_CRYPTOGRAM,		// RDD 151012 P3 added here because we no longer update tags to print
  VIM_TAG_EMV_VISA_SCRIPT_RESULTS,
  VIM_TAG_EMV_UPI_SCRIPT_RESULTS,	// RDD 010716 SPIRA:8979 DF31 to be read from kernel and added to DE55 for UPI reversals if Z4 due to script failure
  VIM_TAG_EMV_CVM_RESULTS,
  VIM_TAG_EMV_RESPONSE_CODE
};

/* Global tags send in authorization request */
const VIM_TAG_ID host_tags_auth[] =
{
	// RDD 180113 SPIRA:6527 Remove this tag again cos they changed their minds ...
	//VIM_TAG_EMV_AID_ICC,		// RDD 2321012 SPIRA:6214 added for EFTPOS EMV (EPAL)
	//VIM_TAG_EMV_ISSUER_COUNTRY_CODE,			// RDD 220316 added for V560
	VIM_TAG_EMV_TRANSACTION_CURRENCY_CODE,
	VIM_TAG_EMV_APPLICATION_INTERCHANGE_PROFILE,
	VIM_TAG_EMV_TVR,
	VIM_TAG_EMV_TRANSACTION_DATE,
	VIM_TAG_EMV_TRANSACTION_TYPE,
	VIM_TAG_EMV_AMOUNT_AUTHORISED_NUMERIC,
	VIM_TAG_EMV_AMOUNT_OTHER_NUMERIC,
	VIM_TAG_EMV_ISSUER_APPLICATION_DATA,
	VIM_TAG_EMV_TERMINAL_COUNTRY_CODE,
	VIM_TAG_EMV_CRYPTOGRAM,
	VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA,
	VIM_TAG_EMV_TERMINAL_CAPABILITIES,
	VIM_TAG_EMV_CVM_RESULTS,
	VIM_TAG_EMV_TERMINAL_TYPE,
	VIM_TAG_EMV_APPLICATION_TRANSACTION_COUNTER,
	VIM_TAG_EMV_UNPREDICTABLE_NUMBER,
	VIM_TAG_EMV_RESPONSE_CODE,
	VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE,  /* for MasterCard only */
	VIM_TAG_EMV_FORM_FACTOR,					// RDD 140714 v430 SPIRA:7565 - only returned by Paypass3 Mobile Phone Payments etc.
	VIM_TAG_EMV_APPLICATION_CURRENCY_CODE,	// RDD 140714 v430 SPIRA:7565 - only returned by Paypass3 Cards and Mobile Phone Payments etc.
    VIM_TAG_EMV_DEDICATED_FILE_NAME,		// RDD SPIRA 4948: Actually needed for one scheme test but causes WOW switch to crash to need to remove ( 060820 : Trello #194 restore this )
                                            // RDD 270720 Trello #194 - restore Tag 84 across the board to all schemes
	VIM_TAG_EMV_UPI_SCRIPT_RESULTS,			// RDD 010716 SPIRA:8979
	VIM_TAG_EMV_VISA_SCRIPT_RESULTS,			// RDD 010716 SPIRA:8979 - noticed these were missing from reversal too
	//VIM_TAG_EMV_AID_TERMINAL					// RDD 221117 JIRA:WWBP-34
											  // RDD 020218 JIRA:WWBP-132 RPAT v571-6 remove this as was a type - should have been 9F66 and put in XML anyway.
											  // RDD SPIRA 8205: Required for EPAL CTAV Scheme Testing (actually turned out ot be optional)
};

  

VIM_BOOL emv_attempt_to_go_online;
VIM_BOOL file41_emv_cashout_flag;
VIM_BOOL emv_predial_done;
VIM_UINT8 local_terminal_action_code_online[5];
VIM_EMV_ONLINE_RESULT local_online_result;
VIM_BOOL emv_blocked_application;
VIM_BOOL emv_fallbak_step_passed;
VIM_SIZE req_de55_length;
VIM_UINT8 req_de55_buffer[1024];	// RDD 260318 JIRA WWBP-195 Double size from 512 -> 1024 as it looks like memory overrun may have caused "freezing issue" MAP file indicates that this buffer precedes the stateMachine memory address
VIM_ERROR_PTR emv_transaction_error;
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR emv_get_track_2(transaction_t *transaction);
//static VIM_ERROR_PTR emv_get_track_1(transaction_t *transaction);
static VIM_ERROR_PTR EMV_OfflineDeclined_Add(void);

/*----------------------------------------------------------------------------*/

void Pause( VIM_UINT16 PauseMS )
{
	VIM_RTC_UPTIME start, now;	  
	vim_rtc_get_uptime(&start);
	DBG_MESSAGE( "---- Start Pause ----" );
	do
	{
		vim_rtc_get_uptime(&now);
	}	
	while( now < ( start + PauseMS ));	
	DBG_MESSAGE( "---- End Pause ----" );
}



/* returns the tags uses on getting mobile phones informations */
static VIM_ERROR_PTR GetMobileDeviceTags(VIM_TLV_LIST *tags_list)
{
	VIM_UINT8 tlv_buffer[1024];
	VIM_TLV_TAG_PTR const tags[] = { VIM_TAG_EMV_DEDICATED_FILE_NAME,	// Tag 84 used to check AMEX cards
		VIM_TAG_EMV_FORM_FACTOR,			// Tag 9F6E to check Visa cards
		VIM_TAG_EMV_CVM_RESULTS			// Tag 9F34 to check master cards
	};

	if (tags_list == VIM_NULL)
		return VIM_ERROR_NOT_FOUND;

	// get the tags value
	vim_tlv_create(tags_list, tlv_buffer, sizeof(tlv_buffer));
	picc_tags();
	vim_picc_emv_get_tags(picc_handle.instance, tags, VIM_LENGTH_OF(tags), tags_list);

	return VIM_ERROR_NONE;
}

static VIM_BOOL MatchAID(VIM_TLV_LIST *tags_list, VIM_UINT8 *aid)
{
	if (vim_tlv_search(tags_list, VIM_TAG_EMV_DEDICATED_FILE_NAME) == VIM_ERROR_NONE)
	{
		if (tags_list->current_item.length)
		{
			// Check if this tap is from Apple
			if (vim_mem_compare(aid, tags_list->current_item.value, tags_list->current_item.length) == VIM_ERROR_NONE)
			{
				VIM_DBG_MESSAGE("AMEX card : Apple Pay device");
				return VIM_TRUE;
			}
#if 0 // redundant code // VN 280818
			// Check if this tap is from Google
			if (vim_mem_compare(aid, tags_list->current_item.value, tags_list->current_item.length) == VIM_ERROR_NONE)
			{
				VIM_DBG_MESSAGE("AMEX card : Google Pay device");
				return VIM_TRUE;
			}
#endif 
		}
	}
	return VIM_FALSE;
}

// VNM 290818 Change return type to enum device_type
// Check form factor bits
static device_type CheckFormFactor(VIM_TLV_LIST *tags_list, VIM_UINT8 form_factor)
{

	// Currently, the check on this function is available only for Visa cards
	if (vim_tlv_search(tags_list, VIM_TAG_EMV_DEDICATED_FILE_NAME) == VIM_ERROR_NONE)
	{
		if (tags_list->current_item.length >= VIM_ICC_RID_LEN)
		{
			// Return False if it's not VISA/UPI/EFTPOS
			if (((vim_emv_is_visa(tags_list->current_item.value)) ||
				(vim_emv_is_upi(tags_list->current_item.value)) ||
				(vim_emv_is_eftpos(tags_list->current_item.value))) == VIM_FALSE) {
				DBG_POINT;
				return device_type_card;
			}
		}
	}

	//Check the form factors
	if (vim_tlv_search(tags_list, VIM_TAG_EMV_FORM_FACTOR) == VIM_ERROR_NONE)
	{
		if (tags_list->current_item.length)
		{
			if (((tags_list->current_item.value[0] & MOBILE_TAP_BIT) == MOBILE_TAP_BIT) ||
				((tags_list->current_item.value[0] & MOBILE_TAP_BIT) == WRIST_WORN_TAP_BIT))
			{
				// check for Google Pay, Cloud based payment credential should be available in bit 2 from bit 0
				if (tags_list->current_item.value[1] & form_factor)
				{
					VIM_DBG_MESSAGE("Visa card : Google Pay device");
					return device_type_google_pay;
				}

				// check for Google Pay, Cloud based payment credential should be disable
				if ((tags_list->current_item.value[1] & form_factor) == 0)
				{
					VIM_DBG_MESSAGE("Visa card : Apple Pay device");
					return device_type_apple_pay;
				}
			}
		}
	}

	return device_type_card;
}




// Check if the CMVR bytes
static VIM_BOOL CheckCVMR(VIM_TLV_LIST *tags_list, VIM_UINT8 cmvr, VIM_UINT8 *val)
{
	// Currently, this check on this function is only available for master cards
	if (vim_tlv_search(tags_list, VIM_TAG_EMV_DEDICATED_FILE_NAME) == VIM_ERROR_NONE)
	{
		if (tags_list->current_item.length)
		{
			// master card AID
			if (vim_mem_compare("\xA0\x00\x00\x00\x04\x10\x10", tags_list->current_item.value, tags_list->current_item.length) != VIM_ERROR_NONE)
				return VIM_FALSE;
		}
	}


	if (vim_tlv_search(tags_list, VIM_TAG_EMV_CVM_RESULTS) == VIM_ERROR_NONE)
	{
		if (tags_list->current_item.length)
		{
			*val = tags_list->current_item.value[cmvr];
			return VIM_TRUE;

		}
	}

	return VIM_FALSE;

}

// RDD 210720 Trello #82 Allow Full EMV refunds for Mastercard and VISA if enabled via LFD
VIM_BOOL AllowFullEMVRefund(transaction_t *transaction)
{
    VIM_DBG_VAR(ENABLE_MC_EMV_REFUNDS);
    VIM_DBG_VAR(ENABLE_VISA_EMV_REFUNDS);

    if ((IsEmvRID(VIM_EMV_RID_MASTERCARD, transaction) && (ENABLE_MC_EMV_REFUNDS)))
    {
        VIM_DBG_MESSAGE("Full EMV Refund MC ");
        return VIM_TRUE;
    }
    else if ((IsEmvRID(VIM_EMV_RID_VISA, transaction) && (ENABLE_VISA_EMV_REFUNDS)))
    {
        VIM_DBG_MESSAGE("Full EMV Refund VISA ");
        return VIM_TRUE;
    }
    VIM_DBG_MESSAGE("Partial EMV Refund ");
    return VIM_FALSE;
}


// check if the mobile tap is from Android Pay
VIM_BOOL IsAndroidPay()
{
	VIM_TLV_LIST tags_list;
	VIM_UINT8 condition = 0xFF; //default

	if (GetMobileDeviceTags(&tags_list) == VIM_ERROR_NOT_FOUND)
		return VIM_FALSE;

	// Check AID 
	if (MatchAID(&tags_list, (VIM_UINT8 *)GOOGLE_PAY_AID))
		return VIM_TRUE;
	// Check if cloud based payments bit is set on Form Factor
	if (CheckFormFactor(&tags_list, CLOUD_BASED_CREDENTIAL_BIT) == device_type_google_pay)
		return VIM_TRUE;
	// Check for CVMR condition bit
	if (CheckCVMR(&tags_list, CVM_BYTE2_CONDITION, &condition))
	{
		if(condition == CVM_BYTE2_CONDITION_03)	// always try to use only if applicable
			return VIM_TRUE;
	}

	return VIM_FALSE;
}

// check if the mobile tap is from Apple Pay
VIM_BOOL IsApplePay()
{
	VIM_TLV_LIST tags_list;
	VIM_UINT8 condition = 0xFF; //default
	if (GetMobileDeviceTags(&tags_list) == VIM_ERROR_NOT_FOUND)
		return VIM_FALSE;

	if (MatchAID(&tags_list, (VIM_UINT8 *)APPLE_PAY_AID))
		return VIM_TRUE;

	// Check if cound based payments bit is set on Form Factor
	if (CheckFormFactor(&tags_list, CLOUD_BASED_CREDENTIAL_BIT) == device_type_apple_pay) {
		return VIM_TRUE;
	}

	// Check for CVMR
	if (CheckCVMR(&tags_list, CVM_BYTE2_CONDITION, &condition))
	{
		if (condition == CVM_BYTE2_CONDITION_00) { // always try to use	
			return VIM_TRUE;
		}
	}

	return VIM_FALSE;
}


VIM_ERROR_PTR GetMobileDevicePayType(VIM_INT8 *DevPayType)
{
	*DevPayType = CARD_WITH_CHIP;

	if (IsApplePay())
	{
		*DevPayType = APPLE_PAY_DEVICE;
	}
	else if (IsAndroidPay())
	{
		*DevPayType = GOOGLE_PAY_DEVICE;
	}

	return VIM_ERROR_NONE;
}


// RDD 231012 added
VIM_BOOL IsEmvRID( const char *RID, transaction_t *transaction ) 
{
	if( vim_mem_compare( RID, terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_ICC_RID_LEN ) == VIM_ERROR_NONE )
		return VIM_TRUE;
	else
		return VIM_FALSE;
}

	
// RDD 231012 added
static VIM_BOOL IsTag( const char *Tag, const char *Data, VIM_UINT8 Len )
{
	if( vim_mem_compare( Tag, Data, Len ) == VIM_ERROR_NONE )
		return VIM_TRUE;
	else
		return VIM_FALSE;
}


VIM_BOOL is_EPAL_cards( transaction_t* txn_ptr )
{
  // DBG_VAR( txn_ptr->aid_index );
  if( txn_ptr->aid_index > MAX_EMV_APPLICATION_ID_SIZE ) return VIM_FALSE;
  if( vim_mem_compare( terminal.emv_applicationIDs[txn_ptr->aid_index].emv_aid, "\xA0\x00\x00\x03\x84\x10", 6 ) == VIM_ERROR_NONE )
  {	 
    return VIM_TRUE;
  }
  if( vim_mem_compare( terminal.emv_applicationIDs[txn_ptr->aid_index].emv_aid, "\xA0\x00\x00\x03\x84\x20", 6 ) == VIM_ERROR_NONE )
  {	 
    return VIM_TRUE;
  }	 
  return VIM_FALSE;
}


/*----------------------------------------------------------------------------*/
void init_emv_test_values(void)
{
  /* Local terminal Options */
   /*  Put Dummy values in card table */
 
   vim_mem_set(&terminal.card_name[0], 0, VIM_SIZEOF(terminal.card_name[0]));
   terminal.card_name[0].number = CARD_NAME_VALID_MIN;
   terminal.card_name[0].PAN_range_low = 0;
   terminal.card_name[0].PAN_range_high = 999999999;
  terminal.card_name[0].pan_length = 0;
   
   //terminal.card_name[0].issuer_table_reference_number = ISSUER_VALID_MIN;
   terminal.card_name[0].acquirer_credit_id = ACQUIRER_VALID_MIN;
   
   terminal.card_name[0].issuer_credit_id= ACQUIRER_VALID_MIN;
   
   terminal.card_name[0].acquirer_debit_id= ACQUIRER_VALID_MIN;
   terminal.card_name[0].issuer_debit_id= ACQUIRER_VALID_MIN;

/* Hardcode The Termianl Info */
  vim_mem_copy( terminal.emv_default_configuration.country_code, "\x00\x36", 2 );
  vim_mem_copy( terminal.emv_default_configuration.terminal_capabilities, "\xE0\xF0\xC8", 3 );
  vim_mem_copy( terminal.emv_default_configuration.additional_terminal_capabilities, "\xF0\x00\xF0\xA0\x01", 5 );
  terminal.emv_default_configuration.terminal_type = 0x22;
  vim_mem_copy( terminal.emv_default_configuration.txn_reference_currency_code, "\x00\x36", 2 );
  terminal.emv_default_configuration.txn_reference_currency_exponent = 2;
/*
  EMV AIDs
*/
  vim_mem_copy(terminal.emv_applicationIDs[0].emv_aid, "\xA0\x00\x00\x00\x04\x10\x10", 7);
  vim_mem_copy(terminal.emv_applicationIDs[0].app_version1, "\x00\x02", 2);
#if 1
  vim_mem_copy(terminal.emv_applicationIDs[0].emv_aid, "\xA0\x00\x00\x00\x03\x10\x10", 7);
  vim_mem_copy(terminal.emv_applicationIDs[0].app_version1, "\x00\x8C", 2);
#endif

  terminal.emv_applicationIDs[0].aid_length = 7;
/*  vim_mem_copy(terminal.emv_applicationIDs[0].tac_default, "\xC0\x00\x00\x80\x00", 5);*/
  vim_mem_copy(terminal.emv_applicationIDs[0].tac_default, "\x00\x00\x00\x00\x00", 5);
  vim_mem_copy(terminal.emv_applicationIDs[0].tac_denial, "\x00\x00\x00\x00\x00", 5);
/*  vim_mem_copy(terminal.emv_applicationIDs[0].tac_online, "\xC0\x00\x00\x80\x00", 5);*/
  vim_mem_copy(terminal.emv_applicationIDs[0].tac_online, "\x00\x00\x00\x00\x00", 5);
  /*vim_mem_clear(terminal.emv_applicationIDs[0].threshold, VIM_SIZEOF(terminal.emv_applicationIDs[0].threshold));*/
  terminal.emv_applicationIDs[0].threshold = 0;
  terminal.emv_applicationIDs[0].tagert_percent = 0;
  terminal.emv_applicationIDs[0].max_target = 0;
  vim_mem_copy(terminal.emv_applicationIDs[0].ddol_default, "\x9F\x37\x04\x00", 4);
  vim_mem_copy(terminal.emv_applicationIDs[0].tdol_default, "\x9F\x02\x06\x5F\x2A\x02\x9A\x03\x9C\x01\x95\x05\x9F\x37\x04\x00\x00\x00\x00\x00", 20);
  terminal.emv_applicationIDs[0].transaction_type = 0;
  terminal.emv_applicationIDs[0].floor_limit = 1000;
  terminal.emv_applicationIDs[0].category_code = '2';
  terminal.emv_applicationIDs[0].selector_index = 0;
  terminal.emv_applicationIDs[0].priority_index = 0;

/*  EMV Public Keys */
  vim_mem_copy(terminal.emv_capk[0].capk.RID.value, "\xA0\x00\x00\x00\x04", 5);
  terminal.emv_capk[0].capk.index = 0xFA;
  terminal.emv_capk[0].capk.modulus_size = 144;
  vim_mem_copy(terminal.emv_capk[0].capk.modulus, "\xA9\x0F\xCD\x55\xAA\x2D\x5D\x99\x63\xE3\x5E\xD0\xF4\x40\x17\x76\x99\x83\x2F\x49\xC6\xBA\xB1\x5C\xDA\xE5\x79\x4B\xE9\x3F\x93\x4D\x44\x62\xD5\xD1\x27\x62\xE4\x8C\x38\xBA\x83\xD8\x44\x5D\xEA\xA7\x41\x95\xA3\x01\xA1\x02\xB2\xF1\x14\xEA\xDA\x0D\x18\x0E\xE5\xE7\xA5\xC7\x3E\x0C\x4E\x11\xF6\x7A\x43\xDD\xAB\x5D\x55\x68\x3B\x14\x74\xCC\x06\x27\xF4\x4B\x8D\x30\x88\xA4\x92\xFF\xAA\xDA\xD4\xF4\x24\x22\xD0\xE7\x01\x35\x36\xC3\xC4\x9A\xD3\xD0\xFA\xE9\x64\x59\xB0\xF6\xB1\xB6\x05\x65\x38\xA3\xD6\xD4\x46\x40\xF9\x44\x67\xB1\x08\x86\x7D\xEC\x40\xFA\xAE\xCD\x74\x0C\x00\xE2\xB7\xA8\x85\x2D", 0x90);
  terminal.emv_capk[0].capk.exponent_size= 0x1;
  terminal.emv_capk[0].capk.exponent[0] = 0x03;

  vim_mem_copy(terminal.emv_capk[1].capk.RID.value, "\xA0\x00\x00\x00\x03", 5);
  terminal.emv_capk[1].capk.index = 0x99;
  terminal.emv_capk[1].capk.modulus_size = 128;
  vim_mem_copy(terminal.emv_capk[1].capk.modulus, "\xAB\x79\xFC\xC9\x52\x08\x96\x96\x7E\x77\x6E\x64\x44\x4E\x5D\xCD\xD6\xE1\x36\x11\x87\x4F\x39\x85\x72\x25\x20\x42\x52\x95\xEE\xA4\xBD\x0C\x27\x81\xDE\x7F\x31\xCD\x3D\x04\x1F\x56\x5F\x74\x73\x06\xEE\xD6\x29\x54\xB1\x7E\xDA\xBA\x3A\x6C\x5B\x85\xA1\xDE\x1B\xEB\x9A\x34\x14\x1A\xF3\x8F\xCF\x82\x79\xC9\xDE\xA0\xD5\xA6\x71\x0D\x08\xDB\x41\x24\xF0\x41\x94\x55\x87\xE2\x03\x59\xBA\xB4\x7B\x75\x75\xAD\x94\x26\x2D\x4B\x25\xF2\x64\xAF\x33\xDE\xDC\xF2\x8E\x09\x61\x5E\x93\x7D\xE3\x2E\xDC\x03\xC5\x44\x45\xFE\x7E\x38\x27\x77", 0x80);
  terminal.emv_capk[1].capk.exponent_size= 0x01;
  terminal.emv_capk[1].capk.exponent[0] = 0x03;
  vim_mem_copy( terminal.emv_capk[1].capk.hash, "\x4A\xBF\xFD\x6B\x1C\x51\x21\x2D\x05\x55\x2E\x43\x1C\x5B\x17\x00\x7D\x2F\x5E\x6D", 20);

  vim_mem_copy(terminal.emv_capk[2].capk.RID.value, "\xA0\x00\x00\x00\x03", 5);
  terminal.emv_capk[2].capk.index = 0x95;
  terminal.emv_capk[2].capk.modulus_size = 144;
  vim_mem_copy(terminal.emv_capk[2].capk.modulus, "\xBE\x9E\x1F\xA5\xE9\xA8\x03\x85\x29\x99\xC4\xAB\x43\x2D\xB2\x86\x00\xDC\xD9\xDA\xB7\x6D\xFA\xAA\x47\x35\x5A\x0F\xE3\x7B\x15\x08\xAC\x6B\xF3\x88\x60\xD3\xC6\xC2\xE5\xB1\x2A\x3C\xAA\xF2\xA7\x00\x5A\x72\x41\xEB\xAA\x77\x71\x11\x2C\x74\xCF\x9A\x06\x34\x65\x2F\xBC\xA0\xE5\x98\x0C\x54\xA6\x47\x61\xEA\x10\x1A\x11\x4E\x0F\x0B\x55\x72\xAD\xD5\x7D\x01\x0B\x7C\x9C\x88\x7E\x10\x4C\xA4\xEE\x12\x72\xDA\x66\xD9\x97\xB9\xA9\x0B\x5A\x6D\x62\x4A\xB6\xC5\x7E\x73\xC8\xF9\x19\x00\x0E\xB5\xF6\x84\x89\x8E\xF8\xC3\xDB\xEF\xB3\x30\xC6\x26\x60\xBE\xD8\x8E\xA7\x8E\x90\x9A\xFF\x05\xF6\xDA\x62\x7B", 0x90);
  terminal.emv_capk[2].capk.exponent_size= 0x01;
  terminal.emv_capk[2].capk.exponent[0] = 0x03;
  vim_mem_copy( terminal.emv_capk[2].capk.hash, "\xEE\x15\x11\xCE\xC7\x10\x20\xA9\xB9\x04\x43\xB3\x7B\x1D\x5F\x6E\x70\x30\x30\xF6", 20 );
  terminal.emv_capk[2].capk.expiry_date.day =0;
  terminal.emv_capk[2].capk.expiry_date.month =0;
  terminal.emv_capk[2].capk.expiry_date.year =0;
}





VIM_ERROR_PTR pos_delete_wait_start()
{  
	// RDD 070115 Don't do all this if theres no client
	if( IS_STANDALONE )
	  return VIM_ERROR_NONE;

	// wait for POS to be deleted from callback and restart it from this, main thread
	VIM_DBG_MESSAGE("pos_delete_wait_start");

	//if( pceftpos_handle.instance == VIM_NULL )
	//	return VIM_ERROR_NONE;
  
	VIM_ERROR_RETURN_ON_FAIL( pceft_settings_reconfigure( VIM_FALSE ) );  
	VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_connected_flag( pceftpos_handle.instance ) );  
	return VIM_ERROR_NONE;
}





// Card Removed Check with Timer
VIM_BOOL CardRemoved( VIM_SIZE timeout )
{
  #define CHECK_INTERVAL 20

  VIM_RTC_UPTIME start_time, now;
  VIM_BOOL icc_removed=VIM_FALSE;
  /* get current time */
  vim_rtc_get_uptime(&start_time);
  //DBG_MESSAGE( "Check for card removal..." );
  do
  {	  		
	vim_event_sleep(5);
    icc_is_removed(icc_handle.instance_ptr, &icc_removed);
    vim_rtc_get_uptime(&now);
	// RDD 260513 - remove possiblility of uptime roll-over causing problems
	if( now < start_time ) start_time = now;

    if( icc_removed )
	{
		DBG_MESSAGE( "!!!! CARD REMOVED !!!!" );
		return VIM_TRUE;
	}
    vim_event_sleep( CHECK_INTERVAL );
  } while( now < (start_time + timeout) );
  //DBG_MESSAGE( "Check for card removal timed out" );

  return VIM_FALSE;
}



VIM_BOOL emv_is_card_international(void)
{
  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[50];
  VIM_UINT8 issuer_curr_code[2];
  VIM_UINT8 terminal_curr_code[2];
  VIM_BOOL IsIntCard = VIM_FALSE;
  emv_aid_parameters_t *aid_ptr = &terminal.emv_applicationIDs[txn.aid_index];	

  const VIM_TAG_ID tags[] = 
  {
    VIM_TAG_EMV_ISSUER_COUNTRY_CODE,
    VIM_TAG_EMV_TERMINAL_COUNTRY_CODE,
  };

  // RDD 041016 Issuer Country code is not reliable for PICC
  if( txn.card.type != card_chip )
	  return VIM_FALSE;
    
  // RDD 301013 SPIRA:6987 Customer Wrongly charged
  vim_mem_clear( tlv_buffer, VIM_SIZEOF( tlv_buffer ) );

  if( txn.emv_data.data_size )	
  {	
	  DBG_POINT;
	  vim_tlv_open( &tags_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE );  
  }
  else
  {	
	  DBG_POINT;
	  vim_tlv_create( &tags_list, tlv_buffer, VIM_SIZEOF(tlv_buffer));
	  vim_emv_read_kernel_tags( tags, 2, &tags_list );
  }

  if ( vim_tlv_search(&tags_list, VIM_TAG_EMV_ISSUER_COUNTRY_CODE) == VIM_ERROR_NONE )      
  {
	    DBG_POINT;

		// RDD 041016 additional check to validate 
		if( tags_list.current_item.length != 2 )
		{
			DBG_MESSAGE( "--- Issuer Country Code has Invalid len ---");
			return VIM_FALSE;
		}
        vim_tlv_get_bytes(&tags_list, issuer_curr_code, VIM_SIZEOF(issuer_curr_code));
		DBG_DATA( issuer_curr_code, VIM_SIZEOF(issuer_curr_code) );
		if ( vim_tlv_search( &tags_list, VIM_TAG_EMV_TERMINAL_COUNTRY_CODE ) == VIM_ERROR_NONE )
        {
          DBG_POINT;
          vim_tlv_get_bytes(&tags_list, terminal_curr_code, VIM_SIZEOF(terminal_curr_code));

		  DBG_DATA( terminal_curr_code, VIM_SIZEOF(terminal_curr_code) );

          if( vim_mem_compare( issuer_curr_code, terminal_curr_code, 2 ) != VIM_ERROR_NONE )
		  {
			/*  Return international */
            IsIntCard = VIM_TRUE;
		  }
		}
  }
  // RDD 230513 SPIRA:6737 - keep Int and Domestic tags separate and copy over into default config when the decision is made re. int or dom 
  if( IsIntCard )
  {
	DBG_MESSAGE( "---------------- This Card Is International ------------------");
	aid_ptr = &terminal.emv_applicationIDs[txn.aid_index];
  }
  else
  {
	DBG_MESSAGE( "---------------- This Card Is Domestic ------------------");
	aid_ptr = &terminal.emv_applicationIDs[txn.aid_index];
  }

  vim_mem_copy( &terminal.emv_applicationIDs[txn.aid_index], aid_ptr, VIM_SIZEOF(emv_aid_parameters_t) );
  return IsIntCard;
}


// RDD v560-7 140916 SPIRA:9046 ADVT #23 Combination CVM needs callback to reset TCU
VIM_ERROR_PTR vim_emv_CB_reset_tcu()
{
  VIM_DBG_MESSAGE( " EMV Kernel 7.0 - Reopen CRYPTO Driver " );
  VIM_ERROR_WARN_ON_FAIL( vim_tcu_renew_handle( &tcu_handle )); 
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_aid_list
(
  VIM_SIZE *aid_count,
  VIM_EMV_AID aid_list[]
)
{
  VIM_UINT8 i;
  VIM_SIZE max_support_AID = *aid_count;
  
  VIM_DBG_WARNING("+++++++++++++++ vim_emv_CB_get_aid_list +++++++++++++++++++");
  
  for (*aid_count = i = 0; i < MAX_EMV_APPLICATION_ID_SIZE; i++)
  {
    //VIM_DBG_VAR(*aid_count);
    //VIM_DBG_VAR(terminal.emv_applicationIDs[i].contactless);
    /* check if there is a record in the structure*/

	DBG_VAR( *aid_count );
	DBG_VAR( i );
	DBG_MESSAGE( terminal.emv_applicationIDs[i].aid_display_text );

    if (0 == terminal.emv_applicationIDs[i].emv_aid[0])
	{
		DBG_MESSAGE( "======= AID INVALID LABEL - TERMINATE !! =======" );
		break;
	}

    /* do not use contactless application */
    if (terminal.emv_applicationIDs[i].contactless)
	{
		DBG_MESSAGE( "= IGNORE CTLS AID =" );
		continue;
	}
	 

    /* AID */
    aid_list[i].aid_size = terminal.emv_applicationIDs[i].aid_length;
    vim_mem_copy(aid_list[i].aid, terminal.emv_applicationIDs[i].emv_aid, aid_list[i].aid_size);   
    /* AID name */
    vim_mem_set(aid_list[i].label_name, 0, MAX_APPLICATION_LABEL_LEN + 1);
    vim_mem_copy(aid_list[i].label_name, terminal.emv_applicationIDs[i].aid_display_text, MAX_APPLICATION_LABEL_LEN);

    VIM_DBG_STRING( aid_list[i].label_name );
    VIM_DBG_DATA( aid_list[i].aid, aid_list[i].aid_size );
    VIM_DBG_DATA(terminal.emv_applicationIDs[i].tac_online, 5);
    VIM_DBG_DATA(terminal.emv_applicationIDs[i].term_cap, 3);


/*    if (terminal.emv_applicationIDs[*aid_count].aid_options1 & AID_OPTION1_SELECT_IF_EXACT_MATCH_ONLY)
      aid_list[*aid_count].partial_match = VIM_FALSE;
    else*/
      aid_list[i].partial_match = VIM_TRUE;
    
    aid_list[i].confirmation_req = VIM_FALSE;
    aid_list[i].priority = terminal.emv_applicationIDs[i].priority_index;

    (*aid_count)++;
  }
  VIM_DBG_NUMBER( max_support_AID );

  if (*aid_count >= max_support_AID )
  {

	  pceft_debug(pceftpos_handle.instance, "!!! SYSTEM ERROR: (Aid List Exceeds Max allowed) EMV_CB.C L1064 !!!!");    
	  VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
#define EMV_APP_LIST_MAX    25
#define EMV_APP_LIST_TITLE  "Select Application"

VIM_ERROR_PTR vim_emv_CB_select_app( VIM_SIZE *aid_count, VIM_EMV_AID candidate_list[] )
{  
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;  
	//char *items[EMV_APP_LIST_MAX];  
	//VIM_SIZE i;  
	//VIM_SIZE max;      

	// RDD 310314 added  
	VIM_SIZE ItemIndex = 0;

	DBG_MESSAGE("vim_emv_CB_select_app");
  
	//max = MIN(EMV_APP_LIST_MAX, *aid_count);

	//for(i = 0; i < max; i++)
	//    items[i] = candidate_list[i].label_name;

	while (1)
	{
		VIM_ERROR_PTR res;
		char *items[EMV_APP_LIST_MAX];
		VIM_SIZE i;
		VIM_SIZE max;

		/* flush kbd buffer or touch screen buffer */			
		vim_screen_kbd_or_touch_read( VIM_NULL, 0, VIM_TRUE );
	
		VIM_DBG_MESSAGE("vim_emv_CB_select_app");  
		max = MIN(EMV_APP_LIST_MAX, *aid_count);
  	
		if( max > 1 )
		{
			for(i = 0; i < max; i++)
			{
                items[i] = candidate_list[i].label_name;
				VIM_DBG_VAR(items[i]);
			}

			// RDD 291217 get event deltas for logging
			vim_rtc_get_uptime( &txn.uEventDeltas.Fields.AppReq );     

            pceft_pos_display("SELECT APPLICATION", "ON PINPAD", VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_PROCESSING);      

            // RDD 200820 Trello #134 Applicaiton selection should not time out
            do
            {
#ifdef _VOS2
                res = display_menu_select_zero_based(EMV_APP_LIST_TITLE, ItemIndex, aid_count, (VIM_TEXT const **)items, max, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE);
#else
                res = display_menu_select(EMV_APP_LIST_TITLE, &ItemIndex, aid_count, (VIM_TEXT **)items, max, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE);
#endif
                if (res == VIM_ERROR_TIMEOUT)
                {
                    vim_kbd_sound_invalid_key();
                    pceft_debug_test(pceftpos_handle.instance, "Trello #134");
                }
            } while (res == VIM_ERROR_TIMEOUT);

            *aid_count -= 1;
            VIM_DBG_WARNING("AAAAAAAAAAAAAAA AID Index AAAAAAAAAAAAAAA");
            VIM_DBG_NUMBER(*aid_count);

            // RDD Trello #69 and #199 : If selected label mentions debit then assign default account at this point.            
            if( CheckDebitLabel( candidate_list[*aid_count].label_name ))
            {
                VIM_DBG_MESSAGE("!!!!!!!!!!!!!!!!!!! Change Account to Default because of DEBIT !!!!!!!!!!!!!!!!!!!");
                txn.account = account_default;
            }

            *aid_count = *aid_count + 1; // ADK EMV expect one-based index
            
            if (res == VIM_ERROR_NONE)
			{
				// RDD 291217 get event deltas for logging
				vim_rtc_get_uptime( &txn.uEventDeltas.Fields.AppResp );

				///DisplayProcessing( DISP_PROCESSING_WAIT );
			}
			return res;
		}
		else if( max == 1 )
		{
			VIM_KBD_CODE key_code=VIM_KBD_CODE_NONE;
			VIM_ERROR_PTR error;
			/* TODO: display confirmation */
			display_screen( DISP_CONFIRM_APPLICATION, VIM_PCEFTPOS_KEY_CANCEL, candidate_list[0].label_name );
			while( VIM_TRUE )
			{
				VIM_ERROR_RETURN_ON_FAIL( EmvCardRemoved(VIM_TRUE) );
				error = screen_kbd_touch_read_with_abort (&key_code, 30000, VIM_FALSE, VIM_TRUE);
				if( error == VIM_ERROR_TIMEOUT )
				{
					display_screen( DISP_CONFIRM_APPLICATION, VIM_PCEFTPOS_KEY_CANCEL, candidate_list[0].label_name );
					vim_kbd_sound_invalid_key ();
					continue;
				}
				VIM_ERROR_RETURN_ON_FAIL( error );
				if( key_code == VIM_KBD_CODE_OK )
				{        
					DisplayProcessing( DISP_PROCESSING_WAIT );        
					return VIM_ERROR_NONE;
				}
				else if( key_code == VIM_KBD_CODE_CANCEL )
				{		         
					DisplayProcessing( DISP_PROCESSING_WAIT );        
					return VIM_ERROR_USER_CANCEL;
				}
				else
					continue;
			}
		}
		else
			return VIM_ERROR_NONE;
	}
}



VIM_ERROR_PTR vim_emv_CB_send_vfi_debug( VIM_UINT16 errorID, VIM_CHAR *Ptr, VIM_UINT32 line )
{
	VIM_CHAR DebugMsg[100];
	vim_mem_clear( DebugMsg, VIM_SIZEOF( DebugMsg ) );

	vim_sprintf( DebugMsg, "VERIX GENAC ERROR: %#x %s, Line %d", errorID, Ptr, line );
	pceft_debug(pceftpos_handle.instance, DebugMsg );
	DBG_MESSAGE( DebugMsg );
	return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_confirm_app( const VIM_EMV_AID *application )
{
  VIM_ERROR_PTR res;

  DBG_MESSAGE("vim_emv_CB_confirm_app");

  VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_CONFIRM_APPLICATION, VIM_PCEFTPOS_KEY_CANCEL, application->label_name));

  res = data_entry_confirm( ENTRY_TIMEOUT, VIM_TRUE, VIM_TRUE );

  if (VIM_ERROR_NONE == res)
    return VIM_ERROR_NONE;
  
  if ((ERR_USER_BACK != res) || (VIM_ERROR_USER_CANCEL != res))
    VIM_ERROR_RETURN(res);
  
  return VIM_ERROR_NONE;
}

// Note only called when building MVT file for apps which use this...
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_terminal_parameters
(
  VIM_EMV_TERMINAL_PARAMETERS *parameters
)
{
  VIM_TEXT tempbuf[8];
  //VIM_DBG_MESSAGE("vim_emv_CB_get_terminal_parameters");

/*TODO:*/
  vim_terminal_get_serial_number(tempbuf, VIM_SIZEOF(tempbuf));
  vim_mem_copy(parameters->ifd_serial_number, tempbuf, VIM_SIZEOF(tempbuf));
  /* Default country : Aus, currency : AUD$ */
  vim_mem_copy(parameters->terminal_country_code, terminal.emv_default_configuration.country_code, VIM_SIZEOF(terminal.emv_default_configuration.country_code)); 
  vim_mem_copy(parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities));
  vim_mem_copy(parameters->additional_terminal_capabilities, terminal.emv_default_configuration.additional_terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.additional_terminal_capabilities));
  parameters->terminal_type = terminal.emv_default_configuration.terminal_type;  
  // RDD 291019 - this was using first two bytes of Add Cap ! 
  vim_mem_copy(parameters->merchant_category_code, terminal.emv_default_configuration.merchant_category_code_bcd,  VIM_SIZEOF(terminal.mcc));
  parameters->pos_entry_mode = POSEM;
#if 1
 VIM_DBG_VAR(parameters->terminal_country_code);
 VIM_DBG_VAR(parameters->terminal_capabilities);
 VIM_DBG_VAR(parameters->additional_terminal_capabilities);
 VIM_DBG_VAR(parameters->terminal_type);
 VIM_DBG_VAR(parameters->merchant_category_code);
 VIM_DBG_VAR(parameters->pos_entry_mode);
 VIM_DBG_VAR(txn.account);
#endif
  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_risk_management_parameters
(
  VIM_EMV_RISK_MANAGEMENT_PARAMETERS *parameters,
  VIM_EMV_AID *aid
)
{
  VIM_UINT8 i;

  //VIM_DBG_MESSAGE("vim_emv_CB_get_risk_management_parameters");
  
  vim_mem_clear(parameters, VIM_SIZEOF(VIM_EMV_RISK_MANAGEMENT_PARAMETERS));

  VIM_DBG_WARNING( " Looking for this AID in our list..." );
  VIM_DBG_DATA( aid->aid, aid->aid_size );

  for (i = 0; i < MAX_EMV_APPLICATION_ID_SIZE; i++)
  {    
	//VIM_DBG_DATA( terminal.emv_applicationIDs[i].emv_aid, terminal.emv_applicationIDs[i].aid_length );

	if (0 == terminal.emv_applicationIDs[i].emv_aid[0])
	{
		VIM_DBG_WARNING( "Corrupt AID in EPAT !!!" );      
		break;
	}

    /* do not use contactless application */
    if (terminal.emv_applicationIDs[i].contactless)
	{
		VIM_DBG_WARNING( "AID is for CTLS" );
		continue;
	}

    if( vim_mem_compare( terminal.emv_applicationIDs[i].emv_aid, aid->aid, aid->aid_size ) == VIM_ERROR_NONE)
    {
      VIM_DBG_WARNING( "AID MATCH - set risk params and return to vfi_emv" );
      txn.aid_index = i;
      VIM_DBG_NUMBER(txn.aid_index);
      VIM_DBG_DATA(terminal.emv_applicationIDs[txn.aid_index].term_cap, 3);
      /* copy icc related data */
      // RDD 270819 Restore this as VISA requires risk analysis and TACs were all zero otherwise

      vim_mem_copy(parameters->terminal_action_code_default, terminal.emv_applicationIDs[i].tac_default, VIM_SIZEOF(terminal.emv_applicationIDs[0].tac_default));
      vim_mem_copy(parameters->terminal_action_code_denial, terminal.emv_applicationIDs[i].tac_denial, VIM_SIZEOF(terminal.emv_applicationIDs[0].tac_denial));
      vim_mem_copy(parameters->terminal_action_code_online, terminal.emv_applicationIDs[i].tac_online, VIM_SIZEOF(terminal.emv_applicationIDs[0].tac_online));
	  
      VIM_DBG_DATA(parameters->terminal_action_code_default,5 );
	  VIM_DBG_DATA(parameters->terminal_action_code_denial, 5);
	  VIM_DBG_DATA(parameters->terminal_action_code_online, 5);

	  // RDD - surcharge apply prior to PIN Block build as modified amount needs to be in Pinblock
	  //if (ApplySurcharge(&txn, VIM_TRUE))
	  //{
	//	  //txn.amount_total += txn.amount_surcharge;
	 // }



      if (tt_preauth == txn.type)
        /* if pre_auth, go online for any amount */
        parameters->terminal_action_code_online[3] |= 0x80;

      parameters->max_target_percent = terminal.emv_applicationIDs[i].max_target;
      parameters->target_percent = terminal.emv_applicationIDs[i].tagert_percent;
      parameters->threshold_amount = terminal.emv_applicationIDs[i].threshold;

	  // RDD 270112 - Modify CTLS and CONTACT FLOOR Limits to Zero if SAF Full - this to force the transaction online
      parameters->floor_limit = saf_full(VIM_FALSE) ? 0 : terminal.emv_applicationIDs[i].floor_limit;

      //VIM_DBG_MESSAGE( "=============== CONTACT limits:===================" );
	  DBG_VAR(parameters->max_target_percent);
	  DBG_VAR(parameters->target_percent);
	  DBG_VAR(parameters->floor_limit);
      //VIM_DBG_MESSAGE( "=============== END CONTACT limits:===================" );

	   ////////// TEST TEST TEST /////////////
	  //parameters->floor_limit = 200000;

      return VIM_ERROR_NONE;
    }
	else
	{
        VIM_DBG_POINT;
	}
  }

  VIM_DBG_WARNING( "Run out of AIDS !!" );

  pceft_debug( pceftpos_handle.instance, "!!! SYSTEM ERROR: (Aid Mismatch) EMV_CB.C L760 !!!!" );    
  VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR emv_get_txn_type(txn_type_t type, VIM_BCD *bcd)
{

  if( type == tt_completion )
  {
    type = ( txn.original_type == tt_none ) ? tt_sale : txn.original_type;
  }

  if ((txn.amount_cash > 0) && (txn.amount_total == txn.amount_cash))
  {
      *bcd = 0x01;
      VIM_DBG_WARNING("----------- Set Cash Only Txn type ------------");
      //return VIM_ERROR_NONE;
  }


  DBG_VAR( type );
  switch(type)
  {
    case tt_cashout: 
        VIM_DBG_WARNING("------------- SET CASHOUT ---------------");
        *bcd = 0x01;
      break;
      
    case tt_sale_cash:
        VIM_DBG_WARNING("------------- SET SALE CASH ---------------");
      *bcd = 0x09;
      break;

    case tt_refund:
        VIM_DBG_WARNING("------------- SET REFUND ---------------");
        *bcd = 0x20;
      break;

  case tt_deposit:
    *bcd = 0x21;
    break;

    //case tt_balance: 
#ifndef WOW
  //case tt_query_card:   // RDD 040913 - added for SUNCORP query card
#endif
   //     *bcd = 0x31;
   //     break;
      
    // RDD 030314 SPIRA:7343 WBC Require Full EMV for Pre-auth
  case tt_preauth:
    *bcd = 0x30;
    break;

  default:  // RDD 231112: EPAL changes add support for EMV txn type. Make All other txn types default to purchase
    *bcd = 0x00;
      break;
            
  }
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_PinEntryHeartBeat( void )
{
	// RDD 120313 - reset the PinEntry HeartBeat to current ticks
	vim_rtc_get_uptime(&LastPinEntryHeartBeat);
	DBG_VAR( LastPinEntryHeartBeat );
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_transaction_data_GPO
(
  VIM_EMV_TRANSACTION_VALUES *parameters,
  VIM_EMV_AID *aid,
  VIM_BOOL TxnGPO
)
{
  VIM_UINT8 i;

  // RDD Setup for Power-up init of ADK 
  emv_aid_parameters_t *aid_ptr = terminal.emv_applicationIDs;

  VIM_DBG_WARNING( " ----- Get GPO Data ------" );
  VIM_DBG_DATA( aid->aid, aid->aid_size );

#ifdef V7 // RDD 240516 Moved to somewhere that always gets called - even if the PIN entry was cancelled
  // RDD 030316 Kernel 7.0 Shuts down the CRYPTO Driver so need to reopen
  if( txn.emv_pin_entry_params.offline_pin )
  {
	    DBG_MESSAGE( " EMV Kernel 7.0 - Reopen CRYPTO Driver " );
		//VIM_ERROR_RETURN_ON_FAIL( vim_tcu_delete( tcu_handle.instance_ptr ));
		VIM_ERROR_WARN_ON_FAIL( vim_tcu_renew_handle( &tcu_handle ));	
  }
#endif

  /* set default terminal capabilities */
  //vim_mem_copy(parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities));
  
  for (i = 0; i < MAX_EMV_APPLICATION_ID_SIZE; i++, aid_ptr++ )
  {
      if (!aid_ptr->emv_aid[0])
      {
          VIM_DBG_PRINTF1("Invalid AID: %d", i);
          break;
      }
    
      /* do not use contactless application */
      if (aid_ptr->contactless)
      {
          VIM_DBG_PRINTF1("CTLS AID: %d", i);
          continue;
      }
    
      // RDD WWBP JIRA WWBP-659 - compare with a length we have in EPAT (partial selection) rather than the extended length from the card
#if 0
      if( vim_mem_compare( aid_ptr->emv_aid, aid->aid, aid->aid_size) == VIM_ERROR_NONE)    
#else
      VIM_DBG_NUMBER( aid->aid_size );
      VIM_DBG_NUMBER( aid_ptr->aid_length );

      if (vim_mem_compare(aid_ptr->emv_aid, aid->aid, aid_ptr->aid_length) == VIM_ERROR_NONE)
#endif
      {      
        /* copy icc related data */
        VIM_DBG_WARNING("GPO - AID_MATCH");
        VIM_DBG_DATA(aid->aid, aid->aid_size);
        txn.aid_index = i;
        VIM_DBG_NUMBER(txn.aid_index);
     
        vim_mem_copy(parameters->default_DDOL, aid_ptr->ddol_default, VIM_SIZEOF(parameters->default_DDOL));      
        vim_mem_copy(parameters->default_TDOL, aid_ptr->tdol_default, VIM_SIZEOF(parameters->default_TDOL));
		// RDD 140714 - WOW v430 SPIRA:7861 Application Version needs to be from EPAT
	  
        vim_mem_copy( &parameters->appication_version_numbers[0], aid_ptr->app_version1, 2 );      
        vim_mem_copy(&parameters->appication_version_numbers[2], aid_ptr->app_version2, 2);      
        vim_mem_copy(&parameters->appication_version_numbers[4], aid_ptr->app_version3, 2);

      
        // RDD 130619 ensure Terminal Capabilities copied over here      
        vim_mem_copy(parameters->terminal_capabilities, aid_ptr->term_cap, VIM_SIZEOF(parameters->terminal_capabilities));
        // RDD 291019 ensurer add term caps also added here
        vim_mem_copy(parameters->additional_terminal_capabilities, aid_ptr->add_term_cap, VIM_SIZEOF(parameters->additional_terminal_capabilities));
      
        VIM_DBG_DATA( parameters->appication_version_numbers,  VIM_SIZEOF(parameters->appication_version_numbers) );      
        VIM_DBG_DATA( parameters->terminal_capabilities, 3 );      
        VIM_DBG_DATA( parameters->default_DDOL, VIM_SIZEOF(parameters->default_DDOL));      
        VIM_DBG_DATA( parameters->default_TDOL, VIM_SIZEOF(parameters->default_TDOL));      
        break;
      }
  }


  if (i >= MAX_EMV_APPLICATION_ID_SIZE)
  {
	//pceft_debug(pceftpos_handle.instance, "!!! SYSTEM ERROR: (Aid Not found) EMV_CB.C L908 !!!!");    
    VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }
#if 0
		VIM_UINT8 CvmBitmap = 0x00;

		for( i=0; i<VIM_SIZEOF(record.appid.data.aid.cvm_list); i++ )
		{
			switch ( record.appid.data.aid.cvm_list[i] & 0x3F )				/* clear the top 2-bits */
			{ /*  see EMV spec for bitmap */
				case 0x01:		/* offline cleartext PIN */
					CvmBitmap |= 0x80;
					break;
				case 0x02:		/* online PIN */
					CvmBitmap |= 0x40;
					break;
				case 0x03:		/* offline cleartext PIN & signature */
					CvmBitmap |= 0x80|0x20;
					break;
				case 0x04:		/* offline enciphered PIN */
					CvmBitmap |= 0x10;
					break;
				case 0x05:		/* offline enciphered PIN & signature */
					CvmBitmap |= 0x10|0x20;
					break;
				case 0x1E:		/* signature */
					CvmBitmap |= 0x20;
					break;
				case 0x1F:		/* no CVM */
					CvmBitmap |= 0x08;  /* not supported by the ICS, but this will be ignored below */
					break;
				default:		/* ignore any other values */
					break;
			}
}

		/* make sure any uncertified CVM bits are ignored (even if sent by the host config) */
		parameters->terminal_capabilities[1] &= CvmBitmap;
#endif
/*  is magstripe fallback on */
    parameters->is_magstripe_fallback_on = VIM_TRUE;
/*  is PIN bybass on */
    parameters->is_pin_bypass_on = aid_ptr->pin_bypass;
/*  cashout flag. Not used by core. */
    file41_emv_cashout_flag = VIM_TRUE;

    if (TxnGPO)
    {
        // RDD 090813 - add account type for EPAL cards
        if (vim_mem_compare(aid->aid, "\xA0\x00\x00\x03\x84\x10", 6) == VIM_ERROR_NONE)
        {
            DBG_MESSAGE("Setting account type - SAVINGS for GPO ");

            // RDD 240516 SPIRA:8967 - Need to Display Processing once returned from PIN entry if no error
            DisplayProcessing(DISP_PROCESSING_WAIT);

            parameters->account_type = 0x10;
        }
        if (vim_mem_compare(aid->aid, "\xA0\x00\x00\x03\x84\x20", 6) == VIM_ERROR_NONE)
        {
            DBG_MESSAGE("Setting account type - CHEQUE for GPO ");
            parameters->account_type = 0x20;
            // RDD 240516 SPIRA:8967 - Need to Display Processing once returned from PIN entry if no error
            DisplayProcessing(DISP_PROCESSING_WAIT);
        }
    }
  // RDD 170912 P3 Fixed  for international useage - this was previously hard-coded to Australia
  vim_mem_copy(parameters->transaction_currency_code, terminal.emv_default_configuration.txn_reference_currency_code, VIM_SIZEOF(terminal.emv_default_configuration.txn_reference_currency_code));  
  DBG_DATA(parameters->transaction_currency_code, 2 );

  parameters->transaction_currency_exponent = terminal.emv_default_configuration.txn_reference_currency_exponent;
  
  vim_mem_copy(parameters->transaction_ref_currency_code, terminal.emv_default_configuration.txn_reference_currency_code, VIM_SIZEOF(terminal.emv_default_configuration.txn_reference_currency_code));
  DBG_DATA(parameters->transaction_ref_currency_code, 2 );

  parameters->transaction_ref_currency_exponent = parameters->transaction_currency_exponent;

  parameters->transaction_category_code[0] = terminal.tcc;
  /*emv_convert_date_to_rtc(&txn.time, &parameters->transaction_time);*/
  vim_mem_copy(&parameters->transaction_time, &txn.time, VIM_SIZEOF(VIM_RTC_TIME));
  vim_mem_copy(parameters->merchant_identifier, terminal.merchant_id, VIM_SIZEOF(parameters->merchant_identifier));

  parameters->amount_authorised = (VIM_UINT32)txn.amount_total;

  // RDD 281019 JIRA WWBP-754 : Transaction type 01 not supported for TAV
  parameters->amount_other = (VIM_UINT32)txn.amount_cash;
  //parameters->amount_other = 0;
  emv_get_txn_type(txn.type, &parameters->transaction_type);

  return VIM_ERROR_NONE;
}
/**
  *Display a dialog to choose EMV account
*/
/*----------------------------------------------------------------------------*/




VIM_BOOL partial_emv(account_t account)
{  
    // RDD 210720 Trello #82 Allow Full EMV refunds for Mastercard and VISA if enabled via LFD
    if( txn.type == tt_refund )
    {
        if ( AllowFullEMVRefund(&txn) == VIM_TRUE )
        {
            VIM_DBG_MESSAGE("RRRRRRRRRRRRR Allow Full EMV Refund for MC or VISA RRRRRRRRRRRR");
            return VIM_FALSE;
        }
    }


  ////////////// TEST TEST TEST /////////////////
  // RDD 170912 - in order to get hold of the IAD tag we need to fudge a Gen Ac until the 0xBF0C issue is resolved with Verifone...
  if( txn.type == tt_query_card ) 
  {
      DBG_MESSAGE( "QQQQQQQQQQ << J8 QUERY CARD: PARTIAL EMV SET FALSE >> QQQQQQQQQQQQ" );
	  return VIM_FALSE;
  }
  // Always full emv on EPAL cards
  if( (is_EPAL_cards (&txn)) && (txn.type != tt_refund) )				//BD EPAL uses partial for refund
  {    
	  DBG_MESSAGE( "EEEEEEEEE << EPAL CARD: PARTIAL EMV SET FALSE >> EEEEEEEEEE" );    
	  return VIM_FALSE;
  }
  // Partial emv on all non-purchase and non-cashout transactions except for EPAL
  if(( txn.type != tt_sale ) && ( txn.type != tt_cashout ) && ( txn.type != tt_sale_cash ) && ( txn.type != tt_preauth ) && ( txn.type != tt_refund ))
  {
      DBG_MESSAGE( "TTTTTTTTTTTT Invalid Txn type for EMV SET Partial TRUE" );
	  DBG_VAR(txn.type);
	  return VIM_TRUE;
  }
  // RDD 030820 Trello #69 and #199 - No Credit for Scheme Debit - use default account instead
  if(( account == account_credit )||( account == account_default ))
  {
	  return VIM_FALSE;
  }
  // Do partial EMV on anything else

  DBG_MESSAGE( "PPPPPPPPPPPPP PARTIAL EMV SET TRUE >> PPPPPPPPPPPPPPP" );	
  return VIM_TRUE;

}



// RDD 250213 - rewritten for WOW phase 4
void emv_set_pin_entry_type(  txn_pin_entry_type *pin_entry_type  )
{
	DBG_MESSAGE( "<<< IN EMV SET PIN ENTRY TYPE >>>" );
	// WOW: PIN Entry Mandatory for International Cards (we think...)

	if( !txn.emv_pin_entry_params.max_digits )
	{	
		VIM_DBG_WARNING( "OK" );
		*pin_entry_type = txn_pin_entry_press_ok;
	}
	else
	{
		emv_is_card_international();
		*pin_entry_type = ( terminal.emv_applicationIDs[txn.aid_index].pin_bypass ) ?  txn_pin_entry_optional : txn_pin_entry_mandatory; 
	}
	DBG_VAR( *pin_entry_type );
}



/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR get_agc_or_select_account(VIM_UINT8 available_accounts)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
   	
  VIM_ERROR_RETURN_ON_FAIL( card_name_search(&txn, &txn.card) );
  DBG_POINT;
   
  // Attempt to set the account type from the CPAT AGC
  if( !set_acc_from_agc( &txn.account ))
  {	   
	  DBG_MESSAGE("No fixed account in AGC. Need to let user select" );

      if (WOW_NZ_PINPAD)
      {
          txn_select_nz_account(VIM_TRUE);
      }
      else
      {
          txn_select_account(VIM_TRUE);
      }
	  if( txn.error != VIM_ERROR_NONE )
	  {
		  DBG_MESSAGE("<<<< ACCOUNT SELCTION CANCELLED >>>>>" );
		  return txn.error;
	  }
  }
  else
  {
	  DBG_MESSAGE("Got EMV Account type from CPAT AGC" );
  }
  DBG_MESSAGE("ACCOUNT TYPE SELECTED" );
  DBG_VAR(txn.account);
  return res;
}



VIM_ERROR_PTR GetEPALAccount(void)
{  
	/* special processing for EPAL */
	if( vim_mem_compare( terminal.emv_applicationIDs[txn.aid_index].emv_aid, "\xA0\x00\x00\x03\x84\x10", 6 ) == VIM_ERROR_NONE )  
	{	  
		DBG_MESSAGE("Got SAV EMV Account type from EPAL AID" );    
		txn.account = account_savings;  
		return VIM_ERROR_NONE;
	}  
	else if( vim_mem_compare( terminal.emv_applicationIDs[txn.aid_index].emv_aid, "\xA0\x00\x00\x03\x84\x20", 6 ) == VIM_ERROR_NONE )  
	{	  
		DBG_MESSAGE("Got CHQ EMV Account type from EPAL AID" );    
		txn.account = account_cheque;  
		return VIM_ERROR_NONE;
	}
    DBG_VAR( txn.account );
	return VIM_ERROR_NOT_FOUND;
}


//Attempt to Set EMV account using available accounts in the AID table
VIM_ERROR_PTR select_emv_account(void)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_UINT8 account_count = 0;

  DBG_MESSAGE( "select_emv_account" );

  // We shouldn't get to this point if the account type was sent over by the POS
  // RDD 070512 SPIRA:5448 - PinPad was not prompting for account type for emv completions
  // RDD 170812 SPIRA:xxx - use CPAT to determine emv account
   
  if( emv_is_card_international() )
  {
    return get_agc_or_select_account(0);
  }

  account_count = ( terminal.emv_applicationIDs[txn.aid_index].aid_preset_acc );
  DBG_VAR(txn.aid_index);
  DBG_VAR(account_count);

  GetEPALAccount();

  /* set account if only one is available or get it from the dialog */
  switch (account_count)
  {      
    case CHEQUE_ACCOUNT:
		DBG_MESSAGE( "EPAT says Default CHQ account");
      txn.account = account_cheque;
      break;

    case SAVINGS_ACCOUNT:
		DBG_MESSAGE( "EPAT says Default SAV account");
      txn.account = account_savings;
      break;

    case CREDIT_ACCOUNT:
		DBG_MESSAGE( "EPAT says Default CR account");
      txn.account = account_credit;
      break;

    default:
      /* get account type from dialog */
		DBG_MESSAGE( "EPAT says No default Account so let user select");
		txn.account = account_unknown_any;
		res = get_agc_or_select_account(account_count);

		break;
  }
  return res;
}



/* Read a set of tags from the kernel and update the tag list in txn structure with the results */
static VIM_ERROR_PTR emv_update_txn_tags
(
  /** List of tag ids to read from emv kernel */
  const VIM_TAG_ID tag_ids[], 
  /** Number of tag ids in tag_ids list*/
  VIM_SIZE tag_count
)
{
  VIM_TLV_LIST new_tags, existing_tags;
  VIM_UINT8 tag_buffer[512];

  // RDD 310113 - Update for new kernel. Need to check if docked card is active before we call this..
  if( !txn.emv_enabled )
  {
	  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< ERROR: TRIED TO READ EMV TAGS WHEN NO ACTIVE EMV TXN >>>>>>>>>>>>>>>>>>>>>");
	  return VIM_ERROR_NOT_FOUND;
  }
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&new_tags, tag_buffer, VIM_SIZEOF(tag_buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_read_kernel_tags(tag_ids, tag_count, &new_tags));

  /* Open existing tags or create new tag if none exist */
	//DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");  
	//DBG_VAR( txn.emv_data.data_size );
	//DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);  
	//DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");

  if (txn.emv_data.data_size)
  {
	  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< OPEN EXISTING BUFFER >>>>>>>>>>>>>>>>>>>>>");
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&existing_tags, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE));
  }
  else
  {
	  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< CREATE NEW BUFFER >>>>>>>>>>>>>>>>>>>>>");
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create( &existing_tags, txn.emv_data.buffer, VIM_SIZEOF(txn.emv_data.buffer)) );
  }
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_rewind(&new_tags));

  while (vim_tlv_goto_next(&new_tags) == VIM_ERROR_NONE)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(&existing_tags, new_tags.current_item.tag, new_tags.current_item.value, new_tags.current_item.length));
  }
    	
  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER UPDATE !!!!!!!!!!!!!!!!");  
  DBG_VAR( txn.emv_data.data_size );	
  DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);  	
  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER UPDATE !!!!!!!!!!!!!!!!");


  txn.emv_data.data_size = existing_tags.total_length;

  return VIM_ERROR_NONE;
}

#if 0
VIM_ERROR_PTR emv_get_account( account_t *account )
{
  VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;

#ifdef AUTO_EMV_DEBUG
	*account_type = account_credit;
	res = VIM_ERROR_USER_CANCEL;
#else

  // If the account type is already known then exit
  if(( txn.account ) && ( txn.pos_account ))
  {
	  DBG_MESSAGE( "----------- POS ALREADY SET ACCOUNT ------- ");
	  res = VIM_ERROR_NONE;
  }

  // RDD 150313 new EPAL cards
  else if((( txn.account == account_savings ) || (  txn.account == account_cheque )) && ( is_EPAL_cards( &txn ) ))
  {	   
	  // RDD 200115 SPIRA:8354 - deal with preauths including case where SAV or CHQ selected via Application selection  
	  if( txn.type == tt_preauth )  
	  {	
		  TXN_RETURN_ERROR( ERR_CARD_NOT_ACCEPTED );
	  }
  }

	  DBG_MESSAGE( "----------- EPAL EMV ACCOUNT ALREADY SET ------------" );
	  res = VIM_ERROR_NONE;
  }
  else
  {  
	  res = select_emv_account();
  }
#endif
  *account = txn.account;
  VIM_DBG_VAR( *account );
  VIM_DBG_ERROR( res );
  return res;
}

#else
VIM_ERROR_PTR emv_get_account( account_t *account )
{
  VIM_BOOL epal_in_candidate_list = VIM_FALSE;
  VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;

  // If the account type is already known then exit
  if(( txn.account ) && ( txn.pos_account ))
  {    
	  DBG_MESSAGE( "----------- POS ALREADY SET ACCOUNT ------- ");  
	  if(( txn.type == tt_preauth || txn.type == tt_completion || txn.type == tt_checkout ) && ( txn.account == account_savings || txn.account == account_cheque ))
	  {
		  VIM_DBG_VAR(txn.account);
		  DBG_MESSAGE( "EPAL Card - Preauth or completion not allowed");
		  return ERR_CARD_NOT_ACCEPTED;
	  }
	  res = VIM_ERROR_NONE;
  }

  // RDD 150313 new EPAL cards
  else if((( txn.account == account_savings ) || (  txn.account == account_cheque )) && ( is_EPAL_cards( &txn ) ))
  {    
	  DBG_MESSAGE( "----------- EPAL EMV ACCOUNT ALREADY SET ------------" );    
	  res = VIM_ERROR_NONE;
  }
  else
  {
	  // RDD 161014 SPIRA:8193 Skip Account selection if non-epal applcation was selected
    vim_emv_is_epal_in_candidate_list( &epal_in_candidate_list );
	VIM_DBG_VAR(epal_in_candidate_list);    
    if( epal_in_candidate_list && !is_EPAL_cards( &txn ) )
    { // EPAL Appliaction was offered, but not selected. Force "Credit" account
		DBG_MESSAGE( "EPAL Was in the candidate list and wasn't selected so force credit" );      
		txn.account = account_credit;      
		res = VIM_ERROR_NONE;
    }
    else
    {  
      res = select_emv_account();
    }
  }
  *account = txn.account;
  VIM_DBG_VAR( *account );
  VIM_DBG_ERROR( res );
  return res;
}
#endif


/*----------------------------------------------------------------------------*/
// RDD 310113 - re-written because the previous version was inadequate for use with the new kernel
VIM_ERROR_PTR emv_get_aid( emv_aid_parameters_t* tmp_aid_param_ptr, VIM_BOOL read_from_kernel )
{
    const VIM_TAG_ID txn_aid_tags[] = { VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_TAG_EMV_AID_TERMINAL };
  
    VIM_TLV_LIST tlv_list;
    VIM_UINT8 tlv_buffer[100];
  
    // RDD 301013 SPIRA:6987 Customer Wrongly charged
    vim_mem_clear( tlv_buffer, VIM_SIZEOF(tlv_buffer) );
  
    DBG_VAR(txn.emv_data.data_size);

    // RDD 310113 - Update for new kernel. Need to check if docked card is active before we call this else pinpad will hang up
    if( read_from_kernel )  
    {
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create ( &tlv_list, tlv_buffer, VIM_SIZEOF(tlv_buffer) ));    
	  // get AID from kernel for current TXN  
	  VIM_ERROR_RETURN_ON_FAIL( vim_emv_read_kernel_tags( txn_aid_tags, 1, &tlv_list ));  
    }  
    else  
    {	
	  // We're not running a current emv txn ( might be processing a SAF ) so try to pick up the AID from the txn list.
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tlv_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE ));  
    }
  
    // RDD if UPI then use the Dedicated Filename as AID because the AID doesn't have the last byte which tells us debit, credit or quasi credit !
    if (SetTxnAIDIndex(&tlv_list, VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_FALSE ) != VIM_ERROR_NONE)  
    {
      if (SetTxnAIDIndex(&tlv_list, VIM_TAG_EMV_AID_TERMINAL, VIM_FALSE) != VIM_ERROR_NONE)
      {
          pceft_debug_error(pceftpos_handle.instance, "SYSTEM ERROR: (AID NOT FOUND) !!!!", VIM_ERROR_SYSTEM);
          VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
      }  
    }
  
    tmp_aid_param_ptr = &terminal.emv_applicationIDs[txn.aid_index];
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR emv_get_current_transaction_AID_param( emv_aid_parameters_t* tmp_aid_param_ptr )
{
  //VIM_SIZE cnt=0;
  //VIM_UINT8 aid_len=0;

  VIM_ERROR_RETURN_ON_FAIL( emv_get_aid( tmp_aid_param_ptr, VIM_TRUE ));
  return VIM_ERROR_NONE;

}

// RDD 201112 SPIRA:xxxx This function was created by splitting up the one below as it was found that 
// GPO params needed to be set before ATR or app selection in order to get the correct txn type etc. to the card

#if 0// ADK Version
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_set_transaction_data(VIM_EMV_TRANSACTION_VALUES *parameters)
{
    //VIM_DBG_MESSAGE("vim_emv_CB_set_transaction_data");

    //return VIM_ERROR_NONE;

    emv_get_txn_type(txn.type, &parameters->transaction_type);
    parameters->amount_authorised = (VIM_UINT32)txn.amount_total;
    parameters->amount_other = (VIM_UINT32)txn.amount_cash;

    VIM_DBG_DATA( parameters->amount_authorised, VIM_SIZEOF(parameters->amount_authorised ));
    VIM_DBG_DATA( parameters->amount_other, VIM_SIZEOF(parameters->amount_authorised ));
    VIM_DBG_NUMBER( parameters->transaction_type );

    // RDD 270213 - ensure that the default capabilities are restored at the start of each txn
    vim_mem_copy(parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities));

    vim_utl_uint64_to_bcd(terminal.stan, parameters->terminal_transaction_counter, VIM_SIZEOF(parameters->terminal_transaction_counter) * 2);

    // parameters->automatic_app_sel = 0; // in APO the PINPAD is present, no Auto App Selection                                       
    // parameters->card_in_blacklist = VIM_FALSE;  // RDD - not used for WOW

    return VIM_ERROR_NONE;
}
#else   // old version

VIM_ERROR_PTR vim_emv_CB_set_transaction_data( VIM_EMV_TRANSACTION_VALUES *parameters )
{
  DBG_MESSAGE("vim_emv_CB_set_transaction_data");
  	
  parameters->automatic_app_sel = 0; // in APO the PINPAD is present, no Auto App Selection                                       
  parameters->card_in_blacklist = VIM_FALSE;  // RDD - not used for WOW

  /* set transasction type */
  VIM_ERROR_RETURN_ON_FAIL(emv_get_txn_type(txn.type, &parameters->transaction_type));
  parameters->amount_authorised = (VIM_UINT32)txn.amount_total;	  
  parameters->amount_other = (VIM_UINT32)txn.amount_cash;	

  VIM_DBG_NUMBER(txn.amount_purchase);
  VIM_DBG_NUMBER(txn.amount_cash);
  VIM_DBG_NUMBER(txn.amount_total);


  // RDD - not used for WOW
  parameters->card_in_blacklist = VIM_FALSE;

  //DBG_DATA(parameters->amount_authorised, VIM_SIZEOF( parameters->amount_authorised ));
  //DBG_DATA( parameters->amount_other, VIM_SIZEOF( parameters->amount_authorised ));
  //DBG_VAR( parameters->transaction_type ); 

  // RDD 270213 - ensure that the default capabilities are restored at the start of each txn
  vim_mem_copy( parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities));
  VIM_DBG_DATA( parameters->terminal_capabilities, VIM_SIZEOF( parameters->terminal_capabilities ));

  return VIM_ERROR_NONE;
}

#endif


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_PDOL_data( VIM_EMV_TRANSACTION_VALUES *parameters )
{	 
	emv_aid_parameters_t tmp_aid_param; 

	//VIM_DBG_MESSAGE("vim_emv_CB_get_PDOL_data");
  
	vim_mem_clear( &tmp_aid_param, VIM_SIZEOF(tmp_aid_param));
  
	// RDD 140313 SPIRA:6646 -  New EPAL cards with account type in PDOL.  
	// We need to ensure that the account type is set according to the AID as that is the only info we have for EPAL   
	VIM_ERROR_RETURN_ON_FAIL( emv_get_current_transaction_AID_param( &tmp_aid_param ));
	 
	// We can only set the account here if we can determine it from the AID - assumes credit unless otherwise specified
#if 1
	// RDD 090813 - add account type for EPAL cards
	if( vim_mem_compare( tmp_aid_param.emv_aid, "\xA0\x00\x00\x03\x84\x10", 6 ) == VIM_ERROR_NONE )
	{
		DBG_MESSAGE( "Setting account type - SAVINGS for GPO ");
		parameters->account_type = 0x10;
	}
	if( vim_mem_compare( tmp_aid_param.emv_aid, "\xA0\x00\x00\x03\x84\x20", 6 ) == VIM_ERROR_NONE )
	{
		DBG_MESSAGE( "Setting account type - CHEQUE for GPO ");
		parameters->account_type = 0x20;
	}
#else

	GetEPALAccount();
	emv_set_account_parameter(txn.account, &parameters->account_type);		  
#endif
	DBG_VAR( parameters->account_type );
	return VIM_ERROR_NONE;
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_transaction_data( VIM_EMV_TRANSACTION_VALUES *parameters )
{
  emv_aid_parameters_t tmp_aid_param;
  txn_state_t card_check_result;

  /*  "Read Application Data" step is done */	
  DBG_MESSAGE("vim_emv_CB_get_transaction_data");

  vim_mem_clear( &tmp_aid_param, VIM_SIZEOF(tmp_aid_param));

  // RDD 140313 SPIRA:6646 -  New EPAL cards with account type in PDOL.
  // We need to ensure that the account type is set according to the AID as that is the only info we have for EPAL 
  VIM_ERROR_RETURN_ON_FAIL( emv_get_current_transaction_AID_param( &tmp_aid_param ));
	
  /*  no fallback to MSR after this point */	
  emv_fallbak_step_passed = VIM_TRUE;
		
  // Read Track2 and associated card data  	
  VIM_ERROR_RETURN_ON_FAIL( emv_get_track_2(&txn) );
    	
  // Read Track1 and ignore any errors as its a optional	
  // RDD 301013 SPIRA:6987 Customer Wrongly charged
  // 301013 - DG Says that she's pretty sure this isn't used so get rid for safety
  //emv_get_track_1(&txn);
	   	
  //run card check	
  // RDD 020212 - Luhn check failure returns card capture state so this needs to be handled too	
  card_check_result = txn_card_check();	
  if( card_check_result == ts_error || card_check_result == ts_card_capture || card_check_result == ts_finalisation )	
  {   
	  DBG_MESSAGE( "EMV Card Check Failed" );    
	  VIM_DBG_ERROR( txn.error );

      if (txn.error == VIM_ERROR_NONE)
      {
          VIM_ERROR_RETURN( VIM_ERROR_EMV_TXN_ABORTED );
      }
      else
      {
          VIM_ERROR_RETURN( txn.error );
      }
  }
	
  // RDD 140912 Special Processing for Rimu Card
  if( txn.type == tt_query_card )	
  {		 	
	  return( get_loyalty_iad( &txn ));	
  }

  VIM_ERROR_RETURN_ON_FAIL( emv_get_account(&txn.account) );  
	

  // RDD added to put App Label data into txn.emv_data buffer in order to display on screen  	
  VIM_ERROR_RETURN_ON_FAIL(emv_update_txn_tags(tags_to_display, VIM_LENGTH_OF(tags_to_display)));

  if( ((txn.account == account_credit) || (txn.account == account_default)) && ( !CARD_NAME_ALLOW_CASH_ON_CR_ACC ) && txn.amount_cash )		//BRD No cashout on default CR a/c	
  {  
	  DBG_MESSAGE( "Error : Cashout on Credit disabled for this card type in CPAT" );      
	  VIM_ERROR_RETURN(ERR_NO_CASHOUT);	
  }
  
  if( partial_emv(txn.account) )	
  {	
	  VIM_DBG_MESSAGE( "+++++++++++ processing as partial EMV ++++++++++++" );	

	  // RDD 061015 SPIRA:8822 - if partial we need to EXIT emv callbacks and return to normal processing
	  transaction_set_status(&txn, st_partial_emv);	
	  return VIM_ERROR_EMV_PARTIAL_TXN;	
  }
    

  DBG_MESSAGE( "processing as FULL EMV ++++++++++++" );	
  transaction_clear_status(&txn, st_partial_emv);

  /* check SVP */
  // RDD 120712 SPIRA:5737 - don't allow SVP for small values if there is a cash component
  // RDD 011214 SPIRA:8264 Do full QPS checking before changing the TCC

  // RDD 310315 v541-6 SPIRA:8544 add new param: qps_term_cap
  /* set default terminal capabilities */
  
  VIM_DBG_NUMBER(txn.aid_index);
  VIM_DBG_DATA(terminal.emv_applicationIDs[txn.aid_index].term_cap, 4);

  if (terminal.emv_applicationIDs[txn.aid_index].term_cap[0] == 0x00)
  {
      VIM_DBG_WARNING("Zero Term Caps -  : Use default");
      vim_mem_copy(parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(tmp_aid_param.term_cap));
  }
  else
  {
      vim_mem_copy(parameters->terminal_capabilities, terminal.emv_applicationIDs[txn.aid_index].term_cap, VIM_SIZEOF(tmp_aid_param.term_cap));
      VIM_DBG_DATA(parameters->terminal_capabilities, 3);
  }

  /* set default terminal capabilities */
  // RDD 090916 NO NO NO - copies over EPAT AID values !
  //vim_mem_copy(parameters->terminal_capabilities, terminal.emv_default_configuration.terminal_capabilities, VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities));
  if( qualify_svp( ))
  {
	  txn.qualify_svp = VIM_TRUE;	
	  VIM_DBG_WARNING( "QPS Qualified! ");
      VIM_DBG_NUMBER(txn.aid_index);
	  vim_mem_copy(parameters->terminal_capabilities, terminal.emv_applicationIDs[txn.aid_index].qps_term_cap, VIM_SIZEOF(tmp_aid_param.term_cap));

  }

  // RDD 210219 Start P400 - need txn amount for driver layer
  parameters->amount_authorised = txn.amount_total;
  parameters->amount_other = txn.amount_cash;
  // RDD 210219 End P400 - need txn amount for driver layer



  DBG_DATA(parameters->terminal_capabilities, 3);  
  /* set SVP qualified flag */      
    
  DBG_MESSAGE( "Exit EMV CB Get Txn Data ++++++++++++" );

  /* return without error */
  DBG_VAR(txn.emv_data.data_size);	
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR add_record_data( VIM_UINT8 **dest, VIM_SIZE *dest_remlen, VIM_UINT8 *src_ptr, VIM_SIZE src_len )
{
	if( *dest_remlen < src_len )
		return VIM_ERROR_OVERFLOW;

	if( src_len )
	{
		vim_mem_copy( *dest, src_ptr, src_len );
		*dest += src_len;
		*dest_remlen -= src_len;
	}
	return VIM_ERROR_NONE;
}

//static VIM_EMV_CAPK *local_capk_storage = VIM_NULL;

// RDD 130519 added CAPK Support JIRA WWBP-596
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_CAPK_all( VIM_EMV_CAPK *capk[], VIM_SIZE *capk_count, VIM_SIZE max_capk_count)
{

    VIM_SIZE out_capk_idx = 0;
    VIM_SIZE in_capk_idx = 0;

    VIM_DBG_NUMBER( max_capk_count );

    VIM_DBG_WARNING(" ------------- vim_emv_CB_get_CAPK_all --------------- ");
    VIM_ERROR_RETURN_ON_NULL(capk);
    VIM_ERROR_RETURN_ON_NULL(capk_count);

    VIM_DBG_NUMBER(max_capk_count);

    *capk_count = 0;

    while (in_capk_idx < VIM_SIZEOF(terminal.emv_capk) / VIM_SIZEOF(terminal.emv_capk[0]) &&
        out_capk_idx < max_capk_count )
    {
        VIM_DBG_PRINTF1("Terminal PKT Record: %d", in_capk_idx);
        if ( !terminal.emv_capk[in_capk_idx].capk.RID.value[0] )
        {
            in_capk_idx++;
            VIM_DBG_WARNING( "----------- CAPK Not Found in terminal Structure --------------" )
            continue;
        }
        else
        {
            VIM_DBG_DATA( terminal.emv_capk[in_capk_idx].capk.RID.value, VIM_SIZEOF(VIM_ICC_RID_LEN));
            VIM_DBG_VAR( terminal.emv_capk[in_capk_idx].capk.index );
        }

        if( !terminal.emv_capk[in_capk_idx].capk.hash[0] )
        {
            picc_generate_capk_checksum( &terminal.emv_capk[in_capk_idx].capk );
        }
        capk[out_capk_idx] = &terminal.emv_capk[in_capk_idx].capk;

        in_capk_idx++;
        out_capk_idx++;
        *capk_count = *capk_count + 1;
    }
    return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_CAPK
(
  VIM_EMV_AID *rid,
  VIM_UINT8 capk_index,
  VIM_EMV_CAPK *capk
)
{
  VIM_UINT32 i;

  // DBG_MESSAGE("vim_emv_CB_get_CAPK");
  //VIM_DBG_VAR(rid->aid);
  //VIM_DBG_VAR(capk_index);

  vim_mem_clear(capk, VIM_SIZEOF(VIM_EMV_CAPK));

  for (i = 0; i < MAX_EMV_PUBLIC_KEYS; i++)
  {
    if ((vim_mem_compare(rid->aid, &terminal.emv_capk[i].capk.RID.value, VIM_SIZEOF(terminal.emv_capk[i].capk.RID.value)) == VIM_ERROR_NONE)
      && (capk_index == terminal.emv_capk[i].capk.index))
    {
      // DBG_MESSAGE("CAPK FOUND");
      //DBG_VAR(i);

      vim_mem_copy(capk->RID.value, &terminal.emv_capk[i].capk.RID.value, VIM_SIZEOF(capk->RID.value));    
      capk->index = terminal.emv_capk[i].capk.index;
      capk->modulus_size = terminal.emv_capk[i].capk.modulus_size;
      //VIM_DBG_VAR(capk->modulus_size);
      vim_mem_copy(capk->modulus, terminal.emv_capk[i].capk.modulus, capk->modulus_size);
      capk->exponent_size = terminal.emv_capk[i].capk.exponent_size;
      vim_mem_copy(capk->exponent, terminal.emv_capk[i].capk.exponent, capk->exponent_size);
      if (terminal.emv_capk[i].capk.expiry_date.day)
        capk->expiry_date = terminal.emv_capk[i].capk.expiry_date;
      vim_mem_copy(capk->hash, terminal.emv_capk[i].capk.hash, VIM_SIZEOF(capk->hash));

      //VIM_DBG_DATA(capk->RID.value, VIM_SIZEOF(capk->RID.value));
      //VIM_DBG_DATA(&capk->index, VIM_SIZEOF(capk->index));
      //VIM_DBG_DATA(&capk->modulus_size, VIM_SIZEOF(capk->modulus_size));
      //VIM_DBG_DATA(capk->modulus, capk->modulus_size);
      //VIM_DBG_DATA(&capk->exponent_size, VIM_SIZEOF(capk->exponent_size));
      //VIM_DBG_DATA(capk->exponent, capk->exponent_size);
      //VIM_DBG_DATA(&capk->expiry_date.day, VIM_SIZEOF(capk->expiry_date.day));
      //VIM_DBG_DATA(&capk->expiry_date.month, VIM_SIZEOF(capk->expiry_date.month));
      //VIM_DBG_DATA(&capk->expiry_date.year, VIM_SIZEOF(capk->expiry_date.year));
      //VIM_DBG_DATA(capk->hash, VIM_SIZEOF(capk->hash));
      break;
    }
    /* No more keys */    
    if (0 == terminal.emv_capk[i].capk.RID.value[0])
    {
      // DBG_MESSAGE("!!! NO MORE KEYS: INVALID RID !!!");
      VIM_ERROR_RETURN(VIM_ERROR_EMV_CAPK_NOT_FOUND);
    }
  }

  if (i >= MAX_EMV_PUBLIC_KEYS)
    VIM_ERROR_RETURN(VIM_ERROR_EMV_CAPK_NOT_FOUND);
    
  return VIM_ERROR_NONE;
}
#if 0
/*----------------------------------------------------------------------------*/
typedef struct
{
  VIM_UINT8 rule;       /* top bit also indicates what to do on failure */
  VIM_UINT8 condition;
}CVM_RULE;

typedef struct
{
  VIM_UINT8 amount_x[4];
  VIM_UINT8 amount_y[4];
  CVM_RULE cvm[1];    /* Array of CVMs up to the maximum length of the cvm list */
}CVM_LIST;
/*----------------------------------------------------------------------------*/
static VIM_BOOL emv_cvm_supported(VIM_UINT8 cvm, VIM_UINT8 term_capabilities[3])
{
  switch (cvm)
  {
    case VIM_EMV_CVM_FAIL_CVM:
      return VIM_TRUE;
      
    case VIM_EMV_CVM_OFFLINE_PLAINTEXT_PIN:
      return VIM_EMV_TERM_CAP_PLAINTEXT_OFFLINE_PIN(term_capabilities);
      
    case VIM_EMV_CVM_ONLINE_PIN:
      return VIM_EMV_TERM_CAP_ONLINE_PIN(term_capabilities);

    case VIM_EMV_CVM_OFFLINE_PLAIN_PIN_AND_SIGNATURE:
      return (VIM_EMV_TERM_CAP_PLAINTEXT_OFFLINE_PIN(term_capabilities) && VIM_EMV_TERM_CAP_SIGNATURE(term_capabilities));

    case VIM_EMV_CVM_OFFLINE_ENCRYPTED_PIN:
      return VIM_EMV_TERM_CAP_ENCRYPTED_OFFLINE_PIN(term_capabilities);
      
    case VIM_EMV_CVM_OFFLINE_ENC_PIN_AND_SIGNATURE:
      return (VIM_EMV_TERM_CAP_ENCRYPTED_OFFLINE_PIN(term_capabilities) && VIM_EMV_TERM_CAP_SIGNATURE(term_capabilities));

    case VIM_EMV_CVM_SIGNATURE:
      return VIM_EMV_TERM_CAP_SIGNATURE(term_capabilities);

    case VIM_EMV_CVM_NO_CVM_REQUIRED:
      return VIM_EMV_TERM_CAP_NO_CVM(term_capabilities);
  }
  
  return VIM_FALSE;
}
/*----------------------------------------------------------------------------*/
/**
  Check if another CVM will be available if the current one fails. Used for checking if PIN bypass should be allowed.
*/
static VIM_ERROR_PTR emv_check_more_cvm_available
(
  /** Currently selected CVM method */
  VIM_UINT8 current_cvm_method,
  /**[out] Will be set to VIM_FALSE if no more CVM methods are available, VIM_TRUE otherwise */
  VIM_BOOL *more_cvm_available
)
{
  VIM_UINT32 x_value, y_value;
  VIM_BOOL cvm_applicable, local_currency;
  VIM_SIZE cvm_count, i, cvm_list_length;
  VIM_UINT8 terminal_capabilities[3], tag_buffer[256];
  VIM_TLV_LIST tags;
  CVM_LIST *cvm_list;
  const VIM_TAG_ID retrieve_tags[] = 
  {
    VIM_TAG_EMV_CVM_LIST,
    VIM_TAG_EMV_TERMINAL_CAPABILITIES,
    VIM_TAG_EMV_APPLICATION_CURRENCY_CODE
  };

  *more_cvm_available = VIM_FALSE;
  
  /* Retrieve all the data we need to check if another cvm is available */
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tags, tag_buffer, VIM_SIZEOF(tag_buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_read_kernel_tags(retrieve_tags, VIM_LENGTH_OF(retrieve_tags), &tags));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_search(&tags, VIM_TAG_EMV_CVM_LIST));
  cvm_list = (CVM_LIST *) tags.current_item.value;
  cvm_list_length = tags.current_item.length;
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_search(&tags, VIM_TAG_EMV_TERMINAL_CAPABILITIES));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, terminal_capabilities, VIM_SIZEOF(terminal_capabilities)));
  local_currency = VIM_FALSE;
  /* It is not an error if currency code tag not present, this just affects the way we parse the CVM list */
  if (vim_tlv_search(&tags, VIM_TAG_EMV_APPLICATION_CURRENCY_CODE) == VIM_ERROR_NONE)
  {
    if (!vim_mem_compare(tags.current_item.value, VIM_EMV_COUNTRY_CODE_AUSTRALIA, 2))
      local_currency = VIM_TRUE;
  }

  /* First check the "apply next CVM if current CVM fails" bit in current CVM */
  if (!(current_cvm_method & VIM_EMV_CVM_APPLY_NEXT_IF_FAILED))
  {
    // DBG_MESSAGE("CANT USE NEXT CVM");
    *more_cvm_available = VIM_FALSE;
    return VIM_ERROR_NONE;
  }

  DBG_DATA(cvm_list, cvm_list_length);

  /* Make sure CVM list is at least the minimum length (incase kernel doesnt check this) */
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(8, cvm_list_length);

  cvm_count = (cvm_list_length - VIM_SIZEOF(cvm_list->amount_x) - VIM_SIZEOF(cvm_list->amount_y)) / 2;
  x_value = (cvm_list->amount_x[0] << 24) + (cvm_list->amount_x[1] << 16) + (cvm_list->amount_x[2] << 8) + cvm_list->amount_x[3];
  y_value = (cvm_list->amount_y[0] << 24) + (cvm_list->amount_y[1] << 16) + (cvm_list->amount_y[2] << 8) + cvm_list->amount_y[3];

  /* Find the currently selected CVM */
  for (i=0; i<cvm_count; i++)
  {
    if (cvm_list->cvm[i].rule == current_cvm_method)
      break;
  }

  i++;
  /* Check if any more CVMs are available and usable */
  for (; i<cvm_count; i++)
  {
    cvm_applicable = VIM_FALSE;
    /* Check if CVM applicable */
    switch (cvm_list->cvm[i].condition)
    {
      case VIM_EMV_CVM_CONDITION_ALWAYS:
        cvm_applicable = VIM_TRUE;
        break;

      case VIM_EMV_CVM_CONDITION_IF_SUPPORTED:
        cvm_applicable = emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_NO_CASH:
        if (txn.amount_cash == 0)
          cvm_applicable = emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_MANUAL_CASH:
        if (txn.type == tt_cashout)
          cvm_applicable = emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_PURCHASE_CASHBACK:
        if (txn.type == tt_sale_cash)
          cvm_applicable = emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_UNDER_X:
        if (local_currency)
          cvm_applicable = (txn.amount_total < x_value) && emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_OVER_X:
        if (local_currency)
          cvm_applicable = (txn.amount_total > x_value) && emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_UNDER_Y:
        if (local_currency)
          cvm_applicable = (txn.amount_total < y_value) && emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_IF_OVER_Y:
        if (local_currency)
          cvm_applicable = (txn.amount_total > y_value) && emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities);
        break;

      case VIM_EMV_CVM_CONDITION_UNATTENDED_CASH:
      default:
        cvm_applicable = VIM_FALSE;
        break;
    }

    /* Found another CVM that it is a supported one. Check that it is a supported one. */
    if (cvm_applicable)
    {
      if (emv_cvm_supported(cvm_list->cvm[i].rule & VIM_EMV_CVM_MASK, terminal_capabilities))
      {
        // DBG_MESSAGE("FOUND NEXT CVM");
        // DBG_VAR(cvm_list->cvm[i].rule);
        /* We have found another supported CVM. We can stop parsing the list now. */
        *more_cvm_available = VIM_TRUE;
        return VIM_ERROR_NONE;
      }
      else
      {
        /* If the cvm is applicable, but we don't support it, then it is considered failed. Check if we can go to next CVM.*/
        if ((cvm_list->cvm[i].rule & 0x80) == 0x00)
        {
          *more_cvm_available = VIM_FALSE;
          return VIM_ERROR_NONE;
        }
      }
    }

    /* CVM is not applicable, keep searching for one that is. */
  }

  /* Reached end of CVM list and no applicable CVM was found */
  *more_cvm_available = VIM_FALSE;
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
/* 
  Check if pin bypass is allowed. Bypass is not allowed when:
    1) CVM processing will fail if pin is bypassed. 
    2) PIN bypass disabled in AID config. 
    3) IAC or TAC denial show that transaction will be declined if pin is bypassed.
  If none of the above conditions apply, bypass is permitted.
*/

static VIM_ERROR_PTR emv_check_pin_bypass_allowed
(
  VIM_UINT8 cvm_method,
  const VIM_EMV_AID *aid, 
  VIM_BOOL *bypass_allowed
)
{
  VIM_UINT8 i, iac_denial[5], tag_buf[50];
  VIM_TLV_LIST tags;
  VIM_BOOL more_cvm_available=VIM_TRUE;
  VIM_TAG_ID read_tags[] = 
  {
    VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL
  };

  *bypass_allowed = VIM_TRUE;

  /* Check CVM list to see if bypassing current CVM will result in failed CVM processing */
  VIM_ERROR_RETURN_ON_FAIL(emv_check_more_cvm_available(cvm_method, &more_cvm_available))
  if (!more_cvm_available)
  {
    // DBG_MESSAGE("NO PIN BYPASS NO MORE CVM");
    *bypass_allowed = VIM_FALSE;
    return VIM_ERROR_NONE;
  }

  for (i = 0; i < MAX_EMV_APPLICATION_ID_SIZE; i++)
  {
    if (0 == aid_ptr->emv_aid[0])
      break;

    if (vim_mem_compare(aid->aid, aid_ptr->emv_aid, aid_ptr->aid_length) == VIM_ERROR_NONE)
      break;
  }
  
  if (i >= MAX_EMV_APPLICATION_ID_SIZE)
    VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);

  *bypass_allowed = aid_ptr->pin_bypass;

  /* Check config for bypass option in AID config*/
  /*  !!!WE DO NOT HAVE OPTIONS IN AID STRUCTURE!!! */
  /*if ( emv_get_app_id(aid, &record) == VIM_ERROR_NONE )
  {
    if(!EMV_GET_BOOL_FROM_BCD(record.appid.data.aid.pin_bypass))
    {
      // DBG_MESSAGE("NO PIN BYPASS AID FLAG");
      *bypass_allowed = VIM_FALSE;
      return VIM_ERROR_NONE;
    }
  }*/
  
  /* Check for IAC and TAC denial for pin bypassed bit */
  /*key.icc.record_type[0] = ICC_RECORD_DATA;
  memcpy(key.icc.rid, aid->aid, 5);
  VIM_ERROR_RETURN_ON_FAIL(table_search(FILE_ICC_TABLE, &key, &record,VIM_NULL));*/
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tags, tag_buf, VIM_SIZEOF(tag_buf)));
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_read_kernel_tags(read_tags, VIM_LENGTH_OF(read_tags), &tags));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_search(&tags, VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, iac_denial, VIM_SIZEOF(iac_denial)));

  // DBG_VAR(aid_ptr->tac_denial);
  // DBG_VAR(iac_denial);
  if (VIM_EMV_TVR_PIN_BYPASSED(aid_ptr->tac_denial) || VIM_EMV_TVR_PIN_BYPASSED(iac_denial))
  {
    // DBG_MESSAGE("NO PIN BYPASS TAC/IAC");
    *bypass_allowed = VIM_FALSE;
    return VIM_ERROR_NONE;
  }

  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
/* Retrieve the application preferred name or application label for the current transaction. */

static VIM_BOOL CheckChar( VIM_CHAR Check )
{
	if(( Check >= 'a' && Check <= 'z' )||( Check >= 'A' && Check <= 'Z' )||( Check == ' ' )||( Check >= '0' && Check <= '9' ))
		return VIM_TRUE;
	else
		return VIM_FALSE;
}

static VIM_BOOL IsAlphaString( VIM_CHAR *Ptr2 )
{
	VIM_SIZE n, len = vim_strlen(Ptr2);
    VIM_CHAR *Ptr = Ptr2;

	for( n=0; n<len; n++, Ptr++ )
	{
		if( !CheckChar( *Ptr ) ) 
		{
			DBG_MESSAGE( "^^^^^^^^^^ CARD NAME TAG WAS INVALID.. REJECT ! ^^^^^^^^^^^^^^" );
            VIM_DBG_STRING(Ptr2);
			return VIM_FALSE;
		}
	}
	//DBG_MESSAGE( "^^^^^^^^^^ CARD NAME TAG WAS OK ^^^^^^^^^^^^^^" );
	return VIM_TRUE;
}


static VIM_BOOL IsValidTagString( transaction_t *transaction, VIM_TLV_TAG_PTR EmvTag, VIM_CHAR *label )
{
	VIM_CHAR temp_label[30];
	vim_mem_clear(temp_label, VIM_SIZEOF(temp_label));

	DBG_VAR( transaction->emv_data.data_size );
	if( transaction->emv_data.data_size )
	{
		VIM_TLV_LIST tags_list;
		vim_tlv_open( &tags_list, transaction->emv_data.buffer, transaction->emv_data.data_size, VIM_SIZEOF(transaction->emv_data.buffer), VIM_FALSE );
	
		if( vim_tlv_search( &tags_list, EmvTag ) == VIM_ERROR_NONE )	
		{  
			//VIM_DBG_TLV(tags_list);  
			//DBG_MESSAGE( "^^^^^^^^^^ Validate ^^^^^^^^^^^^^^" );	  
			if( tags_list.current_item.length )  
			{	  
				VIM_CHAR *Ptr = temp_label;      
				vim_tlv_get_bytes( &tags_list, Ptr, tags_list.current_item.length );	  
				Ptr += tags_list.current_item.length;	  
				*Ptr = '\0';	  
				//DBG_MESSAGE( "^^^^^^^^^^ FOUND CARD NAME TAG ^^^^^^^^^^^^^^" );	  
				//DBG_MESSAGE( temp_label );
				//DBG_VAR( txn.emv_data.data_size );
				if( IsAlphaString( temp_label ))
				{
					vim_strcpy( label, temp_label );
					return VIM_TRUE;
				}
			}
		}
		else
		{
			DBG_MESSAGE( "^^^^^^^^^^ CAN'T FIND CARD NAME TAG ^^^^^^^^^^^^^^" );	  
		}
	}
	//DBG_VAR( txn.emv_data.data_size );
	return VIM_FALSE;
}


VIM_ERROR_PTR emv_get_app_label( transaction_t *transaction, char *label )
{
  // RDD 151012 SPIRA:6168 Need to use exisitng buffer as for deposits we may be using saved data and reading the tags from the kernal can 
  // end up using the data from the LAST emv transaction rather than the saved data (copied from card buffer).
  if( !IsValidTagString( transaction, VIM_TAG_EMV_APPLICATION_PREFERRED_NAME, label ))
  {
	  if( !IsValidTagString( transaction, VIM_TAG_EMV_APPLICATION_LABEL, label ))
	  {
		  return VIM_ERROR_NOT_FOUND;
	  }
  }
  return VIM_ERROR_NONE;
}

//  RDD 110213 Created for EMV Kernel 6.2.0 
//  Callback for the application to perform event checking during Offline PIN entry.

// TEST V7
#if 1

#define PIN_ENTRY_HEARTBEAT_TIMEOUT  100

VIM_ERROR_PTR vim_emv_CB_check_error_code( void )
{	
	VIM_RTC_UPTIME UpTimeNow;	
	VIM_BOOL PinEntered = transaction_get_status( &txn, st_offline_pin_verified );

	vim_rtc_get_uptime(&UpTimeNow);	
	DBG_VAR( PinEntered );
	//DBG_VAR( UpTimeNow );	
	//DBG_VAR( LastPinEntryHeartBeat );
		
	if(( UpTimeNow > ( LastPinEntryHeartBeat + PIN_ENTRY_HEARTBEAT_TIMEOUT )) && !PinEntered )	
	{	
		VIM_BOOL CardPresent = VIM_TRUE;
        vim_icc_check_card_slot(&CardPresent);
        if( CardPresent )
			txn.error = VIM_ERROR_TIMEOUT;
		else
			txn.error = VIM_ERROR_EMV_CARD_REMOVED;

		VIM_ERROR_RETURN_ON_FAIL( txn.error );	
	}

	if( txn.error_backup == VIM_ERROR_EMV_PIN_ENTERED )
	{
		DBG_MESSAGE( "PIN WAS ENTERED SUCESSFULLY" );
		return txn.error_backup;
	}
	else if( txn.emv_error == VIM_ERROR_EMV_PIN_BYPASSED )
	{
		DBG_MESSAGE( "PIN WAS BYPASSED" );
		return txn.emv_error;
	}
	return txn.error;
}
		

VIM_ERROR_PTR vim_emv_CB_get_pos_decision( void )
{
	VIM_ERROR_PTR res;
	//DBG_MESSAGE( "vim_emv_CB_get_pos_decision(" );
	if(( res = pceft_pos_command_check( &txn, 10 )) == VIM_ERROR_POS_CANCEL )
	{
		VIM_DBG_MESSAGE( "!!!!!!!! POS CANCELLED !!!!!!!!!!" );
		txn.error = VIM_ERROR_POS_CANCEL;
	}
	else if( res == VIM_ERROR_EMV_CARD_REMOVED )
	{
		VIM_DBG_MESSAGE( "!!!!!!!! CARD REMOVED !!!!!!!!!!" );
		txn.error = VIM_ERROR_EMV_CARD_REMOVED;
	}
	return VIM_ERROR_NONE;
}

#else

VIM_ERROR_PTR vim_emv_CB_get_pos_decision( void )
{
	VIM_ERROR_PTR res;

	// Let the flow process know that this thread was run so PcEft can be restarted
	DBG_MESSAGE("**************** vim_emv_CB_get_pos_decision ********************");
	DBG_VAR( txn.status );
	DBG_VAR( txn.qualify_svp );
	DBG_VAR( txn.amount_total );
	DBG_VAR( txn.pin_bypassed );
	VIM_DBG_ERROR( txn.error );

	txn.error = VIM_ERROR_NONE;

	if( txn.pin_bypassed )
	{
		DBG_MESSAGE("Already Bypassed");
		return VIM_ERROR_EMV_PIN_BYPASSED;
		//return VIM_ERROR_NONE;
	}

	if( txn.qualify_svp )
	{
		DBG_MESSAGE("bypass pin entry (svp)");
		return VIM_ERROR_EMV_PIN_BYPASSED;
		//return VIM_ERROR_NONE;
	}

	if( !txn.emv_pin_entry_params.max_digits )
	{
		DBG_MESSAGE("no max digits bypass pin entry");
		return VIM_ERROR_EMV_PIN_BYPASSED;
		//return VIM_ERROR_NONE;
	}
	else
	{
		transaction_set_status( &txn, st_offline_pin_thread );
	
		// RDD Noticed MC Interoper 9 caused kernel to call offline PIN thread erven though it was online PIN - this messed up PCEFT I/F so exit here	
		if( !txn.emv_pin_entry_params.offline_pin )	
		{
			DBG_POINT;
			return VIM_ERROR_NONE;
		}
		if( PCEftComm( HOST_WOW )
		{
			// generate a new pceftpos session within this thread  
			VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_new(&pceftpos_handle,&pceftpos_settings));
			VIM_DBG_MESSAGE("PCEFT RX MSG FLAG CLEARED ");
			pceft_debug_test( pceftpos_handle.instance, "--- CHECK EVENTS FROM NEW THREAD ---");
		}

		// RDD 120213 : This is run from a separate thread so need a new UART handle etc.
		res = pceft_pos_command_check( &txn, (ENTRY_TIMEOUT*3)/4 );			

		VIM_DBG_ERROR( res );

		// If the Pin Entry times out or the POS cancelled it then set the correct error
		if(( res == VIM_ERROR_POS_CANCEL )||
			( res == VIM_ERROR_USER_CANCEL )||
			( res == VIM_ERROR_TIMEOUT )||
			( res == VIM_ERROR_EMV_CARD_REMOVED ))
		{
			txn.error = res;
			txn.emv_error = res;
			VIM_DBG_ERROR( txn.error );
		}
		pceft_debug_error( pceftpos_handle.instance, "---- EVENT PROCESSED (CLOSE PCEFT FROM TEMP THREAD) -----", res );
	}
	// Close down POS session as this thread will be terminated by the OS	
	pceft_close();
	//VIM_DBG_MESSAGE( "%%%%%%%%%%% TEMPORARY THREAD PCEFT SESSION CLOSED %%%%%%%%%%" );
	VIM_ERROR_RETURN( res );
}

#endif


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_enter_pin
(
  VIM_UINT8 pin_tries_remaining,
  VIM_UINT8 cvm_method,
  VIM_UINT8 *pin_digits_entered,
  VIM_EMV_AID *aid
)
{
  int min;
  int max;
  VIM_SIZE pin_size=0;
  
  VIM_DBG_WARNING( "---- vim_emv_CB_enter_pin ----");
  VIM_DBG_VAR(cvm_method);
  VIM_DBG_VAR(pin_tries_remaining);
  VIM_DBG_VAR(txn.pin_bypassed);
	
  // RDD 130213 - only need to do this for offline PIN as the PCEFT has already been shut down

  terminal_clear_status(ss_pos_comms_in_main_thread);		

  // RDD SPIRA:6380 This function is being called with an existing error condition - Exit
  VIM_ERROR_RETURN_ON_FAIL( txn.error );

  // RDD 251112 SPIRA:6380 Check for card removal here as sometimes missing this when Pin tries exceeded
  VIM_ERROR_RETURN_ON_FAIL( EmvCardRemoved( VIM_TRUE ) );

  *pin_digits_entered = 0;

  if( txn.pin_bypassed )
  {
	  VIM_DBG_MESSAGE("ALREADY BYPASSED PIN");
      return VIM_ERROR_NONE;


  }
  // RDD 261012 I noticed that this callback was re-called sometimes during a SVC txn when SVC was already set
  if( txn.qualify_svp )
  {
	  VIM_DBG_MESSAGE("ALREADY QUALIFIED SVP");
      return VIM_ERROR_NONE;
  }

  txn.emv_pin_entry_params.offline_pin = VIM_FALSE;
  txn.emv_pin_entry_params.last_pin_try = VIM_FALSE;
  
  switch (cvm_method & VIM_EMV_CVM_MASK)
  {
    case VIM_EMV_CVM_OFFLINE_PLAINTEXT_PIN:
    case VIM_EMV_CVM_OFFLINE_PLAIN_PIN_AND_SIGNATURE:
    case VIM_EMV_CVM_OFFLINE_ENCRYPTED_PIN:
    case VIM_EMV_CVM_OFFLINE_ENC_PIN_AND_SIGNATURE:

        pceft_debugf(pceftpos_handle.instance, "OLP:%d", cvm_method);
	  // RDD 220513 SPIRA:6736 - 1 line added below (was previously in offline PIN entry params setup but this can be called for blocked cards)
	  transaction_set_status( &txn, st_offline_pin );
      txn.emv_pin_entry_params.offline_pin = VIM_TRUE;

      if (pin_tries_remaining == 1)
        txn.emv_pin_entry_params.last_pin_try = VIM_TRUE;
      min = 4;
      max = 12;
      /* set flag that PIN has been prompted */
      txn.emv_pin_entry_params.retry_pin = VIM_TRUE;
      txn.emv_pin_entry_params.wrong_pin = VIM_FALSE;
      break;
    case VIM_EMV_CVM_ONLINE_PIN:

#if 1
		// RDD 110313 SPIRA:6621 - Online Pin after Offline PIN CVM needs to return VIM_ERROR_NONE here if no pin tries remaining
		DBG_VAR( txn.emv_pin_entry_params.offline_pin );
		DBG_VAR( pin_tries_remaining );

		if( !pin_tries_remaining )
		{
			DBG_MESSAGE( " Online Pin CVM with no Pin Tries remaining " );
			return VIM_ERROR_NONE;
		}
#endif

      min = 4;
      max = 12;
      /* set flag that PIN has been prompted */
      txn.emv_pin_entry_params.retry_pin = VIM_TRUE;
      txn.emv_pin_entry_params.wrong_pin = VIM_FALSE;
      break;

    case VIM_EMV_CVM_SIGNATURE:
    default:
      /* Check if we have already bypassed PIN entry. Don't prompt for amount confirmation again if we have. */
#if 0
      if (txn.pin_bypassed)
      {
        VIM_DBG_MESSAGE("ALREADY BYPASSED PIN");
        *pin_digits_entered = 0;
        return VIM_ERROR_NONE;
      }
#endif
      if (txn.emv_pin_entry_params.retry_pin && !txn.emv_pin_entry_params.wrong_pin)
      {
        VIM_DBG_MESSAGE("IT MUST BE SIGNATURE + PIN");
        *pin_digits_entered = 0;        
		return VIM_ERROR_NONE;
      }
	  if( !pin_tries_remaining )
	  {
        VIM_DBG_MESSAGE("IT MUST BE SIGNATURE + PIN TRIES EXCEEDED");
		return VIM_ERROR_NONE;
	  }
      min = 0;
      max = 0;
      break;
  }
  //get_txn_type_string_short(&txn, transaction_type, VIM_SIZEOF(transaction_type));
  //bin_to_amt(txn.amount_total, amount_string, VIM_SIZEOF(amount_string));
  //vim_snprintf(transaction_text, VIM_SIZEOF(transaction_text), "%s %s", transaction_type, amount_string);

 //VIM_DBG_VAR(min);
 //VIM_DBG_VAR(max);
#if 0 // RDD 080512 SPIRA:5418 Need to move this in case of Z1 decline
  if( pin_tries_remaining == 0 )
  {
	 //  
	  display_screen( DISP_PIN_TRY_LIMIT_EXCEEDED, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total)  );	
	  vim_event_sleep( 2000 );
  }
#endif

///////////////////////////////////// CALL PIN ENTRY BELOW ////////////////////////////////////
 while (1)
  {
	  // Run this loop while data entry in in error
    txn.emv_pin_entry_params.min_digits = min;
    txn.emv_pin_entry_params.max_digits = max;
    DBG_MESSAGE("<<< CALL PIN ENTRY FROM EMV_CB ENTER PIN >>>>" );
    /* As the PIN entry is invoked as a callback, there is no state machine. Hence
           any API invoked with regards to state has to be state initialized to 
           check the correct state flow */
    state_set_state(ts_get_pin);
	
    if (ts_get_pin != txn_get_pin(VIM_TRUE))
      break;
  }
///////////////////////////////////// CALL PIN ENTRY ABOVE ////////////////////////////////////
  	 
  DBG_MESSAGE("Returned from Pin Entry. Check txn.error" );
  VIM_DBG_ERROR(txn.error);	 //RDD 210312 - Card removed needs to override Pin Error...
  VIM_ERROR_RETURN_ON_FAIL( txn.error );

  VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_pin_size (tcu_handle.instance_ptr, &pin_size));
  VIM_DBG_NUMBER( pin_size );
  txn.emv_pin_entry_params.entered_digits = pin_size;
  *pin_digits_entered = txn.emv_pin_entry_params.entered_digits;
#if 0
  if (0 == txn.pin_block_size)
    txn.pin_bypassed = VIM_TRUE;
#endif
  	
  vim_emv_CB_PinEntryHeartBeat( );
  VIM_ERROR_RETURN( txn.error );
}


static VIM_ERROR_PTR emv_get_host_tags(VIM_TAG_ID host_tags[], VIM_SIZE *host_tags_cnt, transaction_t *transaction )
{
  VIM_SIZE i, j;

  *host_tags_cnt = VIM_SIZEOF(host_tags_auth)/VIM_SIZEOF(host_tags_auth[0]);

  for (i = j = 0; i < *host_tags_cnt; i++)
  {
	  // RDD - TODO fix this bug as it uses the AID from the last 200 txn NOT the txn_trickle, reversal etc....
    if ((0x04 == terminal.emv_applicationIDs[transaction->aid_index].emv_aid[4]) ||
        (VIM_ERROR_NONE != vim_mem_compare(VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE, host_tags_auth[i], 2)))
    {
      //VIM_ 
      host_tags[j++] = host_tags_auth[i];
    }
  }

  *host_tags_cnt = j;

  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR emv_read_tags_from_kernel(VIM_UINT8 *aid)
{
  VIM_SIZE combined_tags_cnt = 0;
  VIM_SIZE i,j;
  int tag_len;
  VIM_TLV_LIST tag_list;
  VIM_TAG_ID combined_tags[VIM_SIZEOF(host_tags_auth)/VIM_SIZEOF(host_tags_auth[0]) + VIM_SIZEOF(tags_to_print)/VIM_SIZEOF(tags_to_print[0])]; /*  worst case: all host tags are one byte length */

  if( !txn.emv_enabled )
  {
	  // We're not running a current emv txn ( might be processing a SAF ) so try to pick up the AID from the txn list.
	  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< ERROR: TRIED TO READ EMV TAGS WHEN NO ACTIVE EMV TXN >>>>>>>>>>>>>>>>>>>>>");	
	  return VIM_ERROR_NOT_FOUND;
  }
  VIM_ERROR_RETURN_ON_FAIL(emv_get_host_tags(combined_tags, &combined_tags_cnt, &txn ));

  //VIM_ERROR_RETURN_ON_FAIL(emv_get_host_tags(combined_tags, &combined_tags_cnt, aid ));
//	DBG_MESSAGE( "!!!!!!!!!!!!!!! Tag list first !!!!!!!!!!!!!!!!");  
//	DBG_VAR( combined_tags_cnt );


  /*  Add tags for printing */
  for(i=0; i<VIM_SIZEOF(tags_to_print)/VIM_SIZEOF(tags_to_print[0]); i++ )
  {
    tag_len = (*tags_to_print[i]&0x1F)==0x1F ? 2:1;
    for(j=0; j<combined_tags_cnt; j++)
    {
      if (vim_mem_compare( tags_to_print[i], combined_tags[j], tag_len ) == VIM_ERROR_NONE )
	  {
        break;
	  }
    }

    if( j == combined_tags_cnt )
    { /*  Tag not in the list yet. Add it */
      combined_tags[combined_tags_cnt++] = tags_to_print[i];
    }
  }
//	DBG_MESSAGE( "!!!!!!!!!!!!!!! Tag list second !!!!!!!!!!!!!!!!");  
//	DBG_VAR( combined_tags_cnt );
	
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create( &tag_list, txn.emv_data.buffer, VIM_SIZEOF(txn.emv_data.buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_read_kernel_tags(combined_tags, combined_tags_cnt, &tag_list));
  txn.emv_data.data_size = tag_list.total_length;
  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV LENGTH !!!!!!!!!!!!!!!!");
  DBG_VAR( txn.emv_data.data_size );
  DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);  
  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV LENGTH !!!!!!!!!!!!!!!!");
  txn_save();

  return VIM_ERROR_NONE;
}



/* EMV offline pin related CVM status */
VIM_ERROR_PTR vim_pin_status_display(status_state_t status_state)
{
  VIM_DBG_MESSAGE("vim_pin_status_display");
  VIM_DBG_VAR( status_state );
#ifdef _PHASE4
  //return VIM_ERROR_NONE;
#endif
  /* Skip pin status display, if its already done for the txn */
  if (status_state == PIN_STATUS_ONLINE)
  {
	  skip_pin_status_display = VIM_TRUE;
  }
  else if (status_state == PIN_STATUS_FINALIZATION)
  {
    if (skip_pin_status_display)
    {
      skip_pin_status_display = VIM_FALSE;
      return VIM_ERROR_NONE;
    }
  }
  /* Display based on the CVM results */
  DBG_POINT;
  if (emv_pin_status(&txn))
  {
    VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
    VIM_CHAR account_string_short[3+1];
  
    vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );
	get_acc_type_string_short(txn.account, account_string_short);    
    GetCardName( &txn, CardName );
	DBG_MESSAGE( "Display Screen : PIN OK" );	 
	display_screen( DISP_PIN_OK, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), CardName, account_string_short );
	if(/* status_state == PIN_STATUS_FINALIZATION */1 )
	{
		// RDD 170313 - saves a second if good offline pin going for approval online anyway
		vim_event_sleep(1000);
	}
  }
  /* Introduce delays based on states, However has to be reviewed based on customer input as 
        we are introducing delays */
#if 0
  if (status_state == PIN_STATUS_ONLINE)
  {
    DBG_MESSAGE("!!!!!!!!!!!! 1 SECOND DELAY ADDED !!!!!!!!!!!!!!");
    vim_event_sleep(1000);
  }
  else
  {
    DBG_MESSAGE("!!!!!!!!!!!! 2 SECOND DELAY ADDED !!!!!!!!!!!!!!");
    vim_event_sleep(2000);
  }
#endif
  return VIM_ERROR_NONE;
}


static void UpdateTxnHostRC( VIM_TLV_LIST *host_response_tags )
{
  VIM_CHAR emv_rc[2];
  if ( (txn.card.type==card_chip ) && !(transaction_get_status(&txn, st_partial_emv)))
  {	      
    if( vim_tlv_search( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE ) == VIM_ERROR_NONE )
    {
      vim_tlv_get_bytes( host_response_tags, emv_rc, 2 );

	  DBG_MESSAGE( "**** COMPARING DE39 AND DE55 TAG 8A ****");
	  DBG_DATA( txn.host_rc_asc, 2 );	
	  DBG_DATA( emv_rc, 2 );

	  // RDD 080312: BD SAYS: "If DE39 is 00 or 08 and DE55-8A is not 00, then PINpad RC is to reflect the DE55-8A RC and a reversal is to be sent"
	  // RDD 270312 Don't change the response code if the transacation was approved EFB
	  if( ((!vim_strncmp(txn.host_rc_asc, "00", 2)) || (!vim_strncmp(txn.host_rc_asc, "08", 2))) && ( vim_strncmp(emv_rc, "00", 2)) && ( vim_strncmp(emv_rc, "91", 2)) )
	  {		  
		  DBG_MESSAGE( "!!!! MODIFYING DE39..... !!!!");
		  // RDD 220312 SPIRA 5030: Need to Generate a reversal if 8A is a decline and host approved the txn
		  if( TxnWasApproved( emv_rc ) == VIM_FALSE )
		  {			  
			  DBG_VAR( terminal.stan );
			  DBG_MESSAGE( "!!!! SET REVERSAL with STAN and increment STAN!!!!");
			  reversal_set( &txn, VIM_FALSE);
			  DBG_VAR( terminal.stan );
		  }		
		  vim_mem_copy( txn.host_rc_asc, emv_rc, 2 );
		  DBG_DATA( txn.host_rc_asc, 2 );
	  }
    }
  }
}


#if 1

// RDD 270916 v561-0 Added changes to support latest AMEX contact - host responses sent without 8A but including ARC in ARPC ( 10 byte ARPC )

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_go_online( VIM_EMV_ONLINE_RESULT *online_result, VIM_TLV_LIST *host_response_tags, VIM_EMV_AID *aid )
{
  VIM_TLV_LIST tag_list;
  txn_state_t txn_state;
  VIM_UINT8 cid, i;

  const VIM_TAG_ID supported_response_tags[] =
  {  
	  VIM_TAG_EMV_RESPONSE_CODE,  
	  VIM_TAG_EMV_ISSUER_AUTHENTICATION_DATA,    
	  VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_1,    
	  VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_2
  };

  DBG_MESSAGE("vim_emv_CB_go_online");
  de55_length = 0;  /*  reset buffer holding host response */

  VIM_ERROR_RETURN_ON_FAIL( emv_read_tags_from_kernel(aid->aid));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes(&tag_list, &cid, 1));

  // RDD 210219 P400 Start - no response code in tag list from VOS driver - gusess this is reasonable if going online...
  //VIM_ERROR_RETURN_ON_FAIL(vim_tlv_search(&tag_list, VIM_TAG_EMV_RESPONSE_CODE));  
  VIM_ERROR_WARN_ON_FAIL(vim_tlv_search(&tag_list, VIM_TAG_EMV_RESPONSE_CODE))

//VIM_DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);
//VIM_DBG_VAR(cid); 
  /* Display the last offline cvm related to PIN verification status */
  vim_pin_status_display(PIN_STATUS_ONLINE);
  
  /* Update first genAC crypto info's, needed for receipt printing */   
  vim_mem_clear(txn.crypto, VIM_SIZEOF(txn.crypto));  
  vim_mem_clear(txn.cid, VIM_SIZEOF(txn.cid));
  
  /* TC, AAC or ARQC */
  if( VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA) )
  {
    vim_tlv_get_bytes(&tag_list, &cid, 1);
    if (EMV_CID_TC(cid))
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "TC");
    else if (EMV_CID_AAC(cid))
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "AAC");
    else if (EMV_CID_ARQC(cid))
  {
    DBG_MESSAGE( "Saving ARQC label to Txn" );
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "ARQC");
  }
    if( vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM) == VIM_ERROR_NONE )
    {
      vim_mem_clear(txn.crypto, sizeof(txn.crypto));
    DBG_MESSAGE( "Saving cryptogram to Txn" );
      hex_to_asc(tag_list.current_item.value, txn.crypto, VIM_MIN(tag_list.current_item.length*2, VIM_SIZEOF(txn.crypto)-1));
    DBG_STRING( txn.cid );
    DBG_DATA( txn.crypto, 8 );
   }
  }   

  // RDD 300112 - always call efb if completion
  if( txn.type == tt_completion )
  {
    txn_state = ts_efb;
  }

  // RDD 130214 SPIRA 7301 - ts_error being returned from txn_authorisation_processing() not being handled. This causes Txn to be approved but not saved or sent online.
  else if( EMV_CID_ARQC(cid) )
  {
    emv_attempt_to_go_online = VIM_TRUE;

	// RDD 140218 v574-1 JIRA WWBP-158 No processing msg to pos if PIN bypassed

	if( txn.pin_bypassed )
		pceft_pos_display("PROCESSING", "PLEASE WAIT", VIM_PCEFTPOS_KEY_NONE, VIM_PCEFTPOS_GRAPHIC_DATA);
		
    //state_set_state (ts_authorisation_processing);
    if(( txn_state = txn_authorisation_processing()) == ts_error )
    {               
      pceft_debug(pceftpos_handle.instance, "SPIRA 7301" );
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
    }
    else if(( txn.type == tt_preauth ) && ( txn.error != VIM_ERROR_NONE ))
    {
      pceft_debug(pceftpos_handle.instance, "SPIRA 8523" ); 
        VIM_ERROR_RETURN( txn.error );
    }
  }
  else
  {
    txn_state = ts_error;
    VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }
  

  // If response code is 91 then process as efb
  // RDD 270112 - also need to do EFB check if there was a Comms error: this is indicated by ts_efb being returned by the txn_auth state
  if(( txn_state == ts_efb ) || ( !vim_strncmp( txn.host_rc_asc, "91", 2 )) ) 
  {
  VIM_CHAR terminal_rc[2+1];

  DBG_MESSAGE( "<<<< EMV EFB PROCESSING >>>>" );
    local_online_result = VIM_EMV_ONLINE_RESULT_UNABLE_TO_GO_ONLINE;

  // RDD added !!
  // JQ. should think about this, it's better to add this in the emv_cb_finalization
  // RDD 240112: I think we need it here as the resp code tag needs to be updated with the modified response code for 
  // the host to know the txn was approved offline EFB. Field 39 is not sent up in 220 for WOW and 8A is included in the TC

  // RDD update field 8A for second GEN AC - this will generate the correct cryptogram if the txn passed EMV EFB and was stored as an advice      
  vim_strcpy( terminal_rc, "Z3" );
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, terminal_rc, 2));

  if( !vim_strncmp( txn.host_rc_asc, "91", 2 ) )
    txn.saf_reason = saf_reason_91;
  else
    txn.saf_reason = saf_reason_timeout;
  
  efb_process_txn();  
  }
  else
  {
    if( txn.error == VIM_ERROR_NONE )
      local_online_result = VIM_EMV_ONLINE_RESULT_APPROVED;
    else
      local_online_result = VIM_EMV_ONLINE_RESULT_DECLINED;

#if 0 // RDD 080312: SPIRA 5038 - BD Decided that it's best not to do this after all. 
  DBG_MESSAGE( "<<<<< UPDATING 8A WITH HOST RESPONSE (or efb modified host response) >>>>>>>>" );
  DBG_VAR( txn.host_rc_asc );
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2));
#else
  // RDD 080312: BD SAYS: "If DE39 is 00 or 08 and DE55-8A is not 00, then PINpad RC is to reflect the DE55-8A RC and a reversal is to be sent"
  //UpdateTxnHostRC( );

#endif
  }
  // Tell the kernel about what came back from the host
  if ((de55_length) && (!transaction_get_status(&txn, st_efb)))   //BD Finsim sending DE55 and RC91
  {  
    /* Host response contains ICC data. Copy tags to kernel */
      if(VIM_ERROR_NONE == vim_tlv_open( &tag_list, de55_data, de55_length, VIM_SIZEOF(de55_data), VIM_FALSE))
      {
        /* special processing for issuer scripts */
        while( vim_tlv_goto_next (&tag_list) == VIM_ERROR_NONE )
        {
          if( (vim_mem_compare( tag_list.current_item.tag, VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_1, 1 ) == VIM_ERROR_NONE) || 
              (vim_mem_compare( tag_list.current_item.tag, VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_2, 1 ) == VIM_ERROR_NONE) )
          {          
			  DBG_MESSAGE( "EEEEEEEEEEEEEE FOUND ISSUER SCRIPT EEEEEEEEEEEEEEEE" );            
			  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes (host_response_tags, tag_list.current_item.tag, tag_list.current_item.value, tag_list.current_item.length));
          }
        }
        DBG_MESSAGE( "<<<<< UPDATING TAGS WITH HOST DE55 TAGS >>>>>>>>" );

        // RDD 170512 SPIRA????? need txn to approve if no DE55 or 8A missing from DE55
		DBG_MESSAGE( txn.host_rc_asc_backup );   
		DBG_MESSAGE( txn.host_rc_asc );
#if 0
		if(( vim_tlv_search( &tag_list, VIM_TAG_EMV_RESPONSE_CODE ) != VIM_ERROR_NONE ) && ( is_approved_txn(&txn) ))
#else	
		// RDD 260916 05 Decline with missing 8A needs to decline and create 8A from RC
		if(( vim_tlv_search( &tag_list, VIM_TAG_EMV_RESPONSE_CODE ) != VIM_ERROR_NONE ))
#endif
        {
			// RDD 260916 Some Schemes append ARC to ARPC so use this before response code if 8A missing
			if(( vim_tlv_search( &tag_list, VIM_TAG_EMV_RESPONSE_CODE ) != VIM_ERROR_NONE ))
			{
				pceft_debug_test(pceftpos_handle.instance, "8A MISSING FROM DE55. Check Tag 91 for ARC");
				if(( vim_tlv_search( &tag_list, VIM_TAG_EMV_ISSUER_AUTHENTICATION_DATA ) == VIM_ERROR_NONE ))
				{
					// RDD ARPC will be 8 bytes if just the cryptogram or 10 bytes if includes the ARC
                    // RDD Trello #171 Mastercard Z4 Issue
					if(( tag_list.current_item.length == 10 ) && ((( IS_MCD(txn.aid_index) ) == VIM_FALSE )))
					{
						VIM_UINT8 IAD[10];
                        pceft_debug_test(pceftpos_handle.instance, "Create 8A from ARPC ARC" );
						vim_mem_copy( IAD, tag_list.current_item.value, tag_list.current_item.length );
						VIM_DBG_DATA( IAD, tag_list.current_item.length );
						// Copy the last two bytes of the ARPC into 8A
						VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, &IAD[8], 2 ) );
						VIM_DBG_DATA( &IAD[8], 2 );
					}
					else
					{
                        VIM_CHAR DE39_RC[2 + 1];
                        vim_mem_clear( DE39_RC, VIM_SIZEOF( DE39_RC ));
                        pceft_debug_test( pceftpos_handle.instance, "Create 8A from DE39" );
                        vim_strcpy( DE39_RC, txn.host_rc_asc );
                        if( !vim_strncmp( DE39_RC, "08", 2 ))
                        {
                            vim_strcpy( DE39_RC, "00" );
                        }
                        pceft_debug_test(pceftpos_handle.instance, "Convert 08 RC to 00 8A");
                        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, DE39_RC, 2));
                    }
				}			
			}
        }

#if 0   // RDD - remove as this is only due to Finsim - doesn't happen with real issuers
		// RDD 250712 SPIRA:5794 RC 11 in 8A needs to be transalated to 00 ( Only because the 5100 already does this )
		if( !vim_strncmp(txn.host_rc_asc_backup, "11", 2) )    			
		{            				
			DBG_MESSAGE( "--------- 8A is 11 change to 00 -----------");           				
			VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );    			
		}
#endif
        for( i = 0; i < (VIM_LENGTH_OF(supported_response_tags)); i++ )
        {
          if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, supported_response_tags[i]) )
          {
      
			  VIM_UINT8 *Ptr = tag_list.current_item.tag;
      
			  DBG_DATA( de55_data, de55_length );
      
			  DBG_DATA(tag_list.current_item.value, tag_list.current_item.length);
      
			  DBG_VAR( tag_list.current_item.length );
      
			  DBG_DATA( Ptr, tag_list.current_item.tag_length );
      
			  // RDD 250712 SPIRA:5794 RC 11 in 8A needs to be transalated to 00 ( Only because the 5100 already does this )      
			  if( vim_mem_compare( Ptr, VIM_TAG_EMV_RESPONSE_CODE, 1 ) == VIM_ERROR_NONE )      
			  {        
				  DBG_MESSAGE( "-- Found 8A --");        
				  if( !vim_strncmp(txn.host_rc_asc_backup, "11", 2) )        
				  {          
					  DBG_MESSAGE( "--------- 8A is 11 change to 00 -----------");          
					  vim_mem_copy( tag_list.current_item.value, txn.host_rc_asc, 2 );        
				  }        
				  // RDD 170513 SPIRA:6728 also change 08 to 00         
				  else if( !vim_strncmp(txn.host_rc_asc, "08", 2) )        
				  {          
					  VIM_UINT8 Rc[2] = { 0x30,0x30 };          
					  DBG_MESSAGE( "--------- 8A is 08 change to 00 -----------");          
					  vim_mem_copy( tag_list.current_item.value, Rc, 2 );        
				  }        
				  // RDD 170513 moved from below to here as this was being run multiple times         
				  UpdateTxnHostRC( host_response_tags );      
			  }            
			  // RDD 270112 - changed from add to update            
			  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes( host_response_tags, tag_list.current_item.tag, tag_list.current_item.value, tag_list.current_item.length ));
      
			  DBG_MESSAGE( "Tag Updated" );        
			  DBG_DATA( tag_list.current_item.tag, tag_list.current_item.tag_length );      
			  DBG_VAR( tag_list.current_item.length );
//            UpdateTxnHostRC( host_response_tags );          
		  }        
		}      
		DBG_MESSAGE( "Host Rx Tags Updated" );
      }
  }
  // RDD 170512 SPIRA????? Need txn to approve if no DE55 or 8A missing from DE55
  else
  {
    // DBG_MESSAGE( "%%%%%%%% NO DE55 DETECTED: %%%%%%%%%%");
    if( is_approved_txn(&txn) )
    {
      // DBG_MESSAGE( "%%%%%%%% WRITING 8A %%%%%%%%%%");
      //VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
      //JQ Spira issue:006511
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, "00", 2 ) );
    }
    else
    {
      if( vim_strlen( txn.host_rc_asc ) > 0 )
        VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
    }
  }

  *online_result = local_online_result;
  
  /* return without error */
  DBG_MESSAGE("exit OK. vim_emv_CB_go_online");
  return VIM_ERROR_NONE;
}

#else

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_go_online( VIM_EMV_ONLINE_RESULT *online_result, VIM_TLV_LIST *host_response_tags, VIM_EMV_AID *aid )
{
  VIM_TLV_LIST tag_list;
  txn_state_t txn_state;
  VIM_UINT8 cid, i;
#if 0
  const VIM_TAG_ID supported_response_tags[] =
  {
	VIM_TAG_EMV_RESPONSE_CODE,
    VIM_TAG_EMV_ISSUER_AUTHENTICATION_DATA,
    VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_1,
    VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_2
  };
#else
const VIM_TAG_ID supported_response_tags[] =
{
	VIM_TAG_EMV_RESPONSE_CODE,
	VIM_TAG_EMV_ISSUER_AUTHENTICATION_DATA,
    VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_1,
    VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_2
};

#endif
  DBG_MESSAGE("vim_emv_CB_go_online");
  de55_length = 0;  /*  reset buffer holding host response */

  VIM_ERROR_RETURN_ON_FAIL( emv_read_tags_from_kernel(aid->aid));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes(&tag_list, &cid, 1));

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search(&tag_list, VIM_TAG_EMV_RESPONSE_CODE ) )

#if 0
  // RDD 140912 Special Processing for Rimu Card
  if( txn.type == tt_query_card )
  {
	  
	 return( get_loyalty_iad( &txn ));
  }
#endif

//VIM_DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);
//VIM_DBG_VAR(cid); 
  /* Display the last offline cvm related to PIN verification status */
  vim_pin_status_display(PIN_STATUS_ONLINE);
  
  /* Update first genAC crypto info's, needed for receipt printing */  	
  vim_mem_clear(txn.crypto, VIM_SIZEOF(txn.crypto));	
  vim_mem_clear(txn.cid, VIM_SIZEOF(txn.cid));
  
  /* TC, AAC or ARQC */
  if( VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA) )
  {
    vim_tlv_get_bytes(&tag_list, &cid, 1);
    if (EMV_CID_TC(cid))
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "TC");
    else if (EMV_CID_AAC(cid))
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "AAC");
    else if (EMV_CID_ARQC(cid))
	{
		DBG_MESSAGE( "Saving ARQC label to Txn" );
      vim_snprintf(txn.cid,VIM_SIZEOF(txn.cid), "ARQC");
	}
    if( vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM) == VIM_ERROR_NONE )
    {
      vim_mem_clear(txn.crypto, sizeof(txn.crypto));
		DBG_MESSAGE( "Saving cryptogram to Txn" );
      hex_to_asc(tag_list.current_item.value, txn.crypto, VIM_MIN(tag_list.current_item.length*2, VIM_SIZEOF(txn.crypto)-1));
	  DBG_STRING( txn.cid );
	  DBG_DATA( txn.crypto, 8 );
   }
  }   

  // RDD 300112 - always call efb if completion
  if( txn.type == tt_completion )
  {
	  txn_state = ts_efb;
  }

  // RDD 130214 SPIRA 7301 - ts_error being returned from txn_authorisation_processing() not being handled. This causes Txn to be approved but not saved or sent online.
  else if( EMV_CID_ARQC(cid) )
  {
    emv_attempt_to_go_online = VIM_TRUE;
    //state_set_state (ts_authorisation_processing);
#if 0
    txn_state = txn_authorisation_processing();
#else
	if(( txn_state = txn_authorisation_processing()) == ts_error )
	{							  
		pceft_debug(pceftpos_handle.instance, "SPIRA 7301" );
	    VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
	}
	else if(( txn.type == tt_preauth ) && ( txn.error != VIM_ERROR_NONE ))
	{
		pceft_debug(pceftpos_handle.instance, "SPIRA 8523" ); 
	    VIM_ERROR_RETURN( txn.error );
	}
#endif
  }
  else
  {
    txn_state = ts_error;
    VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }
  

  // If response code is 91 then process as efb
  // RDD 270112 - also need to do EFB check if there was a Comms error: this is indicated by ts_efb being returned by the txn_auth state
  if(( txn_state == ts_efb ) || ( !vim_strncmp( txn.host_rc_asc, "91", 2 )) ) 
  {
	VIM_CHAR terminal_rc[2+1];

	DBG_MESSAGE( "<<<< EMV EFB PROCESSING >>>>" );
    local_online_result = VIM_EMV_ONLINE_RESULT_UNABLE_TO_GO_ONLINE;

	// RDD added !!
	// JQ. should think about this, it's better to add this in the emv_cb_finalization
	// RDD 240112: I think we need it here as the resp code tag needs to be updated with the modified response code for 
	// the host to know the txn was approved offline EFB. Field 39 is not sent up in 220 for WOW and 8A is included in the TC

	// RDD update field 8A for second GEN AC - this will generate the correct cryptogram if the txn passed EMV EFB and was stored as an advice		  
	vim_strcpy( terminal_rc, "Z3" );
	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, terminal_rc, 2));

	efb_process_txn();	
  }
  else
  {
    if( txn.error == VIM_ERROR_NONE )
      local_online_result = VIM_EMV_ONLINE_RESULT_APPROVED;
    else
      local_online_result = VIM_EMV_ONLINE_RESULT_DECLINED;

#if 0	// RDD 080312: SPIRA 5038 - BD Decided that it's best not to do this after all.	
	DBG_MESSAGE( "<<<<< UPDATING 8A WITH HOST RESPONSE (or efb modified host response) >>>>>>>>" );
	DBG_VAR( txn.host_rc_asc );
	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2));
#else
	// RDD 080312: BD SAYS: "If DE39 is 00 or 08 and DE55-8A is not 00, then PINpad RC is to reflect the DE55-8A RC and a reversal is to be sent"
	//UpdateTxnHostRC( );

#endif
  }
  DBG_POINT;
  // Tell the kernel about what came back from the host
  if ((de55_length) && (!transaction_get_status(&txn, st_efb)))		//BD Finsim sending DE55 and RC91
  {  
    /* Host response contains ICC data. Copy tags to kernel */
  DBG_POINT;
      if(VIM_ERROR_NONE == vim_tlv_open( &tag_list, de55_data, de55_length, VIM_SIZEOF(de55_data), VIM_FALSE))
      {
        /* special processing for issuer scripts */
  DBG_POINT;
        while( vim_tlv_goto_next (&tag_list) == VIM_ERROR_NONE )
        {
  DBG_POINT;
          if( (vim_mem_compare( tag_list.current_item.tag, VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_1, 1 ) == VIM_ERROR_NONE) || 
              (vim_mem_compare( tag_list.current_item.tag, VIM_TAG_EMV_ISSUER_SCRIPT_TEMPLATE_2, 1 ) == VIM_ERROR_NONE) )
          {
	        DBG_MESSAGE( "EEEEEEEEEEEEEE FOUND ISSUER SCRIPT EEEEEEEEEEEEEEEE" );
	
            VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes (host_response_tags, tag_list.current_item.tag, tag_list.current_item.value, tag_list.current_item.length));
          }
        }
        DBG_MESSAGE( "<<<<< UPDATING TAGS WITH HOST DE55 TAGS >>>>>>>>" );

        // RDD 170512 SPIRA????? Need txn to approve if no DE55 or 8A missing from DE55
		DBG_MESSAGE( txn.host_rc_asc_backup );
		DBG_MESSAGE( txn.host_rc_asc );
        if(( vim_tlv_search( &tag_list, VIM_TAG_EMV_RESPONSE_CODE ) != VIM_ERROR_NONE ) && ( is_approved_txn(&txn) ))
        {
          DBG_MESSAGE( "%%%%%%%% 8A MISSING FROM DE55.. CREATING FROM DE39 %%%%%%%%%%");
          VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
        }

		// RDD 250712 SPIRA:5794 RC 11 in 8A needs to be transalated to 00 ( Only because the 5100 already does this )
#if 0
		if( !vim_strncmp(txn.host_rc_asc_backup, "11", 2) )
		{
            DBG_MESSAGE( "--------- 8A is 11 change to 00 -----------");
            VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
		}
#endif
        for( i = 0; i < (VIM_LENGTH_OF(supported_response_tags)); i++ )
        {
          if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, supported_response_tags[i]) )
          {
			VIM_UINT8 *Ptr = tag_list.current_item.tag;
			DBG_DATA( de55_data, de55_length );
			DBG_DATA(tag_list.current_item.value, tag_list.current_item.length);
			DBG_VAR( tag_list.current_item.length );
			DBG_DATA( Ptr, tag_list.current_item.tag_length );
			// RDD 250712 SPIRA:5794 RC 11 in 8A needs to be transalated to 00 ( Only because the 5100 already does this )
			if( vim_mem_compare( Ptr, VIM_TAG_EMV_RESPONSE_CODE, 1 ) == VIM_ERROR_NONE )
			{
				DBG_MESSAGE( "-- Found 8A --");
				if( !vim_strncmp(txn.host_rc_asc_backup, "11", 2) )
				{
					DBG_MESSAGE( "--------- 8A is 11 change to 00 -----------");
					vim_mem_copy( tag_list.current_item.value, txn.host_rc_asc, 2 );
				}

				// RDD 170513 SPIRA:6728 also change 08 to 00 
				else if( !vim_strncmp(txn.host_rc_asc, "08", 2) )
				{
					VIM_UINT8 Rc[2] = { 0x30,0x30 };
					DBG_MESSAGE( "--------- 8A is 08 change to 00 -----------");
					vim_mem_copy( tag_list.current_item.value, Rc, 2 );
				}
				// RDD 170513 moved from below to here as this was being run multiple times 
				UpdateTxnHostRC( host_response_tags );

			}
            // RDD 270112 - changed from add to update
            VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes( host_response_tags, tag_list.current_item.tag, tag_list.current_item.value, tag_list.current_item.length ));

			DBG_MESSAGE( "Tag Updated" );
		    DBG_DATA( tag_list.current_item.tag, tag_list.current_item.tag_length );
			DBG_VAR( tag_list.current_item.length );

//            UpdateTxnHostRC( host_response_tags );
          }
        }
	    DBG_MESSAGE( "Host Rx Tags Updated" );

      }
  }
  // RDD 170512 SPIRA????? Need txn to approve if no DE55 or 8A missing from DE55
  else
  {
	  // DBG_MESSAGE( "%%%%%%%% NO DE55 DETECTED: %%%%%%%%%%");
	  if( is_approved_txn(&txn) )
	  {
		  // DBG_MESSAGE( "%%%%%%%% WRITING 8A %%%%%%%%%%");
		  //VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
      //JQ Spira issue:006511
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, "00", 2 ) );
    }
    else
    {
      if( vim_strlen( txn.host_rc_asc ) > 0 )
        VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( host_response_tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 ) );
    }
  }

  *online_result = local_online_result;
  
  /* return without error */
  DBG_MESSAGE("exit OK. vim_emv_CB_go_online");
  return VIM_ERROR_NONE;
}

#endif

// RDD 280916 Check Presence of RID in our AID list
static VIM_BOOL IsValidRID( VIM_CHAR *RIDPtr, VIM_BOOL *AllRIDS )
{

#define _ALL_RIDS "A000000000"

	*AllRIDS = VIM_FALSE;
	if( !vim_strncmp( RIDPtr, _ALL_RIDS, VIM_ICC_RID_LEN*2 ))
		*AllRIDS = VIM_TRUE;
	else if( vim_strlen( RIDPtr ) != VIM_ICC_RID_LEN*2 )
		return VIM_FALSE;
	else if( *RIDPtr != 'A' )
		return VIM_FALSE;
	return VIM_TRUE;
}




static VIM_BOOL IsCurrentRID( transaction_t *transaction, VIM_CHAR *RIDPtr )
{
	VIM_CHAR TxnAIDAsc[AID_MAX_LEN*2];
	VIM_SIZE len = (2 * terminal.emv_applicationIDs[transaction->aid_index].aid_length);		
	
	vim_mem_clear( TxnAIDAsc, AID_MAX_LEN*2 );
	hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, TxnAIDAsc, len );
	VIM_DBG_STRING( TxnAIDAsc );

	return ( !vim_strncmp( RIDPtr, TxnAIDAsc, VIM_ICC_RID_LEN*2 ) ? VIM_TRUE : VIM_FALSE );
}



VIM_ERROR_PTR AppendTag( transaction_t *transaction, VIM_CHAR *TagData, VIM_UINT8 **de55_buffer_ptr, VIM_SIZE *de55_length )
{
  VIM_TLV_LIST tag_list;
  VIM_UINT8 HexTag[6];

  vim_mem_clear( HexTag, VIM_SIZEOF( HexTag ));

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, VIM_SIZEOF(transaction->emv_data.buffer), VIM_FALSE ));

  //VIM_DBG_TLV( tag_list );

  asc_to_hex( TagData, HexTag, vim_strlen(TagData));

  if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, (const void *)HexTag ) )  
  {
        VIM_SIZE current_item_length = tag_list.current_item.tag_length + 
                                       tag_list.current_item.length_length + 
                                       tag_list.current_item.length;
				
		// RDD 100616 v560-2 SPIRA:8991 - Zero length tags now forbidden
		if( tag_list.current_item.length == 0 )
		{			
			VIM_CHAR DebugBuffer[50];
			vim_sprintf( DebugBuffer, "SPIRA:8991 Zero Length Tag Removed (ext):%s", TagData  );
			pceft_debug( pceftpos_handle.instance, DebugBuffer );
			return VIM_ERROR_NONE;
		}		
		vim_mem_copy( *de55_buffer_ptr, tag_list.current_item.tag, current_item_length );
		*de55_length += current_item_length;
		*de55_buffer_ptr += current_item_length;
  }
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR AddTags( transaction_t *transaction, VIM_CHAR *TagPtr, VIM_UINT8 **de55_buffer_ptr, VIM_SIZE *de55_length )
{
	#define _DE55_TAG	"DE55TAG>"
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	// RIDs match so look for the tags to add to the buffer
	while( res == VIM_ERROR_NONE )			
	{				
		VIM_CHAR TagData[10];			
		//VIM_SIZE len = *de55_length;	
		VIM_CHAR *EndPtr = VIM_NULL;		
		vim_mem_clear( TagData, VIM_SIZEOF( TagData )); 
        if ((res = xml_get_tag_end(TagPtr, _DE55_TAG, TagData, &EndPtr)) != VIM_ERROR_NONE)
        {
			// Run out of tags so we're done 			
			//VIM_DBG_DATA( TagData, len );			
			return VIM_ERROR_NONE;				
		}		
		TagPtr = EndPtr;
		// Add this tag to the buffer		

		AppendTag( transaction, TagData, de55_buffer_ptr, de55_length );
		VIM_DBG_STRING( TagData );					
	}
	return VIM_ERROR_NONE;
}

// RDD SPIRA:9043 Add Optional Tags from Termcfg.xml
VIM_ERROR_PTR emv_add_optional_tags( transaction_t *transaction, VIM_AS2805_MESSAGE_TYPE msg_type, VIM_UINT8 **de55_buffer_ptr, VIM_SIZE *de55_length, VIM_SIZE destination_max_size )
{
#define _SCHEME_TAG	"SCHEME>"
#define _RID_TAG	"RID>"
	#define RIDS		 7

    VIM_CHAR *StartPtr = terminal.configuration.AdditionalDE55Data;
    VIM_CHAR *EndPtr = StartPtr;
    VIM_SIZE n;

    VIM_DBG_STRING(terminal.configuration.AdditionalDE55Data);

    for (n = 0; n<RIDS; n++)
    {
        VIM_BOOL AllRids = VIM_FALSE;
        VIM_CHAR Container[500];
        VIM_CHAR RID[1 + VIM_ICC_RID_LEN * 2];
        //VIM_CHAR *RIDPtr = Container;
        vim_mem_clear(Container, VIM_SIZEOF(Container));

        // Get the next SCHEME Container
        VIM_ERROR_RETURN_ON_FAIL(xml_find_next(&EndPtr, _SCHEME_TAG, Container));

        vim_mem_clear(RID, VIM_SIZEOF(RID));
        xml_get_tag(Container, _RID_TAG, RID);

        if (IsValidRID(RID, &AllRids))
        {
            VIM_CHAR *TagPtr = Container;

            if (AllRids == VIM_FALSE)
            {
                if (!IsCurrentRID(transaction, RID)) continue;
            }
            AddTags(transaction, TagPtr, de55_buffer_ptr, de55_length);
            AllRids = VIM_FALSE;
        }
    }
    return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/* Create TLV list of data to be included into the host message */
/*VIM_ERROR_PTR emv_get_host_data(transaction_t *transaction, msgAS2805_type_t msg_type, VIM_UINT8 *de55_buffer, VIM_SIZE *de55_length )*/
VIM_ERROR_PTR emv_get_host_data
(
  transaction_t *transaction, 
  VIM_AS2805_MESSAGE_TYPE msg_type, 
  VIM_UINT8 *de55_buffer, 
  VIM_SIZE *de55_length,
  VIM_SIZE destination_max_size
)
{
  VIM_SIZE i;
  VIM_SIZE length = 0;
  VIM_SIZE max_de55_length = destination_max_size;
  VIM_SIZE host_tags_cnt = VIM_LENGTH_OF(host_tags_auth);
  VIM_UINT8 *Ptr;
  VIM_TLV_LIST tag_list;
  //VIM_TAG_ID host_tags[50]; /*  worst case: all host tags are one byte length */
  VIM_TAG_ID *TagPtr = (VIM_TAG_ID *)host_tags_auth;
  VIM_UINT8 *save_de55_buffer = de55_buffer;
  VIM_BOOL ModifyTVR = VIM_FALSE;
  VIM_BOOL IsReversal = ((msg_type == MESSAGE_WOW_420) || (msg_type == MESSAGE_WOW_421)) ? VIM_TRUE : VIM_FALSE;
 // VIM_BOOL IsSAF = ((msg_type == MESSAGE_WOW_220) || (msg_type == MESSAGE_WOW_221)) ? VIM_TRUE : VIM_FALSE;

  *de55_length = 0;
  //VIM_DBG_DATA(transaction->emv_data.buffer, transaction->emv_data.data_size);

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size,
	  VIM_SIZEOF(transaction->emv_data.buffer), VIM_FALSE));

  //VIM_DBG_TLV(tag_list);

  // RDD 241013 SPIRA:6987 Check that the amount returned by the reader matches the amount sent to it 
  DBG_VAR( transaction->type );
  // RDD 260522 JIRA PIRPIN-1633 Surcharge screws this up so remove - even for R10 - hopefully the check is no longer needed
#if 0
  if ((transaction->type == tt_sale) || (transaction->type == tt_refund))
  {
      if ((!IsSAF) && (TERM_USE_PCI_TOKENS))
      {
          VIM_ERROR_RETURN_ON_FAIL( CheckTagAmt( &tag_list, VIM_TRUE, transaction ));
      }
  }
#endif

  //VIM_ERROR_RETURN_ON_FAIL(emv_get_host_tags( host_tags, &host_tags_cnt, transaction ));	
  DBG_MESSAGE( "************** EXTRACT EMV TAGS NEEDED FOR DE55 **************");
  DBG_VAR( host_tags_cnt );

  for( i=0; i<host_tags_cnt; i++, TagPtr++ )
  {
	  ModifyTVR = VIM_FALSE;

	  if( IsTag( VIM_TAG_EMV_UPI_SCRIPT_RESULTS, *TagPtr, vim_strlen(*TagPtr)))
	  {
		  if(( !IsUPI( transaction ))||( !IsReversal ))
			continue;
	  }

	  if( IsTag( VIM_TAG_EMV_VISA_SCRIPT_RESULTS, *TagPtr, vim_strlen(*TagPtr)))
	  {
		  if(( !IsVISA( transaction ))||( !IsReversal ))
			continue;
	  }

	  // Don't send the Response code in 0200 messages ( RDD 020714 - or 0100 messages )
	  if(( IsTag( VIM_TAG_EMV_RESPONSE_CODE, *TagPtr, vim_strlen(VIM_TAG_EMV_RESPONSE_CODE) ) && (( msg_type == MESSAGE_WOW_200 )||( msg_type == MESSAGE_WOW_100 ))))
	  {
		  DBG_MESSAGE( "0200 - skip VIM_TAG_EMV_RESPONSE_CODE...." );
		  continue;
	  }
	  // RDD 081114 Switch team requested that we don't send the Response code in ALH Completions so they can be distiguished from other 220s
	  if(( IsTag( VIM_TAG_EMV_RESPONSE_CODE, *TagPtr, vim_strlen(VIM_TAG_EMV_RESPONSE_CODE) ) && ( transaction->type == tt_checkout )))
	  {
		  DBG_MESSAGE( "0220 checkout - skip VIM_TAG_EMV_RESPONSE_CODE...." );
		  continue;
	  }

	  if(( card_picc == txn.card.type) && ( IsEmvRID( VIM_EMV_RID_EFTPOS, transaction )) && ( IsTag(  VIM_TAG_EMV_TVR, *TagPtr, vim_strlen( VIM_TAG_EMV_TVR) ))) 	  
	  {
		  // RDD - need to take action here for SPIRA:8524 TVR of zeros is always expected by EPAL HOST. If correct value is sent the ARQC is rejected.
		  DBG_MESSAGE( " SPIRA:8524 " );
		  ModifyTVR = VIM_TRUE;
	  }
	  
	  if(( !IsEmvRID( VIM_EMV_RID_MASTERCARD, transaction )) && ( IsTag( VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE, *TagPtr, vim_strlen(VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE) ))) 
	  {
		  DBG_MESSAGE( "( non-MC txn) - Skip tag VIM_TAG_EMV_TRANSACTION_CATEGORY_CODE...." );
		  continue;
	  }

	  // RDD 231012 SPIRA:6226 - Only include VIM_TAG_EMV_AID_ICC for EFTPOS EMV txns
	  if(( !IsEmvRID( VIM_EMV_RID_EFTPOS, transaction )) && ( IsTag( VIM_TAG_EMV_AID_ICC, *TagPtr, vim_strlen(VIM_TAG_EMV_AID_ICC) ))) 
	  {
		  DBG_MESSAGE( "( non-EFTPOS txn) - Skip tag VIM_TAG_EMV_AID_ICC..." );
		  continue;
	  }      

	  //VIM_DBG_TLV(tag_list);

	  // If Tag is not being excluded for some reason then copy it into the DE55 buffer
      if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, *TagPtr ) )
      {
        VIM_SIZE current_item_length = tag_list.current_item.tag_length + 
                                       tag_list.current_item.length_length + 
                                       tag_list.current_item.length;

		//DBG_MESSAGE( "***  DE55 TAG INCLUDED: ****");

		// RDD 100616 v560-2 SPIRA:8991 - Zero length tags now forbidden
		if( tag_list.current_item.length == 0 )
		{			
			VIM_CHAR TagNameBuffer[10];
			VIM_CHAR DebugBuffer[50];
			vim_mem_clear( TagNameBuffer, VIM_SIZEOF( TagNameBuffer ));
			hex_to_asc( (VIM_UINT8 *)*TagPtr, TagNameBuffer, 2*tag_list.current_item.tag_length );
			vim_sprintf( DebugBuffer, "SPIRA:8991 Zero Length Tag Removed:%s", TagNameBuffer  );
			// RDD 061216 v561-5 Made this debug more only
			pceft_debug_test( pceftpos_handle.instance, DebugBuffer );
			continue;
		}

       // DBG_VAR(tag_list.current_item.length);
       // DBG_DATA(tag_list.current_item.value, current_item_length);
        
		// RDD - need to take action here for SPIRA:8524 TVR of zeros is always expected by EPAL HOST. If correct value is sent the ARQC is rejected.
        if( length + current_item_length <= max_de55_length )
        {
			VIM_SIZE len;
			VIM_UINT8 Zero_TVR[] = { 0x95, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00 };
			if( ModifyTVR )
			{
				len = VIM_SIZEOF( Zero_TVR );
				vim_mem_copy( de55_buffer, Zero_TVR, len );
			}
			else
			{
				len = current_item_length;
				vim_mem_copy( de55_buffer, tag_list.current_item.tag, len );
			}
          
			de55_buffer += len;          
			length += len;
        }
      }
	  else
	  {
		  DBG_MESSAGE( "***  DE55 TAG NOT FOUND: ****");
		  //DBG_DATA( *TagPtr, VIM_LENGTH_OF(*TagPtr) );

		  if(( IsTag( VIM_TAG_EMV_RESPONSE_CODE, *TagPtr, vim_strlen(*TagPtr) ) ) && ( msg_type != MESSAGE_WOW_200 ))
		  {
			  VIM_CHAR terminal_rc[2+1];
#if 0		  // Now they don't want 8A in checkout transactions
			  // RDD - temp fix for checkouts...
			  if( transaction->type == tt_checkout )
			  {
				  vim_strcpy( terminal_rc, "00" );
			  }
			  
#endif
			  
			  if( transaction->type != tt_checkout )
			  {				
				  DBG_MESSAGE( "<<<<<<< PICC: ADD Z3 TO TXN EMV BUFFER >>>>>>>>>" );			  
				  vim_strcpy( terminal_rc, "Z3" );
			  }
		
			  // RDD update field 8A for second GEN AC - this will generate the correct cryptogram if the txn passed EMV EFB and was stored as an advice		  
			  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( &tag_list, VIM_TAG_EMV_RESPONSE_CODE, terminal_rc, 2 ));
			  i--; // rewind and try again...

			  // RDD 220413 SPIRA:6691. Need to decrement Tag Pointer too as the tag was being added to the buffer. 
			  TagPtr--;
			  break;
		  }
	  }
  }

  *de55_length = length;
 //VIM_DBG_VAR(*de55_length);
  //VIM_DBG_DATA(save_de55_buffer, *de55_length);

  /*  save de55 data to be sent for "offline declined" printing */
  if (length)
  {
    req_de55_length = length;
    vim_mem_copy(req_de55_buffer, save_de55_buffer, MIN(length, VIM_SIZEOF(req_de55_buffer)));
  }

  // RDD SPIRA:9074 Add Optional Tags from Termcfg.xml
  Ptr = de55_buffer;
  emv_add_optional_tags( transaction, msg_type, &Ptr, de55_length, destination_max_size );
  VIM_DBG_DATA( de55_buffer, *de55_length );
  
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR emv_remove_card( VIM_CHAR *Message )
{  
  VIM_BOOL icc_removed;
  //VIM_BOOL icc_present = VIM_FALSE;
  VIM_CHAR Display[100];
#ifdef _WIN32
  VIM_SIZE n;
  return VIM_ERROR_NONE;
#endif
	DBG_MESSAGE("INSTRUCT REMOVE CARD 1");

  vim_mem_clear( Display, VIM_SIZEOF( Display ) );
  if( vim_strlen( Message ) )
  {	
	  vim_sprintf( Display, "%s\n%s", Message, "REMOVE CARD" );
  }
  else
  {
	  vim_strcpy( Display, "REMOVE CARD" );
  }

  if( icc_handle.instance_ptr == VIM_NULL )
  {
    vim_icc_new(&icc_handle, VIM_ICC_SLOT_CUSTOMER);
  }

  icc_is_removed(icc_handle.instance_ptr, &icc_removed );

  if( txn.emv_enabled ) 
  {
	// 
    /* power off cards anyway */
	if( txn.error != VIM_ERROR_NONE )
		pceft_debug_error( pceftpos_handle.instance, "Power Off chip card:Terminate_CardSlot", txn.error );
    vim_icc_power_off (icc_handle.instance_ptr);
    txn.emv_enabled = VIM_FALSE;
  }

  // RDD 241012 added for testing emv card read issues
  if( terminal_get_status( ss_wow_basic_test_mode ) )
  {
	if( terminal_get_status( ss_auto_test_mode ) )
		return VIM_ERROR_NONE;
  }

  if( !icc_removed )
  {
    VIM_RTC_UPTIME now, beep_time;
	DBG_MESSAGE("INSTRUCT REMOVE CARD 2");

	// RDD 090512 SPIRA:5453 ensure default branding run after card removed during IDLE		
	terminal.screen_id = 0;

	//VIM_DBG_ERROR( emv_transaction_error );
	//VIM_DBG_ERROR( txn.error );
    if (( txn.error == VIM_ERROR_EMV_BAD_CARD_RESET )||( txn.error == VIM_ERROR_EMV_FAULTY_CHIP ))
    {
        
      display_screen( DISP_BAD_CHIP_READ_REMOVE_CARD, VIM_PCEFTPOS_KEY_ERROR );
	  vim_kbd_sound_invalid_key();
	  if( CardRemoved( 500 ) ) return VIM_ERROR_NONE;
    }

    /* read current time */
    vim_rtc_get_uptime(&now);
    beep_time = now + 4000;

	vim_sound(VIM_TRUE);
	//vim_kbd_sound_invalid_key();

#ifdef _WIN32
	for( n=0;n<3;n++ )
#else
    while (1)
#endif
    {

      {
          display_screen(DISP_REMOVE_CARD_B, VIM_PCEFTPOS_KEY_NONE, "");
      }

	  if( CardRemoved( 500 ) ) break;
      display_screen(DISP_REMOVE_CARD_A, VIM_PCEFTPOS_KEY_NONE, "");

	  if( CardRemoved( 500 ) ) break;

	  vim_rtc_get_uptime(&now);
      if (now > beep_time)
      {
        /* beep every 4 seconds */
		vim_sound(VIM_TRUE);
		//vim_kbd_sound_invalid_key();
        beep_time = now + 4000;
      }
    }
  }
  DBG_POINT;
  icc_close();
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR vim_emv_CB_error
(
  VIM_ERROR_PTR error
)
{
  //VIM_KBD_CODE key_pressed;
  VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];  
  VIM_CHAR Account[3+1];   
   // RDD 140416 Kernel 7.0 change   
  VIM_DBG_ERROR( txn.error ); 
  VIM_DBG_ERROR( error );

  // RDD 210519 Quick fix for JIRA WWBP-558
  if( 1 )
  {
      if( error == VIM_ERROR_EMV_TXN_ABORTED )
      {
          if (CardRemoved(500))
          {
              VIM_DBG_WARNING("JIRA WWBP-688");
              txn.error = VIM_ERROR_EMV_CARD_REMOVED;
          }
          else
          {
              txn.error = error;
          }
      }
      if(( txn.error == VIM_ERROR_POS_CANCEL) || ( txn.error == VIM_ERROR_EMV_CARD_REMOVED ))
      {
          VIM_DBG_WARNING("Remap Error");
          error = txn.error;
      }
  }

#if 1
   txn.error = VIM_ERROR_NONE;
#endif
  
  vim_mem_clear( CardName, VIM_SIZEOF(CardName) );  
  vim_mem_clear( Account, VIM_SIZEOF(Account) );  

  // RDD 210314 - Noticed that the PcEft interface is not yet up and running so only display these messages on the PinPad
  terminal.initator = it_pinpad;

  if( GetCardName( &txn, CardName ) == VIM_ERROR_NONE )
  {
    // Don't try to get Account if no Card Name
    get_acc_type_string_short( txn.account, Account );      
  }


  // RDD 130213 - this all needs to be rewritten because VIM_ERROR_EMV_PIN_ENTERED is returned whether the PIN is correct or not
  // VIM_ERROR_EMV_PIN_ENTERED is passed in here by the kernel as soon as OK key has been pressed
  // so basically what we have to do here is to set the st_offline_pin_verified flag so that the thread which is checking for POS 
  // cancel etc. will see that a PIN has been entered and close down the PCEFT interface and exit .
  // Our own PCEFT interface then needs to be restarted from the various entry points, then we can display the results to the screen and
  // the POS - Incorrect PIN will re-enter here. Bypass and Correct PIN will go elsewhere. See incorrectpin.txt and corectpin.txt in phase4 folder for debug
  // of where the code needs to go

  // RDD 180213 Only Certain Errors supplied by the kernel should result in aborting the transaction
  // the rest are for information only ! eg. VIM_ERROR_APPLICATION_EXPIRED should not result in an abort

//#ifdef _PHASE4
#if 1
  VIM_DBG_VAR(txn.emv_pin_entry_params.offline_pin);

  // RDD 220213 - kernel will call here twice for invalid pin so don't try to reset POS a second time
  if(( txn.emv_pin_entry_params.offline_pin )&&( error != VIM_ERROR_EMV_PIN_INCORRECT ))
  {
    //pceft_debug_error( pceftpos_handle.instance, "---- EMV CB ERROR OFFLINE PIN EVENT PROCESSED ----", error );
    if( error == VIM_ERROR_EMV_PIN_BYPASSED )
    {
      DBG_MESSAGE("-------------- OFFLINE PIN WAS BYPASSED --------------");          
      txn.emv_error = VIM_ERROR_EMV_PIN_BYPASSED;
      txn.pin_bypassed = VIM_TRUE;
      DisplayProcessing( DISP_PROCESSING_WAIT );
      //pos_delete_wait_start();
    }
    else if( error == VIM_ERROR_EMV_PIN_ENTERED )
    {   
      DBG_MESSAGE("-------------- OFFLINE PIN WAS ENTERED --------------");         
      transaction_set_status( &txn, st_offline_pin_verified );
      terminal.initator = it_pinpad;
      DisplayProcessing( DISP_PROCESSING_WAIT );
      //pos_delete_wait_start();
      // if the pin was entered then we need to set a flag to let the other thread know it needs to terminate
    }     
    // The cancel key was pressed - this kernel aborts the EMV seesion if this happens and doesn't appear to be configurable
    else if( error == VIM_ERROR_USER_CANCEL )
    {   
      DBG_MESSAGE("-------------- OFFLINE PIN CANCELLED BY USER --------------");         
      txn.emv_error = VIM_ERROR_USER_CANCEL;        
      DisplayProcessing( DISP_PROCESSING_WAIT );
	  
	  //pos_delete_wait_start();
    } 

    // The cancel key was pressed - this kernel aborts the EMV seesion if this happens and doesn't appear to be configurable
    else if( error == VIM_ERROR_POS_CANCEL )
    {   
      DBG_MESSAGE("-------------- OFFLINE PIN CANCELLED BY POS --------------");          
      txn.emv_error = VIM_ERROR_POS_CANCEL;       
      DisplayProcessing( DISP_PROCESSING_WAIT );
      //pos_delete_wait_start();
    } 

    // RDD 150213 - note that we can't send anything to the POS until comms have been restored to this thread
  }

#endif

  if( error == VIM_ERROR_EMV_REMOVE_CARD_ERROR )
  {    
    return emv_remove_card( VIM_FALSE );
  }
  else if (error == VIM_ERROR_EMV_PIN_INCORRECT)
  {
    VIM_TAG_ID read_tags[] = {VIM_TAG_EMV_PIN_TRY_COUNTER};
    VIM_UINT8 pin_tries=0, tag_buffer[100];
    VIM_TLV_LIST tags;
    char transaction_type[10];


    // RDD 270421 v724-12 JIRA PIRPIN-759 : Unexpected offline PIN errors ( for online PIN )
    if (txn.emv_pin_entry_params.offline_pin == VIM_FALSE)
    {
        pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-759");
        return VIM_ERROR_NONE;
    }

    DBG_MESSAGE("--------------PIN WAS CHECKED AND FOUND INVALID --------------");  
  transaction_clear_status( &txn, st_offline_pin_verified );

  /* set PIN error flag */
    txn.emv_pin_entry_params.wrong_pin = VIM_TRUE;

    if (vim_tlv_create(&tags, tag_buffer, VIM_SIZEOF(tag_buffer)) == VIM_ERROR_NONE)
    {
        if (vim_emv_read_kernel_tags(read_tags, VIM_LENGTH_OF(read_tags), &tags) == VIM_ERROR_NONE)
        {
            if (vim_tlv_search(&tags, VIM_TAG_EMV_PIN_TRY_COUNTER) == VIM_ERROR_NONE)
            {
                vim_tlv_get_uint8(&tags, &pin_tries);
                get_txn_type_string(&txn, transaction_type, VIM_SIZEOF(transaction_type), VIM_FALSE);
            }
        }
    }
    pceft_debug(pceftpos_handle.instance, "OLP:Retry");

    display_screen( DISP_INVALID_PIN_RETRY, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), CardName, Account );
    vim_event_sleep( 2000 );

  //vim_screen_kbd_or_touch_read(&key_pressed, 3000, VIM_FALSE);   

  }
  else if (error == VIM_ERROR_EMV_SELECT_APPLICATION_ERROR)
  { /*  memorize this for fallbak purposes */
    /* TODO: display SELECTION BLOCKED and wait 2 seconds */
    display_screen (DISP_SELECTION_APP_BLOCKED, VIM_PCEFTPOS_KEY_NONE);
    vim_kbd_sound_invalid_key ();
    vim_event_sleep( 2000 );
   
    emv_blocked_application = VIM_TRUE;
    /*  TODO: silently return for now */
  }
  else if (VIM_ERROR_EMV_6985_ON_GENAC1 == error)
  {   
    emv_fallbak_step_passed = VIM_FALSE;
  }

  // RDD 080512 SPIRA:5418
  else if( error == VIM_ERROR_EMV_PIN_BLOCKED )
  { 
    VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1]; 
    vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );

    // Already been here once....
    if( txn.error == VIM_ERROR_EMV_PIN_BLOCKED )
      return VIM_ERROR_NONE;

    //VIM_CHAR account_string_short[3+1];

    DBG_MESSAGE("^^^^^^^^^^^^^ PIN BLOCKED !!! ^^^^^^^^^^^^^^^^");
    //get_acc_type_string_short(txn.account, account_string_short);    
      GetCardName( &txn, CardName );

    // RDD 110313 SPIRA:6621 if Pin tries Exceeded we still need to go online in case of sig
#if 0
    txn.error = error;
#else
    VIM_DBG_ERROR( txn.error );
    error = VIM_ERROR_NONE;
#endif

    display_screen( DISP_PIN_TRY_LIMIT_EXCEEDED, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total)  );          
    txn_save();
    vim_event_sleep( 2000 );

    terminal.initator = it_pceftpos;
    VIM_ERROR_IGNORE( PressOKToContinue( CardName, "CR", VIM_TRUE ));
  }
  else if( error == VIM_ERROR_USER_CANCEL )
  {
    txn.error = error;
    DBG_MESSAGE( "Offline PIN User Cancelled" );
  }
  /* return without error */
  DBG_MESSAGE("vim_emv_CB_error EXIT");
  terminal.initator = it_pceftpos;

  //txn.error = error;
  VIM_DBG_ERROR(error);

  //if (IsTerminalType("P400"))
  if(1)
  {
      if (error == VIM_ERROR_EMV_CARD_REMOVED)
      {
          VIM_DBG_WARNING("Remap Error Code");
          txn.error = error;
      }
  }

  VIM_DBG_ERROR(txn.error);

  ////////////// TEST V7 TEST V7 /////////
  txn.error_backup = error;
  ////////////// TEST V7 TEST V7 /////////

  return VIM_ERROR_NONE;
}





/* #define _EMV_FILE_COPY_TEST_ */

#ifdef _EMV_FILE_COPY_TEST_
#define DUMP_DISK_VOLUME_NAME     "HOST"
#define DUMP_FILE_NAME_TEMPLATE "/"DUMP_DISK_VOLUME_NAME"/table_%d.dat"
#define BUF_LEN_MAX   512
/*----------------------------------------------------------------------------*/
static void dump_table_get_file_name(VIM_UINT8 table_id, char *file_name, VIM_UINT32 buf_len)
{  
  vim_snprintf(file_name, buf_len, DUMP_FILE_NAME_TEMPLATE, table_id);
}
/*----------------------------------------------------------------------------*/
static void dumpfile(char* file_name, char* file_name_dump)
{
  char buffer[BUF_LEN_MAX];
  S_FS_FILE *hFile;
  S_FS_FILE *hDumpFile;
  VIM_INT32 res;

  hFile = FS_open((char *)file_name, "r");

  if (hFile == VIM_NULL)
  {
    DBG_STRING(file_name);
    return;
  }
  
  hDumpFile = FS_open((char *)file_name_dump, "a");

  if (hDumpFile == VIM_NULL)
  {
    DBG_STRING(file_name_dump);
    return;
  }

  while((res = FS_read((void *)buffer, 1, BUF_LEN_MAX, hFile)) > 0)
  {
    // DBG_VAR(res);

    if(res < 0)
      break;

    res = FS_write((void *)buffer, 1, res, hDumpFile);
    
    // DBG_VAR(res);
  }

  FS_close(hFile);
  FS_close(hDumpFile);
  DBG_STRING(file_name);
  DBG_STRING(file_name_dump);
  
}
#endif
/*----------------------------------------------------------------------------*/
#if 0
VIM_ERROR_PTR emv_cb_test(void)
{            
#ifdef _EMV_FILE_COPY_TEST_
  char file_name[30];
  char file_name_dump[30];
  VIM_UINT8 i;
  S_FS_PARAM_CREATE ParamCreat;
  int rc ;

  rc = FS_mount("/"DUMP_DISK_VOLUME_NAME,&ParamCreat.Mode);
  // DBG_VAR(rc);

  for(i = FILE_WBLT_TABLE ; i <= FILE_ICC_TABLE ; i++)
  {
    table_get_file_name(i, file_name, VIM_SIZEOF(file_name));
    dump_table_get_file_name(i, file_name_dump, VIM_SIZEOF(file_name_dump));
    dumpfile(file_name, file_name_dump);
  }
  return VIM_ERROR_NONE;
#else
  VIM_EMV_AID rid;
  VIM_UINT8 capk_index;
  VIM_EMV_CAPK capk;
  VIM_EMV_TRANSACTION_VALUES param;
  VIM_EMV_RISK_MANAGEMENT_PARAMETERS parameters;
  VIM_EMV_AID aid;
  VIM_SIZE aid_count = 10;
  VIM_EMV_AID aid_list[10];
  VIM_UINT32 i;
  VIM_BOOL pin_bypassed = VIM_FALSE;

  vim_mem_copy(rid.aid, "\xA0\x00\x00\x00\x03", 5);
  capk_index = 4;
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_get_CAPK(&rid, capk_index, &capk));
  // DBG_VAR(capk.modulus_size);
  // DBG_VAR(capk.modulus);
  // DBG_VAR(capk.exponent_size);
  // DBG_VAR(capk.exponent);
  // DBG_VAR(capk.expiry_date.year);
  // DBG_VAR(capk.expiry_date.month);
  // DBG_VAR(capk.expiry_date.day);
  // DBG_VAR(capk.hash);
  
  vim_mem_clear(&param, VIM_SIZEOF(param));
  vim_mem_clear(&parameters, VIM_SIZEOF(parameters));
  vim_mem_clear(&aid, VIM_SIZEOF(aid));
  vim_mem_copy(aid.aid, "\xA0\x00\x00\x00\x25\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 16);

  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_get_transaction_data_GPO(&param, &aid));
  
  // DBG_VAR(param.appication_version_numbers);
  // DBG_VAR(param.default_DDOL);
  // DBG_VAR(param.default_TDOL);
  // DBG_VAR(param.is_magstripe_fallback_on);
  // DBG_VAR(param.is_pin_bypass_on);
/*   // DBG_VAR(param.cfg_CVM_list); */

  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_get_risk_management_parameters(&parameters, &aid));
  
  // DBG_VAR(parameters.terminal_action_code_default);
  // DBG_VAR(parameters.terminal_action_code_denial);
  // DBG_VAR(parameters.terminal_action_code_online);
  // DBG_VAR(parameters.threshold_amount);
  // DBG_VAR(parameters.floor_limit);
  // DBG_VAR(parameters.target_percent);
  // DBG_VAR(parameters.max_target_percent);

  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_get_aid_list(&aid_count, aid_list));

  // DBG_VAR(aid_count);

  for(i = 0; i < aid_count; i++)
  {
    // DBG_VAR(aid_list[i].aid);
    // DBG_VAR(aid_list[i].aid_size);
    // DBG_VAR(aid_list[i].label_name);
  }
  
  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_select_app(&aid_count, aid_list));
  
  // DBG_VAR(aid_count);  

  VIM_ERROR_RETURN_ON_FAIL(vim_emv_CB_enter_pin(1, 1, &pin_bypassed, &aid));
  
  DBG_VAR(pin_bypassed); 
  
  return VIM_ERROR_NONE;
#endif
}
#endif

#if 1
VIM_BOOL check_emv_expiry_date(VIM_UINT8 *expiry_date)
{
  return ((txn.time.year > 2000 + bcd_to_bin(expiry_date, 2)) ||
          ((txn.time.year == 2000 + bcd_to_bin(expiry_date, 2)) &&
          (txn.time.month > bcd_to_bin(&expiry_date[1], 2))));
}
#endif
/**
 * Retrieve the track 2 data from the kernel.
 *
 * Track 2 will be built from pan and expiry tags (and discretionary data tag) if track 2 doesnt exist.
 * If there is a mismatch between track 2 tag and pan/expiry tags then an error will be reported.
 */

static VIM_ERROR_PTR emv_get_track_2(transaction_t *transaction)
{
  VIM_SIZE i;
  VIM_TLV_LIST tags_list;
 // VIM_UINT8 tlv_buffer[512];

  const VIM_TAG_ID tags[] = {
	VIM_TAG_EMV_ISSUER_COUNTRY_CODE,
    VIM_TAG_EMV_TERMINAL_COUNTRY_CODE,
	VIM_TAG_EMV_AID_ICC,
    VIM_TAG_EMV_TRACK2_EQUIVALENT_DATA,
    VIM_TAG_EMV_APPLICATION_PAN,
    VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE,
	VIM_TAG_EMV_CARDHOLDER_NAME,
	VIM_TAG_EMV_APPLICATION_PREFERRED_NAME,
	// RDD 080313 SPIRA 6622 - need app label here 
	VIM_TAG_EMV_APPLICATION_LABEL,
    VIM_TAG_EMV_TRACK2_DISCRETIONARY_DATA
  };

  DBG_MESSAGE( " ------------- emv_cb GetTrack2 ---------------- ");
  // RDD 310113 If we're not running a current EMV txn and there is existing emv data in the buffer then use it
  if( transaction->emv_enabled )
  {
	  if( transaction->emv_data.data_size )
	  {	
		  // We're not running a current emv txn ( might be processing a SAF ) so try to pick up the AID from the txn list.	  
		  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tags_list, transaction->emv_data.buffer, transaction->emv_data.data_size, VIM_SIZEOF(transaction->emv_data.buffer), VIM_FALSE ));  
	  }
	  else		
	  {			
		  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< CREATE EMV DATA BUFFER  >>>>>>>>>>>>>>>>>>>>>");	

		  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create( &tags_list, transaction->emv_data.buffer, VIM_SIZEOF(transaction->emv_data.buffer)));
		  //VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create( &tags_list, tlv_buffer, VIM_SIZEOF(tlv_buffer)));				
		  VIM_ERROR_RETURN_ON_FAIL( vim_emv_read_kernel_tags( tags, VIM_SIZEOF(tags)/VIM_SIZEOF(tags[0]), &tags_list ) );  
		  transaction->emv_data.data_size = tags_list.total_length;
		  DBG_VAR(transaction->emv_data.data_size);
	  }
  }
  else
  {	
	  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< TRIED TO READ EMV TAGS WHEN NO ACTIVE EMV TXN >>>>>>>>>>>>>>>>>>>>>");		  	
	  return VIM_ERROR_NOT_FOUND;
  }
    DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");  
	DBG_VAR( transaction->emv_data.data_size );
	DBG_DATA(transaction->emv_data.buffer, transaction->emv_data.data_size);  
	DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");

  /* RDD: If the Application Name is present then we use this instead of the card type */
  if ( vim_tlv_search( &tags_list, VIM_TAG_EMV_APPLICATION_PREFERRED_NAME ) == VIM_ERROR_NONE )
  {
	// If there is a CardName tag then copy the data
    if ( tags_list.current_item.length )
    {
		VIM_CHAR *Ptr = transaction->card.data.chip.application_name;
		DBG_VAR( tags_list.current_item.length );
		DBG_STRING( "Found Application Name" );
		vim_mem_copy( Ptr, tags_list.current_item.value, tags_list.current_item.length );
		DBG_STRING(Ptr);
	}
  }

  /* RDD: If the CardHolder Name is present then parse this too */
  if ( vim_tlv_search( &tags_list, VIM_TAG_EMV_CARDHOLDER_NAME ) == VIM_ERROR_NONE )
  {
	// If there is a CardName tag then copy the data
    if (tags_list.current_item.length)
    {
		VIM_CHAR *Ptr = transaction->card.data.chip.customer_name;
		DBG_STRING( "Found Card Holder Name" );
		vim_mem_copy( Ptr, tags_list.current_item.value, tags_list.current_item.length );
		DBG_VAR(Ptr);
	}
  }

  /* If track 2 tag is present, use this data and validate against the pan and expiry tags if they are present */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK2_EQUIVALENT_DATA) == VIM_ERROR_NONE)
  {
    if (tags_list.current_item.length)
    {
      /* Convert to ascii */
	  DBG_STRING( "Found Track2 Equivalent Data" );
      bcd_to_asc(tags_list.current_item.value, transaction->card.data.chip.track2, tags_list.current_item.length * 2);
      transaction->card.data.chip.track2_length = MIN(tags_list.current_item.length * 2, 37);
      
      /* Figure out exact length by searching for ? */
      for (i = 0; i < transaction->card.data.chip.track2_length; i++)
      {
        if (transaction->card.data.chip.track2[i] == '?')
          break;
      }
      transaction->card.data.chip.track2_length = i;

      /* Mask out rest of track with nulls */
      if (transaction->card.data.chip.track2_length < VIM_SIZEOF(transaction->card.data.chip.track2))
      {
		  DBG_POINT;
        vim_mem_set
        (
          &transaction->card.data.chip.track2[transaction->card.data.chip.track2_length], 
          0, 
          VIM_SIZEOF(transaction->card.data.chip.track2) - transaction->card.data.chip.track2_length
        );
      }
	            

	  DBG_DATA( transaction->card.data.chip.track2, transaction->card.data.chip.track2_length );


      /* Check the PAN to see if it matches */
      if( vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_PAN ) == VIM_ERROR_NONE)
      {
		DBG_DATA( tags_list.current_item.value, tags_list.current_item.length );
        if (tags_list.current_item.length > 0 && tags_list.current_item.length <= 10)
        {
          char pan_5A[21], pan_track2[21];
		  DBG_POINT;
          bcd_to_asc(tags_list.current_item.value, pan_5A, tags_list.current_item.length * 2);

		  DBG_DATA( pan_5A, tags_list.current_item.length*2 );

          /* Figure out pan length */
          for (i=0; i<tags_list.current_item.length * 2; i++)
          {
            if (pan_5A[i] == '?')
              break;
          }
          pan_5A[i] = 0;
          card_get_pan(&transaction->card, pan_track2);
          /* compare pan, make sure it is the same */
          /*if (strcmp(pan_5A, pan_track2))*/
          //if (vim_strncmp(pan_5A, pan_track2, 21))
          //  VIM_ERROR_RETURN(VIM_ERROR_EMV_TRACK2_MISMATCH);

        }
      }
     
      /* Check expiry to see if it matches */
      if (vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE) == VIM_ERROR_NONE)
      {
		//VIM_DBG_MESSAGE("!!!!!!!!!!!!!!!!!!!!! FOUND EXPIRY DATE TAG !!!!!!!!!!!!!!!!!!!!!!!!!");
        if (tags_list.current_item.length == 3)
        {
          char track_2_mm_yy[5], yy_mm_5F24[5];

          card_get_expiry(&transaction->card, track_2_mm_yy);
#if 0
          if (check_emv_expiry_date(tags_list.current_item.value))
            VIM_ERROR_RETURN(ERR_CARD_EXPIRED);
#endif
          bcd_to_asc(&tags_list.current_item.value[0], &yy_mm_5F24[0], 2); /* yy */
          bcd_to_asc(&tags_list.current_item.value[1], &yy_mm_5F24[2], 2); /* mm */
          yy_mm_5F24[4] = 0;
          /* compare expiry, make sure it is the same */
		//VIM_DBG_MESSAGE("EXPIRY DATE TRACK2 EQUIV DATA: ( mmyy ) ");
		  //DBG_DATA( &track_2_mm_yy[0], 2 );
		  //DBG_DATA( &track_2_mm_yy[2], 2 );

		//VIM_DBG_MESSAGE("EXPIRY DATE TAG DATA: ( mmyy ) ");
		  //DBG_DATA( &yy_mm_5F24[2], 2 );
		  //DBG_DATA( &yy_mm_5F24[0], 2 );
          if( vim_mem_compare( &track_2_mm_yy[0], &yy_mm_5F24[2], 2) != VIM_ERROR_NONE ||
              vim_mem_compare( &track_2_mm_yy[2], &yy_mm_5F24[0], 2) != VIM_ERROR_NONE )
            VIM_ERROR_RETURN(ERR_CARD_EXPIRED_DATE);
        }
      }

      /* Got a track 2 and matched with expiry/pan tags. */
      return VIM_ERROR_NONE;
    }
  }

  /* If track 2 is not present construct it from the pan, expiry and optionally discretionary data tag */

  /* Find PAN tag */
  if ((vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_PAN) == VIM_ERROR_NONE) && (tags_list.current_item.length > 0) && (tags_list.current_item.length <= 10))
  {
    bcd_to_asc(tags_list.current_item.value, transaction->card.data.chip.track2, tags_list.current_item.length * 2);

    /* add pan */
    for (i=0; i < (tags_list.current_item.length * 2); i++)
    {
      if (transaction->card.data.chip.track2[i] == '?')
        break;
    }
    transaction->card.data.chip.track2[i] = 0;
    
  }
  else
    VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_ERROR);
  
  /*strncat(transaction->card.data.chip.track2, "=", VIM_SIZEOF(transaction->card.data.chip.track2));*/
  vim_strncat(transaction->card.data.chip.track2, "=", VIM_SIZEOF(transaction->card.data.chip.track2));
  
  /* Find expiry tag */
  if ((vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE) == VIM_ERROR_NONE)  && (tags_list.current_item.length == 3))
  {
    char expiry_yymm[5];
#if 0
    if (check_emv_expiry_date(tags_list.current_item.value))
      VIM_ERROR_RETURN(ERR_CARD_EXPIRED);
#endif
    bcd_to_asc(tags_list.current_item.value, expiry_yymm, 4);
    expiry_yymm[4] = 0;
    vim_strncat(transaction->card.data.chip.track2, expiry_yymm, VIM_SIZEOF(transaction->card.data.chip.track2));
  }
  else
    VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_ERROR);

  /* Add discretionary data tag if it is present... not compulsory though */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK2_DISCRETIONARY_DATA) == VIM_ERROR_NONE)
  {
    char discretionary_data[30];
    
    bcd_to_asc(tags_list.current_item.value, discretionary_data, MIN(tags_list.current_item.length*2, VIM_SIZEOF(discretionary_data)));
    for (i=0; i<MIN(tags_list.current_item.length*2, VIM_SIZEOF(discretionary_data)-1); i++)
    {
      if (discretionary_data[i] == '?')
        break;
    }
    discretionary_data[i] = 0;
    vim_strncat(transaction->card.data.chip.track2, discretionary_data, VIM_SIZEOF(transaction->card.data.chip.track2));
  }
  
  transaction->card.data.chip.track2_length = vim_strlen(transaction->card.data.chip.track2);
// //VIM_DBG_MESSAGE("CONSTRUCTING TRACK 2");
//VIM_DBG_STRING(transaction->card.data.chip.track2);

  return VIM_ERROR_NONE;
}
/**
 * Retrieve the track 1 data from the kernel.
 *
 */
#if 0
static VIM_ERROR_PTR emv_get_track_1(transaction_t *transaction)
{
  VIM_SIZE i;
  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[512];
  const VIM_TAG_ID tags[] = {
	  VIM_TAG_EMV_TRACK1_DISCRETIONARY_DATA
  };

  if( !transaction->emv_enabled )
  {
	  if( transaction->emv_data.data_size )
	  {	
		  // We're not running a current emv txn ( might be processing a SAF ) so try to pick up the AID from the txn list.	  
		  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< TRIED TO READ EMV TAGS WHEN NO ACTIVE EMV TXN >>>>>>>>>>>>>>>>>>>>>");		  
		  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tags_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE ));  
	  }
	  else
	  {
		  DBG_MESSAGE( "<<<<<<<<<<<<<<<<< TRIED TO READ EMV TAGS WHEN NO ACTIVE EMV TXN AND NO EXISITNG TXN TAGS >>>>>>>>>>>>>>>>>>>>>");		  
		  return VIM_ERROR_NOT_FOUND;
	  }
  }
  else
  {	
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create( &tags_list, tlv_buffer, VIM_SIZEOF(tlv_buffer)));	
	  VIM_ERROR_RETURN_ON_FAIL( vim_emv_read_kernel_tags( tags, VIM_SIZEOF(tags)/VIM_SIZEOF(tags[0]), &tags_list ) );
  }

  /* Initialize the length */
  transaction->card.data.chip.track1_length = 0;
  
  /* Add discretionary data tag if it is present... not compulsory though */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK1_DISCRETIONARY_DATA) == VIM_ERROR_NONE)
  {
    char discretionary_data[30];
    
    bcd_to_asc(tags_list.current_item.value, discretionary_data, MIN(tags_list.current_item.length*2, VIM_SIZEOF(discretionary_data)));
    for (i=0; i<MIN(tags_list.current_item.length*2, VIM_SIZEOF(discretionary_data)-1); i++)
    {
      if (discretionary_data[i] == '?')
        break;
    }
    discretionary_data[i] = 0;
    vim_strncat(transaction->card.data.chip.track1, discretionary_data, VIM_SIZEOF(transaction->card.data.chip.track1));
  }
  
  transaction->card.data.chip.track1_length = vim_strlen(transaction->card.data.chip.track1);
// //VIM_DBG_MESSAGE("CONSTRUCTING TRACK 1");
//VIM_DBG_STRING(transaction->card.data.chip.track1);

  return VIM_ERROR_NONE;
}
#endif

/* Check if account is asked by the card in PDOL */
VIM_BOOL emv_account_in_PDOL(void)
{
  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[256];
  VIM_UINT8 const *DOL_ptr;
  VIM_SIZE DOL_size;  
  
  const VIM_TAG_ID tags[] = 
  {
    VIM_TAG_EMV_PDOL
  };
  
  /* create a tlv list object */
  vim_tlv_create(&tags_list, tlv_buffer, VIM_SIZEOF(tlv_buffer));
  
  /* request the PDOL tag from the EMV kernel */
  if (VIM_ERROR_NONE != vim_emv_read_kernel_tags(tags, VIM_SIZEOF(tags)/VIM_SIZEOF(tags[0]), &tags_list))
    return VIM_FALSE;
  
  /* check if the PDOL is available */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_PDOL) != VIM_ERROR_NONE)
  {
	DBG_POINT;
    return VIM_FALSE;
  }
  
  DOL_ptr = tags_list.current_item.value;
  DOL_size = tags_list.current_item.length;  
  VIM_DBG_DATA( DOL_ptr, DOL_size );
  while (DOL_size>0)
  {
    VIM_SIZE tag_size;
    VIM_SIZE length_size;
    VIM_SIZE value_size;
      
    if (VIM_ERROR_NONE != vim_tlv_tag_get_size(DOL_ptr,DOL_size,&tag_size))
      return VIM_FALSE;
  
    if (VIM_ERROR_NONE == vim_mem_compare(DOL_ptr,VIM_TAG_EMV_ACCOUNT_TYPE,tag_size))
    {
		// RDD 140313 SPIRA:
      // //VIM_DBG_MESSAGE( "account type is in PDOL" );
	  // SPIRA:6646 If the PDOL doesn't require the account we may need to set it to something other than credit..
	  if( GetEPALAccount() == VIM_ERROR_NONE )       
		  return VIM_TRUE;
    }
        
    if (VIM_ERROR_NONE != vim_tlv_length_get(&value_size,&length_size,&DOL_ptr[tag_size],DOL_size-tag_size))
      return VIM_FALSE;
  
    DOL_size-=tag_size+length_size;
    DOL_ptr+=tag_size+length_size;
  }

  return VIM_FALSE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR emv_update_txn_data(void)
{
  DBG_MESSAGE( "!!!! UPDATING TAGS IN TXN.EMVDATA... !!!!" );
  VIM_ERROR_RETURN_ON_FAIL(emv_update_txn_tags(tags_to_update, VIM_LENGTH_OF(tags_to_update)));
   
  //VIM_ERROR_RETURN_ON_FAIL(emv_update_txn_tags(tags_to_print, VIM_LENGTH_OF(tags_to_print)));
  //VIM_ERROR_RETURN_ON_FAIL(emv_update_txn_tags(host_tags_auth, VIM_LENGTH_OF(host_tags_auth)));

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR emv_perform_transaction(void)
{
  VIM_ERROR_PTR ret;

  /*  init some data and call VIM EMV module */
  emv_attempt_to_go_online = VIM_FALSE;
  file41_emv_cashout_flag = VIM_FALSE;
  emv_predial_done = VIM_FALSE;
  local_online_result = VIM_EMV_ONLINE_RESULT_NO_RESULT;
  emv_blocked_application = VIM_FALSE;
  emv_fallbak_step_passed = VIM_FALSE;
  req_de55_length = 0x00;
  vim_mem_set( local_terminal_action_code_online, 0x00, VIM_SIZEOF(local_terminal_action_code_online));
  emv_transaction_error = VIM_ERROR_NONE;

  display_screen(DISP_PROCESSING_WAIT, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), " ", " ", "Processing"  );

  /* Reset the pin status flag to enable pin status display */
  skip_pin_status_display = VIM_FALSE;
   

  /* set EMV enabled flag */
  // RDD 180712 SPIRA:5775 PinPad Crashing if card inserted and removed v. quickly because this flag was set too early  
  txn.emv_enabled = VIM_TRUE;
  ret = vim_emv_perform_transaction();

  IsPosConnected( VIM_FALSE );

  if(( ret != VIM_ERROR_NONE ) && !transaction_get_status( &txn, st_partial_emv ))
  /* debug */
  {
    VIM_TEXT temp_buffer[512];
    VIM_SIZE debug_size = VIM_SIZEOF(temp_buffer);
    /* debug EMV kernel error */
    vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));

	VIM_DBG_ERROR(ret);

	// RDD 150213 Phase4 Sometime (eg if the card is removed during offline PinEntry the CB_error is not called
	// so we need to ensure that the PCEFTPOS interface has been restarted.

    if( vim_emv_get_debug (temp_buffer, &debug_size) == VIM_ERROR_NONE )
    {
      if( debug_size > 0 )
      {
         // RDD 180712 SPIRA:5775 PinPad Crashing if card inserted and removed v. quickly because this flag was set too early  
		 if( !vim_strncmp( temp_buffer, "NPO", 3 ) )
		 {
			// RDD - disable the emv_enabled flag so that power off is NOT performed if NPO status from debug
			 
			txn.emv_enabled = VIM_FALSE;
		 }
         pceft_debug (pceftpos_handle.instance, temp_buffer);
      }
    }
	  
	// RDD 180712 - attempt to fix case where bad chip init causes PinPad to hang up for 30+ seconds
	if(( ret == VIM_ERROR_EMV_FAULTY_CHIP )||( ret == VIM_ERROR_EMV_BAD_CARD_RESET ))
	{
		VIM_ERROR_PTR error;
		DBG_MESSAGE(" Faulty Chip - terminate card slot and exit ");
		pceft_debug( pceftpos_handle.instance, "Faulty chip: Call power off function" );
		if(( error = vim_icc_power_off (icc_handle.instance_ptr)) != VIM_ERROR_NONE )
		{
			VIM_DBG_ERROR( error );
		}

	    // RDD 240812 - don't actually re-inint the kernel here because very bad (eg. uwaved) cards wil cause problems if left in 
	    // the PinPad while initialisation is taking place...
	    //terminal_set_status(ss_wow_kernel_init_pending);
	    //InitEMVKernel();
	    txn.emv_enabled = VIM_FALSE;	// RDD 060812 - need to ensure that the terminate card slot is not called until the next txn.
	}
  }  
  return ret;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR emv_get_transaction_results(void)
{
  VIM_TLV_LIST tag_list;
  VIM_UINT8 emv_resp_code[2] = { 0, 0 };
  VIM_UINT8 cid;
  VIM_DBG_WARNING("emv_get_transaction_results");
  /*  Update information */
  emv_update_txn_data();
  DBG_POINT;
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE) );

  DBG_POINT;
  if( VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA) )
    vim_tlv_get_bytes(&tag_list, &cid, 1);
  else
    /*  transaction aborted somehow */
    VIM_ERROR_RETURN(VIM_ERROR_EMV_TXN_ABORTED);

  DBG_POINT;
  if( VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_RESPONSE_CODE) )
  {
    vim_tlv_get_bytes(&tag_list, emv_resp_code, 2);
  }

  VIM_DBG_DATA(emv_resp_code, 2);
 
  if( !emv_attempt_to_go_online )
  { /*  decision was made after 1-st GenAC */
    vim_mem_copy(txn.host_rc_asc, emv_resp_code, 2);
    
  DBG_POINT;
    // RDD 200814 why do this here ???? I'll do it after sucessfully storing a pre-auth
    //terminal_increment_roc();
    
    if( EMV_CID_TC(cid) )
    { /*  Approved offline after 1-st GenAC */
      txn.error = VIM_ERROR_NONE;
      vim_mem_copy(txn.host_rc_asc, "Y1", 2);
      vim_strcpy(txn.cnp_error, "Y1");


	  DBG_MESSAGE("<<<<<<<<<<< CARD APPROVED TXN OFFLINE Y1 >>>>>>>>>>>>");
      
      if (tt_preauth == txn.type)
        transaction_set_status(&txn, st_offline);
      else
        transaction_set_status(&txn, st_offline + st_needs_sending);
      txn.saf_reason = saf_reason_offline_emv_purchase;
    }
    else
    { /*  Declined offline after 1-st GenAC */
      txn.error = ERR_EMV_CARD_DECLINED;
      vim_mem_copy(txn.host_rc_asc, "Z1", 2);     
	  DBG_MESSAGE("ADDING Z1 8A TAG TO LIST");
	  
	  // RDD 090312: SPIRA 5017: Need to add 8A to DE55 for Z1 advice so customer won't be debited
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(&tag_list, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2));

#if 0
      if( cid & EMV_ADVICE_REQUIRED )
#else
      if( txn.type != tt_refund )
#endif
      { /*  Advice required */
        DBG_POINT;
	    transaction_set_status(&txn, st_offline);
        transaction_set_status(&txn, st_needs_sending);
        txn.saf_reason = saf_reason_no_advice; /*  do not send advice indicator */
      }
      /*  save for printing */
      //EMV_OfflineDeclined_Add();
    }
  }
  else
  { /*  decision was made after 2-nd GenAC */
	    DBG_POINT;
    if( EMV_CID_TC(cid) )
    { 
  DBG_POINT;
      if( local_online_result == VIM_EMV_ONLINE_RESULT_UNABLE_TO_GO_ONLINE )
      { /*  Approved offline (unable to go online) */
#if 0
        vim_mem_copy(txn.host_rc_asc, emv_resp_code, 2);
#else
	    DBG_MESSAGE("<<<<<<<<<<< CARD APPROVED TXN OFFLINE Y3 >>>>>>>>>>>>");

        vim_mem_copy(txn.host_rc_asc, "Y3", 2);
        vim_strcpy(txn.cnp_error, "Y3");

#endif
        transaction_set_status(&txn, st_offline + st_needs_sending);
        txn.error = VIM_ERROR_NONE;
        txn.saf_reason = saf_reason_offline_emv_purchase;

        // RDD 250619 EFB has already been done so must reverse the sig required due to JIRA WWBP-652
        vim_mem_copy( txn.host_rc_asc, "00", 2 );
        VIM_DBG_MESSAGE("Get Rid of Signature");
				
		// RDD 140616 SPIRA:8992 v560-2 update the Tag for the 420 with Z3 as we were unable to go online							
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( &tag_list, VIM_TAG_EMV_RESPONSE_CODE, "Y3", 2 ));
		// RDD 140616 - Set a reversal without repeat - this ensures that the new EMV buffer is saved to the reversal file

		DBG_MESSAGE( "Set Reversal" );
		reversal_set( &txn, VIM_FALSE);

		// RDD 090512 SPIRA:5470 If the comms fails and the is EFB approved then 
		// the EFB approval code clears SVP but this needs to be restored if Y3 approved
		if( qualify_svp( ) )
		{
			DBG_MESSAGE("<<<<<<<<<<< RESTORE SVP FLAG >>>>>>>>>>>>");
			txn.qualify_svp = VIM_TRUE;				
		}
      }
      else
      { /*  Approved online */
		  DBG_POINT;        
		  txn.error = VIM_ERROR_NONE;
      }
    }
    else
    {
  DBG_POINT;
      if( local_online_result == VIM_EMV_ONLINE_RESULT_UNABLE_TO_GO_ONLINE )
      { /*  Declined offline (unable to go online) */
#if 0
        vim_mem_copy(txn.host_rc_asc, emv_resp_code, 2);
#else
		// RDD 160312 - Don't update the Response Code printed on the receipt as this is set in EFB Processing
        //vim_mem_copy(txn.host_rc_asc, "Z3", 2);
#endif

#if 0	// RDD 270112 - only upload Z3 transaction as advices if they pass EFB processing rules - in this case *08 is printed on the receipt
        if( cid & EMV_ADVICE_REQUIRED )	// RDD 270112 - upload Z3 only if dictated by the CID. WOW only interested in the Z1 (which are trapped by the switch)
        {
          transaction_set_status(&txn, st_offline + st_needs_sending);
          txn.saf_reason = saf_reason_no_advice; /*  do not send advice indicator */
        }
#endif
        //VIM_DBG_ERROR(txn.error);

#if 0	// RDD - 270112: We need to do EFB checking 
        /* check if EFB can be processed */
        if (is_comms_error(txn.error))
        {
          txn.error = ERR_UNABLE_TO_GO_ONLINE_DECLINED;
          efb_process_txn();
        }
        else
          txn.error = ERR_UNABLE_TO_GO_ONLINE_DECLINED;
#endif
        /*  save for printing */
  
		DBG_POINT;
  		  
		VIM_DBG_WARNING( "Update taglist with Z3" );
		// RDD 140616 SPIRA:8992 v560-2 update the Tag for the 420 with Z3 as we were unable to go online							
		// RDD SPIRA:9217 Update Z3
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_update_bytes( &tag_list, VIM_TAG_EMV_RESPONSE_CODE, "Z3", 2 ));
		// RDD 140616 - Set a reversal without repeat - this ensures that the new EMV buffer is saved to the reversal file

		// SPIRA:9080 don't want reversal if we got a 91

		if( vim_mem_compare( txn.host_rc_asc_backup, "91", 2) != VIM_ERROR_NONE )
		{
			DBG_MESSAGE( "!!!! SET REVERSAL");
			reversal_set( &txn, VIM_FALSE);
		}

		//EMV_OfflineDeclined_Add();
      }
      else
      { /*  Declined after online processing */
        if( local_online_result == VIM_EMV_ONLINE_RESULT_APPROVED )
        {
          /* host approved, card declined */
#if 0	 // RDD 230112 - Z4 was writing over Y1 created by card after 00 from HOST
            vim_mem_copy(txn.host_rc_asc, emv_resp_code, 2);
			if( TxnWasApproved( txn.host_rc_asc ) )
			{
				DBG_VAR(txn.host_rc_asc);
				DBG_VAR(cid);
			}
			else
			{			
		        vim_mem_copy(txn.host_rc_asc, "Z4", 2);
				txn.error = ERR_HOST_APPROVED_CARD_DECLINED;
			    EMV_OfflineDeclined_Add();
			}
#else
  DBG_POINT;
            vim_mem_copy(txn.host_rc_asc, emv_resp_code, 2);		          
			vim_mem_copy(txn.host_rc_asc, "Z4", 2);	

			// RDD - update the Tag for the 420				
			VIM_ERROR_RETURN_ON_FAIL(vim_tlv_update_bytes(&tag_list, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2));

  DBG_POINT;

			txn.error = ERR_HOST_APPROVED_CARD_DECLINED;
          /*  save for printing */
  DBG_POINT;
			EMV_OfflineDeclined_Add();
#endif
          
        }
        else
        {
          txn.error = ERR_WOW_ERC;
		  VIM_DBG_ERROR( txn.error );
#if 0	// RDD 270112: EFB processing needs to be done immediatly after receipt of the response code but before the 2nd GEN AC
          if (VIM_ERROR_NONE == vim_mem_compare(txn.host_rc_asc, "91", 2))
            efb_process_txn();
#endif
        }
      }
    }      
  }

  VIM_DBG_ERROR( txn.error );
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_finalisation( VIM_ERROR_PTR error )
{
  #if 0
  VIM_PCEFTPOS_RECEIPT receipt;
  VIM_CHAR out_string[30];
  VIM_PCEFTPOS_KEYMASK key_code;
  VIM_ERROR_PTR key_err;
  #endif
VIM_DBG_MESSAGE("vim_emv_CB_finalisation");
VIM_DBG_ERROR(error);
  emv_transaction_error = error;
  /* PIN Status displayed with 3 seconds delay, however can be removed after review */
  vim_pin_status_display(PIN_STATUS_FINALIZATION);
  
  if( local_online_result == VIM_EMV_ONLINE_RESULT_APPROVED && emv_transaction_error != VIM_ERROR_NONE )
  { /*  inform host that transaction was unsuccessful for any reason */
    txn.saf_reason = saf_reason_no_advice;
    /* print receipt */
    txn.print_receipt = VIM_TRUE;
		  			  

	// RDD 160713 SPIRA:6773 Reversal Response was not waited for after this failure for offline PIN cards because this flag had not been cleared 
	// which in turn caused the command check to exit with VIM_ERROR_NONE, which in turn caused the reversal to be cleared.
	transaction_clear_status( &txn, st_offline_pin_verified );

	DBG_MESSAGE( "!!!! SET REVERSAL !!!!");
	DBG_VAR( terminal.stan );
    reversal_set( &txn, VIM_FALSE);
	DBG_VAR( terminal.stan );

	// RDD 240712 SPIRA:5790 Pulled card during second gen ac resulted in reversal generated but transaction spproved because error was not actually logged correctly
	txn.error = emv_transaction_error;
  }

#if 1
  if (get_emv_query_txn_flag() &&
      (VIM_ERROR_NONE != emv_transaction_error))
  {
    if ((VIM_ERROR_EMV_PARTIAL_TXN == emv_transaction_error) &&
        ((VIM_ERROR_NONE == vim_mem_compare(txn.host_rc_asc, "00", 2)) ||
         (VIM_ERROR_NONE == vim_mem_compare(txn.host_rc_asc, "08", 2))))
    {
     //VIM_ 
      emv_transaction_error = VIM_ERROR_NONE;
      txn.error = emv_transaction_error;
    }

    if (VIM_ERROR_EMV_PARTIAL_TXN != emv_transaction_error)
      txn.error = emv_transaction_error;
    
    return txn.error;
  }
#endif
  /* skip any processing in case of error */
  VIM_ERROR_RETURN_ON_FAIL(emv_transaction_error);

  /*  Get result of EMV transaction */
  // RDD 010317 This is where the txn.emv_data gets updated and the buffer len added to if there are script results added
  emv_transaction_error = emv_get_transaction_results();

  /* print receipt */
  txn.print_receipt = VIM_TRUE;

  if( local_online_result == VIM_EMV_ONLINE_RESULT_APPROVED && (txn.error != VIM_ERROR_NONE || emv_transaction_error != VIM_ERROR_NONE) )
  { /*  Again, inform host that transaction was declined for any reason */
    txn.saf_reason = saf_reason_no_advice;
		  
    DBG_MESSAGE( "!!!! SET REVERSAL !!!!");
	DBG_VAR( terminal.stan );
	// Note that the reversal itself needs to be updated at this point with the latest EMV data buffer
    reversal_set( &txn, VIM_FALSE);
	DBG_VAR( terminal.stan );
  }
  
  return VIM_ERROR_NONE;
}




/**
 * Get a dummy pan for the AID read from transaction_t 
 */
const VIM_UINT8 *pre_swipe_get_dummy_pan( transaction_t *transaction )
{
  VIM_EMV_AID aid;
  VIM_TLV_LIST tag_list;
  VIM_UINT8 tlv_buffer[50];
  static const char *dummy_pan_visa =        "4000000000000002";
  static const char *dummy_pan_mastercard =  "5100000000000008";
  static const char *dummy_pan_amex =        "370000000000002";
  static const char *dummy_pan_default =     "0000000000000000";

  const VIM_TAG_ID tags[] = 
  {
    VIM_TAG_EMV_AID_ICC,
  };

  if( vim_tlv_create(&tag_list, tlv_buffer, VIM_SIZEOF(tlv_buffer)) == VIM_ERROR_NONE )
  {
    if ( vim_emv_read_kernel_tags(tags, 1, &tag_list) == VIM_ERROR_NONE )
    {
      if( vim_tlv_search( &tag_list, VIM_TAG_EMV_AID_ICC) == VIM_ERROR_NONE )
      {
        aid.aid_size = tag_list.current_item.length;
        if( VIM_ERROR_NONE == vim_tlv_get_bytes( &tag_list, aid.aid, VIM_SIZEOF(aid.aid)))
        {
          if (aid.aid_size >= 5)
          {
            if (VIM_ERROR_NONE == vim_mem_compare(aid.aid, VIM_EMV_RID_VISA, 5))
            {
              return (VIM_UINT8 *) dummy_pan_visa;
            }
            else if (VIM_ERROR_NONE == vim_mem_compare(aid.aid, VIM_EMV_RID_MASTERCARD, 5))
            {
              return (VIM_UINT8 *) dummy_pan_mastercard;
            }
            else if (VIM_ERROR_NONE == vim_mem_compare(aid.aid, VIM_EMV_RID_AMEX, 5))
            {
              return (VIM_UINT8 *) dummy_pan_amex;
            }
          }
        }
      }
    }
  }
  return (VIM_UINT8 *) dummy_pan_default;
}

VIM_BOOL emv_cashout_allowed()
{
//VIM_DBG_VAR(file41_emv_cashout_flag);
  return file41_emv_cashout_flag;
}


/*----------------------------------------------------------------------------*/
VIM_BOOL emv_is_fallback_allowed()
{
  VIM_TLV_LIST tags;
  
  if( emv_blocked_application )
    return VIM_FALSE;
  
  if( emv_fallbak_step_passed )
    /*  "no-return" step passed */
    return VIM_FALSE;
  
  if (vim_tlv_open(&tags, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer),VIM_FALSE) == VIM_ERROR_NONE)
  {
    if (vim_tlv_search(&tags, VIM_TAG_EMV_AID_ICC) == VIM_ERROR_NONE)
    { /*  transaction terminated before Application Selected */
      return VIM_FALSE;
    }
  }

  /*  All conditions are met */
  return VIM_TRUE;
}
/*----------------------------------------------------------------------------*/
#define OFFLDECL_TAG_DE55              (VIM_UINT8 *)"\xC0"
#define TAG_AID_TERMINAL					0x9F06
/* static const char *offldeclpath = "/"DISK_VOLUME_NAME"/reversal.dat"; */
/* static const char *offldecldisk = "/"DISK_VOLUME_NAME"/";*/
/* static const char *offldecldir = "/"DISK_VOLUME_NAME"/offl_decl_*";*/
/* #define OFFL_DECL_FILENAME_MASK      "offl_decl_"*/

const VIM_TAG_ID offldecl_tags[] = 
{
  VIM_TAG_EMV_APPLICATION_PAN,
  VIM_TAG_EMV_APPLICATION_PAN_SEQUENCE_NUMBER,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DEFAULT,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL,
  VIM_TAG_EMV_ISSUER_ACTION_CODE_ONLINE,
};

/*----------------------------------------------------------------------------*/
/*  Store current transaction as "offline declined" for future printing */
static VIM_ERROR_PTR EMV_OfflineDeclined_Add(void)
{
#if 0
  VIM_TLV_LIST tags_txn;
  VIM_TLV_LIST tags_out;
  VIM_TLV_LIST tags_file;
  VIM_UINT8 buffer[512];
  VIM_UINT8 file_buffer[512];
  int i;
  VIM_EMV_AID aid;
  VIM_UINT8 index;
  VIM_UINT8 aid_for_conv[16];
  VIM_UINT8 fullfilename[3*FS_FILENAMESIZE+1];
  S_FS_DIR *p_dir;
  S_FS_FILEINFO dirdata;
  VIM_INT32 res;

 //VIM_DBG_MESSAGE("Add as offline declined");

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open(&tags_txn, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create(&tags_out, buffer, VIM_SIZEOF(buffer)));

  /*  Add terminal AID */
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search(&tags_txn, VIM_TAG_EMV_AID_ICC) );
  aid.aid_size = tags_txn.current_item.length;
  if( tags_txn.current_item.length )
    vim_mem_copy( aid.aid, tags_txn.current_item.value, tags_txn.current_item.length);

  VIM_ERROR_RETURN_ON_FAIL( emv_get_app_id(&aid, &record) );
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags_out, VIM_TAG_EMV_AID_TERMINAL, record.appid.data.aid.aid_value, record.appid.data.aid.aid_length ));
  vim_mem_set( aid_for_conv, 0xFF, VIM_SIZEOF(aid_for_conv) );
  vim_mem_copy( aid_for_conv, record.appid.data.aid.aid_value, MIN(record.appid.data.aid.aid_length, VIM_SIZEOF(aid_for_conv)));

  /*  Copy necessary transaction tags */
  for( i=0; i<VIM_SIZEOF(offldecl_tags)/VIM_SIZEOF(offldecl_tags[0]); i++ )
    if( vim_tlv_search(&tags_txn, offldecl_tags[i] ) == VIM_ERROR_NONE )
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags_out, offldecl_tags[i], tags_txn.current_item.value, tags_txn.current_item.length ));

  /*  Add de55 as separate tag */
  if( req_de55_length )
    VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags_out, OFFLDECL_TAG_DE55, req_de55_buffer, req_de55_length ));

  /*  check if this AID has file */
  p_dir = FS_opendir( (char*)offldecldir );
  while (FS_readdir (p_dir, &dirdata) == FS_OK )
  {
    vim_strncpy(fullfilename, offldecldisk, VIM_SIZEOF(fullfilename));
    vim_strncat(fullfilename, dirdata.FileName, VIM_SIZEOF(fullfilename));
    
    res = file_read(fullfilename, file_buffer, VIM_SIZEOF(file_buffer));
    if( res < 0 )
      VIM_ERROR_RETURN(VIM_ERROR_OS);

    if( vim_tlv_open(&tags_file, file_buffer, res, VIM_SIZEOF(file_buffer), VIM_FALSE) == VIM_ERROR_NONE )
    {
      if( vim_tlv_search(&tags_file, VIM_TAG_EMV_AID_TERMINAL) == VIM_ERROR_NONE )
      {
        if( vim_mem_compare( record.appid.data.aid.aid_value, tags_file.current_item.value, tags_file.current_item.length ) == VIM_ERROR_NONE )
        { /*  exists, delete it */
//VIM_ 
          FS_unlink( fullfilename );
          break;
        }
      }
    }
  }
  FS_closedir(p_dir);

  /*  get "free" filename */
  for( index = 0x00; index<0xFF; index++ )
  {
    vim_strncpy(fullfilename, offldecldir, VIM_SIZEOF(fullfilename));
    hex_to_asc(&index, fullfilename+vim_strlen(fullfilename)-1, 2);
    if( FS_exist( fullfilename ) == FS_KO )
      break;
  }  

  if( index == 0xFF )
    VIM_ERROR_RETURN(VIM_ERROR_OS);

  /*  Save file */
  file_write(fullfilename, buffer, tags_out.total_length);
#endif
  return VIM_ERROR_NONE;
}

#define MAX_OFFLINE_DELETED 20
#define EMV_OFFL_DECL_LIST_TITLE "PRINT OFFLINE DCLN"

VIM_ERROR_PTR print_offline_decline( void )
{
#if 0
  VIM_UINT32 i;
  table_record_t record;
  table_record_t icc_record;
  S_FS_DIR *p_dir;
  S_FS_FILEINFO dirdata;
  VIM_INT32 res;
  VIM_UINT8 file_buffer[512];
  offl_item_t offline_deleted_items[MAX_OFFLINE_DELETED];
  VIM_UINT8 index;
  VIM_TLV_LIST tags;
  VIM_EMV_AID aid;
  VIM_UINT8 fullfilename[3*FS_FILENAMESIZE+1];
  char *items[MAX_OFFLINE_DELETED];
  VIM_PCEFTPOS_RECEIPT receipt;
  
  /*  Build list of declined transactions */
  index = 0;
  vim_mem_set( offline_deleted_items, 0x00, VIM_SIZEOF(offline_deleted_items) );
  p_dir = FS_opendir( (char*)offldecldir );
  while( FS_readdir (p_dir, &dirdata) == FS_OK && index<MAX_OFFLINE_DELETED )
  {
    vim_strncpy(fullfilename, offldecldisk, VIM_SIZEOF(fullfilename));
    vim_strncat(fullfilename, dirdata.FileName, VIM_SIZEOF(fullfilename));
    
    res = file_read(fullfilename, file_buffer, VIM_SIZEOF(file_buffer));
    if( res < 0 )
      return res;

    if( vim_tlv_open(&tags, file_buffer, res, VIM_SIZEOF(file_buffer), VIM_FALSE) == VIM_ERROR_NONE )
    {
      if( vim_tlv_search(&tags, VIM_TAG_EMV_AID_TERMINAL) == VIM_ERROR_NONE )
      { /*  search file 41 */
        aid.aid_size = tags.current_item.length;
        if( tags.current_item.length )
          vim_mem_copy( aid.aid, tags.current_item.value, tags.current_item.length);

        if( emv_get_app_id(&aid, &record) == VIM_ERROR_NONE )
        { /*  record found. add to list */
          vim_mem_copy(offline_deleted_items[index].filename, fullfilename, MIN(VIM_SIZEOF(fullfilename),VIM_SIZEOF(offline_deleted_items[index].filename)));
          vim_mem_copy(offline_deleted_items[index].aid_label, record.appid.data.aid.app_label, MIN(VIM_SIZEOF(record.appid.data.aid.app_label),VIM_SIZEOF(offline_deleted_items[index].aid_label)));
          if( emv_get_icc_table_record(&aid, &icc_record) == VIM_ERROR_NONE )
          { 
            vim_mem_copy(offline_deleted_items[index].tac_denial, record.icc.detail.data.tac_denial, 5);
            vim_mem_copy(offline_deleted_items[index].tac_online, record.icc.detail.data.tac_online, 5);
            vim_mem_copy(offline_deleted_items[index].tac_default, record.icc.detail.data.tac_default, 5);
          }
VIM_DBG_VAR(fullfilename);
VIM_DBG_VAR(record.appid.data.aid.app_label);
          index++;
        }
        else
        { /*  record not found. dormant file */
          FS_unlink( fullfilename );
        }
      }
    }
  }
  FS_closedir(p_dir);

  if( index == 0 )
  {
 /*    pceft_pos_display("NO DECLINED OFFLINE", " ", VIM_PCEFTPOS_KEY_OK, 1); */
  /*   pceft_pos_wait_key(60); */
    return VIM_ERROR_NONE;
  }

  for(i = 0; i < index; i++)
    items[i] = offline_deleted_items[i].aid_label;

  res = display_list_select(EMV_OFFL_DECL_LIST_TITLE, 0, items, index, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE);
    
  if (res < 0)
    return VIM_ERROR_USER_CANCEL;

  /*  selescted. read data drom file */
  index = res;
  res = file_read(offline_deleted_items[index].filename, file_buffer, VIM_SIZEOF(file_buffer));
  if( res < 0 )
    return res;

  receipt_offline_decline( &offline_deleted_items[index], file_buffer, res, &receipt);

  receipt_print(&receipt, VIM_TRUE);
#endif
  return VIM_ERROR_NONE;
}

VIM_BOOL emv_requires_signature( transaction_t *transaction )
{
  VIM_TLV_LIST tags;

  /* check CVM results */
  if ((transaction->card.type == card_chip) && (!transaction_get_status(transaction, st_partial_emv)))
  {
    if (vim_tlv_open(&tags, transaction->emv_data.buffer, transaction->emv_data.data_size, VIM_SIZEOF(transaction->emv_data.buffer),VIM_FALSE) == VIM_ERROR_NONE)
    {
      if ((vim_tlv_search(&tags, VIM_TAG_EMV_CVM_RESULTS) == VIM_ERROR_NONE) && (tags.current_item.length >= 3))
      {
       //VIM_DBG_VAR(tags.current_item.value[0]);
        switch (tags.current_item.value[0] & VIM_EMV_CVM_MASK)
        {
          case VIM_EMV_CVM_OFFLINE_ENC_PIN_AND_SIGNATURE:
          case VIM_EMV_CVM_OFFLINE_PLAIN_PIN_AND_SIGNATURE:
          case VIM_EMV_CVM_SIGNATURE:
            return VIM_TRUE;
        }
      }
    }
  }
  return VIM_FALSE;
}

/* Check the offline pin related CVM results */
VIM_BOOL emv_pin_status( transaction_t *transaction )
{
  VIM_TLV_LIST tags;

  VIM_TLV_LIST tag_list ;
  
  DBG_MESSAGE( " emv_cb emv_pin_status " );
  if( emv_attempt_to_go_online == VIM_FALSE )
  {
	  emv_read_tags_from_kernel( terminal.emv_applicationIDs[transaction->aid_index].emv_aid );
  }
  
  vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE);

  //VIM_DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);

  /* check CVM results */
  DBG_VAR( transaction->status );
  if ((transaction->card.type == card_chip) && (!transaction_get_status(transaction, st_partial_emv)))
  {
	  DBG_POINT;
    if (vim_tlv_open(&tags, transaction->emv_data.buffer, transaction->emv_data.data_size, VIM_SIZEOF(transaction->emv_data.buffer),VIM_FALSE) == VIM_ERROR_NONE)
    {
	  DBG_POINT;
      if ((vim_tlv_search(&tags, VIM_TAG_EMV_CVM_RESULTS) == VIM_ERROR_NONE) && (tags.current_item.length >= 3))
      {
        VIM_DBG_DATA(tags.current_item.value, 3);

	  DBG_POINT;
        switch (tags.current_item.value[0] & VIM_EMV_CVM_MASK)
        {
          case VIM_EMV_CVM_OFFLINE_ENCRYPTED_PIN:
	  DBG_POINT;
          case VIM_EMV_CVM_OFFLINE_PLAINTEXT_PIN:
	  DBG_POINT;
            // RDD 0 = unknown, 1 = failed, 2 = success
            if (tags.current_item.value[2] == 0x02)
			{
					  DBG_POINT;
				return VIM_TRUE;
			}

          case VIM_EMV_CVM_OFFLINE_ENC_PIN_AND_SIGNATURE:
          case VIM_EMV_CVM_OFFLINE_PLAIN_PIN_AND_SIGNATURE:
		  	//VIM_DBG_VAR(tags.current_item.value[2]);
            if (tags.current_item.value[2] == 0x00)
				return VIM_TRUE;

		  default: break;
        }
      }
    }
  }
  return VIM_FALSE;
}


typedef struct move_rec
{
  /* Name of the tag */
	/*@null@*/VIM_CHAR *tag_name;
  /* Move routine */
	/*@null@*/void (*tag_move)(VIM_UINT8 *);
  /* Length of the tag */
  VIM_UINT8 tag_length;
}move_rec;

void move_tcc(VIM_UINT8 *value)
{
  /* store tcc */
  terminal.tcc = *value;
}

void move_mcc(VIM_UINT8 *value)
{
  /* store mcc */
  asc_to_bcd((char *) value, terminal.mcc, 4);
}

void revoke_capk(VIM_UINT8 *key_info)
{
  VIM_UINT8 rid[5], key_index;
  emv_capk_parameters_t *pcapk;
  
  /* convert RID from ascii into hex */
  asc_to_hex((char *) key_info, rid, 10);

  /* convert key index from ascii into hex */
  asc_to_hex((char *) &key_info[10], &key_index, 2);

  for (pcapk = terminal.emv_capk; pcapk < &terminal.emv_capk[MAX_EMV_PUBLIC_KEYS]; pcapk++)
  {
    
    /* no keys anymore */
    if (0 == pcapk->capk.RID.value[0])
      break;

    if ((pcapk->capk.index == key_index) &&
      (VIM_ERROR_NONE == vim_mem_compare(rid, pcapk->capk.RID.value, VIM_SIZEOF(pcapk->capk.RID.value))))
    {
      /* overwrite the rest of the keys */
      while (pcapk < &terminal.emv_capk[MAX_EMV_PUBLIC_KEYS - 1])
      {
        vim_mem_copy(pcapk, pcapk + 1, VIM_SIZEOF(emv_capk_parameters_t));
        pcapk++;
      }

      /* clear 'previous' last key */
      vim_mem_set(pcapk, 0, VIM_SIZEOF(emv_capk_parameters_t));
    }
  }
}


void move_enc_flag(VIM_UINT8 *value)
{
  /* update encryption enable flag */
  terminal.message_encryption_level = *value & 0x0F;
}

const struct move_rec tags_table[] =
{
  {"WTC",     move_tcc,       1},
  {"WMC",     move_mcc,       4},
  {"WKR",     revoke_capk,    12},
  {"WME",     move_enc_flag,  1},
  {VIM_NULL,  VIM_NULL,       0}
};

void emv_data_update(VIM_UINT8 *host_data)
{
 
  VIM_UINT8 field_47_length, *pdata;
  struct move_rec *ptags;
  
  /* length of the data */
  field_47_length = bcd_to_bin(host_data, 4);

  /* skip the length, point to the data */
  pdata = host_data + 2;
  while (pdata < host_data + field_47_length + 2)
  {
    for (ptags = (struct move_rec *) tags_table; ; ptags++)
    {
      if (VIM_NULL == ptags)
        /* unknown tag */
        return;
      
      if (VIM_ERROR_NONE == vim_mem_compare(pdata, ptags->tag_name, vim_strlen(ptags->tag_name)))
      {
        /* skip tag name */
        pdata += vim_strlen(ptags->tag_name);

        /* call move routine */
        ptags->tag_move(pdata);

        /* move pointer */
        pdata += ptags->tag_length;

        /* skip '\' */
        pdata++;
        break;
      }
    }
  }
}

/*----------------------------------------------------------------------------*/
/**
 * Get a number of AID supported by terminal
 */
/*----------------------------------------------------------------------------*/
VIM_SIZE get_aid_count(VIM_BOOL contactless)
{
  VIM_SIZE counter, i;

  for (
        counter = i = 0;
        (terminal.emv_applicationIDs[i].emv_aid[0]) &&
        (i < MAX_EMV_APPLICATION_ID_SIZE);
        i++
      )
  {
    /* increment counter if we count contactless applications only or contact applications only */
    if ((contactless && terminal.emv_applicationIDs[i].contactless) ||
        (!contactless && !terminal.emv_applicationIDs[i].contactless))
      counter++;
  }

  return counter;
}

/*----------------------------------------------------------------------------*/
/**
 * Get a number of CAPK supported by terminal
 */
/*----------------------------------------------------------------------------*/
VIM_SIZE get_capk_count(void)
{
  VIM_SIZE counter = 0;
  
  while ((terminal.emv_capk[counter].capk.RID.value[0]) && (counter < MAX_EMV_PUBLIC_KEYS ))
    counter++;

  return counter;
}

#if 1
VIM_TLV_TAG_PTR* get_picc_tags(VIM_SIZE *tags_cnt)
{
#if 1
	static VIM_TAG_ID combined_tags[VIM_SIZEOF(host_tags_auth)/VIM_SIZEOF(host_tags_auth[0]) + VIM_SIZEOF(tags_to_print)/VIM_SIZEOF(tags_to_print[0])];
#else
	static VIM_TAG_ID combined_tags[500];
#endif
  VIM_SIZE i, j;
  int tag_len;
  VIM_SIZE combined_tags_cnt = 0;

  VIM_DBG_NUMBER(VIM_SIZEOF(combined_tags));

  // RDD 231013 added v419a
  vim_mem_clear( (void *)combined_tags, VIM_SIZEOF( combined_tags ) );

  DBG_MESSAGE( "RRRRRRRRRRRRRRRRRRRRRRRRRRRRRR READING PICC TAGS RRRRRRRRRRRRRRRRRRRRRRRRRRRRRR");
  if (VIM_ERROR_NONE == emv_get_host_tags(combined_tags, &combined_tags_cnt, &txn ))
  {
    /*  Add tags for printing */
    for (i = 0; i < VIM_SIZEOF(tags_to_print) / VIM_SIZEOF(tags_to_print[0]); i++)
    {
      tag_len = ((*tags_to_print[i] & 0x1F) == 0x1F) ? 2 : 1;
	  // We have host tags list now - check for duplication
      for (j = 0; j < combined_tags_cnt; j++)
      {
        if (!vim_mem_compare(tags_to_print[i], combined_tags[j], tag_len))
          break;
      }

      if (j == combined_tags_cnt)
        /* Tag not in the list yet. Add it */
        combined_tags[combined_tags_cnt++] = tags_to_print[i];
    }
  }

  *tags_cnt = combined_tags_cnt;

  return (VIM_TLV_TAG_PTR *) (&combined_tags);
}
#endif

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_emv_CB_get_last_txnAmt(VIM_UINT32 * txn_amount_ptr)
{
  // //VIM_DBG_MESSAGE("vim_emv_CB_get_last_txnAmt");

  *txn_amount_ptr = 0;

  /* TODO: get total offline transasction amount for this current card  */
  
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR vim_emv_CB_get_SECPIN_options
(
    /**[out] */
    VIM_EMV_SECPIN_ENTRY_OPTIONS* secPIN_options_ptr,
    /**[in] */
    VIM_EMV_AID *aid,
    /**[in] */
    VIM_UINT8 cvm_method
)
{
  VIM_TCU_PIN_ENTRY_PARAMETERS param;
  txn_pin_entry_type pin_entry_type = txn_pin_entry_mandatory;

  DBG_MESSAGE( "----------- Setting up offline Pin entry options ------------");

  vim_mem_clear( secPIN_options_ptr, VIM_SIZEOF(VIM_EMV_SECPIN_ENTRY_OPTIONS) );
  vim_mem_clear( &param, VIM_SIZEOF(VIM_TCU_PIN_ENTRY_PARAMETERS) );
  
  secPIN_options_ptr->default_fill_char = ' ';
  secPIN_options_ptr->echo_char = '*';			//*param.mask_ptr;
#if 0
  if( txn.emv_pin_entry_params.last_pin_try)
  {
	  DBG_POINT;
	  secPIN_options_ptr->pin_display_line = 235;
  }
  else
#endif
	  secPIN_options_ptr->pin_display_line = 225;

  secPIN_options_ptr->pin_display_col = 100;
  secPIN_options_ptr->pin_min_number = 4;		//param.min_size;
  secPIN_options_ptr->pin_max_number = 12;		//param.max_size;

  // RDD 22052013 SPIRA:6736 - set this flag only when Offline Pin Entry is actually called by the kernel. - remove line below
  // txn.emv_pin_entry_params.offline_pin = VIM_TRUE;  
  txn.emv_pin_entry_params.max_digits = secPIN_options_ptr->pin_max_number;
  txn.emv_pin_entry_params.min_digits = secPIN_options_ptr->pin_min_number;
	 
  // RDD 311211 - Setup the Entry type according to the rules  
  DBG_VAR(txn.emv_data.data_size);

  DBG_VAR( pin_entry_type );
  SetPinEntryType( &pin_entry_type );
  DBG_VAR( pin_entry_type );
  SetupPinEntryParams( &param, &pin_entry_type );
  DBG_VAR( pin_entry_type );

  DBG_VAR(txn.emv_data.data_size);

  DBG_VAR( param.min_size );
  DBG_VAR( param.max_size );

  secPIN_options_ptr->total_timeout = param.timeout;
  secPIN_options_ptr->first_key_timeout = param.timeout;
  secPIN_options_ptr->inter_key_timeout = param.timeout;


  secPIN_options_ptr->pin_bypass = ( pin_entry_type == txn_pin_entry_mandatory ) ? VIM_FALSE : VIM_TRUE;
  DBG_VAR( secPIN_options_ptr->pin_bypass );
  DBG_VAR( secPIN_options_ptr->first_key_timeout );

  DBG_VAR(secPIN_options_ptr->pin_min_number);
  DBG_VAR(secPIN_options_ptr->pin_max_number);
  DBG_VAR(secPIN_options_ptr->total_timeout);

  // RDD 22052013 SPIRA:6736 - set this flag only when Offline Pin Entry is actually called by the kernel. Remove line below
  //transaction_set_status( &txn, st_offline_pin );
  return VIM_ERROR_NONE;
}


// RDD 230516 modified slightly so that we know more easily if we have put anything into the list

//-------------------------------------------------------------------------------------------------
VIM_ERROR_PTR vim_emv_CB_exclude_aids_from_candidate_list( VIM_EMV_AID *exclude_list, VIM_SIZE *excluded )
{ 	
	VIM_DBG_NUMBER( txn.type );  
	*excluded = 0;
	if( txn.type == tt_preauth )  
	{
	  DBG_MESSAGE( "Setting Up exclude List for Preauth" );    
	  vim_mem_copy( exclude_list[0].aid, VIM_EMV_AID_EPAL_SAV, VIM_SIZEOF(VIM_EMV_AID_EPAL_SAV)-1 );    
	  exclude_list[0].aid_size = VIM_SIZEOF(VIM_EMV_AID_EPAL_SAV)-1;    
	  vim_mem_copy( exclude_list[1].aid, VIM_EMV_AID_EPAL_CHQ, VIM_SIZEOF(VIM_EMV_AID_EPAL_CHQ)-1 );    
	  exclude_list[1].aid_size = VIM_SIZEOF(VIM_EMV_AID_EPAL_CHQ)-1; 
	  *excluded = 2;
	}  
	return VIM_ERROR_NONE;
}
