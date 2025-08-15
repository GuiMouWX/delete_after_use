
/**
 * \file saf.c
 *
 * Implements a simple FIFO batch
 *
 * Records are always read from the start, deleted from the start and added to the end.
 */


#include <incs.h>
VIM_DBG_SET_FILE("TP");
#define SAF_RECORD_COUNT (400)
#define PREAUTH_DAYS      7

extern VIM_SIZE TxnFileSize;
extern card_bin_name_item_t CardNameTable[];


typedef struct SAF_HEADER
{
  /* if the head and tail record are equal then the queue is empty*/
  /* index of the last record written */
  VIM_SIZE head_index;
  /* index of the first record written */
  VIM_SIZE tail_index;
  /* size of records */
  VIM_SIZE record_size;
  /* number of records in the table */
  VIM_SIZE record_count;  
}SAF_HEADER;

typedef struct SAF_RECORD
{
  VIM_UINT8 data[WOW_MAX_TXN_SIZE];
  VIM_SIZE size;
}SAF_RECORD;



PREAUTH_SAF_TABLE_T preauth_saf_table;

/*----------------------------------------------------------------------------*/
static VIM_TEXT const preauth_batch[] = VIM_FILE_PATH_LOCAL "pasaf";
static VIM_TEXT const preauth_table[] = VIM_FILE_PATH_LOCAL "patable";

static VIM_TEXT const query_card_batch[] = VIM_FILE_PATH_LOCAL "queryfile";
static VIM_TEXT const saf_file_batch[] = VIM_FILE_PATH_LOCAL "eftsaf";
static VIM_TEXT const saf_totals_file[] = VIM_FILE_PATH_LOCAL "saftot";

/*----------------------------------------------------------------------------*/
/**
 * Delete the entire batch file. For use in testing only
 */
VIM_ERROR_PTR pa_destroy_batch( void )
{
	VIM_ERROR_IGNORE( vim_file_delete( preauth_batch ));	
	VIM_ERROR_RETURN_ON_FAIL( vim_file_delete( preauth_table ));	
	vim_mem_clear( &preauth_saf_table, VIM_SIZEOF(preauth_saf_table) );		
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR roll_batch_key(void)
{
	VIM_ERROR_PTR res = vim_tcu_generate_new_batch_key();
	if (res != VIM_ERROR_NONE )
	{
		pceft_debug(pceftpos_handle.instance, "Gen new Batch Key failed");
	}
	else
	{
		pceft_debug(pceftpos_handle.instance, "Gen new Batch Key OK");
	}
	VIM_ERROR_RETURN( res );
}



VIM_ERROR_PTR saf_destroy_batch( void )
{
  VIM_CHAR Buffer[100];
  VIM_BOOL IsPresent = VIM_FALSE;

  // RDD 061216 v561-5 Only send these messages if there is a batch to delete
  vim_file_exists( saf_file_batch, &IsPresent );

  if( IsPresent == VIM_FALSE )
	  return VIM_ERROR_NONE;

  //if (!TERM_USE_PCI_TOKENS && !TERM_CUTOVER )
  //{
  //    return VIM_ERROR_NONE;
  //}

  vim_sprintf( Buffer, "BATCH DELETE CALLED: Deleting %4.4d SAF, %4.4d SAF Print txns", saf_totals.fields.saf_count, saf_totals.fields.saf_print_count );
  pceft_debug( pceftpos_handle.instance, Buffer );

  // RDD 160713 SPIRA:6775 - Need to delete reversals when clearing batch.
  // RDD 290515 SPIRA:8672 This is really dumb - need to do it from config menu instead
  //reversal_clear(VIM_FALSE);

  // RDD 301114 v538-3 : Brian says NEVER delete the Pre-auth Batch because there may be uncompleted pre-auths to keep across app changes
#if 0
  if( is_standalone_mode( ) )
  {
	  vim_sprintf( Buffer, "Deleting Preauth Table and PreAuth SAF files" );  	
	  pceft_debug( pceftpos_handle.instance, Buffer );
	  VIM_ERROR_IGNORE( vim_file_delete( preauth_table ));
	  VIM_ERROR_IGNORE( vim_file_delete( preauth_batch ));
	  vim_mem_clear( &preauth_saf_table, VIM_SIZEOF(preauth_saf_table) );	
  }
#endif

  DBG_MESSAGE( " !!!! LAST SAF RECORD DELETED : DELETE FILE !!!! " );

  VIM_ERROR_IGNORE( vim_file_delete(saf_file_batch) );

  // RDD 080618 JIRA WWBP-112 - PCI encrypt card data - ensure SAF deleted before changing key !!
  // VN JIRA WWBP-333 Saf tran caused due to base losing power - causes trickle feed process to stop funtioning

  // RDD 010519 JIRA WWBP-611 ensure that key is not rolled unless card buffer not present
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( query_card_batch, &IsPresent ));

  terminal.efb_total_amount = 0;
  reset_saf_totals();

  if( !IsPresent )
  {
	  VIM_BOOL PAPresent = VIM_FALSE;
	  vim_file_exists(preauth_batch, &PAPresent);

	  // RDD 310421 - don't roll the SAF encryption key if theres any pre-auths present
	  if (PAPresent)
		  return VIM_ERROR_NONE;

	  VIM_ERROR_RETURN_ON_FAIL( roll_batch_key());
  }
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR card_destroy_batch(void)
{
	DBG_MESSAGE( " !!!! LAST CARD RECORD DELETED : DELETE FILE !!!! " );
  VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(query_card_batch));
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/**
 * Return the number of records in the pre_auth batch
 */
VIM_UINT16 get_preauth_table_count(void)
{
	VIM_SIZE PendingCount;
	VIM_SIZE PendingTotal;

	preauth_get_pending( &PendingCount, &PendingTotal, VIM_FALSE );				
  
	return PendingCount;
}


/*----------------------------------------------------------------------------*/
/**
 * Check if there is a place to store new pre_auth transaction
 */
VIM_BOOL pre_auth_batch_full(void)
{
#if 1
	VIM_SIZE PendingCount=0, PendingTotal=0;	
	preauth_get_pending( &PendingCount, &PendingTotal, VIM_FALSE );	
	pceft_debugf_test(pceftpos_handle.instance, "Approved PreAuth Pending=%d Batchsize=%d", PendingCount, PREAUTH_SAF_RECORD_COUNT);
	return ( PendingCount >= PREAUTH_SAF_RECORD_COUNT) ? VIM_TRUE : VIM_FALSE;

#else
	VIM_DBG_NUMBER( get_preauth_table_count() );
//	VIM_DBG_NUMBER( PREAUTH_SAF_RECORD_COUNT );  
	// RDD v542-4 Set Preauth Batch size
	//return (get_preauth_table_count() >= terminal.configuration.efb_max_txn_count );
	return (get_preauth_table_count() >= terminal.configuration.PreauthBatchSize );
#endif
}


/*----------------------------------------------------------------------------*/
/**
 * Delete the pre_auth batch file
 */
VIM_ERROR_PTR preauth_batch_delete(void)
{
  VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(preauth_batch));
  
  vim_mem_clear(&preauth_saf_table, VIM_SIZEOF(preauth_saf_table));
  
  return VIM_ERROR_NONE;
}

#ifdef _WIN32
#define STORE_TIME_SHORT ( 600 )	// RDD - 10 mins for testing
#else
#define STORE_TIME_SHORT ( 3600 )	// RDD - 10 mins for testing
#endif

#define STORE_TIME ( 3600*24*7 )	// RDD - 30 days as stipulated by mastercard for chargeback protection



// RDD 280814 - ALH determine whether a PreAuth has expired or not rules say allow one week

VIM_BOOL preauth_expired( VIM_RTC_TIME *SavedTime )
{
	VIM_RTC_TIME time; 
	VIM_RTC_UNIX current_unix_time, txn_unix_time, elapsed_seconds;

	// Get the current time and convert to secs since 1970
	vim_rtc_get_time( &time );	  
	vim_rtc_convert_time_to_unix( &time, &current_unix_time );

	// Then convert the txn time to secs since 1970
	vim_rtc_convert_time_to_unix( SavedTime, &txn_unix_time );

	// Get the elapsed time since the txn was stored (in seconds)
	elapsed_seconds = current_unix_time - txn_unix_time;

	//DBG_VAR( elapsed_seconds );
	if (XXX_MODE)
	{
		return  (elapsed_seconds > STORE_TIME_SHORT) ? VIM_TRUE : VIM_FALSE;
	}
	else
	{
		return  (elapsed_seconds > STORE_TIME) ? VIM_TRUE : VIM_FALSE;
	}
}



VIM_ERROR_PTR preauth_save_table( PREAUTH_SAF_TABLE_T *sTablePtr )
{
	U_PREAUTH_SAF_TABLE_T *uTablePtr = ( U_PREAUTH_SAF_TABLE_T *)sTablePtr;
	VIM_SIZE n;
	VIM_SIZE Valid = 0;

	for( n=0; n<sTablePtr->records_count; n++ )
	{
		// RDD 011214 - if any preauth hasn't expired AND hasn't been checked out then don't delete the batch
		if(( !preauth_expired( &sTablePtr->details[n].time )) && ((sTablePtr->details[n].status & st_checked_out) == VIM_FALSE ))
		{
			Valid = VIM_TRUE;
			break;
		}
	}

	if (!Valid)
	{
		pa_destroy_batch();
	}

	else
	{
		// RDD 160115 SPIRA:8338 PreAuth disappears after a reboot
		vim_file_set( preauth_table, (const void *)&uTablePtr->ByteBuff, VIM_SIZEOF(PREAUTH_SAF_TABLE_T));
	}
	return  VIM_ERROR_NONE;
}


