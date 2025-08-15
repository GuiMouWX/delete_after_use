#include "incs.h"

VIM_DBG_SET_FILE("TJ");
/* Device Types */
#define PCEFT_DEVICE_TYPE_PINPAD                    '0'
#define PCEFT_DEVICE_TYPE_INTERNAL_PRINTER          '1'
#define PCEFT_DEVICE_TYPE_INTERNAL_MODEM            '2'
#define PCEFT_DEVICE_TYPE_INTERNAL_BOTH             '3'
#define PCEFT_DEVICE_TYPE_DEFAULT                   '4'

VIM_PCEFTPOS_KEYMASK last_keymask_set = 0x00;
extern VIM_BOOL ValidateConfigData(VIM_CHAR* terminal_id, VIM_CHAR* merchant_id);
extern VIM_ERROR_PTR config_terminal_id(void);
extern VIM_BOOL ValidateConfigData(VIM_CHAR *terminal_id, VIM_CHAR *merchant_id);
extern VIM_ERROR_PTR ConfigureTerminal(void);
extern VIM_ERROR_PTR vim_dbg_log_error(VIM_TEXT const* filename, VIM_LINE line, VIM_TEXT const* error);



VIM_BOOL pceft_is_connected(void)
{	   
	VIM_BOOL pceftpos_connected = VIM_FALSE;
	//DBG_POINT;
	if (IS_STANDALONE)
		return VIM_FALSE;
	//DBG_POINT;
    vim_pceftpos_get_connected_flag(pceftpos_handle.instance, &pceftpos_connected);
    return pceftpos_connected;
}


