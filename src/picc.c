
/**
 * \file picc.c
 *
 * \brief
 * External contactless reader commands
 */

#include <incs.h>

 // RDD 170720 Trello #71 Diners - check if EPAT contains Diners by looking at count
#define VIM_MAX_CTLS_AIDS 10


extern VIM_BOOL IsEmvRID( const char *RID, transaction_t *transaction );

//extern VIM_ERROR_PTR vim_drv_picc_diners_cfg(VIM_PICC_EMV_PARAMETERS const* psParams);

static VIM_PICC_EMV_DISPLAY_MESSAGES const _messages =
{{
	VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x00",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x01",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x02",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x03",
//    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Thank You",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x04",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x05",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x06",
#if 0 // RDD 020716 - appears on some UPI cards. Probably unwanted
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "BALANCE " VIM_GRX_ESC_CURRENCY VIM_GRX_ESC_BALANCE,
#else
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x07",
#endif
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x08",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Please Tap Again",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Tap One Card",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x0B",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x0C",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x0D",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x0E",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x0F",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x10",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Refer to Phone",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Tap Again",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x13",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x14",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x15",
   // VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "Please Remove Card",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x16",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x17",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x18",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x19",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1A",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1B",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1C",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1D",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1E",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x1F",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x20",
    VIM_GRX_ESC_FONT_LARGE VIM_GRX_ESC_CENTER_ALIGN "INDEX 0x21",
}};

static VIM_PICC_EMV_PARAMETERS picc_parameters;

extern VIM_BOOL IsPosConnected( VIM_BOOL fWaitForPos );

VIM_ERROR_PTR picc_port_configure( VIM_BOOL fFullReset )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_UART_PARAMETERS picc_uart_parameters;

  /** com port */
  picc_uart_parameters.port = 0;
 //VIM_DBG_VAR(picc_uart_parameters.port);

  /** baud rate */
  picc_uart_parameters.baud_rate = VIM_UART_BAUD_115200;

  /** data bits */
  picc_uart_parameters.data_bits = VIM_UART_DATA_BITS_8;

  /** stop bits */
  picc_uart_parameters.stop_bits = VIM_UART_STOP_BITS_1;

  /** parity */
  picc_uart_parameters.parity = VIM_UART_PARITY_NONE;

  /** flow control */
  picc_uart_parameters.flow_control = VIM_UART_FLOW_CONTROL_NONE;

  VIM_DBG_MESSAGE( "open CTLS device" );
  vim_mem_clear( &picc_handle, VIM_SIZEOF(picc_handle));
  // RDD 120115 SPIRA:8333
      
  // RDD - diners
#if 1
  VIM_ERROR_RETURN_ON_FAIL( vim_picc_emv_open_reader( &picc_handle, &picc_uart_parameters, get_aid_count(VIM_TRUE) ));
#else
  VIM_ERROR_RETURN_ON_FAIL( vim_picc_emv_open_reader( &picc_handle, &picc_uart_parameters, VIM_MAX_CTLS_AIDS ));
#endif
  // RDD 010212 - only clear the parameters if full reset required
  if( fFullReset )
  {
	   //VIM_DBG_MESSAGE( "clear default parameters" );
	  res = vim_picc_emv_clear_parameters( picc_handle.instance, &picc_parameters );

	  // RDD 220914 v433_2
	  if(( res != VIM_ERROR_NONE ) && ( res != VIM_ERROR_ALREADY_INTIALIZED ))
		  VIM_ERROR_RETURN_ON_FAIL( res );
  }
  return VIM_ERROR_NONE;
}






#if 1
VIM_ERROR_PTR picc_generate_capk_checksum(VIM_EMV_CAPK *capk_ptr)
{
    VIM_UINT8 hash_data[300];
    VIM_SIZE hash_length=0;
    VIM_UINT8 tmp_index = 0x00;
    VIM_UINT8 expected_digest[20];

    /* add RID */
    vim_mem_copy( hash_data, &capk_ptr->RID, VIM_SIZEOF(capk_ptr->RID) );
    hash_length = VIM_SIZEOF(capk_ptr->RID);
    /* add public key index */
    tmp_index = capk_ptr->index;
    hash_data[hash_length++] = tmp_index;
    /* add modulus */
    vim_mem_copy( &hash_data[hash_length], capk_ptr->modulus, capk_ptr->modulus_size );
    hash_length += capk_ptr->modulus_size;

    /* add exponent */
    vim_mem_copy( &hash_data[hash_length], capk_ptr->exponent, capk_ptr->exponent_size );
    hash_length += capk_ptr->exponent_size;
    vim_sha1_digest(expected_digest,VIM_SIZEOF(expected_digest),hash_data,hash_length);

    /* copy back */
    vim_mem_copy( capk_ptr->hash, expected_digest, 20 );

    return VIM_ERROR_NONE;
}
#endif
/*----------------------------------------------------------------------------*/
/**
 * Load default and LFD parameters into external contactless reader
 */
/*----------------------------------------------------------------------------*/

#if 1

VIM_ERROR_PTR picc_load_parameters( VIM_BOOL Level )
{
  VIM_SIZE aid_count, capk_count;
  VIM_ERROR_PTR result;
  VIM_PICC_EMV_AID_DATA *paid = VIM_NULL;
  VIM_EMV_CAPK *pcapk = VIM_NULL;

  VIM_DBG_WARNING("WWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");

  // RDD 010212 - only reset the parameters if required
  if( !Level )
  {
    return VIM_ERROR_NONE;
  }

  // RDD 160514 added
  picc_parameters.default_country_code = bcd_to_bin( terminal.emv_default_configuration.country_code, 4);
  picc_parameters.default_currency_code = bcd_to_bin( (VIM_UINT8 *)terminal.emv_default_configuration.txn_reference_currency_code, 4 );

  picc_parameters.reader_transaction_limit = CONTACTLESS_READER_LIMIT;

  /* common application parameters */
  picc_parameters.use_signal_tone = VIM_FALSE;

  /* VSDC parameters, taken from test example */
  picc_parameters.qVSDC.MSD_supported = VIM_TRUE;
  picc_parameters.qVSDC.qVSDC_supported = VIM_TRUE;

  /* get a counter of supported applications */
  picc_parameters.AID_count = aid_count = get_aid_count(VIM_TRUE);
  VIM_DBG_MESSAGE( "================ LOADING CTLS PARAMETERS =================" );
  VIM_DBG_VAR(aid_count);

  if (aid_count)
  {
    VIM_SIZE i, j;

    /* allocate a piece of memory */
    VIM_MEM_NEW_ARRAY(paid, VIM_PICC_EMV_AID_DATA, aid_count);

    /* clear the buffer */
    vim_mem_clear(paid, VIM_SIZEOF(VIM_PICC_EMV_AID_DATA) * aid_count);

    /* set pointer to data */
    picc_parameters.AID_data = paid;

    /* get information about all available applications */
    for (i = j = 0; (j < aid_count) && (i < MAX_EMV_APPLICATION_ID_SIZE); i++)
    {
      if (!terminal.emv_applicationIDs[i].contactless) continue;

      paid->AID.size = terminal.emv_applicationIDs[i].aid_length;

      vim_mem_copy(paid->AID.value.RID.value, terminal.emv_applicationIDs[i].emv_aid, 5);

      vim_mem_copy(paid->AID.value.proprietary,
                  &terminal.emv_applicationIDs[i].emv_aid[5],
                  VIM_SIZEOF(paid->AID.value.proprietary));


      paid->threshold = terminal.emv_applicationIDs[i].threshold;
      paid->maximum_target_percentage = terminal.emv_applicationIDs[i].max_target;
      paid->target_percentage = terminal.emv_applicationIDs[i].tagert_percent;
      vim_mem_copy(paid->default_DDOL, terminal.emv_applicationIDs[i].ddol_default, VIM_SIZEOF(paid->default_DDOL));

      vim_mem_copy(paid->default_TDOL, terminal.emv_applicationIDs[i].tdol_default,VIM_SIZEOF(paid->default_TDOL));

	  // RDD 251115 This wasn't being copied over ! Needed for New Paypass 3 test for PPMCD 02 where Tag 9F1D needs a specific value
	  vim_mem_copy(paid->RMP_9F1D, terminal.emv_applicationIDs[i].RMP_9F1D,VIM_SIZEOF(paid->RMP_9F1D));

      /* set first two bytes, the rest of UDOL is 0 */
      vim_mem_copy(paid->TAC_default, terminal.emv_applicationIDs[i].tac_default, VIM_SIZEOF(paid->TAC_default));
      vim_mem_copy(paid->TAC_denial, terminal.emv_applicationIDs[i].tac_denial, VIM_SIZEOF(paid->TAC_denial));
      vim_mem_copy(paid->TAC_online, terminal.emv_applicationIDs[i].tac_online, VIM_SIZEOF(paid->TAC_online));

	  // RDD 060214 added VISA TTQ as this too get downloaded from TM
	  vim_mem_copy( paid->ttq, terminal.emv_applicationIDs[i].visa_ttq, VIM_SIZEOF(paid->ttq) );

	  // RDD 120215 v541 unlikely to change this but if its there then copy over...
	  vim_mem_copy( paid->RMP_9F1D, terminal.emv_applicationIDs[i].RMP_9F1D, VIM_SIZEOF(paid->RMP_9F1D) );

      vim_mem_copy(paid->terminal_version_number,
          terminal.emv_applicationIDs[i].app_version1,
                   VIM_SIZEOF(terminal.emv_applicationIDs[i].app_version1));

      paid->transaction_limit = terminal.emv_applicationIDs[i].contactless_txn_limit;
	  //paid->transaction_limit = 9999999;
      paid->floor_limit = terminal.emv_applicationIDs[i].floor_limit;
      //paid->floor_limit = 2000;
      paid->CVM_required_limit = terminal.emv_applicationIDs[i].cvm_limit;
	  //paid->CVM_required_limit = 6000;
	  // RDD 220616 added for UPI
	  paid->CVM_international_limit = terminal.emv_applicationIDs[i].cvm_international_limit;

       DBG_MESSAGE( "=============== CONTACTLESS limits:===================" );
      VIM_DBG_DATA( &paid->AID.value, paid->AID.size );
	  VIM_DBG_NUMBER(paid->CVM_international_limit);
		VIM_DBG_NUMBER(paid->CVM_required_limit);
		VIM_DBG_NUMBER(paid->transaction_limit);
      VIM_DBG_NUMBER(paid->floor_limit );
       DBG_MESSAGE( "=============== END CONTACTLESS limits:===================" );
#if 0
      paid->customer_application_selection_ID = emv_aid_table.entry[i].selector_index;
#else
      paid->customer_application_selection_ID = 0;
#endif

      paid->card_provider_identifier_present = VIM_FALSE;

      paid->card_provider_identifier = 0x02;

      paid->payment_scheme_to_use_present = VIM_FALSE;

      paid->payment_scheme_to_use = 0x22;

      paid->fDDA_supported_version = 0x0;

      /* country code */
      vim_utl_bcd_to_bin16(terminal.emv_default_configuration.country_code,VIM_SIZEOF(terminal.emv_default_configuration.country_code)*2,&paid->country_code);

      /* currency code */
      vim_utl_bcd_to_bin16((VIM_UINT8 *)terminal.emv_default_configuration.txn_reference_currency_code,VIM_SIZEOF(terminal.emv_default_configuration.txn_reference_currency_code)*2,&paid->currency_code);

      /* currency exponent */
      paid->currency_exponent = terminal.emv_default_configuration.txn_reference_currency_exponent;
      /* terminal capabilities: set LTE TCC as default, this will be reset during transaction */
      //vim_mem_copy( paid->terminal_capabilities, terminal.emv_applicationIDs[i].TCC_LTE_CVM_limit, 3 );

	  // RDD 230812 - Try not resetting limit at start of txn, but waiting for reader firmware to make this decision for us
//#ifdef VIM_BUILD_debug
	  // RDD 271113 - need to use the values supplied by TM_TMS
#if 0
	  // RDD 170113 - Fix this to something that works for now
      vim_mem_copy( paid->terminal_capabilities, "\x00\x68\x88", 3 );
      vim_mem_copy( paid->additional_capabilities, "\x00\x00\x00\x80\x00", 5 );
#else

	  // RDD 050814 SPIRA: ???? Problems with Amex Txn limits were caused by incorrect TCC being loaded
	  // RDD 110115 SPIRA:8317 Fixed issues in EPAT so use term cap
#if 0
	  vim_mem_copy( paid->terminal_capabilities, terminal.emv_applicationIDs[i].TCC_GT_CVM_limit, 3);
#else
	  vim_mem_copy( paid->terminal_capabilities, terminal.emv_applicationIDs[i].term_cap, 3 );
#endif
      //vim_mem_copy( paid->additional_capabilities, terminal.emv_default_configuration.additional_terminal_capabilities,5);
	  // RDD 271113 - I put this into the AID struct so we can change for an individual AID if necessary

	  // vim_mem_copy( paid->additional_capabilities, terminal.emv_applicationIDs[i].add_term_cap, 5 );
#if 0
	  vim_mem_copy( paid->additional_capabilities, terminal.emv_default_configuration.additional_terminal_capabilities, 5 );
#else
	  // SPIRA:8290 Additional Term Cap added per AID
	  vim_mem_copy( paid->additional_capabilities, terminal.emv_applicationIDs[i].add_term_cap, 5 );
#endif

#endif
	  // RDD 191014 - noticed that this was hard coded - need other value for Standalone !
#if 1
      paid->terminal_type = terminal.emv_default_configuration.terminal_type;
#else
      paid->terminal_type = 0x22;
#endif

	  // RDD 140814 Apparently ther real value of this is needed for EPAL...
	  // RDD 191014 Use BCD Native Value
#if 0
      paid->merchant_category_code = bcd_to_bin(terminal.mcc, 4);
#else
	  //paid->merchant_category_code = bcd_to_bin(terminal.emv_default_configuration.merchant_category_code_bcd, 4);
	  vim_mem_copy( paid->merchant_category_code, terminal.emv_default_configuration.merchant_category_code_bcd, 2);
	  DBG_MESSAGE( "Set Catagory Codes from EPAT - MCC=0x009F, TCC=0x52" );
	  DBG_VAR( paid->merchant_category_code );
#endif

      /* set transaction category code for Masterard */
	  // RDD 140814 Brian wanted this changed back to the value that comes over in the EPAT rather than the hardcoded value
#if 1
      paid->transaction_category_code = terminal.emv_applicationIDs[i].mc_txn_category_code;
#else
	  paid->transaction_category_code = terminal.emv_applicationIDs[i].category_code;
#endif
	  DBG_VAR( paid->transaction_category_code );
      paid->POS_entry_mode = 22;

      paid->acquirer_identifier_size = 6;
      /* TODO: how to set acquirer */
      vim_mem_copy(paid->acquirer_identifier, "\x00\x00\x00\x56\x01\x92", 6);
      vim_mem_copy(paid->CAIC, terminal.tms_parameters.merchant_id, VIM_SIZEOF(paid->CAIC));
	  vim_mem_copy(paid->CATID, terminal.tms_parameters.terminal_id, VIM_SIZEOF(paid->CATID));
      paid++;
      j++;
    }
  }
  else
  {
	  DBG_MESSAGE( "PICC OPEN FAILED - NO CTLS AID PARAMS" );
	  return VIM_ERROR_NOT_FOUND;
  }
  /* get a counter of supported applications */
  picc_parameters.key_count = capk_count = get_capk_count();

   VIM_DBG_MESSAGE( "================" );
 VIM_DBG_VAR( capk_count );

  if (capk_count)
  {
    /* allocate a piece of memory */
    VIM_MEM_NEW_ARRAY(pcapk, VIM_EMV_CAPK, capk_count);

    /* clear the buffer */
    vim_mem_clear(pcapk, VIM_SIZEOF(VIM_EMV_CAPK) * capk_count);

    /* set pointer to data */
    picc_parameters.keys = pcapk;

    /* get information about all available keys */
    while (capk_count--)
    {
      vim_mem_copy( pcapk->RID.value, terminal.emv_capk[capk_count].capk.RID.value, 5);
      VIM_DBG_DATA(pcapk->RID.value, 5);
      pcapk->expiry_date = terminal.emv_capk[capk_count].capk.expiry_date;
      vim_mem_copy( pcapk->exponent, terminal.emv_capk[capk_count].capk.exponent, VIM_SIZEOF(terminal.emv_capk[capk_count].capk.exponent));
      pcapk->exponent_size = terminal.emv_capk[capk_count].capk.exponent_size;
      pcapk->index = terminal.emv_capk[capk_count].capk.index;
      VIM_DBG_VAR(pcapk->index);
      vim_mem_copy( pcapk->modulus, terminal.emv_capk[capk_count].capk.modulus, VIM_SIZEOF(terminal.emv_capk[capk_count].capk.modulus));
      pcapk->modulus_size = terminal.emv_capk[capk_count].capk.modulus_size;

      /* generate checksum */
      //VIM_DBG_WARNING("PKTPKTPKTPKTPKTPKTPKTPKTPKTPKTPKTPKTPKTPKT");

      picc_generate_capk_checksum(pcapk);

      if (capk_count)
        pcapk++;
    }
  }

  picc_parameters.messages = &_messages;


  // VIM Sets CAPK and all EMV schemes specific stuff except Diners
  result = vim_picc_emv_set_parameters( picc_handle.instance, &picc_parameters, Level );

  {
      VIM_SIZE ctls_aid_count = get_aid_count(VIM_TRUE);
      VIM_DBG_NUMBER(ctls_aid_count);

#ifndef _WIN32
      // do the unique diners setup stuff in the unique diners vim file
      if (ctls_aid_count > VIM_MAX_CTLS_AIDS)
      {
         //vim_drv_picc_diners_cfg(&picc_parameters);
      }
#endif
  }

  if (paid)
    /* release memory */
    vim_mem_delete((void *) picc_parameters.AID_data);

  if (pcapk)
    /* release memory */
    vim_mem_delete((void *) picc_parameters.keys);

  return result;
}

