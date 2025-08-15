#include <incs.h>
VIM_DBG_SET_FILE("TQ");
/**
 * Serialize a card structure
 *
 * \param[in] card
 * card to serialize
 *
 * \param[out] buffer
 * pointer to buffer to write serialized data to
 *
 * \param[in, out] length
 * Should contain length of buffer when function is called. After call, will contain length of serialized data.
 */

//#define DEBUG_TERM_CAP "\xE0\xF8\xC8"	// RDD - no cvm only
//#define DEBUG_ADD_CAP  "\xEF\x80\xF0\xA0\x01"

// RDD 240214 SPIRA:7314 - Set TTQ to online only

#define VISA_TTQ_ZERO_FLOOR     "\x36\x80\x00\x00"
#define VISA_TTQ				"\x36\x00\x00\x00"

#define DEFAULT_RMP_9F1D		"\x6C\xF0\x00\x00"
#define DPAS_TTQ				"\xB6\x00\x40\x00"

#define MC_FAKE_TTQ  "\x86\x00\x00\x00"


extern file_version_record_t file_status[];




// RDD 050114 SPIRA:8317 Add TCC and Add TCC to all AIDS
static VIM_ERROR_PTR ProcessTermCaps( VIM_TLV_LIST *sub_tlv_list, emv_aid_parameters_t *aid_ptr)
{
	VIM_DBG_WARNING( "Setup TCC" );
	vim_mem_copy( aid_ptr->term_cap, terminal.emv_default_configuration.terminal_capabilities, 3 );
	if( vim_tlv_search( sub_tlv_list, WOW_TCC_APP_SUB_1_SUB_GT_CVM_LIMIT ) == VIM_ERROR_NONE )
	{
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( sub_tlv_list, aid_ptr->term_cap, 3 ));
	}
	vim_mem_copy( aid_ptr->add_term_cap, terminal.emv_default_configuration.additional_terminal_capabilities, 5 );
	if( vim_tlv_find( sub_tlv_list, WOW_EMV_DEFAULT_PARAM_ADDITIONAL_CAPABILITIES) == VIM_ERROR_NONE )
	{
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( sub_tlv_list, aid_ptr->add_term_cap, 5 ));
	}
	VIM_DBG_DATA( aid_ptr->term_cap, 3 );
	return VIM_ERROR_NONE;
}

#define CARD_DATA_ENCRYPT
#define ENCRYPT_DATA_LENGTH   48

// RDD 120618 WW BP-112 Encrypt/Decrypt Card Data for PCI compliance

VIM_ERROR_PTR card_serialize(card_info_t *card, VIM_TLV_LIST *tlv_ptr)
{

	VIM_CHAR output_data[ENCRYPT_DATA_LENGTH];
  VIM_TLV_LIST sub_tlv;

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create_sublist(tlv_ptr,&sub_tlv))

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_CARD_TYPE, &card->type,VIM_SIZEOF(card->type)));

  // RDD added for WOW
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TOKEN_FLAG, &card->pci_data.eTokenFlag,VIM_SIZEOF(card->pci_data.eTokenFlag)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_E_TOKEN, &card->pci_data.eToken,VIM_SIZEOF(card->pci_data.eToken)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_E_PAN, &card->pci_data.ePan,VIM_SIZEOF(card->pci_data.ePan)));

  if (card->type == card_mag)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_EMV_FALLBACK, card->data.mag.emv_fallback));
#ifdef  CARD_DATA_ENCRYPT
	VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.mag.track2,
		card->data.mag.track2_length,
		output_data,
		VIM_SIZEOF(output_data)));

	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, output_data, VIM_SIZEOF(output_data)));

	VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.mag.ccv,
		VIM_SIZEOF(card->data.mag.ccv),
		output_data,
		VIM_SIZEOF(output_data)));

	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_4DBC, output_data, VIM_SIZEOF(output_data)));
#else
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, card->data.mag.track2, card->data.mag.track2_length));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_4DBC, card->data.mag.ccv, VIM_SIZEOF(card->data.mag.ccv)));
#endif

  }
  else if (card->type == card_manual)
  {

#ifdef  CARD_DATA_ENCRYPT
	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.manual.pan,
		  card->data.manual.pan_length,
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_PAN, output_data, VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.manual.expiry_mm,
		  VIM_SIZEOF(card->data.manual.expiry_mm),
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_EXPIRY_MM, output_data, VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.manual.expiry_yy,
		  VIM_SIZEOF(card->data.manual.expiry_yy),
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_EXPIRY_YY, output_data, VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.manual.ccv,
		  VIM_SIZEOF(card->data.manual.ccv),
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_4DBC, output_data, VIM_SIZEOF(output_data)));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_PAN, card->data.manual.pan, card->data.manual.pan_length));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_EXPIRY_MM, card->data.manual.expiry_mm, VIM_SIZEOF(card->data.manual.expiry_mm)));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_EXPIRY_YY, card->data.manual.expiry_yy, VIM_SIZEOF(card->data.manual.expiry_yy)));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_4DBC, card->data.manual.ccv, VIM_SIZEOF(card->data.manual.ccv)));
#endif
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_NO_CCV_REASON, card->data.manual.no_ccv_reason));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_MANUAL_REASON, card->data.manual.manual_reason));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_RECURRENCE_TYPE, card->data.manual.recurrence));
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_EMV_FALLBACK, card->data.manual.emv_fallback));
    // RDD 300621 Added for Avalon JIRA PIRPIN-1105
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_MANUAL_BARCODE, card->data.manual.barcode, VIM_SIZEOF(card->data.manual.barcode)));


  }
  else if (card->type == card_chip)
  {
#ifdef  CARD_DATA_ENCRYPT
	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.chip.track2,
		  card->data.chip.track2_length,
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, output_data, VIM_SIZEOF(output_data)));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, card->data.chip.track2, card->data.chip.track2_length));
#endif
  }
  else if (card_picc == card->type)
  {
#ifdef  CARD_DATA_ENCRYPT
	  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_encrypt_card_data(card->data.picc.track2,
		  card->data.picc.track2_length,
		  output_data,
		  VIM_SIZEOF(output_data)));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, output_data, VIM_SIZEOF(output_data)));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&sub_tlv, TAG_TRACK_2, card->data.picc.track2, card->data.picc.track2_length));
#endif
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&sub_tlv, TAG_MSD, card->data.picc.msd));
  }

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_sublist(tlv_ptr, TAG_CARD_CONTAINER, &sub_tlv));
  return VIM_ERROR_NONE;
}

/**
 * Convert serialized card data back to card structure
 *
 * \param[out] card
 * card to populate
 *
 * \param[in] buffer
 * pointer to buffer containing serialized data
 *
 * \param[in] length
 * Length of data in buffer
 */
static VIM_ERROR_PTR card_deserialize(card_info_t *card, VIM_TLV_LIST *tlv_ptr)
{
    // RDD 161019 - change both below items from signed to unsigned for safety in VIM_MIN comparison.
	VIM_UINT8 input_data[ENCRYPT_DATA_LENGTH];
	VIM_UINT8 decrypted_data[ENCRYPT_DATA_LENGTH];

  vim_mem_clear(card, VIM_SIZEOF(*card));

  card->type = card_none;
  if (vim_tlv_search(tlv_ptr, TAG_CARD_TYPE) == VIM_ERROR_NONE)
  {
     VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes_exact(tlv_ptr, &card->type,VIM_SIZEOF(card->type)));
  }

  // RDD PCI stuff added for WOW
  if (vim_tlv_search(tlv_ptr, TAG_TOKEN_FLAG) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, &card->pci_data.eTokenFlag, VIM_SIZEOF(card->pci_data.eTokenFlag)));
  if (vim_tlv_search(tlv_ptr, TAG_E_TOKEN) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, &card->pci_data.eToken, VIM_SIZEOF(card->pci_data.eToken)));
  if (vim_tlv_search(tlv_ptr, TAG_E_PAN) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, &card->pci_data.ePan, VIM_SIZEOF(card->pci_data.ePan)));

  card->type = card_none;
  if (vim_tlv_search(tlv_ptr, TAG_CARD_TYPE) == VIM_ERROR_NONE)
  {
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes_exact(tlv_ptr, &card->type, VIM_SIZEOF(card->type)));
  }

  if (card->type == card_mag)
  {
    if (vim_tlv_search(tlv_ptr, TAG_TRACK_2) == VIM_ERROR_NONE)
    {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* get a length of original data */
		  card->data.mag.track2_length = VIM_MIN(decrypted_data[0], MAX_TRACK2_LENGTH);

		  /* copy original data to destination */
		  vim_mem_copy(card->data.mag.track2, &decrypted_data[1], card->data.mag.track2_length);
#else
      card->data.mag.track2_length = (VIM_UINT8)MIN(VIM_SIZEOF(card->data.mag.track2), tlv_ptr->current_item.length);
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.mag.track2, card->data.mag.track2_length));
#endif
    }

    if (vim_tlv_search(tlv_ptr, TAG_EMV_FALLBACK) == VIM_ERROR_NONE)
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, &card->data.mag.emv_fallback));
    }

    // RDD 300621 Added for Avalon JIRA PIRPIN-1105 - non PCI so don't encrypt for PFR
    if (vim_tlv_search(tlv_ptr, TAG_MANUAL_BARCODE) == VIM_ERROR_NONE)
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.manual.barcode, VIM_SIZEOF(card->data.manual.barcode)));
    }

    if (vim_tlv_search(tlv_ptr, TAG_4DBC) == VIM_ERROR_NONE)
    {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* copy original data to destination */
		  vim_mem_copy(card->data.mag.ccv, &decrypted_data[1], decrypted_data[0]);
#else
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.mag.ccv, VIM_SIZEOF(card->data.mag.ccv)));
#endif
    }
  }
  else if (card->type == card_manual)
  {
    if (vim_tlv_search(tlv_ptr, TAG_PAN) == VIM_ERROR_NONE)
    {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* get a length of original data */
		  card->data.manual.pan_length = decrypted_data[0];

		  /* copy original data to destination */
		  vim_mem_copy(card->data.manual.pan, &decrypted_data[1], card->data.manual.pan_length);
#else
      card->data.manual.pan_length = MIN(VIM_SIZEOF(card->data.manual.pan), tlv_ptr->current_item.length);
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.manual.pan, card->data.manual.pan_length));
#endif
    }
    if (vim_tlv_search(tlv_ptr, TAG_EXPIRY_MM) == VIM_ERROR_NONE)
	  {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* copy original data to destination */
		  vim_mem_copy(card->data.manual.expiry_mm, &decrypted_data[1], decrypted_data[0]);
#else
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.manual.expiry_mm, VIM_SIZEOF(card->data.manual.expiry_mm)));
#endif
	  }
    if (vim_tlv_search(tlv_ptr, TAG_EXPIRY_YY) == VIM_ERROR_NONE)
	  {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* copy original data to destination */
		  vim_mem_copy(card->data.manual.expiry_yy, &decrypted_data[1], decrypted_data[0]);
#else
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.manual.expiry_yy, VIM_SIZEOF(card->data.manual.expiry_yy)));
#endif
	  }
    if (vim_tlv_search(tlv_ptr, TAG_4DBC) == VIM_ERROR_NONE)
	  {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* copy original data to destination */
		  vim_mem_copy(card->data.manual.ccv, &decrypted_data[1], decrypted_data[0]);
#else
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.manual.ccv, VIM_SIZEOF(card->data.manual.ccv)));
#endif
	  }
    if (vim_tlv_search(tlv_ptr, TAG_NO_CCV_REASON) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, (VIM_UINT8 *)&card->data.manual.no_ccv_reason));
    if (vim_tlv_search(tlv_ptr, TAG_MANUAL_REASON) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, (VIM_UINT8 *)&card->data.manual.manual_reason));
    if (vim_tlv_search(tlv_ptr, TAG_RECURRENCE_TYPE) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, (VIM_UINT8 *)&card->data.manual.recurrence));
    if (vim_tlv_search(tlv_ptr, TAG_EMV_FALLBACK) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, &card->data.manual.emv_fallback));
  }
  else if (card->type == card_chip)
  {
    if (vim_tlv_search(tlv_ptr, TAG_TRACK_2) == VIM_ERROR_NONE)
    {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* get a length of original data */
		  card->data.chip.track2_length = VIM_MIN(decrypted_data[0], MAX_TRACK2_LENGTH);

		  /* copy original data to destination */
		  vim_mem_copy(card->data.chip.track2, &decrypted_data[1], card->data.chip.track2_length);
#else
      card->data.chip.track2_length = (VIM_UINT8)MIN(VIM_SIZEOF(card->data.chip.track2), tlv_ptr->current_item.length);
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.chip.track2, card->data.chip.track2_length));
#endif
    }
  }
  else if (card->type == card_picc)
  {
    if (vim_tlv_search(tlv_ptr, TAG_TRACK_2) == VIM_ERROR_NONE)
    {
#ifdef  CARD_DATA_ENCRYPT
		  /* get encrypted data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, input_data, VIM_SIZEOF(input_data)));

		  /* decrypt data */
		  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_decrypt_card_data((VIM_CHAR*)input_data,
			  VIM_SIZEOF(input_data),
			  (VIM_CHAR*)decrypted_data,
			  VIM_SIZEOF(decrypted_data)));

		  /* get a length of original data */
		  card->data.picc.track2_length = VIM_MIN(decrypted_data[0], MAX_TRACK2_LENGTH);

		  /* copy original data to destination */
		  vim_mem_copy(card->data.picc.track2, &decrypted_data[1], card->data.picc.track2_length);
#else
      card->data.picc.track2_length = (VIM_UINT8)MIN(VIM_SIZEOF(card->data.picc.track2), tlv_ptr->current_item.length);
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(tlv_ptr, card->data.picc.track2, card->data.picc.track2_length));
#endif
    }

    if (vim_tlv_search(tlv_ptr, TAG_MSD) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(tlv_ptr, &card->data.picc.msd));
  }
  return VIM_ERROR_NONE;
}

/**
 * Serialize a transaction structure ready for writing to file
 *
 * \param[in] transaction
 * transaction to serialize
 *
 * \param[out] buffer
 * pointer to buffer to write serialized data to
 *
 * \param[in, out] length
 * Should contain length of buffer when function is called. After call, will contain length of serialized data.
 */
