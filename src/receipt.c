#include <incs.h>
#include "graphic.h"
VIM_DBG_SET_FILE("TN");
/*-------------------------------------------------------------------------*
 * function:    receipt_print
 * @brief: Prints the receipt on the terminal
 */
 
extern const card_bin_name_item_t CardNameTable[];
extern const VIM_TAG_ID tags_to_print_txn[];
extern const VIM_TAG_ID tags_to_print[];
extern PREAUTH_SAF_TABLE_T preauth_saf_table;
extern VIM_BOOL ApplySurcharge(transaction_t *transaction, VIM_BOOL Apply);

static VIM_CHAR *rc_item_R(const VIM_CHAR *label, const VIM_CHAR *item, VIM_SIZE item_length);
static VIM_CHAR *rc_item_L(const VIM_CHAR*label, const VIM_CHAR*item, VIM_SIZE item_length);


/* Add a string to the receipt buffer */
static VIM_ERROR_PTR receipt_add_string(VIM_CHAR *line, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_UINT8 len;

  if (!line || !receipt)
  {
	  pceft_debug(pceftpos_handle.instance, "NULL Pointer!");
	  return VIM_ERROR_NULL;
  }

  if (receipt->lines >= VIM_PCEFTPOS_RECEIPT_MAX_LINES)
  {
    // DBG_MESSAGE("FLUSH RECEIPT!");
   //VIM_DBG_VAR(receipt->lines);
    pceft_pos_print(receipt, VIM_TRUE);
    // DBG_MESSAGE("DONE");
    receipt->lines = 0;
  }


  len = MIN(vim_strlen(line), terminal.configuration.prn_columns);
  vim_mem_copy(receipt->data[receipt->lines], line, len);
  vim_mem_set(&receipt->data[receipt->lines][len], ' ', terminal.configuration.prn_columns-len);
  receipt->lines++;
  return VIM_ERROR_NONE;
}


// RDD 011115 SPIRA:8823 Use token on ALH receipts where available
static void receipt_add_ePan( VIM_PCEFTPOS_RECEIPT *receipt, transaction_t *transaction )
{
	VIM_UINT8 len, n ;
	VIM_CHAR ePan[E_TOKEN_LEN+1], *Ptr;
    VIM_BOOL TokenAvailable = transaction->card.pci_data.eTokenFlag;

	Ptr = TokenAvailable ? transaction->card.pci_data.eToken : transaction->card.pci_data.ePan;

	vim_mem_clear( ePan, VIM_SIZEOF(ePan) );
	//DBG_MESSAGE( txn.card.pci_data.ePan );
  
	// RDD 010312 SPIRA: 4981 Remove the '=' from the ePan on the receipt  
	if(( len = vim_strlen(Ptr)) > VIM_SIZEOF(ePan) ) len = VIM_SIZEOF(ePan);

	for ( n = 0; n < len; Ptr++ ) 
	{
		if(*Ptr != '=') 
		{
			ePan[n++] = *Ptr;
		}
	}
	DBG_MESSAGE( ePan );	 
	receipt_add_string(ePan, receipt);
}

/* Center a string and add it to the receipt buffer */
static void receipt_add_centered_string(VIM_CHAR *line, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR centered_string[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_UINT8 length;
  
  length = MIN( vim_strlen(line), terminal.configuration.prn_columns );

  vim_mem_set(centered_string, ' ', sizeof(centered_string)-1);
  centered_string[terminal.configuration.prn_columns]=0;
  vim_mem_copy(&centered_string[(terminal.configuration.prn_columns-length)/2], line, length);
  
  receipt_add_string(centered_string, receipt);
}


void AddTrainingMode(VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL duplicate)
{
	transaction_t *transaction = (duplicate) ? &txn_duplicate : &txn;

	// RDD 161013 SPIRA:6784
	if( terminal_get_status( ss_offline_test_mode ) )	
	{
		receipt_add_centered_string("** OFFLINE TEST MODE **", receipt);		
	}
	// RDD 181012 SPIRA:6194 Training mode not set correctly for settlement
	else if( transaction->status & st_training_mode )
	{
		receipt_add_centered_string(" *** TRAINING MODE ***  ", receipt);
	}
}


static void receipt_format_line(VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *left_string, VIM_CHAR *right_string )
{
    VIM_CHAR string_buf[100];
    VIM_CHAR temp_buf[100];
    VIM_UINT8 spaces;
	    	// 

	vim_mem_clear( string_buf, VIM_SIZEOF(string_buf) );
	vim_mem_clear( temp_buf, VIM_SIZEOF(temp_buf) );

    vim_strcpy( string_buf, left_string );
	    	// 

DBG_MESSAGE( string_buf );
    spaces = terminal.configuration.prn_columns - ( vim_strlen(left_string) + vim_strlen(right_string) );
	    	// 
			//DBG_VAR(spaces);
    vim_mem_set( temp_buf, ' ', spaces );
	    	// 
DBG_MESSAGE( string_buf );

    temp_buf[spaces] = '\0';
    	// 
    
    vim_strcat( string_buf, temp_buf );
    	// 
DBG_MESSAGE( string_buf );

    vim_strcat( string_buf, right_string );
    	// 
//DBG_MESSAGE( string_buf );
    receipt_add_string(string_buf, receipt); 
	    	// 

}



/* -------------------------------------------------------------------- */
/*  Duplicate Receipt Printing Routine */
/* -------------------------------------------------------------------- */
VIM_ERROR_PTR print_duplicate_receipt(transaction_t *transaction)
{
  VIM_PCEFTPOS_RECEIPT receipt;
  VIM_UINT8 type;

  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));

  type = receipt_customer|receipt_duplicate;

  /* Build merchant first */	
  /*  Change Print type */
  	 //  

  receipt_build_financial_transaction(type, transaction, &receipt);
  /*  Now Print header */
	 //  
  VIM_ERROR_RETURN_ON_FAIL(pceft_pos_print(&receipt, VIM_TRUE));

  return VIM_ERROR_NONE;
}


/* Add the specified amount of VIM_CHARacters as a line on the receipt after centering them. For use when strings arent NULL terminated */

static void receipt_add_centered_characters(VIM_CHAR *line, VIM_UINT8 count, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_INT8 index;

  vim_mem_set(string_buf, 0, VIM_SIZEOF(string_buf));
  vim_mem_copy(string_buf, line, MIN(count, terminal.configuration.prn_columns));

  for (index = vim_strlen(string_buf) - 1; index > 0; index--)
  {
    /* change all spaces by zeroes */
    if (' ' == string_buf[index])
      string_buf[index] = 0;
    else
      break;
  }

  receipt_add_centered_string(string_buf, receipt);
}




static void receipt_add_blank_lines(VIM_UINT8 count, VIM_PCEFTPOS_RECEIPT *receipt)
{
  while (count--)
    receipt_add_string(" ", receipt);
}

static void receipt_add_dotted_line(VIM_CHAR dot, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_SIZE len = terminal.configuration.prn_columns - 3;
  vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
  vim_mem_set(string_buf, dot, len );  
  receipt_add_string(string_buf, receipt);
}


static void rc_item(const VIM_CHAR *label, const VIM_CHAR *item, VIM_SIZE item_length, VIM_CHAR *pBuffer)
{
    VIM_UINT8 label_length = vim_strlen(label);
    VIM_CHAR *Ptr = pBuffer;
    VIM_UINT8 line_length = (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH) ? 24 : PAPER_WIDTH;

    vim_mem_set( Ptr, ' ', line_length );
    Ptr += line_length;
    *Ptr = 0;
    Ptr = pBuffer;
    vim_mem_copy( Ptr, label, label_length);
    Ptr += (line_length - item_length);
    vim_mem_copy( Ptr, item, item_length );
}


static VIM_CHAR *rc_item_R(const VIM_CHAR *label, const VIM_CHAR *item, VIM_SIZE item_length)
{
    static VIM_CHAR Buffer[RECEIPT_LINE_MAX_WIDTH + 1];
    rc_item(label, item, item_length, &Buffer[0]);
    return &Buffer[0];
}

static VIM_CHAR *rc_item_L(const VIM_CHAR *label, const VIM_CHAR *item, VIM_SIZE item_length)
{
    static VIM_CHAR Buffer[RECEIPT_LINE_MAX_WIDTH + 1];
    rc_item(label, item, item_length, &Buffer[0]);
    return &Buffer[0];
}

  
// RDD 150323 JIRA PIRPIN-2252 No data fields in diag receipt when MAX paper width set. Replace PAPER_WIDTH ( which is generic ) to receipt->receipt_columns ( which can be set by calling function )

static VIM_ERROR_PTR receipt_add_right_justified_item(const VIM_CHAR *left_string, const VIM_CHAR *right_string, VIM_UINT8 item_length, VIM_PCEFTPOS_RECEIPT *receipt)
{  
  VIM_CHAR line_buf[RECEIPT_LINE_MAX_WIDTH+1];  /* RDD - changed from hard coded value*/
  VIM_UINT8 label_length;

    item_length = MIN(item_length, receipt->receipt_columns);
    label_length = MIN( vim_strlen( left_string ), receipt->receipt_columns);
    vim_mem_set(line_buf, ' ', receipt->receipt_columns);
    line_buf[receipt->receipt_columns] = 0;
    vim_mem_copy(line_buf, left_string, label_length);
    vim_mem_copy(&line_buf[receipt->receipt_columns - item_length], right_string, item_length);
  receipt_add_string(line_buf, receipt);
  /* return without error */
  return VIM_ERROR_NONE;
}


