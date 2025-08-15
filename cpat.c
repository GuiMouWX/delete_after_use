
#include "incs.h"
VIM_DBG_SET_FILE("T5");

extern const card_bin_name_item_t CardNameTable[];
extern const card_bin_list_t BlockedBinTable[];

VIM_SIZE CardIndex( const transaction_t *transaction )
{
	VIM_UINT32 CpatIndex = transaction->card_name_index;
	return terminal.card_name[CpatIndex].CardNameIndex;
}

VIM_SIZE PcEFTBin(const transaction_t* transaction)
{
	VIM_UINT32 CpatIndex = txn.card_name_index;
	VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
	return CardNameTable[CardNameIndex].bin;
}




/* ------------------------------------------------------------------------ */
/*  Finds the account or how to select for a swiped card */
/* ------------------------------------------------------------------------ */
VIM_BOOL set_acc_from_agc( account_t *account )
{
	VIM_UINT32 CpatIndex = txn.card_name_index;
	VIM_BOOL account_known = VIM_FALSE;

    DBG_MESSAGE("Set ACC from AGC");
	// WOW special - GIFT CARDS are always defined as SAVINGS only a/c - no selection needed
	if( gift_card(&txn) )
	{
		*account = account_savings;
		return VIM_TRUE;
	}
	//DBG_VAR(CpatIndex);
	DBG_POINT;
	switch( terminal.card_name[CpatIndex].AccGroupingCode )
	{
		case '0':
		case '2':
		*account = account_credit;
		account_known = VIM_TRUE;
		DBG_POINT;
		break;

        case '1': *account = account_unknown_debit;
		DBG_POINT;
		break;

		default:
			DBG_POINT;
			*account = account_unknown_any;
			// delete credit a/c from choise if no cash allowed on cr4edit for this card type
		if(( txn.amount_cash ) && ( !CARD_NAME_ALLOW_CASH_ON_CR_ACC ))
		{
			DBG_POINT;
			*account = account_unknown_debit;
		}
		break;
    }
	DBG_VAR( account_known );
	return account_known;
}



VIM_BOOL ProcessManualMotorCharge( VIM_CHAR *Pan )
{
	VIM_UINT8 n;
	VIM_CHAR TempPan[WOW_PAN_LEN_MAX+1];
	VIM_CHAR *Ptr = Pan;
	vim_mem_clear( TempPan, WOW_PAN_LEN_MAX+1 );

	// First check the 13 entered digits to ensure that they're correct
	if( !check_luhn( Pan, vim_strlen(Pan) ))
	{
		return VIM_FALSE;
	}

	// If correct then copy overe the first 5 digits then insert two zeros then copy over the next 8 digits to form a 15 digit PAN
	for( n=0; n<WOW_MANUAL_MOTORCHARGE_PAN_LEN+2; n++ )
	{
		if( n==5 || n==6 )
			TempPan[n] = '0';
		else
			TempPan[n] = *Ptr++;
	}
	TempPan[n++] = calc_pan_luhn( TempPan, WOW_MANUAL_MOTORCHARGE_PAN_LEN+2 );
	TempPan[n] = '\0';
	vim_strcpy( Pan, TempPan );
	return VIM_TRUE;
}


/* ------------------------------------------------------------------------ */
VIM_UINT16 find_card_range(card_info_t* card_details, VIM_UINT8 *pan, VIM_SIZE pan_length)
{
  VIM_UINT32 card_name_index;
  VIM_SIZE binary_pan;
  VIM_UINT16 best_card_name_index = MAX_CARD_NAME_TABLE_SIZE+1;

  // RDD 070612: Horrid hack to get Motorcharge manual entry fixed
  if( ( card_details->type == card_manual ) && ( pan_length == WOW_MANUAL_MOTORCHARGE_PAN_LEN ) && ( !vim_strncmp( (VIM_CHAR *)pan, MOTORCHARGE_BIN, vim_strlen(MOTORCHARGE_BIN ) )))
  {
	  if( !ProcessManualMotorCharge( (VIM_CHAR *)pan ))
	  {
		  return best_card_name_index;
	  }
	  // RDD 100712 SPIRA:
	  else
	  {
		  VIM_UINT8 pan_len = vim_strlen((VIM_CHAR *)pan);
		  vim_mem_copy( card_details->data.manual.pan, pan, pan_len );
		  card_details->data.manual.pan_length = pan_len;
	  }
  }
  binary_pan = asc_to_bin( (const char*)pan, WOW_CPAT_CARD_PREFIX_LEN );

  for (card_name_index = 0; card_name_index < terminal.number_of_cpat_entries; card_name_index++)
  {
      /*  Do we have a match of PAN against table */
	  // RDD 160412 SPIRA:5315 last < -> <= to fix 9th digit check on CPAT
      if(( binary_pan >= terminal.card_name[card_name_index].PAN_range_low ) && ( binary_pan <= terminal.card_name[card_name_index].PAN_range_high ))
      {
        best_card_name_index = card_name_index;
        break;
      }
  }
  return best_card_name_index;
}


