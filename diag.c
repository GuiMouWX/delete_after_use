// RDD 260514 SPIRA:7566 - add new file diags.c to deal with testimal test functions 

#include <incs.h>

#define TEST_PASSED 'P'
#define TEST_FAILED 'F'


const VIM_TEXT DIAG_FILE[] = VIM_FILE_PATH_LOCAL "DIAG.DAT";


extern VIM_ERROR_PTR config_card_read_test( VIM_CHAR *Results );
extern VIM_ERROR_PTR EmvCardRemoved(VIM_BOOL GenerateError);
extern PREAUTH_SAF_TABLE_T preauth_saf_table;

// Define a map for the diag tests
static VIM_ERROR_PTR full_diagnostic( void *ResultsPtr );
static VIM_ERROR_PTR display_versions( void *ResultsPtr );
static VIM_ERROR_PTR mag_test( void *ResultsPtr );
static VIM_ERROR_PTR dock_test( void *ResultsPtr );
static VIM_ERROR_PTR ctls_test( void *ResultsPtr );
static VIM_ERROR_PTR touch_test(  void *ResultsPtr );
static VIM_ERROR_PTR pceft_client_test( void *ResultsPtr );
static VIM_ERROR_PTR lfd_comms_test( void *ResultsPtr );
static VIM_ERROR_PTR host_comms_test( void *ResultsPtr );


	static CONFIG_KEY_MAP const RemoteMap[]=  
	{    
		{VIM_KBD_CODE_KEY2,display_versions},    
		{VIM_KBD_CODE_KEY5,ctls_test},    
		{VIM_KBD_CODE_KEY7,pceft_client_test},    
		//{VIM_KBD_CODE_KEY8,lfd_comms_test},    
		{VIM_KBD_CODE_KEY9,host_comms_test},    
		{VIM_KBD_CODE_NONE,VIM_NULL}
	};

	static CONFIG_KEY_MAP const map[]=  
	{    
		{VIM_KBD_CODE_KEY1,full_diagnostic},    
		{VIM_KBD_CODE_KEY2,display_versions},    
		{VIM_KBD_CODE_KEY3,mag_test},    
		{VIM_KBD_CODE_KEY4,dock_test},    
		{VIM_KBD_CODE_KEY5,ctls_test},    
		{VIM_KBD_CODE_KEY6,touch_test},    
		{VIM_KBD_CODE_KEY7,pceft_client_test},    
		{VIM_KBD_CODE_KEY8,lfd_comms_test},    
		{VIM_KBD_CODE_KEY9,host_comms_test},    
		{VIM_KBD_CODE_NONE,VIM_NULL}
	};


// Define a Structure to hold Diag test results for a single diag run

typedef struct
{
    VIM_CHAR TID[20];
    VIM_CHAR MID[20];
    VIM_CHAR DateTime[20];   
    VIM_CHAR OS_VER[20];
    VIM_CHAR EOS_VER[20];
    VIM_CHAR CTLS_VER[20];
    VIM_CHAR APP_VER[20];
    VIM_CHAR TestResults[20][10];
} s_diag_results_t;    


typedef union
{   
	s_diag_results_t fields;  
	VIM_UINT8 bytebuff[ VIM_SIZEOF( s_diag_results_t ) ];
}  u_diag_results_t;



// define a static data union to hold the latest diagnotic test run results
//static u_diag_results_t u_diag_results;


void init_test_results( u_diag_results_t *ResultsPtr )
{
	vim_mem_set( ResultsPtr->bytebuff, VIM_SIZEOF( ResultsPtr ), ' ' );
}


VIM_ERROR_PTR diag_store_results( uDiagResults *ResultsPtr, VIM_SIZE SelectedIndex, VIM_ERROR_PTR res )
{
	// If individual test then add detail here	
	ResultsPtr->Results.TestResults[SelectedIndex] = ( res == VIM_ERROR_NONE ) ? TEST_PASSED : TEST_FAILED;

	// Append the Return Error Code text to whatever else we stored for this record							
	if( res != VIM_ERROR_NONE )
	{
		vim_strcat( ResultsPtr->Results.AdditionalInfoA, "\n" );				
		vim_strcat( ResultsPtr->Results.AdditionalInfoA, res->name );						
		vim_strcat( ResultsPtr->Results.AdditionalInfoA, "\n" );		
	}
	return VIM_ERROR_NONE;
}