VIM_ERROR_PTR preauth_load_table( void )
{
	VIM_BOOL exists = VIM_FALSE; 
	VIM_SIZE Length = VIM_SIZEOF( preauth_saf_table );
	U_PREAUTH_SAF_TABLE_T *uTablePtr = (U_PREAUTH_SAF_TABLE_T *)&preauth_saf_table;
	vim_file_exists( preauth_table, &exists );
	if( !exists ) 
		return VIM_ERROR_NOT_FOUND;

	vim_mem_clear( &preauth_saf_table, Length );
	
	return( vim_file_get( preauth_table, (void *)&uTablePtr->ByteBuff, &Length, Length ));
}


void preauth_table_update_status ( VIM_SIZE index, txn_status_t flag )
{
	preauth_saf_table.details[index].status &= flag;
	preauth_save_table( &preauth_saf_table );
}


// Determines whether a given Preauth in the table is still valid

VIM_BOOL preauth_pending( PREAUTH_SAF_DETAILS *PreauthDetailsPtr )
{
	if( preauth_expired( &PreauthDetailsPtr->time ))		
		return VIM_FALSE;
	
	if( PreauthDetailsPtr->status & st_checked_out )
		return VIM_FALSE;

	return VIM_TRUE;
}



VIM_ERROR_PTR preauth_get_pending( VIM_SIZE *PendingCount, VIM_SIZE *PendingTotal, VIM_BOOL GetTotal )
{
	PREAUTH_SAF_DETAILS *PreauthDetailsPtr;
	VIM_SIZE Index;
	VIM_SIZE Count = 0;
	// RDD 010515 SPIRA:8611 Problem with Preauth Batch accross software change due to change in format so 
	// change back to original format (in pilot) and read full txn back to get amount instead
	transaction_t txn_temp;

	*PendingTotal = 0;

	preauth_load_table( );
	PreauthDetailsPtr = &preauth_saf_table.details[0];

	for( Index = 0; Index < preauth_saf_table.records_count; Index++, PreauthDetailsPtr++ )
	{
		if( preauth_pending( PreauthDetailsPtr ))
		{
			Count++;

			// RDD - make this optional as could be slow in large batches
			if( GetTotal )
			{
				if( pa_read_record( PreauthDetailsPtr->roc, &txn_temp ) == VIM_ERROR_NONE )
				{
					*PendingTotal += txn_temp.amount_total;
				}
			}
		}
	}
	*PendingCount = Count;
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
/**
 * Add a new record to the pre_auth table
 */
VIM_BOOL preauth_table_add_record(transaction_t *transaction)
{
  if (pre_auth_batch_full())
  {
    VIM_DBG_MESSAGE("FULL TABLE");
    return VIM_FALSE;
  }

  /* add transaction details to the pre_auth table */
  preauth_saf_table.details[preauth_saf_table.records_count].roc = transaction->roc;

  preauth_saf_table.details[preauth_saf_table.records_count].roc_index = transaction->roc_index;

  preauth_saf_table.details[preauth_saf_table.records_count].status = transaction->status;

  vim_mem_copy(preauth_saf_table.details[preauth_saf_table.records_count].auth_no,
                transaction->auth_no, VIM_SIZEOF(preauth_saf_table.details[0].auth_no));

  preauth_saf_table.details[preauth_saf_table.records_count].amount = transaction->amount_total;

  preauth_saf_table.details[preauth_saf_table.records_count].time = transaction->time;

  preauth_saf_table.details[preauth_saf_table.records_count].index = preauth_saf_table.records_count + 1;
  preauth_saf_table.records_count++;

  // RDD 180814 added for ALH
  preauth_save_table( &preauth_saf_table );

  // RDD 200814 added - increment the dumb roc after saving a new pre-auth 
  //terminal_increment_roc();

  return VIM_TRUE;
}

/*----------------------------------------------------------------------------*/
/**
 * Add a new record to the pre_auth table
 */
VIM_BOOL preauth_table_update_record(VIM_SIZE index, transaction_t *transaction)
{

#if 0
  if (pre_auth_batch_full())
  {
    VIM_DBG_MESSAGE("FULL TABLE");
    return VIM_FALSE;
  }
#endif

  if (index >= PREAUTH_SAF_RECORD_COUNT)
  {
    VIM_DBG_MESSAGE("WRONG RECORD NUMBER");
    return VIM_FALSE;
  }

  /* update transaction details */
  preauth_saf_table.details[index].roc = transaction->roc;

  preauth_saf_table.details[index].roc_index = transaction->roc_index;

  preauth_saf_table.details[index].status = transaction->status;

  preauth_saf_table.details[index].amount = transaction->amount_total;

  vim_mem_copy(preauth_saf_table.details[index].auth_no, transaction->auth_no,
              VIM_SIZEOF(preauth_saf_table.details[0].auth_no));

  preauth_saf_table.details[index].time = transaction->time;

  // RDD 180814 added for ALH
  preauth_save_table( &preauth_saf_table );

  return VIM_TRUE;
}



VIM_ERROR_PTR preauth_table_get_record(PREAUTH_SAF_DETAILS **PreauthDetailsPtr, VIM_SIZE roc)
{
    VIM_SIZE Index;
    //VIM_SIZE Count = 0;
    PREAUTH_SAF_DETAILS *RecordPtr = &preauth_saf_table.details[0];

    preauth_load_table();
    
    for (Index = 0; Index < preauth_saf_table.records_count; Index++, RecordPtr++)
    {
        if( roc == RecordPtr->roc )
        {
            *PreauthDetailsPtr = RecordPtr;
            return VIM_ERROR_NONE;
        }
    }
    VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
}



VIM_BOOL saf_full( VIM_BOOL CheckLimit )
{
   // VIM_DBG_MESSAGE("Check SAF Full");

#ifdef _DEBUG
    vim_event_sleep(100);
#endif
    if( saf_totals.fields.saf_count >= TERM_MAX_SAFS )
    {
        return VIM_TRUE;
    }

    if (CheckLimit)
    {
        DBG_POINT;
        if(( saf_totals.fields.DebitAmountNS + txn.amount_total > ( 100*TERM_SAF_TOTAL_LIMIT )) && ( TERM_SAF_TOTAL_LIMIT > 0 ))
        {
            return VIM_TRUE;
        }
    }
    return VIM_FALSE;
}




VIM_ERROR_PTR retrieve_saf_totals( void )
{
    VIM_BOOL exist;
    VIM_TEXT const *file_name = saf_totals_file;

	/* check if the SAF exists */  
	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( file_name, &exist ) );

    VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( file_name, saf_totals.bytebuff, VIM_SIZEOF(saf_totals.bytebuff), 0 ));

	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR save_saf_totals( void )
{
  VIM_BOOL exist = VIM_FALSE;
  VIM_TEXT const *file_name = saf_totals_file;

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( file_name, &exist ) );

  // Create a new totals file each time we add to the totals
  if (exist)
  {
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_delete( file_name ) );
  }
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set( file_name, saf_totals.bytebuff, VIM_SIZEOF(u_saf_totals_t) ) );
  return VIM_ERROR_NONE;
}



// RDD 111215 SPIRA:8873 Reset SAF Totals when all SAFs have been uploaded
VIM_ERROR_PTR reset_saf_totals( void )
{
    vim_mem_clear( saf_totals.bytebuff, VIM_SIZEOF( saf_totals.bytebuff ) );
	save_saf_totals( );
	return VIM_ERROR_NONE;
}




VIM_ERROR_PTR saf_update_status_file(transaction_t *transaction, VIM_BOOL SafWasSentOK )
{
	if( SafWasSentOK )
	{
		if( is_approved_txn(transaction) )
		{
			if( transaction->type == tt_refund || transaction->type == tt_deposit )
			{
				if( saf_totals.fields.CreditAmountNS >= transaction->amount_total )
				{					
					saf_totals.fields.CreditAmountNS -= transaction->amount_total;
					saf_totals.fields.CreditCountNS --;
				}
			}
			else
			{
				if( saf_totals.fields.DebitAmountNS >= transaction->amount_total )
				{
					saf_totals.fields.DebitAmountNS -= transaction->amount_total;
					saf_totals.fields.DebitCountNS --;
				}
			}
		}
		else
		{
			if( transaction->type == tt_refund || transaction->type == tt_deposit )
			{

				if( saf_totals.fields.CreditAmountDECL >= transaction->amount_total )
				{
					saf_totals.fields.CreditAmountDECL -= transaction->amount_total;
					saf_totals.fields.CreditCountDECL --;
				}
			}
			else
			{
				if( saf_totals.fields.DebitAmountDECL >= transaction->amount_total )
				{
					saf_totals.fields.DebitAmountDECL -= transaction->amount_total;
					saf_totals.fields.DebitCountDECL --;
				}
			}
		}
	}
	else
	{
		if( is_approved_txn(transaction) )
		{
			if( transaction->type == tt_refund || transaction->type == tt_deposit )
			{
				saf_totals.fields.CreditAmountNS += transaction->amount_total;
				saf_totals.fields.CreditCountNS ++;
			}
			else if( transaction->amount_total )
			{
				saf_totals.fields.DebitAmountNS += transaction->amount_total;
				saf_totals.fields.DebitCountNS ++;
			}
		}
		else
		{
			if( transaction->type == tt_refund || transaction->type == tt_deposit )
			{
				saf_totals.fields.CreditAmountDECL += transaction->amount_total;
				saf_totals.fields.CreditCountDECL ++;
			}
			else if( transaction->amount_total )
			{
				saf_totals.fields.DebitAmountDECL += transaction->amount_total;
				saf_totals.fields.DebitCountDECL ++;
			}
		}
	}
	save_saf_totals();
	return VIM_ERROR_NONE;
}

