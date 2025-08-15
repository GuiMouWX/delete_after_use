
#include "incs.h"
#include "tms.h"
#include <vim_vas.h>

#ifndef _WIN32
#include <signal.h>
#include <pthread.h>
#endif

VIM_DBG_SET_FILE("TI")

extern VIM_UINT8 com_buf[4096];
extern VIM_RTC_UPTIME LastPinEntryHeartBeat;
extern VIM_ERROR_PTR wow_form_ePan(void);
extern VIM_CHAR type[20];
extern txn_state_t txn_preswipe_amount_entry(void);
extern const card_bin_name_item_t CardNameTable[];
extern const char *duplicate_txn_file_name;
extern VIM_BOOL CardRemoved( VIM_SIZE timeout );
extern VIM_BOOL ProcessBalance( VIM_UINT8 *BcdBal, VIM_INT32 *TextBal );



extern void slave_loop(    /** [in] Self PCEFTPOS Ptr */
    VIM_PCEFTPOS_PTR self_ptr,
    /** [in] Bank type of application */
    VIM_UINT8 bankcode,
    /** [in] Pointer to a uart instance connected to PCEFTPOS */
    VIM_UART_PTR uart_ptr,
    /** [in] Pointer to the UART parameters for the given UART PTR */
    VIM_UART_PARAMETERS const * uart_parameters,
    /** [in] Pointer to the protocol used to connect to PCEFTPOS */
    VIM_UART_PROTOCOL const * protocol_ptr,
    /** [in] Pointer to slave request message */
    void const * req,
    /** [in] Number of bytes in the request message */
    VIM_SIZE reqlen,
    /** Slave mode timeout - set up by enter slave mode command */
    VIM_SIZE *Timeout
);


// 130220 DEVX Disable PCI Token and ePAN return for Third Party Terminals
void mask_out_track_details_txn
(
    VIM_CHAR *des_track_2,
    VIM_UINT8 des_track_2_len,
    card_info_t const *card,
    VIM_UINT8 bin,
    VIM_BOOL slave_masking,
	VIM_BOOL LinklyMasking
);

VIM_ERROR_PTR ConvertPanToEpan(transaction_t *transaction);


//VIM_ERROR_PTR pceft_pos_card_get_bin(VIM_UINT8 issuer_index, account_t account_type, VIM_UINT8 *bin);

// define a global union to store the card data. This data is kept in a file and then restored to use within the txn
card_buffer_t stored_card_info;

#define INTERRUPT_RESOLUTION 50 // RDD: 50ms between checks
#define PIN_ENTRY_HEARTBEAT_TIMEOUT  100


VIM_ERROR_PTR pceft_pos_command_check( transaction_t *transaction, VIM_MILLISECONDS local_timeout )
{
	VIM_MILLISECONDS count = local_timeout/INTERRUPT_RESOLUTION;

	VIM_BOOL default_event=VIM_FALSE;
	VIM_EVENT_FLAG_PTR pceftpos_event_ptr=&default_event;
	VIM_EVENT_FLAG_PTR event_list[1];

	if (IS_STANDALONE)
	{
		return VIM_ERROR_NONE;
	}

    VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceftpos_event_ptr));
    event_list[0] = pceftpos_event_ptr;

	do
	{
		//DBG_POINT;
		vim_event_wait( event_list, VIM_LENGTH_OF(event_list), INTERRUPT_RESOLUTION );
		if( IS_INTEGRATED )
		{
           // VIM_DBG_POINT;
			vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceftpos_event_ptr);
			if(( *pceftpos_event_ptr )||( terminal.abort_transaction ))
			{
				DBG_MESSAGE( "<<<<< PCEFT COMMAND INTERRUPT >>>>>>" );
				if( terminal.abort_transaction )
				{
					VIM_DBG_POINT;
					//terminal.abort_transaction = VIM_FALSE;
				}
				terminal.allow_ads = VIM_FALSE;
				VIM_ERROR_RETURN( VIM_ERROR_POS_CANCEL );
			}
		}
#if 0
		if( transaction_get_status( transaction, st_offline_pin_verified ) )
		{
			DBG_MESSAGE( "<<<<< OFFLINE PIN WAS ENTERED >>>>>>" );
			return VIM_ERROR_NONE;
		}
#endif
		if(( transaction->emv_error == VIM_ERROR_EMV_PIN_BYPASSED )||( transaction->emv_error == VIM_ERROR_USER_CANCEL ))
		{
			VIM_DBG_ERROR( transaction->emv_error );
			VIM_ERROR_RETURN( transaction->emv_error );
		}
#if 0	// RDD TEST V7
		if( transaction_get_status( transaction, st_offline_pin ) )
		{
			VIM_RTC_UPTIME UpTimeNow;
			vim_rtc_get_uptime(&UpTimeNow);
			DBG_VAR( UpTimeNow );
			DBG_VAR( LastPinEntryHeartBeat );
			if( UpTimeNow > ( LastPinEntryHeartBeat + PIN_ENTRY_HEARTBEAT_TIMEOUT ))
			{
				VIM_ERROR_RETURN_ON_FAIL( VIM_ERROR_EMV_CARD_REMOVED );
			}
		}
#endif

#if 0	// RDD 190213
		if( txn.card.type == card_chip )
		{
			VIM_BOOL card_present = VIM_TRUE;
			vim_icc_check_card_slot( &card_present );
			if( !card_present )
			{
				DBG_MESSAGE( "<<<<< EMV CARD WAS REMOVED >>>>>>" );
				return VIM_ERROR_EMV_CARD_REMOVED;
			}
		}
#endif
	}
	while( count-- );

	// We expect regular timeouts or VIM_ERROR_NONE if there was an event - anything else returns the error
	return VIM_ERROR_TIMEOUT;
}

// Pick up any read track1 data and mask out any digits with ascii '0's
static VIM_BOOL get_track_data( VIM_CHAR *track_data, VIM_UINT8 TrackNumber )
{
	VIM_CHAR *ptr;
	VIM_SIZE n;
	VIM_BOOL ValidTrack = VIM_FALSE;

	if( txn.card.type == card_mag )
	{
		switch( TrackNumber )
		{
			case 3:
			if( txn.card.data.mag.track3_present == VIM_FALSE )
				break;
			// intentional drop through...
			case 1:
			if( txn.card.data.mag.track1_length )
    		{
    			vim_mem_copy( track_data, txn.card.data.mag.track1, txn.card.data.mag.track1_length );
	    		ValidTrack = VIM_TRUE;
    		}
			break;

			case 2:
			if( txn.card.data.mag.track2_length )
    		{
    			vim_mem_copy( track_data, txn.card.data.mag.track2, txn.card.data.mag.track2_length );
	    		ValidTrack = VIM_TRUE;
    		}
			break;

			default:
			break;

    	}
    }
	else if( txn.card.type == card_chip )
	{
		if( TrackNumber == 1 )
		{
    		if( txn.card.data.chip.track1_length )
    		{
    			vim_mem_copy( track_data, txn.card.data.chip.track1, txn.card.data.chip.track1_length );
	    		ValidTrack = VIM_TRUE;
			}
		}
		else if( TrackNumber == 2 )
		{
			if( txn.card.data.chip.track2_length )
			{
				vim_mem_copy( track_data, txn.card.data.chip.track2, txn.card.data.chip.track2_length );
	    		ValidTrack = VIM_TRUE;
			}
		}
	}
    // mask out any non-numeric data
    for( n=0, ptr=track_data; n<vim_strlen(track_data); n++, ptr++ )
    	if(( *ptr >= '0' ) && ( *ptr <= '9' )) *ptr = '0';

	return ValidTrack;
}