// RDD 220912 Report status to the POS logfile
VIM_ERROR_PTR pceft_debug( VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT *message  )
{   
	VIM_DBG_MESSAGE(message);
	// Log to file
	vim_dbg_log_error("", 0, message);

	if (IS_STANDALONE)
	{
		return VIM_ERROR_NONE;
	}
	if (pceft_is_connected())
	{
        VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_debug(self_ptr, message, VIM_MIN(vim_strlen(message), MAX_DEBUG_LEN)));
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR pceft_debug_test( VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT *message  )
{
	if( terminal_get_status( ss_wow_basic_test_mode ))
	{
        VIM_ERROR_RETURN_ON_FAIL( pceft_debug( self_ptr, message ));
 	}
    else
    {
		vim_dbg_log_error("", 0, message);
    }
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR pceft_debugf(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT* format, ...)
{
    VIM_TEXT message[MAX_DEBUG_LEN + 1] = { 0 };
    VIM_VA_LIST ap;
    VIM_VA_START(ap, format);
    vim_vsnprintf(message, sizeof(message), format, ap);
    VIM_VA_END(ap);
    VIM_ERROR_RETURN_ON_FAIL( pceft_debug(self_ptr, message));
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR pceft_debugf_test(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT* format, ...)
{
    VIM_TEXT message[MAX_DEBUG_LEN + 1] = { 0 };
    VIM_VA_LIST ap;
    VIM_VA_START(ap, format);
    vim_vsnprintf(message, sizeof(message), format, ap);
    VIM_VA_END(ap);
    VIM_ERROR_RETURN_ON_FAIL( pceft_debug_test(self_ptr, message));
	return VIM_ERROR_NONE;
}


// RDD 220912 Report an error condition to the POS logfile
VIM_ERROR_PTR pceft_debug_error( VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT *message, VIM_ERROR_PTR error )
{
    VIM_CHAR Buffer[100];

    DBG_MESSAGE(message);
    if (error == VIM_NULL || error->name == VIM_NULL)
    {
        vim_sprintf(Buffer, "%s: Error = UNKNOWN", message);
    }
    else
    {
        vim_sprintf(Buffer, "%s: Error = %s", message, error->name);
    }

	if( error != VIM_ERROR_NONE )
	{
        VIM_DBG_STRING(message);
        VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_debug( self_ptr, Buffer, vim_strlen(Buffer) ));
	}
	return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_debug_vaa( VIM_INT8 vaa_id, VIM_TEXT const *msg )
{
    VIM_CHAR request[MAX_DEBUG_LEN+1];
    VIM_CHAR *Ptr = request;
    VIM_SIZE len = vim_strlen(msg);

    VIM_DBG_PRINTF2("Z%1.1d %s", vaa_id, msg);

    if (!pceft_is_connected())
        return(VIM_ERROR_PCEFTPOS_NOT_CONNECTED);

    if( !len )
        VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);

    if( len+3 > MAX_DEBUG_LEN ) 
        VIM_ERROR_RETURN(VIM_ERROR_OVERFLOW);

    vim_mem_clear( request, VIM_SIZEOF(request));

    // RDD 230321 - assume no more than 9 VAAs !
    vim_sprintf( Ptr, "Z%1.1d %s", vaa_id, msg );

    VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_send(pceftpos_handle.instance, Ptr, len+3 ));
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR pceft_debug_vaa_test(VIM_INT8 vaa_id, VIM_TEXT const *msg)
{
    if (terminal_get_status(ss_wow_basic_test_mode))
    {
        return( pceft_debug_vaa( vaa_id, msg ));
    }
    else
    {
        VIM_DBG_PRINTF2("Unsent Z%1.1d %s", vaa_id, msg);
    }
    return VIM_ERROR_NONE;
}



static VIM_CHAR last_line1[50], last_line2[50];

VIM_ERROR_PTR pceft_pos_display( char const *line_1, char const *line_2, VIM_PCEFTPOS_KEYMASK keymask, VIM_PCEFTPOS_GRAPHIC graphic )
{
	if( IS_STANDALONE )
		return VIM_ERROR_NONE;
		
	if( !pceft_is_connected( ) )
		return VIM_ERROR_PCEFTPOS_NOT_CONNECTED;

	// RDD 230212 - ensure that no PinPad initiated messages are sent to the POS
	if(( terminal.initator == it_pceftpos ) && ( pceftpos_handle.instance ))
	{  	
		// Do not send "Set keymask" for repeated masks
		if( keymask != last_keymask_set )
 		{  
            VIM_DBG_MESSAGE("Set Keymask from POS display");
 			VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_keymask( pceftpos_handle.instance,keymask ));
 			last_keymask_set = keymask;
 		}
        // RDD 090819 - If we're sending the same stuff then don't bother
#if 1
		if(( !vim_strncmp( last_line1, line_1, vim_strlen(line_1))) && ( !vim_strncmp( last_line2, line_2, vim_strlen(line_2))))
		{
			DBG_POINT;
			return VIM_ERROR_NONE;
		}
#endif
		vim_strcpy( last_line1, line_1 );
		vim_strcpy( last_line2, line_2 );

		/* Now send the display command */      
        //DBG_POINT;
        VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_display( pceftpos_handle.instance, line_1, line_2, graphic ));
        //DBG_POINT;

		terminal_clear_status( ss_pos_window_closed );
    }  
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_pos_display_close(void)
{		
	DBG_POINT;
	if( IS_STANDALONE )
		return VIM_ERROR_NONE;

    // RDD 141020 JIRA ??????
    if ((txn.error == VIM_ERROR_POS_CANCEL) && (txn.query_sub_code == PCEFT_READ_CARD_SUB_DEFAULT))
    {
        return VIM_ERROR_NONE;
    }

	if (( pceftpos_handle.instance ) && ( !terminal_get_status( ss_pos_window_closed ) ))  
	{
      VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_display_close(pceftpos_handle.instance));
	  terminal_set_status( ss_pos_window_closed ); 

      vim_mem_clear(last_line1, VIM_SIZEOF(last_line1));
      vim_mem_clear(last_line2, VIM_SIZEOF(last_line2));

	  last_keymask_set = VIM_PCEFTPOS_KEY_INVALID_MASK;
	}       
	return VIM_ERROR_NONE;
}

#if 1

