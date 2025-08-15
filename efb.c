
#include <incs.h>
VIM_DBG_SET_FILE("T8");

extern VIM_ERROR_PTR EmvCardRemoved(VIM_BOOL GenerateError);

static const char *picc_cdo_file_name = VIM_FILE_PATH_LOCAL "CDOPAT.XML";

VIM_BOOL CDOFilePresent( void )
{
	VIM_BOOL exists = VIM_FALSE;
	vim_file_exists( picc_cdo_file_name, &exists );
	return exists;
}

#define PICC_TAG				"PICC_TAG>"


/**
 * Start the efb timer. To be called when no response or a network error occurs on a transaction
*/
 
void efb_start_timer(void)
{


  //vim_rtc_get_time(&terminal.efb_timer);
  //terminal.efb_timer_pending = VIM_TRUE;
  // DBG_MESSAGE("START EFB TIMER");
  //DBG_TIMESTAMP;
  // RDD 170412 removed to save on file writes
  //terminal_save();
}

/**
 * Clear the efb timer.
 */
void efb_clear_timer(void)
{
  // DBG_MESSAGE("STOP EFB TIMER");
  //DBG_TIMESTAMP;
  //terminal.efb_timer_pending = VIM_FALSE;
  		  
  // RDD 170412 removed to save on file writes
  //terminal_save();
}



/*----------------------------------------------------------------------------*/
/**
 * Check if we should process the transaction in EFB mode without attempting to go online
 */
VIM_BOOL efb_dont_attempt_online(void)
{
    // RDD 200814 If Sucessful checkout of preauth then store offline as 220  
	if( txn.type == tt_checkout ) 
	{
		if(( ValidateAuthCode( txn.auth_no ) || ( transaction_get_status( &txn, st_pos_sent_pci_pan ))))
			return VIM_TRUE;
	}

	// RDD 300616 SPIRA:xxxx UPI Refunds saved into SAF
	if( txn.type == tt_refund && IsUPI( &txn ))
	{
		return VIM_TRUE;
	}

  // RDD 300112 : Completion/Voucher always goes to EFB but we fool the card into beleiving we attempted to go online
		
  // RDD 170113 SPIRA:6490 Problem with proc code for NZ 'S' type Deposits - looks like card.type was not being set properly for "S"
  //if( transaction_get_status( &txn, st_completion )) 
  if( transaction_get_status( &txn, st_completion ) && ( txn.original_type != tt_none ) ) 
  {
	  // DBG_MESSAGE( " !!!! COMPLETION: NO ONLINE - GO DIRECTLY TO EFB !!! " );
	  txn.error = ERR_UNABLE_TO_GO_ONLINE_APPROVED;		// RDD 300112 - just need to fudge an error so we can get into EFB
	  // RDD SPIRA:6490 - add debug
	  pceft_debug(pceftpos_handle.instance, "Txn Status Completion: Approve Offline" );

      return VIM_TRUE;
  }
  else
  {
	return VIM_FALSE;
  }
}


/** convert offline card prefix from bcd into ascii */
void convert_pan_prefix(VIM_UINT8 *bcd_ptr, VIM_TEXT *asc_ptr, VIM_UINT8 asc_length)
{
  /*  number of bytes */
  VIM_UINT8 num = (asc_length + 1) >> 1;

  while (num-- > 0)
  {
    if (0xF0 == (*bcd_ptr & 0xF0))
      break;
    else
      *asc_ptr++ = *bcd_ptr >> 4 | '0';
    
    /*  if converting an odd number of digits and this is the last bcd byte */
    /*  dont add another character as we are finished */
    if (num || !(asc_length%2))
    {
      if (0x0F == (*bcd_ptr & 0x0F))
        break;
      else
        *asc_ptr++ = (*bcd_ptr & 0x0F) | '0';
    }

    bcd_ptr++;
  }
}


VIM_BOOL is_valid_auth_id(VIM_CHAR* string, VIM_SIZE len)
{
  VIM_SIZE i;
  
  if ( len > 6 || len < 1 )
    return VIM_FALSE;

  for (i = 0 ; i < len; i++)
  {
    if ( (string[i] >= '0') && (string[i] <= '9') )
      continue;
    else if ( (string[i] >= 'a') && (string[i] <= 'z')  )
      continue;
    else if ( (string[i] >= 'A') && (string[i] <= 'Z')  )
      continue;
    else
      return VIM_FALSE;
  }

  return VIM_TRUE;
}


/**
 * Check EMV decline rules
 * VIM_TRUE - transaction is not allowed to be approved in EFB
 * VIM_FALSE - transaction can be approved in EFB
 */