static void receipt_add_merchant_details( VIM_PCEFTPOS_RECEIPT *receipt)
{
//  receipt_add_centered_VIM_CHARacters(terminal.configuration.default_merchant_name, 23, receipt);
    if (txn.type == tt_void)
    {
        receipt_add_centered_string("* VOID RECEIPT *", receipt);
    }

  else if( transaction_get_status( &txn, st_completion ) )
  {
	  if( txn.type == tt_checkout )
		  receipt_add_centered_string("* COMPLETION *", receipt);	
	  else
		  receipt_add_centered_string("*MANUAL EFTPOS VOUCHER *", receipt);	
	  receipt_add_blank_lines(1, receipt);
  }

  if( PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH )
  {
      receipt_add_right_justified_item( terminal.configuration.merchant_name, terminal.configuration.merchant_address, WOW_ADDRESS_LEN, receipt);
  }
  else
  {
      receipt_add_centered_characters(terminal.configuration.merchant_name, VIM_SIZEOF(terminal.configuration.merchant_name), receipt);
      receipt_add_centered_characters(terminal.configuration.merchant_address, VIM_SIZEOF(terminal.configuration.merchant_address), receipt);
  }
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR receipt_add_mid( VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_SIZE caic_size;

  /* get the alpha numeric size of the caic */
  VIM_ERROR_RETURN_ON_FAIL(vim_string_get_alpha_numeric_size(terminal.merchant_id ,&caic_size,VIM_SIZEOF(terminal.merchant_id)));
  /* add a caic line to the reciept */
  VIM_ERROR_RETURN_ON_FAIL(receipt_add_right_justified_item("MERCH ID:", terminal.merchant_id, caic_size, receipt));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR receipt_add_ppid( VIM_PCEFTPOS_RECEIPT *receipt )
{
  VIM_UINT8 ppid_bcd[8];
  VIM_CHAR ppid_asc[16+1];
  vim_mem_clear(ppid_asc, VIM_SIZEOF(ppid_asc));
  
  VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid(
  tcu_handle.instance_ptr, 
  (VIM_DES_BLOCK *)&ppid_bcd));    

  bcd_to_asc( ppid_bcd, ppid_asc, 16 );
  
  VIM_ERROR_RETURN_ON_FAIL(receipt_add_right_justified_item("PPID:", ppid_asc, 16, receipt));
  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR receipt_add_tid( VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_SIZE catid_size;
  /* get the alpha numeric size of the catid */
  VIM_ERROR_RETURN_ON_FAIL(vim_string_get_alpha_numeric_size(terminal.terminal_id,&catid_size,VIM_SIZEOF(terminal.terminal_id)));
  /* add a catid line to the reciept */
  VIM_ERROR_RETURN_ON_FAIL(receipt_add_right_justified_item("TERM ID:", terminal.terminal_id, catid_size, receipt));
  /* return without error */
  return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR receipt_add_date_time(VIM_RTC_TIME const *datetime, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *descriptor, VIM_CHAR *Ptr1 )
{
  VIM_CHAR date_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR rrn[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_UINT8 rrn_len=0;
  VIM_CHAR *Ptr = descriptor;

  vim_mem_clear( rrn, VIM_SIZEOF(rrn) );

  // Remove leading zeros
  //while( VIM_ISZERO( *Ptr++ ));

  // Find the length of the data

  if(( !VIM_ISDIGIT( *Ptr ) && ( *Ptr)))
  {
	  // RDD 200814 below line added for ALH Preauth - 12 len doesnt fit !
	  while( !VIM_ISDIGIT( *++Ptr ));		
	  rrn_len = vim_strlen( Ptr );
  }
  else
  {
	while( VIM_ISDIGIT( *Ptr ) ) {rrn_len++; Ptr++;}
	{
		// Rewind
		Ptr-=rrn_len;	
		if( rrn_len > 6 )
		{
			// 5100 only seems to print first 6 digits
			Ptr += (rrn_len - 6);
			rrn_len = 6;
		}
	}
  }
  vim_mem_copy( rrn, Ptr, rrn_len );
  
  if( vim_strlen( descriptor ) )
  {
	  /*  Format DD/MM/YY HH:MM        <RRN> */	
	  VIM_ERROR_RETURN_ON_FAIL( vim_snprintf( date_buf, VIM_SIZEOF(date_buf), "%02d/%02d/%02d %02d:%02d", datetime->day, datetime->month, datetime->year%2000, datetime->hour, datetime->minute));    
      if (Ptr1 == VIM_NULL)
          receipt_add_right_justified_item(date_buf, rrn, rrn_len, receipt);
      else
          rc_item(date_buf, rrn, rrn_len, Ptr1);
  }
  else
  {
	  /*  Format DD/MM/YY        HH:MM */
	  VIM_CHAR time_buf[10];
	  vim_sprintf( time_buf, "%02d:%02d", datetime->hour, datetime->minute );	
	  VIM_ERROR_RETURN_ON_FAIL( vim_sprintf( date_buf, "%02d/%02d/%02d", datetime->day, datetime->month, datetime->year%2000 ));		
      if( Ptr1 == VIM_NULL )	    
          receipt_add_right_justified_item( date_buf, time_buf, vim_strlen(time_buf), receipt );
      else
          rc_item( date_buf, rrn, rrn_len, Ptr1 );
  }   
  return VIM_ERROR_NONE;
}

#if 0
static VIM_ERROR_PTR receipt_add_tran_type(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR string_buf[RECEIPT_LINE_WIDTH + 1];

  get_txn_type_string(transaction, string_buf,  VIM_SIZEOF(string_buf), VIM_FALSE);

  VIM_DBG_STRING(string_buf);

  if (vim_strlen(string_buf) > 12)  /* truncate the display buffer */
    string_buf[12]=0x00;
  
  receipt_add_right_justified_item("TRANS TYPE", string_buf, vim_strlen(string_buf), receipt);
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR receipt_add_generic_rrn(VIM_TEXT* lable, VIM_TEXT *rrn, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];

  /*  Only print if there is a value and Acquirer says to */
  if ( rrn )
  {
    vim_mem_set(string_buf, 0, RECEIPT_LINE_MAX_WIDTH+1);

    vim_snprintf(string_buf, sizeof(string_buf), "%012s", rrn);

    receipt_add_right_justified_item(lable, string_buf, vim_strlen(string_buf), receipt);
  
	// RDD 120112: add the ref to the receipt header too
	vim_mem_copy( receipt->reference, rrn, 12 );
  }
  return VIM_ERROR_NONE;
}

static const VIM_CHAR* receipt_get_account_string(account_t account)
{
	VIM_CHAR* pType = VIM_NULL;
	switch (account)
	{

	// RDD 231211 : QueryCard may not set account type
	case account_unknown_any:
	  pType = " ";
	  break;
    case account_cheque:
      pType = "CHEQUE";
      break;
    case account_savings:
      pType = "SAVINGS";
      break;
    case account_credit:
    //default:
      pType = "CREDIT";
    //Deliberate fall through

      // RDD 300720 Trello #69 and Trello #199 - change default acc to print nothing for sceme debits
    default:
      pType = " ";


      break;
	}
	return pType;
}

static void receipt_add_account_info(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr)
{
	VIM_CHAR AppLabel[MAX_APPLICATION_LABEL_LEN+1];
	const VIM_CHAR *pType = VIM_NULL;    
		        
	vim_mem_clear( AppLabel, VIM_SIZEOF(AppLabel) );

	GetCardName( transaction, AppLabel );

	/* Add account type */
	pType = receipt_get_account_string(transaction->account);

    if (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH)
    {
        vim_strcpy( Ptr, rc_item_R( AppLabel, pType, vim_strlen(pType)));
    }
    else
    {
		receipt_add_right_justified_item( AppLabel, pType, vim_strlen(pType), receipt );
	}
}

  

static VIM_BOOL is_full_emv_offline_txn(transaction_t *transaction)
{
   
  if ( transaction_get_status(transaction, st_offline) 
    && ((transaction->card.type == card_chip) || (transaction->card.type == card_picc)))
  {
	  VIM_DBG_ERROR( transaction->error );
	  VIM_DBG_VAR( transaction->saf_reason );
    /* Approved */
    if ( (transaction->error == VIM_ERROR_NONE)
      && (transaction->saf_reason == saf_reason_offline_emv_purchase) )
      return VIM_TRUE;
    /* Declined */
    else if ( txn.error == ERR_EMV_CARD_DECLINED )
      return VIM_TRUE;
  }

  return VIM_FALSE;
}


#if 0
static VIM_BOOL is_full_pan_required(transaction_t *transaction)
{
  /*
   * FULL EMV Offline transaction
   * EFB 
   * Pre-auth  
   */
  if(((transaction_get_status(transaction, st_efb) == VIM_TRUE)
    || (transaction->type == tt_preauth) 
    || is_full_emv_offline_txn(transaction))
    && (TERM_FULL_PAN))
    return VIM_TRUE;
  else
    return VIM_FALSE;
}
#endif

VIM_ERROR_PTR pan_mask_parse(VIM_TEXT* pan_mask, VIM_UINT8* first_len_ptr, VIM_UINT8* last_len_ptr)
{
  VIM_UINT32 temp;
  VIM_CHAR * presult = VIM_NULL;
  VIM_TEXT first_len[5];

 //VIM_ 
  vim_mem_clear( first_len, VIM_SIZEOF(first_len));

  *first_len_ptr = 0;
  *last_len_ptr = 0;

  if ( vim_strchr(pan_mask, '-', &presult) == VIM_ERROR_NONE )
  {
    presult++;
    VIM_ERROR_RETURN_ON_NULL(presult);
    if ( *presult != '0' )
    {
      temp = vim_atol(presult);
     //VIM_DBG_VAR(temp);
      if ( temp > 19 )
        *last_len_ptr = 19;
      else
        *last_len_ptr = temp;
     //VIM_DBG_VAR(*last_len_ptr);
    }
    else
    {
      *last_len_ptr = 0;
    }
      
    if ( (presult - pan_mask - 1) > 0 )
    {
      vim_mem_copy(first_len, pan_mask, (presult - pan_mask - 1));
      
     // VIM_DBG_STRING(first_len);
      temp = vim_atol(first_len);
      if ( temp > 19 )
        *first_len_ptr = 19;
      else
        *first_len_ptr = temp;
     //VIM_DBG_VAR(*first_len_ptr);
    }
    else
    {
      *first_len_ptr = 0;
    }
  }
  else
  {
    *first_len_ptr = 0;
    temp = vim_atol(pan_mask);
    if ( temp > 19 )
      *last_len_ptr = 19;
    else
      *last_len_ptr = temp;
  }
  return VIM_ERROR_NONE;
}

void receipt_format_pan(VIM_CHAR *string_buf, const VIM_CHAR *pan, 
  VIM_UINT8 pan_len, VIM_TEXT* pan_mask, VIM_CHAR entry_mode)
{
  VIM_CHAR format_buf[RECEIPT_LINE_MAX_WIDTH+1] = {0};
  VIM_UINT8 last_len;
  VIM_UINT8 first_len;
  if (pan && pan_len)
  {
      vim_mem_clear(format_buf, VIM_SIZEOF(format_buf));
      
      if( pan_mask_parse(pan_mask, &first_len, &last_len) == VIM_ERROR_NONE )
      {
          if ( first_len == 0 )		// RDD - WOW Customer Receipt Format
          {
            vim_snprintf( format_buf, VIM_SIZEOF(format_buf), ".............%%%d.%ds %c", last_len, last_len, entry_mode );
            
            last_len = VIM_MIN(last_len, pan_len);
            
            vim_snprintf(string_buf, terminal.configuration.prn_columns, format_buf, &pan[pan_len-last_len]);
          }
          else						// RDD - WOW Merchant Receipt Format
          {
			  // RDD 170512 SPIRA:5518 PAD RESPONSE TO TXN - needs spaces between pan digits and dots
			if( entry_mode == '1' )
				vim_snprintf(format_buf, VIM_SIZEOF(format_buf), "%%%d.%ds ... %%%d.%ds", first_len, first_len, last_len, last_len);
			else
			  // RDD 170412 SPIRA:5319 SAF PRINT RECEIPT - NO SPACES
				vim_snprintf(format_buf, VIM_SIZEOF(format_buf), "%%%d.%ds...%%%d.%ds", first_len, first_len, last_len, last_len);

            last_len = VIM_MIN(last_len, pan_len);

            vim_snprintf(string_buf, terminal.configuration.prn_columns, format_buf, pan, &pan[pan_len-last_len]);
          }
      }
  }
}

static void receipt_add_card_number(VIM_BOOL merchant_receipt, VIM_BOOL duplicate_receipt, transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *RightPtr )
{
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_UINT8 pan_len;
  VIM_CHAR pan[21];
  VIM_CHAR entry_mode = 'T';
 
  if (transaction->card.type == card_none)
    return;

  vim_mem_set(string_buf, ' ', terminal.configuration.prn_columns);
  string_buf[terminal.configuration.prn_columns] = 0;

  vim_mem_clear( pan, VIM_SIZEOF(pan));

  card_get_pan(&transaction->card, pan);

  pan_len = vim_strlen(pan);
  
  if ( (pan_len < MIN_PAN_LENGTH) || (pan_len > MAX_PAN_LENGTH) )
    return;

  switch (transaction->card.type)
  {
    case card_manual:
        entry_mode = 'K';             
        if( gift_card( transaction ))
        {
            if (txn.card_state == card_capture_new_gift)     // To designate new ( Avalon ) flow
            {
                entry_mode = *QC_EntryMode(transaction);
            }
            else
            {
                /* manually keyed */
                if (IsBGC())
                    entry_mode = 'B';
            }
        }
        break;

    case card_chip:
      /* C for chip */
      entry_mode = 'D';
      break;
    case card_mag:
      /* F for magnetic stripe fallback (technical fallback only) */
        entry_mode = 'S';
      break;
    case card_picc:
        /* T for Tapped contactless chip */
        entry_mode = 'T';
        break;

    case card_qr:
        /* Q for QR-code payment card */
        entry_mode = 'Q';
        break;
    default:
      /* default to T for Tapped contactless chip */
      entry_mode = 'T';
      break;
  }

  // RDD 130212: PS says copy 5100....
  if( transaction->type == tt_mini_history )
  {
	  receipt_format_pan( string_buf, pan, pan_len, "6-4", ' ');
  }
  
  // RDD 280615 SPIRA:8776 DUPLICATE COMPLETIONS DOING 6-4
  //else if(( merchant_receipt == VIM_FALSE ) && transaction_get_status( transaction, st_completion ))
  else if(( merchant_receipt == VIM_FALSE ) && transaction_get_status( transaction, st_completion ) && ( is_integrated_mode()))
  {
	  receipt_format_pan( string_buf, pan, pan_len, "6-4", ' ' );
  }
  else
  {
	  receipt_format_pan( string_buf, pan, pan_len, "0-4", entry_mode );
  }
  
  if (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH)
  {
      vim_strcpy( RightPtr, rc_item_R( "CARD:", string_buf, vim_strlen(string_buf)));
  }
  else
  {  
      receipt_add_right_justified_item("CARD:", string_buf, vim_strlen(string_buf), receipt);
  }

  // RDD 070512 SPIRA 5448 Epan always printed for Manual EFTPOS voucer transactions
  //if( txn_is_customer_present(&txn) == VIM_FALSE )
  //if( txn.type == tt_completion )

  // RDD 280814 ALH Checkout can use epan on the receipt
  // RDD 160115 SPIRA:8278 EPAN on completion not pre-auth
  //if(( transaction->type == tt_preauth )
  // RDD 180315 
  if(( transaction->type == tt_checkout ) || (( transaction->type == tt_preauth ) && merchant_receipt ))
  {	
	  receipt_add_ePan( receipt, transaction );
  }
  // RDD 161214 SPIRA:8278 CR EPAN on merchant receipt required for standalone
  // RDD 301015 SPIRA:8823 Epan for all merchant receipts

  // 210818 RDD JIRA WWBP-316 no standalone features on Integrated
  else if(( is_standalone_mode()) && ( !POS_CONNECTED ))
  {  
	  DBG_POINT;
#if 0
	  if( transaction_get_status( transaction, st_offline ) && merchant_receipt )
#else	
	  // RDD 131115 add duplicate receipt to the list for epan
	  if( merchant_receipt || duplicate_receipt)
#endif
	  {	  
		  DBG_POINT;
		  receipt_add_ePan( receipt, transaction );
	  }
  }
  // Integrated mode stays as it was....
  // RDD 070519 v671 JIRA WWBP-621 Epan added for all Offline merchant copies
  else if( transaction_get_status( transaction, st_offline ) /* && !merchant_receipt  */)
  {
	  // RDD 211112 SPIRA:6338 EPAN or TOKEN is printed separatly for SAF prints  
	  DBG_POINT;
	  if( transaction->type != tt_mini_history )
		receipt_add_ePan( receipt, transaction );
  }
}



static void receipt_add_amount(const VIM_CHAR *label, VIM_BOOL add_negative_sign, VIM_UINT64 amount, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr )
{
  VIM_CHAR amount_string[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR currency_buff[5+1];

  vim_mem_set(string_buf, ' ', terminal.configuration.prn_columns);
  string_buf[terminal.configuration.prn_columns] = 0;

  vim_mem_set(amount_string, ' ', terminal.configuration.prn_columns);
  amount_string[terminal.configuration.prn_columns] = 0;

  vim_strcpy( currency_buff, (WOW_NZ_PINPAD)?"NZ$":"$" );

  if (add_negative_sign)
    vim_snprintf(amount_string, sizeof(amount_string), "-%s%d.%02d", currency_buff, (VIM_UINT32) (amount / 100), (VIM_UINT32) (amount % 100));
  else
    vim_snprintf(amount_string, sizeof(amount_string), "%s%d.%02d", currency_buff, (VIM_UINT32) (amount / 100), (VIM_UINT32) (amount % 100));

  if( PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH )
  {
      receipt_add_right_justified_item(label, amount_string, vim_strlen(amount_string), receipt);
  }
  else
  {
      rc_item(label, amount_string, vim_strlen(amount_string), Ptr);
  }

}

#if 0
static void receipt_add_count(VIM_CHAR *label, VIM_UINT16 count, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR count_string[RECEIPT_LINE_WIDTH+1];
  VIM_CHAR string_buf[RECEIPT_LINE_WIDTH+1];
  VIM_UINT8 len;

  vim_mem_set(string_buf, ' ', RECEIPT_LINE_WIDTH);
  string_buf[RECEIPT_LINE_WIDTH] = 0;
  

  vim_mem_set(count_string, ' ', RECEIPT_LINE_WIDTH);
  count_string[RECEIPT_LINE_WIDTH] = 0;

  vim_snprintf(count_string, sizeof(count_string), "%05d", count);
  len = vim_strlen(count_string);

  vim_mem_copy(string_buf, label, vim_strlen(label));
  vim_mem_copy(&string_buf[RECEIPT_LINE_WIDTH - len], count_string, len);
  
  receipt_add_string(string_buf, receipt);
}
#endif

VIM_BOOL is_approved_txn(transaction_t *transaction)
{  
	//VIM_DBG_ERROR( transaction->error );
	DBG_STRING( transaction->host_rc_asc );
	VIM_DBG_ERROR(transaction->error);
	if (transaction->error == ERR_SIGNATURE_DECLINED) {
		return VIM_FALSE;
	}

	// RDD 200415 SPIRA:8590 completed Checkouts need to return approved to be included in shift totals  
	if( transaction->type == tt_checkout )  
	{	
		// Note that Z1 will generate a host response and we need to exclude these
		if(( ValidateAuthCode(txn.auth_no) && ( !vim_strncmp( transaction->host_rc_asc, "00", 2 ))))
		{
			return VIM_TRUE;	
		}
		else	
		{
			// Return  signature required if bit was previously set.
			if (transaction_get_status(transaction, st_signature_required))
			{
				return VIM_TRUE;
			}
			else
			{
				return VIM_FALSE;
			}
		}
	}

	if ( transaction->error == ERR_SIGNATURE_APPROVED_OFFLINE )
	{  	
		return VIM_TRUE;
	}
	
	// RDD 160115 SPIRA:8342 - Diners does not return DE38 and the field is actually optional in AS2805 so we shouldn;t generate an error if its not there
#if 0
	if(( transaction->type == tt_preauth ) && ( !ValidateAuthCode( transaction->auth_no )))	
	{		  
		if( transaction->error == VIM_ERROR_NONE )	
		{		
			transaction->error = VIM_ERROR_AS2805_FORMAT;	  
		}	  
		return VIM_FALSE;  
	}
#endif

	if(( transaction->error == ERR_WOW_ERC )||( transaction->error == VIM_ERROR_NONE ))
	{
	    if ( vim_mem_compare(transaction->host_rc_asc, "08", 2) == VIM_ERROR_NONE )
		return VIM_TRUE;
	
	    if ( vim_mem_compare(transaction->host_rc_asc, "00", 2) == VIM_ERROR_NONE )
		return VIM_TRUE;	
	}  		
	return VIM_FALSE;
}

#define HALF_RULE "------------"
static VIM_ERROR_PTR receipt_add_horizontal_rule(VIM_PCEFTPOS_RECEIPT* psReceipt, VIM_BOOL fLeft, VIM_BOOL fRight)
{
	return receipt_add_right_justified_item(fLeft ? HALF_RULE : "", fRight ? HALF_RULE : "", fRight ? vim_strlen(HALF_RULE) : 0, psReceipt);
}

static VIM_ERROR_PTR receipt_add_txn_amount(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr )
{  
    /*  Only print base and tip for non retail/lodging terminals */

	// RDD 010716 SPIRA:8977 add ACQ IIN for UPI
	VIM_UINT32 CpatIndex = transaction->card_name_index;
    VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
	// RDD 140212 - Setup the type as original toye if a completion
	txn_type_t type = ( (transaction->type == tt_completion) || (transaction->type == tt_mini_history) ) ? transaction->original_type : transaction->type;

	if( CardNameIndex == UPI_CARD_INDEX )
	{
		VIM_CHAR string_buf[50];
		// RDD 020716 SPIRA:8977
		//vim_strcpy( string_buf, "0062800000" );
		// RDD 200716 changed value to 30620036
		vim_strcpy( string_buf, "0030620036" );

		receipt_add_right_justified_item( "ACQ IIN", string_buf, vim_strlen(string_buf), receipt );
		//receipt_add_blank_lines(1, receipt);
	}

	if(( type == tt_balance ) && ( transaction->LedgerBalance >= 0 ))
	{
		receipt_add_amount("CARD BALANCE", VIM_FALSE, transaction->LedgerBalance, receipt, Ptr);	
	}


  else if(( type != tt_refund ) && ( type != tt_gift_card_activation ) && ( type != tt_moto_refund ))
  {
	if( type == tt_deposit )
	{
		receipt_add_amount("DEPOSIT", VIM_FALSE, transaction->amount_purchase, receipt, Ptr);	
	}
    else
	{
		if( gift_card(transaction) )
		{
            if (transaction->type == tt_gift_card_activation)
            {
                receipt_add_amount("ACTIVATION", VIM_FALSE, transaction->amount_purchase, receipt, Ptr);
            }
            else if( transaction->type == tt_sale )
            {
                receipt_add_amount("REDEMPTION", VIM_FALSE, transaction->amount_purchase, receipt, Ptr );
            }
		}
		else
		{
			// RDD 020512 SPIRA:5422 No purchase line for cashout if purchase amt is zero
			if( transaction->amount_purchase )
			{
				if( transaction->type == tt_preauth )
				{
					receipt_add_amount("PRE-AUTH", VIM_FALSE, transaction->amount_purchase, receipt, Ptr);
				}
				else
				{
					receipt_add_amount("PURCHASE", VIM_FALSE, transaction->amount_purchase, receipt, Ptr);
                    // RDD 270521 JIRA PIRPIN-1076 Narrow format receipt missing CASHOUT when purchase with CASHOUT
                    if(( transaction->amount_cash )&&( Ptr==VIM_NULL))
                    {
                        receipt_add_amount("CASH OUT", VIM_FALSE, transaction->amount_cash, receipt, Ptr);
                    }
				}
			}
            else if ((transaction->amount_cash) && (transaction->type == tt_cashout) && (Ptr==VIM_NULL))
            {
                receipt_add_amount("CASH OUT", VIM_FALSE, transaction->amount_cash, receipt, Ptr);
            }

		}
	}
#if 1   // RDD 050521 JIRA PIRPIN-1056 CASH OUT printed twice on old-style receipts
    // RDD 101014 added
    if (Ptr == VIM_NULL)
    {
        if ( transaction->amount_tip )
            receipt_add_amount( "TIP", VIM_FALSE, transaction->amount_tip, receipt, Ptr );
		if ((ApplySurcharge(transaction, VIM_FALSE) == VIM_TRUE) && (transaction->amount_surcharge))
		{
			receipt_add_amount("SURCHARGE", VIM_FALSE, transaction->amount_surcharge, receipt, Ptr);
		}
    }
#endif
  }
  else // refund activation
  {
	if( txn.type == tt_gift_card_activation )
	{
		receipt_add_amount("ACTIVATION", VIM_FALSE, transaction->amount_refund, receipt, Ptr);
	}
	else
	{
		// RDD 221112 Noticed that Voucher Refunds had zero amount in refund field on receipt because completion amt. is in amt_purch by default
		receipt_add_amount("REFUND", VIM_FALSE, transaction->amount_total, receipt, Ptr );
	}
  }
  return VIM_ERROR_NONE;
}


static VIM_BOOL receipt_add_balances(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr)
{
    // RDD 280720 - Trello #120 NZ Christmas Club - add DE59 Balance to receipt if available
    VIM_CHAR label_buf[RECEIPT_LINE_MAX_WIDTH + 1] = { 0 };
    VIM_BOOL PrintBalance = VIM_TRUE;

    if ((IS_XMAS_CLUB) && (transaction->ClearedBalance >= 0))
    {
        vim_snprintf(label_buf, VIM_SIZEOF(label_buf), "BALANCE %s", "");
        receipt_add_amount(label_buf, VIM_FALSE, transaction->ClearedBalance, receipt, Ptr );
    }
    else if ((transaction->type == tt_balance) && (transaction->ClearedBalance >= 0))
    {
        vim_snprintf(label_buf, VIM_SIZEOF(label_buf), "AVAILABLE %s", "");
        receipt_add_amount(label_buf, VIM_FALSE, transaction->ClearedBalance, receipt, Ptr );
    }
    else
    {
        PrintBalance = VIM_FALSE;
    }
    return PrintBalance;
}

static VIM_ERROR_PTR receipt_add_total( transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr )
{
    VIM_CHAR label_buf[RECEIPT_LINE_MAX_WIDTH + 1] = { 0 };
    vim_snprintf(label_buf, VIM_SIZEOF(label_buf), "TOTAL %s", "");
    receipt_add_amount(label_buf, VIM_FALSE, transaction->amount_total, receipt, Ptr );
    return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR receipt_add_amounts(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
    VIM_CHAR *Ptr = VIM_NULL;
    receipt_add_txn_amount(transaction, receipt, Ptr );
    if( !receipt_add_balances(transaction, receipt, Ptr ) )
    {
        receipt_add_horizontal_rule(receipt, VIM_FALSE, VIM_TRUE);
        receipt_add_total(transaction, receipt, Ptr);
    }
    return VIM_ERROR_NONE;
}




static void print_aid( transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr )
{
	VIM_CHAR string_buf[(2*RECEIPT_LINE_MAX_WIDTH)+1];
	VIM_CHAR line_1[RECEIPT_LINE_MAX_WIDTH+1];
	VIM_CHAR line_2[RECEIPT_LINE_MAX_WIDTH+1];	// RDD 240316 Need bigger buffer for DPAS test E2E_CL_07 ( 32 digit AID )
	VIM_SIZE len;
	VIM_TLV_LIST tag_list;
	VIM_UINT8 Aid[VIM_ICC_AID_LEN];
	VIM_BOOL GotTag = VIM_FALSE;

	vim_mem_clear( Aid, VIM_SIZEOF(Aid) );

    // RDD 170221 - ensure EMV tags only printed when required ( even if present in the system )
    if (!SendEMVTags(transaction))
    {
        return;
    }


	// RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data  
	// RDD 191014 SPIRA:8205 EPAL CTAV Scheme Cert Test AS-03 Need full AID in some cases of partial selection can be longer than what is stored wrt indexing
	if( vim_tlv_open( &tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer), VIM_FALSE ) == VIM_ERROR_NONE )	  
	{		  
		if( vim_tlv_search( &tag_list, VIM_TAG_EMV_DEDICATED_FILE_NAME ) == VIM_ERROR_NONE )  
		{      
			vim_tlv_get_bytes( &tag_list, &Aid, VIM_SIZEOF(Aid) );	

			vim_tlv_tag_get_size(VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_ICC_AID_LEN, &len);

			GotTag = VIM_TRUE;
		}
	}
  	    
	vim_mem_clear( string_buf, VIM_SIZEOF( string_buf ) );

	/* AID for contacless card */
	// RDD 211112 SPIRA:6339 - Simplify getting AID to print
	// RDD 290113 SPIRA:6565 - Need full AID from Tag to pass ADVT tests 5 and 6 so need to restore the original code for these cases
	if( transaction->type == tt_mini_history )
	{
		len = terminal.emv_applicationIDs[transaction->aid_index].aid_length;
		hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, string_buf, len*2 );
		DBG_DATA( string_buf, len*2 );
	}
	else
	{
		if( GotTag )
		{
			len = 2 * tag_list.current_item.length;
			hex_to_asc( Aid, string_buf, len );
		}
		else
		// RDD 270814 AID not printing for diag receipt of docked cards CTLS OK so just use simple code
        // RDD 080221 Huh ? Now printing AID for swipe....  as aid_index = 0 means VISA 
		{
			//len = terminal.emv_applicationIDs[transaction->aid_index].aid_length;
			//hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].emv_aid, string_buf, len*2 );
            DBG_MESSAGE("No Aid to print!!");
            return;
        }
	}

	if( len > (terminal.configuration.prn_columns-4) ) 
	{
		vim_mem_clear(line_1, sizeof(line_1));
		vim_mem_clear(line_2, sizeof(line_2));
		vim_mem_copy(line_1, string_buf, len/2 );	
		vim_mem_copy(line_2, &string_buf[len/2], len-(len/2) );
	
		receipt_add_right_justified_item("AID ", line_1, len/2, receipt );
		receipt_add_right_justified_item(" ", line_2, len-(len/2), receipt);    
	}
	else if( Ptr != VIM_NULL )
	{
        rc_item("AID", string_buf, vim_strlen(string_buf), Ptr);
	} 
    else
    {
        receipt_add_right_justified_item("AID", string_buf, vim_strlen(string_buf), receipt);
    }
}


static void print_cryptograms( transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *Ptr )
{
	VIM_TLV_LIST tag_list;	
	VIM_UINT8 cid;
	VIM_CHAR* label = VIM_NULL;	
	VIM_CHAR string_buf[100];
	/* Add first GenAC Crypto if present */	
	// RDD 120712 - Noticed no ARQC printed on Z4 failures		
	//if( (vim_strlen(transaction->cid) && (txn.error == /*ERR_POWER_FAIL*/VIM_ERROR_NONE) ))
    vim_mem_clear(string_buf, sizeof(string_buf));

    if (!SendEMVTags(transaction))
    {
        return;
    }

	if(( vim_strlen(transaction->cid )) && (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH ))
	{		            
		receipt_add_right_justified_item(transaction->cid, transaction->crypto, vim_strlen(transaction->crypto), receipt);		
	}		
    else if( vim_strlen(transaction->crypto ))
    {
        rc_item(transaction->cid, transaction->crypto, vim_strlen(transaction->crypto), string_buf);
        receipt_add_right_justified_item(string_buf, "", 24, receipt);
    }

	// RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data
	if( vim_tlv_open(&tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer), VIM_FALSE) != VIM_ERROR_NONE )			
		return;

	/* CRYPTOGRAM BINARY */
	if( VIM_ERROR_NONE == vim_tlv_search( &tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA ))		
	{
		VIM_BOOL crypto = VIM_TRUE;

		vim_tlv_get_bytes(&tag_list, &cid, 1);			
		if (EMV_CID_TC(cid))			
			label = "TC";			
		else if (EMV_CID_AAC(cid))			
			label = "AAC";			
		else if (EMV_CID_ARQC(cid))			
			label = "ARQC";			
		else
			crypto = VIM_FALSE;		

		/* CID BINARY */    	
		if( crypto && VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM) )        	
		{     		
			vim_mem_clear(string_buf, sizeof(string_buf));     				
			hex_to_asc(tag_list.current_item.value, string_buf, VIM_MIN(tag_list.current_item.length*2, sizeof(string_buf)-1));		  		
			// RDD 250113 SPIRA:6530 Sometimes print second ARQC on receipt after EFB - if first two chars are same, don't print		  
		
            if (vim_strncmp(transaction->crypto, string_buf, 2))
            {
                if (Ptr == VIM_NULL)
                {
                    receipt_add_right_justified_item(label, string_buf, vim_strlen(string_buf), receipt);
                }
                else
                {
                    rc_item(label, string_buf, vim_strlen(string_buf), Ptr );
                }
            }
		}
	}    
}




static void print_tvr(transaction_t *transaction, VIM_BOOL succeed, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL merchant_receipt, VIM_CHAR *Ptr )
{       
    VIM_TLV_LIST tag_list;
    VIM_CHAR string_buf[50];

    if (!SendEMVTags(transaction))
    {
        return;
    }

    if (vim_tlv_open(&tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer), VIM_FALSE) != VIM_ERROR_NONE)
            return;

    if (vim_tlv_search(&tag_list, VIM_TAG_EMV_TVR) == VIM_ERROR_NONE)        
    {            
        vim_mem_clear( string_buf, sizeof( string_buf ));    
        hex_to_asc( tag_list.current_item.value, string_buf, VIM_MIN( tag_list.current_item.length * 2, sizeof(string_buf) - 1 ));          
        if( Ptr == VIM_NULL )        
            receipt_add_right_justified_item("TVR", string_buf, vim_strlen(string_buf), receipt);        
        else
            rc_item("TVR", string_buf, vim_strlen(string_buf), Ptr );
    }    
}




static void receipt_add_emv_data(transaction_t *transaction, VIM_BOOL succeed, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL merchant_receipt)
{
    VIM_CHAR *Ptr = VIM_NULL;

	if (( card_chip == transaction->card.type ) || ( card_picc == transaction->card.type ))  
	{	
		print_aid( transaction, receipt, Ptr );
		// RDD 160522 JIRA PIRPIN-1610 - missing TVR from short receipt
		print_tvr(transaction, VIM_TRUE, receipt, merchant_receipt, VIM_NULL);

		print_cryptograms( transaction, receipt, Ptr );
		    
		// RDD 010716 SPIRA:8976 TVR addded for all receipts. 
		// RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data
 	
		// RDD - for WOW we print the ARQC and the AAC (if available) on the audit receipt only. Customer receipt contains only the AID
		//if(( !merchant_receipt ) /*&& ( !transaction_get_status( transaction, st_partial_emv ))*/)	  
		//{
		//	VIM_BOOL crypto = VIM_TRUE;		  
		//}  
	}
}






#if 0
static void receipt_add_txn_ref(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR string_buf[RECEIPT_LINE_WIDTH+1];
  VIM_CHAR ref_num[17];

  if ((transaction->error == VIM_ERROR_NONE) && !transaction->offline && TERM_FUNC_PRINT_REF_NUM(transaction->merchant))
  {
    calc_txn_reference_number(transaction, ref_num);
    vim_snprintf(string_buf, sizeof(string_buf), "%4.4s %4.4s %4.4s %4.4s", &ref_num[0], &ref_num[4], &ref_num[8], &ref_num[12]);
    receipt_add_string(string_buf, receipt);
    receipt_add_blank_lines(1, receipt);
  }
}
#endif
static void receipt_add_signature_panel(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
    VIM_SIZE blank_lines = (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH) ? 2 : 4;
    if (!is_approved_txn(transaction))
        return;

    if (txn_requires_signature(transaction) || txn_requires_tip_signature(transaction))
    {
        receipt_add_blank_lines(1, receipt);        
        receipt_add_right_justified_item("SIGNATURE:", "", 0, receipt);        
        receipt_add_blank_lines(blank_lines, receipt);
    }
}



VIM_CHAR *ApprovedCodes[] = { "00","08","Y1","Y2","Y3" };

VIM_BOOL TxnWasApproved( VIM_CHAR *RC ) 
{
	VIM_SIZE n,l = VIM_LENGTH_OF(ApprovedCodes);
	for( n=0; n<l; n++ )
	{
		if( !vim_strncmp( RC, ApprovedCodes[n], 2 ))
			return VIM_TRUE;
	}
	return VIM_FALSE;
}


/* ------------------------------------------------------------------------------------------------ */
/* Add the description of the error   */
/* ------------------------------------------------------------------------------------------------ */
static void receipt_add_response_description(VIM_ERROR_PTR error, VIM_CHAR asc_error[2], VIM_CHAR auth_code[7],
  transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL settlement, VIM_BOOL Diag, VIM_CHAR *Ptr )
{
  VIM_CHAR pos_line_1[21], line_1_length;
  error_t error_data;
  VIM_CHAR rc[10];
  VIM_SIZE stan = 0;
  //VIM_CHAR TransactionResult[22+1]; RDD 260615 caused overflow
  VIM_CHAR TransactionResult[24+1];

  vim_mem_clear( rc, VIM_SIZEOF(rc) );
 
  // RDD 161214 SPIRA:8277 Incorrect Response Data Printed on Receipt - use init.host_response for logon
  if( transaction == VIM_NULL )
  {
	  error_code_lookup( error, asc_error, &error_data );  
  }
  else
  {		
	  stan = transaction->stan;
	  error_code_lookup( error, transaction->host_rc_asc, &error_data );   

	  if ( transaction_get_status( transaction, st_efb ) && is_approved_txn(transaction) )
	  //if ( transaction_get_status( transaction, st_needs_sending ) ) // RDD 140312 - don't want the '*' on Z1s etc....	
	  {	  
		  // RDD - special case for offline approved transactions - add a '*' to the receipt code...
		  if( is_full_emv_offline_txn(transaction) == VIM_FALSE )	  
		  {		
			  vim_strcpy( rc, "*" );	  
		  }  
	  }
  }

  vim_sprintf( TransactionResult, "%s", error_data.pos_text );	

  // add a stan to standalone if there is one
  if( IS_STANDALONE && stan )
  {
	  // 210818 RDD JIRA WWBP-316 no standalone features on VX680
	  // RDD 180222 JIRA PIRPIN-1441 ensure that transaciton->stan is not incremented permanenty as this affect some receipts.
	  //if( !POS_CONNECTED )
	  {
		  vim_sprintf(TransactionResult, "%s %d", error_data.pos_text, transaction->stan);
	  }
  }
  vim_strcat( rc, error_data.ascii_code );

  if( TxnWasApproved( error_data.ascii_code ) )
  {
      if( Ptr == VIM_NULL )	    
          receipt_add_right_justified_item( TransactionResult, rc, vim_strlen(rc), receipt);
      else
          rc_item( TransactionResult , rc, vim_strlen(rc), Ptr );
  }
  else
  {
	  /*Get the POS display message up to the /n*/
      DBG_POINT;
	  vim_mem_clear(pos_line_1, VIM_SIZEOF(pos_line_1));
	  line_1_length = vim_strcspn(error_data.terminal_line2, "\n\0");
	  vim_mem_copy(pos_line_1, error_data.terminal_line2, VIM_MIN(line_1_length, 20));
      if( Ptr == VIM_NULL )	    
          receipt_add_right_justified_item( pos_line_1, rc, vim_strlen(rc), receipt);
      else
      {
          //VIM_CHAR Buffer[24+1];
          //vim_sprintf(Buffer, "%s %s", pos_line_1, rc);
          rc_item(pos_line_1, rc, vim_strlen(rc), Ptr);
          //return;
      }
	  	  
	  // RDD 290115 SPIRA:8365 - they don't want "NO BANK DO MANUAL" On the ALH receipts if a 91 is received and not approved offline
	  if( is_standalone_mode() && ( !vim_strncmp( error_data.ascii_code, "91", 2 )))		
		  receipt_add_right_justified_item( "BANK UNAVAILABLE", "", 0, receipt );
	  else if(IsEverydayPay(transaction) && ( !vim_strncmp( error_data.ascii_code, "01", 2 ) || !vim_strncmp(error_data.ascii_code, "RI", 2 )) && 
		  transaction->print_receipt ) //For the parent EDP transaction, use special text for declined
	  {
		  if (!vim_strncmp(error_data.ascii_code, "01", 2))
				receipt_add_right_justified_item(TERM_QR_DECLINED_RCT_TEXT, "", 0, receipt);
		  else
				receipt_add_right_justified_item(DEFAULT_QR_RESTITEM_RCT_TEXT, "", 0, receipt);
	  }
	  else
		  receipt_add_right_justified_item( TransactionResult, "", 0, receipt );

      // RDD 111220 JIRA PIRPIN-947 Add Error Beep for Declined Transaction
	  vim_sound(VIM_TRUE);

	  // JIRA PIRPIN-1221. Extra alerts that targets unattended, integrated mode
	  vim_event_sleep(500);
	  vim_sound(VIM_TRUE);
	  vim_event_sleep(500);
	  vim_sound(VIM_TRUE);
  }   
}


VIM_ERROR_PTR set_settlement_date_to_ascii(VIM_UINT8 *settle_date, VIM_CHAR *buf)
{
  VIM_RTC_TIME date;
  VIM_UINT8 year, month, settle_month;

  bcd_to_asc(&settle_date[1], &buf[0], 2);    /*  DD */
  bcd_to_asc(&settle_date[0], &buf[2], 2);    /*  MM */
  settle_month = bcd_to_bin(&settle_date[0], 2);

  vim_rtc_get_time(&date);  
  month = date.month;
  year = date.year%100;
 
  if ((month == 12) && (settle_month == 1)) /* Check if year is next year */
    year++;
  
  bin_to_asc(year, &buf[4], 2);  /*  YY */

  return VIM_ERROR_NONE;
}



void receipt_add_net_amount(VIM_TEXT *label, VIM_INT64 amount, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR amount_string[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];

  vim_mem_set(string_buf, ' ', terminal.configuration.prn_columns);
  string_buf[terminal.configuration.prn_columns] = 0;

  vim_mem_set(amount_string, ' ', terminal.configuration.prn_columns);
  amount_string[terminal.configuration.prn_columns] = 0;

  if (amount >= 0)  
    vim_snprintf(amount_string, sizeof(amount_string), "%c%d.%02d ", terminal_currency_symbol,(VIM_INT32)(amount/ 100), (VIM_UINT32)vim_math_abs(amount % 100));
  else
    vim_snprintf(amount_string, sizeof(amount_string), "%c%d.%02d-", terminal_currency_symbol,(VIM_INT32)vim_math_abs(amount/ 100), (VIM_UINT32)vim_math_abs(amount % 100));

  vim_strcpy( string_buf, label );
  receipt_add_right_justified_item( string_buf, amount_string, vim_strlen(amount_string), receipt );

}

static void receipt_add_count_amount(VIM_TEXT *label, VIM_UINT16 count, VIM_INT64 amount, VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR label_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR amount_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_BOOL negative = VIM_FALSE;

  vim_mem_set(label_buf, ' ', terminal.configuration.prn_columns);
  label_buf[terminal.configuration.prn_columns] = 0;

  vim_mem_set(amount_buf, ' ', terminal.configuration.prn_columns);
  amount_buf[terminal.configuration.prn_columns] = 0;

  if (amount < 0)
  {
    amount = vim_math_abs(amount);

    negative = VIM_TRUE;
  }

  if ( negative ) 
    vim_snprintf(amount_buf, sizeof(amount_buf), "%c%d.%02d-", terminal_currency_symbol, 
    (VIM_INT32)(amount/ 100), (VIM_UINT32) (amount % 100));
  else
    vim_snprintf(amount_buf, sizeof(amount_buf), "%c%d.%02d ", terminal_currency_symbol, 
    (VIM_INT32)(amount/ 100), (VIM_UINT32) (amount % 100));

  vim_sprintf( label_buf, "%3.3d %s", count, label );

  receipt_add_right_justified_item(label_buf, amount_buf, vim_strlen(amount_buf), receipt);
}


void receipt_add_software_version(VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *text, VIM_UINT8 file_index )
{
//    VIM_CHAR left_string[RECEIPT_LINE_WIDTH+1];
    VIM_CHAR right_string[RECEIPT_LINE_MAX_WIDTH+1];

	vim_mem_clear( right_string, VIM_SIZEOF(right_string) );

    bin_to_asc(terminal.file_version[file_index], right_string, WOW_FILE_VER_BCD_LEN*2);
  
    receipt_format_line( receipt, text, right_string );
}



// RDD - 021211: Re-written 
void print_settlement_settlement_date( VIM_CHAR *text, VIM_PCEFTPOS_RECEIPT *receipt )
{
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH + 1];
  VIM_RTC_TIME today;
  VIM_UINT8 year, month, settle_day, settle_month;
  VIM_CHAR *Ptr = string_buf;

  vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));

  settle_month = bcd_to_bin( settle.settle_date, 2 );    /*  MM */
  settle_day = bcd_to_bin( &settle.settle_date[1], 2 );    /*  DD */

  /* read current date and time */
  vim_rtc_get_time(&today);

  month = today.month;
  year = today.year%100;
 
  // Year does not come from the host so check the PinPad year does not need to be rolled over before using it
  if ((month == 12) && (settle_month == 1)) 
    year++;

  vim_snprintf( Ptr, VIM_SIZEOF(string_buf), "%02d/%02d/%02d", settle_day, settle_month, year );
  
  receipt_add_right_justified_item( text, string_buf, vim_strlen(string_buf), receipt );
  receipt_add_blank_lines(1, receipt);
}




