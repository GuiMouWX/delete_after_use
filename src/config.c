
#include <incs.h>
#include <tms.h>

VIM_DBG_SET_FILE("T4");

extern VIM_ERROR_PTR picc_load_parameters( VIM_BOOL fFullReset );
extern VIM_BOOL ValidateConfigData( VIM_CHAR *terminal_id, VIM_CHAR *merchant_id );
extern VIM_ERROR_PTR ValidateFileName( VIM_CHAR **FoundFileName, VIM_CHAR *RootFileName, VIM_SIZE *FileVersion );
extern VIM_ERROR_PTR TerminalIdleDisplay( VIM_UINT8 TxnIndex );
extern PREAUTH_SAF_TABLE_T preauth_saf_table;
extern VIM_ERROR_PTR RunKeyload( void );
extern const char *SecurityToken;
extern VIM_ERROR_PTR WifiSetup(void);
extern VIM_ERROR_PTR BTStatus(VIM_BOOL SilentMode, VIM_UINT8 BTUsage);
extern const char* EDPData;
extern int Wally_BuildJSONRequestOfRegistrytoFIle();
extern VIM_ERROR_PTR WifiInit(void);

VIM_CHAR DummyString[1000];

extern VIM_NET_CONNECT_PARAMETERS sTMS_SETTINGS[];
extern VIM_NET_CONNECT_PARAMETERS sTXN_SETTINGS[];

// RDD 280218 Keyload Test Faclity
extern VIM_ERROR_PTR RunKeyload(void);
extern VIM_BOOL IsSlaveMode(void);


VIM_NET_CONNECT_PARAMETERS *NetConnectPtr;

VIM_BOOL set_mem_log = VIM_FALSE;
// RDD 230418 JIRA WWBP-205
const VIM_TEXT NO_LOCAL_PRINT[] = VIM_FILE_PATH_LOCAL "NoLocalPrn.dat";
#define VIM_TMS_LFD_DEFAULT_CFG                  "DEFAULT.CFG"

extern VIM_ERROR_PTR ConnectClient(void);



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_key_map_run( VIM_UINT32 screen_id, CONFIG_KEY_MAP const* map )
{
  void *Ptr = VIM_NULL;

  /* display screen */
  VIM_ERROR_RETURN_ON_FAIL(display_screen(screen_id, VIM_PCEFTPOS_KEY_NONE));	//BRD move inside loop // RDD 141012 - NO !!

  for(;;)
  {
	  VIM_SIZE map_index;
	  VIM_KBD_CODE key_code;
    VIM_EVENT_FLAG_PTR events[1];

	/* display screen */
	if( screen_id == DISP_SAF_FUNCTIONS )
		VIM_ERROR_RETURN_ON_FAIL(display_screen(screen_id, VIM_PCEFTPOS_KEY_NONE));		// RDD move back outside loop !

    /* wait for keypress */
    VIM_ERROR_RETURN_ON_FAIL(vim_adkgui_kbd_touch_or_other_read(&key_code, ENTRY_TIMEOUT, events, VIM_FALSE));

    if (key_code==VIM_KBD_CODE_CANCEL)
    {
      VIM_ERROR_RETURN( VIM_ERROR_USER_CANCEL );
    }
    else if ( key_code==VIM_KBD_CODE_CLR )
    {
      VIM_ERROR_RETURN(VIM_ERROR_USER_BACK);
    }
	else if (key_code==VIM_KBD_CODE_NONE)
    {
      VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);				//BRD
    }
	for( map_index=0; ( map[map_index].key_code!=VIM_KBD_CODE_NONE ) && ( map[map_index].key_code!=key_code ); map_index++ );
	if(map[map_index].key_code==key_code)
	{
		if( map[map_index].method != VIM_NULL )
		{
			map[map_index].method( Ptr );
			DBG_MESSAGE("Fn completed");
			//break;
		}
	}
	else
		VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound_invalid_key());
  }
}


VIM_ERROR_PTR TermSetStandaloneMode( void )
{
    pceft_close();
	terminal.standalone_parameters |= STANDALONE_INTERNAL_PRINTER;
	terminal.standalone_parameters |= STANDALONE_PRINT_SECOND_RECEIPT;    //BRD 050714 default to Cust Copy
    terminal.standalone_parameters |= STANDALONE_INTERNAL_MODEM;
	terminal.mode = st_standalone;
	terminal.original_mode = terminal.mode;

	terminal.initator = it_pinpad;

	// RDD - temporary until PSTN working
	terminal.internal_modem = internal_modem_gprs;
	terminal.IdleScreen = DISP_IDLE;
    terminal_save();
	vim_pceftpos_delete(pceftpos_handle.instance);

	//terminal.configuration.allow_disconnect = VIM_TRUE;

    VIM_DBG_MESSAGE("STANDALONE STANDALONE STANDALONE STANDALONE STANDALONE STANDALONE STANDALONE STANDALONE ");
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR TermSetIntegratedMode( void )
{
	//VIM_CHAR buffer[100];
	terminal.standalone_parameters &= (!STANDALONE_INTERNAL_PRINTER);
	terminal.mode = st_integrated;
	terminal.original_mode = terminal.mode;

    terminal.initator = it_pceftpos;
	terminal.configuration.TXNCommsType[0]=WIFI;
	terminal.configuration.TXNCommsType[1]=WIFI;
	terminal.configuration.TXNCommsType[2]=WIFI;
	terminal_save();
    VIM_DBG_MESSAGE("INTEGRATED INTEGRATED INTEGRATED INTEGRATED INTEGRATED INTEGRATED INTEGRATED INTEGRATED ");
	terminal.configuration.allow_disconnect = VIM_FALSE;

	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_card_read_test_internal( VIM_CHAR *Results )
{
  VIM_SIZE good_read, bad_read;
  VIM_MAG_TRACK_SET track_set;
  VIM_KBD_CODE key_code;
  VIM_EVENT_FLAG_PTR events[1];
  VIM_EVENT_FLAG_PTR mag_event_ptr = VIM_NULL;

  good_read = bad_read = 0;

  while (VIM_TRUE)
  {
      vim_mag_flush(mag_handle.instance_ptr);
      display_screen(DISP_CARD_READ_STATISTICS, VIM_PCEFTPOS_KEY_NONE, good_read, bad_read, good_read + bad_read);

    VIM_ERROR_RETURN_ON_FAIL(vim_mag_get_swiped_flag(mag_handle.instance_ptr, &mag_event_ptr));

    events[0] = mag_event_ptr;

    if (VIM_ERROR_TIMEOUT != vim_event_wait(events, VIM_LENGTH_OF(events), 100))
    {
      if (*mag_event_ptr)
      {
        vim_mag_read_tracks(mag_handle.instance_ptr, &track_set);

        if (((0 == track_set.tracks[0].length) && (0 == track_set.tracks[1].length))
          || ((track_set.tracks[0].length > VIM_SIZEOF(track_set.tracks[0].buffer)) &&
             (track_set.tracks[1].length > VIM_SIZEOF(track_set.tracks[1].buffer))))
		{
			//DBG_VAR(bad_read);
            bad_read++;
		}
        else
		{
			//DBG_VAR(good_read);
            good_read++;
		}
      }
	  display_screen(DISP_CARD_READ_STATISTICS, VIM_PCEFTPOS_KEY_NONE, good_read, bad_read, good_read + bad_read);
    }

    vim_kbd_read(&key_code, 500);

    if (VIM_KBD_CODE_CLR == key_code)
      {good_read = bad_read = 0;}

    if (VIM_KBD_CODE_CANCEL == key_code)
	{
		vim_snprintf( Results, 50, "\nMSR OK:%d NOK:%d\n", good_read, bad_read );

		//VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_MAINTENANCE_FUNCTIONS, VIM_PCEFTPOS_KEY_NONE ));
		//return VIM_ERROR_USER_CANCEL;
		break;
	}
  }

  if( good_read )
	return VIM_ERROR_NONE;
  else
	return VIM_ERROR_SYSTEM;
}

#ifndef _DEBUG
static VIM_ERROR_PTR config_send_and_clear_saf(void)
{
  VIM_KBD_CODE key_code;
  VIM_BOOL loop = VIM_TRUE;

  display_screen(DISP_CLEAR_SAF, VIM_PCEFTPOS_KEY_NONE, "");

  while (loop)
  {
    vim_screen_kbd_or_touch_read( &key_code, TIMEOUT_INACTIVITY, VIM_FALSE );

    switch (key_code)
    {
	  case VIM_KBD_CODE_CANCEL:
      case VIM_KBD_CODE_CLR:
        return VIM_ERROR_USER_CANCEL;

#ifdef _DEBUG
	  case VIM_KBD_CODE_KEY1:
		 // DBG_MESSAGE("DELETING CPAT FILE");
		 terminal.file_version[CPAT_FILE_IDX] = 0;
		 vim_file_delete("~/CPAT");
		 break;

	  case VIM_KBD_CODE_KEY2:
		 // DBG_MESSAGE("DELETING PKT FILE");
		 terminal.file_version[PKT_FILE_IDX] = 0;
		 vim_file_delete("~/PKT");
		 break;

	  case VIM_KBD_CODE_KEY3:
		 // DBG_MESSAGE("DELETING EPAT FILE");
		 terminal.file_version[EPAT_FILE_IDX] = 0;
		 vim_file_delete("~/EPAT");
		 break;

	  case VIM_KBD_CODE_KEY4:
		 // DBG_MESSAGE("DELETING FCAT FILE");
		 terminal.file_version[FCAT_FILE_IDX] = 0;
		 vim_file_delete("~/FCAT");
		 break;
#endif

	  case VIM_KBD_CODE_KEY9:
		 pa_destroy_batch();

		 break;

      case VIM_KBD_CODE_OK:
		if( terminal.reversal_count )
		{
			//DBG_MESSAGE("SENDING REVERSAL");
			reversal_send( VIM_TRUE );
			//DBG_MESSAGE("DELETING REVERSAL");
			reversal_clear( VIM_FALSE );
		}
		//DBG_MESSAGE("SENDING ALL SAF");
        VIM_ERROR_RETURN_ON_FAIL( saf_send( VIM_TRUE, MAX_BATCH_SIZE, VIM_FALSE, VIM_TRUE ));
        //saf_send(VIM_FALSE, MAX_BATCH_SIZE, VIM_FALSE, VIM_TRUE);

		//VIM_ERROR_RETURN_ON_FAIL(pceft_pos_display_close());
		//DBG_MESSAGE("DELETING ALL SAF");
		saf_destroy_batch();


		// RDD 221112: Best delete the card buffer too just in case...
		card_destroy_batch();

        comms_disconnect();
        loop = VIM_FALSE;
        break;

      default:
        break;
    }
  }

  if (0 == saf_totals.fields.saf_count)
    display_screen(DISP_SAF_APPROVED, VIM_PCEFTPOS_KEY_NONE);
  else
    display_screen(DISP_SAF_DECLINED, VIM_PCEFTPOS_KEY_NONE);

  //vim_screen_kbd_or_touch_read(VIM_NULL, 5000);
  return VIM_ERROR_NONE;
}

#endif

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_card_read_test(void *Ptr)
{
  VIM_ERROR_PTR error;

  /* open mag if it is not already open */
  if (VIM_NULL == mag_handle.instance_ptr)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_mag_new(&mag_handle));
  }

  /* perform the actual card read test */
  error = config_card_read_test_internal( (VIM_CHAR *)Ptr );

  /* close mag */
  if (VIM_NULL != mag_handle.instance_ptr)
  {
    vim_mag_destroy(mag_handle.instance_ptr);
    mag_handle.instance_ptr = VIM_NULL;
  }

  /* return any errors that were returned by the internal function */
  VIM_ERROR_RETURN_ON_FAIL(error);
  return VIM_ERROR_NONE;
}



VIM_BOOL config_get_yes_no_timeout(VIM_CHAR *Message, VIM_CHAR *Confirmation, VIM_SIZE Timeout, VIM_BOOL TimeOutIsYes)
{
    VIM_KBD_CODE key_code = VIM_KBD_CODE_NONE;
    // RDD 121115 - Need to keep the real value of this for reason code !
    VIM_ERROR_PTR error_backup = txn.error_backup;
    VIM_RTC_UPTIME StartTime, EndTime;
	VIM_BOOL res = VIM_FALSE;
	VIM_BOOL done = VIM_FALSE;

    vim_rtc_get_uptime(&StartTime);
    EndTime = StartTime + Timeout;

    // Read the buffer to get rid of buffered presses - use safe method from card read
    {
        VIM_KBD_CODE key;
        DBG_POINT;
        while (vim_screen_kbd_or_touch_read(&key, 1, VIM_FALSE) == VIM_ERROR_NONE)
            DBG_VAR(key);
    }

    display_screen(DISP_YES_NO, VIM_PCEFTPOS_KEY_NONE, Message);

    while (!done)
    {
        VIM_RTC_UPTIME CurrentTime;
        vim_rtc_get_uptime(&CurrentTime);

        // Ensure that incorrect keys hit result in same timeout as cannoit afford POS to timeout
        if (CurrentTime >= EndTime)
        {
            txn.error_backup = error_backup;
			break;
        }

        // RDD 230215 SPIRA:8410 - Need to record and act on timeout for standalone sig check.
        if ((txn.error_backup = vim_screen_kbd_or_touch_read(&key_code, (EndTime - CurrentTime), VIM_TRUE)) != VIM_ERROR_NONE)
        {
            // RDD 251115 v547-5 SPIRA:8852 Need to approve if sig check times out
            if (txn.error_backup == VIM_ERROR_TIMEOUT)
            {
                pceft_debug(pceftpos_handle.instance, "Yes No Timeout");

                // RDD 111215 SPIRA:8870 Reset Totals reset if timeout - need to not reset.
                // RDD 111215 SPIRA:
                if (TimeOutIsYes)
                    res = VIM_TRUE;
				break;
            }
            else
            {
                txn.error_backup = error_backup;
				break;
            }
        }
        else
        {
            txn.error_backup = error_backup;
        }

        DBG_VAR(key_code);
        switch (key_code)
        {
        case VIM_KBD_CODE_NONE:
            pceft_debug(pceftpos_handle.instance, "Yes No KeyCode None");
			break;

        case VIM_KBD_CODE_CANCEL:
        case VIM_KBD_CODE_RFUNC0:
            vim_kbd_sound();
            //if( vim_strlen( Confirmation ))
			if(1)
            {
				IdleDisplay();
                //display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "<CANCELLED>", "");
                //vim_event_wait(VIM_NULL, 0, 2000);
            }
			done = VIM_TRUE;
			break;

        case VIM_KBD_CODE_OK:
        case VIM_KBD_CODE_RFUNC1:
            vim_kbd_sound();

            //display_screen(DISP_YES_NO, VIM_PCEFTPOS_KEY_NONE, Message);
            if (vim_strlen(Confirmation))
            {
                display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Confirmation, "" );
                vim_event_sleep(1000);
            }
            res = VIM_TRUE;
			done = VIM_TRUE;
			break;

            // run kbd read again if bad key pressed
        default:
            ERROR_TONE;
            pceft_debug(pceftpos_handle.instance, "Yes No Invalid Key");
            continue;
        }
    }
	pceft_pos_display_close();
    return res;
}