// RDD 040123 JIRA PIRPIN-1607 Global CPAT Use CARDS xml to block card types
static VIM_BOOL SupportedIndex(VIM_UINT32 card_name_index)
{
	VIM_UINT32 CardIndex = terminal.card_name[card_name_index].CardNameIndex;
	return vim_strlen(CardNameTable[CardIndex].card_name) ? VIM_TRUE : VIM_FALSE;
}


// RDD 040123 JIRA PIRPIN-1607 Global CPAT migrates blocked BIN checking to terminal via Branded CARDS.XML file
static VIM_BOOL BlockedBin(VIM_UINT8* pan)
{
	VIM_SIZE n;
	card_bin_list_t* BlockedBinTablePtr = (card_bin_list_t * )BlockedBinTable;

	for (n = 0; n < CARD_INDEX_MAX; n++, BlockedBinTablePtr++ )
	{
		if (vim_mem_compare(pan, BlockedBinTablePtr->card_bin, 9) == VIM_ERROR_NONE )
		{
			pceft_debugf(pceftpos_handle.instance, "BIN Blocked In %s", TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "CARDS.XML"));
			return VIM_TRUE;
		}
	}
	return VIM_FALSE;
}

// RDD - ESC, LUHN etc. checked later in txn_card_check() fn.
VIM_ERROR_PTR initial_cpat_checks( transaction_t* transaction, VIM_UINT32 card_name_index, VIM_UINT8* pan, VIM_UINT8 pan_len )
{

#ifndef _WIN32
	if( WOW_CPAT_REJECT_CARD )
	{
		vim_strcpy( transaction->host_rc_asc, "TY" );
        // RDD 300920 Trello #131 - if docked txn was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
        transaction->error = ERR_WOW_ERC;
        DBG_POINT;
        return transaction->error;
	}
#endif

    // RDD 270821 Card Was routed MCR but sadly blacklisted
    if(( CARD_NO_MCR ) && ( transaction_get_status( &txn, st_mcr_active )))
    {
       // return ERR_MCR_BLACKLISTED_BIN;
    }


	// RDD 040123 JIRA PIRPIN-1607 Global CPAT Use CARDS xml to block card types
	if (!SupportedIndex(card_name_index))
	{
		vim_strcpy(transaction->host_rc_asc, "TY");
		// RDD 300920 Trello #131 - if docked txn was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
		transaction->error = ERR_WOW_ERC;
		DBG_POINT;
		return transaction->error;
	}
	if (BlockedBin(pan))
	{
		vim_strcpy(transaction->host_rc_asc, "TY");
		// RDD 300920 Trello #131 - if docked txn was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
		transaction->error = ERR_WOW_ERC;
		DBG_POINT;
		return transaction->error;
	}


    // 091118 RDD JIRA WWBP-363 Don't allow anything except Gift Cards if "F" transaction.
    // 010620 RDD S type and Type 4 Gift Card refunds need to be disallowed for non-gift cards
    if((transaction->type == tt_gift_card_activation) || transaction_get_status( transaction, st_electronic_gift_card ) || transaction_get_status( transaction, st_giftcard_s_type ))
    {
        if (!gift_card(transaction))
        {
            return ERR_NO_CPAT_MATCH;
        }
    }

	if((( transaction->type == tt_deposit ) || ( transaction->type == tt_query_card_deposit )) && ( !CARD_NAME_ALLOW_DEPOSIT ))
	{
		vim_strcpy( transaction->host_rc_asc, "TW" );
        // RDD 300920 Trello #131 - if docked transaction was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
        transaction->error = ERR_WOW_ERC;
        DBG_POINT;
        return transaction->error;
	}

	// RDD - no cashout on credit unless otherwise specified
    // RDD 030820 non-EFTPOS on Scheme debit now treated as default account
	if( ((transaction->account == account_credit)||(transaction->account == account_default )) && (transaction->amount_cash) && (!CARD_NAME_ALLOW_CASH_ON_CR_ACC) )
	{
		//vim_strcpy( txn.host_rc_asc, "TE" );
		DBG_POINT;
		return ERR_NO_CASHOUT;
	}

	if(( transaction->type == tt_balance ) && ( !CARD_NAME_ALLOW_BALANCE ))
	{
		vim_strcpy( transaction->host_rc_asc, "FE" );
        // RDD 300920 Trello #131 - if docked txn was aborted on purpose ( eg. because of CPAT failure, then do not ovewrite existing error code )
        transaction->error = ERR_WOW_ERC;
		DBG_POINT;
		return transaction->error;
	}

	// RDD 090512 SPIRA:5463 This needs to be set once the card has passed the initial CPAT checks
	transaction_set_status( transaction, st_cpat_check_completed );
	DBG_POINT;
	return VIM_ERROR_NONE;
}

