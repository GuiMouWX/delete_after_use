#include <incs.h>
#ifndef _WIN32
#include <svc.h>
#endif
#include <vim_net.h>
#include <stdio.h>
#include <string.h>
VIM_DBG_SET_FILE("T4");


// Warning - this module is not portable. There are direct calls to Verifone OS/EOS.
#ifdef _VERIFONE

#define COMFIG_COMMS_FILE_PATH VIM_FILE_PATH_LOCAL "comms.cfg"
static VIM_TEXT const bt_fw_filename[] =  "VXBTAP690803.tar.gz";

/*----------------------------------------------------------------------------*/
// RDD 240215 now defined in structs.h as also used in diag.c
#if 0
typedef struct CONFIG_COMMS_KEY_MAP
{
  VIM_KBD_CODE key_code;
  /*@null@*/VIM_ERROR_PTR (*method)(void);
} CONFIG_KEY_MAP;
#endif
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_comms_key_map_run(VIM_UINT32 screen_id,CONFIG_KEY_MAP const* map)
{
  VIM_KBD_CODE key_code;
  void *Ptr = VIM_NULL;
  for(;;)
  {
    VIM_SIZE map_index;
    /* display screen */
    VIM_ERROR_RETURN_ON_FAIL(display_screen(screen_id, VIM_PCEFTPOS_KEY_NONE));
    /* wait for keypress */
    VIM_ERROR_RETURN_ON_FAIL(vim_kbd_read(&key_code,ENTRY_TIMEOUT));
    
    VIM_DBG_POINT;

    if (key_code==VIM_KBD_CODE_CANCEL)
    {
      VIM_ERROR_RETURN(VIM_ERROR_USER_CANCEL);
    }
    else if (key_code==VIM_KBD_CODE_CLR)
    {
      VIM_ERROR_RETURN(VIM_ERROR_USER_BACK);
    }
    VIM_DBG_POINT;

    for(map_index=0;
      (map[map_index].key_code!=VIM_KBD_CODE_NONE )&&
      (map[map_index].key_code!=key_code );
      map_index++)
    {
    }
      VIM_DBG_POINT;

    if (map[map_index].key_code==key_code)
    {
      /*VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound());*/
      VIM_ERROR_RETURN_ON_FAIL(map[map_index].method(Ptr));
    }
    else
    {
      VIM_ERROR_RETURN_ON_FAIL(vim_kbd_sound_invalid_key());
    }
  }
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_bt_base(void)
{
  vim_net_bt_base bt_base = {0x00};
  VIM_BOOL bt_supported;
  VIM_KBD_CODE key_code;

  vim_net_nwif_is_bt_supported( &bt_supported );
  if( !bt_supported )
    return VIM_ERROR_NONE;


  if( VIM_ERROR_NONE == vim_net_nwif_get_connected_bt_base_name( &bt_base ) && bt_base.addr[0] )
  {
    clrscr();
    printf("\n\nDelete Current Pair?" \
           "\n"\
           "Enter  - Delete and Continue\n" \
           "Cancel - Return");
    key_code = VIM_KBD_CODE_NONE;
    vim_kbd_read( &key_code,60000 );
  	if( key_code != VIM_KBD_CODE_OK )
      return VIM_ERROR_NONE;
  }
  return vim_net_nwif_bt_config();
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_bt_fw(void)
{
  VIM_BOOL bt_supported;
  VIM_KBD_CODE key_code;
  VIM_BOOL fw_file_exists = VIM_FALSE;
  VIM_TEXT full_filename[VIM_FILE_NAME_MAX_SIZE];

  vim_net_nwif_is_bt_supported( &bt_supported );
  if( !bt_supported )
    return VIM_ERROR_NONE;

  clrscr();

  vim_strcpy( full_filename, VIM_FILE_PATH_LOCAL );
  vim_strcat( full_filename, bt_fw_filename );
 
  VIM_ERROR_RETURN_ON_FAIL( vim_file_exists(full_filename,&fw_file_exists) );
  if( !fw_file_exists )
  {
    printf("\n\nNo BT Base Firmware Found\n");
    vim_kbd_read( &key_code,5000 );
    return VIM_ERROR_NONE;
  }

  printf("\n\nUpdate BT Base Firmware R9?\n" \
         "\n"\
         "Enter  - Reboot and Update\n" \
         "Cancel - Return");
  key_code = VIM_KBD_CODE_NONE;
  vim_kbd_read( &key_code,60000 );
	if( key_code != VIM_KBD_CODE_OK )
    return VIM_ERROR_NONE;

  return vim_net_nwif_update_bt_base_firmware( bt_fw_filename );
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_pos(void)
{
  return pceft_configure(VIM_FALSE);
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_gprs(void)
{
  VIM_KBD_CODE key_code;
  VIM_BOOL loop = VIM_TRUE;

  while( loop )
  {
    clrscr();
    printf("\n\nConfigure GPRS\n" \
           "\n"\
           "0 - Set APN\n" \
           "1 - Set Address\n" \
           "2 - Set Port\n" \
           "3 - Set SHA\n" \
           "Other - Return");
    key_code = VIM_KBD_CODE_NONE;
    vim_kbd_read( &key_code,60000 );
  	if( key_code == VIM_KBD_CODE_KEY0 )
  	{ 
      char apn_string[VIM_SIZEOF(terminal.acquirers[0].AQGPRSAPN)]={'\0'};
      kbd_options_t options;

      options.timeout = ENTRY_TIMEOUT;
      options.min = 0;
      options.entry_options = KBD_OPT_DISP_ORIG|KBD_OPT_ALPHA|KBD_OPT_ALLOW_BLANK;
      vim_mem_copy( apn_string, terminal.acquirers[0].AQGPRSAPN, VIM_SIZEOF(terminal.acquirers[0].AQGPRSAPN) );
      
      display_screen( DISP_GENERIC_ENTRY_POINT, 0, "APN", "APN", apn_string, apn_string );
      options.max = VIM_SIZEOF(apn_string);
      VIM_ERROR_RETURN_ON_FAIL(data_entry(apn_string, &options));
      if( apn_string[0] )
      {
        vim_mem_copy( terminal.acquirers[0].AQGPRSAPN, apn_string, VIM_SIZEOF(terminal.acquirers[0].AQGPRSAPN) );
        terminal_save();
      }
  	}
    else if( key_code == VIM_KBD_CODE_KEY1 )
    {
      char ip_string[40]={'\0'};
      kbd_options_t options;
      options.timeout = ENTRY_TIMEOUT;
      options.min = 0;
      options.entry_options = KBD_OPT_DISP_ORIG|KBD_OPT_ALPHA|KBD_OPT_ALLOW_BLANK;
      vim_mem_copy( ip_string, terminal.acquirers[0].AQIPTRANS.IPADDR, VIM_SIZEOF(ip_string)-1 );
      display_screen( DISP_GENERIC_ENTRY_POINT, 0, "IP ADDRESS", "IP ADDRESS", ip_string, ip_string );
      options.max = VIM_SIZEOF(ip_string)-1;
      VIM_ERROR_RETURN_ON_FAIL(data_entry(ip_string, &options));
      if( ip_string[0] )
      {
        vim_mem_set( terminal.acquirers[0].AQIPTRANS.IPADDR, 0x00, VIM_SIZEOF( terminal.acquirers[0].AQIPTRANS.IPADDR) );
        vim_mem_copy( terminal.acquirers[0].AQIPTRANS.IPADDR, ip_string, VIM_SIZEOF(ip_string) );
        terminal_save();
      }
    }
    else if( key_code == VIM_KBD_CODE_KEY2 )
    {
      char port_string[VIM_SIZEOF(terminal.acquirers[0].AQIPTRANS.IPPORT)]={'\0'};
      kbd_options_t options;

      options.timeout = ENTRY_TIMEOUT;
      options.min = 0;
      options.entry_options = KBD_OPT_DISP_ORIG|KBD_OPT_ALLOW_BLANK;
      vim_mem_copy( port_string, terminal.acquirers[0].AQIPTRANS.IPPORT, VIM_SIZEOF(port_string) );
      
      display_screen( DISP_GENERIC_ENTRY_POINT, 0, "IP PORT", "IP PORT", port_string, port_string );
      options.max = VIM_SIZEOF(port_string);
      VIM_ERROR_RETURN_ON_FAIL(data_entry(port_string, &options));
      if( port_string[0] )
      {
        vim_mem_copy( terminal.acquirers[0].AQIPTRANS.IPPORT, port_string, VIM_SIZEOF(port_string) );
        terminal_save();
      }
    }
    else if( key_code == VIM_KBD_CODE_KEY3 )
    {
      char sha_string[8+1]={'\0'};
      kbd_options_t options;

      options.timeout = ENTRY_TIMEOUT;
      options.min = 0;
      options.entry_options = KBD_OPT_DISP_ORIG|KBD_OPT_ALLOW_BLANK;
      vim_utl_bcd_to_asc( terminal.acquirers[0].AQHOSTADDR, sha_string, VIM_SIZEOF(sha_string)-1 );
      display_screen( DISP_GENERIC_ENTRY_POINT, 0, "SHA", "SHA", sha_string, sha_string );
      options.max = VIM_SIZEOF(sha_string)-1;
      VIM_ERROR_RETURN_ON_FAIL(data_entry(sha_string, &options));
      if( sha_string[0] )
      {
        vim_utl_asc_to_bcd( sha_string, terminal.acquirers[0].AQHOSTADDR,VIM_SIZEOF(sha_string));
        terminal_save();
      }
    }
    else
      loop = VIM_FALSE;
  }
  terminal_save();
  comms_force_disconnect(); // Comms method was changed, force disconnection now to have fresh start when "connect" command come
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_pstn(void)
{
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_internal_modem(void)
{
  VIM_KBD_CODE key_code;

  clrscr();
  printf("\n\nSelect Internal Modem\n" \
         "\n"\
         "0 - None\n" \
         "1 - PSTN\n" \
         "2 - GPRS\n" \
         "Other - Return");
  key_code = VIM_KBD_CODE_NONE;
  vim_kbd_read( &key_code,60000 );
	if( key_code == VIM_KBD_CODE_KEY0 )
    terminal.internal_modem = internal_modem_none;
  else if( key_code == VIM_KBD_CODE_KEY1 )
    terminal.internal_modem = internal_modem_pstn;
  else if( key_code == VIM_KBD_CODE_KEY2 )
    terminal.internal_modem = internal_modem_gprs;

  terminal_save();
  comms_force_disconnect(); // Comms method was changed, force disconnection now to have fresh start when "connect" command come
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR config_power_off_timeout(void)
{
  char timeout_string[5+1]={'\0'};
  kbd_options_t options;
  VIM_SIZE timeout_val;

  options.timeout = ENTRY_TIMEOUT;
  options.min = 0;
  options.entry_options = KBD_OPT_DISP_ORIG|KBD_OPT_ALPHA|KBD_OPT_ALLOW_BLANK;
  vim_utl_bin_to_asc( timeout_string, terminal.power_off_timer_minutes, VIM_SIZEOF(timeout_string)-1 );
  
  display_screen( DISP_GENERIC_ENTRY_POINT, 0, "POWER OFF TIMEOUT", "TIMEOUT", timeout_val ? timeout_string:" ", "TIMEOUT" );
  options.max = VIM_SIZEOF(timeout_string)-1;
  VIM_ERROR_RETURN_ON_FAIL(data_entry(timeout_string, &options));
  if( timeout_string[0] )
  {
    vim_utl_ascii_decimal_to_size( timeout_string, vim_strlen(timeout_string), &timeout_val );
    terminal.power_off_timer_minutes = timeout_val;
    terminal_save();
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR config_comms_terminal_comms(void)
{
  VIM_KBD_CODE key_code;
  char modelno[20];
  char pos_comms[100];
//  char apaddr[16];
  char block0[200]={0x00};
  char block1[200]={0x00};
  char block2[200]={0x00};
  char block3[200]={0x00};
  char block4[200]={0x00};
  char block5[200]={0x00};
  char block6[200]={0x00};
  char block7[200]={0x00};
  char block8[200]={0x00};
  char ip_port[5+1]={'\0'};
  VIM_BOOL bt_supported;
  char pstn_phone[100]={0x00};
  int i;
  VIM_UINT32 battery_charge = 0x00;
  VIM_BOOL is_docked = VIM_TRUE;
  vim_net_bt_base bt_base = {0x00};
  char fw_version[40];
  char sha[4*2+1];
  VIM_GPRS_INFO gprs_info={0x00,0x00};
  VIM_CHAR con_network = '?';
    
  while( 1 )
  { // Build the screen
    display_screen( DISP_PROCESSING_TERMINAL_ONLY, VIM_PCEFTPOS_KEY_NONE );
    SVC_INFO_MODELNO( modelno );
    modelno[12] = 0x00;
    vim_snprintf( block0, VIM_SIZEOF(block0), "   Hardware = %s\n\n", modelno);
// ---- 1 ----
    vim_net_nwif_is_bt_supported( &bt_supported );
    if( bt_supported )
    {
      if( VIM_ERROR_NONE == vim_net_nwif_get_connected_bt_base_name( &bt_base ) && bt_base.addr[0] )
      {
        vim_snprintf( block1, VIM_SIZEOF(block1), 
          "1. BT Base Connected:\n" \
          "   Name %s\n" \
          "   Addr %s\n\n", bt_base.name, bt_base.addr );
      }
      else
      {
        vim_snprintf( block1, VIM_SIZEOF(block1), 
          "1. BT Base Not Connected\n\n" );
      }      
    }
    else
      vim_snprintf( block1, VIM_SIZEOF(block1), 
          "1. BT Base N/A\n\n" );


// ---- 2 ----
    if( VIM_ERROR_NONE == vim_net_nwif_get_bt_base_fw_ver( fw_version, VIM_SIZEOF(fw_version) ) )
      vim_snprintf( block2, VIM_SIZEOF(block2), "2. BT Base ver = %s\n\n", fw_version);
    else
      vim_snprintf( block2, VIM_SIZEOF(block2), "2. BT Base ver = N/A\n\n");
      
// ---- 3 ----
    pceft_settings_get_conf_string( pos_comms, VIM_SIZEOF(pos_comms) );
    vim_snprintf( block3, VIM_SIZEOF(block3), "3. POS = %s\n", pos_comms);

// ---- 4 ----
    vim_mem_copy( ip_port, terminal.acquirers[0].AQIPTRANS.IPPORT, MIN(sizeof(terminal.acquirers[0].AQIPTRANS.IPPORT),sizeof(ip_port)) );
    ip_port[5] = 0x00;  // fix ip port
    vim_mem_set(sha, 0x00, VIM_SIZEOF(sha));
    vim_utl_bcd_to_asc( terminal.acquirers[0].AQHOSTADDR, sha, VIM_SIZEOF(sha)-1 );


    if( terminal.internal_modem == internal_modem_gprs )
    {
      comms_connect(txn.acquirer_index, msg_advice);  // connect to open GPRS driver
    }

    if( VIM_ERROR_NONE != vim_gprs_get_info( &gprs_info ) )
    {
      vim_strcpy( gprs_info.imei, "N/A" );
      vim_strcpy( gprs_info.iccid, "N/A" );
      gprs_info.con_network = ' ';
    }

    if( gprs_info.con_network == '1' || gprs_info.con_network == '2' )
      con_network = '2';
    else if( gprs_info.con_network == '3' || gprs_info.con_network == '4' || gprs_info.con_network == '5' )
      con_network = '3';
    vim_snprintf( block4, VIM_SIZEOF(block4), 
        "4. GPRS = %s:%s\n" \
        "   %s %s\n" \
        "   IMEI: %s\n" \
        "   ICCID %s\n" \
        "   %cG, signal: %d%%%%\n",
                                terminal.acquirers[0].AQIPTRANS.IPADDR, ip_port,
                                terminal.acquirers[0].AQGPRSAPN, sha,
                                gprs_info.imei,
                                gprs_info.iccid,
                                con_network, gprs_info.signal_level_percent );
// ---- 5 ----
    for( i=0; i< VIM_SIZEOF(pstn_phone)/2-1; i++ )
    {
      if( ((terminal.acquirers[0].primary_phone_no[i]>>4)&0x0F) > 9 )
        break;
      pstn_phone[2*i] = (terminal.acquirers[0].primary_phone_no[i]>>4)+'0';

      if( (terminal.acquirers[0].primary_phone_no[i]&0x0F) > 9 )
        break;
      pstn_phone[2*i+1] = (terminal.acquirers[0].primary_phone_no[i]&0x0F)+'0';
    }
    vim_snprintf( block5, VIM_SIZEOF(block5), "5. PSTN = %s\n", pstn_phone );
   

// ---- 6 ----
    vim_snprintf( block6, VIM_SIZEOF(block6), "6. Power OFF Timer = %d\n", terminal.power_off_timer_minutes);

    display_screen( DISP_IDLE, VIM_PCEFTPOS_KEY_NONE, "", "" );

// ---- 7 ----
    if( VIM_ERROR_NONE == vim_terminal_get_battery_charge( &battery_charge ) )
    {
      vim_terminal_get_dock_status( &is_docked );
      vim_snprintf( block7, VIM_SIZEOF(block7), "   Battery = %d%%%%, %s\n", battery_charge, is_docked?"Docked":"Not Docked" );
    }
    else
      vim_snprintf( block7, VIM_SIZEOF(block7), "   Battery N/A\n");

// ---- 8 ----
    if( terminal.internal_modem == internal_modem_pstn )
      vim_snprintf( block8, VIM_SIZEOF(block7), "8  Internal Modem = PSTN" );
    else if ( terminal.internal_modem == internal_modem_gprs )
      vim_snprintf( block8, VIM_SIZEOF(block7), "8  Internal Modem = GPRS" );
    else
      vim_snprintf( block8, VIM_SIZEOF(block7), "8  Internal Modem = None" );

// ---- end ----

    display_screen( DISP_IDLE, VIM_PCEFTPOS_KEY_NONE, "", "" );

    clrscr();
    printf( block0 );
    printf( block1 );
    printf( block2 );
    printf( block3 );
    printf( block4 );
    printf( block5 );
    printf( block6 );
    printf( block7 );
    printf( block8 );

    VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( &key_code, 30000, VIM_FALSE ) );
    switch( key_code )
    {
      case VIM_KBD_CODE_KEY1: config_bt_base();
        break;

      case VIM_KBD_CODE_KEY2: config_bt_fw();
        break;
        
      case VIM_KBD_CODE_KEY3: config_pos();
        break;
      case VIM_KBD_CODE_KEY4: config_gprs();
        break;
      case VIM_KBD_CODE_KEY5: config_pstn();
        break;
      case VIM_KBD_CODE_KEY6: config_power_off_timeout();
        break;

      case VIM_KBD_CODE_KEY8: config_internal_modem();
        break;

      case VIM_KBD_CODE_CANCEL:
      case VIM_KBD_CODE_CLR:
        VIM_ERROR_RETURN(VIM_ERROR_USER_CANCEL);

      default:
        /*vim_kbd_invalid_key_sound();*/
        vim_kbd_sound_invalid_key();
        break;
    }
  }
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_net_callback_display_bt_pairing()
{
  display_screen( DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "\nBT Pairing\n" );
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_net_callback_display_bt_pairing_result( VIM_BOOL result )
{
  if( !result )
  {
    display_screen( DISP_BLANK, VIM_PCEFTPOS_KEY_NONE, "\nBT Pairing\nError" );
    vim_event_sleep( 2000 );
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_net_callback_bt_base_try_again( VIM_BOOL *try_again )
{
VIM_KBD_CODE key_code = VIM_KBD_CODE_NONE;
  VIM_ERROR_RETURN_ON_NULL( try_again );

  *try_again = VIM_FALSE;

  clrscr();
  printf("\n\nError Connecting to the Base\n" \
         "\n"\
         "Enter  - Try Again\n" \
         "Cancel - Skip");
  vim_kbd_read( &key_code,60000 );
	if( key_code == VIM_KBD_CODE_OK )
    *try_again = VIM_TRUE;

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_net_callback_bt_base_try_reconnect( VIM_BOOL *try_again )
{
VIM_KBD_CODE key_code = VIM_KBD_CODE_NONE;
  VIM_ERROR_RETURN_ON_NULL( try_again );

  *try_again = VIM_TRUE;

  clrscr();
  printf("\n\nError Connecting to the Base\n" \
         "\n"\
         "Enter  - Switch Base ON\n" \
         "         and Try Again\n" \
         "Cancel - New Base Connection");
  vim_kbd_read( &key_code,60000 );
	if( key_code == VIM_KBD_CODE_CANCEL )
    *try_again = VIM_FALSE;

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_net_callback_get_bt_base_name( vim_net_bt_base *base )
{
VIM_UINT32 i;
vim_net_bt_base_list bt_base_list;
VIM_KBD_CODE key_code = VIM_KBD_CODE_KEY0;
//int filtered_array[VIM_NET_BT_SEARCH_MAX];
//int filtered_count;
//int array_idx;
  VIM_ERROR_RETURN_ON_NULL( base );

  DBG_POINT;

  while( key_code == VIM_KBD_CODE_KEY0 )
  {
    clrscr();
    printf("\nSearching for BT Base...\n" \
           "\n" \
           "Press Button on Base\n");

    vim_event_sleep(3000);

    vim_mem_clear( &bt_base_list, VIM_SIZEOF(bt_base_list) );
    base->name[0] = 0x00;
    base->addr[0] = 0x00;
	/* Vyshakh 11052018 Updated Scan to filter only base deevices from verifone device driver API */
    //vim_mem_clear( &filtered_array, VIM_SIZEOF(filtered_array) );
    //filtered_count = 0x00;
    
    VIM_ERROR_RETURN_ON_FAIL( vim_net_get_bt_base_list( &bt_base_list ) );

    //for( i=0,array_idx=0; i<bt_base_list.count; i++ )
    //  if( !memcmp( bt_base_list.base[i].name, "VFI", 3 ) )
    //  {
    //    filtered_array[array_idx++] = i;
    //    filtered_count++;
    //  }
      
  // do not use fancy UI - this may be called before it's setup
    clrscr();
    if( bt_base_list.count == VIM_NET_BT_SEARCH_MAX )
      printf( "\nMore than %d Devices Found\n\n", bt_base_list.count-1 );
      
		  DBG_POINT;
    if( !bt_base_list.count)
    {
      printf("\nNo Base Found, list: %d\n\n", bt_base_list.count);
    }
    else
    {
      printf("\nBases Found: %d\n\n", bt_base_list.count);
      
      printf("\nSelect Base to Pair\n\n");
      for( i=0; i<bt_base_list.count; i++ )
        printf("%d: %s\n", i+1, bt_base_list.base[i].name );
    }
		  DBG_POINT;

    printf("\n0: Search Again\n");

    key_code = VIM_KBD_CODE_NONE;
		  DBG_POINT;
    while( key_code != VIM_KBD_CODE_CANCEL )
    {
      vim_kbd_read( &key_code,60000 );
		  DBG_POINT;
      if( key_code == VIM_KBD_CODE_KEY0 )
        break;

		  DBG_POINT;
      if( key_code == VIM_KBD_CODE_CANCEL )
        return VIM_ERROR_NONE;

		  DBG_POINT;
      if(bt_base_list.count)
      	if( key_code >= VIM_KBD_CODE_KEY1 && key_code <= VIM_KBD_CODE_KEY1 + bt_base_list.count -1)
      	{
          vim_mem_copy( base->name, bt_base_list.base[key_code-VIM_KBD_CODE_KEY0-1].name, VIM_SIZEOF(base->name) );
          vim_mem_copy( base->addr, bt_base_list.base[key_code-VIM_KBD_CODE_KEY0-1].addr, VIM_SIZEOF(base->addr) );
      		clrscr();

		  DBG_POINT;
          put_env( "BT_APADDR", base->addr, vim_strlen(base->addr) );
          put_env( "BT_APNAME", base->name, vim_strlen(base->name) );
          return VIM_ERROR_NONE;
      	}
    }
  }
  return VIM_ERROR_NONE;
}

#endif


