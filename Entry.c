/***
* Copyright (c) 2004 Sagem Monetel SA, rue claude Chappe,
* 07503 Guilherand-Granges, France, All Rights Reserved.
*
* Sagem Monetel SA has intellectual property rights relating
* to the technology embodied in this software.  In particular, 
* and without limitation, these intellectual property rights 
* may include one or more patents.
*
* This software is distributed under licenses restricting 
* its use, copying, distribution, and decompilation.  
* No part of this software may be reproduced in any form 
* by any means without prior written authorization of 
* Sagem Monetel.
*
* @Title:        
* @Description:  SDK30 sample application
* @Reference:    
* @Comment:      
*
* @author        
* @version       
* @Comment:      
* @date:         
*/
#include "incs.h"
#ifdef SAGEM_SPECIFIC

#include <bitmap.h>
VIM_DBG_SET_FILE("TA");
// Bitmaps definitions. 
static const unsigned char appname[]    = "NABEFTPOS";

#define __ENTER_KEY   -1
#define __BACK_KEY    -2
#define __EXIT_KEY    -3

#define NUMBER_OF_ITEMS(a)      (sizeof(a)/sizeof((a)[0]))

// 1 second Delay
#define __1_SECOND__    100
#define __3_SECONDS__   300
#define __500_MSECONDS__  50

void Wait_1Second( void ) 
{
  ttestall( 0, __1_SECOND__ );
}

//
int ManageMenu( const char *szTitle, int bRadioButtons, int nDefaultChoice, 
        int nItems, const char* Items[] )
{
  FILE *hDisplay;
  int DisplayHeaderStatus;

  // Menu.
  StructList Menu;
  int nY;
  int nMaxX;
  int nMaxY;

  ENTRY_BUFFER Entry;

  //
  int i;
  int nInput;
  int nReturn;

  //
  hDisplay = fopen( "DISPLAY", "w" );

  // Get Screen size.
  GetScreenSize( &nMaxY, &nMaxX );

  // For the menu height of the menu,
  nY = 0;
  DisplayHeaderStatus=StateHeader(0); // disable display header

  if ((nDefaultChoice < 0) || (nDefaultChoice >= nItems))
  {
    nDefaultChoice = 0;
  }

  //
  CreateGraphics(_MEDIUM_);

  //
  vim_mem_clear( &Menu, sizeof(Menu) );
  Menu.MyWindow.left   = 0;
  Menu.MyWindow.top    = nY;
  Menu.MyWindow.rigth  = nMaxX - 1;
  Menu.MyWindow.bottom = nMaxY - 1;
  if( nMaxY == 128 )
  {
    Menu.MyWindow.nblines = 10;
  }
  else
  {
    Menu.MyWindow.nblines = 5;
  }

  Menu.MyWindow.fontsize    = _MEDIUM_;
  Menu.MyWindow.type      = _PROPORTIONNEL_;
  Menu.MyWindow.font      = 0;
  Menu.MyWindow.correct   = _ON_;
  Menu.MyWindow.offset    = 0;
  Menu.MyWindow.shortcommand  = _ON_;
  if( bRadioButtons )
  {
    Menu.MyWindow.selected = _ON_;
  }
  else
  {
    Menu.MyWindow.selected = _OFF_;
  }

  Menu.MyWindow.thickness   = 2;
  Menu.MyWindow.border    = _ON_;
  Menu.MyWindow.popup     = _NOPOPUP_;
  Menu.MyWindow.first     = nDefaultChoice;
  Menu.MyWindow.current   = nDefaultChoice;
  Menu.MyWindow.time_out    = 60;
  Menu.MyWindow.title     = (unsigned char*)szTitle;

  for( i = 0; i < nItems; i++ )
  {
    Menu.tab[i] = (unsigned char*)Items[i];
  }

  G_List_Entry((void*)&Menu);
  ttestall(ENTRY, 0);
  nInput = Get_Entry((void*)&Entry);

  switch( nInput )
  {
  case CR_ENTRY_OK:
    nReturn = Entry.d_entry[0];
    break;

  case CR_ENTRY_NOK:
    nReturn = __EXIT_KEY;
    break;

  default:
    nReturn = __BACK_KEY;
    break;
  }
  StateHeader(DisplayHeaderStatus); // move display header in previous state
  fclose( hDisplay );
  
  return nReturn;
}