// RDD 010515 Create or Add a transaction record to a generic file
VIM_ERROR_PTR saf_add_gen_record( transaction_t *transaction, VIM_TEXT const *file_name, VIM_SIZE MaxRecords )
{
  VIM_BOOL exist;
  SAF_HEADER header;
  SAF_RECORD record;

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( file_name, &exist ));

  if( !exist )
  {
	  header.head_index=0;    
	  header.tail_index=0;   
	  header.record_size=VIM_SIZEOF(record);	
	  header.record_count = MaxRecords;	    
	  /* create an empty file */    
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_set( file_name, &header, VIM_SIZEOF(header) ));
  }
  else
  {    
	  /* retreive the queue header from the file */    
	  VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(file_name,&header,VIM_SIZEOF(header),0));
  }

  //VIM_DBG_VAR(header);  
  vim_mem_clear( &record, VIM_SIZEOF(record) );

  transaction->roc_index = header.head_index;
    
  // RDD V419 161013: SPIRA:6710 - clear the label found record for SAF txns as this can screw up the display
  transaction_clear_status( transaction, st_label_found );

  /* convert transaction structure to TLV */
  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize( transaction, record.data, &record.size, VIM_MIN(VIM_SIZEOF(record.data), header.record_size) ));
  
  /* save the record to file */  	  	
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &record, header.record_size, (header.record_size*header.head_index)+VIM_SIZEOF(header))); 	
  header.head_index += 1;	// RDD 120112 changed from above as this was causing problems uploading SAF when full as head index wrapped to zero.	

  /* update the header*/	
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &header, VIM_SIZEOF(header), 0) );
  return VIM_ERROR_NONE;
}




/**
 * Add a record to the end of the batch.
 *
 * \param[in] transaction
 * transaction to add to the batch
 *
 * \return
 * ERR_FILE_SYSTEM_ERROR - failed to add the transaction to batch.
 * VIM_ERROR_NONE - transaction added to batch
 */
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR saf_add_record(transaction_t *transaction, VIM_BOOL QueryCard )
{
  VIM_BOOL exist = VIM_FALSE;
  SAF_HEADER header;
  SAF_RECORD record;
  VIM_TEXT const *file_name;

  file_name = ( QueryCard ) ? query_card_batch : saf_file_batch;

  if( tt_preauth == transaction->type )
  {    
	  file_name = preauth_batch;
	  // RDD SPIRA:8611 290415 added 
	  if( pre_auth_batch_full() )
	  {
		  pceft_debug_test(pceftpos_handle.instance, "Preath SAF Batch full"); 
		  // RDD 270112 : ensure that this transaction is NOT stored for sending or treated as EFB during PDR
		  transaction_clear_status( transaction, st_needs_sending );		
		  transaction_clear_status( transaction, st_efb );		
		  txn.error = VIM_ERROR_SAF_FULL;
		  pceft_debug_test(pceftpos_handle.instance, "Approved Preauth SAF Overflow");
		  VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
	  }
	  pceft_debug_test(pceftpos_handle.instance, "Preath SAF Batch not full");
  }
  /* make sure there is space in the file to store the record  - for now allow unlimited records in the QueryCard file */
  else if(( saf_full(VIM_TRUE) ) && ( !QueryCard ))
  {
	  // RDD 270112 : ensure that this transaction is NOT stored for sending or treated as EFB during PDR
	  transaction_clear_status( transaction, st_needs_sending );
	  transaction_clear_status( transaction, st_efb );
	  txn.error = VIM_ERROR_SAF_FULL;
      pceft_debug(pceftpos_handle.instance, "SAF Full");
	  VIM_ERROR_RETURN( txn.error );
  }

  /* check if the SAF exists */
  VIM_ERROR_IGNORE( vim_file_exists( file_name, &exist ));

  if( !exist )
  {
	  pceft_debug_test(pceftpos_handle.instance, "Create Preauth SAF Batch");
	  header.head_index=0;
	  header.tail_index=0;   
	  header.record_size=VIM_SIZEOF(record);
	
	  // RDD - SAF limit set in WOW CPAT trailer: make this apply to buffer to for now...
	  header.record_count = ( transaction->type == tt_preauth ) ? PREAUTH_SAF_RECORD_COUNT : terminal.configuration.efb_max_txn_count;	
    
	  // RDD 120618 WW BP-112 Encrypt/Decrypt Card Data for PCI compliance
	  // Tell TCU to create and securely store a random key for use with this batch

	  /* create an empty SAF */    
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &header, VIM_SIZEOF(header), 0 ));
	  pceft_debug_test(pceftpos_handle.instance, "Created Preauth SAF Batch OK");
  }
  else
  {
     //VIM_DBG_MESSAGE("Adding to existing batch");
    /* retreive the queue header from the file */
	  pceft_debug_test(pceftpos_handle.instance, "Read Preauth SAF Batch");
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes(file_name,&header,VIM_SIZEOF(header),0 ));
	  pceft_debug_test(pceftpos_handle.instance, "Read Preauth SAF Batch OK");
  }

  //VIM_DBG_VAR(header);
  
  /* make sure there is space in the file to store the record */
  //if (((header.head_index+1)%header.record_count)==header.tail_index)
  // RDD 010415 This is already done in saf_full()
#if 0
  if( header.head_index >= terminal.configuration.efb_max_txn_count )
  {    
	  VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
  }
#endif

  vim_mem_clear( &record, VIM_SIZEOF(record) );

  //VIM_DBG_VAR(header);    
  /* get the RRN index */
  if( transaction->type == tt_preauth )
  {
	  // Preauth file is never likely to be deleted because we're never likely to have zero active records.
	  // Make the preauth batch a circular file, ie. once it's full we start from the beginning again. Should be fine if the max txns is large enough ( ie > 100 or so )
	  transaction->roc_index = ( header.head_index % header.record_count );
      // RDD 240621 added as seems to be missing and is needed for recall.
      transaction->roc = asc_to_bin(transaction->rrn, VIM_SIZEOF(transaction->rrn));
	  pceft_debugf_test(pceftpos_handle.instance, "Preauth ROC=%d roc_index=%d", transaction->roc, transaction->roc_index);
  }
  else
  {	
	  transaction->roc_index = header.head_index;
  }

  // RDD 151112 SPIRA:6295 Stored completions were going up with wrong proc code due to this line of code. Need to modify Mini history
  // to change the transaction type for the completions instead....
#if 0
  // RDD 230212 - ensure that the original type is backed up as this will become a mini-history when recalled for SAF printing
  transaction->original_type = transaction->type;
#endif
    
  // RDD V419 161013: SPIRA:6710 - clear the label found record for SAF txns as this can screw up the display
  transaction_clear_status( transaction, st_label_found );

  /* convert transaction structure to TLV */
  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize( transaction, record.data, &record.size, VIM_MIN(VIM_SIZEOF(record.data), header.record_size) ));
  pceft_debug_test(pceftpos_handle.instance, "Serialised OK");

  // RDD 010415 Write the record to the offset position
  if(1)
  {
	  pceft_debugf_test(pceftpos_handle.instance, "Adding %d bytes to %s", header.record_size, file_name);
	  /* save the record to file */
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &record, header.record_size, (header.record_size*header.head_index)+VIM_SIZEOF(header))); 
	  pceft_debugf_test(pceftpos_handle.instance, "%d bytes Added to %s", header.record_size, file_name );
	  //VIM_DBG_VAR(header);
  }

  header.head_index += 1;	// RDD 120112 changed from above as this was causing problems uploading SAF when full as head index wrapped to zero.
  VIM_DBG_NUMBER(header.head_index);
  DBG_MESSAGE( file_name );

  /* update the header*/
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &header, VIM_SIZEOF(header), 0) );
  pceft_debug_test(pceftpos_handle.instance, "Header updated");

	if( QueryCard )
	{
      terminal.buffer_count++;
        VIM_DBG_PRINTF1( "Query Card Buffer Index = %d", header.head_index );        
        VIM_DBG_DATA(transaction->card.pci_data.ePan, E_PAN_MAX_LEN );		
        VIM_DBG_MESSAGE("<<<<<<<< CARD DATA ADDED TO CARD BUFF >>>>>>>>>");
      return VIM_ERROR_NONE;
	}
	
	else if( transaction->type == tt_preauth )
	{
      if( preauth_table_add_record(transaction))
      {
		  pceft_debug_test(pceftpos_handle.instance, "<<<<<<<< PRE_AUTH ADDED TO PRE_AUTH TABLE >>>>>>>>>");
	  }
	  else
	  {
		  pceft_debug_test(pceftpos_handle.instance, "<<<<<<<< PRE_AUTH NOT ADDED TO PRE_AUTH TABLE >>>>>>>>>");
	  }
      return VIM_ERROR_NONE;	
	}
	else	
	{    	  
      if ((transaction_get_status(transaction, st_needs_sending)) && (!QueryCard))		
	  {
		// //VIM_DBG_MESSAGE("SAF/PRINT += 1");
		saf_totals.fields.saf_count++;
		// RDD 111215 SPIRA:8873 - 	Saf was sent OK so add it to un-sent SAF totals
		saf_update_status_file( transaction, VIM_FALSE );

	          //if (!terminal_get_status(ss_no_saf_prints))
		{
			saf_totals.fields.saf_print_count++;
		}
		if ( transaction_get_status( transaction, st_efb ) )
		{      
	        saf_totals.fields.efb_count++;
	    }	
	  }	
	}  
	return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/**
 * Read the record at specified index from the batch.
 *
 * \param[in] transaction
 * transaction structure to populate
 *
 * \return
 * ERR_RECORD_DOESNT_EXIST - No record exists at specified index
 * VIM_ERROR_NONE - transaction retrieved successfully
 */
VIM_ERROR_PTR txn_read_record_index(VIM_SIZE index, VIM_BOOL QueryCard, transaction_t *transaction)
{
  SAF_RECORD record;
  VIM_BOOL exist;
  SAF_HEADER header;
  VIM_SIZE record_index;
  VIM_TEXT const *file_name;

//  // DBG_MESSAGE( "in txn_read_record_index" );
    
  file_name = (QueryCard) ? query_card_batch : saf_file_batch;
//  // DBG_VAR(index);

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(file_name,&exist));

  if (!exist)
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST)
  }
  else
  {
      /* retreive the queue header from the file */
	  VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes( file_name, &header, VIM_SIZEOF(header), 0 ));
  }