#else


VIM_ERROR_PTR picc_load_parameters( VIM_UINT8 Level )
{
  VIM_SIZE aid_count, capk_count;
  VIM_ERROR_PTR result;
  VIM_PICC_EMV_AID_DATA *paid = VIM_NULL;
  VIM_EMV_CAPK *pcapk = VIM_NULL;

  // RDD 010212 - only reset the parameters if required
  if( !Level )
  {
    return VIM_ERROR_NONE;
  }

  // RDD 280714 added (used in WBC)
  picc_parameters.default_country_code = bcd_to_bin( terminal.emv_default_configuration.country_code, 4);
  picc_parameters.default_currency_code = bcd_to_bin( terminal.emv_default_configuration.txn_reference_currency_code, 4 );

  picc_parameters.reader_transaction_limit = CONTACTLESS_READER_LIMIT;

  /* common application parameters */
  picc_parameters.use_signal_tone = VIM_FALSE;

  /* VSDC parameters, taken from test example */
  picc_parameters.qVSDC.MSD_supported = VIM_TRUE;
  picc_parameters.qVSDC.qVSDC_supported = VIM_TRUE;

  /* get a counter of supported applications */
  picc_parameters.AID_count = aid_count = get_aid_count(VIM_TRUE);
  VIM_DBG_MESSAGE( "================ LOADING CTLS PARAMETERS =================" );
  VIM_DBG_VAR(aid_count);

  if (aid_count)
  {
    VIM_SIZE i, j;

    /* allocate a piece of memory */
    VIM_MEM_NEW_ARRAY(paid, VIM_PICC_EMV_AID_DATA, aid_count);

    /* clear the buffer */
    vim_mem_clear(paid, VIM_SIZEOF(VIM_PICC_EMV_AID_DATA) * aid_count);

    /* set pointer to data */
    picc_parameters.AID_data = paid;

    /* get information about all available applications */
    for (i = j = 0; (j < aid_count) && (i < MAX_EMV_APPLICATION_ID_SIZE); i++)
    {
		VIM_UINT8 *RidPtr = paid->AID.value.RID.value;

		if (!terminal.emv_applicationIDs[i].contactless) continue;

		paid->AID.size = terminal.emv_applicationIDs[i].aid_length;
		vim_mem_copy( RidPtr, terminal.emv_applicationIDs[i].emv_aid, 5 );

		vim_mem_copy( paid->AID.value.proprietary, &terminal.emv_applicationIDs[i].emv_aid[5], VIM_SIZEOF(paid->AID.value.proprietary ));


		paid->threshold = terminal.emv_applicationIDs[i].threshold;
		paid->maximum_target_percentage = terminal.emv_applicationIDs[i].max_target;
		paid->target_percentage = terminal.emv_applicationIDs[i].tagert_percent;

		vim_mem_copy(paid->default_DDOL, terminal.emv_applicationIDs[i].ddol_default, VIM_SIZEOF(paid->default_DDOL));

		vim_mem_copy(paid->default_TDOL, terminal.emv_applicationIDs[i].tdol_default,VIM_SIZEOF(paid->default_TDOL));


		/* set first two bytes, the rest of UDOL is 0 */
		vim_mem_copy(paid->TAC_default, terminal.emv_applicationIDs[i].tac_default, VIM_SIZEOF(paid->TAC_default));
		vim_mem_copy(paid->TAC_denial, terminal.emv_applicationIDs[i].tac_denial, VIM_SIZEOF(paid->TAC_denial));
		vim_mem_copy(paid->TAC_online, terminal.emv_applicationIDs[i].tac_online, VIM_SIZEOF(paid->TAC_online));

		// RDD 060214 added VISA TTQ as this too get downloaded from TM
		vim_mem_copy( paid->visa_ttq, terminal.emv_applicationIDs[i].visa_ttq, VIM_SIZEOF(paid->visa_ttq) );

#if 1	// RDD v430
      vim_mem_copy(paid->terminal_version_number,
                   terminal.emv_applicationIDs[i].app_version1,
                   VIM_SIZEOF(paid->terminal_version_number));
#else
      paid->terminal_version_number[0] = 1;
      paid->terminal_version_number[1] = 5;
#endif

      paid->transaction_limit = 9999999;
      //paid->transaction_limit = terminal.emv_applicationIDs[i].contactless_txn_limit;
      paid->floor_limit = terminal.emv_applicationIDs[i].floor_limit;
      //paid->floor_limit = 2000;
      paid->CVM_required_limit = terminal.emv_applicationIDs[i].cvm_limit;

       DBG_MESSAGE( "=============== CONTACTLESS limits:===================" );
      VIM_DBG_DATA( &paid->AID.value, paid->AID.size );
	  if( paid->CVM_required_limit < 10000 )
		VIM_DBG_NUMBER(paid->CVM_required_limit);
      VIM_DBG_NUMBER(paid->transaction_limit);
      VIM_DBG_NUMBER(paid->floor_limit );
       DBG_MESSAGE( "=============== END CONTACTLESS limits:===================" );
#if 0
      paid->customer_application_selection_ID = emv_aid_table.entry[i].selector_index;
#else
      paid->customer_application_selection_ID = 0;
#endif

      paid->card_provider_identifier_present = VIM_FALSE;

      paid->card_provider_identifier = 0x02;

      paid->payment_scheme_to_use_present = VIM_FALSE;

      paid->payment_scheme_to_use = 0x22;

      paid->fDDA_supported_version = 0x0;

      /* country code */
      vim_utl_bcd_to_bin16(terminal.emv_default_configuration.country_code,VIM_SIZEOF(terminal.emv_default_configuration.country_code)*2,&paid->country_code);

      /* currency code */
      vim_utl_bcd_to_bin16(terminal.emv_default_configuration.txn_reference_currency_code,VIM_SIZEOF(terminal.emv_default_configuration.txn_reference_currency_code)*2,&paid->currency_code);

      /* currency exponent */
      paid->currency_exponent = terminal.emv_default_configuration.txn_reference_currency_exponent;
      /* terminal capabilities: set LTE TCC as default, this will be reset during transaction */
      //vim_mem_copy( paid->terminal_capabilities, terminal.emv_applicationIDs[i].TCC_LTE_CVM_limit, 3 );

	  // RDD 230812 - Try not resetting limit at start of txn, but waiting for reader firmware to make this decision for us
//#ifdef _DEBUG
	  // RDD 271113 - need to use the values supplied by TM_TMS
#if 0
	  // RDD 170113 - Fix this to something that works for now
      vim_mem_copy( paid->terminal_capabilities, "\x00\x68\x88", 3 );
      vim_mem_copy( paid->additional_capabilities, "\x00\x00\x00\x80\x00", 5 );
#else
	  vim_mem_copy( paid->terminal_capabilities, terminal.emv_applicationIDs[i].term_cap, 3 );
      //vim_mem_copy( paid->additional_capabilities, terminal.emv_default_configuration.additional_terminal_capabilities,5);
	  // RDD 271113 - I put this into the AID struct so we can change for an individual AID if necessary
	  vim_mem_copy( paid->additional_capabilities, terminal.emv_applicationIDs[i].add_term_cap, 5 );
#endif
      /* terminal additional capabilities */

#if 0
      vim_utl_bcd_to_bin8(&terminal.emv_default_configuration.terminal_type,VIM_SIZEOF(terminal.emv_default_configuration.terminal_type)*2,&paid->terminal_type);
#else
      paid->terminal_type = 0x22;
#endif

      paid->merchant_category_code = bcd_to_bin(terminal.mcc, 4);

      /* set transaction category code for Masterard */
      //paid->transaction_category_code = terminal.emv_applicationIDs[i].mc_txn_category_code;
      paid->transaction_category_code = terminal.emv_applicationIDs[i].category_code;

      paid->POS_entry_mode = 22;

      paid->acquirer_identifier_size = 6;
      /* TODO: how to set acquirer */
      vim_mem_copy(paid->acquirer_identifier, "\x00\x00\x56\x01\x92", 5);
      vim_mem_copy(paid->CAIC, terminal.tms_parameters.merchant_id, VIM_SIZEOF(paid->CAIC));
	  vim_mem_copy(paid->CATID, terminal.tms_parameters.terminal_id, VIM_SIZEOF(paid->CATID));
      paid++;
      j++;
    }
  }
  else
  {
	  DBG_MESSAGE( "PICC OPEN FAILED - NO CTLS AID PARAMS" );
	  return VIM_ERROR_NOT_FOUND;
  }
  /* get a counter of supported applications */
  picc_parameters.key_count = capk_count = get_capk_count();

   //VIM_DBG_MESSAGE( "================" );
 //VIM_DBG_VAR( capk_count );

  if (capk_count)
  {
    /* allocate a piece of memory */
    VIM_MEM_NEW_ARRAY(pcapk, VIM_EMV_CAPK, capk_count);

    /* clear the buffer */
    vim_mem_clear(pcapk, VIM_SIZEOF(VIM_EMV_CAPK) * capk_count);

    /* set pointer to data */
    picc_parameters.keys = pcapk;

    /* get information about all available keys */
    while (capk_count--)
    {
      vim_mem_copy( pcapk->RID.value, terminal.emv_capk[capk_count].capk.RID.value, 5);
      pcapk->expiry_date = terminal.emv_capk[capk_count].capk.expiry_date;
      vim_mem_copy( pcapk->exponent, terminal.emv_capk[capk_count].capk.exponent, VIM_SIZEOF(terminal.emv_capk[capk_count].capk.exponent));
      pcapk->exponent_size = terminal.emv_capk[capk_count].capk.exponent_size;
      pcapk->index = terminal.emv_capk[capk_count].capk.index;
      vim_mem_copy( pcapk->modulus, terminal.emv_capk[capk_count].capk.modulus, VIM_SIZEOF(terminal.emv_capk[capk_count].capk.modulus));
      pcapk->modulus_size = terminal.emv_capk[capk_count].capk.modulus_size;
      /* generate checksum  - only for woolworths, others supply checksum */
#if 0
	  vim_mem_copy( pcapk->hash, terminal.emv_capk[capk_count].capk.hash, 20 );
#else
      picc_generate_capk_checksum(pcapk);
#endif
      if (capk_count)
        pcapk++;
    }
  }

  picc_parameters.messages = &messages;


  result = vim_picc_emv_set_parameters( picc_handle.instance, &picc_parameters, Level );

  if (paid)
    /* release memory */
    vim_mem_delete((void *) picc_parameters.AID_data);

  if (pcapk)
    /* release memory */
    vim_mem_delete((void *) picc_parameters.keys);

  return result;
}