// 160821 JIRA PIRPIN-1191
static VIM_ERROR_PTR TMS_SchemeCheck(VIM_UINT32 CardIndex)
{
    if ((CardIndex == DINERS) && (TERM_DINERS_DISABLE))
        return ERR_CARD_NOT_ACCEPTED;
    else if ((CardIndex == AMEX) && (TERM_AMEX_DISABLE))
        return ERR_CARD_NOT_ACCEPTED;
    else if ((CardIndex == JCB) && (TERM_JCB_DISABLE))
        return ERR_CARD_NOT_ACCEPTED;
    else if ((CardIndex == UPI_CARD_INDEX) && (TERM_UPI_DISABLE))
        return ERR_CARD_NOT_ACCEPTED;
	else if ((CardIndex == QC_GIFT_AUS || CardIndex == QC_GIFT_NZ) && (TERM_QC_GIFT_DISABLE) && (IS_R10_POS == VIM_FALSE))
	{
		pceft_debug(pceftpos_handle.instance, "QC Card Rejected by TMS or Invalid Index");
		return ERR_CARD_NOT_ACCEPTED;
	}
	else if ((CardIndex == WEX_GIFT_AUS || CardIndex == WEX_GIFT_NZ) && (TERM_WEX_GIFT_DISABLE) && (IS_R10_POS == VIM_FALSE))
	{
		pceft_debug(pceftpos_handle.instance, "WEX Card Rejected by TMS or Invalid Index");
		return ERR_CARD_NOT_ACCEPTED;
	}
    else
        return VIM_ERROR_NONE;
    
}