void PrintCAPKTable(VIM_BOOL internal_printer)
{
  VIM_PCEFTPOS_RECEIPT receipt;
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH + 1];
  VIM_SIZE i, j;
  
  //display_screen(DISP_PRINTING_PLEASE_WAIT, VIM_PCEFTPOS_KEY_NONE);

  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));
  
  receipt_add_centered_string("CA PUBLIC KEYS", &receipt);
  receipt_add_blank_lines(1, &receipt);
  
  for (i = 0; i < MAX_EMV_PUBLIC_KEYS; i++)
  {
    if (0 == terminal.emv_capk[i].capk.RID.value[0])
      break;
    
    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    hex_to_asc(terminal.emv_capk[i].capk.RID.value, string_buf, VIM_SIZEOF(VIM_ICC_RID) * 2);
    receipt_add_right_justified_item("RID:", string_buf, vim_strlen(string_buf), &receipt);

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    bin_to_asc(terminal.emv_capk[i].capk.index, string_buf, 3);
    //VIM_DBG_STRING(string_buf);
    vim_snprintf(&string_buf[vim_strlen(string_buf)], 8, " (0x%X)", terminal.emv_capk[i].capk.index);
    //VIM_DBG_STRING(string_buf);
    receipt_add_right_justified_item("INDEX:", string_buf, vim_strlen(string_buf), &receipt);

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    hex_to_asc(terminal.emv_capk[i].capk.exponent, string_buf, 2);
    receipt_add_right_justified_item("EXPONENT:", string_buf, vim_strlen(string_buf), &receipt);

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    bin_to_asc(terminal.emv_capk[i].capk.exponent_size, string_buf, 2);
    receipt_add_right_justified_item("EXPONENT SIZE:", string_buf, vim_strlen(string_buf), &receipt);

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    vim_snprintf(string_buf, VIM_SIZEOF(string_buf), "%02d/%02d/%04d",
                              terminal.emv_capk[i].capk.expiry_date.day,
                              terminal.emv_capk[i].capk.expiry_date.month,
                              terminal.emv_capk[i].capk.expiry_date.year);
    receipt_add_right_justified_item("EXPIRY DATE:", string_buf, vim_strlen(string_buf), &receipt);
    
    receipt_add_string("KEY:", &receipt);
    for (j = 0; j < terminal.emv_capk[i].capk.modulus_size; j += terminal.configuration.prn_columns)
    {
      vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
      hex_to_asc(&terminal.emv_capk[i].capk.modulus[j], string_buf, terminal.configuration.prn_columns);
      receipt_add_string(string_buf, &receipt);
    }

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    hex_to_asc(terminal.emv_capk[i].capk.hash, string_buf, 20);
    receipt_add_string("HASH:", &receipt);
    receipt_add_string(string_buf, &receipt);
    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    hex_to_asc(&terminal.emv_capk[i].capk.hash[10], string_buf, 20);
    receipt_add_string(string_buf, &receipt);

    vim_mem_clear(string_buf, VIM_SIZEOF(string_buf));
    bin_to_asc(terminal.emv_capk[i].algorithm, string_buf, 2);
    receipt_add_right_justified_item("HASH ALGORITHM:", string_buf, vim_strlen(string_buf), &receipt);
    
    receipt_add_dotted_line('-', &receipt);
    
    receipt.type = PCEFTPOS_RECEIPT_TYPE_MERCHANT;
    vim_mem_set(receipt.reference, '0', VIM_SIZEOF(receipt.reference));

	pceft_pos_print( &receipt, VIM_TRUE );
    vim_mem_clear( &receipt, VIM_SIZEOF(receipt) );
  }
}

