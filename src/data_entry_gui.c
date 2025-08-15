
#include <incs.h>
VIM_DBG_SET_FILE("T6");


#ifdef _WIN32

static VIM_TEXT const * entry_font = DISPLAY_FONT_MEDIUM;

static VIM_GRX_VECTOR entry_offset;
alignment_t entry_alignment = align_center;

char * alpha_table[] = 
{
  "0 ./\\*-@:~,_",
  "1QZqz",
  "2ABCabc",
  "3DEFdef",
  "4GHIghi",
  "5JKLjkl",
  "6MNOmno",
  "7PRSprs",
  "8TUVtuv",
  "9WXYwxy"
};



static char alpha_shift(char c)
{
  VIM_UINT8 i, j, digits;

  for (i=0;i<ARRAY_LENGTH(alpha_table);i++)
  {
    digits = vim_strlen(alpha_table[i]);
    for (j=0; j < digits; j++)
    {
      if (alpha_table[i][j] == c)
      {
        j++;
        j = j % digits;
        return alpha_table[i][j];
      } 
    }
  }
  return c;
}

static VIM_ERROR_PTR decrease_font_size(void)
{
  if (vim_mem_compare(entry_font,DISPLAY_FONT_MEDIUM,VIM_SIZEOF(DISPLAY_FONT_MEDIUM)+1)==VIM_ERROR_NONE)
  {
    entry_font = DISPLAY_FONT_SMALL;
  }
  else if (vim_mem_compare(entry_font,DISPLAY_FONT_LARGE,VIM_SIZEOF(DISPLAY_FONT_LARGE)+1)==VIM_ERROR_NONE)
  {
    entry_font = DISPLAY_FONT_MEDIUM;
  }
  return VIM_ERROR_NONE;
}

/* Format string to display */
static void set_entry_string(char *data_string, char *display_string, VIM_UINT32 buf_len, kbd_options_t const *options)
{
  char seg[4];
  VIM_UINT8 len, i;
  VIM_UINT64 amt;
  VIM_CHAR *Ptr = VIM_NULL;

  if (options->entry_options & KBD_OPT_IP)
  {
    len = vim_strlen(data_string);
    display_string[0] = 0;

    // 190220 DEVX - exit if data is already formatted
    if (vim_strchr(data_string, '.', &Ptr) == VIM_ERROR_NONE)
    {
        vim_strcpy(display_string, data_string);
        return;
    }

    for (i=0; i*3 < len; i++)
    {
      vim_snprintf(seg, sizeof(seg), "%3.3s", &data_string[i*3]);
      vim_strncat(display_string, seg, vim_strlen(seg));
	  if (len >= ((i + 1) * 3) && (i != 3))
	  {
		  vim_strncat(display_string, ".", vim_strlen("."));
	  }
    }
    return;
  }

  if (options->entry_options & KBD_OPT_AMOUNT)
  {
    amt = asc_to_bin(data_string, vim_strlen(data_string));
    bin_to_amt(amt, display_string, buf_len);
    return;
  }

  if (options->entry_options & KBD_OPT_MASKED)
  {
    vim_mem_set(display_string, '*', vim_strlen(data_string));
    display_string[vim_strlen(data_string)] = 0;
    return;
  }

  if((options->entry_options & KBD_OPT_EXPIRY)||(options->entry_options & KBD_OPT_SETTLE))
  {
	  const VIM_CHAR *DispText = (options->entry_options & KBD_OPT_SETTLE) ? "DD/MM" : "MM/YY";    
	  len = vim_strlen(data_string);    
	  vim_strncpy(display_string, DispText, vim_strlen(DispText));    
	  display_string[vim_strlen(DispText)] = 0;    
	  if (len > 0)		
		  display_string[0] = data_string[0];		
	  if (len > 1)      
		  display_string[1] = data_string[1];    
	  if (len > 2)      
		  display_string[3] = data_string[2];    
	  if (len > 3)      
		  display_string[4] = data_string[3];    
	  return;
  }

  vim_strncpy(display_string, data_string, vim_strlen(data_string));
  display_string[vim_strlen(data_string)] = 0;
  
  return;
}