VIM_ERROR_PTR pceft_pos_wait_key( VIM_PCEFTPOS_KEYMASK *key_ptr, /** timeout in milliseconds */ VIM_MILLISECONDS timeout )
{
  VIM_ERROR_PTR error;
  last_keymask_set = VIM_PCEFTPOS_KEY_INVALID_MASK;
	// RDD 020312 - set by default in case of PinPad initition ( eg fn1111 -> fn9999 )
	*key_ptr = VIM_PCEFTPOS_KEY_YES;
  if ((get_operation_initiator() == it_pceftpos) && pceftpos_handle.instance )
  {
    VIM_BOOL pceftpos_connected;
    
    vim_pceftpos_get_connected_flag(pceftpos_handle.instance, &pceftpos_connected);

    if ( pceftpos_connected )
    {
      error = vim_pceftpos_pos_wait_key( pceftpos_handle.instance, key_ptr, timeout, VIM_NULL, 0 );
      if( error == VIM_ERROR_TIMEOUT )
        return error;
      else
      {
        VIM_ERROR_RETURN_ON_FAIL( error )
      }
    }
  }     
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/** 
 * Wait for key to be pressed on POS and allow for data entry on POS 
 *
 * \return
 * If key is pressed, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_KEY_YES, VIM_PCEFTPOS_KEY_NO, VIM_PCEFTPOS_KEY_AUTHORISE, VIM_PCEFTPOS_KEY_OK
 * VIM_ERROR_INVALID_RESPONSE - invalid response received
 * VIM_ERROR_TIMEOUT - timeout while waiting for response.
 * can also return any value returned by pceft_ll_rx().
 */
VIM_ERROR_PTR pceft_pos_wait_data_entry
(
  VIM_UINT32 *key_ptr,
  VIM_MILLISECONDS timeout, 
  char *input_buf, 
  VIM_SIZE buf_size
)
{
  last_keymask_set = VIM_PCEFTPOS_KEY_INVALID_MASK;

  	if( IS_STANDALONE )
		return VIM_ERROR_NONE;

  if ( IS_INTEGRATED && (get_operation_initiator() == it_pceftpos) && pceftpos_handle.instance )
  {
    VIM_BOOL pceftpos_connected;
    
    vim_pceftpos_get_connected_flag(pceftpos_handle.instance, &pceftpos_connected);

    if ( pceftpos_connected )
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_pos_wait_key(pceftpos_handle.instance,key_ptr,timeout,input_buf,buf_size));
    }
  }     
  
  return VIM_ERROR_NONE;
}

#else

/*----------------------------------------------------------------------------*/
/** 
 * Wait for key to be pressed on POS
 *
 * \return
 * If key is pressed, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_KEY_YES, VIM_PCEFTPOS_KEY_NO, VIM_PCEFTPOS_KEY_AUTHORISE, VIM_PCEFTPOS_KEY_OK
 * VIM_ERROR_INVALID_RESPONSE - invalid response received
 * VIM_ERROR_TIMEOUT - timeout while waiting for response.
 * can also return any value returned by pceft_ll_rx().
 */
VIM_ERROR_PTR pceft_pos_wait_key
(
  VIM_PCEFTPOS_KEYMASK *key_ptr,
  /** timeout in milliseconds */
  VIM_MILLISECONDS timeout
)
{
  last_keymask_set = 0x00;
	// RDD 020312 - set by default in case of PinPad initition ( eg fn1111 -> fn9999 )
	*key_ptr = VIM_PCEFTPOS_KEY_YES;
  if ((get_operation_initiator() == it_pceftpos) && pceftpos_handle.instance )
  {
    VIM_BOOL pceftpos_connected;
    VIM_DBG_POINT;

    vim_pceftpos_get_connected_flag(pceftpos_handle.instance, &pceftpos_connected);

    if ( pceftpos_connected )
    {
      VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_pos_wait_key( pceftpos_handle.instance, key_ptr, timeout, VIM_NULL, 0 ));
    }
  }       
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
/** 
 * Wait for key to be pressed on POS and allow for data entry on POS 
 *
 * \return
 * If key is pressed, VIM_PCEFTPOS_KEY_CANCEL, VIM_PCEFTPOS_KEY_YES, VIM_PCEFTPOS_KEY_NO, VIM_PCEFTPOS_KEY_AUTHORISE, VIM_PCEFTPOS_KEY_OK
 * VIM_ERROR_INVALID_RESPONSE - invalid response received
 * VIM_ERROR_TIMEOUT - timeout while waiting for response.
 * can also return any value returned by pceft_ll_rx().
 */