#if 0
  // DBG_VAR(header);

  // DBG_VAR(header.record_size);
  // DBG_VAR(header.head_index);
  // DBG_VAR(header.record_count);
  // DBG_VAR(header.tail_index);
#endif
  if (header.tail_index==header.head_index)
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }
  
  /* make sure the index into the queue exists */
  if (header.tail_index+index>=header.head_index+header.record_count)
  {
    VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }

  record_index=(header.tail_index+index);
  
  /* make sure the record size will fit in the record structure */
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(header.record_size,VIM_SIZEOF(record));

  // Read a whole record ( max bytes )
  VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( file_name, &record.data, header.record_size, ( header.record_size*record_index )+VIM_SIZEOF( header )));
  //  // DBG_VAR(header.record_size);
//  // DBG_VAR(record.size);
  /* convert TLV to transaction structure */
  // 
#if 0
  VIM_ERROR_RETURN_ON_FAIL( transaction_deserialize(transaction, record.data, header.record_size, VIM_TRUE ));
#else
  // RDD 300417 Above had bug in that we should only deserialise the actual TLV record size not the max size of the record as that may override valid data
  VIM_ERROR_RETURN_ON_FAIL( transaction_deserialize( transaction, record.data, record.size, VIM_TRUE ));
#endif

  	  //DBG_DATA( txn.emv_data.buffer, txn.emv_data.data_size );

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/**
 * Update the first record in the batch
 *
 * \param[in] transaction
 * transaction details to overwrite first record with
 *
 * \return
 * ERR_FILE_SYSTEM_ERROR - failed to add the transaction to batch.
 * VIM_ERROR_NONE - transaction added to batch
 */

VIM_ERROR_PTR saf_update_record( VIM_SIZE index, VIM_BOOL QueryCard, transaction_t *transaction )
{
  VIM_SIZE record_index;
  VIM_BOOL exist;
  SAF_HEADER header;
  SAF_RECORD record;
  VIM_TEXT const *file_name;

  file_name = (QueryCard) ? query_card_batch : saf_file_batch;

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(file_name,&exist));

  if (!exist)
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST)
  }
  else
  {
	  /* retreive the queue header from the file */
	  VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(file_name,&header,VIM_SIZEOF(header),0));
  }

  if( header.tail_index==header.head_index )
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }
  
  /* make sure the index into the queue exists */
  if (header.tail_index+index>=header.head_index+header.record_count)
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }

  record_index=(header.tail_index+index);

  vim_mem_clear(&record,VIM_SIZEOF(record));

  /* convert transaction structure to TLV */
  VIM_ERROR_RETURN_ON_FAIL( transaction_serialize( transaction, record.data, &record.size, VIM_MIN(VIM_SIZEOF(record), header.record_size )));

  /* increment the head index */
  header.head_index++;

  /* save the record to file */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( file_name, &record, header.record_size, header.record_size*record_index+VIM_SIZEOF(header) ));

   //VIM_DBG_MESSAGE("UPDATED  SAF");
  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/**
 * Count number of items in batch. Try not to call too often as it has to scan entire batch to count this.
 *
 * \return
 * Number of items in batch
 */

VIM_ERROR_PTR txn_count(  VIM_SIZE* count_ptr, VIM_BOOL QueryCard )
{
  VIM_BOOL exist;
  SAF_HEADER header;    
  VIM_TEXT const *file_name;
    
  *count_ptr=0;

  file_name = (QueryCard) ? query_card_batch : saf_file_batch;

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(file_name,&exist));

  if (exist)
  {
	/* retreive the queue header from the file */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(file_name,&header,VIM_SIZEOF(header),0));
    /* compute the number of records in the SAF */
    *count_ptr=(header.head_index-header.tail_index);
  }
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* delete offline transaction only if the advice has been printed */
VIM_ERROR_PTR saf_delete_record(VIM_SIZE index, transaction_t *tran, VIM_BOOL QueryCard )
{
  
   //VIM_DBG_MESSAGE("<<<< DELETING SAF RECORD... >>>>>");

  // RDD 050312 : ensure that the transaciton is not already deleted as deleting it again will stuff the counters
  if( tran->status & st_deleted_offline ) return VIM_ERROR_NONE;

  /* clear need sending and efb status */
  transaction_clear_status(tran, st_needs_sending + st_efb);

  /* set deleted status */
  transaction_set_status(tran, st_deleted_offline);

  if (0xFFFFFFFF != tran->original_index)
  {
    transaction_t orig_tran;
    
    /* read original record */
    if( VIM_ERROR_NONE == txn_read_record_index( tran->original_index, QueryCard, &orig_tran ))
    {
      /* clear index of the updated transaction in the original record */
      orig_tran.update_index = 0xFFFFFFFF;

      /* allow to read original transaction */
      orig_tran.read_only = VIM_FALSE;

      /* update original record */
      saf_update_record( tran->original_index, QueryCard, &orig_tran );
    }

    /* clear index of the original transaction */
    tran->original_index = 0xFFFFFFFF;

    /* do not read transaction */
    tran->read_only = VIM_TRUE;
  }

  /* update record */
  VIM_ERROR_RETURN_ON_FAIL( saf_update_record(index, QueryCard, tran));


  if( QueryCard )
  {
	  if( terminal.buffer_count ) 
		  terminal.buffer_count -= 1;
	  else
		  card_destroy_batch();
	 //VIM_DBG_VAR(terminal.buffer_count);
  }
  else
  {
	  /* RDD 050312 update saf counter */	
	  //if (saf_totals.fields.saf_count ) saf_totals.fields.saf_count--;

	  // RDD 130212: update both saf count and saf print count as this record is being deleted not updated
	  if( saf_totals.fields.saf_print_count ) 
		  saf_totals.fields.saf_print_count--;
	  if(( saf_totals.fields.saf_print_count == 0 ) && ( saf_totals.fields.saf_count == 0 ))
		saf_destroy_batch();
       //VIM_DBG_MESSAGE("<<<< DELETE SAF UPDATED COUNT: >>>>>");
	 //VIM_DBG_VAR(saf_totals.fields.saf_print_count);
	 //VIM_DBG_VAR(saf_totals.fields.saf_count);
	  
	  /* update efb counter */
	  if (transaction_get_status(tran, st_efb))
	  {
		  if (saf_totals.fields.efb_count > 0)
			saf_totals.fields.efb_count--;
	  }
  }
	
  // Save terminal data as we updated the struct
  VIM_ERROR_RETURN_ON_FAIL( terminal_save());

  return VIM_ERROR_NONE;
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR query_read_next_undeleted( transaction_t *transaction, VIM_SIZE *query_index )
{

  VIM_SIZE counter, index;

  // Count the total number of queries in the card buffer, both deleted and not
  VIM_ERROR_RETURN_ON_FAIL(txn_count(&counter, VIM_TRUE ));

  //DBG_VAR(counter);
  for( index = 0; index < counter; index++ )
  {
    // Read the next record
	vim_mem_clear( transaction, VIM_SIZEOF( transaction ));

    VIM_ERROR_RETURN_ON_FAIL( txn_read_record_index( index, VIM_TRUE, transaction ));
    
	// skip any deleted transactions
	if( transaction->status & st_deleted_offline )
		continue;
	else
	{
		*query_index = index;
		return VIM_ERROR_NONE;
	}
  }
  return VIM_ERROR_NOT_FOUND;
}


VIM_BOOL SafIsValidTxn( transaction_t *stored_txn )
{	
	error_t error_data;
  
	/* RDD 100413 P4 : Check the validity of the Error Code Ptr - sometimes it gets corrupted and points to garbage and causes a crash */  
	vim_mem_clear(&error_data, VIM_SIZEOF(error_data));  
	DBG_POINT;
#if 0
	if( terminal_error_lookup( stored_txn->error, &error_data, VIM_FALSE ) != VIM_ERROR_NONE )
	{		  
		pceft_debug( pceftpos_handle.instance, "This SAF was corrupted" );
		stored_txn->error = VIM_ERROR_NONE;
		stored_txn->error_backup = VIM_ERROR_NONE;
	}
#endif
	if(( stored_txn->amount_total ) && ( stored_txn->amount_total < WOW_TXN_LIMIT_TOTAL ))
	{
		switch( stored_txn->card.type )
		{
			case card_manual:
		    case card_manual_pos:
			case card_mag:
			case card_chip:
			case card_picc:
			case card_qr:
				return VIM_TRUE;

			default:
				break;
		}
	}
	return VIM_FALSE;
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR saf_read_next_by_status( transaction_t *transaction, txn_status_t test_status, VIM_SIZE *index, VIM_BOOL IncludeSafPrints )
{

  VIM_SIZE saf_counter;
  VIM_SIZE saf_index;
  transaction_t tmp_txn;

  VIM_ERROR_RETURN_ON_FAIL(txn_count(&saf_counter, VIM_FALSE ));

    DBG_VAR(saf_counter);
     

  for( saf_index = *index; saf_index < saf_counter; saf_index++ )
  {
	  // RDD 250815 Need to Initialise the pointers
	  tmp_txn.error = VIM_ERROR_NONE;  	  
	  tmp_txn.error_backup = VIM_ERROR_NONE;

	  // Read the next record		
	  vim_mem_clear( &tmp_txn, VIM_SIZEOF( tmp_txn ));
    
	  VIM_ERROR_RETURN_ON_FAIL( txn_read_record_index( saf_index, VIM_FALSE, &tmp_txn ));
    
	// skip any deleted transactions
	if( tmp_txn.status & st_deleted_offline )
		continue;

	// RDD 051212 SPIRA: 6416 added if instructed - skip any messages that have already been sent
	if( !IncludeSafPrints && ( tmp_txn.status & st_message_sent ))
		continue;

	if( tmp_txn.status & test_status )
	{
		*index = saf_index;
		// RDD 270812 - Some evidence of corrupt SAFs so add further check on txn.type
		//if( tmp_txn.amount_total ) 
		if( SafIsValidTxn(&tmp_txn) )
		{
			// The transaction is live, valid and matches the search critera - copy into the active structure
			vim_mem_copy( transaction, &tmp_txn, VIM_SIZEOF( transaction_t ) );
			DBG_MESSAGE( "<<<<<<<<<<<<<<<<<<<<<< READ SAF RECORD INTO STRUCT >>>>>>>>>>>>>>>>>>>>>>>");
			DBG_VAR( transaction->emv_data.data_size );

			return VIM_ERROR_NONE;
		}
		else
		{
			// RDD 190412 - corrupt record. This shouldn't really happen
			VIM_CHAR Buffer[100];
			pceft_debug(pceftpos_handle.instance, "DELETING INVALID SAF RECORD..");
			vim_sprintf( Buffer, "RRN:%s SAF COUNT %d OF %d", tmp_txn.rrn, saf_counter );
			pceft_debug( pceftpos_handle.instance, Buffer );
			saf_delete_record( *index, &tmp_txn, VIM_FALSE );
			
		}
	}
  }
  return VIM_ERROR_NOT_FOUND;
}



/*----------------------------------------------------------------------------*/
VIM_SIZE saf_pending_count( VIM_SIZE *SafCount, VIM_SIZE *SafPrintCount )
{

  VIM_SIZE saf_counter;
  VIM_SIZE saf_index;
  transaction_t tmp_txn;

  // Reset the Counters
  *SafCount = *SafPrintCount = 0;

  txn_count(&saf_counter, VIM_FALSE );

  // DBG_VAR(saf_counter);
  for( saf_index = 0; saf_index < saf_counter; saf_index++ )
  {
    // Read the next record
	vim_mem_clear( &tmp_txn, VIM_SIZEOF( tmp_txn ));

    txn_read_record_index( saf_index, VIM_FALSE, &tmp_txn );
    
	// skip any deleted transactions
	if( tmp_txn.status & st_deleted_offline )
	  continue;
	
	if( tmp_txn.status & st_needs_sending )
	{
		*SafCount += 1;
	}
	else if( tmp_txn.status & st_message_sent )
	{
		*SafPrintCount += 1;
	}
  }
  return *SafCount;
}



VIM_ERROR_PTR pa_read_record(VIM_UINT32 roc, transaction_t *transaction)
{
  SAF_RECORD record;
  VIM_BOOL exist;
  SAF_HEADER header;
  VIM_SIZE record_index;

  DBG_MESSAGE( "PA Read Record" );
  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( preauth_batch, &exist ));

  if (!exist)
  {
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST)
  }
  else
  {    
	  /* retreive the queue header from the file */    
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( preauth_batch, &header, VIM_SIZEOF(header), 0));
  }

  if( header.tail_index==header.head_index )
  {    
	  VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }
  
  // RDD 101014 ALH
  VIM_DBG_NUMBER( header.record_count );
  for( record_index = 0; record_index < header.record_count; record_index++ )
  {	
	  VIM_DBG_NUMBER( record_index );
	  /* make sure the record size will fit in the record structure */
	  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW( header.record_size, VIM_SIZEOF(record));
  
	  /* read the record */
	  VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( preauth_batch, &record,header.record_size, ( header.record_size*record_index)+VIM_SIZEOF(header)));
  
	  /* convert TLV to transaction structure */
	  VIM_ERROR_RETURN_ON_FAIL(transaction_deserialize( transaction, record.data, record.size, VIM_TRUE ));

	  if( transaction->roc == roc )		
	  {
		  VIM_DBG_PRINTF2( "Found Record RRN=%d AMT=%d", transaction->roc, transaction->amount_total );
          transaction->original_index = record_index;
		  return VIM_ERROR_NONE;
	  }
  }

  /* return without error */
  return VIM_ERROR_NOT_FOUND;
}