static VIM_ERROR_PTR display_entry_string(char *display_string)
{
	if (use_vim_screen_functions)
	{
		return vim_screen_display_terminal("Entry", display_string);
	}
	else
	{
	  VIM_GRX_VECTOR screen;
	  VIM_GRX_VECTOR line;
	  VIM_GRX_VECTOR offset;
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_size_in_pixels(&screen));
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_set_font_by_name(entry_font));
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_text_size_in_pixels(display_string,&line));

	  if (line.x > screen.x)
	  {
		decrease_font_size();
		VIM_ERROR_RETURN_ON_FAIL(vim_lcd_set_font_by_name(entry_font));
		VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_text_size_in_pixels(display_string,&line));
	  }
	  offset=entry_offset;

	  switch (entry_alignment)
	  {
		case align_center:
		  if (screen.x > line.x)
			offset.x = ((screen.x - entry_offset.x) - line.x) / 2;
		  break;
	      
		case align_right:
		  if (screen.x > line.x)
			offset.x = screen.x - line.x;
		  break;
	      
		case align_left:
		  offset.x = entry_offset.x;
		  break;
	  }
	  /*VIM_DBG_STRING(display_string);*/
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_put_text_at_pixel(offset,display_string));
	  return VIM_ERROR_NONE;
	}
}

/* Clear out the area occupied by the display string */
static VIM_ERROR_PTR clear_entry_area(char *display_string)
{
	if (use_vim_screen_functions)
	{
		return vim_screen_display_terminal("Entry", "                        ");
	}
	else
	{
	  VIM_GRX_VECTOR screen;
	  VIM_GRX_VECTOR line;
	  VIM_GRX_VECTOR offset;
	  
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_size_in_pixels(&screen));
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_set_font_by_name(entry_font));
	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_text_size_in_pixels(display_string,&line));

	  offset=entry_offset;

	  switch (entry_alignment)
	  {
		case align_center:
			offset.x = ((screen.x - entry_offset.x) - line.x) / 2;
		  break;
	      
		case align_right:
		  if (screen.x > line.x)
			offset.x = screen.x - line.x;
		  break;
	      
		case align_left:
		  offset.x = entry_offset.x;
		  break;
	  }

	  vim_lcd_put_text_at_pixel(offset,"                        ");
	  return VIM_ERROR_NONE;
	}
}


/**
 * Perform data entry on the terminal keypad.
 *
 * display_screen() should be called before calling this function to display the prompt and set point where entered data is displayed
 *
 * \param[out] inbuf
 * buffer to store the entered data in.
 *
 * \param[in] options
 * options structure to specify min and max data to enter. Timeout. Type of entry (amount, ip, expiry etc.)
 *
 * \return
 * VIM_ERROR_NONE - data entered successfully
 * VIM_ERROR_USER_BACK - clear key pressed
 * VIM_ERROR_USER_CANCEL - cancel key pressed
 * VIM_ERROR_TIMEOUT - timeout occured
 * VIM_ERROR_POS_CANCEL - cancelled via pos.
 */