VIM_BOOL config_get_yes_no( VIM_CHAR *Message, VIM_CHAR *Confirmation )
{
	return config_get_yes_no_timeout( Message, Confirmation, ENTRY_TIMEOUT, VIM_FALSE );
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_diagnostics(void)
{
  vim_lcd_close();
  vim_kbd_close();
  vim_mag_destroy(mag_handle.instance_ptr);
  vim_prn_close();
  pceft_close();
  vim_icc_close(icc_handle.instance_ptr);
  picc_close(VIM_TRUE);

  vim_sys_diagnostics();

 // vim_lcd_open();
  vim_kbd_open();
  vim_mag_new(&mag_handle);
  //vim_prn_open();
  pceft_settings_load_only();

  if (TERM_CONTACTLESS_ENABLE)
  {
    /* open contactless reader */
    if( VIM_ERROR_NONE != picc_open( VIM_TRUE,VIM_FALSE ))
    {
      VIM_KBD_CODE key_code;
      display_screen( DISP_CONTACTLESS_NOT_READY, VIM_PCEFTPOS_KEY_NONE );
      if( 1 )
      {
          VIM_EVENT_FLAG_PTR events[1];
          vim_adkgui_kbd_touch_or_other_read(&key_code, VIM_MILLISECONDS_INFINITE, events, 1);
      }
	  DBG_POINT;
      picc_close(VIM_TRUE);
    }
  }

  return VIM_ERROR_NONE;
}


// RDD 140520 Trello #66 VISA Tap Only terminal
static void SetupCTLSOnlyTerminal(void)
{
    VIM_INT32 Password;

    // RDD 040313 - DG asked (in a text today) to make a change so that the SAF was always deleted with the extra password.
    if (/*terminal_get_status(ss_wow_basic_test_mode)*/1)
    {
        // If it's a test terminal and the correct password is entered, then allow SAF deletion without sending

        if (terminal.configuration.CTLSOnlyTerminal)
        {
            if (config_get_yes_no("Disable CTLS Only?", "All Entry Modes\nNow Active"))
            {
                vim_event_wait(VIM_NULL, 0, 2000);
                terminal.configuration.CTLSOnlyTerminal = VIM_FALSE;
            }
        }
        else if (config_get_yes_no("Set this PinPad\nas CTLS Only?", ""))
        {
            if (GetPassword(&Password, "Enter Password") == VIM_ERROR_NONE)
            {
                // Check the password
                if (Password == SAF_DELETE_PASSWORD)
                {
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "This PinPad", "Set CTLS Only");
                    terminal.configuration.CTLSOnlyTerminal = VIM_TRUE;
                }
                else
                {
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Incorrect Password", "");
                    terminal.configuration.CTLSOnlyTerminal = VIM_FALSE;
                }
                vim_event_wait(VIM_NULL, 0, 2000);
            }
        }
    }
}




/*----------------------------------------------------------------------------*/

// "1720" Functions : SAF Control

/*----------------------------------------------------------------------------*/

void delete_lfd_files(VIM_BOOL OfferWebLFD )
{
    VIM_FILE_HANDLE handle;
    const VIM_CHAR *DummyVersion = "dl.82000.7.03.23.tgz,21A5B30ACD84F962858AC940A09DDB20E16306A3,7.03.23,P400\n";
    display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Delete VIM LFD Files", "");
    vim_event_sleep(2000);
    vim_file_delete("~/TMS_STATUS.CFG");
    //vim_file_delete("~/TMS_CFG.DAT");
    //vim_file_delete("~/TMS_SW.CFG");
    //vim_file_delete("~/VERSION.CFG");
    VIM_ERROR_WARN_ON_FAIL( vim_file_open( &handle, "~/TMS_STATUS.CFG", VIM_TRUE, VIM_TRUE, VIM_TRUE ));
    VIM_ERROR_WARN_ON_FAIL( vim_file_set("~/TMS_STATUS.CFG", DummyVersion, vim_strlen(DummyVersion)));
    
    if (OfferWebLFD)
    {
        if (TERM_NEW_LFD == VIM_FALSE)
        {
            if (config_get_yes_no_timeout("Set WebLFD?", "WebLFD Now Set", 10000, VIM_FALSE) == VIM_TRUE)
            {
                vim_event_wait(VIM_NULL, 0, 1000);
                TERM_NEW_LFD = VIM_TRUE;
            }
        }
        else
        {
            if (config_get_yes_no_timeout("Set VIM LFD?", " VIM LFD Now Set", 10000, VIM_FALSE) == VIM_TRUE)
            {
                vim_event_wait(VIM_NULL, 0, 1000);
                TERM_NEW_LFD = VIM_FALSE;
            }
        }
    }
}


static VIM_ERROR_PTR saf_delete(void *Ptr)
{
	const VIM_TEXT* items[] = { "Payment App", "Qwikcilver" };
	VIM_SIZE SelectedIndex = 0xFFFF;
    VIM_SIZE ItemIndex = 0;
	//VIM_SIZE num = QC_IsInstalled(VIM_TRUE) ? 2 : 1;
	// RDD 030322 JIRA PIRPIN-1456 QC stuff shown when QC not enabled in TMS
	VIM_SIZE num = TERM_QC_GIFT_DISABLE ? 1 : 2;

	VIM_DBG_NUMBER(num);

#ifndef _WIN32
    VIM_ERROR_RETURN_ON_FAIL( display_menu_select_zero_based("Select Application\nFor SAF Deletion", ItemIndex, &SelectedIndex, items, num, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_TRUE, VIM_FALSE ));
#else
    VIM_ERROR_RETURN_ON_FAIL( display_menu_select("Select Application\nFor SAF Deletion", &ItemIndex, &SelectedIndex, (VIM_TEXT **)items, num, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE ));
#endif



	// RDD 030512 SPIRA:5405 Additional protection for SAF delete. Can enter when Pinpad in test mode or via extra password
#ifdef _DEBUG
	switch (SelectedIndex-1)
	{
	case 0: saf_destroy_batch(); break;
	case 1: QC_SAFClear();       break;
	default:                     break;
	}
#else
	VIM_INT32 Password;

	// RDD 040313 - DG asked (in a text today) to make a change so that the SAF was always deleted with the extra password.
    if (/*terminal_get_status(ss_wow_basic_test_mode)*/1)
	{
		// If it's a test terminal and the correct password is entered, then allow SAF deletion without sending
		if( GetPassword( &Password, "" ) == VIM_ERROR_NONE )
		{
			// Check the password
			if( Password == SAF_DELETE_PASSWORD )
			{
				switch (SelectedIndex)
				{
				case 1:
					saf_destroy_batch();

					// RDD 290515 SPIRA:8672 This is really dumb - need to do it from config menu instead
					reversal_clear(VIM_FALSE); 
					break;
				case 2:
					if (TERM_QC_GIFT_DISABLE)
					{
						QC_SAFClear();
					}
					break;
				default:
					break;
				}
			}

		}

	}
	else
	{
		// Collect the Password
		if( GetPassword( &Password, "" ) == VIM_ERROR_NONE )
		{
			// Check the password
			if( Password == SAF_DELETE_PASSWORD )
			{
				VIM_ERROR_RETURN_ON_FAIL(config_send_and_clear_saf());
			}
		}
	}

#endif
	return VIM_ERROR_NONE;

}


static VIM_ERROR_PTR config_delete(void *Ptr)
{
	VIM_KBD_CODE key_code;
	display_screen( DISP_CLEAR_CONFIG, VIM_PCEFTPOS_KEY_NONE );

	if (1)
	{
#ifndef _WIN32
        vim_kbd_read(&key_code, TIMEOUT_INACTIVITY);
#else
		vim_screen_kbd_or_touch_read( &key_code, TIMEOUT_INACTIVITY, VIM_FALSE);
#endif
		switch (key_code)
		{
	  	    case VIM_KBD_CODE_CANCEL:
		    case VIM_KBD_CODE_CLR:
				return VIM_ERROR_USER_CANCEL;

			case VIM_KBD_CODE_ASTERISK:
				if( terminal_get_status(ss_wow_basic_test_mode ))
					terminal_set_status( ss_tms_configured );
				return VIM_ERROR_NONE;

			case VIM_KBD_CODE_OK:
			display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING CONFIG", "");
			vim_file_delete("~/CPAT");
			vim_file_delete("~/EPAT");
			vim_file_delete("~/PKT");
			vim_file_delete("~/FCAT");
#if 0
			vim_file_delete("~/terminal.dat");
			vim_file_delete("~/saftot.bin");
			vim_file_delete("~/saf.bin");
			vim_file_delete("~/version.cfg");
			vim_file_delete("~/TMS_STATUS.CFG");
#endif
			display_screen( DISP_DELETING_FILES, VIM_PCEFTPOS_KEY_NONE );
#if 0
			vim_event_sleep(1000); // wait 1 sec
			CheckFiles(VIM_TRUE);
			terminal_save();

			return VIM_ERROR_USER_CANCEL;
#else
			// RDD 201212 SPIRA:6440
			// RDD 010716 SPIRA:9006
			terminal.file_version[CPAT_FILE_IDX] = 0;
			terminal.file_version[EPAT_FILE_IDX] = 0;
			terminal.file_version[PKT_FILE_IDX] = 0;
			terminal.file_version[FCAT_FILE_IDX] = 0;

			terminal.logon_state = logon_required;

			break;
#endif


			case VIM_KBD_CODE_KEY1:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING CPAT", "");
				vim_file_delete("~/CPAT");
				// RDD 010716 SPIRA:9006
				terminal.file_version[CPAT_FILE_IDX] = 0;
				terminal.logon_state = logon_required;
			}
			break;

			case VIM_KBD_CODE_KEY2:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING EPAT", "");
				vim_file_delete("~/EPAT");
				// RDD 010716 SPIRA:9006
				terminal.file_version[EPAT_FILE_IDX] = 0;
				terminal.logon_state = logon_required;
			}
			break;

			case VIM_KBD_CODE_KEY3:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING PKT", "");
				vim_file_delete("~/PKT");
				// RDD 010716 SPIRA:9006
				terminal.file_version[PKT_FILE_IDX] = 0;
				terminal.logon_state = logon_required;

			}
			break;

			case VIM_KBD_CODE_KEY4:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING FCAT", "");
				vim_file_delete("~/FCAT");
				// RDD 010716 SPIRA:9006
				terminal.file_version[FCAT_FILE_IDX] = 0;
				terminal.logon_state = logon_required;
			}
			break;

			case VIM_KBD_CODE_KEY5:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DEL VERSION.CFG", "");
				vim_file_delete("~/VERSION.CFG");
			}
			break;

			case VIM_KBD_CODE_KEY6:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "RESET LFD", "");
				vim_file_delete("~/TMS_STATUS.CFG");
                vim_file_delete("~/TMS_SW.CFG");
                vim_file_delete("~/VERSION.CFG");
                vim_file_copy("~/RESET.CFG", "~/VERSION.CFG" );
			}
			break;

			case VIM_KBD_CODE_KEY9:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				vim_file_delete("~/terminal.dat");
				vim_file_delete("~/saftot.bin");
				vim_file_delete("~/saf.bin");
				vim_file_delete("~/version.cfg");
				vim_file_delete("~/TMS_STATUS.CFG");

				pa_destroy_batch();
				vim_terminal_reboot();

				display_screen( DISP_DELETING_FILES, VIM_PCEFTPOS_KEY_NONE );
			}
			break;

			case VIM_KBD_CODE_KEY0:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETE REVERSAL", "");
				vim_file_delete("~/reversal.dat");
				terminal.reversal_count = 0;
			}
			break;

            case VIM_KBD_CODE_HASH:
                if (terminal_get_status(ss_wow_basic_test_mode)) {
                    display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETE CTLS SCHEME SPECIFIC XML", "");
                    vim_file_delete(VIM_FILE_PATH_LOCAL"EMV_CTLS_Apps_SchemeSpecific.xml");
                }
                break;

			default:
			vim_kbd_sound ();
			return VIM_ERROR_NONE;
		}
		vim_event_sleep(1000); // wait 1 sec
		CheckFiles(VIM_TRUE);
		terminal_save();
		// RDD 020512 SPIRA 5419: Put Pinpad into "W" state if filed removed
		// RDD 070612 SPIRA 5497: but only if in logged on state
	}
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR saf_fill(void *Ptr)
{
	VIM_SIZE n, count = 0;
	VIM_CHAR AsciiCount[3+1];

#ifndef _DEBUG
    if (!terminal_get_status(ss_wow_basic_test_mode))
	{
		return VIM_ERROR_NONE;
	}
#endif
	if( terminal.configuration.efb_max_txn_count > 2 )
	{
		count = terminal.configuration.efb_max_txn_count - 2;
		vim_snprintf( AsciiCount, VIM_SIZEOF(AsciiCount), "%3.3d", count );
	    VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_FILLING_SAF, VIM_PCEFTPOS_KEY_NONE, AsciiCount ));
	}
	else
	{
	    VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_SYSTEM_ERROR, VIM_PCEFTPOS_KEY_NONE, "ZERO SAF LIMIT" ));
	}

	if(( txn_duplicate_txn_read( VIM_TRUE ) == VIM_ERROR_NONE ) && ( count ))
	{
		/* copy back to txn */
		vim_mem_copy( &txn, &txn_duplicate, VIM_SIZEOF(txn));

		// RDD 030412 - need to fudge the status as this is no longer read in the duplicate
		transaction_set_status( &txn, st_needs_sending );
		// RDD 020212 - For security Transaction MUST Be a Z1 advice to enable staorage (ie. financially irrelevent)
#ifndef _DEBUG
		if( vim_strncmp( txn.host_rc_asc, "Z1", 2 ))
		{
		    VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_SYSTEM_ERROR, VIM_PCEFTPOS_KEY_NONE, "LAST TXN NOT Z1" ));
			DBG_MESSAGE( txn.host_rc_asc );
		}
#else
		if(0){}
#endif

		else
		{
			//DBG_MESSAGE( "**** FILLING SAF ****" );
			//DBG_VAR( count );
			for( n=0; n<count; n++ )
			{
				txn.amount_total = txn.amount_purchase = 10000 + ( 100*n );		// Set the purchase amount to $100 + $n
				if( saf_add_record( &txn, VIM_FALSE )  != VIM_ERROR_NONE )
				{
					VIM_CHAR Buff[40];
					vim_snprintf( Buff, VIM_SIZEOF(Buff), "ERR SAV TXN: %3.3d", n );
				    VIM_ERROR_IGNORE( display_screen( DISP_SYSTEM_ERROR, VIM_PCEFTPOS_KEY_NONE, Buff ));
				}
				// RDD 030412 - ensure that the txn is deleted in case of early exit. We don't want it in the buffer
			}
			txn_init();
		}
	}
	vim_event_sleep(2000); // wait 1 sec
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR saf_set_limit( void *Ptr )
{
  char data_buf[4];
  kbd_options_t options;

#ifndef _DEBUG
  if (!terminal_get_status(ss_wow_basic_test_mode))
  {
	return VIM_ERROR_NONE;
  }
#endif
  options.timeout = ENTRY_TIMEOUT;
  options.min = 1;
  options.max = 3;
  options.entry_options = KBD_OPT_NO_LEADING_ZERO;
  options.CheckEvents = VIM_FALSE;

  vim_mem_clear(data_buf, sizeof(data_buf));

#if 1
  // RDD 201218 Create a generic function to handle display and data entry in all platforms
  VIM_ERROR_RETURN_ON_FAIL(display_data_entry(data_buf, &options, DISP_ENTER_SAF_LIMIT, VIM_PCEFTPOS_KEY_NONE, 0));
#else
  VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_ENTER_SAF_LIMIT, VIM_PCEFTPOS_KEY_NONE, 0 ));
  VIM_ERROR_RETURN_ON_FAIL( data_entry( data_buf, &options ));