static VIM_ERROR_PTR saf_read_index(VIM_SIZE index, VIM_TEXT const *file_name, transaction_t *transaction)
{
  SAF_RECORD record;
  VIM_BOOL exist;
  SAF_HEADER header;
  VIM_SIZE record_index;

  /* check if the SAF exists */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( file_name, &exist ));

  if (!exist)
  {
    VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST)
  }
  else
  {
    /* retreive the queue header from the file */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(file_name,&header,VIM_SIZEOF(header),0));
  }

  if (header.tail_index==header.head_index)
  {
    VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }
  
  /* make sure the index into the queue exists */
  if (header.tail_index+index>=header.head_index+header.record_count)
  {
    VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);
  }

  // RDD 101014 ALH
  if( header.record_count )
	record_index=(header.tail_index+index)%header.record_count;
  else
    VIM_ERROR_RETURN(ERR_RECORD_DOESNT_EXIST);

  /* make sure the record size will fit in the record structure */
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(header.record_size,VIM_SIZEOF(record));
  /* read the record */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(file_name,&record,header.record_size,header.record_size*record_index+VIM_SIZEOF(header)));

  /* convert TLV to transaction structure */
  VIM_ERROR_RETURN_ON_FAIL(transaction_deserialize(transaction, record.data, record.size, VIM_TRUE ));

  /* return without error */
  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR saf_read_record_index(VIM_SIZE index, VIM_BOOL pre_auth, transaction_t *transaction)
{
  //SAF_RECORD record;
 // VIM_BOOL exist;
 // SAF_HEADER header;
 // VIM_SIZE record_index;
  VIM_TEXT const *file_name;

  if (pre_auth)
    file_name = preauth_batch;
  else
    file_name = saf_file_batch;

  VIM_ERROR_RETURN_ON_FAIL( saf_read_index( index, file_name, transaction ));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR saf_send_single( VIM_BOOL repeat_only, VIM_SIZE *index )
{
  VIM_ERROR_PTR res;
  VIM_AS2805_MESSAGE msg;
  VIM_AS2805_MESSAGE_TYPE msg_type;
  VIM_BOOL DoDisconnect = VIM_TRUE;

  /* Send SAF's for transmit_count */
  if ( !saf_totals.fields.saf_count )
  {
	  return VIM_ERROR_NOT_FOUND;
  }
   
  // RDD 010512 SPIRA:5406 Abort if No Repeats to send and repeat only ordered
  if( repeat_only )
  {
    VIM_ERROR_PTR error=VIM_ERROR_NONE;
	  // find the first repeat
	  error = saf_read_next_by_status( &txn_trickle, st_repeat_saf, index, VIM_FALSE );
    if( error != VIM_ERROR_NONE ) return error;
	  // then see if it needs sending - if not then ensure it's marked as deleted before returning
	  
	if(( res = saf_read_next_by_status( &txn_trickle, st_needs_sending, index, VIM_FALSE )) != VIM_ERROR_NONE )	  
	{
		saf_delete_record( *index, &txn_trickle, VIM_FALSE );
		return res;	  
	}
  }
  else  
  {
    VIM_ERROR_PTR error = VIM_ERROR_NONE;
	  error = saf_read_next_by_status( &txn_trickle, st_needs_sending, index, VIM_FALSE );
    if( error != VIM_ERROR_NONE )
      return error;
  }
  
  if( 1 )
  {
	  VIM_CHAR type[10];
	  get_txn_type_string_short(&txn_trickle, type, sizeof(type));

#ifndef _WIN32  // Win32 ver all broken.
	  if( terminal_get_status(ss_wow_basic_test_mode) )	
		display_screen( DISP_SAF_UPLOADING, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn_trickle.amount_total), txn_trickle.auth_no );	
	  else	
		display_screen( DISP_SAF_UPLOADING, VIM_PCEFTPOS_KEY_NONE, " ", " ", " " );
#endif

  }
      
  vim_rtc_get_time( &txn_trickle.send_time );

  msg_type = ( txn_trickle.status & st_repeat_saf ) ? MESSAGE_WOW_221 : MESSAGE_WOW_220;
  
	// RDD 270712 SPIRA:5799  TODO (maybe in phase 3) Set repeat after sucessful connection to host
#if 1
  // RDD 290412 SPIRA 5386 Power fail after TX didn't cause 221.
  transaction_set_status( &txn_trickle, st_repeat_saf );  	
#endif

  // RDD 071512 SPIRA 5386 Power Fail after TX of original 220 caused 0 stan in 221
  if( msg_type == MESSAGE_WOW_220 )
  {
	  DoDisconnect = VIM_TRUE;	
	  txn_trickle.stan = terminal.stan; // This pre-empts a power fail so the stan that will be generated is stored in SAF
  }
  else
  {
	  // RDD 050115 - don't disconnect after a 221 in oder to save time
	  DoDisconnect = VIM_FALSE;	
	
	  if( msg_type == MESSAGE_WOW_221 )
	  {
		  // RDD 030317 SPIRA:9220 Need this flag to override reversal generation from emv_cb if a 221 SAF fails to go up during a new txn		
		  saf_totals.fields.saf_repeat_pending = VIM_TRUE;
	  }
  }

  saf_update_record( *index, VIM_FALSE, &txn_trickle );
   
  if (QC_TRANSACTION(&txn_trickle))
  {
      pceft_debug(pceftpos_handle.instance, "This shouldn't happen! SAF handled in Qwikcilver app");
	  res = VIM_ERROR_NONE;
  }
  else
  {
	  // RDD 120422 JIRA PIRPIN-1564 0220 not waiting for 0230 in standalone because of POS intrrupt flag set
      res = msgAS2805_transaction(HOST_WOW, &msg, msg_type, &txn_trickle, VIM_FALSE, VIM_TRUE, DoDisconnect);
  }

  if( res == VIM_ERROR_NONE )
  {
	  // We can delete the saf_pending_flag here because 221s will ALWAYS be sent before 220s
	  saf_totals.fields.saf_repeat_pending = VIM_FALSE;

      // 260721 JIRA-PIRPIN 1118 VDA we sent a SAF so there should be no reversal set exept in case of VDA
      reversal_clear(VIM_FALSE);

	  // We sucessfully uploaded something from the SAF so need to update the totals
	  //saf_update_status_file( &txn_trickle, VIM_TRUE );

  }
 
  else
  {
	  VIM_DBG_ERROR( res );
  }
  // Journal the 220 even if it fails 

  // RDD 030412 : Jounaling the 220 causes problems if the command was interrupted
  // RDD 121012 : if VIM_ERROR_MODEM was returned then the link failed and the SAF was never sent - do not journel
  if(( res == VIM_ERROR_MODEM )||( res == VIM_ERROR_CONNECT_TIMEOUT )||( res == VIM_ERROR_NO_LINK ))
	  return res;
  else if( terminal.allow_idle ) 
  {	  
	  // RDD 030317 v563-2 Need to update Error code if SAF fails as will crash otherwise
	  txn_trickle.error = res;
	  txn_trickle.error_backup = res;
	  pceft_journal_txn( journal_type_advice, &txn_trickle, VIM_FALSE );
  }
  return res;
}