VIM_ERROR_PTR pceft_pos_wait_data_entry
(
  VIM_UINT32 *key_ptr,
  VIM_MILLISECONDS timeout, 
  char *input_buf, 
  VIM_SIZE buf_size
)
{
			
	if( LOCAL_COMMS )
		return VIM_ERROR_NONE;

  if ((get_operation_initiator() == it_pceftpos) && pceftpos_handle.instance )
  {
      VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_pos_wait_key(pceftpos_handle.instance,key_ptr,timeout,input_buf,buf_size));
  }     
  
  return VIM_ERROR_NONE;
}

#endif

VIM_BOOL pceft_command_received( void )
{
	return vim_pceftpos_command_received( pceftpos_handle.instance );
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR pceft_pos_print_standalone(VIM_PCEFTPOS_RECEIPT const *receipt_ptr, VIM_BOOL display_error_on_pos)
{
  VIM_UINT32 screen_id;
  VIM_SIZE  retry;

  retry  = 3;
  screen_id    = 0;
  while(retry--)
  {
    VIM_ERROR_PTR error;
    /* print */
    error=vim_pceftpos_print_on_terminal(receipt_ptr);

    if(error==VIM_ERROR_NO_PAPER)
    {
      screen_id = DISP_PRINTER_OUT_OF_PAPER;
    }
    else if(error==VIM_ERROR_PRINTER)
    {
      screen_id = DISP_PRINTER_ERROR;  
    }
    else
    {
      VIM_ERROR_RETURN_ON_FAIL(error);
    }
    
    if (screen_id!=0)
    {
      if (display_error_on_pos)
      {
        if (VIM_ERROR_NONE != get_yes_no(PRINT_RETRY_TIMEOUT))
        {
          VIM_ERROR_RETURN(VIM_ERROR_PRINTER);
        }
      }  
      else
      {
        /* Display and get out */
        display_screen(screen_id, 0);
        vim_event_wait(VIM_NULL, 0, 3000);
        VIM_ERROR_RETURN(VIM_ERROR_PRINTER);
      }
      screen_id = 0;
    }
    else
    {
      break;
    }
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_BOOL pceft_pos_internal_prn_device_code(void)
{
    return VIM_FALSE;
}
/*----------------------------------------------------------------------------*/
VIM_BOOL pceft_pos_internal_prn(void)
{
    return VIM_FALSE;
}


VIM_ERROR_PTR receipt_print(VIM_PCEFTPOS_RECEIPT *receipt)
{
    VIM_CHAR buffer[5000];
    VIM_SIZE line_index;
    VIM_CHAR *Ptr = &buffer[0];
    VIM_SIZE Cols = terminal.configuration.prn_columns;
    //VIM_SIZE ReceiptDataLen = receipt->lines * Cols;
    VIM_CHAR Brand[3 + 1] = { 0,0,0,0 };
    VIM_CHAR *Ptr3 = terminal.configuration.BrandFileName;
    VIM_CHAR BrandFilePrn[20] = { 0 };
    VIM_BOOL FreeGift = VIM_FALSE;

    vim_mem_clear(BrandFilePrn, VIM_SIZEOF(BrandFilePrn));
    vim_mem_copy(Brand, Ptr3, WOW_BRAND_LEN);

    // Build the Advert file format as specified in JIRA WWBP-26
    if (vim_strlen(Brand) == WOW_BRAND_LEN)
    {
        vim_sprintf(BrandFilePrn, "%3.3s.html", Brand);
    }

    VIM_DBG_STRING(BrandFilePrn);
    VIM_DBG_NUMBER(receipt->lines);
    VIM_DBG_NUMBER(Cols);
    //VIM_DBG_NUMBER(ReceiptDataLen);

    vim_mem_set(buffer, 0, sizeof(buffer));

    for (line_index = 0; line_index < receipt->lines; line_index++)
    {
        VIM_SIZE n;
        VIM_CHAR *Ptr2 = receipt->data[line_index];
        for (n = 0; n < Cols; n++)
        {
            if (n > 2 && n < 22 && *Ptr2 == ' ')
            {
    //            *Ptr++ = ' ';
            }
            *Ptr++ = *Ptr2++;
        }
        *Ptr++ = 0x0D;
        *Ptr++ = 0x0A;
    }
    //VIM_DBG_DATA(buffer, ReceiptDataLen);

    //FreeGift = (txn.amount_total > 20000) ? VIM_TRUE : VIM_FALSE;

    VIM_ERROR_RETURN_ON_FAIL( vim_prn_put_system_text( buffer, BrandFilePrn, FreeGift ));
    return VIM_ERROR_NONE;
}





#if 1	// RDD 270718 re-writen for JIRA WWBP-225

VIM_ERROR_PTR pceft_pos_print(VIM_PCEFTPOS_RECEIPT const *in_ptr, VIM_BOOL write_journal)
{
	VIM_PCEFTPOS_RECEIPT receipt = *(VIM_PCEFTPOS_RECEIPT *)in_ptr;
	VIM_PCEFTPOS_RECEIPT *receipt_ptr = &receipt;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	// RDD 050115 SPIRA:8292 Need variable columns now
    if ((receipt_ptr->receipt_columns == 0)||( receipt_ptr->receipt_columns > MAX_RECEIPT_LINES ))
    {
        receipt_ptr->receipt_columns = terminal.configuration.prn_columns;
        VIM_DBG_NUMBER(receipt_ptr->receipt_columns);
    }

    if( terminal.mode == st_integrated )
    {
	    // RDD 270718 JIRA WWBP-225 Always print to POS first if we can - in standalone mode we'll get VIM_ERROR_NULL				
		// RDD 211221 JIRA PIRPIN-929 PRINT FAILED ERROR for no reason -try doing a repeat if fails...
		// RDD 190122 - Get rid of this as theres already a retry in vim
		if ((res = vim_pceftpos_print(pceftpos_handle.instance, receipt_ptr, write_journal)) != VIM_ERROR_NONE)		
		{		
			pceft_debug(pceftpos_handle.instance, "POS Print Error");			
			VIM_ERROR_RETURN(res);
		}
    }
    // If its a VX690 in standalone mode then print to local printer also
    else
    {
        DBG_POINT;
        /*Check if it is a standalone print only*/
        //if( pceft_pos_internal_prn_device_code() || IS_STANDALONE )
		if(1)
        {
            DBG_POINT;
#ifdef _DEBUG
			if (PRINT_ENABLE)
			{
				res = receipt_print((VIM_PCEFTPOS_RECEIPT*)receipt_ptr);
			}

#else			////////////////// TEST TEST /////////////////
			
			res = receipt_print((VIM_PCEFTPOS_RECEIPT*)receipt_ptr);

#endif
			if (res == VIM_ERROR_NONE)
			{
				// Do nothing
			}

			else if (res == VIM_ERROR_NO_PAPER)
			{
				if( IS_STANDALONE )
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "NO PAPER", "REPLACE AND REPRINT");
				else
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "NO PAPER", "USE POS RECEIPT");

				pceft_debug(pceftpos_handle.instance, "Paper out: Use POS Receipt");
				vim_event_sleep(2000);
			}

			else if (res == VIM_ERROR_BATTERY_LOW)
			{
				if (IS_STANDALONE)
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "LOW BATTERY", "PLACE ON BASE");
				else
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "LOW BATTERY", "USE POS RECEIPT");

				pceft_debug(pceftpos_handle.instance, "Low Battery: ");
				vim_event_sleep(2000);
			}

			else
			{
				if (IS_STANDALONE)
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "LOW BATTERY", "PLACE ON BASE");
				else
					display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "LOW BATTERY", "USE POS RECEIPT");

				pceft_debug(pceftpos_handle.instance, "Printer Error: ");
				vim_event_sleep(2000);
			}
		}
    }
	VIM_ERROR_RETURN(res);
}

