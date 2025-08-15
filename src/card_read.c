#include <incs.h>

VIM_DBG_SET_FILE("T2");
static const char *last_track2_file_name = VIM_FILE_PATH_LOCAL "track2.dat";
extern VIM_ERROR_PTR EmvCardRemoved(VIM_BOOL GenerateError);
extern card_bin_name_item_t CardNameTable[];


extern VIM_ERROR_PTR vim_pceftpos_get_key_input(VIM_PCEFTPOS_PTR self_ptr, VIM_UINT8 *input);
extern VIM_BOOL IsSlaveMode(void);

#define TRACK1_POS  0
#define TRACK2_POS  1
#define TRACK3_POS  2

#define MAG_PTR		0
#define ICC_PTR		1
#define PICC_PTR	2
#define KBD_PTR		3
#define POS_PTR		4
#define PICCA_PTR   5

#define MAX_EVENTS 6

extern VIM_CHAR type[20];

#define PICC_SCHEMELOGOS_1 "SchemeLogos1.pcx"
#define PICC_SCHEMELOGOS_2 "SchemeLogos2.pcx"
#define PICC_SCHEMELOGOS_3 "SchemeLogos3.pcx"
#define PICC_SCHEMELOGOS_4 "SchemeLogos4.pcx"
#define PICC_SCHEMELOGOS_5 "SchemeLogos5.pcx"
#define PICC_SCHEMELOGOS_6 "SchemeLogos6.pcx"
#define PICC_SCHEMELOGOS_7 "SchemeLogos7.pcx"
#define PICC_SCHEMELOGOS_8 "SchemeLogos8.pcx"
#define PICC_SCHEMELOGOS_9 "SchemeLogos9.pcx"


typedef enum cc_state_t
{
  cc_wait_event,
  cc_key_pan,
  cc_key_expiry,
  cc_card_swiped,
  cc_card_inserted,
  cc_bad_swipe,
  cc_complete
}cc_state_t;

// RDD 020212 - Format an amount into a string for display
static void close_MAG_reader(void)
{
  if( mag_handle.instance_ptr != VIM_NULL )
  {
	// RDD 1111119 - reads all track data not read already flush on open instead
    //vim_mag_flush (mag_handle.instance_ptr);
    vim_mag_destroy (mag_handle.instance_ptr);
    mag_handle.instance_ptr = VIM_NULL;
  }
}

void vas_error_call_back(VIM_ERROR_PTR error)
{
	if (error == VIM_ERROR_NOT_FOUND) {
		VIM_DBG_MESSAGE("vas_error_call_back");
		VIM_ERROR_IGNORE(display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassError.png"), VIM_PCEFTPOS_KEY_CANCEL));
		vim_sound(VIM_TRUE);
		VIM_ERROR_IGNORE(vim_event_sleep(2000)); // wait 2 secs

		if (!terminal.abort_transaction)	{
			VIM_ERROR_IGNORE(display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassTap.png"), VIM_PCEFTPOS_KEY_CANCEL));
		}
	}
}


VIM_CHAR *GiftCardType(void)
{
    static VIM_CHAR txn_type[20];
    static VIM_CHAR *Ptr = txn_type;

    vim_mem_clear(txn_type, VIM_SIZEOF(txn_type));

    vim_strcpy(Ptr, "Gift Card");

	if (txn.type == tt_refund)    
	{    
		vim_strcpy(Ptr, "Refund");        
	}    
	else if (txn.type == tt_sale)    
	{    
		vim_strcpy(Ptr, "Redemption");        
	}    
	else if (txn.type == tt_balance)    
	{    
		vim_strcpy(Ptr, "Balance");        
	}	
	else if (txn.type == tt_gift_card_activation)
	{
		vim_strcpy(Ptr, "Activation");		
	}
    return Ptr;
}


/*----------------------------------------------------------------------------*/
VIM_CHAR *AmountString( VIM_UINT64 Amount )
{
    static VIM_CHAR AsciiAmount[16];
	VIM_CHAR *Ptr = AsciiAmount;

    // Set a single space by default
    vim_strcpy( Ptr, " " );

    // RDD 011221 don't display amount unless total is non zero
    if( txn.amount_total == 0 )
    {
        return Ptr;
    }

	VIM_SIZE Dollars = (VIM_UINT32)(Amount/100);
	VIM_SIZE Cents = (VIM_UINT32)(Amount%100);

	vim_mem_clear( AsciiAmount, VIM_SIZEOF(AsciiAmount) );
    vim_snprintf( Ptr, VIM_SIZEOF(AsciiAmount), "$%lu.%2.2lu", Dollars, Cents );	
    DBG_MESSAGE( Ptr );
	return Ptr;
}

VIM_CHAR *AmountString1( VIM_INT64 Amount ) //BD Blank for -1
{
	static VIM_CHAR AsciiAmount[16];
	VIM_CHAR *Ptr = AsciiAmount;

	VIM_SIZE Dollars = (VIM_UINT32)(Amount/100);
	VIM_SIZE Cents = (VIM_UINT32)(Amount%100);

	if( Amount >= 0 )
	{
		vim_snprintf( Ptr, VIM_SIZEOF(AsciiAmount), "$%lu.%2.2lu", Dollars, Cents );
	}
	else
	{
		vim_strcpy( Ptr, " " );
	}
	return Ptr;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_last_track2_load(query_card_details_t *details)
{
  VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(last_track2_file_name,details,VIM_SIZEOF(*details),0));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_last_track2_save(query_card_details_t *details)
{

  VIM_ERROR_RETURN_ON_FAIL(vim_file_set(last_track2_file_name, details, sizeof(*details)));
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR delete_card_last_track2(void)
{
  VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(last_track2_file_name));
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
/**
 * Check if the card is expired
 */
VIM_BOOL card_check_expired
(
  /**[in] Card to check expiry of */
  card_info_t *card_details
)
{
  VIM_UINT8 card_yy;
  VIM_UINT8 card_mm;
  VIM_UINT8 curr_yy;
  VIM_UINT8 curr_mm;
  VIM_SIZE pan_len;
  VIM_RTC_TIME current;
  VIM_CHAR *Ptr = &card_details->data.mag.track2[0];

  vim_rtc_get_time(&current);

  // RDD 050221 - no expiry check for gift cards in Australia or New Zealand
  if( gift_card( &txn ))
  {
      //VIM_DBG_MESSAGE( "---- Gift Card is exempt from Expiry date checking ---- ")
      return VIM_FALSE;
  }

  if (card_details->type == card_mag)
  {
    for ( pan_len = 0; pan_len < card_details->data.mag.track2_length; pan_len++ )
    {
      if ( *Ptr++ == '=' )
        break;
    }

	// RDD 300512 added - some fuel cards have a "000" padding between the '=' and the expiry date !
	//if( fuel_card(&txn) && ( !vim_strncmp( Ptr, "000", 3 )) )
	// RDD 070612 SPIRA 5644 - some cards don't have a 000 filler ...
	if( fuel_card(&txn) )
	{
		if(( CardIndex(&txn) == STARCASH ) || ( CardIndex(&txn) == STARCARD ) || ( CardIndex(&txn) == VITALGAS ))
			Ptr+=3;
		else if( CardIndex(&txn) == FLEETCARD )
			Ptr+=4;	// RDD - extra '=' on FLEETCARDS
	}
    card_yy = asc_to_bin( Ptr, 2);
	Ptr+=2;
    card_mm = asc_to_bin( Ptr, 2);

    VIM_DBG_VAR(card_mm);
    VIM_DBG_VAR(card_yy);

  }
  else if (card_details->type == card_manual)
  {
    card_yy = asc_to_bin(card_details->data.manual.expiry_yy, 2);
    card_mm = asc_to_bin(card_details->data.manual.expiry_mm, 2);
    VIM_DBG_VAR(card_mm);
    VIM_DBG_VAR(card_yy);
  }
  else if (card_details->type == card_chip)
  {
    for (pan_len = 0; pan_len < card_details->data.chip.track2_length; pan_len++)
    {
      if (card_details->data.chip.track2[pan_len] == '=')
        break;
    }
    card_yy = asc_to_bin(&card_details->data.chip.track2[pan_len+1], 2);
    card_mm = asc_to_bin(&card_details->data.chip.track2[pan_len+3], 2);
  }
  else if (card_details->type == card_picc)
  {
    for (pan_len = 0; pan_len < card_details->data.picc.track2_length; pan_len++)
    {
      if (card_details->data.picc.track2[pan_len] == '=')
        break;
    }
    card_yy = asc_to_bin(&card_details->data.picc.track2[pan_len+1], 2);
    card_mm = asc_to_bin(&card_details->data.picc.track2[pan_len+3], 2);
  }
  else
    return VIM_FALSE;

  curr_yy = current.year%100;
  curr_mm = current.month;
  //VIM_DBG_VAR(curr_yy);
  //VIM_DBG_VAR(curr_mm);
  //VIM_DBG_VAR(card_yy);
  //VIM_DBG_VAR(card_mm);


  return (card_yy == curr_yy) ? (curr_mm > card_mm) : (curr_yy > card_yy);
}


void ReportLuhnFail( void )
{
	txn.error = ERR_LUHN_CHECK_FAILED;

	// RDD 090512 SPIRA:5463 This needs to be set once the card has passed the initial CPAT checks
	transaction_clear_status( &txn, st_cpat_check_completed );
	display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
}


#if 1

// RDD 050912 P3: SLICE up the event waiting procedure in order to check the abort flag, set by a staus req.
#define SLICE 5000
#define NUMBER_OF_LOGO_FILES 9
#define INTERVAL 1500


VIM_ERROR_PTR txn_event_wait
(
  VIM_EVENT_FLAG_PTR const* event_ptr_ptr,
  VIM_SIZE event_ptr_count,
  VIM_MILLISECONDS timeout,
  VIM_BOOL CycleLogo,
  VIM_TEXT const *header_ptr
)
{
	VIM_UINT32 n=0, sliced_timeout = timeout/SLICE, zCount = 0;
	VIM_ERROR_PTR res;
	VIM_RTC_UPTIME uptime, last_uptime, end_time;
	//VIM_SIZE LastSecs = 0, Secs = 0;

	//VIM_CHAR *Filenames[NUMBER_OF_LOGO_FILES+1] = { "CtlsAJ.pcx","CtlsAMV.pcx","CtlsEC.pcx", "" };
	VIM_CHAR *Filenames[NUMBER_OF_LOGO_FILES+1] = { PICC_SCHEMELOGOS_1,PICC_SCHEMELOGOS_2,PICC_SCHEMELOGOS_3,PICC_SCHEMELOGOS_4,PICC_SCHEMELOGOS_5,PICC_SCHEMELOGOS_6,PICC_SCHEMELOGOS_7,PICC_SCHEMELOGOS_8,PICC_SCHEMELOGOS_9 };
	VIM_CHAR **FileNamePtr = Filenames;

	vim_rtc_get_uptime( &uptime );
	vim_rtc_get_uptime( &last_uptime );
	end_time = uptime + timeout;

	while( uptime < end_time )
	{
		VIM_INT16 RamUsedKB = 0;
		vim_prn_get_ram_used(&RamUsedKB);
		//VIM_DBG_PRINTF1("!!!!! RAM USED = %d Kb !!!!", RamUsedKB);


		// RDD 141215 Update the Contactless logos so they get display or or three at a time...
		//LastSecs = uptime/5000;
		vim_rtc_get_uptime( &uptime );
#if 0
		Secs = uptime/5000;
		if (Secs != LastSecs)
		{
			Secs = LastSecs;
			if (pceft_debugf(pceftpos_handle.instance, "%d", zCount++) != VIM_ERROR_NONE)
			{
				//VIM_DBG_MESSAGE("Z0-Fail");
				display_result(VIM_ERROR_DISCONNECTED, "", "", "", VIM_PCEFTPOS_KEY_NONE, 2000);
				vim_kbd_sound_invalid_key();
				picc_close(VIM_TRUE);
				pceft_pos_display_close();
				VIM_ERROR_RETURN(VIM_ERROR_DISCONNECTED);
			}
			else
			{
				//VIM_DBG_MESSAGE("Z0-Success");
			}
		}

#endif
		if( CycleLogo == VIM_TRUE )
		{
			//DBG_MESSAGE( "CYCLE LOGO !!! ");
			if( uptime/INTERVAL > last_uptime/INTERVAL )
			{
				VIM_BOOL Exists = VIM_FALSE;
				VIM_CHAR Buffer[50];

				zCount++;
				FileNamePtr++;
				vim_snprintf( Buffer, VIM_SIZEOF(Buffer), "%s%s",VIM_FILE_PATH_LOCAL, *FileNamePtr );
				VIM_ERROR_IGNORE( vim_file_exists( Buffer, &Exists ));
				// Reset pointer to fist file if current doesn't exist ( ie. allow room for future expansion... )
				if( !Exists )
					FileNamePtr = Filenames;

				// Secs have rolled so switch the display
#ifdef _WIN32
				vim_screen_set_control_image( "label", *FileNamePtr );
#endif
				// RDD 230516 SPIRA:
				//VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_CONTACTLESS_PAYMENT, VIM_PCEFTPOS_KEY_CANCEL, " ", header_ptr, AmountString(txn.amount_total) ));

				last_uptime = uptime;
			}
		}

		if(( res = vim_event_wait( event_ptr_ptr, event_ptr_count, sliced_timeout )) != VIM_ERROR_TIMEOUT )
		{
			DBG_MESSAGE( "<<<<<<<<< EVENT DETECTED - EXIT >>>>>>>>>>>" );
			// RDD 291217 get event deltas for logging
			//vim_rtc_get_uptime( &txn.uEventDeltas.Fields.Presented );

			VIM_ERROR_RETURN( res );
		}

		// Check for abort...
		if( terminal.abort_transaction )
		{
			//terminal.abort_transaction = VIM_FALSE;

			DBG_MESSAGE( "<<<<<<<<< SETTING ABORT TXN >>>>>>>>>>>" );
			txn.error = VIM_ERROR_NOT_FOUND;
			return txn.error;
		}
		if(( n++ == SLICE )&&( timeout ))
			break;
	}
	return VIM_ERROR_TIMEOUT;
}

#else

// RDD 050912 P3: SLICE up the event waiting procedure in order to check the abort flag, set by a staus req.
#define SLICE 100

VIM_ERROR_PTR txn_event_wait
(
  VIM_EVENT_FLAG_PTR const* event_ptr_ptr,
  VIM_SIZE event_ptr_count,
  VIM_MILLISECONDS timeout
)
{
	VIM_UINT32 n=0, sliced_timeout = timeout/SLICE;
	VIM_ERROR_PTR res;

	while(1)
	{
		if(( res = vim_event_wait( event_ptr_ptr, event_ptr_count, sliced_timeout )) != VIM_ERROR_TIMEOUT )
		{
			DBG_MESSAGE( "<<<<<<<<< EVENT DETECTED - EXIT >>>>>>>>>>>" );
			return res;
		}
		// Check for abort...
		if( terminal.abort_transaction )
		{
			//terminal.abort_transaction = VIM_FALSE;

			DBG_MESSAGE( "<<<<<<<<< SETTING ABORT TXN >>>>>>>>>>>" );
			txn.error = VIM_ERROR_NOT_FOUND;
			return txn.error;
		}
		if(( n++ == SLICE )&&( timeout ))
			break;
	}
	return VIM_ERROR_TIMEOUT;
}

#endif



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_s_gift_card( card_info_t *card_details_ptr, VIM_KBD_CODE *FirstChar )
{
    VIM_ERROR_PTR res = VIM_ERROR_NONE;
    kbd_options_t entry_opts;
    VIM_CHAR entry_buf[25];
    VIM_SIZE BinLen = vim_strlen(terminal.ecg_bin);

    vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));

    entry_buf[0] = *FirstChar;
    entry_opts.timeout = ENTRY_TIMEOUT;
    entry_opts.max = (WOW_PAN_LEN_MAX - BinLen) + 3;
    entry_opts.min = WOW_PAN_LEN_MAX - BinLen;
    entry_opts.entry_options = KBD_OPT_BEEP_ON_CLR | KBD_OPT_DISP_ORIG;
    // RDD 020115 SPIRA:8285 Need to check events for integrated
    entry_opts.CheckEvents = VIM_TRUE; // for POS Cancel

    VIM_DBG_NUMBER(entry_opts.min);
    VIM_DBG_NUMBER(entry_opts.max);

    do
    {
        // RDD 020413 - If manual because of e-gift card, append luhn and prepend WW IIN to this data to form pseudo-pan
#ifdef _VOS
        DBG_MESSAGE("------------------- Run Data Entry ---------------------");
        if ((res = display_data_entry(entry_buf, &entry_opts, DISP_ENTER_EGC_SW, VIM_PCEFTPOS_KEY_CANCEL, GiftCardType(), AmountString(txn.amount_total))) == VIM_ERROR_NONE)
#else
                // RDD 020413 - If manual because of e-gift card, append luhn and prepend WW IIN to this data to form pseudo-pan
        VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_ENTER_EGC, VIM_PCEFTPOS_KEY_CANCEL, GiftCardType(), AmountString(txn.amount_total)));
        if ((res = data_entry(entry_buf, &entry_opts)) == VIM_ERROR_NONE)
#endif
        {
            VIM_SIZE Len = vim_strlen(entry_buf);
            DBG_VAR(Len);
            DBG_VAR(BinLen);
            transaction_set_status(&txn, st_pos_sent_keyed_pan);

            card_details_ptr->type = card_manual;
            // RDD 130815 v546-0 SPIRA:8799 Allow only 10 or 13 digits to be entered and if so pre-pend 1st 6 digits of BIN
            // Modify BIN len - eg. should become 6 if 13 digits entered
            BinLen = WOW_PAN_LEN_MAX - Len;
            vim_mem_clear(card_details_ptr->data.manual.pan, VIM_SIZEOF(card_details_ptr->data.manual.pan));
            vim_strncpy(card_details_ptr->data.manual.pan, terminal.ecg_bin, BinLen);
            vim_strcat(card_details_ptr->data.manual.pan, entry_buf);
            card_details_ptr->data.manual.pan_length = WOW_PAN_LEN_MAX;
            VIM_DBG_STRING(card_details_ptr->data.manual.pan);
            txn.card_name_index = find_card_range(&txn.card, (VIM_UINT8 *)card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length);

            // RDD 300321 Temporarily make luhn chack for manually keyed cards dependant on CPAT.
            if (CARD_NAME_LUHN_CHECKING)
            {
                //VIM_DBG_MESSAGE("--------------------- Check Luhn ----------------------");
                if (!check_luhn(card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length))
                {
                    //vim_kbd_sound_invalid_key();
                    ReportLuhnFail();
                    vim_mem_clear(card_details_ptr->data.manual.pan, VIM_SIZEOF(card_details_ptr->data.manual.pan));
                    card_details_ptr->data.manual.pan_length = 0;
                    vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));
                    //display_screen(DISP_ENTER_EGC_SW, VIM_PCEFTPOS_KEY_CANCEL, GiftCardType(), AmountString(txn.amount_total));
                    continue;
                }                
            }
            else
            {
                VIM_DBG_MESSAGE("------------------ Bypass Luhn Check ------------------");
            }
            VIM_ERROR_RETURN(res);
        }
    }
    while( res == VIM_ERROR_NONE );
    /* return without error */
    VIM_ERROR_RETURN( res );
}