#endif

VIM_ERROR_PTR picc_display_fw_ver( void )
{
	VIM_CHAR ver_buf[100];
	VIM_SIZE ver_size = 0;

	vim_mem_clear( ver_buf,VIM_SIZEOF(ver_buf) );

	VIM_ERROR_RETURN_ON_FAIL( vim_picc_emv_get_firmware_version (picc_handle.instance, ver_buf, VIM_SIZEOF(ver_buf), &ver_size));
	DBG_MESSAGE( ver_buf );

	if( ver_size > 0 )
	{
#ifndef _WIN32
		// RDD 070219 - no need to skip first 8 bytes for P400        
		display_screen( DISP_CTLS_INIT, VIM_PCEFTPOS_KEY_NONE, ver_buf );
#else	// WIN32 SIM
		display_screen( DISP_CTLS_INIT, VIM_PCEFTPOS_KEY_NONE, &ver_buf[8] );
#endif
        terminal_set_status(ss_ctls_reader_open);
	}
	else
	{
		display_screen( DISP_CTLS_INIT, VIM_PCEFTPOS_KEY_NONE, " " );
	}
	vim_event_sleep( 500 );
	return VIM_ERROR_NONE;
}

#if 0
static VIM_ERROR_PTR CheckCtlsVersionChange( VIM_BOOL *FullReset )
{
	VIM_CHAR VerBuff[FIRMWARE_NAME_SIZE];
	VIM_SIZE VerSize = 0;

	vim_mem_clear( VerBuff, VIM_SIZEOF( VerBuff ) );

	// If the firmware returns a version and the version is the same as the version saved in the terminal struct then it hasn't changed
	if( vim_picc_emv_get_firmware_version( picc_handle.instance, VerBuff, VIM_SIZEOF(VerBuff), &VerSize ) == VIM_ERROR_NONE )
	{
		DBG_MESSAGE( VerBuff );
		DBG_MESSAGE( terminal.contactless_file_name );
		if( vim_strncmp( VerBuff, terminal.contactless_file_name, FIRMWARE_NAME_SIZE ) )
		{
			DBG_MESSAGE( "<<<<< New Contactless Driver Detected >>>>>>" );
			vim_mem_clear( terminal.contactless_file_name, VIM_SIZEOF( terminal.contactless_file_name ) );
			vim_strcpy( terminal.contactless_file_name, VerBuff );
			terminal_save();
			*FullReset = VIM_TRUE;
		}
	}
	return VIM_ERROR_NONE;
}
#endif

/**
 * Open external contactless reader
 */
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR picc_open( VIM_UINT8 Level, VIM_BOOL fDisplayVersion )
{
  VIM_ERROR_PTR error = VIM_ERROR_NONE;
  VIM_BOOL fFullReset = ( Level > PICC_MESSAGES_ONLY ) ? VIM_TRUE : VIM_FALSE;

    DBG_MESSAGE(" ======= OPEN PICC READER ======= " );

	if( terminal_get_status( ss_ctls_reader_open ) )
	{
		VIM_DBG_WARNING( "READER ALREADY OPEN" );
		return VIM_ERROR_NONE;
	}

  /* close first */
  //if( picc_handle.instance != VIM_NULL )

#if 0 // RDD 040314 - this takes too much time
    picc_close(VIM_TRUE);
#endif

  // RDD - this actually creates the instance so can read version after this point and display
  /* open contactless reader */

  // RDD 070217 added for JIRA WWBP-130 CTLS dies after return to present card (no cancel)
  if(( error = picc_port_configure( fFullReset )) != VIM_ERROR_NONE )
  {
#if 0
	  if( error == VIM_ERROR_ALREADY_INTIALIZED )
	  {
		  terminal_set_status( ss_ctls_reader_open );
		  VIM_DBG_MESSAGE("already Initialised")
		  return VIM_ERROR_NONE;
	  }
#endif
	  picc_close(VIM_TRUE);
	  if(( error = picc_port_configure( fFullReset )) != VIM_ERROR_NONE )
	  {
		vim_pceftpos_debug_error( pceftpos_handle.instance, "CTLS Port Config Error", error );
		terminal_clear_status(ss_ctls_reader_open);

		DBG_MESSAGE( "CTLS Open Failed - Set Fatal error" );
		// RDD 180814 added so we can continue if no contactless comms
		terminal_set_status( ss_ctls_reader_fatal_error );

	    return VIM_ERROR_SYSTEM;
	  }
  }
  /* debug port open status */

  // RDD 310316 removed for v560
#ifndef _nnWIN32
  //CheckCtlsVersionChange( &fFullReset);
#endif

  if( fDisplayVersion )
	VIM_ERROR_IGNORE( picc_display_fw_ver( ) );
  /* loading parameters */
#if 1
  if(( error = picc_load_parameters( Level )) != VIM_ERROR_NONE )
  {
	  vim_pceftpos_debug_error( pceftpos_handle.instance, "CTLS Open: Param Load Error", error );
	  return error;
  }
#endif
  terminal_set_status(ss_ctls_reader_open);
  VIM_DBG_WARNING( "READER NOW OPEN - FLAG SET" );
  //pceft_debug_test( pceftpos_handle.instance, "CTLS Reader Opened OK" );
  return VIM_ERROR_NONE;
}



#if 1
// RDD 100912 Get Issuer Discretionary Data
VIM_ERROR_PTR get_loyalty_iad(transaction_t *transaction)
{

  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[256];

  VIM_TEXT LoyaltyNumber[100];
  VIM_SIZE Index = CardIndex(transaction);

  VIM_TLV_TAG_PTR PiccTags[] = { VIM_TAG_EMV_ISSUER_APPLICATION_DATA };
  VIM_TLV_TAG_PTR EmvTags[] = { RIMU_FCI_IDD_TAG };

  // RDD 110912 It might be a RIMU card

  vim_mem_clear( LoyaltyNumber, VIM_SIZEOF(LoyaltyNumber) );

  pceft_debug( pceftpos_handle.instance, "In Loyalty IAD function" );

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_create( &tags_list, tlv_buffer, sizeof(tlv_buffer) ));

  pceft_debug( pceftpos_handle.instance, "Created Loyalty tag buffer" );