VIM_ERROR_PTR transaction_serialize
(
  transaction_t *transaction,
  VIM_UINT8 *destination_ptr,
  VIM_SIZE *destination_size_ptr,
  VIM_SIZE destination_max_size
)
{
  VIM_TLV_LIST tags;

  //DBG_MESSAGE( "******* SAVE TRANSACTION STRUCTURE TO FILE *******" );
  //DBG_VAR( destination_max_size );

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tags, destination_ptr, destination_max_size));



  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_TYPE, &transaction->type,VIM_SIZEOF(transaction->type)));



  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_ORIGINAL_TYPE, &transaction->original_type,VIM_SIZEOF(transaction->type)));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_HOST_RC_ASC, transaction->host_rc_asc, VIM_SIZEOF(transaction->host_rc_asc)));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_HOST_RC_ASC_BACKUP, transaction->host_rc_asc_backup, VIM_SIZEOF(transaction->host_rc_asc_backup)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_ADDITIONAL_DATA, transaction->additional_data_prompt, VIM_SIZEOF(transaction->additional_data_prompt)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_RRN, transaction->rrn, VIM_SIZEOF(transaction->rrn)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_QUALIFY_SVP, transaction->qualify_svp));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_CARD_NAME_INDEX, transaction->card_name_index));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_PAYMENT_OPTION_INDEX, transaction->payment_option_index));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_PAYMENT_CARD_NAME_INDEX, transaction->payment_card_name_index));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_AMT_PURCHASE, transaction->amount_purchase));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_AMT_REFUND, transaction->amount_refund));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_AMT_CASH, transaction->amount_cash));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_AMT_TIP, transaction->amount_tip));
           
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_SURCHARGE_AMOUNT, transaction->amount_surcharge));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TXN_AMT_TOTAL, transaction->amount_total));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_ACCOUNT, &transaction->account,VIM_SIZEOF(transaction->account)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_TIME, &transaction->time, VIM_SIZEOF(transaction->time)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_SEND_TIME, &transaction->send_time, VIM_SIZEOF(transaction->send_time)));

  //VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_BATCH_NO, transaction->batch_no));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_ROC_NO, transaction->roc));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_ROC_INDEX, transaction->roc_index));

  /* todo: fix this - errors are pointers, they should not be directly serialized */

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_ERROR, (VIM_UINT32)transaction->error));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_BACKUP_ERROR, (VIM_UINT32)transaction->error_backup));

  /* todo: find a nice way to serialize errors */

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_STAN, transaction->stan));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_AUTH_NO, transaction->auth_no, VIM_SIZEOF(transaction->auth_no)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_SETTLE_DATE, transaction->settle_date, VIM_SIZEOF(transaction->settle_date)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_STATUS, transaction->status));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_ADDITIONAL_STATUS, transaction->add_status));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_POS_CMD_CODE, transaction->pos_cmd));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_MSG_TYPE, &transaction->msg_type, VIM_SIZEOF(transaction->msg_type)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_POS_REF_NO, &transaction->pos_reference, VIM_SIZEOF(transaction->pos_reference)));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_TURNAROUND, transaction->turnaround));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_PRINT_RECEIPT, transaction->print_receipt));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_UPDATE_INDEX, transaction->update_index));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_READ_ONLY, transaction->read_only));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_ORIGINAL_INDEX, transaction->original_index));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_POS_SUB_CODE, (VIM_UINT8)transaction->pos_subcode));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_PIN_BLOCK_SIZE, transaction->pin_block_size));



  // RDD 010911 - added for WOW
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_PRESWIPE_STATUS, transaction->preswipe_status));

    // BRD 190112 - added for WOW

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_ORIG_STAN, transaction->orig_stan));

  if( fuel_card(transaction) )
  {
	  VIM_SIZE len = VIM_SIZEOF(transaction->u_PadData.ByteBuff);
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes( &tags, TAG_FUEL_DATA, transaction->u_PadData.ByteBuff, len ));
  }
  else
  {
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_OFFLINE_PIN, transaction->emv_pin_entry_params.offline_pin));
	  // JQ
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_CID, transaction->cid, VIM_MIN( vim_strlen(transaction->cid), VIM_SIZEOF(transaction->cid))));
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_CRYPTO, transaction->crypto, VIM_MIN( vim_strlen(transaction->crypto), VIM_SIZEOF(transaction->crypto))));

	  /* Update EMV AID index used for the transaction, as in some PICC scenario's we don't have AID updated to EMV data buffer */
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_EMV_AID_INDEX, transaction->aid_index));

	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_EMV_DATA_SIZE, transaction->emv_data.data_size));
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_EMV_DATA_BUFFER, &transaction->emv_data.buffer,
                    MIN( VIM_SIZEOF(transaction->emv_data.buffer),transaction->emv_data.data_size)));
  }
  // VN JIRA WWBP-332  10/10/18 Tag 8A missing off diagnostic receipts for Contactless
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_SAF_REASON, transaction->saf_reason));


  // RDD 290721 JIRA PIRPIN-1118 VDA - need to save PIN Block in the SAF for VDA
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_PIN_BLOCK, &transaction->pin_block.bytes, VIM_SIZEOF(VIM_DES_BLOCK)));


  /* Add card sub-tag */

  VIM_ERROR_RETURN_ON_FAIL(card_serialize(&transaction->card, &tags));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TXN_EDP_SESSION_ID, transaction->edp_payment_session_id, VIM_MIN(vim_strlen(transaction->edp_payment_session_id), VIM_SIZEOF(transaction->edp_payment_session_id))));

  //DBG_VAR( tags.total_length );

  /*VIM_DBG_TLV(tags);*/

  // RDD 100523 JIRA PIRPIN-2376
  VIM_DBG_VAR(transaction->pceftpos_device_code);
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TXN_DEVICE_CODE, transaction->pceftpos_device_code));

  *destination_size_ptr = tags.total_length;

  return VIM_ERROR_NONE;
}



// Only save tags that have non zero data
VIM_BOOL IsNonZero( const VIM_CHAR *Ptr, VIM_SIZE Len )
{
	do
	{
		if( *Ptr )
			return VIM_TRUE;
	}
	while( --Len );
	return VIM_FALSE;
}



/**
 * Convert serialized transaction data back to transaction structure
 *
 * \param[out] transaction
 * transaction to populate
 *
 * \param[in] buffer
 * pointer to buffer containing serialized data
 *
 * \param[in] length
 * Length of data in buffer
 */
VIM_ERROR_PTR transaction_deserialize
(
  transaction_t *transaction_ptr,
  void *source_ptr,
  VIM_SIZE source_size,
  VIM_BOOL get_txn_status
)
{
  VIM_TLV_LIST tags;

  vim_mem_clear(transaction_ptr, VIM_SIZEOF(*transaction_ptr));

  //DBG_VAR(source_size);
#if 0
  VIM_ERROR_IGNORE(vim_tlv_open(&tags, source_ptr, source_size, source_size, VIM_FALSE));
#else
  // RDD 300317 Error in SAF read record caused this to generate an error which was ignored
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open( &tags, source_ptr, source_size, source_size, VIM_FALSE ));
#endif
  /*VIM_DBG_TLV(tags);*/

  if (vim_tlv_search(&tags, TAG_TXN_TYPE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes_exact(&tags, &transaction_ptr->type,VIM_SIZEOF(transaction_ptr->type)));
  if (vim_tlv_search(&tags, TAG_TXN_ORIGINAL_TYPE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes_exact(&tags, &transaction_ptr->original_type,VIM_SIZEOF(transaction_ptr->original_type)));
  if (vim_tlv_search(&tags, TAG_TXN_HOST_RC_ASC) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->host_rc_asc, VIM_SIZEOF(transaction_ptr->host_rc_asc)));
  if (vim_tlv_search(&tags, TAG_TXN_HOST_RC_ASC_BACKUP) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->host_rc_asc_backup, VIM_SIZEOF(transaction_ptr->host_rc_asc_backup)));
  if (vim_tlv_search(&tags, TAG_TXN_ADDITIONAL_DATA) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->additional_data_prompt, VIM_SIZEOF(transaction_ptr->additional_data_prompt)));
  if (vim_tlv_search(&tags, TAG_TXN_RRN) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->rrn, VIM_SIZEOF(transaction_ptr->rrn)));
  if (vim_tlv_search(&tags, TAG_TXN_QUALIFY_SVP) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, &transaction_ptr->qualify_svp));
  if (vim_tlv_search(&tags, TAG_TXN_PAYMENT_OPTION_INDEX) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, &transaction_ptr->payment_option_index));
  if (vim_tlv_search(&tags, TAG_TXN_PAYMENT_CARD_NAME_INDEX) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, &transaction_ptr->payment_card_name_index));
  if (vim_tlv_search(&tags, TAG_TXN_CARD_NAME_INDEX) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->card_name_index));
#if 0
  // RDD added for WOW
  if (vim_tlv_search(&tags, TAG_CARD_TYPE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->card.type, VIM_SIZEOF(transaction_ptr->card.type)));
#endif
  if (vim_tlv_search(&tags, TAG_TXN_AMT_PURCHASE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_purchase));
  if (vim_tlv_search(&tags, TAG_TXN_AMT_REFUND) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_refund));
  if (vim_tlv_search(&tags, TAG_TXN_AMT_CASH) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_cash));
  if (vim_tlv_search(&tags, TAG_TXN_AMT_TIP) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_tip));
  if (vim_tlv_search(&tags, TAG_TXN_SURCHARGE_AMOUNT) == VIM_ERROR_NONE)
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_surcharge));

  if (vim_tlv_search(&tags, TAG_TXN_AMT_TOTAL) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint64_little(&tags, &transaction_ptr->amount_total));
  if (vim_tlv_search(&tags, TAG_TXN_ACCOUNT) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes_exact(&tags, &transaction_ptr->account,VIM_SIZEOF(transaction_ptr->account)));
  if (vim_tlv_search(&tags, TAG_TXN_TIME) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->time, VIM_SIZEOF(transaction_ptr->time)));
  if (vim_tlv_search(&tags, TAG_TXN_SEND_TIME) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->send_time, VIM_SIZEOF(transaction_ptr->send_time)));
 // if (vim_tlv_search(&tags, TAG_TXN_BATCH_NO) == VIM_ERROR_NONE)
 //   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->batch_no));
  if (vim_tlv_search(&tags, TAG_TXN_ROC_NO) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->roc));
  if (vim_tlv_search(&tags, TAG_TXN_ROC_INDEX) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->roc_index));
  if (vim_tlv_search(&tags, TAG_TXN_ERROR) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->error));
  if (vim_tlv_search(&tags, TAG_TXN_BACKUP_ERROR) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->error_backup));
  if (vim_tlv_search(&tags, TAG_TXN_STAN) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&transaction_ptr->stan));
  if (vim_tlv_search(&tags, TAG_TXN_AUTH_NO) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->auth_no, VIM_SIZEOF(transaction_ptr->auth_no)));
  if (vim_tlv_search(&tags, TAG_TXN_SETTLE_DATE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->settle_date, VIM_SIZEOF(transaction_ptr->settle_date)));
   // RDD 300312 SPIRA 5221 - Recalling the txn status for duplicates can stuff up reversal processing
  if ((vim_tlv_search(&tags, TAG_TXN_STATUS) == VIM_ERROR_NONE)&&(get_txn_status))
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->status));
  if ((vim_tlv_search(&tags, TAG_TXN_ADDITIONAL_STATUS) == VIM_ERROR_NONE) && (get_txn_status))
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->add_status));


  if (vim_tlv_search(&tags, TAG_TXN_POS_CMD_CODE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->pos_cmd));
  if (vim_tlv_search(&tags, TAG_TXN_MSG_TYPE) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->msg_type, sizeof(transaction_ptr->msg_type)));
  if (vim_tlv_search(&tags, TAG_TXN_POS_REF_NO) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->pos_reference, sizeof(transaction_ptr->pos_reference)));

  if (vim_tlv_search(&tags, TAG_TXN_TURNAROUND) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->turnaround));

  if (vim_tlv_search(&tags, TAG_TXN_PRINT_RECEIPT) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->print_receipt));

  if (vim_tlv_search(&tags, TAG_TXN_UPDATE_INDEX) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->update_index));

  if (vim_tlv_search(&tags, TAG_TXN_READ_ONLY) == VIM_ERROR_NONE)
     VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->read_only));

 if (vim_tlv_search(&tags, TAG_TXN_ORIGINAL_INDEX) == VIM_ERROR_NONE)
   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->original_index));

  if (vim_tlv_search(&tags, TAG_TXN_POS_SUB_CODE) == VIM_ERROR_NONE)
     VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->pos_subcode));

  if (vim_tlv_search(&tags, TAG_TXN_PIN_BLOCK_SIZE) == VIM_ERROR_NONE)
   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->pin_block_size));

  //BRD
    if (vim_tlv_search(&tags, TAG_ORIG_STAN) == VIM_ERROR_NONE)
   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->orig_stan));


  // RDD 010911 - added for WOW
  if (vim_tlv_search(&tags, TAG_TXN_PRESWIPE_STATUS) == VIM_ERROR_NONE)
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->preswipe_status));


  // RDD 010612 - deserialise the card first so we know if it's a fuel card...
  if (vim_tlv_search(&tags, TAG_CARD_CONTAINER) == VIM_ERROR_NONE)
  {
    VIM_TLV_LIST sub_tlv;
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open_current(&tags,&sub_tlv,VIM_FALSE))
    /* Extract card subtag */
    VIM_ERROR_RETURN_ON_FAIL(card_deserialize(&transaction_ptr->card, &sub_tlv));
  }

  // RDD 270512 added
  if( fuel_card(transaction_ptr) )
  {
	if (vim_tlv_search(&tags, TAG_FUEL_DATA) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->u_PadData.ByteBuff, VIM_SIZEOF(transaction_ptr->u_PadData)));
  }
  else
  {
	if (vim_tlv_search(&tags, TAG_TXN_OFFLINE_PIN) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->emv_pin_entry_params.offline_pin));
	  //JQ
	if (vim_tlv_search(&tags, TAG_TXN_CID) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->cid, sizeof(transaction_ptr->cid)));
	if (vim_tlv_search(&tags, TAG_TXN_CRYPTO) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->crypto, sizeof(transaction_ptr->crypto)));
	  /* Retrive EMV AID index used for this transaction. This is needed in some PICC scenario's where
        we don't have AID's updated to EMV data buffer */
	if (vim_tlv_search(&tags, TAG_TXN_EMV_AID_INDEX) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, &transaction_ptr->aid_index));
	if (vim_tlv_search(&tags, TAG_TXN_EMV_DATA_SIZE) == VIM_ERROR_NONE)
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&tags, &transaction_ptr->emv_data.data_size));

	if (vim_tlv_search(&tags, TAG_TXN_EMV_DATA_BUFFER) == VIM_ERROR_NONE)
	{
		VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &transaction_ptr->emv_data.buffer,
                     MIN( VIM_SIZEOF(transaction_ptr->emv_data.buffer),transaction_ptr->emv_data.data_size)));
	}
  }
    // VN JIRA WWBP-332  10/10/18 Tag 8A missing off diagnostic receipts for Contactles
    if (vim_tlv_search(&tags, TAG_TXN_SAF_REASON) == VIM_ERROR_NONE)
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&transaction_ptr->saf_reason));

    if (vim_tlv_search(&tags, TAG_TXN_PIN_BLOCK) == VIM_ERROR_NONE)
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, (VIM_UINT8 *)&transaction_ptr->pin_block.bytes, VIM_SIZEOF(VIM_DES_BLOCK)));

    if (vim_tlv_search(&tags, TAG_TXN_EDP_SESSION_ID) == VIM_ERROR_NONE)
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, transaction_ptr->edp_payment_session_id, sizeof(transaction_ptr->edp_payment_session_id)));

    // RDD 100523 JIRA PIRPIN-2376 - restore the device code ( varies for devices )
    if (vim_tlv_search(&tags, TAG_TXN_DEVICE_CODE) == VIM_ERROR_NONE)
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&tags, (VIM_UINT8*)&transaction_ptr->pceftpos_device_code));


  return VIM_ERROR_NONE;
}