static VIM_ERROR_PTR get_pci_card_data( VIM_CHAR *card_data, VIM_BOOL fAddEquals, transaction_t *transaction, VIM_BOOL QueryResponse)
{
  VIM_SIZE size = 0;
  VIM_CHAR *Ptr = card_data;

  if (transaction->card.type != card_none)
  {

      // 130220 DEVX Return Masked PAN if LFD Flag set for Third Party Operator
      if(( terminal.terminal_id[0] == 'L' ) || ((!TERM_USE_PCI_TOKENS) && (!QueryResponse)))
      {
          VIM_UINT8 PceftBin = PCEFT_BIN_UNKNOWN;

          pceft_pos_card_get_bin(0, transaction->account, &PceftBin);
		  if (terminal.terminal_id[0] == 'L')
		  {
			  mask_out_track_details_txn(card_data, WOW_PAN_LEN_MAX, &transaction->card, PceftBin, VIM_TRUE, VIM_TRUE);
		  }
		  else
		  {
			  mask_out_track_details_txn(card_data, WOW_PAN_LEN_MAX, &transaction->card, PceftBin, VIM_FALSE, VIM_FALSE);
		  }
      }

	  // RDD SPIRA IN5253: EPAN DECRYPTION : If there is a token AND PinPad is not full AND the txn was not a "91" do manual then send token
	  else if(( transaction->card.pci_data.eTokenFlag ) && ( vim_strncmp(transaction->host_rc_asc, "TP", 2)) && ( vim_strncmp(transaction->host_rc_asc, "91", 2)  ))
	  {
		  size = vim_strlen(transaction->card.pci_data.eToken);
          vim_mem_copy( Ptr, transaction->card.pci_data.eToken, size );
		  // RDD 180412 - the txn response 'm' needs to have an '=' sign appended for the POS to decode the data correctly
		  // ( but this stuffs up decryption so need it to be optional )
		  if( fAddEquals )
		  {
			Ptr+=size;
			*Ptr = '=';
		  }
	  }
	  // Otherwise send the ePan
	  else
	  {
          // RDD JIRA PIRPIN-935 For Avalon Query Card ( J8 ) no host interraction so generate epan locally
          if ((txn.type == tt_query_card) && (txn.card_state == card_capture_new_gift))
          {
              ConvertPanToEpan(&txn);
          }
		  size = vim_strlen(transaction->card.pci_data.ePan);
		  vim_mem_copy( card_data, transaction->card.pci_data.ePan, size );
	  }
  }
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
/**
 * \brief
 * Search the bin table
 *
 * \details
 */
VIM_ERROR_PTR pceft_pos_card_get_bin(VIM_UINT8 issuer_index, account_t account_type, VIM_UINT8 *bin)
{
  VIM_UINT32 CpatIndex = txn.card_name_index;
  VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

  *bin = PCEFT_BIN_UNKNOWN;

  if ((account_type == account_savings) || (account_type == account_cheque))
  {
    *bin = PCEFT_BIN_DEBIT_CARD;
  }
  else
  {
	if( transaction_get_status( &txn, st_cpat_check_completed ))
	{
		*bin = CardNameTable[CardNameIndex].bin;
	}
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR pceftpos_set_training_mode( VIM_BOOL training_mode )
{
	// RDD 050813 added for v418- Offline test mode is kind on Training mode but with no logon needed
	terminal.training_mode = VIM_FALSE;
	if(( terminal_get_status(ss_offline_test_mode) ) && ( terminal_get_status(ss_wow_basic_test_mode) ))
	{
		terminal.training_mode = VIM_TRUE;
		return VIM_ERROR_NONE;
	}

	// RDD 011211: Don't allow training mode txns if we have anything pending....
	if( training_mode )
	{
		VIM_ERROR_RETURN_ON_FAIL( terminal_check_config_change_allowed( VIM_TRUE ) );
	}

	terminal.training_mode = training_mode;
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/**
 * \brief
 * Validates the transaction type sent and puts transaction into training mode
 *
 * \details
 */



VIM_ERROR_PTR validate_transaction_type(VIM_CHAR txn_type)
{

    //VIM_ERROR_PTR ret = VIM_ERROR_SYSTEM;
    VIM_BOOL training_mode = VIM_FALSE;

  // Filter out any txn types not allowed in the CTLS only terminal
  if (TERM_CTLS_ONLY)
  {
      switch (txn_type)
      {
      case PCEFT_TRAINING_TXN_TYPE_PURCHASE:
          training_mode = VIM_TRUE;
      case PCEFT_TXN_TYPE_PURCHASE:
          txn.type = tt_sale;
          break;

      case PCEFT_TRAINING_TXN_TYPE_REFUND:
          training_mode = VIM_TRUE;
      case PCEFT_TXN_TYPE_REFUND:
          txn.type = tt_refund;
          break;

      default:
          return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
      }
  }

  /* Switch the type */
  switch (txn_type)
  {
    case PCEFT_TRAINING_TXN_TYPE_PURCHASE:
      training_mode = VIM_TRUE;
    case PCEFT_TXN_TYPE_PURCHASE:
      txn.type = tt_sale;
      break;

	case PCEFT_TRAINING_TXN_TYPE_DEPOSIT:
      training_mode = VIM_TRUE;
	case PCEFT_TXN_TYPE_DEPOSIT:
		txn.type = tt_deposit;
		break;

	case PCEFT_TRAINING_TXN_TYPE_BALANCE:
      training_mode = VIM_TRUE;
	case PCEFT_TXN_TYPE_BALANCE:
		txn.type = tt_balance;
		break;

    case PCEFT_TRAINING_TXN_TYPE_CASHOUT:
      training_mode = VIM_TRUE;
    case PCEFT_TXN_TYPE_CASHOUT:
      txn.type = tt_cashout;
      break;

      // RDD 141019 WWBP-689 Remove'F' type txn as this is beyond the scope of the P400 port project
      // RDD 231018 added for new Gift Card POS S/W

    case PCEFT_TXN_TYPE_FUNDS_TRANSFER_TRAINING:
        training_mode = VIM_TRUE;
    case PCEFT_TXN_TYPE_FUNDS_TRANSFER:
        txn.type = tt_gift_card_activation;
        break;


        // RDD 130421 added JIRA PIRPIN-1038 void support
    case PCEFT_TRAINING_TXN_TYPE_VOID:
        training_mode = VIM_TRUE;
    case PCEFT_TXN_TYPE_VOID:
        txn.type = tt_void;
        break;

    case PCEFT_TRAINING_TXN_TYPE_REFUND:
      training_mode = VIM_TRUE;
    case PCEFT_TXN_TYPE_REFUND:
      txn.type = tt_refund;
      break;

    case PCEFT_TRAINING_TXN_TYPE_COMPLETION:
      training_mode = VIM_TRUE;
      /* Currently not support checkout in training mode */
      //return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
    case PCEFT_TXN_TYPE_COMPLETION:
		// RDD 301211 - added voucher txn so can test original txn types for completions
	case PCEFT_TXN_TYPE_VOUCHER:
      txn.type = tt_completion;
      break;

	case PCEFT_TXN_TYPE_MINI_TXN_HISTORY:
        txn.type = tt_mini_history;
        break;

	case PCEFT_TXN_TYPE_PREAUTH:
        // RDD - sent T8 for WOW or other error code for non-WOW
        if((TERM_ALLOW_PREAUTH)||(!TERM_USE_PCI_TOKENS))
        {
            txn.type = tt_preauth;
            break;
        }

    default:
      return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
  }

  VIM_ERROR_RETURN_ON_FAIL( pceftpos_set_training_mode( training_mode ) );

  /* its valid so store it - Save cmd-type for duplicate trans */
  txn.pos_cmd = txn_type;

  return VIM_ERROR_NONE;
}
/**
 * \brief
 * Validates the transaction amounts sent by the POS
 *
 * \details
 */
VIM_ERROR_PTR validate_transaction_amounts(VIM_PCEFTPOS_TRANSACTION_REQUEST const *cmd)
{
    // RDD 220621 Void uses recalled txn - QA may screw up amount deliberately so ensure they can't do that. 
    if ((txn.type == tt_void) && (!TERM_USE_PCI_TOKENS))
    {
        return VIM_ERROR_NONE;
    }


    
  txn.amount_purchase = asc_to_bin(cmd->amount_purchase, sizeof(cmd->amount_purchase));
  txn.amount_refund = asc_to_bin(cmd->amount_refund, sizeof(cmd->amount_refund));
  txn.amount_cash = asc_to_bin(cmd->amount_cash, sizeof(cmd->amount_cash));
  txn.amount_total = txn.amount_purchase + txn.amount_cash;

  VIM_DBG_WARNING("------------- CHECK AMOUNTS ---------------");
  VIM_DBG_NUMBER(txn.amount_purchase);
  VIM_DBG_NUMBER(txn.amount_cash);
  VIM_DBG_NUMBER(txn.amount_total);


  // RDD 301211: Rather dangerous functionality ordered by BD because PCEFT Client doesn't seem to populate Refund amt field for voucher refunds
  if(( transaction_get_status( &txn, st_completion )) && ( txn.type == tt_refund ))
  {
	  if( !txn.amount_refund )
	  {
		  txn.amount_refund = txn.amount_purchase;
		  txn.amount_purchase = 0;
	  }
  }


  if (CheckRefundLimits() == VIM_FALSE)
  {
      VIM_ERROR_RETURN(ERR_OVER_REFUND_LIMIT_T);
  }

#if 1
  if ( txn.amount_cash > ( TERM_CASHOUT_MAX*100 ))
  {
      display_result(ERR_CASH_OVER_MAX, "", "", "", VIM_PCEFTPOS_KEY_OK, THREE_SECOND_DELAY);
      vim_kbd_sound();
      return ERR_CASH_OVER_MAX;
  }
#endif

  /* If zero amounts reject the transaction */
  if( (txn.type ==  tt_sale) && (!txn.amount_purchase) && (!txn.amount_cash))
  {
    VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
  }

  // If there is some amout set return error for some txn types
  if(( txn.amount_purchase ) || (txn.amount_cash ))
  {
	  // RDD - no amounts for balance enquiry - actually 5100 does allow ....
	  if(( txn.type == tt_balance ) || ( txn.type == tt_mini_history ))
	  {
		VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
	  }
  }

  // Don't allow a refund if no refund amount
  if(( txn.amount_refund == 0 ) && ( txn.type == tt_refund ))
  {
    VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
  }

  // RDD 021111 - don't allow cash on refunds
  if(( txn.amount_cash ) && ( txn.type == tt_refund ))
  {
    VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
  }

  /* Check if the transaction is a Purchase + cashout */
  if((txn.amount_purchase) && (txn.amount_cash) && ( txn.type == tt_sale ))
  {
      VIM_DBG_WARNING("------------- SET SALE CASH ---------------");
	  txn.type = tt_sale_cash;
  }
  /* Check if cash out only */
  // RDD 020512 SPIRA:5421 this stuffs up cashout completions which need txn.type as completion
  //if ((txn.amount_cash) && (!txn.amount_purchase))
  if ((txn.amount_cash) && (!txn.amount_purchase) && ( txn.type != tt_completion ))
  {
      VIM_DBG_WARNING("------------- SET CASH ONLY ---------------");
      txn.type = tt_cashout;
  }

  /* Set amount total to refund amount for refunds */
  if (txn.type == tt_refund)
  {
	  txn.amount_total = txn.amount_refund;
  }

  // RDD 231018 Gift Card Processing from POS - change type to refund
  if (txn.type == tt_gift_card_activation)
  {
      //txn.type = tt_refund;
      if (txn.amount_refund == 0)
      {
          txn.amount_refund = txn.amount_purchase;
          txn.amount_total = txn.amount_purchase;
      }
  }

  return VIM_ERROR_NONE;
}
/**
 * \brief
 * Stores account type
 *
 * \details
 * If we get an account type we dont need to prompt for one
 */
#if 1
VIM_BOOL check_account_type( account_t pos_account )
{
	VIM_BOOL AccountVerified = VIM_FALSE;

	txn.pos_account = VIM_TRUE;

	txn.account = ( txn.account == account_unknown_any ) ? pos_account : txn.account;

	switch( pos_account )
	{
    case PCEFT_SAVING_ACCOUNT:
      AccountVerified = ( txn.account == account_savings ) ? VIM_TRUE : VIM_FALSE;
      break;

    case PCEFT_CHEQUE_ACCOUNT:
      AccountVerified = ( txn.account == account_cheque ) ? VIM_TRUE : VIM_FALSE;
      break;

	  // RDD - pretty sure that all default accounts will be mapped to Credit for WOW
    case PCEFT_DEFAULT_ACCOUNT:
    case PCEFT_CREDIT_ACCOUNT:
      AccountVerified = (( txn.account == account_credit )||( txn.account == account_default )) ? VIM_TRUE : VIM_FALSE;
      break;

	  // No account data supplied. This is allowable but need to run account selection
    default:
		// RDD 211112 SPIRA:6219 - account will normally be supplied for deposits but if not default to credit ( really just for GenPos testing )
		if( txn.type == tt_deposit )
		{
			txn.pos_account = VIM_TRUE;
			txn.account = account_credit;
		}
        // RDD 271120 for backwards compatibility we need to set SAV for WEX cards returned as ePAN in J8 query
        else if (txn.card_state == card_capture_new_gift)
        {
            txn.pos_account = VIM_TRUE;
            txn.account = account_savings;
        }
		else
		{
			txn.pos_account = VIM_FALSE;
			txn.account = account_unknown_any;
			AccountVerified = VIM_TRUE;
		}
		break;
	}
	return AccountVerified;
}
#endif
/**
 * \brief
 * Stores RRN
 *
 * \details
 *
 */
void store_roc(transaction_t *transaction, VIM_CHAR* txn_ref, VIM_UINT8 len)
{
  VIM_INT16 i;
  VIM_BOOL invalid = VIM_FALSE;

  for (i = len - 1; i >= 0; i--)
  {
    if ((txn_ref[i] > '9') || (txn_ref[i] < '0'))
      txn_ref[i] = 0;
    else
    {
      invalid = VIM_TRUE;
      break;
    }
  }

  if (invalid == VIM_TRUE)
    len = i + 1;
  else
    len = 0;

  VIM_DBG_STRING(txn_ref);
 //VIM_DBG_VAR(len);
  transaction->roc = asc_to_bin(txn_ref, len);
 //VIM_DBG_VAR(txn.roc);
}
/*----------------------------------------------------------------------------*/
/* Sets the reason for manual entry*/
VIM_ERROR_PTR set_manual_reason(char entry_mode)
{
  switch(entry_mode)
  {
    case PCEFT_ENTRY_MOTO_INTERNET:
    case PCEFT_ENTRY_MOTO_INTERNET_RECURRING:
    case PCEFT_ENTRY_MOTO_INTERNET_INSTALLMENT:
      txn.card.data.manual.manual_reason = moto_internet;
      break;

    case PCEFT_ENTRY_MOTO_TELEPHONE_ORDER:
    case PCEFT_ENTRY_MOTO_TELEPHONE_RECURRING:
    case PCEFT_ENTRY_MOTO_TELEPHONE_INSTALLMENT:
      txn.card.data.manual.manual_reason = moto_telephone;
      break;

    case PCEFT_ENTRY_MOTO_MAIL_ORDER:
    //case PCEFT_ENTRY_MOTO_MAIL_RECURRING:
    case PCEFT_ENTRY_MOTO_MAIL_INSTALLMENT:
      txn.card.data.manual.manual_reason = moto_mail;
      break;
#if 0
    case PCEFT_ENTRY_MOTO_CUSTOMER_PRESENT:
      txn.card.data.manual.manual_reason = moto_customer_present;
      break;
#endif
    default:
      txn.card.data.manual.manual_reason = moto_faulty_card;
      break;
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Sets the recurrence reason for manual entry */
void set_recurrence_reason(char entry_mode)
{
  switch(entry_mode)
  {
    case PCEFT_ENTRY_MOTO_INTERNET:
    case PCEFT_ENTRY_MOTO_TELEPHONE_ORDER:
    case PCEFT_ENTRY_MOTO_MAIL_ORDER:
      txn.card.data.manual.recurrence = moto_single;
      break;

    //case PCEFT_ENTRY_MOTO_MAIL_RECURRING:
    case PCEFT_ENTRY_MOTO_TELEPHONE_RECURRING:
    case PCEFT_ENTRY_MOTO_INTERNET_RECURRING:
      txn.card.data.manual.recurrence = moto_recurring;
      break;

    case PCEFT_ENTRY_MOTO_MAIL_INSTALLMENT:
    case PCEFT_ENTRY_MOTO_INTERNET_INSTALLMENT:
    case PCEFT_ENTRY_MOTO_TELEPHONE_INSTALLMENT:
      txn.card.data.manual.recurrence = moto_installment;
      break;

    default:
      txn.card.data.manual.recurrence = moto_unknown;
      break;
  }
}




/*----------------------------------------------------------------------------
RDD 241011 : 91 DO MANUAL COMPLETION: Check EPAN is Valid before attempting a recovery of card data
-----------------------------------------------------------------------------*/
VIM_BOOL ValidateEpan( VIM_CHAR *ePan )
{
	VIM_SIZE count = 0;
	char *Ptr = ePan;

	while(( *(Ptr++) != '=' ) && ( count++ <= MAX_PAN_LENGTH )); *(Ptr+4) = '\0';
	return (( vim_strlen(ePan) < E_PAN_MIN_LEN ) || ( vim_strlen(ePan) > E_PAN_MAX_LEN )) ? VIM_FALSE : VIM_TRUE;
}

static VIM_UINT8 GetePanLen( VIM_CHAR *ePan )
{
	VIM_UINT8 Len = 0;
	VIM_CHAR *Ptr = ePan;

	while(( *Ptr++ != '=' )&&( Len++ < E_PAN_MAX_LEN ));
	return Len-1; // to account for the PCI key index2563
}

/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR vim_drv_tcu_wow_process_epan
(
  VIM_TCU_PTR self_ptr,
  VIM_CHAR *term_id,
  VIM_CHAR *pan,
  VIM_CHAR *exp_mmyy,
  VIM_CHAR *ePan,
  VIM_BOOL decrypt
)
{
  static VIM_DES_BLOCK const iv=
  {{
    0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF
  }};

  VIM_DES_BLOCK digest;
  VIM_CHAR Buffer[32+1];
  VIM_UINT8 clear_pan_block[16];
  VIM_UINT8 encrypt_pan_block[16];
  VIM_CHAR encrypt_ascii_pan_block[32+1];
  VIM_SIZE PanLen = decrypt ? GetePanLen(ePan) : vim_strlen(pan);

  VIM_UINT8 kpci_index=0;

  VIM_CHAR AsciiDigestBuffer[14];

  // RDD 120112: Firstly build the digest in Ascii format

  // First 5 Bytes are the first 4 digits of the TID ( Store number ) NOT inc. the W as this is added to the compressed data
  vim_mem_copy( AsciiDigestBuffer, &term_id[1], 4  );

  // Add the first 6 digits of the PAN/ePAN
  // Add the last 4 digits of the PAN/ePAN
  if( !decrypt )
  {
	vim_mem_copy( &AsciiDigestBuffer[4], pan, 6  );
    vim_mem_copy( &AsciiDigestBuffer[10], &pan[PanLen-4], 4 );
  }
  else
  {
	vim_mem_copy( &AsciiDigestBuffer[4], ePan, 6  );
    vim_mem_copy( &AsciiDigestBuffer[10], &ePan[PanLen-3], 4 );
  }

  // compress into bcd format
  asc_to_bcd( AsciiDigestBuffer, &digest.bytes[1], 14 );

  // First byte of the digest is the 'W' - first byte of the TID
  digest.bytes[0] = term_id[0];

  // xor the completed digest with the IV constant
  vim_mem_xor( digest.bytes, iv.bytes, VIM_SIZEOF(VIM_DES_BLOCK) );

  if( decrypt )
  {
    // first 6 bytes are used in the clear
    vim_mem_copy( encrypt_ascii_pan_block, ePan, 6 );

	// skip the PCI index ( ePan[6] ) in the ePan and copy over the rest of the PAN. Everything except last 4 digits is 'encrypted'
    vim_mem_copy( &encrypt_ascii_pan_block[6], &ePan[7], PanLen-6 );

    // skip the '=' sign and copy over the encrypted expiry date
    vim_mem_copy( &encrypt_ascii_pan_block[PanLen], &ePan[PanLen+2], 4 );

    //Fill the BCD buffer with 0xFF
    vim_mem_set( encrypt_pan_block, 0xFF, VIM_SIZEOF(encrypt_pan_block) );

	// Convert the formatted ePan and Exp date to BCD -
    asc_to_hex( encrypt_ascii_pan_block, encrypt_pan_block, PanLen+4 );

    // Encrypt the result to obtain the clear PAN and expiry Date
#if 1
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_pci_encrypt(
      self_ptr,
      clear_pan_block,
      VIM_SIZEOF(clear_pan_block),
      encrypt_pan_block,
      &digest));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_des2_ofb_encrypt(
    clear_pan_block,
    encrypt_pan_block,
    VIM_SIZEOF(clear_pan_block),
    &self_ptr->merchant.kpci,
    &digest));
#endif
    // convert the Decypted block back to the ascii buffer - note we only want to keep the middle bit as the rest is rubbish
    hex_to_asc( clear_pan_block, Buffer, PanLen+4 );

    // copy back the first 6 Bytes from the ePan and overwrite the garbage in the Buffer
    vim_mem_copy( Buffer, ePan, 6 );

	// copy back the last 4 Bytes from the ePan and overwrite the garbage in the Buffer
	vim_mem_copy( &Buffer[PanLen-4], &ePan[PanLen-3], 4 );

    // Copy the Pan Into the Pan field and NULL terminate it
    vim_mem_copy( pan, Buffer, PanLen );
    pan[PanLen] = '\0';

    // copy over the expiry date mmyy into the expiry date field and NULL terminate
    vim_mem_copy( exp_mmyy, &Buffer[PanLen+2], 2 );
    vim_mem_copy( &exp_mmyy[2], &Buffer[PanLen], 2 );
    exp_mmyy[4] = '\0';
  }
  else
  {
    vim_mem_set( &clear_pan_block, 0xFF, VIM_SIZEOF(clear_pan_block) );
    vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );
    vim_strncpy( Buffer, pan, PanLen );

	vim_strncpy( &Buffer[PanLen], &exp_mmyy[2], 2 );
    vim_strncpy( &Buffer[PanLen+2], exp_mmyy, 2 );
    asc_to_bcd( Buffer, clear_pan_block, PanLen + 4 );

#if 0
    VIM_ERROR_RETURN_ON_FAIL(vim_des2_ofb_encrypt(
    encrypt_pan_block,
    clear_pan_block,
    VIM_SIZEOF(encrypt_pan_block),
    &self_ptr->merchant.kpci,
    &digest));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_pci_encrypt(
      self_ptr,
      encrypt_pan_block,
      VIM_SIZEOF(encrypt_pan_block),
      clear_pan_block,
      &digest));
#endif
    // convert the entire encrypted block to ascii
    hex_to_asc( encrypt_pan_block, encrypt_ascii_pan_block, 32 );
    encrypt_ascii_pan_block[32] = '\0';

    // form the ascii ePan out of a wierd mixture of the clear text and the encrypted data
    bcd_to_asc( clear_pan_block, ePan, 6 );

    // 7th position is the PCI Index

    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_kpci_index( self_ptr, &kpci_index ));
    ePan[6] = kpci_index;

    // 8th -> PanLen+1 position is the encrypted part of the PAN
    vim_mem_copy( &ePan[7], &encrypt_ascii_pan_block[6], PanLen-6 );

	// Ovwerwrite last four digits of the actual EPan Pan with the last four digits of the clear PAN
	vim_mem_copy( &ePan[PanLen-3], &pan[PanLen-4], 4 );

    // PanLen+1 position is the field seperator
    ePan[PanLen+1] = '=';

    // 22nd - 225th postions are mapped from the expiry date positions from the encrypted block
    vim_mem_copy( &ePan[PanLen+2], &encrypt_ascii_pan_block[PanLen], 4 );

    // NULL terminate so we can use this as a string
    ePan[PanLen+6] = '\0';
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------
RDD 241011 : 91 DO MANUAL COMPLETION: Convert EPan sent from POS to Manual Data
-----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_tcu_wow_process_epan
(
  VIM_TCU_PTR self_ptr,
  VIM_CHAR *term_id,
  VIM_CHAR *pan,
  VIM_CHAR *exp_mmyy,
  VIM_CHAR *ePan,
  VIM_BOOL decrypt
)
{
	VIM_ERROR_PTR res;
    VIM_CHAR *Ptr = decrypt ? ePan : pan;

    VIM_ERROR_RETURN_ON_NULL(self_ptr);

    if (!VIM_ISDIGIT(*Ptr))
    {
        VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
    }
    pceft_debug_test(pceftpos_handle.instance, exp_mmyy);
    if(( res = vim_drv_tcu_wow_process_epan( self_ptr, term_id, pan, exp_mmyy, ePan, decrypt )) == VIM_ERROR_NONE )
	{
		if( decrypt )
		{
			pceft_debug_test(pceftpos_handle.instance, "DECRYPTED EPAN OK:" );
			pceft_debug_test(pceftpos_handle.instance, ePan );
			pceft_debug_test(pceftpos_handle.instance, pan );
			pceft_debug_test(pceftpos_handle.instance, exp_mmyy );
		}
		else
		{
			pceft_debug_test(pceftpos_handle.instance, "ENCRYPTED EPAN OK:" );
			pceft_debug_test(pceftpos_handle.instance, ePan );
		}
	}
	else
	{
		if( decrypt )
			pceft_debug_error(pceftpos_handle.instance, "DECRYPT FAILED", res );
		else
			pceft_debug_error(pceftpos_handle.instance, "ENCRYPT FAILED", res );
	}
    return VIM_ERROR_NONE;
}

VIM_ERROR_PTR ConvertEpanToManual( VIM_CHAR *pos_card_data )
{
    VIM_CHAR pan[20];
    VIM_TEXT exp_mmyy[5] = {0};

	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_wow_process_epan( tcu_handle.instance_ptr, terminal.terminal_id, pan, exp_mmyy, pos_card_data, VIM_TRUE ));

	vim_strcpy( txn.card.data.manual.pan, pan );
	vim_mem_copy( txn.card.data.manual.expiry_mm, exp_mmyy, 4 );
	txn.card.data.manual.pan_length = vim_strlen(pan);
	txn.card.data.manual.no_ccv_reason = ccv_not_provided;
	txn.card.type = card_manual;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR ConvertPanToEpan( transaction_t *transaction )
{
    VIM_CHAR pan[20+1];
    VIM_CHAR epan[E_PAN_MAX_LEN + 1];
    VIM_TEXT exp_mmyy[5] = { 0,0,0,0,0 };

    vim_mem_clear(pan, VIM_SIZEOF(pan));
    vim_mem_clear(epan, VIM_SIZEOF(epan));

    card_get_pan( &transaction->card, pan );
    card_get_expiry( &transaction->card, exp_mmyy);

    VIM_DBG_PRINTF1("Card Expiry: %s", exp_mmyy);

    // RDD 271120 - No expiry for Manually entered Giftcards so fix as some time in distant future as per current BGC
    // Note this is mainly to avoid "Offline Decline due to expired card as gift cards can no longer expire
    // RDD 050221 - QC Mag cards come with expiry of "0000" so probably better to leave this as is and do a general "pass-out" for Gift Cards in expiry checking instead
#if 0
    if (!vim_strlen(exp_mmyy))
    {
        vim_strcpy(exp_mmyy, "1230");
    }
#endif
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_wow_process_epan( tcu_handle.instance_ptr, terminal.terminal_id, pan, exp_mmyy, epan, VIM_FALSE ));

    vim_mem_copy(transaction->card.pci_data.ePan, epan, E_PAN_MAX_LEN);

    return VIM_ERROR_NONE;
}






/*----------------------------------------------------------------------------*/

VIM_BOOL GiftCardTxnType(VIM_PCEFTPOS_TRANSACTION_REQUEST *cmd)
{
    return ((cmd->type == PCEFT_TXN_TYPE_PURCHASE) ||
        (cmd->type == PCEFT_TXN_TYPE_FUNDS_TRANSFER) ||
        (cmd->type == PCEFT_TXN_TYPE_FUNDS_TRANSFER_TRAINING) ||
        (cmd->type == PCEFT_TXN_TYPE_REFUND) ||
        (cmd->type == PCEFT_TRAINING_TXN_TYPE_PURCHASE) ||
        (cmd->type == PCEFT_TRAINING_TXN_TYPE_REFUND) ||
        (cmd->type == PCEFT_TXN_TYPE_BALANCE) ||
        (cmd->type == PCEFT_TRAINING_TXN_TYPE_BALANCE)) ? VIM_TRUE : VIM_FALSE;
}


// RDD re-written for WOW Pre-swipe 250911 - deals with card data setup from PCEFTPOS
VIM_ERROR_PTR determine_entry_type(VIM_PCEFTPOS_TRANSACTION_REQUEST *cmd)
{
	// Reset the pre-swipe status at this point;
	txn.preswipe_status = no_preswipe;

    VIM_DBG_STRING("!!!!!!!!!!!!!!!!!! CARD DATA !!!!!!!!!!!!!!!!!!!!!");
    VIM_DBG_DATA(cmd->card_data, 40);
    // RDD 140520 Trello #66 VISA Tap Only terminal - MB says don't allow non-normal entry modes
    if (TERM_CTLS_ONLY)
    {
        if( cmd->entry_mode != PCEFT_ENTRY_MODE_NONE )
            return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
    }

    // RDD JIRA-PIRPIN 1120 apply tipping option from POS
    transaction_clear_status(&txn, st_tip_option);
    if( cmd->tipping_allowed != '0' )
    {
        transaction_set_status(&txn, st_tip_option);
    }

	switch( cmd->entry_mode )
	{
		// PinPad to Capture Card for this transaction
		case PCEFT_ENTRY_MODE_NONE:
		break;

		// ePan or Token is supplied
		case PCEFT_ENTRY_MODE_POS_SWIPED:
		if(( cmd->type == PCEFT_TXN_TYPE_DEPOSIT ) || ( cmd->type == PCEFT_TXN_TYPE_COMPLETION ) || ( cmd->type == PCEFT_TXN_TYPE_VOUCHER ))
		{
			// Find "91 DO MANUAL" Transaction with matching EPAN
			if( ValidateEpan( cmd->card_data ))
			{
				// If the ePan already exists in the SAF file then copy the Track details into the current transaction struct
				if( FindEpanInQueryBuffer( cmd->card_data, cmd->rrn, &txn ) != VIM_ERROR_NONE )
				{
					//display_result(txn.error, "", "", transaction_get_training_mode() ? STR_TRAINING_MODE : "", VIM_PCEFTPOS_KEY_ERROR, 0 );
					// Could'nt find the ePan in the SAF file so decrypt the card data provided to form a manual txn
					//VIM_ERROR_RETURN_ON_FAIL( ConvertEpanToManual( cmd->card_data ));
					//return res;
					// RDD 050412: SPIRA:5253 need to prompt for card if can't find ePAN
					DBG_MESSAGE( "Unable to find provided Epan in buffer. Prompt for Card Data" );
					//return VIM_ERROR_NONE;
				}
				else
				{
                    DBG_MESSAGE("Found provided Epan in buffer");

					// RDD SPIRA:6490 Problem with proc code for NZ 'S' type Deposits - looks like card.type was not being set properly for "S"
					transaction_set_status( &txn, st_pos_sent_pci_pan );
					if( cmd->type == PCEFT_TXN_TYPE_DEPOSIT )
					{
                        DBG_MESSAGE("----------- Deposit Transaction ----------");
                        transaction_clear_status( &txn, st_completion );
						txn.original_type = tt_none;
					}
				}
			}
		}
        // GC Activation ( refund ) or Redemption ( purchase ) - treat as type 3 for now
        else if( GiftCardTxnType(cmd))
        {
            VIM_CHAR *Ptr = VIM_NULL;

            // RDD 261120 JIRA PIRPIN-935 : Avalon POS I/F changes - New PAD Tag identifies new flow
            if( vim_strstr(cmd->purchase_analysis_data, "WWG", &Ptr) == VIM_ERROR_NONE)
            {
                VIM_CHAR pan[19 + 1];
                VIM_CHAR exp_mmyy[4 + 1];

                vim_mem_clear(pan, VIM_SIZEOF(pan));
                vim_mem_clear(exp_mmyy, VIM_SIZEOF(exp_mmyy));

                DBG_POINT;
                // RDD 220421 Fix issue with Barcode input for Activation and Balance - these come over directly in card data
                if ((cmd->type == PCEFT_TXN_TYPE_FUNDS_TRANSFER) || (cmd->type == PCEFT_TXN_TYPE_BALANCE))
                {
                    DBG_POINT;
                    VIM_ERROR_RETURN_ON_FAIL(QC_ProcessBarcode( &txn, cmd->card_data ));
                }
                else
                {
                    DBG_POINT;
					VIM_DBG_DATA(cmd->card_data, 40);
                
                    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_wow_process_epan(tcu_handle.instance_ptr, terminal.terminal_id, pan, exp_mmyy, cmd->card_data, VIM_TRUE));
                
                    pceft_debug_test(pceftpos_handle.instance, "ePAN included");
                    //if( vim_strstr( cmd->purchase_analysis_data, "ENT001S", &Ptr ) == VIM_ERROR_NONE )                
                
                    DBG_POINT;
                    txn.card.type = card_manual;
                    txn.card.data.manual.pan_length = VIM_MIN(WOW_PAN_LEN_MAX, vim_strlen(pan));
                    vim_strcpy(txn.card.data.manual.pan, pan);
                    vim_strncpy(txn.card.data.manual.expiry_mm, &exp_mmyy[0], 2);
                    vim_strncpy(txn.card.data.manual.expiry_yy, &exp_mmyy[2], 2);
                    VIM_DBG_DATA(txn.card.data.manual.expiry_mm, 2);
                    VIM_DBG_DATA(txn.card.data.manual.expiry_yy, 2);

                    if (vim_strstr(cmd->purchase_analysis_data, "ENT001B", &Ptr) == VIM_ERROR_NONE)
                    {
                        // Barcode 
                        DBG_POINT;
                        transaction_set_status(&txn, st_electronic_gift_card);
                        transaction_set_status(&txn, st_pos_sent_pci_pan);
                    }
                    else if (vim_strstr(cmd->purchase_analysis_data, "ENT001S", &Ptr) == VIM_ERROR_NONE)
                    {
                        // Keyed 
                        DBG_POINT;
                        transaction_set_status(&txn, st_pos_sent_swiped_pan);
                    }
                    else
                    {
                        DBG_POINT;
                        transaction_set_status(&txn, st_electronic_gift_card);
                        transaction_set_status(&txn, st_pos_sent_keyed_pan);
                    }
                    txn.card_state = card_capture_new_gift;     // To designate new ( Avalon ) flow
                    pceft_debug_test(pceftpos_handle.instance, "Epan decrypted OK");
                }
            }
            else
            {
                DBG_POINT;
                pceft_debug_test(pceftpos_handle.instance, "WOW E-GIFT CARD: PROMPT FOR PAN");
                transaction_clear_status(&txn, st_pos_sent_pci_pan);
                transaction_set_status(&txn, st_operator_entry);
            }
            transaction_set_status(&txn, st_electronic_gift_card);
            transaction_set_status(&txn, st_giftcard_s_type);           // New flag for S-type gift cards
        }
		break;

		// Cardholder not present - so no customer at all. Simply decrypt ePan and treat as manual entry
		case PCEFT_ENTRY_MODE_POS_KEYED:
		// Card entry Type has to be Manual by default
		txn.card.type = card_manual;
		transaction_set_status( &txn, st_pos_sent_pci_pan );

		if(( cmd->type != PCEFT_TXN_TYPE_COMPLETION )&&( cmd->type != PCEFT_TXN_TYPE_VOUCHER ))
		{
			return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
		}
		else
		{
			VIM_CHAR buffer[50];
			vim_mem_clear( buffer, 40 );
			vim_mem_copy( buffer, cmd->card_data, 40 );
			// Find "91 DO MANUAL" Transaction with matching EPAN

			pceft_debug_test(pceftpos_handle.instance, "INCOMING EPAN..." );
			pceft_debug_test(pceftpos_handle.instance, buffer );
			if( ValidateEpan( cmd->card_data ))
			{
				// If the ePan already exists in the SAF file then copy the Track details into the current transaction struct
				pceft_debug_test(pceftpos_handle.instance, "EPAN VALIDATED. TRY TO FIND IN BUFFER..." );

				if( FindEpanInQueryBuffer( cmd->card_data, cmd->rrn, &txn ) != VIM_ERROR_NONE )
				{
					// Could'nt find the ePan in the SAF file so decrypt the card data provided to form a manual txn
					pceft_debug_test(pceftpos_handle.instance, "CAN'T FIND EPAN IN BUFFER SO DECRYPT TO FORM PAN and EXPIRY DATE" );
					//VIM_ERROR_RETURN_ON_FAIL( ConvertEpanToManual( cmd->card_data ));
				}
				// RDD 070512 SPIRA:5421 Invalid Card data for K completions when card data found in buffer due to
				// card data not being decrypted into manual format when data found in buffer
				VIM_ERROR_RETURN_ON_FAIL( ConvertEpanToManual( cmd->card_data ));

				// RDD 030512 - Ensure that the current entry status is restored after the query buffer
				txn.card.type = card_manual;

				// RDD 231211 - indicate that the POS sent a valid EPAN or TOKEN so no card capture required
				//transaction_set_status( &txn, st_pos_sent_pci_pan );
			}
			else
			{
				pceft_debug_test(pceftpos_handle.instance, "EPAN VALIDATION FAILED: R1 ERROR" );
				// RDD 061112 SPIRA:6257 EPAN was invalid so prompt for PAN
				transaction_clear_status( &txn, st_pos_sent_pci_pan );
				transaction_set_status( &txn, st_operator_entry );
			}
		}
		break;

        //case PCEFT_ENTRY_BARCODE:
        // 041219 DEVX - this was changed to type 4 as PcEFT Client didn't send card_data for Barcode input.
        case PCEFT_ENTRY_MOTO_MAIL_RECURRING:
            pceft_debug_test(pceftpos_handle.instance, "WOW B-GIFT CARD: NO PROMPT");
            transaction_set_status(&txn, st_pos_sent_pci_pan);
            transaction_clear_status(&txn, st_operator_entry);
            transaction_set_status(&txn, st_electronic_gift_card);
            break;

        case PCEFT_ENTRY_MODE_WOW_GIFT_CARD:
			pceft_debug_test(pceftpos_handle.instance, "WOW E-GIFT CARD: PROMPT FOR PAN" );
			transaction_clear_status( &txn, st_pos_sent_pci_pan );
			transaction_set_status( &txn, st_operator_entry );
			transaction_set_status( &txn, st_electronic_gift_card );
			break;


            // RDD 060721 JIRA PIRPIN-1109 MOTO for Intergrated
        //case PCEFT_ENTRY_MOTO_INTERNET:
        case PCEFT_ENTRY_MOTO_TELEPHONE_ORDER:
        case PCEFT_ENTRY_MOTO_MAIL_ORDER:

            txn.moto_reason = cmd->entry_mode;
            if ( ((txn.type == tt_sale) || (txn.type == tt_refund)) && ( TERM_ALLOW_MOTO_ECOM ))
            {
                pceft_debugf_test(pceftpos_handle.instance, "MOTO reason code %c", cmd->entry_mode );
                transaction_set_status(&txn, st_moto_txn);
               // if( txn.type == tt_sale )
               //     txn.type = tt_moto;                    
                break;
            }
            else
            {
                pceft_debug(pceftpos_handle.instance, "MOTO not enabled for this merchant");
                vim_event_sleep(100);
            }
            // Intentional drop through to error as MOTO selected for invalid tran type

		default:
		return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
	}
	return VIM_ERROR_NONE;

}


/*----------------------------------------------------------------------------*/
void set_ccv(VIM_PCEFTPOS_TRANSACTION_REQUEST const *cmd)
{
  VIM_UINT8 ccv_len;

  if (txn.card.type == card_manual)
  {
    ccv_len = strlen_space_padded(cmd->ccv, sizeof(cmd->ccv));
   //VIM_DBG_VAR(ccv_len);
    VIM_DBG_DATA(cmd->ccv,ccv_len);
    if (ccv_len)
    {
      vim_mem_copy(txn.card.data.manual.ccv, cmd->ccv, VIM_MIN(ccv_len,MAX_CCV_LENGTH));
      txn.card.data.manual.no_ccv_reason = ccv_present;
    }
    else
    {
      txn.card.data.manual.no_ccv_reason = ccv_not_provided;
    }
  }
}

//VIM_SIZE ParsePadTag( VIM_CHAR *TagPtr )
VIM_UINT64 ParsePadTag( VIM_CHAR *TagPtr )
{
	VIM_CHAR *Ptr = TagPtr + PAD_TAG_LEN;
	VIM_SIZE DataLen = asc_to_bin( Ptr, PAD_TAG_FIELD_LEN);
	// RDD 131212 SPIRA:6449 - long ORDs were being truncated when coming in with PAD string - need UINT64 type for 16 digits
	VIM_UINT64 Data = 0;
	//VIM_SIZE Data = 0;

	// Skip the tag itself
	Ptr += PAD_TAG_FIELD_LEN;
	Data = asc_to_bin( Ptr, (VIM_UINT8)DataLen );
	return Data;
}

VIM_ERROR_PTR wow_strstr( VIM_CHAR *PrdPtr, const VIM_CHAR* Tag, VIM_CHAR **Ptr, VIM_SIZE MaxOffset )
{
	VIM_CHAR Buffer[WOW_MAX_PAD_STRING_LEN];
	VIM_SIZE Len,n;
	VIM_CHAR *Ptr2;

	vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );
	vim_mem_copy( Buffer, PrdPtr, MaxOffset );
	// The temp buffer only contains the current product string. If TAG not found in here then return error else return the real ptr
	VIM_ERROR_RETURN_ON_FAIL( vim_strstr( Buffer, Tag, Ptr ));

	Len = asc_to_bin( *Ptr+WOW_PAD_STRING_TAG_LEN, WOW_PAD_STRING_LEN_LEN );

	Ptr2 = *Ptr + WOW_PAD_STRING_TAG_LEN+WOW_PAD_STRING_LEN_LEN;

	// RDD 050612 - additional checks on Pad String format
	for( n=0; n <= Len; n++, Ptr2++ )
	{
		// check that the digits that are supposed ot be digits are actually digits
		if( n < Len )
		{
			if( *Ptr2 < '0' || *Ptr2 > '9' )
				return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
		}
		// Check that there are no extra digits after the end of the tag data
		if( n == Len )
		{
			if( *Ptr2 >= '0' && *Ptr2 <= '9' )
				return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
		}
	}
	VIM_ERROR_RETURN_ON_FAIL( vim_strstr( PrdPtr, Tag, Ptr ));
	return VIM_ERROR_NONE;
}





// RDD - 101111 - takes the Product Analysis Data from the POS Txn request and parses it into txn struct

VIM_BOOL InValidChar( VIM_CHAR PadChar )
{
	VIM_BOOL fInValid = VIM_TRUE;
	VIM_SIZE n;
	const VIM_CHAR ValidChars[] = { '0','1','2','3','4','5','6','7','8','9','P','I','D','A','M','T','P','M','P','V','O','L' };

	for( n=0; n< VIM_SIZEOF(ValidChars); n++ )
	{
		if( PadChar == ValidChars[n] )
		{
			fInValid = VIM_FALSE;
			break;
		}
	}
	return fInValid;
}