VIM_BOOL emv_efb_decline(void)
{
  // RDD 240112 - need to use the TAC STANDIN for this AID rather than the Hardcoded value WBC uses..
  //const VIM_UINT8 tvr_mask[5] = {0xFC, 0x70, 0xF0, 0x08, 0x40};
  VIM_TLV_LIST tag_list;
  VIM_BOOL retval = VIM_FALSE;
  	  
  DBG_POINT;
  DBG_VAR( txn.account );
  DBG_VAR( txn.amount_cash );

  // RDD 241012 SPIRA:6229 Cash on Credit for Scheme debit Cards now not allowed unless full EMV. Full EMV must go online  
  // RDD 030820 Trello #69 and #199 - No Credit for Scheme Debit - use default account instead
  //if(( txn.amount_cash ) && ( txn.account == account_credit ))
  if(( txn.amount_cash ) && ((txn.account == account_credit) || (txn.account == account_default)))
  {
	  DBG_MESSAGE( "$$$$$$ CASH ON CREDIT FALLBACK NOT ALLOWED $$$$" );
      vim_strcpy(txn.cnp_error, "EFC");
	  return VIM_TRUE;
  }

  if (VIM_ERROR_NONE == vim_tlv_open(&tag_list, txn.emv_data.buffer,
      txn.emv_data.data_size, sizeof(txn.emv_data.buffer),VIM_FALSE))
  {
    if (VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_TVR))
    {
      VIM_UINT8 i;		  
	  DBG_MESSAGE( "<<<<<<<<<< EFB EMC CHECK SAC AGAINST TVR >>>>>>>>>>>>>");
      for (i = 0; (i < 5); i++)
      {
		  DBG_DATA( tag_list.current_item.value, 5 );
		  DBG_DATA( terminal.emv_applicationIDs[txn.aid_index].tac_standin, 5 );
		  DBG_VAR( tag_list.current_item.value[i] );
		  DBG_VAR( terminal.emv_applicationIDs[txn.aid_index].tac_standin[i]);
          if (tag_list.current_item.value[i] & terminal.emv_applicationIDs[txn.aid_index].tac_standin[i])
          {
              // RDD 280920 : There is a bit set in both so break here and decline
              VIM_DBG_PRINTF1("EFB Decline due to TVR Match in Byte %d", i);
              retval = VIM_TRUE;
              vim_strcpy(txn.cnp_error, "EFE");
              break;
          }
      }
    }
	else
	{
	  DBG_MESSAGE( "<<<<<<<<<< EFB EMC NO TVR FOUND >>>>>>>>>>>>>");
	}
  }
  DBG_VAR(retval);
  return retval;
}


VIM_BOOL EfbOverFloorLimit( void )
{
	VIM_BOOL OverFloorLimit= VIM_FALSE;

	// RDD 020512 SPIRA:5415 Offline Refunds and Deposits are not using the CPAT offline limit. 
	// RDD 260312 - No floor limit for refunds or deposits
	//if(( txn.type == tt_refund ) || ( txn.type == tt_deposit )) return OverFloorLimit;

	// RDD 300112 - No floor limit check for completions
	if( transaction_get_status( &txn, st_completion )) 
	{
	//DBG_MESSAGE( "!!! COMPLETION: BYPASS EFB FLOOR LIMIT CHECK... !!!" );
		return OverFloorLimit;
	}

	else
	{
		/* Check Credit Limit */
		if ( IS_CREDIT_TXN )
		{
			if ( txn.amount_total > terminal.configuration.efb_credit_limit*100 )
			{
				//VIM_DBG_MESSAGE("Over efb_credit_limit");
				OverFloorLimit = VIM_TRUE;
			}
		}
		else
		/* Check Debit Limit */
		{
			if ( txn.amount_total > terminal.configuration.efb_debit_limit*100 )
			{
				//VIM_DBG_MESSAGE("Over efb_debit_limit");
                pceft_debug_test(pceftpos_handle.instance, "EFBL222");

			    OverFloorLimit = VIM_TRUE;
			}
		}
	}
	return OverFloorLimit;
}

void Set91DoManual(void)
{
	/* Backup the original error */
	txn.error_backup = txn.error;

	vim_mem_copy(txn.host_rc_asc_backup, txn.host_rc_asc, VIM_SIZEOF(txn.host_rc_asc_backup));

	// RDD 150312 : SPIRA 5113 - need 91 do manual for over limit comms error declines	
	vim_mem_copy( txn.host_rc_asc, "91", 2 );	
	txn.error = ERR_WOW_ERC;
	
	// RDD - store these declines but don't ever try to send them to the host	  	
	txn.saf_reason = saf_reason_91;
}