#endif

  terminal.configuration.efb_max_txn_count = vim_atol(data_buf);

  // RDD 290415 Donna requested we set the Preauth Batch limit at the same time
  // RDD 160523 JIRA PIRPIN-2383 This is fixed at 999 in the rest of the code
  // terminal.configuration.PreauthBatchSize = terminal.configuration.efb_max_txn_count;
  // DBG_VAR( terminal.configuration.efb_max_txn_count );

  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR LoadCtlsParams(void *Ptr)
{
	return picc_open( VIM_TRUE, VIM_TRUE );
}


/*----------------------------------------------------------------------------*/

// "3824" Functions : Config Display

/*----------------------------------------------------------------------------*/

static const VIM_CHAR vss_nfc_log[] = "vss_nfc.log";

static VIM_ERROR_PTR config_display_db_status(void *Ptr)
{
    VIM_CHAR dbFiles[30];

    vim_mem_clear(dbFiles, VIM_SIZEOF(dbFiles));

    if (TerminalDBStatus(VIM_FALSE) == VIM_ERROR_NONE)
    {
        vim_strcpy(dbFiles, "VAS db Files Present");
    }
    else
    {
        vim_strcpy(dbFiles, "VAS db Files Missing");
    }
    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, " [ dB Files Status ] ", dbFiles );
    vim_event_sleep(5000);

    return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR config_display_vss_log(void *Ptr)
{
    VIM_CHAR Buffer[1000];
    VIM_SIZE Size = 0;
    //VIM_BOOL Exists = VIM_FALSE;
    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    if(vim_file_is_present(vss_nfc_log))
    {
        vim_file_get( VIM_FILE_PATH_LOGS "vss_nfc.log", Buffer, &Size, VIM_SIZEOF( Buffer ));

        if (Size)
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, " [ vss_nfc.log ] ", Buffer );
            pceft_debug(pceftpos_handle.instance, Buffer);
        }
        else
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "vss_nfc.log", "Err: zero length" );
            pceft_debug(pceftpos_handle.instance, "vss_nfc.log exists but zero len" );
        }
    }
    else
    {
        display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "vss_nfc.log", "Err: File not Found");
        pceft_debug(pceftpos_handle.instance, "vss_nfc.log : file not found" );
    }
    vim_event_sleep(5000);

    if (config_get_yes_no_timeout("Delete vss_nfc.log ?", "Deleting...", 30000, VIM_FALSE) == VIM_TRUE)
    {
        vim_file_delete(VIM_FILE_PATH_LOGS "vss_nfc.log");
        vim_event_sleep(1000);

        if (vim_file_is_present(vss_nfc_log))
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Delete Failed", "");
        }
        else
        {
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Delete Success", "");
        }
    }
    vim_event_sleep(1000);
    return VIM_ERROR_NONE;
}




VIM_ERROR_PTR config_display_software_version(void *Ptr)
{
	VIM_CHAR disp_buf[150];
	VIM_CHAR ver_buf[100];
    VIM_CHAR VrkBuf[50];
	VIM_SIZE ver_size = 0;
    VIM_ERROR_PTR res;
    VIM_UINT8 VrkStatus;
    VIM_TEXT EOS_version[50];

	vim_mem_clear( ver_buf, VIM_SIZEOF( ver_buf ));
    vim_mem_clear( VrkBuf, VIM_SIZEOF( VrkBuf ));

	picc_open( PICC_OPEN_ONLY, VIM_FALSE );

    if ((res = vim_terminal_get_vrk_status(&VrkStatus)) != VIM_ERROR_NONE)
    {
        vim_strcpy(VrkBuf, "VRK Status = ERROR");
    }
    else
    {
        vim_sprintf(VrkBuf, "VRK Status = %d", VrkStatus);
    }

    // Throw away the first 8 bytes of the CTLS ver as it's not needed
    if (vim_picc_emv_get_firmware_version(picc_handle.instance, ver_buf, VIM_SIZEOF(ver_buf), &ver_size) != VIM_ERROR_NONE)
    {
        vim_strcat(&ver_buf[8], "<CTLS ERROR>");
    }

    vim_mem_clear(EOS_version, VIM_SIZEOF(EOS_version));
    vim_terminal_get_EOS_info(EOS_version, VIM_SIZEOF(EOS_version));
    vim_sprintf( disp_buf, "v%8d.%2d\n%s\n%s\nCTLS:%s", BUILD_NUMBER, REVISION, EOS_version, VrkBuf, ver_buf );
    display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, disp_buf, terminal_vas_log_status() );

	picc_close( VIM_TRUE );
    //vim_kbd_read(VIM_NULL, 5000);

	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_display_terminal_id(void *Ptr)
{
  //VIM_DBG_VAR(terminal.terminal_id);
  VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TERMINAL_ID, VIM_PCEFTPOS_KEY_NONE, terminal.terminal_id));
  //VIM_ERROR_RETURN_ON_FAIL(vim_screen_kbd_or_touch_read(VIM_NULL, TIMEOUT_INACTIVITY));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_display_merchant_id(void *Ptr)
{
  //VIM_DBG_VAR(terminal.merchant_id);
  VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_MERCHANT_ID, VIM_PCEFTPOS_KEY_NONE, terminal.merchant_id));
  //VIM_ERROR_RETURN_ON_FAIL(vim_screen_kbd_or_touch_read(VIM_NULL, TIMEOUT_INACTIVITY));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_display_ppid(void *Ptr)
{
  VIM_DES_BLOCK ppid;
  char ascii_ppid[16];

  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_ppid(tcu_handle.instance_ptr, (VIM_DES_BLOCK *)&ppid));
  bcd_to_asc(ppid.bytes, ascii_ppid, 16);
  VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_PPID, VIM_PCEFTPOS_KEY_NONE, ascii_ppid));
  //VIM_ERROR_RETURN_ON_FAIL(vim_screen_kbd_or_touch_read(VIM_NULL, TIMEOUT_INACTIVITY));
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
#if 0
static VIM_ERROR_PTR config_display_saf_print_count(void)
{
  VIM_SIZE saf_count = reversal_present() ? saf_totals.fields.saf_count+1 : saf_totals.fields.saf_count;
  //VIM_ERROR_RETURN_ON_FAIL( display_screen(DISPLAY_SCREEN_SAF_TRANS, VIM_PCEFTPOS_KEY_NONE, "SAF PRINT COUNT", saf_totals.fields.saf_print_count ));
  //VIM_ERROR_RETURN_ON_FAIL(vim_screen_kbd_or_touch_read(VIM_NULL, TIMEOUT_INACTIVITY));
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_display_saf_and_tip_count(void *Ptr)
{
  VIM_SIZE QCSafCount = 0, EFTReversal = 0, QCReversal = 0, QCTotal = 0;
  VIM_CHAR Display1[100] = {0}, Display2[100] = {0}, Display3[100] = {0};

  saf_pending_count( &saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count );

  EFTReversal = reversal_present() ? 1 : 0;

  vim_snprintf( Display1, VIM_SIZEOF(Display1), "EFT SAF %3.3d", saf_totals.fields.saf_count + EFTReversal );
  if (reversal_present())
	  vim_strcat(Display1, "(r)");
	  
  if (TERM_ALLOW_PREAUTH)
  {		  
	  VIM_SIZE PendingCount = 0;
	  VIM_SIZE PendingTotal = 0;
	  preauth_get_pending(&PendingCount, &PendingTotal, VIM_FALSE);	
	  vim_snprintf(Display2, VIM_SIZEOF(Display2), "PRE AUTH %3.3d", PendingCount);		 
  }

  if((TERM_QC_GIFT_DISABLE == VIM_FALSE) && (QC_IsInstalled(VIM_FALSE)))
  {
	  QC_GetSAFCount(&QCTotal, &QCReversal, &QCSafCount );
	  vim_snprintf(Display3, VIM_SIZEOF(Display3), "QC SAF %3.3d", QCTotal);
	  if(QCReversal)		
		  vim_strcat(Display3, "(r)");
  }

  VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_SAF_TRANS, VIM_PCEFTPOS_KEY_NONE, Display1, Display2, Display3 ));

  return VIM_ERROR_NONE;
}



static VIM_ERROR_PTR saf_maintenance_functions(void)
{
  static CONFIG_KEY_MAP const map[]=
  {
    {VIM_KBD_CODE_KEY1,saf_delete},
    {VIM_KBD_CODE_KEY2,config_delete},
    {VIM_KBD_CODE_KEY3,saf_fill},
    {VIM_KBD_CODE_KEY4,saf_set_limit},
    {VIM_KBD_CODE_KEY5,LoadCtlsParams},
    {VIM_KBD_CODE_KEY6,terminal_reset_statistics},
    {VIM_KBD_CODE_KEY7,terminal_debug_statistics},
    {VIM_KBD_CODE_NONE,VIM_NULL}
  };

  VIM_ERROR_RETURN_ON_FAIL(config_key_map_run( DISP_SAF_FUNCTIONS, map ));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_UINT8 days_in_month(VIM_UINT8 month, VIM_UINT16 year)
{
  VIM_UINT8 number_of_days;

  if (month == 4 || month == 6 || month == 9 || month == 11)
    number_of_days = 30;
  else
  if (month == 2)
  {
    VIM_BOOL is_leap_year = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);

    if (is_leap_year)
      number_of_days = 29;
    else
      number_of_days = 28;
  }
  else
    number_of_days = 31;
  return number_of_days;
}

#if 0
static VIM_ERROR_PTR config_business_division_menu(void)
{
	VIM_KBD_CODE key_pressed;
	//Strings are sent here (rather than in the screen struct in display.c) so that if divisions are added/removed this is the only function that needs editing
	//UI Filename #defines in defs.h
	display_screen(DISP_BUSINESS_DIV_MENU, VIM_PCEFTPOS_KEY_NONE, "1 - Woolworths\n2 - Big W\n3 - BWS\n4 - Dan Murphy's\n5 - NBL\n6 - Fuel Stn");
	VIM_ERROR_RETURN_ON_FAIL( screen_kbd_touch_read_with_abort(&key_pressed, ENTRY_TIMEOUT, VIM_FALSE ));

	switch(key_pressed)
	{

	case(VIM_KBD_CODE_KEY1):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "Woolworths Selected");
    set_Woolworths_logo();
		break;

	case(VIM_KBD_CODE_KEY2):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "Big W Selected");

    set_Bigw_logo ();
		break;

	case(VIM_KBD_CODE_KEY3):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "BWS Selected");

    set_BWS_logo ();
		break;

	case(VIM_KBD_CODE_KEY4):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "Dan Murphy's Selected");

    set_DanM_logo ();
		break;

	case(VIM_KBD_CODE_KEY5):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "NB Liquor Selected");

    set_NBL_logo ();
		break;

	case(VIM_KBD_CODE_KEY6):
		display_screen(DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "Fuel Stn Selected");

    set_Fuel_logo ();
		break;


	default:
		break;
	}
	terminal_save();
	return VIM_ERROR_NONE;
}
#endif