VIM_ERROR_PTR ValidatePADData( s_pad_data_t *PadData, VIM_BOOL FuelCard )
{
	VIM_SIZE n;
	VIM_CHAR *TagPtr, *PrdPtr = PadData->DE63;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_BOOL fAmountIncluded = VIM_FALSE;
	VIM_UINT64 TotalAmount = 0;

	// Process the individual baskets to convert product data into binary
	for( n=0; n<=WOW_MAX_PRODUCTS_IN_A_BASKET; n++ )
	{
		VIM_SIZE ProductLen = 0;
		// Look for a "PRD" Tag - if no more then break to read ODO etc.
		if( vim_strstr( PrdPtr, PRD, &TagPtr ) != VIM_ERROR_NONE )
			break;
		else
		{
			VIM_CHAR *NextPtr = PrdPtr;
			//VIM_SIZE m;
			// RDD 310712 SPIRA:5804 if too many product codes return T8
			if( n>=WOW_MAX_PRODUCTS_IN_A_BASKET )
			{
				vim_strcpy( txn.host_rc_asc, "T8" );	// Unable to process
				pceft_debug( pceftpos_handle.instance, "EXCEEDED MAX PRODUCTS" );
				return ERR_WOW_ERC;
			}
			NextPtr = PrdPtr = TagPtr+WOW_PAD_STRING_TAG_LEN;		// Skip this tag so it isn't found again...
			ProductLen = asc_to_bin( PrdPtr, WOW_PAD_STRING_LEN_LEN );
			PrdPtr+=WOW_PAD_STRING_LEN_LEN;
			NextPtr+=WOW_PAD_STRING_LEN_LEN;

			// RDD 130512 SPIRA:???? NOT CHECKING PRD Lengths

#if 0       // RDD 300920 Trello #279 Ignore Unknown PAD Tags
			for( m=0; m< ProductLen; m++, NextPtr++ )
			{
                if (InValidChar(*NextPtr))
                {
                    return VIM_ERROR_EMV_UNABLE_TO_PROCESS;
                }
			}
#endif
			//NextPtr += ( WOW_PAD_STRING_LEN_LEN + ProductLen );
			if( *NextPtr != 'P' && *NextPtr != 0 && *NextPtr != 'V' && *NextPtr != 'O' )					// Expect next Product or NULL ( or O or V ) at end of designated length
				return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
		}

		// Parse the Product ID (PID)
		if(( res = wow_strstr( PrdPtr, PID, &TagPtr, ProductLen )) == VIM_ERROR_NONE  )
			PadData->product_data[n].ProductID = (VIM_UINT8)ParsePadTag( TagPtr );
		else if( res == VIM_ERROR_NOT_FOUND )
		{
			vim_strcpy( txn.host_rc_asc, "F4" );	// Missing PID - exit with an error
			return ERR_WOW_ERC;
		}
		else
			return res;

		// Parse the Volume field (VOL)
		if(( res = wow_strstr( PrdPtr, VOL, &TagPtr, ProductLen )) == VIM_ERROR_NONE  )
		{
			if(( PadData->product_data[n].ProductVolume = (VIM_SIZE)ParsePadTag( TagPtr )) == 0 )
			{
				vim_strcpy( txn.host_rc_asc, "F1" );	// Zero Value in Volume TAG : return F1 error
				return ERR_WOW_ERC;
			}
		}
		else if( res != VIM_ERROR_NOT_FOUND )
			return res;

		// Parse the pump price
		if(( res = wow_strstr( PrdPtr, PMP, &TagPtr, ProductLen )) == VIM_ERROR_NONE  )
		{
			if(( PadData->product_data[n].PumpPrice = (VIM_SIZE)ParsePadTag( TagPtr )) == 0 )
			{
				vim_strcpy( txn.host_rc_asc, "F2" );	// Zero Value in Pump TAG : return F2 error
				return ERR_WOW_ERC;
			}
		}
		else if( res != VIM_ERROR_NOT_FOUND )
			return res;

		// Parse the Amount field (AMT)
		if(( res = wow_strstr( PrdPtr, AMT, &TagPtr, ProductLen )) == VIM_ERROR_NONE  )
		{
			fAmountIncluded = VIM_TRUE;

			if(( PadData->product_data[n].ProductAmount = (VIM_SIZE)ParsePadTag( TagPtr )) == 0 )
			{
				//VN 310518 JIRA WWBP-248: Invalid amount validation of product basket
				pceft_debug_test(pceftpos_handle.instance, "Zero amount in basket");
				#if 0
				vim_strcpy( txn.host_rc_asc, "F3" );	// Zero Value in Amt TAG : return F3 error
				return ERR_WOW_ERC;
				#endif
			}
			TotalAmount += PadData->product_data[n].ProductAmount;		// add the parsed amt to the total for chaecking against the txn amt later..
		}
		else
		{
			//return res;
			// RDD 130712 SPIRA:5740 NO AMT tag requires F3 fail
			vim_strcpy( txn.host_rc_asc, "F3" );	// Zero Value in Amt TAG : return F3 error
			return ERR_WOW_ERC;
		}
	}
	// Store the number of Product Groups for this incoming PAD
	txn.u_PadData.PadData.NumberOfGroups = n;

#if 0
	// Parse any split tender
	if( vim_strstr( PrdPtr, SPL, &TagPtr ) != VIM_ERROR_NOT_FOUND  )
		PadData->SplitTender = ParsePadTag( TagPtr );
#endif

	// Validate the total of all the product groups
	// RDD 151012 SPIRA:6139 This is switched off for Fuel Cards
	if( FuelCard )
	{
		if(( TotalAmount != txn.amount_total ) && ( fAmountIncluded ) && !(PadData->SplitTender))
		{
			vim_strcpy( txn.host_rc_asc, "F5" );
			return ERR_WOW_ERC;
		}
	}
#if 0
	// Parse any Loyalty
	if( vim_strstr( PrdPtr, LYL, &TagPtr ) != VIM_ERROR_NOT_FOUND  )
		PadData->LoyltyCard = ParsePadTag( TagPtr );

	// Parse any fuel discount
	if( vim_strstr( PrdPtr, DSC, &TagPtr ) != VIM_ERROR_NOT_FOUND  )
		PadData->FuelDiscount = ParsePadTag( TagPtr );
#endif
	// Parse the ODO reading
	if( vim_strstr( PrdPtr, ODO, &TagPtr ) != VIM_ERROR_NOT_FOUND )
	{
		PadData->OdoReading = ParsePadTag( TagPtr );
	}
	// or the ODO retry (remembering to set the retry flag for DE63 purposes)
	else if( vim_strstr( PrdPtr, ORT, &TagPtr ) != VIM_ERROR_NOT_FOUND  )
	{
		if(( PadData->OdoReading = ParsePadTag( TagPtr )) != 0 )
			PadData->OdoRetryFlag = VIM_TRUE;
	}

	// Parse the Vehicle number
	if( vim_strstr( PrdPtr, VHL, &TagPtr ) != VIM_ERROR_NOT_FOUND )
		PadData->VehicleNumber = ParsePadTag( TagPtr );

	// Parse the Order number
	if( vim_strstr( PrdPtr, ORD, &TagPtr ) != VIM_ERROR_NOT_FOUND )
		PadData->OrderNumber = ParsePadTag( TagPtr );

	if( FuelCard )
	{
		for( n=0; n<PadData->NumberOfGroups; n++ )
		{
			VIM_SIZE ProductGroupID = PadData->product_data[n].ProductID;
			product_def_t *ProductTable = &fuelcard_data.ProductTables[ProductGroupID];

			// Validate PID against records stored in the FCAT file
#if 1	// RDD 040612 - noticed that some poroducts don't have descriptions in the FCAT so better not check this as QA tests seem to expect these to pass!
			if( !vim_strlen( ProductTable->ProductDescription ))
			{
				res = ERR_WOW_ERC;
				vim_strcpy( txn.host_rc_asc, "F0" );
				break;
			}
#endif
		// Check to see that we have a valid (non-zero) Amount
			// VN 01062018 JIRA WWBP-248 : Invalid amount validation of product basket
			#if 0
			if( !PadData->product_data[n].ProductAmount )
			{
				vim_strcpy( txn.host_rc_asc, "F3" );
				break;
			}
			#endif
		// Check to see that we have a valid (non-zero) product ID
			if( !PadData->product_data[n].ProductID )
			{
				vim_strcpy( txn.host_rc_asc, "F4" );
				break;
			}

		// RDD 120612 SPIRA:5650 PID with NO VOLUME FLAG. PinPad should reject txn if non-zero volume

		// If we have a valid product then check the Volume Data - If Product Volume flag set then can't have zero Volume

			if((( ProductTable->VolumeFlag ) && ( !PadData->product_data[n].ProductVolume ))||(( !ProductTable->VolumeFlag ) && ( PadData->product_data[n].ProductVolume )))
			{
				res = ERR_WOW_ERC;
				vim_strcpy( txn.host_rc_asc, "F1" );
				break;
			}

		// If the Volume flag is set then we must also have a pump price
		    if((( ProductTable->VolumeFlag ) && ( !PadData->product_data[n].PumpPrice ))||(( !ProductTable->VolumeFlag ) && ( PadData->product_data[n].PumpPrice )))
			{
				res = ERR_WOW_ERC;
				vim_strcpy( txn.host_rc_asc, "F2" );
				break;
			}

#if 0 // RDD 040612 - some test scripts use incorrect values so this was removed to pass tests
		// If the pump price x volume != purchase amount
			if( ProductTable->VolumeFlag )
			{
				if( (PadData->product_data[n].ProductAmount)*1000 != (PadData->product_data[n].ProductVolume * PadData->product_data[n].PumpPrice) )
				{
					res = ERR_WOW_ERC;
					vim_strcpy( txn.host_rc_asc, "F5" );
					break;
				}
			}
#endif
		}
	}
	return res;
}




VIM_ERROR_PTR DeleteSafPrints(void)
{
	VIM_SIZE index = 0;
	//VIM_ERROR_PTR res;
	transaction_t transaction;

	saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count );

	// RDD 190315 - some corruption made huge number of SAF prints and routine below runsd for a long time
	if(( is_standalone_mode() ) || ( saf_totals.fields.saf_print_count > MAX_BATCH_SIZE ))
		saf_totals.fields.saf_print_count = 0;

	if( !saf_totals.fields.saf_count )
	{
		saf_destroy_batch( );
	}

	while( saf_totals.fields.saf_print_count )
	{
		if( saf_read_next_by_status( &transaction, st_message_sent, &index, VIM_TRUE ) == VIM_ERROR_NONE )
		{
			saf_delete_record( index, &transaction, VIM_FALSE );
			saf_totals.fields.saf_print_count -= 1;
		}
	}
	return VIM_ERROR_NONE;
}

void ControlSAFPrints( VIM_CHAR *PadString )
{
	VIM_CHAR *TagPtr = VIM_NULL;

	// RDD 030513 added for P4 - value of 0 denotes delete remaining SAF prints and don't store any more
	if( vim_strstr( PadString, SFP, &TagPtr ) == VIM_ERROR_NONE )
	{
		// RDD - beware of mal-formatted strings
		if( vim_strlen( TagPtr ) <= (WOW_PAD_STRING_LEN_LEN + WOW_PAD_STRING_TAG_LEN)) return;
		TagPtr += (WOW_PAD_STRING_LEN_LEN + WOW_PAD_STRING_TAG_LEN);
		if( *TagPtr == '0' )
		{
			terminal_set_status( ss_no_saf_prints );
			DeleteSafPrints();
		}
		else if( *TagPtr == '1' )
			terminal_clear_status( ss_no_saf_prints );
		// else - leave it as it was
	}
}


VIM_ERROR_PTR process_pad( VIM_CHAR *PadString )
{
	VIM_CHAR *TagPtr = PadString;
	VIM_SIZE PadLen, DE63Len = 0;
	VIM_CHAR* Ptr = VIM_NULL;
	s_pad_data_t *PadData = &txn.u_PadData.PadData;

	// RDD 160922 JIRA PIRPIN_1815 set status flag to Return standard TAGS for BPOZ (and LIVE etc.)
	if ((!IS_R10_POS) && (!FUEL_PINPAD) && (!ALH_PINPAD))
	{
		// RDD 160922 - ensure that both old and new Linkly methodology is supported by checking for either "SUR"or"SC2"tags.
		if ((vim_strstr(PadString, "SC2", &Ptr) == VIM_ERROR_NONE)||(vim_strstr(PadString, "SUR", &Ptr) == VIM_ERROR_NONE))
		{
			transaction_set_status(&txn, st_pos_sent_sc_tag);
		}
	}

	if (Wally_GetPaymentSessionIdFromPAD(PadString, txn.edp_payment_session_id, sizeof(txn.edp_payment_session_id)))
	{
		VIM_DBG_PRINTF1("Extracted Payment Session ID '%s' from PAD", txn.edp_payment_session_id);
	}

	Wally_AddPadDataToSession(PadString);

	if(( PadLen = asc_to_bin( PadString, WOW_PAD_STRING_LEN_LEN )) > WOW_MAX_PAD_STRING_LEN )
	{
		vim_strcpy( txn.host_rc_asc, "T8" );	// Unable to process
		pceft_debug( pceftpos_handle.instance, "PAD STRING EXCEEDS LIMIT" );
		return ERR_WOW_ERC;
	}
	if(PadLen)
	{

		ControlSAFPrints( PadString );

        // RDD 160221 JIRA PIRPIN-1008 - Avalon Flow for Balance
        if (vim_strstr(PadString, "WWG", &TagPtr) == VIM_ERROR_NONE)
        {
            txn.card_state = card_capture_new_gift;
            transaction_set_status(&txn, st_pos_sent_pci_pan);
            if (txn.type == tt_balance)
            {
                pceft_debug_test(pceftpos_handle.instance, "New Gift Card Flow - Balance");
            }
            // RDD 120321 Added for Receipts - Avalon is always in PAN / EXP (manual) format but may be swiped, keyed or scanned originally
            else if (vim_strstr(PadString, "ENT001S", &TagPtr) == VIM_ERROR_NONE)
            {
                transaction_set_status(&txn, st_pos_sent_swiped_pan);
            }
            else if (vim_strstr(PadString, "ENT001K", &TagPtr) == VIM_ERROR_NONE)
            {
                transaction_set_status(&txn, st_pos_sent_keyed_pan);
            }
        }


		// Look for existing basket data in the pad string
		if( vim_strstr( PadString, BSK, &TagPtr ) != VIM_ERROR_NONE )
		{
			DE63Len = PadLen - (WOW_PAD_STRING_HEADER_LEN - WOW_PAD_STRING_LEN_LEN); // We don't need the length header at the start of the PAD string for DE63
			TagPtr += WOW_PAD_STRING_HEADER_LEN;
		}
		else
		{
			// RDD 130912 P2 PRODUCTION PILOT ISSUE : LYL, PRC, DSC tags need to be stripped from DE63

			VIM_CHAR *GenPtr = TagPtr;
			if( vim_strstr( PadString, PRC, &GenPtr ) == VIM_ERROR_NONE )
			{
				VIM_UINT64 AfterPrcLen, PrcLen;

				// Save the Pointer to the start of the PRC string
				VIM_CHAR *PrcPtr = GenPtr;

				// Parse any split tender
				if( vim_strstr( PrcPtr, SPL, &GenPtr ) != VIM_ERROR_NOT_FOUND  )
					PadData->SplitTender = (VIM_BOOL)ParsePadTag( GenPtr );

				// Parse any Loyalty
				if( vim_strstr( PrcPtr, LYL, &GenPtr ) != VIM_ERROR_NOT_FOUND  )
					PadData->LoyltyCard = (VIM_BOOL)ParsePadTag( GenPtr );

				// Parse any fuel discount
				if( vim_strstr( PrcPtr, DSC, &GenPtr ) != VIM_ERROR_NOT_FOUND  )
					PadData->FuelDiscount = (VIM_BOOL)ParsePadTag( GenPtr );

				// Reset the Generic Ptr to the start of the PRC sub-container
				GenPtr = PrcPtr;

				// point to the length of the unwanted data
				GenPtr+=WOW_PAD_STRING_TAG_LEN;

				// Get the length of the actual PRC container data
				PrcLen = asc_to_bin( GenPtr, WOW_PAD_STRING_LEN_LEN );

				// Point to the start of the data itself
				GenPtr += WOW_PAD_STRING_LEN_LEN;

				// Calculate how much data there is after the PRC container (needs to be kept)
				if( vim_strlen( GenPtr ) >= PrcLen )
					AfterPrcLen = vim_strlen( GenPtr ) - PrcLen;
				else
				{
					// SPIRA 6063 - invalid PAD string with no data after PRC007 caused crash
					AfterPrcLen = 0;
					vim_strcpy( txn.host_rc_asc, "T8" );	// Unable to process
					return ERR_WOW_ERC;
				}
				// Clear the entire PRC sub-container
				vim_mem_clear( PrcPtr, PrcLen + WOW_PAD_STRING_LEN_LEN + WOW_PAD_STRING_TAG_LEN );

				// If there was anything after it (eg. ODO from a completion then copy it back to the new end of the string
				if( AfterPrcLen )
				{
					GenPtr += PrcLen;
					vim_mem_copy( PrcPtr, GenPtr, AfterPrcLen );
				}
			}
			// RDD END 130912 P2 PRODUCTION PILOT ISSUE : LYL, PRC, DSC tags need to be stripped from DE63
			DE63Len = vim_strlen( TagPtr );
		}
		vim_mem_copy( PadData->DE63, TagPtr, DE63Len );
	}
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR CompletePA(VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr)
{
    transaction_t test_txn;
    VIM_SIZE ROC_len = 0;
    VIM_CHAR *Ptr = request_ptr->rrn;
    VIM_UINT32 roc;
	VIM_CHAR AuthCode[7];
	VIM_SIZE batch_index;

    while (VIM_ISDIGIT(*Ptr) && ROC_len++ < VIM_SIZEOF(request_ptr->rrn)) Ptr++;

	if ((roc = (VIM_UINT32)asc_to_bin(request_ptr->rrn, ROC_len)) == 0)
	{
		// Just exit if no ROC sent in
		return VIM_ERROR_NOT_FOUND;
	}
    
	// Look in the batch for a matching tranaction
	VIM_ERROR_RETURN_ON_FAIL( saf_search_for_roc_tt( roc, &batch_index, tt_checkout, AuthCode));

    if( pa_read_record( roc, &test_txn) == VIM_ERROR_NONE)
    {        
		if (test_txn.amount_total < txn.amount_total)
		{
			pceft_debug(pceftpos_handle.instance, "(POS RRN) Completion exceeds\nAuthorised amount");
		}			
        txn.original_type = test_txn.type;
		txn = test_txn;
        txn.type = tt_checkout;  
		// We have the card details from the original preauth
		transaction_set_status(&txn, st_pos_sent_pci_pan);
		transaction_set_status(&txn, st_completion);
		transaction_set_status(&txn, st_needs_sending);
		return VIM_ERROR_NONE;
	}
	return VIM_ERROR_NOT_FOUND;
}



// RDD 180621 JIRA PIRPIN-1088 VOID supported over entire (settlement) day - need to search the batch for  asuitable matching transaction here
static VIM_ERROR_PTR ProcessThirdPartyVoid(VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr, VIM_SIZE POS_Ref_Len )
{
    transaction_t test_txn;
    VIM_SIZE index = 0;

    txn_do_init( &test_txn );

    // Look in the batch for a matching tranaction
    vim_strncpy(test_txn.pos_reference, request_ptr->reference, POS_Ref_Len);
    vim_strncpy(test_txn.rrn, request_ptr->rrn, 12);

    store_roc( &test_txn, (VIM_CHAR *)request_ptr->rrn, VIM_SIZEOF(request_ptr->rrn));

    if (saf_duplicate_found(&test_txn, st_cpat_check_completed, &index, BITMAP_RRN))
    {
        if (saf_read_record_index(index, VIM_FALSE, &test_txn) == VIM_ERROR_NONE)
        {
            txn = test_txn;
            txn.original_type = test_txn.type;
            txn.original_index = index;
            txn.type = tt_void;
            return VIM_ERROR_NONE;
        }
    }
    return VIM_ERROR_POS_REQUEST_INVALID;
}




VIM_ERROR_PTR ProcessVoid(VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr)
{
    VIM_UINT64 POS_Amount = asc_to_bin(request_ptr->amount_purchase, sizeof(request_ptr->amount_purchase));
    VIM_SIZE POS_Ref_len = 0;
    VIM_CHAR *Ptr = request_ptr->reference;

    // WOW style VOID 

    while (VIM_ISDIGIT(*Ptr) && POS_Ref_len++ < VIM_SIZEOF(request_ptr->reference)) Ptr++;

    VIM_ERROR_RETURN_ON_FAIL(txn_duplicate_txn_read(VIM_FALSE));
    if ((POS_Amount == txn_duplicate.amount_purchase) && (!vim_strncmp(request_ptr->reference, txn_duplicate.pos_reference, POS_Ref_len)))
    {
        txn = txn_duplicate;
        if (txn.type != tt_void)
            txn.original_type = txn_duplicate.type;
        else
            txn.original_type = txn.type;

        txn.type = tt_void;
        return VIM_ERROR_NONE;
    }
    else if(!TERM_USE_PCI_TOKENS)     // PCEFTPOS Style VOID 
    {    
        return ProcessThirdPartyVoid(request_ptr, POS_Ref_len);
    }
    return VIM_ERROR_POS_REQUEST_INVALID;
}



// RDD added 301211 for different types of Completion

VIM_ERROR_PTR ProcessCompletion( VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr )
{

	// if SAF count exceeded
#if 0
	if( saf_full( ) )
	{
	  vim_mem_copy(txn.host_rc_asc, "TP", 2);
	  return ERR_WOW_ERC;
	}
#endif
	txn.type = tt_completion;

	switch( request_ptr->original_txn_type )
	{
		case PCEFT_TXN_TYPE_CASHOUT:
			txn.original_type = tt_cashout;
			break;

		case PCEFT_TXN_TYPE_REFUND:
			txn.original_type = tt_refund;
			break;

		case PCEFT_TXN_TYPE_DEPOSIT:
			txn.original_type = tt_deposit;
			break;

		case PCEFT_TXN_TYPE_PURCHASE:
			txn.original_type = tt_sale;
			break;

		default:
			txn.original_type = tt_sale;
			break;
			//return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
	}
	// Account type can also come over with a completion
	// RDD 061112 SPIRA:6257 Map the account type here now rather than lower down
	switch( request_ptr->account_type )
	{
		case PCEFT_SAVING_ACCOUNT:
			txn.pos_account = VIM_TRUE;
			txn.account = account_savings;
			break;

		case PCEFT_CHEQUE_ACCOUNT:
			txn.pos_account = VIM_TRUE;
			txn.account = account_cheque;
			break;

		case PCEFT_CREDIT_ACCOUNT:
		case PCEFT_DEFAULT_ACCOUNT:
			txn.pos_account = VIM_TRUE;
			txn.account = account_credit;
			break;

		default:
			txn.pos_account = VIM_FALSE;
			break;
	}


	// Set the signature required and completion flags

	// RDD 090512 SPIRA:5470 Note that the above is not always true. This flag must be restored in cases where the card Y3 approves the txn
	transaction_set_status( &txn, st_offline + st_completion );
	return VIM_ERROR_NONE;
}


VIM_SIZE GetEntryMode(card_capture_state_t i_state)
{
    //VIM_SIZE iem;
    switch (i_state)
    {
    case card_capture_keyed_entry:
        return 11;
    case card_capture_icc_test:
    case card_capture_no_picc:
    case card_capture_forced_icc:
    case card_capture_picc_not_allowed:
    case card_capture_icc_retry:
    case card_capture_no_picc_reader:
    case card_capture_no_picc_error:
    case card_capture_comm_error_no_picc:
        return 51;
    case card_capture_forced_swipe:
        return 21;
    case card_capture_standard:
    case card_capture_picc_test:
    case card_capture_standard_retry:
        return 71;
    default:        // Error Condition - shouldn't get this.
        VIM_DBG_PRINTF1("Unknown i_state : %d", i_state);
        break;
    }
    return 0;
}

void SetCaptureState( void )
{
    // RDD - we already set the state.
    if (txn.card_state == card_capture_new_gift)
        return;

    // RDD 100912 P3 Rimu - need to allow contactless tap
    if(( txn.type != tt_preauth )&&( txn.type != tt_sale )&&( txn.type != tt_refund )&&( txn.type != tt_sale_cash)&&( txn.type != tt_cashout ))
	{
        vim_strcpy(txn.cnp_error, "FDT");
		txn.card_state = card_capture_no_picc;
	}
	if(( txn.account == account_savings ) || ( txn.account == account_cheque ))
	{
        vim_strcpy(txn.cnp_error, "FDE");
        txn.card_state = card_capture_no_picc;
	}

	if( terminal.training_mode )
	{
        if (!transaction_get_status(&txn, st_electronic_gift_card))
        {
            txn.card_state = card_capture_no_picc;
        }
	}
	if(	transaction_get_status( &txn, st_operator_entry ))
	{
		txn.card_state = card_capture_keyed_entry;
	}
    // RDD 101219 BGC
    if (transaction_get_status(&txn, st_giftcard_s_type))            // New flag for S-type gift cards
    {
        txn.card_state = card_capture_keyed_entry;
    }

    // RDD 290920 v718 Trello #281
    txn.sw_iem = GetEntryMode(txn.card_state);


}

// RDD 080312 - limit size of POS ref to 6 to keep POS happy
#define WOW_TXN_REF_MAX_DIGITS 10
#define PCEFT_POS_REF_MAX_DIGITS 16

VIM_UINT8 GetDigits( VIM_CHAR **Ptr )
{
	VIM_UINT8 DigitCount = 0;
	VIM_UINT8 n = 0;

	// Skip leading zeros
	//while( **Ptr == '0' ) (*Ptr)++;

	while( n++ < PCEFT_POS_REF_MAX_DIGITS )
	{
		if(( *(*Ptr+DigitCount) >= '0' ) && ( *(*Ptr+DigitCount) <= '9' ))
		{
			DigitCount+=1;
			continue;
		}
		break;
	}
	return DigitCount;
}



static VIM_ERROR_PTR SetRRNFromPOSRef(VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr, VIM_CHAR *rrn )
{
    VIM_CHAR *Ptr = request_ptr->reference;
    VIM_UINT8 DigitCount = GetDigits(&Ptr);
    VIM_CHAR *Ptr2 = rrn;
    vim_mem_set(Ptr2, '0', WOW_RRN_LEN);

    if ((DigitCount > WOW_TXN_REF_MAX_DIGITS) || (DigitCount == 0))
    {
        DigitCount = WOW_TXN_REF_MAX_DIGITS;

        // If the Pos Ref Len is less than or equal to 6 digits copy over the whole ref into the RRN
    }

	// RDD 220322  JIRA-PIRPIN-1466 : Some LIVE POS require up to max digits
    //else if (DigitCount <= WOW_POS_REF_LEN)
	if (DigitCount <= WOW_POS_REF_LEN)
	{
        Ptr2 += (12 - DigitCount);
        vim_mem_copy(Ptr2, Ptr, DigitCount);
    }
    // If the Pos Ref is greater than 7 digits then copy over the first x digits where x = total digit count - 4
    else
    {
        DigitCount -= 4;
        Ptr2 += (12 - DigitCount);
        vim_mem_copy(Ptr2, Ptr, DigitCount);
    }
    return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
void pceft_process_txn
(
  VIM_CHAR sub_code,
    VIM_PCEFTPOS_TRANSACTION_REQUEST *request_ptr,
    VIM_BOOL emv_preswipe_cont
)
{
  VIM_ERROR_PTR res;
  VIM_UINT8 pos_ref_len;

  if (!is_integrated_mode())
  {
    txn.error = ERR_TXN_NOT_SUPPORTED;
    return;
  }

  if( VIM_ERROR_NONE != (res = is_ready()))
  {
    txn.error = res;
    //VIM_DBG_ERROR(res);
    return;
  }

#if 0  // RDD 230212 - move this to below validate type and amt check as need these fields set up in case of epan mismatch
  /* Need to pull the card data from POS */
  if (( res = determine_entry_type( request_ptr )) != VIM_ERROR_NONE )
  {
    txn.error = res;
    return;
  }
#endif

  // Initialise the transaction structure before we start filling it with data for the new txn
  //txn_init();

  // RDD 100523 JIRA PIRPIN-2376 - get device code for txn 
  vim_pceftpos_get_device_code(pceftpos_handle.instance, &txn.pceftpos_device_code);

 //VIM_DBG_VAR(request_ptr->type);
  /* validate the transaction type sent by the POS  */
  if ( (res = validate_transaction_type(request_ptr->type)) != VIM_ERROR_NONE)
  {
    txn.error = res;
    return;
  }

#if 0
  // RDD 180520 Trello #142 - delay reboot if imminent
  {
      VIM_SIZE secs_to_go = 0;
      if( !pci_reboot_delay_available( &secs_to_go ))
      {
          VIM_CHAR Buffer[100];
          vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
          vim_sprintf( Buffer, "Txn declined : PCI Reboot required in %d seconds", secs_to_go );
          pceft_debug( pceftpos_handle.instance, Buffer );
          txn.error = ERR_PCI_REBOOT_PENDING;
          return;
      }
  }
#endif

  // RDD 130421 added JIRA PIRPIN-1038 void support - pre-qualify void by comparing with duplicate
  if (txn.type == tt_void)
  {
      if ((txn.error = ProcessVoid(request_ptr)) != VIM_ERROR_NONE)
      {
          return;
      }
  }

  // Validate Txn type translates 'V' or 'L' to tt_completion, but we need to remap these to original txn type
  if((txn.type == tt_completion))
  {
      if (/*!TERM_USE_PCI_TOKENS*/1)
      {
		  txn.error = CompletePA(request_ptr);
          if(( txn.error == VIM_ERROR_NONE )||( txn.error == VIM_ERROR_NOT_FOUND ))
		  {
              txn.type = tt_checkout;
              //SetRRNFromPOSRef(request_ptr, txn.rrn);
          }
		  else
		  {
			  return;
		  }
      }
      else
      {
          /*
          RRN - RDD: if completion then we should have a valid RRN from a previous txn in the request,
          so copy this over. If not then the txn ref. becomes the rrn
          */
          if ((txn.error = ProcessCompletion(request_ptr)) != VIM_ERROR_NONE)
          {
              return;
          }
          vim_mem_copy(&txn.rrn, request_ptr->rrn, VIM_SIZEOF(txn.rrn));
      }
  }
  else if( txn.type != tt_void )
  {
	  SetRRNFromPOSRef(request_ptr, txn.rrn);
  }
  txn.pos_subcode = sub_code;

  /* See if we have configuration */
  if( !terminal_get_status( ss_tid_configured ))
  {
	  txn.error = ERR_CONFIGURATION_REQUIRED;
	  return;
  }

  // Get the txn type string - needed for all display_result calls....
  get_txn_type_string_short(&txn, type, sizeof(type));

  /* validate the transaction amounts sent by the POS */
  if ( (res = validate_transaction_amounts(request_ptr)) != VIM_ERROR_NONE)
  {
    txn.error = res;
    return;
  }

  // RDD 100812 SPIRA:5872 if PinPad is full reject here (after amount has been set)
  if(( saf_full(VIM_FALSE)) && ( txn.type == tt_completion ))
  {
	  txn.error = ERR_WOW_ERC;
	  vim_mem_copy(txn.host_rc_asc, "TP", 2);
	  display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
	  return;
  }


  // RDD 270212 - don't allow training mode txns if SAFs or SAF prints pending...
  if(( saf_totals.fields.saf_count || saf_totals.fields.saf_print_count ) && ( terminal.training_mode ))
  {
	  txn.error = ERR_WOW_ERC;
	  vim_strcpy( txn.host_rc_asc, "TU" );
	  display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
	  return;
  }

  /* Need to pull the card data from POS */

  if (( res = determine_entry_type( request_ptr )) != VIM_ERROR_NONE )
  {
    txn.error = res;
    return;
  }

  // Its a BGC
  if( IsBGC() && txn.card_state != card_capture_new_gift )
  {
      #define BARCODE_POS_DATA_LEN 20
      VIM_CHAR Barcode[BARCODE_POS_DATA_LEN + 1];
      VIM_CHAR Authcode[6 + 1];
      VIM_SIZE n, Len = BARCODE_POS_DATA_LEN;
      VIM_CHAR *Ptr = request_ptr->card_data;
      VIM_SIZE m = 0;

      vim_mem_clear(Barcode, VIM_SIZEOF(Barcode));
      vim_mem_clear(Authcode, VIM_SIZEOF(Authcode));

      // 041219 DEVX reduce length from 20 according to number of leading zeros
      while((*Ptr == '0')&&( Len ))
      {
          ++Ptr;
          --Len;
      }
      // RDD 140920 Trello #238 : don't allow 20 digit barcodes at all as screws up reversal
      Len = VIM_MIN(Len, WOW_PAN_LEN_MAX);

      if(Len < WOW_GIFT_CARD_LEN_MIN)
      {
          txn.error = ERR_WOW_ERC;
          vim_strcpy(txn.host_rc_asc, "14");
          return;
      }
      // 041219 DEVX Check the validity of the incoming PAN data
      for( n = 0; n < Len; n++, Ptr++ )
      {
          if (!VIM_ISDIGIT(*Ptr))
          {
              txn.error = ERR_WOW_ERC;
              vim_strcpy(txn.host_rc_asc, "14");
              display_result( txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );
              return;
          }
          else
          {
              Barcode[m++] = *Ptr;
          }
      }

      vim_strncpy( txn.card.data.manual.pan, Barcode, Len );
      txn.card.data.manual.pan_length = Len;

      Len = 0;
      while (Len < 4) Authcode[Len++] = *++Ptr;

      // Reverse YYMM as POS writes this in reverse
      txn.card.data.manual.auth_code[0] = Authcode[2];
      txn.card.data.manual.auth_code[1] = Authcode[3];
      txn.card.data.manual.auth_code[2] = Authcode[0];
      txn.card.data.manual.auth_code[3] = Authcode[1];
      txn.card.data.manual.auth_code[4] = '\0';
      pceft_debug_test(pceftpos_handle.instance, "From Barcode Gift Card:");
      pceft_debug_test(pceftpos_handle.instance, txn.card.data.manual.pan);
      pceft_debug_test(pceftpos_handle.instance, txn.card.data.manual.auth_code);

      txn.card.type = card_manual;

      txn.card.data.manual.expiry_mm[0] = '1';
      txn.card.data.manual.expiry_mm[1] = '2';
      txn.card.data.manual.expiry_yy[0] = '3';
      txn.card.data.manual.expiry_yy[1] = '0';

  }


#if 0  // RDD 211112 this was removed a while back for some reason I really should have logged - but probably due to the error
       // returned if no account supplied with EPAN
  /* Store account type if sent down (RDD moved from lower down so the account type can be used to invalidate some amounts below */
  if( transaction_get_status( &txn, st_pos_sent_pci_pan ) )
  {
	  if( !check_account_type((account_t)request_ptr->account_type) )
	  {
		  txn.error = ERR_INVALID_ACCOUNT;
		  return;
	  }
	  // DBG_VAR(txn.account)
  }

#else	// RDD 211112 v305 SPIRA:6294 We need the account type from the POS for Deposits when the POS supplied the EPAN, may also supply the account
  if( transaction_get_status( &txn, st_pos_sent_pci_pan ) )
  {
	  check_account_type((account_t)request_ptr->account_type);
	  DBG_VAR(txn.account);
  }
#endif


  /* Now save the rest of the details */
  /* &txn.pos_reference */
  /* Txn Reference Number   */
  /* cut the right space of pos_reference for printing */
  // RDD 091220 JIRA PIRPIN-955 Update POS ref to relect what is actually sent from POS (8) was previously 6 to support R8
  // RDD 220322  JIRA-PIRPIN-1466 : Some LIVE POS require up to max digits
  pos_ref_len = ( IS_R10_POS ) ? R10_POS_REF_LEN : PCEFT_POS_REF_LEN;
  vim_mem_copy( txn.pos_reference, request_ptr->reference, pos_ref_len );

  // RDD 220322  JIRA-PIRPIN-1466 : Some LIVE POS require up to max digits
  txn.pos_reference[pos_ref_len] = '\0';


  /* Store the RRN - if not already stored ( eg for completion or void etc. ) */
  if( !txn.roc )
    store_roc( &txn, (VIM_CHAR *)request_ptr->reference, VIM_SIZEOF(request_ptr->reference));

  /* RDD 111011: store the Auth Code if present */
  if (!TERM_USE_PCI_TOKENS && txn.type == tt_checkout && !vim_strncmp(request_ptr->auth_number, "000000", 6))
  {
      pceft_debug(pceftpos_handle.instance, "Completion Using Saved AuthNumber");
  }
  else
  {
      vim_mem_copy(txn.auth_no, request_ptr->auth_number, AUTH_CODE_LEN);
      txn.auth_no[AUTH_CODE_LEN] = '\0';
  }
  /* Check we have a CCV */
  //set_ccv(request_ptr);

  // RDD 091111 - process incoming Purchase Analysis Data (used for fuel cards)
  if(( txn.error = process_pad( request_ptr->purchase_analysis_data )) != VIM_ERROR_NONE )
  {
      display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
      return;
  }

#if 0	// RDD 170612 - move this to after card capture as we don't want normal cards being stuffed if the PAD data is invalid foir some reason
  // Check the validity of the PAD
  if(( txn.error = ValidatePADData( &txn.u_PadData.PadData )) != VIM_ERROR_NONE )
  {
	display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
    return;
  }
#endif

  // Set up the initial Card Capture mode according to txn type
  SetCaptureState( );


#ifndef _PHASE4
  txn_save();
#endif

  /* Now process the transaction */
  txn_process();
}

///////////////////////////////////// CALLBACK: PROCESS TOTALS ///////////////////////////////////////////
// This callback handles Settlement and Pre-Settlement Processing
////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_process_totals(VIM_CHAR type)
{
  VIM_BOOL training_mode;

  // Clear any outstanding error before we start
  settle.error = VIM_ERROR_NONE;

  if (!is_integrated_mode())
  {
    settle.error = ERR_TXN_NOT_SUPPORTED;
    return settle.error;
  }

 //VIM_DBG_VAR(terminal.configured);
  /* if not configured dont proceed */
  if( !terminal_get_status( ss_tid_configured ))
  {
    settle.error = ERR_CONFIGURATION_REQUIRED;
    return settle.error;
  }

  /* Turn off training Mode by default */
  training_mode = VIM_FALSE;

  switch (type)
  {
    case PCEFT_TOTALS_SUB_SETTLE_TRAINING:
      training_mode = VIM_TRUE;
	case PCEFT_TOTALS_SUB_SETTLE:
        settle.type = tt_settle;
		break;

    case PCEFT_TOTALS_SUB_SAF_TOTALS:
        settle.type = tt_settle_safs;
	  break;

	case PCEFT_TOTALS_SUB_PRE_SETTLE_TRAINING:
      training_mode = VIM_TRUE;
    case PCEFT_TOTALS_SUB_PRE_SETTLE:
      settle.type = tt_presettle;
      break;

      // RDD 270821 Added
    case PCEFT_TOTALS_SUB_SHIFTTOTALS_NO_RESET:
        shift_totals_process(VIM_FALSE);
        return VIM_ERROR_NONE;

	case PCEFT_TOTALS_SUB_SHIFTTOTALS_TRAINING:
		training_mode = VIM_TRUE;
	case PCEFT_TOTALS_SUB_SHIFTTOTALS:
        shift_totals_process(VIM_TRUE);
        return VIM_ERROR_NONE;


    default:
      settle.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
      return VIM_ERROR_NONE;
   }

   /* Set Training mode  */
   if( settle.error == VIM_ERROR_NONE )
   {
	   VIM_ERROR_RETURN_ON_FAIL( pceftpos_set_training_mode(training_mode) );
   }
   // RDD 120312: SPIRA
   txn_init();
   txn.type = tt_settle;


   /* save the operation initiator */
   set_operation_initiator(it_pceftpos);

     /* Do the settlement */
   settle.error = settle_terminal();

   /* update idle timer */
   pceft_pos_display_close();

   terminal.idle_time_count = 0;
	terminal.screen_id = 0;
   terminal.allow_ads = VIM_TRUE;  //JQ 09012013
   vim_rtc_get_uptime(&terminal.idle_start_time);
   return settle.error;
}

///////////////////////////////////// CALLBACK: PRE-SWIPE STATUS ///////////////////////////////////////////
// This callback picks up and responds with Pre-Swipe Status data
////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_get_preswipe_status
(
  /*@out@*/VIM_PCEFTPOS_PRE_SWIPE_STATUS *status_ptr
)
{
  VIM_ERROR_PTR result = ERR_POS_GET_STATUS_OK;
  error_t error_data;

  if (!is_integrated_mode())
  {
    result = ERR_TXN_NOT_SUPPORTED;
    /* Set Error Code */
    vim_mem_clear(&error_data, VIM_SIZEOF(error_data));
    terminal_error_lookup(result, &error_data, VIM_FALSE);
    vim_mem_copy(status_ptr->response_code, error_data.ascii_code , 2);
    vim_mem_copy(status_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(status_ptr->response_text),vim_strlen(error_data.pos_text)));
    return VIM_ERROR_NONE;
  }


  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  /* Fill buffer with spaces */
  vim_mem_set( status_ptr, ' ', VIM_SIZEOF(VIM_PCEFTPOS_PRE_SWIPE_STATUS ));

  /* Set the Response code */
  result = (txn.preswipe_status == completed_preswipe) ? ERR_PRESWIPE_COMPLETE : ERR_PRESWIPE_INCOMPLETE;

  /* Set Error Code */

  vim_mem_clear(&error_data, VIM_SIZEOF(error_data));
  	DBG_POINT;

  terminal_error_lookup(result, &error_data, VIM_TRUE);
  vim_mem_copy(status_ptr->response_code, error_data.ascii_code , PCEFT_STATUS_RC_LEN );
  vim_mem_copy(status_ptr->response_text, error_data.pos_text, VIM_MIN( PCEFT_RESP_TEXT_LEN, vim_strlen(error_data.pos_text)));

  /* terminal id */
  vim_mem_copy( status_ptr->catid, terminal.terminal_id, PCEFT_CATID_LEN );

  vim_mem_set(status_ptr->cash_amount, '0', VIM_SIZEOF(status_ptr->cash_amount));
  vim_mem_set(status_ptr->purchase_amount, '0', VIM_SIZEOF(status_ptr->purchase_amount));
  vim_mem_set(status_ptr->other_amount, '0', VIM_SIZEOF(status_ptr->other_amount));

  vim_mem_set( status_ptr->epan, '?', PCEFT_STATUS_TRACK2_LEN );

  if( result == ERR_PRESWIPE_COMPLETE )
  {
    /* ePan */
    vim_mem_copy( status_ptr->epan, txn.card.pci_data.ePan, vim_strlen(txn.card.pci_data.ePan) );

    /* Map account type : as per PCEFT_SPEC 2.17 p46 */
	if( txn.account == account_savings ) status_ptr->account_selected = PCEFT_ACC_SAVINGS;
	else if(  txn.account == account_cheque ) status_ptr->account_selected = PCEFT_ACC_CHEQUE;
	else status_ptr->account_selected = PCEFT_ACC_CREDIT;

	bin_to_asc( txn.amount_cash, status_ptr->cash_amount, VIM_SIZEOF(status_ptr->cash_amount) );

	if( txn.amount_cash )
	{
		bin_to_asc( txn.amount_cash, status_ptr->cash_amount, VIM_SIZEOF(status_ptr->other_amount) );
	}
	if( txn.amount_purchase )
	{
		bin_to_asc( txn.amount_purchase, status_ptr->purchase_amount, VIM_SIZEOF(status_ptr->other_amount) );
	}
  }
  return VIM_ERROR_NONE;
}

///////////////////////////////////// CALLBACK: GET STATUS ///////////////////////////////////////////
// This callback picks up and responds with Generic Status data
////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_get_status
(
  /*@out@*/VIM_PCEFTPOS_STATUS *status_ptr
)
{
  VIM_SIZE flash_size;
  VIM_RTC_TIME manufacture_time;
  VIM_DES_BLOCK ppid;
  VIM_ERROR_PTR result = ERR_POS_GET_STATUS_OK;
  error_t error_data;
  VIM_CHAR serial_number[17];
  VIM_CHAR terminal_type[5];

  // Status kills any outstanding preswipe to return terminal to IDLE
#if 0
  if(txn.preswipe_status == incomplete_preswipe)
  {
	//terminal.abort_transaction = TRUE;
  }
#endif
  if (!is_integrated_mode())
  {
    result = ERR_TXN_NOT_SUPPORTED;
    /* Set Error Code */
    vim_mem_clear(&error_data, VIM_SIZEOF(error_data));
    terminal_error_lookup(result, &error_data, VIM_FALSE);
    vim_mem_copy(status_ptr->response_code, error_data.ascii_code , 2);
    vim_mem_copy(status_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(status_ptr->response_text),vim_strlen(error_data.pos_text)));
    return VIM_ERROR_NONE;
  }


  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  /* init buf */
  vim_mem_set(status_ptr, ' ', VIM_SIZEOF(*status_ptr));


  if( !terminal_get_status( ss_tid_configured ))
  {
	  result = ERR_CONFIGURATION_REQUIRED;
  }
  // RDD 170722 JIRA PIRPIN-1747
#if 0
  else if( !terminal_get_status( ss_tms_configured ) )
  {
	  result = ERR_NOT_TMS_ON;
  }
#endif
  else if (terminal.logon_state == logged_on) /* Terminal Logged on, all good */
  {
	  result = ERR_POS_GET_STATUS_OK;
  }
  else
  {
	  result = ERR_NOT_LOGGED_ON;
  }
  if ( txn.type == tt_func ) result = ERR_PP_BUSY;		//BD

  // RDD 050912 P3: Status is used to abort Query Card txn
  if( txn.type == tt_query_card )
  {
	  DBG_MESSAGE( "<<<<<<<<< PROCESSING GET STATUS >>>>>>>>>>>" );
	  //VIM_ERROR_RETURN_ON_FAIL( pceft_pos_command_check( 20000 ));
	  terminal.abort_transaction = VIM_TRUE;
	  result = ERR_POS_GET_STATUS_APPROVED;

	  VIM_DBG_SEND_TARGETED_UDP_LOGS("<<<<<<<<< PROCESSING GET STATUS >>>>>>>>>>>");
	 // VIM_DBG_MESSAGE("Cancel VAS");
	  //vim_vas_cancel();
	  //#ifndef _WIN32
	  //pthread_join(vas_thread, NULL);
	  //#endif
      if(!TERM_NEW_VAS)
      {
	    //vim_vas_close();
      }
	  DBG_POINT;
	  txn.type = tt_none;
	  terminal.allow_ads = VIM_FALSE;	// Don't allow ads until we processed the incoming txn
	  terminal.allow_idle = VIM_FALSE;

	  // RDD 030112 - check for interrupting M command and abort if necessary without sending response
	  //VIM_ERROR_RETURN_ON_FAIL( pceft_pos_command_check( __50_MS__ );

	  // RDD 201212 SPIRA:6459 No response required during query card so just abort with an error. This is what is needed !
	  // Must use this error code to bypass error reporting at termination of preceding J8
	  // RDD - V306a - This one is reported to work with the POS !!
	  //return VIM_ERROR_NOT_FOUND;
	  // RDD - V306b - original (normal) approach
	  //return VIM_ERROR_NONE;
  }

 //VIM_DBG_VAR(terminal.logon_state);

  /* Set Error Code */

  vim_mem_clear(&error_data, VIM_SIZEOF(error_data));


  terminal_error_lookup(result, &error_data, VIM_TRUE);

  vim_mem_copy(status_ptr->response_code, error_data.ascii_code , 2);
  vim_mem_copy(status_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(status_ptr->response_text),vim_strlen(error_data.pos_text)));

 //
  // RDD 010212 : added PPID
  VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid( tcu_handle.instance_ptr, &ppid ));

  bcd_to_asc( ppid.bytes, status_ptr->ppid, VIM_SIZEOF(ppid.bytes)*2 );


 //
  /* merchant number */
  vim_mem_set(status_ptr->merchant_number, '0', VIM_SIZEOF(status_ptr->merchant_number));

  /* ppid */
