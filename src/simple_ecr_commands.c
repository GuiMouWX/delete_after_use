
#include "incs.h"
VIM_DBG_SET_FILE("TS");
#define SIMPLE_ECR_TOTALS_SUB_SETTLE             '1'
#define SIMPLE_ECR_TOTALS_SUB_PRE_SETTLE         '2'
#define SIMPLE_ECR_TOTALS_SUB_LAST_SETTLE        '3'
#define SIMPLE_ECR_TOTALS_SUB_SUBTOTALS          '4'
#define SIMPLE_ECR_TOTALS_SUB_SHIFTTOTALS_NO_RESET  '5'
#define SIMPLE_ECR_TOTALS_SUB_SHIFTTOTALS        '6'
#define SIMPLE_ECR_TOTALS_SUB_TXN_LISTING        '7'
#define SIMPLE_ECR_TOTALS_SUB_SAF_TOTALS         '8'
#define SIMPLE_ECR_TOTALS_SUB_STARTCASH          '9'
#define SIMPLE_ECR_TOTALS_SUB_DAILYCASH          'A'

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_simple_ecr_callback_initialization_bank
(
  VIM_UNUSED(VIM_SIMPLE_ECR_INITIALIZATION_REQUEST const *,request_ptr)
)
{
  if( simple_ecr_handle.instance == VIM_NULL )
    return VIM_ERROR_NONE;

  /* save the operation initiator */
  set_operation_initiator(it_simple_interface);

  /*
  vim_simple_ecr_display(simple_ecr_handle.instance, "INITIALIZATION","PLEASE WAIT",VIM_SIMPLE_ECR_GRAPHIC_PROCESSING);
   */

  /* release the previous connection */
  comms_disconnect();
   
  terminal.logon_state = session_key_logon;
  
  /* logon to exchange session key */
  logon_send(MESSAGE_WOW_800_NMIC_101);

  /* display result and print receipt */
  logon_finalisation(VIM_FALSE);

  /* close the modem */
  comms_disconnect();

  /* reset idle timer */   
  vim_rtc_get_uptime(&terminal.idle_start_time);
  
  /* restore the operation initiator */
  set_operation_initiator(it_pinpad);

  /* return without error */
  return init.error;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR simple_ecr_build_txn_response
(
  /*@out@*/VIM_SIMPLE_ECR_TRANSACTION_RESPONSE *resp, 
  transaction_t *transaction
)
{    
  VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR transaction_pre_process(VIM_CHAR sub_code, VIM_SIMPLE_ECR_TRANSACTION_REQUEST const * cmd)
{
  VIM_UINT8 cents;
  
  txn.amount_purchase = asc_to_bin(cmd->amount_purchase, sizeof(cmd->amount_purchase));
  txn.amount_refund = asc_to_bin(cmd->amount_refund, sizeof(cmd->amount_refund));
  txn.amount_cash = asc_to_bin(cmd->amount_cash, sizeof(cmd->amount_cash));
  txn.amount_total = txn.amount_purchase + txn.amount_cash;

  switch( cmd->transaction_type )
  {
    case 'P':  /* purchase */
      txn.type = tt_sale;
      break;
    case 'C':  /* cashout */
      txn.type = tt_cashout;
      break;
    case 'R':  /* refund */
      txn.type = tt_refund;
      break;
    default:
      VIM_ERROR_RETURN( VIM_ERROR_NOT_IMPLEMENTED );
  }
  
  /* If zero amounts reject the transaction */
  if( (txn.type ==  tt_sale) && (!txn.amount_purchase) && (!txn.amount_cash))
  {
    VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
  }

  /* Check if the transaction is a Purchase + cashout */
  if ((txn.amount_purchase) && (txn.amount_cash))
  {
    txn.type = tt_sale_cash;
  }
  /* Check if cash out only */
  if ((txn.amount_cash) && (!txn.amount_purchase))
  {
    txn.type = tt_cashout;
  }

  /* Need to makle sure cash out is in multiples of 5 cents only */
  if ((txn.type == tt_cashout) || (txn.type == tt_sale_cash))
  {
    cents = txn.amount_cash % 100;
    if ((cents % 5) != 0)
    {
      VIM_ERROR_RETURN(VIM_ERROR_POS_REQUEST_INVALID);
    }
  }  

  /* Set amount total to refund amount for refunds */
  if (txn.type == tt_refund)
  {
    txn.amount_total = txn.amount_refund;
  }
  
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static void simple_ecr_process_txn
(
  VIM_CHAR sub_code,
  VIM_SIMPLE_ECR_TRANSACTION_REQUEST const*request_ptr, 
  VIM_BOOL emv_preswipe_cont
)
{
  VIM_ERROR_PTR res;

  if (VIM_ERROR_NONE != (res = is_ready()))
  {
    txn.error = res;
    return;
  }
  
  /* Init the Txn structure */
  txn_init();
  
  txn.pos_subcode = sub_code;
  
  /* See if we have configuration */
  if (!terminal.configured)
  {
    txn.error = ERR_CONFIGURATION_REQUIRED;
    return;
  }

#if 1
  res = transaction_pre_process( sub_code, request_ptr );
  if( res != VIM_ERROR_NONE )
  {
    txn.error =res;
    return;
  }


  /* Txn Reference Number */
  vim_mem_copy(&txn.pos_reference, request_ptr->refenence_number, 3);
  
#endif

  txn_save();
  
 
  if( !emv_preswipe_cont )
  {
    /* save the operation initiator */
    set_operation_initiator(it_simple_interface);
    
    /* Now process the transaction */
    txn_process();
    
    /* restore the operation initiator */
    set_operation_initiator(it_pinpad);
  }
  else
  {
    txn.error = VIM_ERROR_NONE;
  }
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_simple_ecr_callback_process_transaction
(
  VIM_CHAR sub_code,
  VIM_SIMPLE_ECR_TRANSACTION_REQUEST const * request_ptr,
  VIM_SIMPLE_ECR_TRANSACTION_RESPONSE * response_ptr
)
{
  simple_ecr_process_txn(sub_code, request_ptr, VIM_FALSE);
  /* reset idle timer */   
  vim_rtc_get_uptime(&terminal.idle_start_time);

  VIM_ERROR_RETURN_ON_FAIL(simple_ecr_build_txn_response(response_ptr, &txn));
  
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_simple_ecr_callback_get_initialization_response
(
  VIM_ERROR_PTR error, 
  /*@out@*/VIM_SIMPLE_ECR_INITIALIZATION_RESPONSE *response_ptr
)
{
  /*
  VIM_RTC_TIME date;
  */
   error_t error_data;
  
   vim_mem_set(response_ptr, ' ', VIM_SIZEOF(*response_ptr));
  
   /* Set Error Code */
   error_code_lookup(error, init.logon_response, &error_data);
   vim_mem_copy(response_ptr->response_code, error_data.ascii_code, 2);
   vim_mem_copy(response_ptr->response_text, error_data.pos_text, VIM_MIN(VIM_SIZEOF(response_ptr->response_text),vim_strlen(error_data.pos_text)));

   if( error != VIM_ERROR_NONE )
   {
     vim_simple_ecr_display(simple_ecr_handle.instance,"INITIALIZE FAILED", error_data.pos_text,VIM_SIMPLE_ECR_GRAPHIC_PROCESSING);
   }
   else
   {
     vim_simple_ecr_display(simple_ecr_handle.instance,"INITIALIZE", error_data.pos_text,VIM_SIMPLE_ECR_GRAPHIC_PROCESSING);
   }
  
   /* return without error */
   return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_simple_ecr_process_totals(VIM_CHAR type)
{
  VIM_BOOL training_mode;

  settle_init();
  /* if not configured dont proceed */
  if (!terminal.configured)
  {
    settle.error = ERR_CONFIGURATION_REQUIRED;
    return settle.error;
  }
  /* Turn off training Mode */
  training_mode = VIM_FALSE;

  /* Now switch the type */
  switch (type)
  {
    /* todo: think about TRAINING MODE */
    case SIMPLE_ECR_TOTALS_SUB_PRE_SETTLE:              
      /* Pre Settlment request */
      settle.type = tt_presettle;
      txn.type = tt_settle;
      break;

    case SIMPLE_ECR_TOTALS_SUB_LAST_SETTLE:             
      /* Last Settlement request */
      settle.type = tt_last_settle;
      txn.type = tt_settle;
      break;
      
#if 0
    case PCEFT_TOTALS_SUB_SETTLE_TRAINING:         
      /* Settlement request - training mode */
      training_mode = VIM_TRUE;
    case PCEFT_TOTALS_SUB_SETTLE:                  
      /* Settlement request  */
      settle.type = tt_settle;
      txn.type = tt_settle;
      /* settle.display_index = D996_SETTLEMENT_PLEASE_WAIT; */
      break;


    case PCEFT_TOTALS_SUB_SHIFTTOTALS_TRAINING:    
      /* Training Mode Shift totals */
      /*todo:(DAC) what does this training mode var do?*/
      training_mode = VIM_TRUE;
    case PCEFT_TOTALS_SUB_SHIFTTOTALS:             
      /* print local shift totals - dont reset the totals */
      settle.type = tt_shift_totals;
      txn.type = tt_shift_totals;
      /*settle.error = shift_totals_print(VIM_TRUE, index);*/
      settle.error = shift_totals_process(VIM_TRUE);
      return VIM_ERROR_NONE;

    case PCEFT_TOTALS_SUB_SHIFTTOTALS_NO_RESET:    
      /* print local shift totals - reset the totals */
      settle.type = tt_shift_totals;
      txn.type = tt_shift_totals;
      /*settle.error = shift_totals_print(VIM_FALSE, index);*/
      settle.error = shift_totals_process(VIM_FALSE);
      return VIM_ERROR_NONE;
#endif

    default:
      settle.error = VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED; 
      return VIM_ERROR_NONE;
   }

   /* Set Training mode  */
   if ((settle.error = pceftpos_set_training_mode(training_mode)) == VIM_ERROR_NONE)
   {
       /* save the operation initiator */
       set_operation_initiator(it_simple_interface);
       /* settle terminal */
       settle_terminal();
       /* save the operation initiator */
       set_operation_initiator(it_pinpad);
   } 

  /* reset idle timer */   
  vim_rtc_get_uptime(&terminal.idle_start_time);

  return settle.error;
}
/*----------------------------------------------------------------------------*/
/* Build the settlement repsonse to be sent back to the PC */
VIM_ERROR_PTR vim_simple_ecr_callback_get_totals_response(VIM_CHAR *resp, VIM_SIZE * resp_len)
{
  error_t error_data;
  VIM_SIMPLE_ECR_TOTALS_RESPONSE *totals_response;
  
  vim_mem_set(resp, '0', VIM_SIZEOF(VIM_SIMPLE_ECR_TOTALS_RESPONSE));
  totals_response = (VIM_SIMPLE_ECR_TOTALS_RESPONSE *)resp;
  
  /* Set Error Code */
  error_code_lookup(settle.error, settle.host_rc_asc, &error_data);
  vim_mem_copy(totals_response->response_code, error_data.ascii_code, 2);
  vim_mem_set(totals_response->response_text, ' ', 20);
  vim_mem_copy(totals_response->response_text, error_data.pos_text, vim_strlen(error_data.pos_text));
  *resp_len = 2+vim_strlen(error_data.pos_text);

  if( settle.error != VIM_ERROR_NONE )
  {
    vim_simple_ecr_display(simple_ecr_handle.instance,"SETTLEMENT FAILED", error_data.pos_text,VIM_SIMPLE_ECR_GRAPHIC_PROCESSING);
  }
  else
  {
    vim_simple_ecr_display(simple_ecr_handle.instance,"SETTLEMENT", error_data.pos_text,VIM_SIMPLE_ECR_GRAPHIC_PROCESSING);
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_display(char const *line_1, char const *line_2)
{
  /* Now send the display command */
  if( simple_ecr_handle.instance != VIM_NULL )
    VIM_ERROR_RETURN_ON_FAIL(vim_simple_ecr_display(simple_ecr_handle.instance,line_1,line_2, VIM_SIMPLE_ECR_GRAPHIC_PROCESSING));

  return VIM_ERROR_NONE;
}