static VIM_ERROR_PTR config_part_number(void *Ptr)
{
	VIM_CHAR PartNumber[20];
    VIM_KBD_CODE key;

	//VIM_SIZE Len;
	vim_terminal_get_part_number( PartNumber, VIM_SIZEOF(PartNumber));

	display_screen( DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, PartNumber );

    // Wait for any kepress
    vim_kbd_read(&key, TIMEOUT_INACTIVITY);

	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_maintenance_functions(void)
{
  static CONFIG_KEY_MAP const map[]=
  {
    {VIM_KBD_CODE_KEY0,config_display_software_version},
    {VIM_KBD_CODE_KEY1,config_display_terminal_id},
    {VIM_KBD_CODE_KEY2,config_display_merchant_id},
    {VIM_KBD_CODE_KEY3,config_display_ppid},
	{VIM_KBD_CODE_KEY4,config_display_db_status},               // RDD 260520 Trello #147 - production issues with incorrectly loaded terminals
    {VIM_KBD_CODE_KEY6,config_display_saf_and_tip_count},
    {VIM_KBD_CODE_KEY7,config_display_vss_log},		            // RDD 260520 Trello #147 - production issues with incorrectly loaded terminals
    {VIM_KBD_CODE_KEY9,config_card_read_test},

    //{VIM_KBD_CODE_KEY7,config_change_merchant_refund_password},
    {VIM_KBD_CODE_KEY8,config_part_number},
    //{VIM_KBD_CODE_KEY9,config_card_read_test},
    {VIM_KBD_CODE_NONE,VIM_NULL}
  };

  VIM_ERROR_RETURN_ON_FAIL(config_key_map_run(DISP_MAINTENANCE_FUNCTIONS,map));

  /* return without error */
  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR validate_date(VIM_CHAR *date_buf)
{
  VIM_UINT8 dd;
  VIM_UINT8 mm;
  VIM_UINT16 yy;
  VIM_RTC_TIME new_date;

  /* Convert time */
  dd   = asc_to_bin(date_buf, 2);
  mm = asc_to_bin(&date_buf[2], 2);
  yy   = asc_to_bin(&date_buf[4], 4);

  /* Validate month */
  if ( (mm<1) || (mm > 12))
    return ERR_OUT_OF_RANGE;

  if ( dd > days_in_month(mm, yy))
    return ERR_OUT_OF_RANGE;

  /* read existing date to get year */
  vim_rtc_get_time(&new_date);

  /* Validate year */
  new_date.day = dd;
  new_date.month = mm;
  new_date.year = yy;

  VIM_DBG_VAR( new_date.year );

  VIM_DBG_ERROR(vim_rtc_set_time(&new_date));
  VIM_DBG_ERROR(vim_rtc_get_time(&new_date));

  VIM_DBG_VAR( new_date.year );

  return VIM_ERROR_NONE;
}




/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR validate_time(VIM_CHAR *time_buf, VIM_BOOL PCI_Reboot_Time)
{
  VIM_UINT8 hh;
  VIM_UINT8 mm;
  VIM_RTC_TIME new_date;

  /* Convert time */
  hh   = asc_to_bin(time_buf, 2);
  mm = asc_to_bin(&time_buf[2], 2);

  /* Validate it */
  if ((hh > 23) || (mm > 59))
    return ERR_OUT_OF_RANGE;

  /* read existing date to get year */
  vim_rtc_get_time(&new_date);
  new_date.hour = hh;
  new_date.minute = mm;

  if (PCI_Reboot_Time)
  {
      vim_terminal_set_pci_reboot_time( &new_date, VIM_TRUE );
  }
  else
  {
      vim_rtc_set_time(&new_date);
  }
  return VIM_ERROR_NONE;
}




#if 0
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR EditParameter( VIM_SIZE DisplayMessage, VIM_CHAR *AsciiParam, VIM_SIZE ParamMaxLen, VIM_BOOL AllowAlpha )
{
  //VIM_KBD_CODE key_code;
  kbd_options_t options;
  VIM_CHAR NewParam[50];
  VIM_SIZE AsciiLen;

  vim_mem_clear( NewParam, VIM_SIZEOF(NewParam) );

  AsciiLen = VIM_MIN( ParamMaxLen, vim_strlen(AsciiParam) );

  options.timeout = ENTRY_TIMEOUT;
  options.entry_options = AllowAlpha ? KBD_OPT_ALPHA|KBD_OPT_DISP_ORIG : KBD_OPT_DISP_ORIG;
  options.min = 0;
  options.max = ParamMaxLen;
  options.CheckEvents = VIM_FALSE;

  // Copy and display the original number
  vim_mem_copy( NewParam, AsciiParam, AsciiLen );

#if 0
  // RDD 010414 SPIRA:7399
  display_screen(DisplayMessage, VIM_PCEFTPOS_KEY_NONE, NewParam);
  VIM_ERROR_RETURN_ON_FAIL( data_entry( NewParam, &options ));
#else
  // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
  VIM_ERROR_RETURN_ON_FAIL(display_data_entry(NewParam, &options, DISP_TERMINAL_CONFIG_TERMINAL_ID, DisplayMessage, VIM_PCEFTPOS_KEY_NONE, NewParam));
#endif

  // update the length
  AsciiLen = vim_strlen( NewParam );

  // convert the New number to BCD format and put into the correct spot
  vim_strcpy( AsciiParam, NewParam );
  terminal_save();

  return VIM_ERROR_NONE;
}


#define ASC_NII_SIZE 4

#if 0
static VIM_ERROR_PTR config_pos_fme_or_nii(void)
{
	VIM_CHAR AsciiNii[ASC_NII_SIZE+1];
	VIM_UINT8 *NiiPtr = terminal.acquirers[0].nii;

	vim_mem_clear( AsciiNii, ASC_NII_SIZE );
	bcd_to_asc( NiiPtr, AsciiNii, ASC_NII_SIZE );

	EditParameter( DISPLAY_SCREEN_NII, AsciiNii, ASC_NII_SIZE, VIM_FALSE );
	asc_to_bcd( AsciiNii, NiiPtr, ASC_NII_SIZE );

	return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_transend_id(void)
{
  kbd_options_t options;
  VIM_CHAR date_buf[9];

  vim_mem_clear(date_buf, sizeof(date_buf));

  options.timeout = ENTRY_TIMEOUT;
  options.entry_options = KBD_OPT_DISP_ORIG;
  options.min = 8;
  options.max = 8;

  display_screen(DISP_TERMINAL_CONFIG_TRANSSEND_ID, VIM_PCEFTPOS_KEY_NONE);

  VIM_ERROR_RETURN_ON_FAIL(data_entry(date_buf, &options));

  vim_mem_copy(terminal.comms.transsend, date_buf, VIM_SIZEOF(terminal.comms.transsend));

  return VIM_ERROR_NONE;
}
#endif


VIM_ERROR_PTR AddPABXToDialString( VIM_NET_INTERFACE_CONNECT_PARAMETERS *NetPtr )
{
	VIM_CHAR TempNumber[50];

	// RDD 141215 Clear Temp Number else we risk appending more and more pabx digits
	vim_mem_clear( TempNumber, VIM_SIZEOF( TempNumber ));

	// Read the current primary, append to the PABX then read back into the struct
	vim_strcpy( TempNumber, terminal.comms.pabx );
	vim_strcat( TempNumber, NetPtr->pstn_hdlc.dial.phone_number );
	vim_strcpy( NetPtr->pstn_hdlc.dial.phone_number, TempNumber );
	// Read the current secondary, append to the PABX then read back into the struct
	vim_strcpy( TempNumber, terminal.comms.pabx );
	vim_strcat( TempNumber, NetPtr->pstn_hdlc.dial.phone_number2 );
	vim_strcpy( NetPtr->pstn_hdlc.dial.phone_number2, TempNumber );

	return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/

#define MAX_PACK_DIGITS 4
// RDD 010414 SPIRA:7399
#if 0
static VIM_ERROR_PTR config_tms_packet_len(void)
{
  //VIM_KBD_CODE key_code;
  //kbd_options_t options;
  VIM_CHAR TmsPacketLen[50];
  //VIM_SIZE AsciiLen;

  vim_mem_clear( TmsPacketLen, VIM_SIZEOF(TmsPacketLen));
  bin_to_asc( terminal.tms_parameters.max_block_size, TmsPacketLen, MAX_PACK_DIGITS );

  VIM_ERROR_RETURN_ON_FAIL( EditParameter( DISP_TMS_PACKET_LEN, TmsPacketLen, MAX_PACK_DIGITS, VIM_FALSE ));

  terminal.tms_parameters.max_block_size = asc_to_bin( TmsPacketLen, vim_strlen( TmsPacketLen ));
  return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR config_test_string(void)
{
  //VIM_KBD_CODE key_code;
  //kbd_options_t options;
  VIM_CHAR PacketLen[5];
  VIM_SIZE Len;

  vim_mem_clear( DummyString, VIM_SIZEOF(DummyString));
  vim_mem_clear( PacketLen, VIM_SIZEOF(PacketLen));

  // Max 999
  VIM_ERROR_RETURN_ON_FAIL( EditParameter( DISP_TEST_PACKET_LEN, PacketLen, 3, VIM_FALSE ));

  Len = asc_to_bin( PacketLen, vim_strlen( PacketLen ));
  vim_mem_set( DummyString, 'X', Len );
  return VIM_ERROR_NONE;
}

// RDD 010414 SPIRA:7399
static VIM_ERROR_PTR config_host_phone_number(void)
{
  //VIM_KBD_CODE key_code;
  //kbd_options_t options;
  VIM_CHAR PhoneNumberAscii[50];
  VIM_SIZE AsciiLen;

  vim_mem_clear( PhoneNumberAscii, VIM_SIZEOF(PhoneNumberAscii));

  bcd_to_asc_chk( terminal.acquirers[0].primary_phone_no, PhoneNumberAscii, VIM_SIZEOF( terminal.acquirers[0].primary_phone_no ) * 2 );
  AsciiLen = VIM_MIN( VIM_SIZEOF( terminal.acquirers[0].primary_phone_no ) * 2, vim_strlen(PhoneNumberAscii) );

  VIM_ERROR_RETURN_ON_FAIL( EditParameter( DISP_TERMINAL_CONFIG_TMS_TELEPHONE_NO, PhoneNumberAscii, 12, VIM_FALSE ));

  AsciiLen = vim_strlen( PhoneNumberAscii );

  // convert the New number to BCD format and put into the correct spot
  asc_to_bcd( PhoneNumberAscii, terminal.acquirers[0].primary_phone_no, AsciiLen );
  terminal.logon_state = ktm_logon;
  terminal_save();

  return VIM_ERROR_NONE;
}
#endif


VIM_ERROR_PTR InitGPRS( void )
{
	VIM_DBG_ERROR( display_screen( terminal.IdleScreen, VIM_PCEFTPOS_KEY_NONE, "INIT MOBILE DATA", "NET CONNECT" ));
	VIM_ERROR_RETURN_ON_FAIL( comms_connect_internal( HOST_WOW ));
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

VIM_BOOL GPRS_Available( void )
{
	return ( InitGPRS( ) == VIM_ERROR_NONE ) ? VIM_TRUE : VIM_FALSE;
}






/*----------------------------------------------------------------------------*/

// RDD 2908 JIRA PIRPIN-2661 Malformed reversal gets stuck and blocks transactions
static VIM_ERROR_PTR saf_send_and_clear_repeat( void )
{
	saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);

	pceft_debugf(pceftpos_handle.instance, "#6566: SAFS Pending = %d", saf_totals.fields.saf_count);
	if (saf_totals.fields.saf_repeat_pending)
	{
		pceft_debug(pceftpos_handle.instance, "#6566 : 0221 Pending: Try to Send");
		saf_send(VIM_TRUE, TRICKLE_FEED_COUNT, VIM_FALSE, VIM_TRUE);
	}
	else if( reversal_present() )
	{
		pceft_debug(pceftpos_handle.instance, "#6566: 0421 Pending: Try to Send");
		// load up the reversal from the reversal file
		VIM_ERROR_RETURN_ON_FAIL( reversal_load(&reversal) );

		if( reversal.status & st_repeat_saf )
		{
			// Try to send the reversal, but clear it anyway if it doesn't go
			reversal_send( VIM_TRUE );
			pceft_debug(pceftpos_handle.instance, "#6566: Clear Reversal and upload any pending 0220");
			reversal_clear( VIM_TRUE );
			saf_send(VIM_FALSE, MAX_BATCH_SIZE, VIM_FALSE, VIM_TRUE);
			saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);

			pceft_debugf(pceftpos_handle.instance, "#6566: SAFS Pending = %d", saf_totals.fields.saf_count);
		}
	}
	else
		display_screen(DISP_NO_REPEATS_FOUND, VIM_PCEFTPOS_KEY_NONE);
#ifdef _DEBUG
		if( ValidateConfigData( terminal.terminal_id, terminal.merchant_id ))
			terminal_set_status( ss_tid_configured );
#endif
		vim_event_sleep(2000);
		comms_disconnect();
		return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR config_get_key_clr_ok(VIM_KBD_CODE* pKeyCode)
{
	VIM_KBD_CODE key_code;
	VIM_BOOL loop = VIM_TRUE;

	VIM_RTC_UPTIME uptime, end_time;

	// RDD 230516 SPIRA:8957
	vim_rtc_get_uptime(&uptime);
	end_time = uptime + ENTRY_TIMEOUT;

	do
	{
		vim_kbd_read(&key_code, 50);
		vim_rtc_get_uptime(&uptime);
		switch (key_code)
		{
		case VIM_KBD_CODE_CLR:
		case VIM_KBD_CODE_OK:
			*pKeyCode = key_code;
			return VIM_ERROR_NONE;

		default:
			break;
		}
	} while (loop && (uptime < end_time));
	return VIM_ERROR_TIMEOUT;
}


// RDD 290823 JIRA PIRPIN_2661 Allow reversals to be cleared remotely
static VIM_ERROR_PTR config_send_and_clear_repeat(void)
{
  VIM_KBD_CODE key_code = 0;
  VIM_ERROR_PTR result;

  display_screen(DISP_CLEAR_SAF, VIM_PCEFTPOS_KEY_NONE, "");
  result = config_get_key_clr_ok(&key_code);

  if ((VIM_ERROR_NONE == result) && (VIM_KBD_CODE_OK == key_code))
	  saf_send_and_clear_repeat();
  return result;
}


static VIM_ERROR_PTR config_qc_send_saf(VIM_BOOL RunFromTMS)
{
    display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "UPLOADING QC SAF", "");	
    QC_SAFUpload(TRICKLE_FEED_COUNT);
	QC_SAFClear();
    return VIM_ERROR_NONE;
}


#if 0
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR set_hostID(void)
{
  display_screen(DISP_HOST_ID, VIM_PCEFTPOS_KEY_NONE, terminal.host_id);
  VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( VIM_NULL, ENTRY_TIMEOUT, VIM_FALSE ));
  return VIM_ERROR_NONE;
}
#endif

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_test_logon(VIM_UINT8 type)
{
  // RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
  terminal.last_logon_state = terminal.logon_state;

  if( type == 0 )
  {
    display_screen(DISP_I3070_USB_RS232,VIM_PCEFTPOS_KEY_NONE, "Full Logon", "Please Wait", "" );
    terminal.logon_state = ktm_logon;
  }
  if( type == 1 )
  {
    display_screen(DISP_I3070_USB_RS232,VIM_PCEFTPOS_KEY_NONE, "RSA Logon", "Please Wait", "" );
    terminal.logon_state = rsa_logon;
  }
  /* request logon */

  init.error = logon_request( VIM_FALSE );

  /* prcess return error */
  logon_finalisation( VIM_FALSE );

  /* return error */
  return init.error;
}




/*----------------------------------------------------------------------------*/




VIM_ERROR_PTR GetPassword( VIM_INT32 *Password, VIM_CHAR *prompt )
{
	VIM_CHAR password_buf[WOW_MAX_PASSWORD_LEN+1];
	kbd_options_t options;

	options.timeout = ENTRY_TIMEOUT;
	options.min = WOW_MIN_PASSWORD_LEN;
	options.max = WOW_MAX_PASSWORD_LEN;
	options.entry_options = KBD_OPT_MASKED;
	options.CheckEvents = VIM_FALSE;

	vim_mem_clear(password_buf, sizeof(password_buf));

#ifdef _WIN32
	VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_FUNCTION, VIM_PCEFTPOS_KEY_CANCEL, prompt));
	VIM_ERROR_RETURN_ON_FAIL( data_entry(password_buf, &options ));
#else
    VIM_DBG_POINT;
    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(password_buf, &options, DISP_FUNCTION, VIM_PCEFTPOS_KEY_CANCEL, prompt ));
#endif
	pceft_pos_display_close();

	*Password = vim_atol( password_buf );
    VIM_DBG_NUMBER(*Password);
	return VIM_ERROR_NONE;
}




VIM_BOOL GetAnswer( VIM_CHAR *PinPadText )
{
	VIM_KBD_CODE key_code;
	VIM_BOOL Result = VIM_FALSE;

	display_screen( DISP_GET_ANSWER, VIM_PCEFTPOS_KEY_NONE, PinPadText );

	// wait for a response from the customer
	while(1)
	{
		if( vim_kbd_read( &key_code, ENTRY_TIMEOUT ) == VIM_ERROR_NONE  )
		{
			switch( key_code )
			{
				case VIM_KBD_CODE_OK: return VIM_TRUE;
				case VIM_KBD_CODE_CANCEL:
				case VIM_KBD_CODE_CLR: return VIM_FALSE;
				default:
					break;
			}
		}
		else // Timeout error
		{
			vim_kbd_sound_invalid_key();
			break;
		}
	}
	return Result;
}





/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_reboot_pinpad( VIM_BOOL RunFromTms )
{
  VIM_KBD_CODE key_code=VIM_KBD_CODE_NONE;
  if( !RunFromTms )
  {
	  pceft_debug(pceftpos_handle.instance, "Function number \"99\" entered" );
	  display_screen( DISP_REBOOT_CONFIRMATION, VIM_PCEFTPOS_KEY_NONE );
	  while( VIM_TRUE )
	  {
		  VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read (&key_code, VIM_MILLISECONDS_INFINITE));
		  if( key_code == VIM_KBD_CODE_CANCEL || key_code == VIM_KBD_CODE_CLR )
			break;

		  if( key_code == VIM_KBD_CODE_OK )
		  {
			  // RDD 011217 WWBP-69 this line kills CTLS if reboot cancelled so move to after confim
			  terminal_set_status(ss_wow_reload_ctls_params);
			  display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Rebooting Terminal...", "");
			  VIM_ERROR_RETURN_ON_FAIL( vim_terminal_reboot () );
			  break;
		  }
	  }
  }
  else
  {
	  // RDD 110413 - No confirmation if run from TMS
	  pceft_debug(pceftpos_handle.instance, "Function number \"99\" entered (R)" );
	  display_screen( DISP_REBOOT2, VIM_PCEFTPOS_KEY_NONE );
	  vim_event_sleep(1000);
	  display_screen( DISP_REBOOT1, VIM_PCEFTPOS_KEY_NONE );
	  vim_event_sleep(1000);
	  pceft_pos_display_close();
	  terminal_set_status(ss_wow_reload_ctls_params);
	  terminal_clear_status( ss_tms_configured );				//BD Ensure TMS contact
	  //terminal_save();											//BD
	  VIM_ERROR_RETURN_ON_FAIL( vim_terminal_reboot () );
  }
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR CheckSecurity( VIM_BOOL Renew )
{
	// This stuff not relevent for Internal networks
	if (IS_R10_POS)
		return VIM_ERROR_NONE;
	DBG_POINT;
	if (vim_file_is_present(CLIENT_CERT_PATH))
	{
		VIM_ERROR_PTR result = VIM_ERROR_NONE;
		VIM_SIZE ValidDays = 0;
		VIM_CHAR Buffer[100] = { 0 };

		result = vim_x509Validate(VIM_NULL, CLIENT_CERT, &ValidDays);

		VIM_DBG_NUMBER(ValidDays);

		vim_sprintf(Buffer, "Expires in %d days", ValidDays);

		// Expired
		if (result != VIM_ERROR_NONE)
		{
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Terminal Certifcate Expired", "");
			if (Renew)
			{
				// Go get a new client cert
				display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Terminal Certifcate", "Expired");
				PostCDSValidation(VIM_TRUE);
			}
		}
		// Expiring Soon 
		else if (ValidDays < CERTIFICATE_RENEWAL_WINDOW)
		{
			DBG_POINT; 
#if 1		
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Terminal Certificate", Buffer );
			vim_event_sleep(2000);
			if (Renew)
			{
				DBG_POINT;
				display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Renewal In Progress", "Please Wait");
				DBG_POINT;
				pceft_debug(pceftpos_handle.instance, Buffer);
				DBG_POINT;
				vim_event_sleep(1000);
				DBG_POINT;
				PostCDSValidation(VIM_TRUE);
			}
#endif
		}
		// Not Expiring soon
		else if(!Renew)
		{
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Terminal Certificate", Buffer);
			vim_event_sleep(2000);
		}
		pceft_debug_test(pceftpos_handle.instance, Buffer);
	}
	else
	{
		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Terminal Certifcate Missing", "Run #7410" );
	}
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_run_TMS( VIM_BOOL Continuous, tms_type_t tms_type )
{
  VIM_KBD_CODE key_code=VIM_KBD_CODE_NONE;
  VIM_UINT8 Count = 50;

  pceft_debug(pceftpos_handle.instance, "Function number \"3696\" entered" );

  if( Continuous )
  {
	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "RTM Soak Test", "OK To Continue" );
  }
  else
  {
	display_screen( DISP_TMS_CONFIRMATION, VIM_PCEFTPOS_KEY_NONE );
  }

  while( Count-- )
  {
#ifdef _DEBUG
    VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read (&key_code, 5000 ));
#else
    VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read (&key_code, 60000 ));