static VIM_ERROR_PTR full_diagnostic( void *ResultsPtr )
{
	VIM_UINT8 n;
	uDiagResults *LocalResultsPtr = (uDiagResults *)ResultsPtr;
	VIM_UINT8 max_tests = (LocalResultsPtr->Results.TestSource == dt_Remote) ? VIM_LENGTH_OF(RemoteMap)+1 : VIM_LENGTH_OF(map);

	// RDD 080814 if remotly triggered then only run none-interactive tests 
	CONFIG_KEY_MAP *ItemPtr = (LocalResultsPtr->Results.TestSource == dt_Remote) ? (CONFIG_KEY_MAP *)&RemoteMap[0] : (CONFIG_KEY_MAP *)&map[1];

	for( n=2; n<max_tests; n++, ItemPtr++ )
	{
		VIM_ERROR_PTR res = ItemPtr->method( ResultsPtr );
		diag_store_results( LocalResultsPtr, n-1, res );
	}

	vim_strcat( LocalResultsPtr->Results.TestResults, "\n" );
	terminal_set_status( ss_lfd_remote_command_executed );

	return VIM_ERROR_NONE;
}


static VIM_ERROR_PTR display_versions( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR eos_buf[100];	  
	VIM_TEXT OS_name[50];  
	VIM_TEXT OS_Version[50];
	VIM_CHAR *Ptr = &OS_name[1];
	VIM_CHAR ver_buf[100];
	VIM_SIZE ver_size = 0;
    VIM_KBD_CODE key;

  
	sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;
	vim_mem_clear( OS_name, VIM_SIZEOF(OS_name));  
	vim_mem_clear( OS_Version, VIM_SIZEOF(OS_Version));
	vim_strcpy( OS_name, "OS Ver: ERROR" );
	vim_strcpy( eos_buf, "EOS Ver: ERROR" );

	vim_strcpy( ver_buf, "CTLS:   (Please Wait)" );

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Display\nVersions", "" ); 		
	vim_event_sleep( 2000 );

	/* get OS info */  
	VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_OS_info (OS_name, VIM_SIZEOF(OS_name)-1, OS_Version, VIM_SIZEOF(OS_Version)-1, VIM_NULL, 0));   
  
	VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_EOS_info ( eos_buf, VIM_SIZEOF(eos_buf)-1 ));

	VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_VERSIONS, VIM_PCEFTPOS_KEY_NONE, Ptr, eos_buf, &ver_buf[8], SOFTWARE_VERSION ));  

	if( !terminal_get_status(ss_ctls_reader_open) )	
	{
		if(( res = picc_open( PICC_OPEN_ONLY, VIM_FALSE )) != VIM_ERROR_NONE )
		{
			vim_strcpy( ver_buf, "CTLS : OPEN FAIL" );
		}
		else
		{
			res = vim_picc_emv_get_firmware_version (picc_handle.instance, ver_buf, VIM_SIZEOF(ver_buf), &ver_size);	  
		}
	}
	else
		VIM_ERROR_IGNORE( vim_picc_emv_get_firmware_version (picc_handle.instance, ver_buf, VIM_SIZEOF(ver_buf), &ver_size));	  
    if( !ver_size )
		vim_strcpy( ver_buf, "CTLS Ver: (NO CTLS)" );

	// Put a copy of the display into the results buffer
	vim_snprintf( LocalResultsPtr->AdditionalInfoA, VIM_SIZEOF(LocalResultsPtr->AdditionalInfoA), "%s\n%s\n%s\n%d.%d\n", Ptr, eos_buf, &ver_buf[8], SOFTWARE_VERSION, REVISION );
	// RDD - debug the version info out to the POS
	pceft_debug( pceftpos_handle.instance, LocalResultsPtr->AdditionalInfoA );
	VIM_ERROR_IGNORE( display_screen( DISP_VERSIONS, VIM_PCEFTPOS_KEY_NONE, Ptr, eos_buf, &ver_buf[8], SOFTWARE_VERSION ));  

    vim_kbd_read(&key, TIMEOUT_INACTIVITY);
	return res;
}