#if 1

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_key_PAN
(
  card_info_t *card_details_ptr,
  const VIM_TEXT *TxnTypePtr,
  VIM_CHAR *TxnAmtPtr
)
{
  kbd_options_t entry_opts;
  VIM_CHAR entry_buf[50];
  VIM_CHAR txn_type[20];
  VIM_SIZE BinLen = vim_strlen( terminal.ecg_bin );

  vim_mem_clear( txn_type, VIM_SIZEOF(txn_type) );
  vim_mem_clear( entry_buf, VIM_SIZEOF( entry_buf ) );


  entry_opts.timeout = ENTRY_TIMEOUT;

  // RDD 161015 SPIRA:8826 Man PAN allows 23 digits to be entered
#if 0 // code to v549-1

  // RDD P3 210812 15 digit max for manually entered loyalty card
  entry_opts.max = ( txn.type == tt_query_card ) ? WOW_ONECARD_LEN_MAX : WOW_PAN_LEN_MAX;// RDD P3 210812 OneCard does not need expiry date
  entry_opts.max = transaction_get_status( &txn, st_electronic_gift_card ) ?WOW_PAN_LEN_MAX-BinLen : entry_opts.max;

  entry_opts.min = transaction_get_status( &txn, st_electronic_gift_card ) ? WOW_PAN_LEN_MAX-BinLen : WOW_PAN_LEN_MIN;

  // RDD 130815 v546-0 SPIRA:8799 Allow 13 digits to be entered and if so pre-pend 1st 6 digits of BIN
  entry_opts.max += 3;
#else		// v549-2
  entry_opts.min = WOW_PAN_LEN_MIN;
  entry_opts.max = WOW_PAN_LEN_MAX;

  vim_strcpy(txn_type, TxnTypePtr);

  if( transaction_get_status( &txn, st_electronic_gift_card ))
  {
      vim_strcpy( txn_type, GiftCardType() );
	  entry_opts.max = (WOW_PAN_LEN_MAX-BinLen)+3;
	  entry_opts.min = WOW_PAN_LEN_MAX-BinLen;
  }
  else if( txn.type == tt_query_card )
  {
	  entry_opts.max = WOW_ONECARD_LEN_MAX;
  }
#endif

  entry_opts.entry_options = KBD_OPT_BEEP_ON_CLR;

  entry_opts.entry_options = 0;
  // RDD 020115 SPIRA:8285 Need to check events for integrated
  entry_opts.CheckEvents = is_integrated_mode() ? VIM_TRUE : VIM_FALSE;

  while(1)
  {
	VIM_SIZE ScreenID = transaction_get_status( &txn, st_electronic_gift_card ) ? DISP_ENTER_EGC : DISP_CARD_NUMBER;

	DBG_MESSAGE( TxnAmtPtr );

#ifndef _WIN32
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, ScreenID, VIM_PCEFTPOS_KEY_CANCEL, txn_type, TxnAmtPtr));
#else
    VIM_ERROR_RETURN_ON_FAIL( display_screen(ScreenID, VIM_PCEFTPOS_KEY_CANCEL, txn_type, TxnAmtPtr));
    VIM_ERROR_RETURN_ON_FAIL( data_entry(entry_buf, &entry_opts));
#endif
    if (txn.type == tt_query_card)
    {
        DisplayProcessing(DISP_PROCESSING_WAIT);
    }

    card_details_ptr->type = card_manual;

	// RDD 020413 - If manual because of e-gift card, append luhn and prepend WW IIN to this data to form pseudo-pan
	if( transaction_get_status( &txn, st_electronic_gift_card ))
	{
		VIM_SIZE Len = vim_strlen( entry_buf );
		DBG_VAR( Len );
		DBG_VAR( BinLen );

		// RDD 130815 v546-0 SPIRA:8799 Allow only 10 or 13 digits to be entered and if so pre-pend 1st 6 digits of BIN
#if 0	// RDD 140815 - apparently WOW want 11 and 12 digitis to report invalid card
		if(( Len != entry_opts.max ) && ( Len != entry_opts.min ))
		{
			vim_kbd_sound_invalid_key();
			continue;
		}
#endif
		// Modify BIN len - eg. should become 6 if 13 digits entered
		BinLen = WOW_PAN_LEN_MAX - Len;

		vim_strncpy( card_details_ptr->data.manual.pan, terminal.ecg_bin, BinLen );
		vim_strcat( card_details_ptr->data.manual.pan, entry_buf );
		card_details_ptr->data.manual.pan_length = WOW_PAN_LEN_MAX;
		DBG_DATA( card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length );

		if( !check_luhn(card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length ))
		{
			vim_kbd_sound_invalid_key();
			ReportLuhnFail();
			vim_mem_clear( card_details_ptr->data.manual.pan, VIM_SIZEOF(card_details_ptr->data.manual.pan) );
			card_details_ptr->data.manual.pan_length = 0;
			continue;
		}
		else
		{
			break;
		}
	}
	else
	{
		card_details_ptr->data.manual.pan_length = vim_strlen(entry_buf);
		vim_mem_copy(card_details_ptr->data.manual.pan, entry_buf, card_details_ptr->data.manual.pan_length);

		if( txn.type == tt_query_card )
		{
			return( CheckLoyaltyCard( card_details_ptr, 0 ) );
			// RDD TODO - Query card checking for OneCard Loyalty
		}

		/* Search Card Table and check general validity of card for this txn type */
		else if(( txn.error = card_name_search( &txn, card_details_ptr )) == VIM_ERROR_NONE )
		{
			// Check the validity a bit more....
			if(( card_details_ptr->data.manual.pan_length < entry_opts.min ) ||
		   ( card_details_ptr->data.manual.pan_length > entry_opts.max ) ||
		   ( !check_luhn(entry_buf, vim_strlen(entry_buf))) )
			{
				vim_kbd_sound_invalid_key();
				ReportLuhnFail();
				continue;
			}
			else
			{
				break;
			}
		}
		// The card search returned some other error
		display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
		vim_kbd_sound_invalid_key();
		return txn.error;
	}
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

#else	// Phase 6 code - had bugs in gift card entry


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_key_PAN
(
  card_info_t *card_details_ptr,
  const VIM_TEXT *TxnTypePtr,
  VIM_CHAR *TxnAmtPtr
)
{
  kbd_options_t entry_opts;
  char entry_buf[50];
  VIM_ERROR_PTR error = VIM_ERROR_NONE;

  VIM_SIZE BinLen = vim_strlen( terminal.ecg_bin );

  vim_mem_clear( entry_buf, VIM_SIZEOF( entry_buf ) );

  entry_opts.timeout = ENTRY_TIMEOUT;

    // RDD 161015 SPIRA:8826 Man PAN allows 23 digits to be entered
#if 0 // code to v549-1

  // RDD P3 210812 15 digit max for manually entered loyalty card
  entry_opts.max = ( txn.type == tt_query_card ) ? WOW_ONECARD_LEN_MAX : WOW_PAN_LEN_MAX;// RDD P3 210812 OneCard does not need expiry date
  entry_opts.max = transaction_get_status( &txn, st_electronic_gift_card ) ?WOW_PAN_LEN_MAX-BinLen : entry_opts.max;

  entry_opts.min = transaction_get_status( &txn, st_electronic_gift_card ) ? WOW_PAN_LEN_MAX-BinLen : WOW_PAN_LEN_MIN;

  // RDD 130815 v546-0 SPIRA:8799 Allow 13 digits to be entered and if so pre-pend 1st 6 digits of BIN
  entry_opts.max += 3;
#else		// v549-2
  entry_opts.min = WOW_PAN_LEN_MIN;
  entry_opts.max = WOW_PAN_LEN_MAX;

  if( transaction_get_status( &txn, st_electronic_gift_card ))
  {
	  // RDD 151116 SPIRA:9089 v561-4 - Max must be WOW_PAN_LEN_MAX-BinLen NOT WOW_PAN_LEN_MAX-BinLen +3
#if 1
	  entry_opts.max = (WOW_PAN_LEN_MAX-BinLen)+3;
#else
	  entry_opts.max = WOW_PAN_LEN_MAX-BinLen;
#endif
	  entry_opts.min = WOW_PAN_LEN_MIN-BinLen;
  }
  else if( txn.type == tt_query_card )
  {
	  entry_opts.max = WOW_ONECARD_LEN_MAX;
  }
#endif

#if 0
  entry_opts.entry_options = 0;
#else
  /* RDD 110616 SPIRA:8990 v560-2 */
  entry_opts.entry_options = KBD_OPT_BEEP_ON_CLR;
#endif

  // RDD 020115 SPIRA:8285 Need to check events for integrated
  entry_opts.CheckEvents = is_integrated_mode() ? VIM_TRUE : VIM_FALSE;


  while(1)
  {
	VIM_SIZE ScreenID = transaction_get_status( &txn, st_electronic_gift_card ) ? DISP_ENTER_EGC : DISP_CARD_NUMBER;

	DBG_MESSAGE( TxnAmtPtr );

	VIM_ERROR_RETURN_ON_FAIL( display_screen( ScreenID, VIM_PCEFTPOS_KEY_CANCEL, TxnTypePtr, TxnAmtPtr ));
#if 0
	VIM_ERROR_RETURN_ON_FAIL( data_entry( entry_buf, &entry_opts ));
#else
	// RDD 250516 - V560 make homogenous
	// RDD 260516 - improved for V560 to make more homogenous
	if(( error = data_entry(entry_buf, &entry_opts)) != VIM_ERROR_NONE )
	{
		display_result( error, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
		VIM_ERROR_RETURN( error );
	}
#endif

	card_details_ptr->type = card_manual;

	// RDD 020413 - If manual because of e-gift card, append luhn and prepend WW IIN to this data to form pseudo-pan
	if( transaction_get_status( &txn, st_electronic_gift_card ))
	{
		VIM_SIZE Len = vim_strlen( entry_buf );
		DBG_VAR( Len );
		DBG_VAR( BinLen );

		vim_strncpy( card_details_ptr->data.manual.pan, terminal.ecg_bin, BinLen );
		vim_strcat( card_details_ptr->data.manual.pan, entry_buf );

		// RDD 151116 SPIRA:8089
		//card_details_ptr->data.manual.pan_length = VIM_MIN( WOW_PAN_LEN_MAX;
		card_details_ptr->data.manual.pan_length = VIM_MIN( WOW_PAN_LEN_MAX, vim_strlen(card_details_ptr->data.manual.pan));
		DBG_DATA( card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length );

		if( !check_luhn(card_details_ptr->data.manual.pan, card_details_ptr->data.manual.pan_length ))
		{
			vim_kbd_sound_invalid_key();
			ReportLuhnFail();
			vim_mem_clear( card_details_ptr->data.manual.pan, VIM_SIZEOF(card_details_ptr->data.manual.pan) );
			card_details_ptr->data.manual.pan_length = 0;
			continue;
		}
		else
		{
			break;
		}
	}
	else
	{
		card_details_ptr->data.manual.pan_length = vim_strlen(entry_buf);
		vim_mem_copy(card_details_ptr->data.manual.pan, entry_buf, card_details_ptr->data.manual.pan_length);

		if( txn.type == tt_query_card )
		{
			return( CheckLoyaltyCard( card_details_ptr, 0 ) );
			// RDD TODO - Query card checking for OneCard Loyalty
		}

		/* Search Card Table and check general validity of card for this txn type */
		else if(( txn.error = card_name_search( &txn, card_details_ptr )) == VIM_ERROR_NONE )
		{
			// Check the validity a bit more....
			if(( card_details_ptr->data.manual.pan_length < entry_opts.min ) ||
		   ( card_details_ptr->data.manual.pan_length > entry_opts.max ) ||
		   ( !check_luhn(entry_buf, vim_strlen(entry_buf))) )
			{
				vim_kbd_sound_invalid_key();
				ReportLuhnFail();
				continue;
			}
			else
			{
				break;
			}
		}
		// The card search returned some other error
		display_result(txn.error, txn.host_rc_asc, "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
		vim_kbd_sound_invalid_key();
		return txn.error;
	}
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

#endif


#if 1
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_confirmation
(
  card_info_t *card_details_ptr,
  const VIM_TEXT *TxnTypePtr,
  VIM_CHAR *TxnAmtPtr
)
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
	VIM_CHAR account_string_short[3+1];
	VIM_KBD_CODE key_pressed;

	if ((txn.type == tt_moto) || (txn.type == tt_moto_refund))
	{
		transaction_set_status(&txn, st_moto_txn);
		return VIM_ERROR_NONE;
	}

	// RDD 021114 needs to be after CCV entry in standalone for MOTO txns
	if( transaction_get_status( &txn, st_moto_txn ))
		return VIM_ERROR_NONE;

	// RDD P3 210812 OneCard does not need confirmation
	if( txn.type == tt_query_card ) return VIM_ERROR_NONE;

	// RDD 050413 egc changes
	if( transaction_get_status( &txn, st_electronic_gift_card )) return VIM_ERROR_NONE;

	get_acc_type_string_short(txn.account, account_string_short);
	GetCardName( &txn, CardName );


	// RDD 241012 SPIRA:6229 Cash on Credit for Scheme debit Cards now not allowed unless full EMV
	if( txn.amount_cash )
	{
		return ERR_NO_CASHOUT;
	}

	// RDD 230212 SPIRA ISSUE 4916 "Manual pan entry should default to credit then press enter to confirm"
	// RDD 061112 SPIRA:6257 - Watch out here because some completion type "K" require manual entry and will have account type supplied...
    if( !transaction_get_status( &txn, st_operator_entry ))
	{
		txn.account = account_credit;
	}

	// RDD 050612 - doesn't appear that we need this step for fuel cards...
	if( fuel_card(&txn) ) return VIM_ERROR_NONE;

	// RDD 190412 : SPIRA5239 Mandatory PIN for bal even if manual entry
	if( txn.type == tt_balance ) return VIM_ERROR_NONE;

	get_txn_type_string(&txn, type, sizeof(type), VIM_FALSE);

	// RDD 040715 Allow customer to cancel txn if they're asked to confirm on PINPAD
	// RDD 240516 SPIRA:8942 - restore POS cancel for integrated

	VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_PRESS_ENTER_TO_CONFIRM, VIM_PCEFTPOS_KEY_CANCEL/*VIM_PCEFTPOS_KEY_NONE*/, TxnTypePtr, TxnAmtPtr, CardName, account_string_short ));

	do
	{
		if(( res = screen_kbd_touch_read_with_abort( &key_pressed, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE )) == VIM_ERROR_NONE )
		{
			// No message on screen if OK pressed !
			if( key_pressed == VIM_KBD_CODE_OK )
				return VIM_ERROR_NONE;

			if(/*( is_standalone_mode() ) &&*/ ( key_pressed == VIM_KBD_CODE_CANCEL ))
			{
				res = VIM_ERROR_USER_CANCEL;
				break;
			}
			else
			{
				vim_kbd_sound_invalid_key();
			}
		}
		else
		{
			break;
		}
	}
	while( res == VIM_ERROR_NONE );
	display_result( res, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
	/* return without error */
	return res;
}

#else

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_confirmation
(
  card_info_t *card_details_ptr,
  const VIM_TEXT *TxnTypePtr,
  VIM_CHAR *TxnAmtPtr
)
{
	VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
	VIM_CHAR account_string_short[3+1];
	VIM_KBD_CODE key_pressed;

	// RDD 021114 needs to be after CCV entry in standalone for MOTO txns
	if(( is_standalone_mode() ) && (( txn.type == tt_moto ) || ( txn.type == tt_moto_refund )))
		return VIM_ERROR_NONE;

	// RDD P3 210812 OneCard does not need confirmation
	if( txn.type == tt_query_card ) return VIM_ERROR_NONE;

	// RDD 050413 egc changes
	if( transaction_get_status( &txn, st_electronic_gift_card )) return VIM_ERROR_NONE;

	get_acc_type_string_short(txn.account, account_string_short);
	GetCardName( &txn, CardName );


	// RDD 241012 SPIRA:6229 Cash on Credit for Scheme debit Cards now not allowed unless full EMV
	if( txn.amount_cash )
	{
		return ERR_NO_CASHOUT;
	}

	// RDD 230212 SPIRA ISSUE 4916 "Manual pan entry should default to credit then press enter to confirm"
	// RDD 061112 SPIRA:6257 - Watch out here because some completion type "K" require manual entry and will have account type supplied...
    if( !transaction_get_status( &txn, st_operator_entry ))
	{
		txn.account = account_credit;
	}
	//

	// RDD 050612 - doesn't appear that we need this step for fuel cards...
	if( fuel_card(&txn) ) return VIM_ERROR_NONE;


	// RDD 190412 : SPIRA5239 Mandatory PIN for bal even if manual entry
	if( txn.type == tt_balance ) return VIM_ERROR_NONE;

	for(;;)
	{
		//int res;
		VIM_CHAR type[20];

		get_txn_type_string(&txn, type, sizeof(type), VIM_FALSE);

		VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_PRESS_ENTER_TO_CONFIRM, VIM_PCEFTPOS_KEY_CANCEL, TxnTypePtr, TxnAmtPtr, CardName, account_string_short ));

#if 1	// RDD 200315 SPIRA:8502 - need to be able to POS cancel here
		/* wait for key (or abort)*/
		VIM_ERROR_RETURN_ON_FAIL( screen_kbd_touch_read_with_abort( &key_pressed, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE ));
#else
		VIM_ERROR_RETURN_ON_FAIL( vim_kbd_read( &key_pressed, ENTRY_TIMEOUT ));
#endif

		if( key_pressed == VIM_KBD_CODE_OK ) break;

		if(( is_standalone_mode() ) && ( key_pressed == VIM_KBD_CODE_CANCEL ))
		{
			txn.error = VIM_ERROR_USER_CANCEL;
			display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
			return txn.error;
		}
		else vim_kbd_sound_invalid_key();
	}
	/* return without error */
	return VIM_ERROR_NONE;
}

