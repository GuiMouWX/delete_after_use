#include <incs.h>
VIM_DBG_SET_FILE("TW");
static const char *totals_file_name = VIM_FILE_PATH_LOCAL "totals.dat";

/* ---------------------------------------------------------------- */
/*  Delete totals file */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_delete(void)
{
  VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(totals_file_name));

  /* return without error */
  return VIM_ERROR_NONE;
}

/* ---------------------------------------------------------------- */
/*  Loads details from file */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_load(void)
{
  VIM_BOOL exists;

  vim_mem_clear(&shift_details, sizeof(shift_details));
  
  /* check if there is a saved terminal configuration file */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(totals_file_name, &exists));

  if (exists)
  {
    /* Read the file*/
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(totals_file_name, &shift_details, VIM_SIZEOF(shift_details), 0));
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

/* ---------------------------------------------------------------- */
/*  Loads details to file */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_save(void)
{
  VIM_ERROR_RETURN_ON_FAIL(vim_file_set(totals_file_name, &shift_details, VIM_SIZEOF(shift_details)));
  
  return VIM_ERROR_NONE;
}

/* ---------------------------------------------------------------- */
/*  Reset the Shift totals */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_reset(VIM_UINT16 index)
{
	
	VIM_RTC_TIME temp_time = shift_details.shift_end;
  	
	/* init the whole structure */
  vim_mem_clear(&shift_details, VIM_SIZEOF(shift_details));

  /* read current date and time */
  // Use last rest time as shift start and pick up current time for shift end
  shift_details.shift_start = temp_time;
  VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&shift_details.shift_end));
  
  /* update the file */
  VIM_ERROR_RETURN_ON_FAIL(shift_totals_save());

  return VIM_ERROR_NONE;
}
/* ---------------------------------------------------------------- */
void update_credit_totals(host_totals_t *totals, transaction_t *temp_txn)
{
  switch (temp_txn->type)
  {
    case tt_void:
      break;
      
    case tt_adjust:
    case tt_tip:
      totals->credit_sale_amount += temp_txn->amount_tip;
      break;

    case tt_refund:
      totals->credit_refund_count++;
      totals->credit_refund_amount += temp_txn->amount_refund;
      break;

    case tt_preauth:
      totals->preauth_sale_amount += temp_txn->amount_purchase;
      totals->preauth_sale_count++;
      break;

    default:
      totals->credit_sale_count++;
      totals->credit_sale_amount += temp_txn->amount_total;
      break;  
  }
}
/* ---------------------------------------------------------------- */
void update_debit_totals(host_totals_t *totals, transaction_t *temp_txn)
{
  switch (temp_txn->type)
  {
    case tt_void:
      break;
    
    case tt_adjust:
    case tt_tip:
      totals->debit_sale_amount += temp_txn->amount_tip;
      break;
      
    case tt_refund:
      totals->debit_refund_count++;
      totals->debit_refund_amount += temp_txn->amount_refund;
      break;

    case tt_preauth:
      /* Preauth is always credit */
      break;

    default:
      totals->debit_sale_count++;
      totals->debit_sale_amount += temp_txn->amount_total;
      break;  
  }
}

/* ---------------------------------------------------------------- */
/*  Updates the shift totals for the terminal */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR totals_update(totals_t * totals, transaction_t * temp_txn)
{
  /*  Dont update when in training mode */
  if (!terminal.training_mode)
  {
    switch (temp_txn->type)
    {
      case tt_void:
        break;

      case tt_adjust:
      case tt_tip:
        totals->amount_sale += temp_txn->amount_tip;
        break;

      case tt_refund:
        totals->refund_count++;
        totals->amount_refund += temp_txn->amount_refund;
        break;

      case tt_preauth:
        totals->preauth_count++;
        totals->amount_preauth += temp_txn->amount_purchase;
        break;

      case tt_cashout:
        totals->cash_count++;
        totals->amount_cash += temp_txn->amount_cash;
        break;

      case tt_sale_cash:
        if (temp_txn->amount_purchase)
        {
          totals->sale_count++;
          totals->amount_sale += ( temp_txn->amount_purchase + temp_txn->amount_surcharge );
        }
        if (temp_txn->amount_cash)
        {
          totals->cash_count++;
          totals->amount_cash += temp_txn->amount_cash;
        }
        break;

      default:
        totals->sale_count++;
        totals->amount_sale += temp_txn->amount_total;
        break;
    }

    if ((account_credit == temp_txn->account)||( account_default == temp_txn->account ))

      update_credit_totals(&host_totals, temp_txn);
    else
      update_debit_totals(&host_totals, temp_txn);
  }

  
  return VIM_ERROR_NONE;
}



/* ---------------------------------------------------------------- */
/*  Print the shift totals */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_print(VIM_BOOL reset_totals, VIM_UINT8 index)
{
  VIM_PCEFTPOS_RECEIPT receipt;
 
  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));
  display_screen(DISP_SHIFT_TOTALS_PRINTING, VIM_PCEFTPOS_KEY_NONE);
  
  /*  Always approved */
  settle.error = VIM_ERROR_NONE;
  /*  Set the response size */
  /*settle.short_response = VIM_TRUE;*/
  
  receipt_build_shift_totals(&receipt, &shift_details, reset_totals);

  pceft_pos_print(&receipt, VIM_TRUE);

  //display_result(settle.error, "00", "SHIFT TOTALS", "", VIM_PCEFTPOS_KEY_OK,TWO_SECOND_DELAY);

  pceft_pos_display_close();

  if (reset_totals)
    shift_totals_reset(index);

  return VIM_ERROR_NONE;
}
/* ---------------------------------------------------------------- */
/*  Calculate the void shift totals */
/* ---------------------------------------------------------------- */
void void_shift_totals_update(shift_totals_t *totals, transaction_t *txn)
{
  switch (txn->type)
  {
    case tt_sale:
      totals->amount_purchase -= txn->amount_purchase;
      totals->purchase_count--;
      break;
    
    case tt_sale_cash:
      totals->amount_purchase -= txn->amount_purchase;
      totals->purchase_count--;           
      totals->amount_cash -= txn->amount_cash;
      totals->cash_count--;
      break;
  
    case tt_cashout:
      totals->amount_cash -= txn->amount_cash;
      totals->cash_count--;
      break;
  
    case tt_refund:
      totals->amount_refund -= txn->amount_refund;
      totals->refund_count--;
      break;
    
    case tt_tip:
    case tt_adjust:
        totals->amount_tips -= txn->amount_tip;
        totals->tips_count--;
      break;

    default:
      break;
  }
}


