#include "incs.h"
VIM_DBG_SET_FILE("TG");

/************** module wide globals ****************/
  
//VIM_TCU_HANDLE tcu_handle;
VIM_DES_BLOCK rn;

// msg creation time for WOW 220
static VIM_RTC_TIME msg_build_time;
file_version_record_t file_status[WOW_NUMBER_OF_FILES];
VIM_UINT8 active_file_id;   // RDD - todo : change the input args to as2805 functions to support to pass this down rather than making global

extern VIM_BOOL IsBGC(void);
extern VIM_BOOL IsEGC(void);

extern VIM_BOOL AllowFullEMVRefund(transaction_t *transaction);
extern card_bin_name_item_t CardNameTable[];
extern VIM_BOOL ApplySurcharge(transaction_t *transaction, VIM_BOOL Apply);
extern VIM_BOOL IsSDA(transaction_t *transaction);

VIM_UINT8 attempt;



// RDD 240720 Trello #193 Store the sent STAN to check against.
static VIM_UINT32 last_reversal_stan;


/**************************** NMIC 191 DE 48 *****************************/

VIM_ERROR_PTR wow_set_pkSP( VIM_UINT8 *data, VIM_UINT16 len )
{
      /* declare a pointer to a struct describing the expected format of DE48 */
      s_wow_field48_191resp_t *ptr = (s_wow_field48_191resp_t *)data;
      
      /* return an error of the field length is not as expected */
      if( len != VIM_SIZEOF(s_wow_field48_191resp_t) )
      {
          return VIM_ERROR_MISMATCH;
      }

      /* load the pkSP into the TCU */
      VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_pkSP(
        tcu_handle.instance_ptr,
        ptr->pkSPmod,
        PKSP_MOD_LEN,
        ptr->pkSPexp,
        PKSP_EXP_LEN));
  
      /* store the random number as we need this to generate the 192 security initialisation request */
      /* TODO - maybe need to store this in the SSA ? */
      vim_mem_copy_safe( (void *)&rn, (void const*)&ptr->wow_host_random_number, VIM_SIZEOF(rn) );

      // trash the last byte of the random number to invalidate the clear text on purpose
      //rn.bytes[7] ^= 0xFF;      
      return VIM_ERROR_NONE;
}




  /**************************** NMIC 192 DE 48 *****************************/

  VIM_ERROR_PTR wow_derive_kca_kia( VIM_UINT8 *data, VIM_UINT16 len )
  {
      VIM_UINT32 aiic_len;
      VIM_CHAR aiic_ascii[AIIC_LEN_ASCII+1];
  
      /* declare a pointer to a struct describing the expected format of DE48 */
      s_wow_field48_192resp_t *ptr = (s_wow_field48_192resp_t *)data;
  
	  vim_mem_set( aiic_ascii, '\0', VIM_SIZEOF(aiic_ascii) );

	  /* return an error of the field length is not as expected */
      if( len != VIM_SIZEOF(s_wow_field48_192resp_t) )
      {
          return VIM_ERROR_MISMATCH;
      }
	  /* convert the ascii aiic length field to binary */
	  aiic_len = asc_to_bin( (const char*)ptr->aiic_len_ascii, AIIC_LEN_FIELD );
  
      /* set the aiic to zeros by default */
      vim_mem_set( aiic_ascii, '0', AIIC_LEN_FIELD );
      
      /* convert the bcd aiic field to ascii */
      bcd_to_asc( ptr->aiic_bcd, aiic_ascii, aiic_len );

      if( vim_mem_compare( aiic_ascii, ACQUIRER , VIM_SIZEOF(ACQUIRER)-1 ) != VIM_ERROR_NONE )
      {
        VIM_DBG_MESSAGE( "ACQUIRER CHANGED, ++++++++++++++++++++++++++++++++" );
        VIM_DBG_STRING( aiic_ascii );

      }
      
      /* Set the acquirer */
      VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_acquirer(
        tcu_handle.instance_ptr,
        aiic_ascii));    
	  VIM_ERROR_RETURN_ON_FAIL( vim_tcu_load_KCA_and_KMACH( tcu_handle.instance_ptr, &ptr->eKIv44kca, &ptr->eKIv24kmach ));
      /* Generate acquirer initialisation keys from the KCA */
      VIM_ERROR_RETURN_ON_FAIL(vim_tcu_derive_KIA_and_KMACI_from_KCA(
        tcu_handle.instance_ptr));
            
      return VIM_ERROR_NONE;
  }
  
  

/**************************** NMIC 193 DE 48 *****************************/

VIM_ERROR_PTR wow_derive_kek( VIM_UINT8 *data, VIM_UINT16 len )
{  
    /* declare a pointer to a struct describing the expected format of DE48 */
    s_wow_field48_193resp_t *ptr = (s_wow_field48_193resp_t *)data;
    
    /* return an error of the field length is not as expected */
    if( len != VIM_SIZEOF(s_wow_field48_193resp_t) )
    {
        return VIM_ERROR_MISMATCH;
    }

    /* decrypt and store eKIAv88(PPASN) eKIA(TMK1) and eKIA(TMK2) */
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_load_KEK1_KEK2_and_PPASN_using_KIA(
       tcu_handle.instance_ptr,
       &ptr->eKIkek1,
       &ptr->eKIkek2,
       &ptr->eKIv88ppasn));

    /* verify the KVC */
    if(vim_tcu_verify_merchant_keys
    (
        tcu_handle.instance_ptr,
        &ptr->kvckek1,
        &ptr->kvckek2,
        &ptr->kvcppasn
    ) != VIM_ERROR_NONE )
	{
		return ERR_KVC_ERROR;
	}

    vim_mem_copy( &terminal.kek1kvc, &ptr->kvckek1, VIM_SIZEOF( VIM_DES_KVC ) );    
    return VIM_ERROR_NONE;
}  

/**************************** NMIC 170 DE 48 *****************************/


/************** module wide globals ****************/
/*  
VIM_TCU_HANDLE tcu_handle;
VIM_DES_BLOCK rn;
s_wow_file_status_t file_status[WOW_NUMBER_OF_FILES];
*/

// Define a function to fill out the file status structure with the data returned in the response to NMIC 170
VIM_ERROR_PTR wow_process_file_data( VIM_UINT8 *data )
{
   VIM_UINT8 n;

   /* declare a pointer to a struct describing the expected format of DE48 */
   s_wow_field48_170resp_t *ptr = (s_wow_field48_170resp_t *)data;

   vim_mem_copy( file_status[APP_FILE_IDX].return_code, ptr->RC_AppVer, WOW_FILE_RETURN_CODE_SIZE );
   vim_mem_copy( file_status[CPAT_FILE_IDX].return_code, ptr->RC_CPATVer, WOW_FILE_RETURN_CODE_SIZE );
   vim_mem_copy( file_status[PKT_FILE_IDX].return_code, ptr->RC_PKTVer, WOW_FILE_RETURN_CODE_SIZE );
   vim_mem_copy( file_status[EPAT_FILE_IDX].return_code, ptr->RC_EPATVer, WOW_FILE_RETURN_CODE_SIZE );
   vim_mem_copy( file_status[FCAT_FILE_IDX].return_code, ptr->RC_FCATVer, WOW_FILE_RETURN_CODE_SIZE );

   // RDD - reinitialise the STAN from that SET in field 48
   terminal.stan = bcd_to_bin( ptr->STAN, WOW_DE48_STAN_SIZE*2 );

   terminal_increment_stan();								//BRD must increment stan for first message
   // RDD 270720 Trello #193 - increment again for safety as sometimes last stan is not enough as results in 0220 being declined with same stan.
   terminal_increment_stan();
   // RDD 291112 SPIRA:6402 increment the STAN again if there is a reversal present, otherwise pending 220
   // can end up with the same STAN as the reversal as reversal STANs use old value
   if( reversal_present() )
   {
	 terminal_increment_stan();
   }
   // RDD 140113 SPIRA:6519 Need to set year from host at this point
   if(1)
   {         
	   VIM_RTC_TIME new_date;
    
	   /* read existing date to get year */      
	   vim_rtc_get_time(&new_date);
	   vim_utl_bcd_to_bin16( ptr->year, VIM_SIZEOF(ptr->year)*2, &new_date.year);

#ifdef _nnDEBUG	// RDD 2303 - test to unexpire expired cards while Donna on lunch...
	   new_date.year -= 4;
#endif

	   /* update rtc */      
	   vim_rtc_set_time(&new_date);
   }

   for( n=0; n<WOW_NUMBER_OF_FILES; n++ )
   {
        file_status[n].UpdateRequired = VIM_FALSE;
#ifndef _nDEBUG			
        if( !vim_strncmp( file_status[n].return_code, "ND", WOW_FILE_RETURN_CODE_SIZE ))
        {
            file_status[n].UpdateRequired = VIM_TRUE;
			// RDD 190412 - Report Software Invalid Error
			if( n==0 )
			{
				VIM_ERROR_RETURN( ERR_CNP_ERROR );
			}
            terminal.logon_state = file_update_required;
        }         
#endif
			//Use this line to force file download
            //terminal.logon_state = file_update_required; 
   }   
   return VIM_ERROR_NONE;
}



    
VIM_ERROR_PTR wow_load_session_keys( VIM_UINT8 *data, VIM_UINT16 len, VIM_CHAR KEKflag )
{      
    /* declare a pointer to a struct describing the expected format of DE48 */
    s_wow_field48_170resp_t *ptr = (s_wow_field48_170resp_t *)data;
    
    //vim_mem_copy(init.logon_response, "03", VIM_SIZEOF(init.logon_response));

    /* return an error if the field length is not as expected */
    if( len != VIM_SIZEOF(s_wow_field48_170resp_t) )
    {
		// The host won't return the session keys if there was an error in the proof.
        terminal.logon_state = rsa_logon;
        return ERR_WOW_ERC;
    }
    
    /* 
    ** decrypt and store all the session keys 
    ** 
    */
	// RDD 040512 SPIRA:5443
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_load_pci_session_keys(
         tcu_handle.instance_ptr,
         KEKflag,
         &ptr->eKEK1v24KMACr,
         &ptr->eKEK1v48KMACs,
         &ptr->eKEK1v22KDr,
         &ptr->eKEK1v44KDs,
         &ptr->eKEK1v28KPP,
         &ptr->eKEK1vEEKPci,
         ptr->PCIKeyIndex));

    if( vim_tcu_verify_session_keys(
        tcu_handle.instance_ptr,       
        &ptr->eKPP_kvc,
        &ptr->eKMACs_kvc,
        &ptr->eKMACr_kvc,
        &ptr->eKDs_kvc,
        &ptr->eKDr_kvc,
        &ptr->eKPci_kvc ) != VIM_ERROR_NONE )

	{
		return ERR_KVC_ERROR;
	}
    return VIM_ERROR_NONE;
}  


/**************************** NMIC 101 DE 48 *****************************/
  

    
VIM_ERROR_PTR wow_change_session_keys( VIM_UINT8 *data, VIM_UINT16 len, VIM_CHAR KEKflag )
{      
    /* declare a pointer to a struct describing the expected format of DE48 */
    s_wow_field48_101resp_t *ptr = (s_wow_field48_101resp_t *)data;
    
    /* return an error if the field length is not as expected */
    if( len != VIM_SIZEOF(s_wow_field48_101resp_t) )
    {
        return VIM_ERROR_MISMATCH;
    }

    /* 
    ** decrypt and store all the session keys 
    ** 
    */
    
    if( vim_tcu_load_pci_session_keys(
         tcu_handle.instance_ptr,
         KEKflag,
         &ptr->eKEK1v24KMACr,
         &ptr->eKEK1v48KMACs,
         &ptr->eKEK1v22KDr,
         &ptr->eKEK1v44KDs,
         &ptr->eKEK1v28KPP,
         &ptr->eKEK1vEEKPci,
         ptr->PCIKeyIndex) != VIM_ERROR_NONE )
	{
		return ERR_KVC_ERROR;
	}
    return VIM_ERROR_NONE;
}  


/*----------------------------------------------------------------------------*/


