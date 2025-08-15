
#include "incs.h"

VIM_DBG_SET_FILE("U0");

extern file_version_record_t file_status[];
extern VIM_UINT8 active_file_id;

u_wow_cpat_trailer_t cpat_trailer;

static VIM_TEXT const* param_download_temp_filename=VIM_FILE_PATH_LOCAL "param.new";
VIM_SIZE chunk;
VIM_UINT8 file_id;
VIM_UINT32 file_version;


/////////////////////////////////////////////////////// INTERFACE FUNCTIONS ////////////////////////////////////////////////////

// Convert a Cpat Bin range as it comes over from WOW Host into low and high bcd ranges loaded into the global table

static VIM_ERROR_PTR CpatConvertBinRange( VIM_CHAR *AsciiCardPrefix, VIM_SIZE *CardRangeLow, VIM_SIZE *CardRangeHigh )
{
    VIM_SIZE n;
    VIM_CHAR AsciiCardRangeLow[WOW_CPAT_CARD_PREFIX_LEN],AsciiCardRangeHigh[WOW_CPAT_CARD_PREFIX_LEN];
    VIM_CHAR *Ptr;

    // form the low boundary and high boundary by translating the '?' char to '0' and '9' resp
    for( n=0, Ptr=AsciiCardPrefix; n<WOW_CPAT_CARD_PREFIX_LEN; n++, Ptr++ )
    {
        AsciiCardRangeLow[n] = AsciiCardRangeHigh[n] = *Ptr;
        if( *Ptr == '?' )
        {
            AsciiCardRangeLow[n] = '0';
            AsciiCardRangeHigh[n] = '9';           
        }
    }
    *CardRangeLow = asc_to_bin( AsciiCardRangeLow, WOW_CPAT_CARD_PREFIX_LEN );
    *CardRangeHigh = asc_to_bin( AsciiCardRangeHigh, WOW_CPAT_CARD_PREFIX_LEN );

    return VIM_ERROR_NONE;    
}





/* Activate the temp file and delete old one */

VIM_ERROR_PTR file_download_activate(VIM_UINT8 file_id, VIM_BOOL FileFromLFD )
{
  VIM_TEXT new_name[30];
  VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;

  /* get the name of the temporary file */ 
  
  vim_strcpy( new_name, VIM_FILE_PATH_LOCAL );
  if (file_status[CPAT_FILE_IDX].FileName[0] != 0)
  {
	  // LFD files have already been copied over
	  if( !FileFromLFD )
	  {
		  vim_strcat( new_name, file_status[file_id].FileName );    
		  VIM_ERROR_RETURN_ON_FAIL( vim_file_move( param_download_temp_filename, new_name )) ;
	  }
      // Only the CPAT is not in TLV format - CPAT file version is extracted from first record
      switch( file_id )
	  {
		case CPAT_FILE_IDX: 
            // RDD 210520 JIRA PIRPIN 1069 make a backup of Switch CPAT so it can be restored
            vim_file_delete(VIM_FILE_PATH_LOCAL "B_CPAT");
            vim_file_copy( new_name, VIM_FILE_PATH_LOCAL "B_CPAT");

			res = CpatLoadTable();
			break;

		case PKT_FILE_IDX:
			res = wow_capk_load();
			// RDD 080714 added 
			terminal_set_status(ss_wow_reload_ctls_params);

			break;

		case EPAT_FILE_IDX:
			res = wow_emv_params_load();
			// RDD 080714 added 
			terminal_set_status(ss_wow_reload_ctls_params);
			break;

		case FCAT_FILE_IDX:
			res = wow_fcat_load();
			break;

		default: return VIM_ERROR_SYSTEM;
      }
  }
  return res;
}



static VIM_ERROR_PTR process_cpat_trailer( VIM_UINT8 *field48TrailerRecord, VIM_UINT16 bcd_record_len )
{
    VIM_CHAR AsciiBuffer[100];
    
    // clear the buffer
    vim_mem_set( AsciiBuffer, '\0', VIM_SIZEOF(AsciiBuffer) );

    // convert the whole field to ascii for simplicity
    bcd_to_asc( field48TrailerRecord, AsciiBuffer, bcd_record_len*2 );

    vim_mem_copy( cpat_trailer.bytebuff, AsciiBuffer, bcd_record_len*2 );

    // RDD - need to ditch the first three digits as the version size sent over is 9 but we only use the last 6 digits in the 0800 Logon ! */
    terminal.file_version[CPAT_FILE_IDX] = asc_to_bin ( &cpat_trailer.fields.CPAT_Version[3], WOW_FILE_VER_BCD_LEN*2 );
	bin_to_bcd( terminal.file_version[CPAT_FILE_IDX], file_status[CPAT_FILE_IDX].ped_version.bytes, WOW_FILE_VER_BCD_LEN*2 );
	VIM_DBG_NUMBER(terminal.file_version[CPAT_FILE_IDX]);

    // RDD 010921 - Only WOW terminals use CPAT trailer SAF limits - others use TMS
    if (TERM_USE_PCI_TOKENS)
    {
        terminal.configuration.efb_credit_limit = asc_to_bin(cpat_trailer.fields.OfflineCreditLimit, WOW_CPAT_TRAILER_EFB_LIMIT_DIGITS);
        //DBG_VAR(terminal.configuration.efb_credit_limit);
        terminal.configuration.efb_debit_limit = asc_to_bin(cpat_trailer.fields.OfflineDebitLimit, WOW_CPAT_TRAILER_EFB_LIMIT_DIGITS);
        terminal.configuration.efb_max_txn_count = asc_to_bin(cpat_trailer.fields.SAFStorageLimit, WOW_CPAT_TRAILER_SAF_LIMIT_DIGITS);
    }

    //DBG_VAR(terminal.configuration.efb_debit_limit);
    
    terminal.number_of_cpat_entries = asc_to_bin( cpat_trailer.fields.TotalCPATEntries, WOW_CPAT_TRAILER_NUMBER_OF_ENTRIES_DIGITS );
	    //DBG_VAR(terminal.number_of_cpat_entries);
#ifdef _WIN32    
	//terminal.max_idle_time_seconds = 20;
    terminal.max_idle_time_seconds = asc_to_bin( cpat_trailer.fields.IdleLoopDelay, WOW_CPAT_TRAILER_IDLE_LOOP_DELAY_DIGITS );
#else
    terminal.max_idle_time_seconds = asc_to_bin( cpat_trailer.fields.IdleLoopDelay, WOW_CPAT_TRAILER_IDLE_LOOP_DELAY_DIGITS );
#endif
	//DBG_VAR(terminal.max_idle_time_seconds);
	terminal.configuration.efb_max_txn_count = asc_to_bin(cpat_trailer.fields.SAFStorageLimit, WOW_CPAT_TRAILER_SAF_LIMIT_DIGITS );
	//DBG_VAR(terminal.configuration.efb_max_txn_count);

	// RDD 170412 removed to save on file writes
    VIM_ERROR_RETURN_ON_FAIL(terminal_save());

	//VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "CPAT Parameters", "Processed OK" )); 										
	//vim_event_sleep( 1000 );

	DBG_MESSAGE( "CPAT Trailer Data saved to file OK ");
    return VIM_ERROR_NONE;
}