/**
 * Serialize a terminal structure ready for writing to file
 *
 * \param[in] transaction
 * terminal structure to serialize
 *
 * \param[out] buffer
 * pointer to buffer to write serialized data to
 *
 * \param[in, out] length
 * Should contain length of buffer when function is called. After call, will contain length of serialized data.
 */

VIM_ERROR_PTR terminal_serialize
(
  terminal_t const *terminal_ptr,
  void *destination_ptr,
  VIM_SIZE *destination_size_ptr,
  VIM_SIZE destination_size
)
{
  VIM_TLV_LIST tags;


  // DBG_MESSAGE( "******* SAVE TERMINAL STRUCTURE TO FILE *******" );

  *destination_size_ptr=0;

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tags, destination_ptr, destination_size));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_STAN, terminal_ptr->stan));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_ROC , terminal_ptr->roc));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_SFW_VERSION, terminal_ptr->sfw_version));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_SFW_MINOR, terminal_ptr->sfw_minor));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_SFW_PREV_VERSION, terminal_ptr->sfw_prev_version));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_SFW_PREV_MINOR, terminal_ptr->sfw_prev_minor));
  // RDD 100413 added for P4

  // RDD - added for WOW
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_CTLS_DRV_VERSION, terminal_ptr->contactless_file_name, VIM_SIZEOF(terminal_ptr->contactless_file_name)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_INTERNAL_MODEM, terminal_ptr->internal_modem));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_CPAT_VERSION, terminal_ptr->file_version[CPAT_FILE_IDX]));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_PKT_VERSION, terminal_ptr->file_version[PKT_FILE_IDX]));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_EPAT_VERSION, terminal_ptr->file_version[EPAT_FILE_IDX]));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_FCAT_VERSION, terminal_ptr->file_version[FCAT_FILE_IDX]));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_BUFFER_COUNT, terminal_ptr->buffer_count));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_KEK1_KVC, &terminal_ptr->kek1kvc, VIM_SIZEOF(terminal_ptr->kek1kvc)));

    // RDD 191214 Added for ALH XML file maintenance
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_CFG_FILE_VER, &terminal_ptr->TerminalConfigFileVer, VIM_SIZEOF(terminal_ptr->TerminalConfigFileVer)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_LOGON_STATE, terminal_ptr->logon_state));
  DBG_VAR(terminal_ptr->logon_state);
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MERCHANT_NAME, &terminal_ptr->configuration.merchant_name, VIM_SIZEOF(terminal_ptr->configuration.merchant_name)) );
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MERCHANT_ADDRESS, &terminal_ptr->configuration.merchant_address, VIM_SIZEOF(terminal_ptr->configuration.merchant_address)) );
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_TIP_ENABLED, terminal_ptr->configuration.sale_with_tip));

  // RDD 010921 v590-6 JIRA-PIRPIN 1214 - set default true ie Woolies terminals do no need this to be set to zero.
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_NO_EFT_GIFT, terminal_ptr->configuration.no_eft_gift));

  // RDD 170223 v736 JIRA-PIRPIN 1820 - New Flow for Returns Cards
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_RETURNS_CARD_NEW_FLOW, terminal_ptr->configuration.ReturnsCardNewFlow));
 
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_ENCRYPT_DE47, terminal_ptr->configuration.encrypt_de47));
      
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_SNI_ODD, &terminal_ptr->configuration.SNIOdd, VIM_SIZEOF(terminal_ptr->configuration.SNIOdd)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_SNI_EVEN, &terminal_ptr->configuration.SNIEven, VIM_SIZEOF(terminal_ptr->configuration.SNIEven)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_HOST_URL, &terminal_ptr->configuration.HostURL, VIM_SIZEOF(terminal_ptr->configuration.HostURL)));

  // RDD 310122 v730 JIRA-PIRPIN 1407 WIFI Control via TMS
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_USE_WIFI, terminal_ptr->configuration.use_wifi));  
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_USE_RNDIS, terminal_ptr->configuration.use_rndis));

  // RDD 290322 JIRA PIRPIN-1521 
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_DISP_RES_SECS, terminal_ptr->configuration.disp_res_secs));

  // 160821 JIRA PIRPIN-1191
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_DINERS_DISABLE, terminal_ptr->configuration.diners_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_AMEX_DISABLE, terminal_ptr->configuration.amex_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_UPI_DISABLE, terminal_ptr->configuration.upi_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_JCB_DISABLE, terminal_ptr->configuration.jcb_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_QC_GIFT_DISABLE, terminal_ptr->configuration.qc_gift_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_WEX_GIFT_DISABLE, terminal_ptr->configuration.wex_gift_disable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_QPS_DISABLE, terminal_ptr->configuration.qps_disable));


  // RDD 260821 MCR Stuff
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_ENABLE_MCR, TERM_USE_MERCHANT_ROUTING));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_MCR_VISA_LOW_LIMIT, TERM_MCR_VISA_LOW));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_MCR_VISA_HIGH_LIMIT, TERM_MCR_VISA_HIGH));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_MCR_MC_LOW_LIMIT, TERM_MCR_MC_LOW));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_MCR_MC_HIGH_LIMIT, TERM_MCR_MC_HIGH));


  // RDD 241014 Block Size/window size for TMS ( important for GPRS/3G Comms )
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TXN_TMS_BLOCK_SIZE, terminal_ptr->tms_parameters.max_block_size));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TMS_WINDOW_SIZE, terminal_ptr->tms_parameters.window_size));


  // RDD 030517 added for Phase7 J8 Pass stuff
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TMS_FASTEMV_AMT, terminal_ptr->configuration.FastEmvAmount ));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_J8_ENABLE, terminal_ptr->configuration.j8_enable ));


  // RDD 090118 added for CTLS Cash on 2TapAndroid release
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_CTLS_CASH_ENABLE, terminal_ptr->configuration.CtlsCashOut));

  // RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_ENABLE_TECHNICAL_FALLBACK, terminal_ptr->configuration.technical_fallback_enable));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_TIP_MAX_PERCENT, terminal_ptr->configuration.max_tip_percent));



  // RDD 110917 added for SPIRA:9242
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TMS_MANPAN_PWORD, terminal_ptr->configuration.manpan_password ));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TERM_REFUND_TXN_LIMIT, terminal_ptr->configuration.refund_txn_limit ));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TERM_REFUND_DAILY_LIMIT, terminal_ptr->configuration.refund_daily_limit));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TERM_REFUND_DAILY_TOTAL, terminal_ptr->configuration.refund_daily_total));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_CPAT_ENTRIES, terminal_ptr->number_of_cpat_entries));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TERM_IDLE_LOOP_PROCESSING, terminal_ptr->max_idle_time_seconds));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_EFB_DEBIT_LIMIT, terminal_ptr->configuration.efb_debit_limit));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_EFB_CREDIT_LIMIT, terminal_ptr->configuration.efb_credit_limit));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_EFB_SAF_LIMIT, terminal_ptr->configuration.efb_max_txn_count));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_EFB_TOT_LIMIT, terminal_ptr->configuration.efb_max_txn_limit));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_LAST_TMS_LOGON_TIME, &terminal_ptr->last_real_logon_time, VIM_SIZEOF(terminal_ptr->last_real_logon_time)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_TMS_SCHEDULED_TIME, &terminal_ptr->tms_parameters.last_logon_time, VIM_SIZEOF(terminal_ptr->tms_parameters.last_logon_time)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_TMS_FREQUENCY, terminal_ptr->tms_parameters.logon_time_type));

  if( IsNonZero( terminal_ptr->ecg_bin, VIM_SIZEOF(terminal_ptr->ecg_bin) ))
	VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_ECG_BIN, terminal_ptr->ecg_bin, VIM_SIZEOF(terminal_ptr->ecg_bin)));

  /* Tags for Apple and Android VAS*/
  // VN 211117 added for WOW
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_COLLECTOR_ID, terminal_ptr->configuration.collector_id));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_MERCHANT_URL, terminal_ptr->configuration.merchant_url, vim_strlen(terminal_ptr->configuration.merchant_url)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_PASS_IDENTIFIER, terminal_ptr->configuration.pass_identfier, vim_strlen(terminal_ptr->configuration.pass_identfier)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_USE_GOOGLE_SDK, terminal_ptr->configuration.use_google_sdk));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_GOOGLE_KEY_VERSION, terminal_ptr->configuration.google_key_version));

  // VN 150218 Merchant routing
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_MERCHANT_ROUTING, terminal_ptr->configuration.use_mer_routing));

  // 110520 Trello #137 Change to New LFD
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_NEW_LFD, terminal_ptr->configuration.NewLFD ));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_WALLET,            terminal_ptr->configuration.QrWallet));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_PS_TIMEOUT,        terminal_ptr->configuration.QrPaymentSessionTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_QR_TIMEOUT,        terminal_ptr->configuration.QrQrCodeTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_IDLE_SCAN_TIMEOUT, terminal_ptr->configuration.QrIdleScanTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_WALLY_TIMEOUT,     terminal_ptr->configuration.QrWallyTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_REVERSAL_TIMER,    terminal_ptr->configuration.QrReversalTimer));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QR_UPDATE_PAY_TIMEOUT,terminal_ptr->configuration.QrUpdatePayTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_REWARDS_POLL,      terminal_ptr->configuration.QrRewardsPoll));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_RESULTS_POLL,      terminal_ptr->configuration.QrResultsPoll));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_POLLBUSTER,        terminal_ptr->configuration.QrPollbuster));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes        (&tags, TAG_QR_TENDER_ID,         terminal_ptr->configuration.QrTenderId,   vim_strlen(terminal_ptr->configuration.QrTenderId)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes        (&tags, TAG_QR_TENDER_NAME,       terminal_ptr->configuration.QrTenderName, vim_strlen(terminal_ptr->configuration.QrTenderName)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes        (&tags, TAG_QR_DECLINED_RCT_TEXT, terminal_ptr->configuration.QrDeclinedRctText, vim_strlen(terminal_ptr->configuration.QrDeclinedRctText)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes        (&tags, TAG_QR_MERCH_REF_PREFIX,  terminal_ptr->configuration.QrMerchRefPrefix, vim_strlen(terminal_ptr->configuration.QrMerchRefPrefix)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_SEND_QR_TO_POS,    terminal_ptr->configuration.QrSendQrToPos));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_ENABLE_WIDE_RCT,   terminal_ptr->configuration.QrEnableWideReceipt));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8        (&tags, TAG_QR_DISABLE_FROM_IMAGE, terminal_ptr->configuration.QrDisableFromImage));

  // 280520 Trello #123 LFD Switch for VAS
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_NEW_VAS, terminal_ptr->configuration.NewVAS));

  //Switch to enable/disable background Qwikcilver logon attempt at startup
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_QC_LOGON_ON_BOOT, terminal_ptr->configuration.QcLogonOnBoot));
  //Timeout to wait for a response from the Qwikcilver app
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_QC_RESP_TIMEOUT, terminal_ptr->configuration.QcRespTimeout));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_QC_USERNAME, terminal_ptr->configuration.QcUsername, VIM_SIZEOF(terminal_ptr->configuration.QcUsername)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_QC_PASSWORD, terminal_ptr->configuration.QcPassword, VIM_SIZEOF(terminal_ptr->configuration.QcPassword)));


  // RDD 140520 Trello #66 VISA Tap Only terminal
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_CTLS_ONLY_TERM, terminal_ptr->configuration.CTLSOnlyTerminal));

  // RDD 140520 Trello #129 PCI Reboot sometimes happens more than once between 2-3am ( because VN didn't serialise the setting so it changed every power-up
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_PCI_REBOOT_OFFSET, terminal_ptr->configuration.OffsetTime));

  // 130220 DEVX Disable PCI Token and ePAN return for Third Party Terminals
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_PCI_ENABLE, terminal_ptr->configuration.UsePCITokens));

 // 010420 RDD Control whether balance receipt can be enabled or not
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_ALLOW_BAL_RECEIPT, terminal_ptr->configuration.AllowBalanceReceipt));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_HOST_DISCONNECT, terminal_ptr->configuration.allow_disconnect));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_VHQ_ONLY, terminal_ptr->configuration.VHQ_Only ));
  

  // 030420 RDD v582-7 Serialise the COVID19 LFD CVM override amount
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_COVID19_CVM_LIMIT, terminal_ptr->configuration.term_override_cvm_value));

  // RDD 100821 JIRA PIRPIN-1121 Surcharging   
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_SC_ON_TIP, TERM_SC_ON_TIP ));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_SC_REDUCE_ON_CASH, TERM_SC_EXCLUDE_CASH));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_VISA, terminal_ptr->configuration.surcharge_points_visa, vim_strlen(terminal_ptr->configuration.surcharge_points_visa)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_MC, terminal_ptr->configuration.surcharge_points_mc, vim_strlen(terminal_ptr->configuration.surcharge_points_mc)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_AMEX, terminal_ptr->configuration.surcharge_points_amex, vim_strlen(terminal_ptr->configuration.surcharge_points_amex)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_EFTPOS, terminal_ptr->configuration.surcharge_points_eftpos, vim_strlen(terminal_ptr->configuration.surcharge_points_eftpos)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_UPI, terminal_ptr->configuration.surcharge_points_upi, vim_strlen(terminal_ptr->configuration.surcharge_points_upi)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_DINERS, terminal_ptr->configuration.surcharge_points_diners, vim_strlen(terminal_ptr->configuration.surcharge_points_diners)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_SC_POINTS_JCB, terminal_ptr->configuration.surcharge_points_jcb, vim_strlen(terminal_ptr->configuration.surcharge_points_jcb)));

  // 110520 Trello #142 Configure Pre-PCI reboot check
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_PCI_PRE_REBOOT_CHECK, terminal_ptr->configuration.PreRebootCheckSecs));

  // RDD 160418 JIRA WWBP-214
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_ACTIVE_CFG_VER, terminal_ptr->configuration.ActiveCfgVersion, vim_strlen(terminal_ptr->configuration.ActiveCfgVersion)));


  // RDD 250618 JIRA WWBP-26
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_BRANDING, terminal_ptr->configuration.BrandFileName, vim_strlen(terminal_ptr->configuration.BrandFileName)));


  // RDD 101022 JIRA PIRPIN-1995 New Wifi Params
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_WIFI_SSID, terminal_ptr->configuration.WifiSSID, vim_strlen(terminal_ptr->configuration.WifiSSID)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_WIFI_PWD, terminal_ptr->configuration.WifiPWD, vim_strlen(terminal_ptr->configuration.WifiPWD)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_WIFI_PROT, terminal_ptr->configuration.WifiPROT, vim_strlen(terminal_ptr->configuration.WifiPROT)));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_CPAT_FILENAME, terminal_ptr->configuration.CPAT_Filename, vim_strlen(terminal_ptr->configuration.CPAT_Filename)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_EPAT_FILENAME, terminal_ptr->configuration.EPAT_Filename, vim_strlen(terminal_ptr->configuration.EPAT_Filename)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_PKT_FILENAME, terminal_ptr->configuration.PKT_Filename, vim_strlen(terminal_ptr->configuration.PKT_Filename)));

  // RDD - end of WOW

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_CONFIGURED, terminal_ptr->configured));
  //VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERM_BATCH_NUMBER, terminal_ptr->batch_no));
  //VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_EFB_TIMER_PENDING, terminal_ptr->efb_timer_pending));
  //VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_EFB_TIMER, &terminal_ptr->efb_timer, VIM_SIZEOF(terminal_ptr->efb_timer)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_PULSE_DIAL, terminal_ptr->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.pulse_dial));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_BLIND_DIAL, terminal_ptr->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.blind_dial));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_QUICK_DIAL, terminal_ptr->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.fast_connect));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_HEADER_TYPE, terminal_ptr->tms_parameters.net_parameters.header.type));

  // RDD 281214 SPIRA:8292 Allow option for number of Prn cols to be configured via XML
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_CFG_PRN_COLS, terminal_ptr->configuration.prn_columns));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MODE, &terminal_ptr->mode, VIM_SIZEOF(terminal_ptr->mode)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_PABX, terminal_ptr->comms.pabx, VIM_SIZEOF(terminal_ptr->comms.pabx)));
#if 0 // RDD 200412 SPIRA 5012
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_STANDALONE_PARAMETERS, &terminal_ptr->standalone_parameters, VIM_SIZEOF(terminal_ptr->standalone_parameters)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TRANSSEND_ID, terminal_ptr->comms.transsend, VIM_SIZEOF(terminal_ptr->comms.transsend)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_CARD_MISREADS, terminal_ptr->stats.card_misreads));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_CONNECTION_FAILS, terminal_ptr->stats.connection_failures));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_RESPONSE_TIMEOUTS, terminal_ptr->stats.response_timeouts));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_LINE_FAILS, terminal_ptr->stats.line_fails));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_DIAL_ATTEMPTS, terminal_ptr->stats.dial_attempts));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_STATS_CHIP_MISREADS, terminal_ptr->stats.chip_misreads));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_TERMINAL_TMS_NII, terminal_ptr->tms_parameters.nii));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_HOST_PHONE_NO, &terminal_ptr->comms.bank_host_phone, VIM_SIZEOF(terminal_ptr->comms.bank_host_phone)));
#endif

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_LAST_ACTIVITY, &terminal_ptr->last_activity, VIM_SIZEOF(terminal_ptr->last_activity)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TERMINAL_STATUS, terminal_ptr->status));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_TERMINAL_ID, terminal_ptr->terminal_id, sizeof(terminal_ptr->terminal_id)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MERCHANT_ID, terminal_ptr->merchant_id, sizeof(terminal_ptr->merchant_id)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_LAST_SETTLE_DATE, &terminal_ptr->last_settle_date, sizeof(terminal_ptr->last_settle_date)));