VIM_ERROR_PTR OtherCpatChecks()
{
    VIM_UINT32 CpatIndex = txn.card_name_index;
    VIM_UINT32 CardIndex = terminal.card_name[CpatIndex].CardNameIndex;
    VIM_ERROR_PTR res;

	DBG_MESSAGE( "<<<< DO CPAT CHECK ... >>>>" );

    if(( res = TMS_SchemeCheck(CardIndex)) != VIM_ERROR_NONE )
    {
        txn.error = res;
        txn.error_backup = res;
        picc_close(VIM_TRUE);
        return txn.error;
    }


	// RDD 121212 Added for SPIRA:6437 v305 CR
    // RDD 120821 JIRA PIRPIN-698 Removed for re-purpose CPAT FLAG to denote MCR available
#if 0
	if(( CARD_NO_EMV ) && ( txn.card.type == card_chip ))
	{
		DBG_MESSAGE( " !!!! DOCKING NOT ALLOWED FOR THIS CARD !!!!" );
		txn.error = ERR_CPAT_NO_DOCK;
		return txn.error;
	}
#endif
    // RDD 030620 Trello #156
    if ( IsBGC() &&  !gift_card(&txn))
        return ERR_LUHN_CHECK_FAILED;

    // RDD 310521 JIRA PIRPIN-1079 - don't allow QC cards to be tendered via EFT.'
    if ((CardIndex == QC_GIFT_AUS || CardIndex == QC_GIFT_NZ) && (txn.card_state != card_capture_new_gift))
    {
        // RDD - allow for ALH etc. which don't support Avalon
        if( TERM_NO_EFT_GIFT )
        {
            pceft_debug(pceftpos_handle.instance, " JIRA PIRPIN-1079");
            txn.error = ERR_INVALID_TENDER;
            return txn.error;
        }
    }

	// RDD 100515 SPIRA:8565  -standalone checkouts bomb out earlier so need the finalisation stage. Also check for expired cards
	if(( is_standalone_mode() ) && ( txn.type == tt_checkout ))
	{
		// RDD added after conversation with MB 100415. Need basic expired card check on checkout txns
		// If the checkout is of a valid Preauth which got approved despite the card being expired then allow it.
		if(( card_check_expired( &txn.card )) && ( !txn.roc ))
		{
			  txn.error = ERR_CARD_EXPIRED;
			  // RDD 290415 - also need to ensure that these offline transactions are not stored in the SAF
			  transaction_clear_status( &txn, st_needs_sending );
		}
	}


	if(( WOW_CPAT_SWIPE_ONLY || WOW_CPAT_SWIPE_ONLINE ) && ( txn.card.type == card_manual ))
	{
        if (IsBGC() == VIM_FALSE)
        {
            txn.error = ERR_MANUAL_PAN_DISABLED;
            return txn.error;
        }
	}

		DBG_POINT;
	if( GetEPALAccount() != VIM_ERROR_NONE )
	{
		DBG_POINT;
		// RDD - for deposit (no a/c selection) see if Credit A/C can be set from AGC if not dictated by POS
		if(( !txn.pos_account ) && ( txn.card.type != card_manual ))
		{
			set_acc_from_agc( &txn.account );
		}
	}
    // RDD 290621 JIRA PIRPIN-1081 : Don't allow SAV or CHQ if AGC says Default acc
    else
    {
        account_t acc;
        set_acc_from_agc( &acc );
        VIM_DBG_VAR(acc);
        if( acc == account_credit )
        {
            // Its an EPAL account but CPAT says credit only - throw a warning and go back to present card.
            pceft_debug( pceftpos_handle.instance, "JIRA PIRPIN-1081" );
            txn.error = ERR_INVALID_ACCOUNT;
            return txn.error;
        }
    }

	// RDD - added for gift card processing

	// RDD 040714 - ALH standalone reject invalid gift cards for activate
	if(( txn.type == tt_gift_card_activation ) && ( !gift_card(&txn) ))
	{
		txn.error = ERR_CARD_NOT_ACCEPTED;
		return txn.error;
	}

	// BRD No cashout on default CR a/c
    // RDD 030820 non-EFTPOS on Scheme debit now treated as default account
	if( ( gift_card(&txn) || (((txn.account == account_credit) || (txn.account == account_default)) && ( !CARD_NAME_ALLOW_CASH_ON_CR_ACC ) ) ) && txn.amount_cash )
	{
      txn.error = ERR_NO_CASHOUT;
	  return ERR_NO_CASHOUT;
	}

	// RDD 010812 SPIRA:5811 Need error bleep here
	if ((txn.type == tt_refund) && (!CARD_RANGE_ENABLE_REFUND))
	{
		return ERR_REFUNDS_DISABLED;
	}

	/* Check if mod-10 check is required */
	if( CARD_NAME_LUHN_CHECKING )
	{
		char pan[21], len;
		VIM_ERROR_RETURN_ON_FAIL( card_get_pan(&txn.card, pan) );
		len = vim_strlen(pan);
		if( len )
		{
			if ( !check_luhn(pan, len) )
			{
				vim_kbd_sound_invalid_key();
				return ERR_LUHN_CHECK_FAILED;
			}
		}
	}

#if 0

    // RDD 040418 Merchant Routing
    if ((terminal.configuration.use_mer_routing) && (txn.card.type == card_picc))
    {
        // If We routed the txn then ensure that this is allowed - otherwise fallback!
        if ((txn.mcr_tag87 == 0x02) && (!CARD_NAME_MCR_ENABLE))
        {
            txn.mcr_tag87 = 0x01;
            DBG_MESSAGE("Card is on MCR CPAT Blacklist - Fall back !! ");
            return VIM_ERROR_PICC_FALLBACK;
        }

    }
#endif

	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR CheckT3LoyaltyCard( card_info_t *CardData, VIM_CHAR *LoyaltyNumber )
{
	VIM_UINT8 n;
	VIM_CHAR *Ptr = CardData->data.mag.track1;

	// The loyalty number is the last data element. Look for the end sentinal and then count back
	// RDD 111119 P400 has a 3 track reader
	VIM_DBG_NUMBER(CardData->data.mag.track1_length);
	Ptr = &CardData->data.mag.track1[CardData->data.mag.track1_length];
	DBG_DATA( CardData->data.mag.track1, CardData->data.mag.track1_length );
	// Count Back from the end until a seperator is reached
	for( n=0; n<=ONE_CARD_T3_ISSUER_DATA_MAX_LEN+1; n++, --Ptr )
	{
		if( *Ptr == '=' )
		{
			VIM_SIZE LoyaltyLen = vim_strlen( Ptr ) - 1;

            // RDD 150720 Trello #166 - '=' end sentinel causes zero loyalty length
            if (LoyaltyLen < ONE_CARD_LEGACY_LEN)
            {
                continue;
            }

			VIM_DBG_WARNING("---- Found First FS ---");
			DBG_VAR( LoyaltyLen );

			// The T3 Loyalty Len can be
			if( LoyaltyLen >= ONE_CARD_LEGACY_LEN )
			{
				LoyaltyLen = VIM_MIN( ONE_CARD_MAX_LEN, LoyaltyLen );
				vim_strncpy( LoyaltyNumber, ++Ptr, LoyaltyLen );

				DBG_MESSAGE( LoyaltyNumber );

				// Some Chinese cards have 15 digits after last separator but all zeros...
				if( asc_to_bin( LoyaltyNumber, LoyaltyLen ) == 0 )
					return ERR_LOYALTY_MISMATCH;

				vim_strcat( LoyaltyNumber, LOYALTY_NUMBER_DUMMY_TRACK_DATA );
				DBG_MESSAGE( "<<<<<<< Track3 Loyalty Number OK: >>>>>>>>" );
				return VIM_ERROR_NONE;
			}
			break;
		}
	}
	return ERR_LOYALTY_MISMATCH;
}



VIM_ERROR_PTR SaveLoyaltyNumber( VIM_CHAR *LoyaltyNumber, VIM_UINT8 len, VIM_ICC_SESSION_TYPE SessionType )
{
	// RDD v574-1 ensure that loyalty number termined by '='
	VIM_CHAR *Ptr = LoyaltyNumber;
	if (Ptr[len -1] != '=')
	{
		VIM_CHAR  buf[VAS_MAX_MESSAGE_SIZE + 1] = {0};
		Ptr = buf;
		vim_strncpy(Ptr, LoyaltyNumber, len);
		Ptr[len] = '=';
		Ptr[++len] = ',';
	}
	return( vim_file_set( LOYALTY_FILE, Ptr, len ) );
}

// Function to extract the Barcode data from the MacQuarie Card according to data in SPIRA:8030
// Sample Track1 :%b4363846589626307^vpqpc4/souls.             ^1702201934444147944700360000000
VIM_ERROR_PTR CheckMacQuarieCardBarcode( card_info_t *CardData, VIM_CHAR *LoyaltyNumber )
{
	VIM_CHAR *EndPtr, *StartPtr = CardData->data.mag.track1;

	VIM_ERROR_RETURN_ON_FAIL( vim_strchr( StartPtr, '^', &EndPtr ));

	StartPtr = EndPtr;
	StartPtr++;

	VIM_ERROR_RETURN_ON_FAIL( vim_strchr( StartPtr, '^', &EndPtr ));
	EndPtr += 8;
	vim_mem_copy( LoyaltyNumber, EndPtr, 13 );
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR CheckLoyaltyCard( card_info_t *CardData, VIM_UINT32 CardNameIndex )
{
	// The Old OneCard has 12 digits and starts with a stack of different 2 digit codes
#ifdef _DEBUG  // RDD add "11" so that we can test Track3 reading with My HangSeng Bank card : "Loyalty Number" : 118094001
	const VIM_CHAR PrefixTable[][2+1] = { "11","15","16","17","18","19","22","23","24","50","51","52","53","54","55","70","71","72","73","74","57","59","95" };
#else
	const VIM_CHAR PrefixTable[][2+1] = { "15","16","17","18","19","22","23","24","50","51","52","53","54","55","70","71","72","73","74","57","59","95" };
#endif
	VIM_CHAR LoyaltyNumber[100];
	VIM_SIZE n, elements = VIM_LENGTH_OF(PrefixTable);
	VIM_ERROR_PTR res = ERR_LOYALTY_MISMATCH;

	vim_mem_clear( LoyaltyNumber, VIM_SIZEOF(LoyaltyNumber) );

	DBG_MESSAGE( " !!!!!!! Check Loyalty Card !!!!!!!!! ");

	switch( CardData->type )
	{
		case card_manual:
			vim_mem_copy( LoyaltyNumber, CardData->data.manual.pan, CardData->data.manual.pan_length );
			vim_strcat( LoyaltyNumber, LOYALTY_NUMBER_DUMMY_TRACK_DATA );
			res = SaveLoyaltyNumber( LoyaltyNumber, vim_strlen(LoyaltyNumber), VIM_ICC_SESSION_UNKNOWN );
			break;

		case card_mag:
			// did the track3 flag get set when the card was read ? If so the data will be in track1

			if(( CardData->data.mag.track3_present ) && ( WOW_NZ_PINPAD ))
			{
				if(( res = CheckT3LoyaltyCard( CardData, LoyaltyNumber )) == VIM_ERROR_NONE )
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(DISP_IMAGES, "Label", NZD_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL));
					res = SaveLoyaltyNumber( LoyaltyNumber, vim_strlen( LoyaltyNumber ), VIM_ICC_SESSION_UNKNOWN);
                    VIM_DBG_ERROR(res);
				}
				else
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(DISP_IMAGES, "Label", NZD_ERROR_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL));
				}
			}

			// RDD 070814 SPIRA:8030 added for Macquarie card loyalty support
			else if( CardNameIndex == MACQUARIE_CARD )
			{
				VIM_ERROR_RETURN_ON_FAIL( CheckMacQuarieCardBarcode( CardData, LoyaltyNumber ));
				vim_strcat( LoyaltyNumber, LOYALTY_NUMBER_DUMMY_TRACK_DATA );
				res = SaveLoyaltyNumber( LoyaltyNumber,  vim_strlen(LoyaltyNumber), VIM_ICC_SESSION_UNKNOWN);
			}
			// Get the PAN from Track2 - this will only work on 12 digit legacy cards
			else
			{
				VIM_ERROR_RETURN_ON_FAIL( card_get_pan( CardData, LoyaltyNumber ));

				// If reading track2 it must have length of 12
				if( vim_strlen(LoyaltyNumber) == ONE_CARD_LEGACY_LEN )
				{
					VIM_CHAR expiry[4+1];
					VIM_ERROR_RETURN_ON_FAIL( card_get_expiry( CardData, expiry ));

					// Legacy One Card must have length 12 and 0000 expiry date
					if( !vim_strncmp( expiry, "0000", 4 ) )
					{
						vim_strncpy( LoyaltyNumber, CardData->data.mag.track2, CardData->data.mag.track2_length );
						res = VIM_ERROR_NONE;
					}
					res = ERR_LOYALTY_MISMATCH;
					for( n=0; n<elements; n++ )
					{
						// Check for a match in the array of valid prefixes above. If no match return error
                        if (!vim_strncmp(PrefixTable[n], LoyaltyNumber, 2))
                        {
                            VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(DISP_IMAGES, "Label", NZD_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL));
                            return( SaveLoyaltyNumber( LoyaltyNumber, vim_strlen(LoyaltyNumber), VIM_ICC_SESSION_UNKNOWN ));
                        }
					}
				}
			}
			break;

		case card_picc:
		case card_chip:
			// For tapped or inserted card assume that all Rimu cards are OK
			DBG_MESSAGE( " !!!!!!! Inserted or Swiped Query Card !!!!!!!!! ");
			if(( CardNameIndex == RIMU_CARD )||( CardNameIndex == MACQUARIE_CARD ))
			{
	  	        DBG_MESSAGE( " !!!!!!! Card With Discount/Loyalty ID Detected !!!!!!!!! ");
				res = VIM_ERROR_NONE;
			}
			break;

		default:
			res = VIM_ERROR_SYSTEM;
			break;
	}
	return res;
}





