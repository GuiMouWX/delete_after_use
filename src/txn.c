
#include <incs.h>

VIM_DBG_SET_FILE("TX");

#define TMS_PARAM_PIN_ENTRY_OPTIONAL 0x01
#define TMS_PARAM_PIN_ENTRY_NONE     0x00
#define TMS_PARAM_PIN_ENTRY_REQUIRED 0x02


// WOW Pin Processing

#define WOW_PSC_PIN_MANDATORY                   0x00
#define WOW_PSC_PIN_OPTIONAL                    0x01
#define ACC_LIST_TITLE	                    "Select Account"
// RDD - define a few "specials" that are used in the code. These denote positions within the array below..


extern VIM_ERROR_PTR preauth_select_record( VIM_UINT32 *rrn );

extern VIM_ERROR_PTR ValidateTrack2( VIM_CHAR *Track2, VIM_SIZE TrackLen );
extern VIM_ERROR_PTR emv_get_transaction_results(void);
extern VIM_ERROR_PTR SetReasonCode(void);
extern VIM_BOOL IsSlaveMode(void);

VIM_UINT8 attempt;


// RDD 231211 This matches the Card Name Table in txn.c

card_bin_name_item_t CardNameTable[CARD_INDEX_MAX];
card_bin_list_t BlockedBinTable[CARD_INDEX_MAX];		// 100 blocked BINS should be fine


static txn_state_t txn_enter_passcode(void);

#define CVM_LIMIT   10000

const char *duplicate_txn_file_name = VIM_FILE_PATH_LOCAL "duptxn.dat";
static const char *txn_file_name = VIM_FILE_PATH_LOCAL "txn.dat";
extern VIM_BOOL emv_attempt_to_go_online;


// RDD - txn type is needed to build the top line of the displey in most of the stages
VIM_CHAR type[30];

VIM_SIZE TxnFileSize;

txn_state_t TXN_FINALISATION_IF_ERROR(VIM_ERROR_PTR x)
{
	if( x != VIM_ERROR_NONE )
	{
		txn.error = x;
		//VIM_DBG_ERROR(x);
		return state_set_state(ts_finalisation);
	}
	return state_get_next();
}

#define TXN_RETURN_EXIT(x) {txn.error=x;VIM_DBG_ERROR(x);return state_set_state(ts_exit);}
#define TXN_RETURN_ERROR(x) {txn.error=x;VIM_DBG_ERROR(x);return state_set_state(ts_finalisation);}
#define TXN_RETURN_CAPTURE(x) {txn.error=x;VIM_DBG_ERROR(x);return state_set_state(ts_card_capture);}
#define TXN_RETURN_POS_ABORT(x) {txn.error=x;vim_strcpy( txn.host_rc_asc, "" );return state_set_state(ts_finalisation);}

/* ------------------------------------------------------------- */



VIM_BOOL ValidTxnType( txn_type_t Type )
{
	VIM_SIZE n;
	const txn_type_t ValidList[] = { tt_sale, tt_cashout, tt_sale_cash, tt_refund, tt_completion };

	for( n=0; n<VIM_LENGTH_OF( ValidList ); n++ )
	{
		if( Type == ValidList[n] ) return VIM_TRUE;
	}
	return VIM_FALSE;
}

VIM_BOOL IsEGC(void)
{
    VIM_CHAR Pan[WOW_PAN_LEN_MAX + 1];
    VIM_SIZE egc_bin_len = vim_strlen(terminal.ecg_bin);

    vim_mem_clear(Pan, VIM_SIZEOF(Pan));
    card_get_pan(&txn.card, Pan);
    return (vim_mem_compare(terminal.ecg_bin, Pan, egc_bin_len) == VIM_ERROR_NONE) ? VIM_TRUE : VIM_FALSE;
}

VIM_BOOL IsBGC(void)
{
    if ((transaction_get_status(&txn, st_pos_sent_pci_pan) && transaction_get_status(&txn, st_electronic_gift_card)))
    {
        return VIM_TRUE;
    }
    else
    {
        return VIM_FALSE;
    }
}


VIM_BOOL fuel_card( transaction_t *transaction )
{
    VIM_SIZE Index = CardIndex(transaction);

	if( !ValidTxnType( transaction->type ) ) return VIM_FALSE;

    return (CardNameTable[Index].bin == PCEFT_FUEL_CARD) ? VIM_TRUE : VIM_FALSE;

	//return ( CardIndex(transaction) < FUEL_CARD_MIN_INDEX ) ? VIM_FALSE : VIM_TRUE;
}

VIM_BOOL IsEverydayPay(transaction_t* transaction)
{
	return (transaction && (transaction->card.type == card_qr));
}



VIM_BOOL IsUPI( transaction_t *transaction )
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

	if( CardNameIndex == UPI_CARD_INDEX )
	{
		// RDD 230916 SPIRA:9054 Requirement from BD : "We should NOT print ANY of the Unionpay specific receipt requirements if it is a
		// "domestic card" with a "Debit A/C" as these transactions are routed to E-Hub."
		if(( emv_is_card_international() == VIM_FALSE ) && (( transaction->account == account_savings )||( transaction->account == account_cheque )))
		{
			return VIM_FALSE;
		}
		return VIM_TRUE;
	}
	return VIM_FALSE;
}

VIM_BOOL IsMC( transaction_t *transaction )
{
    VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

    if (!FullEmv(transaction))
    {
        if (CardNameIndex == MASTERCARD)
            return VIM_TRUE;
    }
    // Some EMV cards have other indeces so also return TRUE if RID is VISA
    else if( vim_mem_compare(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_MASTERCARD, 5 ) == VIM_ERROR_NONE )
    {
        return VIM_TRUE;
    }
    return VIM_FALSE;
}

VIM_BOOL IsVISA( transaction_t *transaction )
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

    if (!FullEmv(transaction))
    {
        if (CardNameIndex == VISA)
            return VIM_TRUE;
    }
	// Some EMV cards have other indeces so also return TRUE if RID is VISA - watch out for zero AID index value as can give false pos for swipe and manpan
	else if( vim_mem_compare( terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_VISA, 5 ) == VIM_ERROR_NONE )
	{
		return VIM_TRUE;
	}
	return VIM_FALSE;
}

VIM_BOOL IsAmex(transaction_t *transaction)
{
    VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

    if (CardNameIndex == AMEX)
        return VIM_TRUE;

    // Some EMV cards have other indeces so also return TRUE if RID is VISA
    if (vim_mem_compare(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_AMEX, 5) == VIM_ERROR_NONE)
    {
		VIM_DBG_MESSAGE(" AMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEXAMEX ");
        return VIM_TRUE;
    }
    return VIM_FALSE;
}

VIM_BOOL IsJCB(transaction_t *transaction)
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
	VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

	if (CardNameIndex == JCB)
		return VIM_TRUE;

	// Some EMV cards have other indeces so also return TRUE if RID is VISA
	if (vim_mem_compare(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_JCB, 5) == VIM_ERROR_NONE)
	{
		return VIM_TRUE;
	}
	return VIM_FALSE;
}

VIM_BOOL IsDiners(transaction_t *transaction)
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
	VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

	if (CardNameIndex == DINERS)
		return VIM_TRUE;

	// Some EMV cards have other indeces so also return TRUE if RID is VISA
	if (vim_mem_compare(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_DPAS, 5) == VIM_ERROR_NONE)
	{
		return VIM_TRUE;
	}
	return VIM_FALSE;
}


VIM_BOOL IsEFTPOS( transaction_t *transaction )
{
	// Some EMV cards have other indeces so also return TRUE if RID is VISA
	if( vim_mem_compare( terminal.emv_applicationIDs[transaction->aid_index].emv_aid, VIM_EMV_RID_EFTPOS, 5 ) == VIM_ERROR_NONE )
	{
		VIM_DBG_MESSAGE("Is EFTPOS A");
		return VIM_TRUE;
	}
	VIM_DBG_MESSAGE("EEEEEEEEEEEEEEEEEEEE Not EFTPOS AID");
    if((transaction->account == account_cheque) || (transaction->account == account_savings))
    {
		VIM_DBG_MESSAGE("Chq or SAV");

        if(!gift_card(transaction))
        {
			VIM_DBG_MESSAGE("Is EFTPOS SAV or CHQ Acc");
			return VIM_TRUE;
        }
    }
	VIM_DBG_MESSAGE("Not EFTPOS");
	return VIM_FALSE;
}


VIM_BOOL IsReturnsCard(transaction_t* transaction)
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
	VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

	if (CardNameIndex == RETURNS_CARD)
		return VIM_TRUE;

	return VIM_FALSE;
}


// RDD 010422 - ensure SDA is disabled totally.
VIM_BOOL IsSDA(transaction_t *transaction)
{
   // if( TERM_VDA && IsVISA(transaction) )
   //     return VIM_TRUE;
   // if (TERM_MDA && IsMC(transaction))
   //     return VIM_TRUE;
    return VIM_FALSE;
}

// RDD Trello #69 and #199 : If selected label mentions debit then assign default account at this point.
VIM_BOOL CheckDebitLabel(VIM_CHAR *Label)
{
    VIM_CHAR *Ptr = VIM_NULL;
    if (vim_strstr(Label, "Debit", &Ptr) == VIM_ERROR_NONE)
    {
        return VIM_TRUE;
    }
    if (vim_strstr(Label, "DEBIT", &Ptr) == VIM_ERROR_NONE)
    {
        return VIM_TRUE;
    }
    return VIM_FALSE;
}


VIM_ERROR_PTR GetCardName(transaction_t *transaction, VIM_CHAR *Ptr)
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
	VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
	static VIM_CHAR TempCardName[MAX_APPLICATION_LABEL_LEN + 1];

	DBG_VAR(CpatIndex);

	if ((CpatIndex == WOW_MAX_CPAT_ENTRIES) || (transaction->type == tt_query_card))
	{
		DBG_VAR(transaction->type);
		vim_strcpy(Ptr, " ");
		DBG_MESSAGE("------------- UNABLE TO FIND CARD NAME -------------");
		return VIM_ERROR_NOT_FOUND;
	}

	VIM_DBG_NUMBER(CardNameIndex);
	//DBG_MESSAGE(CardNameTable[CardNameIndex].card_name);

	// RDD 100818 changed a txn -> transaction here while investigating JIRA WWBP-293
	if ((transaction_get_status(transaction, st_label_found)) && (transaction->error != ERR_POWER_FAIL))
	{
		// We already got the best card name for this transaction
		DBG_MESSAGE("*Got label*");
		if (emv_get_app_label(transaction, TempCardName) == VIM_ERROR_NONE)
		{
			vim_strcpy(Ptr, TempCardName);
		}
		DBG_MESSAGE(Ptr);
		return VIM_ERROR_NONE;
	}

	vim_mem_clear(TempCardName, VIM_SIZEOF(TempCardName));

	// RDD 190213 - Setup the Default Card Name (from the Cpat Index)
	vim_strcpy(TempCardName, CardNameTable[CardNameIndex].card_name);

	// RDD 010716 SPIRA:8977 UPI Requires common Card Name : UNIONPAY
	if (IsUPI(transaction))
	{
		vim_strcpy(Ptr, TempCardName);
		return VIM_ERROR_NONE;
	}
	// RDD 190213 - Modify if Training mode
	if ((terminal.training_mode) && (transaction->card.type == card_chip))
	{
		vim_strcpy(TempCardName, "Training Card");
	}
	// RDD 190213 - Modify if Chip Card
	else if (transaction->card.type == card_chip || transaction->card.type == card_picc)
	{
		//DBG_MESSAGE( "---- Try to find App Label ------" );
		DBG_MESSAGE(TempCardName);
		if (emv_get_app_label( transaction, TempCardName) == VIM_ERROR_NONE)
		{
			//DBG_MESSAGE( "---- Success ------" );
			transaction_set_status(transaction, st_label_found);
		}
		else
		{
			//DBG_MESSAGE( "---- Failed ------" );
		}
		DBG_MESSAGE(TempCardName);
	}
	else
	{
		// RDD 230318 - this is a bug ! Causes no label display on Pin entry etc. for magcards
		//transaction_set_status( transaction, st_label_found );
	}

	vim_strcpy(Ptr, TempCardName);
	DBG_MESSAGE(Ptr);
	return VIM_ERROR_NONE;
}


// Decide whether to allow "Connecting", "Sending", "Receiving" type messages during comms
VIM_BOOL ShowCommsStatus( void )
{
	// RDD 210515 MB requested we make the Comms Status display global
#if 0
	if( terminal.configuration.TXNCommsType[0] != PSTN )
		return VIM_FALSE;

	if( txn.type == tt_logon )
		return VIM_FALSE;
#endif
	return VIM_TRUE;
}



VIM_CHAR *GetLogoFile(VIM_UINT32 CardIndex)
{
	static VIM_CHAR FileNameBuff[30];
	static VIM_CHAR *Ptr = FileNameBuff;
	VIM_BOOL exist = VIM_FALSE;
    VIM_CHAR buf_format[16];
    VIM_UINT8 format_len;

	// Set up a default file without a logo
	vim_sprintf(Ptr, "NoLogo.", "");

	// Donna only wants logos displayed for Contactless
	if (txn.card.type == card_picc)
	{
		if (IsApplePay())
		{
			if (CardIndex == VISA)
				vim_sprintf(Ptr, "%sLogoAV.", VIM_FILE_PATH_LOCAL);
			else if (CardIndex == AMEX)
				vim_sprintf(Ptr, "%sLogoAA.", VIM_FILE_PATH_LOCAL);
		}
		else
		{
			// RDD 200217 SPIRA:9209 Multi-network cards with EPAL as primary showed Mastercard as logo (from CPAT)
			if (is_EPAL_cards(&txn))
			{
				vim_sprintf(Ptr, "%sLogo%2.2d.", VIM_FILE_PATH_LOCAL, DEBIT_CARD);
			}
			else
			{
				vim_sprintf(Ptr, "%sLogo%2.2d.", VIM_FILE_PATH_LOCAL, CardIndex);
			}
		}
		if (vim_file_exists(Ptr, &exist) == VIM_ERROR_NONE)
		{
			DBG_VAR(exist);
			if (exist)
				Ptr += vim_strlen(VIM_FILE_PATH_LOCAL);
			else
				vim_sprintf(Ptr, "%sNoLogo.", "");
		}
	}

//    if (vim_adkgui_is_active()) {
        // Replace all file names with image format specified for html based display
    display_get_image_format(buf_format, &format_len);
    vim_strncat(Ptr, buf_format, format_len);
//    }

	DBG_STRING(Ptr);
	return Ptr;
}


void DisplayTxnWarning(transaction_t *transaction, const VIM_CHAR *TxnLabel)
{
    VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN + 1];
    VIM_CHAR Account[3 + 1];
    vim_mem_clear(CardName, VIM_SIZEOF(CardName));
    vim_mem_set(CardName, ' ', VIM_SIZEOF(CardName) - 1);

    vim_mem_clear(Account, VIM_SIZEOF(Account));
    vim_mem_set(Account, ' ', VIM_SIZEOF(Account) - 1);

    if (GetCardName(transaction, CardName) == VIM_ERROR_NONE)
    {
        // Don't try to get Account if no Card Name
        get_acc_type_string_short(transaction->account, Account);
    }
    display_screen( DISP_GEN_TXN, VIM_PCEFTPOS_KEY_NONE, type, AmountString(transaction->amount_total), CardName, Account, TxnLabel );
    vim_event_sleep(2000);
}

VIM_ERROR_PTR CalculateSurcharge(transaction_t *transaction, VIM_CHAR *surcharge_data )
{
    VIM_UINT64 Tip = (TERM_SC_ON_TIP) ? transaction->amount_tip : 0;
    VIM_UINT64 Cash = (TERM_SC_EXCLUDE_CASH) ? 0 : transaction->amount_cash;
	VIM_CHAR surcharge[4+1] = { 0 };
	VIM_CHAR *Ptr = &surcharge[0];		
	VIM_SIZE percentage_points = 0;
	VIM_SIZE Remainder;

	// RDD 200922 JIRA PIRPIN-1597 Change logic relating to sucharge on cash component - don't apply to eftpos
	if (IsEFTPOS(transaction))
		Cash = 0;


	vim_strcpy(surcharge, surcharge_data);
	if ((*Ptr == 'c') || (*Ptr == 'C'))
	{
		VIM_SIZE len = vim_strlen(Ptr+1);
		Ptr += 1;
		transaction->amount_surcharge = asc_to_bin((const VIM_CHAR*)Ptr, len);
		return VIM_ERROR_NONE;
	}
	else
	{
		VIM_SIZE len = vim_strlen(Ptr);
		percentage_points = asc_to_bin((const VIM_CHAR*)Ptr, len);
	}

	// RDD 200922 JIRA PIRPIN-1597 Change logic relating to sucharge on cash component
#if 0
	if( Cash >= ( transaction->amount_purchase + Tip ))
    {
        transaction->amount_surcharge = 0;
		return VIM_ERROR_NONE;
    }
#endif

	// RDD 200922 JIRA PIRPIN-1597 Change logic relating to sucharge on cash component
	transaction->amount_surcharge = ((( transaction->amount_purchase+Tip ) + Cash ) * percentage_points) / (10000);
    Remainder = (((transaction->amount_purchase + Tip) * percentage_points) % (10000))/1000;
    // Add a cent for 6,7,8,9
    if (Remainder > 5)
        transaction->amount_surcharge += 1;
    VIM_DBG_NUMBER(Remainder);
    return VIM_ERROR_NONE;
}



VIM_BOOL ApplySurcharge(transaction_t *transaction, VIM_BOOL Apply)
{
    //VIM_UINT32 CpatIndex = transaction->card_name_index;
    //VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
    VIM_BOOL SurchargeApplied = VIM_FALSE;

    // RDD 221021 - ensure we don't apply surcharge twice - like in case when swipe card pin entry is re-run.
    if (( transaction->amount_surcharge ) && ( Apply ))
    {
        pceft_debug_test(pceftpos_handle.instance, "Surcharge was already applied");
        return VIM_FALSE;
    }

	if( transaction_get_status( transaction, st_giftcard_s_type ))
	{
		return VIM_FALSE;
	}

	// RDD 180122 Don't allow surcharge on pre-auths, refunds etc.
	if(( transaction->type != tt_sale )&&( transaction->type != tt_sale_cash )&&( transaction->type != tt_cashout )&&( transaction->type != tt_moto )&& ( transaction->type != tt_checkout ) && ( transaction->type != tt_completion ))
	{
		DBG_POINT;
		return VIM_FALSE;
	}


    if (Apply)
        transaction->amount_surcharge = 0;

    if (IsEFTPOS(transaction))
    {
        if (terminal.configuration.surcharge_points_eftpos)
        {
            SurchargeApplied = VIM_TRUE;
            if (Apply)
            {
                CalculateSurcharge(transaction, terminal.configuration.surcharge_points_eftpos);
				if(transaction->amount_surcharge)
					pceft_debugf_test(pceftpos_handle.instance, "Apply EFTPOS Surcharge of %d cents", transaction->amount_surcharge);
            }
        }
    }
    else if (IsVISA(transaction) && (terminal.configuration.surcharge_points_visa))
    {
        SurchargeApplied = VIM_TRUE;
        if (Apply)
        {
            CalculateSurcharge(transaction, terminal.configuration.surcharge_points_visa);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply VISA Surcharge of %d cents", transaction->amount_surcharge);
        }
    }
    else if (IsMC(transaction) && (terminal.configuration.surcharge_points_mc))
    {
        SurchargeApplied = VIM_TRUE;
        if (Apply)
        {
            CalculateSurcharge(transaction, terminal.configuration.surcharge_points_mc);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply Mastercard Surcharge of %d cents", transaction->amount_surcharge);
        }
    }
    else if (IsAmex(transaction) && (terminal.configuration.surcharge_points_amex))
    {
        SurchargeApplied = VIM_TRUE;
        if (Apply)
        {
            CalculateSurcharge(transaction, terminal.configuration.surcharge_points_amex);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply Amex Surcharge of %d cents", transaction->amount_surcharge);
        }
    }
	else if (IsUPI(transaction) && (terminal.configuration.surcharge_points_upi))
	{
		SurchargeApplied = VIM_TRUE;
		if (Apply)
		{
			CalculateSurcharge(transaction, terminal.configuration.surcharge_points_upi);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply UPI Surcharge of %d cents", transaction->amount_surcharge);
		}
	}
	else if (IsDiners(transaction) && (terminal.configuration.surcharge_points_diners))
	{
		SurchargeApplied = VIM_TRUE;
		if (Apply)
		{
			CalculateSurcharge(transaction, terminal.configuration.surcharge_points_diners);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply Diners Surcharge of %d cents", transaction->amount_surcharge);
		}
	}
	else if (IsJCB(transaction) && (terminal.configuration.surcharge_points_jcb))
	{
		SurchargeApplied = VIM_TRUE;
		if (Apply)
		{
			CalculateSurcharge(transaction, terminal.configuration.surcharge_points_jcb);
			if (transaction->amount_surcharge)
				pceft_debugf_test(pceftpos_handle.instance, "Apply JCB Surcharge of %d cents", transaction->amount_surcharge);
		}
	}

    if(( SurchargeApplied ) && ( Apply ))
    {
        transaction->amount_total += transaction->amount_surcharge;
		if (transaction->amount_surcharge)
			pceft_debugf_test( pceftpos_handle.instance, "Surcharge %lu Total %lu", transaction->amount_surcharge, transaction->amount_total );
    }
    return SurchargeApplied;
}

VIM_ERROR_PTR DisplayStatus(VIM_CHAR *Text, VIM_SIZE ScreenID)
{
	VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN + 1];
	VIM_CHAR Account[3 + 1];
	DBG_MESSAGE("Processing...");
	vim_mem_clear(CardName, VIM_SIZEOF(CardName));
	vim_mem_set(CardName, ' ', VIM_SIZEOF(CardName) - 1);

	vim_mem_clear(Account, VIM_SIZEOF(Account));
	vim_mem_set(Account, ' ', VIM_SIZEOF(Account) - 1);

	if (GetCardName(&txn, CardName) == VIM_ERROR_NONE)
	{
		// Don't try to get Account if no Card Name
		get_acc_type_string_short(txn.account, Account);
	}

	VIM_DBG_STRING(type);
	VIM_DBG_STRING(AmountString(txn.amount_total));
	VIM_DBG_STRING(CardName);
	VIM_DBG_STRING(Account);

	VIM_ERROR_RETURN( display_screen( ScreenID, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), CardName, Account, Text ));
}



VIM_ERROR_PTR DisplayProcessing(VIM_SIZE ScreenID)
{
    VIM_ERROR_RETURN( DisplayStatus( "Processing", ScreenID ));    
}


VIM_ERROR_PTR DisplayConnecting(VIM_CHAR *Message)
{
	VIM_ERROR_RETURN( DisplayStatus( Message, DISP_CONNECTING_WAIT));
}



void DisplayCommsState(VIM_AS2805_MESSAGE_TYPE msg_type, VIM_CHAR *DisplayText2)
{
	//VIM_CHAR DisplayBuffer[50];		
	return;

	switch (msg_type)
	{
	case MESSAGE_WOW_800_NMIC_191:
		//vim_sprintf(DisplayBuffer, "Initialise 191\n%s", DisplayText2);
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_800_NMIC_192:
		//vim_sprintf(DisplayBuffer, "Initialise 192\n%s", DisplayText2);
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_800_NMIC_193:
		//vim_sprintf(DisplayBuffer, "Initialise 193\n%s", DisplayText2);
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_800_NMIC_170:
		//vim_sprintf(DisplayBuffer, "Initialising\n%s", DisplayText2);
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_800_NMIC_101:
		//vim_sprintf(DisplayBuffer, "Initialise 101\n%s", DisplayText2);
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_600:
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		break;
	case MESSAGE_WOW_200:
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "");
		//DisplayConnecting(DisplayText2);
		break;

	default:
		break;
	}
}



VIM_ERROR_PTR data_entry_screen( VIM_UINT32 screen_id, VIM_CHAR *pinpad_display, VIM_CHAR *pos_line_1, VIM_CHAR *InputPtr, VIM_UINT8 MinDigits, VIM_UINT8 MaxDigits, VIM_UINT32 Opts )
{
  kbd_options_t options;
  VIM_ERROR_PTR res = VIM_ERROR_NONE;

  options.entry_options = Opts;
  options.timeout = ENTRY_TIMEOUT;
  options.min = MinDigits;
  options.max = MaxDigits;
  options.entry_options |= KBD_OPT_DISP_ORIG;
  options.CheckEvents = VIM_TRUE;


  do
  {
#ifdef _WIN32
      VIM_ERROR_RETURN_ON_FAIL(display_screen(screen_id, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), pinpad_display));
      res = data_entry(InputPtr, &options);
#else
      // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
      res = display_data_entry(InputPtr, &options, screen_id, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), pinpad_display);
#endif

  }
  while (VIM_ERROR_USER_BACK == res);

  VIM_DBG_STRING(InputPtr);

  return res;
}

VIM_ERROR_PTR option_entry_screen( VIM_CHAR option, VIM_UINT32 screen_id, VIM_CHAR *pinpad_data, VIM_CHAR *pos_data, VIM_CHAR *Tag, VIM_CHAR *InputPtr, VIM_UINT8 MinDigits, VIM_UINT8 MaxDigits, VIM_UINT64 *Data )
{

  kbd_options_t options;
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_CHAR pos_line_1[20];
  VIM_CHAR pos_line_2[20];
  VIM_CHAR pinpad_display[40];
  VIM_CHAR InBuff[20],TagBuff[30];
  VIM_UINT8 DataLen;
  VIM_UINT64 Value;
  VIM_BOOL OdoRetry = txn.u_PadData.PadData.OdoRetryFlag;

  vim_mem_clear( pos_line_2, VIM_SIZEOF(pos_line_2) );
  vim_mem_clear( pinpad_display, VIM_SIZEOF( pinpad_display) );
  vim_mem_clear( InBuff, VIM_SIZEOF( InBuff ) );
  vim_mem_clear( TagBuff, VIM_SIZEOF( TagBuff ));
  options.timeout = ENTRY_TIMEOUT;
  options.min = MinDigits;
  options.max = MaxDigits;
  options.entry_options = KBD_OPT_DISP_ORIG;
  options.CheckEvents = VIM_TRUE;

  vim_sprintf( pos_line_1, "KEY %s", pos_data );

  if( !option ) return VIM_ERROR_NONE;	// No data entry needed

  if(( *Data )&&( !OdoRetry ))  return VIM_ERROR_NONE;	// Data already supplied in PAD

  // RDD 090812 SPIRA:5845 completion with 'K' (cardholder not present) must run automatically with no interraction from user
  else if(( txn.card.type == card_manual ) && ( transaction_get_status( &txn, st_pos_sent_pci_pan ) ))
  {
/*
	  if (terminal_get_status(ss_wow_basic_test_mode))
	  {
		pceft_debug( pceftpos_handle.instance, "K-COMPLETION: BYPASSING DATA ENTRY...." );
	  }
*/
	  return VIM_ERROR_NONE;
  }

  else if( option == OPTIONAL ) 						// Optional Data Entry
  {
	vim_sprintf( pinpad_display, "Key %s\n%s", pinpad_data, "Or Press Enter" );
	vim_strcpy( pos_line_2, "OR PRESS ENTER" );

	//options.min = 0;
  }
  else											// Mandatory Data Entry
  {
      vim_sprintf( pinpad_display, "Key %s\n%s", pinpad_data, "And Press Enter" );
      vim_strcpy( pos_line_2, "AND PRESS ENTER" );
  }


  // RDD 081119 JIRA WWBP-752 ODO Dispolay freeze due to old UI
  //VIM_ERROR_RETURN_ON_FAIL( display_screen(screen_id, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), pinpad_display ));

  do
  {
      pceft_pos_display(pos_line_1, pos_line_2, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_DATA);

      if(( res = display_data_entry( InBuff, &options, screen_id, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), pinpad_display )) == VIM_ERROR_USER_CANCEL )
      {
          vim_mem_clear( InBuff, VIM_SIZEOF( InBuff ) );
          res = VIM_ERROR_USER_BACK;
	  }

	  // If mandatory and user attempts bypass show warning
	  else if( !vim_strlen( InBuff ) && ( option == 2 ) && ( res == VIM_ERROR_NONE ))
	  {
		vim_sprintf( pinpad_display, "%s Must\nBe Entered", pinpad_data );
		vim_sprintf( pos_line_1, "%s MUST", pos_data );
		vim_strcpy( pos_line_2, "BE ENTERED" );
		VIM_ERROR_RETURN_ON_FAIL( display_screen( screen_id, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), pinpad_display ));
		VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(pos_line_1, pos_line_2, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_DATA ));
		vim_kbd_sound_invalid_key();
		vim_event_sleep(2000);

		vim_sprintf( pos_line_1, "KEY %s", pos_data );
		vim_sprintf( pinpad_display, "Key %s\n%s", pinpad_data, "And Press Enter" );
		vim_strcpy( pos_line_2, "AND PRESS ENTER" );

		VIM_ERROR_RETURN_ON_FAIL( display_screen( screen_id, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), pinpad_display ));
		VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(pos_line_1, pos_line_2, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_DATA ));
		res = VIM_ERROR_USER_BACK;
	  }
	  // DBG_MESSAGE( InBuff );
  }
  while (VIM_ERROR_USER_BACK == res);

  VIM_DBG_STRING(InBuff);
  DataLen = vim_strlen( InBuff );

  if(( DataLen )||( OdoRetry ))
  {
	VIM_CHAR *Ptr;

	Value = asc_to_bin( InBuff, DataLen );
	DBG_VAR( Value );
	*Data = Value;
	// Fix the length of the ODO or ODO retry as It may be replaced within the string
	if( !vim_strncmp( Tag, ODO, PAD_TAG_LEN ))
	{
		if( DataLen )
		{
			VIM_ERROR_RETURN_ON_FAIL(vim_snprintf( TagBuff,  VIM_SIZEOF(TagBuff), "%s%3.3d%7.7d", OdoRetry ? ORT : ODO, MaxDigits, (VIM_SIZE)Value ));
			DBG_STRING( TagBuff );
		}
		// RDD 270712 SPIRA:5802 if bypass on ORT send flag anyway along with zeros
		else if( OdoRetry )
		{
			Value = 0;
			VIM_ERROR_RETURN_ON_FAIL(vim_snprintf( TagBuff,  VIM_SIZEOF(TagBuff), "%s%3.3d%7.7d", ORT, MaxDigits, (VIM_SIZE)Value ));
		}

	}
	else
	{
		VIM_ERROR_RETURN_ON_FAIL(vim_snprintf( TagBuff,  VIM_SIZEOF(TagBuff), "%s%3.3d%s", Tag, vim_strlen(InBuff), InBuff ));
		DBG_STRING( TagBuff );
	}

	// append the acquired tag data to the end of the Tag buffer for submission to the host in DE63
	if((( vim_strstr( InputPtr, ODO, &Ptr ) == VIM_ERROR_NONE )||( vim_strstr( InputPtr, ORT, &Ptr ) == VIM_ERROR_NONE ))&&( !vim_strncmp( Tag, ODO, PAD_TAG_LEN )))
	{
		// RDD 210612 He can't be bothered to get the correct ODO so we've Got to delete the original ODO and shift everything else. What a pain !!
#if 0			// Copy the original position

		if(( OdoRetry )&&( !DataLen ))
		{
			VIM_CHAR *Ptr2 = Ptr;
			VIM_SIZE StringLen = vim_strlen( Ptr );
			VIM_SIZE OdoLen = ( PAD_TAG_LEN + PAD_TAG_FIELD_LEN + asc_to_bin( Ptr + PAD_TAG_LEN, PAD_TAG_FIELD_LEN) );
			// Shift the Ptr to the end of the ODO
			if( StringLen > OdoLen )
			{
				Ptr2 += OdoLen;
				// Move the rest of the string over what was the ODO ... Yawn ....
				vim_strncpy( Ptr, Ptr2, (StringLen - OdoLen) );
				*(Ptr + (StringLen - OdoLen)) = '\0';
			}
		}
		else
		{
			vim_mem_copy( Ptr, TagBuff, vim_strlen(TagBuff) );
			DBG_STRING( TagBuff );
		}
#else
		vim_mem_copy( Ptr, TagBuff, vim_strlen(TagBuff) );
		DBG_STRING( TagBuff );
#endif
	}
	else
	{
		vim_strcat( InputPtr, TagBuff );
	}
  }

  return res;
}


VIM_BOOL ValidateProductIndex( VIM_UINT32 ProductIndex, VIM_BOOL FuelCard )
{
	VIM_BOOL RetVal = VIM_FALSE;
	// If we have a fuelcard then check the fuelcard table
	if( FuelCard )
	{
		// Our Index needs to be not greater than the total Number of Tables parsed from the FCAT file
		if( ProductIndex <= fuelcard_data.NumberOfProductTables )
		{
			// The table needs to have a valid description in it
			if( vim_strlen( fuelcard_data.ProductTables[ProductIndex].ProductDescription ))
			{
				if( fuelcard_data.ProductTables[ProductIndex].VolumeFlag )
				{
					RetVal = VIM_TRUE;
				}
			}
		}
	}
	if( !RetVal )
		vim_kbd_sound_invalid_key();
	return RetVal;
}

// { 0,1,2,0 } = PIN, ODO, REGO, ORDER NO
const VIM_CHAR Option1100[] = { 1,1,0,0 };
const VIM_CHAR Option1110[] = { 1,1,1,0 };
const VIM_CHAR Option1111[] = { 1,1,1,1 };
const VIM_CHAR Option1101[] = { 1,1,0,1 };
const VIM_CHAR Option1001[] = { 1,0,0,1 };
const VIM_CHAR Option2222[] = { 2,2,2,2 };
const VIM_CHAR Option1000[] = { 1,0,0,0 };
const VIM_CHAR Option0111[] = { 0,1,1,1 };
const VIM_CHAR Option0112[] = { 0,1,1,2 };
const VIM_CHAR Option0211[] = { 0,2,1,1 };
const VIM_CHAR Option0212[] = { 0,2,1,2 };
const VIM_CHAR Option0100[] = { 0,1,0,0 };


VIM_ERROR_PTR CardDataOptionsSetup( VIM_UINT32 CardNameIndex, VIM_CHAR *Ptr )
{
	if( txn.u_PadData.PadData.OdoRetryFlag )
	{
		// RDD 190712 SPIRA:5782 - Pin entry to be left as it was in original txn
		txn.FuelData[PROMPT_ODOMETER] = OPTIONAL;
		txn.FuelData[PROMPT_VEHICLE_ID] = NOT_REQUIRED;
		txn.FuelData[PROMPT_ORDER_NUMBER] = NOT_REQUIRED;
		//vim_mem_copy( txn.FuelData, Option0100, WOW_FUEL_DATA_TYPES );
		return VIM_ERROR_NONE;
	}

	if( txn.card.type == card_manual )
	{
		DBG_MESSAGE( "MANUAL FUEL CARD - CONDITIONS CHANGE..." );
	}

	switch( CardNameIndex )
	{
			VIM_UINT64 Options;
			case STARCASH:	// RDD 240512 No manual entry for STARCASH
				if( txn.card.type == card_manual ) return ERR_KEYED_NOT_ALLOWED;
			case STARCARD:
				if( txn.card.type == card_manual )
				{
					vim_mem_copy( txn.FuelData, Option1111, WOW_FUEL_DATA_TYPES );
					return VIM_ERROR_NONE;
				}
			case VITALGAS:
				// RDD 240512 No manual entry for MOTORPASS unless completion with valid AUTH code
				if( txn.card.type == card_manual )
				{
					vim_mem_copy( txn.FuelData, Option1111, WOW_FUEL_DATA_TYPES );
					return ( txn.type == tt_completion ) ? VIM_ERROR_NONE : ERR_KEYED_NOT_ALLOWED;
				}
				Options = asc_to_bin( Ptr, WOW_STARCASH_OPTIONS_LEN );
				switch( Options )
				{
					case 101: vim_mem_copy( txn.FuelData, Option1100, WOW_FUEL_DATA_TYPES ); break;
					case 102: vim_mem_copy( txn.FuelData, Option1110, WOW_FUEL_DATA_TYPES ); break;
					case 103: vim_mem_copy( txn.FuelData, Option1111, WOW_FUEL_DATA_TYPES ); break;
					case 104: vim_mem_copy( txn.FuelData, Option1101, WOW_FUEL_DATA_TYPES ); break;
					case 105: vim_mem_copy( txn.FuelData, Option1001, WOW_FUEL_DATA_TYPES ); break;
					case 999: vim_mem_copy( txn.FuelData, Option2222, WOW_FUEL_DATA_TYPES ); break;
					default:  vim_mem_copy( txn.FuelData, Option1000, WOW_FUEL_DATA_TYPES ); break;
				}
				break;

			case MOTORCHARGE:
				// RDD 240512 No manual entry for MOTORCHARGE unless completion with valid AUTH code
				if( txn.card.type == card_manual )
				{
					vim_mem_copy( txn.FuelData, Option0111, WOW_FUEL_DATA_TYPES );
					break;
				}

				switch( *Ptr )
				{
					case '0':
					case '1':
						vim_mem_copy( txn.FuelData, Option0111, WOW_FUEL_DATA_TYPES ); break;
					case '2':
					case '3':
						vim_mem_copy( txn.FuelData, Option0112, WOW_FUEL_DATA_TYPES ); break;
					case '4':
					case '5':
						vim_mem_copy( txn.FuelData, Option0211, WOW_FUEL_DATA_TYPES ); break;
					case '6':
					case '7':
						vim_mem_copy( txn.FuelData, Option0212, WOW_FUEL_DATA_TYPES ); break;
					default:
						vim_mem_copy( txn.FuelData, Option2222, WOW_FUEL_DATA_TYPES ); break;
				}
				break;

			case MOTORPASS:
			default:
				vim_mem_copy( txn.FuelData, Option1100, WOW_FUEL_DATA_TYPES ); break;
	}
	// If Odo retry flag set then make ODO entry mandatory
	if( txn.u_PadData.PadData.OdoRetryFlag )
		txn.FuelData[PROMPT_ODOMETER] = OPTIONAL;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR SetupDataEntryParams( VIM_UINT32 CardNameIndex )
{
	VIM_CHAR *Ptr = VIM_NULL;
	// Find the '=' sign and then count from there to get the options field
	if( txn.card.type == card_mag )
		VIM_ERROR_RETURN_ON_FAIL( vim_strchr( txn.card.data.mag.track2, '=', &Ptr ));

	// Move to the options field for this card type
	switch( CardNameIndex )
	{
		case MOTORCHARGE:
		case FLEETCARD:		// RDD: Fleetcard like STARCARD but with extra '=' ...
			Ptr+=17;
			break;

		case STARCARD:
		case VITALGAS:
		case STARCASH:
			Ptr+=16;
		default:
			break;
	}
	return CardDataOptionsSetup( CardNameIndex, Ptr );
}



static VIM_ERROR_PTR check_training_error_amounts( VIM_UINT64 Amount )
{
	const s_training_error_t error_array[] = {
												{ 30500, "F0" }, { 3050000, "F0" },
												{ 30600, "F1" }, { 3060000, "F1" },
												{ 30700, "F2" }, { 3070000, "F2" },
												{ 30800, "F3" }, { 3080000, "F3" },
												{ 30900, "F4" }, { 3090000, "F4" },
												{ 31900, "F5" }, { 3190000, "F5" },
												{ 31100, "FD" }, { 3110000, "FD" },
												{ 31300, "FL" }, { 3130000, "FL" },
												{ 31400, "DX" }, { 3140000, "DX" },
												{ 31500, "FR" }, { 3150000, "FR" },
												{ 31600, "FS" }, { 3160000, "FS" },
												{ 31700, "FV" }, { 3170000, "FV" },
												{ 31200, "T3" }, { 3120000, "T3" },
												{ 32400, "T6" }, { 3240000, "T6" },
												{ 32500, "T7" }, { 3250000, "T7" },
												{ 30200, "TP" }, { 3020000, "TP" },
												{ 31800, "TW" }, { 3180000, "TW" },
												{ 32100, "OD" }, { 3210000, "OD" },
												{ 32300, "OJ" }, { 3230000, "OJ" },
												{ 32200, "OM" }, { 3220000, "OM" },
												{ 32000, "OP" }, { 3200000, "OP" },

											 };
	VIM_SIZE n, ArraySize = VIM_SIZEOF(error_array)/VIM_SIZEOF(s_training_error_t);

	// RDD 170612 Special case for ODO RETRY allow to continue with ODO entry
	if(txn.u_PadData.PadData.OdoRetryFlag)
	{
		//txn.FuelData[PROMPT_ODOMETER] = MANDATORY;
		return VIM_ERROR_NONE;
	}

	for( n=0; n< ArraySize; n++ )
	{
		if( Amount == error_array[n].Amount )
		{
			txn.error = ERR_WOW_ERC;
			vim_strcpy( txn.host_rc_asc, error_array[n].ErrorCode );
			return txn.error;
		}
	}
	return VIM_ERROR_NONE;
}
/* ------------------------------------------------------------- */

void AddItem( VIM_CHAR *Tag, VIM_SIZE TagData )
{
	if( TagData)
	{
		VIM_CHAR Buffer[20];
		vim_sprintf( Buffer, "%3.3s%d", Tag, TagData );
		vim_strcat( txn.u_PadData.PadData.DE63, Buffer );
	}
}




VIM_ERROR_PTR fuel_card_processing( void )
{
	//VIM_UINT32 CpatIndex = txn.card_name_index;
#if 0
	VIM_CHAR InputData[20];
	VIM_CHAR Instruction[20];
	VIM_SIZE ProductTableIndex;
#endif
	s_pad_data_t *PadData = &txn.u_PadData.PadData;

	// RDD 170612 moved to here from PCeft_commands so PAD processing only done for fuel cards now
	if(( txn.error = ValidatePADData( PadData, VIM_TRUE )) != VIM_ERROR_NONE )
	{
		pceft_debug( pceftpos_handle.instance, "PAD String Format Error - Rejected2" );
		return txn.error;
	}

	// RDD 010612 - add training mode support for Fuel Card errors
	if( terminal.training_mode )
	{
		VIM_ERROR_RETURN_ON_FAIL( check_training_error_amounts( txn.amount_total ) );
	}

	// RDD 131212 SPIRA:6449 Moved this to top as we're now going to exit early on completions
	// RDD 220612 SPIRA:5681 Need to add "VCH006V99999" to completions
	if( txn.type == tt_completion )
	{
		vim_strcat( txn.u_PadData.PadData.DE63, FIXED_VCH_STRING );
		// RDD 120712 SPIRA:5767 DG says don't prompt for data during completions
		return VIM_ERROR_NONE;
	}

	// Do some additonal validation as we have card data at this point
	if(( PadData->FuelDiscount ) && ( !CARD_NAME_ALLOW_FUEL_DISCOUNT ))
	{
		txn.error = ERR_WOW_ERC;
		vim_strcpy( txn.host_rc_asc, "FD" );
		return txn.error;
	}

 //VIM_
	if(( PadData->LoyltyCard ) && ( !CARD_NAME_ALLOW_LOYALTY_CARD ))
	{
		txn.error = ERR_WOW_ERC;
		vim_strcpy( txn.host_rc_asc, "FL" );
		return txn.error;
	}

 //VIM_
	if(( PadData->SplitTender ) && ( !CARD_NAME_ALLOW_SPLIT_TENDER ))
	{
		txn.error = ERR_WOW_ERC;
		vim_strcpy( txn.host_rc_asc, "FS" );
		return txn.error;
	}
 //VIM_

	SetupDataEntryParams( CardIndex(&txn) );
 //VIM_

	//txn.account = account_credit; RDD - actually set via CPAT ACG
#if 0
	// Check product data
	if(/* !( txn.u_PadData.PadData.NumberOfGroups ) */ 0)	// RDD 240512 - Product prompts not required
	{
		do
		{
			vim_mem_clear( InputData, VIM_SIZEOF(InputData) );
			vim_strcpy( Instruction, "KEY PRODUCT CODE" );

			if( option_entry_screen( '2', DISP_KEY_DATA, "Key Product Code", "KEY PRODUCT CODE", "", InputData, WOW_MIN_PRODUCT_CODE_LEN, WOW_MAX_PRODUCT_CODE_LEN, &ProductTableIndex ) )
				break;
		}
		while( ValidateProductIndex( ProductTableIndex, VIM_TRUE ) == VIM_FALSE );
	}
#endif

	// RDD 130612 SPIRA:5660 No product data if CPAT Flag cleared

	if(( !CARD_NAME_CAPTURE_PRODUCT_CODE )&&( txn.type != tt_completion ))
	{
		vim_strcpy( txn.u_PadData.PadData.DE63, "BSK000" );
	}

	// Get Odo reading if required
	if(( PadData->OdoRetryFlag )&&( txn.type != tt_completion ))
	{
		PadData->OdoReading = 0;
	}

	// RDD 150812 SPIRA:5886 - Ignore Retry Flag if completion
	//if(( !PadData->OdoReading )||( PadData->OdoRetryFlag ))
	if( !PadData->OdoReading )
	{
		VIM_UINT64 tmp_uint64=0;
		VIM_ERROR_RETURN_ON_FAIL( option_entry_screen( txn.FuelData[PROMPT_ODOMETER], DISP_KEY_DATA, "Odometer", "ODOMETER", ODO, txn.u_PadData.PadData.DE63, 0, WOW_MAX_ODO_LEN, &tmp_uint64 ) );
		PadData->OdoReading = (VIM_SIZE)tmp_uint64;
	}

	// Get Rego reading if required
	if( !PadData->VehicleNumber )
	{
		VIM_UINT64 tmp_uint64=0;
		VIM_ERROR_RETURN_ON_FAIL( option_entry_screen( txn.FuelData[PROMPT_VEHICLE_ID], DISP_KEY_DATA, "Vehicle #", "VEHICLE NUMBER", VHL, txn.u_PadData.PadData.DE63, 0, WOW_MAX_REGO_LEN, &tmp_uint64 ) );
		PadData->VehicleNumber = (VIM_SIZE)tmp_uint64;
	}

	// Get Order Number if required
	if( !PadData->OrderNumber )
	{
		VIM_UINT64 tmp_uint64=0;
		VIM_ERROR_RETURN_ON_FAIL( option_entry_screen( txn.FuelData[PROMPT_ORDER_NUMBER], DISP_KEY_DATA, "Order #", "ORDER NUMBER", ORD, txn.u_PadData.PadData.DE63, 0, WOW_MAX_ORDER_NO_LEN, &tmp_uint64 ) );
		PadData->OrderNumber = tmp_uint64;
	}
	// RDD 131212 SPIRA:6449 Moved this to top as we're now going to exit early on completions
#if 0
	// RDD 220612 SPIRA:5681 Need to add "VCH006V99999" to completions
	if( txn.type == tt_completion )
	{
		vim_strcat( txn.u_PadData.PadData.DE63, FIXED_VCH_STRING );
		// RDD 120712 SPIRA:5767 DG says don't prompt for data during completions
	}
#endif
    return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */

// RDD 260313 SPIRA 6658: Gift Cards also include Essentials Card and Returns Card

VIM_BOOL gift_card(transaction_t *transaction)
{
    VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_SIZE Index = CardIndex(transaction);
    VIM_CHAR Buffer[100];

    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    // RDD - new method used BIN
    vim_sprintf( Buffer, "Gift Card Check Index = %d", Index );
    //pceft_debug_test( pceftpos_handle.instance, Buffer );

#if 0
    if( txn.card_state == card_capture_new_gift )
    {
        vim_sprintf( Buffer, "New Gift uses Card BIN = %d", CardNameTable[Index].bin );
        //pceft_debug_test(pceftpos_handle.instance, Buffer );
        if( CardNameTable[Index].bin != PCEFT_BIN_GENERIC_GIFT && CardNameTable[Index].bin != PCEFT_BIN_QC_GIFT )
        {
            //pceft_debug_test(pceftpos_handle.instance, "Not a Gift Card (new flow)" );
            return VIM_FALSE;
        }
    }
    // RDD 151220 Support both old and new CPAT formats. Only change required is Basics card and EG Fuel 27 index need and to be changed to new indeces prior to rollout of this SW

    else
#endif
		if ((terminal.card_name[CpatIndex].CardNameIndex != WISH_GIFT_CARD) &&
      (terminal.card_name[CpatIndex].CardNameIndex != ESSENTIALS_CARD) &&
      (terminal.card_name[CpatIndex].CardNameIndex != GROCERIES_ONLY_CARD) &&  // RDD 030620 Added  as a gift card at Donnas' request Trello #156
      (terminal.card_name[CpatIndex].CardNameIndex != RETURNS_CARD) &&
      (terminal.card_name[CpatIndex].CardNameIndex != WEX_GIFT_AUS) &&
      (terminal.card_name[CpatIndex].CardNameIndex != WEX_GIFT_NZ) &&
      (terminal.card_name[CpatIndex].CardNameIndex != QC_GIFT_AUS) &&
      (terminal.card_name[CpatIndex].CardNameIndex != QC_GIFT_NZ))
    {
        //pceft_debug_test(pceftpos_handle.instance, "Not a Gift Card (old flow)");
        return VIM_FALSE;
    }

        
    // RDD 060412 SPIRA:5240 Wish Gift Card should show Balance as txn type not redemption    
    // RDD 071220 - probably don't need this hack any more ...
    switch (txn.type)
    {
        case tt_gift_card_activation: vim_strcpy(type, "ACTIVATION");
            break;
        case tt_refund: vim_strcpy(type, "REFUND"); break;
        case tt_sale: vim_strcpy(type, "REDEMPTION"); break;
        default: break;
    }
    return VIM_TRUE;
}


VIM_ERROR_PTR icc_is_removed( VIM_ICC_PTR instance, VIM_BOOL * is_removed )
{
    VIM_ERROR_PTR res;
    if(( res = vim_icc_is_removed( icc_handle.instance_ptr, is_removed )) == VIM_ERROR_EMV_CARD_REMOVED )
    {
        *is_removed = VIM_TRUE;
        res = VIM_ERROR_NONE;
    }
	return res;
}

/* ------------------------------------------------------------- */

// RDD 030113 - don't add delays in here because rthis function is called repeatedly during some data entry functions and the
// timeouts can easily get stuffed up !

VIM_ERROR_PTR EmvCardRemoved( VIM_BOOL GenerateError )
{
	VIM_BOOL icc_removed;

	if(( txn.card.type != card_chip ) || ( txn.status & st_pos_sent_pci_pan ))
	{
		DBG_VAR(txn.status);
		return VIM_ERROR_NONE;
	}
	// Is a contact EMV txn so the card should be inserted
	if( icc_handle.instance_ptr==VIM_NULL )
	{
		DBG_MESSAGE( "!!! ERROR: NOHANDLE TO THE CARD - creating new....!!!");
		VIM_ERROR_RETURN_ON_FAIL( vim_icc_new( &icc_handle, VIM_ICC_SLOT_CUSTOMER ));
	}

	VIM_ERROR_RETURN_ON_FAIL( icc_is_removed(icc_handle.instance_ptr, &icc_removed));
	if (icc_removed)
	{
		DBG_MESSAGE( "!!! ERROR : CARD WAS REMOVED !!!");
		// RDD 200412 SPIRA:5228 don't overwrite power failure error code with card removed if card is left in during power cycle
		if(( txn.error != ERR_POWER_FAIL ) && ( GenerateError ))
		{
            if( FullEmv(&txn) )
            {
                pceft_debug(pceftpos_handle.instance, "Trello #272 - full emv card removed");
            }
            else
            {
                pceft_debug(pceftpos_handle.instance, "Trello #272 - partial emv card removed");
            }
            txn.error = VIM_ERROR_EMV_CARD_REMOVED;
        }
		else
		{
			DBG_MESSAGE(" !!! EMV CARD WAS REMOVED. DO NOT SET TXN ERROR !!!" );
		}
		return VIM_ERROR_EMV_CARD_REMOVED;
	}
	else
	{
		//
		//vim_event_sleep( 50 );
		return VIM_ERROR_NONE;
	}
}

/* ------------------------------------------------------------- */
/*
	This is always called for txn.type==tt_preswipe.

*/

VIM_ERROR_PTR screen_kbd_touch_read_with_abort( VIM_KBD_CODE *key_pressed, VIM_MILLISECONDS timeout, VIM_BOOL reset_loop, VIM_BOOL touch_active )
{
	VIM_RTC_UPTIME now, end_time;
	VIM_BOOL default_event=VIM_FALSE;
	VIM_EVENT_FLAG_PTR pceftpos_event_ptr=&default_event;
    VIM_EVENT_FLAG_PTR event_list[1];
    VIM_SIZE EventCount = 0;
    event_list[0] = VIM_NULL;

	vim_rtc_get_uptime(&now);
	end_time = now + timeout;
	//event_list[0] = VIM_NULL;

	if( IS_INTEGRATED )
	{
		VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceftpos_event_ptr));
		event_list[0] = pceftpos_event_ptr;
        EventCount = 1;
	}

	do
	{
        //VIM_BOOL default_event = VIM_FALSE;
        VIM_ERROR_RETURN_ON_FAIL(vim_screen_kbd_touch_or_other_read(key_pressed, timeout, event_list, EventCount, VIM_TRUE ));
        VIM_DBG_NUMBER(*key_pressed);
        VIM_DBG_POINT;

		if( IS_INTEGRATED )
		{
			if(( pceft_pos_command_check( &txn, 1 )) ==  VIM_ERROR_POS_CANCEL )
				return VIM_ERROR_POS_CANCEL;
		}

		if( terminal.abort_transaction )
		{
			DBG_MESSAGE( "!!! ERROR : CANCEL DUE TO TERMINAL ABORT FLAG SET !!!");
			return VIM_ERROR_POS_CANCEL;
		}

		vim_rtc_get_uptime(&now);
		// RDD WOW: there is a long bleep each time the normal data entry timeout is reached
		if( now > end_time )
		{
		  // RDD - some pre-swipe entry functions just wait forever and bleep on timeout
		  if( reset_loop )
		  {
			  // RDD 040714 Removed this for ALH because it was annoying
			  //	vim_kbd_sound_invalid_key();
			  // reset the loop
			  end_time = now + timeout;
		  }
		  else
		  {
			  DBG_MESSAGE( "!!! ERROR : Timeout !!!");
			  return VIM_ERROR_TIMEOUT;
		  }
		}
	}
	while( *key_pressed == VIM_KBD_CODE_NONE );
	DBG_VAR ( key_pressed );
	DBG_MESSAGE( "!Key Pressed!");

	return VIM_ERROR_NONE;
}


/* ------------------------------------------------------------- */
VIM_BOOL transaction_get_add_status(transaction_t *transaction, VIM_UINT32 status)
{
    if (transaction->add_status & status)
        return VIM_TRUE;
    return VIM_FALSE;
}


/* ------------------------------------------------------------- */
VIM_ERROR_PTR transaction_set_add_status(transaction_t *txn, VIM_UINT32 status)
{
    txn->add_status |= status;
    return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */
VIM_ERROR_PTR transaction_clear_add_status(transaction_t *txn, VIM_UINT32 status)
{
    txn->add_status &= ~status;
    return VIM_ERROR_NONE;
}


/* ------------------------------------------------------------- */
VIM_BOOL transaction_get_status(transaction_t *transaction, VIM_UINT32 status)
{
    if (transaction->status & status)
        return VIM_TRUE;
    return VIM_FALSE;
}


/* ------------------------------------------------------------- */
VIM_ERROR_PTR transaction_set_status(transaction_t *txn, VIM_UINT32 status)
{
  txn->status |= status;
  return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */
VIM_ERROR_PTR transaction_clear_status(transaction_t *txn, VIM_UINT32 status)
{
  txn->status &= ~status;
  return VIM_ERROR_NONE;
}

/* ------------------------------------------------------------- */
VIM_BOOL transaction_get_training_mode(void)
{
	if( txn.status & st_training_mode )
		return VIM_TRUE;
	return terminal.training_mode;
}



/* ------------------------------------------------------------- */
VIM_BOOL txn_is_moto(transaction_t *txn)
{
  if ( txn->card.type == card_manual )
  {
    if ( (txn->card.data.manual.manual_reason == moto_telephone)
      || (txn->card.data.manual.manual_reason == moto_mail)
      || (txn->card.data.manual.manual_reason == moto_customer_present) )
      return VIM_TRUE;
  }

  return VIM_FALSE;
}
/* ------------------------------------------------------------- */
VIM_BOOL txn_is_customer_present(transaction_t *transaction)
{
	// RDD 080422 Not sure why st_post_sent_pci_pan is set for completed preauths, but a quick fix is to ensure completions exit customer present at this point
	if( transaction->type == tt_checkout )
	{
		return VIM_TRUE;
	}

	// RDD 020412 - For WOW "K" completions indicate customer not present
	if( transaction_get_status( transaction, st_pos_sent_pci_pan ) && transaction->card.type == card_manual )
	{
	  return VIM_FALSE;
	}

	// RDD 280814 added for ALH
	if( txn.type == tt_moto )
	{
		return VIM_FALSE;
	}

	// RDD 061112 SPIRA:6257 EPAN was invalid so prompt for PAN
	if( transaction_get_status( &txn, st_operator_entry ))
	{
	  return VIM_FALSE;
	}

    return VIM_TRUE;
}
/* ------------------------------------------------------------- */
VIM_BOOL txn_is_ecom(transaction_t *txn)
{
  if ( txn->card.type == card_manual )
  {
    if ( txn->card.data.manual.manual_reason == moto_internet )
      return VIM_TRUE;
  }
  return VIM_FALSE;
}

/* Save the transaction structure to file */
VIM_ERROR_PTR txn_save(void)
{
  VIM_UINT32 buffer_size;
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];		// RDD 010612 - need to increase size to store fuel card data

  VIM_DBG_WARNING( "Save Transaction" );
  // RDD 060412 SPIRA:5231 Don't save transactions if some terrible error
  // RDD 200213 SPIRA:6593 Don't save during Query card transactions as this can create rubbish last receipt reprints
  if(( txn.error == VIM_ERROR_EMV_FAULTY_CHIP )||( txn.type == tt_query_card ))
  {
	  return VIM_ERROR_NONE;
  }
  if( terminal.training_mode )
  {
	  transaction_set_status( &txn, st_training_mode );
  }
  else
  {
	  transaction_clear_status( &txn, st_training_mode );
  }
  /* convert the the structure to TLV */
  VIM_ERROR_RETURN_ON_FAIL(transaction_serialize(&txn, buffer, &buffer_size, VIM_SIZEOF(buffer)));

  /* write the structure to a file */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set(txn_file_name, buffer, buffer_size ));
  TxnFileSize = buffer_size;

  CheckMemory( VIM_FALSE );

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR txn_load(void)
{
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];
  VIM_SIZE buffer_size;
  VIM_BOOL exists;

  /* set default values for transaction structure */
  vim_mem_clear(&txn, sizeof(txn));

  /* check if there is a saved terminal configuration file */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(txn_file_name,&exists));

  if( exists )
  {
    /* read the TLV encoded transaction from a file */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get(txn_file_name, buffer,&buffer_size, VIM_SIZEOF(buffer)));
    /* convert the TLV encoded transaction to a transaction structure */
    VIM_ERROR_RETURN_ON_FAIL(transaction_deserialize(&txn, buffer, buffer_size, VIM_TRUE));

	CheckMemory( VIM_FALSE );

	/* return without error */
	return VIM_ERROR_NONE;
  }
  else
  {
	  VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
  }
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR txn_duplicate_txn_read
(
  VIM_BOOL duplicate_receipt
)
{
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];
  VIM_SIZE buffer_size = VIM_SIZEOF(buffer);
  VIM_BOOL exists;

  vim_mem_clear(&txn_duplicate, sizeof(txn_duplicate));
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(duplicate_txn_file_name, &exists));

  /* read the TLV encoded transaction from a file */
  if( exists )
  {
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_get( duplicate_txn_file_name, buffer, &buffer_size, VIM_SIZEOF(buffer) ));
	  VIM_ERROR_RETURN_ON_FAIL(transaction_deserialize( &txn_duplicate, buffer, buffer_size, VIM_TRUE ));

	  // RDD 200213 SPIRA:6593 Don't save during Query card transactions as this can create rubbish last receipt reprints
	  if(( txn_duplicate.type == tt_none ) || ( txn_duplicate.amount_total == 0 ))
	  {
		  VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
	  }
	  return VIM_ERROR_NONE;
  }
  else
  {
	  VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
  }
}

VIM_ERROR_PTR txn_duplicate_txn_save(void)
{
  VIM_UINT32 buffer_size;
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];

  // RDD 200412 SPIRA:5268

	  // RDD 200213 SPIRA:6593 Don't save during Query card transactions as this can create rubbish last receipt reprints
  if(( txn.card.type == card_none )||( txn.type == tt_query_card_get_token ) || (txn.type == tt_query_card_deposit)||(txn.type == tt_query_card )) return VIM_ERROR_NONE;

  // RDD 210612 added
  if( terminal.training_mode )
  {
	  txn.status |= st_training_mode;
  }

  /* convert the the structure to TLV */
  transaction_serialize(&txn, buffer, &buffer_size, VIM_SIZEOF(buffer));

  /* write the structure to a file */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set( duplicate_txn_file_name, buffer, buffer_size ));

  // RDD 030420 v582-7 assign txn to duplicate in case not retrieved
  txn_duplicate = txn;
  /* return without error */
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR txn_duplicate_txn_delete(void)
{
  VIM_BOOL exists;

  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(duplicate_txn_file_name, &exists));

  if (exists)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(duplicate_txn_file_name));
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

#define NUMBER_OF_TXN_TYPES 37

sTxnTypeInfo TxnTypeInfo[NUMBER_OF_TXN_TYPES] = {
	//					"123456789012345678",	"1234567890"
	{ tt_none,			"        ",			    "        " },
	{ tt_sale,			"PURCHASE",				"PURCHASE" },
	{ tt_cashout,		"CASH-OUT",				"CASH-OUT" },
	{ tt_sale_cash,		"PUR/CASH",				"PUR/CASH" },
	{ tt_refund,		"REFUND",				"REFUND" },
	{ tt_void,			"VOID",					"VOID" },
	{ tt_manual,		"MANUAL ENTRY",			"MANUAL" },
	{ tt_balance,		"BALANCE",				"BALANCE" },
	{ tt_tip,			"TIP",					"TIP" },
	{ tt_adjust,		"ADJUST",				"ADJUST" },
	{ tt_preauth,		"PRE AUTH",				"PRE AUTH" },
	{ tt_offline,		"OFFLINE",				"OFFLINE" },
	{ tt_completion,	"PURCHASE",				"PURCHASE" },
	{ tt_shift_totals,	"SHIFT TOTALS",			"SHIFT TOT" },
	{ tt_presettle,		"PRE SETTLE",			"P-SETTLE"},
	{ tt_settle,		"SETTLE TOTALS",			"SETTLE"},
	{ tt_last_settle,	"LAST SETTLEMENT",		"L-SETTLE"},
	{ tt_settle_safs,"SAF TOTALS",		    "SAF TOTALS" },
	{ tt_payment,		"PAYMENT",				"PAYMENT"},
	{ tt_query_card,    " ",					" " },
	{ tt_query_card_get_token, "TOKEN LOOKUP", "TKN CHECK" },
	{ tt_query_card_deposit, "DEPOSIT",			"DEPOSIT" },
	{ tt_query_acc,		" ",					" " },
	{ tt_preauth_enquiry,"PRE AUTH ENQ",		"PA ENQ" },
	{ tt_preswipe,		" ",					" " },
	{ tt_deposit,		"DEPOSIT",				"DEPOSIT" },
	{ tt_logon,			"EFT LOGON",			"EFT LOGON" },
	{ tt_tms_logon,		"RTM LOGON",			"RTM LOGON" },
	{ tt_reprint_receipt, "REPRINT",			"REPRINT" },
	{ tt_activate,		"REFUND",				"REFUND" },
	{ tt_other,			"ADMIN MENU",			"ADMIN MENU" },
	{ tt_main_menu,		"MAIN MENU",			"MAIN MENU" },
	{ tt_rsa_logon,		"RSA LOGON",			"RSA LOGON" },
	{ tt_moto,			"MOTO SALE",		"MOTO" },		// RDD 111215 SPIRA:8872
	{ tt_checkout,		"COMPLETION",			"COMPLTN"},
    { tt_moto_refund,   "MOTO REFUND",			"MOTO RFND" },
    { tt_gift_card_activation,   "ACTIVATION",	"ACTIVATE"}      // 051118 RDD JIRA WWBP-363

  };

void get_txn_type_string(transaction_t *transaction, char *type_buf, VIM_UINT32 buf_length, VIM_BOOL txn_void)
{
	VIM_SIZE n;
	vim_mem_clear(type_buf, buf_length);

	for( n=0; n< VIM_LENGTH_OF(TxnTypeInfo); n++ )
	{
		if( TxnTypeInfo[n].type == transaction->type )
		{
            vim_strcpy(type_buf, TxnTypeInfo[n].LabelLong);
            break;
        }
	}
}

void get_txn_type_string_short(transaction_t *transaction, char *type_buf, VIM_UINT32 buf_length)
{
	VIM_SIZE n;
	vim_mem_clear(type_buf, buf_length);

	for( n=0; n< VIM_LENGTH_OF(TxnTypeInfo); n++ )
	{

		if( TxnTypeInfo[n].type == transaction->type )
		{
			vim_strcpy( type_buf, TxnTypeInfo[n].LabelShort );
			break;
		}
	}

}

void get_issuer_name_and_txn_type(transaction_t *transaction, VIM_CHAR* out_string)
{
  VIM_SIZE len;
  VIM_INT16 i;
  VIM_CHAR card_name[WOW_CARD_NAME_MAX_LEN];
  VIM_UINT32 CpatIndex = transaction->card_name_index;
  VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

  if( CpatIndex >= WOW_MAX_CPAT_ENTRIES ) return;

  vim_mem_set(out_string,  ' ',  20);
  if( !vim_strlen(type) )
  {
	get_txn_type_string_short(transaction, type, VIM_SIZEOF(type));
  }
  vim_strcpy(card_name, CardNameTable[CardNameIndex].card_name );

  len = vim_strlen(card_name);
  for(i = len-1; i >= 0 ; i--)
  {
    if( card_name[i] == ' ' )
        card_name[i] = 0;
    else
      break;
  }

  vim_snprintf(out_string, 21, "%s %s", card_name, type);
}


// RDD 300720 Trello #69 and Trello #199 - change default acc to print nothing for sceme debits
void get_acc_type_string_short( account_t account_type, VIM_CHAR *out_string )
{
    VIM_CHAR CardName[50];

    vim_mem_clear(CardName, VIM_SIZEOF(CardName));
    GetCardName(&txn, CardName);
    VIM_DBG_STRING(CardName);

    if (CheckDebitLabel(CardName) && ( txn.account == account_credit ))
    {
        VIM_DBG_MESSAGE("!!!!!!!!!!!!!!!!!!! Change Account to Default because of DEBIT !!!!!!!!!!!!!!!!!!!");
        account_type = txn.account = account_default;
    }


    switch( account_type )
    {
        case account_credit:
		//case account_default:
        vim_strcpy( out_string, "CR" );
        break;

        case account_cheque:
        vim_strcpy( out_string, "CHQ" );
        break;

        case account_savings:
        vim_strcpy( out_string, "SAV" );
        break;

		default:
        case account_default:
        vim_strcpy( out_string, " " );
		break;
    }
	if( txn.type == tt_query_card )
		 vim_strcpy( out_string, " " );

}

static txn_state_t txn_check_passwords(void)
{
  VIM_ERROR_PTR result = VIM_ERROR_NONE;

  if (( txn.type == tt_refund || txn.type == tt_moto_refund ) && (vim_strlen(terminal.configuration.refund_password)) >= WOW_MIN_PASSWORD_LEN )
  {
	  if(( result = password_compare(DISP_ENTER_PASSWORD, terminal.configuration.refund_password, "Enter Refund Password", VIM_FALSE)) != VIM_ERROR_NONE)
	  {
		  TXN_RETURN_ERROR(result);
	  }
  }

  if (transaction_get_status(&txn, st_moto_txn))
  {
	  if (vim_strlen(terminal.configuration.moto_password) >= WOW_MIN_PASSWORD_LEN)
	  {
		  if ((result = password_compare(DISP_ENTER_PASSWORD, terminal.configuration.moto_password, "Enter MOTO Password", VIM_FALSE)) == VIM_ERROR_NONE)
		  {
			  if (IS_STANDALONE)
				  SetReasonCode();
		  }
		  else
		  {
			  TXN_RETURN_ERROR(result);
		  }

	  }
	  // RDD 050522 JIRA PIRPIN-1599
	  else if(IS_STANDALONE)
	  {
		  SetReasonCode();
	  }
  }
  return state_get_next();
}


VIM_BOOL CheckRefundLimits(void)
{
	// RDD JIRA PIRPIN-1566 : ensure no limit checking if not a refund
	if (txn.type != tt_refund)
	{
		return VIM_TRUE;
	}

	// RDD 190123 - last minute change for v735-1 ensure if refund limits not set then they're not used.
    if (((txn.amount_refund > TERM_REFUND_TXN_LIMIT * 100))&&(TERM_REFUND_TXN_LIMIT))
    {
        pceft_debugf(pceftpos_handle.instance, "Rejected : Refund Amount Exceeds Txn Limit");
        display_result(ERR_OVER_REFUND_LIMIT_T, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
        return VIM_FALSE;
    }
    else if (((txn.amount_refund + TERM_REFUND_DAILY_TOTAL > ( TERM_REFUND_DAILY_LIMIT*100 )))&&(TERM_REFUND_DAILY_LIMIT))
    {
        display_result(ERR_OVER_REFUND_LIMIT_D, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
        pceft_debugf(pceftpos_handle.instance, "Rejected : Refund Total would exceed Daily Limit");
        return VIM_FALSE;
    }
    return VIM_TRUE;
}

static txn_state_t txn_check_permissions(void)
{

    if (terminal.acquirers[txn.acquirer_index].settlement_reqd)
        TXN_RETURN_ERROR(ERR_SETTLEMENT_REQD);

    if ((txn.type == tt_preauth) && (!TERM_ALLOW_PREAUTH))
        TXN_RETURN_ERROR(ERR_ACQ_PREAUTH_NOT_ALLOWED);

    if ((txn.type == tt_refund) && (!TERM_REFUND_ENABLE))
        TXN_RETURN_ERROR(ERR_REFUNDS_DISABLED);

    if ((txn.type == tt_adjust) && (!TERM_TIP_ADJUST_ENABLE))
        TXN_RETURN_ERROR(ERR_ADJUSTMENT_DISABLED);

    if ((txn.type == tt_completion) && (!TERM_CHECKOUT_ENABLE))
        /* TODO add checkout disable error code */
        TXN_RETURN_ERROR(ERR_CO_NOT_ALLOWED);

    /* Check if voids allowed */
    if ((txn.type == tt_void) && (!TERM_VOID_ENABLE))
        TXN_RETURN_ERROR(ERR_VOID_DISABLED);

    /* Check if manual pan allowed */
    if (( transaction_get_status( &txn, st_moto_txn ) && (!TERM_ALLOW_MOTO_ECOM)))
        TXN_RETURN_ERROR(ERR_MOTO_ECOM_DISABLED);
    
    /* Check if cashout allowed */
    if ((txn.type == tt_cashout || txn.type == tt_sale_cash) && (!TERM_CASHOUT_ENABLE))
        TXN_RETURN_ERROR(ERR_CASHOUT_DISABLED);

	return state_get_next();
}

static txn_state_t txn_check_limits(void)
{
	DisplayProcessing(DISP_PROCESSING_WAIT);

	// RDD 270722 - don't check refund limits on internals
	if(( txn.type == tt_refund )&&( !IS_R10_POS ))
	{
		if (!CheckRefundLimits())
		{			
			TXN_RETURN_ERROR(ERR_OVER_REFUND_LIMIT_T);		
		}
	}
	if ((txn.type == tt_cashout) || (txn.type == tt_sale_cash))
	{
		if (txn.amount_cash > (100 * TERM_CASHOUT_MAX))
		{
			TXN_RETURN_ERROR(ERR_CASH_OVER_MAX);
		}
	}
	return state_get_next();
}



VIM_ERROR_PTR wow_form_ePan(void)
{
    VIM_CHAR pan[20];
    VIM_TEXT exp_mmyy[5] = {0};
    VIM_ERROR_PTR res;

    vim_mem_set(pan, 0, VIM_SIZEOF(pan));

    VIM_ERROR_RETURN_ON_FAIL( card_get_pan( &txn.card, pan ));
    VIM_ERROR_RETURN_ON_FAIL( card_get_expiry( &txn.card, exp_mmyy ));

    vim_mem_set(txn.card.pci_data.ePan, 0, VIM_SIZEOF(txn.card.pci_data.ePan));

	  //DBG_DATA( exp_mmyy, 5 );
	  //DBG_DATA( pan, 20 );
	  //DBG_DATA( tcu_handle.instance_ptr, 8 );
	  //DBG_DATA( terminal.terminal_id, 8 );
	  //DBG_DATA( txn.card.pci_data.ePan, 24 );

    if(( res = vim_tcu_wow_process_epan( tcu_handle.instance_ptr, terminal.terminal_id, pan, exp_mmyy, txn.card.pci_data.ePan, VIM_FALSE )) != VIM_ERROR_NONE )
	{
		//VIM_DBG_ERROR(res);
	}
	//DBG_DATA( txn.card.pci_data.ePan, 25 );
    return res;
}


///////////////////////////////////// STATE: TXN_CHECK_TMS_LOGON ///////////////////////////////////////////
// This state does pre-transaction processing of any pending logon, reversal or repeat SAF ( 221 )
////////////////////////////////////////////////////////////////////////////////////////////////////////////

static txn_state_t txn_check_tms_logon(void)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;

  /* TODO - check to see if terminal is configured */
#if 0
  if( terminal.training_mode )
  {
	  return state_skip_state();
  }
#endif
  // RDD 210312 - Fix for lane change issue where Txns not allowed until logon performed
  // DBG_MESSAGE( "<<<<<<<< TXN STATE CHECK LFD/LOGON >>>>>>>>>>");

  // RDD 020812 SPIRA:5827 if any files missing then exit "Logon required"
  // RDD 010716 SPIRA:9006 Delete EPAT and do txn results in sucessful txn
#if 1
  if( !CheckFiles( VIM_TRUE )  )
  {
	  pceft_debug( pceftpos_handle.instance, "ONE OR MORE FILES COULD NOT BE OPENED" );

	  DBG_MESSAGE( "!!!!!!! DANGER - ONE OR MORE FILES COULD NOT BE OPENED !!!!!!!!" );
	  res = ERR_NOT_LOGGED_ON;
  }
  else
#endif
  {
	  res = ReversalAndSAFProcessing( SEND_NO_SAF, VIM_FALSE, VIM_TRUE, VIM_FALSE );


	  // Done, so do next state
	  // ensure any response code from other transactions is erased

	  // RDD 060813 SPIRA:6707 MO System error was caused by mismatch due to missing de48 when 96 sent back
	  if(( !vim_strncmp( txn.host_rc_asc, "96", 2 ) && ( res != ERR_WOW_ERC )))
		vim_strcpy( txn.host_rc_asc, "" );
  }
  // RDD 020412 Need to return with Error to embedded logons if file download pending
  if(( res != VIM_ERROR_NONE ) && ( res != ERR_LOGON_SUCCESSFUL ))
  {
	  if (IS_STANDALONE)
	  {
		  TXN_RETURN_ERROR(res);
	  }
	  else
	  {
		  TXN_RETURN_EXIT(res);
	  }
  }
  return state_get_next();
}

// RDD 100812 SPIRA:5871 modified to return timeout error etc.
VIM_ERROR_PTR ChipDamaged( VIM_BOOL *Damaged )
{
	VIM_ERROR_PTR key_err = VIM_ERROR_NONE;
	VIM_PCEFTPOS_KEYMASK key_code;

    vim_event_sleep(2000);

    // RDD 220819 JIRA WWBP-691 Need Remove Card before checking chip
    VIM_ERROR_RETURN_ON_FAIL( emv_remove_card(" "));

	// RDD 170412 SPIRA:5153 Need second error beep when tech fallback qn is asked...
	vim_kbd_sound_invalid_key ();

	if( is_integrated_mode() )
	{
		display_screen( DISP_TECHNICAL_FALLBACK,
					VIM_PCEFTPOS_KEY_YES|VIM_PCEFTPOS_KEY_NO,
					type,
					AmountString(txn.amount_total) );

		// wait for a response from the operator
		if(( key_err = pceft_pos_wait_key( &key_code, GET_YES_NO_TIMEOUT )) == VIM_ERROR_NONE )
		{
			// return TRUE only if YES was pressed by the operator
			*Damaged = ( key_code == VIM_PCEFTPOS_KEY_YES ) ? VIM_TRUE : VIM_FALSE;
		}
	}
	else
	{
		*Damaged = config_get_yes_no( DISP_STR_TECH_FALLBACK, VIM_NULL );
		key_err = VIM_ERROR_NONE;
	}
	return key_err;
}

VIM_BOOL PrintBalance( void )
{
	//VIM_KBD_CODE key_code;
	//VIM_BOOL PrintBal = VIM_FALSE;

    // RDD 010420 - v582-7 Only Print the balance if specifically allowed by LFD
    if (!TERM_ALLOW_BALANCE_RECEIPT)
    {
        return VIM_FALSE;
    }

	if( txn.type != tt_balance ) return VIM_FALSE;

	if( !TxnWasApproved( txn.host_rc_asc ) ) return VIM_FALSE;

	return ( config_get_yes_no( "Balance Enquiry\nPrint Receipt?", VIM_NULL ));
}



// RDD 070212 - Create a function to control Tecnical Fallback

static txn_state_t TechnicalFallback(void)
{
	txn_state_t next_state = ts_card_capture;
	VIM_BOOL DamagedChip = VIM_TRUE;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
#ifdef _WIN32
	txn.card_state = card_capture_icc_retry;
#endif
	DBG_VAR(txn.card_state);
	DBG_STRING( "Technical Fallback" );

 
	switch( txn.card_state )
	{
		// RDD: Retry capture state already in retry check damaged chip
		//case card_capture_no_picc:
		case card_capture_standard_retry:
		case card_capture_icc_retry:

            // RDD 260619 replace this with a loop so no exit if card removed ( to inspect chip )
			// JIRA PIRPIN-1219 don't allow technical fallback if there is any cash component			
			// RDD 130922 JIRA PIRPIN-1811 Exit if Cash component
			if (txn.amount_cash)
			{
				DBG_POINT;
				txn.error = ERR_INVALID_TRACK2;
				return ts_finalisation;
			}			
			do
            {
				DBG_POINT;
                res = ChipDamaged(&DamagedChip);
                VIM_DBG_ERROR(res);
            }
            while ( res == VIM_ERROR_EMV_CARD_REMOVED );

			if( DamagedChip )
			{
				// RDD 050412 SPIRA 5231 PinPad Hangs up if chip damaged on tech fallback - soln. don't finalise, just exit.
				//next_state = ts_exit;
				next_state = ts_finalisation;

                // Reason code - cancelled due to damaged chip
                vim_mem_copy(txn.cnp_error, "CDC", 2);

				txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
				DBG_POINT;
			}
			// RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS

			else 
			{
				txn.error = VIM_ERROR_NONE;
				emv_remove_card( "" );
				// Goto forced swipe. ESC checking will be bypassed automatically
                // Reason code - fallback to swipe due to technical error
                vim_mem_copy(txn.cnp_error, "FST", 2);
				transaction_set_status(&txn, st_technical_fallback);
                txn.card_state = card_capture_forced_swipe;
				DBG_POINT;
			}

            if( res != VIM_ERROR_NONE )
            {
                TXN_RETURN_ERROR(res);
            }

            break;

		default :
		txn.card_state = card_capture_standard_retry;
		break;

	}
	DBG_VAR(txn.card_state);
	return next_state;
}


// RDD 190719 WWBP-659 ADVT5 App 3 should decline because of expired card and IACS Denial - ensure this works irresepective of error code

#ifdef _DEBUG
void FakeIACDFailure(VIM_TLV_LIST *TagList )
{
    const VIM_UINT8 FakeIAD[5] = { 0x00, 0x40, 0x00, 0x00, 0x00 };
    vim_tlv_update_bytes(TagList, VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL, FakeIAD, 5);
}
#endif

VIM_BOOL IAC_Denial_Decline(void)
{
    VIM_TLV_LIST tag_list;
    VIM_BOOL retval = VIM_FALSE;
    VIM_UINT8 TVRCopy[5], IACDCopy[5];

    vim_mem_clear(TVRCopy, VIM_SIZEOF(TVRCopy));
    vim_mem_clear(IACDCopy, VIM_SIZEOF(IACDCopy));

    if( VIM_ERROR_NONE == vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, sizeof(txn.emv_data.buffer), VIM_FALSE))
    {
        // Grab the TVR
        if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, VIM_TAG_EMV_TVR ))
        {
            vim_mem_copy( TVRCopy, tag_list.current_item.value, 5);
            VIM_DBG_DATA(TVRCopy, 5);

#ifdef _DEBUG
            // RDD create a fake IACD to cause Z1 on any expired application - test this way cos don't have real card !
            //FakeIACDFailure(&tag_list);
#endif

            // Grab the IAC Denial
            if (VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL))
            {
                VIM_UINT8 i;
                vim_mem_copy(IACDCopy, tag_list.current_item.value, 5);

                VIM_DBG_DATA(IACDCopy, 5);
                VIM_DBG_WARNING("<<<<<<<<<< CHECK IAC Denial AGAINST TVR >>>>>>>>>>>>>");

                for( i = 0; (i < 5); i++ )
                {
                    // Any single bit matching should cause Z1
                    if( IACDCopy[i] & TVRCopy[i] )
                    {
                        VIM_DBG_WARNING("---- IACD Match against TVR - Z1 ----");
                        txn.error = ERR_EMV_CARD_DECLINED;
                        retval = VIM_TRUE;
                        break;
                    }
                }
            }
            else
            {
                VIM_DBG_WARNING("<<<<<<<<<< NO IACD FOUND >>>>>>>>>>>>>");
            }

        }
        else
        {
            VIM_DBG_WARNING("<<<<<<<<<< NO TVR FOUND >>>>>>>>>>>>>");
        }
    }
    DBG_VAR(retval);
    return retval;
}




// RDD 070212 - Note that this can be called effectively from anywhere eg. after Auth, after Pin Entry or even after finalisation

static txn_state_t ProcessCaptureResult_contact(VIM_ERROR_PTR res)
{

  DBG_MESSAGE( "<<<<<<<< PROCESS CAPTURE RESULT FOR CONTACT >>>>>>>>>>");
  VIM_DBG_ERROR( res );
  txn.emv_error = res;

  // RDD 230922 Georgia complaining about endless cards reads allowed if finished second docked read then exit with an error
  DBG_VAR(txn.read_icc_count);
  if (txn.read_icc_count > 2)
  {
	  // Donna wants this error if no TFB 
	  txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
	  DBG_POINT;
	  return ts_finalisation;
  }

  // RDD 310322 - ensure enter RRN state is skipped at this point if docked
  if(txn.type == tt_checkout)
  {
	  DBG_MESSAGE("SSSSSSSSSSS Skip RRN entry SSSSSSSSSSSSS");
	  transaction_set_status(&txn, st_pos_sent_pci_pan);
  }


  // RDD 150920 - check for card removal first as this trumps some other errors
  if (EmvCardRemoved(VIM_TRUE) == VIM_ERROR_EMV_CARD_REMOVED)
  {
      display_result(VIM_ERROR_EMV_CARD_REMOVED, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
      // RDD 211020 JIRA PIRPIN-905 : Fix for small Window for no reversal ( and resulting double debit ) if card removed after bank response. V. hard to reproduce but logs show several instances/day
      if (transaction_get_status(&txn, st_message_sent))
      {
          pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-905");
          reversal_set(&txn, VIM_FALSE);
          return ts_finalisation;
      }
      else
      {
          // RDD 121121 v729-0 remove this as can cause issues with surcharging
          return ts_finalisation;
          //return ts_card_capture;
      }
  }

  if (txn.account != account_credit && txn.error == ERR_INVALID_ACCOUNT)
  {
      return ts_card_capture;
  }


  // RDD 030316 Kernel 7.0 Shuts down the CRYPTO Driver so need to reopen
  if( txn.emv_pin_entry_params.offline_pin )
  {
	    DBG_MESSAGE( " EMV Kernel 7.0 - Reopen CRYPTO Driver " );
		//VIM_ERROR_RETURN_ON_FAIL( vim_tcu_delete( tcu_handle.instance_ptr ));
		VIM_ERROR_WARN_ON_FAIL( vim_tcu_renew_handle( &tcu_handle ));
  }


  // RDD 160712 SPIRA:5686 Training mode 62 cents -> Technical Fallback
  if(( transaction_get_training_mode() ) && ( txn.amount_purchase%100 == 62 ))
  {
	  res = VIM_ERROR_EMV_FAULTY_CHIP;
  }


  // RDD 190719 WWBP-659 ADVT5 App 3 should decline because of expired card and IACS Denial - ensure this works irresepective of error code
  if( IAC_Denial_Decline() && terminal_get_status( ss_wow_basic_test_mode ) )
  {
      vim_strcpy(txn.cnp_error, "CIX");
      return ts_finalisation;
  }


  // RDD 040112 - Card Removal Processing
  if( res == VIM_ERROR_EMV_CARD_REMOVED )
  {
	  // RDD 131212 SPIRA:6454 if this error came from the kernel it wasn't being translated to txn.error
	  txn.error = res;
	  // RDD 160202 Bugfix : Need to display "CANCELLED CARD REMOVED on PinPad and POS"
	  // display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );

	  // Card was removed after message sent to host
#if 0 // RDD 301014 SPIRA:8184 If card removed after "OK to Continue" on Sig Only card then no pin block size but still need to abort txn with declined receipt
	  if(( reversal_present() ) || ( txn.pin_block_size ))
#else
      // RDD Trello #184 Trello #185 - Some EMV issue is causing receipts not to be printed - if message was sent then ensure receipt printed even if no reversal
	  //if(( reversal_present() ) || ( txn.pin_block_size ) || ( txn.print_receipt ))
      if(( reversal_present() ) || ( txn.pin_block_size ) || ( txn.print_receipt ) || (transaction_get_status(&txn, st_message_sent)))
#endif
	  {
		  DBG_VAR( txn.pin_block_size );
		  // DBG_MESSAGE( "**** CARD WAS REMOVED AFTER PIN ENTERED: ABORT ****" );
		  // RDD 150714 v430 SPIRA:7186 - want to display card removed for partial emv too
          vim_strcpy(txn.cnp_error, "CRX");

			return ts_finalisation;
	  }
	  else
	  {
		  // DBG_MESSAGE( "**** CARD WAS REMOVED BEFORE REVERSAL SET: RETRY ****" );
		  // Reset the card and account type to unknown as this can affect Capture method
		  // Customer may wish to change cards

		  // RDD 160202 Bugfix : Need to display "CANCELLED CARD REMOVED on PinPad and POS"
		  display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );

		  txn.card.type = card_none;
		  txn.account = account_unknown_any;
		  return ts_card_capture;
	  }
  }

  // RDD 120413 SPIRA: 6647 - ADVT29 ( VIM_ERROR_CARD_BLOCKED ) needs to return to present card as do some other errors
  if(( res == VIM_ERROR_EMV_APPLICATION_BLOCKED ) || ( res == VIM_ERROR_EMV_CARD_BLOCKED ) || ( res == VIM_ERROR_EMV_6985_ON_GENAC1 ))
  {
	  // RDD 160202 Bugfix : Need to display "CARD REJECTED CONTACT ISSUER on PinPad and POS"
	  display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY );
	  return ts_card_capture;
  }

  // RDD 121212 Added for SPIRA:6437 v305 CR
  if( res == ERR_CPAT_NO_DOCK )
  {
      vim_mem_copy(txn.cnp_error, "FSC", 2);

	  txn.card_state = card_capture_forced_swipe;
	  DBG_POINT;
	  return ts_card_capture;
  }

  // RDD - Partial EMV txns have been finished at this point ???
  // RDD 260914 v434_2 completions were asking for second sig because going into Finalisation state twice.
#if 1
  if( res == VIM_ERROR_EMV_PARTIAL_TXN )
  {
	  // RDD 100515 SPIRA:8565  -standalone checkouts bomb out earlier so need the finalisation stage. Also check for expired cards
	  if( is_standalone_mode() )
	  {

		  if( txn.type == tt_checkout )
		  {
			DBG_POINT;
			//transaction_set_status( &txn, st_incomplete );
			terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855

			//return ts_finalisation;
			// RDD 161115 v547-3
			return ts_get_approval_code;
		  }
		  // RDD 290216 v548-9 Need to cater for partial emv when not checkout ( eg refund )
		  else
		  {

			DBG_POINT;
			return ts_get_pin;	// RDD 201211 bugfix
		  }
	  }
	  else if( is_integrated_mode() )
	  {

#if 1	// RDD 161015 SPIRA:8829 Deposit Query card was broken by Partial EMV -> ctls repeat fix ( SPIRA 8822 )
		if( txn.type == tt_query_card_deposit )
		{
			DBG_MESSAGE( "<--------- Fix SPIRA 8829 ------------->" );
			return ts_exit;
		}
#endif

		if(( txn.type == tt_query_card_get_token )||( txn.type == tt_deposit ))
		{
		  DBG_POINT;
		  return ts_authorisation_processing;	// RDD 201211 bugfix
		}

		else
		{
			DBG_POINT;
			// RDD 301015 SPIRA:8831 - docked completion exits and here and needs to go to PIN entry state
			return ts_get_pin;	// RDD 201211 bugfix
		}
	  }
  }
  // RDD 061114 SPIRA:8245 EFB Approved Full EMV txns have ERR_SIGNATURE_APPROVED_OFFLINE and we need these to goto finalisation
  if(( res == ERR_SIGNATURE_APPROVED_OFFLINE ) && ( txn.type == tt_checkout ))
  {
	  DBG_POINT;
	  return ts_exit;
  }
#else
  if(( res == VIM_ERROR_EMV_PARTIAL_TXN )||( res == ERR_SIGNATURE_APPROVED_OFFLINE ))
  {
	  DBG_POINT;
	  return ts_exit;
  }
#endif

  //if( res == VIM_ERROR_NONE )
  //  return ts_finalisation;

  // RDD 141212 - this is the catch all for any kernel error if txn.error is not already set up.
  // an error is passed back and no txn error is recorded then the txn error is set to the emv error. If there is a kernel error already set then map the kernel error to res.
  if( (res != VIM_ERROR_NONE) && (txn.error == VIM_ERROR_NONE || txn.error == VIM_ERROR_EMV_KERNEL_ERROR) )
  {
	  DBG_POINT;
	  txn.error = res;
  }

  // RDD 070212 - Mute card Goes to Tecnical Fallback Handing and the state is set from there
  if( res == VIM_ERROR_EMV_BAD_CARD_RESET  ||  res == VIM_ERROR_EMV_FAULTY_CHIP )
  {
	  // RDD 080413 Stats for Phase4
	  DBG_POINT;
	  terminal.stats.chip_misreads++;
	  terminal.stats.chip_read_percentage_fail = ( terminal.stats.chip_misreads * 100 )/(terminal.stats.chip_good_reads+terminal.stats.chip_misreads);
	  // RDD 260412: Card removal during Second GenAc can result in card read error which is really due to card removal
      if ((EmvCardRemoved(VIM_TRUE) == VIM_ERROR_EMV_CARD_REMOVED) && (txn.status & st_message_sent))
      {
          vim_strcpy(txn.cnp_error, "CRY");
          return ts_finalisation;
      }

	  if( terminal.training_mode )
	  {
		display_screen( DISP_CARD_READ_ERROR, VIM_PCEFTPOS_KEY_NONE, "(Simulated)" );
	  }
	  else
	  {
		display_screen( DISP_CARD_READ_ERROR, VIM_PCEFTPOS_KEY_NONE, " " );
	  }

	  // RDD 220312 SPIRA 5153 - Error bleep for card read errors
	  vim_kbd_sound_invalid_key ();
	  vim_event_sleep( 2000 );
	  // JIRA PIRPIN-1219 don't allow technical fallback if there is any cash component
	  if(( txn.type == tt_query_card )||( txn.amount_cash ))
		  return ts_card_capture;

	  // RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS
	  else if (TERM_TECHNICAL_FALLBACK_ENABLE)
	  {
		  pceft_debug(pceftpos_handle.instance, "TMS Allows TFB");
		  return TechnicalFallback();
	  }
	  else
	  {
		  pceft_debug(pceftpos_handle.instance, "TMS Forbids TFB");
		  return ts_card_capture;
	  }
  }
  else
  {
	  // RDD 080413 Stats for Phase4
	  terminal.stats.chip_good_reads++;
	  terminal.stats.chip_read_percentage_fail = ( terminal.stats.chip_misreads * 100 )/(terminal.stats.chip_good_reads+terminal.stats.chip_misreads);
  }


  if(( res == VIM_ERROR_EMV_NO_MATCHING_APP )||( res == VIM_ERROR_EMV_INVALID_USAGE ))
  {
	  DBG_POINT;
	  // RDD 010312 SPIRA 4884 : Handle 6985 (Proc Opt Stage) Error correctly
	  if( /*res == VIM_ERROR_EMV_INVALID_USAGE */1 )
	  {
		  // RDD 070313 - need to set scheme fallback flag for 6985 Errors too, otherwise 621 will be incrrrectly set in POSE
		  DBG_MESSAGE( ">>>>>>>>>>>> SET SCHEME FALLBACK <<<<<<<<<<<<<<<<" );
		  transaction_set_status( &txn, st_scheme_fallback );
		  display_screen( DISP_TXN_NOT_ACCEPTED, VIM_PCEFTPOS_KEY_NONE );
		  // DBG_MESSAGE( "!!!! 6985 Error !!!!" );
		  vim_event_sleep( 2000 );
	  }

	  /* TODO: display card must be swiped */
	  vim_kbd_sound_invalid_key ();

	  display_screen( DISP_CARD_MUST_BE_SWIPED, VIM_PCEFTPOS_KEY_NONE );
      vim_mem_copy(txn.cnp_error, "FSS", 2);

	  vim_event_sleep( 2000 );
	  txn.card_state = card_capture_forced_swipe;
	  return ts_card_capture;
  }
  txn.error = res;


  // RDD 051112 SPIRA:6256
  // RDD 180213 SPIRA:6607 User Cancel during Offline PIN entry causes prompt for card to presented again
  VIM_DBG_ERROR(res);
  if(( res == VIM_ERROR_EMV_TXN_ABORTED )||( res == ERR_CARD_EXPIRED_DATE )||( res == VIM_ERROR_USER_CANCEL ))
  {
	  VIM_DBG_ERROR( txn.error );
	  VIM_DBG_STRING( txn.host_rc_asc );
	  VIM_DBG_STRING( txn.host_rc_asc_backup );
	  // RDD 161015 v549-2 SPIRA:8085 Product Restrictions not working for Docked cards
	  if( !vim_strncmp( txn.host_rc_asc, "RI", 2 ))
	  {
		  txn.error = ERR_WOW_ERC;
          vim_strcpy(txn.cnp_error, "CDR");

		  display_result( txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
		  return ts_exit;
	  }
      // RDD 170819 JIRA WWBP-659 ADVT 5 returns this error and as unknown causes return to card entry state when should Z1
      // RDD 300920 Trello #131 - if docked txn was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
      else if (res == VIM_ERROR_EMV_TXN_ABORTED)
      {
          VIM_DBG_ERROR(txn.error);
          if (txn.error == VIM_ERROR_NONE)
          {
              DBG_MESSAGE("JIRA WWBP-659 : EMV Kernel returned Aborted prior to online so Z1 in these cases");
              txn.error = ERR_EMV_CARD_DECLINED;
              vim_strcpy(txn.cnp_error, "CDZ");
          }
          else
          {
              DBG_MESSAGE("Txn was aborted due to previous error");
          }
          return ts_finalisation;
      }

	  DBG_POINT;
	  // RDD 100715 - display only for offline PIN as done separatly for online pin elsewhere...
	  if( transaction_get_status( &txn, st_offline_pin ))
	  {
		// RDD 090715 SPIRA:8784 - contact returns to capture state so make everything else do so too
		display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
	  }
	  return ts_card_capture;
  }


  if(( res == VIM_ERROR_INVALID_DATA )||( res == VIM_ERROR_EMV_KERNEL_ERROR ))
  {
      if( EmvCardRemoved( VIM_TRUE ) != VIM_ERROR_EMV_CARD_REMOVED )
      {
          display_screen( DISP_CARD_DATA_ERROR, VIM_PCEFTPOS_KEY_NONE, " " );
          vim_kbd_sound_invalid_key();
          vim_event_sleep(2000);
          txn.card_state = card_capture_forced_swipe;
          vim_mem_copy(txn.cnp_error, "FSD", 2);

          return ts_card_capture;
      }
      else
      {
          // Full EMV force end transaction .
          pceft_debug_test(pceftpos_handle.instance, "Invalid data -> Card Removed");
      }
  }


  if( terminal.training_mode )
  {
	  // DBG_MESSAGE( "!!!!!! TRAINING MODE CONTACT TXN !!!!!!!");
	  return (state_get_next());
  }

  /* default return to next state */
  DBG_MESSAGE( "-------- Go To Finalisation ----------" );
  return ts_finalisation;
}

static txn_state_t ProcessCaptureResult_contactless( VIM_ERROR_PTR res )
{
  DBG_MESSAGE( "<<<<<<<< PROCESS CAPTURE RESULT FOR PICC >>>>>>>>>>");

  txn.error = res;
  VIM_DBG_ERROR( res );

  if( res == VIM_ERROR_NONE )
  {
	  terminal.stats.contactless_approves++;
	  return state_get_next();
  }

  // RDD 090620 Trello #112 - tight loop for CTLS error - GR says allow CTLS not fallback
  if (res == VIM_ERROR_MISMATCH)
  {
      display_screen(DISP_CARD_READ_ERROR, VIM_PCEFTPOS_KEY_NONE, " ");
      vim_kbd_sound_invalid_key();
      vim_event_sleep(1500);
      return ts_card_capture;
  }


  // RDD 310719 JIRA WWBP-643 UPI Card 18a should repeat not fallback
  if (res == VIM_ERROR_PICC_RETRY)
  {
      vim_kbd_sound_invalid_key();
      vim_adkgui_request_to_update("Label", "Refer to phone", "Label2", "");
      vim_event_sleep(5000);
      return ts_card_capture;
  }


  if( res == VIM_ERROR_INVALID_DATA ||
      res == VIM_ERROR_PICC_FALLBACK||
	  res == VIM_ERROR_EMV_CARD_ERROR )
  {
	  terminal.stats.contactless_fallbacks++;
      VIM_DBG_MESSAGE("------------- Read Error ------------- ");
      if(TERM_CTLS_ONLY)
      {
          vim_adkgui_request_to_update("Label", "Card Read Error", "Label2", "Please Try Again");
          vim_event_sleep(1000);
          return ts_card_capture;
      }
      vim_strcpy(txn.cnp_error, "FDR");


	  txn.card_state = card_capture_no_picc;
	  return ts_card_capture;
  }

  if( res == VIM_ERROR_TIMEOUT ) return ts_exit;

  // RDD 150312: SPIRA 5102 Issues with Contactless Z1 resceipts and Advices - CPAT index needs to be set up for advice and SAF Print
  if( res == VIM_ERROR_PICC_CARD_DECLINED )
  {
	  terminal.stats.contactless_declines++;
	  // RDD 150312 : If the card data is invalid for some reason then we cannot treat this as an advice because the host will not respond to a 220 which is missing DE35
	  if(( card_name_search( &txn, &txn.card ) != VIM_ERROR_NONE )||(txn.card.data.picc.track2_length == 0))
	  {
          vim_strcpy(txn.cnp_error, "FDX");
		  txn.card_state = card_capture_no_picc;
		  return ts_card_capture;
	  }
	  else
	  {
		  // RDD need to read the tags as Z1 failures are sent up to the host in the SAF and PinPad will now crash if there is no tag data.
		  picc_tags();
	  }
  }
  // All other errors or no errors print receipt etc.
  txn.print_receipt = VIM_TRUE;
  /* try to get tags from picc */
#ifdef _OLD_PICC_TAGS
  picc_tags ();
#endif

  return ts_finalisation;
}


txn_state_t ProcessCaptureResult( VIM_ERROR_PTR res, card_type_t card_type )
{
	VIM_DBG_ERROR( res );
	VIM_DBG_ERROR( txn.error );
	DBG_VAR( card_type );

    // RDD 060121 JIRA PIRPIN-969 comment this out 
    if(( txn.error == ERR_HOST_APPROVED_CARD_DECLINED )||( txn.error == ERR_UNABLE_TO_GO_ONLINE_DECLINED ))
    {
        res = txn.error;
    }

	if(( txn.error == ERR_NO_RESPONSE ) && ( txn.type == tt_preauth ))
	{
		res = txn.error;
		return state_set_state( ts_finalisation );
	}

	// RDD 310714 added for ALH
	if( is_standalone_mode() && ( res == VIM_ERROR_USER_CANCEL ))
	{
		txn.error = res;
		return state_set_state( ts_finalisation );
	}

	DBG_VAR( txn.type );
	VIM_DBG_NUMBER( txn.roc );
	DBG_STRING( txn.auth_no );
	// RDD 100515 SPIRA:8565  -standalone checkouts bomb out earlier so need the finalisation stage. Also check for expired cards
	if(( IS_STANDALONE ) && ( txn.type == tt_checkout ) && ( !ValidateAuthCode( txn.auth_no ) ) && ( txn.error == ERR_CARD_EXPIRED ))
	{
		DBG_POINT;
		//transaction_set_status( &txn, st_incomplete );
		terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855

		return ts_finalisation;
	}

	if( txn.type == tt_query_card )
	{
		txn.error = res;
		// RDD 120517 added for PassWallet
        if ((res == VIM_ERROR_POS_CANCEL) || (res == VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED && IsSlaveMode()))
        {
            VIM_DBG_WARNING("------- Exit J8 due to POS Cancel -----");
			//VIM_DBG_SEND_TARGETED_UDP_LOGS("------- Exit J8 due to POS Cancel -----");
            //vim_vas_close(VIM_TRUE);
            //display_screen_cust_pcx(DISP_IMAGES, "Label", WOW_ERROR_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
            return state_set_state(ts_exit);
        }

		if(( res == VIM_ERROR_MISMATCH )|| ( res == VIM_ERROR_NOT_FOUND ))
		{
			if( terminal.abort_transaction )
			{
				terminal.abort_transaction = VIM_FALSE;
//				vim_picc_emv_close_apdu_interface_mode(picc_handle.instance);
				return state_set_state( ts_exit );
			}
			// RDD 190517 - handle issue with cycling error and prompt if J8 interrupted by Txn
			else
			{
				//display_screen_cust_pcx( DISP_BRANDING, "Label", WOW_ERROR_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL );
				if(card_type == card_none)
				{

					if( WOW_NZ_PINPAD )
					{
						txn.card_state = card_capture_standard;
					}
					else
					{
						if (res == VIM_ERROR_NOT_FOUND) {
							/*  Display this screen only if we could not find loyalty
								VIM_ERROR_NOT_FOUND is returned when we have sensed there is really no pass available
								if it's a random error like user time out or user just waves or quickly remove the phone,
								VIM_ERROR_MISMATCH is returned and this error screen shall not be displayed
							*/
							display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassError.png"), VIM_PCEFTPOS_KEY_CANCEL);
							vim_event_sleep(1500);
						}
					}
				}
				return state_set_state(ts_card_capture);
			}
		}
// vyshakh commented on 12/01/2018

/*		else if ((res == VIM_ERROR_GPRS_SIM_LOCK) || (res == VIM_ERROR_MISMATCH)) {
			vim_picc_emv_close_apdu_interface_mode(picc_handle.instance);
			display_screen_cust_pcx(DISP_BRANDING, "Label", WOW_PRESENT_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
			return state_set_state(ts_card_capture);
		}
*/
        // RDD 131119 Card Removed in NZ Loyalty should result in command to swipe card then return to card read state
        if ((WOW_NZ_PINPAD) && (res == VIM_ERROR_EMV_CARD_REMOVED))
        {
            VIM_DBG_WARNING(" -------- Card Removed During NZ J8 --------------");
            txn.error = ERR_CPAT_NO_DOCK;
            return state_set_state(ts_card_capture);
        }

		if(( res == ERR_ONE_CARD_SUCESSFUL )||(res == ERR_LOYALTY_CARD_SUCESSFUL))
		{
			DBG_MESSAGE("<<<<<< LOYALTY CARD FULL EMV EXIT POINT  >>>>>>>>");
			txn.error = VIM_ERROR_NONE;
			display_screen_cust_pcx( DISP_IMAGES, "Label", TermGetBrandedFile("", "PassThanks.png"), VIM_PCEFTPOS_KEY_CANCEL );
			return state_set_state( ts_exit );
		}
		else if(( txn.error == VIM_ERROR_SYSTEM )||( txn.error == VIM_ERROR_NOT_FOUND ))
		{
			return state_set_state( ts_exit );
		}
		else if( txn.error == VIM_ERROR_EMV_TXN_ABORTED)
		{
			return state_set_state( ts_card_capture );
		}
		else if( txn.error == VIM_ERROR_PICC_ACTIVATION_TIMEOUT )
		{
			txn.error = VIM_ERROR_NONE;
			//reset_picc_transaction();
			return state_set_state( ts_card_capture );
		}
		else
		{
			return state_set_state( ts_card_check );
		}
	}

	if(( card_type == card_manual )&&(( res == VIM_ERROR_TIMEOUT )||(res == ERR_WOW_ERC )))
	{
		// RDD 150615 SPIRA:8756
		if(( transaction_get_status( &txn, st_electronic_gift_card ) || ( txn.type == tt_moto ) || ( txn.type == tt_moto_refund )))
		{
			txn.error = res;
			return state_set_state(ts_finalisation);
		}
		else
		{
			return state_set_state(ts_card_capture);
		}
	}

  if(( res == VIM_ERROR_POS_CANCEL )||( res == VIM_ERROR_TIMEOUT ) || (res == VIM_ERROR_SYSTEM) )
  {
	  // RDD 300316 SPIRA:8926 MO system error on 96 responses to docked cards
	  if( txn.error != ERR_WOW_ERC )
			txn.error = res;
	  else
	  {
		  DBG_MESSAGE( "SPIRA:8926" );
	  }
	  return state_set_state(ts_finalisation);
  }

  if( ( res == ERR_KEYED_NOT_ALLOWED )||
      ( res == VIM_ERROR_USER_CANCEL )||
      ( res == ERR_CARD_EXPIRED ) ||
      ( res == ERR_CARD_EXPIRED_DATE )  ||
	  ( res == ERR_NO_CASHOUT ))
  {
	  VIM_DBG_ERROR(res);
	  // Function menu initiated captures return to idle if user cancels
	  // RDD 140322 JIRA PIRPIN-1472 : cashout on 91 response went to exit state.
	  if(( terminal.initator == it_pinpad )&&( IS_INTEGRATED ))
	  {
		  txn.error = res;
		  return state_set_state( ts_exit );
	  }
	  else
	  {
	    /* JQ: set card state back to insert or swipe */
		if( res == ERR_NO_CASHOUT )
		{
            // RDD 120820 Trello #187 - NO_CASHOUT On Offline (91) needs a receipt
            // Maybe improve code in EMV EFB instead but can fix here.
            VIM_DBG_STRING(txn.host_rc_asc);
            VIM_DBG_STRING(txn.host_rc_asc_backup);
            if( !vim_strcmp(txn.host_rc_asc_backup, "91" ))
            {
                pceft_debug( pceftpos_handle.instance, "Trello #187" );
                vim_strcpy(txn.cnp_error, "CDF");

                return state_set_state(ts_finalisation);
            }

			if( txn.card_state == card_capture_forced_icc )
			{
                vim_strcpy(txn.cnp_error, "FDS");
				txn.card_state = card_capture_no_picc;
			}
			else
			{
				display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
			}
			return state_set_state(ts_card_capture);
		}
	  }
  }

  if( card_type == card_picc )
  {
    txn_state_t next_state = ProcessCaptureResult_contactless(res);
    state_set_state(next_state);
    return next_state;
  }
  else if( card_type == card_chip )
  {
    txn_state_t next_state = state_get_current();
	//DBG_VAR( next_state );

	next_state = ProcessCaptureResult_contact(res);
	DBG_VAR( next_state );

    state_set_state(next_state);
	return next_state;
  }
  else if( card_type == card_mag )
  {
    if( res != VIM_ERROR_NONE )
    {
		// RDD 080312 - SPIRA 4853: Need to handle VIM_ERROR_MAG_TRACK_READ
		if(( res == VIM_ERROR_MAG_TRACK_READ )||( res == ERR_INVALID_TRACK2 ))
		{
			terminal_initator_t temp = terminal.initator;

			// RDD 5100 doesn't do a pos display for this error....
			terminal.initator = it_pinpad;
			display_screen( DISP_CARD_READ_ERROR, VIM_PCEFTPOS_KEY_NONE, " " );
			// retore whatever it was origininally....
			terminal.initator = temp;
			vim_kbd_sound_invalid_key();
			vim_event_sleep( 1500 );
		}
		else
		{
			// RDD 160202 Bugfix : Need to display "CANCELLED CARD REMOVED on PinPad and POS"
			display_result( res, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
		}
		// RDD 170512 SPIRA5513 Problems with multiply failed card reads: eventually exit state is called rather than card_read state as history buffer full
		return state_set_state(ts_card_capture);
		//return ts_card_capture;
    }
    else
      return (state_get_next());
  }
  else if( card_type == card_none )
  {
	txn.error = res;
	return state_set_state( ts_exit );
  }
// RDD 100512 SPIRA:5464,5465 timeout on Manual entry return to card capture state
// RDD JIRA WWBP-529 Bad Luhn on Gift Card from POS needs to exit
#if 0
  else if(( card_type == card_manual ) && ( res == ERR_LUHN_CHECK_FAILED ))
  {
	  return state_set_state( ts_exit );
  }
#endif
  return (state_get_next());
}

void icc_close(void)
{
	DBG_POINT;
  vim_icc_close(icc_handle.instance_ptr);
  icc_handle.instance_ptr = VIM_NULL;
}



///////////////////////////////////// STATE: TXN_CARD_CAPTURE ///////////////////////////////////////////
// This state captures customer card data : mag, emv or contactless
////////////////////////////////////////////////////////////////////////////////////////////////////////////

txn_state_t txn_card_capture( void )
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  txn_state_t next_state;
  
  // RDD 260522 JIRA PIRPIN-
  txn.amount_surcharge = 0;


   DBG_MESSAGE("STATE: CARD CAPTURE");
   if (txn.error == ERR_INVALID_ACCOUNT)
   {
       txn.error_backup = txn.error;
       DBG_MESSAGE("Reset txn.error");
       VIM_DBG_VAR( txn.account );
   }

   // RDD - only run this test if "WALLY" actually enabled ....
   if (( txn.type == tt_sale ) && ( 0 < vim_strlen(txn.edp_payment_session_id) ) && ( TERM_EDP_ACTIVE ))
   {
       pceft_debug(pceftpos_handle.instance, "EDP Transaction");
       txn.error = Wally_DoTransaction();

       terminal_set_status( ss_incomplete );

       next_state = ts_finalisation;

       DBG_VAR(next_state);
       return next_state;
   }
   else
   {
       switch (txn.type)
       {
	   case tt_sale:
	   case tt_cashout:
	   case tt_sale_cash:
	   case tt_completion:
	   case tt_moto:
		   // RD 080322 - don't call this stuff if No EDP and EDP not activated via J8 : can cause crashes !
		   if(TERM_EDP_ACTIVE)
		   {
			   VIM_DBG_MESSAGE("Incoming non-everyday pay financial transaction; clearing Wally session data");
			   Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_TRUE);
		   }
		   break;
	   default:
		   //do nothing - we want to keep the wally session for non-financial transactions
		   break;
       }
   }

   // RDD 041114 - sometimes after errors the state isn'nt get reset correctly
   state_set_state( ts_card_capture );
	  
   terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855
       
   if( transaction_get_status( &txn, st_moto_txn ))   
   {   
	   txn.account = account_credit;       
	   // RDD 201021 JIRA PIRPIN-1258 Georgia wants to change this to exit on timeout       
	   if(( res = card_detail_capture_key_entry( &txn.card, type, AmountString(txn.amount_total))) != VIM_ERROR_NONE )       
	   {       
		   txn.error = res;           
		   return state_set_state(ts_exit);           
	   }       
	   return state_get_next();       
   }
  
   // RDD 020112 - reset CPAT index and card type if coming (back) in here  
   // If card data comes over from the POS then we must have pre-stored card-data, so skip this state
	   
   // RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS
   // put a limit on the number of retries allowed
   if ((txn.read_icc_count > 2)&&(transaction_get_status(&txn, st_technical_fallback)))
   {
	   pceft_debug(pceftpos_handle.instance, "Too many docked reads: Decline T8");
	   txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
	   return state_set_state(ts_exit);
   }

  if( transaction_get_status( &txn, st_pos_sent_pci_pan ))      
  {
      if( txn.type == tt_deposit )
      {
          DBG_MESSAGE("------ Deposit with decrypted ePAN -------");
          return state_skip_state();
      }
      else if (txn.card.data.manual.pan_length > WOW_PAN_LEN_MIN)
      {
          pceft_debug_test(pceftpos_handle.instance, "Skip Card Read as PAN provided");
          if (transaction_get_status(&txn, st_electronic_gift_card))
          {
              txn.card.type = card_manual;
          }
          return state_skip_state();
      }
      else if(( txn.type == tt_balance )||(txn.type == tt_gift_card_activation))
      {
          DBG_MESSAGE("POS sent PCI PAN with balance or activation but no card data");
      }
      else if( txn.type == tt_deposit )
      {
          DBG_MESSAGE("POS sent PCI PAN without balance and no card data");
          return state_skip_state();
      }
      else if (txn.type == tt_checkout && !TERM_USE_PCI_TOKENS)
      {
          DBG_MESSAGE("NON WOW Completion - skip Card Capture");
          return state_skip_state();
      }
  }

  // RDD - this state can be entered multiple times so it's important to start off with a clean slate
  txn.card.type = card_none;
  transaction_clear_status( &txn, st_cpat_check_completed );

  // RDD 230516 SPIRA:8953
  //transaction_clear_status( &txn, st_electronic_gift_card );

#ifndef _PHASE4
  vim_mag_flush(mag_handle.instance_ptr);
#endif
  if( mag_handle.instance_ptr != VIM_NULL )
  {
#ifdef _PHASE4
	vim_mag_flush(mag_handle.instance_ptr);
#endif
	VIM_ERROR_IGNORE (vim_mag_destroy (mag_handle.instance_ptr));
    mag_handle.instance_ptr = VIM_NULL;
  }

  // RDD 050412: SPIRA:5231 PinPad hangs when txn serialised and card has be re-read by different method
  vim_mem_clear( &txn.card, VIM_SIZEOF(card_info_t) );


  // RDD 060513: SPIRA:6710
  transaction_clear_status( &txn, st_label_found );

  // RDD SPIRA 6956 - card removed after "last try" bug . Need to ensure that this flag is cleared if offline Pin entry is to re-commence
  vim_mem_clear( &txn.emv_pin_entry_params, VIM_SIZEOF( emv_pin_entry_params_t ) );
  transaction_clear_status( &txn, st_offline_pin_verified );
  transaction_clear_status( &txn, st_pin_bypassed );

  // RDD 091014 ALH FIX for Donna - retry after card removal after pin bypassed was bypassing automatically so need to clear this flag
  txn.pin_bypassed = 0;

  transaction_clear_status( &txn, st_offline_pin );
  transaction_clear_status( &txn, st_bypass_available );

  // RDD 240712 SPIRA:5789 New card inserted after card pulled during txn resulted in some old tags still being present
  txn.emv_data.data_size = 0;
  vim_mem_clear( txn.emv_data.buffer, VIM_SIZEOF(txn.emv_data.buffer) );

  // RDD 280212: SPIRA 4825 txn.error not being reset if card removed one time and card capture run again: results in txn rejection
  txn.error = VIM_ERROR_NONE;
  if( txn.type != tt_checkout )
	vim_strcpy( txn.host_rc_asc, "" );

  // RDD added 270911
  get_txn_type_string_short(&txn, type, VIM_SIZEOF(type));
  vim_icc_new(&icc_handle, VIM_ICC_SLOT_CUSTOMER);

  if(( txn.type == tt_moto )||( txn.type == tt_moto_refund ))
	  txn.card_state = card_capture_keyed_entry;

#ifndef _mmWIN32
  ////////////////////// Card Capture Called Here ////////////////////
  res = card_capture(type, &txn.card, txn.card_state, txn.read_icc_count);
  ////////////////////// Card Capture Called Here ////////////////////
#else

  txn.card.type = card_mag;

 // Debit Only -  vim_strcpy( txn.card.data.mag.track2, "5602512366399231=14121011518483100000" );
  //vim_strcpy( txn.card.data.mag.track2, "3569990012236336=14121011518483100000" );
  //vim_strcpy( txn.card.data.mag.track2, "5494710001182506=14011010000046700000" );
 // vim_strcpy( txn.card.data.mag.track2, "5494710001182308=14111010000046700000" ); // WOW Credit Card

//  vim_strcpy( txn.card.data.mag.track2, "4363846589626307=17023010000036000000" ); // MACQUARIE BACODE CARD
//  vim_strcpy( txn.card.data.mag.track1, "4363846589626307^vpqpc4/souls.             ^1702201934444147944700360000000" );

//  vim_strcpy( txn.card.data.mag.track2, "4363846589626307" ); // MACQUARIE BACODE CARD

  vim_strcpy( txn.card.data.mag.track2, "623061571010200108=17023010000036000000" ); // WOW Credit Card

  //  vim_strcpy( txn.card.data.mag.track1, "4363846589626307^vpqpc4/souls.             ]1702201934444147944700360000000" );
  if(txn.card.data.mag.track2_length)
  {
	  res = VIM_ERROR_USER_CANCEL;
	  next_state = ProcessCaptureResult( res, txn.card.type );
	  DBG_VAR(next_state);
	  return next_state;
  }

  txn.card.data.mag.track2_length = vim_strlen( txn.card.data.mag.track2 );

  ValidateTrack2( txn.card.data.mag.track2, txn.card.data.mag.track2_length );

  res = VIM_ERROR_NONE;
#endif

  VIM_DBG_ERROR(res);

  /* debug only */
  if( terminal_get_status(ss_wow_basic_test_mode) )
  {
    VIM_TEXT icc_dbg_buffer[500];
    vim_mem_clear( icc_dbg_buffer, VIM_SIZEOF(icc_dbg_buffer));
    vim_icc_get_debug (icc_dbg_buffer);
    if( vim_strlen( icc_dbg_buffer ) > 0 )
    {
      icc_dbg_buffer[VIM_SIZEOF(icc_dbg_buffer)-1] = 0x00;
      pceft_debug (pceftpos_handle.instance, icc_dbg_buffer );
    }
  }
  // RDD - setup states according to card capture results note that sucessful EMV transactions will have been completed at this point
  next_state = ProcessCaptureResult( res, txn.card.type );

  DBG_VAR(next_state);
  return next_state;
}

/* FROM EFT WOW SPEC v0.3:
Transactions that are processed as partial EMV are:
	Refunds
	Deposits
	EMV transactions where the account type selected is marked as partial EMV.
*/

VIM_BOOL DoCpatCheck( void )
{
	// RDD - from now on always do a CPAT check except for PAN the comes from POS
	if( transaction_get_status( &txn, st_pos_sent_pci_pan ))
	{
        // check luhn for BGC as arrives in clear
        if (IsBGC())
        {
            return VIM_TRUE;
        }
		return VIM_FALSE;
	}
	else
	{
		return VIM_TRUE;
	}
}

/*
    <RANGE>123456FFF</RANGE>
    <PRODUCT>
      <PRD>041</PRD>
      <AMT>000</AMT>
*/

#define BIN_TAG				"BIN>"
#define BIN_END_TAG			"/BIN>"
#define RANGE_TAG			"RANGE>"
#define PRODUCT_TAG			"PRODUCT>"
#define PRD_TAG				"PID>"
#define AMT_TAG				"AMT>"

// RDD 241117 Added for Basics Card
#define PIDANY				"ANY"


static const char *rpat_file_name = VIM_FILE_PATH_LOCAL "RPAT.XML";


static VIM_CHAR *GetAscTxnType( void )
{
	switch( txn.type )
	{
		case tt_sale: return "PURCH";

		case tt_refund: return "REFUND";

		case tt_cashout:
		case tt_sale_cash:
			return "SALE_CASH";

		default: return "OTHER";
	}
	return "UNKNOWN";
}


VIM_BOOL ProductMatch( VIM_CHAR *BinBuffer, s_product_data_t *ProductInfo )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_UINT16 ProductID = 0;
	VIM_UINT64 MaxAllowedSpend = 0;

	VIM_CHAR ProductData[2000];

	// RDD 081117 : JIRA WWBP-23 - check for Txn type Match too - if no match don't disallow
	VIM_CHAR *Ptr = VIM_NULL;

	// RDD 241117 JIRA:WWBP-81 3 changes required: 1 include Cash only txns with PURCH_CASH restriction. 2. Report Cashout not allowed error if cash compontant 3. Use "PIDANY" for restrict all txns of this type.
	if(( res = vim_strstr( BinBuffer, PIDANY, &Ptr )) == VIM_ERROR_NONE )
	{
		// RDD 241117 Basics Card Will be restricted for no cash component
		if( vim_strstr( BinBuffer, GetAscTxnType( ), &Ptr ) == VIM_ERROR_NONE )
		{
			pceft_debug_test( pceftpos_handle.instance, "ANY PID (This BIN) Restriction for txn type:" );
			pceft_debug_test( pceftpos_handle.instance, GetAscTxnType( ) );
			// We found a match in the PID any scenario so check for Cashout before modification of error code
			if( txn.type == tt_cashout || txn.type == tt_sale_cash )
			{
				txn.error = ERR_NO_CASHOUT;
			}
			return VIM_TRUE;
		}
	}

	if( vim_strstr( BinBuffer, GetAscTxnType( ), &Ptr )!= VIM_ERROR_NONE )
	{
		// Unable to find match for restricted txn type for this PID
		pceft_debug_test( pceftpos_handle.instance, "This PID has no restriction for type:" );
		pceft_debug_test( pceftpos_handle.instance, GetAscTxnType( ) );
		return VIM_FALSE;
	}
	vim_mem_clear( ProductData, VIM_SIZEOF( ProductData ));

	if(( xml_parse_tag( BinBuffer, (void *)&ProductID, PRD_TAG, NUMBER_16_BIT )) != VIM_ERROR_NONE )
		return VIM_FALSE;
	if(( xml_parse_tag( BinBuffer, (void *)&MaxAllowedSpend, AMT_TAG, NUMBER_64_BIT )) != VIM_ERROR_NONE )
		return VIM_FALSE;

	//if(( ProductID == ProductInfo->ProductID ) && ( MaxAllowedSpend == 0 ))
	//	return VIM_TRUE
	// VN 010618 JIRA WWBP-259: Pad string $0 product amount causes RPAT restrictions to be ignored
	if(( ProductID == ProductInfo->ProductID ) && (( ProductInfo->ProductAmount > MaxAllowedSpend ) || (ProductInfo->ProductAmount == 0)))
	{
		if( terminal_get_status( ss_wow_basic_test_mode ))
		{
			VIM_CHAR Buffer1[50];
			VIM_CHAR Buffer2[50];

			vim_sprintf( Buffer1, "PID%3d %s", ProductID, AmountString( ProductInfo->ProductAmount ));
			if( MaxAllowedSpend )
				vim_sprintf( Buffer2, "(Limit = %s)", AmountString( MaxAllowedSpend ));
			else
				vim_strcpy( Buffer2, "(Limit = $0.00)");

			pceft_debug_test( pceftpos_handle.instance, Buffer1 );
			pceft_debug_test( pceftpos_handle.instance, Buffer2 );
			display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer1, Buffer2 );
			vim_event_sleep(5000);
		}
		return VIM_TRUE;
	}
	return VIM_FALSE;
}

VIM_BOOL BinMatch( VIM_CHAR *BinBuffer, VIM_CHAR *Pan )
{
	VIM_SIZE BinLen;
	VIM_CHAR BinRange[12];

	vim_mem_clear( BinRange, VIM_SIZEOF( BinRange ));

	if(( xml_parse_tag( BinBuffer, BinRange, RANGE_TAG, _TEXT )) != VIM_ERROR_NONE )
		return VIM_FALSE;

	BinLen = vim_strlen( BinRange );
    VIM_DBG_STRING( BinRange );

	if( !vim_strncmp( BinRange, Pan, BinLen ))
		return VIM_TRUE;
	return VIM_FALSE;
}


VIM_BOOL XMLProductRestricted( s_product_data_t *PADProductInfo, VIM_CHAR *BinBuffer )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR ProductBuffer[5000];
	VIM_CHAR *Ptr = BinBuffer;
	VIM_SIZE Len = 0;

	// Each BinBuffer may contain multiple product packages

	//vim_mem_clear( ProductBuffer, VIM_SIZEOF(ProductBuffer) );
	while( res == VIM_ERROR_NONE )
	{
		if(( res = vim_strstr( Ptr, PRODUCT_TAG, &Ptr )) == VIM_ERROR_NONE )
		{
			// WWBP-107 Clear buffer prior to each PID parse else you could end up with unwanted data as the parser doesn't seem to NULL terminate.
			vim_mem_clear( ProductBuffer, VIM_SIZEOF(ProductBuffer) );
			if(( res = xml_get_tag( Ptr, PRODUCT_TAG, (VIM_CHAR *)ProductBuffer )) == VIM_ERROR_NONE )
			{
				// The BIN has one or more restricted products
				Len = vim_strlen( ProductBuffer );
				if( ProductMatch( ProductBuffer, PADProductInfo ))
				{
                    VIM_DBG_MESSAGE("Product match found with PAD PRD Data");
					return VIM_TRUE;
				}
				// Push the pointer to the end of the current container
				Ptr += vim_strlen( PRODUCT_TAG );
				Ptr += Len;
			}
		}
	}
	return VIM_FALSE;
}

VIM_BOOL RpatFilePresent( void )
{
	VIM_BOOL exists = VIM_FALSE;
	vim_file_exists( rpat_file_name, &exists );
#ifdef _XPRESS
    if (!txn.u_PadData.PadData.NumberOfGroups)
        return VIM_FALSE;
#endif

    if (exists)
    {
        VIM_DBG_MESSAGE("RPAT FILE FOUND");
    }
    else
    {
        VIM_DBG_MESSAGE("RPAT FILE NOT FOUND");
    }

	return exists;
}


#define MAX_RPAT_BIN_CONTAINER_LEN 20000



VIM_BOOL ProductLimitExceeded( s_product_data_t *PADProductInfo, VIM_CHAR *Pan, VIM_CHAR *RpatFileData, VIM_CHAR *BinBuffer )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//VIM_CHAR RpatFileData[70000];
	VIM_CHAR *Ptr = RpatFileData;
	//VIM_CHAR BinBuffer[20000];	// RDD 070416 V560 - increased size due to crash during testing
	VIM_SIZE /*Len,*/ RpatFileSize;

	// No RPAT file so just use CPAT values
	if( !RpatFilePresent( ))
		return VIM_FALSE;

	vim_file_get_size(rpat_file_name, VIM_NULL, &RpatFileSize);

	//vim_mem_clear( RpatFileData, VIM_SIZEOF( RpatFileData ));

	// Read the file into a buffer
	vim_file_get( rpat_file_name, RpatFileData, &RpatFileSize, RpatFileSize );

    // RDD 020821 JIRA PIRPIN 1014 Report RPAT Ver
    xml_parse_tag((VIM_CHAR *)RpatFileData, terminal.configuration.RPAT_Version, "VER>", _TEXT);
    VIM_DBG_STRING(terminal.configuration.RPAT_Version);

    // Need to run this on power-up to get RPAT Version so exit here if this is case
    if (Pan == VIM_NULL || PADProductInfo == VIM_NULL)
    {
        return VIM_FALSE;
    }

	// RDD 120215 added facility to remove comments from XML strings
	if (RemoveComments(RpatFileData) != VIM_ERROR_NONE)
	{
		pceft_debugf(pceftpos_handle.instance, "FATAL ERROR: Unable to remove comments from RPAT v%s", terminal.configuration.RPAT_Version);
		return VIM_FALSE;
	}

	pceft_debugf_test( pceftpos_handle.instance, "XXX736+ Processing RPAT Ver:%s Size=%d ...", terminal.configuration.RPAT_Version, RpatFileSize);

	while( res == VIM_ERROR_NONE )
	{
		// RDD 080623 JIRA PIRPIN-2426 Issue with BIN Buffer not being correctly cleared for Dynamic Memory Allocation
		//vim_mem_clear( BinBuffer, VIM_SIZEOF( BinBuffer ));
		vim_mem_clear( BinBuffer, MAX_RPAT_BIN_CONTAINER_LEN );

		if(( res = xml_get_tag( (VIM_CHAR *)Ptr, BIN_TAG, (VIM_CHAR *)BinBuffer )) == VIM_ERROR_NONE )
		{
            // RDD 080621 Don't check if PID=0 as these are invalid
			if(( BinMatch( BinBuffer, Pan ))&&( PADProductInfo->ProductID ))
			{
                VIM_DBG_MESSAGE("Found BIN in RPAT File");
				if (XMLProductRestricted(PADProductInfo, BinBuffer))
					return VIM_TRUE;
				// RDD 200618 JIRA WWBP-265 Need to exit if BinMatch and no restrictions in case of less BIN match further down RPAT...
				else
					return VIM_FALSE;
			}
			VIM_DBG_NUMBER(vim_strlen(Ptr));
			VIM_DBG_NUMBER(vim_strlen(BinBuffer));
			// Push the pointer to the end of the current container
			if ((res = vim_strstr(Ptr, BIN_END_TAG, &Ptr)) == VIM_ERROR_NONE)
			{
				Ptr += vim_strlen( BIN_END_TAG ) + 2;
			}
			else
			{
				return VIM_FALSE;
			}
		}
	}
	return VIM_FALSE;
}



VIM_BOOL XMLProductLimitExceeded(s_product_data_t* PADProductInfo, VIM_CHAR* Pan)
{
	VIM_BOOL res = VIM_FALSE;
	void* RpatPtr = VIM_NULL;
	void* BinPtr = VIM_NULL;
	VIM_SIZE RpatFileSize = 0;

	vim_file_get_size(rpat_file_name, VIM_NULL, &RpatFileSize);

	if (!RpatFileSize)
	{
		pceft_debugf(pceftpos_handle.instance, "FATAL ERROR: Unable to process RPAT <zero file len>" );
		return VIM_FALSE;
	}

	if (vim_malloc(&RpatPtr, RpatFileSize + 1) != VIM_ERROR_NONE)
	{
		pceft_debugf(pceftpos_handle.instance, "FATAL ERROR: Unable to process RPAT <malloc %d bytes failed>", RpatFileSize);
		return VIM_FALSE;
	}
	else if (vim_malloc(&BinPtr, MAX_RPAT_BIN_CONTAINER_LEN) != VIM_ERROR_NONE)
	{
		pceft_debugf(pceftpos_handle.instance, "FATAL ERROR: Unable to process RPAT-BIN <malloc %d bytes failed>", MAX_RPAT_BIN_CONTAINER_LEN);
		vim_free(RpatPtr);
		return VIM_FALSE;
	}

	vim_mem_clear(RpatPtr, RpatFileSize + 1);
	vim_mem_clear(BinPtr, MAX_RPAT_BIN_CONTAINER_LEN);

	res = ProductLimitExceeded(PADProductInfo, Pan, (VIM_CHAR*)RpatPtr, (VIM_CHAR*)BinPtr);

	vim_free(RpatPtr);
	vim_free(BinPtr);
	return res;
}



VIM_BOOL ProductRestricted( VIM_UINT8 ProductID )
{
	VIM_SIZE n;
	struct RestrictedProduct { VIM_UINT8 CpatRestriction; VIM_UINT8 ProductId;  } RestrictedProducts[6];

	RestrictedProducts[0].CpatRestriction = BAR_PRODUCT41;
	RestrictedProducts[1].CpatRestriction = BAR_PRODUCT42;
	RestrictedProducts[2].CpatRestriction = BAR_PRODUCT43;
	RestrictedProducts[3].CpatRestriction = BAR_PRODUCT44;
	RestrictedProducts[4].CpatRestriction = BAR_PRODUCT45;
	RestrictedProducts[5].CpatRestriction = BAR_PRODUCT46;
	RestrictedProducts[0].ProductId = 41;
	RestrictedProducts[1].ProductId = 42;
	RestrictedProducts[2].ProductId = 43;
	RestrictedProducts[3].ProductId = 44;
	RestrictedProducts[4].ProductId = 45;
	RestrictedProducts[5].ProductId = 46;

	for( n=0; n<6; n++ )
	{
		if(( ProductID == RestrictedProducts[n].ProductId ) && ( RestrictedProducts[n].CpatRestriction ))
			return VIM_TRUE;
	}
	return VIM_FALSE;
}


VIM_BOOL RestrictedProductsPurchased( void )
{
	VIM_SIZE n=0;
	VIM_CHAR Pan[25];

	vim_mem_clear( Pan, VIM_SIZEOF(Pan) );
	card_get_pan( &txn.card, Pan );

	do
	{
		if( ProductRestricted( txn.u_PadData.PadData.product_data[n].ProductID ))
			return VIM_TRUE;

		// RDD 120215 v541 added additional product data checks by bin via XML file
		if( XMLProductLimitExceeded( &txn.u_PadData.PadData.product_data[n], Pan ))
			return VIM_TRUE;
	}
	while( 	++n < txn.u_PadData.PadData.NumberOfGroups );

	return VIM_FALSE;
}



///////////////////////////////////// STATE: TXN_CARD_CHECK ///////////////////////////////////////////
// This state checks the card against the transactions type, pre-set amounts etc. to validify the txn
////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#define CARD_REJECTED "CARD REJECTED\nINVALID CARD NO\n"
//#define TRAINING_MODE  "( TRAINING MODE )"

//static VIM_TEXT const Label[] = CARD_REJECTED;
//static VIM_TEXT const LabelT[] = CARD_REJECTED TRAINING_MODE;



txn_state_t txn_card_check(void)
{
  /* Check the card has is valid */
  VIM_ERROR_PTR error;
  //VIM_UINT32 CpatIndex;

   DBG_MESSAGE("STATE: CARD CHECK");

  // Set up the CPAT Index and do basic CPAT checks (txn valid, luhn etc)
  // DBG_VAR(txn.card.type);
  VIM_DBG_ERROR( txn.error );

  if ((error = card_name_search(&txn, &txn.card)) != VIM_ERROR_NONE)
  {
    
      txn.error = error;

      vim_kbd_sound_invalid_key();

	  // RDD 290112: SPIRA 5004 - Display Error for 2 seconds
	  //display_result( ERR_WOW_ERC, txn.host_rc_asc, (VIM_CHAR *) "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
	  display_result( error, txn.host_rc_asc, (VIM_CHAR *) "", "", VIM_PCEFTPOS_KEY_NONE, 1000 );

      if (error == ERR_MCR_BLACKLISTED_BIN)
      {
          txn.error_backup = ERR_MCR_BLACKLISTED_BIN;
          // Cancel the txn and close CTLS for fresh start
          picc_close(VIM_TRUE);
      }

	  if(txn.status & st_pos_sent_pci_pan)
	  {
		  TXN_RETURN_EXIT(VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED);
	  }
	  return state_set_state(ts_card_capture);
  }

  // RDD 030912 P3: One Card
  if( txn.type == tt_query_card ) return state_get_next();

  	// 150812 RDD - If there are restricted products then process PAD and check to see if any of these products have been purchased
	if(( RESTRICTED_PRODUCTS ) || ( RpatFilePresent( )))
	{
        VIM_DBG_MESSAGE("------- Restricted Products check -------");

		if(( txn.error = ValidatePADData( &txn.u_PadData.PadData, VIM_FALSE )) != VIM_ERROR_NONE )
		{
			pceft_debug( pceftpos_handle.instance, "PAD String Format Error - Rejected1" );
		    TXN_RETURN_ERROR(txn.error);
		}

		if( RestrictedProductsPurchased( ) )
		{
            VIM_DBG_MESSAGE("------- Restricted Products Purchased -------");
            if( txn.error == ERR_NO_CASHOUT )
			{
				TXN_RETURN_ERROR( ERR_NO_CASHOUT );
			}
			else
			{
				vim_strcpy( txn.host_rc_asc, "RI" );
				TXN_RETURN_ERROR(ERR_WOW_ERC);
			}
		}
	}
    else
    {
        DBG_MESSAGE("No RPAT file");
    }


  /* Check for EMV service code (2 or 6) if we swiped card and ignore service code flag is off for the card swiped */
  if( txn.card.type == card_mag )
  {
	  char service_code[3] = { 0,0,0 };

	  /* If we are in emv fallback mode then set fallback flag. Otherwise force insertion */

      // RDD 230819 JIRA WWBP-
	  if (( txn.card_state == card_capture_forced_swipe )&&( !gift_card(&txn) ))
	  {
		  if( transaction_get_status( &txn, st_technical_fallback ) )
		  {
			DBG_MESSAGE( "<<<< SERVICE CODE CHECK BYPASSED DUE TO TECH FALLBACK >>>>" );
			//txn.card.data.mag.emv_fallback = VIM_TRUE; // JQ
		  }
		  txn.card.data.mag.emv_fallback = VIM_TRUE;
	  }
	  // RDD 010312 - don't check service code if the track data was suppplied by the pos as may have been swiped due to technical fallback
      if(( CARD_NAME_SERVICE_CODE_CHECKING ) && ( card_get_service_code(&txn.card.data.mag, service_code ) == VIM_ERROR_NONE ) && (!transaction_get_status( &txn, st_pos_sent_pci_pan )) )
      {
		DBG_VAR(service_code);
		DBG_MESSAGE("<<<<< Check Service Code >>>>>");
		if( service_code[0] == '2' || service_code[0] == '6' )
		{
			if( !txn.card.data.mag.emv_fallback )
			{
				txn.card_state = card_capture_forced_icc;
                // Reason code - swiped card must be docked
                vim_strcpy( txn.cnp_error, "FDS");
				DBG_MESSAGE( "<<<< SERVICE CODE INDICATES FORCE ICC - GOTO CARD CAPTURE >>>>" );
				display_screen( DISP_CARD_MUST_BE_INSERTED, VIM_PCEFTPOS_KEY_ERROR, type, AmountString(txn.amount_total) );

                // RDD 160919 JIRA WWBP-502 above msg was flashing on screen
                vim_kbd_sound_invalid_key();
                vim_event_sleep(2000);
				return state_set_state(ts_card_capture);
			}
		}
		else
		{
			txn.card.data.mag.emv_fallback = VIM_FALSE;
			DBG_MESSAGE( " Service Code -> 021 " );
		}
      }

  }
  else if ((txn.card.type == card_manual) && (txn.card_state == card_capture_forced_swipe))
  {
	DBG_MESSAGE( "<<<< SET MAG FALLBACK FLAG >>>>" );
    txn.card.data.manual.emv_fallback = VIM_TRUE;
  }

  // DBG_MESSAGE( "<<<< FORM EPAN... >>>>" );
  // RDD- looks like we always come here if the card capture suceeeds so generate WOW ePan at this point
  if(( error = wow_form_ePan() ) != VIM_ERROR_NONE )
  {
	  // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
	  terminal.last_logon_state = terminal.logon_state;
	  terminal.logon_state = session_key_logon;
	  TXN_RETURN_ERROR(error);
  }

  if (DoCpatCheck())
  {
      if ((error = OtherCpatChecks()) != VIM_ERROR_NONE)
      {
          DBG_MESSAGE("<<<< CPAT CHECK FAILED - EXIT >>>>");
          VIM_DBG_ERROR(error);

          // RDD 290621 JIRA PIRPIN-1081 : Don't allow SAV or CHQ if AGC says Default acc
          //if(( error == ERR_INVALID_ACCOUNT) && ( txn.account == account_credit))
          //{
          //    display_result(error, "", "", "", VIM_PCEFTPOS_KEY_OK, 2000);
          //    return state_set_state(ts_card_capture);
          //}

          display_result(error, "", "", "", VIM_PCEFTPOS_KEY_OK, 4000);

          if (IsBGC())
          {
              // RDD 030620 Trello #156
              txn.error = error;
              return state_set_state(ts_exit);
          }
          else
              return state_set_state(ts_card_capture);
      }
  }


  // RDD 240512 Added for fuel card processing
  if( fuel_card(&txn)  )
  {
	  if(( error = fuel_card_processing( )) != VIM_ERROR_NONE ) TXN_RETURN_ERROR(error);
  }

	// RDD 170223 JIRA PIRPIN-1820 No reload for Returns Card reload ( use activation instead )
	if (IsReturnsCard(&txn) && TERM_RETURNS_CARD_NEW_FLOW && txn.type == tt_refund)
	{
	  pceft_debug_test(pceftpos_handle.instance, "Returns Card : Switch type to activate");
	  txn.type = tt_gift_card_activation;
	}


    // RDD 280815 SPIRA:8806 RRN missing for EMV QPS so need to do this even earlier
#if 0
  // RDD 040615 SPIRA:8747 BD wants to move this code moved to earlier in txn process so that RRNs are
  if( IS_STANDALONE && (!txn.rrn[0] ))
#else
  // RDD 190922 JIRA PIRPIN-1804 RRN should not be generated by POS ref for Linkly Complient PinPads as POS ref can include non-numeric chars
  // Leave code as is for R10 and other 
  if ( IS_STANDALONE || (!IS_R10_POS && !FUEL_PINPAD && !ALH_PINPAD ))
#endif
  {
	  VIM_TEXT rrn[12];

	  //VIM_ERROR_RETURN_ON_FAIL( vim_snprintf_unterminated( rrn, 12, "      %06d", terminal.roc ));
	  vim_snprintf_unterminated( rrn, 12, "%012d", terminal.roc );

	  // RDD 200814 added - increment the dumb roc after creating a new pre-auth
	  txn.roc = terminal.roc;
	  vim_mem_copy( txn.rrn, rrn, VIM_SIZEOF(rrn));
	  terminal_increment_roc();
  }


  return state_get_next();
}


// RDD - WOW special check for small value processing to bypass CVM
VIM_BOOL qualify_svp( void )
{
   VIM_BOOL svp_OK = VIM_FALSE;
   txn.qualify_svp = svp_OK;

   DBG_MESSAGE( "<<<< IN QUALIFY SVP >>>>" );

   if (TERM_QPS_DISABLE)
   {
       pceft_debug_test( pceftpos_handle.instance, "No QPS as TMS disable" );
       return VIM_FALSE;
   }

   // RDD 170914 - don't think we want QPS for Preauths
   if( txn.type == tt_preauth )
   {
	   DBG_POINT;
	   return VIM_FALSE;
   }

   if(1)
   {
	 // RDD 160212 Bugfix No SVP on Tech fallback
	 //if(( txn.card.data.mag.emv_fallback )&&( txn.card.type == card_manual ))
	 // RDD 250712 SPIRA:5797 No SVP on Tech Fallback
	 if(( txn.card.data.mag.emv_fallback )&&( txn.card.type == card_mag ))
	 {
	   DBG_POINT;
		 return VIM_FALSE;
	 }

	 if( txn.card.type == card_picc )
	 {
	   DBG_POINT;
		 return VIM_FALSE;						//BRD why didn't we update txn.qualify_svp???
	 }

	 if( txn.type == tt_checkout )
	 {
	   DBG_POINT;
		 return VIM_FALSE;
	 }

	 // RDD 221112 added for SPIRA:6376 - reject QPS for chip cards if epat flag not set
	 if(( txn.card.type == card_chip ) && ( !terminal.emv_applicationIDs[txn.aid_index].small_value_enabled ))
	 {
		 DBG_VAR( txn.aid_index );
		 DBG_VAR( terminal.emv_applicationIDs[txn.aid_index].small_value_enabled );
		 return VIM_FALSE;
	 }

	 if(( txn.account != account_credit )&&( txn.account != account_default ))
	 {
		 DBG_POINT;
		 return VIM_FALSE;
	 }

	 // RDD 100812 SPIRA:5870 BD decided that QPS should be de-qualified if the Pin entry is being run again following RC55
     // RDD 251121 JIRA PIRPIN-1303 QPS limit needs to include surcharge
     ApplySurcharge(&txn, VIM_TRUE);
     if(( txn.card.type == card_mag )&&( !vim_strncmp( txn.host_rc_asc, "55", 2 )))
	 {
	   DBG_POINT;
		 return VIM_FALSE;
	 }
     else if(( txn.type == tt_sale ) && ( txn.amount_cash == 0 ) && ( txn.amount_total + txn.amount_surcharge <= (CARD_NAME_SMALL_VALUE_LIMIT)*100 ) && ( (CARD_NAME_SMALL_VALUE_LIMIT) != 0 ))
     {

		DBG_POINT;
        // RDD 020420 v582-7 Re-purpose this flag for CVM override. SVTL > 0 becomes only control for QPS eligability in CPAT
        if(/* !CARD_NAME_SMALL_VALUE_PROMPT_FOR_PIN */ 1)
        {
			DBG_POINT;
			DBG_MESSAGE( "<<<< SVP: AMOUNT OK, TYPE OK, NO PIN NEEDED - PASS SVP >>>>" );
            svp_OK = VIM_TRUE;
        }

        if( txn.card.type == card_chip )
        {
			DBG_POINT;
			// RDD 160712 SPIRA:5686 Decided to allow Training mode SVP txns - EMV CB does not get to set flag in these cases
			if(( txn.qualify_svp )||( transaction_get_training_mode() ))
			{
				/* TODO - EPAT SVP processing - here */
				/* TODO - ?? should set Signature enabled flag here for EMV */
				DBG_MESSAGE( "<<<< SVP: AMOUNT OK, TYPE OK, EMV_CB OK >>>>" );
				svp_OK = VIM_TRUE;
			}
        }
     }
   }
   // RDD - for mag cards flag set here, for EMV cards the flag has already been set in the callback
   txn.qualify_svp = svp_OK;
    DBG_VAR(svp_OK);
   return svp_OK;
}

//JQ genenerate PIN block
static VIM_ERROR_PTR txn_pin_generate_pinblock(void)
{
  VIM_TCU_STAN tcu_stan;
  VIM_TCU_PAN tcu_pan;
  VIM_TCU_AMOUNT tcu_amount;

  VIM_DBG_WARNING("PPPPPPPPPPPPPPPP Generate NULL Pinblock PPPPPPPPPPPPPPPP");
  /* convert the STAN to TCU format */
  VIM_DBG_VAR_ENDIAN(terminal.stan);

  // RDD 100712 SPIRA:5757 Reversal will use up terminal stan and stan will be incremented for next 200
  if( reversal_present() )
  {
	  VIM_ERROR_RETURN_ON_FAIL( reversal_load(&reversal) );
  }

  VIM_DBG_POINT;

#if 0 // RDD 281112 V304 - getting pinblock errors if reversal is sent up before pin txn.
  if(( reversal_present() ) && ( transaction_get_status( &reversal, st_repeat_saf ) == VIM_FALSE ))
  {
	  // DBG_MESSAGE( "<<<<<<<<<< USE STAN+1 TO GENERATE PINBLOCK >>>>>>>>>>>");
	  // DBG_VAR( (terminal.stan)+1 );
	  vim_utl_size_to_bcd((terminal.stan)+1,tcu_stan.bytes,VIM_SIZEOF(tcu_stan.bytes)*2);
  }
  else
  {
	  // DBG_MESSAGE( "<<<<<<<<< USE STAN+0 TO GENERATE PINBLOCK >>>>>>>>>>>>");
	  // DBG_VAR( (terminal.stan) );
	  vim_utl_size_to_bcd(terminal.stan,tcu_stan.bytes,VIM_SIZEOF(tcu_stan.bytes)*2);
  }
#else	// RDD 281112 - since I changed 420/421 to always use txn stan we don't need to gen pinblock with terminal.stan + 1
		// Basically terminal.stan gets incremented when the rev. is gen, so if 200 has stan=123, then ts=124 after tran. orig stan=123 (txn stan)
		//
	  vim_utl_size_to_bcd((terminal.stan),tcu_stan.bytes,VIM_SIZEOF(tcu_stan.bytes)*2);
#endif
  /* send the STAN to the TCU module */
  vim_tcu_set_stan(tcu_handle.instance_ptr,&tcu_stan);

  /* convert the amount to TCU format */

  // RDD 270921 v728 LIVE - no surcharge in DE4 for Mastercard. don't change the global structure as we need the total unchanged for receipts etc.
  if ((CardIndex(&txn) == MASTERCARD) && (txn.amount_surcharge))
  {
      // 121121 v729-0 Update: MC now handled in connex so treat same as other schemes.
      //VIM_UINT64 temp_total = txn.amount_total - txn.amount_surcharge;
      //vim_utl_size_to_bcd( temp_total, tcu_amount.bytes, VIM_SIZEOF(tcu_amount.bytes) * 2);
      vim_utl_size_to_bcd(txn.amount_total, tcu_amount.bytes, VIM_SIZEOF(tcu_amount.bytes) * 2);
  }
  else
  {
      vim_utl_size_to_bcd(txn.amount_total, tcu_amount.bytes, VIM_SIZEOF(tcu_amount.bytes) * 2);
  }


  /* send the amount to the TCU module */
  vim_tcu_set_amount(tcu_handle.instance_ptr,&tcu_amount);
  /* convert the PAN to TCU format */
  vim_mem_clear(tcu_pan.value,VIM_SIZEOF(tcu_pan.value));
  card_get_pan( &txn.card, tcu_pan.value );
  tcu_pan.size = vim_strlen( tcu_pan.value );
  /* send the PAN to the TCU module */
  vim_tcu_set_pan(tcu_handle.instance_ptr,&tcu_pan);

  /* build PIN block */
  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_encrypted_pin_block(tcu_handle.instance_ptr, &txn.pin_block));

  VIM_DBG_WARNING("PPPPPPPPPPPPPP Pinblock Generated OK PPPPPPPPPPPP");
  VIM_DBG_DATA( &txn.pin_block, 8 );
  VIM_DBG_DATA(&tcu_stan.bytes[0], 3);
  VIM_DBG_DATA(&tcu_amount.bytes[0], 6);
  VIM_DBG_DATA(&tcu_pan.value[0], 20);
  VIM_DBG_NUMBER(tcu_pan.size);

  /* set pin block size */
  txn.pin_block_size = 8;

  /* return without error */
  return VIM_ERROR_NONE;
}

//JQ, this function used only when generating a NULL PIN block
//it will erase previous inputed PIN if existed
VIM_ERROR_PTR txn_pin_generate_NULL_PIN_block( VIM_BOOL fEmv )
{
	VIM_ERROR_PTR  result;

	DBG_MESSAGE( "------------------------------------------------------------");
	DBG_MESSAGE( "-------------- * GENERATE NULL PINBLOCK * ------------------");
	DBG_MESSAGE( "------------------------------------------------------------");
	vim_tcu_set_null_pinblock(tcu_handle.instance_ptr, VIM_TRUE);

	/* Generate Null PIN block */
	result = txn_pin_generate_pinblock();

	/* Reset TCU module flag, used to generate null pin block */
	vim_tcu_set_null_pinblock(tcu_handle.instance_ptr, VIM_FALSE);
	return result;
}


VIM_BOOL ValidateAuthCode( VIM_CHAR *AuthCode )
{
	// efb transaction doesn't need an auth code
	if( transaction_get_status( &txn, st_efb ) )
	{
		return VIM_TRUE;
	}
	if( !asc_to_bin( AuthCode, AUTH_CODE_LEN ) )
	{
		return VIM_FALSE;
	}
	if( vim_strlen( AuthCode ) != AUTH_CODE_LEN )
	{
		return VIM_FALSE;
	}
	else
	{
		return VIM_TRUE;
	}
}






void SetupPinEntryParams( VIM_TCU_PIN_ENTRY_PARAMETERS *param, txn_pin_entry_type *pin_entry_type )
{
	DBG_MESSAGE( "!! SETUP PIN ENTRY PARAMS !!");

	vim_mem_clear( param, VIM_SIZEOF(VIM_TCU_PIN_ENTRY_PARAMETERS) );

	// Setup default parameters
	param->mask_ptr = "*";
	param->min_size = 4;
	param->max_size = 12;
	param->exit_on_back_space_at_start = VIM_FALSE;
	param->is_for_emv = VIM_FALSE;
	param->integrated_mode = IS_INTEGRATED;

	// RDD 010814 added for standalone ALH
	param->allow_cancel = IS_STANDALONE;

    param->allow_zero_length = VIM_FALSE;
	// RDD 140313 - must ensure that we timeout in the callback and NOT in the kernel else we get get removed error instead of timeout error
	// RDD 270814 - SPIRA:8071 Pin Entry Timeout is 2x too long assume VIM fn. now more accurate...
	param->timeout = ENTRY_TIMEOUT;
	param->icc_handle = VIM_NULL;

	DBG_POINT;

	if (txn.card.type == card_chip)
	{
		param->icc_handle = &icc_handle;

		DBG_POINT;

		if (!transaction_get_status(&txn, st_partial_emv))
		{
			param->min_size = txn.emv_pin_entry_params.min_digits;
			param->max_size = txn.emv_pin_entry_params.max_digits;

			// RDD 160412 SPIRA:5249 EPAT Pin Bypass permitted does not trump CPAT pin mandatory. Opposite is true.
			DBG_MESSAGE( "DETERMINE FULL EMV OPTIONAL OR MANDATORY PIN ENTRY");
			DBG_VAR(terminal.card_name[txn.card_name_index].ProcSpecnCode);
			DBG_VAR( param->min_size );
			DBG_VAR( param->max_size );
			DBG_VAR( terminal.emv_applicationIDs[txn.aid_index].pin_bypass );
			//DBG_VAR( terminal.emv_applicationIDs[txn.aid_index].pin_bypass_international );
			DBG_VAR( txn.aid_index );

            // 030820 Trello #82 DE55 Refunds for VISA and Mastercard - maybe ignore if override enabled
            if(( txn.type == tt_refund ) && ( EMV_REFUND_CPAT_OVERRIDE ))
            {
                VIM_DBG_MESSAGE("--------------- EMV_REFUND_CPAT_OVERRIDE SET -----------------");
                return;
            }
            DBG_POINT;


			// RDD 160812 SPIRA:5892 If International Card then read international Bypass Flag
			// RDD 220513 SPIRA:6737 - no longer need to use pin_bypsass_international as seeting copied into default
			//if( ( emv_is_card_international() ) && ( terminal.emv_applicationIDs[txn.aid_index].pin_bypass_international == VIM_FALSE ) )
			if( ( emv_is_card_international() ) && ( terminal.emv_applicationIDs[txn.aid_index].pin_bypass == VIM_FALSE ) )
			{
				if(( param->min_size == 0 ) && ( param->max_size == 0 ))
				{
	//				DBG_MESSAGE( "FULL EMV BYPASS PIN ENTRY");
					VIM_DBG_WARNING( "OK" );
					*pin_entry_type = txn_pin_entry_press_ok;
				}
				else
				{
					DBG_POINT;
//					DBG_MESSAGE( "---------- INTERNATIONAL CARD WITH NO BYPASS -----------");
					*pin_entry_type = txn_pin_entry_mandatory;
				}
			}

			// RDD 120712 SPIRA:5737 Don't allow Pin Bypass if there is a cash componant
			else if(( WOW_CPAT_OPTIONAL_PIN_ENTRY ) && ( param->max_size ) && ( terminal.emv_applicationIDs[txn.aid_index].pin_bypass ) && ( txn.amount_cash == 0 ))
			{
	//			DBG_MESSAGE( "FULL EMV OPTIONAL PIN ENTRY ");
                DBG_POINT;
				param->allow_zero_length = VIM_TRUE;
				*pin_entry_type = txn_pin_entry_optional;
			}
			else
			{
                DBG_POINT;
				if(( param->min_size == 0 ) && ( param->max_size == 0 ))
				{
//					DBG_MESSAGE( "FULL EMV BYPASS PIN ENTRY");
			        VIM_DBG_WARNING( "OK" );
					*pin_entry_type = txn_pin_entry_press_ok;
				}
				else
				{
			        DBG_POINT;
//					DBG_MESSAGE( "FULL EMV MANDATORY PIN ENTRY ");
					param->allow_zero_length = VIM_FALSE;
					*pin_entry_type = txn_pin_entry_mandatory;
				}
			}
			DBG_POINT;
			param->is_for_emv = txn.emv_pin_entry_params.offline_pin;
		}
		else
			DBG_POINT;

		DBG_POINT;
	}
	DBG_VAR(*pin_entry_type);
}



void SetPinEntryViaCpat( txn_pin_entry_type *pin_entry_type )
{

	DBG_MESSAGE( "SET PIN ENTRY VIA CPAT ");
		if( WOW_CPAT_OPTIONAL_PIN_ENTRY )
		{
            DBG_MESSAGE("CPAT OPTIONAL PIN");
            *pin_entry_type = txn_pin_entry_optional;
			return;
		}


		if( WOW_CPAT_SIGNATURE_ONLY )
		{

			transaction_set_status( &txn, st_signature_required );
			*pin_entry_type = txn_pin_entry_press_ok;
			VIM_DBG_WARNING( "OK" );
			return;
		}


		if( WOW_CPAT_MANDATORY_PIN_ENTRY )
		{
            DBG_MESSAGE("CPAT MANDATORY PIN");

			// RDD 270813 SPIRA 6845 Refunds on Docked Credit Txns not prompting for PIN. DG says should follow CPAT rules
			*pin_entry_type = txn_pin_entry_mandatory;
			return;
		}


		if( WOW_CPAT_SIGNATURE_AND_PIN )
		{
			// RDD 270813 SPIRA 6845 Refunds on Docked Credit Txns not prompting for PIN. DG says should follow CPAT rules
			*pin_entry_type = txn_pin_entry_mandatory;
			transaction_set_status( &txn, st_signature_required );
			return;
		}

}


VIM_BOOL OdoRetry( void )
{
	if( !vim_strncmp ( txn.host_rc_asc, "OJ", 2 ))
	{
		txn.u_PadData.PadData.OdoReading = 0;				// Make the ODO reading zero so that we re-read the odo
		txn.u_PadData.PadData.OdoRetryFlag = VIM_TRUE;		// Set the ODO retry Flag
		return VIM_TRUE;
	}
	return VIM_FALSE;
}

#if 0
// RDD 051211 - Logic controlling whether Pin retry is attempted from receipt of online "55" RC
VIM_BOOL PinRetry( void )
{
	VIM_BOOL RetryPin = VIM_FALSE;

	// If there was never any pin block then don't retry - might be a response to non-financial txn type
	if( txn.pin_block_size == 0 ) return VIM_FALSE;

	if ( !vim_strncmp ( txn.host_rc_asc, "55", 2 ))
	{
		if( txn.qualify_svp )
		{
			return VIM_FALSE;
		}
		if( card_chip != txn.card.type && card_picc != txn.card.type )
		{
			RetryPin = VIM_TRUE;
		}
		if( transaction_get_status( &txn, st_partial_emv ) )
		{
			RetryPin = VIM_TRUE;
		}
	}
	return RetryPin;
}
#endif



// RDD 051211 - Logic controlling whether Pin retry is attempted from receipt of online "55" RC
VIM_BOOL PinRetry( void )
{
	// Only consider Pin retry if RC = "55"
	if ( vim_strncmp ( txn.host_rc_asc, "55", 2 )) return VIM_FALSE;

    // RDD 020321 - Robbies code causes PinPad to reboot for some reason if we allow Pin retries so just hard decline for now...
    if (QC_TRANSACTION(&txn))
    {
        return VIM_FALSE;
    }

	if( txn.error != ERR_WOW_ERC ) return VIM_FALSE;

	// RDD v430 - don't know why but PICC < CVM got in a loop with 55 resp from Finsim.
	if( txn.card.type == card_picc ) return VIM_FALSE;

	// RDD 010812 SPIRA:5812 Don't retry if there was never any pin entered (for whatever reason) except for fuel cards...
	if(( fuel_card(&txn) ) && ( txn.FuelData[PROMPT_FOR_PIN] != NOT_REQUIRED )) return VIM_TRUE;

	// RDD 010812 Basically if its a fuel card and pin was not required then we can use the bypass flag to decide....
	//if( txn.pin_bypassed ) return VIM_FALSE;

	// We received RC = "55" and there was a Pin Entered so we return TRUE for partial EMV or Mag cards
	if( txn.card.type == card_mag ) return VIM_TRUE;

    // RDD 191120 JIRA PIRPIN 940 - don't allow PIN retry if card was docked
	//if( transaction_get_status( &txn, st_partial_emv )) return VIM_TRUE;

	return VIM_FALSE;
}


VIM_BOOL epal_contactless(void)
{
	if( GetEPALAccount() == VIM_ERROR_NONE )
	{
		if( txn.card.type == card_picc )
		{
			DBG_MESSAGE( "EEEEEEEEEEEEEE EPAL CONTACTLESS UNDER CVM LIMIT EEEEEEEEEEEEEEEEEE");
			return VIM_TRUE;
		}
	}
	return VIM_FALSE;
}



// RDD 311211 -  Set the PIN Entry Method According to the Spec
void SetPinEntryType( txn_pin_entry_type *pin_entry_type  )
{
	// ----------- CONDITIONS REQUIRED TO BYPASS PIN ENTRY STATE ENTIRELY ----------------
	// Set default for following conditions
	*pin_entry_type = txn_pin_entry_skip_state;
	VIM_DBG_ERROR(txn.error);
	DBG_POINT;
	DBG_VAR(txn.emv_pin_entry_params.offline_pin);

    // 041219 DEVX : This feature now removed - PIN entry required for all gift cards
#if 0
    // No PIN entry for EGC
    if(( IsBGC() ) && ( IsEGC() ))
    {
        //plt_gift_set_auth( tcu_handle.instance_ptr, txn.card.data.manual.auth_code );
        //txn_pin_generate_pinblock();
        txn.pin_block_size = 0;
        VIM_DBG_NUMBER( txn.pin_block_size );
        VIM_DBG_DATA( txn.pin_block.bytes, txn.pin_block_size );
        return;
    }
#endif

    if( txn.type == tt_gift_card_activation )
    {
        return;
    }

	// RDD 170912 No Pin Entry for Rimu Query Card
	if( txn.type == tt_query_card )
	{
		return;
	}

    // RDD 150421 JIRA PIRPIN-1041 No PIN required on MC refunds - causes issues with direct link
    if(( CardIndex(&txn) == MASTERCARD  || CardIndex(&txn) == VISA) && ( txn.type == tt_refund ) && ( txn.account != account_cheque ) && (txn.account != account_savings ))
    {                
        pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-1041");         
        return;            
    }


	// RDD 021114 No PIN entry for MOTO - do OK to continue
	if(( txn.type == tt_moto )||( txn.type == tt_moto_refund ))
	{
		*pin_entry_type = txn_pin_entry_press_ok;
		VIM_DBG_WARNING( "OK" );
		return;
	}


	if( txn.type == tt_refund && IsUPI( &txn ))
	{
		*pin_entry_type = txn_pin_entry_press_ok;
		VIM_DBG_WARNING( "OK" );
		return;
	}

	// No Pin entry for completions - need signature instead (sig reqd. flag already set in initial parsing of PCEFTPOS txn type)
	if( transaction_get_status( &txn, st_completion ) ) return;

	DBG_POINT;
	// RDD 270512 PinProc for Fuel Cards dependant on Card Type rather than CPAT
	if( fuel_card(&txn) )
	{
		switch( txn.FuelData[PROMPT_FOR_PIN] )
		{
			case NOT_REQUIRED:
				*pin_entry_type = txn_pin_entry_press_ok;
				VIM_DBG_WARNING( "OK" );
				break;

			case OPTIONAL:
				*pin_entry_type = txn_pin_entry_optional;
				break;

			default:
				*pin_entry_type = txn_pin_entry_mandatory;
				break;
		}
		return;
	}

	// RDD 080512 SPIRA:5418 "PRESS OK TO CONTINUE" Now handled in emv_cb.c for case when Offline Pin Retry Exceeded
	if( txn.error == VIM_ERROR_EMV_PIN_BLOCKED )
	{
		DBG_MESSAGE( "@@@@@@ OFFLINE PIN WITH RETRIES EXCEEDED ... SKIP PIN ENTRY @@@@" );
		// RDD 080512 if Pin entry was called then the error should be ignored from this point.
		txn.error = VIM_ERROR_NONE;
		return;
	}

	// RDD 230212 : Skip pin entry state if manual card - always signature
	// RDD 190412 : SPIRA5239 Mandatory PIN for bal even if manual entry
	// RDD 020413 : Phase4 Added support for E-Gift Card -this is manually entered but requires mandatory PIN
	if(( txn.card.type == card_manual ) && (txn.type != tt_balance ) && ( !transaction_get_status( &txn, st_electronic_gift_card )))
	{
		transaction_set_status( &txn, st_signature_required );
		return;
	}

	if( txn.type == tt_checkout )
	{
		transaction_set_status( &txn, st_signature_required );
		return;
	}


	// RDD 160412 SPIRA:5314 NO PIN ENTRY FOR PICC CARDS OVER CVM LIMIT
	//if(( txn.card.type == card_picc ) && ( txn.type == tt_sale )) return;

	// No Pin entry required for refunds (activation) of gift cards
	if (gift_card(&txn))
	{
		if (tt_gift_card_activation == txn.type)
		{
			VIM_DBG_MESSAGE("Skipping PIN/passcode entry for Gift card activation");
			return;
		}
		else if(tt_refund == txn.type)
		{
			if (QC_TRANSACTION(&txn))
			{
				// JIRA PIRPIN-1820 New Handling for returns cards
				if (IS_RETURNS_CARD)
				{
					//pceft_debug_test(pceftpos_handle.instance, "Skipping Passcode entry for Returns Card Refund");
					//txn.type = tt_activate;
				}
				else
				{
					pceft_debug_test(pceftpos_handle.instance, "Passcode entry for QC Non-Returns Refund");
				}
			}
			else
			{
				VIM_DBG_MESSAGE("Skipping PIN entry for non-QC Gift card refund");
				return;
			}
		}
	}

    // No Pin entry required for deposits
    // RDD 280720 - Trello #120 NZ Christmas Club needs savings type
    if (txn.type == tt_deposit)
    {
        if (!IS_XMAS_CLUB)
        {
            return;
        }
    }

	// SVP payment ( currently any txn under $35 ) causes bypass of PIN entry but still need to press OK
	if( qualify_svp() )
	{
		DBG_MESSAGE( "!! SVP QUALIFIED !!");

		// RDD 231112 SPIRA:6376 Offline Pin on QPS causing an internal Pin entry error
		txn_pin_generate_NULL_PIN_block( VIM_TRUE );

		return;
	}

	DBG_VAR( terminal.card_name[txn.card_name_index].ProcSpecnCode )


	// ----------- CONDITIONS REQUIRED FOR MANDATORY PIN ENTRY ----------------
	// Reset Default now to mandatory
	*pin_entry_type = txn_pin_entry_mandatory;

	// RDD 130922 JIRA PIRPIN-1811 Force PIN mandatory if TFB
	if (transaction_get_status(&txn, st_technical_fallback))
	{
		pceft_debug(pceftpos_handle.instance, "JIRA_PIRPIN_1811");
		return;
	}

	// RDD 240212 SPIRA 4883 for v15 - offline pin requires mandatory pin entry
#if 0 // DG says offline pin needs to follow same rules as online pin
    if( txn.emv_pin_entry_params.offline_pin ) return;
#endif
	// RDD 120712 SPIRA:5737 Cashout component for Credit for Scheme Debit Cards - force mandatory Pin for these types else can get Z1 on Pin bypass ie. Pin bypass shouldn't have been allowed
	if( txn.amount_cash ) return;

	// Debit Account Types require mandatory PIN entry
	// RDD 050413 - changed order of these two and added extra condition to bypass pin entry for epal_contactless under cvm
	if(( txn.account == account_savings) || (txn.account == account_cheque ))
	{
		if( !epal_contactless() )
			return;
	}
	// RDD 020212 - balance requires pin entry
	if( txn.type == tt_balance ) return;

#if 0
	// RDD 060412 SPIRA 5254 : Make Contactless Refunds Use CPAT for CVM
	if(( transaction_get_status( &txn, st_cpat_check_completed )) && ( txn.card.type != card_picc ))
	{
		SetPinEntryViaCpat( pin_entry_type );
		return;
	}
	else
	{
		DBG_VAR( txn.status );
	}
#endif
	// ----------- CONDITIONAL REQUIREMENTS FOR EMV CONTACT AND CONTACTLESS CARDS  ----------------
	DBG_VAR( txn.card.type );
	switch( txn.card.type )
	{
		case card_picc:
		picc_set_pin_entry_type( pin_entry_type );
		break;

		case card_chip:
		DBG_POINT;
		// RDD 270813 SPIRA 6845 Refunds on Docked Credit Txns not prompting for PIN. DG says should follow CPAT rules
        // RDD 050820 Trello #82 DE55 Refunds - only use CPAT rules if not FullEMV
		if( txn.type == tt_refund )
		{
            if (( !transaction_get_status( &txn, st_partial_emv) && ( !(EMV_REFUND_CPAT_OVERRIDE) )))
            {
                pceft_debug( pceftpos_handle.instance, "REFUND CVM via EMV");
                emv_set_pin_entry_type(pin_entry_type);
            }
            else
            {
                pceft_debug( pceftpos_handle.instance, "EMV Refund CVM via CPAT" );
                SetPinEntryViaCpat(pin_entry_type);
            }
		}
		else
		{
            VIM_DBG_MESSAGE("--------- Set Non-refund CVM via EMV requirements ------------");
            emv_set_pin_entry_type( pin_entry_type );
		}
		break;

		default:
			// RDD 050614 V425 make default use CPAT settings not optional pin
			SetPinEntryViaCpat( pin_entry_type );
		//*pin_entry_type = txn_pin_entry_optional;
		break;
	}
}



// RDD 251112 Amount confirmation screen is displayed for the following conditions:

VIM_ERROR_PTR PressOKToContinue( VIM_CHAR *CardName, VIM_CHAR *account_string_short, VIM_BOOL fEmv )
{
	//VIM_SIZE timeout;

	// RDD 060513 SPIRA:6706 Don't prompt if completion
	if( txn.type == tt_completion )
	{
		return VIM_ERROR_NONE;
	}

	DBG_DATA(account_string_short,4);
	display_screen( DISP_PIN_PRESS_ENTER, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short );

	// RDD 080212 - Save the transaction as we've changed it
	txn_save();

	if (card_picc == txn.card.type)
          vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PRESS_ENTER, VIM_FALSE);

    if (card_chip == txn.card.type)
    {
		vim_icc_new(&icc_handle,VIM_ICC_SLOT_CUSTOMER);
    }

	// RDD 010812 SPIRA:5818 re-written
    // RDD 251112 SPIRA:6380 Check for card removal here as sometimes missing this when Pin tries exceeded
	//timeout = ( txn.card.type == card_chip ) ? ENTRY_TIMEOUT/2 : ENTRY_TIMEOUT;
	VIM_ERROR_RETURN_ON_FAIL( data_entry_confirm( ENTRY_TIMEOUT, IS_STANDALONE, VIM_TRUE ) );
	return VIM_ERROR_NONE;
}



txn_state_t txn_get_pin(VIM_BOOL fEMV)
{

  VIM_TCU_PIN_ENTRY_PARAMETERS param;
  VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
  VIM_CHAR PinEntryString[30];

  VIM_ERROR_PTR res;
  VIM_CHAR account_string_short[3+1];

  txn_pin_entry_type pin_entry_type = txn_pin_entry_optional;
  VIM_SIZE pin_size = 0;


  // RDD - surcharge apply prior to PIN Block build as modified amount needs to be in Pinblock
  ApplySurcharge(&txn, VIM_TRUE);

          
  if (txn.type == tt_checkout)  
  {
      DBG_MESSAGE("NON WOW Completion - skip PIN entry");
      return state_skip_state();      
  }

  DBG_MESSAGE("************* STATE: GET PIN *****************");

  VIM_DBG_ERROR( txn.error_backup );

  vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );
  vim_mem_clear( &param, VIM_SIZEOF( param ));
  vim_mem_clear( PinEntryString, VIM_SIZEOF(PinEntryString) );

  // RDD 280213 - get the tags at this point so the full card name is available in case of PIN entry
#ifndef _OLD_PICC_TAGS
  ////picc_tags();
#endif
  get_acc_type_string_short(txn.account, account_string_short);
  GetCardName( &txn, CardName );

  // RDD 280815 SPIRA:8806 RRN missing for EMV QPS so need to do this even earlier
#if 0
  // RDD 040615 SPIRA:8747 BD wants to move this code moved to earlier in txn process so that RRNs are
  if( IS_STANDALONE && (!txn.rrn[0] ))
  {
	  VIM_TEXT rrn[12];

	  //VIM_ERROR_RETURN_ON_FAIL( vim_snprintf_unterminated( rrn, 12, "      %06d", terminal.roc ));
	  vim_snprintf_unterminated( rrn, 12, "%012d", terminal.roc );

	  // RDD 200814 added - increment the dumb roc after creating a new pre-auth
	  txn.roc = terminal.roc;
	  vim_mem_copy( txn.rrn, rrn, VIM_SIZEOF(rrn));
	  terminal_increment_roc();
  }
#endif

	SetPinEntryType( &pin_entry_type );
  	// RDD 311211 - Setup the TCU PIN parameters according to the entry type
    VIM_DBG_VAR(pin_entry_type);
    if( pin_entry_type != txn_pin_entry_skip_state )
		SetupPinEntryParams( &param, &pin_entry_type );

	VIM_DBG_VAR(pin_entry_type);

  if( pin_entry_type == txn_pin_entry_skip_state )
  {
	  // RDD 201211 added - always generate NULL PIN Block if Pin entry Bypassed
	   DBG_MESSAGE("PIN ENTRY BYPASSED GET NEXT STATE");
	  //txn_pin_generate_NULL_PIN_block( fEMV );
	  return state_get_next();
  }

  // Passcode not PIN for QC transactions
  if (QC_TRANSACTION(&txn))
  {
      // Get the Passcode and dump in PAD buffer - needs to be then removed from PAD buffer by QC processing in QC_Gift.c
      return txn_enter_passcode();
  }


 // RDD 050614 SPIRA:???? JCB Issue - Switch can't handle PIN to JCB
  if( WOW_CPAT_SIGNATURE_ONLY )
  {
	  DBG_MESSAGE( "V425 Override online PIN with SIG CVM cards" );
	  transaction_set_status( &txn, st_signature_required );
	  pin_entry_type = txn_pin_entry_press_ok;
	  VIM_DBG_WARNING( "OK" );
	  txn.pin_bypassed = VIM_TRUE;

	  // RDD v561-3 Noticed that Offline PIN JCB Card hangs when waiting for OK to be pressed - set PIN Bypassed flag to fix
	  txn.emv_error = VIM_ERROR_EMV_PIN_BYPASSED;
  }
   DBG_VAR(pin_entry_type);
   DBG_VAR(param);


  if( txn.card.type == card_chip )
  {
	  // DBG_MESSAGE( "**** NO ICC HANDLE - CREATING NEW.... ****");
	  vim_icc_new(&icc_handle, VIM_ICC_SLOT_CUSTOMER);
  }


  // RDD 291217 get event deltas for logging
  vim_rtc_get_uptime( &txn.uEventDeltas.Fields.CVMReq );

  switch (pin_entry_type)
  {
  case txn_pin_entry_press_ok:
      DBG_POINT;
      res = PressOKToContinue(CardName, account_string_short, fEMV);
      VIM_DBG_ERROR(res);
      break;


      // PIN ENTRY OPTIONAL
  case txn_pin_entry_optional:
      param.allow_zero_length = VIM_TRUE;

      if ((fEMV) && (txn.emv_pin_entry_params.last_pin_try))
      {
          vim_strcpy(PinEntryString, "Last Try or ENTER");
      }
      else
      {
          vim_strcpy(PinEntryString, "  Key PIN or ENTER");
      }

#ifndef _WIN32
      if ((res = adkgui_pin_entry(tcu_handle.instance_ptr, &param, &pin_size, DISP_KEY_PIN_OR_ENTER, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString)) != VIM_ERROR_NONE)
      {
          TXN_RETURN_ERROR(res);
      }
#else
      if ((res = display_screen(DISP_KEY_PIN_OR_ENTER, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString)) != VIM_ERROR_NONE)
      {
          TXN_RETURN_ERROR(res);
      }
        // RDD 080212 - Save the transaction as we've changed it
	  transaction_set_status( &txn, st_bypass_available );
	  if( txn.emv_pin_entry_params.offline_pin )
		DBG_MESSAGE( "+++++++++++++++ OFFLINE PIN WITH BYPASS AVAILABLE +++++++++++++++++" );
	  txn_save();

      //if (card_picc == txn.card.type)
      //  vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PIN_OR_ENTER, VIM_FALSE);

      // RDD 201218 TODO P400 - this will need to be re-written when we get to PIN entry !

      while(( res = vim_screen_tcu_pin_entry(tcu_handle.instance_ptr, &param, "PIN", pceftpos_handle.instance)) == VIM_ERROR_USER_CANCEL )
      {
		  if(( res == VIM_ERROR_USER_CANCEL ) && IS_STANDALONE ) TXN_RETURN_ERROR(res);

		  /* Redraw the PIN entry screen */
		  if(( res = display_screen( DISP_KEY_PIN_OR_ENTER, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString )) != VIM_ERROR_NONE ) TXN_RETURN_ERROR(res);
      }
	  VIM_DBG_ERROR(res);
	  DBG_POINT;
#endif
      break;


    case txn_pin_entry_mandatory:
    default:

		if(( fEMV ) && ( txn.emv_pin_entry_params.last_pin_try ))
		{
			vim_strcpy( PinEntryString, "Key PIN (Last Try)" );
		}
		else
		{
			vim_strcpy( PinEntryString, "Key PIN" );
		}

#ifndef _WIN32
        res = adkgui_pin_entry(tcu_handle.instance_ptr, &param, &pin_size, DISP_KEY_PIN, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString);
        break;
#else
		if(( res = display_screen( DISP_KEY_PIN, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString )) != VIM_ERROR_NONE ) TXN_RETURN_ERROR(res);
		do
		{
			DBG_POINT;
			res = vim_screen_tcu_pin_entry(tcu_handle.instance_ptr, &param, "PIN", pceftpos_handle.instance );
			if(( res == VIM_ERROR_USER_CANCEL ) && IS_STANDALONE ) TXN_RETURN_ERROR(res);
			/* Redraw the PIN entry screen on cancel key press */

			if( res == VIM_ERROR_USER_CANCEL )
			{
				display_screen( DISP_KEY_PIN, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short, PinEntryString );
			}
		}
		while( res == VIM_ERROR_USER_CANCEL );
		break;

#endif
  }


  // RDD 291217 get event deltas for logging
  vim_rtc_get_uptime( &txn.uEventDeltas.Fields.CVMResp );

  VIM_DBG_ERROR(res);
  DBG_VAR(txn.card.type);
  DBG_VAR(txn.emv_pin_entry_params.offline_pin);

  VIM_DBG_ERROR( txn.error_backup );

  // RDD 250516 SPIRA:8955 Removal of card for debit txns needs to go back to Card Capture
  // RDD 260522 JIRA PIRPIN-1639 Standalone Removing card at key PIN does not cancel transaction.
#if 0
  if(( res == VIM_ERROR_EMV_CARD_REMOVED ) && ( transaction_get_status( &txn, st_partial_emv )))
  {
	  display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
	  TXN_RETURN_CAPTURE(res);
  }
#endif

  if(( res == VIM_ERROR_NOT_FOUND ) && ( card_chip == txn.card.type ) && ( txn.emv_pin_entry_params.offline_pin ))
  {
#if 0	// TEST V7
	  DBG_MESSAGE( "############ OFFLINE PIN CVM : DETECTED #############");

	  // RDD 120213 - for Kernel v6.2.0 and above, we need to kill the current PCEFT session
	  if ( pceftpos_handle.instance!=VIM_NULL )
	  {
		  DBG_MESSAGE( "############ OFFLINE PIN CVM : KILL PCEFT SESSION #############");
		  if( pceft_close() == VIM_ERROR_NONE )
		  {
			  DBG_MESSAGE( "############ OFFLINE PIN CVM : SESSION KILLED #############");
		  }
	  }
#endif
	  return state_get_next();
  }


  if (card_chip == txn.card.type)
  {
	  DBG_POINT;
	  icc_close();
  }

  // RDD 180412 TODO : Tidy up below error handling from Pin Entry - for full emv error handling is done on exit from emv_txn
  // for everything else error needs to be fully handled here

  if ( res != VIM_ERROR_NONE )
  {
	  /* debug this to PCEFTPOS */
	  if( terminal_get_status(ss_wow_basic_test_mode) )
      {
	      VIM_TEXT temp_buffer[64];
		  VIM_INT32 platform_error=0;
		  vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer) );
		  vim_tcu_get_platform_error (tcu_handle.instance_ptr, &platform_error);
		  vim_snprintf( temp_buffer, VIM_SIZEOF( temp_buffer )-1, "%s:%x", res->name, platform_error );
		  pceft_debug (pceftpos_handle.instance, temp_buffer);
	  }
	  if( res == VIM_ERROR_POS_CANCEL )
	  {
	      TXN_RETURN_ERROR(res);
	  }
	  // full emv txns are handled when returning from kernel but removed card on partials should be handled here
	  //if(( res == VIM_ERROR_EMV_CARD_REMOVED )&&( txn.type != tt_sale ))
//BD	  if(( res == VIM_ERROR_EMV_CARD_REMOVED )&&( transaction_get_status( &txn, st_partial_emv )))
	  if (0) //BD remove double error display
	  {
		  display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
	  }
	  else
	  {
		// RDD 020312: SPIRA 5171
		//txn.card_state = card_capture_no_picc;
		emv_remove_card( "" );
	  }
      //TXN_RETURN_CAPTURE(res);
#if 0
	  if( txn.card.type == card_chip ) // Error handling is done elsewhere...
	  {
		  TXN_RETURN_EXIT(res);
	  }
	  else
#endif
	  {
		  TXN_RETURN_ERROR(res);
	  }
  }

  if(( res = EmvCardRemoved(VIM_TRUE) ) != VIM_ERROR_NONE )
  {
      TXN_RETURN_CAPTURE(res);
  }
  else
  {
    if (param.is_for_emv) {
        txn.pin_block_size = pin_size;
    }
    else
    {
        vim_tcu_get_pin_size(tcu_handle.instance_ptr, &pin_size);
    }
    VIM_DBG_NUMBER(pin_size);
    if( pin_size > 0 )
      txn.pin_bypassed = VIM_FALSE;
    else
    {
      txn.pin_bypassed = VIM_TRUE;
      //JQ. refresh the screen as soon as finished pin ENTRY
      //display_screen(DISP_PROCESSING_WAIT, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total) );
    }

    /* print receipt because PIN has been entered */
    txn.print_receipt = VIM_TRUE;
    if (txn_pin_entry_press_ok != pin_entry_type && !txn.emv_pin_entry_params.offline_pin)
    {
      /* generate PIN BLOCK */
			 //
      txn_pin_generate_pinblock();
    }

    return state_get_next();
  }
}



VIM_BOOL txn_requires_tip_signature(transaction_t *transaction)
{
  /* Tip adjustment Enable and it is pruchase txn */
  if ((transaction->type == tt_sale) && IS_CREDIT_TXN && TERM_TIP_ADJUST_ENABLE && (transaction->card.type != card_picc))
    return VIM_TRUE;
  else
    return VIM_FALSE;
}



#define UPI_CREDIT_AID "\xA0\x00\x00\x03\x33\x01\x01\x02"
#define UPI_QUASI_CREDIT_AID "\xA0\x00\x00\x03\x33\x01\x01\x03"


VIM_BOOL IsUPICreditOverCVM( transaction_t *transaction )
{
	if( txn.type != tt_sale )
	{
		return VIM_FALSE;
	}  
  
	if (transaction->aid_index > MAX_EMV_APPLICATION_ID_SIZE)
	{
		return VIM_FALSE;
	}
  
	if( vim_mem_compare( &terminal.emv_applicationIDs[transaction->aid_index].emv_aid[0], UPI_QUASI_CREDIT_AID, 8) == VIM_ERROR_NONE)
	{
		if (transaction->amount_total > terminal.emv_applicationIDs[txn.aid_index].cvm_limit)
		{
			return VIM_TRUE;
		}  
	}
  
	if( vim_mem_compare( &terminal.emv_applicationIDs[transaction->aid_index].emv_aid[0], UPI_CREDIT_AID, 8) == VIM_ERROR_NONE)
	{	  
		if(  transaction->amount_total > terminal.emv_applicationIDs[txn.aid_index].cvm_limit )	
			return VIM_TRUE;  
	}  
	return VIM_FALSE;
}



VIM_BOOL txn_requires_signature(transaction_t *transaction)
{
	DBG_MESSAGE( "Signature Checking" );

#ifdef _WIN32
	//return VIM_TRUE;
#endif

    if (txn.type == tt_void)
    {
        return VIM_FALSE;
    }

#if 1 // SPIRA:9080 Getting Sig prompt on 91 from switch
	// No signature on declined
	if ( !is_approved_txn(transaction) )
	{
		DBG_DATA( transaction->host_rc_asc, 2 );
		return VIM_FALSE;
	}
#endif

#if 0	// RDD 160318 remove this requirement. specuial request from UPI
	// RDD 280616 added for UPI CVM Rules
	if( IsUPICreditOverCVM( transaction ) )
	{
		return VIM_TRUE;
	}
#endif

    if (transaction->saf_reason == saf_reason_offline_emv_purchase)
    {
        VIM_DBG_WARNING("Txn was approved offline - No signature !");
        return VIM_FALSE;
    }


	// RDD 020412 SPIRA 5233
	if( txn_is_customer_present(&txn) == VIM_FALSE )
	{
		return VIM_FALSE;
	}
	// Deposit Completion
	if( txn.error == ERR_SIGNATURE_APPROVED_OFFLINE )
	{
		return VIM_TRUE;
	}

	if( txn.error != VIM_ERROR_NONE )
	{
		VIM_DBG_ERROR(txn.error);
		return VIM_FALSE;
	}

#if 0 // SPIRA:9080 Getting Sig prompt on 91 from switch
	// No signature on declined
	if ( !is_approved_txn(transaction) )
	{
		DBG_DATA( transaction->host_rc_asc, 2 );
		return VIM_FALSE;
	}
#endif
	// RDD 090912 PRODUCTION PILOT ISSUE SPIRA:6025 : Gift Card Activation doesn't need signature
	if( gift_card(&txn) && (txn.type == tt_refund) )
	{
		return VIM_FALSE;
	}

#if 0
	// RDD 050412 - ensure that Sig Panel printed if Pin Was Bypassed
	// RDD 130912 P2 PRODUCTION PILOT ISSUE SPIRA:6042 - 00 response to non-refund mag txns should not result in sig CVM if pin bypassed
	// RDD 140513 SPIRA
	if(( txn.pin_bypassed ) && ( txn.qualify_svp == VIM_FALSE ) && ( vim_mem_compare( transaction->host_rc_asc, "08", 2) == VIM_ERROR_NONE ))
	{
		DBG_MESSAGE( "<<<<<<<<< SIG BECAUSE PIN WAS BYPASSED >>>>>>>>>>>");
		return VIM_TRUE;
	}
#endif

	// RDD 311211 added - this flag set from PinEntry State if Rules state that signature is required
	// This should cover both EMV contact and contactless cards as the flag is set here
	if( transaction_get_status( &txn, st_signature_required ) && !transaction_get_status( &txn, st_moto_txn ))
	{
		DBG_MESSAGE( "<<<<<<<<< SIG BECAUSE STATUS WAS SET >>>>>>>>>>>");
		return VIM_TRUE;
	}

	// Ruled by host response: adding EFB and OFFLINE checking because of in EFB mode, we will set the host_rc_asc to *08
	if(( vim_mem_compare( transaction->host_rc_asc, "08", 2) == VIM_ERROR_NONE) && !transaction_get_status(transaction, st_efb + st_offline))
	{
		// RDD 280212: SPIRA 4982 PICC Transactions under CVM limit will not prompt for sig even if 08 returned in DE39
		if( card_picc == transaction->card.type )
		{
			if( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit )
			{
				vim_strcpy( transaction->host_rc_asc, "00" );
				return VIM_FALSE;
			}
			// RDD 090312 SPIRA 4943 - ANZ Returns 08 if VIM_NULL Pin Block - if NO Signature CVM then don't prompt for SIG
			else if(  !transaction_get_status( &txn, st_signature_required ) )
			{
				// RDD 020715 SPIRA:8777
				if( txn.type == tt_sale )
				{
					vim_strcpy( transaction->host_rc_asc, "00" );
					return VIM_FALSE;
				}
				else
				{
					return VIM_TRUE;
				}
			}
		}
		else
		{
			return VIM_TRUE;
		}
	}

	// Deposits do not

	// Any completion requires a signature
    if (transaction_get_status(transaction, st_completion)) return VIM_TRUE;

	// RDD 050412 - ensure that Sig Panel printed if Pin Was Bypassed
	// RDD 130912 P2 PRODUCTION PILOT ISSUE SPIRA:6042 - 00 response to non-refund mag txns should not result in sig CVM if pin bypassed
	// RDD 140513 SPIRA
	if(( txn.pin_bypassed ) && ( txn.qualify_svp == VIM_FALSE ) && ( vim_mem_compare( transaction->host_rc_asc, "08", 2) == VIM_ERROR_NONE ))
	{
		DBG_MESSAGE( "<<<<<<<<< SIG BECAUSE PIN WAS BYPASSED >>>>>>>>>>>");
		return VIM_TRUE;
	}


	// check EMV rules
	if(( emv_requires_signature(transaction) ) && ( card_chip == transaction->card.type))
	{
		// RDD 261012 added to fix QPS after CVM changes
		if( !txn.qualify_svp )
			return VIM_TRUE;
	}

#if 0
	// efb debit signature required
	if ( transaction_get_status(transaction, st_efb) && ((transaction->account == account_savings) || (transaction->account == account_cheque)))
	{

		if( gift_card() && (txn.type == tt_refund) )
		{

			return VIM_FALSE;
		}
		else
			return VIM_TRUE;
	}
#else
	// gift card fix - moved up...
	if ( transaction_get_status(transaction, st_efb) && ((transaction->account == account_savings) || (transaction->account == account_cheque)))
	{
		return VIM_TRUE;
	}

#endif
	// Deposits don't need signature
	if( txn.type == tt_deposit )
	{
		return VIM_FALSE;
	}

	// EFB or OFFLINE transaction with mag swiped, need to be sigature
	if ((card_mag == transaction->card.type) && transaction_get_status(transaction, st_efb + st_offline))
		return VIM_TRUE;

	// Default don't request signature
	return VIM_FALSE;
}


VIM_ERROR_PTR screen_kbd_or_touch_read
(
  VIM_KBD_CODE * key_code_ptr,
  VIM_MILLISECONDS timeout,
  VIM_BOOL touch_active,
  VIM_BOOL check_card_removed
)
{
	VIM_MILLISECONDS sub_timeout = 20;
	VIM_MILLISECONDS count = timeout/sub_timeout;
	VIM_SIZE n;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	VIM_RTC_UPTIME now;
	VIM_RTC_UPTIME end_time;

	/* get starting time */
	VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_uptime(&now));

	end_time = now + timeout;

	*key_code_ptr = VIM_KBD_CODE_NONE;

	//if( txn.card.type == card_chip ) count/=2;

	for( n=0; n<count; n++ )
	{
		if( check_card_removed )
		{
			if(( res = EmvCardRemoved(VIM_TRUE) ) != VIM_ERROR_NONE )
			{
				DBG_POINT;
				return res;
			}
		}

        if (vim_adkgui_is_active())
        {
            VIM_BOOL default_event = VIM_FALSE;
            VIM_EVENT_FLAG_PTR kbd_event_ptr = &default_event;
            VIM_EVENT_FLAG_PTR event_list[1];
            event_list[0] = kbd_event_ptr;
            res = vim_adkgui_kbd_touch_or_other_read( key_code_ptr, 1, event_list, 1 );
        }
        // Allow other tasks to run so PCEFT doesn't timeout and thinks its got disconnected etc
        vim_event_sleep(50);
		if(res == VIM_ERROR_NONE )
		{
			DBG_POINT;
			if( *key_code_ptr != VIM_KBD_CODE_NONE )
			{
				return res;
			}
		}

		if( res != VIM_ERROR_TIMEOUT )
		{
			DBG_POINT;
			return res;
		}

		VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_uptime(&now));

		if( now > end_time )
			return VIM_ERROR_TIMEOUT;

	}
	return VIM_ERROR_TIMEOUT;
}





txn_state_t txn_select_nz_account(VIM_BOOL fEMV)
{
    VIM_KBD_CODE key_pressed;
    VIM_CHAR acc_string[3 + 1];
    VIM_ERROR_PTR res = VIM_ERROR_NONE;
    VIM_BOOL  refresh_display = VIM_TRUE;
    VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN + 1];
    VIM_UINT32 screen_id = DISP_SELECT_ACCOUNT;

    get_acc_type_string_short(txn.account, acc_string);

    vim_mem_clear(CardName, VIM_SIZEOF(CardName));

    DBG_MESSAGE("STATE: SELECT ACC");

    if (IS_XMAS_CLUB)     // RDD 280720 - Trello #120 NZ Christmas Club needs savings : assign here as account_select state isn't run for deposit
    {
        txn.account = account_credit;
        return state_skip_state();
    }

    // RDD 120521 JIRA PIRPIN-1060 NZ Gift Card prompts for account
    if( gift_card( &txn ))
    {
        pceft_debug_test(pceftpos_handle.instance, "Gift Card force Sav Acc");
        txn.account = account_savings;
        return state_skip_state();
    }


    // RDD 200115 SPIRA:8354 - deal with preauths including case where SAV or CHQ selected via Application selection
    if (txn.type == tt_preauth)
    {
        if (txn.pos_account && txn.account != account_credit)
        {
            TXN_RETURN_ERROR(ERR_CARD_NOT_ACCEPTED);
        }
        // RDD 190315 - ensure that Debit cards are not forced to credit if they don't support it for preauth
        else if ((txn.account == account_unknown_debit) || (txn.account == account_savings) || (txn.account == account_cheque))
        {
            TXN_RETURN_ERROR(ERR_CARD_NOT_ACCEPTED);
        }
        else
        {
            txn.account = account_credit;
            return state_skip_state();
        }
    }

    // RDD - account type already set by POS
    if (txn.pos_account)
    {
        return fEMV ? ts_get_pin : state_skip_state();
    }
#if 1
    if ((txn.type == tt_preauth) || (txn.type == tt_checkout) || (txn.type == tt_moto) || (txn.type == tt_moto_refund))
    {
        if (txn.account == account_unknown_debit)
        {
            TXN_RETURN_ERROR(ERR_CARD_NOT_ACCEPTED);
        }
        else
        {
            txn.account = account_credit;
            return state_skip_state();
        }
    }
#endif
    // RDD 040413 added for EPAL contactless
    if (GetEPALAccount() == VIM_ERROR_NONE)
    {
        return state_skip_state();
    }

    // RDD 020413 added for Electronic Gift Card
    if (transaction_get_status(&txn, st_electronic_gift_card))
    {
        txn.account = account_savings; // According to BD...
        return state_skip_state();
    }

    // RDD 120912 P3 added for Rimu card
    if (txn.type == tt_query_card)
    {
        txn.account = account_savings; // Force a fudged partial emv txn
        return state_skip_state();
    }
    // RDD 200512 SPIRA5539 Georgia somehow managed to get the PinPad to go into technical fallback from Account Selection state after a bad card.
    if (txn.error != ERR_SIGNATURE_APPROVED_OFFLINE)	// RDD 270814 - need to conserve this error for ALH Pre-auth completion !
        txn.error = VIM_ERROR_NONE;  // We shouldn't have to do this !

    if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE) return ts_error;
    if (card_picc == txn.card.type)
    {
        // RDD 270112 - Now we check CPAT for CTLS cards. If AGC say it's a debit then return to capture card with PICC disabled
        if ((txn.account == account_unknown_debit) || (CARD_REJECT_CTLS))
        {
            txn.card_state = card_capture_no_picc;
            if (CARD_REJECT_CTLS)
            {
                DBG_MESSAGE(" Contactless Disabled by CPAT for this card range");
                pceft_debug(pceftpos_handle.instance, "<CTLS OFF IN CPAT FOR THIS RANGE>");
            }
            // RDD 280812 - removed this as long beep is confusing now we do the shorter non-blocking one instead
            //vim_kbd_sound_invalid_key();

            return state_set_state(ts_card_capture);
        }
        else
        {
            txn.account = account_credit;
            return fEMV ? ts_get_pin : state_skip_state();
        }
    }
    // RDD- assume that for emv transactions the account has already been set
    DBG_VAR(txn.account);

    // RDD 011211 - added to flush extraneous key presses or screen touches before prompting for swipe
#if 0 // This causing crashes for some reason 
    if (vim_adkgui_is_active()) {
        VIM_EVENT_FLAG_PTR events[1];
        vim_adkgui_kbd_touch_or_other_read(&key_pressed, 0, events, 1);
    }
    else
    {
        vim_screen_kbd_or_touch_read(&key_pressed, 0, VIM_TRUE);
    }
#endif

    GetCardName(&txn, CardName);


    while ((txn.account == account_unknown_debit) || (txn.account == account_unknown_any))
    {
        if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE) return ts_error;

        // RDD 241012 SPIRA:6229 Cash on Credit for Scheme debit Cards now not allowed unless full EMV
        if ((txn.amount_cash) && (txn.card.type != card_chip) && (txn.account == account_unknown_any))
        {
            txn.account = account_unknown_debit;
        }


        // RDD 291217 get event deltas for logging
        vim_rtc_get_uptime(&txn.uEventDeltas.Fields.AccReq);

        //txn.account = account_credit;
        //return state_get_next();

        if (refresh_display)
        {
            screen_id = (txn.account == account_unknown_debit) ? DISP_SELECT_ACCOUNT_CHQ_SAV : DISP_SELECT_ACCOUNT;

            if ((res = display_screen(screen_id, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName )) != VIM_ERROR_NONE) TXN_RETURN_ERROR(res);
            refresh_display = VIM_FALSE;
        }

        /* wait for key (or abort)*/
        if ((res = screen_kbd_touch_read_with_abort(&key_pressed, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE)) != VIM_ERROR_NONE)
        {
            TXN_RETURN_ERROR(res);
        }


        // RDD 291217 get event deltas for logging
        vim_rtc_get_uptime(&txn.uEventDeltas.Fields.AccResp);

        VIM_DBG_KBD_CODE(key_pressed);
        /* Clear keymask */
        /*VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, 0));*/

         /* All ok so process the key */
        switch (key_pressed)
        {
        case VIM_KBD_CODE_SOFT1:
            vim_kbd_sound();
        case VIM_KBD_CODE_KEY1:
            if (!vim_strncmp(txn.host_rc_asc, "52", 2))
            {
                vim_kbd_sound_invalid_key();
                break;
            }
            txn.account = account_cheque;
            break;

        case VIM_KBD_CODE_SOFT2:
            vim_kbd_sound();
        case VIM_KBD_CODE_KEY2:
            if (!vim_strncmp(txn.host_rc_asc, "53", 2))
            {
                vim_kbd_sound_invalid_key();
                break;
            }
            txn.account = account_savings;
            break;

        case VIM_KBD_CODE_SOFT3:
            vim_kbd_sound();
        case VIM_KBD_CODE_KEY3:
            // RDD 080312: SPIRA 5030 sw was setting CR for Debit Cards if Key3 pressed
            if (!vim_strncmp(txn.host_rc_asc, "39", 2))
            {
                vim_kbd_sound_invalid_key();
                break;
            }
            if (txn.account == account_unknown_any)
            {
                txn.account = account_credit;
            }
            break;


        case VIM_KBD_CODE_CANCEL:
            if (IS_STANDALONE)
                TXN_RETURN_ERROR(VIM_ERROR_USER_CANCEL);

        case VIM_KBD_CODE_CLR:
            vim_kbd_sound_invalid_key();
            break;

        default:
            if (key_pressed == VIM_KBD_CODE_NONE)
                continue;
        }

    }
    // RDD 080212 - Save the transaction as we've changed it
    txn_save();

    return state_get_next();
}



txn_state_t txn_select_account( VIM_BOOL fEMV )
{
 // VIM_KBD_CODE key_pressed;
  VIM_CHAR acc_string[3+1];
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];

  get_acc_type_string_short(txn.account,acc_string);

  vim_mem_clear( CardName, VIM_SIZEOF(CardName) );

  DBG_MESSAGE("STATE: SELECT ACC");

  if (gift_card(&txn))
  {
      pceft_debug_test(pceftpos_handle.instance, "Gift Card force Sav Acc");
      txn.account = account_savings;
      return state_skip_state();
  }
  else if (fuel_card(&txn))
  {
      txn.account = account_default;
      return state_skip_state();
  }
  // RDD 200820 Trello #217
  if(txn.card.type == card_manual)
  {
      txn.account = account_credit;
      return state_skip_state();
  }

  //set_acc_from_agc(&txn.account);
  VIM_DBG_MESSAGE("-------------- Set AGC Account -------------");
  VIM_DBG_VAR(txn.account);

  if ((txn.account != account_unknown_any) && (txn.account != account_unknown_debit))
  {
      return state_skip_state();
  }

  // RDD 200115 SPIRA:8354 - deal with preauths including case where SAV or CHQ selected via Application selection
  if ((txn.type == tt_preauth) || (txn.type == tt_checkout) || (txn.type == tt_moto) || (txn.type == tt_moto_refund))
  {
	  if( txn.pos_account && txn.account != account_credit )
	  {
		  TXN_RETURN_ERROR( ERR_CARD_NOT_ACCEPTED );
	  }
	  // RDD 190315 - ensure that Debit cards are not forced to credit if they don't support it for preauth
	  if(( txn.account == account_unknown_debit ) || ( txn.account == account_savings ) || ( txn.account == account_cheque ))
	  {
		  TXN_RETURN_ERROR( ERR_CARD_NOT_ACCEPTED );
	  }
	  if (GetEPALAccount() == VIM_ERROR_NONE)
	  {
		  TXN_RETURN_ERROR( ERR_CARD_NOT_ACCEPTED );
	  }
	  else
	  {
		  txn.account = account_credit;
		  return state_skip_state();
	  }
  }

  // RDD - account type already set by POS
  if (txn.pos_account)
  {
	  return fEMV ? ts_get_pin : state_skip_state();
  }

  // RDD 040413 added for EPAL contactless
  if( GetEPALAccount() == VIM_ERROR_NONE )
  {
	  // RDD 06/04/22 Ctls PA EFTPOS not rejected
	  if(( txn.type == tt_preauth )||( txn.type == tt_completion ) || (txn.type == tt_checkout ))
	  {
		  TXN_RETURN_ERROR(ERR_CARD_NOT_ACCEPTED);
	  }
	  return state_skip_state();
  }

  // RDD 020413 added for Electronic Gift Card
  if( transaction_get_status( &txn, st_electronic_gift_card ))
  {
	  txn.account = account_savings; // According to BD...
	  return state_skip_state();
  }

  // RDD 120912 P3 added for Rimu card
  if( txn.type == tt_query_card )
  {
	  txn.account = account_savings; // Force a fudged partial emv txn
	  return state_skip_state();
  }
  // RDD 200512 SPIRA5539 Georgia somehow managed to get the PinPad to go into technical fallback from Account Selection state after a bad card.
  //if( txn.error != ERR_SIGNATURE_APPROVED_OFFLINE )	// RDD 270814 - need to conserve this error for ALH Pre-auth completion !
//	txn.error = VIM_ERROR_NONE;  // We shouldn't have to do this !

  if( EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE ) return ts_error;
  if (card_picc == txn.card.type)
  {
      DBG_POINT;
	  // RDD 270112 - Now we check CPAT for CTLS cards. If AGC say it's a debit then return to capture card with PICC disabled
	  if((  txn.account == account_unknown_debit ) || ( CARD_REJECT_CTLS ))
	  {
          DBG_POINT;
          DBG_MESSAGE("Contactless Unknown debit");
          vim_strcpy(txn.cnp_error, "FDY");

		  txn.card_state = card_capture_no_picc;
		  if( CARD_REJECT_CTLS )
		  {
			DBG_MESSAGE( " Contactless Disabled by CPAT for this card range" );
			pceft_debug( pceftpos_handle.instance, "<CTLS OFF IN CPAT FOR THIS RANGE>" );
		  }
		  // RDD 280812 - removed this as long beep is confusing now we do the shorter non-blocking one instead
		  //vim_kbd_sound_invalid_key();

		  return state_set_state( ts_card_capture );
	  }
	  else
	  {
		  txn.account = account_credit;
		  return fEMV ? ts_get_pin : state_skip_state();
	  }
  }
  // RDD- assume that for emv transactions the account has already been set
   DBG_VAR(txn.account);

  // RDD 011211 - added to flush extraneous key presses or screen touches before prompting for swipe
#if 0 // This causing crashes for some reason 
   if (vim_adkgui_is_active())
   {
       VIM_EVENT_FLAG_PTR events[1];
	   DBG_POINT;
       vim_adkgui_kbd_touch_or_other_read(&key_pressed, 0, events, 1);
	   DBG_POINT;
   }
#endif

   DBG_POINT;
  GetCardName( &txn, CardName );
  DBG_VAR( CardName );

  if (card_chip == txn.card.type)
  {
      if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE)
          return ts_error;
  }
  DBG_POINT;

  // RDD 241012 SPIRA:6229 Cash on Credit for Scheme debit Cards now not allowed unless full EMV
  if(( txn.amount_cash ) && ( txn.card.type != card_chip ) && ( txn.account == account_unknown_any ))
  {
      txn.account = account_unknown_debit;
  }
  DBG_POINT;

  // RDD 291217 get event deltas for logging
  vim_rtc_get_uptime( &txn.uEventDeltas.Fields.AccReq );

  DBG_POINT;
  {
      char *items[3];
      VIM_SIZE ItemIndex = 0;
      VIM_SIZE SelectedIndex = 0;
      VIM_SIZE MenuItems = (txn.account == account_unknown_debit) ? 2 : 3;

      // Debit Acc Only
      if (MenuItems == 2)
      {
          items[0] = "Savings Account";
          items[1] = "Cheque Account";
      }
      else
      {
          items[0] = CardName;
          items[1] = "Savings Account";
          items[2] = "Cheque Account";
      }
      VIM_DBG_NUMBER(MenuItems);
      pceft_pos_display("SELECT ACCOUNT", "ON PINPAD", VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_PROCESSING);

#ifndef _WIN32
      res = display_menu_select_zero_based(ACC_LIST_TITLE, ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, MenuItems, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE);
#else
      res = display_menu_select(ACC_LIST_TITLE, &ItemIndex, &SelectedIndex, (VIM_TEXT **)items, MenuItems, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE);
      SelectedIndex += 1;
#endif
      DBG_POINT;
      if (res == VIM_ERROR_NONE)
      {
          VIM_DBG_VAR(SelectedIndex);
          if (MenuItems == 3)
          {
              txn.account = account_default;
              if (SelectedIndex == 2)
                  txn.account = account_savings;
              else if (SelectedIndex == 3)
                  txn.account = account_cheque;
          }
          else
          {
              txn.account = account_savings;
              if (SelectedIndex == 2)
                  txn.account = account_cheque;
          }
      }
      else
      {
          display_result( res, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );
          TXN_RETURN_EXIT(res);
      }
  }

  // RDD 080212 - Save the transaction as we've changed it
  txn_save();

  return state_get_next();
}

/*----------------------------------------------------------------------------*/
txn_state_t contactless_fallback(VIM_ERROR_PTR error)
{
  VIM_BOOL term_efb_allowed = VIM_TRUE;
  if (term_efb_allowed)
  {
    txn.account = account_default;
    txn.card_name_index = 0;
    txn.issuer_index = 0xFF;
    txn.acquirer_index = 0;
    txn.card.data.picc.msd = 0;
    transaction_clear_status(&txn, st_partial_emv);
    txn.card_state = card_capture_comm_error_no_picc;
    return state_set_state(ts_card_capture);
  }

  if (ERR_WOW_ERC == error)
    txn.error = ERR_WOW_ERC;
  else if (!transaction_get_status(&txn, st_partial_emv))
  {
    vim_mem_copy(txn.host_rc_asc, "Z3", 2);
    txn.error = ERR_UNABLE_TO_GO_ONLINE_DECLINED;
  }

  return state_set_state(ts_finalisation);
}

VIM_ERROR_PTR txn_print_receipt_type(receipt_type_t type, transaction_t *txn)
{
  VIM_PCEFTPOS_RECEIPT receipt;
  if (type == receipt_customer)
  {
	  txn->customer_receipt_printed = VIM_FALSE;
  }

  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));

  VIM_ERROR_RETURN_ON_FAIL(receipt_build_financial_transaction(type, txn, &receipt));
	 //
  VIM_ERROR_RETURN_ON_FAIL(pceft_pos_print(&receipt, VIM_TRUE));
	 //
  if (type == receipt_customer)
  {
	  txn->customer_receipt_printed = VIM_TRUE;
  }
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static txn_state_t check_efb_type( void )
{
	//comms_disconnect();
	efb_start_timer();

	// RDD 300315 No obtain no trickle feed we need to set txn.error_backup
	txn.error_backup = txn.error;
#if 1	// RDD 280615 SPIRA:8342 preauths getting approved in EFB
	if ((card_chip == txn.card.type) && !transaction_get_status(&txn, st_partial_emv) && ( txn.type != tt_preauth))
#else
	if ((card_chip == txn.card.type) && !transaction_get_status(&txn, st_partial_emv))
#endif
	{
		return state_set_state(ts_efb);
	}
    else if (card_picc == txn.card.type)
	{
		 // RDD 310112 - remove this for now and process as a partial EMV EFB
         //return contactless_fallback(txn.error);
        return state_get_next();
	}
	else
	{
		// Next state is dependant on the transaciton type
        return state_get_next();
	}
}


/*----------------------------------------------------------------------------*/
VIM_UINT64 AscToBin(char const *asc, VIM_UINT8 len, VIM_BOOL *Invalid )
{
  VIM_UINT8 i;
  VIM_UINT64 res = 0;

  for (i=0;i<len;i++)
  {
    if(( asc[i] > '9' )||( asc[i] < '0' ))
	{
		*Invalid = VIM_TRUE;
		return 0;
	}
	res *= 10;
	res += (VIM_UINT64)(asc[i] & 0x0f);
  }
  *Invalid = VIM_FALSE;
  return res;
}
/*----------------------------------------------------------------------------*/

void txn_set_picc_online_result( VIM_BOOL online_OK, VIM_UNUSED(VIM_TEXT*, host_response_code) )
{
  VIM_ERROR_PTR error;


  // RDD 180418 - Don't send this at all !
  return;

  // RDD 080714 - no longer any need to send this - just causes error
  if( txn.card.type == card_picc && picc_handle.instance != VIM_NULL /* && txn.type == tt_sale */ )
  {
	  VIM_UINT8 temp_buffer[100];
      VIM_TLV_LIST temp_tlv;

// RDD 051016 - modified version for retries of failed J8 (Loyalty card not recognised)

#if 0
	  if( vim_mem_compare( terminal.emv_applicationIDs[txn.aid_index].emv_aid, VIM_EMV_RID_EPAL, 5 ) == VIM_ERROR_NONE )
	  {
			VIM_DBG_MESSAGE( "CTLS EPAL CARD - DO NOT ATTEMPT TO SEND ONLINE RESULT" );
	  }
	  // RDD 220812 SPIRA:5949 VISA Falls Back after some AMEX transactions. Seems random. Don't know why but this change seems to work....
	  if( vim_mem_compare( terminal.emv_applicationIDs[txn.aid_index].emv_aid, VIM_EMV_RID_MASTERCARD, 5 ) != VIM_ERROR_NONE )
	  {
		// RDD 290812 SPIRA:5973 - MSD Amex Card can kill the terminal
#if 1	// RDD 251012 : Note that you can't do two XP 04 txns in a row. The second one always falls back. Need an online result from another txn to reset the reader.
		if ( txn.card.data.picc.msd )
		{
			pceft_debug( pceftpos_handle.instance, "MSD Card: Do not send online result" );
			VIM_DBG_MESSAGE( "!!! MSD CARD - DO NOT ATTEMPT TO SEND ONLINE RESULT" );
			return;
		}
	  }
#endif
#endif
		VIM_DBG_MESSAGE( "set ONLINE RESULT TO PICC================" );
		vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
		vim_tlv_create(&temp_tlv,temp_buffer,VIM_SIZEOF(temp_buffer));
		if( online_OK )
		{
			vim_tlv_add_bytes(&temp_tlv, "\x8A", "Z3", 2 );
			vim_tlv_add_bytes(&temp_tlv, "\x91", "00", 0 );
			vim_tlv_add_bytes(&temp_tlv, "\x1F\x14", "00", 0 );
		}
		else
		{
			vim_tlv_add_bytes(&temp_tlv, "\x8A", "Z3", 2 );
			vim_tlv_add_bytes(&temp_tlv, "\x91", "00", 0 );
			vim_tlv_add_bytes(&temp_tlv, "\x1F\x14", "00", 0 );
		}

		DBG_MESSAGE( "<<<< About to Set online result to CTLS reader >>>>" );
#ifdef _DEBUG
		vim_event_sleep( 500 );
#endif

		if(( error = vim_picc_emv_complete_transaction(picc_handle.instance,temp_buffer,temp_tlv.total_length )) == VIM_ERROR_NONE )
		{
			DBG_MESSAGE( "<<<< Set online result to CTLS reader >>>>" );
			pceft_debug ( pceftpos_handle.instance, "Set online result to CTLS reader" );
		}
		else
		{
			DBG_MESSAGE( "<<<< Set online result to CTLS reader FAILED !!! >>>>" );
			pceft_debug_error( pceftpos_handle.instance, "Online result to CTLS reader failed", error );
		}
		txn.picc_online_result_sent = VIM_TRUE;
  }
}

// RDD 020714 added
VIM_AS2805_MESSAGE_TYPE GetMsgType( txn_type_t type )
{
	VIM_AS2805_MESSAGE_TYPE msg_type;

	switch( type )
	{
		case tt_query_card_get_token:
		 msg_type = MESSAGE_WOW_600;
		 break;

		case tt_preauth:
		 msg_type = MESSAGE_WOW_100;
		 break;

         // RDD JIRA PIRPIN-1038 Voids are sent as reversals
        case tt_void:
            msg_type = MESSAGE_WOW_420;
            reversal = txn;
            break;

		default:
		 msg_type = MESSAGE_WOW_200;
		 break;
	}
	return msg_type;
}


VIM_BOOL ConnectViaDialup( void )
{
	VIM_GPRS_INFO gprs_info;

	if( terminal.configuration.TXNCommsType[0] == PSTN )
	{
		DBG_MESSAGE( "Dial because PSTN Comms Selected by default" );
		return VIM_TRUE;
	}
	if( vim_gprs_get_info( &gprs_info ) != VIM_ERROR_NONE )
	{
		DBG_MESSAGE( "Dial because gprs_info returned an error" );
		return VIM_TRUE;
	}
	else if( gprs_info.signal_level_percent < 5 )
	{
		DBG_MESSAGE( "Dial because signal level < 5% " );
		return VIM_TRUE;
	}
	else if( gprs_info.con_network < 2 )
	{
		DBG_MESSAGE( "Dial because no network" );
		return VIM_TRUE;
	}
	DBG_MESSAGE( "Pre-dial not required" );
	return VIM_FALSE;
}

VIM_BOOL IsDocked( void )
{
	// Assume P400 by default
	VIM_BOOL is_docked = VIM_TRUE;	
	// RDD 100522 JIRA PIRPIN-1606
	if(IS_V400m)
	{
		vim_terminal_get_dock_status(&is_docked);
	}
	//VIM_DBG_VAR(is_docked);
	return is_docked;
}



VIM_BOOL IsCableConnected( void )
{
	VIM_BOOL is_connected = VIM_FALSE;
	vim_terminal_get_cable_status( &is_connected );
	return is_connected;
}




txn_state_t txn_establish_network( void )
{
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;
	return state_get_next();
}


void ConditionalDisconnect( void )
{
	if( IS_INTEGRATED )
		return;

	if( reversal_present() && comms_connected(HOST_WOW) )
	{

		DBG_VAR( terminal.trickle_feed );

		terminal.trickle_feed = VIM_TRUE;
		return;
	}

	if( PRIMARY_TXN_COMMS_TYPE == PSTN  )
	{
		if(( txn.error == VIM_ERROR_NONE ) || ( txn.error == ERR_WOW_ERC ))
		{
			if( !vim_strncmp( txn.host_rc_asc, "00", 2 ) )
				return;
			if( !vim_strncmp( txn.host_rc_asc, "76", 2 ) )
				return;
		}
		VIM_DBG_MESSAGE( "DISCONNECT BECAUSE RC WASN'T 00 or 76" );
		comms_disconnect();
		//terminal.trickle_feed = VIM_FALSE;
	}
	return;
}

static VIM_BOOL AllowPiggyBack(void)
{
	// No PiggyBack if there was some error before EFB Processing (eg. Comms Error)
	// RDD 230516 SPIRA:8949 - changed this for V560 as too many good conditions can cause error backup to be set when we want trickle feed
#if 0
	if( txn.error_backup == VIM_ERROR_NONE )
	{
		VIM_DBG_ERROR( txn.error_backup );
		return VIM_FALSE;
	}
#else
	if( transaction_get_status( &txn, st_needs_sending ))
	{
		return VIM_FALSE;
	}
#endif

	// Allow PiggyBack if some current Error or transaction staus indicates txn did not go online
	if ( (( txn.error == VIM_ERROR_NONE ) && ( (!transaction_get_status(&txn, st_needs_sending)) && (!transaction_get_status(&txn, st_offline)) && (!transaction_get_status(&txn, st_efb) )))  )
	{
		return VIM_TRUE;
	}

	// also Allow PiggyBack if reversal is present and we're not printing a Power down recovery receipt (comms may not be up)
	if( reversal_present() && ( txn.error != ERR_POWER_FAIL))
	{
		return VIM_TRUE;
	}

	//VIM_DBG_ERROR( txn.error_backup );
	VIM_DBG_ERROR( txn.error );
	// All other conditions don't allow PiggyBack
	return VIM_FALSE;
}


VIM_BOOL MC_Cert_Fallback( void )
{
	// RDD - this only applies to RC 65
	if ( vim_strncmp ( txn.host_rc_asc, "65", 2 ))
		return VIM_FALSE;

    if(( card_picc == txn.card.type ) && ( CardIndex(&txn) == MASTERCARD ))
		return VIM_TRUE;
	else
		return VIM_FALSE;
}


/*----------------------------------------------------------------------------*/
/* Processes a transaction */
txn_state_t txn_authorisation_processing( void )
{
  VIM_UINT8 erc;
  VIM_UINT8 host = HOST_WOW;
  //VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_AS2805_MESSAGE msg;
  VIM_BOOL picc_is_online=VIM_FALSE;
  VIM_BOOL fForceOnline = VIM_FALSE;

  VIM_RTC_UPTIME start_ticks,end_ticks;

  VIM_AS2805_MESSAGE_TYPE msg_type = GetMsgType( txn.type );

  // WOW special for gift card processing
  DBG_MESSAGE("STATE: TXN AUTHORISATION PROCESING");
  VIM_DBG_ERROR( txn.error );
  vim_rtc_get_uptime(&start_ticks);

  // RDD 160412: SPIRA:5268 Moved here because of problems with interrupted SAF prints
  // RDD 181114: SPIRA:8255 Need this earlier now because need to report timeouts etc.
  //transaction_set_status(&txn, st_incomplete);
  terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855

  // RDD 270212 - Training mode simulation- add online delay type thing
  if( transaction_get_training_mode() )
  {
	  VIM_UINT64 rc = txn.amount_purchase%100;
	  DisplayProcessing( DISP_PROCESSING_WAIT );
	  bin_to_asc( rc, txn.host_rc_asc, 2 );
      vim_event_sleep(2000);
	  // No EFB in training mode (except for 91 response)
	  return ( rc == 91 ) ? state_set_state(ts_efb) : state_set_state(ts_finalisation);
  }

  DBG_POINT;
  gift_card(&txn);

  /* check if we should go into EFB without attempting to go online */
  if( efb_dont_attempt_online() )
  {
      txn.print_receipt = VIM_TRUE;
      txn_save();
      return state_set_state(ts_efb);	// RDD 300112 - EFB Processing
  }

  // Trello #272 ensure that if we exit at this point then txn.error is set to an actual error and just check for removal prior to sending bank message
#if 0
  // RDD Check for Card Removal
  // RDD 130214 SPIRA 7301 - ts_error being returned from txn_authorisation_processing() not being handled. This causes Txn to be approved but not saved or sent online.
  if( EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE ) return ts_error;
#endif

  /* enter into normal processing, enable receipt print */
  txn.print_receipt = VIM_TRUE;

  txn_save();

  // RDD 150312 - move the processing message here as the last screen was being displayed if there was no comms and the pinpad was trying to connect
#if 1
  // RDD 100616 SPIRA:8995 - need processing to be display AFTER reversal/advice etc.
  DisplayProcessing( DISP_PROCESSING_WAIT );
#endif
  DBG_POINT;

#if 0
  // RDD Check for Card Removal
  // RDD 130214 SPIRA 7301 - ts_error being returned from txn_authorisation_processing() not being handled. This causes Txn to be approved but not saved or sent online.
  if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE)
  {
      // Trello #272 ensure that if we exit at this point then txn.error is set to an actual error
      if (txn.error == VIM_ERROR_NONE)
      {
          pceft_debug(pceftpos_handle.instance, "Trello #272");
          txn.error = VIM_ERROR_SYSTEM;
      }
      return ts_error;
  }
#endif

  DBG_POINT;

  if (transaction_get_training_mode())
  {
    /* We take the cents and use that as the response code */
    bin_to_asc(txn.amount_total % 100, txn.host_rc_asc, 2);
    vim_mem_copy(txn.auth_no, "123456", 6);

    erc = asc_to_bin(txn.host_rc_asc, 2);

    /* Add a delay in training mode */
    vim_event_sleep(1000*3);
  }
  else
  {
	  if (card_picc == txn.card.type)
	  {
		  VIM_PICC_EMV_STATUS status;
		  VIM_TLV_LIST tags;
		  if (VIM_NULL == picc_handle.instance)
			  return state_get_next();
		  /* get PICC tags for all scenarioes */
		  vim_picc_emv_get_transaction_status(picc_handle.instance, &status);
		  DBG_MESSAGE("<<<<<<<<<< PICC ONLINE STATUS START >>>>>>>>>");
		  DBG_VAR(status);
#if 1
		  // RDD 060422 - hack to fix JIRA PIRPIN-1554
		  if ((txn.account == account_savings || txn.account == account_cheque) && (txn.type == tt_preauth || txn.type == tt_checkout || txn.type == tt_completion))
		  {
			  txn.print_receipt = VIM_FALSE;
			  TXN_RETURN_ERROR(ERR_CARD_NOT_ACCEPTED);
		  }
#endif

#ifdef _nnXPRESS
		  // RDD 051212 Fiddle this for now as there seems to be a problem in the reader firmware for Amex
		  if (status == VIM_PICC_EMV_STATUS_AUTHORIZATION_REQUEST)
		  {
			  if (txn.amount_total < (terminal.emv_applicationIDs[txn.aid_index].floor_limit))
			  {
				  DBG_MESSAGE("< Reader Ignored floor limit so fix this >");
				  status = VIM_PICC_EMV_STATUS_APPROVED;
			  }
		  }
#endif
		  DBG_MESSAGE("<<<<<<<<<< AID INFO: >>>>>>>>>");
		  DBG_VAR(txn.aid_index);
		  DBG_MESSAGE(terminal.emv_applicationIDs[txn.aid_index].aid_display_text);
		  DBG_VAR(terminal.emv_applicationIDs[txn.aid_index].floor_limit);
		  DBG_MESSAGE("<<<<<<<<<< PICC ONLINE STATUS END >>>>>>>>>");

		  /* get PICC tags for future use */

#ifdef _OLD_PICC_TAGS
		  picc_tags();
#endif
#ifndef _PHASE4
		  txn_save();
#endif
		  // RDD Refunds always go online
		  if (tt_refund == txn.type)
		  {
			  //vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PROCESSING_ONLINE, VIM_FALSE);
			  picc_is_online = VIM_TRUE;
		  }
		  else
		  {
			  switch (status)
			  {
				  /* approved offline*/
			  case VIM_PICC_EMV_STATUS_APPROVED:

				  if (fForceOnline)
				  {
					  //vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PROCESSING_ONLINE, VIM_FALSE);
				  }
				  else
				  {
					  transaction_set_status(&txn, st_offline + st_needs_sending);
					  txn.saf_reason = saf_reason_offline_emv_purchase;
					  txn.error = VIM_ERROR_NONE;
					  DBG_VAR(txn.emv_data.data_size);
					  DBG_VAR(txn.emv_data.buffer);
					  vim_strcpy(txn.cnp_error, "Y1");

					  vim_mem_copy(txn.host_rc_asc, "Y1", VIM_SIZEOF(txn.host_rc_asc));
					  txn.saf_reason = saf_reason_offline_emv_purchase;


					  // RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data
					  if (vim_tlv_open(&tags, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE) == VIM_ERROR_NONE)
						  vim_tlv_update_bytes(&tags, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2); // RDD changed 240112
					  txn.emv_data.data_size = tags.total_length;
					  DBG_VAR(txn.emv_data.data_size);
					  DBG_VAR(txn.emv_data.buffer);

					  return ts_finalisation;
				  }
				  break;

				  /* go online */
			  case VIM_PICC_EMV_STATUS_AUTHORIZATION_REQUEST:
				  //VIM_DBG_MESSAGE( "PICC MUST GO ONLINE ========================" );
				 //vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PROCESSING_ONLINE, VIM_FALSE);
				  picc_is_online = VIM_TRUE;
				  break;

				  /* declined */
			  case VIM_PICC_EMV_STATUS_DECLINED:
				  /*if (tt_refund == txn.type)
				  {
				   //VIM_
					vim_picc_emv_show_message(picc_handle.instance, VIM_PICC_EMV_DISPLAY_MESSAGE_ID_PROCESSING_ONLINE, VIM_FALSE);
					break;
				  }*/

			  case VIM_PICC_EMV_STATUS_NOT_AVAILABLE:
			  default:
				  //VIM_
				  txn.error = VIM_ERROR_PICC_CARD_DECLINED;
				  vim_strcpy(txn.cnp_error, "Z1");

				  return ts_finalisation;
				  //return state_get_next();
			  }
		  }
	  }

	  // RDD 100616 SPIRA:8995 - need processing to be display AFTER reversal/advice etc.
	  DisplayProcessing(DISP_PROCESSING_WAIT);

	  if (!QC_TRANSACTION(&txn))
	  {
		  if ((txn.error = comms_connect(host)) != VIM_ERROR_NONE)
		  {
			  // No comms so try to authorise the transaction offline			
			  if (picc_is_online)
			  {
				  txn_set_picc_online_result(VIM_FALSE, "00");
			  }
			  // RDD 300315 No obtain no trickle feed we need to set txn.error_backup		  
			  txn.error_backup = txn.error;
			  return check_efb_type();
		  }
		  else
		  {
			  // RDD 150522 JIRA PIRPIN-1603 No SAF Processing if QC Transaction. Run it only if good Comms and not QC
			  // We've got comms so Get rid of a possible outstanding reversal and/or 220 repeat (221)
			  if ((txn.error = ReversalAndSAFProcessing(SEND_NO_SAF, VIM_TRUE, VIM_TRUE, VIM_TRUE)) != VIM_ERROR_NONE)
			  {
				  // Some comms error so try to authorise the transaction offline		  
				  // JIRA PIRPIN-1571 : Offline Reversal - but we're still offline so process the new sale offline.
				  if (txn.error == ERR_NO_RESPONSE)
				  {
					  if (picc_is_online)
						  txn_set_picc_online_result(VIM_FALSE, "00");
					  return check_efb_type();
				  }
				  else
				  {
					  // RDD 260422 - some unknown error occurs during reversal proc. and txn isn't sent online	- bomb out with an error
					  pceft_debug_error(pceftpos_handle.instance, "JIRA PIRPIN-1571", txn.error);
					  TXN_RETURN_ERROR(txn.error);
				  }
			  }
		  }
	  }

	  // RDD Check for Card Removal
	  //if( EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE ) return ts_error;


	  // RDD Check for Card Removal
	  //if( EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE ) return ts_error;

	  // RDD 140112 - moved here because EMB_CB Pin Entry not called

	  VIM_DBG_NUMBER(txn.pin_block_size);

	  VIM_DBG_WARNING("NNNNNNNNNNNNNNN Null Pinblock Generation NNNNNNNNNNNNNNNNNN");
	  // RDD 050919 JIRA WWBP-704 Null Pinblock not generated when offline PIN entered
	  if ((txn.pin_block_size == 0) || (txn.emv_pin_entry_params.offline_pin) || (transaction_get_status(&txn, st_offline_pin)))
	  {
		  /* generate a VIM_NULL PIN block */
		  VIM_BOOL fEMV = (txn.card.type == card_chip) ? VIM_TRUE : VIM_FALSE;
		  VIM_DBG_VAR(fEMV);
		  txn_pin_generate_NULL_PIN_block(fEMV);
	  }

	  DBG_MESSAGE("!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");
	  DBG_VAR(txn.emv_data.data_size);
	  DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);
	  DBG_MESSAGE("!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");

	  // RDD Check for Card Removal
	  // RDD 130214 SPIRA 7301 - ts_error being returned from txn_authorisation_processing() not being handled. This causes Txn to be approved but not saved or sent online.
	  if ((txn.error = EmvCardRemoved(VIM_TRUE)) != VIM_ERROR_NONE)
	  {
		  TXN_RETURN_ERROR(txn.error);
	  }

	  // RDD 100616 SPIRA:8995 - need processing to be display AFTER reversal/advice etc.
	  //DisplayProcessing( DISP_PROCESSING_WAIT );

	  // RDD 161221 JIRA PIRPIN-1343 Qwikcilver SAFs going into Payment App queue. This is unnecessary for QC and will burn a STAN
	  if (QC_TRANSACTION(&txn) == VIM_FALSE)
	  {
		  if ((txn.error = msgAS2805_create(txn.acquirer_index, &msg, msg_type)) != VIM_ERROR_NONE)
		  {
			  TXN_RETURN_ERROR(txn.error);
		  }
		  txn.stan = msg.stan;

		  // RDD 250516 noticed that Power Fail after response received but before parsing sometimes creates issues with the reversal gen.
		  txn_save();

		  // Send the message
		  // RDD 300819 JIRA WWBP-696 Power fail reversal is not generated when power is removed before ACK has been sent back from Client
		  // clear this reversal immediatly else we will generate unneccessry reversals if client is disconnected etc.
		  // This reversal is only for Power fail prior to receipt of ACK.
		  reversal_store_original_stan(&txn, VIM_FALSE);
		  reversal_set(&txn, VIM_FALSE);
	  }

	  if (QC_TRANSACTION(&txn))
	  {
		  // RDD for returns card activations we cannot afford to be offline as a SAF will be created by 82002 VAA
		  if ((txn.error = QC_CreateSendMsg(&txn)) != VIM_ERROR_NONE)
		  {
			  vim_strcpy(txn.host_rc_asc, "X0");
			  if ((IsReturnsCard(&txn) && (txn.type == tt_gift_card_activation)))
			  {
				  pceft_debug(pceftpos_handle.instance, "QC Is Offline so abort Returns Card Activation");
				  return ts_finalisation;
			  }
		  }
	  }
	  else
	  {
		  if ((txn.error = comms_transmit(host, &msg)) != VIM_ERROR_NONE)
		  {
			  // Some comms error so try to authorise the transaction offline
			  // RDD 300819 JIRA WWBP-696 Power fail reversal is not generated when power is removed before ACK has been sent back from Client
			  reversal_clear(VIM_FALSE);
			  return check_efb_type();
		  }

		  // RDD: Txn was sucessfully sent - set the "sent" flag for Power Down recovery
		  //display_screen(DISP_PROCESSING_WAIT, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total) );
		  vim_rtc_get_uptime(&start_ticks);
		  // RDD 291217 get event deltas for logging

		  txn.uEventDeltas.Fields.MsgTx = start_ticks;
		  /* Set the transaction state to sent */
		  transaction_clear_status(&txn, st_needs_sending);
		  transaction_set_status(&txn, st_message_sent);
	  }
	  txn_save();

	  // RDD 240418 JIRA-WWBP217 Reverse this change as reversal is cleared before sig decline and then set again with a new STAN which makes the reversal invalid.
	  // Move the reversal VN 160218
	  // RDD 300819 No No No - the reversal must be set prior to the message being sent in order to account for Power fail
	  // reversal_set(&txn);

	  if (QC_TRANSACTION(&txn))
	  {
		  txn.error = QC_ReceiveParseResp(&txn);
		  txn_save();
	  }
	  else if ((txn.error = comms_receive(&msg, VIM_SIZEOF(msg.message))) != VIM_ERROR_NONE)
	  {
		  VIM_DBG_ERROR(txn.error);
		  reversal_set(&txn, VIM_FALSE);
		  //comms_disconnect();
		  // RDD 140920 - Card Removed check on full emv here will reset txn.error
		  if (txn.error == VIM_ERROR_EMV_CARD_REMOVED)
		  {
			  VIM_DBG_MESSAGE("RRRRRRRR Full EMV CARD REMOVED RRRRRRRRRR");
			  vim_strcpy(txn.cnp_error, "CRR");

			  return ts_finalisation;
		  }
		  else
		  {
			  return check_efb_type();
		  }
	  }
	  else
	  {
		  if ((txn.error = msgAS2805_parse(txn.acquirer_index, msg_type, &msg, &txn)) == VIM_ERROR_AS2805_STAN_MISMATCH)
		  {
			  // Shadow message off network - probably repeat of previous
			  pceft_debug_test(pceftpos_handle.instance, "AS2805: JIRA PIRPIN-1469 Repeated Message detected(t)");
			  if ((txn.error = comms_receive(&msg, VIM_SIZEOF(msg.message))) != VIM_ERROR_NONE)
			  {
				  VIM_DBG_ERROR(txn.error);
				  reversal_set(&txn, VIM_FALSE);
				  VIM_DBG_MESSAGE("DISCONNECT BECAUSE RECEIVE FAILED");
				  comms_disconnect();
				  // RDD 140920 - Card Removed check on full emv here will reset txn.error
				  if (txn.error == VIM_ERROR_EMV_CARD_REMOVED)
				  {
					  VIM_DBG_MESSAGE("RRRRRRRR Full EMV CARD REMOVED RRRRRRRRRR");
					  vim_strcpy(txn.cnp_error, "CRR");

					  return ts_finalisation;
				  }
				  else
				  {
					  return check_efb_type();
				  }
			  }
			  else
			  {
				  VIM_DBG_MESSAGE("SSSSSSSSSSSSS Shadow message override. Real msg rx OK ! ");
				  if ((txn.error = msgAS2805_parse(txn.acquirer_index, msg_type, &msg, &txn)) != VIM_ERROR_NONE)
				  {
					  VIM_DBG_MESSAGE("Real Message Parsed Fail");
				  }					
			  }
		  }
	  }

	  // Trello #272 ensure that if we exit at this point then txn.error is set to an actual error
	  if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE)
	  {
		  reversal_set(&txn, VIM_FALSE);
		  vim_strcpy(txn.cnp_error, "CRS");

		  VIM_DBG_MESSAGE("RRRRRRRR CARD REMOVED RRRRRRRRRR");
		  txn.error = VIM_ERROR_EMV_CARD_REMOVED;
		  return ts_finalisation;
	  }

	  //RDD 170513 Get the lapsed time for the stats
	  vim_rtc_get_uptime(&end_ticks);
	  txn.uEventDeltas.Fields.MsgRx = end_ticks;

	  if (end_ticks > start_ticks)
	  {
		  VIM_DBG_NUMBER(end_ticks - start_ticks);
		  SetResponseTimeStats(end_ticks - start_ticks);
	  }

	  if (txn.error != VIM_ERROR_NONE)
	  {
		  // RDD 130520 Trello #139 Double Debit can occur if parsing error - eg STAN or msg type mismatch.
		  reversal_set(&txn, VIM_FALSE);
		  //    reversal_set( &txn );
		  if (picc_is_online)
			  txn_set_picc_online_result(VIM_FALSE, "00");
		  return ts_error;
	  }

	  if (1)
	  {
		  VIM_BOOL NonNumeric = VIM_FALSE;
		  /* Convert response code */
		  erc = AscToBin(VIM_CHAR_PTR_FROM_UINT8_PTR(txn.host_rc_asc), 2, &NonNumeric);
		  if (NonNumeric) erc = 0xFF;
	  }
  }
  
  /////////////////////// CLEAR REVERSAL HERE ///////////////////////////
   reversal_clear(VIM_FALSE);	// Don't add to SAF as we didn't actually send a reversal
  /////////////////////// CLEAR REVERSAL HERE ///////////////////////////

    /* Set the transaction state to sent */
    //transaction_clear_status( &txn, st_needs_sending );
    //transaction_set_status(&txn, st_message_sent);

   //VIM_DBG_MESSAGE("+++++++++++++++++++++++++++++++++ after response +++++++++++");
  if( txn.card.type == card_picc && picc_handle.instance != VIM_NULL && picc_is_online )
  {
    if( erc == RC08 ||erc == RC11 )
      txn_set_picc_online_result(VIM_TRUE,"00");
    else
      txn_set_picc_online_result(VIM_FALSE,txn.host_rc_asc);

	// RDD 030914 - Update 8A so we can send up with any completion data
	/*  Get result of EMV transaction */
	if( txn.type == tt_preauth )
	{
		VIM_TLV_LIST tag_list;
		vim_tlv_open( &tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE );
		vim_tlv_update_bytes( &tag_list, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2 );
	}
  }

  // RDD 270515 SPIRA:
  ConditionalDisconnect();

  switch (erc)
  {

   ///////////////// TEST TEST TEST //////////////
	//case RC12:
   ///////////////// TEST TEST TEST //////////////

    case RC52: // no chq account
	case RC53: // no sav account
	case RC39: // no credit account
	case RC42: // invalid account
		// Reset account selection parameters
		//if(( txn.card.type == card_mag )||(( txn.card.type == card_chip ) && ( !FullEmv() )) )
		if( txn.card.type == card_mag )
		{
			if( txn.qualify_svp == VIM_FALSE )
			{
				display_result( txn.error, txn.host_rc_asc, "" ,"", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );

#if 1			// RDD 221112 SPIRA:6372 - Error in receipt printing rejected account type becuase we reset in OtherCpatChecks so move printing above that...
				// RDD 010812 SPIRA:5812 - Need to Print Pin Retry Advice Receipt and Journal the Pin Retry
				if( !transaction_get_training_mode() )
				{
					txn_print_receipt_type(receipt_advice, &txn);
					pceft_journal_txn(journal_type_txn, &txn, VIM_FALSE);
				}
#endif

				OtherCpatChecks();

				   ///////////////// TEST TEST TEST //////////////
					//txn.account = account_unknown_any;
					///////////////// TEST TEST TEST //////////////

				if(( txn.account == account_unknown_any ) || ( txn.account == account_unknown_debit ))
				{
					vim_kbd_sound_invalid_key();
					reversal_clear(VIM_FALSE);

					// RDD 080812 SPIRA:5865 We need to clear the "sent" status flag else a reversal will be generated if we pull the card during a/c selection
					transaction_clear_status(&txn, st_message_sent);
					txn_save();

	  				return state_set_state(ts_select_account);
				}
			}
	    }
		txn.qualify_svp = VIM_FALSE;
		txn.error = ERR_WOW_ERC;
        vim_strcpy(txn.cnp_error, "CRA");
		return state_set_state(ts_finalisation);

    case RC08:
    case RC11:
    // RDD 281211 - If SVP then no signature + remap any response code to 00
	// RDD 281211 - any 08 or 11 gets translated to 00 for SVP

	if(( txn.qualify_svp ) && ( txn.type == tt_sale ))
	{
	 //VIM_
	  vim_mem_copy( txn.host_rc_asc, "00", 2 );
	}
	// Fall through..

    case RC00:
      txn.error = VIM_ERROR_NONE;
	  // RDD 300112 - skip EFB altogether
	  DBG_VAR( txn.qualify_svp );
	  return state_set_state(ts_finalisation);

    case RC91:
		// 91 - issuer not available should be treated like a comms error and go to EFB
		break;

	case RC55:
		txn.error = ERR_WOW_ERC;
		return state_set_state(ts_finalisation);

    default:
		// RDD 281211: If we're here then the txn no longer qualifies for SVP
		DBG_MESSAGE( "!!!!!!!!!!!!! SET ERR_WOW_ERC !!!!!!!!!!!!!" );
		txn.qualify_svp = VIM_FALSE;
		txn.error = ERR_WOW_ERC;
		return state_set_state(ts_finalisation);
  }
  //comms_disconnect();
  return state_get_next();
  }


/*----------------------------------------------------------------------------*/

/* Perform any processing to be done when txn can be considered complete. i.e. add to saf, update totals, clear incomplete flag */
VIM_ERROR_PTR txn_commit(void)
{
  transaction_t tmp;
  vim_mem_clear(&tmp, VIM_SIZEOF(tmp));

  // RDD 060814 Added for ALH Shift totals
  DBG_POINT;
  if( is_approved_txn( &txn ))
  {
	  shift_totals_update( &shift_details.totals, &txn );
  }
  DBG_POINT;

  //transaction_clear_status(&txn, st_incomplete);
  //txn_save();

  // RDD - don't store traning mode txns
  if( transaction_get_training_mode() )
  {
	  pceft_debug_test(pceftpos_handle.instance, "Error: Exit due to training mode");
	  return VIM_ERROR_NONE;
  }
  DBG_POINT;

  //////////////////////// TEST TEST TEST /////////////////////////
	// return VIM_ERROR_OVERFLOW;
  //////////////////////// TEST TEST TEST /////////////////////////


  // RDD 180814 ALH Project - Store PreAuths in special file for possible checkout later
  if(( txn.type == tt_preauth ) && ( is_approved_txn(&txn) ))
  {
	  pceft_debug_test(pceftpos_handle.instance, "Approved Preauth Save duplicate");
	  txn_duplicate_txn_save();
	  VIM_ERROR_RETURN_ON_FAIL( saf_add_record(&txn, VIM_FALSE) );
	  pceft_debug_test(pceftpos_handle.instance, "Approved Preauth SAF Saved OK");
  }

  // RDD - don't store if Txn was approved or declined online
  if( (( txn.error == VIM_ERROR_NONE ) || (txn.error == ERR_WOW_ERC )) && ( !transaction_get_status( &txn, st_needs_sending )) )
  {
	  // RDD 210212 - surviving 91 response codes must always be saved
	  if( vim_strncmp( txn.host_rc_asc, "91", 2 ))
      {
          // RDD 170621 JIRA PIRPIN-1038 : VOID over entire batch + extend batch
          // RDD 260821 - Implement later as issues with SAF total limits
          if(0)
          //if( !TERM_USE_PCI_TOKENS)
          {
              VIM_ERROR_RETURN_ON_FAIL(saf_add_record(&txn, VIM_FALSE));
              pceft_debug_test(pceftpos_handle.instance, "Txn was saved for VOID purposes");
          }
          else
          {
              return VIM_ERROR_NONE;
          }
      }
  }

  // RDD - don't store if a completion with an invalid Auth number
  if(( txn.type == tt_completion )||( txn.type == tt_checkout ))
  {
	  if( !ValidateAuthCode( txn.auth_no ))
	  {
		  return VIM_ERROR_NONE;
	  }
  }

  // RDD 240814 added for ALH Pre-auth Completion - need to mark record as completed so can't get used again.
  if(( tt_checkout == txn.type ) && ((txn.error == VIM_ERROR_NONE )|| (txn.error == ERR_SIGNATURE_APPROVED_OFFLINE )))
  {
	  VIM_DBG_VAR( txn.original_index );

      if( VIM_ERROR_NONE == saf_read_record_index( txn.original_index, VIM_TRUE, &tmp ))
      {
        VIM_DBG_POINT;
#if 0
        transaction_set_status( &tmp, st_checked_out );

		//saf_update_record( txn.original_index, VIM_FALSE, &tmp );
		saf_update_record( txn.original_index, VIM_FALSE, &tmp );
		// RDD 140116 Need to add the recent completed Preauth to the SAF - this is the index from the preauth file !!!
		// RDD 140116 Found a bug where if first completion was from scratch it never got sent because flag wasn't set
		// We need to add this record to the SAF as well as updating the preauth record
#else
		if( transaction_get_status( &tmp, st_checked_out ) == VIM_FALSE )
		{
            VIM_SIZE Expired = 0, Completed = 0;
            transaction_set_status( &tmp, st_checked_out );
			transaction_set_status( &tmp, st_needs_sending );
			transaction_clear_status( &tmp, st_message_sent );
			// RDD 080316 SPIRA:8916 Need to preserve the new amount to be sent in the 0220
			tmp.amount_total = txn.amount_total;
			tmp.amount_purchase = txn.amount_purchase;

			tmp.type = tt_checkout;
			//saf_update_record( txn.original_index, VIM_FALSE, &tmp );
			saf_add_record( &tmp, VIM_FALSE );
			preauth_table_update_record( txn.original_index, &tmp );
            PurgePreauthFiles(&Completed, &Expired);
			return VIM_ERROR_NONE;
		}
		else
		{
	        transaction_set_status( &tmp, st_checked_out );
            saf_update_record(txn.original_index, VIM_FALSE, &tmp);
        }
		preauth_table_update_record( txn.original_index, &tmp );
#endif

      }
  }

  // RDD 260312 : SPIRA 5176 - ensure that 91 do manual that 200 was never sent do not get reversed
  // We do this by setting the backup response code to 91 as this may get modified if EFB Passes. If EFB
  // Fails then we only want to reverse 91 erros that we generated due to Comms errors ( 200 was sent )
  if( !vim_strncmp( txn.host_rc_asc_backup, "91", 2 ))
  {
	  /////////////////////// CLEAR REVERSAL HERE ///////////////////////////
	  reversal_clear(VIM_FALSE);	// Don't add to SAF as we didn't actually send a reversal
	  /////////////////////// CLEAR REVERSAL HERE ///////////////////////////
  }

  // RDD 210212 - add "91 DO MANUAL " TXNS to the card buffer
#if 0
  if( !vim_strncmp( txn.host_rc_asc, "91", 2 ) )
#else
  // RDD 260615 SPIRA:8518 Preauths with 91 RC were getting stored in PA batch
  if(( !vim_strncmp( txn.host_rc_asc, "91", 2 ) ) && ( IS_INTEGRATED ))
#endif
  {
	  VIM_ERROR_RETURN_ON_FAIL( saf_add_record(&txn, VIM_TRUE) );
	  pceft_debug(pceftpos_handle.instance, "SAF 91 Txn Added to Buffer" );
	  DBG_MESSAGE( "SAF 91 Txn Added to Buffer" );
  }


  // RDD - any approved EMV transaction that is labelled as needs_sending should be stored in SAF
  else if( transaction_get_status( &txn, st_needs_sending ))
  {
	  transaction_clear_status( &txn, st_message_sent );
	  DBG_DATA( txn.emv_data.buffer, txn.emv_data.data_size );
	  VIM_ERROR_RETURN_ON_FAIL( saf_add_record(&txn, VIM_FALSE) );
	  DBG_MESSAGE( "<<<<<< TXN ADDED TO SAF FILE >>>>>>>>");
  }


  // RDD 290412 SPIRA:5357 Reversal missing some data generated during the journel because reversal set prematurely
  // RDD 090512 SPIRA:5471 DO NOT reset the reversal here if finalisation called by power fail as this can overwite reversal with current txn data
  if(( terminal.reversal_count )&&( txn.error != ERR_POWER_FAIL))
  {
	  reversal_set( &txn, VIM_FALSE);
  }


  // RDD 300112 - below "original" logic for storing advices commented out because WOW treats Z3 as a
  // valid financial transaction if sent as an advice... We only want Z3 txns that have passed EFB to be stored
  // in SAF.
#if 0
  // RDD - original logic below...
  else if((!transaction_get_status(&txn, st_partial_emv)) && (transaction_get_status(&txn, st_offline)) &&
         (txn.error == ERR_UNABLE_TO_GO_ONLINE_DECLINED || txn.error == ERR_EMV_CARD_DECLINED) )
  {
	  transaction_set_status( &txn, st_needs_sending );
	  VIM_ERROR_RETURN_ON_FAIL( saf_add_record(&txn) );
  }
#endif

  pceft_debugf_test( pceftpos_handle.instance, "Txns in SAF: %d", saf_totals.fields.saf_print_count );
  pceft_debugf_test( pceftpos_handle.instance,  "SAFS in SAF: %d", saf_totals.fields.saf_count );
  return VIM_ERROR_NONE;
}



txn_state_t txn_display_error(void)
{
  /* For these errors just beep and return to idle */
  if ( (txn.error == VIM_ERROR_TIMEOUT) || (txn.error == VIM_ERROR_USER_CANCEL)
    || (txn.error == VIM_ERROR_USER_BACK) || (txn.error == VIM_ERROR_POS_CANCEL) )
  {
   //VIM_
     vim_kbd_sound_invalid_key();
     return  state_set_state(ts_exit);
  }

 //VIM_DBG_VAR(txn.status);
  /* If message not sent dont print just get out */
  if ((!transaction_get_status(&txn, st_message_sent))  && (!transaction_get_status(&txn, st_offline))
    && (!transaction_get_status(&txn, st_efb)))
  {
    VIM_CHAR line1[30] = {0};

    get_issuer_name_and_txn_type(&txn, line1);
    display_result( txn.error, txn.host_rc_asc, line1 ,"", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );
    return  state_set_state(ts_exit);
  }
  return  state_set_state(ts_finalisation);
}




VIM_ERROR_PTR txn_print_signature_receipt( transaction_t *transaction, VIM_BOOL duplicate )
{
  VIM_PCEFTPOS_RECEIPT receipt;
  VIM_PCEFTPOS_KEYMASK key_code = VIM_PCEFTPOS_KEY_NO;
  //VIM_KBD_CODE key = VIM_KBD_CODE_NONE;
  VIM_ERROR_PTR key_err = VIM_ERROR_NONE;
  VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
  VIM_CHAR account_string_short[3+1];

  vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );

  get_acc_type_string_short( transaction->account, account_string_short );
  GetCardName( transaction, CardName );

 // return VIM_ERROR_NO_PAPER;


  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));

  //transaction_set_status(txn, st_signature_printed);

  /* Print */
  if(( duplicate ) && ( IS_STANDALONE ))
  {
	  VIM_ERROR_RETURN_ON_FAIL(receipt_build_financial_transaction(receipt_signature|receipt_merchant|receipt_duplicate, transaction, &receipt));
  }
  else
  {
	  VIM_ERROR_RETURN_ON_FAIL(receipt_build_financial_transaction(receipt_signature|receipt_merchant, transaction, &receipt));
  }


  if (IS_INTEGRATED_V400M && OFF_BASE)
  {
	  if (config_get_yes_no_timeout("Signature Required\nPrint Receipt\nOn Terminal?", VIM_NULL, TEN_SECOND_DELAY, VIM_FALSE))
	  {
		  PRINT_ON_TERMINAL;
		  // print a physical copy
		  VIM_ERROR_IGNORE(pceft_pos_print(&receipt, VIM_TRUE));
	  }
	  else
	  {
		  if (terminal.original_mode == st_integrated)
		  {
			  PRINT_ON_POS;
		  }
		  // Print a copy to the logs
		  VIM_ERROR_RETURN_ON_FAIL(pceft_pos_print(&receipt, VIM_TRUE));
		  display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Signature Receipt", "At POS");
		  vim_event_sleep(2000);
	  }
  }
  else
  {
	  // Print a copy to the logs
	  VIM_ERROR_RETURN_ON_FAIL(pceft_pos_print(&receipt, VIM_TRUE));
  }

  /* Do not verify signature if Tip adjustment Enable or it is pruchase txn */
  if(( txn_requires_signature( transaction )) && ( txn.error != ERR_POWER_FAIL ))
  {
	  VIM_BOOL TimeoutIsApproved = VIM_TRUE;

	  // RDD 1702JIRA PIRPIN-1440 Only true standalone needs timeout as approved
	  if ((terminal.mode == st_standalone) && (terminal.original_mode != st_standalone))
		  TimeoutIsApproved = VIM_FALSE;

	  /* Wait for the operator */
	  // Below applies to off-base integrated + standalone
	  if( terminal.mode == st_standalone ) 
	  {
		  // RDD 230215 SPIRA:8410 - Don't allow timeout for standalone sig check
		  // RDD 260315 SPIRA:8528 - Need 2 min sig timeout and approval if timed out
#if 1
		  if( !config_get_yes_no_timeout( "VERIFY SIGNATURE\nCORRECT?", VIM_FALSE, TWO_MIN_TIMEOUT, TimeoutIsApproved))
		  {
			  // "NO key was pressed rather than timeout
			  VIM_DBG_POINT;
			  transaction->error = ERR_SIGNATURE_DECLINED;
			  transaction->error_backup = ERR_SIGNATURE_DECLINED;
		  }
#else
		  if( !config_get_yes_no( "VERIFY SIGNATURE\nCORRECT?", VIM_NULL ))
		  VIM_UINT8 Count = 4;
		  do
		  {
			  if( !config_get_yes_no( "VERIFY SIGNATURE\nCORRECT?", VIM_NULL ))
			  {
				  if( transaction->error_backup == VIM_ERROR_NONE )
				  {
					  // "NO key was pressed rather than timeout
					  VIM_DBG_POINT;
					  transaction->error = ERR_SIGNATURE_DECLINED;
					  transaction->error_backup = ERR_SIGNATURE_DECLINED;
					  break;
				  }
				  // RDD 230215 SPIRA:8410 - Ensure that Comms are disconnected
				  else
				  {
					  vim_kbd_sound_invalid_key();
					  // RDD 200515 Now may wish to do a trickle feed
					  //comms_disconnect();
				  }
			  }
		  }
		  while(( transaction->error_backup == VIM_ERROR_TIMEOUT ) && ( --Count ));
#endif
	  }
	  else
	  {
// again:
        /* Verify the signature */
		  VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_APPROVED_WITH_SIG, VIM_PCEFTPOS_KEY_YES|VIM_PCEFTPOS_KEY_NO, type, AmountString(transaction->amount_total), CardName, account_string_short ));
		  if(( key_err = pceft_pos_wait_key( &key_code, TWO_MIN_TIMEOUT )) == VIM_ERROR_PCEFTPOS_NOT_CONNECTED )
		  {
			  return key_err;
		  }

		  if (VIM_ERROR_TIMEOUT == key_err)
		  {
			  vim_kbd_sound_invalid_key();
	//      goto again;   //BRD 050714 loop forever on SIG OK? timeout
		  }
		  VIM_ERROR_RETURN_ON_NULL(pceftpos_handle.instance);

		  /* Clear keymask */
		  VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_keymask( pceftpos_handle.instance, 0 ));

		  if(( VIM_PCEFTPOS_KEY_NO == key_code ) || ( key_err != VIM_ERROR_NONE ))
		  {
			  transaction->error = ERR_SIGNATURE_DECLINED;
		  }
	  }
	  if( transaction->error == ERR_SIGNATURE_DECLINED )
	  {
		  // RDD 081014 - need to ensure that the reversal is set if theres any error in sig check
		  // RDD 040914 SPIRA:8101 - sig declined completions were going online as 220s when they should have been deleted
		  // RDD 161115 DG Complaining that Completion declined sigs were generating reversals so only generate if message already sent
#if 1
		  if(( transaction_get_status( transaction, st_message_sent )) && ( txn.type != tt_checkout ))
#else
		  if( !transaction_get_status( transaction, st_needs_sending ))
#endif
		  {
			  DBG_MESSAGE( "Sig Decline - Set Reversal" );

			  // RDD 100616 v560-2 SPIRA:8930 - Second Reversal gen (after timeout then sig decline) caused 421 instead of 420
			  if( reversal_present() == VIM_FALSE )
				reversal_set( &txn, VIM_FALSE);
		  }
		  transaction_clear_status( transaction, st_needs_sending );
		  /* Print failed receipt but not on POS BRD 050714 */
		  if (terminal.original_mode == st_integrated)
		  {
			  PRINT_ON_POS;
			  VIM_ERROR_RETURN_ON_FAIL(txn_print_receipt_type(receipt_merchant, transaction));		  
		  }
		  else
		  {
			  if (IS_STANDALONE || pceft_pos_internal_prn())
				  VIM_ERROR_RETURN_ON_FAIL(txn_print_receipt_type(receipt_merchant, transaction));
		  }
		  return transaction->error;
	  }
      else if(txn.type == tt_void)
      {
          // Update the SAF as the void was accepted
          transaction_set_status(&txn, st_void);
          saf_update_record(txn.original_index, VIM_FALSE, &txn);
      }
      else if (txn.type == tt_checkout)
      {
          transaction_set_status( &txn, st_needs_sending );
      }

  }
  return VIM_ERROR_NONE;
}



txn_state_t txn_batch_full(void)
{
  return state_get_next();
}


// RDD 240620 Trello #167 OpenPay Implementation
#define BCD_MIN_LEN 12  // UPCA Barcode
#define BCD_MAX_LEN 13  // EAN Barcode

static txn_state_t txn_enter_barcode(void)
{
    VIM_CHAR *Ptr = VIM_NULL;
    VIM_BOOL GetBCD = VIM_FALSE;
    VIM_BOOL GetPCD = VIM_FALSE;
    VIM_CHAR entry_buf[BCD_MAX_LEN+1];
    VIM_SIZE Len = 0;
    kbd_options_t options;

    options.timeout = ENTRY_TIMEOUT * 10;
    options.min = BCD_MIN_LEN;
    options.max = BCD_MAX_LEN;
    options.entry_options = 0;          // no special options for BCD
    options.CheckEvents = VIM_TRUE;     // Support for POS Cancel CMD

    vim_mem_clear( entry_buf, VIM_SIZEOF( entry_buf ));

    if( vim_strstr( (const VIM_CHAR*)txn.u_PadData.ByteBuff, "ALL", &Ptr ) == VIM_ERROR_NONE )
    {
        GetBCD = GetPCD = VIM_TRUE;
    }
    else if( vim_strstr( (const VIM_CHAR*)txn.u_PadData.ByteBuff, "BCD", &Ptr ) == VIM_ERROR_NONE )
    {
        GetBCD = VIM_TRUE;
    }
    else if( vim_strstr( (const VIM_CHAR*)txn.u_PadData.ByteBuff, "PCD", &Ptr) == VIM_ERROR_NONE)
    {
        GetPCD = VIM_TRUE;
    }
    // Flags are all set so we're done with the PAD and can clear it to hold the response
    vim_mem_clear( txn.u_PadData.ByteBuff, VIM_SIZEOF( txn.u_PadData.ByteBuff ));

    if (!GetPCD && !GetBCD)
    {
        txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
        return state_set_state(ts_exit);
    }

    if( GetBCD )
    {
        if(( txn.error = display_data_entry( entry_buf, &options, DISP_BCD_ENTRY, VIM_PCEFTPOS_KEY_CANCEL, 0)) != VIM_ERROR_NONE )
        {
            DBG_POINT;
            return state_set_state(ts_exit);
        }
        DBG_POINT;
        Len = vim_strlen(entry_buf);
        if(GetPCD)
        {
            vim_sprintf( (VIM_CHAR*)txn.u_PadData.ByteBuff, "BCD%3.3d%s", Len, entry_buf);
        }
        else
        {
            VIM_SIZE PadLen = Len + 6;
            vim_sprintf( (VIM_CHAR*)txn.u_PadData.ByteBuff, "WWO%3.3dBCD%3.3d%s", PadLen, Len, entry_buf);
            return state_set_state(ts_exit);
        }
    }
    return state_get_next();
}

#define PCD_MIN_LEN 4  // MB Says use standard PIN max,min
#define PCD_MAX_LEN 6 // RDD 191020 JIRA PIRPIN-903 Restrict OpenPay Passcode to 6 digits

// RDD 240620 Trello #167 OpenPay Implementation
static txn_state_t txn_enter_passcode(void)
{
    VIM_CHAR entry_buf[BCD_MAX_LEN+1];
    VIM_SIZE Len = 0;
    kbd_options_t options;
    VIM_CHAR BCDBuf[50];
    VIM_SIZE PadLen, BCDLen;
    VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN + 1];
    VIM_CHAR account_string_short[3 + 1];
	VIM_SIZE ScreenID = DISP_PCD_ENTRY;

    // make a copy of the BCD data
    vim_mem_clear( BCDBuf, VIM_SIZEOF(BCDBuf) );
    vim_strcpy( BCDBuf, (const VIM_CHAR*)txn.u_PadData.ByteBuff );
    BCDLen = vim_strlen( (const VIM_CHAR*)BCDBuf );
    get_acc_type_string_short(txn.account, account_string_short);
    GetCardName(&txn, CardName);

    options.timeout = ENTRY_TIMEOUT * 10;   // MB wants a v. long timeout so effectively the POS times out
    options.min = PCD_MIN_LEN;
    options.max = PCD_MAX_LEN;
    options.entry_options = 0;              // no special options for BCD
    options.CheckEvents = VIM_TRUE;         // Support for POS Cancel CMD
    options.entry_options |= KBD_OPT_MASKED;
	DisplayProcessing(DISP_PROCESSING_WAIT);
    vim_mem_clear( entry_buf, VIM_SIZEOF( entry_buf ));

	// RDD 170223 JIRA PIRPIN-1820 No reload for Returns Card reload ( use activation instead )
	//if( IsReturnsCard(&txn) && TERM_RETURNS_CARD_NEW_FLOW && txn.type == tt_activate )
	//{
	//	pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-1820");
	//}

    if(( txn.error = display_data_entry( entry_buf, &options, ScreenID, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total), CardName, account_string_short)) == VIM_ERROR_NONE )
	//if ((txn.error = display_data_entry(entry_buf, &options, ScreenID, VIM_PCEFTPOS_KEY_CANCEL, type, AmountString(txn.amount_total))) == VIM_ERROR_NONE)
	{
        Len = vim_strlen(entry_buf);
        PadLen = 6 + Len + BCDLen;
        vim_sprintf(txn.u_PadData.ByteBuff, "WWO%3.3dPCD%3.3d%s%s", PadLen, Len, entry_buf, BCDBuf);
    }

    // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
    if (!QC_TRANSACTION(&txn))
    {
        display_screen_cust_pcx(DISP_IMAGES, "Image", terminal.configuration.BrandFileName, VIM_PCEFTPOS_KEY_NONE);
    }
    else
    {
        DisplayProcessing( DISP_PROCESSING_WAIT );
    }

	if (VIM_ERROR_NONE != txn.error)
	{
		VIM_DBG_ERROR(txn.error);
		return state_set_state(ts_finalisation);
	}

    return state_get_next();

    //return state_set_state(ts_exit);
}



VIM_BOOL txn_is_customer_copy_required(transaction_t *transaction)
{

 //VIM_
  /* if it is customer present txn, the customer copy should be printer on the internal printer */
  if ( txn.card.type == card_manual )
  {
    if ( txn.card.data.manual.manual_reason == moto_customer_present )
      return VIM_TRUE;
  }


  return VIM_TRUE;
}




txn_state_t txn_saf_print(void)
{
  VIM_SIZE index = 0;
  VIM_ERROR_PTR res;

//  if( 0 ) // RDD - disable SAF print for initial release
  //if(( saf_totals.fields.saf_print_count )&&( !saf_totals.fields.saf_count ))
  // RDD 160312 SPIRA 5117 - don't disable SAF Prints if SAFs pending
  // DBG_MESSAGE( "<<<< SAF PRINT STATE >>>>>>");
  if( saf_totals.fields.saf_print_count )
  {
	  if( saf_read_next_by_status( &txn, st_message_sent, &index, VIM_TRUE ) == VIM_ERROR_NONE )
	  {

		// found a sent SAF - print it then delete it
	    // "Mini History" Txn Type has now been wiped, so re-enstate it
		VIM_PCEFTPOS_RECEIPT receipt;


		// RDD 151112 SPIRA:6295 Stored completions were going up with wrong proc code due to this line of code. Need to modify Mini history
	    // to change the transaction type for the completions instead....
		txn.original_type = ( txn.type == tt_completion ) ? txn.original_type : txn.type;

		txn.type = tt_mini_history;

		DBG_VAR(txn.error);

		//receipt_build_saf_print(  &txn, &receipt );
		if(( res = pceft_pos_print(&receipt, VIM_TRUE)) == VIM_ERROR_NONE )
		{
			saf_delete_record( index, &txn, VIM_FALSE );

			// ensure that the POS response relects the SAF prints status and not the status (approved/declined) of the original txn
			vim_mem_copy( txn.host_rc_asc, "00", 2 );
			txn.error = VIM_ERROR_NONE;
		}
		else
		{
			txn.error = res;
		}

#if 0 // RDD 050312
		DBG_MESSAGE( "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
		DBG_VAR(saf_totals.fields.saf_count);
		DBG_VAR(saf_totals.fields.saf_print_count);
		DBG_MESSAGE( "$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
#endif
		return ts_exit;
	  }
  }

  // "NO SAF PRINT DATA"
  vim_strcpy( txn.host_rc_asc, "R0" );
  return ts_exit;
}


// RDD - Offline Approved EMV txns need to be stored in SAF for uploading later as 220
void EmvMapFinaliseOffline( void )
{
	if ( !vim_strncmp ( txn.host_rc_asc, "Y1", 2 ) || !vim_strncmp ( txn.host_rc_asc, "Y3", 2 ))
	{
		DBG_MESSAGE( "Yx APPROVED: MAPPED TO 00 AND STORED FOR SENDING" );
		// Y1 approved offline (eg. through offline pin)
		transaction_set_status( &txn, st_needs_sending );
		transaction_set_status( &txn, st_offline );
		vim_strcpy( txn.host_rc_asc, "00" );

		// Y3 unable to go online but approved anyway
		if ( !vim_strncmp ( txn.host_rc_asc, "Y3", 2 ))
		{
			if (emv_requires_signature(&txn))
			{
				vim_strcpy( txn.host_rc_asc, "08" );
				transaction_set_status( &txn, st_signature_required );
			}
		}
	}

	// RDD 050212 SPIRA IN5017 Send Z1 Decline Advices for both Contact and contactless
	// RDD 300823 JIRA PIRPIN-2673 Remove all Z1 advices for refunds as some defect is causing some of these to be sent up without DE55 resulting in multiple refunds.
#if 1
//	if( txn.error == VIM_ERROR_PICC_CARD_DECLINED )
	if ((txn.error == VIM_ERROR_PICC_CARD_DECLINED)&&( txn.type == tt_sale ))
	{
		VIM_TLV_LIST tag_list;

		// RDD 120312 - for offloine declined PICC write the Z1 at this point into the txn host rc AND the EMV DE55 buffer for the advice
		vim_mem_copy(txn.host_rc_asc, "Z1", 2 );
		DBG_MESSAGE( "Z1 DECLINED: STORED AS ADVICE FOR SENDING (INFO ONLY)" );
	    transaction_set_status(&txn, st_offline);
        transaction_set_status(&txn, st_needs_sending);

		if( vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE) == VIM_ERROR_NONE )
		{

			DBG_VAR(tag_list.total_length);

			// RDD 090312: SPIRA 5017: Need to add 8A to DE55 for Z1 advice so customer won't be debited
			if( vim_tlv_update_bytes(&tag_list, VIM_TAG_EMV_RESPONSE_CODE, txn.host_rc_asc, 2) == VIM_ERROR_NONE )
			{
				DBG_MESSAGE( "$$$$ TAG 8A UPDATED FOR Z1 ADVICE $$$$" );
				txn.saf_reason = saf_reason_no_advice; /*  do not send advice indicator */
				txn.emv_data.data_size = tag_list.total_length;
				DBG_VAR(tag_list.total_length);
			}
		}
	}
#endif
}


VIM_BOOL CustomerCopyS(VIM_BOOL SigReceipt)
{
    VIM_BOOL DoCustCopy = VIM_FALSE;
    // RDD 010720 Trello #157 - some issue around printing receipts off base where PinPad seems to go offline for a couple of mins
    VIM_BOOL DoLocalCopy = VIM_FALSE;
    //VIM_ERROR_PTR txn_error_backup = txn.error_backup;

    if(1)
    {
        // RDD 010720 Trello #157 - some issue around printing receipts off base where PinPad seems to go offline for a couple of mins
        if ((DoLocalCopy = config_get_yes_no("Customer Copy?", "")) == VIM_TRUE)
        {
            // RDD JIRA WWBP-205 - Try setting standalone mode temporarily if printing required on PinPad
            pceft_debug(pceftpos_handle.instance, "Print Receipt On PinPad confirmed");
			//PRINT_ON_TERMINAL;
			DoCustCopy = VIM_TRUE;
		}
        // RDD 010720 Trello #157 - some issue around printing receipts off base where PinPad seems to go offline for a couple of mins
        else
        {
            pceft_debug(pceftpos_handle.instance, "Print Receipt On PinPad denied");
			DoCustCopy = VIM_FALSE;
		}
		//???
		IdleDisplay();
    }

    return DoCustCopy;
}


VIM_BOOL CustomerCopy(VIM_BOOL SigReceipt)
{
	VIM_BOOL DoCustCopy = VIM_FALSE;
	DoCustCopy = SigReceipt;

	// RDD 100523 JIRA PIRPIN-2374 P400 printing customer receipt for signature declines and v400m sig receipts when customer receipt disabled
	if (IS_P400 && txn.error == ERR_SIGNATURE_DECLINED)
		return VIM_FALSE;

	return DoCustCopy;
}


VIM_ERROR_PTR PrintReceipts( transaction_t *transaction, VIM_BOOL duplicate )
{
	VIM_CHAR line1[30];
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//VIM_BOOL PaperOK = VIM_TRUE;
	//VIM_BOOL IntegratedReceiptOnPinPad = VIM_FALSE;
	VIM_BOOL SigReceipt = VIM_FALSE;
	receipt_type_t bit_duplicate = duplicate ? receipt_duplicate : 0;
	// No sig receipt for duplicate - just print the error receipt
	if (IS_V400m)
	{
		pceft_debug_test(pceftpos_handle.instance, "v400m");
	}
	if (OFF_BASE)
	{
		pceft_debug_test(pceftpos_handle.instance, "Off Base");
	}
	if ((terminal.original_mode == st_integrated)&&( IS_V400m ))
	{
		pceft_debug_test(pceftpos_handle.instance, "Set Printing Off Base");
	}
	if (IS_STANDALONE)
	{
		DisplayStatus( "Printing Receipt", DISP_PROCESSING_WAIT);
	}

	if( txn_requires_signature( transaction ) && !duplicate )
	{
		SigReceipt = VIM_TRUE;

		// RDD 190712 - just to ensure that we don't get any dumb looking receipts where a sig panel is printed along with the "NO PIN OR SIG REQD" line....
		transaction->qualify_svp = VIM_FALSE;
			//////////// TEST TEST TEST TEST ///////////

		if(( res = txn_print_signature_receipt( transaction, duplicate )) != VIM_ERROR_NONE )
		{
			// RDD 200916 - prevent SAF being saved if error printing sig receipt.
			transaction_clear_status( transaction, st_needs_sending );
	
			// RDD 140322 JIRA-PIRPIN 1473 - If out of paper during sig receipt, the terminal reports approved and carries on with sig receipt. 
			//if( res == VIM_ERROR_PRINTER )
			if(( transaction->error != ERR_SIGNATURE_DECLINED) && ( transaction->error != VIM_ERROR_TIMEOUT ))
			{
				//PaperOK = VIM_FALSE;                   
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PRINT FAILED", "Txn Cancelled");
				Pause(1000);
				VIM_ERROR_RETURN(res);
			}
		}
	}
	else
	{
		get_txn_type_string( &txn, line1, VIM_SIZEOF(line1), VIM_FALSE );
		  //display_result( txn.error, txn.host_rc_asc, line1, "", VIM_PCEFTPOS_KEY_NONE, __1_SECOND__ );
		// if the txn was 91 and not efb approved then store in card buffer

		if( !vim_strncmp( transaction->host_rc_asc, "91", 2 ))
			transaction->saf_reason = saf_reason_91;

		// RDD 190313 - CR for phase4 Send the detail of any failed emv transactions to the audit printer
        if( FullEmv( transaction ))
        {
			// RDD 190122 JIRA PIRPIN-1348 LIVE requested don't automatically print last EMV recept
			//if( !is_approved_txn( transaction )) 
			if(( !is_approved_txn( transaction )) && ( terminal.terminal_id[0] != 'L') && ( IS_INTEGRATED ))
                PrintLastEMVTxn(VIM_FALSE);
        }

		if( IS_STANDALONE )
		{
			PRINT_ON_TERMINAL;
			if ((res = txn_print_receipt_type(receipt_merchant | bit_duplicate, transaction)) != VIM_ERROR_NONE)
			{
				pceft_debug_error(pceftpos_handle.instance, "Local Receipt Print Error:", res);
				//VIM_ERROR_RETURN_ON_FAIL(res);
			}
		}
		else
		{
			if (terminal.original_mode == st_integrated)
			{
				PRINT_ON_POS;
			}
			if ((res = txn_print_receipt_type(receipt_customer | bit_duplicate, transaction)) != VIM_ERROR_NONE)
			{
				pceft_debug_error(pceftpos_handle.instance, "POS Receipt Print Error:", res);
				VIM_ERROR_RETURN_ON_FAIL(res);
			}
		}
	}
	// RDD SPIRA:8747 030615 - this needs to be moved to before the customer copy print in order to avoid possible fraud
	// RDD 300316 SPIRA:8927 Ensure that Paper Out amnd related errors during printing don't allow a SAF to be stored
	//if(( txn.error != ERR_POWER_FAIL ) && ( txn.error != VIM_ERROR_NO_PAPER ) && ( txn.error != VIM_ERROR_PRINTER ))

	// RDD 110522 JIRA PIRPIN-1605 Double debit after card removed post Y1 approval. 
	if(( txn.error != ERR_POWER_FAIL ) && ( txn.error != VIM_ERROR_NO_PAPER ) && ( txn.error != VIM_ERROR_PRINTER ) && ( txn.error != VIM_ERROR_EMV_CARD_REMOVED ))
	{
	  // Add to SAF if necessary and store the transaction - note overflow needs to be reported
	  // RDD 010515 - note that because Z1 transactions need to be stored in the SAF we can't bunch the commit with the ST update
	  // RDD 290523 JIRA PIRPIN-2395 Transaction SAved to SAF when Signature declined - commit needs to be between two receipts
	  if(( res = txn_commit() ) != VIM_ERROR_NONE )
	  {			  
		  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN - 2074 Commit Failed");			
		  txn.error = res;
	  }
	  else	// RDD 090615 No Power Fail Receipt in Standlone if merchant receipt was printed and txn was saved
	  {
		  terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855
		  // RDD 060715 added as we need approved duplicate for completion
		  txn_duplicate_txn_save();
		  txn_save();
	  }
	}

#if 1
	// RDD 070315 SPIRA:8410 Approved missing from some txns.
	// RDD 151116 v561-4 ensure that zero timeout as final result will remain on screen
	// RDD 210217 SPIRA:9210 If reversal present ensure 2 secs here as post txn processing will replace the message with the reversal.
	// RDD 020222 v730-2 Improve Speed by getting rid of 2 secs approved here
	// RDD 290322 JIRA PIRPIN-1521 - they want to define this in TMS
	// 


	display_result(txn.error, txn.host_rc_asc, line1, "", VIM_PCEFTPOS_KEY_NONE, VIM_MAX( 1000, (1000*TERM_DISPLAY_RESULT_SECS)));
#endif
	// RDD v547-0 Donna says no customer copy prompt if paper out

	// Print the second receipt	if required
	// RDD 240412 SPIRA:5356 Also print Sig Declined receipt here

    //if(( IS_V400m )&&( terminal.original_mode == st_integrated ))
	// RDD 080222 Need to prompt for customer copy on standalone.
	if((( OFF_BASE )&&(terminal_get_status(ss_enable_local_receipt))) || (IS_STANDALONE))
	{
		//if (( OFF_BASE )||( SigReceipt )||(IS_STANDALONE))
		if(1)
		{
			DBG_POINT;
			if (CustomerCopyS(SigReceipt))
			{
				if(1)
				{
					//PRINT_ON_TERMINAL;
					if (IS_STANDALONE)
					{
						DisplayStatus("Customer Receipt", DISP_PROCESSING_WAIT);
					}
					else
					{
						//PRINT_ON_POS;
					}
				}
				if ((!SigReceipt) && (OFF_BASE))
				{
					// RDD 120422 JIRA-PIRPIN 1443 non-sig off-base customer receipt always prints on terminal.
					PRINT_ON_TERMINAL;
				}
				if (txn.error == ERR_SIGNATURE_DECLINED)
				{
					// RDD 120522 JIRA PIRPIN-1443 Georgia complained that Off Base Integrated V400m declined sig -> POS
					PRINT_ON_TERMINAL;
				}

				// No need for new customer copy on POS for non-sig as merchant copy can be used as customer copy.
				if((txn_print_receipt_type(receipt_customer | bit_duplicate, transaction) != VIM_ERROR_NONE)&&( SigReceipt ))
				{
					pceft_debug(pceftpos_handle.instance, "Customer receipt print failed on PinPad");
					if (terminal.original_mode == st_integrated)
					{
						PRINT_ON_POS;
					}
					if (txn_print_receipt_type(receipt_customer | bit_duplicate, transaction) != VIM_ERROR_NONE)
					{
						pceft_debug(pceftpos_handle.instance, "Customer receipt print failed on POS");
					}
				}
			}
		}
	}

	else if( CustomerCopy( SigReceipt ))
	{
		// RDD 200515 SPIRA:8673 Disconnect if we're over time
		//CheckConnTime( );
		VIM_ERROR_RETURN_ON_FAIL( txn_print_receipt_type( receipt_customer|bit_duplicate, transaction ));

	}
	return VIM_ERROR_NONE;
}


txn_state_t txn_finalisation(void)
{
  VIM_CHAR line1[30];
  VIM_BOOL GenerateError = VIM_FALSE;

  //VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_ERROR_PTR ret = VIM_ERROR_NONE;

  VIM_RTC_UPTIME now, min_display_time = 2000;

  // We previously completed finalisation state so don't run it again for any reason
  //if( !transaction_get_status( &txn, st_incomplete ))
  if( !terminal_get_status( ss_incomplete ) && ( txn.type != tt_checkout ))	// RDD 161215 SPIRA:8855
  {
	  DBG_MESSAGE( "!!!!!!!!!!!!!! TRANSACTION WAS COMPLETED ALREADY SO DON'T RUN THIS STATE AGAIN !!!!!!!!!!!!!!!!!" );

	  if( txn.error != VIM_ERROR_NONE )
		display_result( txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, __2_SECONDS__ );

	  return state_get_next();
  }
  // RDD 050715 Incredibly during a completion txn has not been saved at this point so incomplete flag is not correct
  txn_save();

  DBG_MESSAGE( "<<<<<<<<<< TXN FINALISATION PROCESSING STATE >>>>>>>>>>>>>>>" );
  VIM_DBG_ERROR( txn.error );
  DBG_VAR( txn.qualify_svp );

  if( txn.card.type == card_picc )
  {
    VIM_PICC_EMV_STATUS picc_txn_status = VIM_PICC_EMV_STATUS_NOT_AVAILABLE;
    vim_picc_emv_get_transaction_status (picc_handle.instance, &picc_txn_status);
    if( (picc_txn_status == VIM_PICC_EMV_STATUS_AUTHORIZATION_REQUEST) && !txn.picc_online_result_sent )
    {
      VIM_ERROR_IGNORE( txn_set_picc_online_result (VIM_FALSE, "91"));
      txn.picc_online_result_sent = VIM_TRUE;
    }
  }

  get_issuer_name_and_txn_type(&txn, line1);

  // DBG_MESSAGE("STATE: TXN FINALISATION");

  if( OdoRetry() && fuel_card(&txn) && (txn.error != ERR_POWER_FAIL ) )
  {
    // DBG_MESSAGE("!!! ODO RETRY QUALIFIED !!!");

	// BRD L5 to allow for 2 different 55 errors    display_result(txn.error, txn.host_rc_asc, line1, transaction_get_training_mode()?STR_TRAINING_MODE:"", VIM_PCEFTPOS_KEY_OK);
    display_result(txn.error, "OJ", line1, "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );
	vim_kbd_sound_invalid_key();

    vim_mem_clear(txn.host_rc_asc, VIM_SIZEOF(txn.host_rc_asc));

	// RDD 180512 - problems with history buffer when Pin retired too many times
    state_set_state(ts_card_check);
    return ts_card_check;
  }

  // RDD 161214 SPIRA:8273 MCD01 02 01b and MCC35 20 20 1b - Response code 65 for MC needs to return to "Insert or Swipe"
  if( MC_Cert_Fallback() )
  {
	  txn.error = VIM_ERROR_NONE;
      vim_strcpy(txn.cnp_error, "FDM");

	  txn.card_state = card_capture_no_picc;

	  // RDD 010812 SPIRA:5812 - Need to Print Pin Retry Advice Receipt and Journal the Pin Retry
	  if( !transaction_get_training_mode() )
	  {
		txn_print_receipt_type(receipt_advice, &txn);
		pceft_journal_txn(journal_type_txn, &txn, VIM_FALSE);
	  }
	  return state_set_state( ts_card_capture );
  }

  if( PinRetry() )
  {
     DBG_MESSAGE("!!! PIN RETRY QUALIFIED !!!");

	// BRD L5 to allow for 2 different 55 errors    display_result(txn.error, txn.host_rc_asc, line1, transaction_get_training_mode()?STR_TRAINING_MODE:"", VIM_PCEFTPOS_KEY_OK);
    display_result(txn.error, "L5", line1, "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );
	vim_kbd_sound_invalid_key();

	// RDD 290412 SPIRA:5363 Problems when card removed when prompting for retry PIN.
	txn.pin_block_size = 0;

	// RDD 030612 - Some Manually Entered Fuelcards will be forced to do mandatory pin if originally bypassed and RC55 returned.
	if( fuel_card(&txn) && ( txn.card.type == card_manual ))
	{
		txn.FuelData[PROMPT_FOR_PIN] = MANDATORY;
	}

	// RDD 180512 - problems with history buffer when Pin retired too many times
    state_set_state(ts_get_pin);

    // RDD Trello #184 Trello #185 - Some EMV issue is causing receipts not to be printed - no need for line below as receipt not printed from pin entry state anyway
    //txn.print_receipt = VIM_FALSE;

	// RDD 010812 SPIRA:5812 - Need to Print Pin Retry Advice Receipt and Journal the Pin Retry
	if( !transaction_get_training_mode() )
	{
		txn_print_receipt_type(receipt_advice, &txn);
		pceft_journal_txn(journal_type_txn, &txn, VIM_FALSE);
	}
    //vim_mem_clear(txn.host_rc_asc, VIM_SIZEOF(txn.host_rc_asc));
    return ts_get_pin;
  }

  // RDD 300523 JIRA PIRPIN-2401 : timeout error was treated as decline and result in double credit to returns card. Ensure only GA errors result in reload
  //if (IsReturnsCard(&txn) && TERM_RETURNS_CARD_NEW_FLOW && txn.type == tt_gift_card_activation && !is_approved_txn(&txn))
  if (IsReturnsCard(&txn) && TERM_RETURNS_CARD_NEW_FLOW && txn.type == tt_gift_card_activation && !vim_strcmp( txn.host_rc_asc, "GA"))
  {
	  pceft_debug_test(pceftpos_handle.instance, "Returns Card : Already Activated. Switch to refund");
	  txn.type = tt_refund;
	  return state_set_state(ts_get_pin);
  }


  // RDD 180412: SPIRA 5335 Window for card removal after Pin entry cause "Card Read Error" to be returned from kernel instead of card removed error so check for card removal here
  if( txn.card.type == card_chip )
  {
	  // RDD 260412: Card removal during Second GenAc can result in card read error which is really due to card removal
	  //VIM_BOOL ErrorOverride = ( txn.error == VIM_ERROR_EMV_FAULTY_CHIP ) ? VIM_TRUE : VIM_FALSE;
	  //VIM_CHAR DisplayMessage[100];

	  if( transaction_get_status( &txn, st_offline_pin_verified ))
	  {
#if 0
		  VIM_CHAR CardName[20];
		  VIM_CHAR account_string_short[3+1];

		  GetCardName( &txn, CardName );
		  get_acc_type_string_short(txn.account, account_string_short);

		  display_screen( DISP_PIN_OK, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), CardName, account_string_short );
		  vim_event_sleep(1000);
#endif
		  //vim_strcpy( DisplayMessage, "PIN OK" );
		  // RDD 050213 - ensure that this is only displayed once (in case of fallback)
		  transaction_clear_status( &txn, st_offline_pin_verified );
	  }
	  //if( EmvCardRemoved(VIM_FALSE) == VIM_ERROR_NONE ) emv_remove_card( DisplayMessage );
	  if( EmvCardRemoved(VIM_FALSE) == VIM_ERROR_NONE ) emv_remove_card( "" );
  }
  if( EmvCardRemoved(VIM_FALSE) == VIM_ERROR_NONE ) emv_remove_card( "" );
  // RDD added - TODO - add customer query to print balance receipt..



  // RDD Trello #184 Trello #185 - Some EMV issue is causing receipts not to be printed - card removed error or POS_CANCEL error
#if 0
  if(( txn.error == VIM_ERROR_POS_CANCEL )||(txn.type == tt_balance ))
  {
	  txn.print_receipt = VIM_FALSE;
  }
#else

  if (txn.type == tt_balance)
  {
      txn.print_receipt = VIM_FALSE;
  }

  else if(( transaction_get_status( &txn, st_message_sent ) && ( txn.error != VIM_ERROR_NONE )))
  {
      //pceft_debug(pceftpos_handle.instance, "Trello #184 #185");
      txn.print_receipt = VIM_TRUE;

	  // JIRA PIRPIN-2270 Double debit due to no reversal after chip error delivers declined receipt. 
	  if (!vim_strncmp(txn.host_rc_asc, "00", 2) || (!vim_strncmp(txn.host_rc_asc, "08", 2)))
	  {
		  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2270 : Set Reversal");
		  reversal_set(&txn, VIM_FALSE);
	  }
  }

  else if (txn.error == VIM_ERROR_POS_CANCEL)
  {
      txn.print_receipt = VIM_FALSE;
  }

#endif

  // RDD 020112 : SVC Receipts were not printing
  else if(( terminal.training_mode )&&( txn.error != VIM_ERROR_TIMEOUT )&&( txn.error != VIM_ERROR_SYSTEM ))
  {
	  txn.print_receipt = VIM_TRUE;
  }

  // RDD JIRA PIRPIN-1542 : Checkout with Authcode needs a receipt
  if ((txn.type == tt_checkout) && ValidateAuthCode(txn.auth_no))
  {
	  txn.print_receipt = VIM_TRUE;
  }


  // RDD 061211 added: Map "Y1" to "00" RC and ensure that txn is saved as SAF
  EmvMapFinaliseOffline();

  // Check to see if SAF is full before the receipt is printed
  if( transaction_get_status( &txn, st_needs_sending ) )
  {
	  if( saf_full(VIM_TRUE) )
	  {
		DBG_MESSAGE( "!!!! SAF IS FULL - RETURN OVERFLOW ERROR !!!!" );
		transaction_clear_status( &txn, st_needs_sending );
		transaction_clear_status( &txn, st_efb );
		if (vim_strncmp(txn.host_rc_asc, "Z1", 2 )) txn.error = VIM_ERROR_SAF_FULL;		//BD don't translate Z1, let the Z1 be discarded
	  }
  }

  get_txn_type_string( &txn, line1, VIM_SIZEOF(line1), VIM_FALSE );

  if ((IS_P400) && (IS_STANDALONE))
  {
      txn.print_receipt = VIM_FALSE;
  }


  // JIRA PIRPIN-2109 Simulate various Issues with errors immediately prior to or during journel processing
  if (YYY_MODE)
  {
	  VIM_KBD_CODE key_code = 0;
	  display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enhanced Test Mode", "OK = Continue\nOther key = print err\nor reboot here\nor kill client");
	  vim_kbd_read(&key_code, 30000);
	  if (key_code == VIM_KBD_CODE_OK)
	  {
		  ret = VIM_ERROR_NONE;
	  }
	  else
	  {
		  ret = ERR_POS_COMMS_ERROR;
		  txn.error = ret;
		  //display_result(txn.error, txn.host_rc_asc, line1, "", VIM_PCEFTPOS_KEY_NONE, __2_SECONDS__);
	  }
  }


  if (( txn.print_receipt ) && ( txn.type != tt_balance ))
  {
	  // RDD 050715 v545-3 commit now inside receipt printing between merch and cust
	  if(( ret = PrintReceipts( &txn, VIM_FALSE )) != VIM_ERROR_NONE )
	  {
		  // RDD 151014 SPIRA:8199 They Don't want Cancelled result
		  //display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, __2_SECONDS__ );
	  }
      else if (txn.type == tt_void)    
      {        
          // Update the SAF as the void was accepted      
          transaction_set_status(&txn, st_void);
          saf_update_record(txn.original_index, VIM_FALSE, &txn);
      }
  }
  else
  {
	  // RDD SPIRA:8747 030615 - this needs to be moved to before the customer copy print because
	  // Add to SAF if necessary and store the transaction - note overflow needs to be reported
	  // RDD 010515 - note that because Z1 transactions need to be stored in the SAF we can't bunch the commit with the ST update
	  display_result( txn.error, txn.host_rc_asc, line1, "", VIM_PCEFTPOS_KEY_NONE, __2_SECONDS__ );
	  // RDD 270315 SPIRA:8540
	  //vim_event_sleep( 2000 );

	  if ( txn.error != ERR_POWER_FAIL )
	  {
		  /* read current time and set timer */
		  vim_rtc_get_uptime(&now);
		  min_display_time = now + 2000;
		  //display_result( txn.error, txn.host_rc_asc, line1, "", VIM_PCEFTPOS_KEY_NONE, 0 );
	  }
  }

  // Restore original mode if migrated temporarily to standalone becuase of off-base printing
  if (terminal.original_mode == st_integrated)
  {
	  PRINT_ON_POS;
  }


  // RDD 091014 - need to save duplicate here in case of sig failure (for reprint)
  txn_duplicate_txn_save();





  /* read current time - send a POS key mask to "OK" and wait until 2 seconds has passed since the original message was sent */

#ifdef _PHASE4
  terminal.idle_time_count = 0;
  vim_rtc_get_uptime(&terminal.idle_start_time);

#else
	vim_rtc_get_uptime(&now);
	VIM_DBG_NUMBER(now);
	if (now < min_display_time)
	{
	  VIM_PCEFTPOS_KEYMASK key_code;
	  //VIM_DBG_NUMBER(min_display_time - now);

	  vim_pceftpos_set_keymask( pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK );

	  pceft_pos_wait_key( &key_code, ( min_display_time - now ));
	}
#endif

  //pceft_pos_display_close();

  // RDD 150212 - Print the balance if required
  if(( PrintBalance() ) && ( ret == VIM_ERROR_NONE ))
  {
      ret = txn_print_receipt_type( receipt_customer, &txn );
  }
 
  DBG_POINT;

  // JIRA PIRPIN-2109 Simulate various Issues with errors immediately prior to or during journel processing
  if(YYY_MODE)
  {
	  VIM_KBD_CODE key_code = 0;
	  display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enhanced Test Mode", "OK = Continue\nOther key = journal err\nor reboot here\nor kill client");
	  vim_kbd_read(&key_code, 30000);
	  if (key_code != VIM_KBD_CODE_OK)
	  {
		  GenerateError = VIM_TRUE;
	  }
  }

  // Journal processing
  DBG_POINT;

  if(!transaction_get_training_mode())
  {
	  VIM_ERROR_PTR res;
	  // RDD 240222 JIRA PIRPIN-1447 No reversal after Client died when we sent journal - POS and PinPad were rebooted but no reversal -> Double debit
	  // RDD 301122 JIRA PIRPIN-2109 Approved receipt printed but Journal save failed - this resulted in a loss for merchant as customer had approved receipt but txn was reversed due to below	  
	  if (( res = pceft_journal_txn(journal_type_txn, &txn, GenerateError )) != VIM_ERROR_NONE)
	  {
		  if((txn.customer_receipt_printed) && ( ret == VIM_ERROR_NONE ))
		  {
			  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2109 Journal failed but customer receipt printed. No reversal");
		  }
		  else			
		  {
			  // Customer receipt not printed - reverse txn
			  txn.error = ret;

			  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-1447 Journal failed and customer receipt not printed. Set reversal");
			  reversal_set(&txn, VIM_FALSE);
		  }
	  }
  }

  DBG_POINT;

  //////////////////////////////////////////////
  /* Transaction is considered complete here. */
  /////////////////////////////////////////////
	// RDD 270223 - JIRA PIRPIN-2220 Reported from SMT that power fail reversal not working in QC - if terminal reboots after receipt printed 
  if (QC_TRANSACTION(&txn))
  {
	  QC_ClearReversal();
	  pceft_debug_test(pceftpos_handle.instance, "QC Reversal Cleared in QC App");
  }


  //transaction_clear_status( &txn, st_incomplete );
  terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855

  txn_save();

  if( ret != VIM_ERROR_NONE )
  {
	  VIM_DBG_ERROR(ret);
	  // RDD 071212 Found it was possible to set a reversal with a pending 221 if the print fails. this resulted in 221 being deleted
	  // RDD 200622 JIRA PIRPIN-1652 Reversal generated in error when paper-out during completion printing.
	  if( !transaction_get_status( &txn, st_repeat_saf ) && transaction_get_status( &txn, st_message_sent ))
	  {	
		  pceft_debug( pceftpos_handle.instance, "Unknown receipt error. Setting reversal" );
		  reversal_set( &txn, VIM_FALSE);
	  }
	  // Report the Error back to the POS
	  txn.error = ret;
	  DBG_POINT;
	  display_result( txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );

	  VIM_DBG_WARNING( "Clear Trickle Feed Flag !!" );
	  terminal.trickle_feed = VIM_FALSE;
	  return ts_exit;
 }

 // RDD 260412 - If Card Removed and this was an error then ensure reversal set here
 if(( txn.error == VIM_ERROR_EMV_CARD_REMOVED )&&( txn.status & st_message_sent ))
 {
	  terminal.reversal_count = 1;
 }


  // RDD 131114 SPIRA:????? Need to use st_incomplete flag to determine whether we can run finalisation state
  DBG_MESSAGE( "!!!!!!!!!!!!!! TRANSACTION IS COMPLETE !!!!!!!!!!!!!!!!!" );
  //transaction_clear_status( &txn, st_incomplete );
  terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855

  txn_save();

  // RDD 020112 : return here for training mode
  if( terminal.training_mode )
  {
	  return ts_exit;
  }

  // Trickle feed one SAF before we hangup if acquirer wants it after approved txn
  // If txn signature was declined, generate a reversal here..
  // RDD - don't want trickle feed after power fail as PinPad often gets comms errors if trickle feeding immediatly after power up
  if( AllowPiggyBack() )
  {
		// Trickle feed any sig decline reversal and next 220
		DBG_MESSAGE( "!!!! SET PIGGYBACK FLAG !!!!! ");
		terminal.trickle_feed = VIM_TRUE;
		// RDD 160714 v430 SPIRA:7568 Masters Issue - send Key change logon before transaction response
		if( IS_INTEGRATED )
		{
			if( terminal.logon_state == session_key_logon )
			{
				logon_request( VIM_FALSE );
				// RDD 100616 SPIRA:8996 v560-2 Recover original transaction after 101 logon
				txn_load();
			}
		}
		// RDD kill piggy back and ensure disconnected if declined etc.
		ConditionalDisconnect(  );
  }
  else
  {
	  VIM_DBG_WARNING( "Clear Trickle Feed Flag !!" );
	  terminal.trickle_feed = VIM_FALSE;
		//VIM_DBG_ERROR( txn.error );
		//VIM_DBG_ERROR( txn.error_backup );
  }

#ifndef _XPRESS
  //pceft_pos_display_close(); // 5100 doesn't do a clear display on power up but BD says send it anyway...
#endif
  return ts_exit;
}



VIM_ERROR_PTR txn_amount_entry( VIM_CHAR *DisplayText, VIM_UINT64 *Amount )
{
    VIM_CHAR text_amount[20];
    kbd_options_t options;

	/* Get txn type info  string */
    get_txn_type_string( &txn, type, sizeof(type), VIM_FALSE );

	options.timeout = ENTRY_TIMEOUT;
    options.min = 1;
    options.max = terminal_max_txn_digits;
    options.entry_options = KBD_OPT_AMOUNT|KBD_OPT_ALLOW_BLANK;
	options.CheckEvents = IS_STANDALONE ? VIM_FALSE : VIM_TRUE;

    vim_mem_set(text_amount, 0, sizeof(text_amount));

#ifdef _WIN32
    VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_AMOUNT_ENTRY, VIM_PCEFTPOS_KEY_CANCEL, DisplayText, (VIM_UINT32)AmountString(txn.amount_total), DisplayText));
    VIM_ERROR_RETURN_ON_FAIL( data_entry(text_amount, &options) );
#else
    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry( text_amount, &options, DISP_AMOUNT_ENTRY, VIM_PCEFTPOS_KEY_CANCEL, type, (VIM_UINT32)AmountString(txn.amount_total), DisplayText ));
#endif

    *Amount = vim_atol(text_amount);

	/* Now we have amount get shorter txn type info string */
    get_txn_type_string_short( &txn, type, sizeof(type) );

	return VIM_ERROR_NONE;
}

// RDD 041114 added for ALH


static VIM_ERROR_PTR TxnSetMOTOSale(void)
{
	txn.type = tt_moto;
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR TxnSetMOTORefund(void)
{
	txn.type = tt_moto_refund;
	return VIM_ERROR_NONE;
}

S_MENU MOTOMenu[] = {{ "Purchase", TxnSetMOTOSale },
					 { "Refund", TxnSetMOTORefund }};


txn_state_t txn_is_refund(void)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	if (!IS_STANDALONE)
		return state_get_next();

	if(( res = MenuRun( MOTOMenu, VIM_LENGTH_OF(MOTOMenu), "Select MOTO\nTransaction Type", VIM_TRUE, VIM_TRUE )) != VIM_ERROR_NONE )
	{
		TXN_RETURN_ERROR(res);
	}
	return state_get_next();
}



txn_state_t txn_sale_amount_entry(void)
{
	VIM_ERROR_PTR res;
	VIM_UINT64 *AmountPtr;
	VIM_CHAR *Text;

	if (!IS_STANDALONE)
		return state_get_next();

	// RDD 221114 Set  the Incomplete flag here so we get an error msg if timeout
	//transaction_set_status( &txn, st_incomplete );
	//terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855

	switch( txn.type )
	{
		case tt_refund:
		case tt_moto_refund:
			AmountPtr = &txn.amount_refund;
			Text = "Enter Refund\nAmount";
			break;

		case tt_cashout:
			AmountPtr = &txn.amount_cash;
			Text = "Enter CashOut\nAmount";
			break;

		case tt_gift_card_activation:
			AmountPtr = &txn.amount_refund;
			Text = "Enter Activation\nAmount";
			break;

		default:
			AmountPtr = &txn.amount_purchase;
			Text = "Enter Purchase\nAmount";
			break;
	}

	while(1)
	{
		if(( res = txn_amount_entry( Text, AmountPtr )) != VIM_ERROR_NONE )
		{
            VIM_DBG_ERROR(res);
			// RDD 160115 SPIRA:8343
			if (res == VIM_ERROR_USER_BACK)
				return(state_get_current());
#if 0		// RDD 010515 - if error on amount entry then exit without a receipt
			TXN_RETURN_ERROR(res);
#else
			display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
			TXN_RETURN_EXIT(res);
#endif
		}

		if( !*AmountPtr )
		{
			if( config_get_yes_no( "INVALID AMOUNT\nRE-ENTER?", VIM_NULL ))
				continue;
			else
				TXN_RETURN_ERROR(ERR_INVALID_TOTAL_AMOUNT);
		}
		if (txn.type == tt_checkout)
		{
			txn.amount_total = *AmountPtr;
		}
		else
		{
			txn.amount_total += *AmountPtr;
		}
		return state_get_next();
	}
}

// RDD JIRA-PIRPIN 1120 apply tipping option from POS : @ALLOW_TIP = '1' only allow tip if tipping flag set from POS, if @ALLOW_TIP > '1' allow tip anyway
VIM_BOOL TipAllowed( transaction_t *transaction )
{
    if( TERM_ALLOW_TIP > 0 )
    {
        // RDD - set 1 if need Tip option from POS
        if (TERM_ALLOW_TIP == 1)
        {
            if (transaction_get_status(transaction, st_tip_option))
                return VIM_TRUE;
            else
                return VIM_FALSE;
        }
        else
        {
            return VIM_TRUE;
        }
    }
    return VIM_FALSE;
}

/*

typedef enum eTipMethod
{
    no_tip,
    inline_tip,
    five_percent_tip,
    ten_percent_tip,
    other_tip
} eTipMethod;

*/


VIM_ERROR_PTR TipAdd5Percent(void)
{
    terminal.configuration.tip_type = five_percent_tip;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR TipAdd10Percent(void)
{
    terminal.configuration.tip_type = ten_percent_tip;
    return VIM_ERROR_USER_BACK;
}

static VIM_UINT8 TipSetCustomPercent;

#define CUSTOM_PERCENT_MAX_LEN 2

VIM_ERROR_PTR TipAddCustomPercent(void)
{
    kbd_options_t entry_opts;
    VIM_CHAR entry_buf[50];
    VIM_CHAR txn_type[20];
    VIM_SIZE ScreenID = DISP_GENERIC_ENTRY_POINT;

    TipSetCustomPercent = 0;
    vim_mem_clear(txn_type, VIM_SIZEOF(txn_type));
    vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));

    entry_opts.timeout = ENTRY_TIMEOUT;

    entry_opts.min = 1;
    entry_opts.max = CUSTOM_PERCENT_MAX_LEN;

    vim_strcpy(txn_type, type);
    entry_opts.entry_options = KBD_OPT_BEEP_ON_CLR;

    // RDD 020115 SPIRA:8285 Need to check events for integrated
    entry_opts.CheckEvents = is_integrated_mode() ? VIM_TRUE : VIM_FALSE;

    terminal.configuration.tip_type = other_tip;

#ifndef _WIN32
    VIM_ERROR_RETURN_ON_FAIL( display_data_entry( entry_buf, &entry_opts, ScreenID, VIM_PCEFTPOS_KEY_CANCEL, "ENTER TIP %", "Enter Tip %" ));
#else
    VIM_ERROR_RETURN_ON_FAIL( display_screen( ScreenID, VIM_PCEFTPOS_KEY_CANCEL, "ENTER TIP %", "Enter Tip %" ));
    VIM_ERROR_RETURN_ON_FAIL( data_entry( entry_buf, &entry_opts ));
#endif
    TipSetCustomPercent = asc_to_bin( entry_buf, VIM_MIN( vim_strlen( entry_buf ), CUSTOM_PERCENT_MAX_LEN ));

    return VIM_ERROR_USER_BACK;
}



VIM_ERROR_PTR FixedTip(void)
{    
    terminal.configuration.tip_type = inline_tip;
    return VIM_ERROR_USER_BACK;
}

VIM_ERROR_PTR NoTip(void)
{
    terminal.configuration.tip_type = no_tip;
    return VIM_ERROR_USER_BACK;
}



S_MENU PercentageTipMenu[] = { { "Add 5% Tip", TipAdd5Percent },
                            {  "Add 10% Tip", TipAdd10Percent },
                            {  "Add other % Tip", TipAddCustomPercent } };



VIM_ERROR_PTR PercentageTip(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(PercentageTipMenu, VIM_LENGTH_OF(PercentageTipMenu), "Percentage Tip", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
    VIM_ERROR_RETURN( res );
}


S_MENU TipConfigMenu[] = { { "Percentage Tip", PercentageTip },
                                { "Fixed Tip", FixedTip },
                                { "No Tip"  , NoTip } };


static void rc_item(const VIM_CHAR *label, const VIM_CHAR *item, VIM_CHAR *pBuffer)
{
    VIM_UINT8 label_length = vim_strlen(label);
    VIM_CHAR *Ptr = pBuffer;
    VIM_UINT8 line_length = 24;
    VIM_SIZE item_length = vim_strlen(item);

    vim_mem_set(Ptr, ' ', line_length);
    Ptr += line_length;
    *Ptr = 0;
    Ptr = pBuffer;
    vim_mem_copy(Ptr, label, label_length);
    Ptr += (line_length - item_length);
    vim_mem_copy(Ptr, item, item_length);
}

VIM_ERROR_PTR tip_configure(void)
{
    VIM_CHAR Buffer[100] = { 0 };
    rc_item(type, AmountString(txn.amount_total), Buffer);
#ifndef _XPRESS
    pceft_pos_display("Refer to PINpad", "", VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_GRAPHIC_PROCESSING);
#endif
    VIM_ERROR_RETURN( MenuRun( TipConfigMenu, VIM_LENGTH_OF(TipConfigMenu), Buffer, VIM_FALSE, VIM_TRUE));
}


// JIRA PIRPIN-1120 add tipping support with max %ge etc. 
txn_state_t txn_tip_amount_entry(void)
{
  VIM_ERROR_PTR res;
  VIM_SIZE TipPercentage = 0;
  VIM_DBG_MESSAGE( "Tip Processing State" );

  if (transaction_get_status(&txn, st_giftcard_s_type))
  {
	  return state_get_next();
  }
	  

  /* Dont do tip entry when base only flag set, unless its an adjustment txn */
  if( TipAllowed(&txn) )
  {
      if ((res = tip_configure()) == VIM_ERROR_POS_CANCEL)
      {
          TXN_RETURN_ERROR(res);
      }
      else if (res == VIM_ERROR_TIMEOUT)
      {
          TXN_RETURN_ERROR(res);
      }
      else if (res == VIM_ERROR_USER_CANCEL)
      {
          return(state_get_current());
      }

      if (terminal.configuration.tip_type != inline_tip)
      {
          switch (terminal.configuration.tip_type)
          {
              case no_tip:
              default:
              return state_get_next();

              case five_percent_tip:
                  txn.amount_tip = (txn.amount_purchase * 5) / 100;
                  break;

              case ten_percent_tip:
                  txn.amount_tip = (txn.amount_purchase * 10) / 100;
                  break;
                  
              case other_tip:
                  txn.amount_tip = (txn.amount_purchase * TipSetCustomPercent)/100;
                  break;
          }
      }

      else if(( res = txn_amount_entry( "Key Tip in $Aud\nThen Press Enter", &txn.amount_tip )) != VIM_ERROR_NONE )
	  {
		  // RDD 301015 SPIRA:8830
	    if (res == VIM_ERROR_USER_BACK)
			return(state_get_current());
		  //return(state_get_last());
		TXN_RETURN_EXIT(res);
	  }
#if 0	// WOW don't want a max tip limit
	  if (( terminal.configuration.max_tip_percent * txn.amount_total ) < ( txn.amount_tip*100 ))
	  {
		  //TXN_RETURN_ERROR(ERR_TIP_OVER_MAX);
		  // RDD 240114 - TODO seems dumb to kill the whole txn over a too big tip better just make them re-enter the tip
		  display_result( ERR_TIP_OVER_MAX , "", "", "", VIM_PCEFTPOS_KEY_NONE, THREE_SECOND_DELAY );
		  return state_get_current();
	  }
#endif
      TipPercentage = (txn.amount_tip * 100) / txn.amount_purchase;
      if(( TipPercentage > terminal.configuration.max_tip_percent ) && ( terminal.configuration.max_tip_percent != 0 ))
      {
          VIM_CHAR Buffer[100]; 
          vim_sprintf(Buffer, "Tip Exceeds Max of %d%%", terminal.configuration.max_tip_percent);
          vim_sound(VIM_TRUE);
          vim_event_sleep(250);
          vim_sound(VIM_TRUE);
          display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer, "Please Reduce Tip" );
          vim_event_sleep(4000);
          return state_get_current();
      }
      else
      {
          display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Thank you !", "Tip added to total");
          vim_event_sleep(1000);
      }
	  txn.amount_total += txn.amount_tip;
  }
  return state_get_next();
}



txn_state_t txn_cashout_amount_entry(void)
{
	VIM_ERROR_PTR res;
	DBG_MESSAGE("State: Cashout amount entry ");
	if (!IS_STANDALONE)
		return state_get_next();
	if (transaction_get_status(&txn, st_giftcard_s_type))
		return state_get_next();

	if (!TERM_CASHOUT_MAX)
	{
		if (txn.type == tt_cashout)
		{
			txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
			TXN_RETURN_EXIT(txn.error);
		}
		// RDD 050814 - not in spec but probably useful to bypass cash entry if no max cash or max cash = 0;
		else
		{
			DBG_MESSAGE("Cashout zero limit: skip state");
			return state_get_next();
		}
	}
	
	while( TERM_CASHOUT_ENABLE )
	{
		DBG_MESSAGE("Cashout permitted - go to amount entry");
		if(( res = txn_amount_entry( "Enter CashOut Amount", &txn.amount_cash )) != VIM_ERROR_NONE )
		{
			if (res == VIM_ERROR_USER_BACK)
				return(state_get_current());

			TXN_RETURN_ERROR(res);
		}

		if(( !txn.amount_cash ) && ( txn.type == tt_cashout ))
		{
			if( config_get_yes_no( "INVALID AMOUNT\nRE-ENTER?", VIM_NULL ))
				continue;
			else
				TXN_RETURN_ERROR(ERR_INVALID_TOTAL_AMOUNT);
		}


		if (( txn.amount_cash  > (100*TERM_CASHOUT_MAX))&&( TERM_CASHOUT_MAX ))
		{
			display_result( ERR_CASH_OVER_MAX , "", "", "", VIM_PCEFTPOS_KEY_OK, THREE_SECOND_DELAY );
			return state_get_current();
		}


		txn.amount_total += txn.amount_cash;
    if( txn.amount_purchase && txn.amount_cash )
      txn.type = tt_sale_cash;

		break;
	}
	return state_get_next();
}


static VIM_BOOL is_valid_ccv(VIM_CHAR *ccv)
{
  VIM_UINT8 len;
  VIM_INT8 i;

  len = vim_strlen(ccv);

  // RDD 070114 - determine whether CCV bypass is available or not
  if( !len ) return VIM_FALSE;

  VIM_DBG_VAR(len);
  VIM_DBG_DATA(ccv,len);

  if (len > MAX_CCV_LENGTH)
    return VIM_FALSE;
  // RDD 030614 SPIRA:7795
  if (len < MIN_CCV_LENGTH)
    return VIM_FALSE;

  /* skip the space */
  for ( i = len-1; i >= 0; i-- )
  {
    if( ccv[i] == ' ' )
    {
      ccv[i] = 0;
      len--;
    }
    else
      break;
  }

  for ( i = 0; i < len; i++ )
  {
    if ( (ccv[i] < '0') || (ccv[i] > '9') )
      return VIM_FALSE;
  }

  return VIM_TRUE;
}


txn_state_t txn_get_CCV(void)
{
  VIM_CHAR entry_buf[20];
  kbd_options_t options;
  VIM_ERROR_PTR res;
  VIM_BOOL BypassAllowed = VIM_FALSE;
  VIM_SIZE Display;

  DBG_VAR( terminal.configuration.ccv_entry );


  if (!transaction_get_status(&txn, st_moto_txn))
  {
      return state_get_next();
  }

  if( terminal.configuration.ccv_entry != 'O' && terminal.configuration.ccv_entry != 'M' )
	  return state_get_next();

  //if (!is_standalone_mode())
	//  return state_get_next();

  // RDD 170113 added for WBC - too long for large amounts !!!
  //get_txn_type_string( &txn, type, VIM_SIZEOF(type), VIM_FALSE );
  get_txn_type_string_short( &txn, type, VIM_SIZEOF(type) );

  // RDD CVV is only prompted for in integrated for manual entry
  if( txn.card.type != card_manual )
	  return (state_skip_state());

  // No CVV if CASH component
  if( txn.amount_cash != 0 )
	  return (state_skip_state());

  /* Clear the ccv */
  vim_mem_clear(txn.card.data.manual.ccv, VIM_SIZEOF(txn.card.data.manual.ccv));
  

  if(1)
  {
    //VIM_UINT32 key;

    if( ((txn.card.type == card_manual) && ((txn.card.data.manual.ccv[0] == 0) || !is_valid_ccv(txn.card.data.manual.ccv)))
      || ((txn.card.type == card_mag) && ((txn.card.data.mag.ccv[0] == 0) || !is_valid_ccv(txn.card.data.mag.ccv))) )
    {
 
	  if(1)
      {
        VIM_DBG_POINT;
        vim_mem_set(txn.card.data.manual.ccv, 0, VIM_SIZEOF(txn.card.data.manual.ccv));

        while ( VIM_TRUE )
        {
          /* Set the options */
          options.timeout = ENTRY_TIMEOUT;
          options.min  = MIN_CCV_LENGTH;
          options.max  = MAX_CCV_LENGTH;
		  options.entry_options = KBD_OPT_NO_LEADING_ZERO;
		  options.CheckEvents = is_standalone_mode() ? VIM_FALSE : VIM_TRUE;

		  // RDD 070114 - data entry options determined by macro wrt whether to allow bypass or not

		  // If LFD says bypass allowed then allow bypass
          // Refund may not have customer present so make optional
		  if(( terminal.configuration.ccv_entry == 'O' )||( txn.type == tt_refund ))
		  {
              options.min = 0;			
              BypassAllowed = VIM_TRUE;
		  }

          Display = BypassAllowed ? (DISP_CCV_OPT_DATA_ENTRY) : (DISP_CCV_DATA_ENTRY);
          // RDD 251121 JIRA PIRPIN-1256 : No decision yet on whether to actually change this 
          // RDD 130320 - code clean up
#ifndef _WIN32
          vim_mem_set(entry_buf, 0, sizeof(entry_buf));
          res = display_data_entry(entry_buf, &options, Display, VIM_PCEFTPOS_KEY_CANCEL, type, (VIM_UINT32)AmountString(txn.amount_total));
#else
          display_screen( Display, VIM_PCEFTPOS_KEY_CANCEL, type, (VIM_UINT32)AmountString(txn.amount_total));

          /* Now get prompt */
          vim_mem_set(entry_buf, 0, sizeof(entry_buf));
          res = data_entry(entry_buf, &options);
#endif

          /*pceft_pos_display_close();*/

          if (res == VIM_ERROR_TIMEOUT)
          {
              display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, 1000 );
              TXN_RETURN_EXIT(VIM_ERROR_TIMEOUT);
          }
          VIM_DBG_POINT;
          if (res == VIM_ERROR_USER_CANCEL)
          {
            txn.card.type = card_none;
            /*state_set_state(ts_card_capture);*/
            VIM_DBG_POINT;
            return ts_card_capture;
          }
           /*TXN_RETURN_ERROR(VIM_ERROR_USER_CANCEL);*/

          if (VIM_ERROR_POS_CANCEL == res)
            TXN_RETURN_ERROR(VIM_ERROR_POS_CANCEL);

          if (res == VIM_ERROR_NONE)
          {
			  if( vim_strlen( entry_buf ) || !BypassAllowed )
			  {
				  if ( is_valid_ccv(entry_buf) )
				  {
					  /* Save the data entered */
					  if ( txn.card.type == card_manual )
					  {
						  /* Save the data entered */
						  vim_mem_copy(txn.card.data.manual.ccv, entry_buf, MIN(vim_strlen(entry_buf), MAX_CCV_LENGTH));
					  }
					  else
					  {
						  /* Save the data entered */
						  vim_mem_copy(txn.card.data.mag.ccv, entry_buf, MIN(vim_strlen(entry_buf), MAX_CCV_LENGTH));
					  }
					  return state_get_next();
				  }
				  else
				  {
					  //display_screen(DISP_INVALID_CCV, VIM_PCEFTPOS_KEY_OK);
					  display_screen( DISP_INVALID_CCV, VIM_PCEFTPOS_KEY_NONE );
					  vim_event_sleep(2000);
					  //vim_kbd_read(&key_code, VIM_MILLISECONDS_INFINITE);
					  /* Disply invlad CVV, please re-enter it */
					  continue;
				  }
			  }
			  else
				  break;
		  }
        }
      }
    }
  }
  return (state_skip_state());
}


txn_state_t txn_confirm_details(void)
{
    VIM_CHAR terminal_text[100] = { 0 };
    vim_mem_clear(terminal_text, VIM_SIZEOF(terminal_text));

    VIM_CHAR Pan[19 + 1];
    VIM_SIZE PanLen = 0;
    vim_mem_clear(Pan, VIM_SIZEOF(Pan));
	transaction_t temp_txn = txn;

#if 0
    if (TERM_USE_PCI_TOKENS)
    {
        return state_skip_state();
    }
#endif

    // Don't alow void of voided txn )
    if(( txn.type == tt_void ) && (transaction_get_status(&txn, st_void)))
    {
        DisplayTxnWarning(&txn, "Transaction was\nAlready Voided");
        txn.error = VIM_ERROR_EMV_UNABLE_TO_PROCESS;
        pceft_debug(pceftpos_handle.instance, "Attempt to void voided transaction");
        TXN_RETURN_EXIT(txn.error);
    }

    if(( txn.type != tt_checkout ) && ( txn.type != tt_preauth_enquiry ) && (!is_approved_txn(&txn)))
    {
        DisplayTxnWarning(&txn, "Transaction was\nOriginally Declined");
        txn.error = VIM_ERROR_EMV_UNABLE_TO_PROCESS;
        pceft_debug(pceftpos_handle.instance, "Attempt to adjust declined transaction");
        TXN_RETURN_EXIT(txn.error);
    }

    else if ((txn.type == tt_checkout)||(txn.type == tt_preauth_enquiry ))
    {
        PREAUTH_SAF_DETAILS PreauthDetails;
        PREAUTH_SAF_DETAILS *PreauthDetailsPtr = &PreauthDetails;

		txn.type = tt_checkout;

        if (transaction_get_status(&txn, st_checked_out))
        {
            DisplayTxnWarning(&txn, "Transaction was\nAlready Completed");
            txn.error = VIM_ERROR_EMV_UNABLE_TO_PROCESS;
            pceft_debug(pceftpos_handle.instance, "Attempt to complete completed txn");
            TXN_RETURN_EXIT(txn.error);
        }
        else if (preauth_table_get_record( &PreauthDetailsPtr, txn.roc ) == VIM_ERROR_NONE)
        {
            if ( PreauthDetailsPtr->amount < txn.amount_total)
            {
                //DisplayTxnWarning(&txn, "Completion exceeds\nAuthorised amount");
				pceft_debug(pceftpos_handle.instance, "(USER RRN) Completion exceeds\nAuthorised amount");
			}
			if (pa_read_record(txn.roc, &temp_txn) != VIM_ERROR_NONE)
			{			
				DisplayTxnWarning(&txn, "Unable to Recall\nOriginal Transaction");				
				txn.type = tt_sale;				
				txn.card_state = card_capture_standard;				
				return state_set_state(ts_card_capture);				
			}
        }
    }

    // get the pan of the current txn 
    card_get_pan(&temp_txn.card, Pan);
    PanLen = vim_strlen(Pan);
    // Make the Truncated pan

    vim_sprintf(terminal_text, "RRN %d FOUND\nAUTH AMT $%d.%2.2d\nCARD ...%3.3s\nCONTINUE?", txn.roc,
        (VIM_INT32)(temp_txn.amount_total / 100), (VIM_INT32)(temp_txn.amount_total % 100), &Pan[PanLen-3] );

    // Timeout results in void - only cancel here will actually cancel it.
    if( config_get_yes_no_timeout(terminal_text, VIM_NULL, ENTRY_TIMEOUT, VIM_TRUE) == VIM_TRUE )
    {
        // RRN .... AMT $..... Continue ? = YES
        if (txn.type == tt_checkout)
		{
			// Assign recalled SAF txn to current txn
			txn = temp_txn;
            // Checkout requires a signature
            transaction_set_status(&txn, st_completion);
            // We have card details from SAF
            transaction_set_status(&txn, st_pos_sent_pci_pan);
			// Needed to avoid unncessary reversal generation
			transaction_clear_status(&txn, st_message_sent);
			txn.type = tt_checkout;
        }
        return state_get_next();
    }
    txn.error = VIM_ERROR_POS_CANCEL;
    display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
    TXN_RETURN_EXIT(txn.error);
}



VIM_BOOL allow_cashout(void)
{
	// no cashout on credit accounts unless otherwise dictated by CPAT
	if((( txn.account == account_credit )||( txn.account == account_default )) && (!CARD_NAME_ALLOW_CASH_ON_CR_ACC )) return VIM_FALSE;

	// no cashout for gift cards
	if( gift_card(&txn) ) return VIM_FALSE;

	return VIM_TRUE;
}

VIM_BOOL is_valid_roc(VIM_CHAR* string, VIM_SIZE len)
{
  VIM_UINT32 i;

  if ( len > 10 )
    return VIM_FALSE;

  for(i = 0; i < len; i++)
  {
    if (string[i] < '0' || string[i] > '9')
      return VIM_FALSE;
  }
  return VIM_TRUE;
}



txn_state_t txn_select_preauth(void)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR AuthCode[7];
	VIM_SIZE batch_index;
	if(( res = preauth_select_record( &txn.roc )) != VIM_ERROR_NONE )
	{
		//TXN_RETURN_ERROR( res );
		TXN_RETURN_EXIT( res );
	}

	// RDD 010515 Special request from Donna...
#if 0
	if( saf_full(VIM_FALSE) )
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Pinpad Full", "Logon Required" );
		vim_event_sleep(2000);
		TXN_RETURN_EXIT( VIM_ERROR_SAF_FULL );
	}
#endif

	/* Now search for the roc in batch */
	if(( res = saf_search_for_roc_tt( txn.roc, &batch_index, txn.type, AuthCode )) == VIM_ERROR_NONE )
	{
		txn.original_index = batch_index;
	}
	return state_get_next();
}



txn_state_t txn_get_roc(void)
{
  VIM_CHAR entry_buf[20];
  VIM_ERROR_PTR res;
  VIM_UINT16 roc;
  VIM_UINT32 batch_index;
  VIM_CHAR auth_no[7];
  VIM_UINT32 Opts = 0;

  vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));
  vim_mem_clear(auth_no, VIM_SIZEOF(auth_no));

  // RDD 250222 - Altready have recalled details from PA
  if (transaction_get_status(&txn, st_pos_sent_pci_pan))
  {
	  DBG_MESSAGE("SSSSSSSSSSS Skipping RRN entry SSSSSSSSSSSSS");
	  return state_set_state(ts_get_approval_code);
  }

  /* Get txn type info  string */
  get_txn_type_string( &txn, type, sizeof(type), VIM_FALSE );

  if(( res = data_entry_screen( DISP_GET_ROC_STANDALONE, type, "", entry_buf, 0, 6, Opts )) != VIM_ERROR_NONE )
  {
	  // RDD 011214 - ensure operator cancel/timeout is reported if error
	  //transaction_set_status( &txn, st_incomplete );
	  terminal_set_status( ss_incomplete );	// RDD 161215 SPIRA:8855

	  txn_save();
	  TXN_RETURN_ERROR( res );
  }
  else
  {
	  VIM_DBG_ERROR(res);

	  if( !is_valid_roc( entry_buf, vim_strlen(entry_buf) ))
		  return state_get_current();

	  /* Save the data entered */
	  roc = asc_to_bin(entry_buf, MIN(vim_strlen(entry_buf), 10));
  }
  VIM_DBG_VAR(roc);

  /* Display processing screen */
  display_screen(DISP_SCANNING_BATCH_PLEASE_WAIT,VIM_PCEFTPOS_KEY_NONE);
  vim_event_sleep(500);

  VIM_DBG_VAR(txn.type);
  VIM_DBG_VAR(txn.roc);

  /* Now search for the roc in batch */
  res = saf_search_for_roc_tt( roc, &batch_index, txn.type, auth_no );

  VIM_DBG_ERROR(res);

  if ( res != VIM_ERROR_NONE )
  {
	  //VIM_BOOL StartNew = VIM_TRUE;

	  if( res == ERR_ROC_COMPLETED )
	  {
		  DisplayTxnWarning(&txn, "Already Completed");
		  txn.error = ERR_INVALID_ROC;
		  pceft_debug(pceftpos_handle.instance, "Invalid RRN Already Completed");
		  TXN_RETURN_EXIT(txn.error);
	  }
	  else if( res == ERR_ROC_EXPIRED )
	  {
		  DisplayTxnWarning(&txn, "Preauth Expired");
		  txn.error = ERR_INVALID_ROC;
		  pceft_debug(pceftpos_handle.instance, "Invalid RRN Already Expired");
		  TXN_RETURN_EXIT(txn.error);
	  }
	  else if( res != VIM_ERROR_NONE )
	  {
#if 0
		  if (IS_INTEGRATED)
		  {
			  DisplayTxnWarning(&txn, "No Match for RRN");
			  txn.error = ERR_INVALID_ROC;
			  pceft_debug(pceftpos_handle.instance, "No Match for RRN");
			  TXN_RETURN_EXIT(txn.error);
		  }
#endif
		  // Standalone go to amount entry but need to prompt for Auth Code ( as per VX680 )
		  if (config_get_yes_no("RRN NOT FOUND\nNEW COMPLETION?", VIM_NULL))
		  {
			  txn.type = tt_checkout;
			  //txn.error_backup = ERR_INVALID_ROC;
			  if (IS_INTEGRATED)
			  {
				  return state_set_state(ts_card_capture);
			  }
			  else
			  {
				  return state_set_state(ts_sale_amount_entry);
			  }
		  }
		  else
		  {
			  txn.error = VIM_ERROR_USER_CANCEL;
			  TXN_RETURN_EXIT(txn.error);
		  }
	  }
  }
  else
  {
        /* store original txn for completion and pre-auth enquiry transaction */
        if ((tt_checkout == txn.type) || (tt_preauth_enquiry == txn.type))
          txn.original_index = batch_index;

        txn.roc = roc;
        txn.roc_index = batch_index;
        vim_mem_copy(txn.auth_no, auth_no, VIM_SIZEOF(txn.auth_no));
        transaction_set_status(&txn, st_read_from_batch);
        return state_get_next();
  }
  return state_get_next();
}



txn_state_t txn_preswipe_amount_entry(void)
{
  VIM_TEXT acc_string[3+1];
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_KBD_CODE key_pressed;

  get_acc_type_string_short(txn.account,acc_string);

  if( allow_cashout() )
  {
      display_screen(DISP_SELECT_OPTION, VIM_PCEFTPOS_KEY_CANCEL, "ACCOUNT", acc_string );
  }
  else
  {
      display_screen(DISP_PS_ENTER_AMOUNT, VIM_PCEFTPOS_KEY_CANCEL, "ACCOUNT", acc_string );
  }

  while (1)
  {
     /* wait for key */
	if(( res = screen_kbd_touch_read_with_abort( &key_pressed, ENTRY_TIMEOUT, VIM_FALSE, VIM_FALSE )) != VIM_ERROR_NONE )
	{
		TXN_RETURN_ERROR(res);
	}

	/* All ok so process the key */
    switch (key_pressed)
    {
       case VIM_KBD_CODE_SOFT1:
       txn.preswipe_status = completed_preswipe;
       TXN_RETURN_ERROR(res);

       case VIM_KBD_CODE_SOFT3:
		  if(( res = txn_amount_entry( "OTHER AMOUNT", &txn.amount_purchase )) != VIM_ERROR_NONE )
		  {
			TXN_RETURN_CAPTURE(res);
		  }
		  txn.preswipe_status = completed_preswipe;
		  TXN_RETURN_ERROR(res);

       case VIM_KBD_CODE_SOFT2:
		  if(( res = txn_amount_entry( "CASH AMOUNT", &txn.amount_cash )) != VIM_ERROR_NONE )
		  {
			TXN_RETURN_CAPTURE(res);
		  }
		  txn.preswipe_status = completed_preswipe;
		  TXN_RETURN_ERROR(res);

	   case VIM_KBD_CODE_CANCEL:
	   TXN_RETURN_CAPTURE(res);

       default:
       vim_kbd_sound_invalid_key();
       continue;
     }
  }
}



txn_state_t txn_get_approval_code(void)
{
  VIM_ERROR_PTR res;
  VIM_UINT32 Opts = KBD_OPT_ALPHA;

  DBG_MESSAGE("Get Auth Number");
  // If the txn isn't a completion or a checkout then also skip
  if(( tt_checkout != txn.type ) && ( tt_completion != txn.type ))
  {
	  DBG_POINT;
	  return state_skip_state();
  }


  // If we have a valid AuthCode (from POS or from recalled transaction ) then set flags for 0220 and Skip this state
  if (ValidateAuthCode(txn.auth_no))
  {
	  transaction_set_status(&txn, st_needs_sending);
	  //transaction_set_status( &txn, st_pos_sent_pci_pan );
	  transaction_set_status(&txn, st_completion);

	  txn.print_receipt = VIM_TRUE;
	  // RDD 250514 SPIRA:7793 - get sig if checkout OK
	  transaction_set_status(&txn, st_signature_required);
	  return state_skip_state();
  }


  // clear any invalid auth number
  vim_mem_clear(txn.auth_no, VIM_SIZEOF(txn.auth_no));

  DBG_POINT;
  if(( res = data_entry_screen( DISP_GET_APPROVAL_CODE, "", "", txn.auth_no, AUTH_CODE_LEN, AUTH_CODE_LEN, Opts )) == VIM_ERROR_NONE )
  {
	  // Auth code validates checkout
	  // RDD 131115 SPIRA:8845 Agreed with DG to not allow authcode of all zeros
	  if( ValidateAuthCode( txn.auth_no ) == VIM_FALSE )
	  {
		  display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Auth Code Invalid", "Please Re-enter" );
		  vim_event_sleep(2000);
		  return state_get_current();
	  }
	  if( txn.type == tt_checkout )
	  {
		  transaction_set_status( &txn, st_needs_sending );
		  //transaction_set_status( &txn, st_pos_sent_pci_pan );
		  transaction_set_status( &txn, st_completion );
		  //txn.error = ERR_SIGNATURE_APPROVED_OFFLINE;

		  //vim_strcpy( txn.host_rc_asc, "08" );

		  txn.print_receipt = VIM_TRUE;
		  // RDD 250514 SPIRA:7793 - get sig if checkout OK
		  transaction_set_status( &txn, st_signature_required );
	  }
	  return state_get_next();
  }
  else
  {
	  TXN_RETURN_ERROR(res);
  }
}

txn_state_t txn_emv_transaction(void)
{
  return ts_finalisation;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR txn_do_init(transaction_t* transaction)
{
	if (!transaction)
	{
		VIM_ERROR_RETURN(VIM_ERROR_NULL);
	}
	vim_mem_clear(transaction, VIM_SIZEOF(transaction_t));

	// DBG_MESSAGE(" !!! CLEARING TXN STRUCT.... !!!!");
	vim_rtc_get_time(&transaction->time);

	// RDD 291217 get event deltas for logging

	vim_mem_clear( transaction->uEventDeltas.ByteBuff, VIM_SIZEOF( u_TxnEventCheck ));

	vim_rtc_get_uptime( &transaction->uEventDeltas.Fields.POSReq );

	//VIM_DBG_NUMBER( transaction->uEventDeltas.Fields.POSReq );

	//VIM_DBG_NUMBER( transaction->uEventDeltas.Fields.POSReq );

	transaction->read_icc_count = 0;

	// RDD 010812 SPIRA:5812 this flag is used to determine if RC 55 triggers a retry PIN so we only clear it if a pin was actually entered
	transaction->pin_bypassed = VIM_FALSE;

	// RDD 100812 SPIRA:5872 Set Default Index to Max so that we can max BIN range invalid if card data not captured before txn is cancelled
	transaction->card_name_index = WOW_MAX_CPAT_ENTRIES;

	transaction->issuer_index = 0xFF;
	transaction->roc_index = 0xFFFFFFFF;

	transaction->update_index = 0xFFFFFFFF;   /* No udpate index */
	transaction->original_index = 0xFFFFFFFF; /* No original index */

	transaction->error = VIM_ERROR_NONE;
	transaction->error_backup = VIM_ERROR_NONE;

	transaction->ClearedBalance = -1;
	transaction->LedgerBalance = -1;

	// transaction->batch_no = terminal.batch_no;
	transaction->card_state = card_capture_standard;

	transaction->read_only = VIM_FALSE;      /* default to enable read */

    // RDD 170719 added to avoid crash in loyalty build and ensure loyalty data is not sent when not available
    //transaction->session_type = VIM_ICC_SESSION_UNKNOWN;

  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR txn_init(void)
{
	terminal.abort_transaction = VIM_FALSE;
	//terminal.allow_idle = VIM_TRUE;
	//terminal.allow_ads = VIM_FALSE;

	txn_do_init(&txn);

	// RDD - only need to do this once !
	get_txn_type_string(&txn, type, VIM_SIZEOF(type), VIM_FALSE);

	// RDD 291217 JIRA WWBP-19 Used to always delete existing Loyalty data but now only delete on receipt of J8
	//vim_file_delete( LOYALTY_FILE );

#ifndef _PHASE4
	txn_save();
#else
	txn.type = tt_none;
#endif

	return VIM_ERROR_NONE;
}




VIM_ERROR_PTR txn_process_states( txn_state_t cont_state )
{
  VIM_UINT32 state;

  /* Setup the txn flow we are going to follow */
  determine_transaction_states(txn.type);

  // RDD 230215 SPIRA:8411 Set Backlight to 100% when key pressed
  vim_lcd_backlight_level(100);

  if( ts_no_state == cont_state )
  { /* Extract the initial state from the current flow */
    state = state_get_next();
  }
  else
  { /* Set state to requested */
    state = state_set_state( cont_state );
  }

  while (1)
  {
   //VIM_DBG_VAR(state);
    DBG_TIMESTAMP;
    switch (state)
    {
      case ts_check_tms_logon:
        state = txn_check_tms_logon();
        break;

      case ts_check_password:
        state = txn_check_passwords();
        break;

      /* todo: should change this to only check terminal permissions */
      case ts_check_permissions:
        state = txn_check_permissions();
        break;

	  case ts_check_limits:
		  state = txn_check_limits();
		  break;


      case ts_get_roc_number:
        //state = state_skip_state();   /* RDD removed */
        state = txn_get_roc();
        break;

	  case ts_select_preauth:			// RDD 200315 added
		  state = txn_select_preauth();
		  break;

      case ts_complete_variations:
        //state = txn_complete_variations();
        break;

      case ts_card_capture:
        state = txn_card_capture();
        break;

      case ts_card_check:
        state = txn_card_check();
        break;

      case ts_get_CCV:
         state = txn_get_CCV();
         break;

      case ts_get_user_id:
        state = state_skip_state();   /* TODO: */
        break;

      case ts_sale_amount_entry:
        state = txn_sale_amount_entry();
        break;

	  case ts_tip_amount_entry:
		state = txn_tip_amount_entry();
        break;

      case ts_select_account:
          if (WOW_NZ_PINPAD)
          {
              state = txn_select_nz_account(VIM_FALSE);
          }
          else
          {
              state = txn_select_account(VIM_FALSE);
          }
        break;

      case ts_preswipe_amount_entry:
        state = txn_preswipe_amount_entry();
        break;

      case ts_cashout_amount_entry:
        state = txn_cashout_amount_entry();
        break;

	  case ts_is_refund:
		state = txn_is_refund();
		break;

      case ts_confirm_details:
        state = txn_confirm_details();
        break;

      case ts_get_pin:
        state = txn_get_pin(VIM_FALSE);
        break;

      case ts_establish_network:
        state = txn_establish_network();   // RDD 121114 added for ALH standalone
        break;

      case ts_authorisation_processing:
        state = txn_authorisation_processing();
        break;

      case ts_get_approval_code:
        state = txn_get_approval_code();
        break;

      case ts_efb:
        efb_process_txn();
        state = ts_finalisation;
        break;

      case ts_error:
        state = txn_display_error();
        break;

      case ts_saf_print:
        //state = txn_saf_print();
        break;

      case ts_finalisation:
        state = txn_finalisation();
        break;

      case ts_emv_transaction:
        // DBG_MESSAGE("EMV TRAN");
        return ERR_EMV_TRAN;

      case ts_batch_full:
        state = txn_batch_full();
        break;

        // RDD 240620 Trello #167 OpenPay Implementation
      case ts_enter_barcode:
          state = txn_enter_barcode();
          break;

          // RDD 240620 Trello #167 OpenPay Implementation
      case ts_enter_passcode:
          state = txn_enter_passcode();
          break;

      case ts_exit:
		// RDD - if preswipe then always return to
		DBG_MESSAGE( "<<<<<<<<<< EXIT STATE >>>>>>>>>");
		VIM_DBG_NUMBER(txn.type);

        // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
        if (txn.type == tt_query_card_openpay)
        {
            //pceft_debug_test( pceftpos_handle.instance, "Exit Query - Clear display" );
            //IdleDisplay();
        }

        if(!TERM_NEW_VAS)
        {
            if (txn.type == tt_query_card)
            {
                DBG_POINT;
                //VIM_DBG_SEND_TARGETED_UDP_LOGS("<<<<<<<<<< txn.c EXIT STATE >>>>>>>>>");
#ifndef _WIN32
                vim_vas_close();
#endif
            }
		}
		terminal.idle_time_count = 0;
		//terminal.allow_ads = VIM_TRUE;   //JQ 10-01-2013
		vim_rtc_get_uptime(&terminal.idle_start_time);
		terminal.screen_id = 0;

		if( txn.type == tt_mini_history )
		{
			// RDD 161215 SPIRA:8855
			//transaction_clear_status(&txn, st_incomplete);
			terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855

			txn_save();
			return VIM_ERROR_NONE;	// RDD 160412 SPIRA:5276 No end of txn processing for mini-history
		}

		// RDD 250112 - always close PICC reader else will be active during IDLE
		//reset_picc_transaction();
#if 0
			picc_close( VIM_TRUE );
#endif
		// RDD 230215 SPIRA:8411 Set Backlight to 100% when key pressed
		vim_lcd_backlight_level(100);

		if( txn.card.type == card_chip )
		{
#if 1
			if( !terminal_get_status(ss_wow_basic_test_mode) || !terminal_get_status(ss_auto_test_mode) )
				emv_remove_card( "" );
#else

			if(( txn.type == tt_query_card )&&( txn.error == VIM_ERROR_NONE ))
			{
				VIM_DBG_ERROR(txn.error);
			}
			else
			{
				emv_remove_card();
			}
#endif
		}
		// RDD 080814 changed to retrieve little used status bit !
		if( txn.error != VIM_ERROR_PRINTER )
		{
		    // DBG_MESSAGE( "<<<<<<<<<< CLEAR INCOMPLETE STATUS NO POWER ON REVERSAL FROM THIS POINT >>>>>>>>>");
			//transaction_clear_status(&txn, st_incomplete);
			terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855
		}

		txn_save();

		DBG_POINT;

		// RDD 060412 SPIRA:5260 DO NOT SAVE DUPLICATES OF CANCELLED TXNS
		// RDD 061112 SPIRA:6259 DO NOT SAVE TRANSACTION IF CARD WAS BLOCKED
		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			if(( txn.error != VIM_ERROR_POS_CANCEL ) && ( txn.error != VIM_ERROR_TIMEOUT ) && ( txn.error != VIM_ERROR_EMV_CARD_BLOCKED ))
			{
				// RDD 290312: SPIRA 5221 - Reversals being sent when duplicate txn recalled because "incomplete" flag not being SET
				DBG_MESSAGE( "<<<<<<<<<< SAVE DUPLICATE TXN >>>>>>>>>");
				txn_duplicate_txn_save();
			}
		}
#if 1
		else if( txn.error == VIM_ERROR_NONE )
#else
		else if(( txn.error != VIM_ERROR_POS_CANCEL ) && ( txn.error != VIM_ERROR_TIMEOUT ) && ( txn.error != VIM_ERROR_EMV_CARD_BLOCKED ))
#endif
		{
		    // RDD 290312: SPIRA 5221 - Reversals being sent when duplicate txn recalled because "incomplete" flag not being SET
		    DBG_MESSAGE( "<<<<<<<<<< SAVE DUPLICATE TXN >>>>>>>>>");
			txn_duplicate_txn_save();
		}
		else
		{
		    DBG_MESSAGE( "<<<<<<<<<< DON'T SAVE DUPLICATE TXN >>>>>>>>>");
		}

		/* write the structure to a file */
		if( txn.type != tt_preswipe )
			//pceft_pos_display_close();
#ifndef _PHASE4	// RDD 070213 - this was already done and gets done again before the next event handling session.
        vim_mag_flush(mag_handle.instance_ptr);
#endif
		// RDD 101214 - PCEFTPOS system we must not disconnect until here
		// RDD 311214 SPIRA:8293 Experiment with disabling Disconnect to speed things up
		if (TERM_OPTION_PCEFT_DISCONNECT_ENABLED)
		{
			VIM_DBG_MESSAGE("DISCONNECT BECAUSE EXIT");
			comms_disconnect();
		}
        vim_kbd_flush();
        clear_emv_query_flag();
			DBG_POINT;

		CheckMemory( VIM_FALSE );

        // RDD 090620 Trello #112 5000 /day of these errors locking up the terminal - GR say goto CTLS instead of fall back. Ensure cancelled after each txn
        if (txn.card.type == card_picc)
        {
           // vim_picc_emv_cancel_transaction(picc_handle.instance);
        }

		terminal.screen_id = 0;		// RDD 140312 reset the screen ID to show the Divisional Default immediatly after the txn

		// RDD 160412 SPIRA:5111 Sometimes POS display not cleared on timeout when card read errors occur
        // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
//#ifndef _XPRESS
		//if(( txn.error != VIM_ERROR_NONE )&&( txn.type != tt_query_card_openpay ))
		//{
		//	pceft_pos_display_close();
		//}
//#endif
        // RDD TEST TEST TEST TEST
        //display_idle();
        // RDD 230621 - v727 this flag blocks access to function menu as txn not reset until SAFs have been sent etc.
        transaction_clear_status(&txn, st_cpat_check_completed);
        return VIM_ERROR_NONE;

      default:
        state = state_get_next();
        break;
    }
  }
}

VIM_ERROR_PTR txn_process( void )
{
  txn_state_t state;
  VIM_ERROR_PTR ret;

  if (VIM_ERROR_NONE != (ret = is_ready()))
  {
    VIM_MAIN_APPLICATION_TYPE application_type;

    txn.error = ret;

    vim_main_callback_get_application_type(&application_type);

    return txn.error;
  }

  state = ts_no_state;

  do
  {
	  terminal_set_status(ss_locked);

    ret = txn_process_states( state );

    //VIM_DBG_ERROR(ret);

    if( ERR_EMV_TRAN == ret )
    {
      if( txn_emv_transaction() == ts_emv_fallback_to_magstripe )
	  {
        state = ts_card_capture;
      }
    }
  }while( ret == ERR_EMV_TRAN );

  terminal_clear_status(ss_locked);
  return ret;
}


VIM_ERROR_PTR emv_trans_steps_after_query(void)
{
#if 0 /* TODO: */
  txn_check_logon();
  if( txn.error != VIM_ERROR_NONE )
    VIM_ERROR_RETURN( VIM_ERROR_EMV_TXN_ABORTED );

  if( txn.error != VIM_ERROR_NONE )
    VIM_ERROR_RETURN( VIM_ERROR_EMV_TXN_ABORTED );

  txn_check_permissions();
  if( txn.error != VIM_ERROR_NONE )
    VIM_ERROR_RETURN( VIM_ERROR_EMV_TXN_ABORTED );
#endif
  return VIM_ERROR_NONE;
}


VIM_CHAR txn_get_current_pceftpos_device_code()
{
  VIM_CHAR device_code = 0x00;

  if( txn.pceftpos_device_code )
    return txn.pceftpos_device_code;

  vim_pceftpos_get_device_code(pceftpos_handle.instance, &device_code);
  return device_code;
}



