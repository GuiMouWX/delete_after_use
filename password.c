#include <incs.h>
VIM_DBG_SET_FILE("TH")

/*---------------------------------------------------------------------------------------------*/
VIM_ERROR_PTR password_compare(VIM_UINT32 screen_id, VIM_TEXT const *password, VIM_TEXT *password_prompt, VIM_BOOL no_cancel)
{
  VIM_INT32  password_bin;
  VIM_UINT8  i;

  /* Try 3 password attempts */
  for(i=0; i<3;i++)
  {
    /* display prompt */	 
    VIM_ERROR_RETURN_ON_FAIL( GetPassword( &password_bin, password_prompt ));
	if( (VIM_UINT64) password_bin == asc_to_bin( password, vim_strlen(password) ))
		return VIM_ERROR_NONE;
 
    /* See if a retry is in order */
    display_screen( DISP_PASSWORD_INVALID, VIM_PCEFTPOS_KEY_NONE, password_prompt );
	vim_event_sleep(2000);
   }	
   VIM_ERROR_RETURN( ERR_PASSWORD_MISMATCH );
}

typedef struct config_menu_t
{
  txn_type_t   type;
  VIM_KBD_CODE key_code;
  VIM_CHAR*    title;
}config_menu_t;

const config_menu_t pw_table[] = 
{
  {tt_sale,     VIM_KBD_CODE_KEY1, "TRAN"},
  {tt_refund,   VIM_KBD_CODE_KEY2, "RFND"},
  {tt_void,     VIM_KBD_CODE_KEY3, "VOID"},
  {tt_settle,   VIM_KBD_CODE_KEY4, "SETL"},
};
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR verify_old_password( txn_type_t type, VIM_TEXT *pw )
{
  VIM_TEXT *pw_old;
  
  switch( type )
  {
    case tt_sale:
      pw_old = terminal.configuration.password;
      break;
      
    case tt_refund:
      pw_old = terminal.configuration.refund_password;
      break;
      
    case tt_void:
      pw_old = terminal.configuration.void_password;
      break;
      
    case tt_settle:
      pw_old = terminal.configuration.settlement_password;
      break;

    default:
      return VIM_ERROR_NOT_FOUND;
  }

  VIM_DBG_STRING(pw);
  VIM_DBG_STRING(pw_old);

  if( vim_strncmp(pw, pw_old, vim_strlen(pw)) == 0 )
    return VIM_ERROR_NONE;

  return VIM_ERROR_MISMATCH;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR enter_old_password( config_menu_t *item )
{
  kbd_options_t entry_opts;
  VIM_CHAR entry_buf[5];

  entry_opts.timeout = ENTRY_TIMEOUT;
  entry_opts.min = 1;
  entry_opts.max = 4;
  entry_opts.entry_options = KBD_OPT_MASKED;
  entry_opts.CheckEvents = VIM_FALSE;
  
  while( VIM_TRUE )
  {
#if 0
    VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_PW_VERIFY_OLD, VIM_PCEFTPOS_KEY_CANCEL, item->title));
    VIM_ERROR_RETURN_ON_FAIL(data_entry(entry_buf, &entry_opts));
#else
      // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
      VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, DISP_PW_VERIFY_OLD, VIM_PCEFTPOS_KEY_CANCEL, item->title));