#if 0 // RDD 200412 SPIRA 5012
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint32_little(&tags, TAG_TMS_LINE_SPEED, terminal_ptr->comms.tms_line_speed));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_SHA, terminal_ptr->comms.sha, VIM_SIZEOF(terminal_ptr->comms.sha)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_AUTO_LOGON, terminal_ptr->tms_auto_logon));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags,TAG_SETTLEMENT_AUTO_RECEIPT, terminal_ptr->auto_settlement_receipt_prt));
#endif
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_HOST_ID, &terminal_ptr->host_id, VIM_SIZEOF(terminal_ptr->host_id)));
#if 0 // RDD 200412 SPIRA 5012
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags,TAG_TMS_SOFTWARE_CONFIGURED, terminal_ptr->tms_software_updated));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags,TAG_TERM_SAF_REPEAT_PENDING, terminal_ptr->saf_repeat_pending));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_HOST_PHONE_NO, terminal_ptr->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number, sizeof(terminal_ptr->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint16_little(&tags, TAG_TMS_HOST_NII, terminal_ptr->comms.bank_host_nii));
  /* LFD auto download settings */
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_AUTO_DOWNLOAD_TYPE, terminal_ptr->tms_auto_download_settings.download_type));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags,TAG_TMS_AUTO_DOWNLOAD_TIME, terminal_ptr->tms_auto_download_settings.download_time, VIM_SIZEOF(terminal_ptr->tms_auto_download_settings.download_time)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags,TAG_TMS_AUTO_DOWNLOAD_VERSION, terminal_ptr->tms_auto_download_settings.version, VIM_SIZEOF(terminal_ptr->tms_auto_download_settings.version)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TMS_AUTO_DOWNLOAD_STATUS, terminal_ptr->tms_auto_download_settings.status));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MERCHANT_PASSWORD, &terminal_ptr->configuration.password, VIM_SIZEOF(terminal_ptr->configuration.password)));
#endif
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags, TAG_TERM_REFUND_PASSWORD, &terminal_ptr->configuration.refund_password, VIM_SIZEOF(terminal_ptr->configuration.refund_password )));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MOTO_PASSWORD, &terminal_ptr->configuration.moto_password, VIM_SIZEOF(terminal_ptr->configuration.moto_password)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint64_little(&tags, TAG_TERM_CFG_CASHOUT_MAX, terminal_ptr->configuration.cashout_max));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags, TAG_TERM_CFG_PREAUTH_FLAG, &terminal_ptr->configuration.preauth_enable, VIM_SIZEOF(terminal_ptr->configuration.preauth_enable )));

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_ENABLE_CHECKOUT, terminal_ptr->configuration.checkout_enable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_ENABLE_REFUND, terminal_ptr->configuration.refund_enable));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_ENABLE_VOID, terminal_ptr->configuration.void_enable));


  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_CFG_ALLOW_MOTO, &terminal_ptr->configuration.moto_ecom_enable, VIM_SIZEOF(terminal_ptr->configuration.moto_ecom_enable)));
  

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_add_bytes( &tags, TAG_TERM_CFG_CCV_ENTRY, &terminal_ptr->configuration.ccv_entry, VIM_SIZEOF(terminal_ptr->configuration.ccv_entry )));

  // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_MC_EMV_REFUND, &terminal_ptr->configuration.MCEMVRefundEnabled, VIM_SIZEOF(terminal_ptr->configuration.MCEMVRefundEnabled)));

  // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_VISA_EMV_REFUND, &terminal_ptr->configuration.VisaEMVRefundEnabled, VIM_SIZEOF(terminal_ptr->configuration.VisaEMVRefundEnabled)));

  // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_EMV_REFUND_CPAT_OVERRIDE, &terminal_ptr->configuration.EMVRefundCPATOverride, VIM_SIZEOF(terminal_ptr->configuration.EMVRefundCPATOverride)));

  // 280920 Trello #281 - MB wants to be able to disable extra tags if needs be
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TMS_FORBID_OPTIONAL_XML_TAGS, &terminal_ptr->configuration.ForbidExtraXMLTags, VIM_SIZEOF(terminal_ptr->configuration.ForbidExtraXMLTags)));

  // 260721 JIRA-PIRPIN 1118 VDA
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_VDA_DISABLE, &terminal_ptr->configuration.VDA, VIM_SIZEOF(terminal_ptr->configuration.VDA )));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_MDA_DISABLE, &terminal_ptr->configuration.MDA, VIM_SIZEOF(terminal_ptr->configuration.MDA)));

  // RDD 120523 JIRA PIRPIN_2383 SLYP Authcode
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_USE_DE38, terminal_ptr->configuration.de38_to_pos));

  // RDD 140923 JIRA PIRPIN_2712 Countdown -> Woolworths
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_NZ_WOW, terminal_ptr->configuration.nz_wow));



#if 0
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_VOID_PASSWORD, &terminal_ptr->configuration.void_password, VIM_SIZEOF(terminal_ptr->configuration.void_password)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, TAG_TERM_SETTLEMENT_PASSWORD, &terminal_ptr->configuration.settlement_password, VIM_SIZEOF(terminal_ptr->configuration.settlement_password)));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_TMS_IS_FINISHED, terminal_ptr->tms_is_finished));
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_uint8(&tags, TAG_TERM_TMS_APP_LAST_SCHEDULED_TMS_DOWNLOAD_ATTEMPT_MONTH, terminal_ptr->tms_last_scheduled_tms_download_month));
#endif
  *destination_size_ptr = tags.total_length;
  return VIM_ERROR_NONE;
}


/*
  typedef struct emv_default_parameters_t
{
  VIM_UINT8 amount_authorised_binary[4];			// RDD 311011 - added for WOW for purpose as yet unknown !
  VIM_UINT8 amount_other_binary[4];					// RDD 311011 - added for WOW for purpose as yet unknown !
  VIM_UINT8 country_code[2];
  VIM_UINT8 terminal_capabilities[3];
  VIM_UINT8 terminal_type;
  VIM_CHAR txn_reference_currency_code[2];
  VIM_UINT8 txn_reference_currency_exponent;
  VIM_UINT8 additional_terminal_capabilities[5];
  VIM_UINT8 merchant_category_code_bcd[2];			// RDD 311011 - added for WOW
}emv_default_parameters_t;
*/

VIM_ERROR_PTR deserialise_emv_default_configuration( VIM_TLV_LIST *root_tlv_list )
{
  VIM_TLV_LIST temp_tlv_list;
  VIM_TLV_LIST sub_tlv_list;

  VIM_ERROR_RETURN_ON_NULL( root_tlv_list );
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_rewind( root_tlv_list ));

  VIM_DBG_WARNING("Load Default EMV Config");

  // RDD P400 210719 - pick up the file version: this should be the first tag
  if (vim_tlv_search( root_tlv_list, WOW_EMV_AID_PARAMS_FILE_VERSION ) == VIM_ERROR_NONE)
  {
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes(root_tlv_list, &file_status[EPAT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN ));
      terminal.file_version[EPAT_FILE_IDX] = bcd_to_bin((VIM_UINT8 *)&file_status[EPAT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN * 2);
  }


  /* open the sub container directly */
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_find(root_tlv_list,WOW_EMV_DEFAULT_PARAM_CONTAINER));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current(root_tlv_list,&temp_tlv_list,VIM_FALSE));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_find(&temp_tlv_list, WOW_EMV_DEFAULT_PARAM_SUB_CONTAINER));
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current(&temp_tlv_list,&sub_tlv_list,VIM_FALSE));

  // RDD - get amount authorised binary
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_AMOUNT_AUTHORISED_BINARY) == VIM_ERROR_NONE )
  {
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.amount_authorised_binary,VIM_SIZEOF(terminal.emv_default_configuration.amount_authorised_binary)));
  }


  // RDD - get amount other binary
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_AMOUNT_OTHER_BINARY) == VIM_ERROR_NONE )
  {
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.amount_other_binary,VIM_SIZEOF(terminal.emv_default_configuration.amount_other_binary)));
  }

  // RDD - get merchant category code
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_MERCHANT_CATEGORY_CODE) == VIM_ERROR_NONE )
  {
	  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.merchant_category_code_bcd,VIM_SIZEOF(terminal.emv_default_configuration.merchant_category_code_bcd)));
  }

  // RDD - get country code
  DBG_MESSAGE( "!!!!!!!! CHECKING COUNTRY !!!!!!!!!" );

  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_COUNTRY_CODE) == VIM_ERROR_NONE )
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.country_code,VIM_SIZEOF(terminal.emv_default_configuration.country_code)));
    DBG_DATA( terminal.emv_default_configuration.country_code, 2 );
  }

  //////////////////// ************************************************************************************* ////////////////////////
  ////////////////////////////////////////////////// CONTACT DEFAULT TERMINAL CAPABILITIES ///////////////////////////////////////////////
  //////////////////// ************************************************************************************* ////////////////////////

  // RDD - get terminal capabilities
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_TERMINAL_CAPABILITIES) == VIM_ERROR_NONE )
  {
#ifdef _nDEBUG
	vim_mem_copy( terminal.emv_default_configuration.terminal_capabilities, DEBUG_TERM_CAP, VIM_SIZEOF(DEBUG_TERM_CAP) );
#else
   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.terminal_capabilities,VIM_SIZEOF(terminal.emv_default_configuration.terminal_capabilities)));
// RDD 060213 - Kill off encrypted and plaintext offline pin capability until kernal bug is fixed
   //terminal.emv_default_configuration.terminal_capabilities[1] &= 0x6F; // Clear the bits for enc and plain offline pin cap
#endif
   DBG_DATA( terminal.emv_default_configuration.terminal_capabilities, 3 );
  }

  // RDD - get terminal type
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_TERMINAL_TYPE) == VIM_ERROR_NONE )
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&sub_tlv_list,&terminal.emv_default_configuration.terminal_type));
  }

  // RDD - get currency code
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_TXN_CURRENCY_CODE) == VIM_ERROR_NONE )
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.txn_reference_currency_code,VIM_SIZEOF(terminal.emv_default_configuration.txn_reference_currency_code)));
    DBG_DATA( terminal.emv_default_configuration.txn_reference_currency_code, 2 );
  }

  // RDD - get currency exponent
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_CURRENCY_EXPONENT) == VIM_ERROR_NONE )
  {
    if( sub_tlv_list.current_item.length > 0 )
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&sub_tlv_list,&terminal.emv_default_configuration.txn_reference_currency_exponent));
      DBG_VAR( terminal.emv_default_configuration.txn_reference_currency_exponent );
    }
    else
    {
      terminal.emv_default_configuration.txn_reference_currency_exponent = 2;
    }
  }
  //////////////////// ************************************************************************************* ////////////////////////
  ////////////////////////////////////////////////// ADDITIONAL TERMINAL CAPABILITIES ///////////////////////////////////////////////
  //////////////////// ************************************************************************************* ////////////////////////
  // RDD - get additional terminal capabilities
  if( vim_tlv_find(&sub_tlv_list,WOW_EMV_DEFAULT_PARAM_ADDITIONAL_CAPABILITIES) == VIM_ERROR_NONE )
  {
	  DBG_MESSAGE( "TTTTTTTTTTTTTTTTTTTTTTTTTTT ADDITIONAL TERMINAL CAPABILITIES TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT");
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list,terminal.emv_default_configuration.additional_terminal_capabilities,VIM_SIZEOF(terminal.emv_default_configuration.additional_terminal_capabilities)));
	DBG_DATA( terminal.emv_default_configuration.additional_terminal_capabilities, 5 );

	//vim_mem_copy( terminal.emv_default_configuration.additional_terminal_capabilities, "\xFF\x80\xF0\xA0\x01", 5 );
//	vim_mem_copy( terminal.emv_default_configuration.additional_terminal_capabilities, DEBUG_ADD_CAP, VIM_SIZEOF(DEBUG_ADD_CAP) );
//	DBG_DATA( terminal.emv_default_configuration.additional_terminal_capabilities, 5 );
  }
  return VIM_ERROR_NONE;
}