VIM_ERROR_PTR data_entry
(
    char *inbuf,
    kbd_options_t const* options
)
{
  VIM_UINT32 timeout;
  VIM_BOOL wait_alpha = VIM_FALSE;
  VIM_UINT8 min, max;
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  char data[100];
  char display_string[100];
  //VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_KBD_CODE last_key_code;
  VIM_UINT8 count=0;
  //VIM_SEMAPHORE_TOKEN_HANDLE token_handle;

  if (options->entry_options & KBD_OPT_IP)
  {
    min = 12;
    max = 12;
  }
  else if((options->entry_options & KBD_OPT_EXPIRY)||(options->entry_options & KBD_OPT_SETTLE))
  {	  
	  // RDD 190315 - allow bypass for settle date		
	  min = (options->entry_options & KBD_OPT_SETTLE) ? 0 : 4;    
	  max = 4;
  }
  else
  {
    min = options->min;
    max = options->max;
  }
  
  vim_mem_clear(data, sizeof(data));

  /* flush buffered characters */
  /* read(kbd_handle, read_buf, sizeof(read_buf)); */

  display_string[0] = 0;

  if (options->entry_options & KBD_OPT_DISP_ORIG)
  {
    vim_strncpy(data, inbuf, vim_strlen(inbuf));
    data[vim_strlen(inbuf)] = 0;
    count = vim_strlen(data);
    set_entry_string(data, display_string, sizeof(display_string), options);
    display_entry_string(display_string);
  }
  else if (options->entry_options & (KBD_OPT_SETTLE|KBD_OPT_EXPIRY|KBD_OPT_AMOUNT|KBD_OPT_IP))
  {
    set_entry_string(data, display_string, sizeof(display_string), options);
    display_entry_string(display_string);
  }
  else
  {
	  clear_entry_area(display_string);
  }

  last_key_code = VIM_KBD_CODE_OK;

  VIM_DBG_NUMBER( options->timeout );
  vim_kbd_flush();  

  while (1)
  {
    VIM_KBD_CODE key_code;
    VIM_EVENT_FLAG_PTR events[1];

    timeout = wait_alpha ? 1000 : options->timeout;
    res = vim_adkgui_kbd_touch_or_other_read(&key_code, timeout, events, 1);

	if( res == VIM_ERROR_TIMEOUT ) /* On timeout  */
    {      
		if (!wait_alpha )      
		{        
			VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);      
		}     
		wait_alpha = VIM_FALSE;      
		continue;
    }
    else 
    {      
		VIM_ERROR_RETURN_ON_FAIL(res);
    }
	DBG_POINT;

    if ((key_code >= VIM_KBD_CODE_KEY0) && (key_code <= VIM_KBD_CODE_KEY9) && ((count < max) || (wait_alpha && (last_key_code == key_code)))    /* number key pressed */
     && !((key_code == VIM_KBD_CODE_KEY0) && (count == 0) && ((options->entry_options & KBD_OPT_AMOUNT) || (options->entry_options & KBD_OPT_NO_LEADING_ZERO)))) /* no leading zero if this option on or amount entry*/
    {
      if (wait_alpha && (count > 0) && (last_key_code == key_code))
      {
        data[count-1] = alpha_shift(data[count-1]);
      }
      else 
      {
        if (options->entry_options & KBD_OPT_ALPHA)
        {
          wait_alpha = VIM_TRUE;
          last_key_code = key_code;
        }
        data[count] = key_code;
        count++;
        data[count] = 0;
      }
    }
    else if ((key_code == VIM_KBD_CODE_OK) &&                                   /* Enter */
             ((count >= min) /* have entered min digits */
          || ((options->entry_options & KBD_OPT_ALLOW_BLANK) && (count == 0))) /* or option to allow zero is on */
            )
    {
      DBG_STRING(data);
	  // 300818 RDD JIRA WWBP-118 - need whatever is on the display as it includes the '.' chars
	  if (options->entry_options & KBD_OPT_IP)
	  {
		  vim_strcpy(data, display_string);
	  }

      vim_strncpy(inbuf, data, vim_strlen(data));
      inbuf[vim_strlen(data)] = 0;

      // RDD 200121 JIRA PIRPIN-981
      DBG_MESSAGE("------------------ Clear Data Area --------------------");
      clear_entry_area(display_string);

      return VIM_ERROR_NONE;
    }
    else if (key_code == VIM_KBD_CODE_CLR)                                       /* backspace */
    {
      wait_alpha = VIM_FALSE;

      // 190220 DEVX Wipe the screen if clear on IP 
      if (options->entry_options & KBD_OPT_IP)
      {
          count = 0;
          vim_mem_clear(data, sizeof(data));
      }

      else if (count)
      {
        count--;
        data[count] = 0;
      }
	  /* RDD 110616 SPIRA:8990 v560-2 */
	  else if( options->entry_options & KBD_OPT_BEEP_ON_CLR )
	  {
		  vim_kbd_sound_invalid_key();
		  continue;
	  }
      else if (!(options->entry_options & KBD_OPT_PASSWORD))
      {
        //DBG_MESSAGE("RETURN CLEAR");
        return VIM_ERROR_USER_BACK;
      }
    }
    else if ((key_code == VIM_KBD_CODE_CANCEL) && (!(options->entry_options & KBD_OPT_PASSWORD)))
    {
      /* cancel key pressed */
      wait_alpha = VIM_FALSE;
		if((transaction_get_status( &txn, st_electronic_gift_card ))&&( is_integrated_mode() ))
			vim_kbd_sound_invalid_key();
      //DBG_MESSAGE("RETURN CANCEL");
		else
			if (count)	//BD CANCEL first clears buffer, then cancels
			{
				count = 0;
				vim_mem_clear(data, sizeof(data));
			}
			else
				return VIM_ERROR_USER_CANCEL;
    }
    else                                                    /* Invalid key pressed */
    {
      wait_alpha = VIM_FALSE;
      vim_kbd_sound_invalid_key();
      //vim_kbd_invalid_key_sound();
    }
    /* Clear entry line and write entered data to screen*/
    //vim_lcd_lock_update(&token_handle);
	DBG_POINT;
    clear_entry_area(display_string);
	DBG_POINT;
    set_entry_string(data, display_string, sizeof(display_string), options);
	DBG_POINT;
    display_entry_string(display_string);
	DBG_POINT;
    //vim_lcd_unlock_update(token_handle.instance);
  }
}


