	#include <incs.h>

extern file_version_record_t file_status[];

extern VIM_UINT64 original_idle_time;

extern VIM_ERROR_PTR wow_load_files( void );

VIM_DBG_SET_FILE("TE");

VIM_BOOL FileValidate( VIM_CHAR *FileName )
{
	VIM_CHAR full_path[20];
	VIM_BOOL exists;
	vim_sprintf( full_path, "%s%s", VIM_FILE_PATH_LOCAL, FileName );
	vim_file_exists( full_path, &exists );

	// RDD - TODO: probably want to do FVV check here to later
	return exists ? VIM_TRUE : VIM_FALSE;
}

static VIM_ERROR_PTR update_files( file_download_t download_source )
{
	VIM_UINT8 file_id;
	//VIM_BOOL ReloadCtlsParams = VIM_FALSE;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;

	// RDD 170714 v430 SPIRA:7189 - don't allow file download unless PinPad is logged on
	if( terminal.logon_state != logged_on )
		return ERR_NOT_LOGGED_ON;
       
	/* RDD- run through the file list and download any outdated parameter files */     
	for( file_id=CPAT_FILE_IDX; file_id<=FCAT_FILE_IDX; file_id++ )
	{
        VIM_CHAR *Ptr = VIM_NULL;
        // RDD v723-1 JIRA PIRPIN-1003 Use Pre-loaded CPAT - noticed empty filename will return detected with size of 5560 bytes so perform some filename validation !!
        if(( vim_file_is_present( terminal.configuration.CPAT_Filename )) && ( file_id == CPAT_FILE_IDX ) && (( vim_strstr(terminal.configuration.CPAT_Filename, "CPAT", &Ptr )==VIM_ERROR_NONE )))
        {
            pceft_debug( pceftpos_handle.instance, "Using Pre-loaded CPAT so skip Switch CPAT");
            pceft_debug( pceftpos_handle.instance, terminal.configuration.CPAT_Filename );
            continue;
        }

        if ((vim_file_is_present(terminal.configuration.EPAT_Filename)) && (file_id == EPAT_FILE_IDX) && ((vim_strstr(terminal.configuration.EPAT_Filename, "EP", &Ptr) == VIM_ERROR_NONE)))
        {
            pceft_debug(pceftpos_handle.instance, "Using Pre-loaded EPAT so skip Switch EPAT");
            pceft_debug(pceftpos_handle.instance, terminal.configuration.EPAT_Filename);
            continue;
        }

        if ((vim_file_is_present(terminal.configuration.PKT_Filename)) && (file_id == PKT_FILE_IDX) && ((vim_strstr(terminal.configuration.PKT_Filename, "PKT", &Ptr) == VIM_ERROR_NONE)))
        {
            pceft_debug(pceftpos_handle.instance, "Using Pre-loaded PKT so skip Switch PKT");
            pceft_debug(pceftpos_handle.instance, terminal.configuration.PKT_Filename);
            continue;
        }

        
        // RDD 090512 - SPIRA ISSUE 5450. Embeded logons should not download files under any circumstances
		if( download_source == no_download )
		{
			if( FileValidate( file_status[file_id].FileName ))
			{
				// RDD 090512 - current file OK but out of date - continue to check next file
				continue;
			}
			else
			{
				// RDD 090512 - something wrong with the file or not present - exit and force non-embedded logon
				return ERR_NOT_LOGGED_ON;
			}
		}

		// RDD 090512 - SPIRA ISSUE 5450. Embeded logons should not download files under any circumstances
		// RDD 240212 - SPIRA ISSUE 165. Embedded logons should not download files unless the file is invalid
        if(( file_status[file_id].UpdateRequired ) || ( !FileValidate( file_status[file_id].FileName )) )
        {			  
			// RDD 280312 SPIRA 5209 Ensure that the file version is cleared in case the load does not succeed
			terminal.file_version[file_id] = 0;
	DBG_POINT;
			terminal_save();

			if( file_id == EPUMP_FILE_IDX ) continue;	// Don't need the EPUMP file

			if(( file_id == FCAT_FILE_IDX )&&( WOW_NZ_PINPAD )) continue;	// Don't need the FCAT file in New Zealand
			if(( ALH_PINPAD )&&( file_id==FCAT_FILE_IDX )) continue;		// RDD 161014 ALH SPIRA:8197 FCAT Not Required for ALH
			
			if(( res = file_download(file_id, download_source )) != VIM_ERROR_NONE )
			{
				//VIM_CHAR Buffer[40];
				VIM_DBG_ERROR(res);
				if( res != VIM_ERROR_TMS_CONFIG )
				{
					//vim_sprintf( Buffer, "%s Load Error", file_status[file_id].FileName );						
					//display_result( ERR_FILE_DOWNLOAD , "", Buffer, "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);	 
					//display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);	 
#if 0
					// RDD 171013 SPIRA:6846 Update any file causes contactless to be reinitialised				
					// RDD 171013 Ensure deletion of any partially downloaded file.
					vim_sprintf( Buffer, "%s%s", VIM_FILE_PATH_LOCAL, file_status[file_id].FileName );
					vim_file_delete( Buffer ); 
#endif
					// RDD 251013 SPIRA:6983 Ensure that logon is required after failed file download
					// RDD 060115 removed so that actual error is returned
					//res = ERR_FILE_DOWNLOAD;						
					terminal.logon_state = logon_required;
				}
				break; 
			}
			else
			{
				// RDD 171013 SPIRA:6846 Update any file causes contactless to be reinitialised							
				terminal_set_status(ss_wow_reload_ctls_params);
			}
			// RDD 171013 SPIRA:6846 Update any file causes contactless to be reinitialised				
#if 1			
			// RDD 060912 P3 SPIRA:6013 ensure that ctls reader updated if PKT file changed
			if(( file_id == EPAT_FILE_IDX )||( file_id == PKT_FILE_IDX ))
			{
				terminal_set_status(ss_wow_reload_ctls_params);
			}		
#endif
			file_status[file_id].UpdateRequired = VIM_FALSE;
			terminal_save();
			// RDD 170412 removed to save on file writes
        }
	}
#if 1	// RDD - because files can now be updated directly from TMS we need to make this more generic
	// RDD 171013 SPIRA:6846 - Files won't have been loaded due to FCAT missing on power up so load them now as all files 
	// should be present and correct 
	if( res == VIM_ERROR_NONE )
		  wow_load_files();

    // RDD 140519 - POS Cancel not working during CPAT download
    if (res == VIM_ERROR_POS_CANCEL)
    {
        comms_disconnect();
        VIM_ERROR_RETURN( res );
    }

	if( terminal_get_status(ss_wow_reload_ctls_params) )
	{				
		/* TODO: detected a known issue that: if CTLS init is done before EMV init, 
		Pinpad might crash randomly, JQ */
		InitEMVKernel();
		// RDD 121211 - enable the contactless reader after sucessfully downloading the EPAT file
		// RDD 210912 - Use the same procedure as during power up to initialise the contactless reader. ie. if a failure occurs, log it and retry
		// RDD 151014 - on power on we'll init ctls anyway after logon so don't need this step
#if 1 
		// RDD 020516 - noticed that EMV dock params didn't change when we updated EPAT via !2 - need to reload all files
		  
		if( wow_load_files() != VIM_ERROR_NONE )
		{
			pceft_debug( pceftpos_handle.instance, "Unable to load config files" );
			// If theres an error loading any of the WOW files reset the logon state to update the files unless already set to rsa logon	
			if( terminal.logon_state > file_update_required )
			{
				// RDD 190713 SPIRA:6777 revert to previous logon state if logon failed with comms error
				terminal.last_logon_state = terminal.logon_state;		
				terminal.logon_state = file_update_required;
			}
		}
		else
		{
			CtlsInit( );
		}
#else		
		CtlsInit( );
#endif

	}
#endif
    comms_disconnect();

	return res;
}