void SetEfbApproved( VIM_BOOL Completion )
{
	/* Backup the original error */
	DBG_MESSAGE( "---------- SET EFB APPROVED ----------");
	  
	txn.error_backup = txn.error;
	vim_mem_copy(txn.host_rc_asc_backup, txn.host_rc_asc, VIM_SIZEOF(txn.host_rc_asc_backup));

	// RDD 210212 do not require sig for deposit completions
	// RDD 020312 SPIRA 5011 do not require sig for gift card EFB activation 
	if(
		( txn.type == tt_deposit ) || 
		(( txn.type == tt_refund ) && ( gift_card(&txn) )) || 
		(( txn.type == tt_completion ) && ( !txn_is_customer_present(&txn) )) 
		)
	{
		 
		vim_mem_copy(txn.host_rc_asc, "00", 2);
		// RDD 260312 SPIRA: 4911
		txn.error = VIM_ERROR_NONE;
	}
    else
	{
		vim_mem_copy(txn.host_rc_asc, "08", 2);
		txn.error = ERR_SIGNATURE_APPROVED_OFFLINE;
		VIM_DBG_ERROR( txn.error );
	}

	// RDD 260412 SPIRA:5366 Add Tag 8A "Z3" to DE55 for Contactless Fallback
	if( txn.card.type == card_picc )
	{
		VIM_TLV_LIST tag_list;
		VIM_CHAR terminal_rc[2+1];
	    // DBG_MESSAGE( "<<<<<<< PICC: ADD Z3 TO TXN EMV BUFFER >>>>>>>>>" );

		vim_strcpy( terminal_rc, "Z3" );
		// RDD update field 8A for second GEN AC - this will generate the correct cryptogram if the txn passed EMV EFB and was stored as an advice		  
		vim_tlv_open(&tag_list, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF(txn.emv_data.buffer), VIM_FALSE);
		vim_tlv_update_bytes( &tag_list, VIM_TAG_EMV_RESPONSE_CODE, terminal_rc, 2 );

		// RDD SPIRA:9215 missing 8A from results 010317 added
		txn.emv_data.data_size = tag_list.total_length;

		// RDD 030317 SPIRA:9220 Can't gen a reversal here else we can have a reversal and a 221 existing at the same time
		//reversal_set( &txn );
	}

	// BRD If approved in EFB SVP no longer qualifies
	// RDD 090512 SPIRA:5470 Note that the above is not always true. This flag must be restored in cases where the card Y3 approves the txn
	txn.qualify_svp = VIM_FALSE;				

	// RDD 160612 SPIRA:5655 Set offline flag so EPAN printed for EFB

	// RDD 051212 SPIRA:6416 - I think its safe to mark completions as EFB for EOV processing - completion as a txn type is checked before EFB flag in AS2805
	if( Completion )
	{
		// RDD - maybe need to change this if there are problems with setting the efb flag for cerrainl types of completions...
		transaction_set_status( &txn, st_efb );
	}
	else
	{
		transaction_set_status( &txn, st_efb );
	}
  
	/* Set the transaction as requires trickle feed */
	transaction_set_status(&txn, st_needs_sending);
	// RDD 171214 Need to set this flag for EFB approved TXNS in order to print EPAN
	transaction_set_status( &txn, st_offline );
}


VIM_BOOL FullEmv(transaction_t *transaction)
{
    // RDD Trello #203 - always Full EMV for CTLS
    if( transaction->card.type == card_picc )
        return VIM_TRUE;

    if( transaction->card.type == card_chip )
    {
        if( !transaction_get_status( transaction, st_partial_emv ))
        {
            return VIM_TRUE;
        }
    }
    return VIM_FALSE;
}


// RDD 051212 New Requirement Added for New Zealand - Check SAF for existing txns 
static VIM_BOOL EovAllowed( void )
{
    VIM_SIZE index = 0;
	if(( txn.account == account_savings ) || ( txn.account == account_cheque ) || (( txn.account == account_credit ) && ( txn.card.type == card_mag )) )
	{
		if( txn.amount_cash ) 
		{
            vim_strcpy(txn.cnp_error, "ENC");

			vim_mem_copy(txn.host_rc_asc, "TE", 2);
			txn.error = ERR_NO_CASHOUT;
			return VIM_FALSE;
		}
		else
		{
			// Check to see if there are any matching efb txns already stored in the SAF
            if (saf_duplicate_found(&txn, st_efb, &index, (BITMAP_CARD + BITMAP_TIME)))
			{
				DBG_MESSAGE( "DUPLICATE FOUND IN SAF - FAIL EFB" );
                vim_strcpy(txn.cnp_error, "END");
                vim_mem_copy(txn.host_rc_asc, "T3", 2);
				txn.error = ERR_WOW_ERC;
				return VIM_FALSE;
			}
		}
	}
	return VIM_TRUE;
}