// RDD 240619 - rewritten to handle contact and contactless
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR deserialise_aid_configuration( VIM_TLV_LIST *tlv_list_root_ptr, VIM_SIZE* cnt, VIM_BOOL Ctls )
{

  VIM_TLV_LIST *tlv_list_ptr = tlv_list_root_ptr;
   // Back to start
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_rewind( tlv_list_ptr ));

  VIM_DBG_WARNING("Load EMV AID Config");


  if (Ctls)
  {
      VIM_TLV_LIST cless_tlv_list;
      if (vim_tlv_search( tlv_list_ptr, WOW_EMV_APP_CONTACTLESS_CONTAINER ) != VIM_ERROR_NONE)
      {
          DBG_MESSAGE("$$$$$$$$$$$ AID CONTACTLESS NOT ENABLED: $$$$$$$$$$$$$$");
          return VIM_ERROR_NOT_FOUND;
      }
      else
      {
          VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current( tlv_list_ptr, &cless_tlv_list, VIM_FALSE));
          tlv_list_ptr = &cless_tlv_list;
      }

  }
  while( vim_tlv_find_next( tlv_list_ptr, WOW_EMV_APP_CONTAINER) == VIM_ERROR_NONE )
  {
      VIM_TLV_LIST aid_tlv_list;

      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current( tlv_list_ptr, &aid_tlv_list, VIM_FALSE ));

      //if( Ctls )
      //  VIM_DBG_TLV( aid_tlv_list );

      if( vim_tlv_search( &aid_tlv_list, WOW_EMV_APP_AID ) == VIM_ERROR_NONE)
      {
          VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&aid_tlv_list, terminal.emv_applicationIDs[*cnt].emv_aid, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].emv_aid)));
          terminal.emv_applicationIDs[*cnt].aid_length = aid_tlv_list.current_item.length;

          VIM_DBG_PRINTF1("AID FOUND: CTLS = %d", Ctls);
          DBG_DATA(terminal.emv_applicationIDs[*cnt].emv_aid, terminal.emv_applicationIDs[*cnt].aid_length);

          // RDD: 311011 - get the AID display details
          if (vim_tlv_search(&aid_tlv_list, WOW_EMV_APP_AID_DISPLAY_TEXT) == VIM_ERROR_NONE)
          {
              VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&aid_tlv_list, terminal.emv_applicationIDs[*cnt].aid_display_text, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].aid_display_text)));
              VIM_DBG_PRINTF2("%s ( cnt=%d )", terminal.emv_applicationIDs[*cnt].aid_display_text, *cnt);
          }

          /* sub container 1 */
          if (vim_tlv_search(&aid_tlv_list, WOW_EMV_APP_SUB_1_CONTAINER) == VIM_ERROR_NONE)
          {
              VIM_TLV_LIST temp_tlv_list;
              VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open_current(&aid_tlv_list, &temp_tlv_list, VIM_FALSE));

              if (vim_tlv_search(&temp_tlv_list, WOW_EMV_APP_SUB_1_CARD_ORIGIN) == VIM_ERROR_NONE)
                  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little( &temp_tlv_list, &terminal.emv_applicationIDs[*cnt].international_domestic_overide));

              if (vim_tlv_search(&temp_tlv_list, WOW_EMV_APP_SUB_1_ORIGIN_TAGS) == VIM_ERROR_NONE)
              {
                  VIM_TLV_LIST sub_tlv_list;
                  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open_current( &temp_tlv_list, &sub_tlv_list, VIM_FALSE));

                  /* todo: pase sub container of sub container 1 */
                  // RDD 311011 added aid_txn_type for WOW

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TRANSACTION_TYPE) == VIM_ERROR_NONE)
                  {
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].aid_txn_type, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].aid_txn_type)));
                  }
                  // RDD 311011 added aid_preset_acc for WOW
                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_PRESET_ACCOUNT) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].aid_preset_acc, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].aid_preset_acc)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TAC_DEFAULT) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].tac_default, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].tac_default)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TAC_DENIAL) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].tac_denial, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].tac_denial)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TAC_ONLINE) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].tac_online, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].tac_online)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TAC_STANDIN) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].tac_standin, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].tac_standin)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_THRESHOLD) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint16_little(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].threshold));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TARGET_PERCENT) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint16_little(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].tagert_percent));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_MAX_TARGET) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint16_little(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].max_target));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_DDOL_DEFAULT) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].ddol_default, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].ddol_default)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_TDOL_DEFAULT) == VIM_ERROR_NONE)
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].tdol_default, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].tdol_default)));

                  /* if it's empty, set some default value */
                  if (terminal.emv_applicationIDs[*cnt].tdol_default[0] == 0x00)
                      vim_mem_copy(terminal.emv_applicationIDs[*cnt].tdol_default, "\x9F\x08\x02", 3);

                  /* JQ WOW only has one application version number */
                  if (vim_tlv_find(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_APP_VERSION) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].app_version1, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].app_version1)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_FLOOR_LIMIT) == VIM_ERROR_NONE)
                  {
                      VIM_UINT32 tmp_floor_limit = 0;
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint32_big(&sub_tlv_list, &tmp_floor_limit));

                      terminal.emv_applicationIDs[*cnt].floor_limit = tmp_floor_limit * 100;
                  }

                  // RDD 311011 added mc_txn_category_code for WOW
                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_MC_TXN_CATAGORY_CODE) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].mc_txn_category_code, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].mc_txn_category_code)));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_EFB_FLOOR_LIMIT) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint32_big(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].efb_floor_limit));

                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_CONTACTLESS_TTQ) == VIM_ERROR_NONE)
                  {
                      VIM_ERROR_WARN_ON_FAIL( vim_tlv_get_bytes( &sub_tlv_list, &terminal.emv_applicationIDs[*cnt].visa_ttq, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].visa_ttq)));
                  }
                  else
                  {
                      vim_mem_clear(&terminal.emv_applicationIDs[*cnt].visa_ttq, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].visa_ttq));
                  }

                  // RDD 311011 added small value enabled for WOW
                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_SMALL_VALUE_ENABLED) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].small_value_enabled, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].small_value_enabled)));

                  // RDD 310315 SPIRA:8544 Small Value TCC
                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_SMALL_TERMINAL_CAPABILITIES) == VIM_ERROR_NONE)
                  {
                      VIM_DBG_WARNING( "SMALL VALUE TCC" );
                      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].qps_term_cap, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].qps_term_cap)));
                      VIM_DBG_DATA(terminal.emv_applicationIDs[*cnt].qps_term_cap, VIM_SIZEOF(terminal.emv_applicationIDs[*cnt].qps_term_cap));
                  }


                  if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_PIN_BYPASS) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint8(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].pin_bypass));

                  if (vim_tlv_search(&sub_tlv_list, WOW_TCC_APP_SUB_1_SUB_CONTACTLESS_ENABLED) == VIM_ERROR_NONE)
                      VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint8(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].contactless));

                  if( terminal.emv_applicationIDs[*cnt].contactless )
                  {
                      if( vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_CVM_LIMIT) == VIM_ERROR_NONE)
                      {
                          VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint32_big(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].cvm_limit));
                      }
                      if( vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_CVM_INT_LIMIT) == VIM_ERROR_NONE)
                      {
                          VIM_ERROR_WARN_ON_FAIL( vim_tlv_get_uint32_big(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt].cvm_international_limit));
                      }
                      else
                          terminal.emv_applicationIDs[*cnt].cvm_international_limit = terminal.emv_applicationIDs[*cnt].cvm_limit;

                      /*  contactless TXn limit */
                      if (vim_tlv_search(&sub_tlv_list, "\x1F\x66") == VIM_ERROR_NONE)
                      {
                          VIM_UINT32 tmp_TXN_limit = CONTACTLESS_READER_LIMIT;
                          VIM_ERROR_WARN_ON_FAIL(vim_tlv_get_uint32_big(&sub_tlv_list, &tmp_TXN_limit));
                          VIM_DBG_NUMBER(tmp_TXN_limit);
                          terminal.emv_applicationIDs[*cnt].contactless_txn_limit = tmp_TXN_limit;
                      }
                      else
                      {
                          terminal.emv_applicationIDs[*cnt].contactless_txn_limit = CONTACTLESS_READER_LIMIT;
                      }
                      // RDD 110914 - Woolworths may add this Paypass3 parameter to the EPAT at a later date, so use their value or the default if not present
                      if (vim_tlv_search(&sub_tlv_list, WOW_EMV_APP_SUB_1_SUB_CONTACTLESS_RMP) == VIM_ERROR_NONE)
                      {
                          VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&sub_tlv_list, terminal.emv_applicationIDs[*cnt].RMP_9F1D, 4));
                      }
                      else
                      {
                          vim_mem_copy(terminal.emv_applicationIDs[*cnt].RMP_9F1D, DEFAULT_RMP_9F1D, 4);
                      }
                      // RDD 101019 JIRA WWBP - 662 - setup default values for DRL ( note not used by all CTLS schemes and only currently tested by AMEX XP3.1
                      {
                          VIM_SIZE n;
                          for (n = 0; n < EMV_ADK_DRL_ENTRIES; n++)
                          {
                              terminal.emv_applicationIDs[*cnt].drl_tables[n].ContactlessCVMRequiredLimit_DFAB42 = terminal.emv_applicationIDs[*cnt].cvm_limit;
                              terminal.emv_applicationIDs[*cnt].drl_tables[n].ContactlessFloorLimit_DFAB40 = terminal.emv_applicationIDs[*cnt].floor_limit;
                              terminal.emv_applicationIDs[*cnt].drl_tables[n].ContactlessTransactionLimit_DFAB41 = terminal.emv_applicationIDs[*cnt].contactless_txn_limit;
                              terminal.emv_applicationIDs[*cnt].drl_tables[n].OnOffSwitch_DFAB49 = 0;
                          }
                      }
                  }
                  ProcessTermCaps(&sub_tlv_list, &terminal.emv_applicationIDs[*cnt]);

                  //DBG_DATA( terminal.emv_applicationIDs[*cnt].TCC_GT_CVM_limit, 3 );
              }
          }
      }
      // RDD - copy over into INT and DOM structures
      //terminal.emv_int_applicationIDs[*cnt] = terminal.emv_applicationIDs[*cnt];
      //terminal.emv_dom_applicationIDs[*cnt] = terminal.emv_applicationIDs[*cnt];

      if( (*cnt)++ > MAX_EMV_APPLICATION_ID_SIZE )
          VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
  }
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR deserialise_emv_aid_configuration( VIM_TLV_LIST *tlv_list_ptr )
{
  VIM_SIZE cnt = 0;

  // RDD 231012 SPIRA:6225 EPAL not working until reboot - this is because the tag isn't present and the struct was not being cleared
  // before deserialisation so missing tags from new AIDs ( eg. EPAL aids ) would retain the values of the previous AID of that index (ie. ctls)
  vim_mem_clear( terminal.emv_applicationIDs, VIM_SIZEOF( terminal.emv_applicationIDs ));

   //VIM_DBG_MESSAGE( "Loading EMV AID parameters............" );
  /* parse contact EMV AID parameters */
  deserialise_aid_configuration(tlv_list_ptr, &cnt, VIM_FALSE);

   //VIM_DBG_MESSAGE( "Loading Contactless EMV AID parameters............" );
   /* parse contactless EMV AID parameters */

  deserialise_aid_configuration(tlv_list_ptr, &cnt, VIM_TRUE);

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

/* RDD- WOW: TAGS for EMV public keys */
#define WOW_EMV_PKT_FILE_VERSION		"\xDF\xE9\x59"

#define WOW_EMV_CAKEYS_CONTAINER         "\xFF\xE9\x11"
#define WOW_EMV_CAKEYS_START_DATE		 "\xDF\xE9\x5B"			/* WBC: - not used WOW DDMMYYYY (binary format) */
#define WOW_EMV_CAKEYS_EXPIRY_DATE       "\xDF\xE9\x5C"			/* WBC: YYYYMMDD (ascii): WOW DDMMYYYY (binary format) */
#define WOW_EMV_CAKEYS_RID               "\xDF\xE9\x18"
#define WOW_EMV_CAKEYS_INDEX             "\xDF\xE9\x19"
#define WOW_EMV_CAKEYS_KEY_BLOCK         "\xDF\xE9\x1A"
#define WOW_EMV_CAKEYS_EXPONENT          "\xDF\xE9\x1B"
#define WOW_EMV_CAKEYS_EXPONENT_LENGTH   "\xDF\xE9\x1C"
//#define WOW_EMV_CAKEYS_HASH              "\xDF\xE9\x22"		not used for WOW
#define WOW_EMV_CAKEYS_HASH_ALGORITHM    "\xDF\xE9\x1C"


VIM_ERROR_PTR capk_deserialise( VIM_TLV_LIST *tlv_list_ptr )
{
  VIM_SIZE cnt=0;

  // DBG_MESSAGE("!!! LOAD PKT FILE !!!" );

  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_rewind(tlv_list_ptr));

  //BD This was causing the problem when moving from dev to cert, do other files have similar issue????
  //BD Ensure structure is cleared
  vim_mem_clear(terminal.emv_capk,VIM_SIZEOF( terminal.emv_capk));

  // RDD - pick up the file version: this should be the first tag
  if( vim_tlv_search( tlv_list_ptr, WOW_EMV_PKT_FILE_VERSION ) == VIM_ERROR_NONE )
  {
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( tlv_list_ptr, &file_status[PKT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN ) );
	  terminal.file_version[PKT_FILE_IDX] = bcd_to_bin ( (VIM_UINT8 *)&file_status[PKT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN*2 );
  }

  while( vim_tlv_find_next(tlv_list_ptr, WOW_EMV_CAKEYS_CONTAINER) == VIM_ERROR_NONE)
  {
    VIM_TLV_LIST detail_tlv_list;

    //DBG_MESSAGE("!!! FOUND FIRST KEY CONTAINER !!!" );
    VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current(tlv_list_ptr, &detail_tlv_list, VIM_FALSE));

    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_EXPIRY_DATE) == VIM_ERROR_NONE )
    {
      /* save expiry date */
      terminal.emv_capk[cnt].capk.expiry_date.day = detail_tlv_list.current_item.value[0];
      terminal.emv_capk[cnt].capk.expiry_date.month = detail_tlv_list.current_item.value[1];
      terminal.emv_capk[cnt].capk.expiry_date.year = detail_tlv_list.current_item.value[2] + (0x100 * detail_tlv_list.current_item.value[3]);
    }

    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_RID) == VIM_ERROR_NONE )
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&detail_tlv_list,terminal.emv_capk[cnt].capk.RID.value, VIM_SIZEOF(terminal.emv_capk[cnt].capk.RID.value)));
    }
	VIM_DBG_VAR(terminal.emv_capk[cnt].capk.RID.value);
    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_INDEX) == VIM_ERROR_NONE )
    {
      VIM_UINT8 temp_index;
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&detail_tlv_list, &temp_index));
      terminal.emv_capk[cnt].capk.index = temp_index;
    }
	DBG_VAR( terminal.emv_capk[cnt].capk.index );

    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_KEY_BLOCK) == VIM_ERROR_NONE )
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&detail_tlv_list, terminal.emv_capk[cnt].capk.modulus, VIM_SIZEOF(terminal.emv_capk[cnt].capk.modulus)));
       terminal.emv_capk[cnt].capk.modulus_size = detail_tlv_list.current_item.length;
    }

    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_EXPONENT) == VIM_ERROR_NONE )
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&detail_tlv_list, terminal.emv_capk[cnt].capk.exponent, VIM_SIZEOF(terminal.emv_capk[cnt].capk.exponent)));
      terminal.emv_capk[cnt].capk.exponent_size = detail_tlv_list.current_item.length;
    }