static VIM_ERROR_PTR msg_number_get
(
  VIM_UINT32* message_number_ptr,
  VIM_AS2805_MESSAGE_TYPE msg_type
)
{
  *message_number_ptr=0;

 ////VIM_DBG_VAR(msg_type);

  switch (msg_type)
  {

    case MESSAGE_WOW_100:
        *message_number_ptr=100;
        break;        

	case MESSAGE_WOW_200:
        // 260721 JIRA-PIRPIN 1118 VDA
    case MESSAGE_WOW_200_VDA:
    case MESSAGE_WOW_200_VDAR:
        *message_number_ptr=200;
        break;        

    case MESSAGE_WOW_220:
        *message_number_ptr=220;
        break;    

	case MESSAGE_WOW_221:
        *message_number_ptr=221;
        break;        
        
    case MESSAGE_WOW_300:
    case MESSAGE_WOW_300_NMIC_151:
        *message_number_ptr = 300;
        break;

    case MESSAGE_WOW_420:
        *message_number_ptr = 420;
        break;

	case MESSAGE_WOW_421:
        *message_number_ptr = 421;
        break;

    case MESSAGE_WOW_600:
    case MESSAGE_WOW_600_QC:
        *message_number_ptr = 600;
        break;

    case MESSAGE_WOW_800_NMIC_191:
    case MESSAGE_WOW_800_NMIC_191_M:
    case MESSAGE_WOW_800_NMIC_192:
    case MESSAGE_WOW_800_NMIC_193:
    case MESSAGE_WOW_800_NMIC_170:
    case MESSAGE_WOW_800_NMIC_101:
        *message_number_ptr = 800;
        break;
		
    default: 				
	  pceft_debug_error(pceftpos_handle.instance, "SOFTWARE ERROR: (Unknown Msg Type) MSGAS2805.C", VIM_ERROR_SYSTEM );
	  DBG_VAR( msg_type );
      VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);	// RDD TODO - Generate XC for invalid MSG Type
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_BOOL message_is_encrypted
(
  VIM_AS2805_MESSAGE_TYPE message_type
)
{
  switch (message_type)
  {
    case MESSAGE_WOW_100:
    case MESSAGE_WOW_200:
    case MESSAGE_WOW_200_VDA:
    case MESSAGE_WOW_200_VDAR:
    case MESSAGE_WOW_220:
    case MESSAGE_WOW_221:
    case MESSAGE_WOW_420:
    case MESSAGE_WOW_421:
    case MESSAGE_WOW_600:
    case MESSAGE_WOW_600_QC:
      return VIM_TRUE;
    default:
      return VIM_FALSE;
  }
}
/*----------------------------------------------------------------------------*/
VIM_BOOL message_has_mac
(
  VIM_AS2805_MESSAGE_TYPE message_type
)
{
  switch (message_type)
  {
    case MESSAGE_WOW_200:
    case MESSAGE_WOW_220:
    case MESSAGE_WOW_300_NMIC_151:
    case MESSAGE_WOW_600:
    case MESSAGE_WOW_600_QC:
      return VIM_TRUE;
    default:
      return VIM_FALSE;
  }
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_pan
(
  VIM_AS2805_MESSAGE *msg, 
  card_info_t *card,
  // RDD 260412 SPIRA:5346 Corrupt advice being sent after TOKEN lookup
  transaction_t *transaction
)
{
	VIM_CHAR ascii_pan[30];
	VIM_UINT8 bcd_pan[15];
	VIM_SIZE pan_len;

	// Exit if the card was not manually entered
	if(( card->type != card_manual )&&( transaction->type != tt_query_card_get_token ))
	{
		return VIM_ERROR_NONE;
	}
	VIM_ERROR_RETURN_ON_FAIL( card_get_pan( card, ascii_pan ) );
	VIM_DBG_VAR(card->type);

	pan_len = vim_strlen( ascii_pan );

	asc_to_bcd( ascii_pan, bcd_pan, pan_len );

    VIM_DBG_DATA( bcd_pan, VIM_SIZEOF(bcd_pan) );
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field_as_nibbles( msg, 2, bcd_pan, pan_len ) );

	/* return without error */  
	return VIM_ERROR_NONE;
 
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_processing_code
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{
    VIM_UINT8 prcode_bcd[3] = { 0x00,0x00,0x00 };
    VIM_BOOL fin_txn = VIM_TRUE;
	VIM_BOOL credit_acc = VIM_FALSE;
	txn_type_t type;

    // RDD 130421 added JIRA PIRPIN-1038 void support
	if(( transaction->type == tt_completion ) || (transaction->type == tt_void ))
		type = transaction->original_type;
	else
		type = transaction->type;
	
    // position 1 and 2 : transaction type
    switch( type )
    {


        case tt_sale:
        case tt_completion:
		case tt_moto:
        case tt_checkout:
            prcode_bcd[0] = 0x00;
        break;

		case tt_preauth:
		prcode_bcd[0] = 0x00;
		break;

		case tt_refund:
		//case tt_activate:
		case tt_moto_refund:
        case tt_gift_card_activation:
		credit_acc = VIM_TRUE;
		prcode_bcd[0] = 0x20;
		break;

        case tt_cashout:
        prcode_bcd[0] = 0x01;
        break;
        
        case tt_sale_cash:        
        prcode_bcd[0] = 0x09;
        break;
    
        case tt_balance:          
        prcode_bcd[0] = 0x31;
        break;

        case tt_settle: 
		case tt_presettle:
        fin_txn = VIM_FALSE;
		prcode_bcd[0] = ( settle.type == tt_presettle ) ? 0x97 : 0x96;
        break;
        
		case tt_query_card_get_token:
		fin_txn = VIM_FALSE;
		prcode_bcd[0] = 0x90;
		break;
        
        case tt_deposit:
            fin_txn = VIM_TRUE;
            credit_acc = VIM_TRUE;
            if ( terminal.card_name[transaction->card_name_index].CardNameIndex == NZ_XMAS_CLUB )       // RDD 280720 - Trello #120 NZ Christmas Club needs refund type
                prcode_bcd[0] = 0x20;
            else
                prcode_bcd[0] = 0x21;
            break;

        default:
		DBG_VAR(type);
        return VIM_ERROR_AS2805_FORMAT;
    }

	// RDD 090312 - ensure PICC txns are credit !
	// RDD SPIRA:6490 Problem with proc code for NZ 'S' type Deposits - looks like card.type was not being set properly for "S"
	if(( transaction->card.type == card_picc )&&( transaction->type != tt_deposit ))
	{
		credit_acc = (transaction->type == tt_refund) ? VIM_TRUE : VIM_FALSE;
	}

	// position 2 and 3 : transaction type
    if ((IsBGC() == VIM_TRUE) || (IsEGC() == VIM_TRUE))
    {
        transaction->account = account_savings;
    }
	
    if( fin_txn )
    {
        switch( transaction->account )
        {
            default:  
            case account_credit:
				{
	
					// RDD 010716 SPIRA:8973 UPI Card have 00 for Credit Account
#if 0				// RDD 180716 - this requirement as reversed
					VIM_UINT32 CpatIndex = transaction->card_name_index;
					VIM_UINT8 CardNameIndex = terminal.card_name[CpatIndex].CardNameIndex;
					if( CardNameIndex == UPI_CARD_INDEX )
					{
			            prcode_bcd[1] = 0x00;
						prcode_bcd[2] = 0x00;
					}
					else
#endif
					{
						prcode_bcd[1] = (credit_acc) ? 0x00:0x30;
						prcode_bcd[2] = (credit_acc) ? 0x30:0x00;
					}
				}				
				break;

            case account_savings:
            prcode_bcd[1] = (credit_acc) ? 0x00:0x10;
            prcode_bcd[2] = (credit_acc) ? 0x10:0x00;
            break;
        
            case account_cheque:        
            prcode_bcd[1] = (credit_acc) ? 0x00:0x20;
            prcode_bcd[2] = (credit_acc) ? 0x20:0x00;
            break;

        }
    }
   //VIM_DBG_VAR( prcode_bcd );

    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 3, prcode_bcd, VIM_SIZEOF(prcode_bcd)));

    /* return without error */
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_transaction_amount
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{
  VIM_UINT8 amount_bcd[6];
  VIM_UINT64 temp_total = transaction->amount_total;

#if 0
  // RDD 270921 v728 LIVE - no surcharge in DE4 for Mastercard. don't change the global structure as we need the total unchanged for receipts etc.
  if(( CardIndex(transaction) == MASTERCARD )&&( transaction->amount_surcharge ))
  {
      temp_total -= transaction->amount_surcharge;
  }
#endif

  VIM_DBG_PRINTF1("TTTTTTTTTTTTTTT Transaction Amount: %d", transaction->amount_total);

  bin_to_bcd( temp_total, amount_bcd, VIM_SIZEOF(amount_bcd)*2);

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 4, amount_bcd, VIM_SIZEOF(amount_bcd)));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_transmit_time( VIM_AS2805_MESSAGE *msg, transaction_t *transaction )
{
  VIM_UINT8 bcd_time_buf[5];

  /* Get msg build date/time */
  vim_utl_uint64_to_bcd(transaction->send_time.month ,&bcd_time_buf[0], 2);
  vim_utl_uint64_to_bcd(transaction->send_time.day   ,&bcd_time_buf[1], 2);
  vim_utl_uint64_to_bcd(transaction->send_time.hour  ,&bcd_time_buf[2], 2);
  vim_utl_uint64_to_bcd(transaction->send_time.minute,&bcd_time_buf[3], 2);
  vim_utl_uint64_to_bcd(transaction->send_time.second,&bcd_time_buf[4], 2);
  return vim_as2805_message_add_field(msg, 7, bcd_time_buf, VIM_SIZEOF(bcd_time_buf));
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR msg_add_time
(
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction
)
{
  VIM_UINT8 bcd_time_buf[3];
   
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.hour, &bcd_time_buf[0], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.minute, &bcd_time_buf[1], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.second, &bcd_time_buf[2], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 12, bcd_time_buf, VIM_SIZEOF(bcd_time_buf)));

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR msg_add_date
(
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction
)
{
  VIM_UINT8 bcd_date_buf[2];
    
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.month, &bcd_date_buf[0], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.day, &bcd_date_buf[1], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 13, bcd_date_buf, VIM_SIZEOF(bcd_date_buf)));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR terminal_increment_stan(void)
{
  /* Increment stan */
  if ( terminal.stan >= 999999l )
    terminal.stan = 1;
  else
    terminal.stan++;
  
  return( vim_file_set( STAN_FILE, (const void *)&terminal.stan, VIM_SIZEOF(terminal.stan) ) );
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_stan
(
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction
)
{
  VIM_UINT8 stan[3];

  // RDD Terminal Stan must be setup here for next message because it is used in Pinblock generation
  msg->stan = terminal.stan;


  VIM_ERROR_RETURN_ON_FAIL( vim_utl_uint32_to_bcd(msg->stan, stan, VIM_SIZEOF(stan)*2) );

  terminal_increment_stan();

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 11, stan, VIM_SIZEOF(stan)));

  transaction->stan = bcd_to_bin(stan, 6);

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_stan_original
(
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction
)
{
  VIM_UINT8 stan[3];

  msg->stan = transaction->stan;

  /* create an rrn from the stan */
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint32_to_bcd(transaction->stan, stan, VIM_SIZEOF(stan)*2));

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 11, stan, VIM_SIZEOF(stan)));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
extern VIM_UINT8 GetDigits(VIM_CHAR** Ptr);


static VIM_ERROR_PTR message_wow_add_rrn( VIM_AS2805_MESSAGE *msg, transaction_t *transaction )
{
  VIM_CHAR* Ptr = transaction->rrn;

  // RDD 050123 JIRA PIRPIN-1567v : If non numeric chars in RRN rebuild using Stan and time in format SSSSSSMMSSHH as prescribed by EFTPOS
  if (GetDigits(&Ptr) != VIM_SIZEOF(transaction->rrn))
  {
      vim_snprintf_unterminated(transaction->rrn, VIM_SIZEOF(transaction->rrn), "%6.6d%2.2d%2.2d%2.2d", transaction->stan, transaction->time.minute, transaction->time.second, transaction->time.hour);
  }

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 37, transaction->rrn, VIM_SIZEOF(transaction->rrn)));
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR msg_add_settle_date( VIM_AS2805_MESSAGE *msg, VIM_UINT8 *SettleDateMMDD )
{
	if( SettleDateMMDD[0] && SettleDateMMDD[1] )
		VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field( msg, 15, SettleDateMMDD, 2 ));
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR msg_add_card_expiry
(
  VIM_AS2805_MESSAGE *msg, 
  card_info_t *card,
// RDD 260412 SPIRA:5346 Corrupt advice being sent after TOKEN lookup
  transaction_t *transaction
)
{
  VIM_CHAR exp_mmyy_asc[5];
  VIM_UINT8 exp_yymm_bcd[2];
  
  if(( card->type != card_manual )&&( transaction->type != tt_query_card_get_token ))
  {
	  return VIM_ERROR_NONE;
  }
  else
  {	      
	VIM_ERROR_RETURN_ON_FAIL( card_get_expiry( &transaction->card, exp_mmyy_asc ));

    /* YYMM */
	// RDD 050413: SPIRA 6672 - expiry date incorrectly formatted in AS2805
#if 0
    asc_to_bcd( &exp_mmyy_asc[0], &exp_yymm_bcd[0], 2);
    asc_to_bcd( &exp_mmyy_asc[2], &exp_yymm_bcd[1], 2);
#else
    asc_to_bcd( &exp_mmyy_asc[0], &exp_yymm_bcd[1], 2);
    asc_to_bcd( &exp_mmyy_asc[2], &exp_yymm_bcd[0], 2);
#endif
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field( msg, 14, exp_yymm_bcd, 2 ));
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_pose
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{
  VIM_UINT8 pose_buf[2];
  VIM_UINT16 pose = 0;
 //VIM_DBG_VAR(transaction->card.type);
  switch (transaction->card.type)
  {
    case card_manual:
        if (IsBGC())
        {
            // 041219 Change back to 31 as we now have PIN entry for BGC
            pose = 31;
        }
        else
        {
            pose = 11;
        }
        break;

    case card_manual_pos:
		 
      pose = 12;
      break;

    case card_mag:
		// RDD 240113 SPIRA:6554 - No tech fallback flag if forced swipe due to scheme fallback
		if( transaction_get_status( transaction, st_scheme_fallback ))
		{
			DBG_MESSAGE( "$$$$$$$$$$$$$ Scheme Fallback $$$$$$$$$$$$$$" );
			// RDD 041016 SPIRA:9055 - special for UPI they insist it should be 621 for scheme fallback
			if( IsUPI( transaction ))
				pose = 621;
			else
				pose = 21;      
		}
		else if( transaction->card.data.mag.emv_fallback )
		{
			DBG_MESSAGE( "$$$$$$$$$$$$$ Technical Fallback $$$$$$$$$$$$$$" );
			pose = 621;
		}
		else
		{
			DBG_MESSAGE( "$$$$$$$$$$$$$ Normal Magswipe $$$$$$$$$$$$$$" );
	        pose = 21;      
		}
		break;


    case card_chip:
      pose = 51;
      break;

    case card_picc:
      if (transaction->card.data.picc.msd)
        pose = 911;
      else
        pose = 71;
      break;

    default:
		DBG_VAR(pose);			  
		pceft_debug_error(pceftpos_handle.instance, "SOFTWARE ERROR: INVALID POSE: ABORT TXN", VIM_ERROR_SYSTEM );    
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);      
  }
  
#if 0  
  /* RDD 270711 - additional rule for debit cards processed as mag */
  if(( transaction->account == account_savings )||( transaction->account == account_cheque))
  {
    pose = 051;
  }
#endif

  VIM_DBG_NUMBER( pose );

  // RDD 290920 v718 Trello #281
  transaction->sw_fem = pose;

  bin_to_bcd ((VIM_UINT64)pose, pose_buf, VIM_SIZEOF(pose_buf)*2);
  VIM_DBG_DATA( pose_buf, 2 );
  
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 22, pose_buf, VIM_SIZEOF(pose_buf)));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_card_sequence_no
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{
  VIM_TLV_LIST tag_list;

  // Setup default value so zeros are written even if no data.
  VIM_UINT8 buf[2] = { 0x00,0x00 };
  // RDD 230212 SPIRA ISSUE 4815 : This tag needs to be added whenever available
#if 0
  if ((card_chip == transaction->card.type || card_picc == transaction->card.type) &&
      !transaction_get_status(transaction, st_partial_emv))
#else
  if ( (card_chip == transaction->card.type) || (card_picc == transaction->card.type) )
#endif
  {
	  // RDD 161214 v539 SPIRA:8279 Now EPAL are saying they do need this even if it's zero....
#if 0
	  // RDD 140714 v430 SPIRA:7856 No DE23 required for EPAL cards (From DG) so exit here if epal and rest of old code not affected
	  if( is_EPAL_cards( transaction ))
	  {
		  DBG_POINT;
		  return VIM_ERROR_NONE;
	  }
#endif
	 // RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data

    VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open(&tag_list, transaction->emv_data.buffer, transaction->emv_data.data_size, sizeof(transaction->emv_data.buffer), VIM_FALSE ));		
    
	DBG_DATA( transaction->emv_data.buffer, transaction->emv_data.data_size );

    if (VIM_ERROR_NONE == vim_tlv_search( &tag_list, VIM_TAG_EMV_APPLICATION_PAN_SEQUENCE_NUMBER ))
    {
		DBG_VAR( tag_list.current_item.length );
		// RDD 271113 v422 SPIRA:7083 Only make this special for EPAL cards
		// RDD 120315 v541 SPIRA:8470 OP-1 TAV15 requires that we DON'T Send DE23 if it's not present. I think the kernel returns this tag with zero length of not returned from the card during AEF
	  if(/* ( is_EPAL_cards( transaction )) ||*/ (  tag_list.current_item.length != 0 ))
      //if( tag_list.current_item.length != 0 )
	  // RDD 070813 SPIRA:6770 EFTPOS TAV cards require DE23 to have some contents - load zeros if no tag data
 	  //if(1)	
      {
        vim_mem_clear(buf, VIM_SIZEOF(buf));
        vim_tlv_get_bytes(&tag_list, &buf[1], 1);
	    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 23, buf, VIM_SIZEOF(buf)));
      }
    }
  }
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_posc
(
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction
)
{
  VIM_UINT8 posc_bcd = IS_STANDALONE ? 0x42 : 0x04;
  if(( transaction->type == tt_moto )||( transaction->type == tt_moto_refund )) 
	  posc_bcd = 0x08;
  else if (transaction_get_status(transaction, st_moto_txn))
  {
      posc_bcd = 0x08;
  }

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 25, &posc_bcd, VIM_SIZEOF(posc_bcd)));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

static VIM_BOOL ValidT2(VIM_UINT8* T2_bcd, VIM_SIZE len )
{
    VIM_SIZE n;
    VIM_CHAR T2_Asc[MAX_TRACK2_LENGTH + 1] = { 0 };
    VIM_CHAR* Ptr = T2_Asc;

    bcd_to_asc(T2_bcd, T2_Asc, len);
    for (n = 0; n < MIN_PAN_LENGTH; n++, Ptr++)
    {
        if (!VIM_ISDIGIT(*Ptr))
        {
            return VIM_FALSE;
        }
    }
    return VIM_TRUE;
}