static VIM_ERROR_PTR process_cpat_entry( VIM_UINT8 *field48CpatRecord, VIM_CHAR *AsciiBuffer, VIM_UINT16 bcd_record_len, VIM_BOOL *last_entry_found )
{  
    // convert the whole field to ascii for simplicity
    bcd_to_asc( field48CpatRecord, AsciiBuffer, bcd_record_len*2 );

    // first check to ensure that this isn't the last record
    if( vim_mem_compare( AsciiBuffer, WOW_CPAT_RECORD_TERMINATOR, WOW_CPAT_RECORD_LEN*2 ) == VIM_ERROR_NONE )
    {
        // last record so set flag and return 
        *last_entry_found = VIM_TRUE;
        return VIM_ERROR_NONE;
    }
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR process_de48_cpat( VIM_UINT8 *ptr, VIM_BOOL *last_entry_found, VIM_BOOL last_message, VIM_SIZE *total_entries )
{
    VIM_UINT16 number_of_entries,record_number,n;    
    static VIM_UINT16 file_offset;
    VIM_CHAR AsciiBuffer[100];


    // clear the buffer
    vim_mem_set( AsciiBuffer, '\0', VIM_SIZEOF(AsciiBuffer) );

    record_number =  bcd_to_bin( ptr, 4 );

    // reset the offset if it's the first incoming message for this file type
    if( record_number == 1 )
    {
        file_offset = 0;
    }
    
    // check that the CPAT record number is correct
    if( record_number != file_status[active_file_id].NextEntryNumber  )
    {
		return ERR_FILE_DOWNLOAD;
	}
    
    // move ptr to the number of entries
    ptr+=2;

    /* extract file data block length from the message */
    number_of_entries = bcd_to_bin( ptr++, 2 );

    *total_entries += number_of_entries;
    if( number_of_entries != 12 )
    {
         //VIM_DBG_MESSAGE( "Last CPAT Entry" );
    }

    // ptr now points to start of first record
    for( n=0; n<(number_of_entries); n++ )
    {  
		///////////////////////////// TEST TEST TEST TEST //////////////////////////
		//return ERR_FILE_DOWNLOAD;

        VIM_ERROR_RETURN_ON_FAIL( process_cpat_entry( ptr, AsciiBuffer, WOW_CPAT_RECORD_LEN, last_entry_found ));

        if( (*last_entry_found) == VIM_FALSE )
        {
            /* add data to the temporary download file */
            VIM_ERROR_RETURN_ON_FAIL( vim_file_set_bytes( param_download_temp_filename, ptr, WOW_CPAT_RECORD_LEN, file_offset ));        
                
            /* increment the position within the file */
            file_status[active_file_id].NextEntryNumber += 1;
            file_offset += WOW_CPAT_RECORD_LEN;
		    ptr+=WOW_CPAT_RECORD_LEN;
        }
		else
		{
		    ptr+=WOW_CPAT_RECORD_LEN;
			break;
		}
	}
	/* RDD The stuff after the last entry is counted as an entry so need a special case to process this... */
    if( (*last_entry_found) && (last_message) )
    {
		process_cpat_trailer( ptr, WOW_CPAT_RECORD_LEN );
	}
	return VIM_ERROR_NONE;
}

/*
typedef struct
{
    VIM_UINT8 FileSize[2];
    VIM_UINT8 BlockOffset[2];
    VIM_UINT8 BlockSize[2];
    VIM_UINT8 BlockData[512];
    VIM_UINT32 ProcSpecnCode;
} s_wow_generic_entry_t;
*/

static VIM_ERROR_PTR process_de48_generic( VIM_UINT8 *ptr, VIM_BOOL *last_entry_found, VIM_UINT32 field_len, VIM_BOOL FirstMsg )
{
    u_wow_generic_entry_t u_generic_entry;    
    VIM_UINT32 block_offset,block_size,file_size;
    static VIM_UINT32 expected_offset;
    VIM_ERROR_PTR file_error=VIM_ERROR_NONE;
    static VIM_SIZE file_start_offset=0;

	///////////////////////////// TEST TEST TEST TEST //////////////////////////
	//return ERR_FILE_DOWNLOAD;

    // clear the structure 
    vim_mem_clear( (void *)u_generic_entry.bytebuff, VIM_SIZEOF(u_wow_generic_entry_t ));

    // copy over the data from field 48
    vim_mem_copy( u_generic_entry.bytebuff, ptr, field_len );

    // covert the block offset and size to binary
    block_offset = bcd_to_bin( u_generic_entry.fields.BlockOffset, 4 );
    /* fix 1 byte difference in the first offset between Dev swith and Finsim swith */
    if( FirstMsg )
    {
      file_start_offset = block_offset;
    }
    if( FirstMsg )
    {
      expected_offset = block_offset;
    }
    else
    {
        // check that the Block Offset is correct
        if( block_offset != expected_offset )
        {
			// RDD TODO - map these errors to supported ones in error.c
            return VIM_ERROR_AS2805_FORMAT;
        }
    }

    
    // get the binary file size 
    file_size = bcd_to_bin( u_generic_entry.fields.FileSize, 4 );

    // get the binary block size 
    block_size = bcd_to_bin( u_generic_entry.fields.BlockSize, 4 );

    /* RDD: add data to the temporary download file: 
    ** for some reason the first offset is 0x01 not 
    ** 0x00 so subtract 1 from the offset to write to the file */
    file_error = vim_file_set_bytes( param_download_temp_filename, &u_generic_entry.fields.BlockData, block_size, block_offset-file_start_offset );
    if( file_error != VIM_ERROR_NONE )
    {
      vim_file_delete (param_download_temp_filename);
      VIM_ERROR_RETURN( file_error );
    }

    // update the expected offset - the global is used in the de48 of the next 0300    
    file_status[active_file_id].NextEntryNumber += block_size;

    if(( expected_offset += block_size ) >=  file_size )
    {
        *last_entry_found = VIM_TRUE;
    }
    return VIM_ERROR_NONE;
}

/*
typedef struct
{
    VIM_CHAR CardPrefix[WOW_CPAT_CARD_PREFIX_LEN];
    VIM_CHAR CardNameIndex[2];
    VIM_CHAR AccGroupingCode;
    VIM_CHAR ProcessingOptionsBitmap[WOW_CPAT_PROC_OPT_BITMAP_LEN*2];
    VIM_CHAR ProcSpecnCode[2];
} s_wow_cpat_entry_t;
*/



/////////////////////////////////////////////////////// INTERFACE FUNCTIONS ////////////////////////////////////////////////////

VIM_ERROR_PTR CpatLoadTable( void )
{
    VIM_TEXT new_name[30];
    VIM_FILE_HANDLE file_handle;
    s_wow_cpat_entry_t s_cpat_entry;    
    VIM_UINT8 buffer[VIM_SIZEOF(s_wow_cpat_entry_t)];
    VIM_SIZE BytesRead,n,FileSize;
    VIM_ERROR_PTR res=VIM_ERROR_NONE;
    VIM_BOOL exists = VIM_FALSE;
    		
	vim_strcpy( new_name, VIM_FILE_PATH_LOCAL );	
	vim_strcat( new_name, file_status[CPAT_FILE_IDX].FileName );
	VIM_ERROR_RETURN_ON_FAIL( vim_file_exists( new_name, &exists ));
	if( !exists ) 
		return VIM_ERROR_NOT_FOUND;

	if ( file_status[CPAT_FILE_IDX].FileName[0] != 0 )
    {
        VIM_ERROR_RETURN_ON_FAIL( vim_file_get_size( new_name, VIM_NULL, &FileSize ));

        terminal.number_of_cpat_entries = FileSize/WOW_CPAT_RECORD_LEN;

        if(( res = vim_file_open( &file_handle, new_name, VIM_FALSE, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )
		{
			for( n=0; n<terminal.number_of_cpat_entries; n++ )
			{
				// load the ascii file data into a Cpat structure
				if(( res = vim_file_read( file_handle.instance, buffer, &BytesRead, WOW_CPAT_RECORD_LEN )) == VIM_ERROR_NONE )
				{
					// convert the whole field to ascii for simplicity - the processing option bitmap will be invalid so need to process this separatly
					bcd_to_asc( buffer, (VIM_CHAR *)&s_cpat_entry, VIM_SIZEOF(s_wow_cpat_entry_t) );

					// convert the bin range in binary
					if(( res = CpatConvertBinRange( s_cpat_entry.CardPrefix, &terminal.card_name[n].PAN_range_low, &terminal.card_name[n].PAN_range_high )) == VIM_ERROR_NONE )
					{
						// Set the index to the actual card name (the card names are currently hard coded but the intenetion is to move them into the FCAT file)
						terminal.card_name[n].CardNameIndex = asc_to_bin( s_cpat_entry.CardNameIndex, 2 );

			            // Copy the ACC Grouping code
			            terminal.card_name[n].AccGroupingCode = s_cpat_entry.AccGroupingCode;

			            // copy the proc spec code
						terminal.card_name[n].ProcSpecnCode = asc_to_bin( s_cpat_entry.ProcSpecnCode, 2 );

						// copy the card processing options bitmap
						vim_mem_copy( &terminal.card_name[n].ProcessingOptionsBitmap, &buffer[6], WOW_CPAT_PROC_OPT_BITMAP_LEN );
					}
				}
			}
        }
		res = vim_file_close( file_handle.instance );
    }
    return res;
}



/*----------------------------------------------------------------------------*/

#define MATCH_LEN 2
#define VERSION_LEN 6
#define MIN_VERSION_LEN 6

VIM_ERROR_PTR ValidateFileName( VIM_CHAR **FoundFileName, VIM_CHAR *RootFileName, VIM_SIZE *Version )
{
	VIM_CHAR VersionString[VERSION_LEN+1];
	VIM_CHAR *FoundPtr = *FoundFileName;
	VIM_CHAR *RootPtr = RootFileName;	
	VIM_UINT8 n;
	VIM_SIZE FileVersion = 0;


	//VIM_DBG_MESSAGE( "--------------- Validating this filename... -------------");
	// RDD: Format is CPAT: CPATxxxx, EPAT: EPATxxxx, PKT: PKATxxxx, FCAT: FCATxxxx where xxxxxx is a fixed 6 digit version number
	VIM_DBG_STRING( RootFileName );
	VIM_DBG_STRING( *FoundFileName );
	//VIM_DBG_MESSAGE( RootFileName );

	//VIM_DBG_MESSAGE( "--------------- Check first two letters match... -------------");
	// Check 1: first two letters match the root filename
	for( n=0; n< MATCH_LEN; n++, FoundPtr++, RootPtr++ )
	{
		if( *FoundPtr != *RootPtr )
		{
			VIM_DBG_STRING( "NO");
			return VIM_FALSE;
		}
	}

	VIM_DBG_MESSAGE( "YES" );

	// Allow a few extra chars 
	for( n=0; n<3; n++ )
	{
		if( !VIM_ISDIGIT(*FoundPtr) ) 
			FoundPtr++;
	}

	// Check 2: next 6 characters of the filename are digits
	vim_mem_clear( VersionString, VIM_SIZEOF(VersionString) );
	for( n=0; n< VERSION_LEN; n++, FoundPtr++ )
	{
		// If no digits then this could be the current config file ( eg. CPAT or EPAT etc. so exit as this is not the new file )
		if( !VIM_ISDIGIT(*FoundPtr) )
		{
			VIM_CHAR DebugString[100];
			vim_mem_clear( DebugString, VIM_SIZEOF( DebugString ));
			if( n < MIN_VERSION_LEN )
			{
				VIM_DBG_MESSAGE( "Fail" );
				return VIM_ERROR_NOT_FOUND;
			}
			else
			{
				// ensure that anything other than 
				if( vim_strlen( FoundPtr ) )
				{
					// if the extension is neither .p7s or P7S then delete the file as someone stuffed up
					if( vim_strncmp( FoundPtr, ".p7s", 4 ) && ( vim_strncmp( FoundPtr, ".P7S", 4 )))
					{
						vim_sprintf( DebugString, "WARNING: Deleting Invalid File Extension %s", FoundPtr );							  
						VIM_DBG_ERROR( vim_file_delete_path( *FoundFileName, VIM_FILE_PATH_GLOBAL ));
					}
					else
					{
						// RDD - null terminate : V. Important !
						vim_sprintf( DebugString, "OK Found Valid Signature File: %s", *FoundFileName );
					}
				}
				else
				{
					vim_sprintf( DebugString, "OK Found Valid Config File: %s", *FoundFileName );
				}
				DBG_STRING( DebugString );
				pceft_debug_test( pceftpos_handle.instance, DebugString );				
				break;
			}
		}
		else
			VersionString[n] = *FoundPtr;
	}

	//Check 3: next character is a period
	//if( *FoundPtr != '.' ) return VIM_FALSE;

	VIM_DBG_STRING( VersionString );
	FileVersion = asc_to_bin( VersionString, VERSION_LEN );
	VIM_DBG_NUMBER( FileVersion );
	*Version = FileVersion;
	VIM_DBG_NUMBER( *Version );
	return VIM_ERROR_NONE;
}

#if 0 // RDD - TODO Get File Version From Within File Itself - method varies according to type
VIM_ERROR_PTR FindVersionFromFile( VIM_UINT8 file_id, VIM_SIZE *FileVersion )
{
	VIM_FILE_HANDLE *handle_ptr,

	// Read the New File and get the version from inside the TLV or whatever
	if( vim_file_open( &file_handle, file_status[file_id].FileName, VIM_FALSE, VIM_TRUE, VIM_FALSE ) != VIM_ERROR_NONE 
	{
		switch( file_id )
		{
			case CPAT_FILE_IDX:
				break;
		

			default:
				break;
		}
	}
	return VIM_ERROR_NONE;
}
#endif


VIM_ERROR_PTR GetFileVersion( VIM_UINT8 file_id, VIM_SIZE *FileVersion, VIM_CHAR *ValidFileName )
{
	VIM_CHAR *Ptr;
	VIM_TEXT current_file_name[100];// RDD - enusre that these are big enough !!!		
	VIM_CHAR next_file_name[100];	// RDD - enusre that these are big enough !!!


	vim_mem_clear( current_file_name, VIM_SIZEOF( current_file_name ) );	
	vim_mem_clear( next_file_name, VIM_SIZEOF( next_file_name ) );	
	vim_strcpy( current_file_name, "." );
		
	VIM_DBG_MESSAGE( "------------ search for valid config file (requires version number in filename)  ----------------" );

#ifdef _WIN32
	*FileVersion = 123456;
	vim_strcpy( ValidFileName, file_status[file_id].FileName );
#else

	while(1)	
	{
		VIM_SIZE Version = 0;
		// look for the CPAT.ZIP, EPAT.ZIP etc. - will exit with not found if the file is not there
		vim_mem_clear( next_file_name, VIM_SIZEOF( next_file_name ) );
		Ptr = next_file_name;
	
		// Get the next filename
		VIM_ERROR_RETURN_ON_FAIL( vim_file_list_directory( VIM_FILE_PATH_GLOBAL, current_file_name, Ptr, 10000 ));
	
		DBG_STRING( Ptr );
		// Check the current directory listing against what we're looking for
		ValidateFileName( &Ptr, file_status[file_id].FileName, &Version );

		VIM_DBG_NUMBER( Version );
		if(( 0 < Version ) && ( Version <= 999999 ))
		{	
			// If the file downloaded is the same as the current version exit and continue to download via switch				
			DBG_STRING( next_file_name );
			VIM_DBG_NUMBER( terminal.file_version[file_id] );
			if( Version == terminal.file_version[file_id] ) 
			{
				DBG_MESSAGE( "Already using this version - why was it downloaded ????");
				
				//VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );	
			}
			// File version is valid and doesn't match the current version - copy this file name over
			vim_strcpy( ValidFileName, Ptr );
			break;
		}

		//DBG_STRING("INVALID FILE");
		vim_strcpy( current_file_name, Ptr );
	}
#endif
	return VIM_ERROR_NONE;
}

//#ifdef _DEBUG	// RDD 070316 Very New Version allows pickup from my own generated files without need for signing
#if 1

typedef struct {
	VIM_UINT8 NumberOfRecords[2];
	VIM_UINT8 RecordNumber[2];
	VIM_UINT8 TotalLength[2];
	VIM_UINT8 RecordLength[2];
} S_CFG_RECORD_HDR;


// RDD 010513 Implemented for P4 Config file download from LFD - these contain extra info not found in the files sent from the switch - need to strip this out
VIM_ERROR_PTR ProcessConfigFile( VIM_CHAR *PathName, VIM_CHAR *NewPath, VIM_UINT8 file_id )
{
	VIM_SIZE FileSize = 0;
    VIM_FILE_HANDLE file_handle;
	VIM_ERROR_PTR res;
	VIM_UINT8 buffer[25000];
	VIM_UINT8 *Ptr;
  //VIM_UINT8 *Ptr2;

	//VIM_SIZE HeaderSize = ( file_id == CPAT_FILE_IDX ) ? 6 : 8;
	vim_mem_clear( buffer, VIM_SIZEOF(buffer) );
	VIM_ERROR_IGNORE( vim_file_get_size( PathName, VIM_NULL, &FileSize ));

	if( FileSize > VIM_SIZEOF(buffer) )
	{
		VIM_DBG_PRINTF1( "!!!File Size is too big: %s!!!", PathName );
		VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
	}
	// Open our manufactured file and read it into a buffer for re-formatting
	if(( res = vim_file_open( &file_handle, PathName, VIM_FALSE, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )		
	{
		VIM_SIZE BytesRead = 0;
    //VIM_SIZE RecordLength = 0;

		if(( res = vim_file_read( file_handle.instance, buffer, &BytesRead, FileSize )) == VIM_ERROR_NONE )				
		{
			vim_file_close( file_handle.instance );
			//VIM_ERROR_IGNORE( vim_file_delete_path( file_status[file_id].FileName, VIM_FILE_PATH_LOCAL ));

			// RDD 231115 added 
			VIM_ERROR_IGNORE( vim_file_delete(  NewPath ));
			// Open the Destination (native) file for writing the raw TLV data
			Ptr = buffer;
			if(( res = vim_file_open( &file_handle, NewPath, VIM_TRUE, VIM_TRUE, VIM_TRUE )) == VIM_ERROR_NONE )		
			{	
				VIM_SIZE RecordNumber, NumberOfRecords;
/*
				typedef struct {
					VIM_UINT8 NumberOfRecords[2];
					VIM_UINT8 RecordNumber[2];
					VIM_UINT8 TotalLength[2];
					VIM_UINT8 RecordLength[2];
				} S_CFG_RECORD_HDR;
*/			
				do			
				{	
					// We need to process the CPAT trailer here. Note that this doesn't get written to the final version of the file				
					S_CFG_RECORD_HDR *RecordPtr = (S_CFG_RECORD_HDR *)Ptr;							
					VIM_SIZE  RecordLength = ( file_id == CPAT_FILE_IDX ) ? ( 256 * RecordPtr->TotalLength[0] + RecordPtr->TotalLength[1] ): 
																			( 256 * RecordPtr->RecordLength[0] + RecordPtr->RecordLength[1] );

					NumberOfRecords = 256 * RecordPtr->NumberOfRecords[0] + RecordPtr->NumberOfRecords[1];
					RecordNumber = 256 * RecordPtr->RecordNumber[0] + RecordPtr->RecordNumber[1];					

					if(( file_id == CPAT_FILE_IDX ) && ( RecordPtr->RecordNumber[1] == RecordPtr->NumberOfRecords[1] ))
					{
						VIM_SIZE SkipLastXBytes = (WOW_CPAT_RECORD_LEN*2);
						Ptr += VIM_SIZEOF(S_CFG_RECORD_HDR)-2;
						res = vim_file_write( file_handle.instance, Ptr, RecordLength-SkipLastXBytes );
						Ptr += (RecordLength-WOW_CPAT_RECORD_LEN);
						process_cpat_trailer( Ptr, WOW_CPAT_RECORD_LEN );
					}
					else
					{
						Ptr += ( file_id == CPAT_FILE_IDX ) ? VIM_SIZEOF(S_CFG_RECORD_HDR)-2 : VIM_SIZEOF(S_CFG_RECORD_HDR);
						if(( RecordNumber==1 ) && ( file_id != CPAT_FILE_IDX ))
						{
							VIM_UINT8 *VerPtr = Ptr+4;
							terminal.file_version[file_id] = bcd_to_bin( VerPtr, 6 );
 						}
						res = vim_file_write( file_handle.instance, Ptr , RecordLength );
                        VIM_DBG_ERROR(res);
					}
					Ptr += RecordLength;
				}
				while( ++RecordNumber <= NumberOfRecords );
			}
		}
	}
#if 0	// RDD 140218 v574-1 this caused 'D' state always on power up
	else
	{
		VIM_DBG_PRINTF1( "Set TMS required as unable to Read Config file: %s", PathName );
		// RDD - no file so set the version to zero and force TMS
		terminal.file_version[file_id] = 0;
		terminal_clear_status( ss_tms_configured );
	}
#endif
	vim_file_close( file_handle.instance );
	VIM_ERROR_RETURN_ON_FAIL( res );
    return VIM_ERROR_NONE;
}
	

#else	// RDD 190315 New Version

typedef struct {
	VIM_UINT8 NumberOfRecords[2];
	VIM_UINT8 RecordNumber[2];
	VIM_UINT8 TotalLength[2];
	VIM_UINT8 RecordLength[2];
} S_CFG_RECORD_HDR;


// RDD 010513 Implemented for P4 Config file download from LFD - these contain extra info not found in the files sent from the switch - need to strip this out
VIM_ERROR_PTR ProcessConfigFile( VIM_CHAR *PathName, VIM_CHAR *NewPath, VIM_UINT8 file_id )
{
	VIM_SIZE FileSize = 0;
    VIM_FILE_HANDLE file_handle;
	VIM_ERROR_PTR res;
	VIM_UINT8 buffer[10000];
	VIM_CHAR DisplayBuffer[50];
	VIM_UINT8 *Ptr,*Ptr2;

	VIM_SIZE HeaderSize = ( file_id == CPAT_FILE_IDX ) ? 6 : 8;
	vim_mem_clear( buffer, VIM_SIZEOF(buffer) );
	VIM_ERROR_RETURN_ON_FAIL( vim_file_get_size( PathName, VIM_NULL, &FileSize ));

	if( FileSize > VIM_SIZEOF(buffer) ) VIM_ERROR_RETURN( VIM_ERROR_OVERFLOW );
     
	// Open our manufactured file and read it into a buffer for re-formatting

	vim_sprintf( DisplayBuffer, "Reading\n%s", PathName );
	VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "" )); 
	vim_event_sleep( 1000 );

	if(( res = vim_file_open( &file_handle, PathName, VIM_FALSE, VIM_TRUE, VIM_FALSE )) == VIM_ERROR_NONE )		
	{
		VIM_SIZE BytesRead = 0, RecordLength = 0;

		if(( res = vim_file_read( file_handle.instance, buffer, &BytesRead, FileSize )) == VIM_ERROR_NONE )				
		{
			vim_file_close( file_handle.instance );
			VIM_ERROR_IGNORE( vim_file_delete_path( file_status[file_id].FileName, VIM_FILE_PATH_LOCAL ));
			// Open the Destination (native) file for writing the raw TLV data
			Ptr = buffer;
			if(( res = vim_file_open( &file_handle, NewPath, VIM_TRUE, VIM_TRUE, VIM_TRUE )) == VIM_ERROR_NONE )		
			{	
				VIM_SIZE RecordNumber, NumberOfRecords;
			
				do			
				{	
					// We need to process the CPAT trailer here. Note that this doesn't get written to the final version of the file				
					S_CFG_RECORD_HDR *RecordPtr = (S_CFG_RECORD_HDR *)Ptr;							
					VIM_SIZE  RecordLength = ( file_id == CPAT_FILE_IDX ) ? ( 256 * RecordPtr->TotalLength[0] + RecordPtr->TotalLength[1] ): 
																			( 256 * RecordPtr->RecordLength[0] + RecordPtr->RecordLength[1] );

					NumberOfRecords = 256 * RecordPtr->NumberOfRecords[0] + RecordPtr->NumberOfRecords[1];
					RecordNumber = 256 * RecordPtr->RecordNumber[0] + RecordPtr->RecordNumber[1];					

					vim_sprintf( DisplayBuffer, "Read Record %d", RecordNumber );					
					VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuffer, "" )); 					
					vim_event_sleep( 2 );

					if(( file_id == CPAT_FILE_IDX ) && ( RecordPtr->RecordNumber[1] == RecordPtr->NumberOfRecords[1] ))
					{
						VIM_SIZE SkipLastXBytes = (WOW_CPAT_RECORD_LEN*2);
						Ptr += VIM_SIZEOF(S_CFG_RECORD_HDR)-2;
						res = vim_file_write( file_handle.instance, Ptr, RecordLength-SkipLastXBytes );
						Ptr += (RecordLength-WOW_CPAT_RECORD_LEN);
						process_cpat_trailer( Ptr, WOW_CPAT_RECORD_LEN );
					}
					else
					{
						Ptr += ( file_id == CPAT_FILE_IDX ) ? VIM_SIZEOF(S_CFG_RECORD_HDR)-2 : VIM_SIZEOF(S_CFG_RECORD_HDR);
						res = vim_file_write( file_handle.instance, Ptr , RecordLength );
					}
					Ptr += RecordLength;
				}
				while( ++RecordNumber <= NumberOfRecords );
			}
		}
	}
	vim_file_close( file_handle.instance );
	VIM_ERROR_RETURN( res );
}
#endif


	

void DeleteGroup15Files( VIM_CHAR *FoundFileName )
{
	VIM_CHAR TempFileName[100];	

	vim_mem_clear( TempFileName, VIM_SIZEOF( TempFileName ) );
	DBG_MESSAGE( "DDDDDDDDDDDDD Dele group 15 DDDDDDDDDDDDDDDD" );
	DBG_STRING( FoundFileName );

	vim_file_delete_path( FoundFileName, VIM_FILE_PATH_GLOBAL );						
					
	// Delete the file as it has been copied to gp1 by now if it was any good
	vim_file_delete_path( FoundFileName, VIM_FILE_PATH_GLOBAL );
								
	// Delete the .P7S file - even if it wasn't validated		
	vim_strcpy( TempFileName, FoundFileName );				
	vim_strcat( TempFileName, ".P7S" );		
	vim_file_delete_path( TempFileName, VIM_FILE_PATH_GLOBAL );		
		
	// Delete the SIG file 			
	vim_strcpy( TempFileName, FoundFileName );				
	vim_strcat( TempFileName, ".SIG" );				
	vim_file_delete_path( TempFileName, VIM_FILE_PATH_GLOBAL );		
}



VIM_ERROR_PTR ValidateConfigFile( VIM_UINT8 file_id, VIM_CHAR *FileName, VIM_SIZE *BinFileVer )
{		
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//VIM_SIZE max_file_size = 10000;		
	VIM_BOOL exists = VIM_FALSE;
	VIM_BOOL authentic = VIM_FALSE;
	VIM_TEXT PathName[100];// RDD - enusre that these are big enough !!!		
	VIM_TEXT FoundFileName[100];// RDD - ensure that these are big enough !!!	
	VIM_CHAR TempFileName[100];	
	VIM_CHAR TempFileName2[100];

	VIM_SIZE FileVersion = *BinFileVer;
	VIM_CHAR DebugBuffer[150];

	vim_mem_clear( PathName, VIM_SIZEOF(PathName) );
	vim_mem_clear( FoundFileName, VIM_SIZEOF(FoundFileName) );
	vim_mem_clear( TempFileName, VIM_SIZEOF( TempFileName ) );

#ifdef _WIN32
	vim_strcpy( PathName, VIM_FILE_PATH_LOCAL );
#else
	vim_strcpy( PathName, VIM_FILE_PATH_GLOBAL );
#endif

	if( FileVersion )
	{
		// Basically keep a copy of the filename without the path or the P7S extension for copying later
		DBG_MESSAGE( "FileVersion Provided:" );
		VIM_DBG_NUMBER( FileVersion );
		vim_strcpy( FoundFileName, FileName );
		vim_strcat( PathName, FileName );
	}
	else
	{
		// RDD - if we don't know the file version we have to search through group 15 to find something valid
		VIM_CHAR *Ptr = &FoundFileName[0];
		DBG_MESSAGE( "Need to Find FileVersion from FileName:" );
		DBG_MESSAGE( FoundFileName );

		// This will return error if there is no file version OR if it matches the current registered version for that file
		if( GetFileVersion( file_id, BinFileVer, Ptr ) == VIM_ERROR_NONE )
		{
			DBG_MESSAGE( "Found file version number and it is new" );				
			vim_strcat( PathName, Ptr );
		}
		else
		{
			DBG_MESSAGE( FoundFileName );
			DeleteGroup15Files( FoundFileName );
			VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
		}
	}	
#ifndef _WIN32
	vim_strcat( PathName, ".P7S" );
#endif
	VIM_DBG_STRING( PathName );
	
	// Check to see if a signature file has been included
	VIM_ERROR_IGNORE( vim_file_exists( PathName, &exists ));

	if( exists )
	{
		DBG_POINT;		
#if 0
		VIM_ERROR_IGNORE( vim_terminal_authenticate_file( PathName, &authentic ) );
#else
		// RDD 101116 v561-3 need logon to fail if error occurs otherwise things get stuffed up !
		VIM_ERROR_RETURN_ON_FAIL( vim_terminal_authenticate_file( PathName, &authentic ) );
#endif
			
		// If the file is verified as authentic then just copy it over the original CPAT etc.
		// Also need to delete the downloaded files and their corresponding signature files

		if( authentic )		
		{						
			VIM_DBG_MESSAGE( "File Was sucessfully Validated" );
			vim_sprintf( DebugBuffer, "File Validated:\n%s", PathName );
			pceft_debug_test( pceftpos_handle.instance, DebugBuffer );
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DebugBuffer, "" )); 	
			vim_event_sleep( 1000 );

#ifndef _WIN32
			vim_mem_clear( TempFileName2, VIM_SIZEOF( TempFileName2 ) );
						
			// Copy the file over the current one
			vim_strcpy( TempFileName, VIM_FILE_PATH_GLOBAL );
			vim_strcat( TempFileName, FoundFileName );

			vim_strcpy( TempFileName2, VIM_FILE_PATH_LOCAL );
			vim_strcat( TempFileName2, file_status[file_id].FileName );

			VIM_DBG_STRING( TempFileName );
			VIM_DBG_STRING( TempFileName2 );

//			if(( res = vim_file_copy( TempFileName, TempFileName2 )) != VIM_ERROR_NONE ) 
			if(( res = ProcessConfigFile( TempFileName, TempFileName2, file_id )) != VIM_ERROR_NONE ) 
			{
				vim_sprintf( DebugBuffer, "ERROR: Copy Error: %s to %s", TempFileName, TempFileName2 );
				pceft_debug( pceftpos_handle.instance, DebugBuffer );
				VIM_DBG_ERROR(res);

				// RDD 101116 v561-3 need logon to fail if error occurs otherwise things get stuffed up !
				VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
			}
			else
			{
				vim_sprintf( DebugBuffer, "OK: File Transmuted: %s to %s", TempFileName, TempFileName2 );
				pceft_debug_test( pceftpos_handle.instance, DebugBuffer );
			}

#else
			if(1)
			{
				VIM_CHAR NewPath[50];
				vim_strcpy( NewPath, VIM_FILE_PATH_LOCAL );				
				vim_strcat( NewPath, file_status[file_id].FileName );
				ProcessConfigFile( PathName, NewPath, file_id );
			}
#endif			
		}
		else
		{
			vim_sprintf( DebugBuffer, "Validation Failed\n%s", PathName );
			pceft_debug( pceftpos_handle.instance, DebugBuffer );
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DebugBuffer, "" )); 	
			vim_event_sleep( 1000 );
			res = VIM_ERROR_SYSTEM;
		}

		// Always delete the group15 files - even if something went wrong.

	}	
	else
	{
		vim_sprintf( DebugBuffer, "ERROR: Signature File Not Found: %s", PathName );
		pceft_debug( pceftpos_handle.instance, DebugBuffer );
		res = VIM_ERROR_NOT_FOUND;
	}

	DBG_MESSAGE( FoundFileName );    
	DeleteGroup15Files( FoundFileName );
	return res;
}