#endif


/*----------------------------------------------------------------------------*/
/**
 * Wait for a key to be pressed and listen on POS port at the same time
 *
 * Assumes keyboard handle is already opened
 *
 * \return
 * >= 0 - Key pressed as defined in OEM.h, eg, VIM_KBD_CODE_SOFT1, VIM_KBD_CODE_SOFT2, N2 etc
 * VIM_ERROR_POS_CANCEL - a command that wasn't a heartbeat was received from the POS
 * VIM_ERROR_TIMEOUT - timeout expired without getting a keypress or non-heartbeat command from the pos
 */
VIM_ERROR_PTR pceft_get_key
(
  VIM_KBD_CODE * key_code_ptr,
  VIM_MILLISECONDS timeout
)
{
  VIM_KBD_CODE key_code;
  VIM_EVENT_FLAG_PTR events[1];

  if (key_code_ptr==VIM_NULL)
  {
    key_code_ptr=&key_code;
  }

  VIM_ERROR_RETURN_ON_FAIL(vim_adkgui_kbd_touch_or_other_read(key_code_ptr, timeout, events, 1));

  VIM_ERROR_RETURN_ON_FALSE(*key_code_ptr!=VIM_KBD_CODE_CANCEL, VIM_ERROR_USER_CANCEL);
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR ConnectClient(void)
{
    VIM_ERROR_PTR res = VIM_ERROR_NONE;
    VIM_CHAR buffer[100];
    VIM_SIZE count = 2;

    vim_mem_clear( buffer, VIM_SIZEOF( buffer ));
    terminal.IdleScreen = DISP_IDLE_PCEFT;
    vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "Connect To Register");
    pceft_settings_load_only();
    do
    {
        display_screen( DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, buffer, "");
        if((( res = pceft_settings_reconfigure(VIM_FALSE)) != VIM_ERROR_NONE )&&( terminal.terminal_id[0] != 'L' ))
        {
            vim_sprintf((VIM_CHAR *)buffer, "v%8d.%2d\n%s", BUILD_NUMBER, REVISION, "PCEFT Comms Error");
            display_screen( DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
            VIM_DBG_STRING(buffer);
        }

    } while ((res != VIM_ERROR_NONE) && ( --count ));

    // RDD Wait forever for someone to set up the pos interface or for the POS to connect
    IsPosConnected(VIM_TRUE);

    //VIM_ERROR_IGNORE(txn_load());

	// New Terminals need to at least try to pick up a TID from the lane
    if( 1 )	
    {        
        if (pceft_request_config() == VIM_ERROR_NONE)
        {
            terminal_set_status(ss_tid_configured);
	        CompleteTxn(VIM_TRUE);
        }
        else
        {
            // RDD 260516 SPIRA:8958 Power-fail reversal processing. BD noticed that if PCEFT Heartbeat was set to 2 secs then        
            // The config failed and necessary reversal wasn't generated       
            terminal_clear_status(ss_tid_configured);
            vim_sprintf((VIM_CHAR *)buffer, "v%d.%d\n%s", BUILD_NUMBER, REVISION, "PCEFT Config Failed\nCheck POS Settings");

            if (ValidateConfigData(terminal.terminal_id, terminal.merchant_id))
            {
                display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, (VIM_CHAR *)buffer, "");
                vim_event_sleep(10000);
                CompleteTxn(VIM_TRUE);
            }
        }
    }     
    return VIM_ERROR_NONE;
}