//  if( vim_tcu_get_ppid(tcu_handle.instance_ptr, &ppid) == VIM_ERROR_NONE )
//    bcd_to_asc(ppid.bytes, status_ptr->ppid, VIM_SIZEOF(status_ptr->ppid));
 //

  /* terminal id */
  if(terminal.terminal_id[0] != 0)
    vim_mem_copy(status_ptr->catid, terminal.terminal_id, VIM_SIZEOF(status_ptr->catid));

  /* merchant id */
  if(terminal.merchant_id[0] != 0)
    vim_mem_copy(status_ptr->caic, terminal.merchant_id, VIM_SIZEOF(status_ptr->caic));

  /* software version */
  bin_to_asc(terminal.file_version[APP_FILE_IDX], status_ptr->version, 6 );

  /* nii */
  bin_to_asc(POS_BANK_NII, status_ptr->nii, VIM_SIZEOF(status_ptr->nii));

  /* aiic */
  vim_mem_copy(status_ptr->aiic, POS_BANK_AIIC, vim_strlen(POS_BANK_AIIC));

  /* timeout */
  bin_to_asc(0, status_ptr->txn_timeout, VIM_SIZEOF(status_ptr->txn_timeout));

  /* bank code */
  status_ptr->bank_code = PCEFT_BANK_CODE;

  /* description */
  vim_mem_copy(status_ptr->bank_description, POS_BANK_DESCRIPTION, VIM_SIZEOF(status_ptr->bank_description));

  // KVC
  vim_mem_set( status_ptr->kvc, '0', 6 );

  /* get the number of SAF entries pending */
  bin_to_asc( saf_totals.fields.saf_count, status_ptr->saf_count, VIM_SIZEOF(status_ptr->saf_count));

  /* 1. = Leased, 2.= Dial-up */
  status_ptr->networkType = '2';

   //VIM_DBG_VAR(serialNo);
  //VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_physical_terminal_id( (VIM_TEXT*)ptid, VIM_SIZEOF(ptid) ) );

  /* serial number */
  vim_mem_clear(serial_number, VIM_SIZEOF(serial_number));
  vim_terminal_get_raw_serial_number( (VIM_TEXT*)serial_number, VIM_SIZEOF(serial_number) );
  //vim_terminal_get_serial_number(serial_number,VIM_SIZEOF(serial_number));
  vim_mem_copy(status_ptr->serial, serial_number, VIM_MIN(VIM_SIZEOF(status_ptr->serial),vim_strlen(serial_number)));

  /* merchant name */
  vim_mem_copy(status_ptr->retailer_name, terminal.configuration.merchant_name, vim_strlen(terminal.configuration.merchant_name));
  vim_mem_copy(&status_ptr->retailer_name[20], terminal.configuration.merchant_address, vim_strlen(terminal.configuration.merchant_address));

  /* options */
  vim_mem_set(&status_ptr->options, '0', VIM_SIZEOF(status_ptr->options));
  /* set the following flags correctly (RDD- modified for WOW in absence of current LFD info - may need to change...) */
  status_ptr->options.allow_tipping = TERM_ALLOW_TIP ? '1':'0';
  status_ptr->options.allow_pre_auth = TERM_ALLOW_PREAUTH ? '1':'0';
  status_ptr->options.allow_completion = TERM_ALLOW_PREAUTH ? '1' : '0';   // assume if preauth OK then completion is too
  status_ptr->options.allow_cashout = '1';
  status_ptr->options.allow_refund = '1';
  status_ptr->options.allow_balance_enquiries = '1';
  status_ptr->options.allow_deposits = '1';
  status_ptr->options.allow_moto_transactions = TERM_ALLOW_MOTO_ECOM ? '1':'0';
  status_ptr->options.allow_efb = '1';

  status_ptr->options.is_training_mode = terminal.training_mode?'1':'0';

  /* limit */
  bin_to_asc(terminal.configuration.efb_credit_limit*100, status_ptr->stand_in_credit_limit, VIM_SIZEOF(status_ptr->stand_in_credit_limit));
  bin_to_asc(terminal.configuration.efb_debit_limit*100, status_ptr->stand_in_debit_limit, VIM_SIZEOF(status_ptr->stand_in_debit_limit));
  bin_to_asc(terminal.configuration.efb_max_txn_count, status_ptr->max_standin_txns, VIM_SIZEOF(status_ptr->max_standin_txns));

  /* 0. = Single DES, 1. = Triple DES */
  status_ptr->key_handling_scheme = '1';

  /* todo: where cash limit can be found ? */
  bin_to_asc( WOW_CASHOUT_LIMIT, status_ptr->cash_limit, VIM_SIZEOF(status_ptr->cash_limit));
  bin_to_asc( WOW_REFUND_LIMIT, status_ptr->refund_limit, VIM_SIZEOF(status_ptr->refund_limit));

  // CPAT File Version
  bin_to_asc( terminal.file_version[CPAT_FILE_IDX], status_ptr->cpat_version, 6 );

  /* 0 = cable */
  status_ptr->rs232_type = '0';

  /* card misread count */
  bin_to_asc(terminal.stats.card_misreads, status_ptr->card_misread_count, VIM_SIZEOF(status_ptr->card_misread_count));
  /* memory status */
  vim_terminal_get_flash_memory_size(&flash_size);
  bin_to_asc(flash_size/(1024*1024), status_ptr->mem_pages_total, VIM_SIZEOF(status_ptr->mem_pages_total));
  bin_to_asc(0, status_ptr->mem_pages_free, VIM_SIZEOF(status_ptr->mem_pages_free));

  /* terminal type */
  vim_mem_clear(terminal_type, VIM_SIZEOF(terminal_type));
  vim_terminal_get_part_number(terminal_type,VIM_SIZEOF(terminal_type));
  vim_mem_copy(status_ptr->terminal_type, terminal_type, VIM_MIN(VIM_SIZEOF(status_ptr->terminal_type),vim_strlen(terminal_type)));

  /* application number */
  bin_to_asc(1, status_ptr->num_apps_in_terminal, VIM_SIZEOF(status_ptr->num_apps_in_terminal));

  /* display lines */
  bin_to_asc(5, status_ptr->display_lines, VIM_SIZEOF(status_ptr->display_lines));
  /* convert date to ascii DDMMYY format */
  vim_terminal_get_manufacture_date(&manufacture_time);
  vim_snprintf_unterminated(status_ptr->device_inception_date,VIM_SIZEOF(status_ptr->device_inception_date),"%02d%02d%02d",manufacture_time.day,manufacture_time.month,manufacture_time.year%100);

  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);
  DBG_POINT;

#ifndef _WIN32
  //if( !terminal_get_status(ss_incomplete))	// RDD 161215 SPIRA:8855
  {
      // RDD 291019 JIRA WWBP-755 : Branding Broken in P400 - if no branding display IDLE status
      VIM_DBG_MESSAGE("DDDDDDDDDDDDDDDDD Branding DDDDDDDDDDDDDDDDDDDDDD");
      display_screen_cust_pcx(DISP_IMAGES, "Image", terminal.configuration.BrandFileName, VIM_PCEFTPOS_KEY_NONE);
  }
#endif
 return VIM_ERROR_NONE;
}
///////////////////////////////////// CALLBACK: GET TOTALS RESPONSE ///////////////////////////////////////////
// This callback builds the response data structure for a settlement request
////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
VIM_ERROR_PTR vim_pceftpos_callback_get_totals_response(VIM_CHAR *resp, VIM_SIZE * resp_len)
{
  error_t error_data;
  VIM_SIZE  pos;
  VIM_UINT8   card_count, i;
  VIM_PCEFTPOS_TOTALS_RESPONSE *totals_response;
  VIM_PCEFTPOS_CARD_TOTALS *totals;


  card_count = 0;
  pos = 0;
 //VIM_
  vim_mem_set(resp, '0', VIM_SIZEOF(VIM_PCEFTPOS_TOTALS_RESPONSE));
  totals_response = (VIM_PCEFTPOS_TOTALS_RESPONSE *)&resp[pos];
  /* Set Error Code */
  error_code_lookup(settle.error, settle.host_rc_asc, &error_data);
  vim_mem_copy(totals_response->response_code, error_data.ascii_code, 2);
  vim_mem_set(totals_response->response_text, ' ', VIM_SIZEOF(error_data.pos_text));
  vim_mem_copy(totals_response->response_text, error_data.pos_text, vim_strlen(error_data.pos_text));
  vim_mem_set(totals_response->merchant_num, '0', VIM_SIZEOF(totals_response->merchant_num));
  /* Set merchant number */
  vim_mem_copy(totals_response->merchant_num, "00", 2);
  if (settle.error != VIM_ERROR_NONE)
  {
    /* Set totals to zero */
    vim_mem_copy(totals_response->card_totals_count, "00", 2);
    totals_response->totals_amount_present = '0';
    pos += VIM_SIZEOF(VIM_PCEFTPOS_TOTALS_RESPONSE);
    *resp_len = pos;
    return VIM_ERROR_NONE;
  }

  /* Total amount is always present */
  totals_response->totals_amount_present = '1';

  pos += VIM_SIZEOF(VIM_PCEFTPOS_TOTALS_RESPONSE);
 //VIM_DBG_VAR(pos);

  if (tt_last_settle == settle.type)
  {
    for (i = 0; i < MAX_NET_COUNT; i++)
    {
      totals = (VIM_PCEFTPOS_CARD_TOTALS *)&resp[pos];
      vim_mem_set(totals, '0', VIM_SIZEOF(VIM_PCEFTPOS_CARD_TOTALS));
      vim_mem_set(totals->card_name, ' ', VIM_SIZEOF(totals->card_name));

      /* set card name */
      switch (i)
      {
        case 0:
          vim_mem_copy(totals->card_name, "CREDIT TOTALS", vim_strlen("CREDIT TOTALS"));
          break;

        case 1:
          vim_mem_copy(totals->card_name, "DEBIT TOTALS", vim_strlen("DEBIT TOTALS"));
          break;

        case 2:
          vim_mem_copy(totals->card_name, "AMEX TOTALS", vim_strlen("AMEX TOTALS"));
          break;

        case 3:
          vim_mem_copy(totals->card_name, "JCB TOTALS", vim_strlen("JCB TOTALS"));
          break;

        case 4:
          vim_mem_copy(totals->card_name, "DINERS TOTALS", vim_strlen("DINERS TOTALS"));
          break;
      }

      /* convert amount */
      //bin_to_asc(settle.totals_by_card[i].card_amount, totals->amount_total, 9);

      /* convert count */
      //bin_to_asc(settle.totals_by_card[i].card_count, totals->count_total, 3);

      /* set amount's sign */
      //totals->total_sign = settle.totals_by_card[i].amount_sign ? '+' : '-';
      pos += VIM_SIZEOF(VIM_PCEFTPOS_CARD_TOTALS);
    }

    totals = (VIM_PCEFTPOS_CARD_TOTALS *)&resp[pos];
    vim_mem_set(totals, '0', VIM_SIZEOF(VIM_PCEFTPOS_CARD_TOTALS));
    vim_mem_set(totals->card_name, ' ', VIM_SIZEOF(totals->card_name));
    vim_mem_copy(totals->card_name, "TOTALS", vim_strlen("TOTALS"));

    /* count total amounts and counts */
    for (i = 1; i < MAX_NET_COUNT; i++)
    {
      //card_amount += settle.totals_by_card[i].card_amount;
      //card_count += settle.totals_by_card[i].card_count;
    }

    //card_count += settle.totals_by_card[0].card_count;
  }
  /* Set card count */
  bin_to_asc(card_count, totals_response->card_totals_count, 2);

  /* Set Settle Date */
    set_settlement_date_to_ascii(settle.settle_date, &resp[pos]);

  pos += 6;
  *resp_len = pos;

  return VIM_ERROR_NONE;
}