if( transaction->card.type == card_chip )
  {
	  pceft_debug( pceftpos_handle.instance, "ICC Searching for Tag" );
	  VIM_DBG_POINT;

	  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");
	  DBG_VAR( txn.emv_data.data_size );
	  DBG_DATA(txn.emv_data.buffer, txn.emv_data.data_size);
	  DBG_MESSAGE( "!!!!!!!!!!!!!!! TLV BUFFER !!!!!!!!!!!!!!!!");

	  VIM_ERROR_RETURN_ON_FAIL( vim_emv_read_kernel_tags( (const VIM_TAG_ID *)EmvTags, VIM_LENGTH_OF(EmvTags), &tags_list ));

	  VIM_DBG_TLV(tags_list);
	  VIM_DBG_POINT;
	  //VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( &tags_list, VIM_TAG_EMV_ISSUER_APPLICATION_DATA ));
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( &tags_list, RIMU_FCI_IDD_TAG ));
	  VIM_DBG_POINT;
  }
  else if( transaction->card.type == card_picc )
  {
	  pceft_debug( pceftpos_handle.instance, "PICC Searching for Tag" );
	  VIM_ERROR_RETURN_ON_FAIL( vim_picc_emv_get_tags( picc_handle.instance, PiccTags, VIM_LENGTH_OF(PiccTags), &tags_list) );
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( &tags_list, VIM_TAG_EMV_ISSUER_APPLICATION_DATA ));
  }

  VIM_DBG_POINT;
  VIM_DBG_TLV(tags_list);

  pceft_debug( pceftpos_handle.instance, "Found Loyalty number Tag" );

  if( tags_list.current_item.length )
  {
      VIM_UINT8 Buffer[100];
	  VIM_CHAR TextData[100];
	  VIM_CHAR DataBuff[100];
	  VIM_UINT8 *Ptr = Buffer;
	  VIM_SIZE n = 0;
	  vim_mem_clear( Buffer, VIM_SIZEOF(Buffer));
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &tags_list, Buffer, tags_list.current_item.length ));
	  vim_mem_clear( TextData, VIM_SIZEOF( TextData ) );
	  vim_mem_clear( DataBuff, VIM_SIZEOF( DataBuff ) );

	  // Macquarie card is encoded in hex so convert to ascii and then copy over last 13 chars but 1 (trailing F)
	  if( Index == MACQUARIE_CARD )
	  {
		  hex_to_asc( tags_list.current_item.value, DataBuff, tags_list.current_item.length * 2 );
		  vim_sprintf( TextData, "Len: %d, Data: %s", tags_list.current_item.length, DataBuff );
		  pceft_debug( pceftpos_handle.instance, TextData );
		  DBG_MESSAGE( "^^^^^^^^^^ FOUND APPLICATION DATA ^^^^^^^^^^^^^^" );
		  DBG_DATA( Ptr, tags_list.current_item.length );
		  if( transaction->card.type == card_chip )
		  {
			  ////// TEST TEST TEST ////////
			  //vim_strcpy( DataBuff, "9344441479447F" );
			  ////// TEST TEST TEST ////////
			  vim_mem_copy( LoyaltyNumber, DataBuff, MACQUARIE_BARCODE_LEN );
		  }
		  else
		  {
			  VIM_CHAR *tPtr;
			  //vim_strcpy( DataBuff, "06011103A0000008009344441479447F" );
			  tPtr = DataBuff + ( vim_strlen(DataBuff) - ( MACQUARIE_BARCODE_LEN + 1 ));
			  ////// TEST TEST TEST ////////
			  //vim_strcpy( DataBuff, "06011103A0000008009344441479447F" );
			  ////// TEST TEST TEST ////////
			  vim_mem_copy( LoyaltyNumber, tPtr, MACQUARIE_BARCODE_LEN );
		  }

	  }
	  else if( Index == RIMU_CARD )
	  {
		  if( transaction->card.type == card_chip )
		  {
			  vim_mem_copy( LoyaltyNumber, Ptr, tags_list.current_item.length );
		  }
		  else
		  {
			  while(( *Ptr++ != 0x0f ) && ( ++n < tags_list.current_item.length ));
			  vim_mem_copy( LoyaltyNumber, Ptr, tags_list.current_item.length - n );
		  }
	  }
	  else
	  {
#if 0	// RDD 010416 V560 CPAT error needs to result in loyaly error
		  return VIM_ERROR_NOT_FOUND;
#else
		  return ERR_LOYALTY_MISMATCH;
#endif
	  }
	  // Append the dummy track data onto the end to make it look like a legacy card
	  vim_strcat( LoyaltyNumber, LOYALTY_NUMBER_DUMMY_TRACK_DATA );
	  DBG_MESSAGE( LoyaltyNumber );

	  pceft_debug( pceftpos_handle.instance, "Saving Loyalty number:" );
	  pceft_debug( pceftpos_handle.instance, LoyaltyNumber );
	  VIM_ERROR_RETURN_ON_FAIL( SaveLoyaltyNumber( LoyaltyNumber, vim_strlen(LoyaltyNumber), VIM_ICC_SESSION_UNKNOWN ));

	  // RDD 170912 - need to return Error code to kernel so that EMV txn is aborted at this point
	  if( transaction->card.type == card_chip )
		  return ERR_ONE_CARD_SUCESSFUL;
	  else
		  return VIM_ERROR_NONE;
  }
  pceft_debug( pceftpos_handle.instance, "Error: Not found Loyalty Number" );
  return VIM_ERROR_NOT_FOUND;
}
#endif



VIM_ERROR_PTR CheckTagAmt( VIM_TLV_LIST *tags_list, VIM_BOOL fAbort, transaction_t *transaction )
{

  VIM_UINT8 check_amount_bcd[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  VIM_UINT64 check_amount;
  VIM_CHAR Buffer[150];

  vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );

  // RDD 030517 Fast EMV - if LFD returned FastEMV amount then don't do this check!!
  if( terminal.configuration.FastEmvAmount )
  {
	  pceft_debug_test(pceftpos_handle.instance, "WARNING:LFD FastEMV_Amount Overrides DE4-9F02 Check" );
	  return VIM_ERROR_NONE;
  }

  if( vim_tlv_search( tags_list, VIM_TAG_EMV_AMOUNT_AUTHORISED_NUMERIC ) == VIM_ERROR_NONE )
  {
    if( tags_list->current_item.length )
    {
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( tags_list, check_amount_bcd, VIM_SIZEOF(check_amount_bcd) ));

		check_amount = bcd_to_bin( check_amount_bcd, 12 );
		// RDD 100616 SPIRA:8997 Same problem as above

		if(( check_amount != transaction->amount_total ) && ( vim_strncmp( transaction->host_rc_asc, "Z1", 2 )))
		{
			vim_sprintf( Buffer, "DISABLE READER : CTLS AMOUNT MISMATCH Tag %lu Txn %lu", (VIM_SIZE)check_amount, (VIM_SIZE)transaction->amount_total );
			// Disable the reader until the terminal has been power cycled
			// Send out the tags for analysis later

			pceft_debug( pceftpos_handle.instance, Buffer );
			// RDD SPIRA:8484 - all we need to do in these cases is to do a full cancel - killing the reader is a bit OTT
            // RDD 090620 Trello #112 5000 /day of these errors locking up the terminal - GR say goto CTLS instead of fall back.
            vim_picc_emv_cancel_transaction(picc_handle.instance);
            vim_event_sleep(100);
            VIM_ERROR_RETURN(VIM_ERROR_MISMATCH);
		}
    }

  }
  return VIM_ERROR_NONE;
}

/**
 * Retrieve the track 2 data from the kernel.
 *
 * Track 2 will be built from pan and expiry tags (and discretionary data tag) if track 2 doesnt exist.
 * If there is a mismatch between track 2 tag and pan/expiry tags then an error will be reported.
 */