#endif
    if( key_code == VIM_KBD_CODE_CANCEL || key_code == VIM_KBD_CODE_CLR )
      break;
    if( key_code == VIM_KBD_CODE_OK )
    {
        error_t error_data;
        //vim_tms_reset();

		VIM_ERROR_PTR error = tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE, VIM_FALSE, tms_type );
        vim_tms_reset();
        error_code_lookup( error, "", &error_data );
        display_result( error, init.logon_response, error_data.terminal_line1, error_data.terminal_line1, VIM_PCEFTPOS_KEY_NONE, 2000 );

		if(( Continuous ) && ( error != VIM_ERROR_NONE ))
		{
			VIM_PCEFTPOS_RECEIPT receipt;
			receipt_build_logon( error, &receipt, error_data.pos_text, VIM_TRUE );
			pceft_pos_print( &receipt, VIM_TRUE );
		}
		else
		{
			return error;
		}
    }
  }

  return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_date_and_time_configuration(VIM_BOOL SetPCINextRebootTime )
{
  kbd_options_t options;
  VIM_CHAR date_buf[10];

  /* Initial values */
  vim_mem_clear(date_buf, sizeof(date_buf));
  options.timeout = ENTRY_TIMEOUT;
  options.entry_options = KBD_OPT_DISP_ORIG;
  options.CheckEvents = VIM_FALSE;
  options.min = 4;
  options.max = 4;

  // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
  VIM_ERROR_RETURN_ON_FAIL(display_data_entry(date_buf, &options, DISP_CHANGE_TIME, VIM_PCEFTPOS_KEY_NONE));

  /* check the time */
  VIM_ERROR_RETURN_ON_FAIL(validate_time(date_buf, SetPCINextRebootTime));

  return VIM_ERROR_NONE;
}



VIM_ERROR_PTR config_pceft_configure( void )
{
	VIM_ERROR_RETURN_ON_FAIL( pceft_configure( VIM_FALSE ));
	VIM_ERROR_RETURN_ON_FAIL( pceft_settings_reconfigure( VIM_FALSE ));
	VIM_ERROR_RETURN_ON_FAIL( pceft_settings_save() );
	return VIM_ERROR_NONE;
}



#ifdef _DEBUG
static VIM_ERROR_PTR auto_transaction_test( void )
{
  VIM_SIZE cnt = 0;
  VIM_SIZE err_cnt = 0;
  VIM_TEXT temp_buffer[64];

  saf_destroy_batch();
  reversal_clear(VIM_FALSE);
  while( VIM_TRUE )
  {
	txn_init();
	txn.emv_error = VIM_ERROR_NONE;
	txn.amount_purchase = 500;
	//  txn.amount_cash = 500;
	txn.amount_total = 500;
	txn.account = account_credit;
	txn.type = tt_sale;
	txn_save();
	txn_process();
	if( txn.emv_error == VIM_ERROR_EMV_FAULTY_CHIP )
	    err_cnt++;
	vim_mem_clear( temp_buffer, VIM_SIZEOF(temp_buffer));
	vim_snprintf( temp_buffer, VIM_SIZEOF(temp_buffer)-1, "%d Errs %d Txns", err_cnt, ++cnt );
	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, temp_buffer, "Processed" );
	//if( vim_kbd_read( VIM_NULL, 1500 ) != VIM_ERROR_TIMEOUT )
  	//	break;
	DBG_POINT;
	CheckMemory( VIM_FALSE );

	if( txn.error == VIM_ERROR_USER_CANCEL )
		break;
  }
  //saf_destroy_batch();	// Get rid of any offline txns

  return VIM_ERROR_NONE;
}
#endif


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR swap_terminal_mode(void)
{
    /* if runing in simple interface mode, close link layer first */
    if (IS_STANDALONE)
      display_screen(DISP_SWITCH_TO_INTEGRATED_MODE, VIM_PCEFTPOS_KEY_CANCEL);
    else
      display_screen(DISP_SWITCH_TO_STANDALONE_MODE, VIM_PCEFTPOS_KEY_CANCEL);

    if (VIM_ERROR_NONE == get_yes_no(ENTRY_TIMEOUT))    
	{
      if( IS_STANDALONE )
		  TermSetIntegratedMode();
      else
		  TermSetStandaloneMode();    
	}  
	return VIM_ERROR_NONE;
}


void ResetTMS(void)
{
    VIM_CHAR Buffer[200];
    vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

    VIM_ERROR_WARN_ON_FAIL(vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_HOST_VERSION_CFG));
    VIM_ERROR_WARN_ON_FAIL(vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_DOWNLOADING_STAUS));
    VIM_ERROR_WARN_ON_FAIL(vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LAST_CFG_FILENAME));
    VIM_ERROR_WARN_ON_FAIL(vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LAST_DLD_FILE));
    VIM_ERROR_WARN_ON_FAIL(vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LOCAL_VERSION_CFG));
	VIM_ERROR_WARN_ON_FAIL(vim_file_copy(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_DEFAULT_CFG, VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LOCAL_VERSION_CFG));

    pceft_debug(pceftpos_handle.instance, "Trello #170 : Deleted Temp LFD Files");
}



VIM_BOOL set_standalone_option(VIM_UINT8 *value, VIM_UINT8 mask)
{
  VIM_ERROR_PTR res;

  while (VIM_ERROR_USER_CANCEL != (res = get_yes_no(ENTRY_TIMEOUT)))
  {
    /* set option */
    if (VIM_ERROR_NONE == res)
    {
      *value |= mask;
      terminal_save();
      return VIM_TRUE;
    }

    /* clear option */
    if (VIM_ERROR_USER_BACK == res)
    {
      *value &= ~mask;
      terminal_save();
      return VIM_TRUE;
    }
  }

  return VIM_FALSE;
}

VIM_ERROR_PTR BlMin(void) { return vim_lcd_set_backlight(BRIGHTNESSLEVELNONE); }
VIM_ERROR_PTR BlLow(void) { return vim_lcd_set_backlight(BRIGHTNESSLEVELLOW); }
VIM_ERROR_PTR BlMed(void) { return vim_lcd_set_backlight(BRIGHTNESSLEVELMEDIUM); }
VIM_ERROR_PTR BlMax(void) { return vim_lcd_set_backlight(BRIGHTNESSLEVELINTENSE); }


#define FORCE_COALESCE_LIMIT 0x400000	// RDD 220313 - P4 best to set this well above the 2mB OS limit in order to avoid 10sec hang-up during in txn

VIM_ERROR_PTR ValidateMemory(VIM_BOOL fForceCoalesce)
{
    VIM_FILE_FSINFO fs_info;
    VIM_SIZE mem_count = 0;
    VIM_CHAR MemoryDataBuff[500];
    VIM_UINT32 MaxHeap = 0, Heap = 0, MaxStack = 0, Stack = 0;

    vim_mem_clear(MemoryDataBuff, VIM_SIZEOF(MemoryDataBuff));
#if 1	// RDD - no need to coalesce on later OS versions
    VIM_ERROR_RETURN_ON_FAIL(vim_file_filesystem_info(&fs_info));
    DBG_MESSAGE("*********** Check Flash Memory ************ ");
    DBG_VAR(fs_info.space_free);
    vim_snprintf(MemoryDataBuff, VIM_SIZEOF(MemoryDataBuff), "Free FLASH: 0x%X Recommended MIN: 0x400000", fs_info.space_free);
    pceft_debug(pceftpos_handle.instance, MemoryDataBuff);

    if ((fs_info.space_free < FORCE_COALESCE_LIMIT) && (fForceCoalesce))
    {
        DBG_MESSAGE(" !!!!! DANGER - Low Memory Detected: Coalescing..... !!!!! ");
        pceft_debug(pceftpos_handle.instance, "Low Memory Detected. Coalescing now.....");
        VIM_ERROR_RETURN_ON_FAIL(vim_flash_coalesce());
        VIM_ERROR_RETURN_ON_FAIL(vim_file_filesystem_info(&fs_info));
        vim_snprintf(MemoryDataBuff, VIM_SIZEOF(MemoryDataBuff), "Free Memory: %d", fs_info.space_free);
        pceft_debug(pceftpos_handle.instance, MemoryDataBuff);
        DBG_VAR(fs_info.space_free);
    }

#endif
    DBG_MESSAGE("*********** Check Memory ************ ");
    vim_mem_get_item_count(&mem_count);
    DBG_VAR(mem_count);
    //DBG_VAR( file_count );
    //VIM_DBG_VAR( unfreed );
    vim_terminal_get_heap_stack_info(&MaxHeap, &Heap, &MaxStack, &Stack);
    DBG_VAR(Heap);
    DBG_VAR(MaxHeap);
    DBG_VAR(Stack);
    DBG_VAR(MaxStack);

    vim_snprintf(MemoryDataBuff, VIM_SIZEOF(MemoryDataBuff), "ItemCount:%d MaxHeap:%d CurrentHeap:%d MaxStack:%d CurrentStack:%d", mem_count, MaxHeap, Heap, MaxStack, Stack);
    pceft_debug(pceftpos_handle.instance, MemoryDataBuff);

    return VIM_ERROR_NONE;
}

VIM_ERROR_PTR configure_standalone_mode(void)
{
#if 0
  display_screen(DISP_STANDALONE_INTERNAL_PRINTER, VIM_PCEFTPOS_KEY_CANCEL);
  if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_INTERNAL_PRINTER))
    return VIM_ERROR_NONE;

  display_screen(DISP_STANDALONE_WINDOWS_PRINTER, VIM_PCEFTPOS_KEY_CANCEL);
  if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_WINDOWS_PRINTER))
    return VIM_ERROR_NONE;

  pceft_settings_set_windows_default_printer();
#endif
  display_screen(DISP_STANDALONE_INTERNAL_MODEM, VIM_PCEFTPOS_KEY_CANCEL);
  if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_INTERNAL_MODEM))
    return VIM_ERROR_NONE;

  if ((terminal.standalone_parameters & STANDALONE_INTERNAL_PRINTER) &&
    (terminal.standalone_parameters & STANDALONE_INTERNAL_MODEM))
  {
    display_screen(DISP_STANDALONE_JOURNAL_TXN, VIM_PCEFTPOS_KEY_CANCEL);
    if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_JOURNAL_TXN))
      return VIM_ERROR_NONE;
  }

  display_screen(DISP_STANDALONE_PRINT_SECOND_RECEIPT, VIM_PCEFTPOS_KEY_CANCEL);
  if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_PRINT_SECOND_RECEIPT))
    return VIM_ERROR_NONE;

#if 0	// RDD 180314 Not supported
  display_screen(DISP_STANDALONE_SWIPE_START_TXN, VIM_PCEFTPOS_KEY_CANCEL);
  if (!set_standalone_option(&terminal.standalone_parameters, STANDALONE_SWIPE_START_TXN))
    return VIM_ERROR_NONE;
#endif

  return VIM_ERROR_NONE;
}

void CrashMe(VIM_UINT8 *Buffer)
{
	VIM_UINT8 StackKiller[200000000] = { 0 };

	for (;;)
	{
		pceft_debug_test(pceftpos_handle.instance, "I still have... ");
		CrashMe(StackKiller);
		pceft_debug_test(pceftpos_handle.instance, "Stack !! ");
	}
}

#if 0
typedef struct vim_tms_vhq_app_parameter_t {
	char parameterName[64];
	char parameterValue[256];
	int parameterType;
} vim_tms_vhq_app_parameter_t;
#endif

#ifdef _DEBUG	// RDD - for use with simulaor on exported XML file
VIM_ERROR_PTR tms_get_params(void)
{
	vim_tms_vhq_app_parameter_t app_parameters[MAX_APP_PARAMS];
	vim_tms_vhq_app_parameter_t *app_parameters_ptr = &app_parameters[0];
	VIM_SIZE num_app_parameters = 0;

	VIM_DBG_MESSAGE("---- VHQ : Get Params ----");

	vim_mem_clear((void *)app_parameters_ptr, VIM_SIZEOF(vim_tms_vhq_app_parameter_t)*MAX_APP_PARAMS);

	tms_vhq_callback_get_app_parameters(app_parameters_ptr, &num_app_parameters);
	tms_vhq_callback_app_parameters_updated();
	VIM_DBG_PRINTF1("---- VHQ : Get Params : %d Params", num_app_parameters );

	return VIM_ERROR_NONE;
}
#endif