#if 0
    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_EXPONENT_LENGTH) == VIM_ERROR_NONE )
    {
      VIM_UINT8 temp_length;
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint8(&detail_tlv_list, &temp_length));
      terminal.emv_capk[cnt].capk.exponent_size= temp_length;
    }
    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_HASH) == VIM_ERROR_NONE )
    {
      if( detail_tlv_list.current_item.length > 0 )
        VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&detail_tlv_list, terminal.emv_capk[cnt].capk.hash, VIM_SIZEOF(terminal.emv_capk[cnt].capk.hash)));
    }
#endif

    if( vim_tlv_search(&detail_tlv_list,WOW_EMV_CAKEYS_HASH_ALGORITHM) == VIM_ERROR_NONE )
      VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_uint32_little(&detail_tlv_list, &terminal.emv_capk[cnt].algorithm));

	VIM_DBG_VAR(cnt);

	// RDD 071212 - We ran out of SPACE. Too many keys !
    if( ++cnt >= MAX_EMV_PUBLIC_KEYS )
      VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
  }

  // RDD - TODO need to process CTLS config file HERE !!

  return VIM_ERROR_NONE;
}



/**
 * Convert serialized terminal data back to terminal structure
 *
 * \param[out] terminal
 * terminal structure to populate
 *
 * \param[in] buffer
 * pointer to buffer containing serialized data
 *
 * \param[in] length
 * Length of data in buffer
 */
VIM_ERROR_PTR terminal_deserialize(terminal_t *term, VIM_UINT8 *buffer, VIM_UINT32 length)
{
  VIM_TLV_LIST tags;
#if 0
  VIM_TLV_LIST merch_tags;
#endif
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&tags, buffer, length, length, VIM_FALSE));

  if (vim_tlv_search(&tags, TAG_TERM_STAN) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->stan);
  if (vim_tlv_search(&tags, TAG_TERM_ROC) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->roc);

  if (vim_tlv_search(&tags, TAG_TERM_SFW_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->sfw_version);

  if (vim_tlv_search(&tags, TAG_TERM_SFW_PREV_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->sfw_prev_version);

  if (vim_tlv_search(&tags, TAG_TERM_SFW_MINOR) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->sfw_minor);

  if (vim_tlv_search(&tags, TAG_TERM_SFW_PREV_MINOR) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->sfw_prev_minor);


  // RDD 191214 Added for ALH XML file maintenance
  if (vim_tlv_search(&tags, TAG_TERM_CFG_FILE_VER) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->TerminalConfigFileVer, VIM_SIZEOF(term->TerminalConfigFileVer));

  // RDD 170914 added v433.2
  if (vim_tlv_search(&tags, TAG_TERM_SFW_MINOR) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->sfw_minor);



    // RDD - added for WOW

  // RDD 081012 P3 Added Contactless File name
  if (vim_tlv_search(&tags, TAG_TERM_CTLS_DRV_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->contactless_file_name, VIM_SIZEOF(term->contactless_file_name));

  if (vim_tlv_search(&tags, TAG_TERM_CPAT_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->file_version[CPAT_FILE_IDX]);
  if (vim_tlv_search(&tags, TAG_TERM_PKT_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->file_version[PKT_FILE_IDX]);
  if (vim_tlv_search(&tags, TAG_TERM_EPAT_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->file_version[EPAT_FILE_IDX]);
  if (vim_tlv_search(&tags, TAG_TERM_FCAT_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->file_version[FCAT_FILE_IDX]);
  if (vim_tlv_search(&tags, TAG_TERM_BUFFER_COUNT) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->buffer_count);
  if (vim_tlv_search(&tags, TAG_TERM_KEK1_KVC) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->kek1kvc, VIM_SIZEOF(term->kek1kvc));
  if (vim_tlv_search(&tags, TAG_TERM_CPAT_ENTRIES) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->number_of_cpat_entries);

 if (vim_tlv_search(&tags, TAG_TERM_LOGON_STATE) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, (VIM_UINT8*)&term->logon_state);
 // DBG_VAR(term->logon_state);

  if (vim_tlv_search(&tags, TAG_TERM_ECG_BIN) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->ecg_bin, VIM_SIZEOF(term->ecg_bin));
  //if (vim_tlv_search(&tags, TAG_TERM_SCREENSAVER2_FILENAME) == VIM_ERROR_NONE)
    //vim_tlv_get_bytes(&tags, &term->screensaver_2_file_name, VIM_SIZEOF(term->screensaver_2_file_name));

  if (vim_tlv_search(&tags, TAG_TERM_MERCHANT_NAME) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.merchant_name, VIM_SIZEOF(term->configuration.merchant_name));
  if (vim_tlv_search(&tags, TAG_TERM_MERCHANT_ADDRESS) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.merchant_address, VIM_SIZEOF(term->configuration.merchant_address));

  // RDD 010814 added for ALH
  if (vim_tlv_search(&tags, TAG_TERM_TIP_ENABLED) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->configuration.sale_with_tip);


  // RDD 010921 v590-6 JIRA-PIRPIN 1214 - set default true ie Woolies terminals do no need this to be set to zero.
  if (vim_tlv_search(&tags, TAG_TERM_NO_EFT_GIFT) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.no_eft_gift);

  // RDD 170223 v736 JIRA-PIRPIN 1820 - New Flow for Returns Cards
  if (vim_tlv_search(&tags, TAG_RETURNS_CARD_NEW_FLOW) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.ReturnsCardNewFlow);


  if (vim_tlv_search(&tags, TAG_TERM_ENCRYPT_DE47) == VIM_ERROR_NONE)
	  vim_tlv_get_uint8(&tags, &term->configuration.encrypt_de47);

  if (vim_tlv_search(&tags, TAG_TERM_SNI_ODD) == VIM_ERROR_NONE)
      vim_tlv_get_bytes(&tags, &term->configuration.SNIOdd, VIM_SIZEOF(term->configuration.SNIOdd));
  if (vim_tlv_search(&tags, TAG_TERM_SNI_EVEN) == VIM_ERROR_NONE)
      vim_tlv_get_bytes(&tags, &term->configuration.SNIEven, VIM_SIZEOF(term->configuration.SNIEven));
  if (vim_tlv_search(&tags, TAG_TERM_HOST_URL) == VIM_ERROR_NONE)
      vim_tlv_get_bytes(&tags, &term->configuration.HostURL, VIM_SIZEOF(term->configuration.HostURL));


  // RDD 310122 v730 JIRA-PIRPIN 1407 WIFI Control via TMS
  if (vim_tlv_search(&tags, TAG_TERM_USE_WIFI) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.use_wifi );

  if (vim_tlv_search(&tags, TAG_TERM_USE_RNDIS) == VIM_ERROR_NONE)
	  vim_tlv_get_uint8(&tags, &term->configuration.use_rndis);


  // RDD 290322 JIRA PIRPIN-1521 
  if (vim_tlv_search(&tags, TAG_TERM_DISP_RES_SECS) == VIM_ERROR_NONE)
	  vim_tlv_get_uint8(&tags, &term->configuration.disp_res_secs);

  if (vim_tlv_search(&tags, TAG_TXN_TMS_BLOCK_SIZE) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->tms_parameters.max_block_size );
  if (vim_tlv_search(&tags, TAG_TMS_WINDOW_SIZE) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->tms_parameters.window_size );

  // 160821 JIRA PIRPIN-1191
  if (vim_tlv_search(&tags, TAG_TERM_DINERS_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.diners_disable);
  if (vim_tlv_search(&tags, TAG_TERM_AMEX_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.amex_disable);
  if (vim_tlv_search(&tags, TAG_TERM_UPI_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.upi_disable);
  if (vim_tlv_search(&tags, TAG_TERM_JCB_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.jcb_disable);
  if (vim_tlv_search(&tags, TAG_TERM_QC_GIFT_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.qc_gift_disable);
  if (vim_tlv_search(&tags, TAG_TERM_WEX_GIFT_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.wex_gift_disable);
  if (vim_tlv_search(&tags, TAG_TERM_QPS_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.qps_disable);

// RDD 260821 MCR Stuff
      if (vim_tlv_search(&tags, TAG_TERM_ENABLE_MCR) == VIM_ERROR_NONE)
          vim_tlv_get_uint8(&tags, (VIM_UINT8*)&TERM_USE_MERCHANT_ROUTING);

      if (vim_tlv_search(&tags, TAG_TERM_MCR_VISA_LOW_LIMIT) == VIM_ERROR_NONE) 
          vim_tlv_get_uint32_little(&tags, &TERM_MCR_VISA_LOW);
      if (vim_tlv_search(&tags, TAG_TERM_MCR_VISA_HIGH_LIMIT) == VIM_ERROR_NONE) 
           vim_tlv_get_uint32_little(&tags, &TERM_MCR_VISA_HIGH);
      if (vim_tlv_search(&tags, TAG_TERM_MCR_MC_LOW_LIMIT) == VIM_ERROR_NONE) 
           vim_tlv_get_uint32_little(&tags, &TERM_MCR_MC_LOW);
      if (vim_tlv_search(&tags, TAG_TERM_MCR_MC_HIGH_LIMIT) == VIM_ERROR_NONE) 
           vim_tlv_get_uint32_little(&tags, &TERM_MCR_MC_HIGH);

  // RDD 030517 added for Phase7 J8 Pass stuff
  if (vim_tlv_search(&tags, TAG_TMS_FASTEMV_AMT) == VIM_ERROR_NONE)
    vim_tlv_get_uint64_little(&tags, (VIM_UINT64*)&term->configuration.FastEmvAmount );
   if (vim_tlv_search(&tags, TAG_TMS_J8_ENABLE) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, (VIM_UINT8*)&term->configuration.j8_enable );

   // RDD 080922 JIRA PIRPIN-1433 Allow merchant to control technical fallback via TMS
   if (vim_tlv_search(&tags, TAG_TMS_ENABLE_TECHNICAL_FALLBACK) == VIM_ERROR_NONE)
       vim_tlv_get_uint8(&tags, (VIM_UINT8*)&term->configuration.technical_fallback_enable);
  
   if (vim_tlv_search( &tags, TAG_TMS_TIP_MAX_PERCENT ) == VIM_ERROR_NONE )
  	   vim_tlv_get_uint8(&tags, (VIM_UINT8*)&term->configuration.max_tip_percent);

   if (vim_tlv_search( &tags, TAG_TERM_REFUND_TXN_LIMIT ) == VIM_ERROR_NONE) 
       vim_tlv_get_uint64_little( &tags, (VIM_UINT64*)&term->configuration.refund_txn_limit );

   if (vim_tlv_search( &tags, TAG_TERM_REFUND_DAILY_LIMIT ) == VIM_ERROR_NONE) 
       vim_tlv_get_uint64_little( &tags, (VIM_UINT64*)&term->configuration.refund_daily_limit );

   if (vim_tlv_search(&tags, TAG_TERM_REFUND_DAILY_TOTAL) == VIM_ERROR_NONE)
	   vim_tlv_get_uint64_little(&tags, (VIM_UINT64*)&term->configuration.refund_daily_total);


   // RDD 090118 added for CTLS Cash on 2TapAndroid release
   // RDD 300118 JIRA:WWBP-133 Terminal forgot CTLS cash flag
   if (vim_tlv_search(&tags, TAG_TMS_CTLS_CASH_ENABLE) == VIM_ERROR_NONE)
	   vim_tlv_get_uint8(&tags, (VIM_UINT8*)&term->configuration.CtlsCashOut);



   // VN 21/11/17 Added for Aplle and Android VAS for WOW
   if (vim_tlv_search(&tags, TAG_COLLECTOR_ID) == VIM_ERROR_NONE) {
	   vim_tlv_get_uint32_little(&tags, &term->configuration.collector_id);
   }
   if (vim_tlv_search(&tags, TAG_GOOGLE_KEY_VERSION) == VIM_ERROR_NONE) {
	   vim_tlv_get_uint32_little(&tags, &term->configuration.google_key_version);
   }

   // RDD 100821 JIRA PIRPIN-1121 Surcharging  

   if (vim_tlv_search(&tags, TAG_SC_ON_TIP) == VIM_ERROR_NONE) 
       vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&TERM_SC_ON_TIP );   

   if (vim_tlv_search(&tags, TAG_SC_REDUCE_ON_CASH) == VIM_ERROR_NONE)
       vim_tlv_get_uint8(&tags, (VIM_UINT8 *)&TERM_SC_EXCLUDE_CASH);

   if (vim_tlv_search(&tags, TAG_SC_POINTS_VISA) == VIM_ERROR_NONE) {
       VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_visa, VIM_SIZEOF(term->configuration.surcharge_points_visa)));
   }    
   if (vim_tlv_search(&tags, TAG_SC_POINTS_MC) == VIM_ERROR_NONE) {
       VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_mc, VIM_SIZEOF(term->configuration.surcharge_points_mc)));
   }
   if (vim_tlv_search(&tags, TAG_SC_POINTS_AMEX) == VIM_ERROR_NONE) {
       VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_amex, VIM_SIZEOF(term->configuration.surcharge_points_amex)));
   }           
   if (vim_tlv_search(&tags, TAG_SC_POINTS_EFTPOS) == VIM_ERROR_NONE) {
       VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_eftpos, VIM_SIZEOF(term->configuration.surcharge_points_eftpos)));
   }
   if (vim_tlv_search(&tags, TAG_SC_POINTS_UPI) == VIM_ERROR_NONE) {
	   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_upi, VIM_SIZEOF(term->configuration.surcharge_points_upi)));
   }
   if (vim_tlv_search(&tags, TAG_SC_POINTS_DINERS) == VIM_ERROR_NONE) {
	   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_diners, VIM_SIZEOF(term->configuration.surcharge_points_diners)));
   }
   if (vim_tlv_search(&tags, TAG_SC_POINTS_JCB) == VIM_ERROR_NONE) {
	   VIM_ERROR_RETURN_ON_FAIL(vim_tlv_get_bytes(&tags, &term->configuration.surcharge_points_jcb, VIM_SIZEOF(term->configuration.surcharge_points_jcb)));
   }

   if (vim_tlv_search(&tags, TAG_MERCHANT_URL) == VIM_ERROR_NONE) {
	   vim_tlv_get_bytes(&tags, &term->configuration.merchant_url, VIM_SIZEOF(term->configuration.merchant_url));
   }
   if (vim_tlv_search(&tags, TAG_PASS_IDENTIFIER) == VIM_ERROR_NONE) {
	   vim_tlv_get_bytes(&tags, &term->configuration.pass_identfier, VIM_SIZEOF(term->configuration.pass_identfier));
   }
   if (vim_tlv_search(&tags, TAG_USE_GOOGLE_SDK) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.use_google_sdk);
   }

   // 130220 DEVX Disable PCI Token and ePAN return for Third Party Terminals
   if (vim_tlv_search(&tags, TAG_TMS_PCI_ENABLE) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.UsePCITokens);
   }

   if (vim_tlv_search(&tags, TAG_TMS_MERCHANT_ROUTING) == VIM_ERROR_NONE) {
	   vim_tlv_get_uint8(&tags, &term->configuration.use_mer_routing);
   }

   // 110520 Trello #137 Change to New LFD
   if (vim_tlv_search(&tags, TAG_NEW_LFD) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.NewLFD);
   }

   if (vim_tlv_search(&tags, TAG_QR_WALLET) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QrWallet);
   }
   if (vim_tlv_search(&tags, TAG_QR_PS_TIMEOUT) == VIM_ERROR_NONE) {
      vim_tlv_get_uint32_little(&tags, &term->configuration.QrPaymentSessionTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QR_QR_TIMEOUT) == VIM_ERROR_NONE) {
      vim_tlv_get_uint32_little(&tags, &term->configuration.QrQrCodeTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QR_IDLE_SCAN_TIMEOUT) == VIM_ERROR_NONE) {
      vim_tlv_get_uint32_little(&tags, &term->configuration.QrIdleScanTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QR_REVERSAL_TIMER) == VIM_ERROR_NONE) {
       vim_tlv_get_uint32_little(&tags, &term->configuration.QrReversalTimer);
   }
   if (vim_tlv_search(&tags, TAG_QR_UPDATE_PAY_TIMEOUT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint32_little(&tags, &term->configuration.QrUpdatePayTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QR_WALLY_TIMEOUT) == VIM_ERROR_NONE) {
      vim_tlv_get_uint32_little(&tags, &term->configuration.QrWallyTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QR_REWARDS_POLL) == VIM_ERROR_NONE) {
      vim_tlv_get_uint8(&tags, &term->configuration.QrRewardsPoll);
   }
   if (vim_tlv_search(&tags, TAG_QR_RESULTS_POLL) == VIM_ERROR_NONE) {
      vim_tlv_get_uint8(&tags, &term->configuration.QrResultsPoll);
   }
   if (vim_tlv_search(&tags, TAG_QR_POLLBUSTER) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QrPollbuster);
   }
   if (vim_tlv_search(&tags, TAG_QR_TENDER_ID) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QrTenderId, VIM_SIZEOF(term->configuration.QrTenderId));
   }
   if (vim_tlv_search(&tags, TAG_QR_TENDER_NAME) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QrTenderName, VIM_SIZEOF(term->configuration.QrTenderName));
   }
   if (vim_tlv_search(&tags, TAG_QR_DECLINED_RCT_TEXT) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QrDeclinedRctText, VIM_SIZEOF(term->configuration.QrDeclinedRctText));
   }
   if (vim_tlv_search(&tags, TAG_QR_MERCH_REF_PREFIX) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QrMerchRefPrefix, VIM_SIZEOF(term->configuration.QrMerchRefPrefix));
   }
   if (vim_tlv_search(&tags, TAG_QR_SEND_QR_TO_POS) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QrSendQrToPos);
   }
   if (vim_tlv_search(&tags, TAG_QR_ENABLE_WIDE_RCT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QrEnableWideReceipt);
   }
   if (vim_tlv_search(&tags, TAG_QR_DISABLE_FROM_IMAGE) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QrDisableFromImage);
   }

   // 280520 Trello #123 LFD Switch for VAS
   if (vim_tlv_search(&tags, TAG_NEW_VAS) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.NewVAS);
   }

   // Switch to enable/disable background Qwikcilver logon attempt at startup
   if (vim_tlv_search(&tags, TAG_QC_LOGON_ON_BOOT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.QcLogonOnBoot);
   }
   // Timeout to wait for response from Qwikcilver app
   if (vim_tlv_search(&tags, TAG_QC_RESP_TIMEOUT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint32_little(&tags, &term->configuration.QcRespTimeout);
   }
   if (vim_tlv_search(&tags, TAG_QC_USERNAME) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QcUsername, VIM_SIZEOF(term->configuration.QcUsername));
   }
   if (vim_tlv_search(&tags, TAG_QC_PASSWORD) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.QcPassword, VIM_SIZEOF(term->configuration.QcPassword));
   }




   // RDD 140520 Trello #66 VISA Tap Only terminal
   if (vim_tlv_search(&tags, TAG_CTLS_ONLY_TERM) == VIM_ERROR_NONE)
   {
       // RDD a this is not initialised by LFD ensure that garbage was not stored
       vim_tlv_get_uint8(&tags, &term->configuration.CTLSOnlyTerminal);
       if (term->configuration.CTLSOnlyTerminal != VIM_TRUE)
           term->configuration.CTLSOnlyTerminal = VIM_FALSE;
   }


   // RDD 140520 Trello #129 PCI Reboot sometimes happens more than once between 2-3am ( because VN didn't serialise the setting so it changed every power-up
   if (vim_tlv_search(&tags, TAG_PCI_REBOOT_OFFSET) == VIM_ERROR_NONE)
   {
       // RDD a this is not initialised by LFD ensure that garbage was not stored
       vim_tlv_get_uint8(&tags, &term->configuration.OffsetTime);
   }


  // Tag to control whether disconnect required after host message
   if (vim_tlv_search(&tags, TAG_HOST_DISCONNECT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint8(&tags, &term->configuration.allow_disconnect);
   }

   if (vim_tlv_search(&tags, TAG_VHQ_ONLY) == VIM_ERROR_NONE) {
	   vim_tlv_get_uint8(&tags, &term->configuration.VHQ_Only );
   }


   // 030420 RDD v582-7 Serialise the COVID19 LFD CVM override amount
   if (vim_tlv_search(&tags, TAG_COVID19_CVM_LIMIT) == VIM_ERROR_NONE) {
       vim_tlv_get_uint32_little(&tags, &term->configuration.term_override_cvm_value);
   }

   // 110520 Trello #142 Configure Pre-PCI reboot check
   if (vim_tlv_search(&tags, TAG_PCI_PRE_REBOOT_CHECK) == VIM_ERROR_NONE) {
       vim_tlv_get_uint32_little(&tags, &term->configuration.PreRebootCheckSecs);
   }


   if (vim_tlv_search(&tags, TAG_TMS_ACTIVE_CFG_VER) == VIM_ERROR_NONE) {
	   vim_tlv_get_bytes(&tags, &term->configuration.ActiveCfgVersion, VIM_SIZEOF(term->configuration.ActiveCfgVersion));
   }

   // RDD 250618 JIRA WWBP-26
   if (vim_tlv_search(&tags, TAG_TMS_BRANDING) == VIM_ERROR_NONE) {
	   vim_tlv_get_bytes(&tags, &term->configuration.BrandFileName, VIM_SIZEOF(term->configuration.BrandFileName));
   }

   if (vim_tlv_search(&tags, TAG_CPAT_FILENAME) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.CPAT_Filename, VIM_SIZEOF(term->configuration.CPAT_Filename));
   }
   if (vim_tlv_search(&tags, TAG_EPAT_FILENAME) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.EPAT_Filename, VIM_SIZEOF(term->configuration.EPAT_Filename));
   }
   if (vim_tlv_search(&tags, TAG_PKT_FILENAME) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.PKT_Filename, VIM_SIZEOF(term->configuration.PKT_Filename));
   }

   // RDD 101022 JIRA PIRPIN-1995 New Wifi Params
   if (vim_tlv_search(&tags, TAG_TERM_WIFI_SSID) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.WifiSSID, VIM_SIZEOF(term->configuration.WifiSSID));
   }
   if (vim_tlv_search(&tags, TAG_TERM_WIFI_PWD) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.WifiPWD, VIM_SIZEOF(term->configuration.WifiPWD));
   }
   if (vim_tlv_search(&tags, TAG_TERM_WIFI_PROT) == VIM_ERROR_NONE) {
       vim_tlv_get_bytes(&tags, &term->configuration.WifiPROT, VIM_SIZEOF(term->configuration.WifiPROT));
   }



   // RDD 110917 added for SPIRA:9242
  if (vim_tlv_search(&tags, TAG_TMS_MANPAN_PWORD) == VIM_ERROR_NONE)
    vim_tlv_get_uint64_little(&tags, (VIM_UINT64*)&term->configuration.manpan_password );

  // RDD 110917 - added because it was missing !
  if (vim_tlv_search(&tags, TAG_TERM_CPAT_ENTRIES) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->number_of_cpat_entries );

  if (vim_tlv_search(&tags, TAG_TERM_IDLE_LOOP_PROCESSING) == VIM_ERROR_NONE)
    vim_tlv_get_uint64_little(&tags, (VIM_UINT64*)&term->max_idle_time_seconds );
  if (vim_tlv_search(&tags, TAG_TERM_EFB_CREDIT_LIMIT) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->configuration.efb_credit_limit );
  if (vim_tlv_search(&tags, TAG_TERM_EFB_DEBIT_LIMIT) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->configuration.efb_debit_limit );
  if (vim_tlv_search(&tags, TAG_TERM_EFB_SAF_LIMIT) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->configuration.efb_max_txn_count );
  if (vim_tlv_search(&tags, TAG_TERM_EFB_TOT_LIMIT) == VIM_ERROR_NONE)
      vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->configuration.efb_max_txn_limit);


  if (vim_tlv_search(&tags, TAG_TERM_LAST_TMS_LOGON_TIME) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->last_real_logon_time, VIM_SIZEOF(term->last_real_logon_time));
  if (vim_tlv_search(&tags, TAG_TERM_TMS_SCHEDULED_TIME) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->tms_parameters.last_logon_time, VIM_SIZEOF(term->tms_parameters.last_logon_time));
  if (vim_tlv_search(&tags, TAG_TERM_TMS_FREQUENCY) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_parameters.logon_time_type);

  // RDD - end added for WOW

      if (vim_tlv_search(&tags, TAG_TERM_INTERNAL_MODEM) == VIM_ERROR_NONE)
		vim_tlv_get_bytes_exact(&tags, &term->internal_modem, VIM_SIZEOF(term->internal_modem));


  if (vim_tlv_search(&tags, TAG_TERMINAL_TMS_NII) == VIM_ERROR_NONE)
    vim_tlv_get_uint16_little(&tags, &term->tms_parameters.nii);
  if (vim_tlv_search(&tags, TAG_TERM_CONFIGURED) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->configured);