VIM_ERROR_PTR CheckForAACOnPurchase( VIM_TLV_LIST *tags_list, transaction_t *transaction )
{
	VIM_CHAR Buffer[150];
	VIM_UINT8 cid;
	vim_mem_clear( Buffer, VIM_SIZEOF(Buffer) );
	if( vim_tlv_search( tags_list, VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA ) == VIM_ERROR_NONE )
	{
		if( tags_list->current_item.length )
		{
			VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_uint8( tags_list, &cid ));
#if 0
			if(( EMV_CID_AAC(cid) ) && ( transaction->type != tt_refund ))
#else
			// RDD 130315 - SPIRA:8485 This actually causes AMEX XP3 Test 11.7.1 Card 09 $3.00 to Fail because Z1 is expected
			if(( EMV_CID_AAC(cid) ) && ( transaction->type != tt_refund ) && ( IsEmvRID( VIM_EMV_RID_MASTERCARD, transaction )))
#endif
			{
				// We need to fall back in order to pass MAP03-01-01
				return VIM_ERROR_PICC_FALLBACK;
			}
		}
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR picc_get_cvm_results( VIM_UINT8 *CVMResultPtr )
{
	//VIM_SIZE i;
	VIM_TLV_LIST tags_list;
	VIM_UINT8 tlv_buffer[1024];

	VIM_TLV_TAG_PTR const tags[] = { VIM_TAG_EMV_CVM_RESULTS };

	vim_tlv_create( &tags_list, tlv_buffer, sizeof(tlv_buffer) );
	vim_picc_emv_get_tags( picc_handle.instance, tags, VIM_LENGTH_OF(tags), &tags_list );

	/* get CVM_RESULTS */
	if( vim_tlv_search( &tags_list, VIM_TAG_EMV_CVM_RESULTS ) == VIM_ERROR_NONE )
	{
        VIM_DBG_MESSAGE("------------- CVM RESULTS --------------");
        VIM_DBG_DATA(tags_list.current_item.value, tags_list.current_item.length);
		if( tags_list.current_item.length >= 3 )
			vim_mem_copy( CVMResultPtr, tags_list.current_item.value, tags_list.current_item.length );
	}
	return VIM_ERROR_NONE;
}

// RDD 240720 added for Diners odd CVMR tag out of kernel.
VIM_ERROR_PTR picc_update_cvm_results(VIM_UINT8 *CVMResultPtr)
{
    VIM_TLV_LIST tags;

    // RDD 251013 v420 Can cause crash here so need to exit if we can't open the buffer due to no data
    if( vim_tlv_open( &tags, txn.emv_data.buffer, txn.emv_data.data_size, VIM_SIZEOF( txn.emv_data.buffer ), VIM_FALSE ) == VIM_ERROR_NONE )
    {
        VIM_ERROR_WARN_ON_FAIL( vim_tlv_update_bytes(&tags, VIM_TAG_EMV_CVM_RESULTS, CVMResultPtr, 3));
        VIM_DBG_DATA(CVMResultPtr, 3);
    }
    return VIM_ERROR_NONE;
}





VIM_ERROR_PTR picc_get_track_2(transaction_t *transaction)
{
  VIM_SIZE i;
  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[1024];


  VIM_TLV_TAG_PTR const tags[] = {
	VIM_TAG_EMV_AMOUNT_AUTHORISED_NUMERIC,
	VIM_TAG_EMV_CRYPTOGRAM_INFORMATION_DATA,
    VIM_TAG_EMV_TRACK2_EQUIVALENT_DATA,
    VIM_TAG_EMV_APPLICATION_PAN,
    VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE,
    VIM_TAG_EMV_TRACK2_DISCRETIONARY_DATA,
    VIM_TAG_EMV_TRACK2_DATA,
	VIM_TAG_EMV_CVM_RESULTS
  };

  vim_tlv_create( &tags_list, tlv_buffer, sizeof(tlv_buffer) );

#if 1
  picc_tags ();
  vim_picc_emv_get_tags( picc_handle.instance, tags, VIM_LENGTH_OF(tags), &tags_list );
#else
  vim_picc_emv_get_tags( picc_handle.instance, tags, VIM_LENGTH_OF(tags), &tags_list );
#endif

 //VIM_
  //VIM_DBG_TLV(tags_list);
  //vim_sound(VIM_TRUE);
  //SOUND_TONE(VIM_FALSE);

  // RDD 241013 SPIRA:6987 Check that the amount returned by the reader matches the amount sent to it
  // RDD 110821 - non WOW terminals may add surcharge to total so it differs from ARQC amount.
#if 0
	// RDD 260522 JIRA PIRPIN-1633 Surcharge screws this up so remove - even for R10 - hopefully the check is no longer needed
  if( TERM_USE_PCI_TOKENS )
    VIM_ERROR_RETURN_ON_FAIL( CheckTagAmt( &tags_list, VIM_FALSE, transaction ) );
#endif
  // RDD 161214 SPIRA:8273 Paypass3 Scheme Cert Issue MAP03 01 01 - Ensure that we fall back to offer another interface when AAC is returned for a SALE.
  // We have to be very careful here as 0A - 42 (with an AAC) is also returned by ctls refunds and we need to process these and NOT fall back.
  VIM_ERROR_RETURN_ON_FAIL( CheckForAACOnPurchase( &tags_list, transaction ) );

  /* get track2 data, it's contactless mag cards */
  if( vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK2_DATA) == VIM_ERROR_NONE )
  {
    if (tags_list.current_item.length)
    {
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes(&tags_list,transaction->card.data.picc.track2,VIM_SIZEOF(transaction->card.data.picc.track2)));
      transaction->card.data.picc.track2_length = tags_list.current_item.length;
      return VIM_ERROR_NONE;
    }
  }

  /* If track 2 tag is present, use this data and validate against the pan and expiry tags if they are present */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK2_EQUIVALENT_DATA) == VIM_ERROR_NONE)
  {
    if (tags_list.current_item.length)
    {
      /* Convert to ascii */
      bcd_to_asc(tags_list.current_item.value, transaction->card.data.picc.track2,
      tags_list.current_item.length * 2);
      transaction->card.data.picc.track2_length = MIN(tags_list.current_item.length * 2, 37);

      /* Figure out exact length by searching for ? */
      for (i = 0; i < transaction->card.data.picc.track2_length; i++)
      {
        if (transaction->card.data.picc.track2[i] == '?')
          break;
      }
      transaction->card.data.picc.track2_length = i;

      /* Mask out rest of track with nulls */
      if (transaction->card.data.picc.track2_length < sizeof(transaction->card.data.picc.track2))
      {
        vim_mem_set
        (
          &transaction->card.data.picc.track2[transaction->card.data.picc.track2_length],
          0,
          sizeof(transaction->card.data.picc.track2) - transaction->card.data.picc.track2_length
        );
      }

      /* Check the PAN to see if it matches */
      if (vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_PAN) == VIM_ERROR_NONE)
      {
        if (tags_list.current_item.length > 0 && tags_list.current_item.length <= 10)
        {
          char pan_5A[21], pan_track2[21];
          bcd_to_asc(tags_list.current_item.value, pan_5A, tags_list.current_item.length * 2);

          /* Figure out pan length */
          for (i=0; i<tags_list.current_item.length * 2; i++)
          {
            if (pan_5A[i] == '?')

				break;
          }
          pan_5A[i] = 0;
          card_get_pan(&transaction->card, pan_track2);
          /* compare pan, make sure it is the same */
          // RDD 220719 JIRA WWBP-657 Paypass E9 Issues - should approve.
#if 0

          if( vim_strncmp( pan_5A, pan_track2, vim_strlen(pan_5A) ))
          {
              pceft_debug_test( pceftpos_handle.instance, "a.5A-Track2 euqiv mismatch - listing 5A then T2");
              pceft_debug_test( pceftpos_handle.instance, pan_5A );
              pceft_debug_test( pceftpos_handle.instance, pan_track2 );
              VIM_ERROR_RETURN(VIM_ERROR_EMV_TRACK2_MISMATCH);
          }
#endif
		  // RDD 240712 SPIRA:5492 Luhn check for contactless cards
		  // RDD 110912 Both Rimu test cards have bad luhns !
		  // RDD 300616 SPIRA:???? There are bad luhns for ANZ cards out there that need to pass through this...
#ifndef _DEBUG
		  // RDD 210916 9009 ANZ eftpos BIN 560254 - remove compulsory luhn check to let cpat decide
		  //if ( !check_luhn( pan_track2, vim_strlen(pan_track2)) )
          //  VIM_ERROR_RETURN(ERR_LUHN_CHECK_FAILED);
#endif
		}
      }

      /* Check expiry to see if it matches */
      if (vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE) == VIM_ERROR_NONE)
      {
        if (tags_list.current_item.length == 3)
        {
          char track_2_mm_yy[5], yy_mm_5F24[5];
          if (check_emv_expiry_date(tags_list.current_item.value))
          {
			  pceft_debug( pceftpos_handle.instance, "Warning: Expired Application");
			  //VIM_DBG_ERROR(ERR_CARD_EXPIRED);
            //VIM_ERROR_RETURN(ERR_CARD_EXPIRED);
          }
          card_get_expiry(&transaction->card, track_2_mm_yy);
          bcd_to_asc(&tags_list.current_item.value[0], &yy_mm_5F24[0], 2); /* yy */
          bcd_to_asc(&tags_list.current_item.value[1], &yy_mm_5F24[2], 2); /* mm */
          yy_mm_5F24[4] = 0;
          /* compare expiry, make sure it is the same */
		  // RDD 120115 - this causes CTAV test Card 08 to fail as Mag expiry has earlier expiry than Emv Tag - will this be practice in prod cards ? Better not risk it
          if( vim_mem_compare( &track_2_mm_yy[0], &yy_mm_5F24[2], 2) != VIM_ERROR_NONE ||
              vim_mem_compare( &track_2_mm_yy[2], &yy_mm_5F24[0], 2) != VIM_ERROR_NONE )
		  {
			  if( is_EPAL_cards( transaction ))
			  {
				  // RDD 120115 - We need EPAL to pass cert ....
				  pceft_debug( pceftpos_handle.instance, "Warning: EPAL Waiver: T2 Expiry does not match Tag 5F24");
			  }
			  else
			  {
                  pceft_debug_test(pceftpos_handle.instance, "Expiry mismatch - 5F24 and Mag Exp");
                  // RDD 220719 JIRA WWBP-657 - remove this as may be required to cert Paypass
				  //VIM_ERROR_RETURN(VIM_ERROR_EMV_TRACK2_MISMATCH);
			  }
		  }
        }
      }
      else
      {
        if (card_check_expired(&transaction->card))
		{

			// RDD 230516 v560 SPIRA:8946 No Expiry Date checking required on contactless cards
			pceft_debug( pceftpos_handle.instance, "Warning: Expired Card (no 5F24 tag)" );
            //VIM_ERROR_RETURN(ERR_CARD_EXPIRED);
		}
      }
      /* Got a track 2 and matched with expiry/pan tags. */
      return VIM_ERROR_NONE;
    }
  }

  /* If track 2 is not present construct it from the pan, expiry and optionally discretionary data tag */
  /* Find PAN tag */
  if ((vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_PAN) == VIM_ERROR_NONE) && (tags_list.current_item.length > 0) && (tags_list.current_item.length <= 10))
  {
    bcd_to_asc(tags_list.current_item.value, transaction->card.data.picc.track2, tags_list.current_item.length * 2);

    /* add pan */
    for (i=0; i < (tags_list.current_item.length * 2); i++)
    {
      if (transaction->card.data.picc.track2[i] == '?')
        break;
    }
    transaction->card.data.picc.track2[i] = 0;

  }
  else
  {
	  //picc_send_tags();
      VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_ERROR);
  }

  /*strncat(transaction->card.data.picc.track2, "=", sizeof(transaction->card.data.picc.track2));*/
  vim_strncat(transaction->card.data.picc.track2, "=", sizeof(transaction->card.data.picc.track2));

  /* Find expiry tag */
  if ((vim_tlv_search(&tags_list, VIM_TAG_EMV_APPLICATION_EXPIRATION_DATE) == VIM_ERROR_NONE)  && (tags_list.current_item.length == 3))
  {
    char expiry_yymm[5];
    if (check_emv_expiry_date(tags_list.current_item.value))
    {
      VIM_ERROR_RETURN(ERR_CARD_EXPIRED);
    }
    bcd_to_asc(tags_list.current_item.value, expiry_yymm, 4);
    expiry_yymm[4] = 0;
    vim_strncat(transaction->card.data.picc.track2, expiry_yymm, sizeof(transaction->card.data.picc.track2));
  }
  else
    VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_ERROR);

  /* Add discretionary data tag if it is present... not compulsory though */
  if (vim_tlv_search(&tags_list, VIM_TAG_EMV_TRACK2_DISCRETIONARY_DATA) == VIM_ERROR_NONE)
  {
    char discretionary_data[30];

    bcd_to_asc(tags_list.current_item.value, discretionary_data, MIN(tags_list.current_item.length*2, sizeof(discretionary_data)));
    for (i=0; i<MIN(tags_list.current_item.length*2, sizeof(discretionary_data)-1); i++)
    {
      if (discretionary_data[i] == '?')
        break;
    }
    discretionary_data[i] = 0;
    vim_strncat(transaction->card.data.picc.track2, discretionary_data, sizeof(transaction->card.data.picc.track2));
  }

  transaction->card.data.picc.track2_length = vim_strlen(transaction->card.data.picc.track2);
  //VIM_DBG_STRING(transaction->card.data.picc.track2);
 //VIM_DBG_VAR(transaction->card.data.picc.track2_length);

  if (card_check_expired(&transaction->card))
    VIM_ERROR_RETURN(ERR_CARD_EXPIRED);

  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR picc_tags(void)
{
  VIM_SIZE picc_tags_count;
  VIM_TLV_TAG_PTR *picc_tags;
  VIM_TLV_LIST tag_list;

  if (txn.emv_data.data_size)
  {
      DBG_MESSAGE("---------- Already read PICC tags !! ----------");
      return VIM_ERROR_NONE;
  }

  if( txn.card.type != card_picc )
	  return VIM_ERROR_NONE;

  /* get tags that should be sent to the host */
  picc_tags = get_picc_tags(&picc_tags_count);
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tag_list, txn.emv_data.buffer, sizeof(txn.emv_data.buffer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_picc_emv_get_tags(picc_handle.instance, picc_tags, picc_tags_count, &tag_list));
 // VIM_DBG_TLV( tag_list );
  txn.emv_data.data_size = tag_list.total_length;
  return VIM_ERROR_NONE;
}


// RDD 280616 Update AID search to include variable tag search
VIM_ERROR_PTR SetTxnAIDIndex( VIM_TLV_LIST *tags_list, const void *TagName, VIM_BOOL Picc )
{
	VIM_SIZE i;

	VIM_DBG_TLV( *tags_list );

	VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( tags_list, TagName ));

	for ( i = 0; i < MAX_EMV_APPLICATION_ID_SIZE; i++ )
	{
        // RDD 230819 This is being used for contaCT AND CONTACTLESS !!!
        if ((!terminal.emv_applicationIDs[i].contactless) && (Picc))
            continue;
        else if ((terminal.emv_applicationIDs[i].contactless) && (!Picc))
            continue;

        if( VIM_ERROR_NONE == vim_mem_compare(	tags_list->current_item.value,
												terminal.emv_applicationIDs[i].emv_aid,
												terminal.emv_applicationIDs[i].aid_length))
        {
			txn.aid_index = i;
            VIM_DBG_NUMBER(txn.aid_index);
            VIM_DBG_DATA( terminal.emv_applicationIDs[i].emv_aid, terminal.emv_applicationIDs[i].aid_length );
            VIM_DBG_NUMBER(terminal.emv_applicationIDs[txn.aid_index].cvm_limit);
			return VIM_ERROR_NONE;
        }
	}
	VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
}


// RDD 280616 re-written for UPI which does not return adequete info. in the AID so need to use dedicated filename
VIM_ERROR_PTR picc_find_application(void)
{
  VIM_TLV_LIST tags_list;
  VIM_UINT8 tlv_buffer[256];
  const VIM_TAG_ID tags[] = { VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_TAG_EMV_AID_TERMINAL };

  vim_tlv_create( &tags_list, tlv_buffer, sizeof( tlv_buffer ));
  vim_picc_emv_get_tags( picc_handle.instance, ( VIM_TLV_TAG_PTR *)tags, sizeof( tags )/sizeof( tags[0] ), &tags_list );

  // RDD if UPI then use the Dedicated Filename as AID because the AID doesn't have the last byte which tells us debit, credit or quasi credit !
  if (SetTxnAIDIndex(&tags_list, VIM_TAG_EMV_DEDICATED_FILE_NAME, VIM_TRUE ) != VIM_ERROR_NONE)
  {
      if (SetTxnAIDIndex(&tags_list, VIM_TAG_EMV_AID_TERMINAL, VIM_TRUE ) != VIM_ERROR_NONE)
      {
          pceft_debug_error(pceftpos_handle.instance, "SYSTEM ERROR: (PICC AID NOT FOUND) !!!!", VIM_ERROR_SYSTEM);
          VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
      }
  }
  return VIM_ERROR_NONE;
}


VIM_ERROR_PTR CheckCTLSCashEnabled(transaction_t *transaction)
{
	VIM_UINT8 AddCap[5];
	vim_mem_copy(AddCap, terminal.emv_applicationIDs[transaction->aid_index].add_term_cap, VIM_SIZEOF(AddCap));
	if( AddCap[0] & 0x10 )
	{
		return VIM_ERROR_NONE;
	}
	pceft_debug_error(pceftpos_handle.instance, "Cashout on CTLS Not enabled for this RID", VIM_ERROR_PICC_FALLBACK);
	return VIM_ERROR_PICC_FALLBACK;
}