static VIM_BOOL skip_EMV_tag( VIM_CHAR* tag_name, VIM_SIZE tag_size )
{
  static VIM_TEXT remove_tag_list[][10]={
   VIM_TAG_EMV_AID_ICC,
   VIM_TAG_EMV_ISSUER_ACTION_CODE_DEFAULT,
   VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL,
   VIM_TAG_EMV_ISSUER_ACTION_CODE_ONLINE,
   VIM_TAG_EMV_CVM_RESULTS
  };
  VIM_SIZE cnt, max_cnt = VIM_LENGTH_OF (remove_tag_list);

  // RDD P3 161012 allow us to see CVM results on diag receipt when in XXX mode
  max_cnt = terminal_get_status(ss_wow_basic_test_mode) ? max_cnt-1 : max_cnt;
  for( cnt=0; cnt<max_cnt; cnt++ )
  {
    if( tag_size == vim_strlen( remove_tag_list[cnt] ) &&
        vim_mem_compare( tag_name, remove_tag_list[cnt], tag_size ) == VIM_ERROR_NONE )
    {
      return VIM_TRUE;
    }
  }

  return VIM_FALSE;
}

static void format_EMV_tag( VIM_CHAR* tag_name, VIM_CHAR* tag_value)
{
  if( vim_mem_compare( tag_name, "5A", 2 ) == VIM_ERROR_NONE )
  {
    if( vim_strlen( tag_value ) < 4 ) return;
    vim_mem_copy( tag_value, tag_value+vim_strlen(tag_value) - 4, 4 );
    tag_value[4] = 0x00;
  }
}