/*------------------------- RDD 280212 Auto-Config Request --------------------------*/
// RDD 171221 change to flow - assume current config is correct if we run out of retries
VIM_ERROR_PTR pceft_request_config(void)
{	  
	VIM_PCEFTPOS_INITIALIZATION_REQUEST config_request;
	VIM_PCEFTPOS_CONFIG_RESPONSE config_response;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_UINT8 Count=0, Retries=3;
	// Send out the auto-config request to the POS and copy the TID and MID into a normal request structure
	 
	if( IS_STANDALONE )
		return VIM_ERROR_NONE;

	do
	{
		VIM_ERROR_IGNORE( vim_pceftpos_eft_config( pceftpos_handle.instance, &config_response ) );

		if (terminal.terminal_id[0] && terminal.terminal_id[0] == 'L')
		{
			pceft_debug_test(pceftpos_handle.instance, "L TID : Ignore Data returned from POS");
			return VIM_ERROR_NONE;
		}
		
		// RDD 260516 - make this return an error if failed as we need to go into config required state
		// RDD JIRA PIRPIN-1352 - ignore the error and simply retry
		if(( config_response.terminal_id[7] < '0' )||( config_response.terminal_id[7] > '9' ))
		{
			pceft_debug(pceftpos_handle.instance, "JIRA PIRPIN-1352" );
			vim_event_sleep(2000);
		}
		else
		{
			vim_mem_copy( config_request.terminal_id, config_response.terminal_id, 8 );
			vim_mem_copy( config_request.merchant_id, config_response.merchant_id, 15 );
			// simulate a real config request
			terminal.initator = it_pinpad;
			res = vim_pceftpos_callback_initialization_config( &config_request );
			break;
		}	 
	}
	while( Count++ < Retries );

	// Check the config we picked up
	if( !ValidateConfigData(terminal.terminal_id, terminal.merchant_id))
	{
		pceft_debug(pceftpos_handle.instance, "Current Config is Invalid");
		res = VIM_ERROR_SYSTEM;
	}

	//terminal_save();
	return res;
}