// RDD 280312 : ReWritten for V23
VIM_ERROR_PTR txn_picc(void)
{
  VIM_ERROR_PTR error = VIM_ERROR_NONE;

  /* for VX820, sleep 150 ms to wait for the callback messages */
  //vim_event_sleep(150);
  vim_event_sleep(0);
 // DisplayProcessing( DISP_PROCESSING_WAIT );

  //pceft_debug( pceftpos_handle.instance, "In txn_picc" );

  // RDD 251012 SPIRA:6219 - Don't want Key Pin Or Enter for PICC over CVM limit
  txn.card.data.picc.msd = VIM_FALSE;

  // RDD 070217 added for JIRA WWBP-130 CTLS dies after return to present card (no cancel)
  txn.read_icc_count++;


  // RDD 181012 SPIRA:6137 If activation timeout then need to restart transaction.
  if(( error = vim_picc_emv_process_transaction(picc_handle.instance)) == VIM_ERROR_PICC_ACTIVATION_TIMEOUT )
  {
	  VIM_ERROR_RETURN(error);
  }


  // RDD 200716 UPI CTLS Card6 is supposed to fallback even if currency matched but didn't - check CTQ
  else if( error == VIM_ERROR_PICC_FALLBACK )
  {
	  VIM_ERROR_RETURN(error);
  }
#if 0	// RDD V560 Close the reader thread in any case
		DBG_POINT;
		DBG_POINT;
		picc_close(VIM_TRUE);
		DBG_POINT;
#endif

  // RDD 310719
  else if (error == VIM_ERROR_PICC_RETRY)
  {
      VIM_ERROR_RETURN(error);
  }

#if 1 // RDD 120115 - need the AID to bypass tag vs track2 exp date checking to get round the crappy EPAL test card which has a mismatch but expect it to be ignored
  picc_find_application();
#endif

  // RDD 300118 JIRA:WWBP-132 Additional Capabilities doesn't affect VISA Cash out on CTLS
#ifndef _DEBUG
  if (txn.amount_cash)
  {
	  VIM_ERROR_RETURN_ON_FAIL( CheckCTLSCashEnabled( &txn ) );
  }
#endif

  // RDD 120912 Always try to get track2 - we only need this for MSD or Refund or Pre-Tap Loyalty
  if(( error = picc_get_track_2(&txn)) == VIM_ERROR_NONE )
  {
	  // RDD 010714 SPIRA:7909 Problems caused by failed cancel cmd
	  terminal_set_status( ss_picc_response_received );

	  // special processing on REFUND : Basically if we can get track data then we can approve the txn...
	  if( txn.type == tt_refund && error != VIM_ERROR_NONE )
	  {
		if( picc_get_track_2(&txn) == VIM_ERROR_NONE )
			error = VIM_ERROR_NONE;
	  }
	  // RDD 100912 added for P3 RIMU contactless Loyalty
#if 1
	  else if( txn.type == tt_query_card )
	  {
		  VIM_PICC_EMV_STATUS status;
		  vim_picc_emv_get_transaction_status(picc_handle.instance, &status);

		  // RDD 140814 added
		  {
			VIM_UINT8 pan[50];
			VIM_SIZE pan_len;
			vim_mem_clear( pan, VIM_SIZEOF(pan) );
			VIM_ERROR_RETURN_ON_FAIL(card_get_pan(&txn.card, (char *)pan));
			pan_len = MIN(vim_strlen((char *)pan), 19);
			txn.card_name_index = find_card_range(&txn.card, pan, pan_len);
		  }
		  // RDD 140814 added

		  if( status == VIM_PICC_EMV_STATUS_AUTHORIZATION_REQUEST )
		  {
			  txn_set_picc_online_result(VIM_TRUE,"00");
		  }
		  return( get_loyalty_iad( &txn ));
	  }
#endif
  }

  if(( error == VIM_ERROR_NONE )||( error == VIM_ERROR_PICC_CARD_DECLINED ))
  {

    // RDD 140312 V18 SPIRA 5102 - need Track2 Equivalent to send up with Z1 advice
	// RDD 120115 V539-3 This is read about 20 lines above !!!
#if 0
    VIM_ERROR_IGNORE(picc_get_track_2(&txn));

	// RDD 280312 Also need AID for declined cards to go on the receipt
    VIM_ERROR_IGNORE( picc_find_application());
#endif

	DBG_POINT;

    VIM_ERROR_RETURN_ON_FAIL(vim_picc_emv_is_mag_card(picc_handle.instance, &txn.card.data.picc.msd));

    if(( txn.card.data.picc.msd )||( tt_refund == txn.type ))
	{
        DBG_MESSAGE( "~~~~~~~~~~~ CARD IS MSD OR TXN IS REFUND SO SET PARTIAL EMV ~~~~~~~~~~" );
		transaction_set_status( &txn, st_partial_emv );
	}
	else
	{
        DBG_MESSAGE( "~~~~~~~~~~~ PICC TXN IS FULL EMV ~~~~~~~~~~" );
		transaction_clear_status( &txn, st_partial_emv );
	}
	DBG_VAR(txn.card.data.picc.msd);
  }
  return error;
}


// RDD 080513 - New Version to hopefully fix issue where cancel failed causing next open to kill reader.
VIM_ERROR_PTR picc_close( VIM_BOOL DisableThread )
{
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;
//	VIM_UINT8 retry = 3;
	// RDD 010714 - reduce to zero retires as cancel doesn't work any more
	//VIM_UINT8 retry = 0;


	if( DisableThread )
	{
		DBG_MESSAGE(" ======= CLOSE PICC READER (FULL CLOSE) ======= " );
	}
	else
	{
		DBG_MESSAGE(" ======= CLOSE PICC READER (CANCEL ONLY) ======= " );
	}

	// RDD 260315 - read and report any CTLS misreads for this transaction before we exit.
	//picc_debug_errors( );

	if ((picc_handle.instance) == VIM_NULL)
	{
		DBG_MESSAGE("Already Closed");
		return VIM_ERROR_NONE;
	}

	pceft_debug_test( pceftpos_handle.instance, "PICC Cancel Txn" );

	// RDD 200213 - if there is no instance for the reader (ie it didn't open sucessully) then calling this can cause a crash
	if (terminal_get_status(ss_ctls_reader_open))
	{
		// RDD 220517 SPIRA:9231 Always ensure that APDU Mode is closed
		if( txn.card_state == card_capture_picc_apdu )
		{
			vim_picc_emv_close_apdu_interface_mode(picc_handle.instance);
		}
		else
		{
			vim_picc_emv_cancel_transaction( picc_handle.instance );
		}

		if( DisableThread )
		{
			//vim_event_sleep( 50 );
			// RDD 210716 v560-4 don't mark the reader as closed if the close failed
			//VIM_ERROR_IGNORE(vim_picc_emv_close(picc_handle.instance));
			VIM_ERROR_RETURN_ON_FAIL(vim_picc_emv_close(picc_handle.instance));
			picc_handle.instance = VIM_NULL;
			terminal_clear_status( ss_ctls_reader_open );

			VIM_DBG_WARNING( "READER NOW CLOSED - FLAG CLEARED" );

			pceft_debug_test( pceftpos_handle.instance, "PICC Reader Closed" );
		}
	}
	else
	{
		VIM_DBG_WARNING( "READER NOT OPEN SO CAN'T CLOSE" );
	}
	// RDD 070217 added for JIRA WWBP-130 CTLS dies after return to present card (no cancel)
	terminal_clear_status(ss_ctls_reader_open);
	return VIM_ERROR_NONE;
}


/***************************************************************************/
VIM_ERROR_PTR CtlsReaderEnable( VIM_UINT8 Level )
{
  VIM_ERROR_PTR res;
  DBG_MESSAGE( "======== CTLS READER ENABLE ========");
  picc_close(VIM_TRUE);
  vim_mem_clear( &picc_handle, VIM_SIZEOF(picc_handle));

  // RDD 191112 - Mod to always display contactless version on Power-up
  if(( res = picc_open( Level, VIM_TRUE )) != VIM_ERROR_NONE )
  {
	  // RDD 141112 added - sometimes the CTLS enabled flags get turned off in the EPAT so warn with new msg
	  if( res == VIM_ERROR_NOT_FOUND )
	  {
		  display_screen( DISP_CONTACTLESS_DISABLED, VIM_PCEFTPOS_KEY_NONE );
	  }
	  else
	  {
		  // Failed to initialise the reader
		  display_screen( DISP_CONTACTLESS_NOT_READY, VIM_PCEFTPOS_KEY_NONE );
	  }
	  vim_event_sleep( 2000 );
      //picc_close();
      return res;
  }
  //picc_close(VIM_TRUE);
  return VIM_ERROR_NONE;
}

#if 1 // v425 ver

VIM_BOOL VisaTestCase40( void )
{
	VIM_BOOL ctq_valid = VIM_TRUE;

	// RDD 180412 Visa test case 40 requires no CVM if under limit and CTQ missing
	if( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit )
    {
		vim_picc_emv_is_ctq_valid(picc_handle.instance, &ctq_valid);
	}
	DBG_VAR( ctq_valid );
	return ctq_valid ? VIM_FALSE : VIM_TRUE;
}


VIM_BOOL MSDUnderlimit( void )
{
	VIM_BOOL MsdUnder = VIM_FALSE;

	if(( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit ) && ( txn.card.data.picc.msd ))
    {
	  if( picc_find_application() == VIM_ERROR_NONE )
			MsdUnder = VIM_TRUE;
	}
	DBG_VAR( MsdUnder );
	return MsdUnder;
}


VIM_BOOL UPIOverrideCVM( void )
{
	VIM_BOOL picc_override = VIM_FALSE;

	vim_picc_emv_is_upi_cvm_override( picc_handle.instance, &picc_override );

	 if(( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit ) && ( picc_override ))
		 return VIM_TRUE;
	 else
		 return VIM_FALSE;
}


// RDD 020420 v582-7 added EPAT Override ( temporary increase in CTLS CVM limit for some issuers due to COVID19 )
static VIM_BOOL Covid19_Override_CVM( transaction_t *transaction, VIM_BOOL *PinRequired, VIM_BOOL *SigRequired )
{
    // Check to see if the reg item actually exists and is set to a finite value
    if (!terminal.configuration.term_override_cvm_value)
        return VIM_FALSE;

    // LFD CVM limit is in $ amount is in cents - if over both limits
    if ( transaction->amount_total > terminal.configuration.term_override_cvm_value )
    {          
        pceft_debug_test(pceftpos_handle.instance, "COVID19 : Amount Exceeds higher limit");
        return VIM_FALSE;
    }

    // Under the limit so check the Primary flag - the CVM limit will not be changed
    if (CARD_NAME_OVERRIDE_MERCHANT_CVM_LIMIT) 
    {
        if ( txn.amount_total < terminal.configuration.term_override_cvm_value )
        {
	    DBG_MESSAGE("COVID COVID COVID Override Merchant CVM -----------------");
            if (*PinRequired || *SigRequired)
            {
		DBG_MESSAGE("COVID COVID COVID CVM Required -----------------");
                if (CARD_NAME_OVERRIDE_ISSUER_CVM_LIMIT)
                {
		    DBG_MESSAGE("COVID COVID COVID Override Issuer CVM -----------------");
                    pceft_debug_test( pceftpos_handle.instance, "COVID19 : !!! CPAT says Ignore Issuer CVM Limit !!!" );
                    *PinRequired = VIM_FALSE;
                    *SigRequired = VIM_FALSE;
                    return VIM_TRUE;
                }
                else
                {
                    pceft_debug_test( pceftpos_handle.instance, "COVID19 : CPAT says Respect Issuer CVM Limit" );
                    return VIM_FALSE;
                }
            }
            else if( transaction->amount_total >= terminal.emv_applicationIDs[transaction->aid_index].cvm_limit )
            {
                VIM_DBG_MESSAGE("COVID19 : CPAT Says use LFD CVM limit");
            }
            return VIM_TRUE;
        }
    }
    else if (transaction->amount_total >= terminal.emv_applicationIDs[transaction->aid_index].cvm_limit)
    {
        pceft_debug_test( pceftpos_handle.instance, "COVID19 : Normal EPAT limit applies" );
        if( !*PinRequired && !*SigRequired )
            *PinRequired = VIM_TRUE;
    }
    return VIM_FALSE;
}


