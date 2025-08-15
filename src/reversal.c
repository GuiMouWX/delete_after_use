
#include <incs.h>
VIM_DBG_SET_FILE("TO");

extern card_bin_name_item_t CardNameTable[];
extern VIM_BOOL IsSDA(transaction_t *transaction);

static const char *reversal_file = VIM_FILE_PATH_LOCAL "reversal.dat";


VIM_BOOL reversal_present(void)
{
  VIM_BOOL exists;
  
  if (vim_file_exists(reversal_file,&exists)==VIM_ERROR_NONE)
  {
    return exists;
  }
  return VIM_FALSE;
}



VIM_ERROR_PTR reversal_load( transaction_t *transaction )
{
  VIM_SIZE buffer_size;
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];

  VIM_ERROR_RETURN_ON_FAIL(vim_file_get(reversal_file,buffer,&buffer_size,VIM_SIZEOF(buffer)));

  VIM_ERROR_RETURN_ON_FAIL( transaction_deserialize( transaction, buffer, buffer_size, VIM_TRUE ));

  return VIM_ERROR_NONE;
}

// RDD 280721 - JIRA PIRPIN-1171 don't exit on repeat if SDA
VIM_ERROR_PTR reversal_store_original_stan( transaction_t *transaction, VIM_BOOL SDA )
{
	/* No reversals in training mode*/
	if ((transaction_get_training_mode() || transaction->type == tt_query_card_get_token))
		return VIM_ERROR_NONE;

	if(( saf_totals.fields.saf_repeat_pending ) && ( !SDA ))
	{
		pceft_debug_test(pceftpos_handle.instance, "SPIRA:9220 No 0420 if 0221 pending");
		return VIM_ERROR_NONE;
	}

	if (!reversal_present())
	{
		transaction->orig_stan = transaction->stan;		// BRD Save for DE90 - MGC 170120 - Only save the original STAN once
		VIM_DBG_NUMBER(transaction->orig_stan);
	}

	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR reversal_set( transaction_t *transaction, VIM_BOOL SDA )
{
  VIM_UINT32 record_length;
  VIM_UINT8 record_buffer[WOW_MAX_TXN_SIZE];
  VIM_RTC_TIME send_time = transaction->send_time;
  transaction_t temp_txn;

  /* No reversals in training mode*/
  if(( transaction_get_training_mode() || transaction->type == tt_query_card_get_token ))
    return VIM_ERROR_NONE;

  // RDD 150121 - decided to handle QC reversals entirely in QC App
  if (QC_TRANSACTION(transaction))
  {
      //pceft_debug_test(pceftpos_handle.instance, "QC Reversal not handled in PA");
      return VIM_ERROR_NONE;
  }
  if (IsEverydayPay(transaction))
  {
      pceft_debug_test(pceftpos_handle.instance, "EDP Reversal not handled in PA");
      return VIM_ERROR_NONE;
  }

  if(( saf_totals.fields.saf_repeat_pending )&&( !SDA ))
  {
	  pceft_debug_test( pceftpos_handle.instance, "SPIRA:9220 No 0420 if 0221 pending-" );
	  return VIM_ERROR_NONE;
  }

  // RDD 290412 SPIRA 5378 Y3->Y1 problems in reversal. Looks like reversal data is being overwritten. Ensure that data doesn't change
  if( reversal_present() ) 
  {
	  VIM_DBG_MESSAGE("%%%%%%%%%%%% REVERSAL PRESENT %%%%%%%%%%%%%%%");
	  reversal_load( &temp_txn );
	  if( transaction_get_status( transaction, st_repeat_saf ) )
	  {
		  // RDD SPIRA:5471 090512 - Ensure that we ONLY update the status, to show repeat, otherwise we can write crap into the reversal
		  // RDD 261012  - it was noted that the STAN of repeat reversals can sometimes get stuffed so ensure that the original reversal file is not changed
		  // except to update the repeat status. We do this by loading the reversal into a temp buffer and testing it, rather than overwriting the reversal 
		  // whatever is in the current txn store
		  if( transaction_get_status( &temp_txn, st_repeat_saf ))
		  {
			  // RDD 090512 Status already set to repeat so exit here
			  return VIM_ERROR_NONE;
		  }
		  else
		  {
              // RDD 280721 - don't think we want to do this here as reversal can be saved multiple times
              // RDD 020821 - no we do need it in order to label reversal repeats ( 0421 ) 
			  transaction_set_status( &temp_txn, st_repeat_saf );
		  }
	  }
	  // RDD 010317 if the stan is the same then we're still in the txn and we're just using the reversal save to update the DE55 with Script results or Z3 etc. 
	  if( temp_txn.stan == transaction->stan )
	  {
		  VIM_DBG_MESSAGE( "STAN Match and EMV data change - allow DE55 update" );
          // This is needed for Z4 updates to EMV data buffer to be sent in reversal
		  if( transaction->emv_data.data_size >= temp_txn.emv_data.data_size ) 
		  {
			  vim_mem_copy( temp_txn.emv_data.buffer, transaction->emv_data.buffer, transaction->emv_data.data_size );
			  temp_txn.emv_data.data_size = transaction->emv_data.data_size;
			  VIM_DBG_DATA( temp_txn.emv_data.buffer, temp_txn.emv_data.data_size );
		  }
	  }
	  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize( &temp_txn, record_buffer, &record_length, sizeof(record_buffer)) );	  
  }
  else	// Create Reversal from current Txn
  {
      VIM_DBG_MESSAGE("%%%%%%%%%%%% CREATE REVERSAL DE90 %%%%%%%%%%%%%%%");
	  //transaction->orig_stan = transaction->stan;		// BRD Save for DE90 - MGC 170120 - Only save the original STAN once - Move this out of here, because reversals can apparently be re-created on this project
	 // VIM_DBG_NUMBER(transaction->orig_stan);
	  // RDD 180222 JIRA PIRPIN-1441 ensure that transaciton->stan is not incremented permanenty as this affect some receipts.
	  temp_txn.stan = transaction->stan;
	  transaction->stan = terminal.stan;
	  // RDD 200512 - now changed AS2805 to use next stan rather than original stan for 420 so no need to increment terminal stan here
	  // RDD 041112 - changed back to using original stan for all reversals so increment here so this stan can't be reused except by a repeat.
      // RDD 171019 JIRA WWBP-742 This causes problems if reversals are being cleared and then re-created during the same transaction (required for power fail reversal handling)
	  VIM_DBG_MESSAGE("Increment STAN");

      // RDD 280721 - Repeat was set in 0220 which causes 0421 to be sent instead of 0420
      if( IsSDA( transaction ))
        transaction_clear_status(transaction, st_repeat_saf);

	  terminal_increment_stan();
	  VIM_DBG_NUMBER(transaction->stan);
	  transaction->send_time = send_time;

	  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize( transaction, record_buffer, &record_length, sizeof(record_buffer)) );	  
	  // RDD 180222 JIRA PIRPIN-1441 ensure that transaciton->stan is not incremented permanenty as this affect some receipts.
	  transaction->stan = temp_txn.stan;
  }	  
  // RDD 241012 Bugfix to bug found by BD - need to save send time passed in txn struct as this may not have been saved 
  // when the reversal was originally saved

  VIM_DBG_MESSAGE("%%%%%%%%%%%% SET REVERSAL %%%%%%%%%%%%%%%");
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set(reversal_file, record_buffer, record_length) ); 
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR reversal_clear( VIM_BOOL fAddToSaf )
{
	VIM_BOOL exist;

	DBG_MESSAGE("CLEAR REVERSAL");
	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( reversal_file, &exist ));

	if( exist )
	{
		DBG_MESSAGE( "Deleting reversal file" );

		// RDD 281112 SPIRA:6402 Need to increment stan once a reversal has been sucessfully sent, otherwise the following message may have the same stan
		//terminal_increment_stan();

		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(reversal_file));

		// RDD 051211 - If a reversal was sucessfully sent then we add it to the SAF for prinitng later

		if(( fAddToSaf )&&( reversal.type != tt_preauth ))
		{
			transaction_set_status( &reversal, st_void );
			transaction_set_status( &reversal, st_message_sent );
			transaction_clear_status( &reversal, st_needs_sending );

			if( !terminal_get_status( ss_no_saf_prints ) )
			{
				DBG_MESSAGE( "Adding Reversal to SAF file" );			
				VIM_ERROR_RETURN_ON_FAIL( saf_add_record( &reversal, VIM_FALSE ) );			
				{				
					saf_totals.fields.saf_print_count += 1;			
				}
			}
		}
	}
	 
    terminal.reversal_count = 0;  
	return VIM_ERROR_NONE;
}