//  if (vim_tlv_search(&tags, TAG_TERM_BATCH_NUMBER) == VIM_ERROR_NONE)
 //   vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->batch_no);
 // if (vim_tlv_search(&tags, TAG_EFB_TIMER_PENDING) == VIM_ERROR_NONE)
 //   vim_tlv_get_uint8(&tags, &term->efb_timer_pending);
 // if (vim_tlv_search(&tags, TAG_EFB_TIMER) == VIM_ERROR_NONE)
 //   vim_tlv_get_bytes(&tags, &term->efb_timer, VIM_SIZEOF(term->efb_timer));
  if (vim_tlv_search(&tags, TAG_TERM_MODE) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->mode, VIM_SIZEOF(term->mode));
  //if (vim_tlv_search(&tags, TAG_TRANSSEND_ID) == VIM_ERROR_NONE)
  //  vim_tlv_get_bytes(&tags, &term->comms.transsend, VIM_SIZEOF(term->comms.transsend));
  //if (vim_tlv_search(&tags, TAG_STANDALONE_PARAMETERS) == VIM_ERROR_NONE)
  //  vim_tlv_get_bytes(&tags, &term->standalone_parameters, VIM_SIZEOF(term->standalone_parameters));
  if (vim_tlv_search(&tags, TAG_PULSE_DIAL) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.pulse_dial);
  if (vim_tlv_search(&tags, TAG_BLIND_DIAL) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.blind_dial);
  if (vim_tlv_search(&tags, TAG_QUICK_DIAL) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.fast_connect);
  if (vim_tlv_search(&tags, TAG_TERM_HEADER_TYPE) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, (VIM_UINT8 *) &term->tms_parameters.net_parameters.header.type);

  // RDD 281214 SPIRA:8292 Allow option for number of Prn cols to be configured via XML
  if (vim_tlv_search(&tags, TAG_TERM_CFG_PRN_COLS) == VIM_ERROR_NONE)
  {
      // RDD - new terminal will crash on Logon if bypass TMS somehow.
      VIM_UINT8 prn_cols;
      vim_tlv_get_uint8( &tags, &prn_cols );
      if (prn_cols >= 24)
      {
          term->configuration.prn_columns = prn_cols;
      }
  }
  if (vim_tlv_search(&tags, TAG_PABX) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->comms.pabx, VIM_SIZEOF(term->comms.pabx));
  if (vim_tlv_search(&tags, TAG_LAST_ACTIVITY) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->last_activity, VIM_SIZEOF(term->last_activity));
  // RDD 230516 Term status now gets stored in its own file to save on huge terminal save writes every time we change the status
  //if (vim_tlv_search(&tags, TAG_TERMINAL_STATUS) == VIM_ERROR_NONE)
  //  vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->status);
  if (vim_tlv_search(&tags, TAG_HOST_PHONE_NO) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->comms.bank_host_phone, VIM_SIZEOF(term->comms.bank_host_phone));
  if (vim_tlv_search(&tags, TAG_TERM_TERMINAL_ID) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->terminal_id, sizeof(term->terminal_id));
  if (vim_tlv_search(&tags, TAG_TERM_MERCHANT_ID) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->merchant_id, sizeof(term->merchant_id));
  if (vim_tlv_search(&tags, TAG_TERM_LAST_SETTLE_DATE) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->last_settle_date, sizeof(term->last_settle_date));
  if (vim_tlv_search(&tags, TAG_TMS_LINE_SPEED) == VIM_ERROR_NONE)
    vim_tlv_get_uint32_little(&tags, (VIM_UINT32*)&term->comms.tms_line_speed);
  if (vim_tlv_search(&tags, TAG_TMS_SHA) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->comms.sha, sizeof(term->comms.sha));
  if (vim_tlv_search(&tags, TAG_TMS_AUTO_LOGON) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_auto_logon);
  if (vim_tlv_search(&tags, TAG_SETTLEMENT_AUTO_RECEIPT) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->auto_settlement_receipt_prt);
  if (vim_tlv_search(&tags, TAG_TERM_SAF_REPEAT_PENDING) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->saf_repeat_pending);
  if (vim_tlv_search(&tags, TAG_TMS_SOFTWARE_CONFIGURED) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_software_updated);
  if (vim_tlv_search(&tags, TAG_TMS_HOST_PHONE_NO) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number, sizeof(term->tms_parameters.net_parameters.interface_settings.pstn_hdlc.dial.phone_number));
  if (vim_tlv_search(&tags, TAG_TMS_HOST_NII) == VIM_ERROR_NONE)
    vim_tlv_get_uint16_little(&tags, &term->comms.bank_host_nii);

  /* LFD auto download settings */
  if (vim_tlv_search(&tags, TAG_TMS_AUTO_DOWNLOAD_TYPE) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_auto_download_settings.download_type);
  if (vim_tlv_search(&tags, TAG_TMS_AUTO_DOWNLOAD_TIME) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->tms_auto_download_settings.download_time, sizeof(term->tms_auto_download_settings.download_time));
  if (vim_tlv_search(&tags, TAG_TMS_AUTO_DOWNLOAD_VERSION) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, term->tms_auto_download_settings.version, sizeof(term->tms_auto_download_settings.version));
  if (vim_tlv_search(&tags, TAG_TMS_AUTO_DOWNLOAD_STATUS) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_auto_download_settings.status);

  if (vim_tlv_search(&tags, TAG_TERM_MERCHANT_PASSWORD) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.password, VIM_SIZEOF(term->configuration.password));

  // RDD 010814 - ALH new params
  if (vim_tlv_search(&tags, TAG_TERM_REFUND_PASSWORD) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.refund_password, VIM_SIZEOF(term->configuration.refund_password));

  if (vim_tlv_search(&tags, TAG_TERM_MOTO_PASSWORD) == VIM_ERROR_NONE)
      vim_tlv_get_bytes(&tags, &term->configuration.moto_password, VIM_SIZEOF(term->configuration.moto_password));

  if (vim_tlv_search(&tags, TAG_TERM_CFG_CASHOUT_MAX) == VIM_ERROR_NONE)
    vim_tlv_get_uint64_little(&tags, (VIM_UINT64*)&term->configuration.cashout_max);

  if (vim_tlv_search(&tags, TAG_TERM_CFG_PREAUTH_FLAG) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->configuration.preauth_enable);

  if (vim_tlv_search(&tags, TAG_TERM_ENABLE_CHECKOUT) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.checkout_enable);
  if (vim_tlv_search(&tags, TAG_TERM_ENABLE_REFUND) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.refund_enable);
  if (vim_tlv_search(&tags, TAG_TERM_ENABLE_VOID) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.void_enable);


  if (vim_tlv_search(&tags, TAG_TERM_CFG_ALLOW_MOTO) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.moto_ecom_enable);

  if (vim_tlv_search(&tags, TAG_TERM_CFG_CCV_ENTRY) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->configuration.ccv_entry);

  if (vim_tlv_search(&tags, TAG_TMS_MC_EMV_REFUND) == VIM_ERROR_NONE)       // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
      vim_tlv_get_uint8(&tags, &term->configuration.MCEMVRefundEnabled);

  if (vim_tlv_search(&tags, TAG_TMS_VISA_EMV_REFUND) == VIM_ERROR_NONE)       // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
      vim_tlv_get_uint8(&tags, &term->configuration.VisaEMVRefundEnabled);

  if (vim_tlv_search(&tags, TAG_TMS_EMV_REFUND_CPAT_OVERRIDE) == VIM_ERROR_NONE)       // 030820 Trello #82 DE55 Refunds for VISA and Mastercard
      vim_tlv_get_uint8(&tags, &term->configuration.EMVRefundCPATOverride);

  if (vim_tlv_search(&tags, TAG_TMS_FORBID_OPTIONAL_XML_TAGS) == VIM_ERROR_NONE)      // 280920 Trello #281 - MB wants to be able to disable extra tags if needs be
      vim_tlv_get_uint8(&tags, &term->configuration.ForbidExtraXMLTags);

  // 260721 JIRA-PIRPIN 1118 VDA
  if ( vim_tlv_search( &tags, TAG_TERM_VDA_DISABLE ) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.VDA );
  if (vim_tlv_search(&tags, TAG_TERM_MDA_DISABLE) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.MDA);


  if (vim_tlv_search(&tags, TAG_TERM_VOID_PASSWORD) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.void_password, VIM_SIZEOF(term->configuration.void_password));
  if (vim_tlv_search(&tags, TAG_TERM_SETTLEMENT_PASSWORD) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->configuration.settlement_password, VIM_SIZEOF(term->configuration.settlement_password));

  if (vim_tlv_search(&tags, TAG_TERM_TMS_IS_FINISHED) == VIM_ERROR_NONE)
    vim_tlv_get_uint8(&tags, &term->tms_is_finished);

  if (vim_tlv_search(&tags, TAG_TERM_HOST_ID) == VIM_ERROR_NONE)
    vim_tlv_get_bytes(&tags, &term->host_id, sizeof(term->host_id));

  // RDD 120523 JIRA PIRPIN_2383 SLYP Authcode
  if (vim_tlv_search(&tags, TAG_TERM_USE_DE38) == VIM_ERROR_NONE)   
      vim_tlv_get_uint8(&tags, &term->configuration.de38_to_pos);

  // RDD 140923 JIRA PIRPIN_2712 Countdown -> Woolworths
  if (vim_tlv_search(&tags, TAG_TERM_NZ_WOW) == VIM_ERROR_NONE)
      vim_tlv_get_uint8(&tags, &term->configuration.nz_wow);


