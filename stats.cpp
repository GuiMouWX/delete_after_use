#include "incs.h"

VIM_DBG_SET_FILE("TS");
static const char *stats_file = VIM_FILE_PATH_LOCAL "stats.dat";


VIM_BOOL stats_present(void)
{
  VIM_BOOL exists;
  
  if (vim_file_exists(stats_file,&exists)==VIM_ERROR_NONE)
  {
    return exists;
  }
  return VIM_FALSE;
}


#if 0
VIM_ERROR_PTR stats_load( transaction_t *transaction )
{
  VIM_SIZE buffer_size;
  VIM_UINT8 buffer[WOW_MAX_TXN_SIZE];

  VIM_ERROR_RETURN_ON_FAIL(vim_file_get(reversal_file,buffer,&buffer_size,VIM_SIZEOF(buffer)));

  VIM_ERROR_RETURN_ON_FAIL( transaction_deserialize( transaction, buffer, buffer_size, VIM_TRUE ));

  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR reversal_set(transaction_t *transaction)
{
  VIM_UINT32 record_length;
  VIM_UINT8 record_buffer[WOW_MAX_TXN_SIZE];
  VIM_RTC_TIME send_time = transaction->send_time;
  transaction_t temp_txn;

  /* No reversals in training mode*/
  if(( transaction_get_training_mode() || transaction->type == tt_query_card_get_token ))
    return VIM_ERROR_NONE;

  // RDD 290412 SPIRA 5378 Y3->Y1 problems in reversal. Looks like reversal data is being overwritten. Ensure that data doesn't change
  if( reversal_present() ) 
  {
	  if( transaction_get_status( transaction, st_repeat_saf ) )
	  {
		  // RDD SPIRA:5471 090512 - Ensure that we ONLY update the status, to show repeat, otherwise we can write crap into the reversal
		  // RDD 261012  - it was noted that the STAN of repeat reversals can sometimes get stuffed so ensure that the original reversal file is not changed
		  // except to update the repeat status. We do this by loading the reversal into a temp buffer and testing it, rather than overwriting the reversal 
		  // whatever is in the current txn store
		  reversal_load( &temp_txn );
		  if( transaction_get_status( &temp_txn, st_repeat_saf ))
		  {
			  // RDD 090512 Status already set to repeat so exit here
			  return VIM_ERROR_NONE;
		  }
		  else
		  {
			  transaction_set_status( transaction, st_repeat_saf );
		  }
	  }
  }
  else
  {
      // DBG_MESSAGE("%%%%%%%%%%%% CREATE REVERSAL DE90 %%%%%%%%%%%%%%%");
	  transaction->orig_stan = transaction->stan;		// BRD Save for DE90
	  transaction->stan = terminal.stan;
	  // RDD 200512 - now changed AS2805 to use next stan rather than original stan for 420 so no need to increment terminal stan here
	  // RDD 041112 - changed back to using original stan for all reversals so increment here so this stan can't be reused except by a repeat.
	  terminal_increment_stan(); 
  }	  
  // RDD 241012 Bugfix to bug found by BD - need to save send time passed in txn struct as this may not have been saved 
  // when the reversal was originally saved
  transaction->send_time = send_time;

  // DBG_MESSAGE("%%%%%%%%%%%% SET REVERSAL %%%%%%%%%%%%%%%");
  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize(transaction, record_buffer, &record_length, sizeof(record_buffer)) );	  
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set(reversal_file, record_buffer, record_length) ); 
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR reversal_clear( VIM_BOOL fAddToSaf )
{
	VIM_BOOL exist;

	DBG_MESSAGE("CLEAR REVERSAL");

	// RDD 241111 - need to keep aspproved reversals in the SAF file for later printing
   
	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( reversal_file, &exist ));

	if( exist )
	{
		DBG_MESSAGE( "Deleting reversal file" );

		// RDD 281112 SPIRA:6402 Need to increment stan once a reversal has been sucessfully sent, otherwise the following message may have the same stan
		//terminal_increment_stan();

		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(reversal_file));

		// RDD 051211 - If a reversal was sucessfully sent then we add it to the SAF for prinitng later

		if( fAddToSaf )
		{
			transaction_set_status( &reversal, st_void );
			transaction_set_status( &reversal, st_message_sent );
			transaction_clear_status( &reversal, st_needs_sending );

			DBG_MESSAGE( "Adding Reversal to SAF file" );
			VIM_ERROR_RETURN_ON_FAIL( saf_delete_record( &reversal, VIM_FALSE ) );
			saf_totals.fields.saf_print_count += 1;
		}
	}
	 
    terminal.reversal_count = 0;  
	return VIM_ERROR_NONE;
}

#endif




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

	vim_rtc_get_time( &reversal.send_time );

	if( ShowDisplay )
	{
		display_screen( DISP_SENDING_REVERSAL, VIM_PCEFTPOS_KEY_NONE, 0 );
	}

	// RDD 270712 TODO (maybe in phase 3) SPIRA:5799 Set repeat after sucessful connection to host 
	// RDD 050412 - Set the repeat just before we send the reversal in case of power fail before the PinPad times out
	transaction_set_status( &reversal, st_repeat_saf );

	// Update the reversal file with the send time and repeat flags
	reversal_set( &reversal );

	// RDD 170113 SPIRA:6505 Allow interupts for Reversals and repeat SAFs
	// RDD 250113 - need to move this error to BEFORE the journal otherwise approved reversal jounals show with errors
	//if(( res = msgAS2805_transaction( HOST_WOW, &msg, msg_type, &reversal, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )
	if(( reversal.error = msgAS2805_transaction( HOST_WOW, &msg, msg_type, &reversal, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )
	{
		// reversal sent OK so delete the file
		reversal_clear( VIM_TRUE );	// Delete the reversal file but add the details to the SAF for printing

		// journal the reversal
		// RDD 210213 - SPIRA:6610 This was "journal type advice" until today !
		pceft_journal_txn( journal_type_reversal, &reversal );
		//comms_get_last_turnaround_time( &reversal.turnaround );
	}

	//reversal.error = res;	

	VIM_ERROR_RETURN_ON_FAIL( reversal.error );      
  } 
  // RDD Always clear reversal before SAFs we can't have a Reversal AND a repeat SAF at the same time
  else if(( saf_totals.fields.saf_count ) && ( saf_totals.fields.saf_repeat_pending ))
  {
	  // Send any repeat SAF
	  res = saf_send( VIM_TRUE, TRICKLE_FEED_COUNT, VIM_FALSE, VIM_FALSE );
  }  
  		
  //VIM_ERROR_RETURN_ON_FAIL(pceft_pos_display_close());

  return res;
}