void test_stuff(void)
{
  VIM_UINT8 hour;
  DATE curr;
  read_date(&curr);
  
  terminal.dll_scheduled = TRUE;
  terminal.dll_preferred_version = 654321;
  terminal.merchant_info[0].func_vars.flags[4] |= (1 << 3);

  hour = asc_to_bin(curr.hour, 2);
  bin_to_bcd(hour, terminal.merchant_info[0].term_vars.dll_start_hour, 2);
  bin_to_bcd(hour, terminal.merchant_info[0].term_vars.dll_end_hour, 2);
  bin_to_bcd(0, terminal.merchant_info[0].term_vars.dll_start_minute, 2);
  bin_to_bcd(59, terminal.merchant_info[0].term_vars.dll_end_minute, 2); 
}

int more_function( NO_SEGMENT no, void *p1, void *p2 )
{
#if 0
#endif  
  return FCT_OK;
}


int after_reset( NO_SEGMENT no, void *p1, S_TRANSOUT *etatseq )
{
  unsigned char chgt;
  TYPE_CHGT  type;

  first_init( no, &chgt, &type);
  if ( chgt == 0xFF )
    raz_init(no);

  power_up_routine();

  return FCT_OK;
}

int is_state(NO_SEGMENT no,void *p1,S_ETATOUT *etatseq)
{
  S_ETATOUT etatout;
  int retour; 
  
  vim_mem_copy (&etatout, etatseq, sizeof(etatout));
  etatout.returned_state[etatout.response_number].state.response = REP_OK;
  vim_mem_copy (etatseq, &etatout, sizeof(etatout));
  retour = is_name (no, PT_NULL, etatseq);
  return (retour);
}

int is_name(NO_SEGMENT no,void *p1,S_ETATOUT *etatseq)
{
  S_ETATOUT etatout;
  
  vim_mem_copy ((char *)&etatout, (char *)etatseq, sizeof(etatout));
  vim_strncpy((char *)etatout.returned_state[etatout.response_number].appname,(char *)appname,vim_strlen(appname)+1);
  etatout.returned_state[etatout.response_number].no_appli = no;
  etatout.response_number++;
  vim_mem_copy (etatseq, &etatout, sizeof(etatout));
  return (FCT_OK);
}

int give_your_domain(NO_SEGMENT no,void *p1,S_INITPARAMOUT *param_out)
{
  S_INITPARAMOUT etatout;
  
  vim_mem_copy (&etatout, param_out, sizeof(etatout));
  etatout.returned_state[etatout.response_number].mask     = MSK_MDP|MSK_SWIPE|MSK_TYPE_PPAD|MSK_PINPAD|MSK_STANDARD|MSK_LANGUE|MSK_FRMT_DATE|MSK_DATE;
  etatout.returned_state[etatout.response_number].application_type = TYP_EXPORT;
  etatout.response_number++;
  vim_mem_copy (param_out, &etatout, sizeof(etatout));
  return (FCT_OK);
}

int is_delete(NO_SEGMENT no,void *paramin,S_DELETE *paramout)
{
  paramout->deleting=DEL_YES;
  return (FCT_OK);
}

int state (NO_SEGMENT noappli,void *p1,void *p2)
{
  object_info_t infos;
  FILE     *hPrinter;
  
  ObjectGetInfo(OBJECT_TYPE_APPLI, noappli, &infos);

  hPrinter = fopen( "PRINTER", "w-*" );
  if ( hPrinter != NULL ) 
  { 
    pprintf ("\n\n\n");
    pprintf ("\x1b""E%s\n""\x1b""F",appname);

    // get and print application file name and CRC
    pprintf ("Vers. : %s\n",infos.file_name);
    pprintf ("CRC   : %04x\n",infos.crc);

    ttestall (PRINTER, 0 );   
    fclose( hPrinter ); 
  }
    
  return FCT_OK;
}


int mcall (NO_SEGMENT noappli,void *p1,void *p2)
{
  return FCT_OK;
}

int consult (NO_SEGMENT noappli,void *p1,void *p2)
{
  return FCT_OK;
}

int time_function (NO_SEGMENT noappli,void *p1,void *p2)
{
  VIM_ERROR_PTR res=VIM_ERROR_NONE;
  VIM_SIZE saf_record_count;
  
  //DBG_MESSAGE("TIME");
  //DBG_TIMESTAMP;
  return (FCT_OK);
}