// RDD - backdoor to get WebLFD back if VHQ access get stuffed up ( for one of many possible reasons )
VIM_ERROR_PTR enable_vhqonly(void)
{
	TERM_OPTION_VHQ_ONLY = VIM_TRUE;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR enable_weblfd(void)
{
	TERM_OPTION_VHQ_ONLY = VIM_FALSE;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR RunFunctionMenu( VIM_UINT32 Password, VIM_BOOL RunFromTMS, VIM_CHAR *DisplayMsg )
{
	VIM_KBD_CODE key_code;
	VIM_EVENT_FLAG_PTR events[1];
	//VIM_ERROR_PTR res;
	// Indicate that this was initiated by the PinPad and no by the POS - kills all POS displays
	terminal.initator = it_pinpad;

	if( RunFromTMS )
		pceft_debug( pceftpos_handle.instance, "Function Menu Accessed Remotely" );
	else
		pceft_debug( pceftpos_handle.instance, "Function Menu Accessed via KBD" );

	VIM_DBG_NUMBER( Password );

	switch ( Password )
    {
	case 77:
	{
		if (RunFromTMS)
			return VIM_ERROR_NONE;
		if( IS_STANDALONE )
			return VIM_ERROR_NONE;

		// 010818 RDD JIRA WWBP-293 - Try setting standalone mode temporarily if re-print required on PinPad
		terminal.mode = st_standalone;
		terminal.standalone_parameters |= STANDALONE_INTERNAL_PRINTER;
		txn_admin_copy();
		terminal.mode = st_integrated;
		terminal.standalone_parameters &= ~STANDALONE_INTERNAL_PRINTER;
		break;
	}
#ifdef _DEBUG

    // #LFD -> Route via LFD
    case 533:
    {
        if( terminal_get_status( ss_wow_basic_test_mode ))
        {
			//if (IS_V400m)
			//{
			//	vim_file_delete(WEB_TMS_WIFI_CONFIG_FILE);				
			//	vim_file_copy(WEB_LFD_WIFI_CONFIG_PATH, WEB_TMS_WIFI_CONFIG_FILE);
			//}
			//else
			{
				vim_file_delete(WEB_TMS_PROXY_CONFIG_FILE);
				vim_file_copy(WEB_LFD_PROXY_CONFIG_PATH, WEB_TMS_PROXY_CONFIG_FILE);
			}
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Set TMS via", " LFD ");
            vim_event_sleep(2000);
        }
        break;
    }


	case 4777:
#ifdef _DEBUG
		if (GetAnswer("Force GPRS\nEnter=Enable Clr=Disable") == VIM_TRUE)
		{
			terminal.configuration.ForceGPRS = VIM_TRUE;
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Force GPRS", "Enabled");
		}
		else
		{
			terminal.configuration.ForceGPRS = VIM_FALSE;
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Force GPRS", "Disabled");
		}
		vim_event_sleep(2000);
		break;
#endif


    // #RTM -> Route via RTM
    case 786:
    {
        if (terminal_get_status(ss_wow_basic_test_mode))
        {
			if (IS_V400m)
			{
				vim_file_delete(WEB_TMS_WIFI_CONFIG_FILE);
				vim_file_copy(WEB_RTM_WIFI_CONFIG_PATH, WEB_TMS_WIFI_CONFIG_FILE);
			}
			else
			{
				vim_file_delete(WEB_TMS_PROXY_CONFIG_FILE);
				vim_file_copy(WEB_RTM_PROXY_CONFIG_PATH, WEB_TMS_PROXY_CONFIG_FILE);
			}
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Set TMS via", " RTM ");
            vim_event_sleep(2000);
        }
        break;
    }



	case 2222:
		{
			//VIM_UINT8 Data[] = { 0x08, 0x00, 0x80, 0x20, 0x00, 0x01, 0x00, 0xc1, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x53, 0x64, 0x31, 0x31, 0x00, 0x06, 0x28, 0x00, 0x00, 0x0f, 0x57, 0x32, 0x37, 0x37, 0x32, 0x30, 0x30, 0x33, 0x36, 0x31, 0x31, 0x30, 0x30, 0x30, 0x36, 0x33, 0x35, 0x37, 0x34, 0x30, 0x31, 0x34, 0x31, 0x30, 0x32, 0x35, 0x05, 0x52, 0x62, 0x20, 0x08, 0x08, 0x00, 0x77, 0x03, 0x20, 0x49, 0x2c, 0x4b, 0x63, 0x89, 0x3b, 0x68, 0x74, 0xc3, 0xd6, 0xe4, 0x84, 0x4f, 0x90, 0xbd, 0x01, 0x01 };
			//if (RunFromTMS) return VIM_ERROR_NONE;
			//comms_disconnect();
			//comms_connect( HOST_XML, MESSAGE_WOW_800_NMIC_101 );
			//comms_transmit_internal( Data, VIM_SIZEOF(Data) );
			//comms_disconnect();
			break;
		}

        // RDD 280218 Keyload
        // RDD 161019 Removed prior to initial prod. deployment
    case 5397:
        //RunKeyload();
        break;

    case 53373738:
        display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "RESET LFD", "");
        vim_file_delete("~/TMS_STATUS.CFG");
        vim_file_delete("~/TMS_SW.CFG");
        break;

/*
		VIA_PCEFTPOS,
			GPRS,
			PSTN,
			ETHERNET,
			WIFI,
			RNDIS
*/




		// RDD 260318 added for memory monitoring in basic test mode
	case 6361:
		if (RunFromTMS) return VIM_ERROR_NONE;
		if (terminal_get_status(ss_wow_basic_test_mode))
		{
			ValidateMemory(VIM_FALSE);
		}
		break;

	case 6362:
		if (RunFromTMS) return VIM_ERROR_NONE;
		if (terminal_get_status(ss_wow_basic_test_mode))
		{
			ValidateMemory(VIM_TRUE);
		}
		break;

	case 6363:
		// Toggles Z0 memory log that is send out in Idle
		if (RunFromTMS) return VIM_ERROR_NONE;
		if (terminal_get_status(ss_wow_basic_test_mode))
		{
			set_mem_log  = ~set_mem_log;
		}
		break;

#if 0
	case 666:
		if (RunFromTMS) return VIM_ERROR_NONE;
		config_test_string();
		break;
#endif

#endif
	case 99:
		// RDD 240812 Set Contactless init to reload all params on reboot
		//terminal_save();

		// RDD 011217 WWBP-69 this line kills CTLS if reboot cancelled so move to after confim
		//terminal_set_status(ss_wow_reload_ctls_params);

		config_reboot_pinpad( RunFromTMS );
		break;

		case 667:
			//if( LOCAL_PRINTER )
				PrintLastEMVTxn( VIM_TRUE );
			break;

            // RDD 250920 Trello #257
        case 523:
        {
            S_MENU BacklightMenu[] = { { "Backlight Min", BlMin },
                                { "Backlight Medium", BlLow }};

              while( MenuRun( BacklightMenu, VIM_LENGTH_OF( BacklightMenu ), "Backlight Menu", VIM_FALSE, VIM_TRUE ) == VIM_ERROR_NONE);
              return VIM_ERROR_NONE;
        }
        break;


		case 841:
		{
			//extern VIM_ERROR_PTR tms_vhq_open_v(void);

			if (terminal_get_status(ss_wow_basic_test_mode))		//BRD 190114 Offline mode not working for ALH AND not required
			{

				S_MENU VHQMenu[] = {
					//{ "VHQ Open", tms_vhq_open_v },
					{ "VHQ disable WebLFD", enable_vhqonly },
					{ "VHQ Enable WebLFD", enable_weblfd },
#ifdef _DEBUG
					{ "VHQ test Params", tms_get_params }
#endif
				};
				while (MenuRun(VHQMenu, VIM_LENGTH_OF(VHQMenu), "VHQ Menu", VIM_FALSE, VIM_TRUE) == VIM_ERROR_NONE);
				return VIM_ERROR_NONE;
			}
		}
		break;


        // RDD 030221 - added new config to run only vim lfd in case web lfd really screws things up so badly that vim_lfd can't run afterwards
        case 3695:
            config_run_TMS(VIM_FALSE, web_lfd );
            break;

	    case 3696:
		// RDD 240413 - we're already in TMS logon when this is called so just set the flag to run a TMS next idle.
		if( RunFromTMS )
		{
			pceft_debug(pceftpos_handle.instance, "Function number \"3696\" entered (R)" );
//BD			terminal_clear_status( ss_tms_configured );
		}
		else
        {
            config_run_TMS(VIM_FALSE, web_then_vim );
        }
		break;

	    case 3697:
		// RDD 240413 - we're already in TMS logon when this is called so just set the flag to run a TMS next idle.
		if( RunFromTMS )
		{
			pceft_debug(pceftpos_handle.instance, "Function number \"3697\" entered (R)" );
//BD			terminal_clear_status( ss_tms_configured );
		}
		else
			config_run_TMS(VIM_TRUE, web_then_vim);
		break;

		case 44583:	// "4GLTE"
		{
			VIM_GPRS_PPP_CONNECT_PARAMETERS params;
			VIM_GPRS_INFO info;
			VIM_GPRS_INFO *InfoPtr = &info;
			VIM_CHAR Data[100] = { 0 };

			if( IS_P400 )
				break;

			vim_mem_clear( InfoPtr, VIM_SIZEOF(VIM_GPRS_INFO));
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Getting Mobile\nData Information", "");
			vim_mem_clear(Data, VIM_SIZEOF(Data));
			if( vim_gprs_ppp_setup( &params ) == VIM_ERROR_NONE)
			{
				vim_sprintf(Data, "APN:%s", params.apn );
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PPP Setup:\n", Data));
			}
			else
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PPP Setup:\n", "Invalid"));
			}
			vim_event_sleep(5000);
			vim_mem_clear(Data, VIM_SIZEOF(Data));
			if( vim_gprs_get_info( &info ) == VIM_ERROR_NONE )			
			{
				VIM_PERCENT Level;
				VIM_CHAR SimStatus[20] = { 0 };
				if (!info.SimStatus)
				{
					vim_strcpy(SimStatus, "SIM OK");
				}
				else
				{
					vim_strcpy(SimStatus, "SIM ERROR");
				}
				vim_gprs_get_signal_strength(&Level);
				vim_sprintf(Data, "SimStatus:%s\nSig Level:%d", SimStatus, Level );
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "SIM Info:", Data ));
			}
			else
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "SIM Info:", "Invalid"));
			}
			// Wait for a keypress
			vim_adkgui_kbd_touch_or_other_read(&key_code, ENTRY_TIMEOUT, events, VIM_FALSE);
			break;
		}





        // RDD 230620 Trello #170 Issues with temp LFD files getting stuffed in v710 and v711 run this as remote CMD
        case 73738:
        {
            ResetTMS();
            break;
        }

        case 74438:
        {
			shift_totals_process(VIM_FALSE);
            break;
        }

		case 28:
		{
			BTStatus(VIM_FALSE, VIM_TRUE);
			break;
		}

        case 9434:
        {
            WifiSetup();
            break;
        }

        case 56466:
		case 564660:
        {
            VIM_ERROR_PTR res;
			VIM_DBG_MESSAGE("Logon to Qwikcilver");

			if (!RunFromTMS)
			{
				VIM_DBG_MESSAGE("Logon to Qwikcilver");
				display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "Qwikcilver", "Logon");
			}
			else
			{
				pceft_debug(pceftpos_handle.instance, "Qwikcilver Logon run remotely");
			}
			// RDD 170622 If no merchant details from Switch Logon then QC Logon will bomb out immediatl, so ensure logged onto switch first
#if 0
			if (terminal.logon_state != logged_on)
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Logging On To WW", "Please Wait.." ));
				vim_event_sleep(1000);
				if (logon_request(VIM_TRUE) != VIM_ERROR_NONE)
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "WW Logon Failed", "Call Helpdesk"));
					vim_event_sleep(10000);
					break;
				}
			}
#endif
            if ((res = QC_Logon(VIM_TRUE, 564660 == Password)) == VIM_ERROR_NONE)
            {
				if (!RunFromTMS)
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "QC LOGON", "Success"));
				}
				pceft_debug_test(pceftpos_handle.instance, "Qwikcilver Logon Success");
			}
            else
            {
				if (!RunFromTMS)
				{
					VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "QC LOGON", "Failed"));
				}
				pceft_debug(pceftpos_handle.instance, "Qwikcilver Logon Failed");
			}
            vim_event_sleep(2000);
        }
        break;


        case 639533:
            TERM_NEW_LFD = VIM_TRUE;
            tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE, VIM_TRUE, web_lfd );
            break;

		case 26667:	// Comms
		{
			VIM_CHAR Line1[100] = { 0 };
			VIM_CHAR Line2[100] = { 0 };

			adk_com_init(VIM_FALSE, VIM_FALSE, Line1);
			VIM_DBG_PRINTF1("Adk_Comm_init result :%s", Line1);

			CommsSetup(VIM_TRUE);

			vim_sprintf(Line1, "Comms: %d%d%d\n", terminal.configuration.TXNCommsType[0], terminal.configuration.TXNCommsType[1], terminal.configuration.TXNCommsType[2]);
			vim_sprintf(Line2, "\n0=EFT Server\n1=Mobile Data (4G)\n2=PSTN\n3=Ethernet\n4=WiFi\n5=RNDIS");
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Line1, Line2));
			vim_kbd_read(&key_code, TIMEOUT_INACTIVITY);
			break;
		}

		case 38242825:
		{
			pceft_debug(pceftpos_handle.instance, "Debug Error out to logs");
			VIM_DBG_ERROR(VIM_ERROR_SYSTEM);
			break;
		}



#ifdef _DEBUG

		case 123:
			terminal.logon_state = logged_on;
			terminal_set_status( ss_tms_configured );
			terminal_set_status(ss_wow_reload_ctls_params);
			CtlsInit( );
			break;


	
			// #error
		case 37767:
			VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);




		case 1234566:
        {
            VIM_CHAR NextFileToDownload[TMS_FILE_LIST_NAME_LENGTH];
            vim_mem_clear( NextFileToDownload, TMS_FILE_LIST_NAME_LENGTH );
            vim_tms_lfd_check_versions( NextFileToDownload, VIM_FALSE );
            VIM_DBG_STRING( NextFileToDownload );
        }
		break;
#endif

		case 1234567:
		{
			VIM_SIZE VSSLogFileSize = 0;
			vim_file_delete(VIM_FILE_PATH_LOGS "vss_nfc.log");
			vim_file_get_size(VIM_FILE_PATH_LOGS "vss_nfc.log", VIM_NULL, &VSSLogFileSize);
			pceft_debugf(pceftpos_handle.instance, "VAS logfile size is : %d", VSSLogFileSize);
			break;
		}

		// RDD 040123 JIRA PIRPIN-2111 : Add #PRINT to enable/disable on terminal printing
		case 77468:
		{
			if (IS_STANDALONE)
				break;

			if (terminal_get_status(ss_enable_local_receipt))
			{
				terminal_clear_status(ss_enable_local_receipt);
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Local Receipt Print", "Disabled" ));
			}
			else
			{
				terminal_set_status(ss_enable_local_receipt);
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Local Receipt Print", "Enabled"));
			}
			vim_event_sleep(2000);
			break;
		}


#ifdef _WIN32
		// RDD 260418 added #DOCK for simulating toggling dock/undock in WIN32
		case 3625:
		{
			VIM_BOOL docked;
			vim_terminal_get_dock_status(&docked);
			docked = ~docked;
			vim_terminal_set_dock_status(docked);
			pceft_debugf(pceftpos_handle.instance, "DockStatus = %d", docked);
			terminal_save();
			break;
		}
#endif

		case 3824:
		if( RunFromTMS ) return VIM_ERROR_NONE;

        /* View terminal configuration */
		// RDD 160412 SPIRA:5316 Z DEBUG for Function key presses
		pceft_debug(pceftpos_handle.instance, "Function number \"3824\" entered" );
        VIM_ERROR_RETURN_ON_FAIL( config_maintenance_functions() );
		break;

		case 78743:
			if (RunFromTMS) return VIM_ERROR_NONE;
			if (terminal_get_status(ss_wow_basic_test_mode))		//BRD 190114 Offline mode not working for ALH AND not required
			{
				VIM_CHAR Buffer[100];
                VIM_SIZE Expired = 0, Completed = 0;
                PurgePreauthFiles(&Completed, &Expired);
                vim_snprintf( Buffer, VIM_SIZEOF(Buffer), "Purged Preauths\nCompleted=%3.3d\nExpired=%3.3d", Completed, Expired );
				VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer, "" ));
				vim_event_sleep(2000);
			}
			break;

#ifdef _DEBUG


        case 3472:
            if ((GetAnswer("Host Disconnect\nEnter=Enable Clr=Disable") == VIM_TRUE) && terminal_get_status(ss_wow_basic_test_mode))
            {
                terminal.configuration.allow_disconnect = VIM_TRUE;
                display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Host Disconnect", "Enabled");
            }
            else
            {
                terminal.configuration.allow_disconnect = VIM_FALSE;
                display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Host Disconnect", "Disabled");
            }
            vim_event_sleep(2000);
            break;
#endif

        case 3347:
#ifdef _DEBUG
			if(1)
#else
            if (terminal_get_status(ss_wow_basic_test_mode))