static void receipt_add_emv_data_diagnostic( transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL fDiag )
{
  VIM_TLV_LIST tag_list;
  VIM_UINT8 cid;
  VIM_SIZE n,Tags;
  VIM_CHAR* label = VIM_NULL;
  VIM_CHAR string_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR temp_buf[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_ERROR_PTR ret=VIM_ERROR_NONE;
  VIM_CHAR *Ptr = VIM_NULL;

  if (!FullEmv(transaction))
      return;

  /* TC, AAC or ARQC */ 	
  if( fDiag )
	print_aid( transaction, receipt, Ptr );

  // RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data
  if( vim_tlv_open(&tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer),VIM_FALSE) != VIM_ERROR_NONE )
	  return;

  DBG_STRING( transaction->cid );
  DBG_STRING( transaction->crypto );
  VIM_DBG_NUMBER( vim_strlen( transaction->crypto ));
  if(( vim_strlen( transaction->crypto ) ) && ( fDiag ))
	receipt_add_right_justified_item(transaction->cid, transaction->crypto, VIM_MIN( vim_strlen( transaction->crypto ), 16 ), receipt);

  if(( VIM_ERROR_NONE == vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA) ) && ( fDiag ))
  {
      vim_tlv_get_bytes(&tag_list, &cid, 1);
      if (EMV_CID_TC(cid))
        label = "TC";
      else if (EMV_CID_AAC(cid))
        label = "AAC";
      else if (EMV_CID_ARQC(cid))
        label = "ARQC";
      if( vim_tlv_search(&tag_list, VIM_TAG_EMV_CRYPTOGRAM) == VIM_ERROR_NONE )
      {
        vim_mem_clear(string_buf, sizeof(string_buf));
        hex_to_asc(tag_list.current_item.value, string_buf, VIM_MIN(tag_list.current_item.length*2, sizeof(string_buf)-1));

		// RDD 030615 - only add the last cryptogram if its different to the original one (from GENAC1)
		if( vim_strncmp( transaction->crypto, string_buf, 16 ))
		{
			receipt_add_right_justified_item(label, string_buf, vim_strlen(string_buf), receipt);
			/* add blank line */
			if( is_integrated_mode() )
				receipt_add_blank_lines (1, receipt);
		}
      }
    }  

    /* TVR */
    if( vim_tlv_search(&tag_list, VIM_TAG_EMV_TVR) == VIM_ERROR_NONE )
    {
      vim_mem_clear(string_buf, sizeof(string_buf));
      hex_to_asc(tag_list.current_item.value, string_buf, VIM_MIN(tag_list.current_item.length*2, sizeof(string_buf)-1));
      receipt_add_right_justified_item("AUTH TVR", string_buf, vim_strlen(string_buf), receipt);
      /* add blank line */
	  if( is_integrated_mode() )
		receipt_add_blank_lines (1, receipt);
    }

    /* get AID index in table */
    //ret = search_EMVAPP(aid.aid, aid.aid_size, &index);

    /* TAC */
    if(( ret == VIM_ERROR_NONE ) && ( fDiag ))
    {
      receipt_add_string ("EMV TACS", receipt);
      vim_mem_clear(temp_buf, sizeof(temp_buf));
      hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].tac_default, temp_buf, 10);
      receipt_add_right_justified_item ("DEFAULT", temp_buf, vim_strlen(temp_buf), receipt);
      vim_mem_clear(temp_buf, sizeof(temp_buf));
      hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].tac_denial, temp_buf, 10);
      receipt_add_right_justified_item ("DENIAL", temp_buf, vim_strlen(temp_buf), receipt);
      vim_mem_clear(temp_buf, sizeof(temp_buf));
      hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].tac_online, temp_buf, 10);
      receipt_add_right_justified_item ("ONLINE", temp_buf, vim_strlen(temp_buf), receipt);
      /* add blank line */
      receipt_add_blank_lines (1, receipt);
    }

    /* IAC */
	 
	if( fDiag )
	{
		receipt_add_string ("EMV IACS", receipt);
		if( vim_tlv_search(&tag_list, VIM_TAG_EMV_ISSUER_ACTION_CODE_DEFAULT) == VIM_ERROR_NONE )
		{
			vim_mem_clear(temp_buf, sizeof(temp_buf));
			hex_to_asc(tag_list.current_item.value, temp_buf, 10);
			receipt_add_right_justified_item ("DEFAULT", temp_buf, vim_strlen(temp_buf), receipt);    
		}    
		if( vim_tlv_search(&tag_list, VIM_TAG_EMV_ISSUER_ACTION_CODE_DENIAL) == VIM_ERROR_NONE )
		{
			vim_mem_clear(temp_buf, sizeof(temp_buf));
			hex_to_asc(tag_list.current_item.value, temp_buf, 10);
			receipt_add_right_justified_item ("DENIAL", temp_buf, vim_strlen(temp_buf), receipt);    
		}
		if( vim_tlv_search(&tag_list, VIM_TAG_EMV_ISSUER_ACTION_CODE_ONLINE) == VIM_ERROR_NONE )
		{
			vim_mem_clear(temp_buf, sizeof(temp_buf));
			hex_to_asc(tag_list.current_item.value, temp_buf, 10);
			receipt_add_right_justified_item ("ONLINE", temp_buf, vim_strlen(temp_buf), receipt);
		}    
		/* add blank line */
		if( is_integrated_mode() )
			receipt_add_blank_lines (1, receipt);
	}
    /* EMV tags */
    if( vim_tlv_rewind(&tag_list) != VIM_ERROR_NONE )
      return;

    //while(vim_tlv_goto_next(&tag_list)!=VIM_ERROR_TAG_NOT_FOUND)
	// RDD 080312 SPIRA 4822 + 5067 - Incorrect Tags printed on diag receipt

	Tags = fDiag ? WOW_PRINT_TAGS_LEN : WOW_RECEIPT_TAGS_LEN;
    
	for( n=0; n<Tags; n++ )
    {
        VIM_CHAR long_buffer[256];
		if( fDiag )
		{
			if( vim_tlv_search( &tag_list, tags_to_print[n] ) != VIM_ERROR_NONE ) continue;
		}
		else
		{
			if( vim_tlv_search( &tag_list, tags_to_print_txn[n] ) != VIM_ERROR_NONE ) continue;
		}
            
		if( !tag_list.current_item.length ) 
		  continue;
     
        vim_mem_clear( string_buf, VIM_SIZEOF(string_buf));
        vim_mem_clear( long_buffer, VIM_SIZEOF(long_buffer));

        hex_to_asc(tag_list.current_item.tag, string_buf, tag_list.current_item.tag_length * 2);
        if( tag_list.current_item.length*2 <= VIM_SIZEOF(long_buffer) -1 )        
          hex_to_asc(tag_list.current_item.value, long_buffer, tag_list.current_item.length * 2 );
        else
        {
          hex_to_asc(tag_list.current_item.value, long_buffer, VIM_SIZEOF(long_buffer)-1 );
        }   
        VIM_DBG_STRING(long_buffer);

        if( !skip_EMV_tag( string_buf, vim_strlen(string_buf) ) )		
        {
          VIM_SIZE lines=0;
          VIM_SIZE DataLen = vim_strlen(string_buf) + vim_strlen(long_buffer) + 1;

          format_EMV_tag( string_buf, long_buffer);
          /* calculate lines */

          VIM_DBG_STRING(string_buf);
          VIM_DBG_STRING(long_buffer);

          lines = DataLen / terminal.configuration.prn_columns;

          if( DataLen % terminal.configuration.prn_columns )
            lines += 1;

          VIM_DBG_NUMBER(lines);

          if( lines == 1 )
            receipt_add_right_justified_item (string_buf, long_buffer, vim_strlen(long_buffer), receipt);
          else
          {
            VIM_SIZE cnt;
            VIM_SIZE one_line_length = terminal.configuration.prn_columns - ( vim_strlen(string_buf) +1 );
            VIM_TEXT dummy_string_buffer[RECEIPT_LINE_MAX_WIDTH+1];
            vim_mem_clear( dummy_string_buffer, VIM_SIZEOF(dummy_string_buffer));
            vim_mem_set( dummy_string_buffer, ' ', vim_strlen(string_buf));
            for( cnt=0; cnt<=lines; cnt++ )
            {
              if( cnt == 0 )
                receipt_add_right_justified_item (string_buf, long_buffer+cnt*one_line_length, one_line_length, receipt);
              else if( vim_strlen(long_buffer + cnt * one_line_length))
              {
                  receipt_add_right_justified_item(dummy_string_buffer, long_buffer + cnt * one_line_length, one_line_length, receipt);
              }
            }
          }
        }
        else
        {
          // DBG_MESSAGE( "TAG IN SKIP LIST..." );
        }    
	}
  
    // VN JIRA WWBP-332  10/10/18 Tag 8A missing off diagnostic receipts for Contactless
    if ((transaction->saf_reason == saf_reason_offline_emv_purchase) &&  (transaction->card.type == card_picc)) {
        // Add Tag 8A & Y1
        // According to Donna contactless won't Y3
        VIM_CHAR tag_asc_buf[8] = {0}, val_asc_buf[8] = { 0 };
        hex_to_asc((VIM_UINT8*)VIM_TAG_EMV_RESPONSE_CODE, tag_asc_buf, 2);
        hex_to_asc((VIM_UINT8*)"Y1", val_asc_buf, 4);
        receipt_add_right_justified_item(tag_asc_buf, val_asc_buf, vim_strlen(val_asc_buf), receipt);
    }
  
	/* TODO: for debug, remove later */
  
	if(( terminal_get_status( ss_wow_basic_test_mode )) && ( fDiag ))
	{
		VIM_CHAR sac_buffer[20];
		VIM_SIZE cnt=0;
		receipt_add_blank_lines(1, receipt);
		if( transaction->aid_index < MAX_EMV_APPLICATION_ID_SIZE ) 
		{
			vim_mem_clear( sac_buffer, VIM_SIZEOF(sac_buffer) );		
			hex_to_asc(terminal.emv_applicationIDs[transaction->aid_index].tac_standin, sac_buffer, 5 * 2 );		
			receipt_add_right_justified_item ( "9FE951", sac_buffer, vim_strlen(sac_buffer), receipt );      
			receipt_add_blank_lines (1, receipt);    
		}
    
		/* EMV contactless limits */
		receipt_add_string ("EMV CONTACTLESS LIMITS", receipt);

		for( cnt=0; cnt<MAX_EMV_APPLICATION_ID_SIZE; cnt++ )
		{        
			vim_mem_clear( sac_buffer, VIM_SIZEOF(sac_buffer) );		
			if( terminal.emv_applicationIDs[cnt].contactless )		
			{
				hex_to_asc (terminal.emv_applicationIDs[cnt].emv_aid, sac_buffer, terminal.emv_applicationIDs[cnt].aid_length *2);        
				receipt_add_string (sac_buffer, receipt);
				vim_snprintf( sac_buffer, VIM_SIZEOF(sac_buffer), "%d", terminal.emv_applicationIDs[cnt].contactless_txn_limit );
				receipt_add_right_justified_item ("TXN LIMIT", sac_buffer, vim_strlen(sac_buffer), receipt);
				vim_snprintf( sac_buffer, VIM_SIZEOF(sac_buffer), "%d", terminal.emv_applicationIDs[cnt].cvm_limit);
				receipt_add_right_justified_item ("CVM LIMIT", sac_buffer, vim_strlen(sac_buffer), receipt);
				vim_snprintf( sac_buffer, VIM_SIZEOF(sac_buffer), "%d", terminal.emv_applicationIDs[cnt].floor_limit);
				receipt_add_right_justified_item ("FLOOR LIMIT", sac_buffer, vim_strlen(sac_buffer), receipt);
				receipt_add_blank_lines( 1, receipt );
			}		
	    }
	}
  
	/* add blank line */
	receipt_add_blank_lines(1, receipt);  
}


static void receipt_add_emv_data_diagnostic_w(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL fDiag, VIM_SIZE cols)
{
    receipt_add_emv_data_diagnostic(transaction, receipt, fDiag);
}
	  
/* -------------------------------------------------------------------------------------------------- */
/*  Build a Last EMV transaction data receipt */
/* -------------------------------------------------------------------------------------------------- */
void PrintLastEMVTxn(VIM_BOOL fUseDuplicate)
{
    VIM_PCEFTPOS_RECEIPT receipt;
    transaction_t *transaction = (fUseDuplicate) ? &txn_duplicate : &txn;
    VIM_SIZE cols = PAPER_WIDTH;

    if (!FullEmv(transaction))
    {
        // RDD 010420 v582-7 Georgia found that declined non-emv receipts printed this
        return;
    }

	if (txn.error == ERR_POWER_FAIL)
	{
		return;
	}


  /* source data stored in txn_duplicate */
  vim_mem_set(&receipt, ' ', VIM_SIZEOF(receipt));

  receipt.lines = 0;
  receipt.receipt_columns = 0;

  // RDD 190313 : P4 Send this receipt to the audit printer
  // RDD 220413 : SPIRA:6686 Keep type as original (L) unless ordered as standard debug for logs (all failed EMV txns) use (A)
  if( fUseDuplicate )
	receipt.type = PCEFTPOS_RECEIPT_TYPE_LOGON;
  else
	receipt.type = PCEFTPOS_RECEIPT_TYPE_ADVICE;

  receipt.stan = txn_duplicate.stan;
  
  if( fUseDuplicate )
  {
	  if( txn_duplicate_txn_read( VIM_TRUE ) != VIM_ERROR_NONE ) 
          return;
  }
  
  //display_screen(DISP_PRINTING_PLEASE_WAIT, VIM_PCEFTPOS_KEY_NONE);

  /* print line of - */

  if( !LOCAL_PRINTER )
	receipt_add_dotted_line ('-', &receipt);

  // RDD 180221 - Some issue with eft client not handling the 6177 bytes of the diag receipt with all the extra spaces needed for wide format.
  receipt.receipt_columns = 24 ;

  /* merchant ID */
  receipt_add_mid (&receipt);
  /* terminal ID */
  receipt_add_tid (&receipt);
  /* TXN ref */
  // RDD 030322 JIRA PIRPIN-1457 TXNREF all zeros in Standaloine ( as there is no txn ref )
  if (IS_INTEGRATED)
  {
	  receipt_add_generic_rrn("TXNREF", transaction->pos_reference, &receipt);
  }
  /* blank */
  receipt_add_blank_lines(1, &receipt);
  /* add title */
  receipt_add_centered_string("EMV CONFIGURATION", &receipt);
  /* blank */
  receipt_add_blank_lines(1, &receipt);
  /* EMV DATA */
  receipt_add_emv_data_diagnostic_w( transaction, &receipt, VIM_TRUE, receipt.receipt_columns);
  /* TXN time */

  /* dotted line */
  receipt_add_dotted_line ('-', &receipt);

  /* print and journal */
  pceft_pos_print( &receipt, VIM_TRUE );

  // Restore actual width.
  PAPER_WIDTH = cols;

}



/* -------------------------------------------------------------------------------------------------- */
/*  Build a Token Query Card receipt */
/* -------------------------------------------------------------------------------------------------- */
VIM_ERROR_PTR receipt_build_token_query(VIM_UINT8 acquirer_idx, VIM_PCEFTPOS_RECEIPT *receipt)
{
  receipt->lines = 0;
  receipt->approved = (VIM_ERROR_NONE == vim_mem_compare(txn.host_rc_asc, "00", 2));

  receipt->type = PCEFTPOS_RECEIPT_TYPE_CUSTOMER;
  receipt->stan = txn.stan;

  vim_mem_set(receipt->reference, ' ', VIM_SIZEOF(receipt->reference));

  if( !LOCAL_PRINTER )
	receipt_add_dotted_line('-', receipt);

  receipt_add_centered_string("TOKEN LOOKUP RECORD", receipt);

  receipt_add_string( "TOKEN:", receipt );
  receipt_add_string( txn.card.pci_data.eToken, receipt );

  receipt_add_string( "ePAN:", receipt );
  receipt_add_ePan( receipt, &txn );

  receipt_add_string( "TRANSACTN PROCESSED", receipt );
        
  receipt_add_date_time( &txn.time, receipt, "" , VIM_NULL );
  receipt_add_dotted_line('-', receipt);
          
  //receipt_add_response_description(VIM_ERROR_NONE, txn.host_rc_asc, "", &txn, receipt, VIM_FALSE);  
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR receipt_build_offline_totals(VIM_PCEFTPOS_RECEIPT *receipt)
{
  VIM_CHAR rc[10];
  VIM_RTC_TIME date_time;

  vim_rtc_get_time(&date_time);

  receipt->lines = 0;
  receipt->stan = settle.stan;

  if( !LOCAL_PRINTER )  
	  receipt_add_dotted_line('-', receipt);  

  receipt_add_mid(receipt);
  receipt_add_tid(receipt);
  receipt_add_centered_string("ELECTRONIC FALLBACK TOTS", receipt);
  receipt_add_right_justified_item( "PENDING", "", 0, receipt);
  receipt_add_count_amount( "CR AMOUNT", saf_totals.fields.CreditCountNS, saf_totals.fields.CreditAmountNS, receipt );
  receipt_add_count_amount( "DR AMOUNT", saf_totals.fields.DebitCountNS, saf_totals.fields.DebitAmountNS, receipt );
  receipt_add_right_justified_item( "REJECTED", "", 0, receipt);
  receipt_add_count_amount( "CR AMOUNT", saf_totals.fields.CreditCountDECL, saf_totals.fields.CreditAmountDECL, receipt );
  receipt_add_count_amount( "DR AMOUNT", saf_totals.fields.DebitCountDECL, saf_totals.fields.DebitAmountDECL, receipt );
  
    vim_mem_clear( rc, VIM_SIZEOF(rc) );

  receipt_add_response_description( settle.error, settle.host_rc_asc, "", &txn, receipt, VIM_FALSE, VIM_FALSE, VIM_NULL );

  // No STAN on the Offline totals receipt
  receipt_add_date_time( &date_time, receipt, "" , VIM_NULL );

  receipt_add_dotted_line('-', receipt);
  
  return VIM_ERROR_NONE;
}


/* -------------------------------------------------------------------------------------------------- */
/*  Build the Settlement Report */
/* -------------------------------------------------------------------------------------------------- */
VIM_ERROR_PTR receipt_build_settlement(VIM_UINT8 acquirer_idx, VIM_PCEFTPOS_RECEIPT *receipt, VIM_BOOL pre_settled)
{
  VIM_CHAR rc[10];
  VIM_RTC_TIME date_time;
  VIM_CHAR stan_string[6+1];
  VIM_UINT16 Count1,Count2;
  VIM_INT64 Amount1,Amount2;

  VIM_INT64 net_amount;

  vim_rtc_get_time(&date_time);

  receipt->lines = 0;
  receipt->receipt_columns = 24;

  receipt->stan = settle.stan;

  receipt->approved = (VIM_ERROR_NONE == vim_mem_compare(settle.host_rc_asc, "00", 2));

  receipt->type = PCEFTPOS_RECEIPT_TYPE_SETTLEMENT;

  vim_mem_set(receipt->reference, ' ', VIM_SIZEOF(receipt->reference));
    
  if( !LOCAL_PRINTER )  
	  receipt_add_dotted_line('-', receipt);

  //receipt_add_amex_logo();

  //receipt_add_merchant_details(receipt);
  AddTrainingMode(receipt,VIM_FALSE);

  receipt_add_mid(receipt);

  receipt_add_tid(receipt);
  receipt_add_blank_lines(1, receipt);

  if ( VIM_TRUE == pre_settled )
	  print_settlement_settlement_date( "PRE-SETTLEMENT", receipt );
  else if( settle.type != tt_settle_safs )
	  print_settlement_settlement_date( "SETTLEMENT", receipt );
  else
  {
      print_settlement_settlement_date("Standin Totals", receipt);
  }

  receipt_add_count_amount( "CR AMOUNT", settle.Credits-settle.CreditReversals, settle.CreditTotalAmount-settle.CreditReversalsTotalAmount, receipt );
  receipt_add_count_amount( "DR AMOUNT", settle.Debits-settle.DebitReversals, settle.DebitTotalAmount-settle.DebitReversalsTotalAmount, receipt );

  #if 0
  net_amount = settle.AmountNetSettlement;
  #else
  net_amount = (settle.DebitTotalAmount-settle.DebitReversalsTotalAmount) - (settle.CreditTotalAmount-settle.CreditReversalsTotalAmount);
  #endif
  receipt_add_net_amount( "NET AMOUNT", net_amount, receipt );

  receipt_add_count_amount( "CASH AMOUNT", settle.CashOuts, settle.CashOutTotalAmount, receipt );

  Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositCount, 10);
  Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositAmount, 12);

  receipt_add_count_amount( "DEP AMOUNT", Count1, Amount1, receipt );

  // RDD 140922 JIRA PIRPIN-1810 Add tips to settlement totals - deferred as tips not stored
  //Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositCount, 10);
  //Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositAmount, 12);
  //receipt_add_count_amount("TIPS AMOUNT", Count1, Amount1, receipt);


  receipt_add_right_justified_item( "AMEX CARD", "", 0, receipt);

  Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexRefundCount, 10);
  Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexRefundAmount, 12);
  Count2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexPurchaseCount, 10);
  Amount2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexPurchaseAmount, 12);

  receipt_add_count_amount( "CR AMOUNT", Count1, Amount1, receipt );
  receipt_add_count_amount( "DR AMOUNT", Count2, Amount2, receipt );

 // net_amount = MAX(Amount2, Amount1) - MIN(Amount2, Amount1);
  net_amount = Amount2 - Amount1;
  receipt_add_net_amount( "NET AMOUNT", net_amount, receipt );

  receipt_add_right_justified_item( "DINERS", "", 0, receipt);

  Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersRefundCount, 10);
  Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersRefundAmount, 12);
  Count2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersPurchaseCount, 10);
  Amount2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersPurchaseAmount, 12);

  receipt_add_count_amount( "CR AMOUNT", Count1, Amount1, receipt );
  receipt_add_count_amount( "DR AMOUNT", Count2, Amount2, receipt );

  //net_amount = MAX(Amount2, Amount1) - MIN(Amount2, Amount1);
  net_amount = Amount2 - Amount1;
  
  receipt_add_net_amount( "NET AMOUNT", net_amount, receipt );

  vim_mem_clear( rc, VIM_SIZEOF(rc) );
    
  receipt_add_response_description( settle.error, settle.host_rc_asc, "", &txn, receipt, VIM_FALSE, VIM_FALSE, VIM_NULL );

  // convert the settlement stan to ascii and print this with the current date and time
  vim_mem_clear( stan_string, VIM_SIZEOF(stan_string) );
  bin_to_asc( settle.stan, stan_string, 6 ); 

  receipt_add_date_time( &date_time, receipt, stan_string, VIM_NULL );

  receipt_add_dotted_line('-', receipt);
  
  return VIM_ERROR_NONE;
}