static VIM_ERROR_PTR msg_add_track_2
(
  VIM_AS2805_MESSAGE *msg, 
  card_info_t *card
)
{
  VIM_UINT8 track2_bcd[30];
  VIM_SIZE T2_len = 0;

  vim_mem_clear(track2_bcd, sizeof(track2_bcd));

  switch (card->type)
  {     
    case card_mag:
        T2_len = card->data.mag.track2_length;
        asc_to_bcd(card->data.mag.track2, track2_bcd, T2_len);
        break;

    case card_chip:
        T2_len = card->data.chip.track2_length;
        asc_to_bcd(card->data.chip.track2, track2_bcd, T2_len);
        break;

    case card_picc:
        T2_len = card->data.picc.track2_length;
        asc_to_bcd(card->data.picc.track2, track2_bcd, T2_len);
        break;

    /* RDD 27/07/11 - remove error as we'll now come here for manual entry */
  default:
    return VIM_ERROR_NONE;
  }

  // RDD 2908 JIRA PIRPIN-2661 Malformed reversal gets stuck and blocks transactions
  if (ValidT2(track2_bcd, T2_len))
  {
      VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field_as_nibbles(msg, 35, track2_bcd, T2_len));
  }
  else
  {
      pceft_debugf(pceftpos_handle.instance, "JIRA-PIRPIN 2661: Invalid Track2 Data %4.4d MSG Not Sent", msg->msg_type);
      VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
  }

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_auth_id
(
  VIM_AS2805_MESSAGE *msg, 
  VIM_TEXT const* auth_no_ptr
)
{
  char auth_buf[6];

  vim_mem_set(auth_buf, ' ', sizeof(auth_buf));

  vim_mem_copy(auth_buf, auth_no_ptr, MIN(vim_strlen(auth_no_ptr), VIM_SIZEOF(auth_buf)));

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 38, auth_buf, VIM_SIZEOF(auth_buf)));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_catid
(
  VIM_AS2805_MESSAGE *msg
)
{
    // RDD 2908 JIRA PIRPIN-2661 Malformed reversal gets stuck and blocks transactions 
    if (terminal.terminal_id[0] > 'Z' || terminal.terminal_id[0] < 'A')
    {
        pceft_debugf(pceftpos_handle.instance, "JIRA-PIRPIN 2661: Invalid TID Data %4.4d MSG Not Sent", msg->msg_type);
        VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
    }
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 41, terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id)));
    /* return without error */
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_caic( VIM_AS2805_MESSAGE *msg )
{
    VIM_DBG_DATA(terminal.merchant_id, 15);
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 42, terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id)));
/* return without error */
return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR msg_add_adp_191( VIM_AS2805_MESSAGE *msg )
{
    VIM_SIZE cryptogram_size;
    u_wow_field48_191req_t u_private_data;
    
     //VIM_DBG_MESSAGE("<<<< NMIC 191: RSA INITIALISATION >>>>");

    /* 0x03 denotes terminal uses 3DES */
    u_private_data.fields.KeyMgmtVer = 0x03;

    /* get skMAN(pkTCU) and store it in the private data structure */
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_skMAN_pkTCU(
      tcu_handle.instance_ptr,
      &u_private_data.fields.skMAN_pkTCU,
      &cryptogram_size,
      SKMAN_PKTCU_LEN));

	VIM_DBG_NUMBER( cryptogram_size );
	VIM_DBG_NUMBER( SKMAN_PKTCU_LEN );

    /* Verify the size of the returned skMAN(pkTCU) */
    VIM_ERROR_RETURN_ON_MISMATCH(cryptogram_size,SKMAN_PKTCU_LEN);

    /* get the PPID and store it in the private data field */
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_ppid(
    tcu_handle.instance_ptr,
    &u_private_data.fields.PPID));

	// TEST TEST TEST PPID
    VIM_DBG_VAR( u_private_data.fields.PPID );   
    // TEST TEST TEST PPID

	// RDD 100418 JIRA WWBP-206 - add HW type to DE48 ( eg. VX820, VX690, VX680 )
	{
		VIM_CHAR part_number[100];
		vim_terminal_get_part_number(part_number, VIM_SIZEOF(part_number));
		vim_mem_set(&u_private_data.fields.cHWType, ' ', VIM_SIZEOF(u_private_data.fields.cHWType));
		vim_mem_copy(&u_private_data.fields.cHWType, part_number, 5);	// Just use first 5 bytes
	}
            
    /* pack the private data (field 48 into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, &u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff)));
    
    /* return without error */
    return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR msg_add_adp_192( VIM_AS2805_MESSAGE *msg )
{
    VIM_RTC_TIME dts;
    VIM_SIZE cryptogram_size;

    u_wow_field48_192req_t u_private_data;
    
     //VIM_DBG_MESSAGE("<<<< NMIC 192: SECURITY INITIALISATION >>>>");

    /* 0x03 denotes terminal uses 3DES */
    u_private_data.fields.KeyMgmtVer = 0x03;

    /* get date and time */
    vim_rtc_get_time(&dts);

    /* get the TCU signature and store it in the private data field*/
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_skTCU_pkSP_KI(
      tcu_handle.instance_ptr,
      u_private_data.fields.skTCU_pkSP_KI,
      &cryptogram_size,
      SKTCU_PKSP_KI_MAX_LEN,
      &dts,
      (VIM_DES_BLOCK *)&rn));

    /* get the ppid from the tcu */
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_ppid(
      tcu_handle.instance_ptr,
      &u_private_data.fields.PPID));

    /* pack the private data (field 48 into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, &u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff)));


    return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR msg_add_adp_193( VIM_AS2805_MESSAGE *msg )
{
    u_wow_field48_193req_t u_private_data;

    vim_mem_clear( u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff) );

     //VIM_DBG_MESSAGE("<<<< NMIC 193: KEK INITIALISATION REQ >>>>");

    /* 0x03 denotes terminal uses 3DES */
    u_private_data.fields.KeyMgmtVer = 0x03;

    /* send acquirer proof eKIA(PPID) */
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_eKIA_PPID(
    tcu_handle.instance_ptr,
    &u_private_data.fields.eKIA_PPID));

	vim_mem_clear( &u_private_data.bytebuff[5], 4 );
    
    /* send plaintext PPID */
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid(
    tcu_handle.instance_ptr, 
    &u_private_data.fields.PPID));   
    
    /* pack the private data (field 48 into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, &u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff)));

    return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR msg_add_adp_170( VIM_AS2805_MESSAGE *msg )
{
    VIM_UINT8 n;
    VIM_DES_BLOCK eKIaPPID;
    WOW_FILE_VER_T *ptr;
    u_wow_field48_170req_t u_private_data;

     //VIM_DBG_MESSAGE("<<<< NMIC 170: NORMAL SIGN ON >>>>");

    /* Clear the output bcd buffer */
    vim_mem_clear( u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff) );

	// RDD 171114 - probably some oversight in the spec to add an extra field length here

    /* send plaintext PPID */
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid(
    tcu_handle.instance_ptr, 
    &u_private_data.fields.PPID));    

    /* 0x03 denotes terminal uses 3DES */
    u_private_data.fields.KeyMgmtVer = 0x03;

// TEST TEST TEST
//    terminal.file_version[1] -= 1;     // force CPAT download
// TEST TEST TEST    
    
    /* get the current file versions - initialise the ptr to the start of the file info */  
    for( n=0, ptr=&u_private_data.fields.FileVersions[0]; n< WOW_NUMBER_OF_FILES; n++,ptr++ )
    {    
        bin_to_bcd( (VIM_UINT32)terminal.file_version[n], (VIM_UINT8 *)ptr, WOW_FILE_VER_BCD_LEN*2 );
    }
    /* Get the PPID encrypted under Acquirer Master Key */
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_eKIA_PPID(
      tcu_handle.instance_ptr,
      &eKIaPPID));

    /* WOW host only wants left hand 4 bytes of result */
    vim_mem_copy( (void *)&u_private_data.fields.ePedAuthority, (const void *)&eKIaPPID, sizeof(VIM_TCU_PROOF) );
      
    /* Get the PPASN encrypted under KEK1 (new tcu fn) */    
    VIM_ERROR_IGNORE(vim_tcu_get_ekek1_ppasn(
    tcu_handle.instance_ptr,
    &u_private_data.fields.eKEK1ppasn));
      
    /* Get the PPASN encrypted under KEK2 (new tcu fn) */    
    VIM_ERROR_IGNORE(vim_tcu_get_ekek2_ppasn(
    tcu_handle.instance_ptr,
    &u_private_data.fields.eKEK2ppasn));
	
    /* pack the private data (field 48) into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, &u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff)));
   
    return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR msg_add_adp_101( VIM_AS2805_MESSAGE *msg )
{
    u_wow_field48_101req_t u_private_data;

     //VIM_DBG_MESSAGE("<<<< NMIC 101: SESSION KEY CHANGE REQ >>>>");

    vim_mem_clear( u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff) );

    /* send plaintext PPID */
    VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid(
    tcu_handle.instance_ptr, 
    &u_private_data.fields.PPID));    

    /* 0x03 denotes terminal uses 3DES */
    u_private_data.fields.KeyMgmtVer = 0x03;

    /* Get the PPASN encrypted under KEK1 (new tcu fn) */    
    VIM_ERROR_IGNORE(vim_tcu_get_ekek1_ppasn(
    tcu_handle.instance_ptr,
    &u_private_data.fields.eKEK1ppasn));
      
    /* Get the PPASN encrypted under KEK2 (new tcu fn) */    
    VIM_ERROR_IGNORE(vim_tcu_get_ekek2_ppasn(
    tcu_handle.instance_ptr,
    &u_private_data.fields.eKEK2ppasn));

    /* pack the private data (field 48) into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, &u_private_data.bytebuff, VIM_SIZEOF(u_private_data.bytebuff)));
      
    return VIM_ERROR_NONE;
}

/*
(001)Extended Bit Map              :\x06\x00\x00 \x08\x00\x00\x01
(011)S T A N                       :000012
(032)A I I C                       :0062800000F
(041)Card Acceptor Term ID         :W2774026
(042)Card Acceptor ID Code         :611000604002774
(048)Additional Private Data       :\x00\x01
(070)Network Manage Info Code      :151
(071)Message Number                :0001
(091)File Update Code              :4
(101)File Name                     :CPAT
(128)Msg Authentication Code       :\xE9\x95\xB9\xBF\x00\x00\x00\x00
*/

static VIM_ERROR_PTR msg_add_adp_151
(
  VIM_AS2805_MESSAGE *msg
)
{
    VIM_CHAR adp_buffer[2];

    // convert the binary data into bcd 
	bin_to_bcd(file_status[active_file_id].NextEntryNumber, (VIM_UINT8*)adp_buffer, VIM_SIZEOF(adp_buffer)*2 );
	
    /* pack the private data (field 48) into the message */      
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 48, adp_buffer, VIM_SIZEOF(adp_buffer)));
    return VIM_ERROR_NONE;
}





/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_add_data_private
(
  VIM_AS2805_MESSAGE *msg
)
{
    switch(msg->msg_type)
    {
        case MESSAGE_WOW_800_NMIC_191:
        case MESSAGE_WOW_800_NMIC_191_M:
            VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_191(msg));
          break;
        case MESSAGE_WOW_800_NMIC_192:
          VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_192(msg));
          break;
        case MESSAGE_WOW_800_NMIC_193:
          VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_193(msg));
          break;
        case MESSAGE_WOW_800_NMIC_170:
          VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_170(msg));
          break;
        case MESSAGE_WOW_800_NMIC_101:
          VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_101(msg));
          break; 
        case MESSAGE_WOW_300_NMIC_151:
          VIM_ERROR_RETURN_ON_FAIL(msg_add_adp_151(msg));
          break; 
          
        default:					
          VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
    }


    /* return without error */
    return VIM_ERROR_NONE;
}






#if 0
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_append_tag
(
  VIM_TEXT * destination_ptr,
  VIM_SIZE destination_size,
  VIM_TEXT const * tag_ptr,
  VIM_UINT32 value
)
{
  VIM_TEXT buffer[100];
  /* construct the tag, value and delimter */
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf(buffer,VIM_SIZEOF(buffer),"%s%d\\",tag_ptr,value));
  /* append the tag to the destination buffer */
  VIM_ERROR_RETURN_ON_FAIL(vim_strncat(destination_ptr,buffer,destination_size));
  /* return without error */
  return VIM_ERROR_NONE;
}
#endif
/*----------------------------------------------------------------------------*/
VIM_UINT32 set_tcc_flag(transaction_t *tran)
{
  VIM_UINT8 card_type, ret_val;

  card_type = ret_val = 8;

  switch (tran->card.type)
  {
    case card_manual:
      card_type = tran->card.data.manual.pan[0];
      break;
      
    case card_mag:
      card_type = tran->card.data.mag.track2[0];
      break;

    case card_chip:
      card_type = tran->card.data.chip.track2[0];
      break;

    case card_picc:
      card_type = tran->card.data.picc.track2[0];
      break;
      
    default:
      break;
  }

 //VIM_DBG_VAR(card_type);
  
  if ('4' == card_type)
  {
    /* VISA */
    if ((account_credit == tran->account)|| (account_default == tran->account))
      ret_val = 5;
    else
      ret_val = (TERM_CONTACTLESS_ENABLE) ? 7 : 3;
  }
  else if ('5' == card_type)
  {
    /* MASTERCARD */
    if ((account_credit == tran->account) || (account_default == tran->account))
      ret_val = (TERM_CONTACTLESS_ENABLE) ? 7 : 3;
    else if (card_picc == tran->card.type)
      ret_val = 3;
  }
  
  return ret_val;
}
/*----------------------------------------------------------------------------*/
#if 0 // RDD - this field 47 Tag is not referred to in the WOW spec, but better keep the code just in case !
VIM_UINT32 original_wcv(transaction_t *tran)
{
  if (tran->pin_block_size/* && !transaction_get_status(tran, st_efb)*/)
  {
    if (tran->emv_pin_entry_params.offline_pin)
      /* original offline PIN */
      return 3;
    
    /* it was online PIN */
    if (VIM_ERROR_NONE == vim_mem_compare(tran->host_rc_asc, "08", 2) ||
        transaction_get_status(tran, st_efb))
      /* response code was 08 or EFB */
      return 5;
    
    /* original online PIN */
    return 2;
  }
  
  /* no PIN */
  return 1;
}
/*----------------------------------------------------------------------------*/