VIM_ERROR_PTR file_download( VIM_UINT8 file_id, file_download_t source )
{
	VIM_AS2805_MESSAGE msg;
	VIM_UINT32 field_len;
	VIM_UINT8 *ptr;
	VIM_BOOL last_message = VIM_FALSE;
	VIM_BOOL last_entry_found = VIM_FALSE;
	VIM_UINT32 screen_id;
	VIM_BOOL FirstMsg = VIM_TRUE;
    VIM_SIZE total_entries = 0;

	//display_screen(D144_DOWNLOADING_FILE, VIM_PCEFTPOS_KEY_NONE, file_id, percent_complete);
	/* delete the temporary file */

	if( source == lfd_download )
	{
		// RDD 030413 - Phase4 - attempt download via LFD first. If this fails then continue via switch
		if( tms_contact( VIM_TMS_DOWNLOAD_PARAMETERS, VIM_TRUE, web_then_vim ) == VIM_ERROR_NONE )
		{	
			return VIM_ERROR_TMS_CONFIG;
		}
	}
	else if( source != switch_download ) return VIM_ERROR_NONE;

	// RDD 030413 - The lfd attempt failed for some reason so get the file from the switch
	VIM_ERROR_RETURN_ON_FAIL(vim_file_delete(param_download_temp_filename));

	file_status[file_id].MessageNumber = 1;
	file_status[file_id].NextEntryNumber = 1;
  
  // RDD - TODO - get rid of this global and change fn below to take the file id as an arguement
  active_file_id = file_id;

  while( !last_message )
  {
    // Send a 300 file download message to the WOW host
    if(( init.error = msgAS2805_transaction(HOST_WOW, &msg, MESSAGE_WOW_300_NMIC_151, &txn, VIM_FALSE, VIM_FALSE, VIM_FALSE )) != VIM_ERROR_NONE )
	{
		// RDD 230516 SPIRA:8944 - odd things happening when we receive a 96 response from Finsim while downloading CPAT etc.
		vim_strcpy( init.logon_response, txn.host_rc_asc );
		return init.error;
	}

#if 0  /* TODO: check with Richard, this not in PH1 */
    if( msg.fields[39] == VIM_NULL )
    {
        return MSG_ERR_FORMAT_ERR;
    }
#endif
	// Field 72 only has data in it if this is the last msg
	if( msg.fields[72] != VIM_NULL )
	{
		last_message = VIM_TRUE;
	}
    
	if ((vim_mem_compare(txn.host_rc_asc, "00", 2) != VIM_ERROR_NONE)&& 
        (vim_mem_compare(txn.host_rc_asc, "NG", 2) != VIM_ERROR_NONE))
    {
      vim_mem_copy(init.logon_response, txn.host_rc_asc, 2);
      VIM_ERROR_RETURN(ERR_WOW_ERC);
    }

	// 
    // return an error if no field 48
    VIM_ERROR_RETURN_ON_NULL(msg.fields[48]);
    
	// 

	/* extract file data block from the message  - skip the block header (3 bytes) */
	ptr = msg.fields[48];
	field_len = asc_to_bin( (const char*)ptr, 3 );
	ptr+=3;	// skip the length bytes

    switch( file_id )
    {
        case CPAT_FILE_IDX:
        screen_id = DISP_LOADING_CPAT;
        VIM_ERROR_RETURN_ON_FAIL(process_de48_cpat( ptr, &last_entry_found, last_message, &total_entries ));
        break;

        case EPAT_FILE_IDX:
        screen_id = DISP_LOADING_EPAT;
        VIM_ERROR_RETURN_ON_FAIL(process_de48_generic( ptr, &last_entry_found, field_len, FirstMsg ));
        break;

        case PKT_FILE_IDX:        
        screen_id = DISP_LOADING_PKT;
        VIM_ERROR_RETURN_ON_FAIL(process_de48_generic( ptr, &last_entry_found, field_len, FirstMsg ));
        break;

        case FCAT_FILE_IDX:
        screen_id = DISP_LOADING_FCAT;
        VIM_ERROR_RETURN_ON_FAIL(process_de48_generic( ptr, &last_entry_found, field_len, FirstMsg ));
        break;

        default:               
			VIM_ERROR_RETURN(ERR_USER_BACK);
    }

	FirstMsg = VIM_FALSE;
#ifdef _DEBUG
	// RDD 030413 - allow cancel of file downloads in debug version
    VIM_ERROR_RETURN_ON_FAIL( display_screen( screen_id, VIM_PCEFTPOS_KEY_CANCEL, file_status[file_id].MessageNumber, file_status[file_id].MessageNumber ));
#else
    VIM_ERROR_RETURN_ON_FAIL( display_screen( screen_id, VIM_PCEFTPOS_KEY_NONE, file_status[file_id].MessageNumber, file_status[file_id].MessageNumber ));
#endif

    if( last_entry_found )
    {		
		// RDD 050115 - add a disconnect here as we've removeed them from during the message itself				
		DBG_MESSAGE( "Force Disconnect" );
		//comms_force_disconnect();

		VIM_DBG_LABEL_VAR( "<<<< Last File Entry Processed >>>> ",file_id )
        //display_screen(D144_DOWNLOADING_FILE, VIM_PCEFTPOS_KEY_NONE, file_id, 100);
        VIM_ERROR_RETURN_ON_FAIL( file_download_activate( file_id, VIM_FALSE ));
        //set_changed_flag( file_id );
		//////////////////////////////////// TEST TEST TEST /////////////////////////////////////
		 //break; // RDD 210512 - only put this in here while de72 not fixed in WOW test CPAT for fuel cards
		/////////////////////////////////////////////////////////////////////////////////////////


    }    
    else
    {    
        file_status[file_id].MessageNumber++;
    }
  }
  //pceft_pos_display_close();
  return VIM_ERROR_NONE;
}