//////////////////////////////////////////////////// LOGON RECEIPT ///////////////////////////////////////


VIM_ERROR_PTR receipt_build_logon(VIM_ERROR_PTR error, VIM_PCEFTPOS_RECEIPT *receipt, VIM_CHAR *string, VIM_BOOL RTM )
{
  VIM_RTC_TIME current_time;
  VIM_CHAR saf_buff[3+1];
  // For WOW Reversal is added to SAF count

  receipt->type = PCEFTPOS_RECEIPT_TYPE_LOGON;
  receipt->stan = terminal.stan;

  // RDD 060123 - another obvious bug never spotted by QA - causes very obvious malformed logon receipts when wide format used.
  //receipt->receipt_columns = 24;
  receipt->receipt_columns = PAPER_WIDTH;

  vim_mem_set(receipt->reference, ' ', VIM_SIZEOF(receipt->reference));  
  vim_mem_clear( &receipt->data, VIM_PCEFTPOS_RECEIPT_MAX_LINES * VIM_PCEFTPOS_RECEIPT_LINE_WIDTH );

  VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&current_time));
  receipt->lines = 0;

  if( !LOCAL_PRINTER )
	receipt_add_dotted_line('-', receipt);
  
//  AddTrainingMode(receipt,VIM_FALSE);

  receipt_add_merchant_details(receipt);    

  if( RTM )
  {
	  receipt_add_centered_string("RTM Failure", receipt);
  }
  else
  {
	  receipt_add_centered_string("LOGON", receipt);
  }

  // RDD 060123 v735 - no idea why this restriction was included - remove it !
  //if (PAPER_WIDTH != 50)
  {
	  receipt_add_mid(receipt);  
	  receipt_add_tid(receipt);
  }

  if( !RTM )
  {
	receipt_add_software_version( receipt, "ACTIVE S/W:", APP_FILE_IDX );
	receipt_add_software_version( receipt, "CPAT VERS:", CPAT_FILE_IDX );
	receipt_add_software_version( receipt, "EPAT VERS:", EPAT_FILE_IDX );
	receipt_add_software_version( receipt, "PKT VERS:", PKT_FILE_IDX );
	if( terminal.file_version[FCAT_FILE_IDX])
		receipt_add_software_version( receipt, "FCAT VERS:", FCAT_FILE_IDX );

	// RDD 250822 JIRA PIRPIN-1782 add RPAT version to logon receipt
	XMLProductLimitExceeded(VIM_NULL, VIM_NULL);
	receipt_format_line(receipt, "RPAT VERS:", terminal.configuration.RPAT_Version);

	receipt_add_ppid( receipt );
  
  
	saf_buff[3] = '\0';    
	saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count );
  
	if( reversal_present() )
		bin_to_asc( (saf_totals.fields.saf_count)+1, saf_buff, 3  );
	else	
		bin_to_asc( (saf_totals.fields.saf_count), saf_buff, 3  );
	
	receipt_format_line( receipt, "SAF TO BE SENT", saf_buff );

	
	if( is_integrated_mode() && ( !terminal_get_status( ss_no_saf_prints )))
	{
		bin_to_asc( saf_totals.fields.saf_print_count, saf_buff, 3  );	
		receipt_format_line( receipt, "SAF TO BE PRINTED", saf_buff );
	}

	// RDD 180315 SPIRA:8477
	if( is_standalone_mode() )
	{	
		VIM_SIZE PendingCount = 0;
		VIM_SIZE PendingTotal = 0;
		VIM_CHAR auth_buff[4];	  

		if (TERM_ALLOW_PREAUTH)
		{
			preauth_get_pending(&PendingCount, &PendingTotal, VIM_FALSE);
			vim_sprintf(auth_buff, "%3.3d", PendingCount);
			receipt_format_line(receipt, "PREAUTHS PENDING", auth_buff);
			receipt_add_blank_lines(1, receipt);
		}
	}
  }
  receipt_add_response_description( error, init.logon_response, "", VIM_NULL, receipt, VIM_FALSE, VIM_FALSE, VIM_NULL );
  receipt_add_blank_lines(1, receipt);
 
  receipt_add_date_time(&current_time, receipt, "", VIM_NULL);
  receipt_add_dotted_line('-', receipt);

  receipt->type = PCEFTPOS_RECEIPT_TYPE_LOGON;

  return VIM_ERROR_NONE;
}



static void remove_spaces( VIM_CHAR *Buffer, VIM_CHAR *Msg, VIM_UINT8 MsgLen )
{
	VIM_CHAR *Ptr1 = Msg;
	VIM_CHAR *Ptr2 = Buffer;
	VIM_UINT8 n, SpaceCount = 0;

	// The data is formatted for 40 col receipt so split into block of 40 and remove spaces

	for( n=0; n < MsgLen; n++, Ptr1++ )
	{
		if( *Ptr1 == ' ' )  
		{
			if( ++SpaceCount == 1 ) 
				*Ptr2++ = *Ptr1;	
			else
				continue;
		}
		else
		{
			SpaceCount = 0;
			*Ptr2++ = *Ptr1;
		}
	}
}


#if 0

VIM_ERROR_PTR receipt_add_issuer_msg( VIM_CHAR *Msg, VIM_PCEFTPOS_RECEIPT *receipt )
{
	VIM_UINT8 n;
	VIM_CHAR Buffer[WOW_MAX_PAD_STRING_LEN+1];
	VIM_CHAR *Ptr = Msg;
	VIM_CHAR *Ptr2;
	VIM_UINT8 Offset = 0;
	VIM_UINT8 MsgLen = vim_strlen( Msg );

	if( !*Ptr  ) return VIM_ERROR_NOT_FOUND; 

	// RDD 290512 - make a copy of the message as we'll corrupt it for formatting
	vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );

	remove_spaces( Buffer, Ptr, MsgLen );

	Ptr = Ptr2 = Buffer;

	for( n=0; n<WOW_MAX_PAD_STRING_LEN/RECEIPT_LINE_WIDTH; n++ )
	{
		// Go to the end of a line and then run back until theres a space
		Offset=RECEIPT_LINE_WIDTH;
		Ptr2+=Offset;
		// Rewind to the last space
		while(( *Ptr2 != ' ' ) && ( Offset-- )) Ptr2--;
		if( !Offset ) 
			return VIM_ERROR_NONE;
		*Ptr2++ = '\0';
	    receipt_add_centered_string( Ptr, receipt );
		Ptr+=Offset+1;
	}
	return VIM_ERROR_NONE;
}

#else

#define WOW_FCD_DE63_LINE_LEN 40

static void receipt_add_format_string( VIM_CHAR *MsgLine, VIM_PCEFTPOS_RECEIPT *receipt )
{
	VIM_CHAR Buffer[WOW_FCD_DE63_LINE_LEN+1];
	VIM_CHAR *Ptr;
	VIM_UINT8 CountBack = 0;

	// Fill buffer with spaces but NULL terminate
	vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );
	//vim_mem_set( Buffer, ' ', WOW_FCD_DE63_LINE_LEN );

	// Remove excess spaces from the line
	remove_spaces( Buffer, MsgLine, WOW_FCD_DE63_LINE_LEN );
	
	Ptr = Buffer;
	while(( *Ptr == ' ' ) && ( CountBack++ < WOW_FCD_DE63_LINE_LEN )) ++Ptr;

	if( vim_strlen( Ptr ) > terminal.configuration.prn_columns )
	{
#if 0
		receipt_add_string( Buffer, receipt );
		Ptr += terminal.configuration.prn_columns;
		receipt_add_string( Ptr, receipt );
#else
		// RDD 280812 SPIRA:5964 Check for incomplete words at end of line.
		// Need to Wind back ptr and start full word on next line
		CountBack = 0;
		Ptr+=terminal.configuration.prn_columns;
		// move back towards the start of the buffer until we find a word boundary
		while((*Ptr != ' ') && ( CountBack<terminal.configuration.prn_columns ))
		{
			CountBack++;
			Ptr--;
		}
		*Ptr++ = '\0';
		receipt_add_string( Buffer, receipt );
		receipt_add_string( Ptr, receipt );

#endif
	}
	else
	{
		receipt_add_string( Buffer, receipt );
	}
}


static VIM_ERROR_PTR receipt_add_issuer_msg( VIM_CHAR *Msg, VIM_PCEFTPOS_RECEIPT *receipt )
{
	VIM_UINT8 n,MsgLen = vim_strlen( Msg );
	VIM_UINT8 MsgLines = MsgLen%WOW_FCD_DE63_LINE_LEN ? MsgLen/WOW_FCD_DE63_LINE_LEN + 1 : MsgLen/WOW_FCD_DE63_LINE_LEN;
	VIM_CHAR *Ptr = Msg;

	for( n=0; n< MsgLines; n++ )
	{
	    receipt_add_format_string( Ptr, receipt );
		Ptr += WOW_FCD_DE63_LINE_LEN;
	}
	return VIM_ERROR_NONE;
}

#endif