VIM_UINT32 set_wcv_flag(transaction_t *tran)
{
  VIM_UINT32 ret_val = 0;

  if (txn_is_moto(tran) || txn_is_ecom(tran))
  {
    if (moto_customer_present == tran->card.data.manual.manual_reason)
      ret_val = 1;
  }
  else
  {
    switch (tran->type)
    {
      case tt_adjust:
        break;

      default:
        if (tran->pin_block_size)
        {
          if (tran->emv_pin_entry_params.offline_pin)
            /* offline PIN */
            ret_val = /*transaction_get_status(tran, st_efb) ? 1 :*/ 3;
          else
            /* EFB or online PIN */
            ret_val = transaction_get_status(tran, st_efb) ? 5 : 2;
        }
        else
        {
          /* for contactless transaction with "NO CVM" WCV should be 1 */
          if (card_picc == tran->card.type &&
              !tran->card.data.picc.msd &&
              !transaction_get_status(tran, st_partial_emv))
          {
            VIM_BOOL status;

            /* if amount below CVM limit WCV should be 6 */
            if (tran->amount_total > terminal.emv_applicationIDs[tran->aid_index].cvm_limit)
            {
              if (tran->pin_bypassed)
                ret_val = 1;
              else
              {
                vim_picc_emv_is_signature_required(picc_handle.instance, &status);
                ret_val = (status) ? 1 : 6;
              }
            }
            else
              ret_val = 6;
          }
          else
            /* no PIN, signature */
            ret_val = 1;
        }
        break;
    }
  }

  return ret_val;
}
#endif


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_standalone_add_national_data(VIM_AS2805_MESSAGE *message_ptr, transaction_t *tran)
{
  VIM_TEXT buffer[100];

  VIM_UINT32 nlen = VIM_SIZEOF(buffer);
  VIM_TEXT *p = buffer;
  
  /* ensure that the string is null terminated */
  vim_mem_clear(buffer, VIM_SIZEOF(buffer));
  
 //VIM_DBG_VAR(message_ptr->msg_type);

  switch(message_ptr->msg_type)
  {  
    case MESSAGE_WOW_220:
    case MESSAGE_WOW_221:
    case MESSAGE_WOW_420:
    case MESSAGE_WOW_421:
		// RDD 160412 SPIRA:5277 New Spec for FBKx Flag
		if( tran->type == tt_completion )
		{
          VIM_ERROR_RETURN_ON_FAIL( vim_strncpy(p,"FBKV\\",VIM_MIN(nlen,vim_strlen("FBKV\\"))));
          nlen -= vim_strlen("FBKV\\");
          p += vim_strlen("FBKV\\");
		}
		else if( tran->status & st_efb )
        {
          VIM_ERROR_RETURN_ON_FAIL( vim_strncpy(p,"FBKE\\",VIM_MIN(nlen,vim_strlen("FBKE\\"))));
          nlen -= vim_strlen("FBKE\\");
          p += vim_strlen("FBKE\\");
        }      
        break;

     default: break;
  }

  switch (message_ptr->msg_type)
  {
    case MESSAGE_WOW_100:
    case MESSAGE_WOW_200:

	  // RDD 290814 added CCV related stuff
	
	  if(( tran->card.type == card_mag ) && ( terminal.configuration.ccv_entry ))    
	  {
		  if( vim_strlen(tran->card.data.mag.ccv))		
		  {			
			  // add CCV + CCI = 1			
			  vim_snprintf( p, nlen, "CCV%s\\CCI1\\", tran->card.data.mag.ccv );
		  }		
		  else if(transaction_get_status( tran, st_moto_txn ))
		  {			
			  // add CCI = 0			
			  vim_strcpy( p, "CCI0\\" );		
		  }
      }    
      else if(( tran->card.type == card_manual) && ( terminal.configuration.ccv_entry ))
      {
		if( vim_strlen( tran->card.data.manual.ccv ))
		{
			// add CCV + CCI = 1
			vim_snprintf( p, nlen, "CCV%s\\CCI1\\", tran->card.data.manual.ccv );
		}
		else if(transaction_get_status(tran, st_moto_txn))
		{
			// RDD 121114 - MB sent me an email indicating that they don't need this tag at all if no CCV present
			// add CCI = 0
			vim_strcpy( p, "CCI0\\" );
		}
      }
	  nlen -= vim_strlen(p);      
	  p += vim_strlen(p);
	  // RDD 290814 Drop through to add other stuff  

	  if(transaction_get_status(tran, st_moto_txn))
	  {
          switch (txn.moto_reason)
          {
          case moto_reason_telephone:
          default:
              vim_strcpy(p, "ECM11\\");
              break;

          case moto_reason_mail_order:
              vim_strcpy(p, "ECM21\\");
              break;
          }
		  nlen -= vim_strlen(p);      	  
		  p += vim_strlen(p);
	  }

    case MESSAGE_WOW_220:
    case MESSAGE_WOW_221:
    case MESSAGE_WOW_420:
    case MESSAGE_WOW_421:
      
        if(TERM_CONTACTLESS_ENABLE)
        {  
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"TCC07\\",VIM_MIN(nlen,vim_strlen("TCC07\\"))));
        }
        else
        {
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"TCC03\\",VIM_MIN(nlen,vim_strlen("TCC03\\"))));
        }
        nlen -= vim_strlen(p);
        p += vim_strlen(p);
        break;

    default:
        break;        
   }

  // 260721 JIRA-PIRPIN 1118 VDA
  if (message_ptr->msg_type == MESSAGE_WOW_200_VDA)
  {
      vim_strcat(p, "VDAA\\");
  }
  else if (message_ptr->msg_type == MESSAGE_WOW_200_VDAR)
  {
      vim_strcat(p, "VDAR\\");
  }

  // RDD 120821 JIRA PIRPIN-698 added MCR Tag to be forwarded to EHUB
  if (((message_ptr->msg_type == MESSAGE_WOW_200) || (message_ptr->msg_type == MESSAGE_WOW_100)) && (transaction_get_status(tran, st_mcr_active)))
  {
      vim_strcat(p, "MCR\\");
  }


  pceft_debugf_test(pceftpos_handle.instance, "DE47=%s", buffer);

 
   if (vim_strlen(buffer) > 0)
   {
     /* add the field to the message */
     VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(message_ptr,47,buffer,vim_strlen(buffer)));
   }
   /* return without error */
   return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR message_wow_add_national_data(VIM_AS2805_MESSAGE *message_ptr, transaction_t *tran, VIM_BOOL repeat, VIM_BOOL VDA)
{
  VIM_TEXT buffer[100];

  VIM_UINT32 nlen = VIM_SIZEOF(buffer);
  VIM_TEXT *p = buffer;
  
  /* ensure that the string is null terminated */
  vim_mem_clear(buffer, VIM_SIZEOF(buffer));
  
  // RDD 151214 - call the standalone version if needed
  // RDD 190721 Add CVV etc. for Non-WoW terminals
  if( !TERM_USE_PCI_TOKENS )
  {
	  return message_standalone_add_national_data( message_ptr, tran);
  }

 //VIM_DBG_VAR(message_ptr->msg_type);

  switch(message_ptr->msg_type)
  {  
    case MESSAGE_WOW_220:
    case MESSAGE_WOW_221:
    case MESSAGE_WOW_200_VDA:
    case MESSAGE_WOW_200_VDAR:

#if 0						// RDD 230516 According to DG FBK flag does belong to $2x messages
    case MESSAGE_WOW_420:
    case MESSAGE_WOW_421:
#endif

		// RDD 230516 SPIRA:8945 Manual Entry should result in FBKV and shouldn't be sent for 42x
		if ( tran->card.type == card_manual )
        {      
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"FBKV\\",VIM_MIN(nlen,vim_strlen("FBKV\\"))));
          nlen -= vim_strlen("FBKV\\");
          p += vim_strlen("FBKV\\");
          if( terminal.last_swipe_failed )
          {
            VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"FCR\\",VIM_MIN(nlen, vim_strlen("FCR\\"))));    
            nlen -= vim_strlen("FCR");
            p += vim_strlen("FCR");
          }
        }

		else if(( tran->status & st_efb )||( ValidateAuthCode( tran->auth_no )))
        {
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"FBKE\\",VIM_MIN(nlen,vim_strlen("FBKE\\"))));
          nlen -= vim_strlen("FBKE\\");
          p += vim_strlen("FBKE\\");
        }      
        break;

     default: break;
  }

  switch (message_ptr->msg_type)
  {
    case MESSAGE_WOW_200:
    case MESSAGE_WOW_200_VDA:
    case MESSAGE_WOW_200_VDAR:
    case MESSAGE_WOW_220:
    case MESSAGE_WOW_221:
    case MESSAGE_WOW_420:
    case MESSAGE_WOW_421:
      
        if(TERM_CONTACTLESS_ENABLE)
        {  
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"TCC07\\",VIM_MIN(nlen,vim_strlen("TCC07\\"))));
        }
        else
        {
          VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(p,"TCC03\\",VIM_MIN(nlen,vim_strlen("TCC03\\"))));
        }
        nlen -= vim_strlen(p);
        p += vim_strlen(p);
        break;

    default:
        break;      
  }

  // 260721 JIRA-PIRPIN 1118 VDA
  if( message_ptr->msg_type == MESSAGE_WOW_200_VDA )
  {
      vim_strcat(p, "VDAA\\");
  }
  else if (message_ptr->msg_type == MESSAGE_WOW_200_VDAR)
  {
      vim_strcat(p, "VDAR\\");
  }

  // RDD 120821 JIRA PIRPIN-698 added MCR Tag to be forwarded to EHUB
  if (((message_ptr->msg_type == MESSAGE_WOW_200) || (message_ptr->msg_type == MESSAGE_WOW_100)) && (transaction_get_status(tran, st_mcr_active)))
  {
      vim_strcat(p, "MCR\\");
  }

  pceft_debugf_test(pceftpos_handle.instance, "DE47=%s", buffer);


   if (vim_strlen(buffer) > 0)
   {
     /* add the field to the message */
     VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(message_ptr,47,buffer,vim_strlen(buffer)));
   }
   /* return without error */
   return VIM_ERROR_NONE;
}




static VIM_ERROR_PTR msg_add_currency_code(VIM_AS2805_MESSAGE *msg, VIM_UINT8 field)
{
  char asc_currency[4+1];
  VIM_UINT8 bcd_currency[2] = { 0,0 };

  vim_strcpy( asc_currency, (WOW_NZ_PINPAD) ? "0554":"0036" );
  
  asc_to_bcd(asc_currency, bcd_currency, 4);

  // RDD 060912 P3 - all terminals will include the currency code
  //if( WOW_NZ_PINPAD )
	VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, field, bcd_currency, 2));
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_add_aiic(VIM_AS2805_MESSAGE *msg)
{

	//actual sent by Ingenico terminal: 31 31 00 06 28 00 00 0f
  VIM_UINT8 const aiic[6] = {0x00, 0x06, 0x28, 0x00, 0x00, 0x0F};       
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field_as_nibbles(msg, 32, aiic, 11));
  /* return without error */
  return VIM_ERROR_NONE;
}



VIM_BOOL SendEMVTags( transaction_t *transaction )
{
	DBG_VAR(transaction->type);
	DBG_VAR(transaction->original_type);
	DBG_VAR(transaction->status);
	DBG_VAR(transaction->card.data.picc.msd);
    DBG_VAR(transaction->account);

  /* special processing for EPAL */
  if( is_EPAL_cards (transaction) && transaction->card.type == card_chip && transaction->type != tt_refund )
  {
		return VIM_TRUE;    
  }
 
  else if(( transaction->type == tt_sale ) || ( transaction->type == tt_completion ) || ( transaction->type == tt_sale_cash ) || ( transaction->type == tt_cashout ) || ( transaction->type == tt_preauth ))
   
	  // RDD 120312 SPIRA:4939 Field 55 only for Sale (no cash)    
	  // RDD 120712 SPIRA:5737 Field 55 Required for Credit/Cash
	{
      DBG_POINT;
		if(( transaction->card.type == card_chip) && (( transaction->account == account_credit )||( transaction->account == account_default )))
		{	
			return VIM_TRUE;
		}
		if(( card_picc == transaction->card.type ) && ( !transaction->card.data.picc.msd ))
		{
			return VIM_TRUE;
		}
	}
  DBG_POINT;
#if 1  // RDD 270720 Trello #82 added DE55 to refunds for Mastercard and VISA if enabled via LFD
  if(( transaction->type == tt_refund ) &&  AllowFullEMVRefund( transaction ) && ( transaction->account != account_cheque ) && (transaction->account != account_savings))
  { 
      VIM_DBG_MESSAGE("RRRRRRRRRRRRR Send EMV Refund Tags for Mastercard or VISA RRRRRRRRRRRR");
      return VIM_TRUE;
  }       
#endif

	return VIM_FALSE;
}

/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR msg_add_EMV_tags(VIM_AS2805_MESSAGE *msg, transaction_t *transaction)
{
  VIM_UINT8 de55_buffer[2000];
  VIM_SIZE de55_size = 0;

  // RDD 301013 SPIRA:6987 Customer Wrongly charged
  vim_mem_clear( de55_buffer, VIM_SIZEOF(de55_buffer) );

  // RDD 260412 SPIRA:5366 Missing DE55 from Contactless completions
  if( !SendEMVTags( transaction )) 
      return VIM_ERROR_NONE;

  // Get the EMV tags and put into the transaction struct, otherwise one would hope that the data is already stored
  VIM_ERROR_RETURN_ON_FAIL( emv_get_host_data( transaction, msg->msg_type, de55_buffer, &de55_size, VIM_SIZEOF(de55_buffer)));

  if( de55_size )
  {		
	  DBG_DATA( de55_buffer, de55_size );    	
	  VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_add_field( msg, 55, de55_buffer, de55_size ));
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_cash_amount
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{
  VIM_UINT8 amount_bcd[6];

  // RDD 230212: SPIRA ISSUE 4815 - no cash field on balance
  if( transaction->type == tt_balance )
	  return VIM_ERROR_NONE;

  bin_to_bcd(transaction->amount_cash, amount_bcd, VIM_SIZEOF(amount_bcd)*2);

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 57, amount_bcd, VIM_SIZEOF(amount_bcd)));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_additional_data_private
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{  
  VIM_CHAR preswipe_status = '0';

 
  ////// TEST TEST TEST - bulk out message by adding variable length string set in config //////
#if 0	// RDD 140218 JIRA-WWBP 161
  extern VIM_CHAR DummyString[];
  {
	  VIM_SIZE len = vim_strlen( DummyString );
	  if( len )
	  {		
		  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field( msg, 60, DummyString, len ));
	  }
	  else
	  {		
		  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 60, &preswipe_status, VIM_SIZEOF(preswipe_status) ));
	  }
  }
  ////// TEST TEST TEST - bulk out message by adding variable length string set in config //////
#else
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 60, &preswipe_status, VIM_SIZEOF(preswipe_status) ));
#endif
  /* return without error */
  return VIM_ERROR_NONE;
}


#if 0
static VIM_ERROR_PTR filter_PAD_DE63
( 
  VIM_TEXT* new_DE63_ptr, 
  VIM_TEXT* old_DE63_ptr, 
  VIM_SIZE old_DE63_len
)
{
  if( old_DE63_len > 0 )
  {
    VIM_SIZE offset=0;
    VIM_TEXT tag_buf[4];
    VIM_TEXT len_buf[4];
    while( offset < old_DE63_len )
    {
      vim_mem_copy( tag_buf, old_DE63_ptr+offset, 3 );
      offset += 3;
      tag_buf[3] = 0x00;
      if( vim_mem_compare(tag_buf, "PRC", 3) == VIM_ERROR_NONE )
      {
        /* skip these tags */
        vim_mem_copy( len_buf, old_DE63_ptr+offset, 3 );
        offset += 3;
        len_buf[3] = 0x00;
        offset += vim_atol (len_buf);
      }
      else
      {
        /* copy tag */
        vim_strcat( new_DE63_ptr, tag_buf );
        /* pack other tags */
        vim_mem_copy( len_buf, old_DE63_ptr+offset, 3 );
        offset += 3;
        len_buf[3] = 0x00;
        /* copy length */
        vim_strcat( new_DE63_ptr, len_buf );
        /* copy value */
        vim_mem_copy( new_DE63_ptr + vim_strlen(new_DE63_ptr), old_DE63_ptr+offset, vim_atol(len_buf));
        offset += vim_atol(len_buf);
      }
    }
  }
  /* return */
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_add_reserved_private	// RDD 220512 added for WOW fuel cards
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{  
  VIM_SIZE PadLen = vim_strlen( transaction->u_PadData.PadData.DE63 );

  if( fuel_card(transaction) && PadLen )
  {
    //VIM_TEXT new_DE63[WOW_MAX_PAD_STRING_LEN+100];
    //vim_mem_clear( new_DE63, VIM_SIZEOF(new_DE63));
    //VIM_ERROR_RETURN_ON_FAIL( filter_PAD_DE63( new_DE63, transaction->u_PadData.PadData.DE63, PadLen ));
    //VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_add_field( msg, 63, new_DE63, vim_strlen(new_DE63)));
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_add_field( msg, 63, transaction->u_PadData.PadData.DE63, PadLen ));
  }
  /* return without error */
  return VIM_ERROR_NONE;
}


/*
typedef struct
{
    VIM_UINT8 OriginalMsgType[2];      
    VIM_UINT8 OriginalSTAN[3];
    VIM_UINT8 OriginalRespTime[3];
    VIM_UINT8 OriginalRespDate[2];
    VIM_UINT8 Unsued[11];
} s_wow_field90_420req_t;    
*/

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_original_data_elements
(
  VIM_AS2805_MESSAGE *msg, 
  transaction_t *transaction
)
{  
  u_wow_field90_420req_t u_original_data;

  // clear the buffer to zeros
  vim_mem_clear( u_original_data.bytebuff, VIM_SIZEOF(u_original_data.bytebuff) );

  // copy over the original msg type
  vim_mem_copy( u_original_data.fields.OriginalMsgType, transaction->msg_type, 2 );

  // copy over the orginal stan
  bin_to_bcd( transaction->orig_stan, u_original_data.fields.OriginalSTAN, 6 );		// BRD 

  // copy over the original time  
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.hour, &u_original_data.fields.OriginalRespTime[0], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.minute, &u_original_data.fields.OriginalRespTime[1], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.second, &u_original_data.fields.OriginalRespTime[2], 2));

  // copy over the orginal date
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.day , &u_original_data.fields.OriginalRespDate[0], 2));
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint64_to_bcd(transaction->time.month, &u_original_data.fields.OriginalRespDate[1], 2));
  
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 90, u_original_data.bytebuff, VIM_SIZEOF(u_original_data.bytebuff) ));

  /* return without error */
  return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_nmic(VIM_AS2805_MESSAGE *msg, VIM_AS2805_MESSAGE_TYPE nmic)
{
  VIM_UINT8 nmic_bcd[2];

  bin_to_bcd(nmic, nmic_bcd, 4);

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 70, nmic_bcd, 2));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
#define WOW_300_MESSAGE_NUMBER_BYTES 2