VIM_BOOL EfbOverFuelCardLimit( void )
{
	VIM_UINT8 n, CpatIndex = FUEL_CARD_OLT_INDEX;
	s_pad_data_t *PadData = &txn.u_PadData.PadData;
	offline_limit_table_t LocalLimitsWorkArea;													// Define a work area for decermenting Offline Limits wrt incoming PAD data amounts
	offline_limit_table_t *table_ptr = &LocalLimitsWorkArea;									// Create a Ptr to the work area
	offline_limit_table_t *offline_limit_ptr = &fuelcard_data.OfflineLimitTables[CpatIndex];	// Create a pointer to the relevent index in the FCAT OLT tables

	if (terminal_get_status(ss_wow_basic_test_mode))
	{
		VIM_CHAR buffer[50];
		vim_sprintf( buffer, "%s %d", "FCD EFB. OLT INDEX=", CpatIndex );
		pceft_debug(pceftpos_handle.instance, buffer );
	}
	vim_mem_copy( table_ptr, offline_limit_ptr, VIM_SIZEOF(offline_limit_table_t) );			// Copy over the table into the work area

	// RDD 040812 - If no producet groups defined used generic credit limit
	if( !PadData->NumberOfGroups ) return EfbOverFloorLimit( );

	// Number of groups was set when validating the incoming Pad String. We don't add Products in VX820 version.
	for( n=0; n<PadData->NumberOfGroups; n++ )
	{
		VIM_SIZE ProductGroupID = PadData->product_data[n].ProductID;
		product_def_t *ProductTable = &fuelcard_data.ProductTables[ProductGroupID];
		
		if( !ProductGroupID )
		{
			// RDD 120712 : If no product groups then use Credit limit
			if( n==0 ) return EfbOverFloorLimit( );
			break;
		}
		if( PadData->product_data[n].ProductAmount <= table_ptr->OfflineLimitTable[ProductTable->GroupType] )
			table_ptr->OfflineLimitTable[ProductTable->GroupType] -= PadData->product_data[n].ProductAmount;
		else
			return VIM_TRUE;		// Product Amount has exceeded defined offline limit
	}
	return VIM_FALSE;				// We've trawled through the basket and offline limits have not been exceeded
}
  
/**
 * Process the transaction in EFB mode. If EFB not allowed, txn error code is left as is so decline reason will be the comms error
 */

VIM_BOOL IsTokenisedTxn( transaction_t *transaction )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR CDOFileData[2000];
	VIM_CHAR *Ptr = CDOFileData;
	VIM_CHAR *TagPtr = Ptr;
	VIM_CHAR BinBuffer[100];	
	VIM_SIZE /*Len,*/ CDOFileSize;

	if( !CDOFilePresent() )
		return VIM_FALSE;

	vim_mem_clear( CDOFileData, VIM_SIZEOF( CDOFileData ));
		
	// Read the file into a buffer 
	vim_file_get_size( picc_cdo_file_name, VIM_NULL, &CDOFileSize );

	if( CDOFileSize >= VIM_SIZEOF(CDOFileData) )
	{
		VIM_CHAR Buffer[100];
		pceft_debug( pceftpos_handle.instance, "SPIRA:9020 ERROR:CDOPAT.XML TOO LARGE!!" );
		vim_sprintf( Buffer, "SPIRA:9020 ERROR: CDOPAT.XML Too Large !! : SIZE:%d, MAX SIZE %d", CDOFileSize, VIM_SIZEOF( CDOFileData )); 
		pceft_debug( pceftpos_handle.instance, Buffer );
		return VIM_FALSE;
	}
	vim_file_get( picc_cdo_file_name, Ptr, &CDOFileSize, VIM_SIZEOF(CDOFileData) );

	// RDD 120215 added facility to remove comments from XML strings	
	RemoveComments( CDOFileData );

	DBG_MESSAGE( "CDO File Opened... " );

	while( res == VIM_ERROR_NONE )
	{
		VIM_CHAR TlvBuffer[500];
		VIM_CHAR TagData[500];
		VIM_UINT8 HexData[250];
		vim_mem_clear( BinBuffer, VIM_SIZEOF( BinBuffer ));
		vim_mem_clear( TlvBuffer, VIM_SIZEOF( TlvBuffer ));
		vim_mem_clear( TagData, VIM_SIZEOF( TagData ));
		vim_mem_clear( HexData, VIM_SIZEOF( HexData ));

		if( vim_strstr( TagPtr, PICC_TAG, &TagPtr ) != VIM_ERROR_NONE )
			return VIM_FALSE;

		if(( res = xml_get_tag( (VIM_CHAR *)TagPtr, PICC_TAG, (VIM_CHAR *)BinBuffer )) == VIM_ERROR_NONE )	
		{
			VIM_TLV_LIST cdo_tags;
			VIM_TLV_LIST txn_tags;

			VIM_SIZE AscLen, HexLen = 0;

			TagPtr += vim_strlen(BinBuffer) + vim_strlen( PICC_TAG ) + vim_strlen( PICC_TAG ) +2;
			AscLen = vim_strlen( BinBuffer );

			asc_to_hex( BinBuffer, HexData, AscLen );
			HexLen = AscLen%2 ? 1+(AscLen/2) : AscLen/2;
			vim_tlv_create( &cdo_tags, TlvBuffer, VIM_SIZEOF( TlvBuffer )); 
			if( vim_tlv_open( &cdo_tags, HexData, HexLen, VIM_SIZEOF( TlvBuffer ), VIM_FALSE ) != VIM_ERROR_NONE )
				return VIM_FALSE;

			vim_tlv_rewind( &cdo_tags );

			if( VIM_ERROR_NONE == vim_tlv_open( &txn_tags, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer), VIM_FALSE ))
			{
				// RDD 070816 Run through the list of EMV tags in this XML Tag
				VIM_BOOL Match = VIM_FALSE;
				VIM_DBG_TLV( cdo_tags );
				while(( res = vim_tlv_goto_next( &cdo_tags )) == VIM_ERROR_NONE )
				{
					if( VIM_ERROR_NONE == vim_tlv_search( &txn_tags, (const void *)cdo_tags.current_item.tag ))
					{
						// Match stays true if all tag data match up in a single line
						if( vim_mem_compare( txn_tags.current_item.value, cdo_tags.current_item.value, cdo_tags.current_item.length ) == VIM_ERROR_NONE )
							Match = VIM_TRUE;
						else
						{
							Match = VIM_FALSE;
							break;
						}
					}
					else
					{
						Match = VIM_FALSE;
						break;
					}
				}
				if( Match == VIM_TRUE )
					return VIM_TRUE;
			}
		}
	}
	return VIM_FALSE;
}