VIM_ERROR_PTR logon_send(VIM_AS2805_MESSAGE_TYPE nmi_type)
{
  VIM_AS2805_MESSAGE message;
  VIM_ERROR_PTR res = VIM_ERROR_NONE;

  init.stan = terminal.stan;
  //txn.type = tt_logon;		// RDD 080212 Bugfix as DE48 was not being loaded after a settlement

  init.error = msgAS2805_transaction( HOST_WOW, &message, nmi_type, &txn, VIM_FALSE, VIM_FALSE, VIM_TRUE );
  VIM_DBG_MESSAGE("------- Logon Result -------");
  VIM_DBG_ERROR(init.error);
  
  /* get turnaround */
  comms_get_last_turnaround_time(&init.turnaround_time);

  // DBG_VAR( txn.host_rc_asc );
  vim_mem_copy( init.logon_response, txn.host_rc_asc, 2 );
  init.logon_response[2] = '\0';
  
  //vim_strcpy( init.logon_response, "N2" );

  /* RDD - TEST TEST change back to "00" to perform file downloads...*/
  if (VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "00", 2 ))
  {
	  // DBG_MESSAGE( "Approved Response to Logon" );
  }
  else if( VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "N2", 2 ))
  {
	  // DBG_MESSAGE( "PPID error. RSA required" );
		  
	  // RDD: Can't resend a 191 - Host doesn't respond
	  res = ERR_WOW_ERC;
	  terminal.logon_state = rsa_logon;
  }
  else if( VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "N3", 2 ))
  {
	  DBG_MESSAGE( "File update required" );
	  terminal.logon_state = file_update_required;
      res = init.error;
  }
  else if( VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "96", 2 ))
  {
	  // DBG_MESSAGE( "Set RSA Logon" );
      //terminal.logon_state = rsa_logon;
      res = ERR_WOW_ERC;
  }
  else if( VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "12", 2 ))
  {
	  // DBG_MESSAGE( "Set RSA Logon" );
      terminal.logon_state = rsa_logon;
      res = ERR_WOW_ERC;
  }
  // RDD 020512 Need a special case for "08" response to logons because this will otherwise show APPROVED
  else if( VIM_ERROR_NONE == vim_mem_compare(init.logon_response, "08", 2 ))
  {
	  res = ERR_LOGON_FAILED_08;
  }
  else	// Some other Response code indicates failure - exit
  {
	  // RDD 020512 SPIRA:5423 Need this line to be uncommented in order to pass destructive host testing
	  if( init.error ==  VIM_ERROR_NONE )		
		  res = ERR_WOW_ERC;
	  else
		  res = init.error;
  }

  //pceft_journal_logon( &init );