/* ------------------------------------------------------------------------
  Searchs the card name table for a match against the PAN obtained.
  Also do basic validity checks on the card to match with transaction
 ------------------------------------------------------------------------ */
VIM_ERROR_PTR card_name_search(transaction_t *transaction, card_info_t *card)
{
  VIM_UINT8   pan_length;
  VIM_UINT8   pan[21];

  // RDD 231211 added - make sure that the CPAT table is not searched muliple times in a single txn
  // RDD 251121 v729-1 JIRA PIRPIN-1320 this creates a bug with MAN PAN entry so remove
  if( transaction_get_status( transaction, st_cpat_check_completed ))
  {
	  // We already did this search !
	  //return VIM_ERROR_NONE;
  }

  // DBG_VAR(transaction->card.type);

  VIM_ERROR_RETURN_ON_FAIL(card_get_pan(card, (char *)pan));
  pan_length = MIN(vim_strlen((char *)pan), 19);

  // It's a PCI violation to put PAN data in the log.
  //VIM_DBG_DATA(pan, pan_length);


  // RDD 120912 P3: For RIMU EMV cards we need to check CardNameIndex to determine loyalty cards so, process the track data...
  if( pan_length == 0 )
  {
	  VIM_ERROR_RETURN(ERR_INVALID_TRACK2);
  }

  /*  Initialise the last Pan Values */
  transaction->card_name_index = 0;
  transaction->issuer_index  = 0xFF;
  transaction->acquirer_index  = 0;

  if (IsEverydayPay(transaction))
  {
	  if (0 < vim_strlen(transaction->edp_payment_session_id))
	  {
		  VIM_DBG_MESSAGE("Non-card transaction; Skipping card lookup");
	  }
	  else
	  {
		  //Everyday pay subtransaction lookup
		  transaction->card_name_index = find_card_range(&transaction->card, pan, pan_length);
		  //Validation has already occurred on the host, we're just populating card info - skip further checks
	  }

	  return VIM_ERROR_NONE;
  }

  // This line sets the card_name_index in the transaction structure
  if( MAX_CARD_NAME_TABLE_SIZE+1 != (transaction->card_name_index = find_card_range(&transaction->card, pan, pan_length)))
  {
	// Set the status to indicate that we've searched the CPAT and found a match
    // RDD 090512 SPIRA:5463 This needs to be set once the card has passed the initial CPAT checks NOT here
	//transaction_set_status( transaction, st_cpat_check_completed );

	VIM_DBG_NUMBER( transaction->card_name_index );
	//DBG_MESSAGE( CardNameTable[terminal.card_name[transaction->card_name_index].CardNameIndex.card_name );


#if 1 // RDD 070814 Need to Get the CPAT index BEFORE we do this...
	  // RDD 030912 P3: Pass Card data as for future expansion we'll need to include Chip as well as mag data
	if(( transaction->type == tt_query_card )&&( transaction->card.type == card_mag )&&( transaction->card_state != card_capture_new_gift))
	{
		return( CheckLoyaltyCard( &transaction->card, CardIndex(transaction)));
	}
#endif

	// RDD WOW - check luhn, transaction allowed flags etc.
    VIM_ERROR_RETURN_ON_FAIL( initial_cpat_checks( transaction, transaction->card_name_index, pan, pan_length ));

	if(( transaction->type == tt_query_card )&&( transaction->card.type != card_mag ) && (transaction->card_state != card_capture_new_gift))
    {
		return( CheckLoyaltyCard( &transaction->card, CardIndex(transaction) ) );
    }
  }
  else
  {
	  vim_strcpy( transaction->host_rc_asc, "TY" );
	  // DBG_MESSAGE("<<<< ERROR. UNABLE TO FIND CPAT INDEX IN CPAT TABLE >>>>" );
	  return ERR_WOW_ERC;
  }
  return VIM_ERROR_NONE;
}

extern VIM_ERROR_PTR GetPanLen(VIM_CHAR *pan, VIM_SIZE *pan_len);


VIM_BOOL IsPCICardCallback(VIM_CHAR *T2Data, VIM_SIZE *PanLen )
{
    card_info_t CardData;
    VIM_UINT32 CardIndex;
    VIM_UINT8 Pan[15] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    vim_mem_copy(Pan, T2Data, 14);
    GetPanLen(T2Data, PanLen);
    // This line sets the card_name_index in the transaction structure
    if( MAX_CARD_NAME_TABLE_SIZE + 1 == ( CardIndex = find_card_range( &CardData, Pan, 14 )))
        return VIM_FALSE;
    if (terminal.card_name[CardIndex].CardNameIndex == UPI_CARD_INDEX)
        return VIM_FALSE;
    return VIM_TRUE;
}