static VIM_ERROR_PTR mag_test( void *ResultsPtr )
{
	sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;
	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "MagStripe Reader\nTest", "" ); 		
	vim_event_sleep( 2000 );
	return ( config_card_read_test( LocalResultsPtr->AdditionalInfoA ));
}

static VIM_ERROR_PTR dock_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;

	card_info_t card_data;
	txn_init();
	
	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Chip Reader\nTest", "" ); 		
	vim_event_sleep( 2000 );

	txn.card_state = card_capture_icc_test;	
	res = card_capture( "Test Card Slot", &card_data, card_capture_icc_test, txn.read_icc_count );
	DBG_POINT;
	display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);
	
	if( res == VIM_ERROR_NONE )
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", "" ); 		
	}
	else	
	{		
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", res->name ); 		
	}		
	vim_event_sleep( 2000 );

	VIM_DBG_ERROR( res );

	if( EmvCardRemoved(VIM_FALSE) == VIM_ERROR_NONE ) emv_remove_card( "" );

	return res;
}

static VIM_ERROR_PTR ctls_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;

	card_info_t card_data;
	txn_init();
	txn.type = tt_sale;

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Contactless\nReader Test", "" ); 		
	vim_event_sleep( 2000 );


	if(( res = picc_open( PICC_OPEN_ONLY, VIM_TRUE )) == VIM_ERROR_NONE )
	{			
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Reader Connected", "OK" ); 				
		vim_event_sleep( 1000 );

		if( LocalResultsPtr->TestSource == dt_Remote )
			return VIM_ERROR_NONE;

		//res = CtlsInit( );
#if 1
		if(( res = CtlsReaderEnable( VIM_FALSE )) != VIM_ERROR_NONE )
		{
			display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Init Failed", "Reader Fatal Error" ); 				
			return VIM_ERROR_NONE;
		}
#endif
		res = card_capture( "TEST CONTACTLESS", &card_data, card_capture_picc_test, txn.read_icc_count );
	}
	if( res == VIM_ERROR_NONE )
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", "" ); 		
	}
	else	
	{		
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", res->name ); 		
	}		
	vim_event_sleep( 1000 );
	return res;
}

static VIM_ERROR_PTR touch_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_KBD_CODE key_pressed;
	sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;
	VIM_CHAR Instructions[50];
	VIM_UINT8 Step = 1;
	VIM_BOOL Passed = VIM_FALSE;

	screen_kbd_touch_read_with_abort( &key_pressed, 0, VIM_FALSE, VIM_TRUE );	
	txn.account = account_unknown_any;

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Touch Screen Test", "Follow Instructions" ); 		
	vim_event_sleep( 2000 );

	while( Step < 4 )
	{
		Passed = VIM_FALSE;
		if( Step == 1 )
			vim_snprintf( Instructions, VIM_SIZEOF(Instructions), "%s", "Select Cheque Account" );

		if(( res = display_screen( DISP_SELECT_ACCOUNT, VIM_PCEFTPOS_KEY_CANCEL, Instructions, 0, "", "" ))!= VIM_ERROR_NONE ) 	
			display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);	 
		  
		/* wait for key (or abort)*/
		if(( res = screen_kbd_touch_read_with_abort( &key_pressed, SELECT_TIMEOUT, VIM_TRUE, VIM_TRUE )) != VIM_ERROR_NONE )
			display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY);	 	  
		
		switch (key_pressed)	
		{
			case VIM_KBD_CODE_SOFT1:
			if( Step == 1 ) 
			{
				vim_snprintf( Instructions, VIM_SIZEOF(Instructions), "%s", "Select SAV Account" );
				Passed = VIM_TRUE;
			}
			break;

			case VIM_KBD_CODE_SOFT2:					  
			if( Step == 2 ) 
			{
				vim_snprintf( Instructions, VIM_SIZEOF(Instructions), "%s", "Select CR Account" );
				Passed = VIM_TRUE;
			}
			break;

			case VIM_KBD_CODE_SOFT3:					  
			if( Step == 3 ) 
			{
				Passed = VIM_TRUE;
			}
			break;

			// RDD - SoftKey not pressed so assume Touch not working
			default:
				vim_snprintf( LocalResultsPtr->AdditionalInfoA, VIM_SIZEOF(LocalResultsPtr->AdditionalInfoA), "Invalid Key Selected: %d", key_pressed );
				display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", "Invalid Action" ); 		
				vim_event_sleep( 1000 );
				return VIM_ERROR_NOT_FOUND;
		}
		if( !Passed ) 
			break;
		else
			Step+=1;
	}	

	if( Passed )	
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", "" ); 		
		Step++;
	}
	else
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", "" ); 		
		Step = 1;
	}
	vim_event_sleep( 1000 );
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR pceft_client_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_CHAR ClientData[100];
	//sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;

	VIM_PCEFTPOS_CONFIG_RESPONSE config_response;

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "PcEft Client\nTest", "Connecting.." ); 		
	vim_event_sleep( 2000 );

	if(( res = vim_pceftpos_eft_config( pceftpos_handle.instance, &config_response )) != VIM_ERROR_NONE )
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", "" ); 		
		vim_event_sleep( 2000 );
		display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	 
	}
	else
	{
		vim_snprintf( ClientData, VIM_SIZEOF(ClientData), "TID:%8.8s\nMID:%15.15s", config_response.terminal_id, config_response.merchant_id );
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", ClientData ); 		
	}
	vim_event_sleep( 2000 );
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR lfd_comms_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "RTM Comms\nTest", "Connecting.." ); 		
	vim_event_sleep( 2000 );

	if(( res = tms_contact( VIM_TMS_LOGON_ONLY, VIM_FALSE, web_then_vim )) != VIM_ERROR_NONE )	
	{	
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", "" ); 		
		vim_event_sleep( 2000 );
		display_result( res , "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	 
	}			
	else	
	{ 
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", "" ); 		
	}
	vim_event_sleep( 2000 );
	return res;
}