static VIM_ERROR_PTR msg_add_message_number(VIM_AS2805_MESSAGE *msg)
{
  VIM_UINT8 message_number[WOW_300_MESSAGE_NUMBER_BYTES];

  bin_to_bcd(file_status[active_file_id].MessageNumber, message_number, WOW_300_MESSAGE_NUMBER_BYTES*2);

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 71, message_number, WOW_300_MESSAGE_NUMBER_BYTES));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_file_update_code(VIM_AS2805_MESSAGE *msg)
{
  VIM_UINT8 file_update_code = '4';

  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, 91, &file_update_code, 1));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

/* 101 - file name */
static VIM_ERROR_PTR msg_add_file_name(VIM_AS2805_MESSAGE *msg)
{
  VIM_CHAR *ptr = file_status[active_file_id].FileName;
  
  return vim_as2805_message_add_field( msg, 101, ptr, vim_strlen(ptr) );
}



/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_pin_block( VIM_AS2805_MESSAGE *message_ptr, transaction_t *transaction )
{  
  /* check PIN block */

  // Reset the PinBlock size as a Pin block is being sent in this msg
  transaction->pin_block_size = 8;

  /* add the pin block to the message */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(message_ptr, 52, &transaction->pin_block, 8 ));

  /* return without error */
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_additional_amounts(VIM_AS2805_MESSAGE *msg, transaction_t *transaction)
{
    VIM_SIZE len = 0;
    VIM_CHAR add_amts[60 + 1];
    VIM_CHAR amount_ascii[12 + 1];
    vim_mem_clear(add_amts, VIM_SIZEOF(add_amts));
    vim_mem_clear(amount_ascii, VIM_SIZEOF(amount_ascii));

    if( transaction->amount_tip )
    {
        if (transaction->account == account_cheque)
            vim_strcat(add_amts, "20");
        else if (transaction->account == account_savings)
            vim_strcat(add_amts, "10");
        else
            vim_strcat(add_amts, "30");

        vim_strcat(add_amts, "57036D");

        bin_to_asc(transaction->amount_tip, amount_ascii, 12);

        vim_strcat(add_amts, amount_ascii);
        len = vim_strlen(add_amts);
    }
    // RDD if Surcharge is setup for selectred scheme then add data here even if surcharge is zero
	// RDD 150322 JIRA-PIRPIN-???? only send surcharge tag if the surcharge is finite - causes issues upstream if amount is sent as zero.
    //if( ApplySurcharge(transaction, VIM_FALSE) == VIM_TRUE )
	if( transaction->amount_surcharge )
    {
        if (transaction->account == account_cheque)
            vim_strcat(add_amts, "20");
        else if (transaction->account == account_savings)
            vim_strcat(add_amts, "10");
        else
            vim_strcat(add_amts, "30");

        vim_strcat(add_amts, "70036D");

        bin_to_asc(transaction->amount_surcharge, amount_ascii, 12);

        vim_strcat(add_amts, amount_ascii);
        len = vim_strlen(add_amts);
    }	  
    
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_add_field( msg, 54, add_amts, len ));  
    return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR msg_add_mac(VIM_AS2805_MESSAGE *msg, VIM_BOOL extended )
{
  VIM_DES_BLOCK mac;

  // RDD - add support for MACs of extended bitmap messages (for WOW 300/310 )
  VIM_UINT8 field_id = ( extended ) ? 128 : 64;

  /* clear the mac buffer */
  vim_mem_clear(&mac, sizeof(mac));

  /* Add the mac field so that it is used in generating the MAC */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field(msg, field_id, &mac, VIM_SIZEOF(mac)));
 //VIM_ 

  /* generate the MAC over the whole message (exluding the MAC field) */
  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_generate_mac(tcu_handle.instance_ptr,&mac,
                                                msg->message,
                                                msg->msg_size - VIM_SIZEOF(mac),
                                                VIM_TCU_MAC_KEY_KMAC));
  /* Now add the mac data */
  VIM_ERROR_RETURN_ON_NULL(msg->fields[field_id]);

// TEST TEST TEST - corrupt the MAC
  //mac.bytes[3] ^= 0xFF;
// TEST TEST TEST 

  vim_mem_copy(msg->fields[field_id], mac.bytes, 4);
  
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/


static VIM_ERROR_PTR message_wow_800(VIM_AS2805_MESSAGE *message_ptr, VIM_AS2805_MESSAGE_TYPE nmi)
{
  // RDD 010811 - wow set stan to zeros for 0800 msgs
  VIM_UINT8 wow_0800_stan[] = { 0x00, 0x00, 0x00 };
  
  /* 1 - Extended bitmap */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_extended_bitmap(message_ptr));
  
  /* 11 -  */
  if( nmi != MESSAGE_WOW_800_NMIC_101 )
  {
	message_ptr->stan = 0;	// so the parse doesn'nt fail with a mismatch !
    // WOW 0800 messages have a fixed stan of all zeros except for the 101 logon which uses the real stan
    VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_field_as_nibbles(message_ptr, 11, wow_0800_stan, VIM_SIZEOF(wow_0800_stan)*2 )); 
    // reset the message stan so that there isn't a mismatch on checking the response
    //message_ptr->stan = 0;
  }
  else
  {  
    VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(message_ptr, &txn));
  }
  
  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(message_ptr));
  
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(message_ptr));
  
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(message_ptr));
  
  /* 48 - Additional Data Private */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_add_data_private(message_ptr));
  
  // RDD 101012 P3 SPIRA:615849 - currency code to be added to 170 logon
  if( nmi == MESSAGE_WOW_800_NMIC_170 )
  {	
	  VIM_ERROR_RETURN_ON_FAIL(msg_add_currency_code(message_ptr, 49));
  }

  /* 70 - NMIC */
  if (nmi == MESSAGE_WOW_800_NMIC_191_M)
      nmi = MESSAGE_WOW_800_NMIC_191;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_nmic(message_ptr, nmi));
  
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_200(VIM_AS2805_MESSAGE *message_ptr)
{
  /* 2 - PAN */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pan(message_ptr, &txn.card, &txn ));
  /* 3 - processing code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_processing_code(message_ptr, &txn));
  /* 4 - Transaction amount */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_transaction_amount(message_ptr, &txn));
  /* 11 - stan*/
  VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(message_ptr, &txn));
  /* 14 - card expiry */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_expiry(message_ptr, &txn.card, &txn ));
  /* 22 - POS entry mode */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pose(message_ptr, &txn));
  /* 23 - card sequence number */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_sequence_no(message_ptr, &txn));
  /* 25 - POS condition code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_posc(message_ptr, &txn));
  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(message_ptr));

  /* 35 - track 2 data */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_track_2(message_ptr, &txn.card));
  
  /* 37 - RRN */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_rrn( message_ptr, &txn ));
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(message_ptr));
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(message_ptr));
  /* 47 - Additional Data - National */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_national_data(message_ptr, &txn, VIM_FALSE, VIM_FALSE ));

  /* 49 - currency code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_currency_code(message_ptr, 49));

  /* 52 - pin block */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pin_block(message_ptr, &txn));  

  /* RDD 280814 ALH - 54 - Additional Amounts */
  if( !TERM_USE_PCI_TOKENS )	
	  VIM_ERROR_RETURN_ON_FAIL(msg_add_additional_amounts(message_ptr, &txn));

  /* 55 - ICC tags */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_EMV_tags(message_ptr, &txn));
  /* 57 - cash amout */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_cash_amount(message_ptr, &txn));
  /* 60 - Additional Data Private */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_additional_data_private(message_ptr, &txn));
  // 63 - RDD 210512 PadData added for fuel cards 
  VIM_ERROR_RETURN_ON_FAIL( message_wow_add_reserved_private( message_ptr, &txn ));
  /* 64 - mac */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_mac(message_ptr, VIM_FALSE));

  /* return without error */
  return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR message_wow_300(VIM_AS2805_MESSAGE *msg, VIM_AS2805_MESSAGE_TYPE nmic)
{
  /* 1 - Extended bitmap */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_extended_bitmap(msg));
  /* 11 - stan*/
  VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(msg, &txn));
  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(msg));
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(msg));
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(msg));
  /* 48 - Additional Data Private */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_add_data_private(msg));   
  /* 70 - NMIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_nmic(msg, nmic));
  /* 71 - message number */ 
  VIM_ERROR_RETURN_ON_FAIL(msg_add_message_number(msg));
  /* 91 - file update code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_file_update_code(msg));
  /* 101 - file name */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_file_name(msg));
  /* 128 - mac */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_mac(msg, VIM_TRUE));
  /* return without error */
  return VIM_ERROR_NONE;
}  
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_420(VIM_AS2805_MESSAGE *msg, VIM_BOOL repeat )
{
  /* 1 - Extended bitmap */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_add_extended_bitmap(msg));
  /* 2 - PAN */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pan(msg, &reversal.card, &reversal ));
  /* 3 - processing code */ /* Same as for the transaction being reversed*/
  VIM_ERROR_RETURN_ON_FAIL(msg_add_processing_code(msg, &reversal));
  /* 4 - Transaction amount */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_transaction_amount(msg, &reversal));  
  /* 11 - stan*/

  // RDD 200512 - This surely isn't right according to the spec Reversal has a new stan only repeated for repeat reversals
#if 1
  VIM_ERROR_RETURN_ON_FAIL(msg_add_stan_original(msg, &reversal));
#else
  if (repeat)		// RDD: if repeat then get stan from original msg
    {VIM_ERROR_RETURN_ON_FAIL(msg_add_stan_original(msg, &reversal));}
  else
    {VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(msg, &reversal));}
#endif

  // RDD 240720 Trello #193 Store the sent STAN to check against.
  last_reversal_stan = reversal.stan;


  /* 14 - card expiry */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_expiry(msg, &reversal.card, &reversal ));
  /* 22 - POS entry mode */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pose(msg, &reversal));
  /* 23 - card sequence number */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_sequence_no(msg, &reversal));
  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(msg));
  /* 35 - track 2 data */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_track_2(msg, &reversal.card));
  /* 37 - RRN */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_rrn(msg, &reversal));
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(msg));
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(msg));
  /* 47 - Additional Data - National */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_national_data(msg, &reversal, repeat, VIM_FALSE ));
  /* 49 - currency code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_currency_code(msg, 49));
  /* 55 - ICC tags */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_EMV_tags(msg, &reversal));
  /* 57 - cash amout */
 //  
  VIM_ERROR_RETURN_ON_FAIL(msg_add_cash_amount(msg, &reversal));		// BRD This must be a bug, txn is not still valid
  /* 60 - Additional Data Private */
 //  
  VIM_ERROR_RETURN_ON_FAIL(message_wow_additional_data_private(msg, &reversal));
  // 63 - RDD 210512 PadData added for fuel cards 
 //  
  VIM_ERROR_RETURN_ON_FAIL( message_wow_add_reserved_private( msg, &reversal ));
  /* 90 - Original Data elements */
 //  
  VIM_ERROR_RETURN_ON_FAIL(message_wow_original_data_elements(msg, &reversal));  
  /* 128 - mac */
 //  
  VIM_ERROR_RETURN_ON_FAIL(msg_add_mac(msg, VIM_TRUE));
  /* return without error */
  return VIM_ERROR_NONE;
} 

static VIM_ERROR_PTR message_wow_220(VIM_AS2805_MESSAGE *message_ptr, VIM_BOOL repeat, VIM_BOOL VDA )
{
    /* 2 - PAN */
    DBG_POINT;


    VIM_ERROR_RETURN_ON_FAIL(msg_add_pan(message_ptr, &txn_trickle.card, &txn_trickle));
    /* 3 - processing code */
    DBG_POINT;


    VIM_ERROR_RETURN_ON_FAIL(msg_add_processing_code(message_ptr, &txn_trickle));
    /* 4 - Transaction amount */
    DBG_POINT;


    VIM_ERROR_RETURN_ON_FAIL(msg_add_transaction_amount(message_ptr, &txn_trickle));
    /* 7 - transmission date and time*/

    DBG_POINT;

    VIM_ERROR_RETURN_ON_FAIL(msg_add_transmit_time(message_ptr, &txn_trickle));
    /* 11 - stan*/

    DBG_POINT;

    if((repeat)&&(!VDA))		// RDD: if repeat then get stan from original msg
    {
        VIM_ERROR_RETURN_ON_FAIL(msg_add_stan_original(message_ptr, &txn_trickle));
    }
    else
    {
        // RDD 240720 Trello #193 Store the sent STAN to check against. Sometimes after logon original stan is not good and can match 0220
        if (terminal.stan == last_reversal_stan)
        {
            pceft_debug(pceftpos_handle.instance, "Trello #193");
            terminal_increment_stan();
        }
        VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(message_ptr, &txn_trickle));
    }

  /* 12 - transaction time */
         
    DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_time(message_ptr, &txn_trickle));
  /* 13 - transaction date */
         
    DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_date(message_ptr, &txn_trickle));
  /* 14 - card expiry */
         
    DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_expiry(message_ptr, &txn_trickle.card, &txn_trickle ));
  /* 22 - POS entry mode */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_pose(message_ptr, &txn_trickle));
  /* 23 - card sequence number */
        DBG_POINT;
 
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_sequence_no(message_ptr, &txn_trickle));
  /* 25 - POS condition code */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_posc(message_ptr, &txn_trickle));
  /* 32 - AIIC */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(message_ptr));
  /* 35 - track 2 data */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_track_2(message_ptr, &txn_trickle.card));
  /* 37 - RRN */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_rrn(message_ptr,&txn_trickle));
  /* 38 - authid*/
    DBG_POINT;

  if (vim_strlen(txn_trickle.auth_no) > 0)
    VIM_ERROR_RETURN_ON_FAIL(msg_add_auth_id(message_ptr, txn_trickle.auth_no));
    DBG_POINT;

       
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(message_ptr));
  /* 42 - CAIC */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(message_ptr));
  /* 47 - Additional Data - National */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_national_data(message_ptr, &txn_trickle, repeat, VDA));
    /* 49 - currency code */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_currency_code(message_ptr, 49));
    DBG_POINT;

    // RDD - need NULL PinBlock for the VDA 0200
    if (VDA)
    {
        /* 52 - pin block */
        VIM_ERROR_RETURN_ON_FAIL(msg_add_pin_block(message_ptr, &txn));
    }

  /* 55 - ICC tags */
       
  VIM_ERROR_RETURN_ON_FAIL(msg_add_EMV_tags(message_ptr, &txn_trickle));
  /* 57 - cash amout */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(msg_add_cash_amount(message_ptr, &txn_trickle));
  /* 60 - Additional Data Private */
         DBG_POINT;

  VIM_ERROR_RETURN_ON_FAIL(message_wow_additional_data_private(message_ptr, &txn_trickle));
    DBG_POINT;

  // 63 - RDD 210512 PadData added for fuel cards 
       
  VIM_ERROR_RETURN_ON_FAIL( message_wow_add_reserved_private( message_ptr, &txn_trickle ));
    DBG_POINT;

  /* 64 - mac */
       
  VIM_ERROR_RETURN_ON_FAIL(msg_add_mac(message_ptr,VIM_FALSE));
    DBG_POINT;

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_600(VIM_AS2805_MESSAGE *message_ptr)
{
  /* 2 - PAN */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_pan(message_ptr, &txn.card, &txn ));
  /* 3 - processing code */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_processing_code(message_ptr, &txn));
  /* 11 - stan*/
  VIM_ERROR_RETURN_ON_FAIL(msg_add_stan(message_ptr, &txn));
  /* 14 - card expiry */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_card_expiry(message_ptr, &txn.card, &txn ));

  // RDD 190315 SPIRA:8483 Add Settle Date for ALH
  /* 15 - Settle Date */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_settle_date(message_ptr, &txn.settle_date[0] ));

  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL(message_wow_add_aiic(message_ptr));
  /* 35 - track 2 data */