/*----------------------------------------------------------------------------*/

void data_entry_set_cursor(int x, int y, VIM_TEXT const* font_ptr, alignment_t alignment)
{
  entry_offset = vim_grx_vector(x, y);
  entry_font = font_ptr;
  entry_alignment = alignment;
}
#endif


/*----------------------------------------------------------------------------*/
/**
 * Wait for enter, clear or cancel to be pressed.
 */
VIM_ERROR_PTR data_entry_confirm( VIM_MILLISECONDS timeout, VIM_BOOL fAllowCancel, VIM_BOOL fCheckCardRemoved )
{
  VIM_KBD_CODE key_pressed;
  VIM_ERROR_PTR res;

  while ( VIM_TRUE )
  {		  
    if( fCheckCardRemoved )
	{
		res = screen_kbd_touch_read_with_abort( &key_pressed, timeout, VIM_FALSE, VIM_FALSE );
		if(( res != VIM_ERROR_USER_CANCEL ) && ( res != VIM_ERROR_NONE ))
		{
			txn.error = res;
			VIM_ERROR_RETURN_ON_FAIL( res );
		}
	}
	else
	{
        if (vim_adkgui_is_active()) 
        {
            VIM_EVENT_FLAG_PTR events[1];
            res = vim_adkgui_kbd_touch_or_other_read(&key_pressed, timeout, events, 1);
        }
	}
      
	switch (key_pressed)    
	{
        case VIM_KBD_CODE_OK:    /*  enter */
          return VIM_ERROR_NONE;

        case VIM_KBD_CODE_CANCEL: /*  cancel */
        case VIM_KBD_CODE_CLR:       /*  clear */
		  if( fAllowCancel )
		  {			   
			  if( fCheckCardRemoved )
				txn.error = VIM_ERROR_USER_CANCEL;
			  return VIM_ERROR_USER_CANCEL;
		  }
		  continue;
		  // otherwise run again with a new timeout

        default:
          /*vim_kbd_invalid_key_sound();*/
          //vim_kbd_sound_invalid_key();
          break;
	}
  }
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
#if 1
VIM_ERROR_PTR GetSettlementDate()
{  
	kbd_options_t entry_opts;  
	VIM_CHAR entry_buf[4+1];  
	VIM_UINT8 day, month;
	VIM_UINT64 res;
	entry_opts.timeout = ENTRY_TIMEOUT; 
	entry_opts.max = 4;

	entry_opts.entry_options = KBD_OPT_ALLOW_BLANK;

	//entry_opts.entry_options = KBD_OPT_SETTLE;
	entry_opts.entry_options |= KBD_OPT_BEEP_ON_CLR;
	entry_opts.CheckEvents = is_integrated_mode() ? VIM_TRUE : VIM_FALSE;

	for(;;)  
	{
		VIM_CHAR type[20];
		
		vim_mem_clear( entry_buf, VIM_SIZEOF(entry_buf) );
		get_txn_type_string(&txn, type, sizeof(type), VIM_FALSE);
#if 0		
		VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_SETTLE_DATE, VIM_PCEFTPOS_KEY_CANCEL, "SETTLEMENT", ""));
	    VIM_ERROR_RETURN_ON_FAIL( data_entry(entry_buf, &entry_opts));
#else
        // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
        VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, DISP_SETTLE_DATE, VIM_PCEFTPOS_KEY_CANCEL, "SETTLEMENT", ""));
#endif
        res = asc_to_bin(entry_buf, 4);
        day = res / 100;
        month = res % 100;

	    if(((month > 12) || (month == 0) || (day > 31) || ( day==0 )) && res )
		{	
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_INVALID_DATE, VIM_PCEFTPOS_KEY_NONE, &entry_buf[0], &entry_buf[2] ));
			vim_kbd_sound_invalid_key();
			VIM_ERROR_RETURN_ON_FAIL(vim_event_sleep(5000)); // wait 5 secs 
			//pceft_pos_display_close();    
			continue; 
		}
		break;
	}
	res = (100 * month) + day;
	bin_to_bcd( res, txn.settle_date, 4 );
    /* return without error */
    return VIM_ERROR_NONE;
}
#endif