#endif


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR card_detail_capture_key_expiry_date
(
    card_info_t *card_details_ptr,
    const VIM_TEXT *TxnTypePtr,
    VIM_CHAR *TxnAmtPtr
)
{
    kbd_options_t entry_opts;
    VIM_CHAR entry_buf[20];

    entry_opts.timeout = ENTRY_TIMEOUT;
    entry_opts.entry_options = KBD_OPT_EXPIRY;

    /* RDD 110616 SPIRA:8990 v560-2 */
    entry_opts.entry_options |= KBD_OPT_BEEP_ON_CLR;

    // RDD 020115 SPIRA:8285 Need to check events for integrated
    entry_opts.CheckEvents = is_integrated_mode() ? VIM_TRUE : VIM_FALSE;

    // RDD P3 210812 OneCard does not need expiry date
    if (txn.type == tt_query_card) return VIM_ERROR_NONE;

    // RDD 040413 - put in a dummy expiry date for electronic gift cards
    if (transaction_get_status(&txn, st_electronic_gift_card))
    {
        card_details_ptr->data.manual.expiry_mm[0] = '1';
        card_details_ptr->data.manual.expiry_mm[1] = '2';
        card_details_ptr->data.manual.expiry_yy[0] = '3';
        card_details_ptr->data.manual.expiry_yy[1] = '0';
        return VIM_ERROR_NONE;
    }

    for (;;)
    {
        int res;
        VIM_ERROR_PTR error = VIM_ERROR_NONE;
        VIM_CHAR type[20];

        get_txn_type_string(&txn, type, sizeof(type), VIM_FALSE);

        vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));
#ifdef _WIN32

        VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_EXPIRY, VIM_PCEFTPOS_KEY_CANCEL, TxnTypePtr, TxnAmtPtr));

        // RDD 260516 - improved for V560 to make more homogenous
        if ((error = data_entry(entry_buf, &entry_opts)) != VIM_ERROR_NONE)
        {
            display_result(error, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
            VIM_ERROR_RETURN(error);
        }
#else

        // RDD 201218 Create a generic function to handle display and data entry in all platforms
        if ((error = display_data_entry(entry_buf, &entry_opts, DISP_EXPIRY, VIM_PCEFTPOS_KEY_CANCEL, TxnTypePtr, TxnAmtPtr)) != VIM_ERROR_NONE)
        {
            display_result(error, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
            VIM_ERROR_RETURN(error);
        }
#endif
        vim_mem_copy(card_details_ptr->data.manual.expiry_mm, entry_buf, 2);
        vim_mem_copy(card_details_ptr->data.manual.expiry_yy, &entry_buf[2], 2);
        res = asc_to_bin(card_details_ptr->data.manual.expiry_mm, 2);

        if ((res > 12) || (res == 0))
        {
            VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_INVALID_DATE, VIM_PCEFTPOS_KEY_NONE, card_details_ptr->data.manual.expiry_mm, card_details_ptr->data.manual.expiry_yy));
            vim_kbd_sound_invalid_key();
            VIM_ERROR_RETURN_ON_FAIL(vim_event_sleep(5000)); // wait 5 secs
            //pceft_pos_display_close();
            continue;
        }

        // RDD 061112 SPIRA:6257 - Watch out here because some completion type "K" don't check expiry because customer has gone off with the goods already
        if ((card_check_expired(&txn.card)) && (!transaction_get_status(&txn, st_operator_entry)))
        {
            //vim_mem_copy(txn.host_rc_asc, "TQ", 2);
            txn.error = ERR_CARD_EXPIRED;
            // Terminal must reprompt from card entry
            card_details_ptr->type = card_none;

            display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
            return txn.error;
        }

        /* return without error */
        return VIM_ERROR_NONE;
    }
}


VIM_ERROR_PTR card_detail_capture_key_entry
(
  card_info_t *card_details_ptr,
  const VIM_TEXT *TxnTypePtr,
  VIM_CHAR *TxnAmtPtr
)
{
  struct
  {
    VIM_ERROR_PTR (*method)
    (
      card_info_t *card_details_ptr,
	  const VIM_TEXT *TxnTypePtr,
	  VIM_CHAR *TxnAmtPtr
    );
  } dialogs[]=
  {
    {card_detail_capture_key_PAN},
    {card_detail_capture_key_expiry_date},
    {card_detail_capture_confirmation},
  };
  VIM_SIZE dialog_index;
  VIM_SIZE dialog_length;
  VIM_BOOL expiry_date_req;


  // RDD 100512 SPIRA:5464,5465 timeout on Manual entry return to card capture state - need to set manual card type even if error
  card_details_ptr->type = card_manual;

  if (card_capture_forced_swipe == txn.card_state)
    return VIM_ERROR_USER_BACK;

  dialog_length=VIM_LENGTH_OF(dialogs);

  /* disable warning on win32 simulator */
  expiry_date_req = ISSUER_OPTION1_EXPIRY_DATE_REQD;

  if ( !expiry_date_req )
  {
    //VIM_
    dialogs[1].method = VIM_NULL;
    dialog_length--;
  }

  /* iterate through each of the manual entry dialogs */
  for(dialog_index=0;dialog_index<dialog_length;)
  {
    VIM_ERROR_PTR error;

    if ( dialogs[dialog_index].method )
    {

      /* run current dialog in the list */
	  DBG_MESSAGE( TxnAmtPtr );

      if(( error=dialogs[dialog_index].method(card_details_ptr,TxnTypePtr,TxnAmtPtr)) == VIM_ERROR_NONE )
		  dialog_index++;

	  // RDD 200313 SPIRA:6608 Clear Screen and restart current data entry state if cancel pressed
      //if(( error==VIM_ERROR_USER_BACK )
      else if(( error==VIM_ERROR_USER_BACK )||( error==VIM_ERROR_USER_CANCEL ))
      {
        /* goto the previous dialog in the list */
		//return VIM_ERROR_USER_CANCEL;
		  if(( error==VIM_ERROR_USER_CANCEL ) )
		  {
			  if( is_integrated_mode() )
			  {
				VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);	//BD allow USER CANCEL to return to Present Card
			  }
			  else
				  return error;	// RDD 091014 - don't need this hack for standalone...
		  }
		  continue;
      }
      else
      {
        /* return the untreated error from the dialog */
        VIM_ERROR_RETURN(error);
      }
    }
    else
      dialog_index++;
  }
  /* return without error */
  return VIM_ERROR_NONE;
}

/* -------------------------------------------------------------------- */
VIM_ERROR_PTR card_extract_customer_name
(
  VIM_CHAR *track1_in,
  VIM_UINT8 track1_length_in,
  VIM_CHAR *customer_name
)
{
  VIM_CHAR *p_cardholder, string_buf[30];
  VIM_UINT8 cardholder_length = 0;
  VIM_UINT8 i;
  VIM_CHAR *p_buf;
  VIM_BOOL  b_exist;

  /*  To find the frist ^ */
  p_cardholder = track1_in;

  for(i=0; i < track1_length_in; i++)
  {
    if( *p_cardholder++ == '^')
     break;
  }

  if(p_cardholder == track1_in)
   return ERR_INVALID_TRACK1;

  /*  To check whether there is CC (country code) before Card holder's name */
  if(*p_cardholder >= '0' && *p_cardholder <= '9')
    p_cardholder += 3;

  /*  To get the length of card holder's name */
  p_buf = p_cardholder;

  while(p_buf < (track1_in+track1_length_in))
  {
    if( *p_buf++ == '^')
      break;
    cardholder_length++;
  }

  /*  To check whether the name is available or not */
  /*  If this field is not used the content will be an space followed by a surname separator (/). */
  if(*p_cardholder == ' ' && *(p_cardholder+1) == '/')
  {
     b_exist = VIM_FALSE;
  }
  else
  {
     b_exist = VIM_TRUE;

     /*  To remove the space in the end of card holder's name */
     p_buf = p_cardholder+cardholder_length-1;

     for(i = cardholder_length ; i > 1 ; i--)
     {
        if(*p_buf == ' ')
        {
          p_buf--;

          cardholder_length--;
        }
        else
           break;
     }
  }

  if(b_exist == VIM_TRUE)
  {
     vim_mem_set(string_buf, ' ', 24);
     string_buf[24] = 0;

     cardholder_length = MIN(cardholder_length, 24);

     vim_mem_copy(string_buf, p_cardholder, cardholder_length);
     vim_mem_copy(customer_name, string_buf, 24);
     return VIM_ERROR_NONE;
  }
  return ERR_INVALID_TRACK1;
}