/*------------------------- RDD 280212 Auto-Config Request --------------------------*/
VIM_ERROR_PTR pceft_pos_send_heartbeat
(
  VIM_BOOL extended
)
{
  typedef struct pceft_heartbeat_response_t{
    VIM_CHAR command_code;
    VIM_CHAR sub_code;
    VIM_CHAR device_code;
    VIM_CHAR response_code[2];
    VIM_CHAR bank_code;
    VIM_CHAR hardware_type;
    VIM_CHAR extend_timeout;
  }pceft_heartbeat_response_t;
  
  pceft_heartbeat_response_t resp;
 
  vim_mem_set(&resp, '0', sizeof(resp));

  resp.command_code = PCEFT_CMD_HEARTBEAT_RESP;
  resp.sub_code = PCEFT_HEARTBEAT_SUB_DEFAULT;
  VIM_ERROR_RETURN_ON_NULL( pceftpos_handle.instance );
  vim_pceftpos_get_device_code(pceftpos_handle.instance, &resp.device_code);

  resp.bank_code = 'N';     /* todo: use a macro */
  resp.hardware_type = '4'; /* todo: use a macro */
  
  resp.extend_timeout = extended ? '1' : '0';

  VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_send(pceftpos_handle.instance, &resp,sizeof(resp)));

  return VIM_ERROR_NONE;
}

#ifndef _WIN32
static VIM_ERROR_PTR pceftpos_custom_wait_for_response(VIM_UINT32 ulTimeoutMs, VIM_UINT32 ulExpectedResponseLength)
{
    VIM_RTC_UPTIME tCurrent, tEndTimer;
    vim_rtc_get_uptime(&tCurrent);
    tEndTimer = tCurrent + ulTimeoutMs;
    while(tCurrent < tEndTimer)
    {
        VIM_UINT32 ulPending = mal_nfc_GetPendingPOSMessageLen();
        if(0 < ulPending)
        {
            if(ulExpectedResponseLength == ulPending)
            {
                //Purge response only if it exactly matches what we're expecting - we don't want to dequeue any events
                VIM_UINT8 *pbData = malloc(ulPending);
                VIM_DBG_PRINTF1("detected expected %u pending bytes, purging", ulPending);
                mal_nfc_ReadPendingPOSMessage(pbData, ulPending);
                FREE_MALLOC(pbData);
            }
            else
            {
                pceft_debugf(pceftpos_handle.instance, "Pending length %u does not match expected length %u - skipping purge", ulPending, ulExpectedResponseLength);
            }
            return VIM_ERROR_NONE;
        }
    }
    pceft_debug(pceftpos_handle.instance, "Timed out waiting for POS response");
    VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);
}
#endif


static VIM_ERROR_PTR pceftpos_custom_set_keymask(VIM_BOOL fCancelButton, VIM_BOOL fYesButton, VIM_BOOL fDeclineButton, VIM_BOOL fAcceptButton, VIM_BOOL fOKButton, PCEFTPOS_INPUT_DATA eInputData)
{
    typedef struct pceft_set_keymask_request_t {
        VIM_CHAR cCommandCode;
        VIM_CHAR cCancelButton;
        VIM_CHAR cYesButton;
        VIM_CHAR cDeclineButton;
        VIM_CHAR cAcceptButton;
        VIM_CHAR cInputDataField;
        VIM_CHAR cOkButton;
        VIM_CHAR cRFU1;
        VIM_CHAR cRFU2;
    } pceft_set_keymask_request_t;
    pceft_set_keymask_request_t sReq = { 0 };

#define BOOL_TO_CHAR(x) ((x) ? '1' : '0')

    sReq.cCommandCode    = 'U';
    sReq.cCancelButton   = BOOL_TO_CHAR(fCancelButton);
    sReq.cYesButton      = BOOL_TO_CHAR(fYesButton);
    sReq.cDeclineButton  = BOOL_TO_CHAR(fDeclineButton);
    sReq.cAcceptButton   = BOOL_TO_CHAR(fAcceptButton);
    sReq.cInputDataField = (VIM_CHAR)eInputData;
    sReq.cOkButton       = BOOL_TO_CHAR(fOKButton);
    sReq.cRFU1           = '0';
    sReq.cRFU2           = '0';

    VIM_DBG_LABEL_DATA("Sending Keymask Data", &sReq, sizeof(sReq));
    VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_send(pceftpos_handle.instance, &sReq, sizeof(sReq)));
