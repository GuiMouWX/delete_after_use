
#include "incs.h"
VIM_DBG_SET_FILE("TR");


// RDD: Process the response received from settlement message */
VIM_ERROR_PTR settlement_process_response(VIM_AS2805_MESSAGE *msg)
{
  VIM_UINT8 *Ptr;
  VIM_RTC_TIME date_time;

  // Get the current date/time because the settlement response doesn't provide the year!  
  vim_rtc_get_time(&date_time);

  // Save the response code  
  vim_mem_set(settle.host_rc_asc, '0', 2);

  settle.host_rc_bin = asc_to_bin(VIM_CHAR_PTR_FROM_UINT8_PTR(msg->fields[39]), 2);    
  vim_mem_copy(settle.host_rc_asc, msg->fields[39], 2);

/////////////////////// TEST TEST TEST TEST //////////////////////////
  //settle.host_rc_bin = 93;   
  //vim_strcpy( settle.host_rc_asc, "93" );
/////////////////////// TEST TEST TEST TEST //////////////////////////
        
  /* Save settlement date and month */
  vim_mem_copy(settle.settle_date, msg->fields[15], 2);

  /* Check Response code - return now if not approved */
  if( settle.host_rc_bin != RC00 )
  {
	settle.error = ERR_WOW_ERC;
    return ERR_WOW_ERC;  
  }

  settle.Credits = bcd_to_bin(msg->fields[74], 10);
  settle.CreditReversals = bcd_to_bin(msg->fields[75], 10);
  settle.Debits = bcd_to_bin(msg->fields[76], 10);
  settle.DebitReversals = bcd_to_bin(msg->fields[77], 10);

  settle.CreditTotalAmount = bcd_to_bin(msg->fields[86], 16);
  settle.CreditReversalsTotalAmount = bcd_to_bin(msg->fields[87], 16);
  settle.DebitTotalAmount = bcd_to_bin(msg->fields[88], 16);
  settle.DebitReversalsTotalAmount = bcd_to_bin(msg->fields[89], 16);

  Ptr = msg->fields[97];
  settle.NegativeTotal = (*Ptr++ == 'D') ? VIM_TRUE : VIM_FALSE;
  settle.AmountNetSettlement = bcd_to_bin(Ptr, 16);

  settle.CashOuts = bcd_to_bin(msg->fields[118], 10);
  settle.CashOutTotalAmount = bcd_to_bin(msg->fields[119], 16);

  // Field 48 : Deposit, Amex and Diners data
  Ptr = msg->fields[48];
  Ptr+=3; // skip the field length (3 bytes)
  vim_mem_copy( settle.DepositAndSchemeTotals.bytebuff, Ptr, sizeof(u_wow_field48_610resp_t) );

  // Field 63 : Print Text - optional field
  if(( Ptr = msg->fields[63] ) != VIM_NULL )
  {
	vim_mem_copy( settle.print_text, Ptr+2, bcd_to_bin( Ptr, 4 ) );
  }
  return VIM_ERROR_NONE;
}



// RDD: Process the Settlement Totals Retreival 
static VIM_ERROR_PTR settlement_process(VIM_CHAR *settle_text)
{ 
	VIM_AS2805_MESSAGE msg;

	//DBG_MESSAGE( "<<<<<<<<<< CLEAR INCOMPLETE STATUS NO POWER ON REVERSAL FROM THIS POINT >>>>>>>>>");
	//transaction_clear_status(&txn, st_incomplete);			    
	terminal_clear_status( ss_incomplete );	// RDD 161215 SPIRA:8855


	display_screen( DISP_WOW_PROCESSING, VIM_PCEFTPOS_KEY_NONE, settle_text );

	if ( !transaction_get_training_mode() )
	{
		if(( settle.error = msgAS2805_transaction(HOST_WOW, &msg, MESSAGE_WOW_600, &txn, VIM_FALSE, VIM_FALSE, VIM_TRUE )) == VIM_ERROR_NONE )
		{
			 settle.error = settlement_process_response( &msg );
		}	 
	}
	else
	{
		// TODO - what to print in training mode ?
	}
	return settle.error;
}

// RDD: Print the Settlement totals receipt
void print_settlement_receipt(VIM_PCEFTPOS_RECEIPT *receipt)
{
	receipt_build_settlement(0, receipt, (tt_presettle == settle.type));
	pceft_pos_print(receipt, VIM_TRUE);
}

void print_offline_totals_receipt(VIM_PCEFTPOS_RECEIPT *receipt)
{
	receipt_build_offline_totals( receipt );
	pceft_pos_print(receipt, VIM_TRUE);
}


// RDD: Order Settlement totals
VIM_ERROR_PTR settle_terminal( void )
{
  VIM_PCEFTPOS_RECEIPT receipt;   
  VIM_PCEFTPOS_JOURNAL journal;  
  //VIM_RTC_UPTIME now, min_display_time;
//  VIM_ERROR_PTR error;

  //error_t error_data;    

  VIM_CHAR *SettleText = ( settle.type == tt_presettle ) ? "Pre-Settlement" : "Settlement";

  if (settle.type == tt_settle_safs)
      SettleText = "SAF Report";

  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));
  settle.stan = terminal.stan;

  if( terminal.training_mode )
  {		
	  settle.error = VIM_ERROR_NONE;		
	  print_settlement_receipt(&receipt);
	  return settle.error;
  }

  if (settle.type == tt_settle_safs)
  {
        print_settlement_receipt(&receipt);
        return VIM_ERROR_NONE;
  }

#if 0
  // RDD 121114 added for pre-dial base connection checks
  if(( settle.error = EstablishNetwork( HOST_WOW, MESSAGE_WOW_600 )) != VIM_ERROR_NONE )
  		display_result( settle.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	  							
#endif
  //  Send a reversal and flush SAF  
  else if(( settle.error = ReversalAndSAFProcessing( MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )
  {
		VIM_ERROR_PTR res;
		if(( res = settlement_process( SettleText )) == VIM_ERROR_NONE )
		{
			// Remap the result ..
			res = ( settle.type == tt_presettle ) ? ERR_PRESETTLE_SUCCESSFUL : ERR_SETTLE_SUCCESSFUL;
		}

		settle.error = res;
		// Send the settlement totals request and process the host response

		/* display response text on the screen */ 
		// RDD 020415 SPIRA:8553 Display Settle Errors 93 = unavailable
#if 0
		display_result( settle.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	  							
#else
		display_result( settle.error, settle.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	  							
#endif
  }

  if(( settle.error == ERR_SETTLE_SUCCESSFUL )||( settle.error == ERR_PRESETTLE_SUCCESSFUL ))
  {
		// Print the receipt
		print_settlement_receipt(&receipt);
  }
  else if( settle.error != ERR_WOW_ERC )
  {
		// something went wrong with the settlement so print the offline totals receipt
		print_offline_totals_receipt(&receipt);
  }	  
    
  // RDD - remap settle error 
  if( settle.error == VIM_ERROR_NONE )
  {
	  settle.error = ERR_SETTLE_SUCCESSFUL;
  }
  settle_pceftpos_jounal(&journal);

  return settle.error;
}