VIM_ERROR_PTR saf_send( VIM_BOOL repeat_only, VIM_SIZE transmit_count, VIM_BOOL delete_offline, VIM_BOOL delete_stuck_repeat )
{
	VIM_SIZE n;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_SIZE index = 0;

	// RDD 030513 - if SAF Prints are disabled then delete any SAF that gets sent OK
	if( terminal_get_status( ss_no_saf_prints ) )
	{
		delete_offline = VIM_TRUE;
	}

	for( n=0; n<transmit_count; n++ )
	{						
		if(( res = saf_send_single( repeat_only, &index )) == VIM_ERROR_NOT_FOUND )
		{
			if( repeat_only == VIM_FALSE )
				saf_totals.fields.saf_count = 0;
			res = VIM_ERROR_NONE;
			DBG_MESSAGE( "<<<<<<<<< Unable to Find SAF >>>>>>>>>>>");
			saf_totals.fields.saf_repeat_pending = VIM_FALSE;
			break;
		}
		else if( res == VIM_ERROR_NONE )
		{
			// there was a saf which got sent sucessfully then either delete it or mark it as sent..
			if( delete_offline )
			{
				// if it was a repeat that failed and we're deleting stuck repeats then delete it
#if 1
				saf_delete_record( index, &txn_trickle, VIM_FALSE );
								
				// RDD 110615 SPIRA:8754 - counter was not being decremented if the offline txn was deleted
				if( saf_totals.fields.saf_count ) 
				{					
					saf_totals.fields.saf_count -= 1;
					if( saf_totals.fields.saf_count == 0 )
						saf_destroy_batch();				
				}
#else
				// Both SAF count and SAF print count are decremented if the transaction is "deleted" (within the fn)
				// RDD 140212 - need to mark these "deleted" records as "deleted" but still be able to print them

				// Mark as cleared advice - this should have been sent but never will be - SAF print will show "DELETED"
				transaction_set_status( &txn_trickle, st_cleared_advice );

				// Mark as sent even though it wasn't
				transaction_set_status( &txn_trickle, st_message_sent );				
				saf_update_record( index, VIM_FALSE, &txn_trickle );
				if( saf_totals.fields.saf_count ) saf_totals.fields.saf_count--;

#endif
			}
			else
			{
				// RDD - Record needs to be updated for the SAF Print				    
				VIM_UINT8 erc;

				DBG_DATA( txn_trickle.host_rc_asc, 3 );	// RDD 160412 SPIRA:5276 Problems with SAFs disappearing
				erc = asc_to_bin(VIM_CHAR_PTR_FROM_UINT8_PTR(txn_trickle.host_rc_asc), 2);

				DBG_MESSAGE( "<<<<<<<<< UPDATE SAF RECORD FOR PRINT >>>>>>>>>>>");
				DBG_VAR( saf_totals.fields.saf_count );
				DBG_VAR( saf_totals.fields.saf_print_count );

				DBG_DATA( txn_trickle.host_rc_asc, 3 );	// RDD 160412 SPIRA:5276 Problems with SAFs disappearing
				// RDD 210312 SPIRA 5148 - declined advices should be marked as ** DELETED ** in the SAF print
				if( erc )
				{
					DBG_VAR( erc );	// RDD 160412 SPIRA:5276 Problems with SAFs disappearing
				    // DBG_MESSAGE( "<<<<<<<<< UPDATE SAF RECORD AS DECLINED FOR PRINT >>>>>>>>>>>");
					// RDD 160922 - repurposed as only used in SAF prints which were made obsolete a while back.
					//transaction_set_status(  &txn_trickle, st_advice_declined );
				}
				transaction_set_status( &txn_trickle, st_message_sent );
				transaction_clear_status( &txn_trickle, st_needs_sending );
				// update the record
				saf_update_record( index, VIM_FALSE, &txn_trickle );

				if( saf_totals.fields.saf_count )
				{
					saf_totals.fields.saf_count -=1;
					if( !terminal_get_status( ss_no_saf_prints ) )
					{
						saf_totals.fields.saf_print_count += 1;
					}
				}
			}
			// RDD 111215 SPIRA:8873 - 	Saf was sent OK so add it to sent totals		
			saf_update_status_file( &txn_trickle, VIM_TRUE );
		}

		else // some other error - probably comms related. SAF was sent but failed..
		{
			if( repeat_only && delete_stuck_repeat )
			{
			    DBG_MESSAGE( "<<<<<<<<< DELETING STUCK SAF AFTER REPEAT  >>>>>>>>>>>");
				// if it was a repeat that failed and we're deleting stuck repeats then delete it

				// RDD 030512 SPIRA:5406 Don't delete stuck SAFs but mark them for SAF print instead
				DBG_MESSAGE( "<<<<<<<<< UPDATE SAF RECORD AS DECLINED FOR PRINT >>>>>>>>>>>");
				// RDD 160922 - repurposed as only used in SAF prints which were made obsolete a while back.
				//transaction_set_status(  &txn_trickle, st_advice_declined );

				transaction_set_status( &txn_trickle, st_message_sent );
				transaction_clear_status( &txn_trickle, st_needs_sending );
				// update the record
				saf_update_record( index, VIM_FALSE, &txn_trickle );

				saf_totals.fields.saf_repeat_pending = VIM_FALSE;

				if( saf_totals.fields.saf_count )
				{
					saf_totals.fields.saf_count -=1;
					if( !terminal_get_status( ss_no_saf_prints ) )
					{
						saf_totals.fields.saf_print_count += 1;
					}
				}
				//saf_delete_record( index, &txn_trickle, VIM_FALSE );
			}
			else
			{
				// RDD 2908 JIRA PIRPIN-2661 Malformed SAF gets stuck and gets resent forever. Delete here if already set as repeat !

				if ((txn_trickle.error == VIM_ERROR_SYSTEM)&&( transaction_get_status(&txn_trickle, st_repeat_saf)))
				{
					pceft_debug( pceftpos_handle.instance, "JIRA PIRPIN-2661 Deleting Stuck Malformed SAF");
					saf_delete_record(index, &txn_trickle, VIM_FALSE);
				}
				else
				{

					// otherwise just mark it as a repeat for next time
					DBG_MESSAGE("<<<<<<<<<<< SAF SEND FAILURE : SET 221 REPEAT >>>>>>>>>>>>");
					transaction_set_status(&txn_trickle, st_repeat_saf);
					// update the record
					saf_update_record(index, VIM_FALSE, &txn_trickle);

					// RDD 230212 Flag added to speed things up - check flag rather than read whole saf for 221 before going online
					saf_totals.fields.saf_repeat_pending = VIM_TRUE;
				}
			}

			// Break from the loop
			break;
		}
	}
	//saf_update_status_file( &txn_trickle, VIM_FALSE );
	return res;
}


#ifdef _DEBUG
#define ONE_DAY_IN_SECONDS (1*3600)
#else
#define ONE_DAY_IN_SECONDS (24*3600)
#endif

static VIM_BOOL QueryExpired( transaction_t *query )
{
	VIM_RTC_TIME time; 
	VIM_RTC_UNIX current_unix_time, txn_unix_time, elapsed_seconds;

	// Get the current time and convert to secs since 1970
	vim_rtc_get_time( &time );	  
	vim_rtc_convert_time_to_unix( &time, &current_unix_time );

	// Then convert the txn time to secs since 1970
	vim_rtc_convert_time_to_unix( &query->time, &txn_unix_time );

	// Get the elapsed time since the txn was stored (in seconds)
	elapsed_seconds = current_unix_time - txn_unix_time;
	//DBG_VAR( elapsed_seconds );

	return( elapsed_seconds >= ONE_DAY_IN_SECONDS ) ? VIM_TRUE : VIM_FALSE;
}



// RDD 290316 Call this from 3 min idle to save file reads and improve terminal lifetime