#if 0 /*  TODO: */
  /* Read merchant info */
  term->merchant_count = 0;

  vim_tlv_rewind(&tags);
  while (vim_tlv_find(&tags ,TAG_MERCHANT_CONTAINER) == VIM_ERROR_NONE)
  {
    if (vim_tlv_open(&merch_tags, tags.current_item.value, tags.current_item.length, tags.current_item.length, FALSE) == VIM_ERROR_NONE)
    {
      if (vim_tlv_search(&merch_tags, TAG_CATID) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, term->merchants[term->merchant_count].catid, sizeof(term->merchants[term->merchant_count].catid));
      if (vim_tlv_search(&merch_tags, TAG_CAIC) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, term->merchants[term->merchant_count].caic,sizeof(term->merchants[term->merchant_count].caic));
      if (vim_tlv_search(&merch_tags, TAG_MERCH_VISIBILITY) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, term->merchants[term->merchant_count].visibility, sizeof(term->merchants[term->merchant_count].visibility));
      if (vim_tlv_search(&merch_tags, TAG_MERCHANT_NAME) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, term->merchant_info[term->merchant_count].merchant_name, sizeof(term->merchant_info[term->merchant_count].merchant_name));
      if (vim_tlv_search(&merch_tags, TAG_MERCHANT_INITIALISED) == VIM_ERROR_NONE)
        vim_tlv_get_uint8(&merch_tags, &term->merchant_info[term->merchant_count].initialised);
      if (vim_tlv_search(&merch_tags, TAG_TERM_VARS) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].term_vars, sizeof(term->merchant_info[term->merchant_count].term_vars));
      if (vim_tlv_search(&merch_tags, TAG_FUNC_VARS) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].func_vars, sizeof(term->merchant_info[term->merchant_count].func_vars));
      if (vim_tlv_search(&merch_tags, TAG_NAME_FLAGS) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].name_flags, sizeof(term->merchant_info[term->merchant_count].name_flags));
      if (vim_tlv_search(&merch_tags, TAG_FLOOR_LIMITS) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].floor_limits, sizeof(term->merchant_info[term->merchant_count].floor_limits));
      if (vim_tlv_search(&merch_tags, TAG_SAF_UPLOAD) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].saf_upload, sizeof(term->merchant_info[term->merchant_count].saf_upload));
      if (vim_tlv_search(&merch_tags, TAG_CARD_ACCEPTANCE) == VIM_ERROR_NONE)
        vim_tlv_get_bytes(&merch_tags, &term->merchant_info[term->merchant_count].blocked_cards, sizeof(term->merchant_info[term->merchant_count].blocked_cards));

      term->merchant_info[term->merchant_count].ICC_limit_count=0;

      if (vim_tlv_search(&merch_tags, TAG_MERCHANT_ICC_LIMITS) == VIM_ERROR_NONE)
      {

        VIM_TLV_LIST icc_records;
        if (vim_tlv_open_current(&merch_tags,&icc_records,VIM_FALSE)==VIM_ERROR_NONE)
        {

          VIM_SIZE count=0;
          while(vim_tlv_find_next(&icc_records,TAG_MERCHANT_ICC_LIMIT_RECORD)==VIM_ERROR_NONE)
          {
            VIM_TLV_LIST icc_limits;
            icc_transaction_limits_t* current=&term->merchant_info[term->merchant_count].ICC_limit[count];

            vim_tlv_open_current(&icc_records,&icc_limits,VIM_FALSE);
            if (vim_tlv_search(&icc_limits, TAG_ICC_RID) == VIM_ERROR_NONE)
              vim_tlv_get_bytes(&icc_limits,&current->RID,sizeof(current->RID));

            if (vim_tlv_search(&icc_limits, TAG_ICC_LIMIT_DOMESTIC) == VIM_ERROR_NONE)
              vim_tlv_get_uint32_little(&icc_limits,&current->domestic);

            if (vim_tlv_search(&icc_limits, TAG_ICC_LIMIT_INTERNATIONAL) == VIM_ERROR_NONE)
              vim_tlv_get_uint32_little(&icc_limits,&current->international);

            if (++count>=VIM_LENGTH_OF(term->merchant_info[term->merchant_count].ICC_limit))
              break;
          }
          term->merchant_info[term->merchant_count].ICC_limit_count=count;
        }
      }

      term->merchant_count++;
    }
    if (vim_tlv_goto_next(&tags) != VIM_ERROR_NONE)
      break;
  }
#endif
  return VIM_ERROR_NONE;
}


// RDD 091111 - Implementation of FCAT file processing for Woolworths

VIM_ERROR_PTR deserialise_fuelcard_configuration( fuelcard_table_t *fuelcard_config, VIM_TLV_LIST *tlv_list_ptr )
{
  VIM_SIZE cnt = 0;
  VIM_TLV_LIST container_tlv_list;

  // Wipe the structure
  vim_mem_clear( fuelcard_config, VIM_SIZEOF(fuelcard_table_t) );

  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_rewind( tlv_list_ptr));

  if( vim_tlv_search (tlv_list_ptr, WOW_FCAT_FILE_VERSION ) == VIM_ERROR_NONE )
  {
	 VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( tlv_list_ptr, &file_status[FCAT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN ) );
	 terminal.file_version[FCAT_FILE_IDX] = bcd_to_bin ( (VIM_UINT8 *)&file_status[FCAT_FILE_IDX].ped_version, WOW_FILE_VER_BCD_LEN*2 );
  }

  // Jump to the containerfor the Product tables
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( tlv_list_ptr, WOW_FCPD_CONTAINER ));

  // Open this container as a new TLV list
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current( tlv_list_ptr, &container_tlv_list, VIM_FALSE ));

  while( vim_tlv_find_next( &container_tlv_list, WOW_FCPD_SUB ) == VIM_ERROR_NONE )
  {
    VIM_TLV_LIST detail_tlv_list;
	VIM_SIZE ProductGroupIndex = 0;

    VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current( &container_tlv_list, &detail_tlv_list, VIM_FALSE ));

	// RDD: first tag is product group ID - we use this as the table entry for easy indexing later
    if( vim_tlv_search( &detail_tlv_list, WOW_PG_ID ) == VIM_ERROR_NONE )
    {
	  VIM_CHAR ProductGroupID[WOW_FC_PRODUCT_GROUP_ID_LEN];
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, ProductGroupID, WOW_FC_PRODUCT_GROUP_ID_LEN ));
	  ProductGroupIndex = asc_to_bin( ProductGroupID, WOW_FC_PRODUCT_GROUP_ID_LEN );
    }

	// RDD: next tag is product group description (truncated to a nominal 30 bytes)
	if( vim_tlv_search( &detail_tlv_list, WOW_PG_DESC ) == VIM_ERROR_NONE )
    {
      VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, fuelcard_config->ProductTables[ProductGroupIndex].ProductDescription, WOW_FC_PRODUCT_DESCRIPTION_LEN ));
    }

	// RDD: next tag is the volume flag
	if( vim_tlv_search( &detail_tlv_list, WOW_PG_VOLUME ) == VIM_ERROR_NONE )
    {
		VIM_CHAR VolumeFlag;
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, &VolumeFlag, VIM_SIZEOF(VIM_UINT8) ));
		fuelcard_config->ProductTables[ProductGroupIndex].VolumeFlag = (VolumeFlag == '0') ? VIM_FALSE : VIM_TRUE;
    }


	// Next tag is the Group Type
	if( vim_tlv_search( &detail_tlv_list, WOW_PG_GROUP_TYPE ) == VIM_ERROR_NONE )
    {
		VIM_CHAR GroupType;
		VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, &GroupType, VIM_SIZEOF(VIM_UINT8) ));
		fuelcard_config->ProductTables[ProductGroupIndex].GroupType = GroupType - '0';
    }
	if( cnt++ > WOW_MAX_PRODUCT_TABLES )
      VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
  }

  // record the actual number of product tables and then reset the counter
  fuelcard_config->NumberOfProductTables = cnt;
  cnt = 0;

  // Jump to the containerfor the Product tables
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_search( tlv_list_ptr, WOW_FC_OFFLINE_LIMIT_CONTAINER ));

  // Open this container as a new TLV list
  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_open_current( tlv_list_ptr, &container_tlv_list, VIM_FALSE ));

  // Pull out the Offline limit tables
  while( vim_tlv_find_next( &container_tlv_list, WOW_FC_OFFLINE_LIMIT_SUB ) == VIM_ERROR_NONE )
  {
	VIM_SIZE n;
	VIM_UINT8 OLT_Index;
	VIM_TLV_LIST detail_tlv_list;

	// Deserialise this product group into it's relevent structure
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open_current( &container_tlv_list, &detail_tlv_list, VIM_FALSE ));

	// First tag is OLT Index - this should increment from 0->(max) 9
    if( vim_tlv_search( &detail_tlv_list, WOW_OL_INDEX ) == VIM_ERROR_NONE )
    {
	  VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, &OLT_Index, VIM_SIZEOF(VIM_UINT8) ));
	  OLT_Index -= '0'; // to get an index.
    }

	// Next Tags are the Offline Limits By group. The OLT_Index should increment from 0 upwards, but we'll index off the array element anyway when X-ref with CPAT
	for( n=0; n<WOW_MAX_OFFLINE_LIMIT_GROUPS; n++ )
	{
		VIM_UINT8 tag = 0xC0 + n;

		if( vim_tlv_search( &detail_tlv_list, &tag ) == VIM_ERROR_NONE )
		{
			VIM_CHAR OfflineLimit[WOW_OFFLINE_LIMIT_ASC_LEN];
			VIM_ERROR_RETURN_ON_FAIL( vim_tlv_get_bytes( &detail_tlv_list, OfflineLimit, WOW_OFFLINE_LIMIT_ASC_LEN ));
			fuelcard_config->OfflineLimitTables[OLT_Index].OfflineLimitTable[n] = asc_to_bin( OfflineLimit, WOW_OFFLINE_LIMIT_ASC_LEN );
		}
	}
    if( cnt++ > WOW_MAX_OFFLINE_LIMIT_TABLES )
      VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
  }
  fuelcard_config->NumberOfOfflineTables = cnt;

  return VIM_ERROR_NONE;
}