static VIM_ERROR_PTR host_comms_test( void *ResultsPtr )
{
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
//	sDiagResults *LocalResultsPtr = (sDiagResults *)ResultsPtr;

	if( terminal.logon_state == logged_on )
		terminal.logon_state = logon_required;

	display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Host Comms\nTest", "Connecting.." ); 		
	vim_event_sleep( 2000 );

	if(( res = logon_request( VIM_TRUE )) != ERR_LOGON_SUCCESSFUL )
	{
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Failed", "" ); 		
		vim_event_sleep( 2000 );
		display_result( res, "", "", "", VIM_PCEFTPOS_KEY_OK, TWO_SECOND_DELAY );	 
	}			
	else	
	{ 
		display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Test Passed", "" ); 		
		res = VIM_ERROR_NONE;
	}
	vim_event_sleep( 2000 );
	return res;
}

#if 0
VIM_ERROR_PTR diag_run_tests(void)
{
	static CONFIG_KEY_MAP const map[]=  
	{    
		{VIM_KBD_CODE_KEY1,full_diagnostic},    
		{VIM_KBD_CODE_KEY2,display_versions},    
		{VIM_KBD_CODE_KEY3,mag_test},    
		{VIM_KBD_CODE_KEY4,dock_test},    
		{VIM_KBD_CODE_KEY5,ctls_test},    
		{VIM_KBD_CODE_KEY6,touch_test},    
		{VIM_KBD_CODE_KEY7,pceft_client_test},    
		{VIM_KBD_CODE_KEY8,lfd_comms_test},    
		{VIM_KBD_CODE_KEY9,host_comms_test},    
		{VIM_KBD_CODE_NONE,VIM_NULL}
	};

	// Initialise the data structure
	init_test_results( &u_diag_results );

	VIM_ERROR_RETURN_ON_FAIL( config_key_map_run( diag_run_tests, map ) );
	/* return without error */
	return VIM_ERROR_NONE;
}
#else