#ifndef _WIN32
    VIM_ERROR_RETURN_ON_FAIL(pceftpos_custom_wait_for_response(2000, 6));
#endif
    return VIM_ERROR_NONE;
}

#define DISPLAY_LINE_LEN      20
#define DIALOG_PARAMETERS_LEN 45
VIM_ERROR_PTR pceftpos_custom_display(const VIM_CHAR* pszDisplayLine1, const VIM_CHAR* pszDisplayLine2, VIM_PCEFTPOS_GRAPHIC eGraphicsCode, PCEFTPOS_INPUT_DATA eInputData, VIM_PCEFTPOS_KEYMASK eKeymask, const VIM_CHAR* pszPAD)
{
    typedef struct pceft_display_request_t {
        struct sBase
        {
            VIM_CHAR cCommandCode;
            VIM_CHAR cSubCode;
            VIM_CHAR cDeviceCode;
            VIM_CHAR acDisplayLine1[20];
            VIM_CHAR acDisplayLine2[20];
            VIM_CHAR cGraphicsCode;
            VIM_CHAR acDialogParameters[45];
        } sBase;
        struct sPad
        {
            VIM_CHAR szPadData[999+1];
        } sPad;
    } pceft_display_request_t;
    pceft_display_request_t sReq;

    vim_mem_clear(&sReq, sizeof(sReq));

#define M2B(x) ((eKeymask&(x))?VIM_TRUE:VIM_FALSE)
    VIM_ERROR_RETURN_ON_FAIL(pceftpos_custom_set_keymask(M2B(VIM_PCEFTPOS_KEY_CANCEL), M2B(VIM_PCEFTPOS_KEY_YES), M2B(VIM_PCEFTPOS_KEY_NO), M2B(VIM_PCEFTPOS_KEY_AUTHORISE), M2B(VIM_PCEFTPOS_KEY_OK), eInputData));

    sReq.sBase.cCommandCode = 'S'; //Command Code: POS_DISPLAY
    sReq.sBase.cSubCode     = '0'; //Command Sub-Code: Display Data
    sReq.sBase.cDeviceCode  = '0'; //Device Code: EFTPOS PIN pad device

    vim_strncpy(sReq.sBase.acDisplayLine1, pszDisplayLine1, sizeof(sReq.sBase.acDisplayLine1)); //Data for line 1
    vim_strncpy(sReq.sBase.acDisplayLine2, pszDisplayLine2, sizeof(sReq.sBase.acDisplayLine2)); //Data for line 2

    sReq.sBase.cGraphicsCode = eGraphicsCode; //Graphics Code

    vim_mem_set(sReq.sBase.acDialogParameters, ' ', sizeof(sReq.sBase.acDialogParameters)); //Dialog Parameters

    if (pszPAD)
    {
        vim_snprintf(sReq.sPad.szPadData, sizeof(sReq.sPad.szPadData), "%03u%s", vim_strlen(pszPAD), pszPAD);
    }

    VIM_DBG_LABEL_DATA("Sending Display", &sReq, sizeof(sReq.sBase) + vim_strlen(sReq.sPad.szPadData));
    VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_send(pceftpos_handle.instance, &sReq, sizeof(sReq.sBase) + vim_strlen(sReq.sPad.szPadData)));
#ifndef _WIN32
    VIM_ERROR_RETURN_ON_FAIL(pceftpos_custom_wait_for_response(2000, 8));
#endif
    return VIM_ERROR_NONE;
}