#else

/*

typedef struct custom_totals_t
{
	VIM_PCEFTPOS_TOTALS_RESPONSE TotalsHeader;
	VIM_CHAR TotalsLength[4];
	wow_acquirer_totals_t AcquirerTotals;
	VIM_CHAR count_cashout[3];
    VIM_CHAR amount_cashout[9];
    VIM_CHAR count_deposit[3];
    VIM_CHAR amount_deposit[9];
    VIM_CHAR charge_card_number[3];
	wow_acquirer_totals_t SchemeTotals[2];
	VIM_CHAR SettleDate[6];
} custom_totals_t;

*/


// RDD 300312 rewritten for WOW SPIRA 5175 Settlement Data for POS Response "p"
VIM_ERROR_PTR vim_pceftpos_callback_get_totals_response(VIM_CHAR *resp, VIM_SIZE * resp_len)
{
  //VIM_UINT8 n;
  error_t error_data;
  custom_totals_t *totals_response;
  VIM_UINT16 Count1,Count2;
  VIM_INT64 Amount1,Amount2;

 //VIM_

  vim_mem_set(resp, ' ', VIM_SIZEOF(custom_totals_t));
  totals_response = (custom_totals_t *)&resp[0];

  *resp_len = VIM_SIZEOF(custom_totals_t);

  /* Set Error Code */
  error_code_lookup(settle.error, settle.host_rc_asc, &error_data);
  vim_mem_copy(totals_response->TotalsHeader.response_code, error_data.ascii_code, 2);
  vim_mem_set(totals_response->TotalsHeader.response_text, ' ', VIM_SIZEOF(error_data.pos_text));
  vim_mem_copy(totals_response->TotalsHeader.response_text, error_data.pos_text, vim_strlen(error_data.pos_text));
  vim_mem_set(totals_response->TotalsHeader.merchant_num, '0', VIM_SIZEOF(totals_response->TotalsHeader.merchant_num));

  /* Set Settle Date */
  set_settlement_date_to_ascii( settle.settle_date, totals_response->SettleDate );
  /* Set merchant number */
  vim_mem_copy(totals_response->TotalsHeader.merchant_num, "00", 2);
  vim_mem_copy(totals_response->TotalsHeader.card_totals_count, "00", 2);

  //totals_response->totals_amount_present = '1';
  totals_response->TotalsHeader.totals_amount_present = '2';		// RDD 300312 : WOW uses "custom" totals see PCEFT spec
  vim_sprintf( totals_response->TotalsLength, "%4.4d", VIM_SIZEOF( custom_totals_t ) - (VIM_SIZEOF(VIM_PCEFTPOS_TOTALS_RESPONSE)+4+6) ); // RDD: Totals only: Don't include size or date

  vim_mem_copy( totals_response->AcquirerTotals.acquirer_name, "ACQUIRER", 8 );
  vim_mem_copy( totals_response->AcquirerTotals.totals_bin, "00", 2 );
  vim_sprintf( totals_response->AcquirerTotals.count_refund, "%3.3d", settle.Credits );
  vim_sprintf( totals_response->AcquirerTotals.amount_refund, "%9.9d", settle.CreditTotalAmount );
  vim_sprintf( totals_response->AcquirerTotals.count_purchase, "%3.3d", settle.Debits );
  vim_sprintf( totals_response->AcquirerTotals.amount_purchase, "%9.9d", settle.DebitTotalAmount );
  vim_sprintf( totals_response->AcquirerTotals.amount_total, "%9.9d", settle.DebitTotalAmount );
  totals_response->AcquirerTotals.total_sign = settle.NegativeTotal ? ' ' : '-';	// RDD: Negative is considered DEBITS more than CREDITS !

  Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositCount, 10);
  Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DepositAmount, 12);

  vim_sprintf( totals_response->count_cashout, "%3.3d", settle.CashOuts );
  vim_sprintf( totals_response->amount_cashout, "%9.9d", settle.CashOutTotalAmount );
  vim_sprintf( totals_response->count_deposit, "%3.3d", Count1 );
  vim_sprintf( totals_response->amount_deposit, "%9.9d", Amount1 );

  vim_sprintf( totals_response->charge_card_number, "%3.3d", 2 );	// RDD - Number of charge card totals following...

  // Ext Scheme Totals ( note MC and VISA and included in gen acquirer totals above

  // Amex
  vim_mem_copy( totals_response->SchemeTotals[0].acquirer_name, "AMEX CARD", 9 );
  vim_mem_copy( totals_response->SchemeTotals[0].totals_bin, "05", 2 );

	Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexRefundCount, 10);
	Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexRefundAmount, 12);
	Count2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexPurchaseCount, 10);
	Amount2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.AmexPurchaseAmount, 12);

  vim_sprintf( totals_response->SchemeTotals[0].count_refund, "%3.3d", Count1 );
  vim_sprintf( totals_response->SchemeTotals[0].amount_refund, "%9.9d", Amount1 );
  vim_sprintf( totals_response->SchemeTotals[0].count_purchase, "%3.3d",  Count2 );
  vim_sprintf( totals_response->SchemeTotals[0].amount_purchase, "%9.9d", Amount2 );

  if( Amount2 >= Amount1 )
  {
	  // Net zero or puchase total is more than refund total
	  vim_sprintf( totals_response->SchemeTotals[0].amount_total, "%9.9d", Amount2-Amount1 );
	  totals_response->SchemeTotals[0].total_sign = ' ';
  }
  else
  {
	  // Refund total is more than purchase total
	  vim_sprintf( totals_response->SchemeTotals[0].amount_total, "%9.9d", Amount1-Amount2 );
	  totals_response->SchemeTotals[0].total_sign = '-';
  }

  // Diners
  vim_mem_copy( totals_response->SchemeTotals[1].acquirer_name, "DINERS", 6 );
  vim_mem_copy( totals_response->SchemeTotals[1].totals_bin, "06", 2 );

  	Count1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersRefundCount, 10);
	Amount1 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersRefundAmount, 12);
	Count2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersPurchaseCount, 10);
	Amount2 = bcd_to_bin(settle.DepositAndSchemeTotals.fields.DinersPurchaseAmount, 12);


  vim_sprintf( totals_response->SchemeTotals[1].count_refund, "%3.3d", Count1 );
  vim_sprintf( totals_response->SchemeTotals[1].amount_refund, "%9.9d", Amount1 );
  vim_sprintf( totals_response->SchemeTotals[1].count_purchase, "%3.3d", Count2 );
  vim_sprintf( totals_response->SchemeTotals[1].amount_purchase, "%9.9d", Amount2 );

  if( Amount2 >= Amount1 )
  {
	  // Net zero or puchase total is more than refund total
	  vim_sprintf( totals_response->SchemeTotals[1].amount_total, "%9.9d", Amount2-Amount1 );
	  totals_response->SchemeTotals[1].total_sign = ' ';
  }
  else
  {
	  // Refund total is more than purchase total
	  vim_sprintf( totals_response->SchemeTotals[1].amount_total, "%9.9d", Amount1-Amount2 );
	  totals_response->SchemeTotals[1].total_sign = '-';
  }

  return VIM_ERROR_NONE;
}
#endif

/*
 RDD 030323 JIRA PIRPIN-2205 SLYP :ensure total chars same as PAN ( was PAN len - 1 ) and PAN masked with '0' + includes '='  
From Linkly Terminal Spec Note 1 Card Data is in the following format:
<Card Data>, ‘=’, <Expiry Date>, <pad with end sentinels (?)>
e.g. 4567890123456789012=9912????????????
If the card data is for a financial card, then all digits of the PAN except the first six and last four digits must be
masked with zeros.
If the card data is for a financial card, then all digits following the four-digit expiry date (after the equals sign) must
be masked with ‘?’
*/



/*----------------------------------------------------------------------------*/
/* Masks out PAN from Track 2 for Bank cards only */
void mask_out_track_details_txn
(
    VIM_CHAR *des_track_2,
    VIM_UINT8 des_track_2_len,
    card_info_t const *card,
    VIM_UINT8 bin,
    VIM_BOOL slave_masking,
	VIM_BOOL LinklyMasking
)
{
  VIM_UINT8 pan_len;
  VIM_BOOL  mask_pan;
  VIM_UINT8 *track_2 = VIM_NULL;
  VIM_UINT8 track_2_length = 0;
  VIM_BOOL IsT2 = VIM_FALSE;
  // RDD 030323 JIRA PIRPIN-2205 SLYP :ensure total chars same as PAN ( was PAN len - 1 ) and PAN masked with '0' + includes '='
  VIM_CHAR mask_char = LinklyMasking ? '0' : '.';

  /* Only blank out bank cards */
 //VIM_DBG_VAR(bin);

  switch(bin)
  {
    case PCEFT_FUEL_CARD:
      mask_pan = VIM_FALSE;
      break;

    default:
      mask_pan = VIM_TRUE;
      break;
  }

  // RDD 190320 allow gift cards to be sent back in clear in slave mode - just in debug mode for now
#ifdef _DEBUG
  if( gift_card(&txn) && slave_masking )
  {
      mask_pan = VIM_FALSE;
  }
#endif

  /* Determine track2 start and length */
  if( card->type == card_mag )
  {
    track_2 = (VIM_UINT8*)&card->data.mag.track2;
    /* start/end sentences and lrc */
    track_2_length = card->data.mag.track2_length;
  }
  else if( card->type == card_chip )
  {
    track_2 = (VIM_UINT8*)card->data.chip.track2;
    track_2_length = card->data.chip.track2_length;
  }
  else if( card->type == card_manual )
  {
    /* all other use mag */
    track_2 = (VIM_UINT8*)&card->data.manual.pan;
	track_2_length = card->data.manual.pan_length;
  }

  // RDD 060122 JIRA PIRPIN-1378 : Need CTLS masked PAN for IGA journel as they use for settlement apparently. 
  else if (card->type == card_picc)
  {
	  /* all other use picc */
	  track_2 = (VIM_UINT8*)&card->data.picc.track2;
	  track_2_length = card->data.picc.track2_length;
  }

  if( !track_2_length )
    return;

  for (pan_len = 0; pan_len < track_2_length; pan_len++)
  {
	  if (track_2[pan_len] == '=')
	  {
		  IsT2 = VIM_TRUE;
		  break;
	  }
  }

  // RDD 180320 - PcEFTPOS slave mode requires masking using zeros and all bytes are masked
  if(( slave_masking )&&( mask_pan ))
  {
      VIM_CHAR *Ptr;
      VIM_SIZE n, MaskedBytes = VIM_MIN( 9, pan_len - (6 + 4));    // MAX_PAN_LEN (19) - (6+4) = 9

      vim_mem_copy(des_track_2, track_2, pan_len);
      for (n = 0, Ptr = des_track_2 + 6; n < MaskedBytes; n++, Ptr++)
      {
          *Ptr = mask_char;
      }
	  if (!LinklyMasking)
		  return;
  }


  // RDD 030323 JIRA PIRPIN-2205 SLYP :ensure total chars same as PAN ( was PAN len - 1 ) and PAN masked with '0' + includes '='
  if (mask_pan)
  {
    if (track_2_length < VIM_SIZEOF(des_track_2))
    {
      vim_mem_set( des_track_2+track_2_length, '?', track_2_length-track_2_length);
    }
#if 0
    mask_track2((VIM_UINT8*)des_track_2, des_track_2_len, track_2, track_2_length, pan_len, terminal.issuers[txn.issuer_index].customer_pan);
#else
	if (LinklyMasking && IsT2)
	{
		VIM_CHAR* Ptr = des_track_2 + pan_len;
		VIM_UINT8* Ptr2 = track_2 + pan_len;

		vim_mem_copy(Ptr, Ptr2, 5);
		return;
	}
	else
	{
		vim_mem_copy(des_track_2, track_2, 6);
	}
    vim_mem_set(&des_track_2[6], '.', 3);
    vim_mem_copy(&des_track_2[9], &track_2[pan_len - 4], 4);
	//des_track_2[13] = '\0';
#endif
  }
  else
  {
    vim_mem_copy(des_track_2, track_2, MIN(track_2_length, des_track_2_len));
  }
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_after_query_card(VIM_CHAR sub_code)
{
#if 0
  VIM_RTC_UPTIME now, time, wake_time;
  VIM_ERROR_PTR error = VIM_ERROR_NONE;
  VIM_EVENT_FLAG_PTR pceft_post_pending_flag_ptr=VIM_NULL;
#endif
  if ( txn.error != VIM_ERROR_NONE )
    return txn.error;

  transaction_clear_status( &txn, st_offline_pin );
  if( sub_code == PCEFT_WOW_DEPOSIT_QUERY_CARD )
  {
  		/* RDD: display for 30 seconds, or until interrupted by a PC_EFT command */
		pceft_pos_command_check( &txn, __30_SECONDS__ );
  }
#if 0
  else
  {
	display_screen(DISP_PROCESSING_TERMINAL_ONLY,VIM_PCEFTPOS_KEY_NONE);
  }
  /* get the addresses of the event flags */
  VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceft_post_pending_flag_ptr));

  /* read current time */
  vim_rtc_get_uptime(&now);

  /* add 2 minutes */
  wake_time = now + 120 * 1000;

  while (VIM_TRUE)
  {
    vim_task_process(&wake_time);

    /* read current time */
    vim_rtc_get_uptime(&time);

    if (time > wake_time)
    {
      error = VIM_ERROR_TIMEOUT;
      break;
    }
    if (*pceft_post_pending_flag_ptr)
    {
      error = VIM_ERROR_NONE;
      break;
    }
  }

  //VIM_DBG_ERROR(error);