VIM_ERROR_PTR InitResults( sDiagResults *ResultsPtr )
{
	vim_mem_clear( ResultsPtr, VIM_SIZEOF( sDiagResults ));

	VIM_ERROR_RETURN_ON_FAIL( vim_rtc_get_time( &ResultsPtr->TestTimeStamp ));
	vim_mem_set( ResultsPtr->TestResults, '-', NUMBER_OF_TESTS );
	vim_strcpy( ResultsPtr->AdditionalInfoA, "\nNo Information" );
	ResultsPtr->TestSource = dt_Unknown;
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR diag_display_record( VIM_CHAR *FormatText )
{
	VIM_KBD_CODE key_1;
	VIM_MILLISECONDS timeout = 20000;
	display_screen( DISP_DIAG_RECORD, VIM_PCEFTPOS_KEY_NONE, FormatText );
	vim_event_sleep( 2000 );
	screen_kbd_touch_read_with_abort( &key_1, timeout, VIM_FALSE, VIM_FALSE ); 
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR diag_format_record( uDiagResults *ResultsPtr, VIM_CHAR *DisplayBuffer )
{

	vim_snprintf( DisplayBuffer, 40, "%2.2d/%2.2d/%2.2d %2.2d:%2.2d (%c)\n\n", ResultsPtr->Results.TestTimeStamp.day,
																					ResultsPtr->Results.TestTimeStamp.month, 
																					ResultsPtr->Results.TestTimeStamp.year%2000, 
																					ResultsPtr->Results.TestTimeStamp.hour, 
																					ResultsPtr->Results.TestTimeStamp.minute,
																					ResultsPtr->Results.TestSource ); 
	vim_strcat( DisplayBuffer, ResultsPtr->Results.TestResults );
	vim_strcat( DisplayBuffer, "\n" );

	vim_strcat( DisplayBuffer, ResultsPtr->Results.AdditionalInfoA );
	return VIM_ERROR_NONE;
}

/*

typedef struct PREAUTH_SAF_DETAILS
{
  VIM_UINT32 roc;
  VIM_UINT32 roc_index;
  VIM_UINT32 status;
  VIM_UINT16 index;
  VIM_CHAR auth_no[7];
  VIM_RTC_TIME time;
}PREAUTH_SAF_DETAILS;

#define PREAUTH_SAF_RECORD_COUNT (1000)

typedef struct PREAUTH_SAF_TABLE_T
{
  VIM_UINT16 records_count;
  PREAUTH_SAF_DETAILS details[PREAUTH_SAF_RECORD_COUNT];
} PREAUTH_SAF_TABLE_T;

typedef union U_PREAUTH_SAF_TABLE_T
{
	PREAUTH_SAF_TABLE_T Table;
	VIM_UINT8 ByteBuff;
} U_PREAUTH_SAF_TABLE_T;

*/

VIM_ERROR_PTR preauth_select_record( VIM_UINT32 *rrn )
{	
	VIM_UINT16 RecordIndex; 
	VIM_SIZE AllRecords=0, ValidRecords=0;			
	//VIM_SIZE PendingTotal;
	VIM_CHAR Items[255][20];
	VIM_CHAR *ItemPtrs[255];
	//VIM_CHAR DisplayBuffer[200];
	VIM_UINT16 Indeces[PREAUTH_SAF_RECORD_COUNT];
	static VIM_SIZE ItemIndex = 0;
	VIM_SIZE SelectedIndex = 0xFFFF;
	VIM_TEXT *status_ptr = "Preauth Records\nDate/Time    RRN";
	PREAUTH_SAF_DETAILS *sTablePtr = &preauth_saf_table.details[0];
		
	preauth_load_table(  );

	AllRecords = preauth_saf_table.records_count;

	if( AllRecords )
	{
		// Calculate the number of unexpired/uncompleted records
		for( RecordIndex=0; RecordIndex < AllRecords; RecordIndex++, sTablePtr++  )	
		{		
			if( preauth_expired( &sTablePtr->time )) continue;
			if( sTablePtr->status & st_checked_out ) continue;
			vim_snprintf( Items[ValidRecords], VIM_SIZEOF(Items[ValidRecords]), "%2.2d/%2.2d %2.2d:%2.2d %6.6d", sTablePtr->time.day,
																			sTablePtr->time.month, 																					
																			sTablePtr->time.hour, 
																			sTablePtr->time.minute,
																			sTablePtr->roc ); 
			ItemPtrs[ValidRecords] = Items[ValidRecords];
			// Store the actual array index for the valid preauth
			Indeces[ValidRecords] = RecordIndex;
			ValidRecords+=1;
		}

		if( ValidRecords )
		{
#ifndef _WIN32
            VIM_ERROR_RETURN_ON_FAIL(display_menu_select_zero_based(status_ptr, ItemIndex, &SelectedIndex, (VIM_TEXT const **)ItemPtrs, ValidRecords, ENTRY_TIMEOUT, VIM_TRUE, VIM_FALSE, VIM_TRUE, VIM_FALSE));
			SelectedIndex -= 1;
#else
            VIM_ERROR_RETURN_ON_FAIL(display_menu_select(status_ptr, &ItemIndex, &SelectedIndex, (VIM_TEXT const **)ItemPtrs, ValidRecords, 10000, VIM_TRUE, VIM_FALSE, VIM_TRUE, VIM_FALSE));
#endif
			*rrn = (VIM_SIZE)preauth_saf_table.details[Indeces[SelectedIndex]].roc;
			return VIM_ERROR_NONE;
		}
	}

	display_screen( DISP_DIAG_RECORD, VIM_PCEFTPOS_KEY_NONE, "No Preauth\nRecords Found" );
	vim_event_sleep(2000);
	return VIM_ERROR_NOT_FOUND;	
}



VIM_ERROR_PTR diag_select_record( void )
{
	VIM_BOOL Exists = VIM_FALSE;
	VIM_SIZE FileSize = 0, Records = 0;
	VIM_TEXT *status_ptr = "Diagnostic Records";
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;
	static VIM_SIZE ItemIndex = 0;
	VIM_SIZE SelectedIndex = 0xFFFF;
	uDiagResults TestResults;
	uDiagResults *ResultsPtr = &TestResults;
	VIM_CHAR Items[255][20];
	VIM_CHAR *ItemPtrs[255];
	VIM_CHAR DisplayBuffer[200];

	vim_mem_clear( Items, VIM_SIZEOF(Items) );

	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( DIAG_FILE, &Exists )); 
	
	if( !Exists )
	{
		diag_display_record( "No Diagnostic\nRecords Found" );		
	}
	else
	{
		while( 1 )
		{
			VIM_ERROR_RETURN_ON_FAIL( vim_file_get_size( DIAG_FILE, VIM_NULL, &FileSize ));
			if( FileSize )
			{
				VIM_SIZE RecordIndex, RecordLength = VIM_SIZEOF( sDiagResults );
				Records = FileSize/VIM_SIZEOF( sDiagResults );

				for( RecordIndex=0; RecordIndex < Records; RecordIndex++ )
				{	
					VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( DIAG_FILE, ResultsPtr->ByteBuf, RecordLength, RecordLength*RecordIndex ));
					vim_snprintf( Items[RecordIndex], VIM_SIZEOF(Items[RecordIndex]), "%2.2d/%2.2d/%2.2d %2.2d:%2.2d", ResultsPtr->Results.TestTimeStamp.day,
																					ResultsPtr->Results.TestTimeStamp.month, 
																					ResultsPtr->Results.TestTimeStamp.year%2000, 
																					ResultsPtr->Results.TestTimeStamp.hour, 
																					ResultsPtr->Results.TestTimeStamp.minute ); 
					ItemPtrs[RecordIndex] = Items[RecordIndex];
				}
#ifdef _VOS2
                VIM_ERROR_RETURN_ON_FAIL(display_menu_select_zero_based(status_ptr, ItemIndex, &SelectedIndex, (VIM_TEXT const **)ItemPtrs, Records, ENTRY_TIMEOUT, VIM_TRUE, VIM_FALSE, VIM_TRUE, VIM_FALSE));
#else
                //VIM_ERROR_RETURN_ON_FAIL(display_menu_select(status_ptr, &ItemIndex, &SelectedIndex, (VIM_TEXT const **)ItemPtrs, Records, 10000, ));
#endif			
				// Read the selected record
				VIM_ERROR_RETURN_ON_FAIL( vim_file_get_bytes( DIAG_FILE, ResultsPtr->ByteBuf, RecordLength, RecordLength*SelectedIndex ));

				// Format the selected record for Display
				VIM_ERROR_RETURN_ON_FAIL( diag_format_record( ResultsPtr, DisplayBuffer ));

				diag_display_record( DisplayBuffer );		
			}
			else
			{
				diag_display_record( "No Diagnostic\nRecords Found" );		
				break;
			}
		}
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR diag_append_record( uDiagResults *ResultsPtr )
{
	VIM_BOOL Exists = VIM_FALSE;
	VIM_SIZE FileSize = 0;// Records = 0;

	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( DIAG_FILE, &Exists )); 
	
	if( !Exists )
	{
		VIM_ERROR_RETURN_ON_FAIL( vim_file_set( DIAG_FILE, (const void *)ResultsPtr->ByteBuf, VIM_SIZEOF( ResultsPtr->ByteBuf )));  
	}
	else
	{
		VIM_ERROR_RETURN_ON_FAIL( vim_file_get_size( DIAG_FILE, VIM_NULL, &FileSize ));
		if( FileSize )
		{
			//Records = FileSize/VIM_SIZEOF( sDiagResults );
			VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( DIAG_FILE, ResultsPtr->ByteBuf, VIM_SIZEOF( sDiagResults ), FileSize ));
		}
	}
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR diag_run_tests( VIM_UINT32 Password, DiagTest_t Source )
{
	VIM_TEXT *status_ptr = "Diagnostic Menu";
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	static VIM_SIZE ItemIndex = 0;
	VIM_SIZE SelectedIndex = 0xFFFF;
	uDiagResults TestResults;
	uDiagResults *ResultsPtr = &TestResults;

	const VIM_CHAR *items[] = { "Run All Tests", "Versions", "Test Mag", "Test Dock", "Test Ctls", "Test Touch", "Test PcEft", "Test RTM", "Test Host" };

	// Initialise the struct with defaults incl. getting current date and time
	InitResults( &ResultsPtr->Results );

	// Log the source for this diag request
	ResultsPtr->Results.TestSource = Source;

	// Run an already designated test or series of test
	if( Password > DIAG_RUN_MENU )
	{
		SelectedIndex = Password%DIAG_RUN_MENU -1; 
		diag_store_results( ResultsPtr, SelectedIndex, map[SelectedIndex].method( (void *)ResultsPtr ) );
		VIM_ERROR_RETURN_ON_FAIL( diag_append_record( ResultsPtr ));

	}
	else
	{
		// Run the menu to let the user decide
		do
		{

			//VIM_KBD_CODE key_pressed;
			//SelectedIndex = 0xFFFF;
			DBG_POINT;
#ifdef _VOS2
            if ((res = display_menu_select_zero_based(status_ptr, ItemIndex, &SelectedIndex, (VIM_TEXT const **)items, VIM_LENGTH_OF(items), ENTRY_TIMEOUT, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE )) != VIM_ERROR_USER_CANCEL )
#else
            if ((res = display_menu_select( status_ptr, &ItemIndex, &SelectedIndex, (VIM_TEXT **)items, VIM_LENGTH_OF(items), 10000, VIM_TRUE, VIM_TRUE, VIM_TRUE, VIM_FALSE)) != VIM_ERROR_USER_CANCEL )
#endif	
            {
				if( res == VIM_ERROR_EMV_CARD_REMOVED )
				{
					txn_init();
					DBG_POINT;
					continue;
				}
				else if( res == VIM_ERROR_TIMEOUT )
				{
					return VIM_ERROR_NONE;
				}

				DBG_VAR( SelectedIndex );
				if( SelectedIndex )
					diag_store_results( ResultsPtr, SelectedIndex, map[SelectedIndex].method( (void *)ResultsPtr ));
				else
					map[SelectedIndex].method( (void *)ResultsPtr );
				  
				//VIM_ERROR_RETURN_ON_FAIL( diag_append_record( ResultsPtr ));
				//pceft_debug( pceftpos_handle.instance, ResultsPtr->Results.TestSource );
				pceft_debug( pceftpos_handle.instance, ResultsPtr->Results.TestResults );
				//pceft_debug( pceftpos_handle.instance, ResultsPtr->Results.AdditionalInfoA );
			}
		}
		while( res != VIM_ERROR_USER_CANCEL );
		VIM_ERROR_RETURN_ON_FAIL( diag_append_record( ResultsPtr ));
	}		
	return VIM_ERROR_NONE;
}

#endif