VIM_ERROR_PTR card_detail_capture_read_track12_data
(
  VIM_MAG_PTR mag_ptr,
  card_info_t * card_details_ptr
)
{
    VIM_MAG_TRACK_SET track_set;

    /* read the track */
    VIM_ERROR_RETURN_ON_FAIL(vim_mag_read_tracks(mag_ptr, &track_set));

  /* ensure that the track 1 data is the correct length */
  //VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(track_set.tracks[TRACK1_POS].length,VIM_SIZEOF(card_details_ptr->data.mag.track1));

  /* ensure that the track 2 data is the correct length */
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(track_set.tracks[TRACK2_POS].length,VIM_SIZEOF(card_details_ptr->data.mag.track2));

  /* ensure that the track 3 data is the correct length  */
  //VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(track_set.tracks[TRACK3_POS].length,VIM_SIZEOF(card_details_ptr->data.mag.track3));

  /* initialize the output structure */
  vim_mem_clear(card_details_ptr,VIM_SIZEOF(*card_details_ptr));

  /* set the card type */
  card_details_ptr->type=card_mag;

  /* set the length of the track2 data */
  card_details_ptr->data.mag.track2_length=track_set.tracks[TRACK2_POS].length;
  /* copy the track 2 data */
  vim_mem_copy( card_details_ptr->data.mag.track2,track_set.tracks[TRACK2_POS].buffer, track_set.tracks[TRACK2_POS].length );

  // RDD 030912 P3: If We have Track3 then use track1 space for it and set the flag that T1 is actually T3
  if( track_set.tracks[TRACK3_POS].length )
  {
	  DBG_MESSAGE(" ####### TRACK3 FOUND ######## " );
	  card_details_ptr->data.mag.track3_present = VIM_TRUE;
	  card_details_ptr->data.mag.track1_length = track_set.tracks[TRACK3_POS].length;
	  vim_mem_copy( card_details_ptr->data.mag.track1, track_set.tracks[TRACK3_POS].buffer,track_set.tracks[TRACK3_POS].length );
	  DBG_VAR( card_details_ptr->data.mag.track1_length );
	  DBG_DATA( card_details_ptr->data.mag.track1, card_details_ptr->data.mag.track1_length );
  }
  else
  {
	  /* set the length of the track1 data */
	  card_details_ptr->data.mag.track3_present = VIM_FALSE;
	  card_details_ptr->data.mag.track1_length=track_set.tracks[TRACK1_POS].length;

	  /* If we have read track1 but not 2 we have a problem */
      if ( track_set.tracks[TRACK1_POS].length )
	  {
		  if( !track_set.tracks[TRACK2_POS].length )
		  {
			return ERR_INVALID_TRACK2;
		  }
		  else
		  {
			/* copy the track 1 data */
	 	    card_details_ptr->data.mag.track1_length = track_set.tracks[TRACK1_POS].length;
			vim_mem_copy(card_details_ptr->data.mag.track1,track_set.tracks[TRACK1_POS].buffer,track_set.tracks[TRACK1_POS].length);
			// RDD 110917 SPIRA:9234 - don't exit with error if error extracting customer name
#if 0
			VIM_ERROR_RETURN_ON_FAIL(card_extract_customer_name(track_set.tracks[TRACK1_POS].buffer, track_set.tracks[TRACK1_POS].length, card_details_ptr->data.mag.customer_name));
#else
			{
				VIM_ERROR_PTR res;
				if(( res = card_extract_customer_name(track_set.tracks[TRACK1_POS].buffer, track_set.tracks[TRACK1_POS].length, card_details_ptr->data.mag.customer_name)) != VIM_ERROR_NONE )
				{
					pceft_debug_test( pceftpos_handle.instance, "SPIRA:9234" );
				}
			}
#endif
		  }
	  }
  }

  /* return without error */
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR card_detail_capture_card_swipe_read( VIM_MAG_PTR mag_ptr, card_info_t *card_details_ptr )
{
    VIM_ERROR_PTR error;

	/* read the track */
    if(( error=card_detail_capture_read_track12_data( mag_ptr,card_details_ptr )) != VIM_ERROR_NONE )
	{
		terminal.stats.card_misreads++;
		terminal.last_swipe_failed = VIM_TRUE;
		// RDD 170412 removed to save on file writes
		//terminal_save();
    }
    else
    {
		terminal.stats.card_good_reads++;
		terminal.last_swipe_failed = VIM_FALSE;
    }
	// RDD 080413 - added for phase4
	terminal.stats.card_read_percentage_fail = (terminal.stats.card_misreads * 100)/(terminal.stats.card_good_reads+terminal.stats.card_misreads);
	return error;
}


// RDD 170513 added as a fix for SPIRA:6729
VIM_ERROR_PTR ValidateTrack2( VIM_CHAR *Track2, VIM_SIZE TrackLen )
{
	VIM_ERROR_PTR res = ERR_INVALID_TRACK2;
	VIM_SIZE count = 0;
	VIM_CHAR *Ptr;

	// Check overall length - needs to be >= Min Pan Len + 4 ( 3 FS + ES )
	if( TrackLen < TRACK2_MIN_LEN ) return res;

	// Check for the first FS
	VIM_ERROR_RETURN_ON_FAIL( vim_strchr( Track2, TRACK2_FS, &Ptr ));

	// If we found the FS then check for expiry date and service code

	// RDD 210813 SPIRA:6815 Fleetcards being rejected by new track2 checking - these have a second '=' so have to account for this.
	//if( vim_strchr( ++Ptr, TRACK2_FS, &Ptr2 ) != VIM_ERROR_NONE )
	if(1)
	{
		while( count < MAX_POST_PAN_DATA )
		{
			if(( !VIM_ISDIGIT( *(Ptr+count)) ) && (*(Ptr+count)) != TRACK2_FS ) break;
			count++;
		}
	}

	if( count < MIN_POST_PAN_DATA ) return res;

	return VIM_ERROR_NONE;
}

/**
 * Extract pan from card details structure into a null terminated string
 */

VIM_ERROR_PTR GetPanLen( VIM_CHAR *pan, VIM_SIZE *pan_len )
{
	VIM_ERROR_PTR res = ERR_INVALID_TRACK2;

	VIM_SIZE len = *pan_len = 0;

    for( len = 0; len <= WOW_PAN_LEN_MAX; len++ )
    {
      if ((pan[len] == '=')||(pan[len] == 0)||(pan[len]<'0')||(pan[len]>'9'))
	  {
		  break;
	  }
    }
	if(( len >= WOW_PAN_LEN_MIN )&&( len <= WOW_PAN_LEN_MAX ))
	{
		res = VIM_ERROR_NONE;
	}
	else if( len > 0)
	{
		pceft_debugf(pceftpos_handle.instance, "PAN LENGTH : %d", len );
		DBG_DATA( pan, len );
		DBG_MESSAGE( "S/W ERROR: INVALID PAN LEN" );
	}
	*pan_len = len;
	VIM_DBG_NUMBER( *pan_len );
	return res;
}


VIM_ERROR_PTR HandleSwipedPan( card_info_t *card_details_ptr )
{
	VIM_SIZE pan_len;

    VIM_ERROR_RETURN_ON_NULL(mag_handle.instance_ptr);
    VIM_ERROR_RETURN_ON_FAIL(card_detail_capture_card_swipe_read(mag_handle.instance_ptr, card_details_ptr));

	//VIM_DBG_DATA( card_details_ptr->data.mag.track2,card_details_ptr->data.mag.track2_length);

	/* after reading, close mag reader */
	close_MAG_reader ();

	// RDD 160513 - SPIRA:6729 Short track data getting through system and being sent up in AS2805
	VIM_ERROR_RETURN_ON_FAIL( ValidateTrack2( card_details_ptr->data.mag.track2, card_details_ptr->data.mag.track2_length ));
	return GetPanLen( card_details_ptr->data.mag.track2, &pan_len );
}
// RDD 060212 - Method to check to see if manual entry is allowed

static VIM_ERROR_PTR set_reason_phone(void)
{
    txn.moto_reason = moto_reason_telephone;
    return VIM_ERROR_USER_BACK;
}

static VIM_ERROR_PTR set_reason_mail(void)
{
    txn.moto_reason = moto_reason_mail_order;
    return VIM_ERROR_USER_BACK;
}



static S_MENU MotoReasonMenu[] = {
    { "Phone Order", set_reason_phone },
    { "Mail Order", set_reason_mail }
};


VIM_ERROR_PTR SetReasonCode(void)
{
    VIM_ERROR_PTR res;
    while ((res = MenuRun(MotoReasonMenu, VIM_LENGTH_OF(MotoReasonMenu), "Select MOTO Reason", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);
    if (res == VIM_ERROR_USER_BACK)
        res = VIM_ERROR_NONE;
    return res;
}



static VIM_ERROR_PTR PosAllowManualEntry( const VIM_TEXT *TxnTypePtr, VIM_CHAR *TxnAmtPtr )
{
	VIM_INT32 Password;
	VIM_PCEFTPOS_KEYMASK key_code;
    VIM_UINT64 moto_password = asc_to_bin(terminal.configuration.moto_password, vim_strlen(terminal.configuration.moto_password));
    //VIM_UINT64 refund_password = asc_to_bin(terminal.configuration.refund_password, vim_strlen(terminal.configuration.refund_password));

	// RDD 180912 MI Says we need POS interaction for manual entry, even for J8 Query Card
	set_operation_initiator(it_pceftpos);

	// Collect the Password
	if( GetPassword( &Password, "Enter Password" ) == VIM_ERROR_NONE )
	{
		if( TERM_ALLOW_MOTO_ECOM && ( Password > 0 ) && ( Password == moto_password )&&(( txn.type == tt_sale )||(txn.type == tt_refund )))     
		{
                //txn.type = tt_moto;
                transaction_set_status(&txn, st_moto_txn);

                VIM_ERROR_RETURN_ON_FAIL( SetReasonCode());
                return VIM_ERROR_NONE;
		}
        
		// Check the password
		if(( Password == terminal.configuration.manpan_password )&&( Password > 0 ))
		{
			// RDD 190412 SPIRA:5239 No manual entry for deposit query card
			if(( txn.type == tt_query_card_deposit ) || ( txn.type == tt_balance ))
			{
				txn.error = ERR_MANUAL_PAN_DISABLED;
				display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
				return txn.error;
			}

			if (IS_STANDALONE)
			{
				// Probably don't need any confirmation for man pan in standalone ...
				return VIM_ERROR_NONE;
			}
			else
			{
				// Prompt the operator to allow or deny manual entry
				display_screen(DISP_WAIT, VIM_PCEFTPOS_KEY_YES | VIM_PCEFTPOS_KEY_NO, TxnTypePtr, TxnAmtPtr);

				// wait for a response from the operator
				VIM_ERROR_RETURN_ON_FAIL(pceft_pos_wait_key(&key_code, ENTRY_TIMEOUT));

				// return TRUE only if YES was pressed by the operator
				if (key_code == VIM_PCEFTPOS_KEY_YES)
				{
					return VIM_ERROR_NONE;
				}
			}
		}

		// RDD 090513 SPIRA:6725 Shortcut to Electronic gift card processing
		else if( Password == WOW_ELECTRONIC_GIFT_CARD )
		{
			transaction_set_status( &txn, st_electronic_gift_card );
			return VIM_ERROR_NONE;
		}

	}

	// Any error do invalid key sound
	vim_kbd_sound_invalid_key();
	return VIM_ERROR_USER_CANCEL;
}


VIM_ERROR_PTR HandleKeyedPan( card_info_t *card_details_ptr )
{
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR simulate_emv_transaction(void)
{
	// Any contact txn ending with 62c returns FAULTY CHIP to simulate technical fallback
	return ( txn.amount_purchase % 100 == 62 ) ? VIM_ERROR_EMV_FAULTY_CHIP : VIM_ERROR_NONE;
}


// RDD 251112 - this is all horrible becuase sometimes we care about the error returned by the kernel (res) and sometimes we need
// what was set in the callback ( txn.error ). Its kind of difficult to sort out which error to use...

VIM_ERROR_PTR HandleEMVContact(void)
{
	VIM_ERROR_PTR res;

	txn.card.type = card_chip;

	// The card has been read now within this transaction, so don't allow pre-insert
	txn.read_icc_count++;
	VIM_DBG_NUMBER(txn.read_icc_count);

	// RDD 190315 - moved to here as we don't want to do this for mag cards
#if 1
   // RDD 200115 SPIRA:8354 EPAL docked needs to be forced to credit for Preauth so can't select epal AIDs
   if( txn.type == tt_preauth )
   {
	   // RDD - need these both set for emv_cb to no allow EPAL dueing App selection
	   txn.account = account_credit;
	   txn.pos_account = VIM_TRUE;
   }
#endif

	// RDD 020113 Allow Real Query Card for NZ J8 as this does not return financial data
	if(( transaction_get_training_mode() ) && ( txn.type != tt_query_card ))
	{
		// Simulate an emv transaction but use dummy data
		res = simulate_emv_transaction();
	}
	// RDD 060714 added for diag dock test
	else if( txn.card_state == card_capture_icc_test )
	{
		VIM_UINT8 atr[1000];
		VIM_CHAR Text[100];
		VIM_SIZE atr_size = 0;
		vim_mem_clear( atr, VIM_SIZEOF(atr) );
		if(( res = vim_icc_power_on( icc_handle.instance_ptr, VIM_FALSE, atr, &atr_size, VIM_SIZEOF( atr ))) == VIM_ERROR_NONE )
		{
			vim_snprintf( Text, VIM_SIZEOF(Text), "%d Interface Bytes Returned from Reset", atr_size );
			pceft_debug( pceftpos_handle.instance, Text );
			VIM_DBG_ERROR( res );
		}
		return res;
	}
	else
	{
		/* Call EMV engine to perform transaction */
		res = emv_perform_transaction();
	}
	VIM_DBG_ERROR(res);
	VIM_DBG_ERROR(txn.error);
	VIM_DBG_ERROR(txn.emv_error);

	//// RDD 140416 TEST V7
	if(( txn.emv_error == VIM_ERROR_POS_CANCEL )||( res == VIM_ERROR_POS_CANCEL ))
		txn.error = VIM_ERROR_POS_CANCEL;

	// RDD 220213 Hacky fix for Z1 receipt after offline Pin timeout within Kernel issue
	// for some reason the txn.error is getting replaced by err_emv_card_declined somewhere in the callbacks...
   // RDD Trello #184 Trello #185 - Some EMV issue is causing receipts not to be printed
	if( txn.emv_error == VIM_ERROR_TIMEOUT )
	{
		txn.error = txn.emv_error;
#if 0
		txn.print_receipt = VIM_FALSE;
#endif
	}

	if(( res == VIM_ERROR_NONE ) && ( transaction_get_status( &txn, st_offline_pin_verified )))
	{
		VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
		VIM_CHAR account_string_short[3+1];

		DBG_MESSAGE( "-------------- DISPLAY OFFLINE PIN RESULT : SUCCESS 2 --------------" );
		vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );
		get_acc_type_string_short(txn.account, account_string_short);
		GetCardName( &txn, CardName );
#if 1
		//VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_PIN_OK, VIM_PCEFTPOS_KEY_NONE, type, AmountString(txn.amount_total), CardName, account_string_short ));
		DBG_MESSAGE("-------------- DISPLAY OFFLINE PIN RESULT : SUCCESS 2 --------------");
		//vim_event_sleep(1000);
#endif
	}

    VIM_DBG_ERROR(res);
    VIM_DBG_ERROR(txn.error);

	if( res == ERR_CARD_EXPIRED_DATE )
	{

		display_screen(DISP_CARD_ERROR_EXPIRED_DATE, VIM_PCEFTPOS_KEY_NONE );
		vim_kbd_sound_invalid_key ();
		VIM_ERROR_RETURN_ON_FAIL( vim_event_sleep( 2000 ));
	}
	// RDD: EMV kernal remaps CARD REMOVED to a generic kernel error but the actual error is stored in the global
	// RDD 121212 Added for SPIRA:6437 v305 CR
	// RDD 251112 SPIRA:6380 txn.error for POS cancel needs to override what is returned by the kernel for most situations
	// RDD 020113 SPIRA:6480 Also need to check for ERR_SIGNATURE_APPROVED_OFFLINE

	if(( txn.error == VIM_ERROR_EMV_CARD_REMOVED )||
		( txn.error == VIM_ERROR_TIMEOUT )||
		( txn.error == VIM_ERROR_POS_CANCEL )||
		( txn.error == VIM_ERROR_USER_CANCEL )||
		( txn.error == ERR_NO_CASHOUT )||
		( txn.error == ERR_CPAT_NO_DOCK )||
		( txn.error == ERR_SIGNATURE_APPROVED_OFFLINE ))
	{
		res = txn.error;
	}

	// RDD 060412 - SPIRA 5231 Fix. And yes this is all a horrible mess and needs to be rationalised but too dangerous to do so right now...
	if (( res == VIM_ERROR_EMV_BAD_CARD_RESET )||( res == VIM_ERROR_EMV_FAULTY_CHIP ))
	{
		txn.error = res;
	}

    //VIM_DBG_ERROR(txn.error);
	return res;
}


VIM_ERROR_PTR HandleEMVContactless(void)
{
	/* Call EMV engine to perform transaction */
	VIM_ERROR_PTR res;

	VIM_TEXT debug_buffer[310];
	//VIM_TEXT text_buffer[310];
	//VIM_SIZE debug_size=256;
    VIM_PICC_EMV_STATUS status;
	vim_mem_clear( debug_buffer, VIM_SIZEOF(debug_buffer));
	//debug_size = VIM_SIZEOF(debug_buffer) -1;

	// RDD 070812 - ensure that this is NOT set for contactless by default else will prompt sig
	txn.pin_bypassed = VIM_FALSE;

	// RDD 180712 - moved PICC Z debug from above. Don't send debug unless txn failed for some reason

    vim_picc_emv_get_transaction_status(picc_handle.instance, &status);

    // RDD 231018 Fix Z debug so sends if some error
    if (((res = txn_picc()) != VIM_ERROR_NONE)||(( status != VIM_PICC_EMV_STATUS_APPROVED )&&( status != VIM_PICC_EMV_STATUS_AUTHORIZATION_REQUEST)))
    {
        picc_debug_errors();
    }
	// RDD 010813 - report collisions and tap errors here

    VIM_DBG_ERROR(res);
    VIM_DBG_ERROR(txn.error);
	//DBG_VAR(txn.card.data.picc.track2_length);
	//DBG_DATA( txn.card.data.picc.track2, txn.card.data.picc.track2_length );
	return res;
}


/*----------------------------------------------------------------------------*/


static VIM_BOOL default_event = VIM_FALSE;



static VIM_ERROR_PTR FlushHandles( 	VIM_EVENT_FLAG_PTR *event_list )
{
	//VIM_DBG_MESSAGE("Flush Event Handles")

	event_list[KBD_PTR]=&default_event;
  event_list[POS_PTR]=&default_event;
  event_list[MAG_PTR] = &default_event;
  event_list[PICCA_PTR] = &default_event;


  //////////////// RDD TEST TEST TEST ////////////////////
  event_list[PICC_PTR]=&default_event;

  if(( txn.type == tt_query_card ) && ( !WOW_NZ_PINPAD ) && ( txn.card_state != card_capture_new_gift ))
  {
      vim_mag_destroy(mag_handle.instance_ptr);
  }
  else
  {
      if (mag_handle.instance_ptr == VIM_NULL)
      {
          VIM_DBG_WARNING("---- Mag New ----");
          VIM_ERROR_IGNORE(vim_mag_new(&mag_handle));
      }
      /* make sure the mag device is flushed and there are no stray track2's contained within */
      VIM_ERROR_IGNORE(vim_mag_flush(mag_handle.instance_ptr));
      // Setup valid addresses for each event pointer by default. Except for Keyboard which is set to VIM_NULL as default
      VIM_ERROR_RETURN_ON_FAIL(vim_mag_get_swiped_flag(mag_handle.instance_ptr, &event_list[MAG_PTR]));

      if (icc_handle.instance_ptr == VIM_NULL)
      {
          VIM_ERROR_IGNORE(vim_icc_new(&icc_handle, VIM_ICC_SLOT_CUSTOMER));
      }
      VIM_ERROR_RETURN_ON_FAIL(vim_icc_get_presence_flag(icc_handle.instance_ptr, &event_list[ICC_PTR]));
  }

  // Flush the kbd buffer
  vim_kbd_flush();



  VIM_ERROR_RETURN_ON_FAIL( vim_kbd_get_pressed_flag( &event_list[KBD_PTR] ));

  if( is_integrated_mode() )
  {
    DBG_POINT;
      VIM_ERROR_IGNORE( vim_pceftpos_get_message_received_flag( pceftpos_handle.instance, &event_list[POS_PTR] ));
  }
  return VIM_ERROR_NONE;
}

static VIM_BOOL ctls_enabled( card_capture_state_t *capture_state )
{
	// We HAVE to come into the card capture with "standard" as everything else precludes CTLS
	if( *capture_state < card_capture_standard )
	{
		VIM_DBG_WARNING( " --------------- Contactless Disabled previously ------------------" );
		return VIM_FALSE;
	}

	if( *capture_state == card_capture_picc_test )
	{
		return VIM_TRUE;
	}

	if( !(TERM_CONTACTLESS_ENABLE) )
	{
		*capture_state = card_capture_no_picc_reader;
		VIM_DBG_WARNING( "----------------- Contactless Disabled externally -----------------------" );
		return VIM_FALSE;
	}

	// from WOW spec ( RDD 260222 - disable Preauth CTLLS for now as requires re-config of driver )
	if((( txn.type != tt_sale_cash ) && ( txn.type != tt_cashout ) && ( txn.type != tt_sale ) && ( txn.type != tt_refund )&&( txn.type != tt_query_card) && ( txn.type != tt_preauth )) ||( txn.amount_total >= CONTACTLESS_READER_LIMIT ))
	//if((( txn.type != tt_sale_cash ) && ( txn.type != tt_cashout ) && ( txn.type != tt_sale )&&( txn.type != tt_refund )&&( txn.type != tt_query_card)) ||( txn.amount_total >= CONTACTLESS_READER_LIMIT ))
	{
		*capture_state = card_capture_picc_not_allowed;
		VIM_DBG_WARNING( "------------------- Contactless - Invalid txn type ----------------------" );
	    return VIM_FALSE;
	}

	if(( txn.amount_cash ) && ( !TERM_CTLS_CASHOUT ))
	{
		*capture_state = card_capture_picc_not_allowed;
		DBG_MESSAGE( "CTLS Cash but Not enabled" );
		return VIM_FALSE;
	}

	// RDD 191217 Cashout on CTLS Controlled by LFD Parameter



	// RDD 240215 SPIRA:8428 They want this restriction removed now as the Aus Switch supports ctls refund
#if 0
	// RDD 131212 SPIRA:6451 last minute CR for v305: NZ contactless Refunds Not Allowed
	if(( WOW_NZ_PINPAD ) && ( txn.type == tt_refund ))
	{
		//VIM_DBG_MESSAGE( "Contactless - No NZ refund" );
		*capture_state = card_capture_picc_not_allowed;
	    return VIM_FALSE;
	}
#endif

#if 0
	if( txn.uEventDeltas.Fields.Prompt )
	{
		picc_close( VIM_FALSE );
		//VIM_DBG_MESSAGE( "Already Presented" );
	}
#endif
	return VIM_TRUE;
}


static VIM_ERROR_PTR ctls_reupdate_parameters( VIM_BOOL SafFull )
{
	VIM_SIZE index=0;
	VIM_UINT8 TCC[3]={0};
	VIM_SIZE floor_limit=0;
	VIM_EMV_AID CTLS_AID;

	/* iterate CTLS AID table */
	DBG_MESSAGE( "+++++++++++ RE-UPDATE CTLS PARAMETERS +++++++++++++" );

	/* floor limit */
	DBG_MESSAGE( "PICC SET FLOOR LIMIT..." );
	if( SafFull )
	{
		DBG_MESSAGE( "->->-> SAF FULL !! PICC FORCE ONLINE <-<-<-" );
        floor_limit = 0;
	}
	else
	{
		DBG_MESSAGE( "->->-> SAF NO LONGER FULL !! RESTORE FLOOR LIMIT <-<-<-" );
	    floor_limit = 2000;		// RDD 200615 - Just needs to be non-zero actual value isn't used
	}

      /* update reader */
      vim_mem_clear( &CTLS_AID, VIM_SIZEOF(CTLS_AID));
      vim_mem_copy( &CTLS_AID.aid, terminal.emv_applicationIDs[index].emv_aid, terminal.emv_applicationIDs[index].aid_length );
      CTLS_AID.aid_size = terminal.emv_applicationIDs[index].aid_length;
      VIM_ERROR_RETURN_ON_FAIL( vim_picc_emv_update_TCC_and_Floor_limit (picc_handle.instance, &CTLS_AID, TCC, floor_limit));

	  return VIM_ERROR_NONE;
}

#if 0
static void contactless_set_required_TLV(void)
{
  VIM_UINT8 source[128]={0};
  VIM_SIZE source_size=0;
  if( picc_handle.instance != VIM_NULL ){
    source_size = 18;
    vim_mem_copy( source, "\xE0\x07\xA0\x00\x00\x00\x04\x10\x10\x00\x01\x01\x9F\x1D\x03\x6C\xF0\x00", source_size );
    if( vim_picc_emv_set_required_TLV (picc_handle.instance, source, source_size) != VIM_ERROR_NONE ){
      //VIM_DBG_MESSAGE( "======== set 9F1D to A0000000041010 failed !!========" );
    }

    vim_mem_copy( source, "\xE0\x07\xA0\x00\x00\x00\x04\x30\x60\x00\x01\x01\x9F\x1D\x03\x6C\xF0\x00", source_size );
    if( vim_picc_emv_set_required_TLV (picc_handle.instance, source, source_size) != VIM_ERROR_NONE ){
      //VIM_DBG_MESSAGE( "======== set 9F1D to A0000000043060 failed !!========" );
    }
  }
}

#else

#if 0
static void contactless_set_required_TLV(void)
{
  VIM_UINT8 source[128]={0};
  VIM_SIZE source_size=0;
  VIM_BOOL Set;
  if( picc_handle.instance != VIM_NULL )
  {
	  source_size = 18;
	  //VIM_DBG_MESSAGE( "Set Paypass3 Terminal Risk Management Data using <Set Specific Kernel Tags> command" );

	  vim_mem_copy( source, "\x80\x07\xA0\x00\x00\x00\x04\x10\x10\x00\x01\x01\x9F\x1D\x03\x6C\xF0\x00", 18 );

      if( vim_picc_emv_set_kernel_tags (picc_handle.instance, source, source_size) != VIM_ERROR_NONE )
	  {
		//VIM_DBG_MESSAGE( "======== set 9F1D to A0000000041010 failed !!========" );
	  }
	  else
	  {
		  //VIM_DBG_MESSAGE( "======== set 9F1D to A0000000041010 failed !!========" );
		  Set = VIM_TRUE;
	  }
  }
}
#endif

#if 0
static void contactless_set_required_TLV(void)
{
  VIM_UINT8 source[128]={0};
  VIM_SIZE source_size=18;
  VIM_SIZE index;

  if( picc_handle.instance != VIM_NULL )
  {

	  VIM_UINT8 tag_data_len = VIM_SIZEOF( terminal.emv_applicationIDs[index].RMP_9F1D );
	  source_size = 18;
	  vim_mem_copy( source, "\xE0\x07\xA0\x00\x00\x00\x04\x10\x10\x00\x01\x01\x9F\x1D\x03\x6C\xF0\x00", source_size );

	  for( index = 0; index<MAX_EMV_APPLICATION_ID_SIZE; index++ )
	  {
		  if( !terminal.emv_applicationIDs[index].contactless )
			  continue;
		  if( IS_MASTERCARD(index) == VIM_ERROR_NONE )
		  {
			  vim_mem_copy( &source[source_size-tag_data_len], terminal.emv_applicationIDs[index].RMP_9F1D, tag_data_len );
			  break;
		  }
	  }
	  if( vim_picc_emv_set_required_TLV (picc_handle.instance, source, source_size) != VIM_ERROR_NONE )
	  {
		//VIM_DBG_MESSAGE( "======== set 9F1D to A0000000041010 failed !!========" );
	  }
  }
}
#endif
#endif


VIM_BOOL AttemptMCR(void)
{

    transaction_clear_status(&txn, st_mcr_active);
    
    if ( txn.type != tt_sale && txn.type != tt_refund && txn.type != tt_sale_cash && txn.type != tt_cashout )
        return VIM_FALSE;

    // First attempt resulted in blacklisted card so try again without routing
    if (txn.error_backup == ERR_MCR_BLACKLISTED_BIN)
    {
        pceft_debug(pceftpos_handle.instance, "Retry with MCR disabled due to MCR blacklist on first try");
        return VIM_FALSE;
    }
    if (TERM_USE_MERCHANT_ROUTING == VIM_FALSE)
    {
        return VIM_FALSE;
    }

    if(( txn.amount_total < VIM_MIN(100*TERM_MCR_MC_LOW, 100*TERM_MCR_VISA_LOW )))
        return VIM_FALSE;
    if(( txn.amount_total > VIM_MAX(100*TERM_MCR_MC_HIGH, 100*TERM_MCR_VISA_HIGH )))
        return VIM_FALSE;

#ifdef _WIN32
    //transaction_set_status(&txn, st_mcr_active);
#endif

    return VIM_TRUE;
}



static VIM_ERROR_PTR SetupCtls( card_capture_state_t *capture_state_ptr, VIM_EVENT_FLAG_PTR* picc_event_ptr_ptr, VIM_EVENT_FLAG_PTR *picc_activity_event_ptr_ptr )
{
  VIM_ERROR_PTR ret = VIM_ERROR_NONE;
  VIM_UINT64 ARQC_Amount = terminal.configuration.FastEmvAmount ? terminal.configuration.FastEmvAmount : txn.amount_total;
  VIM_BOOL MCRActive = AttemptMCR();
  VIM_BCD emv_txn_type = 0x00;
  VIM_UINT8 txn_type = 0;
  VIM_UINT16 txn_curr = 0;

  //VIM_DBG_MESSAGE("----------------------- SETUP CTLS --------------------");
  VIM_DBG_VAR(MCRActive);

  if(( WOW_NZ_PINPAD ) && ( txn.type == tt_query_card ))
  {
	  VIM_DBG_WARNING("------ NZ Onecard ------");
	  *capture_state_ptr = card_capture_forced_swipe;
      VIM_ERROR_RETURN(VIM_ERROR_NONE);
  }

  emv_get_txn_type(txn.type, &emv_txn_type);
/*
  if (terminal_get_status(ss_wow_basic_test_mode))
	  pceft_debug(pceftpos_handle.instance, "TXN Setup CTLS took txn type");
*/

  // RDD 040913 - convert to binary, will be converted back later !
  txn_type = (VIM_UINT8)bcd_to_bin(&emv_txn_type, 2);

  // Check that the capture state wasn't set to disable contactless already
  VIM_DBG_NUMBER( *capture_state_ptr );

  if(*capture_state_ptr >= card_capture_standard)
  {
	  DBG_POINT;
	  // Check to see if the transaction is allowed to be contactless
	  if( ctls_enabled( capture_state_ptr ) )
	  {
		  DBG_POINT;
		  /*if (terminal_get_status(ss_wow_basic_test_mode))
			  pceft_debug(pceftpos_handle.instance, "CTLS Enabled check pass - ready to open");
		 */
		  // Check to see that the contactless reader is in an initialised state
		  if( !terminal_get_status(ss_wow_reload_ctls_params) )
		  {
			  DBG_POINT;

			  // This Txn is OK for CTLS so if the reader was previously closed for some reason then re-open it
			  if( !terminal_get_status(ss_ctls_reader_open) )
			  {

				  DBG_POINT;
				  if(( ret = picc_open(PICC_OPEN_ONLY,VIM_FALSE)) != VIM_ERROR_NONE ) // MGC 06022020 - It seems someone modified picc_open without modifying the way it is called
				  {
					  /*
					  if (terminal_get_status(ss_wow_basic_test_mode))
						  pceft_debug_error( pceftpos_handle.instance, "CTLS ERROR: re-open reader failed", ret );
					 */
					  *capture_state_ptr = card_capture_no_picc_reader;
					  DBG_POINT;
					  picc_close(VIM_TRUE);
					  return ret;
				  }
				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug(pceftpos_handle.instance, "CTLS Open");
				  */
			  }
			  // we can talk to the reader now so update the reader floor limits if necessary
#if 0
			  if(1)		// Always update the floor limit
#else
			  if( terminal_get_status(ss_reset_ctls_floor_limits) == VIM_TRUE )
#endif
			  {
				  DBG_POINT;
				  VIM_BOOL saf_full = terminal_get_status(ss_saf_full);

				  // RDD 300322 - only allow offline for Sales and only if SAF is not full.
				  VIM_BOOL OfflineApprovalForbidden = (txn.type == tt_sale) ? VIM_FALSE : VIM_TRUE;
				  OfflineApprovalForbidden |= saf_full;

				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug(pceftpos_handle.instance, "CTLS Reupdate Params required - ss_reset_ctls_floor_limits");
				  */
				  VIM_ERROR_RETURN_ON_FAIL( ctls_reupdate_parameters( terminal_get_status(OfflineApprovalForbidden) ) );
				  terminal_clear_status( ss_reset_ctls_floor_limits );
				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug(pceftpos_handle.instance, "CTLS Reupdate Params DONE");
				  */
			  }
			  DBG_POINT;
#if 0
			  //JQ added for 9F1D issue
			  //contactless_set_required_TLV();
			  if(( txn.type == tt_query_card ) && ( txn.card_state == card_capture_picc_apdu ))
			  {
				  //picc_close(VIM_TRUE);
				  vim_picc_emv_setup_vas( vas_handle.instance, tt_query_card );
				  //ret = vim_picc_emv_open_apdu_interface_mode( picc_handle.instance, (VIM_UINT8)txn.type, terminal.configuration.PollResponse, terminal.configuration.PollRespLen );
			  }
#endif
			  // Now send the trasnaction data to the reader
			  /*else*/
              //txn.read_icc_count += 1;
			  // MGC 07022020 - Take this out once we solve JIRA 806
			  txn_curr = (VIM_UINT16)(bcd_to_bin((VIM_UINT8 *)terminal.emv_default_configuration.txn_reference_currency_code, 4));
			  /*
			  if (terminal_get_status(ss_wow_basic_test_mode))
			  {
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire Amount: %ld", ARQC_Amount);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire Cash: %ld", txn.amount_cash);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire Stan: %ld", terminal.stan);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire TXN Ref Currency Code: %d", txn_curr);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire TXN Ref Currency Exp: %d", terminal.emv_default_configuration.txn_reference_currency_exponent);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire TXN Type: %d", txn_type);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire Time: %02d:%02d:%02d", txn.time.hour, txn.time.minute, txn.time.second);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
				  vim_mem_clear(tempCTLS, VIM_SIZEOF(tempCTLS));
				  vim_sprintf(tempCTLS, "CTLS Acquire MCR: %d", TERM_USE_MERCHANT_ROUTING);
				  pceft_debug(pceftpos_handle.instance, tempCTLS);
			  }
			  */
              VIM_DBG_PRINTF1( "!!!!!!!!!!!!!! MCR: %d", TERM_USE_MERCHANT_ROUTING );
              
			  // Some problem with converting PA type
			  if (txn.type == tt_preauth)
				  txn_type = 0x30;

                if ((ret = vim_picc_emv_acquire_transaction(picc_handle.instance,
				  ARQC_Amount,
				  txn.amount_cash,
				  terminal.stan,
				  txn_curr,
				  terminal.emv_default_configuration.txn_reference_currency_exponent,
				  // RDD 130318 JIRA:178
				  txn_type,
				  &txn.time, MCRActive)) != VIM_ERROR_NONE)
			  {

				  DBG_POINT;
				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug_error( pceftpos_handle.instance, "CTLS ERROR: vim_picc_acquire_transaction", ret );
				  */
                  vim_strcpy(txn.cnp_error, "FDA");

				  *capture_state_ptr = card_capture_no_picc;
				  // RDD 191112 Close Reader
				  DBG_POINT;
				  picc_close(VIM_TRUE);
			  }
			  else
			  {

				  DBG_POINT;
				  // RDD 010714 SPIRA:7909 Problems caused by failed cancel cmd
				  terminal_clear_status( ss_picc_response_received );
                  VIM_DBG_WARNING("------- Set Activity Flag for Refer to Phone -----");
				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug(pceftpos_handle.instance, "CTLS Acquire trasanction performed");
				  */
                  VIM_ERROR_RETURN_ON_FAIL(vim_picc_emv_get_activity_flag(picc_handle.instance, picc_activity_event_ptr_ptr ));

				  /*
				  if (terminal_get_status(ss_wow_basic_test_mode))
					  pceft_debug(pceftpos_handle.instance, "CTLS get activity flag performed");
				  */
			  }

			  /* wait a little while to let cless txn started */
			  //vim_event_sleep(100);
			  //vim_event_sleep(0);

			  /* get polling flag */

#if 0           // RDD 010819 - returns NULL and can cause PinPad to crash
			  if(( ret = vim_picc_emv_get_polling_flag(picc_handle.instance, picc_event_ptr_ptr)) != VIM_ERROR_NONE )
			  {
				  vim_pceftpos_debug_error( pceftpos_handle.instance, "CTLS ERROR: vim_picc_emv_get_polling_flag", ret );
				  *capture_state_ptr = card_capture_no_picc;
			  }
#endif

		  }
		  else
		  {
		  /*
		  if (terminal_get_status(ss_wow_basic_test_mode))
			  pceft_debug( pceftpos_handle.instance, "CTLS ERROR: parameters not loaded" );
		  */

			  // RDD 110413 - poos fix for SPIRA:6657
			  //*capture_state_ptr = card_capture_no_picc_reader;
              // Fallback to dock as CTLS init pending
              vim_strcpy(txn.cnp_error, "FDI");

			  *capture_state_ptr = card_capture_no_picc;
		  }
	  }
  }


  else if( *capture_state_ptr < card_capture_standard )
  {
	  //JQ added for 9F1D issue
	  //contactless_set_required_TLV();
	  /*
	  if (terminal_get_status(ss_wow_basic_test_mode))
		  pceft_debug(pceftpos_handle.instance, "CTLS Capture state not standard");
	  */
	  if(( txn.type == tt_query_card ) && ( txn.card_state == card_capture_picc_apdu ))
	  {
		  //picc_close(VIM_TRUE);
          if ( !WOW_NZ_PINPAD )
          {
			  //VIM_DBG_MESSAGE( "---------- Create VAS Instance ---------" );
			  //VIM_ERROR_RETURN_ON_FAIL(vim_vas_open(&vas_handle))
			  //vim_picc_emv_setup_vas(vas_handle.instance, tt_query_card);
#ifdef _WIN32

#else
			  //vim_vas_set_callback(&vas_error_call_back);
#endif
              //pceft_debug_test(pceftpos_handle.instance, "Using vim_vas");

          }
	  }
	  else
	  {
		  DBG_VAR( *capture_state_ptr );
		  // RDD 070416 V560 Shut down contactless here
		  //picc_close(VIM_TRUE);
		  picc_close(VIM_FALSE);
	  }
  }
  //VIM_DBG_MESSAGE( "Exit SetupCTLS" );
  /*
  if (terminal_get_status(ss_wow_basic_test_mode))
	  pceft_debug(pceftpos_handle.instance, "CTLS Setup EXIT");
  */
  return ret;
}


static VIM_ERROR_PTR ProcessPICCActivity(void)
{
    VIM_SIZE event_id;
    VIM_DBG_WARNING("PAPAPAPAPAPAPA picc activity event PAPAPAPAPAPAPAPA");
    VIM_ERROR_RETURN_ON_FAIL(vim_picc_emv_read_event_id( picc_handle.instance, &event_id ));

    switch (event_id)
    {
        case VIM_PICC_EMV_CALLBAACK_EVENT_READ_ERROR:
            vim_kbd_sound_invalid_key();
            vim_adkgui_request_to_update( "Label", "Error, Tap Again", "", "" );
            vim_event_sleep(1000);
            vim_adkgui_request_to_update("Label", "Present Card", "", "");
            /* increment picc error count */
            //txn.read_icc_count += 0x10;
            break;

        case VIM_PICC_EMV_CALLBAACK_EVENT_PRESENT_ONE_CARD:
            vim_kbd_sound_invalid_key();
            vim_adkgui_request_to_update("Label", "Present One Card", "", "");
            vim_event_sleep(1000);
            vim_adkgui_request_to_update("Label", "Present Card", "", "");
            break;

        case VIM_PICC_EMV_CALLBAACK_EVENT_REFER_TO_PHONE:
            if (txn.read_icc_count > 1)
            {
                VIM_DBG_NUMBER( txn.read_icc_count );
                VIM_DBG_WARNING( "PPPPPPPPPPP 2nd Call - Bypass Refer to Phone PPPPPPPPPPPPP" );
            }
            else
            {
                vim_kbd_sound_invalid_key();
                vim_adkgui_request_to_update("Label", "Refer to phone", "Label2", "");
                vim_event_sleep(5000);
                return VIM_ERROR_PICC_EMV_AUTHENTICATION_FAILURE;
            }
            break;

        default:
            VIM_DBG_WARNING("EEEEEEEEEEEE UNKNOWN PICC EVENT EEEEEEEEEEE");
            break;

    }
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR HandleEvent( VIM_EVENT_FLAG_PTR *event_list, card_info_t *card_details_ptr )
{
	// Contactless Event Detected
	if(event_list[PICC_PTR] != VIM_NULL && (*event_list[PICC_PTR]) )
	{
		DBG_MESSAGE( "<<<<<<<<< PICC EVENT DETECTED >>>>>>>>>>>" );
#if 0	// RDD 070213 - no real need to do this I think especially as it seems to take hundreds of ms to close flush the reader.
		close_MAG_reader();
#endif

        // old VAS code
		if( txn.type == tt_query_card )
		{
			VIM_CHAR LoyaltyData[128];
			vim_mem_clear( LoyaltyData, VIM_SIZEOF( LoyaltyData ));
			VIM_DBG_SEND_TARGETED_UDP_LOGS("card_read.c: Before vim_vas_read_payload");
			//vim_vas_read_payload(LoyaltyData, VIM_SIZEOF(LoyaltyData), &txn.session_type);
			VIM_DBG_SEND_TARGETED_UDP_LOGS("card_read.c: After vim_vas_read_payload");
			if( vim_strlen( LoyaltyData ))
			{
                if (!(TERM_NEW_VAS))
                {
                    //vim_vas_close();
                }
				SaveLoyaltyNumber( LoyaltyData,  vim_strlen(LoyaltyData), VIM_ICC_SESSION_UNKNOWN);
				VIM_DBG_SEND_TARGETED_UDP_LOGS("card_read.c: SaveLoyaltyNumber");

				return ERR_LOYALTY_CARD_SUCESSFUL;
			}
			else
			{
				VIM_ERROR_RETURN( VIM_ERROR_MISMATCH );
			}
		}
		else
		{
			txn.card.type = card_picc;
			return HandleEMVContactless();
		}
	}

	// RDD kill the reader thread if another interface has been chosen
	picc_close(VIM_FALSE); //Just cancel TXN, no need to kill the thread

	// Mag Swipe Event Detected
	if( event_list[MAG_PTR] != VIM_NULL && (*event_list[MAG_PTR]) )
	{
		DBG_MESSAGE( "<<<<<<<<< MAG EVENT DETECTED >>>>>>>>>>>" );
		txn.card.type = card_mag;
		return HandleSwipedPan(card_details_ptr);
	}
	close_MAG_reader();

	// Insert Event Detected
	if ( event_list[ICC_PTR] != VIM_NULL && (*event_list[ICC_PTR]))
	{
		DBG_MESSAGE( "<<<<<<<<< INSERT EVENT DETECTED >>>>>>>>>>>" );
		return HandleEMVContact();
	}

	// Keyboard Event Detected
	if (event_list[KBD_PTR] != VIM_NULL && (*event_list[KBD_PTR]))
	{
		return HandleKeyedPan(card_details_ptr);
	}
	// RDD 050912 P3: Add Interrupt facility for Preswipe
	// RDD 230516 SPIRA:8939 Cycle Logo POS Cancel needs to be supported
	if(( event_list[POS_PTR] != VIM_NULL ) && (*event_list[POS_PTR]))
	{
		DBG_MESSAGE( "<<<<<<<<< POS EVENT DETECTED >>>>>>>>>>>" );
        // RDD 271020 - demo code - Barcode from Key Input data during card processing
        if (txn.card_state == card_capture_new_gift)
        {
            VIM_CHAR Barcode[PCEFT_CARD_DATA_LEN +1];
            vim_mem_clear(Barcode, VIM_SIZEOF(Barcode));
            vim_pceftpos_get_key_input( pceftpos_handle.instance, (VIM_UINT8 *)Barcode );
			pceft_debugf_test(pceftpos_handle.instance, "Barcode:%s", Barcode);
            VIM_ERROR_RETURN_ON_FAIL( QC_ProcessBarcode(&txn, Barcode ));
            return VIM_ERROR_NONE;
        }
		VIM_DBG_MESSAGE("POS Event with txn.card_state != card_capture_new_gift");
		terminal.abort_transaction = VIM_TRUE;
		if( txn.type == tt_query_card )
		{
			txn.error = VIM_ERROR_NOT_FOUND;
		}
		else
		{
			txn.error = VIM_ERROR_POS_CANCEL;
		}
		return txn.error;
	}
	DBG_MESSAGE( "<<<<<<<<< UNKNOWN EVENT DETECTED >>>>>>>>>>>" );
	pceft_debug( pceftpos_handle.instance, "User Cancel");
	return VIM_ERROR_POS_CANCEL;
}

VIM_BOOL UserCancelAllowed( card_capture_state_t capture_state )
{
	if (IS_STANDALONE)
		return VIM_TRUE;

	if( terminal.initator == it_pinpad )
	{
		DBG_POINT;
		// RDD 310714 added for ALH
		if( txn.type == tt_query_card_get_token )
			return VIM_TRUE;

		if(( capture_state == card_capture_picc_test )||( capture_state == card_capture_icc_test ))
			return VIM_TRUE;

		if( capture_state == card_capture_forced_icc )
			return VIM_TRUE;
	}
	return VIM_FALSE;
}


VIM_ERROR_PTR HandleKeyedEntry( VIM_KBD_CODE key, card_info_t *card_details_ptr, VIM_TEXT const *header_ptr, card_capture_state_t capture_state )
{
	switch( key )
	{
		case VIM_KBD_CODE_CANCEL:
			// RDD 230112: SPIRA BUG 4839 - Token lookup initiated from PinPan (fn 1111) must support cancel...
			if( UserCancelAllowed( capture_state ))
			{
				txn.error = VIM_ERROR_USER_CANCEL;
				// RDD 120115 SPIRA:8333 May need to cancel here to prevent problems later
				picc_close(VIM_FALSE);
				VIM_ERROR_RETURN( txn.error );
			}
			else
				return VIM_ERROR_NOT_FOUND;

			break;

			// RDD 191014 SPIRA:8185 Do not allow manual entry if theres a cashout componant
		case VIM_KBD_CODE_HASH:
		case VIM_KBD_CODE_FUNC:

            if (transaction_get_status(&txn, st_giftcard_s_type))
            {
                return VIM_ERROR_NOT_FOUND;
            }

			// RDD 151015 SPIRA:8827 if forced swipe don't allow #9999


            // RDD 150720 Trello #104 - need kbd support in NZ for OneCard manual entry
			if(( capture_state == card_capture_forced_swipe )&&( !WOW_NZ_PINPAD))
					  return VIM_ERROR_NOT_FOUND;

			// RDD 150615 SPIRA:8757 Ctls last txn return Error after Man PAN requires close
			picc_close(VIM_FALSE);

			//. RDD 010322 JIRA PIRPIN-1450
			if(/* is_integrated_mode() */1)
			{
				if( PosAllowManualEntry( header_ptr, AmountString(txn.amount_total)) != VIM_ERROR_NONE )
				{
					if( txn.error == ERR_MANUAL_PAN_DISABLED )
						return txn.error;
					if( txn.type == tt_query_card )
						set_operation_initiator(it_pinpad);
					//return VIM_ERROR_POS_CANCEL;	// RDD 220315 - Go back to present card
					return VIM_ERROR_USER_CANCEL;	// RDD 220315 - Go back to present card
				}
				else		 // RDD 301214 SPIRA:8285 Need to do something if accepted !
				{
					  // RDD 181114 SPIRA:8257 ManPan needs to Force Credit
					  VIM_ERROR_RETURN_ON_FAIL( card_detail_capture_key_entry(card_details_ptr, header_ptr, AmountString(txn.amount_total)) );
					  txn.account = account_credit;
					  return VIM_ERROR_NONE;
				}
			}
			// Standalone
#if 0
			else if(( txn.type != tt_preauth ) && ( txn.amount_cash == 0 ))
#else
			// RDD 110216 SPIRA:xxxx Allow manual entry for Preauth
			else if( txn.amount_cash == 0 )
#endif
			{
				VIM_INT32 Password;
				// Collect the Password
				VIM_ERROR_RETURN_ON_FAIL( GetPassword( &Password, "Enter Password" ));
				if( 1 )
				{
					// RDD 090222 JIRA PIRPIN-1338 Disable default Man Pan Password
					// Check the password
					//if(( Password == WOW_MANUAL_ENTRY_PASSWORD )||( Password == WOW_ELECTRONIC_GIFT_CARD ))
					if( Password == WOW_ELECTRONIC_GIFT_CARD )					
					{
						// RDD 090513 SPIRA:6725 Shortcut to Electronic gift card processing
						if( Password == WOW_ELECTRONIC_GIFT_CARD )
						{
							transaction_set_status( &txn, st_electronic_gift_card );
						}
						else
						{
							// RDD 221014 SPIRA:8210 - no man pan for balance in standalone
							// RDD 190412 SPIRA:5239 No manual entry for deposit query card
							if(( txn.type == tt_query_card_deposit ) || ( txn.type == tt_balance ))
							{
								txn.error = ERR_MANUAL_PAN_DISABLED;
								display_result(txn.error, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
								return txn.error;
							}
						}
						// RDD 080212 - bugfix : kill PICC reader before manual entry
						//reset_picc_transaction();
						close_MAG_reader();
						DBG_POINT;
						picc_close(VIM_FALSE);
						// RDD 181114 SPIRA:8257 ManPan needs to Force Credit
						VIM_ERROR_RETURN_ON_FAIL( card_detail_capture_key_entry(card_details_ptr, header_ptr, AmountString(txn.amount_total)) );
						txn.account = account_credit;
						// RDD 100512 SPIRA:5464,5465 timeout on Manual entry return to card capture state - need to set manual card type even if error
						//card_details_ptr->type = card_manual;
						return VIM_ERROR_NONE;
					}
					// RDD 070915 v547-1 SPIRA:8811
					else
					{
						return VIM_ERROR_POS_CANCEL;	// RDD 220315 - Go back to present card
					}
				}
			}
			else
			{
				// RDD 171215 SPIRA:8211
				vim_kbd_sound_invalid_key();
				return VIM_ERROR_NOT_FOUND;
			}
			break;

		default:
            VIM_DBG_MESSAGE("------------ Gift Card S Mode -------------");

            if (transaction_get_status(&txn, st_giftcard_s_type) || ((txn.card_state == card_capture_new_gift) && (txn.type == tt_balance)))
            {
                VIM_ERROR_RETURN(card_detail_capture_s_gift_card(card_details_ptr, &key));
            }

            //vim_kbd_sound_invalid_key();
			return VIM_ERROR_NOT_FOUND;
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR card_get_check_events( VIM_EVENT_FLAG_PTR *event_list )
{
   VIM_ERROR_PTR res = VIM_ERROR_NONE;

   // msg waiting to read on UART/socket. Check if it's a POS cancellation event, and respond to any heartbeat msgs
   event_list[MAG_PTR] = VIM_NULL;
   event_list[ICC_PTR] = VIM_NULL;
   event_list[KBD_PTR] = VIM_NULL;

   // call with short (1msec) timeout to allow pceftpos task to run
   //DBG_MESSAGE( "----------------- calling vim_event_wait --------------------" );
   if(( res = vim_event_wait( event_list, MAX_EVENTS, 1000 )) != VIM_ERROR_TIMEOUT )
   {
       //VIM_DBG_WARNING("--------------- event detected! ---------------");
       //VIM_DBG_ERROR(res);

       if( event_list[POS_PTR] != VIM_NULL && *event_list[POS_PTR] )
       {
           // this is caused by a non-abort message, but treat it as abort
           DBG_MESSAGE( "<<<<<<<<< pos event detected >>>>>>>>>>>" );
           terminal.abort_transaction = VIM_TRUE;
       }
   }

   // check for comms disconnect
   if( !pceft_is_connected() )
   {
      DBG_MESSAGE( "<<<<<<<<< POS disconnected >>>>>>>>>>>" );
      terminal.abort_transaction = VIM_TRUE;
   }

   // Check for abort... set by app code: vim_pceftpos_callback_get_status
   if( terminal.abort_transaction )
   {
       DBG_MESSAGE( "<<<<<<<<< about_transaction flag set, aborting J8 >>>>>>>>>>>" );

       // RDD 091220 JIRA PIRPIN-960 ensure display changes as soon as J8 cancelled
       pceft_debug_test(pceftpos_handle.instance, "JIRA PIRPIN-960");
       //IdleDisplay();

       txn.error = VIM_ERROR_NOT_FOUND;
       return txn.error;
   }

   return VIM_ERROR_NONE;
}

#ifndef _WIN32

static VIM_TEXT const terminal_file[] = VIM_FILE_PATH_LOCAL "terminal.json";
static VIM_TEXT const wowpass_file[] = VIM_FILE_PATH_LOCAL "wowpass.json";
static VIM_TEXT const dynamic_file[] = VIM_FILE_PATH_LOCAL "dynamic.json";

VIM_ERROR_PTR card_get_wow_australia_vas_tap( VIM_EVENT_FLAG_PTR *event_list )
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;

    // display 'present pass' screen
	//VIM_ERROR_IGNORE(display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassTap.png"), VIM_PCEFTPOS_KEY_CANCEL));
	VIM_ERROR_IGNORE(display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassTap.png"), VIM_PCEFTPOS_KEY_NONE));

    VIM_DBG_PRINTF1( "comms_fd = %d", comms_fd );
    mal_nfc_SetCommsFd( comms_fd );

    //pceft_debug_test( pceftpos_handle.instance, "Using mal_vas" );

    while( 1 )
    {

		VIM_INT16 RamUsedKB = 0;
		vim_prn_get_ram_used(&RamUsedKB);
		VIM_DBG_PRINTF1("!!!!!! RAM USED = %d Kb !!!!", RamUsedKB);

        // blocks main thread until something happens - returns on:
        // - VAS tap successful
        // - POS msg received (could be a cancel, or a heartbeat)
        // - VAS tap read error
        // - VAS read timeout. Timeout defined (in msec) in 'PollTime' parameter in terminal.json

		VIM_DBG_MESSAGE("!!!!!!!!!!!!!!! Do VAS Activation !!!!!!!!!!!!!!!!!!!");

        //eRet = mal_nfc_DoVasActivation( WALLET_WOW_EDR, "terminal.json", TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "wowpass.json"), "dynamic.json"), mal_nfc_Thread_CancelOnRxPending, VIM_FALSE );
		// (works) eRet = mal_nfc_DoVasActivation(WALLET_WOW_EDR, terminal_file, wowpass_file, dynamic_file, mal_nfc_Thread_CancelOnRxPending, VIM_FALSE);
		eRet = mal_nfc_DoVasActivation(WALLET_WOW_EDR, terminal_file, TermGetBrandedFile(VIM_FILE_PATH_LOCAL, "wowpass.json"), dynamic_file, mal_nfc_Thread_CancelOnRxPending, VIM_FALSE);
		VIM_DBG_MESSAGE("!!!!!!!!!!!!!!! VAS Activation ended !!!!!!!!!!!!!!!!!!!");
		if( eRet == VIM_ERROR_NONE )
        {
            VIM_CHAR LoyaltyData[128];
            vim_mem_clear( LoyaltyData, VIM_SIZEOF( LoyaltyData ));

            // card read success
            mal_nfc_GetVasCardNumber( LoyaltyData, VIM_SIZEOF(LoyaltyData), &txn.session_type );
            if( vim_strlen( LoyaltyData ))
            {
                SaveLoyaltyNumber( LoyaltyData,  vim_strlen(LoyaltyData), txn.session_type );
                VIM_DBG_MESSAGE("card_read.c: SaveLoyaltyNumber");
                VIM_DBG_VAR(txn.session_type);

                return ERR_LOYALTY_CARD_SUCESSFUL;
            }
            else
            {
                VIM_ERROR_RETURN( VIM_ERROR_MISMATCH );
            }
        }
        else
        {
            //VIM_DBG_PRINTF3( "mal_nfc_DoVasActivation error = %d (%s,%s)", eRet, SAFE_STR(eRet->id), SAFE_STR(eRet->name) );

            if ((VIM_ERROR_POS_CANCEL != eRet) && (VIM_ERROR_TIMEOUT != eRet))
            {
                // assume a card read error. Call callback to display an error to user, then re-instate the VAS read again
                vas_error_call_back(VIM_ERROR_NOT_FOUND);
            }

            // RDD 110520 v710-3 Trello #125 : Payment card in field for long time at J8 causes freeze.
            // WC : check events and return if interrupt event occurred
            VIM_ERROR_RETURN_ON_FAIL( card_get_check_events( event_list ) );
			VIM_DBG_MESSAGE("!!!!!!!!!!!!!!! Timeout VAS Activation !!!!!!!!!!!!!!!!!!!");
		}
    }
}
#endif

VIM_ERROR_PTR card_capture(VIM_TEXT const *header_ptr, card_info_t *card_details_ptr, card_capture_state_t capture_state, VIM_UINT8 icc_read_count)
{
	VIM_EVENT_FLAG_PTR event_list[MAX_EVENTS] = { 0, };
	VIM_SIZE timeout = CARD_SWIPE_TIMEOUT;		// RDD 180912 No Timeout for J8 Query Card (260516 V560 - moved code for handling this )
	VIM_KBD_CODE key;

	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	// RDD 190213 added as VIM_ERROR_USER_CANCEL now causes re-prompting for card
	txn.emv_error = VIM_ERROR_NONE;
	VIM_DBG_STRING("Card Capture");

    // RDD 140520 Trello #66 VISA Tap Only terminal
    if (TERM_CTLS_ONLY)
    {
        // Allow J8 and thats it !
        if (capture_state != card_capture_picc_apdu)
            capture_state = card_capture_standard;
    }


	DBG_POINT;

	// RDD 010714 SPIRA:7909 Problems caused by failed cancel cmd
	terminal_clear_status(ss_picc_response_received);

#if 0
	// This fix for WWBP-171 breaks gift card (JIRA:WWBP-172), hence reverting the change
	// RDD 270218 JIRA:WWBP-171
		transaction_clear_status(&txn, st_electronic_gift_card);
#endif

	// RDD 141013 SPIRA:6957 NZ prblem - this seems to fix it.
	//txn.emv_pin_entry_params.offline_pin = VIM_FALSE;
	DBG_POINT;
	// RDD 190315 - clear any bufferred keypresses or touches
	{
		  VIM_KBD_CODE key;
		  DBG_POINT;
		  while( vim_screen_kbd_or_touch_read( &key, 1, VIM_FALSE ) == VIM_ERROR_NONE )
			DBG_VAR( key );

		  DBG_POINT;
	}

	// RDD 241012 SPIRA:6214 PinPad beeps if screen pressed during card capture state
	vim_tch_close();

    DBG_POINT;

	// RDD 010212 - ensure that the card has been removed before we enter this state
	if( txn.read_icc_count )
	{
        DBG_POINT;
		emv_remove_card( "" );
        DBG_POINT;
	}
	else
	{
		VIM_BOOL card_present = VIM_FALSE;

        DBG_POINT;

		vim_icc_check_card_slot( &card_present );
		DBG_VAR( card_present );

        DBG_POINT;

		if( card_present )
		{
			VIM_ERROR_IGNORE( FlushHandles( event_list ) );
			return HandleEMVContact();
		}
        // RDD 131119 - Crashes the P400 terminal in NZ query card second go
        // (BDM 210420): This does not cause any crash; if next two lines ARE re-enabled, we sail through).
		//else //BD Clear events in case card inserted/removed in idle
		//	emv_remove_card( "" );
	}
#if 1
	{
		/* Clear CPA search flag */
		txn.card_name_index = WOW_MAX_CPAT_ENTRIES;
		transaction_clear_status (&txn, st_cpat_check_completed);
		if( !txn.pos_account )
			txn.account = account_unknown_any;
#if 0
		VIM_ERROR_RETURN_ON_FAIL( FlushHandles( event_list ) );
		if(( res = SetupCtls( &capture_state, &event_list[PICC_PTR] )) != VIM_ERROR_NONE )
		{
			if( original_capture_state == card_capture_picc_test )
				return res;
		}
#endif
	}
#endif
#ifndef _WIN32
	// RDD 071220 - skip this hack by Warren if new Gift Card J8
	if ((TERM_NEW_VAS) && (txn.card_state != card_capture_new_gift))
	{
		// if australian loyalty card, call separate function for simplicity's sake

		// RDD 220221 Fix for New Zealand showing Australia VAS rather than NZ loyalty prompt if @NEW_VAS set
		if (txn.type == tt_query_card && !WOW_NZ_PINPAD)
			//   if ( txn.type == tt_query_card )
		{
			VIM_CHAR* Ptr = NULL;
			VIM_ERROR_IGNORE(FlushHandles(event_list));
			if (TERM_EDP_ACTIVE)
			{
				VIM_BOOL fQrOnly = (VIM_ERROR_NONE == vim_strstr(txn.u_PadData.ByteBuff, "NFC001", &Ptr)) && ('0' == Ptr[6]);
				return Wally_HandleJ8Session(event_list, !fQrOnly);
			}
			else
			{
				if (!IsSlaveMode())
					return card_get_wow_australia_vas_tap(event_list);
				else
				{
					terminal.abort_transaction = VIM_TRUE;
					return VIM_ERROR_NONE;
				}
			}
		}
	}
#endif

	while(1)
	{
		card_capture_state_t original_capture_state = capture_state;

		DBG_POINT;

        VIM_ERROR_RETURN_ON_FAIL( FlushHandles( event_list ) );

        // RDD 220421 Change for Wex Activations in Avalon mode - swipe only
        if((!QC_TRANSACTION(&txn)) && (txn.type == tt_gift_card_activation))
        {
            capture_state = card_capture_forced_swipe;
        }

        // RDD 271120 Only Setup PICC Reader if CTLS is actually required
        if( capture_state >= card_capture_no_picc_error )
        {
            if ((res = SetupCtls(&capture_state, &event_list[PICC_PTR], &event_list[PICCA_PTR])) != VIM_ERROR_NONE)
            {
                if (original_capture_state == card_capture_picc_test)
                    return res;
            }
        }


		// RDD 291217 get event deltas for logging
		vim_rtc_get_uptime( &txn.uEventDeltas.Fields.Prompt );
		VIM_DBG_NUMBER( txn.uEventDeltas.Fields.Prompt );
        VIM_DBG_PRINTF1( "----------------- capture_state = %d --------------------", capture_state );

#ifndef _WIN32
		if (IS_V400m) {
			hideStatusBar();
		}
#endif
		switch (capture_state)
	    {
            // RDD 140520 Trello #126 wrong prompt for type S gift card
			// RDD 061112 SPIRA:6257 EPAN was invalid so prompt for PAN
            // RDD 251120 JIRA PIRPIN-
            case card_capture_new_gift:
                set_operation_initiator(it_pceftpos);
                // RDD 120221 - Set Authorise in keymask to activate barcode reader
                display_screen(DISP_AVALON_ALL, VIM_PCEFTPOS_KEY_OK | VIM_PCEFTPOS_KEY_CANCEL | VIM_PCEFTPOS_KEY_AUTHORISE | VIM_PCEFTPOS_KEY_INPUT, "Gift Card", AmountString(txn.amount_total));
                //display_screen( DISP_AVALON_ALL, VIM_PCEFTPOS_KEY_CANCEL|VIM_PCEFTPOS_KEY_INPUT, "Query", AmountString(txn.amount_total));
                event_list[PICC_PTR] = VIM_NULL;
                event_list[ICC_PTR] = VIM_NULL;
                break;

			case card_capture_keyed_entry:
                if (transaction_get_status(&txn, st_giftcard_s_type))          // New flag for S-type gift cards
                {
                    event_list[PICC_PTR] = VIM_NULL;
                    event_list[ICC_PTR] = VIM_NULL;

                    display_screen( DISP_ENTER_EGC_SW_NO_ENTRY, VIM_PCEFTPOS_KEY_CANCEL, GiftCardType(), AmountString(txn.amount_total));
                    //VIM_ERROR_RETURN_ON_FAIL(data_entry(entry_buf, &entry_opts));
                }
                else
                {
                    return(card_detail_capture_key_entry(card_details_ptr, header_ptr, AmountString(txn.amount_total)));
                }
                break;


			case card_capture_no_picc:
			case card_capture_no_picc_reader:
			case card_capture_no_picc_error:
			case card_capture_comm_error_no_picc:
			default:
			{
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN No PICC Display");
				*/
				VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_SWIPE_OR_INSERT, VIM_PCEFTPOS_KEY_CANCEL, header_ptr, AmountString(txn.amount_total ) ));
			}
			event_list[PICC_PTR] = VIM_NULL;
			break;

			case card_capture_forced_swipe:
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN capture forced swipe");
				*/
				if ((txn.type == tt_query_card) && (WOW_NZ_PINPAD))
				{
#ifdef _WIN32
                    VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_PRESENT_ONE_CARD, VIM_PCEFTPOS_KEY_NONE, " ", " " ));
#else
					VIM_ERROR_RETURN_ON_FAIL( display_screen_cust_pcx( DISP_IMAGES, "Label", NZD_PRESENT_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL ));
#endif
				}
				else
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_USE_MAG_STRIPE, VIM_PCEFTPOS_KEY_CANCEL, header_ptr, AmountString(txn.amount_total)));
				}

				// RDD 111119 - close the ICC so card removal doesn't throw and error
				//vim_icc_close(icc_handle.instance_ptr);

				//event_list[PICC_PTR] = VIM_NULL;
				//event_list[ICC_PTR] = VIM_NULL;
				break;

			case card_capture_icc_test:
			case card_capture_icc_retry:
			case card_capture_forced_icc:
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN capture ICC");
				*/
				VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_PLEASE_INSERT_CARD, VIM_PCEFTPOS_KEY_CANCEL, header_ptr, AmountString(txn.amount_total) ));

				// RDD 210912 - don't know what this line was doing so removed it as could cause unexpected behavior
				//if((icc_read_count & 0x0F) > 1) event_list[ICC_PTR] = VIM_NULL;
				event_list[PICC_PTR] = VIM_NULL;
				event_list[MAG_PTR] = VIM_NULL;
				break;

			// RDD 020714 added for diagnostics
			case card_capture_picc_test:
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN capture PICC Test");
				*/
				VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_CONTACTLESS_TEST, VIM_PCEFTPOS_KEY_CANCEL, " ", header_ptr, AmountString(txn.amount_total) ));
				event_list[ICC_PTR] = VIM_NULL;
				event_list[MAG_PTR] = VIM_NULL;
				break;

			case card_capture_picc_apdu:
			{
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN capture PICC APDU");
				*/
				//VIM_CHAR imagename[64] = { 0 };
                    event_list[MAG_PTR] = VIM_NULL;
                    event_list[ICC_PTR] = VIM_NULL;
                    event_list[KBD_PTR] = VIM_NULL;
#ifdef _WIN32
					VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(DISP_BRANDING, "Label", TermGetBrandedFile("", "PassTap.pcx"), VIM_PCEFTPOS_KEY_CANCEL));