int keyboard_event(NO_SEGMENT noappli,S_KEY *p1,S_KEY *p2)
{
  S_KEY keyA, keyB;
  
  vim_mem_copy(&keyA,p1,sizeof(keyA));
  vim_mem_copy(&keyB,p2,sizeof(keyB));
  switch (keyA.keycode)
  {
    case T_POINT:
    case T_F :
      {
        peripherals_open(FALSE);
        data_entry_key_beep();
        switch (keyA.keycode)
        {
          case T_POINT:
            mnu_admin();
            keyB.keycode = 0;
            break;
          case T_F:
            if (config_functions(0)!=ERR_TELLIUM_MENU_REQUESTED)
            {
               keyB.keycode = 0;
            }
            break;
        }
        //read_date(&terminal.last_activity);
        peripherals_close(FALSE);
        
      }
      break;

    case N0:
    case N1:  
    case N2:
    case N3: 
    case N4: 
    case N5: 
    case N6: 
    case N7: 
    case N8: 
    case N9: 
    case T_VAL :
    case F4 : 
    case F1 : 
    case F2 : 
    case F3 : 
    case T_CORR :
    case T_ANN : 
    case NAVI_CLEAR : 
    case NAVI_OK : 
    case UP : 
    case DOWN : // return the same key value for keys above ! 
      // inhibits these keys for International domain
      //DBG_VAR(keyA.keycode);
      keyB.keycode = 0 ; 
      break; 
  }
  vim_mem_copy(p2,&keyB,sizeof(keyB));
  return (FCT_OK);
}

int file_received(NO_SEGMENT no,S_FILE * param_in,void *p2)
{
  FILE           *prt;
  S_FS_PARAM_CREATE ParamCreat;
  int    Retour;
  char     Dir_File[25];
  char     Dir_Label[25];
  S_FS_FILE      *pFile;
  
  prt  = fopen("PRINTER","w-");
  pprintf("File Received :\n/%s/%s",param_in->volume_name,param_in->file_name);
  pprintf("\n\n\n\n\n");
  ttestall(PRINTER,0);
  
  memclr(Dir_File,sizeof(Dir_File));
  memclr(Dir_Label,sizeof(Dir_Label));
  
  vim_snprintf(Dir_Label,sizeof(Dir_Label),"/%s",param_in->volume_name);
  ParamCreat.Mode = FS_WRITEONCE;
  Retour = FS_mount (Dir_Label,&ParamCreat.Mode);
  if (Retour == FS_OK)
  {
    vim_snprintf(Dir_File,sizeof(Dir_File),"/%s/%s",param_in->volume_name,param_in->file_name);
    // the file can be open at this stage
    pFile = FS_open (Dir_File, "r");
    // read the file and get parameters 
    FS_close(pFile);
    // cannot be deleted as it is located in system disk 
    FS_unmount(Dir_Label);
  }
  fclose(prt);
  return (FCT_OK);
}



int is_evol_pg(NO_SEGMENT noappli, void *p1, S_ETATOUT *param_out)
{
  S_ETATOUT etatout;
  int       retour;
  vim_mem_copy(&etatout, param_out, sizeof(etatout));
  etatout.returned_state[etatout.response_number].state.response=REP_OK;
  vim_mem_copy(param_out,&etatout,sizeof(etatout));
  retour = is_name (noappli, PT_NULL, param_out);
  return(FCT_OK);    
}

int is_time_function(NO_SEGMENT noappli, void *p1, S_ETATOUT *param_out)
{
  S_ETATOUT etatout;
  int       retour;
  vim_mem_copy(&etatout, param_out, sizeof(etatout));
  if (tms_in_dll_window() || saf_in_upload_window())
    etatout.returned_state[etatout.response_number].state.response=REP_OK;
  else
    etatout.returned_state[etatout.response_number].state.response=REP_KO;
  vim_mem_copy(param_out,&etatout,sizeof(etatout));
  retour = is_name (noappli, PT_NULL, param_out);
  return(FCT_OK);    
}


int is_change_init(NO_SEGMENT noappli, void *p1, S_ETATOUT *param_out)
{
  S_ETATOUT etatout;
  int       retour;
  vim_mem_copy(&etatout, param_out, sizeof(etatout));
  // accept all 
  etatout.returned_state[etatout.response_number].state.mask=0;
  vim_mem_copy(param_out,&etatout,sizeof(etatout));
  retour = is_name (noappli, PT_NULL, param_out);
  return(FCT_OK);    
}

int modif_param(NO_SEGMENT noappli, S_MODIF_P *param_in, void *p2)
{
  return(FCT_OK);
}

int is_card_specific(NO_SEGMENT noappli, S_TRANSIN *param_in, S_ETATOUT *param_out)
{
  S_ETATOUT etatout;
  int       retour;

  vim_mem_copy(&etatout, param_out, sizeof(etatout));
//	param_out->returned_state[param_out->response_number].state.response = (param_in->support == CHIP_SUPPORT) ? REP_OK : REP_KO;
	etatout.returned_state[param_out->response_number].state.response = REP_OK;
  vim_mem_copy(param_out,&etatout,sizeof(etatout));
  retour = is_name (noappli, PT_NULL, param_out);
 //VIM_DBG_MESSAGE("is_card_specific");
  return(FCT_OK);    
}