VIM_ERROR_PTR MaintainCardBuffer( void )
{
	VIM_SIZE txn_counter, index = 0;	  
	transaction_t query;
	VIM_BOOL exist;

	// check if the SAF exists  
    VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(query_card_batch, &exist));
    if (!exist) return VIM_ERROR_NONE;

	// Get a count of all txns in the buffer
	VIM_ERROR_RETURN_ON_FAIL(txn_count(&txn_counter, VIM_FALSE ));

	// If no undeleted records - simply delete the file
	if( query_read_next_undeleted( &query, &index ) == VIM_ERROR_NOT_FOUND )
	{
		card_destroy_batch();
		 
		return VIM_ERROR_NONE;
	}
		  
	else if( QueryExpired( &query ) )
	{
		VIM_ERROR_RETURN_ON_FAIL( saf_delete_record( index, &query, VIM_TRUE ));
	}
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------
RDD 200212: Card Buffer recall of Track data: Check EPAN/TOKEN from POS with pci data stored from a stored Txn
If match, then return true, if no match return FALSE
-----------------------------------------------------------------------------*/
VIM_BOOL VerifyCardDetails( VIM_CHAR *pos_card_data, transaction_t *transaction )
{
  VIM_CHAR *ptr;
  VIM_SIZE compare_len;

  VIM_DBG_STRING( pos_card_data );
  
  // RDD 150812 SPIRA:5767 for cases where theres an ODO retry followed by 91 or comms error resp to the retry
  // the Token originally returned from the host will have been stored but because the last m response only returned the 
  // ePan the completion is attempted with this epan rather than the token we're expecting
  if( transaction->card.pci_data.eTokenFlag ) 
  {
      VIM_DBG_MESSAGE("E-Token");
      compare_len = E_TOKEN_LEN;
	  ptr = transaction->card.pci_data.eToken;
	  if( vim_mem_compare( pos_card_data, ptr, compare_len ) == VIM_ERROR_NONE )	
	  {
		// We have a match so return VIM_TRUE;
		return VIM_TRUE;
	  }
  }
  else
  {
      VIM_DBG_MESSAGE("E-PAN");
  }
  // If it fails to match with the token, try the ePan
  VIM_DBG_STRING( transaction->card.pci_data.ePan );

  // RDD 010519 JIRA WWBP-611 - this can be zero and will result in a positive compare
#if 0
  compare_len = vim_strlen( transaction->card.pci_data.ePan );
#else
  compare_len = vim_strlen( pos_card_data );
#endif

  ptr = transaction->card.pci_data.ePan;

  VIM_DBG_NUMBER( compare_len );
  VIM_DBG_STRING( pos_card_data );
  VIM_DBG_DATA( ptr, compare_len);

  // If the eCard Data matches with that supplied by the POS then check the txn ref
  if( vim_mem_compare( pos_card_data, ptr, compare_len) == VIM_ERROR_NONE )
  {
	  // We have a match so return VIM_TRUE;
      VIM_DBG_WARNING("Match");
      return VIM_TRUE;
  }
  VIM_DBG_WARNING("No Match");
  return VIM_FALSE;
}


/*----------------------------------------------------------------------------
RDD 251011 : Find ePan in SAF: Check through SAF file for a matching ePan and load up txn if found
-----------------------------------------------------------------------------*/
VIM_ERROR_PTR FindEpanInQueryBuffer( VIM_CHAR *pos_card_data, VIM_CHAR *pos_ref, transaction_t *transaction )
{
  VIM_UINT32 index;
  VIM_SIZE query_counter;
  transaction_t stored_query;
  VIM_ERROR_PTR res = VIM_ERROR_NOT_FOUND;

  VIM_ERROR_RETURN_ON_FAIL( txn_count( &query_counter, VIM_TRUE ));

  /* read batch */
   DBG_VAR( query_counter );
#if 1
  // RDD JIRA WWBP-611 
  for (index = 0; index<query_counter; index++)
#else
  for (index = 1; index<=query_counter; index++)
#endif
  {
	DBG_VAR( index );
    if ( txn_read_record_index( index, VIM_TRUE, &stored_query ) == VIM_ERROR_NONE )
    {
		if( transaction_get_status( &stored_query, st_deleted_offline ))
		{
            DBG_MESSAGE("Already Deleted....");
            continue;        
        }
        // RDD JIRA WWBP-611 
        else if (vim_strlen(stored_query.card.pci_data.ePan))
        {
            VIM_DBG_STRING(stored_query.card.pci_data.ePan);
            DBG_MESSAGE("!!! Found Undeleted Query Buffer Txn !!!");
        }
        else
        {
            VIM_DBG_VAR( stored_query.type );
            continue;
        }

        VIM_DBG_STRING(pos_card_data);

		if( VerifyCardDetails( pos_card_data, &stored_query ) )
		{
			DBG_MESSAGE( "!!! Found Epan Match !!!");
			// RDD 080312 : SPIRA 5044 - no POS REF NO to recall for Deposit completion
		// RDD 050412 - don't do POS REF check until we can get testing time on real POS...
			if(( vim_strlen( pos_ref ) ) && ( txn.type != tt_deposit ))
			{
				VIM_CHAR *Ptr = stored_query.pos_reference;
			    // DBG_MESSAGE( "!! POS Ref and not deposit completion so check against stored record !!");
				// Skip any leading zeros
				//while( *Ptr == '0' ) Ptr++;
				if( vim_strncmp( pos_ref, Ptr, vim_strlen(Ptr) )  )
				{
					DBG_MESSAGE( "!! POS Ref does not match that of stored TXN: Delete record and continue searching !!");
					// EPAN Matches but not POS ref - skip to next and delete this match as it's useless 
					if( !terminal_get_status(ss_wow_basic_test_mode ))		
						saf_delete_record( index, &stored_query, VIM_TRUE );
					continue;
				}
			}
			// We found a match so copy over the data we need into the current transaction and exit
			DBG_MESSAGE( "!! Copying Card Data into txn struct !!");
			DBG_VAR( stored_query.type );
			vim_mem_copy( &transaction->card, &stored_query.card, VIM_SIZEOF( card_info_t ) );

			// RDD 151012 - SPIRA:6168 Clear any existing emv data from the buffer - DO NOT EVER read tags from Kernel when POS data supplied
			vim_mem_clear( &transaction->emv_data, VIM_SIZEOF( emv_data_t ) );
			vim_mem_copy( &transaction->emv_data, &stored_query.emv_data, VIM_SIZEOF( emv_data_t ) );

			// RDD 120712 SPIRA:5767 Need to retrieve stored PAD data as well for fuel cards...
			//vim_mem_copy( &transaction->u_PadData.ByteBuff, &stored_query.u_PadData.ByteBuff, VIM_SIZEOF(stored_query.u_PadData.ByteBuff) );
			//vim_mem_copy( &transaction->FuelData, &stored_query.FuelData, VIM_SIZEOF(stored_query.FuelData) );
			vim_mem_copy( &transaction->stan, &stored_query.stan, VIM_SIZEOF( &stored_query.stan ) );	// needed for receipt

			// RDD 260412 SPIRA:5366 Set txn type as we want to ensure that DE55 gets sent in the 220 ...
			// RDD 020512 SPIRA:5421 Original Type in POS msg trumps that stored in original txn (though they should match really)
			if( transaction->original_type == tt_none )
				transaction->original_type = stored_query.type;
			DBG_VAR( transaction->original_type );

			// RDD 221112 - noticed that AID was sometimes incorrect so need to txfr CPAT index
			transaction->aid_index = stored_query.aid_index;

			// We've now completed this index now so delete it							
			if( !terminal_get_status(ss_wow_basic_test_mode ))		
				saf_delete_record( index, &stored_query, VIM_TRUE );
			return VIM_ERROR_NONE;
		}
		else
		{
			DBG_MESSAGE( "Epan didn't match");
		}
	} 
  }

  //txn.error = ERR_CANNOT_PROCESS_EPAN;
  return res;
}


// RDD 051212 Implemented for WOW NZ V305 - checks for EFB duplicates already stored in SAF 
VIM_BOOL saf_duplicate_found( transaction_t *transaction, txn_status_t MatchStatusBits, VIM_SIZE *first_index, VIM_UINT8 ReqdMatchBitmap )
{
	transaction_t saf_txn;
	transaction_t *saf_txn_ptr = &saf_txn;
	VIM_SIZE index = *first_index;
	VIM_CHAR CardPan[WOW_PAN_LEN_MAX+1], SafPan[WOW_PAN_LEN_MAX+1];
    VIM_UINT8 FoundMatchBitmap = 0;

	vim_mem_clear( CardPan, VIM_SIZEOF( CardPan ) );

	// get the pan of the current txn 
	card_get_pan( &transaction->card, CardPan );

	// read the next saf with the required status bit(s) set
	while( saf_read_next_by_status( &saf_txn, MatchStatusBits, &index, VIM_TRUE ) == VIM_ERROR_NONE )
	{
		// read the PAN of the stored txn
        VIM_SIZE PanLen = VIM_MIN(WOW_PAN_LEN_MAX, vim_strlen(CardPan));
		vim_mem_clear( SafPan, VIM_SIZEOF( SafPan ) );
		card_get_pan( &saf_txn_ptr->card, SafPan );

		// Check to see if there is a match with the current txn PANs and account types
		if( (ReqdMatchBitmap & BITMAP_CARD) && ( transaction->account == saf_txn.account ) && ( vim_mem_compare( CardPan, SafPan, PanLen ) == VIM_ERROR_NONE ))
		{
			// check the date - if we've rolled since the last txn was stored then it's OK
            pceft_debug_test( pceftpos_handle.instance, "SAF Match : Match On PAN" );
            FoundMatchBitmap |= BITMAP_CARD;
        }

        if((ReqdMatchBitmap & BITMAP_RRN) && ( transaction->roc == saf_txn.roc ))
        {
            pceft_debugf_test(pceftpos_handle.instance, "SAF Match : Match On RRN %d", saf_txn.roc);
            FoundMatchBitmap |= BITMAP_RRN;
        }

        if ((ReqdMatchBitmap & BITMAP_REF) && (vim_mem_compare(saf_txn.pos_reference, transaction->pos_reference, vim_strlen(transaction->pos_reference)) == VIM_ERROR_NONE))
        {
            pceft_debug_test(pceftpos_handle.instance, "SAF Match : Match On POS REF");
            FoundMatchBitmap |= BITMAP_REF;
        }

        if ((ReqdMatchBitmap & BITMAP_TIME) && ( transaction->time.day == saf_txn_ptr->time.day ))
        {
            pceft_debug_test(pceftpos_handle.instance, "SAF Match : Match On TIME");
            FoundMatchBitmap |= BITMAP_TIME;
        }

        if (FoundMatchBitmap == ReqdMatchBitmap)
        {
            pceft_debug_test(pceftpos_handle.instance, "!!! Found Enough Matches - exit TRUE !!!");
            *first_index = index;
				return VIM_TRUE;
		}
        else
        {
            //pceft_debug_test(pceftpos_handle.instance, "Insufficient match criteria. Continue to next txn...");
        }

		// increment the index so we don't find the same txn again
		index+=1;
	}
    //pceft_debug_test(pceftpos_handle.instance, "!!! Not Enough Matches... exit FALSE !!!");
	return VIM_FALSE;
}

