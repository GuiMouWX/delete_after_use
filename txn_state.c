#include <incs.h>

#include <txn_states.h>
VIM_DBG_SET_FILE("TY")
/*  Defines for State flows */
typedef enum state_flow_pos_t
{
  tf_nothing=0, 
  tf_sale=1, 
  tf_manual=2,
  tf_refund=3,
  tf_void=4,
  tf_completion=5,
  tf_cash_only=6,
  tf_preauth=7,
  tf_adjustment=8,
  tf_query_card = 9,
  tf_query_card_get_token = 10,
  tf_query_card_deposit = 11,  
  tf_preswipe = 12,
  tf_deposit = 13,
  tf_balance = 14,
  tf_mini_history = 15,
  tf_activate = 16,
  tf_preauth_enquiry = 17,
  tf_checkout = 18,
  tf_moto = 19,
  tf_query_card_openpay = 20,   // RDD 240620 Trello #167 OpenPay Implementation
  // new states here....
  tf_unknown = 21		// RDD 190916 ALways make this the last state
} state_flow_pos_t;  

typedef struct 
{
  /*@dependent@*/txn_state_t    *current;                    /*  The txn flow we're currently following */
  txn_state_t     state;                      /*  The current txn state */
  txn_state_t     history[MAX_TXN_FLOWS]; /*  List of previous states for current txn */
  VIM_UINT8     emv;                        /*  Indicates if the current flow is an EMV flow */
  VIM_UINT8     history_idx;                /*  Where we're currently up to in the history list */
  VIM_UINT8     state_idx;                  /*  The current index into the current txn flow */
} txnFlowInfo_t;

txnFlowInfo_t stateMachine;

/* ---------------------------------------------------------------------- */
txn_state_t state_set_state(txn_state_t state)
{
  VIM_UINT8 index = 0;

  /*  Store the current state in the history, unless this is the EMV Processing state */
  if (stateMachine.state != ts_emv_transaction)
  {
    stateMachine.history[stateMachine.history_idx] = stateMachine.state;

	// RDD 200512 SPIRA5539 : Better fix for history overflow issue - only increment history index if state has changed
	if( state_get_last() != stateMachine.state )
		stateMachine.history_idx++;
  }

  //VIM_DBG_VAR(stateMachine);

  // RDD 051016 v561-0 Noticed that crash can occur if too many retries of the same state ( eg. J8 bad-loyalty )
  if( *stateMachine.current > ts_exit )
  {
	  txn.error = VIM_ERROR_TIMEOUT;
	  stateMachine.state = ts_exit;
	  return stateMachine.state;
  }

  /*  Search through our flow from the start until we find the state we want */
  if (stateMachine.current != VIM_NULL)
  {
    do
    {
      /*  The first state is always COMPLETED so start history_index from 1. */
      stateMachine.state = stateMachine.current[++index];
      /*  If we get to the end without finding it, just return. */
      if (stateMachine.state == ts_exit)
        return stateMachine.state;
    } while (stateMachine.state != state);
  }
  return stateMachine.state;
}


/* ---------------------------------------------------------------- */
void state_set_flow(VIM_UINT8 index)
{
    //VIM_DBG_UINT8(index);
      // RDD 020118
    if (terminal.mode == st_standalone)
        stateMachine.current = txn_flow[index];
    else
  stateMachine.current = int_flow[index];
  stateMachine.emv = VIM_FALSE;
  stateMachine.state = ts_exit;
  stateMachine.history_idx = 0;
}

/* ---------------------------------------------------------------------- */
static VIM_UINT8 state_get_index(void)
{
  txn_state_t state;
  VIM_UINT8 index = 0;

  if (stateMachine.current != VIM_NULL)
  {
    do
    {
      state = stateMachine.current[index];
      if (state == stateMachine.state)
        return index;
    } while (!index++ || (state != ts_exit));
  }
  return 0;
}

/* ---------------------------------------------------------------- */
txn_state_t state_skip_state(void)
{
  /*  If its an emv flow, the next state may be EMV process, otherwise continue as normal */
  if (stateMachine.emv)
    state_set_state(ts_emv_transaction); 
  else if (stateMachine.current != VIM_NULL)  
    stateMachine.state = stateMachine.current[state_get_index()+1];

  return stateMachine.state;
}