#endif
  return txn.error;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR validate_query_card(void)
{
  /* Check the card has is valid */
  if (card_name_search(&txn, &txn.card) != VIM_ERROR_NONE)
  {
    return ERR_NO_CPAT_MATCH;
  }
  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR RetrieveLoyaltyNumber( VIM_CHAR *LoyaltyNumber, VIM_SIZE *Length )
{
    VIM_SIZE MaxLen = *Length;
    *Length = 0;
	VIM_ERROR_RETURN_ON_FAIL( vim_file_get( LOYALTY_FILE, LoyaltyNumber, Length, MaxLen ) );
	return VIM_ERROR_NONE;
}

VIM_BOOL UsePCICardData( VIM_CHAR *pci_card_data, VIM_BOOL QueryResponse )
{
    if(( txn.type == tt_query_card ) && ( txn.card_state != card_capture_new_gift ))
    {
        return VIM_FALSE;
    }
    if(( get_pci_card_data( pci_card_data, VIM_TRUE, &txn, QueryResponse )) == VIM_ERROR_NONE )
    {
        return VIM_TRUE;
    }
    return VIM_FALSE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_build_query_response(VIM_PCEFTPOS_QUERY_CARD_RESPONSE *resp)
{
    error_t error_data;
    VIM_SIZE len = VIM_SIZEOF(*resp);

    VIM_DBG_PRINTF1("####### BUILD QUERY RESPONSE ######## Len=%d", len);

    vim_mem_set(resp, ' ', len);

    /* Set Error Code */
    error_code_lookup(txn.error, txn.host_rc_asc, &error_data);

    vim_mem_copy(resp->response_code, error_data.ascii_code, VIM_SIZEOF(resp->response_code));
    vim_mem_copy(resp->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(resp->response_text), vim_strlen(error_data.pos_text)));

    if (txn.error != VIM_ERROR_NONE)
        return VIM_ERROR_NONE;

    resp->tracks_available = '0';

    /* Put info only if card read was successful */

    // RDD v430 change to merge with WBC VIM
    //if(( get_pci_card_data( resp->ePan, VIM_TRUE, &txn ) == VIM_ERROR_NONE )&&( txn.type != tt_query_card ))
    if (UsePCICardData(resp->track_2_data, VIM_TRUE) )
    {
        VIM_UINT8 WOW_AIIC[3] = { 0x05,0x02,0x0F };
        resp->tracks_available = '2'; // RDD - is this right ? What about T1 for Amex ?
        // RDD changed - ACG
        resp->cpat_agc = terminal.card_name[txn.card_name_index].AccGroupingCode;
        //resp->cpat_prcode = (txn.cpat_record.txn_initiation << 4) + txn.cpat_record.txn_authority;

        /* aiic */
        //vim_mem_copy(resp->aiic, POS_BANK_AIIC, vim_strlen(POS_BANK_AIIC));
        vim_mem_copy(resp->aiic, WOW_AIIC, 3);

        /* bin */
        resp->bin_number = PCEFT_BIN_UNKNOWN;
        if (txn.card.type != card_none)
        {
            VIM_UINT32 CpatIndex = txn.card_name_index;
            VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
            resp->bin_number = CardNameTable[CardNameIndex].bin;
        }

        //card_last_track2_save(&card_details);
        if (get_track_data(resp->track_1_data, 1))
        {
            resp->tracks_available = '3'; // RDD - denotes track1 data present (just name) as well as track2
        }
    }

    // RDD 030912 P3: OneCard Loyalty - Card Data can be sourced from a number of different places and mustn't be encrypted
    else if (txn.type == tt_query_card)
    {
        VIM_CHAR LoyaltyNumber[100];
        VIM_CHAR *Ptr = LoyaltyNumber;
        VIM_SIZE Length = VIM_SIZEOF(LoyaltyNumber);

        DBG_MESSAGE("####### SEND ONE CARD QUERY RESPONSE IN CLEAR ########");
        vim_mem_clear(LoyaltyNumber, Length);

        if (!Wally_HasRewardsId())
        {
            VIM_DBG_MESSAGE("No EverydayPay scan - last chance check");
            Wally_CheckPaymentSession();
        }

        resp->tracks_available = '2'; // Loyalty is always stored in the Track2 spot irrespective of the source of the loyalty card number
        VIM_ERROR_RETURN_ON_FAIL(RetrieveLoyaltyNumber(LoyaltyNumber, &Length));

        // RDD 120912 P3: Skip any leading zeros in the loyalty number
        while (*Ptr == '0') Ptr++;

        // RDD v430 change to merge with WBC VIM
        //vim_mem_copy( resp->ePan, Ptr, vim_strlen(Ptr) );
        vim_mem_copy(resp->track_2_data, Ptr, vim_strlen(Ptr));
        DBG_MESSAGE(Ptr);
    }


    if (txn.query_sub_code == PCEFT_WOW_TOKEN_QUERY_CARD)
    {
        if (txn.error == VIM_ERROR_NONE)
        {
            switch (txn.account)
            {
            case account_cheque:
                resp->account_selected = PCEFT_CHEQUE_ACCOUNT;
                break;
            case account_savings:
                resp->account_selected = PCEFT_SAVING_ACCOUNT;
                break;
            case account_credit:
                resp->account_selected = PCEFT_CREDIT_ACCOUNT;
                break;
            case account_default:
                resp->account_selected = PCEFT_DEFAULT_ACCOUNT;
                break;
            default:
                break;
            }
        }
    }

    else if (txn.query_sub_code == PCEFT_READ_CARD_SUB_DEFAULT)
    {
        VIM_SIZE Len = vim_strlen((const VIM_CHAR *)txn.u_PadData.ByteBuff);
        vim_sprintf(resp->transfer_data, "%3.3d%s", Len, txn.u_PadData.ByteBuff);
        resp->transfer_data[Len + 3] = ' ';
    }
    // RDD 261120 JIRA PIRPIN-Avalon
    else if (txn.query_sub_code == PCEFT_WOW_SPECIAL_QUERY)
    {
        VIM_CHAR PadBody[256] = { 0 };
        VIM_SIZE Len = 0;
        if (txn.card_state == card_capture_new_gift)
        {
            QC_GetCommonPadTags(&txn, PadBody, sizeof(PadBody));
            Len = vim_strlen(PadBody);
            if (0 < Len)
            {
                vim_snprintf(resp->transfer_data, sizeof(resp->transfer_data), "%3.3dWWG%3.3d%s", Len + 6, Len, PadBody);
            }
        }
        else if(TERM_EDP_ACTIVE)
		{		
			if (Wally_HasRewardsId() && Wally_GetSessionPADData(PadBody, sizeof(PadBody)))
			{
				Len = vim_strlen(PadBody);
				VIM_DBG_PRINTF2("Got PAD Data '%s'(%u)", PadBody, Len);
				// RDD 181121 JIRA PIRPIN-1189 Add PAD Data to indicate ENTRY Mode ( Wallet NFC or Magstripe )
				// RDD 230522 - ditch ENT tag as this causes PAD data in J8 resp. to go over the 50 byte limit for PcEFTPOS spec.
		  		//vim_snprintf(resp->transfer_data, sizeof(resp->transfer_data), "%3.3dENT001QWWE%3.3d%s", Len + 13, Len, PadBody);
		  		vim_snprintf(resp->transfer_data, sizeof(resp->transfer_data), "%3.3dWWE%3.3d%s", Len + 6, Len, PadBody);
			}
        }     
		else
        {
            VIM_BOOL Exists = VIM_FALSE;
            vim_file_exists(LOYALTY_FILE, &Exists);
            if (Exists)
            {
			  if (WOW_NZ_PINPAD)
			  {
				  // NZ is always 'M'
                vim_strcpy(resp->transfer_data, "007ENT001M");
			  }
			  else
			  {
				  // Australia is always 'N' - NFC
				  vim_strcpy(resp->transfer_data, "007ENT001N");
			  }
            }
        }
	  
		VIM_DBG_STRING(resp->transfer_data);
        Len = vim_strlen(resp->transfer_data);
        if (Len < sizeof(resp->transfer_data))
        {
            resp->transfer_data[Len] = ' ';
        }
    }

    // RDD 100920 Trello #263 J8 Response is too long because of 750 bytes empty space for PAD
    else
    {
        VIM_SIZE Len = 0;
        vim_sprintf(resp->transfer_data, "%3.3d", Len);
        resp->transfer_data[Len + 3] = ' ';
    }

    // RDD 290113 add aiic as there appeared to be garbage in here
    vim_mem_copy(resp->aiic, POS_BANK_AIIC, vim_strlen(POS_BANK_AIIC));

    DBG_MESSAGE("Query card complete, clearing EDP PF bit");
    terminal_clear_status(ss_edp_query_powerfail);

    return VIM_ERROR_NONE;
}




////////////////////////////////////////
// QUERY CARD
////////////////////////////////////////

// RDD 240620 Trello #167 OpenPay Implementation
VIM_ERROR_PTR vim_pceftpos_callback_process_pad_query( const VIM_PCEFTPOS_QUERY_CARD_REQUEST *req, VIM_PCEFTPOS_QUERY_CARD_RESPONSE *resp )
{
    VIM_DBG_MESSAGE( "OOOOOOOOOOOOO OpenPay Txn OOOOOOOOOOOOO" );
    txn_init();

    txn.query_sub_code = PCEFT_READ_CARD_SUB_DEFAULT;
    txn.type = tt_query_card_openpay;
    vim_mem_copy(txn.u_PadData.ByteBuff, req->transfer_data, VIM_MIN(vim_strlen(req->transfer_data), TRANSFER_DATA_LEN));
    txn_process();
    VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_callback_build_query_response(resp));
    //pceft_pos_display_close();
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR vim_pceftpos_callback_process_query_card
(
    VIM_CHAR sub_code,
    const VIM_PCEFTPOS_QUERY_CARD_REQUEST *req,
    VIM_PCEFTPOS_QUERY_CARD_RESPONSE *resp,
    VIM_BOOL *chip_card,
    VIM_BOOL function_menu_call
)
{
  VIM_CHAR type[15];
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_SIZE PadLen = vim_strlen(req->transfer_data);

  /* save the operation initiator */
  TERM_EDP_ACTIVE = VIM_FALSE;

  if( st_standalone == terminal.mode )
  {
	  txn.error = ERR_TXN_NOT_SUPPORTED;
  }
  else
  {
      txn_init();
	// RDD 121211 - set err by default
	txn.error = VIM_ERROR_NONE;

	// RDD - lock against VHQ reboots
	terminal_set_status(ss_locked);

	if( function_menu_call )
	{
		set_operation_initiator(it_pinpad);
	}
	else
	{
		set_operation_initiator(it_pceftpos);
	}

	txn.query_sub_code = sub_code;

    switch (sub_code)
	{

		case PCEFT_WOW_SPECIAL_QUERY:
		{
			// RDD 070814 SPIRA:8030 Allow J8 in Australia too for Macquarie Card
			// RDD 190417 run directly into APDU mode for Pass testing
			//txn_init();
            if (TERM_USE_PCI_TOKENS == VIM_FALSE)
            {
			   // RDD 260422 - allow to run J8 without PCI tokens
               // txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
               // break;
            }

            if (PadLen)
            {
                VIM_CHAR *Ptr = VIM_NULL;
                VIM_DBG_STRING( req->transfer_data );
                pceft_debug_test(pceftpos_handle.instance, (VIM_TEXT *)req->transfer_data);
                if (vim_strstr(req->transfer_data, "WWG", &Ptr) == VIM_ERROR_NONE)
                {
                    vim_mem_copy(txn.u_PadData.ByteBuff, req->transfer_data, VIM_MIN(vim_strlen(req->transfer_data), TRANSFER_DATA_LEN));
                    txn.card_state = card_capture_new_gift;
                    transaction_set_status(&txn, st_giftcard_s_type);
                    pceft_debug_test(pceftpos_handle.instance, "New Gift Card Flow" );
                }
				// RDD 180722 JIRA PIRPIN-1749 uplift EDP defaults

                else if ((vim_strstr(req->transfer_data, "WWE", &Ptr) == VIM_ERROR_NONE) && (TERM_QR_WALLET))
                {
					// Ensure the EDP-related stuff is run
					TERM_EDP_ACTIVE = VIM_TRUE;	

                    if (vim_strstr(req->transfer_data, "DEL", &Ptr) == VIM_ERROR_NONE || 
						vim_strstr(req->transfer_data, "CAN", &Ptr) == VIM_ERROR_NONE)
                    {
                        VIM_CHAR szPaymentSessionId[UUID_LEN + 1] = { 0 };
						VIM_BOOL isDelete = (vim_mem_compare(Ptr, "DEL", 3) == VIM_ERROR_NONE);

						vim_mem_set(resp->response_text, ' ', sizeof(resp->response_text));

						pceft_debugf_test(pceftpos_handle.instance, "EverydayPay - %s Payment Session", isDelete ? "Delete" : "Remove");
                        txn.error = VIM_ERROR_INVALID_DATA;
                        resp->tracks_available = '0';
						vim_mem_set(resp->track_1_data, ' ', sizeof(resp->track_1_data));
						vim_mem_set(resp->track_2_data, ' ', sizeof(resp->track_2_data));
						vim_mem_set(resp->aiic, ' ', sizeof(resp->aiic));

                        // Set up default WWE PAD data
                        vim_snprintf_unterminated(resp->transfer_data, sizeof(resp->transfer_data), "006WWE000");

                        if (Wally_GetPaymentSessionIdFromPAD(req->transfer_data, szPaymentSessionId, sizeof(szPaymentSessionId)) == VIM_ERROR_NONE)
                        {
                            VIM_UINT32 ulDelLen = 0;
                            VIM_CHAR szDeleteReason[100] = { 0 };
                            VIM_CHAR szNoHyphens[UUID_NO_HYPHEN_LEN + 1] = { 0 };

							if (isDelete)
							{
								Ptr += 3;                           // Step over "DEL"
								ulDelLen = VAA_get_PAD_size( Ptr );
								Ptr += 3;                           // Step over 3 'length' digits

								// Ptr now points to the DEL payload (length ulDelLen) - e.g. "ZERODOLLAR" or "CANCELLED" - but it's
								// not (necessarily) null-terminated, so we'll copy it to its own buffer.
                            
								vim_strncpy( szDeleteReason, Ptr, ulDelLen );
								VIM_DBG_PRINTF1( "Delete reason = [%s]", szDeleteReason );

								if (VAA_GetUUIDNoHyphens(szPaymentSessionId, szNoHyphens, sizeof(szNoHyphens)))
								{
									VIM_SIZE ulLen = vim_strlen(szNoHyphens);
									vim_snprintf_unterminated(resp->transfer_data, sizeof(resp->transfer_data), "%03uWWE%03uPSI%03u%s", ulLen + 12, ulLen + 6, ulLen, szNoHyphens);
								}

								if (Wally_DeleteSession(szPaymentSessionId, szDeleteReason, VIM_TRUE))
								{
									vim_strncpy(resp->response_code, "00", 2);
									vim_mem_copy(resp->response_text, "Success", 7);
									txn.error = VIM_ERROR_NONE;
								}
								else
								{
									vim_strncpy(resp->response_code, "05", 2);
									vim_mem_copy(resp->response_text, "Host failure", 12);
								}
							}
							else
							{
								if (VAA_GetUUIDNoHyphens(szPaymentSessionId, szNoHyphens, sizeof(szNoHyphens)))
								{
									VIM_SIZE ulLen = vim_strlen(szNoHyphens);
									VIM_SIZE ulPadLen = 3 + 3 + 6 + 6 + (3 + ulLen);
									if (ulPadLen > sizeof(resp->transfer_data))	// Truncate PSI to fit in the transfer_data
										ulLen -= (ulPadLen - sizeof(resp->transfer_data));
									vim_mem_set(resp->transfer_data, ' ', sizeof(resp->transfer_data));
									vim_snprintf_unterminated(resp->transfer_data, sizeof(resp->transfer_data), "%03uWWE%03uCAN000PSI%03u%s", ulLen + 18, ulLen + 12, ulLen, szNoHyphens);
								}

								VIM_ERROR_PTR e = Wally_RewardsRemoveDialogue(szPaymentSessionId);
								txn.error = e;
								if (e == VIM_ERROR_NONE)
								{
									vim_strncpy(resp->response_code, "00", 2);
									vim_mem_copy(resp->response_text, "Success", 7);
								}
								else if (e == VIM_ERROR_USER_IDLE)
								{
									vim_strncpy(resp->response_code, "TI", 2);
									vim_mem_copy(resp->response_text, "User Timeout", 12);
								}
								else
								{
									vim_strncpy(resp->response_code, "05", 2);
									vim_mem_copy(resp->response_text, "Host failure", 12);
								}
							}
                        }
                        else
                        {
                            vim_strncpy(resp->response_code, "12", 2);
							vim_mem_copy(resp->response_text, "Missing ID", 10);
                            pceft_debugf(pceftpos_handle.instance, "Invalid EDP %s session request. Missing Payment Session ID", isDelete ? "delete" : "remove");
                        }

						terminal_clear_status(ss_locked);
						// Need to return VIM_ERROR_NONE for the response to get sent in VIM
						// We do not want a response if POS sent a message as it causes the POS to receive a 
						// J8 response at a payment request, a situation it cannot handle.
						return txn.error == VIM_ERROR_POS_CANCEL ? txn.error :  VIM_ERROR_NONE;
                    }
                    else
                    {
                        pceft_debug_test(pceftpos_handle.instance, "EverydayPay - Loyalty Flow");
                        vim_strcpy(txn.u_PadData.ByteBuff, req->transfer_data);
                        txn.card_state = card_capture_picc_apdu;
                        // RDD 291217 Wipe previous saved loyalty number for XML reporting purposes
                        vim_file_delete(LOYALTY_FILE);
                        if ((res = pceft_pos_command_check(&txn, 0)) == VIM_ERROR_POS_CANCEL)
                        {
                            terminal.allow_ads = VIM_FALSE;	// Don't allow ads until we processed the incoming txn
							terminal_clear_status(ss_locked);
							return res;
                        }
                        set_operation_initiator(it_pinpad);
                        DBG_MESSAGE("Query card started, setting EDP PF bit");
                        terminal_set_status(ss_edp_query_powerfail);
                    }
                }
            }
            else
            {
                pceft_debug_test(pceftpos_handle.instance, "Loyalty Flow");
                txn.card_state = card_capture_picc_apdu;
                // RDD 291217 Wipe previous saved loyalty number for XML reporting purposes
                vim_file_delete(LOYALTY_FILE);
                if ((res = pceft_pos_command_check(&txn, 0)) == VIM_ERROR_POS_CANCEL)
                {
                    terminal.allow_ads = VIM_FALSE;	// Don't allow ads until we processed the incoming txn
					terminal_clear_status(ss_locked);
					return res;
                }
                set_operation_initiator(it_pinpad);
            }
			txn.type = tt_query_card;

			// RDD 030113 added to try to speed up aborting of J8
			transaction_clear_status( &txn, st_offline_pin );

            // FFL - set keymask to allow POS keydown - This is needed now to allow POS to cancel J8
            vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_CANCEL);

			// RDD 150212: Now run the query as a transaction
				  DBG_POINT;
			txn_process();
				  DBG_POINT;
			if( txn.error == VIM_ERROR_SYSTEM )
					display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 1000 );
			else if( txn.error != VIM_ERROR_NOT_FOUND )
			{
				// Remap sucessful read to new error code so that correct text is displayed
				if( txn.error == VIM_ERROR_NONE )
				{
					// RDD 070814 SPIRA:8030
					if( WOW_NZ_PINPAD )
						txn.error = ERR_ONE_CARD_SUCESSFUL;
					else
						txn.error = ERR_LOYALTY_CARD_SUCESSFUL;
						  DBG_POINT;

#if 0
						  if (WOW_NZ_PINPAD)
							  display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 0);
						  else
#endif
                        // RDD 071119 This adds a totally unnecessary 2 second delay to every J8 as the Thanks screen was already displayed.
						//VIM_ERROR_RETURN_ON_FAIL( display_screen_cust_pcx( DISP_IMAGES, "Label", WOW_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL ));
						//VIM_ERROR_IGNORE(vim_event_sleep(2000)); // wait 2 secs


					//display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 0 );
					txn.error = VIM_ERROR_NONE;
						  DBG_POINT;
				}
			}
			else
			{
				//display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
				transaction_clear_status( &txn, st_offline_pin );
				//pceft_pos_command_check( &txn, __50_MS__ );
				terminal_clear_status(ss_locked);
				return txn.error;
			}
			//pceft_pos_command_check( &txn, __50_MS__ );
			//pceft_pos_command_check( &txn, 0 );

			break;
		}


		case PCEFT_WOW_TOKEN_QUERY_CARD:
		{
			txn.type = tt_query_card_get_token;

			// RDD 060212 - Force Credit Account as no account selection required
			txn.account = account_credit;
			txn.pos_account = VIM_TRUE;

            // 300320 DEVX J5 not supported in fuel now.
            if (TERM_USE_PCI_TOKENS == VIM_FALSE)
            {
                txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
                break;
            }


			// RDD 150212: Now run the query as a transaction
			txn_process();

			if( txn.error != VIM_ERROR_NONE )
			{
				if( txn.error == ERR_WOW_ERC )
				{
					if( !txn.card.pci_data.eTokenFlag )
					{
						// RDD 290212: SPIRA 4984 Behavior when no token available
						txn.error = ERR_WOW_ERC;
						vim_strcpy( txn.host_rc_asc, "R2" );
						display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
						terminal.idle_time_count = 0;
						terminal.screen_id = 0;
						terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
						vim_rtc_get_uptime(&terminal.idle_start_time);
						break;
					}
					else
					{
						display_result( txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
					}
				}
				else
				{
					//display_result( txn.error, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000 );
					// RDD 200213 - Go direct to Ready Screen
					txn.type = tt_none;
				}

				terminal.idle_time_count = 0;
				terminal.screen_id = 0;
				terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
				vim_rtc_get_uptime(&terminal.idle_start_time);
				break;
			}


			// if this routine was called from the function menu we display rather than print the data
			if( function_menu_call )
			{
				VIM_TEXT masked_pan[20];

				// get the masked pan details
				mask_out_track_details_txn( masked_pan, VIM_SIZEOF(masked_pan), &txn.card, PCEFT_BIN_UNKNOWN, VIM_FALSE, VIM_FALSE );

				VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TOKEN_LOOKUP_RESULT, VIM_PCEFTPOS_KEY_CANCEL, masked_pan, txn.card.pci_data.eToken ));

				// wait 60 secs or ENTER, CLR or CNCL key pressed
				if(get_yes_no(GET_YES_NO_TIMEOUT) != VIM_ERROR_NONE )
				{
          terminal.idle_time_count = 0;
		  terminal.screen_id = 0;
          terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
          vim_rtc_get_uptime(&terminal.idle_start_time);
					break;
				}
			}

			// If there is a token then we need to send a receipt to the POS
			else
			{
				VIM_PCEFTPOS_RECEIPT receipt;
				vim_mem_clear(&receipt, VIM_SIZEOF(receipt));
				get_txn_type_string_short(&txn, type, sizeof(type));
				display_screen( DISP_APPROVED_TXN_ONLY, VIM_PCEFTPOS_KEY_OK, type );
				VIM_ERROR_RETURN_ON_FAIL( receipt_build_token_query( HOST_WOW, &receipt ));
				pceft_pos_print(&receipt, VIM_TRUE);
			}
		}
        terminal.idle_time_count = 0;
	    terminal.screen_id = 0;
        terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
        vim_rtc_get_uptime(&terminal.idle_start_time);
		break;

		case PCEFT_WOW_DEPOSIT_QUERY_CARD:
		{
			txn.type = tt_query_card_deposit;
			txn.pos_account = VIM_TRUE;
			txn.account = account_credit;

			// RDD 150212: Now run the query as a transaction
			txn_process();

			if(( txn.error ) == VIM_ERROR_NONE )
			{
				// RDD 200212 - Store a record in the Card Buffer. Creates the file if no existing records
				VIM_ERROR_RETURN_ON_FAIL( saf_add_record( &txn, VIM_TRUE ) );
				display_screen( DISP_CARD_READ_SUCCESSFUL, VIM_PCEFTPOS_KEY_NONE );
				terminal.idle_time_count = 0;
				terminal.screen_id = 0;
				terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
				vim_rtc_get_uptime(&terminal.idle_start_time);
				break;
			}
			//if( vim_strlen(txn.host_rc_asc) == 2 )
			//	display_result( txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, 2000 );
			//else
			//	txn.type = tt_none;

			terminal.idle_time_count = 0;
			terminal.screen_id = 0;
			terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
			vim_rtc_get_uptime(&terminal.idle_start_time);
			break;
		}

		/* Todo */
        case PCEFT_WOW_PRESWIPE:
            // RDD 091220 removed redundant code.
            pceft_debug( pceftpos_handle.instance, "J7 No longer Supported in Code" );
            // Intentional drop through to default

		default:
			txn.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
	     	terminal.idle_time_count = 0;
			terminal.screen_id = 0;
			terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
			vim_rtc_get_uptime(&terminal.idle_start_time);
		break;

	  }
      // RDD - TODO add Query Card Jounal Type
      //pceft_journal_txn(journal_type_advice, &txn_trickle);

      vim_pceftpos_callback_build_query_response(resp); 	  
	//pceft_pos_display_close();

    if (txn.card_state == card_capture_new_gift)
    {
        DisplayProcessing(DISP_PROCESSING_WAIT);
        //vim_event_sleep(1000);
    }

      /* restore the operation initiator */	  
      set_operation_initiator(it_pinpad);	
	}
	terminal_clear_status(ss_locked);
	return VIM_ERROR_NONE;
}



/* RDD - POS Response Text is built from Error code Lookup - if a VIM error generated then the Asc Resp
   code will be indexed from the Error
*/

static void BuildPadString( transaction_t *transaction, VIM_CHAR *PadString, VIM_SIZE BufSize )
{
	//Allow for 3 bytes of length at the beginning
	VIM_SIZE offset = 3;
	VIM_CHAR Pan[24 + 1] = { 0 };
	VIM_BOOL CreateSurchargeTag = transaction_get_status(transaction, st_pos_sent_sc_tag);

	vim_strcpy(PadString, "000");	

	// RDD 160922 JIRA PIRPIN_1815 Return standard TAGS for BPOZ (and LIVE etc.)
	if ((!IS_R10_POS) && (!FUEL_PINPAD) && (!ALH_PINPAD))
	{
		if (CreateSurchargeTag)
		{
			vim_sprintf(&PadString[offset], "SUR009%9.9d", transaction->amount_surcharge );
			offset += 6+9;
		}
		vim_sprintf(&PadString[offset], "AMT009%9.9d", transaction->amount_total);
		offset += 6 + 9;
	}
	else // R10 + ALH + EG
	{
		if (card_get_pan(&transaction->card, Pan) == VIM_ERROR_NONE)
		{
			VIM_CHAR FormatPan[24 + 1] = { 0 };
			VIM_SIZE panLen = vim_strlen(Pan);

			// format the Pan for sending to POS - note can use same alloc here
			receipt_format_pan(FormatPan, Pan, panLen, "6-4", '1');
			vim_snprintf(&PadString[offset], BufSize - offset, "WID008%8.8sTRU015%15.15s", terminal.terminal_id, FormatPan);
			offset += 6 + 8 + 6 + 15;
		}

		// RDD 170712 Need to add DE63 to PAD string for any response to 91
		// RDD 060812 SPIRA:5845 Need to return full PAD if PinPad full as well so that POS can recall the txn with the full data
		if (((transaction->saf_reason == saf_reason_91) || (!vim_strncmp(transaction->host_rc_asc, "TP", 2))) && (fuel_card(transaction)))
		{
			VIM_SIZE FcdLen = vim_strlen(transaction->u_PadData.PadData.DE63);
			// Need to tell the length of the FCD data
			vim_snprintf(&PadString[offset], BufSize - offset, "FCD%3.3d%s", FcdLen, transaction->u_PadData.PadData.DE63);
			offset += 6 + FcdLen;
		}

		if (IsEverydayPay(&txn))
		{
			VIM_CHAR szUUIDNoHyphens[UUID_NO_HYPHEN_LEN + 1] = { 0 };
			if (VAA_GetUUIDNoHyphens(txn.edp_payment_session_id, szUUIDNoHyphens, sizeof(szUUIDNoHyphens)))
			{
				VIM_SIZE uuidLen = vim_strlen(szUUIDNoHyphens);
				vim_snprintf(&PadString[offset], BufSize - offset, "WWE%03uPSI%03u%s", uuidLen + 6, uuidLen, szUUIDNoHyphens);
				offset += 6 + 6 + uuidLen;
			}
			else
			{
				pceft_debug_test(pceftpos_handle.instance, "Everyday Pay transaction, but no Payment Session ID to add to PAD data");
			}
			VIM_DBG_MESSAGE("Everyday transaction response, clearing Wally session data");
			Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_FALSE);
		}

		if (0 < vim_strlen(transaction->u_PadData.ByteBuff))
		{
			vim_snprintf(&PadString[offset], BufSize - offset, "%s", transaction->u_PadData.ByteBuff);
			offset += vim_strlen(transaction->u_PadData.ByteBuff);
		}
	}

	vim_snprintf_unterminated(PadString, 3, "%3.3d", offset-3 );

	if (offset < BufSize)
	{
		// Get rid of any terminating NULL
		PadString[offset] = ' ';
	}
}


static VIM_CHAR ReturnEntryMode( transaction_t *transaction )
{
    VIM_CHAR type_code = '0';
    if( transaction->card.type )
	{
		switch( transaction->card.type )
		{
			case card_mag: return 'S';

			case card_chip: return 'E';

            case card_picc: return 'C';
			
            default: type_code = *QC_EntryMode(transaction);
		}
	}
	return type_code;
}



void SettlementDate( transaction_t *transaction, VIM_CHAR *SettleDayPtr, VIM_CHAR *SettleMonthPtr )
{
	VIM_UINT8 last_settle_day = bcd_to_bin( &terminal.last_settle_date[1], 2 );
	VIM_UINT8 last_settle_month = bcd_to_bin( &terminal.last_settle_date[0], 2 );

	// if the txn was sucessfully sent or the currently stored settle date is more than the current date then use the returned settlement date
	if( transaction->settle_date[0]  )
	{
		bcd_to_asc( &transaction->settle_date[0], SettleMonthPtr, 2 );
		bcd_to_asc( &transaction->settle_date[1], SettleDayPtr, 2 );
	}
	// If the current day is more than the last settle day then use current day
	else if(( transaction->time.day > last_settle_day ) && ( transaction->time.month >= last_settle_month ))
	{
		bin_to_asc( transaction->time.month, SettleMonthPtr, 2 );
		bin_to_asc( transaction->time.day, SettleDayPtr, 2 );
	}
	// Otherwise the settle day had rolled before we went offline today
	else
	{
		bcd_to_asc( &terminal.last_settle_date[0], SettleMonthPtr, 2 );
		bcd_to_asc( &terminal.last_settle_date[1], SettleDayPtr, 2 );
	}
}


VIM_ERROR_PTR pceft_check_override_edp_tender(transaction_t* transaction, VIM_CHAR* pszCardName, VIM_CHAR* pszCardDescription, VIM_SIZE ulCardDescriptionSize)
{
	VIM_ERROR_PTR eRet = VIM_ERROR_NULL;

	if (!IS_R10_POS)
	{
		return VIM_ERROR_NONE;
	}

	if (transaction && pszCardName && pszCardDescription)
	{
		if (IsEverydayPay(transaction))
		{
			VIM_CHAR szExistingDescription[20 + 1] = { 0 };

			vim_mem_copy(szExistingDescription, pszCardDescription, VIM_MIN(sizeof(szExistingDescription) - 1, vim_strlen(pszCardDescription)));
			pceft_debugf_test(pceftpos_handle.instance, "Overriding existing Tender (ID)Name (%c%c)%s with EverydayPay (%s)%s", pszCardName[0], pszCardName[1], szExistingDescription, TERM_QR_TENDER_ID, TERM_QR_TENDER_NAME);

			vim_mem_copy(pszCardName, TERM_QR_TENDER_ID, 2);
			vim_mem_set(pszCardDescription, ' ', ulCardDescriptionSize);
			vim_snprintf_unterminated(pszCardDescription, ulCardDescriptionSize, "%s", TERM_QR_TENDER_NAME);
		}
		eRet = VIM_ERROR_NONE;
	}
	return eRet;
}

VIM_ERROR_PTR pceft_build_txn_response
(
  /*@out@*/VIM_PCEFTPOS_TRANSACTION_RESPONSE *resp,
  transaction_t *transaction
)
{
  VIM_RTC_TIME date;
  error_t error_data;
  VIM_UINT32 CpatIndex = transaction->card_name_index;
  VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;

  // RDD 060412 - create a temp buffer for pceft "Z" response
  //VIM_CHAR buffer[VIM_SIZEOF(VIM_PCEFTPOS_TRANSACTION_RESPONSE)+1];
  //vim_mem_clear( buffer, VIM_SIZEOF(buffer) );

  /* clear the buffer */
  vim_mem_set(resp, ' ', sizeof(*resp));

  /* Set Error Code */
  error_code_lookup(transaction->error, transaction->host_rc_asc, &error_data);

  // RDD 070912 P3: NZ POS Doesn't want to receive 91 as voucher processing not supported
  DBG_VAR( terminal.merchant_id[8] );
  if(( WOW_NZ_PINPAD )&&( !vim_mem_compare( error_data.ascii_code, "TR", 2 )))
  {
	  DBG_MESSAGE( "!!!! NZ: REMAP 91 -> 01 !!!!");
	  vim_strcpy( error_data.ascii_code, "TR" );
  }

  // RDD 250116 SPIRA:8774 Need to reboot overnight if error is M0 system error due to issues with EMV GENAC1 getting killed
  terminal.idle_time_count = 0;
  if( vim_mem_compare( error_data.ascii_code, "M0", 2 ) == VIM_ERROR_NONE )
  {
	  terminal.configuration.ctls_reset |= CTLS_ONCE_OFF_REBOOT;
  }

  vim_mem_copy(resp->response_code, error_data.ascii_code, VIM_SIZEOF(resp->response_code));
  vim_mem_copy(resp->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(resp->response_text),vim_strlen(error_data.pos_text)));

  if (transaction->error == ERR_NO_DUPLICATE_TXN)
  {
    transaction->amount_purchase = 0;
    transaction->amount_tip = 0;
    transaction->amount_cash = 0;
    transaction->amount_refund = 0;
    transaction->amount_total = 0;
    return VIM_ERROR_NONE;
  }

  if( transaction->error == ERR_TXN_NOT_SUPPORTED )
  {
    transaction->amount_purchase = 0;
    transaction->amount_tip = 0;
    transaction->amount_cash = 0;
    transaction->amount_refund = 0;
    transaction->amount_total = 0;
    return VIM_ERROR_NONE;
  }


  /* terminal id */
  if(terminal.terminal_id[0] != 0)
    vim_mem_copy(resp->catid, terminal.terminal_id, VIM_SIZEOF(resp->catid));

  /* merchant id */
  if(terminal.merchant_id[0] != 0)
    vim_mem_copy(resp->caic, terminal.merchant_id, VIM_SIZEOF(resp->caic));


  /* set the amount */
  bin_to_asc(transaction->amount_purchase, resp->purchase_amount, VIM_SIZEOF(resp->purchase_amount));
  bin_to_asc(transaction->amount_cash, resp->cash_amount, VIM_SIZEOF(resp->cash_amount));
  bin_to_asc(transaction->amount_refund, resp->refund_amount, VIM_SIZEOF(resp->refund_amount));

  /* date/time */
  vim_snprintf_unterminated(resp->bank_date_dd,VIM_SIZEOF(resp->bank_date_dd),"%02d",transaction->time.day);
  vim_snprintf_unterminated(resp->bank_date_mm,VIM_SIZEOF(resp->bank_date_mm),"%02d",transaction->time.month);
  vim_snprintf_unterminated(resp->bank_date_yy,VIM_SIZEOF(resp->bank_date_yy),"%02d",transaction->time.year%100);
  vim_snprintf_unterminated(resp->bank_time_hh,VIM_SIZEOF(resp->bank_time_hh),"%02d",transaction->time.hour);
  vim_snprintf_unterminated(resp->bank_time_mm,VIM_SIZEOF(resp->bank_time_mm),"%02d",transaction->time.minute);
  vim_snprintf_unterminated(resp->bank_time_ss,VIM_SIZEOF(resp->bank_time_ss),"%02d",transaction->time.second);

  // RDD 100812 SPIRA:5872 Set Default Index to Max so that we can max BIN range invalid if card data not captured before txn is cancelled
  if( transaction->card_name_index < WOW_MAX_CPAT_ENTRIES )
  {
	  /* RDD 231211 updated: Card Name and PCEFT Bin (mapped from WOW bin) */
	  vim_snprintf_unterminated( resp->card_bin, VIM_SIZEOF(resp->card_bin),"%02d", CardNameTable[CardNameIndex].bin );
#if 1
	  // RDD 020714 SPIRA:7562 Return EMV Preferred Name if available
	  {
		  VIM_CHAR Buffer[50];
		if( GetCardName( transaction, Buffer ) != VIM_ERROR_NONE )
			vim_mem_copy( resp->card_description, CardNameTable[CardNameIndex].card_name, vim_strlen(CardNameTable[CardNameIndex].card_name)  );
		else
			vim_mem_copy( resp->card_description, Buffer, vim_strlen(Buffer) );
	  }
#else
		vim_mem_copy( resp->card_description, CardNameTable[CardNameIndex].card_name, vim_strlen(CardNameTable[CardNameIndex].card_name)  );
#endif
  }

  pceft_check_override_edp_tender(transaction, resp->card_bin, resp->card_description, sizeof(resp->card_description));

  /* RDD - WOW get pci PAN/Token */

  // Set '?' char by default - v. important !
  vim_mem_set( resp->track_2, '?', VIM_SIZEOF(resp->track_2) );
  get_pci_card_data( resp->track_2, VIM_TRUE, transaction, VIM_FALSE );

  /* stan */
  bin_to_asc(transaction->stan, resp->stan, VIM_SIZEOF(resp->stan));

  /* account type */
  switch (transaction->account)
  {
    case account_savings:
      resp->account = PCEFT_SAVING_ACCOUNT;
      break;
    case account_cheque:
      resp->account = PCEFT_CHEQUE_ACCOUNT;
      break;
    case account_credit:
      resp->account = PCEFT_CREDIT_ACCOUNT;
      break;
    default:
      resp->account = PCEFT_DEFAULT_ACCOUNT;
      break;
  }

  /* txn ref */

  if( transaction->pos_reference[0] != 0 )
    vim_mem_copy(resp->reference_num, transaction->pos_reference, vim_strlen(transaction->pos_reference) );

  /* settle date */

  // RDD 250113 SPIRA:6528 Handle Setlement Date for Offline Txns
  vim_rtc_get_time(&date);

  // RDD 250516 SPIRA:8985 Settle Date should always reflect that returned from host
  SettlementDate( transaction, resp->settle_date_dd, resp->settle_date_mm );

  /* auth code */
  vim_mem_copy(resp->auth_code, transaction->auth_no, MIN(vim_strlen(transaction->auth_no), VIM_SIZEOF(resp->auth_code)));

  /* tip amount */
  /*bin_to_asc(0, resp->tip_amount, 9);*/
  bin_to_asc(transaction->amount_tip, resp->tip_amount, 9 );

  /* txn type */
  resp->txn_type = transaction->pos_cmd;

  /* rrn */
  //vim_snprintf_unterminated(resp->rrn, VIM_SIZEOF(resp->rrn), "%012d", transaction->roc );
  vim_mem_copy(resp->rrn, transaction->rrn, VIM_SIZEOF(resp->rrn) );

  // RFU Settings:

  // Flag1 : Offline or Online
  // RDD 300714 BD requested this change to be made
  resp->offline_flag = ((transaction_get_status(transaction, st_efb) || transaction_get_status(transaction, st_offline)) && ( is_approved_txn( &txn ))) ? '1' : '0';

  // Flag1 : Receipt Printed (RDD 300312 changed to reflect actual receipt printing as flag now added to status word)
  resp->rfu_flag_1 = ( transaction->error == VIM_ERROR_PRINTER ) ? '0' : '1';

  // Flag2 : Entry Mode
  resp->rfu_flag_2 = ReturnEntryMode( transaction );

  // Flag3 : Comms method (not used)
  resp->rfu_flag_3 = '0';

  // Flag4 : Entry Mode
  resp->rfu_flag_4 = '0';

  // Flag5 : Paspass or not paypass: TODO - make this run off Pceft Bin once this has been fixed
  resp->rfu_flag_5 = (( transaction->card.type == card_picc) && ( transaction->card.pci_data.ePan[0] == '5' )) ? '1' : '0';

  // Flag6 : Not used
  resp->rfu_flag_6 = '0';

  // Flag7 : Not used
  resp->rfu_flag_7 = '0';

  /* RDD: build PAD string */
  BuildPadString( transaction, resp->transfer_data, sizeof(resp->transfer_data));
  //vim_mem_copy( buffer, resp, VIM_SIZEOF(buffer) );
  //pceft_debug(pceftpos_handle.instance, buffer );
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR vim_pceftpos_callback_generic_initialisation( logon_state_t set_logon_state )
{

//	if( !is_integrated_mode() )
//	  return VIM_ERROR_NONE;

	// RDD 290312 - clear the init struct or config will pick up last debug error
	vim_mem_clear( &init, VIM_SIZEOF(init) );
	txn_init();

  // Check We have Keys Loaded
  if( terminal.logon_state == initial_keys_required ) return ERR_NO_KEYS_LOADED;

  // Check Terminal Is configured
  if ( !terminal_get_status( ss_tid_configured ))
	  return ERR_CONFIGURATION_REQUIRED;

  // Set the logon State to that requested

  // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
  terminal.last_logon_state = terminal.logon_state;

  if( set_logon_state < terminal.logon_state ) terminal.logon_state = set_logon_state;

  // RDD 150212 added: TODO - this actually stuffs up duplicate recall being preserved accross logons etc. so probably need to fix this at some point
  txn_init();
  txn.type = tt_logon;	// needed to prevent last txn type being displayed at top of screen on error

  // RDD 180412 SPIRA:5268 Delete Duplicate Txn because this has now been trashed
  //txn_duplicate_txn_delete();

  // Save the operation initiator
  set_operation_initiator(it_pceftpos);

  init.error = ReversalAndSAFProcessing( MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_TRUE );


  // Display result and print receipt
  logon_finalisation(VIM_FALSE);

  // Restore the operation initiator
  set_operation_initiator(it_pinpad);

  //vim_pceftpos_display_close(pceftpos_handle.instance);

  /* update idle timer */
  terminal.idle_time_count = 0;
  terminal.allow_idle = VIM_TRUE;

  vim_rtc_get_uptime(&terminal.idle_start_time);

  pceft_pos_display_close();

  /* return without error */
  return init.error;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_bank
(
  VIM_UNUSED(VIM_PCEFTPOS_INITIALIZATION_REQUEST const *,request_ptr)
)
{
	if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

#ifdef _WIN32
	vim_strcpy( (VIM_CHAR *)request_ptr->tag_data, "SFP0031" );
#endif

	// RDD 130215 SPIRA:8305
	ControlSAFPrints( ( VIM_CHAR *)request_ptr->tag_data );

	return vim_pceftpos_callback_generic_initialisation( logon_required );
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_rsa
(
  VIM_UNUSED(VIM_PCEFTPOS_INITIALIZATION_REQUEST const *,request_ptr)
)
{
	  if( !is_integrated_mode() )
		  return VIM_ERROR_NONE;

	return vim_pceftpos_callback_generic_initialisation( rsa_logon );
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_diagnostics
(
  VIM_UNUSED(VIM_PCEFTPOS_INITIALIZATION_REQUEST const *,request_ptr)
)
{
  /* print public keys */
  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  // RDD 290312 - clear the init struct or config will pick up last debug error
	vim_mem_clear( &init, VIM_SIZEOF(init) );

  set_operation_initiator(it_pceftpos);

  PrintLastEMVTxn( VIM_TRUE );

  VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_display_close(pceftpos_handle.instance));
  vim_mem_copy(init.logon_response, "00", VIM_SIZEOF(init.logon_response));  //BRD need to return 00 not last logon response

  return VIM_ERROR_NONE;

}


VIM_BOOL ConfigChanged( VIM_PCEFTPOS_INITIALIZATION_REQUEST const *request_ptr, VIM_CHAR *terminal_id, VIM_CHAR *merchant_id )
{
#if 0
	VIM_CHAR *Ptr = (VIM_CHAR *)&request_ptr->merchant_id[0];
#endif
	VIM_BOOL Changed = VIM_FALSE;

	vim_mem_copy( terminal_id, request_ptr->terminal_id, sizeof(terminal.terminal_id));

    // VN JIRA WWBP-388 Remove Config Merchant TID manipulation
    vim_mem_copy(merchant_id, request_ptr->merchant_id, sizeof(terminal.merchant_id));
#if 0
	// RDD 150812 NZ Processing: If byte 1 of the TID in the config req. is a '9' then the MID is constructed differently

	if( terminal_id[1] == '9' )
	{
		vim_strcpy( merchant_id, "61100060900" );
		vim_strncat( merchant_id, &request_ptr->terminal_id[1], 4 );
		if( terminal.terminal_id[1] != '9' )
		{
			DBG_MESSAGE( "<<<<<<<< MOVING FROM AUS -> NZ >>>>>>>>>>>" );
		}
	}
	else
	{
		// RDD 8 digit MIDs from POS require pre-pended header. Do a test by copying over 9 digits and looking for a space
		vim_mem_copy( merchant_id, request_ptr->merchant_id, UNIQUE_MID_LEN+1 );
		merchant_id[UNIQUE_MID_LEN+1] = '\0';

		if( vim_strchr( merchant_id, ' ', &Ptr ) != VIM_ERROR_NONE )
		{
			vim_mem_copy( merchant_id, request_ptr->merchant_id, MID_LEN );
		}
		else
		{
			if( Ptr - &merchant_id[0] == UNIQUE_MID_LEN )
			{
				// We have an 8 digit MID so pre-pend the header
				vim_strcpy( merchant_id, "6110006" );
				vim_strncat( merchant_id, request_ptr->merchant_id, UNIQUE_MID_LEN );
			}
		}
	}
	// RDD 100415 SPIRA:8581 v541-7 - modification of TID[0] to 'W' is absolutly required for Australian Integrated terminals
	// RDD 200718 JIRA WWBP-216 Need to support TID starting with 'A'
	//if( is_integrated_mode() )
	if(( is_integrated_mode()) && ( terminal_id[0] != 'A' ))
		terminal_id[0] = ( merchant_id[8] == '9' ) ? 'N' : 'W';
#endif
	if( vim_strncmp( terminal_id, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id) )) Changed = VIM_TRUE;
	if( vim_strncmp( merchant_id, terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id) )) Changed = VIM_TRUE;

    // RDD 161019 Need to ensure that this is clear unless TID/MID has actually changed otherwise TMS checks don't get run on powerup
    terminal_clear_status(ss_config_changed);

    if( terminal_get_status(ss_tms_configured))
    {
        if (Changed)
        {
            terminal_set_status(ss_config_changed);
		    // RDD 170722 JIRA PIRPIN-1747
			//terminal_clear_status(ss_tms_configured);
        }
    }
	return Changed;
}