#endif
            {
                if (GetAnswer("DE47 Enryption\nEnter=Enable Clr=Disable") == VIM_TRUE)
                {
                    TERM_ENCRYPT_DE47 = VIM_TRUE;
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "DE47 Encryption", "Enabled");
                }
                else
                {
                    TERM_ENCRYPT_DE47 = VIM_FALSE;
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "DE47 Encryption", "Disabled");
                }
                terminal_save();
            }
            else
            {
                TERM_ENCRYPT_DE47 = VIM_TRUE;
                display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "DE47 Encryption", "Enabled");
            }
            vim_event_sleep(2000);
            break;



		case 7410:	// Configure Terminal
			if (RunFromTMS) 
                return VIM_ERROR_NONE;

            pceft_debug(pceftpos_handle.instance, "Function number \"7410\" entered");

			if (IS_R10_POS)
			{
				pceft_debug(pceftpos_handle.instance, "Auto-Exit as R10 terminal and #7410 not allowed");
				return VIM_ERROR_NONE;
			}

            ConfigureTerminal();
#if 0
			if( IS_INTEGRATED )
				return VIM_ERROR_NONE;
			do
			{
				res = MenuRun( ConfigTerminalMenu, VIM_LENGTH_OF(ConfigTerminalMenu), "Terminal Config", VIM_FALSE, VIM_TRUE );
			}
			while( res == VIM_ERROR_USER_BACK );
#endif

            pceft_pos_display_close();
			break;


		case 1111:
		if( RunFromTMS ) return VIM_ERROR_NONE;
		pceft_debug(pceftpos_handle.instance, "Function number \"1111\" entered" );
		{
			VIM_BOOL chip_card = VIM_FALSE;
			VIM_PCEFTPOS_QUERY_CARD_RESPONSE resp;
            VIM_PCEFTPOS_QUERY_CARD_REQUEST req;

            // 300320 DEVX J5 and 1111 not supported in fuel now.
            if (!TERM_USE_PCI_TOKENS)
            {
                return VIM_ERROR_NONE;
            }

			VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_callback_process_query_card( PCEFT_WOW_TOKEN_QUERY_CARD, &req, &resp, &chip_card, VIM_TRUE ));
		}
		break;

		case 772:
			if (terminal_get_status(ss_wow_basic_test_mode))
			{
				terminal.logon_state = rsa_logon;
			}
			break;

		// RDD - # SWAP
		case 7927:
			if (RunFromTMS) return VIM_ERROR_NONE;
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Decommission", "Terminal"));
			vim_event_sleep(1500);
			if( GetPassword( (VIM_INT32 *)&Password, "Enter Password" ) == VIM_ERROR_NONE )
			{
				// Check the password
				if( Password == SAF_DELETE_PASSWORD )
				{
					ReversalAndSAFProcessing( MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_TRUE );
					display_screen( DISP_DELETING_FILES, VIM_PCEFTPOS_KEY_NONE );
//					vim_file_delete("~/terminal.dat");
					vim_file_delete("~/saftot.bin");
					vim_file_delete("~/version.cfg");
					vim_file_delete("~/TMS_STATUS.CFG");
					vim_file_delete("~/duptxn.dat");
					vim_file_delete("~/txn.dat");
					vim_file_delete("~/totals.dat");
					saf_destroy_batch();
					pa_destroy_batch();
					vim_mem_clear( terminal.terminal_id, VIM_SIZEOF( terminal.terminal_id ));
					vim_mem_clear( terminal.merchant_id, VIM_SIZEOF( terminal.merchant_id ));
					//vim_mem_clear( (void *)&terminal, VIM_SIZEOF( terminal ));
					//terminal_clear_status( ss_tms_configured ); RDD 080116 - don't do this because we'll contact TMS on power up with no config !
					terminal_clear_status( ss_tid_configured );
					terminal_save();
					vim_event_sleep(1500);
				}
			}
			break;


		case 4648:
		if( terminal_get_status( ss_tid_configured ) )
		{
			if( RunFromTMS )
			{
				pceft_debug(pceftpos_handle.instance, "Function number \"4648\" entered (R)" );
				terminal.logon_state = rsa_logon;
				txn.type = tt_logon;
				ReversalAndSAFProcessing( MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_TRUE );
			}
			else
			{
				pceft_debug(pceftpos_handle.instance, "Function number \"4648\" entered" );
				if( GetAnswer( "Force RSA Logon\nEnter=YES Clr=NO" ) )
				{
					terminal.logon_state = rsa_logon;
					txn.type = tt_logon;
					ReversalAndSAFProcessing( MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_TRUE );
				}
			}
		}
		else
		{
			terminal_check_config_change_allowed( VIM_TRUE );
		}
		comms_disconnect();

		// RDD 080618 JIRA WWBP-112 - PCI encrypt card data - ensure SAF deleted before changing key !!
		// RDD 120618 We need to ensure that both SAF and Card Buffer are empty before changing the batch key

        // RDD 291019 JIRA WWBP-375 RE-opened VN didn't merge hotfix code for to main branch
		card_destroy_batch();

        // RDD 291019 don't generate a new batch key here as answering NO to 4648 results in SAF not being sent.
#if 0
		if (vim_tcu_generate_new_batch_key() != VIM_ERROR_NONE)
		{
			pceft_debug(pceftpos_handle.instance,"Gen new Batch Key failed");
		}
		else
		{
			pceft_debug(pceftpos_handle.instance,"Gen new Batch Key OK");
		}
#endif
		break;

        case 11112222:
            /* switch to standlaone mode */
            VIM_ERROR_RETURN_ON_FAIL(swap_terminal_mode());

            if (!IS_INTEGRATED)
            {
                VIM_ERROR_RETURN_ON_FAIL(TerminalIdleDisplay(0));
            }
            else
            {
                ConnectClient();
            }
            return VIM_ERROR_NONE;


		case 11112227:
        {
#ifdef _DEBUG
            extern VIM_PCEFTPOS_PARAMETERS pceftpos_settings;
#endif
            if (RunFromTMS) return VIM_ERROR_NONE;
            // Load the current settings
            // 190220 DEVX Was a bug here doing load instead of load only will load defualts rather than file settings
            pceft_settings_load_only();
            pceft_configure(VIM_FALSE);

            if (config_get_yes_no("Are you sure you\nwish to change\nthe configuration?", "Comms Reset"))
            {

                VIM_ERROR_RETURN_ON_FAIL(pceft_settings_save());
#ifdef _DEBUG
                VIM_DBG_PRINTF1("PPPPPPPPPPPP Protocol Saved: %d", pceftpos_settings.protocol);
#endif
                pceft_settings_reconfigure(VIM_FALSE);
            }
            else
            {
                pceft_settings_load_only();
#ifdef _DEBUG
                VIM_DBG_PRINTF1("PPPPPPPPPPPPP Protocol Not Saved - use default or file ver: %d", pceftpos_settings.protocol);
#endif
            }
            return VIM_ERROR_NONE;
        }

        case 6566:
		if( RunFromTMS )
		{
			pceft_debug(pceftpos_handle.instance, "Function number \"6566\" entered (R)" );
			saf_send_and_clear_repeat(  );
		}
		else
		{
			pceft_debug(pceftpos_handle.instance, "Function number \"6566\" entered" );
			VIM_ERROR_RETURN_ON_FAIL( config_send_and_clear_repeat() );
		}
		break;

#ifdef _DEBUG
		case 337:
		{
			display_screen(DISP_IMAGES_AND_OVERLAY, VIM_PCEFTPOS_KEY_NONE, TermGetBrandedFile("", "QrPaid.png"), "QrPaidOverlay.gif", "88", "177");
			pceft_debug(pceftpos_handle.instance, "Function number \"337\" entered");
			//TERM_QR_WALLET = VIM_TRUE;
			TERM_QR_QR_TIMEOUT = 270;
			TERM_QR_PS_TIMEOUT = 86400;
			TERM_QR_POLLBUSTER = VIM_TRUE;
			TERM_QR_WALLY_TIMEOUT = 31;
			vim_event_sleep(5000);
			break;
		}
#endif


		case 6567:
			if (TERM_QC_GIFT_DISABLE == VIM_FALSE)
			{
				config_qc_send_saf(RunFromTMS);
			}
			break;

 
        case 83781811:
                if (GetAnswer("TEST Mode\nEnter=Enable Clr=Disable") == VIM_TRUE)
                {
                    terminal_set_status(ss_wow_basic_test_mode);
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "TEST Mode", "Enabled");
                }
                else
                {
                    terminal_clear_status(ss_wow_basic_test_mode);
                    display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "TEST Mode", "Disabled");
                }
                terminal_save();            
                vim_event_sleep(2000);            
                break;

		// #CRASH
		case 27274:
		{

			if (terminal_get_status(ss_wow_basic_test_mode))
			{ 
				display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Crashing Terminal", "Please wait ...");
				pceft_debug(pceftpos_handle.instance, "Deliberatly Crash this terminal");
				vim_event_sleep(100);
				// Cause a crash by re-entrant code
				{
					CrashMe(VIM_NULL);
				}
			}
		}
		break;


		case 8378:
		case 83781:
		case 83782:
		case 83783:
		case 83784:
		case 83785:
		case 83786:
		case 83787:
		//case 83788: - RDD don't run LFD test from remote command because it's recurcive and crashes the PinPad !
		case 83789:
			{
				VIM_CHAR Buffer[100];
				if( RunFromTMS )
				{
					vim_snprintf( Buffer, VIM_SIZEOF(Buffer), "Function number \"%d\" entered (R)", Password );
					pceft_debug( pceftpos_handle.instance, Buffer );
					VIM_ERROR_RETURN_ON_FAIL( diag_run_tests( Password , dt_Remote ));
				}
				else
				{
					vim_snprintf( Buffer, VIM_SIZEOF(Buffer), "Function number \"%d\" entered", Password );
					pceft_debug( pceftpos_handle.instance, Buffer );
					VIM_ERROR_RETURN_ON_FAIL( diag_run_tests( Password, dt_Menu ));
				}
			}
			break;

        case 83780:
            vim_prn_put_system_text("", "", VIM_FALSE);
            break;


		case 2884:
			if (RunFromTMS) return VIM_ERROR_NONE;
			{
				VIM_UINT32 rrn;
				preauth_select_record( &rrn );
				break;
			}

		case 7469:
			if (RunFromTMS) return VIM_ERROR_NONE;
			diag_select_record( );
			break;

		case 87246:
			if (RunFromTMS) return VIM_ERROR_NONE;
			if( !terminal.training_mode )
			{
				if( config_get_yes_no( "Enter Training\nMode?", VIM_NULL ))
					terminal.training_mode = VIM_TRUE;
			}
			else if( config_get_yes_no( "Exit Training\nMode?", VIM_NULL ))
			{
				terminal.training_mode = VIM_FALSE;
			}
			break;


            // RDD 140520 Trello #66 VISA Tap Only terminal
        case 2857:
            SetupCTLSOnlyTerminal();
            terminal_save();
            break;

		case 1720:
		if( RunFromTMS ) return VIM_ERROR_NONE;
		pceft_debug(pceftpos_handle.instance, "Function number \"1720\" entered" );
		VIM_ERROR_RETURN_ON_FAIL( saf_maintenance_functions() );
		break;


		case 17205:
		if( RunFromTMS )
			pceft_debug( pceftpos_handle.instance, "Function number \"17205\" entered (R)" );
		else
			pceft_debug( pceftpos_handle.instance, "Function number \"17205\" entered" );
		display_screen( DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "Deleting Config\n", "" );
		vim_event_sleep(1000);

		display_screen(DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, "DELETING CONFIG", "");
		vim_file_delete("~/CPAT");
		vim_file_delete("~/EPAT");
		vim_file_delete("~/PKT");
		vim_file_delete("~/FCAT");
		display_screen( DISP_DELETING_FILES, VIM_PCEFTPOS_KEY_NONE );
		vim_event_sleep(1000); // wait 1 sec
		CheckFiles(VIM_TRUE);
		terminal_save();
		break;

        case 737381:
            //if (terminal_get_status(ss_wow_basic_test_mode))
            {
                vim_file_delete(SECURITY_TOKEN_PATH);
                vim_mem_clear(terminal.terminal_id, VIM_SIZEOF(terminal.terminal_id));
                vim_mem_clear(terminal.merchant_id, VIM_SIZEOF(terminal.merchant_id));
                terminal_save();
            }
            ConfigureTerminal();

            break;

		case 17206:
		if( RunFromTMS )
			pceft_debug( pceftpos_handle.instance, "Function number \"17206\" entered (R)" );
		else
			pceft_debug( pceftpos_handle.instance, "Function number \"17206\" entered" );
		display_screen( DISP_RESET_STATS, VIM_PCEFTPOS_KEY_NONE );
		vim_event_sleep(1000);
		terminal_debug_statistics( VIM_NULL );
		terminal_reset_statistics( VIM_NULL );
		break;

		case 17207:
		if( RunFromTMS )
		{
			pceft_debug( pceftpos_handle.instance, "Function number \"17207\" entered (R)" );
//			terminal_clear_status( ss_tms_configured );		//BD Force TMS contact, done elsewhere
		}
		else
			pceft_debug( pceftpos_handle.instance, "Function number \"17207\" entered" );
		display_screen( DISP_DEBUG_STATS, VIM_PCEFTPOS_KEY_NONE );
		vim_event_sleep(1000);
		terminal_debug_statistics( VIM_NULL );

		break;
#ifdef _DEBUG
		case 17209:
		if( RunFromTMS )
		{
			pceft_debug( pceftpos_handle.instance, "Function number \"17209\" entered (R)" );
			display_screen( DISP_PROCESSING, VIM_PCEFTPOS_KEY_NONE, DisplayMsg, "" );
			vim_event_sleep(1000);
		}
		break;