#else
					VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(DISP_IMAGES, "Label", TermGetBrandedFile("", "PassTap.png"), VIM_PCEFTPOS_KEY_CANCEL));
#endif

					vim_picc_emv_get_polling_flag(vas_handle.instance, &event_list[PICC_PTR] );
				//event_list[PICC_PTR] = VIM_NULL;
				//timeout = 100;
				break;
			}

			case card_capture_standard:
			case card_capture_standard_retry: // RDD 070212 - added for technical fallback support

#if 1
			// RDD 020113 SPIRA:6485 Allow Contactless for NZ J8 Query Card as this does not return Financial data
			if(( terminal.training_mode ) && ( txn.type != tt_query_card ))
#else
			if( terminal.training_mode )
#endif
			{
				/*
				if (terminal_get_status(ss_wow_basic_test_mode))
					pceft_debug(pceftpos_handle.instance, "TXN capture standard");
				*/
				VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_SWIPE_OR_INSERT, VIM_PCEFTPOS_KEY_CANCEL, header_ptr, AmountString(txn.amount_total) ));
				event_list[PICC_PTR] = VIM_NULL;
			}
			else
			{
                {
                    /*
                    if (terminal_get_status(ss_wow_basic_test_mode))
                        pceft_debug(pceftpos_handle.instance, "TXN capture CTLS Payment");
                    */

                    // RDD 140520 Trello #66 VISA Tap Only terminal
                    if (TERM_CTLS_ONLY)
                    {
                        event_list[ICC_PTR] = VIM_NULL;
                        event_list[KBD_PTR] = VIM_NULL;
                        event_list[MAG_PTR] = VIM_NULL;
                        VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_CONTACTLESS_PAYMENT, VIM_PCEFTPOS_KEY_CANCEL, " ", header_ptr, AmountString(txn.amount_total), "Tap Card/Device"));
                    }
                    else
                    {
                        VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_CONTACTLESS_PAYMENT, VIM_PCEFTPOS_KEY_CANCEL, " ", header_ptr, AmountString(txn.amount_total), "Present Card"));
                    }
					/* get polling flag */
					vim_picc_emv_get_polling_flag( picc_handle.instance, &event_list[PICC_PTR] );
				}
			}
			break;
		}

		while(1)
		{
			if((txn.type == tt_query_card) && ( capture_state != card_capture_new_gift ))
            {
				DBG_POINT;

				// VN JIRA WWBP-776 200120 J8 allowing One Cards to be docked - resulting in MO System error
				event_list[ICC_PTR] = VIM_NULL;
                // RDD 150720 Trello #104 - need kbd support in NZ for OneCard ma entry
                if (!WOW_NZ_PINPAD)
    				event_list[KBD_PTR] = VIM_NULL;

                if (WOW_NZ_PINPAD)
                {
					DBG_POINT;
					// RDD 071119 SPIRA WWBP-751 - allow only Mag for NZ in P400
                    event_list[PICC_PTR] = VIM_NULL;
                }
			}
			else
			{
                if ((capture_state == card_capture_standard) || (capture_state == card_capture_standard_retry))
                {
                    vim_picc_emv_get_polling_flag(picc_handle.instance, &event_list[PICC_PTR]);
                    //VIM_DBG_WARNING("------- Set Activity Flag for Refer to Phone -----");
                    vim_picc_emv_get_activity_flag(picc_handle.instance, &event_list[PICCA_PTR]);
                }
            }

			////////////////////////////////////////////////////////////////////////////////////////
            res = txn_event_wait( event_list, MAX_EVENTS, timeout, CYCLE_LOGO, header_ptr );
			////////////////////////////////////////////////////////////////////////////////////////
            VIM_DBG_WARNING("EEEEEEEEEEEEE EVENT DETECTED ! EEEEEEEEEEEEE");
            VIM_DBG_ERROR(res);
                                // RDD 140520 Trello #66 VISA Tap Only terminal
            if(( res == VIM_ERROR_EMV_CARD_REMOVED ) && ( TERM_CTLS_ONLY ))
            {
                continue;
            }


            // RDD v560 Handle endless timeout for J8
			if(( txn.type == tt_query_card )&&( !WOW_NZ_PINPAD ))
			{

				if (res == VIM_ERROR_TIMEOUT) {
					DBG_POINT;

					break;
				}
			}

			DBG_POINT;

			//VN JIRA WWBP-773 PINpad beeps and reports "Declined System Error" when J8 is left idle prompting for ONECARD
			if (WOW_NZ_PINPAD && (res == VIM_ERROR_TIMEOUT)) {
				break;
			}

			if( res != VIM_ERROR_NONE )
			{
#if 0
				picc_close(VIM_TRUE);
#endif
				close_MAG_reader();
				return res;
			}


			DBG_POINT;

			// RDD added for ADK PICC ACTIVITY
            if (event_list[PICCA_PTR] != VIM_NULL && (*event_list[PICCA_PTR]))
            {
				DBG_POINT;
				if (( res = ProcessPICCActivity()) == VIM_ERROR_PICC_EMV_AUTHENTICATION_FAILURE )
                {
                    VIM_DBG_WARNING( "--------------- Refer To Phone -------------")
                    break;
                    // refer to phone- reactivate;
                }
                else
                    continue;
            }


			DBG_POINT;

			// RDD 060212: continue to run capture loop if VIM_ERROR_USER back returned from handler (eg. for refused keyed entry)
			//if((event_list[KBD_PTR] != VIM_NULL) && ( *event_list[KBD_PTR] )&&( txn.type != tt_query_card))
			if ((event_list[KBD_PTR] != VIM_NULL) && (*event_list[KBD_PTR]))
			{
				DBG_MESSAGE( "<<<<<<<<< KBD EVENT DETECTED >>>>>>>>>>>" );
				VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( &key, VIM_MILLISECONDS_INFINITE, 1 ));
				DBG_VAR( key );

				// RDD 191115 SPIRA:8849 - ignore keypress for preauth
				// RDD 110216 CR SPIRA:xxxx - allow manual entry for Preauth
#if 0
				if(( txn.type == tt_preauth )&&( key != VIM_KBD_CODE_CANCEL ))
					continue;
#endif
				if(( res = HandleKeyedEntry( key, card_details_ptr, header_ptr, capture_state )) == VIM_ERROR_NOT_FOUND )
					continue;

				// RDD 300315 SPIRA:8520 - add USER_CANCEL to break rather than return
                // RDD 200121 JIRA PIRPIN-981 POS Cancel should exit totally
				if (res == VIM_ERROR_USER_CANCEL)
				{
					DBG_POINT;
					break;
				}
				else
					return res;
			}
			break;
		}

		DBG_POINT;
		if( res == VIM_ERROR_PICC_EMV_AUTHENTICATION_FAILURE )
        {
            VIM_DBG_WARNING("--------------- Refer To Phone - reset -------------")
            continue;
        }

		DBG_POINT;
		if( res == VIM_ERROR_TIMEOUT && txn.type == tt_query_card )
		{
			//vim_tms_callback_pceft_debug("VIM_ERROR_TIMEOUT && txn.type == tt_query_card");
			VIM_DBG_MESSAGE("VIM_ERROR_TIMEOUT && txn.type == tt_query_card");
			continue;
			//break;
		}

		DBG_POINT;
		if( res == VIM_ERROR_POS_CANCEL )
			continue;
		if (res == VIM_ERROR_USER_CANCEL)
		{
			if (IS_STANDALONE)
			{
				VIM_ERROR_RETURN(res);
			}
			else continue;
		}
		else
			break;
	}

	// RDD v560 Handle endless timeout for J8
	//vim_tms_callback_pceft_debug("Before Handle event");
	return( HandleEvent( event_list, card_details_ptr ) );
}