#if 0	// RDD 060212 - 5100 sends DE2 and DE14 NOT DE35 for Token Lookup as it supports keyed entry
  if( txn.type != tt_settle )
  {
	VIM_ERROR_RETURN_ON_FAIL( msg_add_track_2( message_ptr, &txn.card ));
  }
#endif
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_catid(message_ptr));
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_caic(message_ptr));
  /* 64 - mac */
  VIM_ERROR_RETURN_ON_FAIL(msg_add_mac(message_ptr,VIM_FALSE));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR message_wow_600_QC(VIM_AS2805_MESSAGE *message_ptr)
{
  transaction_t transaction;
  const VIM_CHAR* pszF48 = QC_GetOTP();
  VIM_RTC_TIME date_time;
  char szMonth[2+1];
  char szYear[2+1];

  txn_do_init( &transaction );

// What should the PAN be? I pulled this data off of a test card on my desk.
#define DUMMY_PAN       "374245002751005"

  vim_rtc_get_time( &date_time );

  // Set expiry to 'this month, but next year', so it should always be valid.

  vim_sprintf( szMonth, "%02d", date_time.month );
  vim_sprintf( szYear,  "%02d", ( date_time.year % 100 ) + 1 );

  transaction.card.type = card_manual;
  vim_strcpy( transaction.card.data.manual.pan, DUMMY_PAN );
  transaction.card.data.manual.pan_length = (VIM_UINT8)vim_strlen( DUMMY_PAN );
  vim_strcpy( transaction.card.data.manual.expiry_yy, szYear );
  vim_strcpy( transaction.card.data.manual.expiry_mm, szMonth );

  transaction.type = tt_query_card_get_token;

  /* 2 - PAN */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_pan( message_ptr, &transaction.card, &transaction ) );
  /* 3 - processing code */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_processing_code( message_ptr, &transaction ) );
  /* 11 - stan*/
  VIM_ERROR_RETURN_ON_FAIL( msg_add_stan( message_ptr, &transaction ) );
  /* 14 - card expiry */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_card_expiry( message_ptr, &transaction.card, &transaction ) );

  // RDD 190315 SPIRA:8483 Add Settle Date for ALH
  /* 15 - Settle Date */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_settle_date( message_ptr, &transaction.settle_date[0] ) );

  /* 32 - AIIC */
  VIM_ERROR_RETURN_ON_FAIL( message_wow_add_aiic( message_ptr ) );
  /* 41 - CATID */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_catid( message_ptr ) );
  /* 42 - CAIC */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_caic( message_ptr ) );
  /* 48 - Private data */
  if ( !VAA_IsNullOrEmpty( pszF48 ) )
  {
    VIM_SIZE ulF48Len = vim_strlen(pszF48);
    VIM_UINT8 aucHexData[32] = { 0 };

    asc_to_hex( pszF48, aucHexData, ulF48Len );
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_add_field( message_ptr, 48, aucHexData, ulF48Len / 2 ) );
  }

  /* 64 - mac */
  VIM_ERROR_RETURN_ON_FAIL( msg_add_mac( message_ptr, VIM_FALSE ) );

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

 
static VIM_ERROR_PTR msg_encrypt_decrypt(VIM_AS2805_MESSAGE *msg, VIM_BOOL encrypt)
{
  static VIM_SIZE const encrypted_fields_47[] = {2, 14, 35, 47};
  static VIM_SIZE const encrypted_fields[] = {2, 14, 35};
    VIM_UINT8 dest_buffer2[500];
  VIM_SIZE  n,encrypt_length,length_size,field_size,field_index,field_position;
  VIM_UINT8 *Ptr;
  VIM_DES_BLOCK wow_iv_block;

  // RDD 061021 Make whether to encrypt DE47 or not configurable
  VIM_SIZE number_of_e_fields = TERM_ENCRYPT_DE47 ? ARRAY_LENGTH(encrypted_fields_47) : ARRAY_LENGTH(encrypted_fields);
  VIM_SIZE *Ptr2 = TERM_ENCRYPT_DE47 ? (VIM_SIZE *)&encrypted_fields_47[0] : (VIM_SIZE *)&encrypted_fields[0];

  static VIM_DES_BLOCK const iv=
  {{    
          0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF 
  }};

  // form the dynamic IV
  vim_mem_clear( wow_iv_block.bytes, VIM_SIZEOF(wow_iv_block) );  
  bin_to_bcd( msg->stan, wow_iv_block.bytes, VIM_SIZEOF(wow_iv_block)*2 );  
  vim_mem_xor( wow_iv_block.bytes, iv.bytes, VIM_SIZEOF(wow_iv_block));

  vim_mem_clear( dest_buffer2, VIM_SIZEOF(dest_buffer2) );

  // set the ptr to the start of the buffer for encryption
  Ptr = dest_buffer2;
  encrypt_length = 0;
  
  for( n=0; n<number_of_e_fields; n++, Ptr2++ )
  {
    field_index = *Ptr2;

    // skip any fields that have no data
    if( msg->fields[field_index] == VIM_NULL ) continue;    
    
    // get the size of the fields we want to encrypt
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_get_field_value_size( msg, field_index, &field_size ));
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_get_field_length_size( msg ,field_index, &length_size ));

    // get the offset of the encrypted field
    field_position = msg->fields[field_index] - msg->message;
  
    // copy the clear field into the encrypt buffer
    vim_mem_copy( Ptr, &msg->message[field_position+length_size], field_size );
#if 0
    encrypt_length += field_position+length_size;
    Ptr += field_position+length_size;
#else
    encrypt_length += field_size;
    Ptr += field_size;