#endif

    if ( verify_old_password(item->type, entry_buf) == VIM_ERROR_NONE )
      return VIM_ERROR_NONE;

    return change_password_menu();
  }
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR save_new_pw( txn_type_t type, VIM_TEXT *pw )
{
  switch( type )
  {
    case tt_sale:
      vim_strncpy(terminal.configuration.password, pw, VIM_SIZEOF(terminal.configuration.password));
      break;
      
    case tt_refund:
      vim_strncpy(terminal.configuration.refund_password, pw, VIM_SIZEOF(terminal.configuration.refund_password));
	 VIM_DBG_DATA(terminal.configuration.refund_password, VIM_SIZEOF(terminal.configuration.refund_password)); 
      break;
      
    case tt_void:
      vim_strncpy(terminal.configuration.void_password, pw, VIM_SIZEOF(terminal.configuration.void_password));
      break;
      
    case tt_settle:
      vim_strncpy(terminal.configuration.settlement_password, pw, VIM_SIZEOF(terminal.configuration.settlement_password));
      break;

    default:
      return VIM_ERROR_NOT_FOUND;
  }
  VIM_ERROR_RETURN_ON_FAIL(terminal_save());

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR enter_new_password(VIM_UINT32 screen_id, config_menu_t *item, VIM_BOOL menu)
{
  kbd_options_t entry_opts;
  char entry_buf[5];

  entry_opts.timeout = ENTRY_TIMEOUT;
  entry_opts.min = 1;
  entry_opts.max = 4;
  entry_opts.entry_options = KBD_OPT_MASKED;
  entry_opts.CheckEvents = VIM_FALSE;
  
  while( VIM_TRUE )
  {
      vim_mem_clear(entry_buf, sizeof(entry_buf));
#if 0           
      VIM_ERROR_RETURN_ON_FAIL(display_screen(screen_id, VIM_PCEFTPOS_KEY_CANCEL, item->title));
      VIM_ERROR_RETURN_ON_FAIL(data_entry(entry_buf, &entry_opts));
#else
      // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
      VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, screen_id, VIM_PCEFTPOS_KEY_CANCEL, item->title));
#endif


    save_new_pw(item->type, entry_buf);

    if (!menu)
      return VIM_ERROR_NONE;
    
    VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TXN_ACCEPTED, VIM_PCEFTPOS_KEY_CANCEL));
    vim_event_sleep(ERROR_DISPLAY_TIMEOUT);
    return change_password_menu();
  }
}
VIM_ERROR_PTR enter_new_pswd(VIM_UINT32 screen_id, txn_type_t type )
{
  config_menu_t item;
  
  item.type = type;
  item.title = "";
  VIM_ERROR_RETURN_ON_FAIL(enter_new_password(screen_id, &item, VIM_FALSE));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR change_password( config_menu_t *item )
{
  VIM_ERROR_RETURN_ON_FAIL(enter_old_password(item));
  
  VIM_ERROR_RETURN_ON_FAIL(enter_new_password(DISP_PW_ENTER_NEW, item, VIM_TRUE));

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR change_password_menu( void )
{
  VIM_KBD_CODE key_code;
  VIM_ERROR_PTR result;
  config_menu_t *item;

  while( VIM_TRUE )
  {
    VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_PW_CHANGE_MENU,VIM_PCEFTPOS_KEY_CANCEL));
    VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read(&key_code,ENTRY_TIMEOUT));
        
    switch(key_code)
    {   
      case VIM_KBD_CODE_KEY1:
      case VIM_KBD_CODE_KEY2:
      case VIM_KBD_CODE_KEY3:
      case VIM_KBD_CODE_KEY4:
        item = (config_menu_t*)&pw_table[key_code - VIM_KBD_CODE_KEY1];
        result = change_password(item);
	 if (result != VIM_ERROR_USER_BACK)
	   return result;	
        break;
        
      case VIM_KBD_CODE_CANCEL:  /*  CANCEL */
      case VIM_KBD_CODE_CLR:  /*  CLEAR */
        return VIM_ERROR_NONE;

      default:        
		  ERROR_TONE;
        break;       
    }
  }   
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR change_merchant_password(void)
{
  kbd_options_t entry_opts;
  char entry_buf[5], pswd[5];

  entry_opts.timeout = ENTRY_TIMEOUT;
  entry_opts.min = 1;
  entry_opts.max = 4;
  entry_opts.entry_options = KBD_OPT_MASKED;
  entry_opts.CheckEvents = VIM_FALSE;

  while (1)
  {
    vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));
#if 0  
    VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_SCREEN_NEW_ENTER_MERCHANT_PASSWORD, VIM_PCEFTPOS_KEY_NONE));
    VIM_ERROR_RETURN_ON_FAIL(data_entry(entry_buf, &entry_opts));
#else
    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, DISP_SCREEN_NEW_ENTER_MERCHANT_PASSWORD, VIM_PCEFTPOS_KEY_NONE));
#endif

    vim_mem_copy(pswd, entry_buf, VIM_SIZEOF(entry_buf));
    vim_mem_clear(entry_buf, VIM_SIZEOF(entry_buf));
  
#if 0
    VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_SCREEN_VERIFY_ENTER_MERCHANT_PASSWORD, VIM_PCEFTPOS_KEY_NONE ));
    VIM_ERROR_RETURN_ON_FAIL( data_entry(entry_buf, &entry_opts));
#else
    // RDD 201218 JIRA WWBP-414 Create a generic function to handle display and data entry in all platforms
    VIM_ERROR_RETURN_ON_FAIL(display_data_entry(entry_buf, &entry_opts, DISP_SCREEN_NEW_ENTER_MERCHANT_PASSWORD, VIM_PCEFTPOS_KEY_NONE));
#endif

    if( VIM_ERROR_NONE == vim_mem_compare( pswd, entry_buf, vim_strlen(entry_buf)))
    {
      vim_mem_copy(terminal.configuration.password, pswd, VIM_SIZEOF(terminal.configuration.password));
      VIM_ERROR_RETURN_ON_FAIL(terminal_save());

      break;
    }
    else
    {
      /* See if a retry is in order */
      display_screen( DISP_PASSWORD_INVALID, VIM_PCEFTPOS_KEY_NONE );
      if(get_yes_no( ENTRY_TIMEOUT ) != VIM_ERROR_NONE)
         return VIM_ERROR_USER_CANCEL;
    }
  }

  return VIM_ERROR_NONE;
}