void picc_set_pin_entry_type( txn_pin_entry_type *pin_entry_state )
{
	VIM_BOOL picc_pin_required = VIM_TRUE;
	VIM_BOOL picc_sig_required = VIM_TRUE;

	DBG_MESSAGE( "PICC CVM FLAG SETTING" );
	DBG_VAR( txn.card.data.picc.msd );

	// RDD 111212 - make the default state optional so if something really stuffs up then the host can decide
	*pin_entry_state = txn_pin_entry_optional;

	// Shouldn't ever have to do this
    if( VIM_NULL == picc_handle.instance )
	{
		return;
	}

	// Set no pin entry by default for PICC
	*pin_entry_state = txn_pin_entry_skip_state;

	// RDD 220616 added UPI CVM Override - PICC reader reports PIN required or SIG required but this is overridden if tag 9f68 present (basically)
	// RDD 090318 Westpac reported issue with some CTLS refunds so ensure IsUPI to confirm this behavior
	if( UPIOverrideCVM( ) && IsUPI(&txn))
		return;

	vim_picc_emv_is_PIN_required( picc_handle.instance, &picc_pin_required );
	DBG_VAR( picc_pin_required );

	//JQ check signature first
    vim_picc_emv_is_signature_required( picc_handle.instance, &picc_sig_required );
	DBG_VAR( picc_sig_required );

	// RDD 111212 - check QVSDC test Case 40

	if( VisaTestCase40( ) ) return; 	


    // RDD 020420 v582-7 Spoke to Donna and she decided that the new PICC CVM rule would go above MSD, so MSD will follow the rule
    if( Covid19_Override_CVM( &txn, &picc_pin_required, &picc_sig_required  ))
    {
        pceft_debug(pceftpos_handle.instance, "COVID19 EPAT CVM Override");
        return;
    }

    // RDD 240912 SPIRA:6084 WOW now want all MSD transaction not to not prompt if under CVM limit
	if( MSDUnderlimit( ) ) 
        return;

	// RDD 111212 Check for Sig first
	if( ( !picc_pin_required ) && ( picc_sig_required ) && ( txn.type != tt_refund ) && ( !txn.card.data.picc.msd ))
	{
		DBG_MESSAGE( "!!!!!!!!!!!!!! SIGNATURE REQUIRED FOR PICC TXN !!!!!!!!!!!!!!!!!!!" );
		transaction_set_status( &txn, st_signature_required );
			return;
    }

	//////// TEST TEST TEST TEST /////////
	//picc_pin_required  = VIM_FALSE;
	//////// TEST TEST TEST TEST ////////

    // RDD 240720 - can't get expected CVMR out of Verifone kernel so just set what is expected
    if (IS_DINERS(txn.aid_index))
    {
        VIM_UINT8 CVMResults[3];
        const VIM_UINT8 DPAS_PIN[3] = { 0x42,0x03,0x00 };
        const VIM_UINT8 DPAS_NOCVM[3] = { 0x1F,0x03,0x02 };
        VIM_DBG_MESSAGE("------------ set Diners CVMR ------------");
        vim_mem_clear(CVMResults, VIM_SIZEOF(CVMResults));
        if (picc_pin_required)
        {
            vim_mem_copy(CVMResults, DPAS_PIN, 3);
        }
        else
        {
            vim_mem_copy(CVMResults, DPAS_NOCVM, 3);
        }
        picc_update_cvm_results(CVMResults);
    }



	// RDD 111212 need all this due to scheme test issues QVSDC 24,25,26,27,35  - CTQ/Pin reqd under CVM added additional qualifiers for WOW change req wrt refund and msd
	if(( picc_pin_required ) && ( txn.type == tt_sale ) && ( txn.card.data.picc.msd == VIM_FALSE ))
	{

		*pin_entry_state = txn_pin_entry_mandatory;
		// RDD 260312 VISA Scheme Tests. CTQ is ignored if Online PIN entry is mandatory as no dual CVM for PICC cards MUST NOT PRINT SIG LINE
		transaction_clear_status( &txn, st_signature_required );
		return;
	}

	// RDD 161214 SPIRA:8273 If Debug or XXX mode Release then no not override the reader decision as this causes some scheme tests to fail
	// eg. MAP040601
	if ( /*!terminal_get_status(ss_wow_basic_test_mode)*/1)
	{
        VIM_DBG_NUMBER( txn.aid_index );
		VIM_DBG_NUMBER(terminal.emv_applicationIDs[txn.aid_index].cvm_limit);
		// RDD 251012 SPIRA:6219 Reader seems to decide NO Pin reqd for Contactless Refunds - OVERRIDE reader decision ....
		// RDD 221112 v305 After Scheme testing issues with the above approach WOW decided that MSD card and refunds over CVM limit will follow CPAT
		// RDD TODO - fix this so it uses real CVM limit and identify why limit is zero at this point

        if( txn.amount_total >= terminal.emv_applicationIDs[txn.aid_index].cvm_limit )
		{
			// RDD 120115 SPIRA:8331 No signature for On-device-CVM for Mastercard in cases over CVM limit where PIN was entered on phone etc.
			// See page 427 of Paypass Spec v3.0.1 - Condition CVM.E4 CVM Results = 0x01,0x00,0x02
			VIM_UINT8 CVMResults[10];
			const VIM_UINT8 Paypass301_CVM_E4[3] = { 0x01,0x00,0x02 };
			const VIM_UINT8 XP3_PreKeyed_PIN[3] = { 0x41,0x03,0x02 };

			vim_mem_clear( CVMResults, VIM_SIZEOF(CVMResults) );
			picc_get_cvm_results( CVMResults );
			if(( IS_AMEX( txn.aid_index )) && ( vim_mem_compare( CVMResults, XP3_PreKeyed_PIN, VIM_SIZEOF(XP3_PreKeyed_PIN) ) == VIM_ERROR_NONE ))
			{
				pceft_debug(pceftpos_handle.instance, "SPIRA:8331 Amex XP3 XPM01 OD CVM OK" );
				return;
			}
 
			if( IS_MASTERCARD( txn.aid_index ) && ( vim_mem_compare( CVMResults, Paypass301_CVM_E4, VIM_SIZEOF(Paypass301_CVM_E4) ) == VIM_ERROR_NONE ))
			{
				pceft_debug(pceftpos_handle.instance, "SPIRA:8331 Paypass301 OD CVM OK" );
				return;
			}
			// RDD 020115 SPIRA:8284 - For mastercard ensure that this case doesn't apply if amt = CVM limit as needs to be greater for CVM
			if( IS_MASTERCARD( txn.aid_index  ) && ( txn.amount_total == terminal.emv_applicationIDs[txn.aid_index].cvm_limit ))
				return;
			// RDD 060412 SPIRA 5254: Set PinEntry VIA CPAT for Contactless Refunds

			if(( txn.card.data.picc.msd ) || ( txn.type == tt_refund ))
			{

                // RDD 190820 Trello #130 remove this nonsense from Vysh
#if 0
				if (IS_AMEX( txn.aid_index ) && (IsApplePay() || IsAndroidPay()))	{
					// VN JIRA WWBP-764 010820 AMEX cert test EP020
					// Mobile test cards
					pceft_debug(pceftpos_handle.instance, "JIRA WWBP-764");
					return;
				}
#endif
                SetPinEntryViaCpat( pin_entry_state );
			}
			else if( picc_pin_required )
			{
				*pin_entry_state = txn_pin_entry_mandatory;
			}
			// RDD SPIRA:7563 Fix for CTLS cards with no CVM list - just default to sig if over the CVM limit
			else
			{
				// RDD 191115 TEST TEST TEST - remove so no Sig required for AMEX ApplePay over CVM
#if 1
				if( IsApplePay() == VIM_FALSE )
				{
#if 0               // RDD 190719 JIRA WWBP-643 UPI CDCVM - Get rid of this as no longer valid and causes issues with scheme cert
					DBG_MESSAGE( "SPIRA:7563 NO CVM reqd over limit so Force Sig CVM For v425" );
					pceft_debug(pceftpos_handle.instance, "SPIRA:7563 Force Sig" );
                    // RDD 140519 Bypass this for now as CVM Limit seems to be zero at this point always
					transaction_set_status( &txn, st_signature_required );
#endif
				}
				else
				{
					//pceft_debug(pceftpos_handle.instance, "SPIRA:7563 Force Sig Applepay Bypass" );
				}
#endif
				// RDD 191115 TEST TEST TEST
			}
		}
	}
	DBG_VAR( pin_entry_state );
	return;
}

#else


VIM_BOOL VisaTestCase40( void )
{
	VIM_BOOL ctq_valid = VIM_TRUE;

	// RDD 180412 Visa test case 40 requires no CVM if under limit and CTQ missing
	if( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit )
    {
		vim_picc_emv_is_ctq_valid(picc_handle.instance, &ctq_valid);
	}
	DBG_VAR( ctq_valid );
	return ctq_valid ? VIM_FALSE : VIM_TRUE;
}


VIM_BOOL MSDUnderlimit( void )
{
	VIM_BOOL MsdUnder = VIM_FALSE;

	if(( txn.amount_total < terminal.emv_applicationIDs[txn.aid_index].cvm_limit ) && ( txn.card.data.picc.msd ))
    {
	  DBG_POINT;
	  if( picc_find_application() == VIM_ERROR_NONE )
			MsdUnder = VIM_TRUE;
	}
	DBG_VAR( MsdUnder );
	return MsdUnder;
}


void picc_set_pin_entry_type( txn_pin_entry_type *pin_entry_state )
{
	VIM_BOOL picc_pin_required = VIM_TRUE;
	VIM_BOOL picc_sig_required = VIM_TRUE;

	DBG_MESSAGE( "PICC CVM FLAG SETTING" );
	DBG_VAR( txn.card.data.picc.msd );

	// RDD 111212 - make the default state optional so if something really stuffs up then the host can decide
	*pin_entry_state = txn_pin_entry_optional;

	// Shouldn't ever have to do this
    if( VIM_NULL == picc_handle.instance )
	{
		DBG_POINT;
		return;
	}

	// Set no pin entry by default for PICC
	*pin_entry_state = txn_pin_entry_skip_state;

	vim_picc_emv_is_PIN_required( picc_handle.instance, &picc_pin_required );
	DBG_VAR( picc_pin_required );

	//JQ check signature first
    vim_picc_emv_is_signature_required( picc_handle.instance, &picc_sig_required );
	DBG_VAR( picc_sig_required );

	// RDD 111212 - check QVSDC test Case 40
	if( VisaTestCase40( ) ) return;

	// RDD 240912 SPIRA:6084 WOW now want all MSD transaction not to not prompt if under CVM limit
	if( MSDUnderlimit( ) ) return;

	// RDD 111212 Check for Sig first
	if( ( !picc_pin_required ) && ( picc_sig_required ) && ( txn.type != tt_refund ) && ( !txn.card.data.picc.msd ))
	{
		DBG_MESSAGE( "!!!!!!!!!!!!!! SIGNATURE REQUIRED FOR PICC TXN !!!!!!!!!!!!!!!!!!!" );
		transaction_set_status( &txn, st_signature_required );

		// RDD 221112 - need to exit here or may end up with Dual CVM ...
		return;
    }

	// RDD 111212 need all this due to scheme test issues QVSDC 24,25,26,27,35  - CTQ/Pin reqd under CVM added additional qualifiers for WOW change req wrt refund and msd
	if(( picc_pin_required ) && ( txn.type == tt_sale ) && ( txn.card.data.picc.msd == VIM_FALSE ))
	{
		DBG_POINT;
		*pin_entry_state = txn_pin_entry_mandatory;
		// RDD 260312 VISA Scheme Tests. CTQ is ignored if Online PIN entry is mandatory as no dual CVM for PICC cards MUST NOT PRINT SIG LINE
		transaction_clear_status( &txn, st_signature_required );
		return;
	}

	VIM_DBG_NUMBER( terminal.emv_applicationIDs[txn.aid_index].cvm_limit );
	// RDD 251012 SPIRA:6219 Reader seems to decide NO Pin reqd for Contactless Refunds - OVERRIDE reader decision ....
	// RDD 221112 v305 After Scheme testing issues with the above approach WOW decided that MSD card and refunds over CVM limit will follow CPAT
	if(  txn.amount_total >= terminal.emv_applicationIDs[txn.aid_index].cvm_limit )
	{
			// RDD 120115 SPIRA:8331 No signature for On-device-CVM for Mastercard in cases over CVM limit where PIN was entered on phone etc.
			// See page 427 of Paypass Spec v3.0.1 - Condition CVM.E4 CVM Results = 0x01,0x00,0x02
			VIM_UINT8 CVMResults[10];
			const VIM_UINT8 Paypass301_CVM_E4[3] = { 0x01,0x00,0x02 };
			const VIM_UINT8 XP3_PreKeyed_PIN[3] = { 0x41,0x03,0x02 };

			vim_mem_clear( CVMResults, VIM_SIZEOF(CVMResults) );
			picc_get_cvm_results( CVMResults );
			if(( IS_AMEX( txn.aid_index )) && ( vim_mem_compare( CVMResults, XP3_PreKeyed_PIN, VIM_SIZEOF(XP3_PreKeyed_PIN) ) == VIM_ERROR_NONE ))
			{
				pceft_debug(pceftpos_handle.instance, "SPIRA:8331 Amex XP3 XPM01 OD CVM OK" );
				return;
			}
			if(( IS_MASTERCARD( txn.aid_index )) && ( vim_mem_compare( CVMResults, Paypass301_CVM_E4, VIM_SIZEOF(Paypass301_CVM_E4) ) == VIM_ERROR_NONE ))
			{
				pceft_debug(pceftpos_handle.instance, "SPIRA:8331 Paypass301 OD CVM OK" );
				return;
			}
			// RDD 020115 SPIRA:8284 - For mastercard ensure that this case doesn't apply if amt = CVM limit as needs to be greater for CVM
			if(( IS_MASTERCARD( txn.aid_index )) && ( txn.amount_total == terminal.emv_applicationIDs[txn.aid_index].cvm_limit ))
				return;
			// RDD 060412 SPIRA 5254: Set PinEntry VIA CPAT for Contactless Refunds

			if(( txn.card.data.picc.msd ) || ( txn.type == tt_refund ))
			{
				SetPinEntryViaCpat( pin_entry_state );
			}
			else if( picc_pin_required )
			{
				*pin_entry_state = txn_pin_entry_mandatory;
			}
			// RDD SPIRA:7563 Fix for CTLS cards with no CVM list - just default to sig if over the CVM limit
#if 0		// RDD v550-0 SPIRA:8906 Remove this so that Applepay etc. don't need Sig over CVM
			else
			{
				DBG_MESSAGE( "SPIRA:7563 NO CVM reqd over limit so Force Sig CVM For v425" );
				pceft_debug(pceftpos_handle.instance, "SPIRA:7563 Force Sig" );
				transaction_set_status( &txn, st_signature_required );
			}
#endif
	}
	DBG_VAR( pin_entry_state );
	return;
}