/* ---------------------------------------------------------------- */
txn_state_t state_get_next(void)
{
  /*  If its an emv flow, the next state may be EMV process, otherwise continue as normal */
  if (stateMachine.emv) 
    state_set_state(ts_emv_transaction);
  else
  {
    stateMachine.history[stateMachine.history_idx] = stateMachine.state;
    stateMachine.history_idx++;
    state_skip_state();
  }		
  //VIM_DBG_DATA(&stateMachine.state, 1);
  return stateMachine.state;
}

/* ---------------------------------------------------------------- */
txn_state_t state_get_last(void)
{
  if (stateMachine.history_idx)
  {
    stateMachine.history_idx--;
    stateMachine.state = stateMachine.history[stateMachine.history_idx];
  }	
  return stateMachine.state;
}

/* ---------------------------------------------------------------------- */
txn_state_t state_get_current(void)
{
  return (stateMachine.state);
}

/* ---------------------------------------------------------------- */
VIM_ERROR_PTR determine_transaction_states(txn_type_t type)
{
  VIM_UINT8 txn_states;

  switch (type)
  {
    case tt_sale:
    case tt_sale_cash:
      txn_states = tf_sale;
      break;

    case tt_refund:
    case tt_gift_card_activation:
      txn_states = tf_refund;
      break;

    case tt_cashout:
      txn_states = tf_cash_only;
      break;

     case tt_completion:
      txn_states = tf_completion;
      break;
      
    case tt_query_card:
      txn_states = tf_query_card;
      break;

	case tt_query_card_get_token:
      txn_states = tf_query_card_get_token;
      break;

      // RDD 240620 Trello #167 OpenPay Implementation
    case tt_query_card_openpay:
        txn_states = tf_query_card_openpay;
        break;


	case tt_query_card_deposit:
      txn_states = tf_query_card_deposit;
      break;
            
	case tt_deposit:
	  txn_states = tf_deposit;
	  break;

	case tt_balance:
	  txn_states = tf_balance;
	  break;

	case tt_preswipe:
	  txn_states = tf_preswipe;
	  break;

	case tt_mini_history:
	  txn_states = tf_mini_history;
	  break;

	case tt_preauth:
	  txn_states = tf_preauth;
	  break;

	case tt_preauth_enquiry:
	  txn_states = tf_preauth_enquiry;
	  break;

	case tt_checkout:
		txn_states = tf_checkout;
		break;

	case tt_moto:
		txn_states = tf_moto;
		break;

	case tt_moto_refund:
		txn_states = tf_moto;
		break;


    case tt_void:
        txn_states = tf_void;
        break;

    default:
      txn_states = tf_nothing;	
	   VIM_DBG_MESSAGE("!!!! ERROR - UNKNOWN TRANSACTION TYPE !!!!" );
      break;
  }
  //VIM_DBG_UINT8(txn_states);
  if( txn_states >= tf_unknown )
  {
	  VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
  }
  else
  {
	state_set_flow( txn_states );
  }
  return (tf_nothing == txn_states) ? VIM_ERROR_UNDEFINED : VIM_ERROR_NONE;
}

void state_emv_fallback(void)
{
  stateMachine.emv = VIM_FALSE;
}

VIM_ERROR_PTR determine_tpi_transaction_type(VIM_TXN_TYPE type)
{
 //VIM_DBG_VAR(type);
  switch (type)
  {
    case VIM_TXN_TYPE_UNKNOWN:
    case VIM_TXN_TYPE_PURCHASE:
    case VIM_TXN_TYPE_PURCHASE_WITH_CASH:
    case VIM_TXN_TYPE_CASH:
    case VIM_TXN_TYPE_REFUND:
    case VIM_TXN_TYPE_TIP_ADJUSTMENT:
    case VIM_TXN_TYPE_PRE_AUTH:
    case VIM_TXN_TYPE_COMPLETION:
    case VIM_TXN_TYPE_VOID:
     //VIM_ 
      return VIM_ERROR_NONE;
    default:
     //VIM_ 
      return VIM_ERROR_UNDEFINED;
  }
}