/* ---------------------------------------------------------------- */
/*  Calculate the shift totals */
/* ---------------------------------------------------------------- */

void shift_totals_one_scheme(scheme_totals_t *scheme_totals, transaction_t *transaction, VIM_BOOL add )
{
	scheme_totals->purchase_count += 1;
	scheme_totals->amount_purchase += (add) ? transaction->amount_total : -transaction->amount_total;
}


void shift_totals_scheme_update(shift_totals_t *totals, transaction_t *transaction, VIM_BOOL add)
{

	if (IsEFTPOS( transaction ))
	{
		shift_totals_one_scheme( &totals->scheme_totals[_eftpos], transaction, add );
	}
	else if(IsVISA( transaction ))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_visa], transaction, add );
	}
	else if(IsMC(transaction))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_mastercard], transaction, add );
	}
	else if(IsAmex(transaction))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_amex], transaction, add );
	}
	else if(IsUPI(transaction))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_upi], transaction, add );
	}
	else if(IsJCB(transaction))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_jcb], transaction, add );
	}
	else if (IsDiners(transaction))
	{
		shift_totals_one_scheme(&totals->scheme_totals[_diners], transaction, add );
	}
}


void shift_totals_update(shift_totals_t *totals, transaction_t *txn)
{
  switch (txn->type)
  {

    case tt_sale:  
	case tt_checkout:
	case tt_moto:
      totals->amount_purchase += (txn->amount_purchase+ txn->amount_surcharge);
      totals->purchase_count++;

	  if( txn->amount_cash )
	  {
	      totals->amount_cash += txn->amount_cash;
		  totals->cash_count++;
	  }
	  if( txn->amount_tip )
	  {
	      totals->amount_tips += txn->amount_tip;
		  totals->tips_count ++;
	  }
	  shift_totals_scheme_update( totals, txn, VIM_TRUE);
      break;
    
    case tt_sale_cash:
      totals->amount_purchase += (txn->amount_purchase+txn->amount_surcharge);
      totals->purchase_count++;           
      totals->amount_cash += txn->amount_cash;
      totals->cash_count++;
	  if( txn->amount_tip )
	  {
	      totals->amount_tips += txn->amount_tip;
		  totals->tips_count ++;
	  }
	  shift_totals_scheme_update(totals, txn, VIM_TRUE);
	  break;
  
    case tt_cashout:
      totals->amount_cash += txn->amount_cash;
      totals->cash_count++;
	  shift_totals_scheme_update(totals, txn, VIM_TRUE);
      break;

	  // RDD 190315 SPIRA:8477
	case tt_preauth:
      totals->amount_preauth += txn->amount_total;
      totals->preauth_count++;
      break;

  
    case tt_refund:
	case tt_moto_refund:
      totals->amount_refund += txn->amount_refund;
      totals->refund_count++;
	  shift_totals_scheme_update(totals, txn, VIM_FALSE);
	  // RDD 120422 JIRA PIRPIN-1566 Keep refund daily totals separate to Shift totals - reset on cutover
	  TERM_REFUND_DAILY_TOTAL += txn->amount_refund;
      break;

	case tt_deposit:
		totals->amount_deposit += txn->amount_total;
		totals->deposit_count++;
		shift_totals_scheme_update(totals, txn, VIM_FALSE);
		break;

  
    case tt_tip:
    case tt_adjust:
      totals->amount_tips += txn->amount_tip;
      totals->tips_count++;
      break;

    case tt_void:
      {
        transaction_t tmp_txn;
        
        /* find original transaction */
        if (VIM_ERROR_NONE == saf_read_record_index(txn->original_index, VIM_FALSE, &tmp_txn))
        {
          void_shift_totals_update(totals, &tmp_txn);

          if (0xFFFFFFFF != tmp_txn.original_index)
          {
            /* if transaction was tiped, find original transaction */
			  if (VIM_ERROR_NONE == saf_read_record_index(tmp_txn.original_index, VIM_FALSE, &tmp_txn))
			  {
				  void_shift_totals_update( totals, &tmp_txn );
				  shift_totals_scheme_update( totals, txn, VIM_FALSE );
			  }
          }
        }
      }
      break;
      
    default:
      break;
  }

  shift_totals_save();
}
/* ---------------------------------------------------------------- */
/*  Shift totals processing */
/* ---------------------------------------------------------------- */
VIM_ERROR_PTR shift_totals_process(VIM_BOOL reset_totals)
{
  VIM_ERROR_RETURN_ON_FAIL( is_ready() );
  
  display_screen(DISP_PRINTING_PLEASE_WAIT, VIM_PCEFTPOS_KEY_NONE);  
  shift_totals_print(reset_totals, 0);
    
  if (!reset_totals)
  {
      if (config_get_yes_no("RESET SHIFT TOTALS?", "RESET OK"))
          shift_totals_reset(0);
  }
  else
      shift_totals_reset(0);
  return VIM_ERROR_NONE;
}