#endif
		// RDD 250718 added for JIRA WWBP-116
		case 2288:
		if(RunFromTMS) return VIM_ERROR_NONE;
		{
			if (IS_V400m)
			{
				VIM_CHAR BatteryStatus[40];
				VIM_SIZE charge = 0;
				vim_terminal_get_battery_charge(&charge);
				vim_snprintf(BatteryStatus, VIM_SIZEOF(BatteryStatus), "Battery: %d%%", charge);
				display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, BatteryStatus, "");
				vim_event_sleep(2000);
			}
		}
		break;


        case 724:
        {
            VIM_CHAR Buffer[100];
            vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));

            vim_sprintf(Buffer, "SecsSinceLastReboot: %d", terminal_secs_since_last_reboot() );
            pceft_debug_test(pceftpos_handle.instance, Buffer);

            vim_sprintf(Buffer, "Scheduled Reboot: %d", terminal_get_secs_to_go());
            pceft_debug_test(pceftpos_handle.instance, Buffer);

            vim_sprintf(Buffer, "Controlled Reboot: %d", terminal_get_secs_to_go() - TERM_PRE_PCI_REBOOT_CHECK );
            pceft_debug_test(pceftpos_handle.instance, Buffer);

            break;

        }


        case 7743:
        {
#ifdef _DEBUG
			if(1)
#else
			if (terminal_get_status(ss_wow_basic_test_mode))
#endif
			{
				VIM_DES_BLOCK test_wow_ppid =
				{
					{0x06,0x21,0x10,0x10,0x00,0x00,0x00,0x00}
				};
				VIM_CHAR serial_number[30];

				if (GetAnswer("Set PPID from SN\nEnter=ON Clr=OFF") == VIM_TRUE)
				{
					vim_mem_clear(serial_number, VIM_SIZEOF(serial_number));
					VIM_ERROR_RETURN_ON_FAIL(vim_terminal_get_serial_number(serial_number, VIM_SIZEOF(serial_number) - 1));

					// Serial number comes back as 10 ascii decimal digits we only need the last 9 and in BCD format
					vim_utl_asc_to_bcd(serial_number, &test_wow_ppid.bytes[3], 10);
					test_wow_ppid.bytes[3] |= 0x10;   // ensure set to TEST terminal
					VIM_DBG_DATA(test_wow_ppid.bytes, 8);
					VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_ppid(tcu_handle.instance_ptr, &test_wow_ppid));

					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PPID from SN", "Enabled");
				}
				else
				{
					static VIM_DES_BLOCK const debug_wow_ppid =
					{
						{0x06,0x12,0x02,0x13,0x25,0x06,0x02,0x89}
					};
					VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_ppid(tcu_handle.instance_ptr, &debug_wow_ppid));

					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PPID from SN", "Disabled");
				}
				vim_event_sleep(2000);
			}
            break;
        }


		case 726:
		{
			VIM_INT16 RamUsedKB = 0;
			VIM_CHAR Buffer[100] = { 0 };

			vim_prn_get_ram_used(&RamUsedKB);

			vim_sprintf(Buffer, "RAM Used = %d Kb", RamUsedKB);
			display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, Buffer);
			vim_kbd_read(&key_code, TIMEOUT_INACTIVITY);
			break;
		}

		case 5647:
		{
			VIM_TEXT const ErrorFile[] = VIM_FILE_PATH_LOGS "82000.LOG";
			VIM_SIZE LogFileSize = 0;
			VIM_CHAR Buffer[100] = { 0 };

			vim_file_get_size(ErrorFile, VIM_NULL, &LogFileSize);

			vim_sprintf(Buffer, "Current Logfile Size = %d\n Delete ?", LogFileSize);

			if (config_get_yes_no_timeout(Buffer, "Deleting...", 30000, VIM_FALSE))
			{
				if (vim_file_delete(ErrorFile) == VIM_ERROR_NONE)
				{
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "82000.LOG Delete", "Success");
				}
				else
				{
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "82000.LOG Delete", "Failed");
				}
			}
			vim_event_sleep(2000);
			break;
		}


		case 2378:
		{
			CheckSecurity(VIM_FALSE);
			vim_kbd_read(&key_code, TIMEOUT_INACTIVITY);
			break;
		}

		case 22824539://roll batchkey
		{

			VIM_SIZE QCSafCount = 0, EFTReversal = 0, QCReversal = 0, QCTotal = 0, preAuthPendingCount = 0, preAuthPendingTotal = 0;

			saf_pending_count(&saf_totals.fields.saf_count, &saf_totals.fields.saf_print_count);
			EFTReversal = reversal_present() ? 1 : 0;

			if (TERM_ALLOW_PREAUTH)
			{
				preauth_get_pending(&preAuthPendingCount, &preAuthPendingTotal, VIM_FALSE);
			}

			if ((TERM_QC_GIFT_DISABLE == VIM_FALSE) && (QC_IsInstalled(VIM_FALSE)))
			{
				QC_GetSAFCount(&QCTotal, &QCReversal, &QCSafCount);
			}

			if (saf_totals.fields.saf_count + EFTReversal + preAuthPendingCount + QCTotal + QCReversal > 0) {
				if (GetAnswer("Saf present, Roll batch key\nEnter=ON Clr=OFF") == VIM_FALSE) {
					break;
				}
			} else {
				if (GetAnswer("Saf not present, Roll batch key\nEnter=ON Clr=OFF") == VIM_FALSE) {
					break;
				}
			}
			if (vim_tcu_generate_new_batch_key() != VIM_ERROR_NONE) {
				DBG_MESSAGE("Gen new Batch Key failed");
			} else {
				DBG_MESSAGE("Gen new Batch Key OK");
			}
			break;
		}


		// YYY mode implementation
		case 7877363:		
		if (RunFromTMS) return VIM_ERROR_NONE;

		if (terminal_get_status(ss_wow_basic_test_mode) && is_integrated_mode())		//BRD 190114 Offline mode not working for ALH AND not required
		{
			terminal_set_status(ss_wow_enhanced_test_mode);
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enhanced Test Mode\n ON ", "[Reboot Will Disable]");
			vim_event_sleep(2000);
		}			
		break;

		// RDD 050813 SPIRA:6784 toggle offline test mode
		case 6335463:
		if (RunFromTMS) return VIM_ERROR_NONE;

		if( terminal_get_status( ss_wow_basic_test_mode ) && is_integrated_mode())		//BRD 190114 Offline mode not working for ALH AND not required
		{
			if( terminal_get_status( ss_offline_test_mode ))
			{
				terminal_clear_status( ss_offline_test_mode );
				display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "", "Offline Test Mode\n< OFF >" );
				terminal.allow_ads = VIM_TRUE;
			}
			else
			{
				// RDD 161013 If SAFs pending do not enter offline test mode
				if( saf_totals.fields.saf_count || saf_totals.fields.saf_print_count )
				{
					display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "", "SAF Not Empty\n" );
				}
				else
				{
					terminal_set_status( ss_offline_test_mode );
					display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "", "Offline Test Mode\n< ON >" );
					terminal.allow_ads = VIM_FALSE;
				}
			}
			vim_event_sleep(3000);
		}
		else
		{
			terminal_clear_status( ss_offline_test_mode );
		}
		break;


		//////////////////// RDD 241012 added some handy test mode functions ///////////////////////

        // RDD v582-7 added #COVID to determine CTLS limit in place
        case 26843:
        {
            VIM_CHAR Buffer[100];
            if (!XXX_MODE)
                break;

            vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
            if (terminal.configuration.term_override_cvm_value)
            {
                vim_snprintf(Buffer, VIM_SIZEOF(Buffer), "Temporary CTLS\nLimit :$%d active", terminal.configuration.term_override_cvm_value / 100);
            }
            else
            {
                vim_snprintf(Buffer, VIM_SIZEOF(Buffer), "Normal CTLS CVM\nLimit $100 Applies");
            }
            display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, Buffer, "");
            vim_event_sleep(5000);
            break;
        }


	case 2677878:
#ifdef _DEBUG
		if(1)
#else
		if (terminal_get_status(ss_wow_basic_test_mode))
#endif
		{
			pceft_debug(pceftpos_handle.instance, "Function number \"1720\" entered");
			// Next reversal or SAF will be corrupted deliberately.
			roll_batch_key();
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Batch Key Rolled", "SAF Is Corrupted");
			vim_event_sleep(2000);
		}
		break;


#ifdef _DEBUG
    case 100:
		if( terminal_get_status(ss_wow_basic_test_mode ))
		{
			terminal_set_status( ss_auto_test_mode );
			auto_transaction_test();
			terminal_clear_status( ss_auto_test_mode );
		}
		break;

    case 101:
		if( terminal_get_status(ss_wow_basic_test_mode ))
			vim_emv_init ();
		break;

	case 102:
		if( terminal_get_status(ss_wow_basic_test_mode ))
			picc_load_parameters(VIM_TRUE);
		break;

		// RDD 191112 - Toggle test mode
	case 103:
		if( terminal_get_status( ss_auto_test_mode ) )
		{
			terminal_clear_status( ss_auto_test_mode );
		}
		else
		{
			if( terminal_get_status(ss_wow_basic_test_mode ))
				terminal_set_status( ss_auto_test_mode );
		}
		break;

	case 104:
		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			//VIM_DES_BLOCK ppid;
			VIM_ERROR_RETURN_ON_FAIL( RunKeyload( ));
		}
		break;

    case 105:
        //mal_nfc_GetLastSessionType();
            break;

    case 8377466:
    {
#if 0
        //VIM_ERROR_WARN_ON_FAIL( vim_file_delete( VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LOCAL_VERSION_CFG ));
        VIM_ERROR_WARN_ON_FAIL( vim_file_delete( VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_HOST_VERSION_CFG ));
        VIM_ERROR_WARN_ON_FAIL( vim_file_delete(VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_DOWNLOADING_STAUS));
        VIM_ERROR_WARN_ON_FAIL( vim_file_delete( VIM_FILE_PATH_LOCAL""VIM_TMS_LFD_LAST_CFG_FILENAME ));
#else
        delete_lfd_files(VIM_TRUE);
#endif
        break;
    }

	case 114:
		{
			VIM_DES_BLOCK ppid;
			VIM_ERROR_RETURN_ON_FAIL( vim_tcu_get_ppid( tcu_handle.instance_ptr, &ppid ));
			VIM_DBG_VAR( ppid );
		}

	case 1234:
		terminal.logon_state = logged_on;
		terminal_set_status( ss_tid_configured );
		terminal_set_status( ss_tms_configured );
		reversal_clear(VIM_FALSE);
		break;

	case 124:
		{
			VIM_CHAR Buffer[100];
			VIM_ERROR_PTR error = VIM_ERROR_MISMATCH;
			vim_snprintf( Buffer, VIM_SIZEOF(Buffer), "Fatal Error: %s, %s, line:%d", error->name, __FILE__, __LINE__ );
			DBG_MESSAGE( Buffer );
			pceft_debug( pceftpos_handle.instance, Buffer );
		}
		break;



//#endif


#if 0
		// RDD 170113 SPIRA:6519 Test to set year forward by 4 years so can ensure logon will reset correct year
	case 104:
		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			VIM_RTC_TIME new_date;

			/* read existing date to get year */
			vim_rtc_get_time(&new_date);
			new_date.year+=10;
			vim_rtc_set_time(&new_date);
		}
		break;

		// RDD 170113 SPIRA:6519 Test to set year back by 1 year so can ensure logon will reset correct year
	case 105:
		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			VIM_RTC_TIME new_date;

			/* read existing date to get year */
			vim_rtc_get_time(&new_date);
			new_date.year-=1;
			vim_rtc_set_time(&new_date);
		}
		break;
#else
	case 115:
#ifdef _WIN32
		if(1)
#else
		if( terminal_get_status( ss_wow_basic_test_mode ) )
#endif
		{
			/* read existing date to get year */
			config_date_and_time_configuration(VIM_FALSE);
		}
		break;
#endif





#ifdef _DEBUG
	case 106:
		DBG_POINT;
		if (RunFromTMS) return VIM_ERROR_NONE;
		CheckMemory( VIM_FALSE );
		break;

	case 107:
		if (RunFromTMS) return VIM_ERROR_NONE;
		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			while(1)
			{
				VIM_FILE_FSINFO fs_info;
				VIM_CHAR Text[50];
				VIM_ERROR_RETURN_ON_FAIL( vim_file_filesystem_info( &fs_info ) );
				DBG_MESSAGE( "*********** Check Memory ************ " );
				DBG_VAR( fs_info.space_free );
				vim_snprintf( Text, VIM_SIZEOF(Text), "Free Memory: %d", fs_info.space_free );
				//DBG_MESSAGE( Text );
				//pceft_debug( pceftpos_handle.instance, Text );
				terminal_save();
				VIM_ERROR_RETURN_ON_FAIL( vim_file_filesystem_info( &fs_info ) );
				DBG_MESSAGE( "*********** Check Memory ************ " );
				DBG_VAR( fs_info.space_free );
				//vim_snprintf( Text, VIM_SIZEOF(Text), "Free Memory: %d", fs_info.space_free );
				//DBG_MESSAGE( Text );
				txn_save();
				vim_event_sleep(5);
			}
		}
		break;

		// toggle allow ads
	case 108:
		if (RunFromTMS) return VIM_ERROR_NONE;
		if( terminal.allow_ads == VIM_FALSE )
			terminal.allow_ads = VIM_TRUE;
		else
			terminal.allow_ads = VIM_FALSE;
		break;


	case 110:
		if (RunFromTMS) return VIM_ERROR_NONE;
		terminal_clear_status(ss_ctls_reader_open);
		  break;

	case 111:
		if (RunFromTMS) return VIM_ERROR_NONE;

		if( terminal_get_status( ss_wow_basic_test_mode ) )
		{
			terminal.logon_state = logged_on;
		}
		break;
#endif  //BD
	case 777:
#if 0
		txn.card_state = 0x0A;
		txn.read_icc_count = 0;
		txn.amount_purchase = 0x64;
		txn.amount_total = 0x64;
		txn.type = 0x01;
		TERM_USE_MERCHANT_ROUTING = VIM_TRUE;

		card_capture("PURCHASE", &txn.card, txn.card_state, txn.read_icc_count);
#endif
		if (RunFromTMS) return VIM_ERROR_NONE;
		TERM_USE_MERCHANT_ROUTING = VIM_TRUE;
		break;

	case 888:
		if (RunFromTMS) return VIM_ERROR_NONE;
		TERM_USE_MERCHANT_ROUTING = VIM_FALSE;
		break;

	case 281:
		#ifndef _WIN32
		//config_bt_base();
		#endif
		break;

	case 6368:
		break;


#endif

      default:
		  break;
	}
#if 0	// RDD - this absolutely should not be here 
	if (TERM_QR_WALLET)
	{

		VIM_SIZE FileSize = 0;
		VIM_FILE_HANDLE file_handle;
		VIM_BOOL Exists = VIM_FALSE;
		vim_file_exists(EDPData, &Exists);
		if (Exists)
		{
			vim_file_delete(EDPData);
		}
		if (vim_file_open(&file_handle, EDPData, VIM_TRUE, VIM_TRUE, VIM_TRUE) == VIM_ERROR_NONE)
		{
			FileSize = VIM_SIZEOF(TERM_BRAND);
			pceft_debug(pceftpos_handle.instance, "Banner string added to wally.data successfully");
			pceft_debug(pceftpos_handle.instance, TERM_BRAND);
			VIM_ERROR_PTR res = vim_file_write(file_handle.instance, TERM_BRAND, FileSize);
			vim_file_close(file_handle.instance);
		}
		else {
			pceft_debug(pceftpos_handle.instance, "Failed to add Banner string into wally.data");
		}
		Wally_BuildJSONRequestOfRegistrytoFIle();
	}
#endif
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR config_functions( VIM_SIZE menu_item )
{
	VIM_INT32 Password = menu_item;
	VIM_ERROR_PTR ret = VIM_ERROR_NONE;
#ifdef _WIN32
    //ret = RunFunctionMenu(5397, VIM_FALSE, "");
#endif

    if(IsSlaveMode())
    {
        return VIM_ERROR_NONE;
    }

	// RDD 090212 - Get rid of any data from the last txn
	txn_init();
	terminal.screen_id = 0;
    terminal.allow_ads = VIM_TRUE;  //JQ 10-01-2013
    txn.type = tt_func;				//BD 27-06-2013
    // VN JIRA WWBP-289 020818 Pressing 'Cancel' on Pinpad is not instantaneous when exiting Menu / function
	do {
		if (!menu_item) {
			if ((ret = GetPassword( &Password, "Enter Password" )) != VIM_ERROR_NONE)
            {
                VIM_DBG_ERROR(ret);
				break;
			}
		}
        DBG_POINT;
        ret = RunFunctionMenu(Password, VIM_FALSE, "");
        DBG_POINT;

	} while (0);

    txn.type = tt_none;				// RDD 130320

    // RDD 121021 - close the display
    pceft_pos_display_close();

    DBG_POINT;
#if 0
    if (vim_adkgui_is_active())
    {
        VIM_KBD_CODE key_pressed;
        VIM_EVENT_FLAG_PTR events[1];
		DBG_POINT;
		vim_adkgui_kbd_touch_or_other_read(&key_pressed, 0, events, 1);
    }
#endif
	DBG_POINT;

    // RDD - never go to IDLE display if we're hitting # while POS connecting
	if( pceft_is_connected() )
	{
		DBG_POINT;
		IdleDisplay();
	}
	VIM_ERROR_RETURN( ret );
}