//  terminal_save();		
  // RDD 100118 JIRA:WWBP7
#if 0
  return( res );
#else
  if( res == VIM_ERROR_NONE )
  {
	  res = init.error;
  }	
  return( res );
#endif
}
    


//#define _OFFLINESW
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR logon_request( VIM_BOOL DownloadFiles )
{
  VIM_ERROR_PTR res = VIM_ERROR_NONE;
  VIM_BOOL loop = VIM_TRUE;
  VIM_BOOL FilesFromSwitch = VIM_FALSE;

  if( !terminal_get_status( ss_tid_configured ))
  {    
	  return ERR_CONFIGURATION_REQUIRED;
  }

  // RDD 050813 added for v418- Offline test mode is kind on Training mode but with no logon needed
  if(( terminal_get_status(ss_offline_test_mode) ) && ( terminal_get_status(ss_wow_basic_test_mode) ))
  {
	vim_mem_set(terminal.configuration.merchant_name,0x20, WOW_NAME_LEN);
	vim_mem_set(terminal.configuration.merchant_address, 0x20, WOW_ADDRESS_LEN);
	vim_mem_copy(terminal.configuration.merchant_name,"Offline Demo Store", 18);
	vim_mem_copy(terminal.configuration.merchant_address,"Test Lab", 8);

	terminal.logon_state = logged_on;

	terminal_save();
	return ERR_LOGON_SUCCESSFUL;
  }

  // RDD - terminal seems to get stuck in libo if power fail during this state	
  if(terminal.logon_state == ktm_logon)	
  {	  
	  terminal.logon_state = rsa_logon;	
  }

  // RDD 170722 JIRA PIRPIN-1747
#if 0
  // RDD 291211 : If LFD failed during a config then it needs to be done during the next logon (perhaps).  	
  if( !terminal_get_status( ss_tms_configured ) )	
  {				
	  // RDD 210215 SPIRA:8417 Do not continue with Logon if RTM failed
#if 0
	  VIM_ERROR_IGNORE( tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE ) );  	
#else
	  init.error = tms_contact( VIM_TMS_DOWNLOAD_SOFTWARE, VIM_TRUE, web_then_vim );  	
	  VIM_ERROR_RETURN_ON_FAIL( init.error );
#endif
  }