/* Shift total */ 
VIM_ERROR_PTR receipt_build_shift_totals(VIM_PCEFTPOS_RECEIPT *receipt, shift_details_t *shift, VIM_BOOL reset_totals )
{
  VIM_UINT16 net_count;
  VIM_UINT64 amount_net;
  VIM_RTC_TIME time;
  VIM_CHAR Buffer[RECEIPT_LINE_MAX_WIDTH+1];
  VIM_CHAR Buffer1[RECEIPT_LINE_MAX_WIDTH + 1];
  vim_rtc_get_time(&time);
  
  vim_sprintf( Buffer, "%2.2d/%2.2d/%2.2d %2.2d:%2.2d", shift_details.shift_start.day, shift_details.shift_start.month, shift_details.shift_start.year%2000, shift_details.shift_start.hour, shift_details.shift_start.minute );
  vim_sprintf( Buffer1, "%2.2d/%2.2d/%2.2d %2.2d:%2.2d", time.day, time.month, time.year % 2000, time.hour, time.minute );


  receipt->lines = 0;
  receipt->receipt_columns = 24;

  receipt->stan = terminal.stan;

  receipt->approved = VIM_TRUE;

  receipt->type = PCEFTPOS_RECEIPT_TYPE_SETTLEMENT;

  vim_mem_set(receipt->reference, ' ', VIM_SIZEOF(receipt->reference));

  if( !LOCAL_PRINTER )
	  receipt_add_dotted_line('-', receipt);

  //receipt_add_amex_logo();

  receipt_add_merchant_details(receipt);  

  receipt_add_centered_string("SHIFT TOTALS REPORT", receipt);
    
  receipt_add_blank_lines(1, receipt);
  
  receipt_add_mid(receipt);
  receipt_add_tid(receipt);

  receipt_add_blank_lines(1, receipt);

  //receipt_add_date_time(&time, receipt, "PROCESSED");    
  receipt_add_right_justified_item( "FROM:", Buffer, vim_strlen(Buffer), receipt );
  receipt_add_right_justified_item( "UNTIL:", Buffer1, vim_strlen(Buffer1), receipt );

  receipt_add_blank_lines(1, receipt);
  
  receipt_add_centered_string("SUB-TOTALS BY TYPE", receipt);
  
  receipt_add_blank_lines(1, receipt);

  receipt_add_count_amount("SALES", shift->totals.purchase_count, shift->totals.amount_purchase, receipt);
  if (TERM_CASHOUT_MAX)
  {
	  receipt_add_count_amount("CASH OUT", shift->totals.cash_count, shift->totals.amount_cash, receipt);
  }
  if (TERM_ALLOW_TIP)
  {
	  receipt_add_count_amount("TIPS", shift->totals.tips_count, shift->totals.amount_tips, receipt);
  }

  receipt_add_blank_lines(1, receipt);
  if (TERM_REFUND_ENABLE)
  {
	  receipt_add_count_amount("REFUNDS", shift->totals.refund_count, shift->totals.amount_refund, receipt);
  }
  receipt_add_count_amount("DEPOSITS", shift->totals.deposit_count, shift->totals.amount_deposit, receipt);

  receipt_add_blank_lines(1, receipt);
  receipt_add_dotted_line('-', receipt);

  net_count = shift->totals.purchase_count + shift->totals.cash_count + shift->totals.tips_count + shift->totals.refund_count;
  amount_net = shift->totals.amount_cash + shift->totals.amount_tips + shift->totals.amount_purchase - shift->totals.amount_refund;
  receipt_add_count_amount("NET", net_count, amount_net, receipt);

  receipt_add_blank_lines(1, receipt);

  // RDD 190315 SPIRA:8477 - removed 060515 by order of Donna
//receipt_add_count_amount("PA(TOT)", shift->totals.preauth_count, shift->totals.amount_preauth, receipt);

  if (1)
  {
	  receipt_add_centered_string( "SUB-TOTALS BY SCHEME", receipt );
	  receipt_add_blank_lines(1, receipt);
	  receipt_add_count_amount( "VISA", shift->totals.scheme_totals[_visa].purchase_count, shift->totals.scheme_totals[_visa].amount_purchase, receipt );
	  receipt_add_count_amount( "MC", shift->totals.scheme_totals[_mastercard].purchase_count, shift->totals.scheme_totals[_mastercard].amount_purchase, receipt);
	  receipt_add_count_amount( "EFTPOS", shift->totals.scheme_totals[_eftpos].purchase_count, shift->totals.scheme_totals[_eftpos].amount_purchase, receipt);

	  // Optional Schemes displayed only if permitted by RTM
	  if (TERM_AMEX_DISABLE == VIM_FALSE)
	  {
		  receipt_add_count_amount("AMEX", shift->totals.scheme_totals[_amex].purchase_count, shift->totals.scheme_totals[_amex].amount_purchase, receipt);
	  }
	  if (TERM_UPI_DISABLE == VIM_FALSE)
	  {
		  receipt_add_count_amount("UPI", shift->totals.scheme_totals[_upi].purchase_count, shift->totals.scheme_totals[_upi].amount_purchase, receipt);
	  }
	  if (TERM_JCB_DISABLE == VIM_FALSE)
	  {
		  receipt_add_count_amount("JCB", shift->totals.scheme_totals[_jcb].purchase_count, shift->totals.scheme_totals[_jcb].amount_purchase, receipt);
	  }
	  if (TERM_DINERS_DISABLE == VIM_FALSE)
	  {
		  receipt_add_count_amount("DINERS", shift->totals.scheme_totals[_diners].purchase_count, shift->totals.scheme_totals[_diners].amount_purchase, receipt);
	  }
  }

  receipt_add_blank_lines(1, receipt);

  // RDD 210314 SPIRA:7396.17

  if( TERM_ALLOW_PREAUTH )
  // RDD 270315 SPIRA:8523
  {
	  VIM_SIZE PendingCount = 0;
	  VIM_SIZE PendingTotal = 0;
	  receipt_add_dotted_line('-', receipt);
	  preauth_get_pending(&PendingCount, &PendingTotal, VIM_TRUE);
	  receipt_add_count_amount("PA OPEN", PendingCount, PendingTotal, receipt);
  }
  receipt_add_blank_lines(1, receipt);

  if (reset_totals)
	  receipt_add_centered_string("SHIFT TOTALS RESET", receipt);


  receipt_add_dotted_line('-', receipt);

  return VIM_ERROR_NONE;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// FINANCIAL TRANSACTION RECEIPT ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_BOOL valid_stan( VIM_UINT32 stan )
{
	return (( stan != 0 ) && ( stan <= 999999 )) ? VIM_TRUE : VIM_FALSE;
}


extern VIM_UINT64 AscToBin(char const *asc, VIM_UINT8 len, VIM_BOOL *Invalid );

// RDD 270815 SPIRA: 8747 Only want diag if response code is Y1,Y3,Z1,Z3 or Z4
 VIM_BOOL DiagnosticsRequired( transaction_t *transaction ) 
 {
	 return VIM_FALSE;
	 // RDD 010322 JIRA PIRPIN-1453 : Georgia reported this as a defect even though it is a specific requirement for standalone. 
	 // Probably correct for LIVE who hate the DIAG receipt so implement the change only for them.
	 if (terminal.terminal_id[0] == 'L')
	 {
		 return VIM_FALSE;
	 }

	 // VN 260718 JIRA WWBP-285 EMV tags should not print on merch or customer receipts
	 if( is_integrated_mode() || (terminal.original_mode == st_integrated))
		 return VIM_FALSE;

	 // RDD 011115 SPIRA:8747
	 if( transaction->type == tt_checkout )
		 return VIM_FALSE;

	 if( !FullEmv(transaction) )
		 return VIM_FALSE;

	 // Diag was missing on Full EMV 91 responses and they need that
	 if( !vim_strncmp( transaction->host_rc_asc, "91", 2 ) || ( !vim_strncmp( transaction->host_rc_asc_backup, "91", 2 )))
		 return VIM_TRUE;

	 pceft_debug_error( pceftpos_handle.instance, "Diagnostics txn.error", transaction->error );

	 {
		VIM_UINT8 erc;
	 	VIM_BOOL NonNumeric = VIM_FALSE;
		/* Convert response code */
		erc = AscToBin(VIM_CHAR_PTR_FROM_UINT8_PTR(txn.host_rc_asc), 2, &NonNumeric );
		if( !NonNumeric ) 
		{
			// RDD 070915 v547-1 Charlene found that comms errors resulting in EFB sig approval were'nt getting diag receipts
			if(( transaction->error == ERR_SIGNATURE_APPROVED_OFFLINE ) && ( transaction->error_backup != VIM_ERROR_NONE ))
				return VIM_TRUE;

			if(( erc != 0 )&&( erc != 11 )&&( erc != 8 )&&( erc !=91 ))
				return VIM_FALSE;
		}
	 }

	 if(( transaction->error == ERR_WOW_ERC )||( transaction->emv_error == ERR_WOW_ERC ))
	 {
		 return VIM_FALSE;
	 }


	 if(( !vim_strncmp( transaction->host_rc_asc, "00", 2 )) && ( transaction_get_status( transaction, st_message_sent )))
		 return VIM_FALSE;

	 if(( !vim_strncmp( transaction->host_rc_asc, "08", 2 )) && ( transaction_get_status( transaction, st_message_sent )))
		 return VIM_FALSE;

	 return VIM_TRUE;
 }

 VIM_ERROR_PTR receipt_add_moto_reason_code(transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
 {
     VIM_CHAR Buffer[30] = { 0 };
     if (transaction->moto_reason == moto_reason_telephone)
     {
         vim_strcpy(Buffer, "Telephone Order");
     }
     else if (transaction->moto_reason == moto_reason_mail_order)
     {
         vim_strcpy(Buffer, "Mail Order");
     }
     receipt_add_right_justified_item("MOTO:", Buffer, vim_strlen(Buffer), receipt );
     return VIM_ERROR_NONE;
 }



VIM_ERROR_PTR receipt_add_reason_code( VIM_ERROR_PTR error, VIM_CHAR *asc_response, VIM_PCEFTPOS_RECEIPT *receipt )
{	  
	error_t error_data;
	VIM_CHAR Buffer[30];

	error_code_lookup( error, "", &error_data );  
	if( !vim_strncmp( asc_response, "91", 2 ))
	{
		vim_sprintf( Buffer, "%s", asc_response ); 
	}
	else
	{
		vim_sprintf( Buffer, "%s", error_data.ascii_code );
	}
	receipt_add_right_justified_item( "REASON CODE", Buffer, vim_strlen(Buffer), receipt );
	return VIM_ERROR_NONE;
}

//Assumes buffer pszLine is at least ulLineWidth + 1 in size
static VIM_ERROR_PTR receipt_build_line(const VIM_CHAR* pszLeft, const VIM_CHAR* pszRight, VIM_CHAR* pszLine, VIM_SIZE ulLineWidth)
{
	if ((!pszLeft && !pszRight) || !pszLine)
	{
		pceft_debug(pceftpos_handle.instance, "Null param!");
		return VIM_ERROR_NULL;
	}
	if (!pszLeft)  pszLeft  = "";
	if (!pszRight) pszRight = "";

	if ((vim_strlen(pszLeft) + vim_strlen(pszRight)) > ulLineWidth)
	{
		pceft_debugf(pceftpos_handle.instance, "Items '%s'&'%s' too long to fit in line of %u width", pszLeft, pszRight, ulLineWidth);
		return VIM_ERROR_OVERFLOW;
	}

	vim_snprintf(pszLine, ulLineWidth + 1, "%s%*s", pszLeft, ulLineWidth - vim_strlen(pszLeft), pszRight);
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR receipt_add_edp_sub_transaction(VIM_UINT32 type, VIM_PCEFTPOS_RECEIPT* receipt, const cJSON* pCJSubTran, transaction_t* subTransaction)
{
	VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

	if (receipt && pCJSubTran && subTransaction)
	{
		VIM_BOOL duplicate_receipt = (type & receipt_duplicate);
		VIM_BOOL merchant_receipt = (type & receipt_merchant);
		VIM_CHAR szLeft[24 + 1] = { 0 }, szRight[24 + 1] = { 0 };

		if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
		{
			receipt_add_horizontal_rule(receipt, VIM_FALSE, VIM_TRUE);
		}

		if (!VAA_IsNullOrEmpty(subTransaction->card.pci_data.ePan))
		{
			//We've got enough card infomation to build card information as per a normal transaction
			receipt_add_card_number(merchant_receipt, duplicate_receipt, subTransaction, receipt, szRight);
			receipt_add_account_info(subTransaction, receipt, szLeft);
			if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
			{
				receipt_add_txn_amount(subTransaction, receipt, VIM_NULL);
				receipt_add_response_description(subTransaction->error, subTransaction->host_rc_asc, subTransaction->auth_no, subTransaction, receipt, VIM_FALSE, DiagnosticsRequired(subTransaction), VIM_NULL);
			}
			else
			{
				//Account|Card
				receipt_add_right_justified_item(szLeft, szRight, 24, receipt);
				//Response Description
				vim_mem_clear(szLeft, sizeof(szLeft));
				vim_mem_clear(szRight, sizeof(szRight));
				receipt_add_response_description(subTransaction->error, subTransaction->host_rc_asc, subTransaction->auth_no, subTransaction, receipt, VIM_FALSE, DiagnosticsRequired(subTransaction), szLeft);
				//Response Code|Amount
				receipt_add_txn_amount(subTransaction, receipt, szRight);
				receipt_add_right_justified_item(szLeft, szRight, 24, receipt);
			}
		}
		else
		{
			//Not enough card information, we're going to have to build the receipt from what we know
			VIM_CHAR szTemp[RECEIPT_LINE_MAX_WIDTH + 1] = { 0 };
			VIM_CHAR szCard[24 + 1] = { 0 };
			VIM_CHAR szAccount[24 + 1] = { 0 };
			const VIM_CHAR* pszSuffix   = VAA_GetJsonStringValue(pCJSubTran, "cardSuffix");
			const VIM_CHAR* pszScheme   = VAA_GetJsonStringValue(pCJSubTran, "scheme");
			const VIM_CHAR* pszAccount  = receipt_get_account_string(subTransaction->account);
			const VIM_CHAR* pszCardType = VAA_GetJsonStringValue(pCJSubTran, "instrumentType");
			const VIM_CHAR* pszAmountType = "PURCHASE";
			VIM_BOOL fGiftcard = pszCardType && (0 == vim_strcmp(pszCardType, "GIFT_CARD"));

			//Sanitise the card suffix - If we don't have one, continue dots to the end. Ensure a length of 4
			if (!pszSuffix)
				pszSuffix = "....";
			else if (4 < vim_strlen(pszSuffix))
				pszSuffix = &pszSuffix[vim_strlen(pszSuffix) - 4];

			receipt_format_pan(szTemp, pszSuffix, vim_strlen(pszSuffix), "0-4", 'Q');
			receipt_build_line("CARD:", szTemp, szCard, 24);
			vim_mem_clear(szTemp, sizeof(szTemp));

			if (!pszScheme)
				pszScheme = fGiftcard ? "GIFTCARD" : "CREDITCARD";
			if (!pszAccount)
				pszAccount = " ";
			receipt_build_line(pszScheme, pszAccount, szAccount, 24);

			if (tt_refund == subTransaction->type)
				pszAmountType = "REFUND";
			else if (fGiftcard)
				pszAmountType = "REDEMPTION";

			if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
			{
				receipt_add_right_justified_item(szCard, NULL, 0, receipt);
				receipt_add_right_justified_item(szAccount, NULL, 0, receipt);
				receipt_add_amount(pszAmountType, VIM_FALSE, subTransaction->amount_purchase, receipt, NULL);
				receipt_add_response_description(subTransaction->error, subTransaction->host_rc_asc, subTransaction->auth_no, subTransaction, receipt, VIM_FALSE, DiagnosticsRequired(subTransaction), VIM_NULL);
			}
			else
			{
				VIM_CHAR szAmount[24 + 1] = { 0 };
				receipt_add_amount(pszAmountType, VIM_FALSE, subTransaction->amount_purchase, receipt, szAmount);

				//Account|Card
				receipt_add_right_justified_item(szAccount, szCard, 24, receipt);
				//Response Description
				receipt_add_response_description(subTransaction->error, subTransaction->host_rc_asc, subTransaction->auth_no, subTransaction, receipt, VIM_FALSE, DiagnosticsRequired(subTransaction), szTemp);
				//Response Code|Amount
				receipt_add_right_justified_item(szTemp, szAmount, 24, receipt);
			}
		}

		eRet = VIM_ERROR_NONE;
	}
	else
	{
		pceft_debug_test(pceftpos_handle.instance, "NULL Subtransaction");
	}

	return eRet;
}

static VIM_ERROR_PTR receipt_add_edp_rollback(VIM_CHAR* pszDest, VIM_UINT32 ulSize, VIM_PCEFTPOS_RECEIPT* receipt)
{
	VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
	const VIM_CHAR* pszRollback = Wally_GetEdpRollback();
	if (pszRollback)
	{
		if ((0 == vim_strcmp(pszRollback, "SUCCESSFUL")) || (0 == vim_strcmp(pszRollback, "NOT_REQUIRED")))
		{
			pceft_debugf_test(pceftpos_handle.instance, "Got Rollback = %s, but skipping printing on receipt", pszRollback);
		}
		else
		{
			VIM_CHAR szRollback[24 + 1] = { 0 };
			vim_snprintf(szRollback, sizeof(szRollback), "ROLLBACK:%15s", pszRollback);
			if (pszDest && (0 < ulSize))
			{
				vim_mem_clear(pszDest, ulSize);
				vim_strncpy(pszDest, szRollback, ulSize - 1);
			}
			else if(receipt)
			{
				eRet = receipt_add_right_justified_item(szRollback, NULL, 0, receipt);
			}
			else
			{
				pceft_debug(pceftpos_handle.instance, "NULL Pointer!");
				eRet = VIM_ERROR_NULL;
			}
		}
	}
	return eRet;
}

static VIM_ERROR_PTR receipt_build_edp_receipt(VIM_UINT32 type, transaction_t* transaction, VIM_PCEFTPOS_RECEIPT* receipt)
{
	VIM_SIZE i = 0;

	if (!transaction || !receipt)
		return VIM_ERROR_NULL;

	for (i = 0; i < Wally_GetEdpSubTransactionCount(); i++)
	{
		transaction_t sTxn;
		const cJSON* pCJSubTran = NULL;
		
		vim_mem_set(&sTxn, 0x00, sizeof(sTxn));
		pCJSubTran = Wally_GetEdpSubTransaction(i, &sTxn);
		if (VIM_ERROR_NONE != receipt_add_edp_sub_transaction(type, receipt, pCJSubTran, &sTxn))
		{
			pceft_debugf(pceftpos_handle.instance, "Failed to add subtransaction #%d/%d to receipt", i + 1, Wally_GetEdpSubTransactionCount());
		}
	}

	receipt_add_horizontal_rule(receipt, VIM_FALSE, VIM_TRUE);
	if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
	{
		receipt_add_total(transaction, receipt, VIM_NULL);

		receipt_add_response_description(transaction->error, transaction->host_rc_asc, transaction->auth_no, transaction, receipt, VIM_FALSE, DiagnosticsRequired(transaction), VIM_NULL);

		receipt_add_edp_rollback(NULL, 0, receipt);

		receipt_add_date_time(&transaction->time, receipt, transaction->rrn, VIM_NULL);
	}
	else
	{
		VIM_CHAR szLeft[24 + 1] = { 0 }, szRight[24 + 1] = { 0 };
		//Rollback|Total
		receipt_add_edp_rollback(szLeft, sizeof(szLeft), receipt);
		receipt_add_total(transaction, receipt, szRight);
		receipt_add_right_justified_item(szLeft, szRight, vim_strlen(szRight), receipt);
		//Response description
		vim_mem_clear(szLeft, sizeof(szLeft));
		vim_mem_clear(szRight, sizeof(szRight));
		receipt_add_response_description(transaction->error, transaction->host_rc_asc, transaction->auth_no, transaction, receipt, VIM_FALSE, DiagnosticsRequired(transaction), szRight);
		//Date time|response code
		receipt_add_date_time(&transaction->time, receipt, transaction->rrn, szLeft);
		receipt_add_right_justified_item(szLeft, szRight, vim_strlen(szRight), receipt);
	}
	receipt_add_dotted_line('-', receipt);

	return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR receipt_build_fin_trans(VIM_UINT32 type, transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
    VIM_BOOL duplicate_receipt;
  VIM_BOOL merchant_receipt;
  VIM_BOOL customer_receipt;
  VIM_BOOL signature_required;
  VIM_BOOL advice_receipt;
  VIM_CHAR LeftBuffer[1 + RECEIPT_LINE_MAX_WIDTH / 2], RightBuffer[1 + RECEIPT_LINE_MAX_WIDTH / 2];
  VIM_CHAR *LeftPtr = LeftBuffer, *RightPtr = RightBuffer;

  receipt->lines = 0; 
  
  //receipt->stan = transaction->stan;
  receipt->stan = valid_stan(transaction->orig_stan) ? transaction->orig_stan : transaction->stan; // RDD 120612 SPIRA:5645 Transaction STAN gets incremented if reversal set
  // DBG_VAR(receipt->stan);
  receipt->approved = is_approved_txn(transaction);
  
  vim_mem_set(receipt->reference, ' ', VIM_SIZEOF(receipt->reference));

  // RDD 010812 - noticed that the 5100 fils out the POS REF in the receipt header and we didn't 
  vim_mem_copy( receipt->reference, txn.pos_reference, WOW_POS_REF_LEN );

 //VIM_DBG_VAR(type);
  
  /* work out types */
  duplicate_receipt = (type & receipt_duplicate);
  merchant_receipt = (type & receipt_merchant);
  customer_receipt = (type & receipt_customer);
  signature_required = (type & receipt_signature);
  advice_receipt = (type & receipt_advice);

  if(( merchant_receipt )&&( !duplicate_receipt ))
  {
	  if( terminal.standalone_parameters & STANDALONE_INTERNAL_PRINTER )    
		  receipt_add_centered_string("Merchant Copy", receipt);       
	  receipt->type = PCEFTPOS_RECEIPT_TYPE_MERCHANT;
  }

  if(( customer_receipt )&&( !duplicate_receipt ))
  {
	  if( terminal.standalone_parameters & STANDALONE_INTERNAL_PRINTER )    
		  receipt_add_centered_string("Customer Copy", receipt);    
	  receipt->type = PCEFTPOS_RECEIPT_TYPE_CUSTOMER;
  }

  if ( duplicate_receipt )
    receipt->type = PCEFTPOS_RECEIPT_TYPE_DUPLICATE;
  
  if( advice_receipt )
    receipt->type = PCEFTPOS_RECEIPT_TYPE_ADVICE;

  if( !LOCAL_PRINTER )
	receipt_add_dotted_line('-', receipt);

  /* add top line on the receipt */
  //receipt_add_centered_string( terminal.configuration.receipt_top_line, receipt );
  if (duplicate_receipt)
    receipt_add_centered_string("*   DUPLICATE RECEIPT  *", receipt);
  if(merchant_receipt && signature_required)
    receipt_add_centered_string("*SIGNATURE CONFIRMATION*", receipt);
  
  AddTrainingMode(receipt, duplicate_receipt);

  receipt_add_merchant_details(receipt);    

  if (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH )
  {
      VIM_CHAR *PtrL = rc_item_L("MERCH ID:", terminal.merchant_id, 15);
      VIM_CHAR *PtrR = rc_item_R("TERM ID:", terminal.terminal_id, 8);
      receipt_add_right_justified_item( PtrL, PtrR, 24, receipt );
  }
  else
  {  
      receipt_add_mid(receipt);  
      receipt_add_tid(receipt);
  }

  if (IsEverydayPay(transaction) && (0 < Wally_GetEdpSubTransactionCount()))
  {
	  return receipt_build_edp_receipt(type, transaction, receipt);
  }
	
  // If not MAX width - print within fn else build static lh and rh strings and print after

  vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));
  vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));

  RightPtr = &RightBuffer[0];
  LeftPtr = &LeftBuffer[0];

  receipt_add_card_number( merchant_receipt, duplicate_receipt, transaction, receipt, RightPtr );
  receipt_add_account_info( transaction, receipt, LeftPtr );

  if (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH)
  {	    
      receipt_add_right_justified_item(LeftBuffer, RightBuffer, 24, receipt);
  }

  if ( transaction_get_training_mode() || ( duplicate_receipt && (txn_duplicate.status & st_training_mode) ) )
  {
	  if (( card_chip == transaction->card.type ) || ( card_picc == transaction->card.type ))
	  {
		receipt_add_centered_string("AID:      A1234567890123", receipt);
		receipt_add_centered_string("ARQC:     A1234567890123", receipt);
		if( receipt->approved )
		{
			receipt_add_centered_string("TC:       A1234567890123", receipt);
		}
		else	// RDD note that the Ingenico 5100 Training mode receipt prints TC even if declined !
		{
			receipt_add_centered_string("AAC:      A1234567890123", receipt);
		}
	  }
  }
  else
  {
      if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
      {
          receipt_add_emv_data(transaction,
              EMV_DECLINED_RESPONSE_CODE(transaction->host_rc_asc) ||
              (card_picc == transaction->card.type && !transaction->card.data.picc.msd &&
                  VIM_ERROR_NONE != transaction->error &&
                  ERR_WOW_ERC != transaction->error &&
                  ERR_SIGNATURE_DECLINED != transaction->error),
              receipt, merchant_receipt);
      }
  }

  // RDD 230212: Moved from before emv data to after so to be like 5100
  if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
  {
      receipt_add_amounts(transaction, receipt);
  }

  if (PAPER_WIDTH == RECEIPT_LINE_MAX_WIDTH)
  {
      vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));
      vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));

      print_aid(transaction, receipt, LeftPtr);          
      receipt_add_txn_amount(transaction, receipt, RightPtr);      
      receipt_add_right_justified_item(LeftPtr, RightPtr, 24, receipt);
      
      //if (transaction->amount_cash > 0 && (transaction->amount_cash != transaction->amount_total))
      if (transaction->amount_cash )
      {
          receipt_add_amount("CASH OUT", VIM_FALSE, transaction->amount_cash, receipt, RightPtr );
          receipt_add_right_justified_item( "", RightPtr, 24, receipt);
      }

      // TVR + ----
      vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));
      vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));
      print_tvr(transaction, VIM_TRUE, receipt, merchant_receipt, LeftPtr);
      vim_strcpy( RightPtr, "------------" );
      receipt_add_right_justified_item(LeftPtr, RightPtr, 24, receipt);

      // ARQC + TOTAL
      vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));
      vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));
      print_cryptograms(transaction, receipt, LeftPtr);
      receipt_add_total(transaction, receipt, RightPtr);
      receipt_add_right_justified_item(LeftPtr, RightPtr, 24, receipt);
  }

  // RDD 110712 SPIRA:5733 - Fuel data goes directly after amounts
  // RDD 270512 added for fuel cards
  if( fuel_card( transaction ) )
  {
	  if( transaction->u_PadData.PadData.VehicleNumber )
	  {
		  VIM_CHAR Buffer[10];
		  vim_sprintf( Buffer, "%d", (VIM_SIZE)transaction->u_PadData.PadData.VehicleNumber );
		  receipt_add_right_justified_item("VEHICLE #", Buffer, vim_strlen(Buffer), receipt);
	  }
	  if( transaction->u_PadData.PadData.OdoReading )
	  {
		  VIM_CHAR Buffer[10];
		  vim_sprintf( Buffer, "%d", (VIM_SIZE)transaction->u_PadData.PadData.OdoReading );
		  receipt_add_right_justified_item("ODOMETER", Buffer, vim_strlen(Buffer), receipt);
	  }
	  if( transaction->u_PadData.PadData.OrderNumber )
	  {
		  VIM_CHAR Buffer[20];
		  VIM_CHAR *Ptr = Buffer;
		  vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
		  bin_to_asc( transaction->u_PadData.PadData.OrderNumber, Buffer, WOW_MAX_ORDER_NO_LEN );
		  while( *Ptr == '0' ) ++Ptr;
		  receipt_add_right_justified_item("ORDER #", Ptr, vim_strlen(Ptr), receipt);
	  }

	  // RDD 070612 SPIRA:5635 - always print a STAN but it might be all zeros if the msg wasn't sent
	  // RDD 130812 SPIRA:5766 - Now they don't want a STAN printed if zero
	  if(receipt->stan)
	  {
		  VIM_CHAR buffer[6+1];
		  vim_sprintf( buffer, "%6.6d", receipt->stan );
		
		  receipt_add_right_justified_item("STAN", buffer, 6, receipt);
	  }
	  
	  if(( transaction->u_PadData.PadData.DE63_Response[0] )&&( customer_receipt ))
	  {
		  receipt_add_issuer_msg( transaction->u_PadData.PadData.DE63_Response, receipt );
	  }
  }
  else
  {
	  if( IsUPI( transaction ))
	  {		  	  
		  if(receipt->stan)
		  {
			  VIM_CHAR buffer[6+1];
			  vim_sprintf( buffer, "%6.6d", receipt->stan );
			  receipt_add_right_justified_item("STAN", buffer, 6, receipt);
		  }
	  }
  }
  // 210818 RDD JIRA WWBP-316 They don't want the reason code on Integrated
  if(( merchant_receipt && signature_required ) && ( POS_CONNECTED ))
  {
	  DBG_MESSAGE( "No response description for signature receipt" );
  }
  else
  {
      if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
      {
          receipt_add_response_description(transaction->error, transaction->host_rc_asc, transaction->auth_no, transaction, receipt, VIM_FALSE, DiagnosticsRequired(transaction), VIM_NULL );
      }
	  if( is_standalone_mode() && transaction_get_status( transaction, st_efb )  && is_approved_txn( transaction ) && ( merchant_receipt||duplicate_receipt ))
	  {
		  receipt_add_reason_code( transaction->error_backup, transaction->host_rc_asc_backup, receipt );
	  }
      if(( txn.moto_reason == moto_reason_telephone || txn.moto_reason == moto_reason_mail_order ))
      {
          receipt_add_moto_reason_code(transaction, receipt);
      }
  }

  // RDD 070612 SPIRA:5628
  // RDD 200712 SPIRA:5788 remove this line
  //receipt_add_blank_lines(1, receipt);

  if ( (transaction->qualify_svp) && (receipt->approved) && (!vim_strncmp( transaction->host_rc_asc, "00", 2 ) )) 
	//BRD Only to be printed on approved receipts
  {
	  receipt_add_centered_string( "NO PIN OR SIGNATURE REQD", receipt );
  }

  // RDD. For customer copy of a completion the 5100 adds the auth number
  // RDD 230212 - Noticed 5100 doesn't print Auth number on Merchant receipt

  // RDD 190612 SPIRA:5674 Auth Number to go on merchant receipt if present

  if(1)
  {
	  // RDD - genpos sets auth code to all zeros unless valid txn for auth code (eg. completion) 
	  VIM_UINT64 AuthCode = asc_to_bin( transaction->auth_no, AUTH_CODE_LEN );
		
	  // RDD 021214 Email from WOW indicates that Authcode is requred on both receipts
#if 0
	  if(( AuthCode ) && ( customer_receipt ))
#else
	  if( AuthCode )
#endif
	  //if(( transaction_get_status( transaction, st_completion ) ) && ( customer_receipt ))
	  {
		receipt_add_right_justified_item("AUTH NO:", transaction->auth_no, vim_strlen(transaction->auth_no), receipt);
		// RDD. Add date/time and the first 6 bytes of the RRN
	  }
  }

  // RDD 070512 SPIRA:5439 Add balance to gift card txns
  // RDD 260713 SPIRA:

  // Add some dummy balance amts for training mode
  if (transaction_get_training_mode())
  {
      txn.LedgerBalance = 60;
      txn.ClearedBalance = 50;
  }

  if(( gift_card(transaction) ) && ( duplicate_receipt == VIM_FALSE ) && (( transaction->LedgerBalance >= 0 )||( transaction->ClearedBalance >= 0 )) )
  {
	  // RDD 221221 JIRA PIRPIN-1367 Shouldn't display zero balance on "GA" already activated receipts
	  if(( !vim_strcmp(transaction->host_rc_asc, "GA" )) && (transaction->LedgerBalance == 0))
	  {			
		  pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-1367");
	  }

	  else if ((!vim_strcmp(transaction->host_rc_asc, "GP")) && (transaction->LedgerBalance == 0))
	  {
		  pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-2397");
	  }

      else if( PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH )
      {
          receipt_add_blank_lines(1, receipt);
          receipt_add_amount("BALANCE", VIM_FALSE, transaction->LedgerBalance, receipt, VIM_NULL);
      }
      else
      {
          vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));
          vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));
          receipt_add_amount("BALANCE", VIM_FALSE, transaction->LedgerBalance, receipt, RightPtr );
          receipt_add_right_justified_item( "", RightPtr, 24, receipt);
      }
  }


  if (PAPER_WIDTH != RECEIPT_LINE_MAX_WIDTH)
  {
      VIM_CHAR *Ptr = VIM_NULL;
      receipt_add_date_time(&transaction->time, receipt, transaction->rrn, Ptr);
  }
  else
  {
      vim_mem_clear(LeftBuffer, VIM_SIZEOF(LeftBuffer));
      vim_mem_clear(RightBuffer, VIM_SIZEOF(RightBuffer));
      receipt_add_date_time(&transaction->time, receipt, transaction->rrn, LeftPtr );
      if (!signature_required)
      {
          receipt_add_response_description(transaction->error, transaction->host_rc_asc, transaction->auth_no, transaction, receipt, VIM_FALSE, DiagnosticsRequired(transaction), RightPtr);
      }
      receipt_add_right_justified_item(LeftPtr, RightPtr, 24, receipt);

  }

  // RDD 270415 SPIRA:8590 Z1 decline was still printing sig panel for standalone completions
  //if( merchant_receipt && signature_required ) 

  if( merchant_receipt && signature_required )
  {
	  receipt_add_signature_panel( transaction, receipt );
  }
  receipt_add_dotted_line('-', receipt);

  // RDD 030615 SPIRA:8747 Add diagnostics to standalone receipts
  if( DiagnosticsRequired( transaction ) )
  {		  
	  receipt_add_emv_data_diagnostic_w( transaction, receipt, VIM_FALSE, PAPER_WIDTH);
  }

  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR receipt_build_financial_transaction(VIM_UINT32 type, transaction_t *transaction, VIM_PCEFTPOS_RECEIPT *receipt)
{
    VIM_ERROR_PTR res;
    VIM_SIZE Cols = PAPER_WIDTH;

#ifdef _WIN32   // Fudge to test EDP receipt is always narrow
 //   if( transaction->amount_total == 1200 )
//        transaction->card.type = card_qr;
#endif

    // Hardcode EDP receipts to narrow format
    if (IsEverydayPay(transaction) && !TERM_QR_ENABLE_WIDE_RCT)
    {
        PAPER_WIDTH = _24_COL_PRN;
    }
    receipt->receipt_columns = PAPER_WIDTH;

    res = receipt_build_fin_trans(type, transaction, receipt);
    // Restore global value
    PAPER_WIDTH = Cols;
    return res;
}