VIM_ERROR_PTR reversal_send( VIM_BOOL ShowDisplay )
{
  VIM_AS2805_MESSAGE msg;
  VIM_AS2805_MESSAGE_TYPE msg_type;
  VIM_ERROR_PTR res;

  reversal.error = res = VIM_ERROR_NONE;
  
  if( reversal_present() )
  {
	// load up the reversal from the reversal file
    VIM_ERROR_RETURN_ON_FAIL( reversal_load(&reversal) );

	msg_type = ( reversal.status & st_repeat_saf ) ? MESSAGE_WOW_421 : MESSAGE_WOW_420;

	// JIRA PIRPIN-2019 Malformed reversals need to be filtered out. If card type is "card_none" then invalid reversal was saved somehow so just throw away
	if (reversal.card.type == card_none)
	{
		pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2109 Discard Invalid Reversal");
		reversal_clear(VIM_FALSE);
		return VIM_ERROR_NONE;
	}


	vim_rtc_get_time( &reversal.send_time );

	if( ShowDisplay )
	{
		display_screen( DISP_SENDING_REVERSAL, VIM_PCEFTPOS_KEY_NONE, 0 );
	}

	// RDD 270712 TODO (maybe in phase 3) SPIRA:5799 Set repeat after sucessful connection to host 
	// RDD 050412 - Set the repeat just before we send the reversal in case of power fail before the PinPad times out
	//transaction_set_status( &reversal, st_repeat_saf );

	// Update the reversal file with the send time and repeat flags
	//reversal_set( &reversal );

	// RDD 170113 SPIRA:6505 Allow interupts for Reversals and repeat SAFs
	// RDD 250113 - need to move this error to BEFORE the journal otherwise approved reversal jounals show with errors
       
    reversal.error = msgAS2805_transaction(HOST_WOW, &msg, msg_type, &reversal, VIM_FALSE, VIM_FALSE, VIM_FALSE);

	// RDD 010322: System Error reversals can block the PinPad from trading indefinately. report the STAN and delete it.
	if (reversal.error == VIM_ERROR_SYSTEM)
	{
		// This reversal will block the terminal so send it to the logs and delete it
		pceft_debugf(pceftpos_handle.instance, "Malformed Reversal STAN: %6.6d Forcing Delete", reversal.orig_stan );
		// reversal sent OK so delete the file
		reversal_clear(VIM_TRUE);	// Delete the reversal file but add the details to the SAF for printing

		// journal the reversal
		// RDD 210213 - SPIRA:6610 This was "journal type advice" until today !
		pceft_journal_txn(journal_type_reversal, &reversal, VIM_FALSE);
		//comms_get_last_turnaround_time( &reversal.turnaround );


		// RDD 2908 JIRA PIRPIN-2661 Malformed reversal gets stuck and blocks transactions. Add Exit here so reversal is not regenerated !
		return VIM_ERROR_NONE;

	}


    if( reversal.error == VIM_ERROR_NONE )
	{
		// reversal sent OK so delete the file
		reversal_clear( VIM_TRUE );	// Delete the reversal file but add the details to the SAF for printing

		// journal the reversal
		// RDD 210213 - SPIRA:6610 This was "journal type advice" until today !
		pceft_journal_txn( journal_type_reversal, &reversal, VIM_FALSE );
		//comms_get_last_turnaround_time( &reversal.turnaround );
	}
    else
    {
        // RDD 280721 JIRA PIRPIN-1171 Set repeat here - once reversal is actually sent.
        transaction_set_status(&reversal, st_repeat_saf);

        // RDD 020821 - If reversal failed ensure set with repeat for 421 gen
        reversal_set( &reversal, VIM_TRUE );
        //reversal_set(&reversal);
    }


	VIM_ERROR_RETURN_ON_FAIL( reversal.error );      
  } 
  // RDD Always clear reversal before SAFs we can't have a Reversal AND a repeat SAF at the same time

  // SPIRA:6947 SAF repeat sent after txn following power fail
  //else if(( saf_totals.fields.saf_count ) && ( saf_totals.fields.saf_repeat_pending ))
  else if( saf_totals.fields.saf_count )
  {
	  // Send any repeat SAF
	  res = saf_send( VIM_TRUE, TRICKLE_FEED_COUNT, VIM_FALSE, VIM_FALSE );
  }  
  		
  //VIM_ERROR_RETURN_ON_FAIL(pceft_pos_display_close());

  return res;
}