int card_inside(NO_SEGMENT noappli, S_TRANSIN *param_in, S_TRANSOUT *param_out )
{
  S_TRANSOUT etatout;
//			wErr = CallAppStart (bAppNo, UI_EVENT_CARD_INSERT, NULL, 0);

 //VIM_DBG_MESSAGE("card_inside entry");
  vim_mem_copy(&etatout, param_out, sizeof(etatout));
	etatout.noappli = noappli;
	etatout.rc_payment = PAY_OK;
  vim_mem_copy(param_out,&etatout,sizeof(etatout));
//  retour = is_name (noappli, PT_NULL, param_out);

 //VIM_DBG_MESSAGE("card_inside done");
  return(FCT_OK);    
}

//// Functions //////////////////////////////////////////////////
int idle_message (NO_SEGMENT no,void *p1,void *p2)
{
  FILE *hDisplay;
  int nFont;
  int ret;
  char filename[50];
  char display_type;
  StructScreen ScreenBitmap;
  // Open the display driver.
  hDisplay = fopen("DISPLAY","w");

  /* get reid of header */
  StateHeader(1);
  //EventHeader(_GPRS_STATE_);
  EventHeader(_NIV_RECEP_);

  //
  nFont = GetDefaultFont();
  
  // Display screen.
  CreateGraphics( _LARGE_ );
  /* select bitmap category 'S' for signon or 'R' for ready */
  display_type='R';
  if(terminal.logon_state!=logged_on)
  {
     display_type='S';
  }
  /* build bitmap filename */
  vim_snprintf(filename,sizeof(filename),VIM_FILE_PATH_GLOBAL "%c%04d.BMP",display_type,terminal.idle_screen);

  /* load bitmap from file */
  ret = BmpToPattern(filename,&ScreenBitmap);
  if (ret==0) 
  { 
       /* display bitmap*/
       _DisplayBitmap(0, 0, (unsigned char *)&ScreenBitmap, 8, _OFF_);
       PaintGraphics();
  }
  else
  {
      /* fallback to text only display */
      if(terminal.logon_state!=logged_on)
      {
         display_screen(terminal.IdleScreen, VIM_PCEFTPOS_KEY_CANCEL, "Sign On Required", "");
      }
      else
      {
         display_screen(terminal.IdleScreen, VIM_PCEFTPOS_KEY_CANCEL, "Ready", "");
      }
  }
  /* revert back to the default font */
  SetDefaultFont( nFont );

  /* Close the display driver.*/
  fclose( hDisplay );

  return FCT_OK;
}


typedef int (*T_SERVICE_FUNCTION)(unsigned int nSize, void*Data);

int give_interface(unsigned short no,void *p1,void *p2)
{
  service_desc_t MesServices[40];
  int i ; 
  i = 0 ;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = MESSAGE_RECEIVED;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_NAME;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = GIVE_YOUR_DOMAIN;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = MORE_FUNCTION;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IDLE_MESSAGE;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 254;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = KEYBOARD_EVENT;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = AFTER_RESET;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_STATE;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_DELETE;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = DEBIT_NON_EMV;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_FOR_YOU_AFTER;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = STATE;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = MCALL;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = CONSULT;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_EVOL_PG;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_TIME_FUNCTION;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = TIME_FUNCTION;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_CHANGE_INIT;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = MODIF_PARAM;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = FILE_RECEIVED;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
 	/* Events for insert detection */
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = IS_CARD_SPECIFIC;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;
  MesServices[i].appli_id  = no;
  MesServices[i].serv_id   = CARD_INSIDE;
  MesServices[i].sap       = (T_SERVICE_FUNCTION)Main;
  MesServices[i].priority  = 2;
  i++;

  ServiceRegister(i,MesServices);

  return FCT_OK;
}

void entry(void)
{ 
  object_info_t info;
    
  ObjectGetInfo(OBJECT_TYPE_APPLI, ApplicationGetCurrent(),&info);
  give_interface(info.application_type, NULL, NULL);
  give_interface_emv(info.application_type, NULL, NULL);
  // Open all the external libraries.

  paramlib_open();  // TELIUM Manager Library 
  extenslib_open(); // Extension Library 
  svlib_open();   // SV Library (for date management).
    
  libgrlib_open();  // Graphic Library  
  entrylib_open();  //

#ifndef VIM_DEBUG_ON 
    vim_dbg_info_set_mask(0);
#else
#warning debug has not been disabled 
#endif  
  /* DLLs opened by EMV-Custom (not sure if this is necessary, but open it unless we're short of memory) */
  iamlib_open();// create your events to be dispatched via Telium-Manager (for ex, detect incoming call, etc in idle)
}
#endif