static VIM_ERROR_PTR saf_search_by_index( transaction_t *tmp_txn )
{
  if( saf_read_record_index(tmp_txn->update_index, VIM_FALSE, tmp_txn) == VIM_ERROR_NONE)
  {
    if( tmp_txn->read_only == VIM_FALSE)
      return VIM_ERROR_NONE;
  }
  
  VIM_ERROR_RETURN(VIM_ERROR_MISMATCH);
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR saf_search_for_roc_tt(VIM_UINT32 roc, VIM_UINT32 *roc_index, txn_type_t type, VIM_CHAR *auth_no)
{
  VIM_UINT32 index;
  VIM_SIZE saf_counter;
  transaction_t tmp_txn;
  VIM_BOOL pre_auth;

  vim_mem_clear(&tmp_txn, sizeof(tmp_txn));

  pre_auth = ((tt_checkout == type) || (tt_preauth_enquiry == type));

  if (pre_auth)
    saf_counter = preauth_saf_table.records_count;
  else
    VIM_ERROR_RETURN_ON_FAIL( txn_count( &saf_counter, VIM_FALSE ));
  
  if(pre_auth)
  {
    /* find a record in the pre_auth table */
    for (index = 0; index < saf_counter; index++)
    {	
		VIM_DBG_NUMBER( roc );
		if( roc == preauth_saf_table.details[index].roc )
		{
			VIM_DBG_PRINTF1( "Found RRN : %d", preauth_saf_table.details[index].roc );
			// RDD - if already checked out then return ERR_CHECKED_OUT
			if( preauth_saf_table.details[index].status & st_checked_out )
			{
				DBG_MESSAGE( "This one already checked out" );
				return ERR_ROC_COMPLETED;
			}
			else if( preauth_expired( &preauth_saf_table.details[index].time ))
			{
				DBG_MESSAGE( "This one already Expired" );
				return ERR_ROC_EXPIRED;
			}
			else
			{
				*roc_index = index; 
				vim_mem_copy( auth_no, preauth_saf_table.details[index].auth_no, VIM_SIZEOF(preauth_saf_table.details[index].auth_no));
				auth_no[6] = '\0';
				VIM_DBG_PRINTF1( "Auth Number %s", auth_no );
				return VIM_ERROR_NONE;
			}
		}
    }
	VIM_DBG_PRINTF1( "No Match Found for %d", roc );
    return ERR_INVALID_ROC;
  }

  /* read batch */
  for( index=0; index < saf_counter; index++ )
  {
    if( saf_read_record_index( index, VIM_FALSE, &tmp_txn ) == VIM_ERROR_NONE)
    {
      VIM_DBG_VAR(roc);
      VIM_DBG_VAR(tmp_txn.roc);
      VIM_DBG_VAR(tmp_txn.status);
      VIM_DBG_VAR(tmp_txn.read_only);
  
      if (tmp_txn.roc == roc && tmp_txn.read_only == VIM_FALSE &&
          !transaction_get_status(&tmp_txn, st_deleted_offline))
      {
        if( tmp_txn.read_only == VIM_TRUE )
          VIM_ERROR_RETURN_ON_FAIL( saf_search_by_index( &tmp_txn ));
            
		*roc_index = index; 		
		return VIM_ERROR_NONE;     
	  }
    } 
  }
  DBG_POINT;
  return VIM_ERROR_MISMATCH;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR saf_search_for_auth_no_for_co(VIM_BOOL pre_auth, VIM_CHAR *auth_no, VIM_UINT32* roc, VIM_UINT32 *roc_index)
{
  VIM_UINT32 index;
  VIM_SIZE saf_counter;
  transaction_t tmp_txn;

  vim_mem_clear(&tmp_txn, sizeof(tmp_txn));

  if (pre_auth)
    saf_counter = get_preauth_table_count();
  else
    VIM_ERROR_RETURN_ON_FAIL(txn_count(&saf_counter,VIM_FALSE));

  VIM_DBG_VAR(saf_counter);

  if (pre_auth)
  {
    /* find a record in the pre_auth table */
    for (index = 0; index < saf_counter; index++)
    {
      if ((vim_strlen((const VIM_CHAR *) auth_no) > 0) &&
          (VIM_ERROR_NONE == vim_mem_compare(auth_no,
                                             preauth_saf_table.details[index].auth_no,
                                             VIM_SIZEOF(preauth_saf_table.details[index].auth_no))) &&
         !(preauth_saf_table.details[index].status & st_checked_out))
      {
        *roc = preauth_saf_table.details[index].roc;
        *roc_index = index;
        return VIM_ERROR_NONE;
      }
    }
    return ERR_NO_ROC_NO_APPR;
  }

  /* read batch */
  for(index=0; index<saf_counter; index++)
  {
    if (saf_read_record_index(index, VIM_FALSE, &tmp_txn) == VIM_ERROR_NONE)
    {
      if (transaction_get_status(&tmp_txn, st_deleted_offline))
        continue;
      
      if ((vim_strlen((const VIM_CHAR*)tmp_txn.auth_no) > 0) &&
           vim_strncmp((const VIM_CHAR*)auth_no,
                       (const VIM_CHAR*)tmp_txn.auth_no,
                        vim_strlen((const VIM_CHAR*)tmp_txn.auth_no)) == 0)
          
      {
        if ((tmp_txn.type == tt_preauth) && !(tmp_txn.status & st_checked_out ))
        {
          *roc = tmp_txn.roc;
          *roc_index = index; 
          return VIM_ERROR_NONE;
        }
        else
        {
          return ERR_INVALID_AUTH_NO;
        }
      }
    } 
  }
  DBG_POINT;
  return ERR_NO_ROC_NO_APPR;
}


VIM_ERROR_PTR PurgePreauthFiles(VIM_SIZE* Completed, VIM_SIZE* Expired)
{
	PREAUTH_SAF_DETAILS* PreauthDetailsPtr = &preauth_saf_table.details[0];
	VIM_SIZE Index;// , PendingCount;
	PREAUTH_SAF_TABLE_T preauth_temp_table;
	static VIM_TEXT const pa_temp_file[] = VIM_FILE_PATH_LOCAL "pa_tempfile.bin";
	transaction_t txn_temp;

	//if( IS_STANDALONE == VIM_FALSE )
	//	return VIM_ERROR_NONE;

	// wipe the temp table
	preauth_temp_table.records_count = 0;

	preauth_load_table();

	vim_file_delete(pa_temp_file);

	// Read Each Historical Record and Process Any that are Pending
	for (Index = 0; Index < preauth_saf_table.records_count; Index++, PreauthDetailsPtr++)
	{
		if (preauth_expired(&PreauthDetailsPtr->time))
			*Expired += 1;

		if (PreauthDetailsPtr->status & st_checked_out)
			*Completed += 1;

		if (preauth_pending(PreauthDetailsPtr) == VIM_TRUE)
		{
			if (pa_read_record(PreauthDetailsPtr->roc, &txn_temp) == VIM_ERROR_NONE)
			{
				preauth_temp_table.details[preauth_temp_table.records_count] = *PreauthDetailsPtr;
				preauth_temp_table.records_count += 1;

				VIM_ERROR_RETURN_ON_FAIL(saf_add_gen_record(&txn_temp, pa_temp_file, PREAUTH_SAF_RECORD_COUNT));
			}
		}
	}

	// don't write files if no compression necessary
	if (!preauth_temp_table.records_count)
	{
		DBG_MESSAGE("PA BATCH PURGE -> ensure files are deleted");
		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(preauth_batch));
		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(preauth_table));
	}
	else
	{
		VIM_DBG_PRINTF1("PA BATCH PURGE -> Removed %d Records", preauth_saf_table.records_count - preauth_temp_table.records_count);

		DBG_MESSAGE("Delete Old files");
		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(preauth_batch));
		VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(preauth_table));

		DBG_MESSAGE("Write New files");

		// We're done, so store the new table as the pa_table.bin
		if (preauth_temp_table.records_count)
		{
			// Save the compressed pa batch file
			preauth_save_table(&preauth_temp_table);
			vim_file_move(pa_temp_file, preauth_batch);
			preauth_load_table();
		}
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR TestWritePASAF(VIM_CHAR* Data, VIM_SIZE Len)
{

	if (vim_file_set_bytes(preauth_batch, Data, Len, 0) != VIM_ERROR_NONE)
	{
		pceft_debugf_test(pceftpos_handle.instance, "Unable to %d bytes to %s", Len, preauth_batch);
	}
	else
	{
		pceft_debugf_test(pceftpos_handle.instance, "added %d to %s", Len, preauth_batch);
	}

	if (vim_file_is_present("pasaf"))
	{
		pceft_debugf_test(pceftpos_handle.instance, "%s found OK", preauth_batch);
	}

	if (Len)
	{
		if (vim_file_delete(preauth_batch) != VIM_ERROR_NONE)
		{
			pceft_debugf_test(pceftpos_handle.instance, "Unable to delete %s", preauth_batch);
		}
		else
		{
			pceft_debugf_test(pceftpos_handle.instance, "%s deleted OK", preauth_batch);
		}
	}

	if (vim_file_is_present("pasaf"))
	{
		pceft_debugf_test(pceftpos_handle.instance, "%s found NOK", preauth_batch);
	}
	return VIM_ERROR_NONE;
}