VIM_BOOL ValidateConfigData( VIM_CHAR *terminal_id, VIM_CHAR *merchant_id )
{
	VIM_UINT8 n;
	VIM_CHAR *Ptr = terminal_id;
 //   VIM_BOOL exists = VIM_FALSE;
	DBG_MESSAGE( "<<<<<<<< VALIDATING CONFIG DATA >>>>>>>>>>>" );
	VIM_DBG_DATA( terminal.terminal_id, 8 );
	VIM_DBG_DATA( terminal.merchant_id, 15 );

	// RDD 150914 v533 - check TID for Standalone
	// RDD 040814 SPIRA:8038 Allow Alpha TID
    // RDD 070519 JIRA WWBP-620 need to check last digit of TID
	for( n=1; n<=8; n++, Ptr++ )
	{
		if(( *Ptr < '0' )||( *Ptr > 'Z' ))
		{
			VIM_DBG_DATA( Ptr, 1 );
            VIM_DBG_MESSAGE("Invalid TID");
            return VIM_FALSE;
		}
	}

	for( Ptr = merchant_id, n=0; n< VIM_SIZEOF(terminal.merchant_id); n++, Ptr++ )
	{
		if(( *Ptr < '0' )||( *Ptr > 'Z' ))
		{
			if( is_integrated_mode() )
			{
                VIM_DBG_MESSAGE("Invalid MID");
				return VIM_FALSE;
			}
			else
			{
				// RDD 221114 - for standalone we need to pick up MID from TMS
				DBG_MESSAGE( "MID Invalid - need TMS" );
				terminal_clear_status( ss_tms_configured );
			}
		}
	}

	DBG_MESSAGE( "<<<<<<<< CONFIG DATA VALIDATED >>>>>>>>>>>" );
	terminal_save();
	return VIM_TRUE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_config
(
  VIM_PCEFTPOS_INITIALIZATION_REQUEST const *request_ptr
)
{
	VIM_CHAR terminal_id[TID_LEN+1];
	VIM_CHAR merchant_id[MID_LEN+1];

	VIM_ERROR_PTR ret;
#ifndef _WIN32
  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;
#endif
	// RDD 290312 - clear the init struct or config will pick up last debug error
	vim_mem_clear( &init, VIM_SIZEOF(init) );
	vim_mem_clear( terminal_id, VIM_SIZEOF( terminal_id ));
	vim_mem_clear( merchant_id, VIM_SIZEOF( merchant_id ));

	// Config response uses txn error !!	
	// RDD 100523 JIRA PIRPIN-2376 Missing Power Fail receipt - stan getting deleted prevents power fail receipt print
	//txn_init();

	// VN This was clearing txn after powerfail. so the receipts were not printed. To fix WWBP-795 Power fail receipt not printed
	//txn_init();

	// Check to see if the config is actually being changed
	if( !ConfigChanged( request_ptr, terminal_id, merchant_id ) )
	{
		// RDD 170512 SPIRA 5524 Set Host ID even if TID not changed
		if( request_ptr->terminal_id[7] %2 )
		{
			vim_mem_copy( terminal.host_id, WOW_HOST1, vim_strlen( WOW_HOST1 ) );
		}
		else
		{
			vim_mem_copy( terminal.host_id, WOW_HOST2, vim_strlen( WOW_HOST2 ) );
		}
		terminal_set_status( ss_tid_configured );
		//terminal_set_status( ss_tms_configured);

		// RDD 130812 - We can't set TMS required if the config wasn't allowed because of a SAF or Reversal pending.
		// If we did then nothing would work at all - txns rejected with LFD pending and LFD rejected with SAF pending !

		// If there are reversals or safs pending on power up with config not changed then ensure that TMS is NOT pending...
#if 0
		if( !terminal.tms_configured )
			terminal.tms_configured = ( terminal_check_config_change_allowed( VIM_FALSE ) == VIM_ERROR_NONE ) ? VIM_FALSE : VIM_TRUE;
#endif
		// RDD SPIRA:5501 170512 : added to ensure comms in correct state after SCO POS boot up
		comms_disconnect();

		DBG_MESSAGE( "<<<<<<<< CONFIG NOT CHANGED: EXIT >>>>>>>>>>>" );

		// Clear any previous txn errors
		// VN 300119 JIRA WWBP-689 PINpad is unable to logon after a Power fail
		vim_mem_copy(txn.host_rc_asc, "00", 2);

		//terminal_save();
		return VIM_ERROR_NONE;
	}

	// TID or MID is being changed so check to see if the config change is allowed
    if(( ret = terminal_check_config_change_allowed( VIM_TRUE )) != VIM_ERROR_NONE )
	{
		// RDD 050412 : SPIRA 5142 PinPad must become useless in new lane if SAFs pending from old.
		// This will force the operator to move the PinPad back to the original lane to clear the SAF
		DBG_MESSAGE( "<<<<<<<< CONFIG CHANGE NOT ALLOWED: EXIT  >>>>>>>>>>>" );
		pceft_debug(pceftpos_handle.instance, "CONFIG CHANGE NOT ALLOWED: EXIT" );
		// RDD 110712 - if the config is not allowed then leave the previous config intact
		//terminal.configured = ValidateConfigData( terminal_id, merchant_id );
		// RDD 080812 SPIRA:5862 BD Says it's fine to have the PinPad in "C" state after a lane change with SAFs etc. pending so remove line above.
		terminal.configured = VIM_FALSE;
		// VN JIRA WWBP-772 231219 P400 v702.9 PINpad goes into a D state when a
		// config (TID change) is attempted and a SAF is stored
		terminal_clear_status(ss_config_changed);
		terminal_set_status(ss_tms_configured);
		return ret;
	}
	else
	{
		if( ValidateConfigData( terminal_id, merchant_id ))
		{
			pceft_debug(pceftpos_handle.instance, "CONFIG CHANGE ALLOWED" );
			DBG_MESSAGE( "<<<<<<<< CONFIG CHANGE ALLOWED >>>>>>>>>>>" );

			// RDD - for WOW if Term ID is even then use WOW_HOST2 if odd then use WOW_HOST1
			if( request_ptr->terminal_id[7] %2 )
			{
				vim_mem_copy( terminal.host_id, WOW_HOST1, vim_strlen( WOW_HOST1 ) );
			}
			else
			{
				vim_mem_copy( terminal.host_id, WOW_HOST2, vim_strlen( WOW_HOST2 ) );
			}


			// TID or MID has changed. Save the new value and set RSA Logon. Also we MUST Do a LFD Logon
			pceft_debug(pceftpos_handle.instance, "NEW CATID/CAIC OK... COPYING INTO TERMINAL FILE" );
			DBG_MESSAGE( "NEW CATID/CAIC OK... COPYING INTO TERMINAL FILE" );
			vim_mem_copy( terminal.terminal_id, terminal_id, TID_LEN );
			vim_mem_copy( terminal.merchant_id, merchant_id, MID_LEN );

			// RDD 020623 : If theres a letter change reset Comms and Set TMS required - this is really just for test
			if ((request_ptr->terminal_id[0] != terminal.terminal_id[0]) && (TERM_TEST_SIGNED))
			{
				pceft_debug(pceftpos_handle.instance, "v736 (test Signed) DIV Change -> TMS Required ");
				CommsSetup(VIM_TRUE);
				terminal_clear_status(ss_tms_configured);
			}

			// RDD 210312 - Special logon state created to prevent txns after lane change before terminal logged on with new config
			// Also delete any SAF Prints. It's safe to delete the whole SAF file here because we can't get to this point if Advices are pending so only prints are left...
			//saf_destroy_batch();

			// RDD 261012 - ensure that duplicate txn file is removed when changing TID (BD request)
			vim_file_delete(duplicate_txn_file_name);

			// RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
			terminal.last_logon_state = terminal.logon_state;
			terminal.logon_state = rsa_logon;

			// Change of TID may mean change in comms methods etc. but only in test
#if 0		// Causes more trouble than its worth!
			
			if (XXX_MODE)
			{
				display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Checking TMS Type...", "[XXX Mode Only]" );
			}
			CommsSetup(VIM_FALSE);
#endif
			pceft_pos_display_close();
		}
		else
		{
			DBG_MESSAGE( "NEW CATID/CAIC INVALID !" );
			DBG_DATA( terminal_id, 8 );
			DBG_DATA( merchant_id, 15 );

			terminal_clear_status( ss_tid_configured );
			terminal_clear_status( ss_tms_configured );

			return VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED;
		}
	}

	// RDD 150312 - think we only want to do a LFD logon if this is not being run from auto config
#if 0
	if( terminal.initator == it_pceftpos )
	{
		if(( ret = tms_contact( )) != VIM_ERROR_NONE )
			return ret;

	}
#endif
	terminal.initator = it_pceftpos;
	terminal_clear_status( ss_tms_configured );
	terminal_set_status( ss_tid_configured );
    vim_mem_copy(init.logon_response, "00", VIM_SIZEOF(init.logon_response));  //BRD need to return 00 not last logon response
	init.error = VIM_ERROR_NONE;
	DBG_POINT;
	terminal_save();
	VIM_ERROR_RETURN( VIM_ERROR_NONE );
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_tms_all
(
  VIM_UNUSED(VIM_PCEFTPOS_INITIALIZATION_REQUEST const *,request_ptr)

)
{
  VIM_ERROR_PTR ret;

    if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  // RDD 291211 - POS Initiated LFD so enable POS display
  txn_init();

  // RDD 161019 ensure that the temp dld file is deleted if TMS is not being run from power up
  vim_tms_reset();

  ret = tms_contact(VIM_TMS_DOWNLOAD_SOFTWARE,VIM_FALSE, web_then_vim);

  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);

  pceft_pos_display_close();
  VIM_ERROR_RETURN_ON_FAIL(ret);
  /* update idle timer */
  terminal.idle_time_count = 0;
  terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
  vim_rtc_get_uptime(&terminal.idle_start_time);
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_tms_parameters
(
  VIM_PCEFTPOS_INITIALIZATION_REQUEST const *request_ptr
)
{
  return VIM_ERROR_NOT_IMPLEMENTED;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_initialization_tms_software
(
  VIM_PCEFTPOS_INITIALIZATION_REQUEST const *request_ptr
)
{
  VIM_ERROR_PTR ret;

    if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

	/* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  // RDD 291211 - POS Initiated LFD so enable POS display
  txn_init();
  ret = tms_contact(VIM_TMS_DOWNLOAD_SOFTWARE, VIM_FALSE, web_then_vim );

  pceft_pos_display_close();
  VIM_ERROR_RETURN_ON_FAIL(ret);

  /* update idle timer */
  terminal.idle_time_count = 0;
  terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
  vim_rtc_get_uptime(&terminal.idle_start_time);

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_get_initialization_response
(
  VIM_ERROR_PTR error,
  /*@out@*/VIM_PCEFTPOS_INITIALIZATION_RESPONSE *response_ptr,
  VIM_CHAR sub_code
)
{
  VIM_RTC_TIME date;
  error_t error_data;

  DBG_POINT;

  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  vim_mem_set(response_ptr, ' ', VIM_SIZEOF(*response_ptr));

#if 0	// RDD - if this is called here then it can prevent other error msgs getting to the POS
  if( terminal.terminal_id[0] == ' ' || terminal.terminal_id[0] == 0x00 )
  {
    terminal.configured = VIM_FALSE;
    terminal_save();
    error = ERR_CONFIGURATION_REQUIRED;
  }
#endif
  /* Set Error Code */
  // VN 230718 JIRA WWBP-57 MO returned instead of 'W1' for 'Request File not Found'
  // RDD JIRA WWBP-295 System Error XX returned to Config merch cmd. Vyshakhs' fix broke this

  if( error == VIM_ERROR_TMS_FILESYSTEM )
  {
	  host_error_lookup( init.logon_response, &error_data );
  }
  else
  {
	  error_code_lookup( error, txn.host_rc_asc, &error_data );
  }

  if( TERM_EDP_ACTIVE )
  {

	  VIM_SIZE FileSize = 0;
	  VIM_FILE_HANDLE file_handle;
	  if (vim_file_open(&file_handle, "flash/Wally.data", VIM_TRUE, VIM_TRUE, VIM_TRUE) == VIM_ERROR_NONE)
	  {
		  FileSize = VIM_SIZEOF(TERM_BRAND);
		  pceft_debug(pceftpos_handle.instance, "Banner string added to wally.data successfully");
		  pceft_debug(pceftpos_handle.instance, TERM_BRAND);
		  vim_file_write(file_handle.instance, TERM_BRAND, FileSize);
		  vim_file_close(file_handle.instance);
	  }
	  else {
		  pceft_debug(pceftpos_handle.instance, "Failed to add Banner string into wally.data");
	  }
  }

  VIM_DBG_ERROR(error);

  vim_mem_copy(response_ptr->response_code, error_data.ascii_code, 2);
  vim_mem_copy(response_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(response_ptr->response_text),vim_strlen(error_data.pos_text)));

  if( sub_code == PCEFT_INITIALISATION_SUB_DIAGS )
  {
	  return VIM_ERROR_NONE;
  }

  /* merchant number */
  vim_mem_set(response_ptr->merchant_num, '0', VIM_SIZEOF(response_ptr->merchant_num));

  /* software version */
  bin_to_asc(terminal.file_version[0], response_ptr->software_version, VIM_SIZEOF(response_ptr->software_version));

  /* current datetime */
  vim_rtc_get_time(&date);
  vim_snprintf_unterminated(response_ptr->date,VIM_SIZEOF(response_ptr->date),"%02d%02d%02d",date.day,date.month,(date.year)%2000);
  vim_snprintf_unterminated(response_ptr->time,VIM_SIZEOF(response_ptr->time),"%02d%02d%02d",date.hour,date.minute,date.second);

  /* stan */
  bin_to_asc(init.stan, response_ptr->stan, VIM_SIZEOF(response_ptr->stan));

  /* bank code */
  response_ptr->bank_code = PCEFT_BANK_CODE;

  /* merchant name and address */
  vim_mem_copy(response_ptr->retailer_name, terminal.configuration.merchant_name, vim_strlen(terminal.configuration.merchant_name));
  vim_mem_copy(&response_ptr->retailer_name[VIM_SIZEOF(terminal.configuration.merchant_name)],
                terminal.configuration.merchant_address,
                VIM_SIZEOF(response_ptr->retailer_name) - VIM_SIZEOF(terminal.configuration.merchant_name));
  /* terminal id */
  vim_mem_copy(response_ptr->catid, terminal.terminal_id, VIM_SIZEOF(response_ptr->catid));

  /* merchant id */
  vim_mem_copy(response_ptr->caic, terminal.merchant_id, VIM_SIZEOF(response_ptr->caic));

  /* flag array */
  vim_mem_set(response_ptr->flag_array, '0', VIM_SIZEOF(response_ptr->flag_array));

  /* Eftpos transactions are allowed */
  response_ptr->flag_array[0] = '1';


  // RDD JIRA WWBP-292 this happens in D state to so move to a more generic point
  main_display_idle_screen("", "");



  /* return without error */
  return VIM_ERROR_NONE;
}



// RDD 120115 Speed up transactions by using more intelligent use of reader closure
VIM_BOOL CloseReader()
{
	//VIM_SIZE charge = 0;

	if( IsTerminalType( "VX820" ) )
#ifdef _DEBUG
		return VIM_TRUE;
#else
		return VIM_FALSE;
#endif
	//vim_terminal_get_battery_charge( &charge );

	// If battery more than half full then don't bother shutting down reader between transactions
	//if( charge > 50 )
	//	return VIM_FALSE;

	return VIM_TRUE;
}


//////////////////////////////// POST TRANSACTION  PROCESSING /////////////////////////////////
#if 0
VIM_ERROR_PTR vim_pceftpos_post_transaction_processing( void )
{
	if( !IsTerminalType( "VX820" ))
	{
		// RDD 201213 - full close required for vx690 in order to save battery life
		//if(( txn.card.type != card_mag ) && ( txn.card.type != card_chip ))
		if( txn.card.type == card_picc )
			picc_close( CloseReader() );
		DBG_MESSAGE( "******************PICC Disabled******************" );
	}
	else
	{
		// VX820 Keep reader open ready for next txn
		if( !terminal_get_status( ss_ctls_reader_open ))
		{
			VIM_ERROR_PTR res = VIM_ERROR_NONE;
			if(( res = picc_open( PICC_OPEN_ONLY, VIM_FALSE )) == VIM_ERROR_NONE )
			{
				pceft_debug_test( pceftpos_handle.instance, "CTLS Reopened OK" );
			}
			else
			{
				pceft_debug_error( pceftpos_handle.instance, "CTLS Reopen Failed", res );
			}
		}
	}
	return VIM_ERROR_NONE;
}

#else
VIM_ERROR_PTR vim_pceftpos_post_transaction_processing( void )
{
	// RDD 291217 get event deltas for logging
	vim_rtc_get_uptime( &txn.uEventDeltas.Fields.POSResp );


	// RDD 150518 Restore this line so reader closes after each txn on VX690
	if( IsTerminalType( "VX690" ))
	{
		picc_close( CloseReader(VIM_TRUE) );
	}
	//terminal.idle_time_count;		// RDD 160916 SPIRA:9051
    // RDD 100920 Trello #240 - no XML logging - because this is sent after txn NOT query card....

    if ((!WOW_NZ_PINPAD)&&( terminal.terminal_id[0] != 'L' ))
    {
        BuildXMLReport(&txn);
    }
    else
    {
        VIM_DBG_MESSAGE("------------- SKIP XML REPORT -------------");
    }
	DBG_POINT;
	//VIM_DBG_SEND_TARGETED_UDP_LOGS("vim_pceftpos_post_transaction_processing: Before event wait");
    // RDD - keep the current display for another second or so after POS message returned
    //vim_event_sleep(1500);
	//VIM_DBG_SEND_TARGETED_UDP_LOGS("vim_pceftpos_post_transaction_processing: After event wait");
	DBG_POINT;
	return VIM_ERROR_NONE;
}
#endif

//////////////////////////////// PROCESS TRANSACTION /////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_process_transaction
(
  VIM_CHAR sub_code,
  VIM_PCEFTPOS_TRANSACTION_REQUEST * request_ptr,
  VIM_PCEFTPOS_TRANSACTION_RESPONSE * response_ptr
)
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_CHAR buffer[VIM_SIZEOF(VIM_PCEFTPOS_TRANSACTION_REQUEST)+1];
  error_t error_data;

  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  vim_mem_clear( buffer, VIM_SIZEOF(buffer) );
  vim_mem_copy( buffer, request_ptr, VIM_SIZEOF(VIM_PCEFTPOS_TRANSACTION_REQUEST) );

  DBG_MESSAGE( "<<<<<<< PROCESS TRANSACTION >>>>>>>" );
  //pceft_debug(pceftpos_handle.instance, "EDPTiming: Received Transaction Request from POS");

  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  if ( txn.type == tt_func )
  {
    res = ERR_PP_BUSY;   //BD

    /* Set Error Code */
    vim_mem_clear(&error_data, VIM_SIZEOF(error_data));

    terminal_error_lookup(res, &error_data, VIM_TRUE);

    vim_mem_copy(response_ptr->response_code, error_data.ascii_code , 2);
    vim_mem_copy(response_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(response_ptr->response_text),vim_strlen(error_data.pos_text)));

    txn.error = res;
    res = pceft_build_txn_response( response_ptr, &txn );

    /* restore the operation initiator */
    set_operation_initiator(it_pinpad);

    terminal.idle_time_count = 0;
    VIM_ERROR_RETURN(res);
  }
  // RDD 220612 - set this by default else last training mode txn will be saved during txn init.
  terminal.training_mode = VIM_FALSE;

  // RDD 210212 - Initialise the txn from here
  txn_init();
/*
  if (terminal_get_status(ss_wow_basic_test_mode))
	  pceft_debug(pceftpos_handle.instance, "TXN Init Done");
*/

  /*pceft_process_txn(sub_code,request_ptr,VIM_FALSE);*/
  pceft_process_txn(sub_code, request_ptr, get_emv_query_flag());

  /* delete previous track2 */
  delete_card_last_track2();

  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);


  if( request_ptr->type == PCEFT_TXN_TYPE_MINI_TXN_HISTORY )
  {
	  txn.pos_cmd = request_ptr->type;
  }

  if (!get_emv_query_flag())
  {
	  res = pceft_build_txn_response( response_ptr, &txn );
  }
  // RDD 020113 - SPIRA:6482 PcEft display was not closed after completion with SAF full in NZ
  //if( !WOW_NZ_PINPAD )
  DBG_POINT;
#ifndef _XPRESS
	//pceft_pos_display_close();
#endif
  /* cause vim task is not working as we wanted */
  terminal.idle_time_count = 0;
  VIM_ERROR_RETURN(res);

  /* return without error */
}