#endif



VIM_ERROR_PTR picc_debug_errors( void )
{
	uPiccDebugData PiccDebugDataUnion;
	uPiccDebugData *PiccDebugDataUnionPtr = &PiccDebugDataUnion;

	if( vim_picc_emv_debug_vivotech_transaction_data ( PiccDebugDataUnionPtr ) == VIM_ERROR_NONE )
	{
		VIM_TEXT DebugData[VIM_PICC_DATA_BUFFER_LEN+1];
		vim_mem_clear( DebugData, VIM_SIZEOF( DebugData ));

		VIM_DBG_NUMBER( PiccDebugDataUnionPtr->s_PiccDebugData.TagDataLen );
		VIM_DBG_NUMBER( PiccDebugDataUnionPtr->s_PiccDebugData.TapErrors );
		VIM_DBG_NUMBER( PiccDebugDataUnionPtr->s_PiccDebugData.CollisionErrors );

		if( PiccDebugDataUnionPtr->s_PiccDebugData.TagDataLen )
		{
			vim_mem_copy( DebugData, PiccDebugDataUnionPtr->s_PiccDebugData.TagData, VIM_MIN( VIM_SIZEOF( DebugData ), PiccDebugDataUnionPtr->s_PiccDebugData.TagDataLen ));
			pceft_debug_test( pceftpos_handle.instance, DebugData );
			vim_mem_clear( DebugData, VIM_SIZEOF( DebugData ));
		}
		if( PiccDebugDataUnionPtr->s_PiccDebugData.TapErrors )
		{
			vim_sprintf( DebugData, "TAP ERRORS:%3.3d", PiccDebugDataUnionPtr->s_PiccDebugData.TapErrors );
			pceft_debug_test( pceftpos_handle.instance, DebugData );
			terminal.stats.contactless_tap_fails += PiccDebugDataUnionPtr->s_PiccDebugData.TapErrors;
		}
		if( PiccDebugDataUnionPtr->s_PiccDebugData.CollisionErrors )
		{
			vim_sprintf( DebugData, "COLLISION ERRORS:%3.3d", PiccDebugDataUnionPtr->s_PiccDebugData.CollisionErrors );
			pceft_debug_test( pceftpos_handle.instance, DebugData );
			terminal.stats.contactless_collisions += PiccDebugDataUnionPtr->s_PiccDebugData.CollisionErrors;
		}

	}
	return VIM_ERROR_NONE;
}


// RDD 210218 Create a function to process PPSE response and X-ref with CPAT table
VIM_ERROR_PTR vim_picc_callback_process_ppse(VIM_UINT8 *PPSERespPtr, VIM_SIZE PPSERespLen, VIM_UINT8 *AIDToDisablePtr, VIM_SIZE *AIDToDisableLen)
{
	//VIM_TLV_LIST tags;
	//VIM_TLV_LIST container_tags;

	// RDD 210218 Search the buffer for the EFTPOS RID and exit if not present in the PPSE resp.
	VIM_ERROR_RETURN_ON_FAIL(vim_mem_search(PPSERespPtr, PPSERespLen, (VIM_UINT8 *)VIM_EMV_RID_EPAL, VIM_ICC_RID_LEN));

	if (vim_mem_search(PPSERespPtr, PPSERespLen, (VIM_UINT8 *)VIM_EMV_MASTERCARD_AID_GEN, VIM_EMV_STD_AID_LEN) == VIM_ERROR_NONE)
	{
		vim_mem_copy(AIDToDisablePtr, VIM_EMV_MASTERCARD_AID_GEN, VIM_EMV_STD_AID_LEN);
		*AIDToDisableLen = VIM_EMV_STD_AID_LEN;
		return VIM_ERROR_NONE;
	}
	else if (vim_mem_search(PPSERespPtr, PPSERespLen, (VIM_UINT8 *)VIM_EMV_VISA_AID_GEN, VIM_EMV_STD_AID_LEN) == VIM_ERROR_NONE)
	{
		vim_mem_copy(AIDToDisablePtr, VIM_EMV_VISA_AID_GEN, VIM_EMV_STD_AID_LEN);
		*AIDToDisableLen = VIM_EMV_STD_AID_LEN;
		return  VIM_ERROR_NONE;
	}

#if 0
	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&tags, (void *)PPSERespPtr, PPSERespLen, PPSERespLen, VIM_FALSE));

	// Open the "E1" container which is common to all RIDs
	if (vim_tlv_find(&tags, VIM_TAG_EMV_FCI_TEMPLATE) == VIM_ERROR_NONE)
	{
		DBG_MESSAGE("FCI template found");

		// If this is a valid TLV structure then open it
		if (vim_tlv_open(&container_tags, tags.current_item.value, tags.current_item.length, tags.current_item.length, VIM_FALSE) == VIM_ERROR_NONE)
		{
			VIM_DBG_TLV(container_tags);
			// Run a loop to find any non-EFTPOS AID and set this as disabled
			while (1)
			{
				//if (vim_tlv_search(&container_tags, VIM_TAG_EMV_AID_ICC) == VIM_ERROR_NONE)
				if (vim_tlv_search(&container_tags, VIM_TAG_EMV_DEDICATED_FILE_NAME) == VIM_ERROR_NONE)
				{
					if (vim_mem_compare(container_tags.current_item.value, VIM_EMV_RID_EPAL, VIM_ICC_RID_LEN) == VIM_ERROR_NONE)
						continue;
					else
					{
						// Found a non-EFTPOS AID in a multi-network card so now process the BIN
						// RDD 210218 - TODO - BIN Processing from CPAT

						vim_mem_copy(AIDToDisablePtr, container_tags.current_item.value, container_tags.current_item.length);
						*AIDToDisableLen = container_tags.current_item.length;

						return VIM_ERROR_NONE;
					}
				}
				else
					break;
			}
		}
	}
#endif
	return VIM_ERROR_NOT_FOUND;
}

#define CTLS_DRL_FILE "CTLSDRL.XML"
#define FILE_MAX_SIZE 2048
#define _INDEX_TAG "INDEX>"
#define _FLOOR_TAG "FLOOR>"
#define _TXN_TAG "TXN>"
#define _CVM_TAG "CVM>"
#define _CAP_TAG "CAP>"

// RDD 101019 JIRA WWBP - 662 - Scope restricted to AMEX only update for this release

VIM_ERROR_PTR ctls_drl_load( void )
{
    const char *ctls_drl_file = VIM_FILE_PATH_LOCAL CTLS_DRL_FILE;
    emv_aid_parameters_t *aid_ptr = terminal.emv_applicationIDs;
    emv_ctls_drl_t *drl_table_ptr = VIM_NULL;
    //VIM_CHAR *Ptr = VIM_NULL;
    VIM_BOOL AmexIncluded = VIM_FALSE;

    VIM_UINT8 CfgFileBuffer[FILE_MAX_SIZE];
    VIM_SIZE CfgFileSize = 0;
    VIM_BOOL exists = VIM_FALSE;
    VIM_CHAR *StartPtr = VIM_NULL;
    VIM_CHAR *EndPtr = StartPtr;
    VIM_SIZE n;
    // RDD 101019 Initiliase the table pointer to the start of the aid
    VIM_CHAR Container[500];

    vim_mem_clear( CfgFileBuffer, FILE_MAX_SIZE );

    VIM_ERROR_IGNORE( vim_file_exists( ctls_drl_file, &exists ));

    // If there's no file then we can leave everything as defaults
    if( !exists )
    {
        return VIM_ERROR_NOT_FOUND;
    }

    // RDD - find the Amex aid configuration
    for( n = 0; n < MAX_EMV_APPLICATION_ID_SIZE; n++, aid_ptr++ )
    {
        if (!aid_ptr->contactless)
            continue;

        if( IS_AMEX(n) )
        {
            VIM_DBG_DATA(terminal.emv_applicationIDs[n].emv_aid, 5);
            AmexIncluded = VIM_TRUE;
            break;
        }
    }

    if ( !AmexIncluded )
        VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );

    // Now we're pointing to the correct table, we need to see if theres anything to parse into it;

    // Read the file into a buffer
    VIM_ERROR_RETURN_ON_FAIL( vim_file_get( ctls_drl_file, CfgFileBuffer, &CfgFileSize, CFG_FILE_MAX_SIZE ));

    RemoveComments((VIM_CHAR *)CfgFileBuffer);
    StartPtr = (VIM_CHAR *)CfgFileBuffer;

    VIM_DBG_STRING(terminal.configuration.AdditionalDE55Data);

    //VIM_CHAR *RIDPtr = Container;
    vim_mem_clear( Container, VIM_SIZEOF( Container ));

    // Get the next SCHEME Container
    while( xml_find_next( &StartPtr, _INDEX_TAG, Container) == VIM_ERROR_NONE )
    {
        VIM_UINT8 k,m = asc_to_bin( Container, VIM_MIN( vim_strlen(Container), 2 ));

        // init the table ptr to the start of the Amex drl tables then push forward to the index
        drl_table_ptr = aid_ptr->drl_tables;

        for( k = 1; k < m; k++ )
            drl_table_ptr++;

        if( xml_get_tag_end( StartPtr, _INDEX_TAG, Container, &EndPtr ) == VIM_ERROR_NONE )
        {
            xml_parse_tag(EndPtr, (void *)&drl_table_ptr->ContactlessCVMRequiredLimit_DFAB42, _CVM_TAG, NUMBER_64_BIT);
            xml_parse_tag(EndPtr, (void *)&drl_table_ptr->ContactlessFloorLimit_DFAB40, _FLOOR_TAG, NUMBER_64_BIT);
            xml_parse_tag(EndPtr, (void *)&drl_table_ptr->ContactlessTransactionLimit_DFAB41, _TXN_TAG, NUMBER_64_BIT);
            xml_parse_tag(EndPtr, (void *)&drl_table_ptr->OnOffSwitch_DFAB49, _CAP_TAG, NUMBER_8_BIT);
            xml_get_tag_end(EndPtr, _CAP_TAG, Container, &StartPtr);
        }
    }
    return VIM_ERROR_NONE;
}


VIM_BOOL vim_drv_picc_emv_cb_mcr_limit_check(VIM_UINT64 amount, VIM_UINT8 *DefaultRID)
{
    VIM_DBG_NUMBER(amount);
    VIM_DBG_DATA(DefaultRID, 5);

    if (vim_mem_compare(DefaultRID, VIM_EMV_RID_VISA, VIM_ICC_RID_LEN) == VIM_ERROR_NONE)
    {           
        VIM_DBG_MESSAGE("MCR - VISA Limit Check");
        if ((amount >= (TERM_MCR_VISA_LOW * 100)) && (amount <= (TERM_MCR_VISA_HIGH * 100)))
        {
            pceft_debug_test(pceftpos_handle.instance, "MCR - Limits OK : Route VISA to EFTPOS");
            transaction_set_status(&txn, st_mcr_active);
            return VIM_TRUE;
        }
    }
    else if (vim_mem_compare(DefaultRID, VIM_EMV_RID_MASTERCARD, VIM_ICC_RID_LEN) == VIM_ERROR_NONE)
    {
        VIM_DBG_MESSAGE("MCR - MC Limit Check");
        if ((amount >= (TERM_MCR_MC_LOW * 100)) && (amount <= (TERM_MCR_MC_HIGH * 100)))
        {
            pceft_debug_test(pceftpos_handle.instance, "MCR - Limits OK : Route MC to EFTPOS");
            transaction_set_status(&txn, st_mcr_active);
            return VIM_TRUE;
        }
    }
    pceft_debug_test(pceftpos_handle.instance, "MCR No routing as outside TMS limit for this scheme");
    return VIM_FALSE;
}