VIM_ERROR_PTR card_capture_test(void)
{
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR card_get_pan( card_info_t *card_details, VIM_CHAR *pan_buf )
{
  VIM_SIZE pan_len = 0;
  VIM_CHAR dummy_track2[22] = { 0x35,0x32,0x39,0x30,0x32,0x30,0x39,0x39,0x39,0x39,0x39,0x39,0x39,0x39,0x39,0x33,0x3d,0x34,0x39,0x31,0x32,0x00};
  VIM_CHAR *pan;

  DBG_POINT;

  switch( card_details->type )
  {
	case card_manual:
    pan = card_details->data.manual.pan;
    pan_len = card_details->data.manual.pan_length;
    if (pan_len > WOW_PAN_LEN_MAX)
        return VIM_ERROR_OVERFLOW;
    else
	break;

	case card_mag:
	pan = &card_details->data.mag.track2[0];
	VIM_ERROR_RETURN_ON_FAIL( GetPanLen( pan, &pan_len ));
	break;

	case card_chip:
	if( terminal.training_mode )
	{
		pan = &dummy_track2[0];
		VIM_ERROR_RETURN_ON_FAIL( GetPanLen( pan, &pan_len ));
	}
	else
	{
		pan = &card_details->data.chip.track2[0];
		VIM_ERROR_RETURN_ON_FAIL( GetPanLen( pan, &pan_len ));
		DBG_VAR( pan_len );
	}
	break;

	case card_picc:
    pan = &card_details->data.picc.track2[0];
	VIM_ERROR_RETURN_ON_FAIL( GetPanLen( pan, &pan_len ));
	break;

    case card_qr:
    pan = &card_details->pci_data.ePan[0];
    VIM_ERROR_RETURN_ON_FAIL( GetPanLen( pan, &pan_len ));
    break;

	default:
		//DBG_VAR(card_details->type);
		return VIM_ERROR_NOT_FOUND;
  }

  vim_mem_copy(pan_buf, pan, pan_len);
  pan_buf[pan_len] = 0;

  return VIM_ERROR_NONE;
}


/**
 * Extract expiry date from card details structure into a null terminated string in mmyy format
 */
VIM_ERROR_PTR card_get_expiry(card_info_t *card_details, VIM_CHAR *exp_mmyy)
{
  VIM_SIZE pan_len;
  char *pan;

  vim_mem_set(exp_mmyy, ' ', 4);
  exp_mmyy[4] = 0;

  if (card_details->type == card_manual)
  {
    vim_mem_copy(exp_mmyy, card_details->data.manual.expiry_mm, 2);
    vim_mem_copy(&exp_mmyy[2], card_details->data.manual.expiry_yy, 2);
  }
  else
    if (card_details->type == card_mag)
    {
      pan = &card_details->data.mag.track2[1];
      for (pan_len = 0; pan_len < card_details->data.mag.track2_length; pan_len++)
      {
        if (pan[pan_len] == '=')
          break;
      }
      vim_mem_copy(exp_mmyy, &pan[pan_len+3], 2);
      vim_mem_copy(&exp_mmyy[2], &pan[pan_len+1], 2);
    }
    else
      if (card_details->type == card_chip)
      {
        pan = &card_details->data.chip.track2[0];
        for (pan_len = 0; pan_len < card_details->data.chip.track2_length; pan_len++)
        {
          if (pan[pan_len] == '=')
            break;
        }
        vim_mem_copy(exp_mmyy, &pan[pan_len+3], 2);
        vim_mem_copy(&exp_mmyy[2], &pan[pan_len+1], 2);
      }
#if 1
      else if (card_picc == card_details->type)
      {
        pan = &card_details->data.picc.track2[0];
        for (pan_len = 0; pan_len < card_details->data.picc.track2_length; pan_len++)
        {
          if (pan[pan_len] == '=')
            break;
        }
        vim_mem_copy(exp_mmyy, &pan[pan_len+3], 2);
        vim_mem_copy(&exp_mmyy[2], &pan[pan_len+1], 2);
      }
#endif
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR card_get_service_code(const card_mag_t *mag_details, VIM_CHAR *service_code)
{
  VIM_SIZE index = 0;
  VIM_ERROR_PTR res = VIM_ERROR_NOT_FOUND;

  /* Find field separator */
  if( txn.card.type != card_mag ) return VIM_ERROR_NONE;

  for (index=0; index < mag_details->track2_length; index++)
  {
    if (mag_details->track2[index] == '=')
	{
		/* Check we have enough data to fit a service code (1 byte separator, 4 bytes expiry 3 byte service code)*/
		VIM_ERROR_RETURN_ON_INDEX_OVERFLOW(index + 1 + 4 + 3, mag_details->track2_length);
		/* Copy service code into passed buffer */
		vim_mem_copy(service_code, &mag_details->track2[index + 1 + 4], 3);
		res = VIM_ERROR_NONE;
	}
  }
  return res;
}


/**
 * Display PAN+expiry if issuer wants us to
 */
VIM_BOOL card_confirm_pan(card_info_t *card_details)
{
  VIM_CHAR line1[30], line2[30];
  VIM_SIZE index, display_size;

  /*  Only display for mag stripe  */
  if (card_details->type != card_mag)
    return VIM_TRUE;

  /* Find field separator */
  for (index=0; index < card_details->data.mag.track2_length; index++)
  {
    if (card_details->data.mag.track2[index] == '=')
      break;
  }
  /*  Add 5 for expiry date & = sign */
  display_size = index + 5;
  /*  Clear the lines  */
  vim_mem_set(line1, 0, sizeof(line1));
  vim_mem_set(line2, 0, sizeof(line2));
  /*  Copy PAN into display buffers */
  vim_mem_copy(line1, card_details->data.mag.track2, MIN(display_size, 20));
  /*  If longer than 20 need to display on 2 lines */
  if(display_size > 20)
  {
    display_size -= 20;
    vim_mem_copy(line2, &card_details->data.mag.track2[20], MIN(display_size, 20));
  }
  else
     vim_mem_set(line2, 0x20, 20);

  //VIM_DBG_DATA(line1, sizeof(line1));
  //VIM_DBG_DATA(line2, sizeof(line2));
  /*  Now display and get confirmation */
  display_screen(DISP_CONFIRM_PAN, VIM_PCEFTPOS_KEY_CANCEL, line1, line2);
  return( (get_yes_no(GET_YES_NO_TIMEOUT) == VIM_ERROR_NONE));
}

VIM_BOOL check_luhn_new(char *pan, VIM_UINT8 pan_length)
{
  VIM_UINT8 multiplier = 1;
  VIM_SIZE sum1=0, sum2=0;
  VIM_SIZE cnt=0;

  if( !pan_length )
    return VIM_FALSE;

  for( cnt=0; cnt<pan_length; cnt++ )
  {
    if( multiplier == 1 ) sum1 += pan[cnt] - '0';
    if( multiplier == 2 ) sum2 += (pan[cnt]-'0') *2;
    multiplier = ( multiplier == 1 )? 2 : 1;
  }

  return ((sum1+sum2)%10 == 0);
}


// RDD 180320 added callback from slave mode to process track data
VIM_ERROR_PTR card_mag_process_tracks(VIM_MAG_PTR mag_ptr, VIM_MAG_TRACK_SET* track_set_ptr)
{
	card_info_t card_details, * card_details_ptr = &card_details;
	VIM_UINT8 PceftBin = PCEFT_BIN_UNKNOWN;
	VIM_UINT16 CardIndex = MAX_CARD_NAME_TABLE_SIZE;
	VIM_TEXT ClearPAN[WOW_PAN_LEN_MAX + 1];
	VIM_TEXT MaskedPAN[WOW_PAN_LEN_MAX + 1];
	VIM_SIZE PanLen;
	VIM_UINT8* PtrT1, * PtrT2;

	VIM_DBG_MESSAGE("---------- Slave Mag Callback ---------");

#if 1

	// Grab the track data and convert into standard WoW format
	VIM_ERROR_RETURN_ON_FAIL(vim_mag_read_tracks(mag_ptr, track_set_ptr));

	/* initialize the output structure */
	vim_mem_clear(card_details_ptr, VIM_SIZEOF(*card_details_ptr));

	/* set the card type */
	card_details_ptr->type = card_mag;

	/* set the length of the track2 data */
	card_details_ptr->data.mag.track2_length = track_set_ptr->tracks[TRACK2_POS].length;
	/* copy the track 2 data */
	vim_mem_copy(card_details_ptr->data.mag.track2, track_set_ptr->tracks[TRACK2_POS].buffer, track_set_ptr->tracks[TRACK2_POS].length);

	/* set the length of the track1 data */
	card_details_ptr->data.mag.track3_present = VIM_FALSE;
	card_details_ptr->data.mag.track1_length = track_set_ptr->tracks[TRACK1_POS].length;

	/* If we have read track1 but not 2 we have a problem */
	if (track_set_ptr->tracks[TRACK1_POS].length)
	{
		if (!track_set_ptr->tracks[TRACK2_POS].length)
		{
			return ERR_INVALID_TRACK2;
		}
		else
		{
			/* copy the track 1 data */
			card_details_ptr->data.mag.track1_length = track_set_ptr->tracks[TRACK1_POS].length;
			vim_mem_copy(card_details_ptr->data.mag.track1, track_set_ptr->tracks[TRACK1_POS].buffer, track_set_ptr->tracks[TRACK1_POS].length);
		}
	}

	VIM_DBG_STRING(track_set_ptr->tracks[0].buffer);
	VIM_DBG_STRING(track_set_ptr->tracks[1].buffer);

#endif

	//VIM_ERROR_RETURN_ON_FAIL( card_detail_capture_read_track12_data( mag_ptr, card_details_ptr ));

	// Grab the PAN so we can get the CPAT Index
	VIM_ERROR_RETURN_ON_FAIL(card_get_pan(card_details_ptr, ClearPAN));
	PanLen = VIM_MIN(WOW_PAN_LEN_MAX, vim_strlen(ClearPAN));

	if ((CardIndex = find_card_range(card_details_ptr, (VIM_UINT8*)ClearPAN, VIM_MIN(WOW_PAN_LEN_MAX, PanLen))) == MAX_CARD_NAME_TABLE_SIZE + 1)
	{
		VIM_ERROR_RETURN(VIM_ERROR_OVERFLOW);
	}
	pceft_pos_card_get_bin(0, PCEFT_DEFAULT_ACCOUNT, &PceftBin);
	mask_out_track_details_txn(MaskedPAN, PanLen, card_details_ptr, PceftBin, VIM_TRUE, VIM_FALSE);

	// Ensure masked PAN is copied over both T1 and T2
	PtrT1 = 1 + (VIM_UINT8*)track_set_ptr->tracks[0].buffer;  // skip the T1 start sentinal
	PtrT2 = (VIM_UINT8*)track_set_ptr->tracks[1].buffer;
	vim_mem_copy(PtrT1, MaskedPAN, PanLen);
	vim_mem_copy(PtrT2, MaskedPAN, PanLen);

	return VIM_ERROR_NONE;
}