#endif
  }

  // round up the encrypt length to the nearest DES BLOCK.
  encrypt_length +=  ( VIM_SIZEOF( VIM_DES_BLOCK ) - ( encrypt_length % VIM_SIZEOF( VIM_DES_BLOCK )) );

  ///////////////////////////////////// ENCRYPT DATA //////////////////////////////////////  
  VIM_ERROR_RETURN_ON_FAIL( vim_tcu_ofb_encrypt_using_kds(tcu_handle.instance_ptr, dest_buffer2, dest_buffer2, encrypt_length, &wow_iv_block) );    
  ///////////////////////////////////// ENCRYPT DATA //////////////////////////////////////  
  VIM_DBG_DATA( dest_buffer2, encrypt_length );

  // now we have to copy the fields from the encrypted buffer back into their original positions in the message buffer

  // set the ptr back to the start of the now encrypted field buffer
  Ptr = dest_buffer2;
  Ptr2 = TERM_ENCRYPT_DE47 ? (VIM_SIZE *)&encrypted_fields_47[0] : (VIM_SIZE *)&encrypted_fields[0];

  for( n=0; n<number_of_e_fields; n++, Ptr2++ )
  {
    field_index = *Ptr2;

    // skip any fields that have no data
    if( msg->fields[field_index] == VIM_NULL ) 
        continue;    
    
    // get the size of the fields we want to encrypt
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_get_field_value_size( msg, field_index, &field_size ));
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_get_field_length_size( msg ,field_index, &length_size));

    // get the offset of the encrypted field
    field_position = msg->fields[field_index] - msg->message;
  
    // copy the encrypted field back into the message buffer
    vim_mem_copy( &msg->message[field_position+length_size], Ptr, field_size );
    Ptr += field_size;
  }
  
  VIM_DBG_DATA( msg->message, msg->msg_size );

  return VIM_ERROR_NONE;
}
 
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR msgAS2805_create
(
  VIM_UINT8 host, 
  VIM_AS2805_MESSAGE *msg, 
  VIM_AS2805_MESSAGE_TYPE msg_type
)
{
  VIM_UINT32 message_number;

  //if (QC_TRANSACTION(&txn))
  //    return VIM_ERROR_NONE;

  /* convert the message type to message number */
  VIM_ERROR_RETURN_ON_FAIL(msg_number_get(&message_number, msg_type));

  /* initialize the message structure */  
  //VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_initialise(msg, message_number, VIM_TRUE));
  // RDD 080811 - WBC use bcd llvar lengths but CBA/WOW use ascii */
  VIM_ERROR_RETURN_ON_FAIL(vim_as2805_message_initialise(msg, message_number, VIM_FALSE));

  /* Add message type */
  msg->msg_type = msg_type;

  DBG_POINT;
  /* Set msg build date/time */
  VIM_ERROR_RETURN_ON_FAIL(vim_rtc_get_time(&msg_build_time));

  switch (msg_type)
  {
    case MESSAGE_WOW_800_NMIC_101:
    case MESSAGE_WOW_800_NMIC_170:
    case MESSAGE_WOW_800_NMIC_191:
    case MESSAGE_WOW_800_NMIC_191_M:
    case MESSAGE_WOW_800_NMIC_192:
    case MESSAGE_WOW_800_NMIC_193:    
    VIM_ERROR_RETURN_ON_FAIL(message_wow_800(msg, msg_type));
      break;

    case MESSAGE_WOW_300_NMIC_151:    
    VIM_ERROR_RETURN_ON_FAIL(message_wow_300(msg, msg_type));
      break;

	case MESSAGE_WOW_100:
    case MESSAGE_WOW_200:
      /* save original message type */
      VIM_ERROR_RETURN_ON_FAIL(vim_utl_uint32_to_bcd(message_number, txn.msg_type, VIM_SIZEOF(txn.msg_type) * 2));

      // the MAC needs to be generated before the Pin block or Track2 are added
      VIM_ERROR_RETURN_ON_FAIL(message_wow_200(msg));

      break;

    case MESSAGE_WOW_220:
		VIM_ERROR_RETURN_ON_FAIL(message_wow_220(msg, VIM_FALSE, VIM_FALSE ));
	  break;

    case MESSAGE_WOW_221:
        VIM_ERROR_RETURN_ON_FAIL(message_wow_220(msg, VIM_TRUE, VIM_FALSE));
        break;

    case MESSAGE_WOW_200_VDA:
        txn = txn_trickle;
        txn_pin_generate_NULL_PIN_block(VIM_FALSE);
        VIM_ERROR_RETURN_ON_FAIL(message_wow_220(msg, VIM_FALSE, VIM_TRUE));
        break;

    case MESSAGE_WOW_200_VDAR:
        txn = txn_trickle;
        txn_pin_generate_NULL_PIN_block(VIM_FALSE);
        VIM_ERROR_RETURN_ON_FAIL(message_wow_220(msg, VIM_TRUE, VIM_TRUE));
      break;

	case MESSAGE_WOW_420:
      VIM_ERROR_RETURN_ON_FAIL(message_wow_420(msg, VIM_FALSE ));
	  break;

	case MESSAGE_WOW_421:
      VIM_ERROR_RETURN_ON_FAIL(message_wow_420(msg, VIM_TRUE ));
      break;

	case MESSAGE_WOW_600:
      VIM_ERROR_RETURN_ON_FAIL(message_wow_600(msg));
      break;

    case MESSAGE_WOW_600_QC:
      VIM_ERROR_RETURN_ON_FAIL( message_wow_600_QC( msg ) );
      break;
   
    default:
      VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }

  DBG_DATA(msg->message, msg->msg_size);

  /* check if the message is expected to be encrypted */
#if 1
  if( message_is_encrypted( msg->msg_type ))
  {
    /* encrypt the message */
    VIM_ERROR_RETURN_ON_FAIL( msg_encrypt_decrypt( msg, VIM_TRUE ))    
  }
#endif  
 
  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR process_wow_pci_data( VIM_AS2805_MESSAGE *msg, transaction_t *transaction )  
{
    VIM_SIZE field_size = 0;

    // set a pointer to the field containing the Token (or not as the case may be)
    VIM_CHAR *ptr = (VIM_CHAR *)msg->fields[47] +3; // skip the length bytes
    VIM_CHAR *ptr2 = VIM_NULL;
    
    VIM_ERROR_RETURN_ON_FAIL( vim_as2805_message_get_field_total_size( msg, 47, &field_size ));

	// RDD 060115 - clear the PCI token first for all cases
	vim_mem_set( transaction->card.pci_data.eToken, '0', 20 );	
	transaction->card.pci_data.eToken[20] = '\0';
	transaction->card.pci_data.eTokenFlag = VIM_FALSE;

    field_size -= 3; // skip the actual length bytes
	if (field_size == 0)		//BD Empty DE47 caused crash 
	{
	   return VIM_ERROR_NONE;
	}

	// RDD 060115 SPIRA:8323 Don't allow Token of all zeros after "TOK"	
    // RDD 301120 AVALON - also check for "TOK" at start - Finsim not always adding this for Gift Cards and and 6280 is a zero 4th byte...
    if(( vim_strstr( ptr, "TOK", &ptr2 ) == VIM_ERROR_NONE ) && ( vim_mem_chk( (VIM_UINT8 *)ptr+3, '0', field_size-3 ) != VIM_ERROR_NONE ))
    {
        // We have a token
        transaction->card.pci_data.eTokenFlag = VIM_TRUE;
	}
	else
	{
		return VIM_ERROR_NONE;
	}
	// skip also the backslash at the end - result should be 20 bytes
    vim_mem_copy( transaction->card.pci_data.eToken, ptr+3, field_size-4 );

	// NULL terminate so we can use as a string
	transaction->card.pci_data.eToken[field_size-4] = '\0';
    
    return VIM_ERROR_NONE;
}

// RDD 270412 - According to DG 96,98 and X7 should all behave the same: set 101 then 170 then rsa reqd.
void SetLogonState( VIM_AS2805_MESSAGE_TYPE msg_type, VIM_BOOL CryptoError )
{
#if 0
	// RDD 020512 SPIRA 5423 Destructive host testing - fallback to "R" if 96 resp to logon
	if(( msg_type == MESSAGE_WOW_800_NMIC_191 )||
		( msg_type == MESSAGE_WOW_800_NMIC_192 )||
		( msg_type == MESSAGE_WOW_800_NMIC_193 )||
		( msg_type == MESSAGE_WOW_800_NMIC_170 )||
		( msg_type == MESSAGE_WOW_800_NMIC_101 ))
	{
		terminal.logon_state = rsa_logon;
		return;
	}
#else
	// RDD 100616 v560-2 SPIRA:8996 BD wants to go to logon required state if 101 fails for ANY reason
	// RDD 020512 SPIRA 5423 Destructive host testing - fallback to "R" if 96 resp to logon
	if(( msg_type == MESSAGE_WOW_800_NMIC_191 )||
        ( msg_type == MESSAGE_WOW_800_NMIC_191_M) ||
        ( msg_type == MESSAGE_WOW_800_NMIC_192 )||
		( msg_type == MESSAGE_WOW_800_NMIC_193 )||
		( msg_type == MESSAGE_WOW_800_NMIC_170 ))
	{
		terminal.logon_state = rsa_logon;
		return;
	}
#endif
	switch( terminal.logon_state )
	{
		case logged_on:
			terminal.logon_state = session_key_logon;
			break;

			// RDD 160522 JIRA PIRPIN-1603 Duplicate Issue : In logs we see that Terminal gets locked up with 96 response to 101 when going to 170.
		case session_key_logon:
			if (CryptoError)
			{
				terminal.logon_state = rsa_logon;
			}
			else
			{
				terminal.logon_state = logon_required;
			}
			break;

		default: 
			terminal.logon_state = rsa_logon;
			break;
	}
	DBG_POINT;
	terminal_save();
}


VIM_ERROR_PTR process_wow_private_data( VIM_AS2805_MESSAGE *msg )  
{
      VIM_UINT8 *pdata;
      VIM_UINT16 field_48_length;
      VIM_CHAR kek_update_flag = 0;
	  VIM_UINT16 nmic;
  
      /* nmic - denotes how to process field 48 */
	  if( msg->fields[70] == VIM_NULL )
	  {
		  return VIM_ERROR_NONE;
	  }

      nmic = (VIM_UINT16)bcd_to_bin( msg->fields[70], 4 );

      /* copy response code */
      vim_mem_copy(init.logon_response, txn.host_rc_asc, VIM_SIZEOF(init.logon_response));

	  /* 
      ** wow kek_update flag is derived from field53 for NMIC 170 : 
      ** ascii of LSB. Only '1' or '2' supported by current version of TCU 
      */
	  if(( nmic == 170 )||( nmic == 101))
	  {
		VIM_UINT8 *ptr = msg->fields[53];
		if( ptr != VIM_NULL )
		{
			kek_update_flag = *(ptr+7) | '0';
		}
	  }
      
      /* length of the data */
      field_48_length = (VIM_UINT16)asc_to_bin( (char *)msg->fields[48], 3 );
      
      /* point to the data */
      pdata = msg->fields[48];

      /* skip the length */
      pdata+=3;
  
      switch( nmic )
      {
          case 191:
          VIM_ERROR_RETURN_ON_FAIL( wow_set_pkSP( pdata,field_48_length ));
		  terminal.stats.logons_191 += 1;
          break;

          case 192:          
          VIM_ERROR_RETURN_ON_FAIL( wow_derive_kca_kia( pdata,field_48_length ));
          break;

          case 193:          
          VIM_ERROR_RETURN_ON_FAIL( wow_derive_kek( pdata, field_48_length ));
          break;

          case 170:         
          VIM_ERROR_RETURN_ON_FAIL( wow_process_file_data( pdata ));
          VIM_ERROR_RETURN_ON_FAIL( wow_load_session_keys( pdata, field_48_length, kek_update_flag ));           
		  terminal.stats.logons_170 += 1;
          break;

          case 101:          
	      VIM_ERROR_RETURN_ON_FAIL( wow_change_session_keys( pdata, field_48_length, kek_update_flag ));
		  terminal.stats.logons_101 += 1;
		  break;

          default:
          break;
      } 
      return VIM_ERROR_NONE;
}
  


VIM_ERROR_PTR process_wow_fuel_data( VIM_AS2805_MESSAGE *msg )  
{
      VIM_UINT8 *pdata;
      VIM_UINT16 field_63_length;
  
     /* length of the data */
      field_63_length = (VIM_UINT16)asc_to_bin( (char *)msg->fields[63], 3 );
      
      /* point to the data */
      pdata = msg->fields[63];

      /* skip the length */
      pdata+=3;
	  // Copy over the data to be printed on the receipt
	  vim_mem_copy( txn.u_PadData.PadData.DE63_Response, pdata, field_63_length );
	  // NULL terminate the string
	  txn.u_PadData.PadData.DE63_Response[field_63_length] = '\0';

	  return VIM_ERROR_NONE;
}


// RDD - created to convert WOW Credit Card and GiftCard Balances to binary format 
VIM_BOOL ProcessBalance( VIM_UINT8 *BcdBal, VIM_INT32 *Bal )
{
	VIM_CHAR TextBal[12+1];
	*Bal = 0;

	if( BcdBal != VIM_NULL )
	{
		bcd_to_asc( BcdBal, TextBal, 12  );
		TextBal[12] = '\0';
		// Skip the first byte as this is the C/D indicator and isn't displayed
		*Bal = (VIM_UINT16)asc_to_bin( &TextBal[1], 11 );
	}
	else
		*Bal = -1;			//BD 
	return (*Bal) ? VIM_TRUE : VIM_FALSE;
}    



#define MAX_INVALID_CODES 48

static VIM_ERROR_PTR wow_remap_fuelcard_rc( VIM_CHAR *rc )
{
	VIM_CHAR *MapArray[MAX_INVALID_CODES+1] = { "N0", "N1", "N2", "N3", "N4", "N5", "N6", "N7", "N8", "N9", "NA", "NB", "NC", "ND", "NE", "NF", "NG", "NH",
							   "NI", "NJ", "NK", "NL", "NM", "NN", "NO", "NQ", "NR", "NS", "NT", "NU", "NV", "NW", "NX", "NY", "NZ", "P0", 
							   "P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "P9", "PA", "PB", "PC", "PD" };
	VIM_UINT8 *InvalidPID = &txn.u_PadData.PadData.InvalidProductID;

	VIM_UINT8 n;

	for( n=0; n<=MAX_INVALID_CODES; n++ )
	{
		if( !vim_strncmp( rc, MapArray[n], 2 ) )
		{
			*InvalidPID = n;
			vim_strcpy( rc, "DX" );
			break;
		}
	}
	// RDD 100812 SPIRA 5876
	if( !vim_strncmp( rc, "OO", 2 )) vim_strcpy( rc, "OP" );
	return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR wow_remap_rc( VIM_AS2805_MESSAGE_TYPE msg_type, transaction_t *transaction )
{	
#if 0
	if( msg_type == MESSAGE_WOW_221 )
	{
	  vim_strcpy( transaction->host_rc_asc, "98" );
	}
	if( msg_type == MESSAGE_WOW_800_NMIC_101 )
	{
	  vim_strcpy( transaction->host_rc_asc, "96" );
	}
#endif
	///////// TEST TEST TEST TEST /////////
	///////////////////////////////////////
	//vim_strcpy( transaction->host_rc_asc, "93" );
	///////////////////////////////////////

	// RDD 070912 P3: NZ POS MAP 05->01 as NZ POS treat 05 as approved
	//DBG_VAR( terminal.merchant_id[8] );

	if(( WOW_NZ_PINPAD )&&( !vim_strncmp( transaction->host_rc_asc, "05", 2 )))
	{
	  DBG_MESSAGE( "!!!! NZ: REMAP 05 -> 01 !!!!");
	  vim_strcpy( transaction->host_rc_asc, "01" );
	}

	if( fuel_card(transaction) && (msg_type == MESSAGE_WOW_200) && transaction->u_PadData.PadData.NumberOfGroups )
	{
		wow_remap_fuelcard_rc( transaction->host_rc_asc );
	}

	if( VIM_ERROR_NONE == vim_mem_compare( transaction->host_rc_asc, "76", 2 ))
    {
        // remap any 76 code to a 00 as the transaction was actually approved
        vim_strcpy( transaction->host_rc_asc, "00" );
        if (!transaction_get_training_mode())
		{
			/* update keys after MAC error */
			DBG_MESSAGE( "!!!! SET TRICKLE FEED FLAG !!!!! ");
			  // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error	 
			terminal.last_logon_state = terminal.logon_state;		
			terminal.logon_state = session_key_logon;		
			terminal.trickle_feed = VIM_TRUE;
		}
    }

	// RDD 260312 - No Bank do manual. Don't reverse these as Txn got to switch
	else if( VIM_ERROR_NONE == vim_mem_compare( transaction->host_rc_asc, "91", 2 ))
	{
		vim_strcpy( transaction->host_rc_asc_backup, "91" );
	}

	// RDD 250516 BD says make 96 behave as 98
	else if( ( VIM_ERROR_NONE == vim_mem_compare(transaction->host_rc_asc, "98", 2 )) || ( VIM_ERROR_NONE == vim_mem_compare(transaction->host_rc_asc, "96", 2 )) )
	{
#ifdef _nnWIN32
		vim_strcpy( transaction->host_rc_asc, "00" );
		return VIM_ERROR_NONE;
#endif
		// RDD 180412 SPIRA:5333 Set to NMI 101 instead of 191 for "98"
        //terminal.logon_state = rsa_logon;
		//terminal.logon_state = session_key_logon;		
		//terminal.logon_state = logon_required;		
		//reversal_clear(VIM_FALSE);
		// RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error	  	
		terminal.last_logon_state = terminal.logon_state;		

		SetLogonState(msg_type, VIM_TRUE);

        // 260721 JIRA-PIRPIN 1118 VDA - ensure MAC error so SAF is not deleted
        if ((msg_type == MESSAGE_WOW_220) || (msg_type == MESSAGE_WOW_221) || (msg_type == MESSAGE_WOW_420) || (msg_type == MESSAGE_WOW_421) || ( msg_type == MESSAGE_WOW_200_VDA) || (msg_type == MESSAGE_WOW_200_VDAR ))
        {
            VIM_ERROR_RETURN( ERR_MAC_ERROR );			//BRD We want the 98 printed for 210 but need error code for Advices so they resend.
        }
        
	}
	else if( VIM_ERROR_NONE == vim_mem_compare(transaction->host_rc_asc, "11", 2 ))
	{
		// RDD 250712 SPIRA:5794 RC 11 in 8A needs to be transalated to 00 ( Only because the 5100 already does this )
		vim_strcpy( transaction->host_rc_asc_backup, transaction->host_rc_asc );
		vim_strcpy( transaction->host_rc_asc, "00" );
		DBG_MESSAGE( transaction->host_rc_asc_backup );
		DBG_MESSAGE( transaction->host_rc_asc );
	}
	else if( VIM_ERROR_NONE == vim_mem_compare(transaction->host_rc_asc, "Z3", VIM_SIZEOF(transaction->host_rc_asc)) )
	{
		vim_strcpy( transaction->host_rc_asc, "Q3" );
	}

	// RDD 110816 SPIRA:9036 This is probably unwise as we changed acc to credit and now offer optional Pin Entry
#if 0
	else if( VIM_ERROR_NONE == vim_mem_compare( transaction->host_rc_asc, "08", VIM_SIZEOF(transaction->host_rc_asc)) )
	{
		// Remap 08 -> 00 for WOW gift cards 
		if( gift_card() )
		{
			vim_strcpy( transaction->host_rc_asc, "00" );
		}
	}
#endif
	return VIM_ERROR_NONE;
}


/* Unpack message and make sure all required fields are present */
VIM_ERROR_PTR msgAS2805_parse(
  VIM_UNUSED(VIM_UINT8, host), 
  VIM_AS2805_MESSAGE_TYPE msg_type, 
  VIM_AS2805_MESSAGE *msg,
  transaction_t *transaction )
{
  /* Set msg build function */
  msg->msg_type = msg_type;

  VIM_UINT32 tx_msg_number = msg->msg_number;
  VIM_UINT32 rx_expected_msg_number = (tx_msg_number - (tx_msg_number % 10)) + 10;

  /* check if the message is expected to be encrypted */
  if (PCEftComm(host, attempt, VIM_FALSE) == VIM_FALSE)
  {
      VIM_CHAR AscBuffer[PCEFT_MAX_HOST_RESP_LEN * 2];
      vim_mem_clear(AscBuffer, VIM_SIZEOF(AscBuffer));
      hex_to_asc(msg->message, AscBuffer, 2 * msg->msg_size);
      pceft_debug(pceftpos_handle.instance, AscBuffer);
  }


  VIM_ERROR_RETURN_ON_FAIL( vim_AS2805_message_parse( msg ));

  // RDD - test out of seq. msg
#ifdef _DEBUG
  //msg->msg_number = 310;
#endif

  if (msg->msg_number != rx_expected_msg_number)
  {
      pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-2684 : Discard AS2805 Message and Wait for next");
      return(VIM_ERROR_MISMATCH);
  }

  VIM_DBG_DATA(msg->message, msg->msg_size);

  /* Check TID */
  if (msg->fields[41] != VIM_NULL)
  {
    if (vim_mem_compare(msg->fields[41], terminal.terminal_id, sizeof(terminal.terminal_id)) != VIM_ERROR_NONE)
    {
	  VIM_CHAR *tid_mismatch_rc = "X6";
      //DBG_DATA(terminal.terminal_id, sizeof(terminal.terminal_id));
      //DBG_DATA(msg->fields[41], sizeof(terminal.terminal_id));
	  vim_mem_copy( transaction->host_rc_asc, (VIM_UINT8 *)tid_mismatch_rc, 2 );
	  return MSG_ERR_CATID_MISMATCH;
    }
  }


  if(( msg->fields[42] != VIM_NULL) && (msg_type == MESSAGE_WOW_800_NMIC_191_M ))
  {
      if( vim_mem_compare( msg->fields[42], terminal.merchant_id, sizeof( terminal.merchant_id )) != VIM_ERROR_NONE )
      {
          VIM_CHAR CAIC[15 + 1];
          vim_mem_clear( CAIC, VIM_SIZEOF(CAIC) );
          vim_mem_copy( CAIC, msg->fields[42], VIM_SIZEOF(terminal.merchant_id ));
          vim_mem_copy( terminal.merchant_id, msg->fields[42], VIM_SIZEOF(terminal.merchant_id));
          vim_mem_copy( terminal.tms_parameters.merchant_id, msg->fields[42], VIM_SIZEOF(terminal.merchant_id));
          VIM_DBG_PRINTF1( "CAIC Changed to %s", CAIC );
          terminal_save();
      }
  }

  /* Check STAN */
  if (msg->fields[11] != VIM_NULL)
  {
    VIM_UINT32 received_stan;
    received_stan = (VIM_UINT32)bcd_to_bin(msg->fields[11], 6);

    if (msg->stan != received_stan)
    {
	  VIM_CHAR *stan_mismatch_rc = "X4";
      // DBG_VAR(msg->stan);
      // DBG_VAR(received_stan);
	  vim_mem_copy( transaction->host_rc_asc, (VIM_UINT8 *)stan_mismatch_rc, 2 );
	  return VIM_ERROR_AS2805_STAN_MISMATCH;
    }
  }

  /* Check MAC */
  if (message_has_mac(msg_type))
  {
    VIM_DES_BLOCK mac;
    
	// If MAC is missing altogether then something is horribly wrong !
    if ((VIM_NULL == msg->fields[64]) && (VIM_NULL == msg->fields[128]))
      return VIM_ERROR_AS2805_FORMAT;

    vim_mem_copy(&mac, &msg->message[msg->msg_size-VIM_SIZEOF(mac)], VIM_SIZEOF(mac));
#if 1	// change to zero to stuff MAC ...
    if (VIM_ERROR_NONE != vim_tcu_verify_mac(tcu_handle.instance_ptr,
                                             &mac,
                                             4,
                                             msg->message,msg->msg_size-VIM_SIZEOF(mac),
                                             VIM_TCU_MAC_KEY_KMAC))
#else
    if (VIM_ERROR_NONE != vim_tcu_verify_mac(tcu_handle.instance_ptr,
                                             &mac,
                                             4,
                                             msg->message,msg->msg_size-VIM_SIZEOF(mac),
                                             VIM_TCU_MAC_KEY_KMACH))
#endif	
    {
      /* update session keys */
	  vim_strcpy( transaction->host_rc_asc, "X7" );
	  // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error	  	
	  terminal.last_logon_state = terminal.logon_state;		

	  SetLogonState(msg_type, VIM_FALSE);
	  VIM_ERROR_RETURN( ERR_MAC_ERROR );
    }
  }
  

  /* do not update date and time if it is a reversal or advice message */

  if((MESSAGE_WOW_420 != msg_type)&&(MESSAGE_WOW_220 != msg_type)&&(MESSAGE_WOW_221!= msg_type)&&(MESSAGE_WOW_421!= msg_type))
  {
    /* Update date/time from response */
    if ((msg->fields[12] != VIM_NULL) && (msg->fields[13] != VIM_NULL))
    {
      VIM_RTC_TIME new_date;
      /* read existing date to get year */
      vim_rtc_get_time(&new_date);

      vim_utl_bcd_to_bin8(&msg->fields[12][0],2,&new_date.hour);
      vim_utl_bcd_to_bin8(&msg->fields[12][1],2,&new_date.minute);
      vim_utl_bcd_to_bin8(&msg->fields[12][2],2,&new_date.second);

      vim_utl_bcd_to_bin8(&msg->fields[13][0],2,&new_date.month);
      vim_utl_bcd_to_bin8(&msg->fields[13][1],2,&new_date.day);

      /* update rtc */
#ifndef _WIN32
      vim_rtc_set_time(&new_date);
#endif
      /* update transaction date and time */
      vim_mem_copy(&transaction->time, &new_date, sizeof(transaction->time));
    }    
  }
  
  if (msg->fields[15] != VIM_NULL)
  {
    /* Save settlement date */
    vim_mem_copy(transaction->settle_date, msg->fields[15], 2);


    // RDD 030123 JIRA PIRPIN-2056 standalone settlement clears shift totals

    // RDD 170621 JIRA PIRPIN-1038 : VOID over entire batch + extend batch
    TERM_CUTOVER = VIM_FALSE;
    if( vim_mem_compare( transaction->settle_date, terminal.last_settle_date, 2 ) != VIM_ERROR_NONE )
    {
        // Set the CUTOVER flag - only cleared once the batch has been deleted, either now or after last SAF has been sent. Either way, no voids can be allowed again until CUTOVER flag is cleared.
        pceft_debug( pceftpos_handle.instance, "Settlement Date was rolled Over - Reset totals and limits" );
        if (!TERM_USE_PCI_TOKENS)
        {
            TERM_CUTOVER = VIM_TRUE;

            // Reset the terminal totals at this point
            //shift_totals_reset(0);
			TERM_REFUND_DAILY_TOTAL = 0;
			terminal_save();

            /*saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);
            // CUTOVER - delete BATCH if no SAFs pending
            if (saf_totals.fields.saf_count)
            {
                pceft_debug(pceftpos_handle.instance, "Settlement Date Rolled Over: SAFs Pending");
            }
            else
            {
                saf_destroy_batch();
                pceft_debug(pceftpos_handle.instance, "Settlement Date Rolled Over: Deleted SAF File (if not alreadey deleted)");
            }*/
        }
    }
      
	// SPIRA:8985 v560 Settlement Date Issues - save last settle date here
    vim_mem_copy( terminal.last_settle_date, msg->fields[15], 2);
	
  }
  
  if (msg->fields[37] != VIM_NULL)
  {
      vim_mem_copy(transaction->rrn, msg->fields[37], 12);
  }  

  // RDD 130315 SPIRA:8342 - Diners does not return DE38 for Pre-auths. This field is optional so thats OK but ensure that we don't depend on it
  if( msg->fields[38] != VIM_NULL )
  {
      // RDD 120523 JIRA PIRPIN_2383 SLYP Authcode 
      if( TERM_USE_DE38 || msg_type == MESSAGE_WOW_100 )
      {
          /* Save auth number */
          vim_mem_copy(transaction->auth_no, msg->fields[38], 6);
      }
  }
	
  VIM_DBG_DATA(msg->fields[39], 2);
  if (msg->fields[39] != VIM_NULL)
  {
    // RDD Save and then remap RC according to WOW rules
	if(( msg_type == MESSAGE_WOW_221 )||( msg_type == MESSAGE_WOW_220 ))
	{
		// RDD: copy the response code into the backup so the original remains.
		// Need this to determine if original txn was declined offline or not
		// RDD 030113 - remove this next line !
		//vim_mem_copy( transaction->host_rc_asc_backup, transaction, 2 );
		vim_mem_clear( transaction->host_rc_asc, VIM_SIZEOF(transaction->host_rc_asc) );
		vim_mem_copy( transaction->host_rc_asc, msg->fields[39], 2 );

		// RDD 270815 SPIRA:8747 - ensure that original response code is always backed up
		//vim_strcpy( transaction->host_rc_asc_backup, transaction->host_rc_asc );

		VIM_ERROR_RETURN_ON_FAIL( wow_remap_rc( msg_type, transaction ) );
	}
	else
	{
		vim_mem_copy( transaction->host_rc_asc, msg->fields[39], 2 );
#if 1// TEST TEST TEST TEST 
		if( msg_type == MESSAGE_WOW_800_NMIC_101 )
		{
			DBG_MESSAGE( "Embedded 101" );			 
			//vim_strcpy( transaction->host_rc_asc, "96" );
		}
#endif
		VIM_ERROR_RETURN_ON_FAIL( wow_remap_rc( msg_type, transaction ) );
	}
  }

  // RDD 231012 SPIRA:6220 Need to generate an error if missing DE39
  else
  {
	vim_strcpy( transaction->host_rc_asc, "XB" );
	return MSG_ERR_FORMAT_ERR;
  }

  // Parse the WOW Merchant neame and address
  if( msg->fields[43] != VIM_NULL )
  {
      VIM_UINT8 *Ptr = msg->fields[43];
	  vim_mem_clear( terminal.configuration.merchant_name, VIM_SIZEOF(terminal.configuration.merchant_name) );
      vim_mem_copy( terminal.configuration.merchant_name, Ptr, WOW_NAME_LEN );

      Ptr+=WOW_NAME_LEN;
	  vim_mem_clear( terminal.configuration.merchant_address, VIM_SIZEOF(terminal.configuration.merchant_address) );
      vim_mem_copy(terminal.configuration.merchant_address, Ptr, WOW_ADDRESS_LEN );

      if (TERM_NZ_WOW)
      { 
          VIM_UINT8 SpaceCount = 0;
          VIM_CHAR TempMerchName[WOW_NAME_LEN + 2] = { 0 };
          VIM_CHAR* Ptr = &TempMerchName[0];
          VIM_CHAR* Ptr2 = &TempMerchName[0];
          VIM_CHAR* Ptr3 = terminal.configuration.merchant_name;
          vim_strncpy( TempMerchName, terminal.configuration.merchant_name, WOW_NAME_LEN );
          if(( vim_strstr( Ptr, "COUNTDOWN", &Ptr ) == VIM_ERROR_NONE ))
          {
              Ptr2 += vim_strlen("COUNTDOWN ");
              vim_strcpy( Ptr3, "WOOLWORTHS " );
              vim_strcat(Ptr3, Ptr2);                
          }
      }
  }
    
  /* Store the de55 field */
  if (msg->fields[55] != VIM_NULL)
  {
    VIM_TEXT tmp_buf[4];
    de55_length = 0;
    vim_mem_clear( tmp_buf, VIM_SIZEOF(tmp_buf));
    vim_mem_copy( tmp_buf, msg->fields[55], 3 );
     //VIM_DBG_MESSAGE( tmp_buf );
    de55_length = vim_atol( tmp_buf );
    if (de55_length)
	{
	  DBG_VAR( de55_length );
      vim_mem_copy(de55_data, &msg->fields[55][3], de55_length);	  
	}
  }
  /* Additional Response Data */
  if (msg->fields[44] != VIM_NULL)
  {
    /* Additional Response Data */
    if (tt_settle == transaction->type)
    {
      /* store the auto LFD download settings */
      if( msg->fields[44] )
      {
        VIM_UINT8 nlen;
        nlen = bcd_to_bin(&msg->fields[44][0],2);
        if( nlen > 7 )
        {
          VIM_DBG_DATA( msg->fields[44], 7 );
          terminal.tms_auto_download_settings.download_type = msg->fields[44][1];
          vim_mem_copy( terminal.tms_auto_download_settings.download_time, &msg->fields[44][2], 4 );
          vim_mem_copy( terminal.tms_auto_download_settings.version, &msg->fields[44][6], 2 );
          if( terminal.tms_auto_download_settings.download_type == 'P' ||
              terminal.tms_auto_download_settings.download_type == 'F' )
          {
            terminal.tms_auto_download_settings.status = VIM_FALSE;
            terminal.tms_auto_logon = VIM_TRUE;  /* change this auto logon flag */
            terminal_save();
          }
        }
      }
    }
    else
    {
      VIM_UINT8 nlen;
      nlen = bcd_to_bin(&msg->fields[44][0],2);
     //VIM_DBG_VAR(nlen);
      nlen = VIM_MIN(nlen, VIM_SIZEOF(transaction->additional_data_prompt)-1);
     //VIM_DBG_VAR(nlen);
      vim_mem_copy(transaction->additional_data_prompt, &msg->fields[44][1], nlen);
      VIM_DBG_DATA(&msg->fields[44][1], nlen);
      transaction->additional_data_prompt[nlen] = '\0';
      VIM_DBG_STRING(transaction->additional_data_prompt);
    }
  }
  /* Store Ledger and Cleared Balance */
  ProcessBalance( msg->fields[58], &transaction->LedgerBalance );
  ProcessBalance( msg->fields[59], &transaction->ClearedBalance );


/* Additional Data, National */
/*
  5. eToken (TOKbbbbbbinnnnnnnnnnnnn\), (used in „0210., „0230. and '0430' messages) The eToken is returned by the host in certain response messages. 
     The pinpad will log this value in the PCEFTPOS journal file along with the corresponding ePAN.
     a. bbbbbb - card BIN number.
     b. i - PCI Key Index
     c. nnnnnnnnnnnn - an incremental number controlled by the host.
     d. Note - 20 Zeroes are returned if there is any form of error at the host.
*/     

  if (VIM_NULL != msg->fields[47])
  {     
   //emv_data_update(msg->fields[47]);

	  // RDD 210212 - Copy dodgy action of 5100 by discarding tokens recieved in 91 response
	  if( vim_strncmp( transaction->host_rc_asc, "91", 2 ))
	  {
		VIM_ERROR_RETURN_ON_FAIL(process_wow_pci_data( msg, transaction ));
	  }
  }   
  
  /* Additional Data, Private - these are related for WOW as the NMIC (DE70) determines the processing of the private data (DE48) */
  if ((VIM_NULL != msg->fields[48]) /*&& (transaction->type != tt_settle )*/)
      VIM_ERROR_RETURN_ON_FAIL(process_wow_private_data( msg ));

  // RDD 290512 Process DE63 for FuelCard Transactions
  if ((VIM_NULL != msg->fields[63]) && ( fuel_card(transaction) ))
      VIM_ERROR_RETURN_ON_FAIL(process_wow_fuel_data( msg ));
#ifdef _WIN32
  //vim_strcpy( transaction->u_PadData.PadData.DE63_Response, "Hello there. This is an issuer message  from your friendly issuer. What do you  think you're playing at Donald? Buying  Toffee???");
  //vim_strcpy( transaction->u_PadData.PadData.DE63_Response, "Overlmt.  Card value   is         100.00Card Expiry                        05/15EFT Trans No                      012937");
#endif
        
  return VIM_ERROR_NONE;
}

// RDD 170513 added for SPIRA:6718
void SetResponseTimeStats( VIM_UINT64 elapsed_ticks )
{
	static VIM_SIZE total_timeout;
	//VIM_UINT8 n;

	// If the stats have been reset then clear the running total
	if( !terminal.stats.ResetStats )
	{
		total_timeout = 0;
		terminal.stats.ResetStats = VIM_TRUE;
	}		
	total_timeout += elapsed_ticks;

	if( terminal.stats.responses_received )
		terminal.stats.AverageResponseTime = ( total_timeout / terminal.stats.responses_received );

	// RDD 251013 - Bands changed for v420 (post Pilot) as 96% of txns were in first band
	if( elapsed_ticks < 1000 ) terminal.stats.ResponseTimes[0] += 1;
	if( 1000 <= elapsed_ticks && elapsed_ticks < 2000 ) terminal.stats.ResponseTimes[1] += 1;
	if( 2000 <= elapsed_ticks && elapsed_ticks < 4000 ) terminal.stats.ResponseTimes[2] += 1;
	if( 4000 <= elapsed_ticks && elapsed_ticks < 6000 ) terminal.stats.ResponseTimes[3] += 1;
	if( 6000 <= elapsed_ticks && elapsed_ticks < 10000 ) terminal.stats.ResponseTimes[4] += 1;
	if( elapsed_ticks >= 10000  ) terminal.stats.ResponseTimes[5] += 1;


}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR msgAS2805_transaction
(
  VIM_UINT8 host, 
  VIM_AS2805_MESSAGE *message_ptr, 
  VIM_AS2805_MESSAGE_TYPE message_type,
  transaction_t *transaction,
  VIM_BOOL fAllowInterrupt,
  VIM_BOOL fUpdateSafCount,
  VIM_BOOL fDisconnect
)
{
    VIM_BOOL SDA_Set = VIM_FALSE;
	VIM_RTC_UPTIME start_ticks,end_ticks;
	VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;
    VIM_SIZE AllowedMismatchMsgs = 1;

  // RDD 110512 SPIRA:5485 Comms connect fail on 220 prevents terminal stan from incrementing thus causing next 200 to go with same stan as 221
  if( message_type == MESSAGE_WOW_220 )
  {
	if(( res = comms_connect(host)) != VIM_ERROR_NONE ) 
	{
		// RDD: Increment the terminal.stan 
		terminal.stan += 1;
		return res;
	}
  }
  else
  {
	VIM_ERROR_RETURN_ON_FAIL( comms_connect(host));
  }

  if (( IsSDA(transaction)==VIM_TRUE ) && ((message_type == MESSAGE_WOW_220) || (message_type == MESSAGE_WOW_221)))
  {
      message_type = (message_type == MESSAGE_WOW_220) ? MESSAGE_WOW_200_VDA : MESSAGE_WOW_200_VDAR;
      SDA_Set = VIM_TRUE;
  }

	/* generate the message */
  VIM_ERROR_RETURN_ON_FAIL( msgAS2805_create(host,message_ptr,message_type));

  if( SDA_Set )
  {
      // Modify the message type and generate a pending reversal - update the original STAN first !
      reversal_store_original_stan(transaction, VIM_TRUE );
      reversal_set(transaction, VIM_TRUE );
  }

  if ((res = comms_transmit(host, message_ptr)) != VIM_ERROR_NONE)
  {
	  pceft_debug_error(pceftpos_handle.instance, "TX Msg failed with error:", res );
  }

  // RDD 010317 - Set the repeat flag here for the reversals 
  // RDD 280721 - JIRA PIRPIN-1171 Not a good idea to do it here as can get 421 instead of 420 comment this code out
#if 0
  if( reversal_present() )
  {
	  transaction_set_status( &reversal, st_repeat_saf );
	  reversal_set( &reversal );
  }
#endif

  vim_rtc_get_uptime(&start_ticks);

  // check for POS cancel key pressed or another PC EFT COMMAND Coming In
  transaction_clear_status( transaction, st_offline_pin );
  DBG_VAR( transaction->status );
  if( fAllowInterrupt )
  {	
	  VIM_ERROR_PTR error = pceft_pos_command_check( transaction, __1_SECOND__ );
	  if( error != VIM_ERROR_TIMEOUT )
	  {
		  VIM_DBG_ERROR( error );
		  return error;
	  }
  }

  /* get the response */
	  	// RDD 140312 Update SAF Count - note this can take time when the SAF is large and full...
  if( fUpdateSafCount )
		saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count );

  // RDD 190912 Clear Keymask while message is being sent
  //VIM_ERROR_IGNORE(vim_pceftpos_set_keymask(pceftpos_handle.instance, 0));
  ////////// TEST TEST TEST ///////////////
  //vim_event_sleep(5000);
  ////////// TEST TEST TEST ///////////////


  do
  {
      VIM_ERROR_RETURN_ON_FAIL(comms_receive(message_ptr, VIM_SIZEOF(message_ptr->message)));

      // RDD 170513 Get the lapsed time for the stats
      vim_rtc_get_uptime(&end_ticks);

      /* parse the response - now check for mismatch is response msg type */
      if ((res = msgAS2805_parse(host, message_type, message_ptr, transaction)) == VIM_ERROR_MISMATCH)
      {
          if (AllowedMismatchMsgs)
              continue;
          else
          {
              VIM_ERROR_RETURN_ON_FAIL(res);
          }
      }

      else
      {
         VIM_ERROR_RETURN_ON_FAIL(res);
      }

  } while (res == VIM_ERROR_MISMATCH && AllowedMismatchMsgs--);

  if (end_ticks > start_ticks)
  {
      VIM_DBG_NUMBER(end_ticks - start_ticks);
      SetResponseTimeStats(end_ticks - start_ticks);
  }

  // RDD 170513 Get the lapsed time for the stats
  vim_rtc_get_uptime(&end_ticks);

  if (end_ticks > start_ticks)
  {
	  //VIM_DBG_NUMBER(end_ticks - start_ticks);
	  SetResponseTimeStats(end_ticks - start_ticks);
  }


#if 0
  // RDD 111114 Force a disconnect after the txn has finished if PSTN txn
  if( CommsType( HOST_WOW ) == PSTN )
  {
	  if(( message_type != MESSAGE_WOW_800_NMIC_191 )&&( message_type != MESSAGE_WOW_800_NMIC_192 )&&( message_type != MESSAGE_WOW_800_NMIC_193 ))
	  {
		DBG_MESSAGE( "Force Disconnect" );
		comms_force_disconnect();
	  }
  }
#endif

  // JIRA PIRPIN-1571 Disconnect here causes issues with in-transaction reversal processing.
 // if ((!PCEftComm(HOST_WOW, attempt, VIM_FALSE)) && TERM_OPTION_PCEFT_DISCONNECT_ENABLED)
 // {
 //     comms_disconnect();
 // }
 // else
  {
      VIM_DBG_MESSAGE("disconnect disabled");
  }


  /* return without error */
  return VIM_ERROR_NONE;
}