///////////////////////////////////// CALLBACK: GET DUP RECEIPT ///////////////////////////////////////////
// This callback builds the response data for a call to re-send the last receipt as a duplicate
////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_get_dup_receipt
(
  /*@out@*/VIM_PCEFTPOS_DUP_RECEIPT_RESPONSE *resp
)
{
  VIM_ERROR_PTR ret = VIM_ERROR_NONE;
  error_t error_data;

  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

	vim_mem_set(resp, ' ', sizeof(*resp));

  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  if ( txn.type == tt_func ) ret = ERR_PP_BUSY;		//BD

  /* Set Error Code */
  vim_mem_clear(&error_data, VIM_SIZEOF(error_data));

  terminal_error_lookup(ret, &error_data, VIM_TRUE);

  vim_mem_copy(resp->response_code, error_data.ascii_code , 2);
  vim_mem_copy(resp->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(resp->response_text),vim_strlen(error_data.pos_text)));

  if (ret == VIM_ERROR_NONE)
  {
    if(( ret = txn_duplicate_txn_read(VIM_TRUE)) == VIM_ERROR_NONE )
    {
      ret = print_duplicate_receipt( &txn_duplicate );
    }
    else
    {
      ret = ERR_NO_DUPLICATE_RECEIPT;
    }

    error_code_lookup(ret, "", &error_data );
    vim_mem_copy(resp->response_code, error_data.ascii_code, VIM_SIZEOF(resp->response_code));
    vim_mem_copy(resp->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(resp->response_text),vim_strlen(error_data.pos_text)));
  }
  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);

  return VIM_ERROR_NONE;
}

///////////////////////////////////// CALLBACK: GET DUP TXN ///////////////////////////////////////////
// This callback builds the response data for a call to send back the details of the last transaction
////////////////////////////////////////////////////////////////////////////////////////////////////////////

VIM_ERROR_PTR vim_pceftpos_callback_get_dup_txn
(
  /*@out@*/VIM_PCEFTPOS_TRANSACTION_RESPONSE *resp
)
{
  VIM_ERROR_PTR ret;
  error_t error_data;

    if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  vim_mem_set(resp, ' ', sizeof(*resp));
  vim_mem_set(resp->cash_amount, '0', PCEFT_RESP_AMT_LEN );
  vim_mem_set(resp->purchase_amount, '0', PCEFT_RESP_AMT_LEN );
  vim_mem_set(resp->refund_amount, '0', PCEFT_RESP_AMT_LEN );

  /* save the operation initiator */
  set_operation_initiator(it_pceftpos);

  if(( ret = txn_duplicate_txn_read(VIM_TRUE)) == VIM_ERROR_NONE )
  {
      //ret = pceft_build_txn_response(resp, &txn);
      ret = pceft_build_txn_response(resp, &txn_duplicate);
	  VIM_DBG_ERROR(ret);
  }
  else
  {
	ret = ERR_NO_DUPLICATE_TXN;

	VIM_DBG_ERROR(ret);
	error_code_lookup(ret, "", &error_data );
	vim_mem_copy(resp->response_code, error_data.ascii_code, VIM_SIZEOF(resp->response_code));
	vim_mem_copy(resp->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(resp->response_text),vim_strlen(error_data.pos_text)));
  }

  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);

  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR pceft_journal_txn(journal_type_t type, transaction_t *transaction, VIM_BOOL FakeError)
{
  VIM_PCEFTPOS_JOURNAL jnl;
  //VIM_RTC_TIME current;
  //VIM_UINT8 settle_month;
  error_t error_data;
  VIM_CHAR card_name[21];
  VIM_ERROR_PTR ret = VIM_ERROR_NONE;

#ifdef _XPRESS
  return VIM_ERROR_NONE;
#endif

  if (FakeError)
  {
	  VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }


  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  VIM_ERROR_RETURN_ON_NULL(pceftpos_handle.instance);
  //pceft_debug_test(pceftpos_handle.instance, "EDPTiming: Journal to POSBuilding Journal");

  vim_mem_set(&jnl, ' ', VIM_SIZEOF(VIM_PCEFTPOS_JOURNAL));

  jnl.journal_type = type;

  if ((type == journal_type_txn) &&
      (transaction->error != VIM_ERROR_NONE) &&
      (transaction->error != ERR_SIGNATURE_APPROVED_OFFLINE) &&
      (transaction->error != ERR_EMV_CARD_APPROVED) &&
      (transaction->error != ERR_UNABLE_TO_GO_ONLINE_APPROVED))
    jnl.journal_type = journal_type_exception;

  /* VIM_CHAR acquirer; */
  jnl.acquirer = PCEFT_BANK_CODE;

  jnl.txn_subcode = transaction->pos_subcode;

  vim_mem_set(jnl.merchant, '0', VIM_SIZEOF(jnl.merchant));

  // RDD 220213 - Journal STAN needs to use original STAN if this is not a reversal journal but a reversal is pending
#if 0
  bin_to_asc(transaction->stan, jnl.stan, VIM_SIZEOF(jnl.stan));
#else
  if(( type != journal_type_reversal ) && ( reversal_present() ))
  {
	  bin_to_asc( transaction->orig_stan, jnl.stan, VIM_SIZEOF(jnl.stan));
  }
  else
  {
	  bin_to_asc( transaction->stan, jnl.stan, VIM_SIZEOF(jnl.stan));
  }
#endif

  bin_to_asc(transaction->time.day, jnl.bank_date_dd, VIM_SIZEOF(jnl.bank_date_dd));
  bin_to_asc(transaction->time.month, jnl.bank_date_mm, VIM_SIZEOF(jnl.bank_date_mm));

  bin_to_asc(transaction->time.year, jnl.bank_date_yy, VIM_SIZEOF(jnl.bank_date_yy));
  bin_to_asc(transaction->time.hour, jnl.bank_time_hh, VIM_SIZEOF(jnl.bank_time_hh));
  bin_to_asc(transaction->time.minute, jnl.bank_time_mm, VIM_SIZEOF(jnl.bank_time_mm));
  bin_to_asc(transaction->time.second, jnl.bank_time_ss, VIM_SIZEOF(jnl.bank_time_ss));

  /* Set Error Code */
  vim_mem_clear(&error_data, VIM_SIZEOF(error_data));
  error_code_lookup(transaction->error, transaction->host_rc_asc, &error_data);
  vim_mem_copy(jnl.response_code, error_data.ascii_code, VIM_SIZEOF(jnl.response_code));
  vim_mem_copy(jnl.response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(jnl.response_text),vim_strlen(error_data.pos_text)));

  /* TID & MID */
  vim_mem_copy(jnl.catid, terminal.terminal_id, VIM_SIZEOF(jnl.catid));
  vim_mem_copy(jnl.caic, terminal.merchant_id, VIM_SIZEOF(jnl.caic));

  /* Turnaround Time */
  bin_to_asc(transaction->turnaround, jnl.turnaround_time, VIM_SIZEOF(jnl.turnaround_time));

  vim_mem_copy(jnl.reference_number, transaction->pos_reference, VIM_MIN(vim_strlen(transaction->pos_reference), 16));

  bin_to_asc(transaction->amount_purchase, jnl.purchase_amount, VIM_SIZEOF(jnl.purchase_amount));
  bin_to_asc(transaction->amount_cash, jnl.cash_amount, VIM_SIZEOF(jnl.cash_amount));
  bin_to_asc(transaction->amount_refund, jnl.refund_amount, VIM_SIZEOF(jnl.refund_amount));
  bin_to_asc(transaction->amount_tip, jnl.tip_amount, VIM_SIZEOF(jnl.tip_amount));

  if (transaction->card.type != card_none)
  {
	  VIM_UINT32 CpatIndex = transaction->card_name_index;
	  VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
	  vim_snprintf_unterminated( jnl.card_bin, VIM_SIZEOF(jnl.card_bin),"%02d", CardNameTable[CardNameIndex].bin );

#if 1
	  // RDD 100712 SPIRA:5732 Journel Transaction PAN was in clear for Manually enterted cards
	  // RDD 060122 JIRA PIRPIN-1378 Require mask PAN in jounal for IGA
	  vim_mem_set(jnl.pan, ' ', VIM_SIZEOF(jnl.pan));
	  get_pci_card_data( jnl.pan, VIM_TRUE, transaction, VIM_FALSE );

#else
	  /* mask the track2 - Westpac style */
	  if (transaction->card.type == card_mag || transaction->card.type == card_chip)
	  {
		mask_out_track_details_txn(jnl.pan, 14, &transaction->card, bin);
	  }
	  else if (transaction->card.type == card_manual)
	  {
		vim_mem_copy(jnl.pan, transaction->card.data.manual.pan, transaction->card.data.manual.pan_length);
		jnl.pan[transaction->card.data.manual.pan_length] = '=';
		vim_mem_copy(&jnl.pan[transaction->card.data.manual.pan_length+1], transaction->card.data.manual.expiry_yy, 2);
		vim_mem_copy(&jnl.pan[transaction->card.data.manual.pan_length+3], transaction->card.data.manual.expiry_mm, 2);
	  }
#endif


	  // RDD 100812 SPIRA:5872 Set Default Index to Max so that we can max BIN range invalid if card data not captured before txn is cancelled
	  if( transaction->card_name_index < WOW_MAX_CPAT_ENTRIES )
	  {
		  /* RDD 231211 updated: Card Name and PCEFT Bin (mapped from WOW bin) */
		  vim_mem_clear(card_name, VIM_SIZEOF(card_name));
		  if( GetCardName( transaction, card_name ) == VIM_ERROR_NONE )
		  {
			  vim_mem_copy( jnl.card_description, card_name, VIM_MIN(VIM_SIZEOF(jnl.card_description),vim_strlen(card_name)));
			  DBG_MESSAGE( jnl.card_description );
		  }
		  else
		  {
			  DBG_MESSAGE( "GET CARD NAME FAILED" );
		  }
	  }
	  VIM_DBG_VAR(transaction->card_name_index);
	  VIM_DBG_STRING(card_name);

	  pceft_check_override_edp_tender(transaction, jnl.card_bin, jnl.card_description, sizeof(jnl.card_description));
  }

  vim_mem_copy( jnl.auth_id, transaction->auth_no, MIN(vim_strlen(transaction->auth_no), VIM_SIZEOF(jnl.auth_id) ));

  // SPIRA:8985 Settlement Date Issues - create a common function to handle this
#if 1
  SettlementDate( transaction, jnl.settlement_date_dd, jnl.settlement_date_mm );
#else
  bcd_to_asc(&transaction->settle_date[1], jnl.settlement_date_dd, 2);
  bcd_to_asc(&transaction->settle_date[0], jnl.settlement_date_mm, 2);
  vim_rtc_get_time(&current);
  settle_month = bcd_to_bin(&transaction->settle_date[0], 2);
  if ((current.month == 12) && (settle_month == 1)) /* Check if year is next year */
    current.year++;
  bin_to_asc(current.year, jnl.settlement_date_yy, 2);
#endif

  switch (transaction->account)
  {
    case account_cheque:
      jnl.account_type = PCEFT_CHEQUE_ACCOUNT;
      break;
    case account_savings:
      jnl.account_type = PCEFT_SAVING_ACCOUNT;
      break;
    case account_credit:
      jnl.account_type = PCEFT_CREDIT_ACCOUNT;
      break;
    case account_default:
	default:
      jnl.account_type = PCEFT_DEFAULT_ACCOUNT;
      break;
  }

  switch (transaction->type)
  {
    case tt_refund:
      jnl.txn_type = jnl_refund;
      break;
    case tt_cashout:
      jnl.txn_type = jnl_cash;
      break;
    //case tt_completion:
    //jnl.txn_type = jnl_completion;
    // break;
    case tt_preauth:
      jnl.txn_type = jnl_preauth;
      break;
    case tt_sale:
    case tt_sale_cash:
    default:
      jnl.txn_type = jnl_purchase;
      break;
  }

  // RDD 210213 SPIRA:6610
  if( type == journal_type_reversal )
  {
	  bin_to_asc(transaction->orig_stan, jnl.original_stan, VIM_SIZEOF(jnl.original_stan));
  }


  if (transaction_get_status(transaction, st_efb) ||
      transaction_get_status(transaction, st_offline))
    jnl.offline_flag = '1';
  else
    jnl.offline_flag = '0';

  /* these flags all unused atm. */
  jnl.receipt_print_flag = transaction->customer_receipt_printed ? '1' : '0';
  jnl.rfu_flag_1 = '0';
  jnl.comms_flag = '0';
  jnl.foreign_currency_flag = '0';
  jnl.preswipe_flag = '0';
  jnl.rfu_flag_2 = '0';
  jnl.rfu_flag_3 = '0';

  // RDD 301122 JIRA PIRPIN-2109 Send the journal - add a high level retry after a second ( low level retries should already be attempted in the vim layer )

  if(( ret = vim_pceftpos_journal(pceftpos_handle.instance, &jnl)) != VIM_ERROR_NONE)
  {
	  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2109 Journal Failed - retry");
	  vim_event_sleep(1000);
	  if ((ret = vim_pceftpos_journal(pceftpos_handle.instance, &jnl)) != VIM_ERROR_NONE)
	  {
		  pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2109 Journal Retry Failed");
		  ret = ERR_POS_COMMS_ERROR;
	  }
  }

  // Will be either VIM_ERROR_NONE or ERR_POS_COMMS_ERROR (X4)
  return ret;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR settle_pceftpos_jounal(VIM_PCEFTPOS_JOURNAL *journal)
{
  error_t error_data;
  VIM_RTC_TIME date_time;

    if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  // RDD 270212 - no journal for training txns
  if( terminal.training_mode ) return VIM_ERROR_NONE;

  vim_mem_set(journal, ' ', sizeof(VIM_PCEFTPOS_JOURNAL));

  switch (settle.type)
  {
    case tt_settle:
      journal->txn_subcode = '1';
      break;

    case tt_presettle:
      journal->txn_subcode = '2';
      break;

    case tt_last_settle:
      journal->txn_subcode = '3';
      break;
  }

  journal->journal_type = VIM_PCEFTPOS_JOURNAL_TYPE_SETTLEMENT;

  vim_mem_set(journal->merchant, '0', VIM_SIZEOF(journal->merchant));

  vim_mem_copy(journal->catid, terminal.terminal_id, 8);

  vim_mem_copy(journal->caic, terminal.merchant_id, 15);

  error_code_lookup(settle.error, settle.host_rc_asc, &error_data);

  vim_mem_copy(journal->response_text, error_data.pos_text, vim_strlen(error_data.pos_text));

  vim_mem_copy(journal->response_code, error_data.ascii_code, 2);

  vim_rtc_get_time(&date_time);

  bin_to_asc(date_time.day, journal->bank_date_dd, 2);

  bin_to_asc(date_time.month, journal->bank_date_mm, 2);

  date_time.year%=2000;
  bin_to_asc(date_time.year, journal->bank_date_yy, 2);

  bin_to_asc(date_time.hour, journal->bank_time_hh, 2);

  bin_to_asc(date_time.minute, journal->bank_time_mm, 2);

  bin_to_asc(date_time.second, journal->bank_time_ss, 2);

  /*bin_to_asc(settle.turnaround_time, journal->turnaround_time, sizeof(journal->turnaround_time));*/

 //VIM_DBG_VAR(settle.stan);

  bin_to_asc(settle.stan, journal->stan, sizeof(journal->stan));

  journal->acquirer = PCEFT_BANK_CODE;

  return vim_pceftpos_journal( pceftpos_handle.instance, journal );

 }
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_journal_logon(init_details_t *logon_data)
{
  error_t error_data;
  VIM_PCEFTPOS_JOURNAL jnl;
  VIM_RTC_TIME current;

  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

  VIM_ERROR_RETURN_ON_NULL(pceftpos_handle.instance);

  vim_mem_set(&jnl, ' ', VIM_SIZEOF(VIM_PCEFTPOS_JOURNAL));

  jnl.journal_type = journal_type_logon;
  jnl.txn_subcode = '0';

  vim_mem_set(jnl.merchant, '0', VIM_SIZEOF(jnl.merchant));

  jnl.acquirer = PCEFT_BANK_CODE;

  /* Set Error Code */
  vim_mem_clear(&error_data, VIM_SIZEOF(error_data));
  error_code_lookup(logon_data->error, logon_data->logon_response, &error_data);
  vim_mem_copy(jnl.response_code, error_data.ascii_code, VIM_SIZEOF(jnl.response_code));
  vim_mem_copy(jnl.response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(jnl.response_text),vim_strlen(error_data.pos_text)));

  /* TID & MID */
  vim_mem_copy(jnl.catid, terminal.terminal_id, VIM_SIZEOF(jnl.catid));
  vim_mem_copy(jnl.caic, terminal.merchant_id, VIM_SIZEOF(jnl.caic));

  vim_rtc_get_time(&current);
  vim_snprintf_unterminated(jnl.bank_date_dd, VIM_SIZEOF(jnl.bank_date_dd), "%02d", current.day);
  vim_snprintf_unterminated(jnl.bank_date_mm, VIM_SIZEOF(jnl.bank_date_mm), "%02d", current.month);
  vim_snprintf_unterminated(jnl.bank_date_yy, VIM_SIZEOF(jnl.bank_date_yy), "%02d", current.year);
  vim_snprintf_unterminated(jnl.bank_time_hh, VIM_SIZEOF(jnl.bank_time_hh), "%02d", current.hour);
  vim_snprintf_unterminated(jnl.bank_time_mm, VIM_SIZEOF(jnl.bank_time_mm), "%02d", current.minute);
  vim_snprintf_unterminated(jnl.bank_time_ss, VIM_SIZEOF(jnl.bank_time_ss), "%02d", current.second);

  bin_to_asc(logon_data->stan, jnl.stan, VIM_SIZEOF(jnl.stan));
  bin_to_asc(logon_data->turnaround_time, jnl.turnaround_time, VIM_SIZEOF(jnl.turnaround_time));

  /* ?? */
  /* jnl->txn_subcode = last_pos_cmd.sub_code; */

  if( pceftpos_handle.instance != VIM_NULL )
    return vim_pceftpos_journal(pceftpos_handle.instance,&jnl);
  else
    VIM_ERROR_RETURN( VIM_ERROR_NULL );
}

VIM_ERROR_PTR vim_pceftpos_callback_HeartBeatAlert(void)
{
	//static Pixel;
	static VIM_GRX_VECTOR offset;
	if (++offset.x > 20)
	{
		offset.x = 0;
		offset.y += 1;
	}
	vim_lcd_put_text_at_cell(offset, "*");
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_pceftpos_callback_build_query_transaction_response(VIM_PCEFTPOS_TRANSACTION_RESPONSE *resp)
{
  VIM_ERROR_RETURN_ON_FAIL(pceft_build_txn_response(resp, &txn));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/** Build the response to send back to the POS, invoked in powering up routine */
VIM_ERROR_PTR pceft_send_pos_power_fail
(
  /** result of the command */
  VIM_ERROR_PTR action_result,
  /** command code from the original POS command */
  VIM_CHAR cmd_code,
  /** command sub code from the original POS command */
  VIM_CHAR sub_code
)
{
  error_t error_data;
  struct error_resp_t{
    VIM_CHAR cmd_code;
    VIM_CHAR sub_code;
    VIM_CHAR device_code;
    VIM_CHAR response_code[2];
    VIM_CHAR response_text[20];
  };
  struct error_resp_t error_resp;
  VIM_PCEFTPOS_TRANSACTION_RESPONSE txn_response;
  VIM_CHAR temp_buffer[1000];
  /*
  struct
  {
    VIM_CHAR command;
    VIM_CHAR sub_code;
    VIM_CHAR device_code;
    VIM_CHAR host_id[8];
    VIM_CHAR SHA[8];
  } disconnect_request;
  */

//VIM_

  if( !is_integrated_mode() )
	  return VIM_ERROR_NONE;

 if( pceftpos_handle.instance == VIM_NULL )
  return VIM_ERROR_NONE;

  vim_mem_set(&error_resp, ' ', VIM_SIZEOF(error_resp));

  if( cmd_code == 'b' )
    error_resp.cmd_code = cmd_code;
  else
    error_resp.cmd_code = cmd_code+0x20;

  error_resp.sub_code = sub_code;
  vim_pceftpos_get_device_code(pceftpos_handle.instance, &error_resp.device_code);

#if 0  /* disable send command to GENPOS */
  /* try to close the host connection on GENPOS */
  disconnect_request.command = 'A';
  disconnect_request.sub_code = '0';
  disconnect_request.device_code = error_resp.device_code;
  vim_mem_copy( disconnect_request.host_id, terminal.host_id, 8 );
  vim_mem_set( disconnect_request.SHA, ' ', 8 );
  vim_pceftpos_send(pceftpos_handle.instance,&disconnect_request,VIM_SIZEOF(disconnect_request));
#endif

  /* Set Error Code */
  if ( cmd_code == 'M' )
  {
    /* transaction request */
    VIM_ERROR_RETURN_ON_FAIL(pceft_build_txn_response(&txn_response, &txn));
    vim_mem_set( temp_buffer, ' ', VIM_SIZEOF( temp_buffer ));
    vim_mem_copy( temp_buffer, &error_resp, 3 );
    vim_mem_copy( temp_buffer+3, &txn_response, VIM_SIZEOF( txn_response ));
    vim_pceftpos_send(pceftpos_handle.instance, temp_buffer,3+VIM_SIZEOF(txn_response));
  }
  else if (cmd_code == 'J')
  {
    VIM_PCEFTPOS_QUERY_CARD_RESPONSE query;
    vim_pceftpos_callback_build_query_response(&query);
    vim_mem_set( temp_buffer, ' ', VIM_SIZEOF( temp_buffer ));
    vim_mem_copy( temp_buffer, &error_resp, 3 );
    vim_mem_copy( temp_buffer+3, &query, VIM_SIZEOF( query ));
    vim_pceftpos_send(pceftpos_handle.instance, temp_buffer, 3+VIM_SIZEOF(query));
  }
  else
  {
    /* general error return */
	  	DBG_POINT;

    terminal_error_lookup(action_result, &error_data, VIM_TRUE);
    vim_mem_copy(error_resp.response_code, error_data.ascii_code, 2);
    vim_mem_copy(error_resp.response_text, error_data.pos_text, MIN(vim_strlen(error_data.pos_text), 20));
    vim_pceftpos_send(pceftpos_handle.instance, &error_resp, VIM_SIZEOF(error_resp));
  }
 //VIM_
  /* set flag for last cmd code to be sent */
  vim_pceftpos_update_last_cmd( VIM_FALSE );

  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR vim_pceftpos_callback_before_slave_mode( void )
{
	//VIM_ERROR_PTR error1,error3,error4;

	if (!is_integrated_mode())
		return ERR_TXN_NOT_SUPPORTED;

	//DBG_MESSAGE( "close devices for slave mode" );
	//error1 = vim_lcd_close();
	//error3 = vim_mag_destroy(mag_handle.instance_ptr);
	//vim_prn_close();
	//error4 = vim_icc_close(icc_handle.instance_ptr);
	//VIM_DBG_ERROR( error1 );
	//VIM_DBG_ERROR( error3 );
	//VIM_DBG_ERROR( error4 );

	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR vim_pceftpos_callback_after_slave_mode(void)
{
	//VIM_ERROR_PTR error1;

	if (!is_integrated_mode())
		return ERR_TXN_NOT_SUPPORTED;
	return VIM_ERROR_NONE;
}

#define DEFAULT_SLAVE_MODE_TIMEOUT		(3000)

VIM_SIZE SlaveModeTimeout = DEFAULT_SLAVE_MODE_TIMEOUT;
static VIM_SIZE SlaveModeEndTime;


VIM_BOOL IsSlaveMode(void)
{
    VIM_RTC_UPTIME CurrentTime;
    VIM_BOOL SlaveModeActive;

    vim_rtc_get_uptime(&CurrentTime);

    SlaveModeActive = ( CurrentTime > SlaveModeEndTime ) ? VIM_FALSE : VIM_TRUE;

    if (SlaveModeActive)
    {
        static VIM_SIZE SlaveModeLastSecs;
        VIM_SIZE SlaveModeSecs = (SlaveModeEndTime - CurrentTime) / 1000;
        if (SlaveModeSecs != SlaveModeLastSecs)
        {
            VIM_DBG_PRINTF1( "Slave Mode Ends in: %d", SlaveModeSecs );
            SlaveModeLastSecs = SlaveModeSecs;
        }
       // vim_pceftpos_set_slave( pceftpos_handle.instance );
    }
    else
    {
        vim_pceftpos_clear_slave( pceftpos_handle.instance );
    }

    return SlaveModeActive;
}

void SlaveModeResetTimer(VIM_SIZE SlaveModeTimeout)
{
    VIM_RTC_UPTIME CurrentTime;

    vim_rtc_get_uptime(&CurrentTime);
    SlaveModeEndTime = CurrentTime + SlaveModeTimeout;
}



VIM_ERROR_PTR vim_pceftpos_callback_slave_mode(    /** [in] Self PCEFTPOS Ptr */
    VIM_PCEFTPOS_PTR self_ptr,
    /** [in] Bank type of application */
    VIM_UINT8 bankcode,
    /** [in] Pointer to a uart instance connected to PCEFTPOS */
    VIM_UART_PTR uart_ptr,
    /** [in] Pointer to the UART parameters for the given UART PTR */
    VIM_UART_PARAMETERS const * uart_parameters,
    /** [in] Pointer to the protocol used to connect to PCEFTPOS */
    VIM_UART_PROTOCOL const * protocol_ptr,
    /** [in] Pointer to slave request message */
    void const * request_ptr,
    /** [in] Number of bytes in the request message */
    VIM_SIZE request_size
)
{
#if 1
    {
        terminal_initator_t backup;
        VIM_DBG_WARNING("SSSSSSSSSSSSSSSS ENTER SLAVE MODE SSSSSSSSSSSSSSSSS");

        // RDD - don't want any POS message being sent during slave mode
        backup = terminal.initator;
        terminal.initator = it_pinpad;

        slave_loop(self_ptr, bankcode, uart_ptr, uart_parameters, protocol_ptr, request_ptr, request_size, &SlaveModeTimeout);

        terminal.initator = backup;
        // Reset the local display timer to the new value as a new slave command came in
        SlaveModeResetTimer(SlaveModeTimeout);
        if (!SlaveModeTimeout)
        {
            // Call Idle Display on exit if slavbe mode timed out else it could be up to 10 secs before Idle display is updated
            VIM_DBG_PRINTF1("SSSSSSSSSSSSSSSS EXIT SLAVE MODE restore IDLE display Timeout %d ", SlaveModeTimeout );
            IdleDisplay();
        }
        else
        {
            VIM_DBG_PRINTF1("SSSSSSSSSSSSSSSS EXIT SLAVE MODE Keep SLAVE display Timeout %d ", SlaveModeTimeout );
        }
        return VIM_ERROR_NONE;
    }

#else
    VIM_ERROR_RETURN(VIM_ERROR_NOT_IMPLEMENTED);
#endif
}