VIM_BOOL SPIRA_9020( void )
{
	// RDD 070816 V560-6 New function to attempt to identify tokenised txns
	if(( txn.card.type == card_picc ) && ( IsTokenisedTxn( &txn )))
	{
		vim_strncpy( txn.host_rc_asc, "T9", 2 );
		pceft_debug( pceftpos_handle.instance, "SPIRA:9020 T9 TOKENISED" );
		txn.error = ERR_WOW_ERC;
		return VIM_TRUE;
	}
	return VIM_FALSE;
}


void efb_process_txn(void)
{  
    //VIM_BOOL auth_no_required = VIM_FALSE;
	VIM_CHAR service_code[3] = {'0','0','0'};

	VIM_UINT8 RC = asc_to_bin(VIM_CHAR_PTR_FROM_UINT8_PTR(txn.host_rc_asc), 2);
	VIM_DBG_NUMBER( txn.type == tt_checkout);

	if( txn.type == tt_refund && IsUPI( &txn ))
	{
        vim_strcpy(txn.cnp_error, "EUR");
		SetEfbApproved( VIM_TRUE );
		return;
	}

	// RDD 190716 SPIRA:9020 - Disable CTLS fallback on refunds for VISA and EFTPOS
	if( SPIRA_9020() )
	{
        vim_strcpy( txn.cnp_error, "EMP" );
        return;
	}

    // RDD 310814 If checkout of existing preauth then code is ERR_SIGNATURE_APPROVED_OFFLINE
    if (txn.error == ERR_SIGNATURE_APPROVED_OFFLINE)
    {
        vim_strcpy(txn.cnp_error, "ESO");
        return;
    }

	// RDD 251112 SPIRA:8596 Check for card removal here as Donna complaining that card removal for offlines doesn't produce an error
    if (EmvCardRemoved(VIM_TRUE) != VIM_ERROR_NONE)
    {
        vim_strcpy(txn.cnp_error, "ECR");
        return;
    }
  
    // if the transaction was approved or declined by the host or the card then skip this state except for 91 (issuer not available)  
    if((( txn.error == VIM_ERROR_NONE )||( txn.error == ERR_WOW_ERC )) && ( !FullEmv(&txn) ))  
    {
	  if( RC == RC91 )
	  {	
          DBG_POINT;
	  }
	  else
	  {	
          DBG_POINT;        
          vim_strcpy(txn.cnp_error, "ETD");	    
          // DBG_MESSAGE( "<<<<<<< EFB FAILED: NON-EMV TXN WAS APPROVED OR DECLINED (except 91) >>>>>>>>>" );		
          return;
	  }
  }
  DBG_POINT;

  
#if 0
  // RDD 200412 SPIRA:5341 "Yx" Transactions should be stored in SAF but not be treated as EFB
  if( txn.saf_reason == saf_reason_offline_emv_purchase )
  {
	  // DBG_MESSAGE( "<<< EFB PROCESSING: Y3 TRANSACTION IS NOT APPROVED EFB. EXIT HERE >>>" );
	  return;
  }
#endif

#if 0	// RDD 051212 BD Says move this to bottom as TP may result in txns for expired cards being stored in POS if SAF is full
  // if MAX SAF count exceeded  
  if( saf_full() )
  {
	  // //VIM_DBG_MESSAGE("<<< SAF FULL >>>");

	  // RDD 050412 SPIRA:5253 Need to store txn in buffer if SAF full reported					
	  saf_add_record( &txn, VIM_TRUE );

	  //vim_mem_copy(txn.host_rc_asc, "91", 2);
	  vim_mem_copy(txn.host_rc_asc, "TP", 2);
	  txn.error = ERR_WOW_ERC;
	  return;
  }
#endif

  // RDD 281211: If we're here then the txn no longer qualifies for SVP
  txn.qualify_svp = VIM_FALSE;

  // RDD 130212: If it's a completion with an auth code then approve it offline without further ado
  if(( txn.type == tt_completion ) && ( ValidateAuthCode( txn.auth_no )) )
  {
	// RDD 051212 - the flag denotes whether or not to set approved as completion type. Affects status flag for later EOV processing
	DBG_POINT;
	SetEfbApproved( VIM_TRUE );
	return;
  }

  // RDD 020614 V424 SPIRA:7569 - some cards don't support offline deposits
  // RDD 020420 v582-7 Re-purpose these flags for CVM override - from current CPAT only WoW cards (4363xx ) allow deposit and these are all forced online
#if 0
  if(( CARD_NAME_FORCE_DEPOSIT_ONLINE ) && ( txn.type == tt_deposit ))
#else
  if( txn.type == tt_deposit )          // RDD 020420 v582-7 all deposits must be online. Force online flag now used for CTLS CVM override
#endif
  {
	  // RDD 110614 v425b - for RC91 force error T3 as we don't want to allow manual
	  // RDD 040914 v431b - SPIRA:8066 Looks like they want T3 for any eventuality
	  if(/* RC == RC91*/1 )
	  {
          vim_strcpy(txn.cnp_error, "EDP");

		  vim_strncpy( txn.host_rc_asc, "T3", 2 );
		  txn.error = ERR_WOW_ERC;
	  }
	  DBG_MESSAGE( "EFB Deposit Not Allowed by CPAT" );

	  return;
  }


  // RDD 020714 - no EFB for preauth ... for now 
  if( txn.type == tt_preauth )
  {
      vim_strcpy(txn.cnp_error, "EPA");
      return;
  }

  // Check Expiry Date
  // RDD 260312 : SPIRA 4911 Deposit should not fail EFB if card expired
  if ((card_check_expired(&txn.card)) && ( txn.type != tt_deposit ))
  {
	  // DBG_MESSAGE( "<<<<<<< EFB FAILED: CARD EXPIRED >>>>>>>>>" );
	  // RDD 270312 : SPIRA 4911 - Mitch I. says that EMV card should reject TQ not Z3
	  if(/* !FullEmv()*/1 )
	  {		
		  // RDD 270814 SPIRA: 8076 Want "TQ" here was changed from TQ to T3 some time ago, but don't know why.
		  //vim_mem_copy(txn.host_rc_asc, "T3", 2);		
		  vim_mem_copy( txn.host_rc_asc, "TQ", 2 );		
          vim_strcpy(txn.cnp_error, "EXP");
          txn.error = ERR_CARD_EXPIRED;
	  }
	  return;
  }

  
#if 1
  // RDD 151116 SPIRA:9087 91 response to Cash on credit needs to return this error rather than Z3 - so check first !
  // RDD 240114 SPIRA:7185 No Cashout on EFB for both Aus and NZ
  if( txn.amount_cash )
  {			
      vim_strcpy(txn.cnp_error, "ENC");
      vim_mem_copy(txn.host_rc_asc, "TE", 2);
	  txn.error = ERR_NO_CASHOUT;
	  return;
  }
#endif


#if 1	// RDD 140218 JIRA WWBP-155 UPI Cards doing NO Bank Do Manual instead of hard decline due to 'SPIRA 9239'.
  // WOW - no EFB if PSC 3x or 4x indicate online only
  if(( WOW_CPAT_SWIPE_OR_KEY_ONLINE ) || ( WOW_CPAT_SWIPE_ONLINE ))
  {
      // RDD 151220 JIRA PIRPIN-945 Avalon Gift Card Processing - Offline Barcode Activations need to SAF
      if(txn.type == tt_gift_card_activation) 
      {         
          if (transaction_get_status(&txn, st_pos_sent_keyed_pan))
          {
              pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-945 Offline Activation K - offline decline");
              vim_strcpy(txn.cnp_error, "EOA");
              DBG_MESSAGE("CPAT SAYS ONLINE ONLY - GIFT ACTIVATE");
              vim_mem_copy(txn.host_rc_asc, "T3", 2);
              txn.error = ERR_WOW_ERC;
              return;
          }
          else
          {
              pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-945 Offline Activation not Keyed - don't decline here");
          }
      }
	  // DBG_MESSAGE( "<<<<<<< EFB FAILED: CPAT MUST GO ONLINE >>>>>>>>>" );
	  else if ( /*!FullEmv()*/1)	// RDD 280312 - Donna says ALL cards must check CPAT in EFB
	  {
		  // RDD 270312 : SPIRA 5177 - Donna G. Says T3 Code reported in case when CPAT says tran must go online
          vim_strcpy(txn.cnp_error, "EOO");
          DBG_MESSAGE("CPAT SAYS ONLINE ONLY");
		  //vim_mem_copy(txn.host_rc_asc, "91", 2);
		  vim_mem_copy(txn.host_rc_asc, "T3", 2);
		  txn.error = ERR_WOW_ERC;
	  }
	  return;
  }

#endif

  // RDD 170212 - Handle EMV Cards seperatly
  if(( txn.card.type == card_picc ) || ((txn.card.type == card_chip) && !transaction_get_status( &txn, st_partial_emv ))) 
  {
	  DBG_POINT;
	  // RDD - if any bits set then this classifies as declined by TVR
	  if( emv_efb_decline() )
	  {		      
		  txn.error = ERR_UNABLE_TO_GO_ONLINE_DECLINED;
		  // RDD 071112 SPIRA:6261 this error is not being detected, but we should fudge a Z3 resonse here surely?
		  vim_mem_copy(txn.host_rc_asc, "Z3", 2);
		  VIM_DBG_MESSAGE("EMV EFB Declined");
		  return;
	  }
	  // RDD 110917 SPIRA:9239 "No Bank Do Manual" required for txn that passed SAC and over floor limit
	  else
	  {
		  /* Check decline rules for EMV transaction */
		  // RDD added 040112 - EFB Floor Limit check for failed full EMV txns (WBC didn't check and declined all these)	  
		  if( txn.amount_total > terminal.emv_applicationIDs[txn.aid_index].efb_floor_limit *100 )
		  {
			  DBG_MESSAGE("Over AID efb limit");
			  DBG_VAR( txn.aid_index );
			  DBG_VAR( terminal.emv_applicationIDs[txn.aid_index].efb_floor_limit );

			  // RDD 080312 : SPIRA 5024 "VIM_ERROR_NO_LINK" results in "S7" (EFT SERVER) Comm Error reported on receipt instead of "Z3"
		  
			  //txn.error = ERR_UNABLE_TO_GO_ONLINE_DECLINED;
		  
			  // RDD 190716 SPIRA:9021 Set T3 for UPI offline decline
		  
			  // SPIRA:9020 080816
		  
			  // From MB 080816: "if it is declined due to the EPAT CDO floor limit it should be "TT Decllined\nPlease Insert Card".
		  
			  if( txn.card.type == card_picc )
			  {
                  vim_strcpy(txn.cnp_error, "EFT");
				  vim_mem_copy(txn.host_rc_asc, "TT", 2);			
			  }
              // RDD JIRA WWBP-652
              else if (txn.card.type == card_chip)
              {
                  vim_strcpy(txn.cnp_error, "EFD");
                  pceft_debug_test(pceftpos_handle.instance, "JIRA:WWBP-652 (Over EFB docked)");
                  vim_mem_copy(txn.host_rc_asc, "Z3", 2);
                  return;

              }
			  else if (!(IS_R10_POS))
			  {
				  vim_mem_copy(txn.host_rc_asc, "T3", 2);
				  txn.error = ERR_WOW_ERC;
				  return;
			  }
              else
              {
                  vim_strcpy(txn.cnp_error, "E91");
                  pceft_debug_test(pceftpos_handle.instance, "SPIRA:9239");
                  Set91DoManual();
                  return;
              }
			  txn.error = ERR_WOW_ERC;
			  return;
		  }
		  else
		  {
			  //SetEfbApproved( VIM_FALSE );
			  DBG_MESSAGE("Under AID efb limit and TVR OK");
			  //return;
			  // RDD - don't return here because we need to check the other stuff but note EMV cards keep the Z3 on the receipt
		  }
	  }
  }
  else
  {
	  DBG_POINT;
  }

  
  // WOW - no EFB for manually entered cards 
  if( txn.card.type == card_manual ) 
  {
      // RDD 151220 JIRA PIRPIN-945 Avalon Gift Card Processing - Offline Barcode Activations need to SAF
      if (txn.type == tt_gift_card_activation)
      {
          if (transaction_get_status(&txn, st_pos_sent_keyed_pan))
          {
              pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-945 Offline Activation K - offline decline");
              vim_strcpy(txn.cnp_error, "EFA");
              DBG_MESSAGE("<<<<<<< EFB FAILED: NO MANUAL GIFTCARD ACTIVATION >>>>>>>>>");
              vim_mem_copy(txn.host_rc_asc, "Q6", 2);
              txn.error = ERR_WOW_ERC;
              return;
          }
          else
          {
              pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-945 Offline Activation not Keyed - don't decline here");
          }
      }
      else
      {
          vim_strcpy(txn.cnp_error, "EFM");
          DBG_MESSAGE("<<<<<<< EFB FAILED: NO MANUAL >>>>>>>>>");
          vim_mem_copy(txn.host_rc_asc, "Q6", 2);
          txn.error = ERR_WOW_ERC;
          return;
      }
  }

#if 0	// RDD 140218 JIRA WWBP-155 UPI Cards doing NO Bank Do Manual instead of hard decline due to 'SPIRA 9239'.
  // WOW - no EFB if PSC 3x or 4x indicate online only
  if(( WOW_CPAT_SWIPE_OR_KEY_ONLINE ) || ( WOW_CPAT_SWIPE_ONLINE ))
  {
	  // DBG_MESSAGE( "<<<<<<< EFB FAILED: CPAT MUST GO ONLINE >>>>>>>>>" );
	  if( /*!FullEmv()*/1 )	// RDD 280312 - Donna says ALL cards must check CPAT in EFB
	  {
		  // RDD 270312 : SPIRA 5177 - Donna G. Says T3 Code reported in case when CPAT says tran must go online
		  DBG_MESSAGE( "CPAT SAYS ONLINE ONLY" );
		  //vim_mem_copy(txn.host_rc_asc, "91", 2);
		  vim_mem_copy(txn.host_rc_asc, "T3", 2);
		  txn.error = ERR_WOW_ERC;
	  }
	  return;
  }
  
#endif
#if 0
  // RDD 151116 SPIRA:9087 91 response to Cash on credit needs to return this error rather than Z3
  // RDD 240114 SPIRA:7185 No Cashout on EFB for both Aus and NZ
  if( txn.amount_cash )
  {			
	  vim_mem_copy(txn.host_rc_asc, "TE", 2);			
	  txn.error = ERR_NO_CASHOUT;
	  return;
  }
#endif

  // RDD 300512 EFB Processing for Fuel Cards added
  if( fuel_card(&txn) )
  {
	  if( EfbOverFuelCardLimit() )
	  {
          vim_strcpy(txn.cnp_error, "EFF");
          pceft_debug_test(pceftpos_handle.instance, "EFBL795");
		  Set91DoManual();
		  return;
	  }
  }
  else
  {
	if(( card_get_service_code( &txn.card.data.mag, service_code ) == VIM_ERROR_NONE ) && ( CARD_NAME_SERVICE_CODE_CHECKING ))
	{
		if( service_code[1] != '0' )	// Issuer says normal rules apply (not forced online)
		{
            vim_strcpy(txn.cnp_error, "EFS");

			//DBG_MESSAGE( "<<<<<<< EFB FAILED: ESC MUST GO ONLINE >>>>>>>>>" );
			if( /*!FullEmv()*/1 )
			{
				// RDD 161012 Changed from TJ eror at Donnas' request
				vim_mem_copy(txn.host_rc_asc, "T3", 2);
				txn.error = ERR_WOW_ERC;
			}
			return;
		}
	}
	 
	// RDD 051212 EFB Changes - New Zealand Requirement for only allowing 1 EFB per day per card
	if(( WOW_NZ_PINPAD && !EovAllowed() )) return;

	// Floor limit checks for all cards - different limits apply to credit, debit and emv cards
	if(( EfbOverFloorLimit() ) && (( !FullEmv(&txn) || ( TERM_USE_PCI_TOKENS == VIM_FALSE) )))
	{
		// //VIM_DBG_MESSAGE("<<< EFB OVER FLOOR LIMIT >>>");

		// RDD 150312 SPIRA 5113 - Non EMV txns need to goto 91 DO MANUAL if over floor limit
		// RDD 101212 SPIRA 6416 - except for New Zealand Debits over the limit - these are hard rejected ( T3 according to DG )
		if((( WOW_NZ_PINPAD ) && ( IS_DEBIT_TXN ))||(!IS_R10_POS))
		{
			vim_mem_copy(txn.host_rc_asc, "T3", 2);
			txn.error = ERR_WOW_ERC;
		}
		else
		{
            pceft_debug_test(pceftpos_handle.instance, "EFBL833");
			Set91DoManual();
		}
		return;
	}
  }

  // RDD 020312: SPIRA 5011 v15 : No EFB for Gift Card Purchase
  if( gift_card(&txn) && ( txn.type == tt_sale ))
  {
	  // RDD 260713: SPIRA 6782 v417 : Gift Cards Purchase must return T3 ( Bank unavailable ).
	  vim_mem_copy(txn.host_rc_asc, "T3", 2);
	  txn.error = ERR_WOW_ERC;

	  // RDD 010512: SPIRA 5408 v15 : No EFB for Gift Card Purchase but save in buffer as 91
	  //Set91DoManual();
	  return;
  }

#if 1	// RDD 051212: V305 - EFB Changes: BD Says move this to just above SAF checking so other checks are done first.
  // RDD 080212 - Cards Swiped due to technical fallback are not valid for EFB because of FRAUD risks
  
  // VN JIRA WWBP-423 140219 F function (Gift Card Activation) displays NO BANK DO MANUAL when OFFLINE
  if((  txn.card.data.mag.emv_fallback ) && ( txn.card.type == card_mag ) && (!gift_card(&txn))  && ( IS_R10_POS ))
  {
	//DBG_MESSAGE( "!!! MAG CARD FALLBACK - DON'T ALLOW EFB !!!" );
    pceft_debug_test(pceftpos_handle.instance, "EFBL859");
	Set91DoManual();
	return;
  }
#endif


#if 1  // RDD 051212: V305 - EFB Changes: BD Says Check if SAF full last as TXN will be kept on POS as approved if TP sent back
  if( saf_full(VIM_TRUE) )
  {
	  // //VIM_DBG_MESSAGE("<<< SAF FULL >>>");

	  // RDD 050412 SPIRA:5253 Need to store txn in buffer if SAF full reported					
	  saf_add_record( &txn, VIM_TRUE );

	  //vim_mem_copy(txn.host_rc_asc, "91", 2);
	  vim_mem_copy(txn.host_rc_asc, "TP", 2);
	  txn.error = ERR_WOW_ERC;
	  return;
  }
#endif

  // RDD 051212 - the flag denotes whether or not to set approved as completion type. Affects status flag for later EOV processing
  SetEfbApproved( VIM_FALSE );
}