#endif

  // RDD 300312 - the line below was causing a bug by preventing power fail reversals from happenning
  // ... not quite sure what it was doing there anyway... 
  //transaction_clear_status(&txn, st_incomplete); 

  // RDD 270714 SPIRA:7997 Clear Keymask before Logon	
  if( is_integrated_mode() )
	VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_keymask( pceftpos_handle.instance, 0 ));

  while( loop )
  {
	switch (terminal.logon_state)
    {
      case logged_on:
        loop = VIM_FALSE;
        // DBG_MESSAGE( "<<<<<<<< PINPAD IS LOGGED ON >>>>>>>>>>");
        res = ERR_LOGON_SUCCESSFUL;
        break;
        
        /* Session Key Logon - triggered by 76 response code to a txn */
      case session_key_logon:
		//DBG_MESSAGE( "<<<<<<<< SESSION KEY LOGON REQUIRED >>>>>>>>>>");
		
		// RDD 151112 added so QA can see when a logon goes up due to 76 response code
	    //if( terminal_get_status(ss_wow_basic_test_mode) )	
			//display_screen( DISP_INIT101_PLEASE_WAIT, VIM_PCEFTPOS_KEY_NONE );

        if(( res = logon_send( MESSAGE_WOW_800_NMIC_101 )) == VIM_ERROR_NONE )
		{
			DBG_POINT;
			terminal.logon_state = logged_on;
		}
		else
		{
			DBG_POINT;
			// RDD 060813 SPIRA:6707 MO System error was caused by mismatch due to missing de48 when 96 sent back
			// fix this by exiting with ERR_WOW_ERC
			// RDD 100616 SPIRA:8996 v560-2 BD says go to logon_required state for 101 now. Resore txn instead of doing this...
#if 0
			vim_mem_copy( init.logon_response, txn.host_rc_asc, 2 );	// RDD 060813 added line here.
			terminal.logon_state = rsa_logon;
#endif
		}
		loop = VIM_FALSE;

		// RDD 010513 SPIRA 6707 - needs to follow the rules - if session key failed then need RSA state
		// RDD 190712 - fix the state as logged on even if the key change failed
		//terminal.logon_state = logged_on;
        break;

         /* Normal Logon - always after 193 */
      case logon_required:
		display_screen( DISP_INIT_PLEASE_WAIT, VIM_PCEFTPOS_KEY_NONE );
		if( XXX_MODE )
			vim_event_sleep(1000);
        if(( res = logon_send( MESSAGE_WOW_800_NMIC_170 )) != VIM_ERROR_NONE )
		{
			// RDD 160513 - if the switch logon fails and files are missing then try TMS
			// RDD 020115 SPIRA:8277 - if 96 or 98 error to logon we don't want to attempt to download the files
#if 0
			if( !CheckFiles(VIM_FALSE) )
			{
				DBG_MESSAGE( "!CheckFiles goto file update" );
				terminal.logon_state = file_update_required;
				continue;
			}
			else
#endif
			{
				DBG_MESSAGE( "CheckFiles fail logon" );
				loop = VIM_FALSE;
				init.error = res;
				break;
			}
		}
		else if(( terminal.logon_state != file_update_required )||( !DownloadFiles ))
		{
			DBG_VAR(  terminal.logon_state );
			DBG_VAR( DownloadFiles );
			DBG_MESSAGE( "Force Logged On State !!" );
			terminal.logon_state = logged_on;
		}
		// RDD - if we're in Mode !2 then go to LFD to check for files after normal logon whether
		// we get an N3 or not....
#ifdef _DEBUG
		if( terminal_get_status( ss_lfd_only_config )  )
#else
		if(( terminal_get_status( ss_lfd_only_config )) && ( terminal.logon_state != logged_on ))
#endif
		{
			loop = VIM_FALSE;
			// RDD 250515 Need a disconnect here in case the TMS Host is on a different dial number
#if 0
			if( is_standalone_mode() )
				VIM_ERROR_RETURN_ON_FAIL( comms_disconnect() );
#endif
			if(( res = tms_contact( VIM_TMS_DOWNLOAD_PARAMETERS, VIM_FALSE, web_then_vim )) == VIM_ERROR_NONE )
			{	
				terminal.logon_state = logged_on;
			}
			else
			{ 
				init.error = res;
				VIM_DBG_ERROR( res );
			}
		}
        break;

       /* KEK logon - always after 192 */
      case kek_logon:
		display_screen(DISP_INITISALISE_193, VIM_PCEFTPOS_KEY_NONE);     
        loop = (( res = logon_send( MESSAGE_WOW_800_NMIC_193 )) == VIM_ERROR_NONE ) ? VIM_TRUE : VIM_FALSE;
		if( loop )
		{
			/* Set the login state so that the 0800 192 gets sent next */ 
			terminal.logon_state = logon_required;
			DBG_MESSAGE( "193 Suceeded. Goto 170" );
		}
		// RDD 270218 JIRA:WWBP-165
		else
		{
			terminal.logon_state = rsa_logon;
		}
        break;

        /* change terminal master key  - eg after MAC error (WBC) */
      case ktm_logon:      
		  terminal.logon_state = rsa_logon;
		  display_screen(DISP_INITISALISE_192, VIM_PCEFTPOS_KEY_NONE);
        loop = (( res = logon_send( MESSAGE_WOW_800_NMIC_192 )) == VIM_ERROR_NONE ) ? VIM_TRUE : VIM_FALSE;
		if( loop )
		{
			/* Set the login state so that the 0800 192 gets sent next */ 
			terminal.logon_state = kek_logon;
			DBG_MESSAGE( "192 Suceeded. Goto 193" );
		}
		// RDD 270218 JIRA:WWBP-165
		else
		{
			terminal.logon_state = rsa_logon;
		}
		break;

        /*  RSA logon  */
	  case rsa_logon:
		DBG_MESSAGE( "<<<<<<<< RSA LOGON REQUIRED >>>>>>>>>>");
	    display_screen(DISP_INITISALISE_191, VIM_PCEFTPOS_KEY_NONE);
        loop = (( res = logon_send( MESSAGE_WOW_800_NMIC_191 )) == VIM_ERROR_NONE ) ? VIM_TRUE : VIM_FALSE;
		if( loop )
		{
			/* Set the login state so that the 0800 192 gets sent next */ 
			terminal.logon_state = ktm_logon;
			DBG_MESSAGE( "191 Suceeded. Goto 192" );
		}
        break;

        /* File download - response to 170 indicates that parameter files are out of date */
      case file_update_required: 

		  pceft_debug_test( pceftpos_handle.instance, "----------File Update Required------------" );

		// RDD 031212 V305 SPIRA:6413 Send any pending reversal or 221 up BEFORE files are downloaded in order 
		// to minimise issues with these txns being discarded by host due to use of an old STAN.

		reversal_send( VIM_FALSE );
			 
		// RDD 230916 SPIRA:9064 upload a pending 221 before running the file download - this should be handled by the line above but apparently isn't
		VIM_ERROR_RETURN_ON_FAIL( saf_send( VIM_TRUE, TRICKLE_FEED_COUNT, VIM_FALSE, VIM_FALSE ));

		// If we we need to download files and we haven't got any from LFD then check to see if we need to go to LFD first.
		terminal.logon_state = logged_on;
		if( !FilesFromSwitch )
		{
			if( terminal_get_status( ss_lfd_then_switch_config ))
			{
				DBG_MESSAGE( "Try LFD get files first" );
				res = update_files( lfd_download );
				terminal.logon_state = logon_required;
				FilesFromSwitch = VIM_TRUE;							//BD Condition below wasn't happening so set by default
			}
			else if( terminal_get_status( ss_lfd_only_config ))
			{
				DBG_MESSAGE( "Try LFD get files only" );
				res = update_files( lfd_download );
				res = update_files( no_download );					//BD Don't continue without valid files
				if (res != VIM_ERROR_NONE)
				{
					terminal.logon_state = logon_required;
					loop = VIM_FALSE;
				}
			}
			else
			{
				DBG_MESSAGE( "Try switch get files default" );
				// RDD 251013 SPIRA:6983 - exit loop if file download error
				if(( res = update_files( switch_download )) != VIM_ERROR_NONE )
					loop = VIM_FALSE;
				//BD reset N3 to 00 ???
			}
		}
		// If we already got config files from LFD then we're here because we logged on again and the files didn't match
		else 
		{
			if( terminal_get_status( ss_lfd_then_switch_config ))
			{
				DBG_MESSAGE( "Try Switch get files second" );
				res = update_files( switch_download );		
				//BD reset N3 to 00 ???
			}
		}
#if 0 //BD
		// RDD 230413 added for P4 Config via LFD - if files successfully uploaded via LFD need to logon again to check ver
		if( res == VIM_ERROR_TMS_CONFIG )  //BD This isn't set !!!!!!!!!
		{
			res = VIM_ERROR_NONE;
			// Set this flag so that we know to get the files from the switch if the next logon fails ND
			FilesFromSwitch = VIM_TRUE;
		}
#endif
		break;

      default:
		loop = VIM_FALSE;
		pceft_debug_error(pceftpos_handle.instance, "Software Error: (Unknown Logon state) LOGON.C L261 !!!!", VIM_ERROR_SYSTEM );    
		res = VIM_ERROR_SYSTEM;
        break;
    }
	// RDD SPIRA:6606 added a save here
	// actually this can cause the pinpad to crash!
//	DBG_POINT;
//	terminal_save();	// This save is required!! moved outside loop

  }
	DBG_POINT;
	terminal_save();	// This save is required!!

    // 

  // Map any VIM_ERROR_NONE to ERR_LOGON_SUCESSFUL in order to get the correct approval msg
  init.error = res;
  // RDD 251121 v729-1 JIRA PIRPIN-1321 Need to drop connection after LOGON.
  comms_disconnect();
  return res;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR logon_finalisation( VIM_BOOL no_receipt )
{
  VIM_PCEFTPOS_RECEIPT receipt;
  error_t error_data;    
  VIM_RTC_UPTIME now, min_display_time = 0;

  vim_mem_clear(&receipt, VIM_SIZEOF(receipt));
  // RDD 161214 SPIRA:8277 Added
  error_code_lookup( init.error, init.logon_response, &error_data);

  // RDD 280814 - noticed that power up logon receipts had completion header after completion was last txn.
   txn_init();
  /* display response text on the screen */
  if( init.error != ERR_FILE_DOWNLOAD )
  {
	display_result(init.error, init.logon_response, "", "", VIM_PCEFTPOS_KEY_NONE, 0 );  			  
	/* read current time and set timer */
	vim_rtc_get_uptime(&now);
	min_display_time = now + TWO_SECOND_DELAY;
  }

  /* print logon receipt */
  if(( !no_receipt ) && ( init.error != VIM_ERROR_POS_CANCEL ))
  {
    receipt_build_logon( init.error, &receipt, error_data.pos_text, VIM_FALSE );
    pceft_pos_print( &receipt, VIM_TRUE );
  }

  /* read current time - send a POS key mask to "OK" and wait until 2 seconds has passed since the original message was sent */
  vim_rtc_get_uptime(&now);        
  VIM_DBG_NUMBER(now);
  if( init.error != ERR_FILE_DOWNLOAD )
  {
	if (now < min_display_time)
	{
	  VIM_PCEFTPOS_KEYMASK key_code;
	  VIM_DBG_NUMBER(min_display_time - now);
	          
	  if( is_integrated_mode() )
	  {
		  vim_pceftpos_set_keymask( pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK );	  
		  pceft_pos_wait_key( &key_code, (min_display_time - now) );
	  }
	}
  }
    
  set_operation_initiator(it_pinpad);
    
  // RDD JIRA WWBP-292 this happens in D state to so move to a more generic point
//  main_display_idle_screen("", "");

  return VIM_ERROR_NONE;
}
