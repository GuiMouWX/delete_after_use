#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>

//#include <incs.h>
#include <vim.h>
#include <errno.h>

#include <mal_nfc.h>

#include <nfc/NFC_Client.h>
#include <nfc/nfc_ipc.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sysinfo/sysinfo.h>
#include <vficom.h>
#include <svcInfoAPI.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <com/libcom.h>

#include <pthread.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include "defs.h"

#include <nfc/utils.h>

#ifdef __cplusplus
extern "C" {
#endif
    extern VIM_ERROR_PTR Wally_SessionStateMachine(void);
    extern VIM_PCEFTPOS_HANDLE pceftpos_handle;
    extern VIM_ERROR_PTR pceft_debug(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT *message);
    extern VIM_ERROR_PTR pceft_debug_test(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT *message);
    extern VIM_ERROR_PTR pceft_debugf(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT* format, ...);
    extern VIM_ERROR_PTR pceft_debugf_test(VIM_PCEFTPOS_PTR self_ptr, VIM_TEXT* format, ...);

	extern VIM_BOOL IsStandalone(void);
#ifdef __cplusplus
}
#endif




// _NFC_DEBUG_LEVEL
// 0 = Off
// 1 = Min - high level only
// 2 = Med - a bit more
// 3 = Max - everything!
#define _NFC_DEBUG_LEVEL_OFF 0
#define _NFC_DEBUG_LEVEL_MIN 1
#define _NFC_DEBUG_LEVEL_MED 2
#define _NFC_DEBUG_LEVEL_MAX 3

#define _NFC_DEBUG_LEVEL _NFC_DEBUG_LEVEL_MIN

#define MAX_JSON_FILE_SIZE 8192
#define MAX_VAS_CARD_NUMBER 128

#if 1
typedef struct VIM_NET_INTERFACE
{
    int os_handle;
    VIM_UINT8 ip_address[4];
    VIM_UINT16 port;
} VIM_NET_INTERFACE;
#endif

// large buffer, put in program memory rather than stack
static char s_szBuffer[MAX_JSON_FILE_SIZE+1] = { 0, };

static VIM_ERROR_PTR s_eEventResult = VIM_ERROR_NOT_INTIALIZED;
static char s_szLoyaltyCardNumber[MAX_VAS_CARD_NUMBER+1] = { 0, };
static VIM_ICC_SESSION_TYPE s_eSessionType = VIM_ICC_SESSION_UNKNOWN;
static VIM_BOOL s_bEnableCancel = VIM_FALSE;
static int s_iCommsFd = -1;
static VIM_BOOL s_bIsTcp = VIM_FALSE;
static VIM_BOOL s_bWallyEnabled = VIM_FALSE;


static VIM_ERROR_PTR mal_nfc_SetRawDataBytes( rawData *psData, uint8_t* pucDataBytes, uint32_t ulDataLen )
{
   VIM_ERROR_RETURN_ON_NULL( psData );
   VIM_ERROR_RETURN_ON_NULL( pucDataBytes );

   memset( psData, 0x00, sizeof( rawData ) );

   psData->buffer = pucDataBytes;
   psData->bufferLen = ulDataLen;
   psData->bufferMaxSize = ulDataLen;

   return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR mal_nfc_SetRawDataString( rawData *psData, const char* pszDataString )
{
   VIM_ERROR_RETURN_ON_NULL( pszDataString );

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataBytes( psData, (uint8_t *)pszDataString, strlen( pszDataString ) ) );

   return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR mal_nfc_SetRawDataStringFromFile( rawData *psData, const char* pszDataFileName )
{
   VIM_SIZE ulBytesRead = 0;

   VIM_ERROR_RETURN_ON_NULL( pszDataFileName );

   vim_mem_clear(s_szBuffer,sizeof(s_szBuffer));

   VIM_ERROR_RETURN_ON_FAIL( vim_file_get( pszDataFileName, s_szBuffer, &ulBytesRead, sizeof(s_szBuffer) ) );

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataBytes( psData, (uint8_t *)s_szBuffer, strlen( s_szBuffer ) ) );

   return VIM_ERROR_NONE;
}

VIM_ERROR_PTR mal_nfc_Init( void )
{
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE( "Calling: NFC_Client_Init(CL_TYPE_FUNCTION)" );
#endif

   // RDD 150121 JIRA PIRPIN-947 TODO - move this somewhere more appropriate, but this is called on power-up and not part of VIM so put here for now as no plans to remove mal_vas
   sysSetPropertyInt(vfisysinfo::SYS_PROP_KEYBOARD_BEEP, 1);
   sysSetPropertyInt(vfisysinfo::SYS_PROP_VOLUME, 80);

   CL_STATUS eStatus = NFC_Client_Init(CL_TYPE_FUNCTION);
   if (eStatus != CL_STATUS_SUCCESS)
   {
       VIM_DBG_MESSAGE("--------------------- ERROR: NFC_Client_Init ------------------------");      
      
       // RDD 130121 - often get an error here these days but doesn't seem to stop MAL VAS from working - prob. need to find out which error and check it out..
       VIM_DBG_PRINTF1( "NFC_Client_Init Err:%d", eStatus );
       VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
   VIM_DBG_MESSAGE("NFC_Client_Init Successful");

   mal_nfc_GetVersion();

   return VIM_ERROR_NONE;
}

VIM_ERROR_PTR mal_nfc_Open()
{
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE( "Calling: NFC_PT_Open" );
#endif
   ResponseCodes eResp = NFC_PT_Open();
   if (eResp != EMB_APP_OK)
   {
      VIM_DBG_PRINTF1("ERROR: NFC_PT_Open: %d", eResp);
      if ((eResp = NFC_PT_Close()) != EMB_APP_OK)
      {
          VIM_DBG_PRINTF1("ERROR: NFC_PT_Close: %d", eResp);
          VIM_ERROR_RETURN(VIM_ERROR_OS);
      }
      else
      {
          eResp = NFC_PT_Open();
          VIM_DBG_PRINTF1("[retry] NFC_PT_Open: %d", eResp);
      }
   }
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE("NFC_PT_Open Successful");
#endif

   return VIM_ERROR_NONE;
}

VIM_ERROR_PTR mal_nfc_Close()
{
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE( "Calling: NFC_PT_Close" );
#endif
   ResponseCodes eResp = NFC_PT_Close();
   if (eResp != EMB_APP_OK)
   {
      VIM_DBG_MESSAGE("ERROR: NFC_PT_Close");
      VIM_DBG_VAR(eResp);
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE("NFC_PT_Close Successful");
#endif

   return VIM_ERROR_NONE;
}

VIM_ERROR_PTR mal_nfc_GetVersion()
{
   uint8_t aucOutData[1024] = { 0x00, };
   rawData ucOutRaw = { aucOutData, 0, sizeof(aucOutData) };

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE( "Calling: NFC_Get_Version" );
#endif
   ResponseCodes eResp = NFC_Get_Version(&ucOutRaw);
   if (eResp != EMB_APP_OK)
   {
      VIM_DBG_PRINTF1("ERROR: NFC_Get_Version = 0x%02X\r\n", eResp);
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_LABEL_STRING("NFC_Get_Version Successful", (VIM_TEXT *)ucOutRaw.buffer);
#endif

   return VIM_ERROR_NONE;
}

VIM_ERROR_PTR mal_nfc_terminal_ReadConfig(const char* pszWalletId)
{
   VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
   uint8_t aucOutData[4096];
   rawData sOutRaw = { aucOutData, 0, sizeof(aucOutData) };
   rawData sInWalletId;

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataString( &sInWalletId, pszWalletId==NULL?"":pszWalletId ) );

   VIM_DBG_PRINTF1( "Calling: NFC_TERMINAL_ReadConfig for '%s'", pszWalletId ? pszWalletId : "<NULL>" );
   VasStatus eResp = NFC_TERMINAL_ReadConfig( &sInWalletId, &sOutRaw);
   if (eResp == VAS_OK)
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_MESSAGE("NFC_TERMINAL_ReadConfig Successful");
#endif
   }
   else
   {
      VIM_DBG_PRINTF1("ERROR: NFC_TERMINAL_ReadConfig = 0x%02X\r\n", eResp);
      eRet = VIM_ERROR_OS;
   }

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_LABEL_STRING("NFC_TERMINAL_ReadConfig output", (VIM_TEXT *)sOutRaw.buffer);
#endif

   VIM_ERROR_RETURN(eRet);
}

VIM_ERROR_PTR mal_nfc_terminal_UpdateConfig(const char* pszJsonConfigTerminalFileName)
{
   uint8_t aucOutData[4096] = { 0x00, };
   rawData sOutRaw = { aucOutData, 0, sizeof(aucOutData) };
   rawData sInRaw;

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MED )
   VIM_DBG_PRINTF1( "mal_nfc_terminal_UpdateConfig reading from file '%s'", pszJsonConfigTerminalFileName );
#endif
   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataStringFromFile( &sInRaw, pszJsonConfigTerminalFileName ) );

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_LABEL_STRING( "InData", (VIM_TEXT *)sInRaw.buffer);
#endif
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_MESSAGE( "Calling: NFC_Terminal_Config" );
#endif
   VasStatus eResp = NFC_Terminal_Config(&sInRaw, &sOutRaw);
   if (eResp == VAS_OK)
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_MESSAGE("NFC_Terminal_Config Successful");
      VIM_DBG_LABEL_STRING("NFC_Terminal_Config output", (VIM_TEXT *)sOutRaw.buffer);
#endif
      return VIM_ERROR_NONE;
   }
   else
   {
      VIM_DBG_PRINTF1("ERROR: NFC_Terminal_Config = 0x%02X\r\n", eResp);
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
}

VIM_ERROR_PTR mal_nfc_vas_CancelConfig( const char* pszWalletId )
{
   rawData sInWalletId;

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataString( &sInWalletId, pszWalletId ) );

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_PRINTF1( "Calling: NFC_VAS_CancelConfig for '%s'", pszWalletId );
#endif
   VasStatus eResp = NFC_VAS_CancelConfig(&sInWalletId );
   if( eResp == VAS_OK )
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_MESSAGE( "NFC_VAS_CancelConfig Successful" );
#endif
      return VIM_ERROR_NONE;
   }
   else
   {
      VIM_DBG_PRINTF1( "ERROR: NFC_VAS_CancelConfig = 0x%02X\r\n", eResp );
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
}

VIM_ERROR_PTR mal_nfc_vas_ReadConfig( const char* pszWalletId )
{
   uint8_t aucOutData[4096] = { 0x00, };
   rawData sOutRaw = { aucOutData, 0, sizeof(aucOutData) };
   rawData sInWalletId;

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataString( &sInWalletId, pszWalletId ) );

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_PRINTF1( "Calling: NFC_VAS_ReadConfig for '%s'", pszWalletId );
#endif
   VasStatus eResp = NFC_VAS_ReadConfig(&sInWalletId, &sOutRaw);
   if (eResp == VAS_OK)
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_MESSAGE("NFC_VAS_ReadConfig Successful");
      VIM_DBG_LABEL_STRING("NFC_VAS_ReadConfig output", (VIM_TEXT *)sOutRaw.buffer);
#endif
      return VIM_ERROR_NONE;
   }
   else
   {
      VIM_DBG_PRINTF1("ERROR: NFC_VAS_ReadConfig = 0x%02X\r\n", eResp);
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
}

VIM_ERROR_PTR mal_nfc_vas_UpdateConfig( const char* pszWalletId, const char* pszJsonConfigVasFileName )
{
   uint8_t aucOutData[4096] = { 0x00, };
   rawData sOutRaw = { aucOutData, 0, sizeof(aucOutData) };
   rawData sInWalletId, sInRaw;

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_PRINTF1( "mal_nfc_vas_UpdateConfig reading from file '%s'", pszJsonConfigVasFileName );
#endif

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataString( &sInWalletId, pszWalletId ) );
   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataStringFromFile( &sInRaw, pszJsonConfigVasFileName ) );

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_LABEL_STRING( "InData", (VIM_TEXT *)sInRaw.buffer);
   VIM_DBG_PRINTF1( "Calling: NFC_VAS_UpdateConfig for '%s'", pszWalletId );
#endif
   VasStatus eResp = NFC_VAS_UpdateConfig(&sInWalletId, &sInRaw, &sOutRaw);
   if (eResp == VAS_OK)
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_MESSAGE("NFC_VAS_UpdateConfig Successful");
      VIM_DBG_LABEL_STRING("NFC_VAS_UpdateConfig output", (VIM_TEXT *)sOutRaw.buffer);
#endif
      return VIM_ERROR_NONE;
   }
   else
   {
      VIM_DBG_PRINTF1("ERROR: NFC_VAS_UpdateConfig = 0x%02X\r\n", eResp);
      VIM_ERROR_RETURN(VIM_ERROR_OS);
   }
}

static VIM_ERROR_PTR mal_nfc_vas_DecryptApplePayResponse( VasStatus eResp, rawData *psInWalletId, rawData *psOutRaw, char *pszLoyaltyCardNo, uint32_t ulLoyaltyCardNoBufferSize )
{
   uint8_t aucDecryptedData[1024] = { 0x00, };
   rawData sDecryptedRaw = { aucDecryptedData, 0, sizeof(aucDecryptedData) };

   VIM_ERROR_RETURN_ON_NULL( psInWalletId );
   VIM_ERROR_RETURN_ON_NULL( psOutRaw );
   VIM_ERROR_RETURN_ON_NULL( pszLoyaltyCardNo );

   memset( pszLoyaltyCardNo, 0, ulLoyaltyCardNoBufferSize );

   // NFC_VAS_Activate has to return VAS_OK_DECRYPT_REQ for ApplePay decryption to have been successful
   if( eResp != VAS_OK_DECRYPT_REQ )
   {
      VIM_ERROR_RETURN( VIM_ERROR_INVALID_DATA );
   }

   VasStatus eDecryptRes = NFC_VAS_Decrypt( psInWalletId, psOutRaw, &sDecryptedRaw );

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
   VIM_DBG_PRINTF1( "NFC_VAS_Decrypt ret %d", eDecryptRes );
   VIM_DBG_LABEL_STRING( "DecryptData", (VIM_TEXT *)sDecryptedRaw.buffer);
#endif

#undef SEARCH_STRING
#define SEARCH_STRING "VAS_Data\":\""

   // find "Vas_Data"
   char *pszVasData = strstr( (char *)sDecryptedRaw.buffer, SEARCH_STRING );
   if( pszVasData == NULL )
   {
      VIM_ERROR_RETURN( VIM_ERROR_INVALID_DATA );
   }

   char szDecryptedDataAsciiHex[256+1] = { 0, };
   char *pszOutPtr = szDecryptedDataAsciiHex;
   uint32_t ulLen = 0;
   uint32_t ulOutBufferLen = sizeof(szDecryptedDataAsciiHex)-1;

   for( char *pszCopyPtr = pszVasData + strlen(SEARCH_STRING); *pszCopyPtr != '\"' && ulLen<ulOutBufferLen; pszCopyPtr++ )
   {
      *pszOutPtr++ = *pszCopyPtr;
      ulLen++;
   }

   VIM_ERROR_RETURN_ON_FAIL( vim_utl_ascii_to_hex( (uint8_t *)pszLoyaltyCardNo, szDecryptedDataAsciiHex, ulLoyaltyCardNoBufferSize ) );
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
   VIM_DBG_PRINTF1( "ApplePay card data : [%s]", pszLoyaltyCardNo );
#endif

   return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR mal_nfc_vas_DecryptGooglePayResponse( rawData *psInWalletId, rawData *psOutRaw, char *pszLoyaltyCardNo, uint32_t ulLoyaltyCardNoBufferSize )
{
   VIM_ERROR_RETURN_ON_NULL( psInWalletId );
   VIM_ERROR_RETURN_ON_NULL( psOutRaw );
   VIM_ERROR_RETURN_ON_NULL( pszLoyaltyCardNo );

#undef SEARCH_STRING
#define SEARCH_STRING "Service_Number\":\""

   VIM_DBG_PRINTF1("Android Raw Data: %s", psOutRaw->buffer);

   memset( pszLoyaltyCardNo, 0, ulLoyaltyCardNoBufferSize );

   // find "Service_Number"
   char *pszVasData = strstr( (char *)psOutRaw->buffer, SEARCH_STRING );
   if( pszVasData == NULL )
   {
      VIM_ERROR_RETURN( VIM_ERROR_INVALID_DATA );
   }

   uint32_t ulLen = 0;

//#ifdef _DEBUG
   char *pszLoyaltyCardNoStart = pszLoyaltyCardNo;
//#endif

   for( char *pszCopyPtr = pszVasData + strlen(SEARCH_STRING); *pszCopyPtr != '\"' && ulLen<ulLoyaltyCardNoBufferSize; pszCopyPtr++ )
   {
      *pszLoyaltyCardNo++ = *pszCopyPtr;
      ulLen++;
   }

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
   VIM_DBG_PRINTF1( "GooglePay card data : [%s]", pszLoyaltyCardNoStart );
#endif

   return VIM_ERROR_NONE;
}

VIM_BOOL mal_nfc_CheckPendingPOSMessage(void)
{
	if( IsStandalone() )
	{
		VIM_KBD_CODE key_code;
		VIM_ERROR_PTR res;
		VIM_EVENT_FLAG_PTR events[1];

		res = vim_adkgui_kbd_touch_or_other_read(&key_code, 5, events, VIM_FALSE);
		if (res == VIM_ERROR_NONE)
		{
			VIM_DBG_MESSAGE("KKKKKKKKKKKKK Key Pressed KKKKKKKKKKKKK");
			return VIM_TRUE;
		}
	}
	else
	{
		return 0 < mal_nfc_GetPendingPOSMessageLen();
	}
	return VIM_FALSE;
}

VIM_UINT32 mal_nfc_GetPendingPOSMessageLen(void)
{
    int iBytesRemaining = -1;
    int iRet = -1;

    iRet = ioctl(s_iCommsFd, TIOCINQ, &iBytesRemaining);

    if ((0 == iRet) && (0 < iBytesRemaining))
    {
        VIM_DBG_PRINTF2("Pending POS data - TIOCINQ returned %d (%d bytes)", iRet, iBytesRemaining);
    }

    return iBytesRemaining;
}

VIM_INT32 mal_nfc_ReadPendingPOSMessage(VIM_UINT8 *pbBuf, VIM_UINT16 ushSize)
{
    VIM_INT32 lBytesRead = -1;
    if(!pbBuf)
    {
        VIM_DBG_MESSAGE("Null pointer")
        return -1;
    }

    lBytesRead = read(s_iCommsFd, pbBuf, ushSize);
    if(0 > lBytesRead)
    {
        VIM_DBG_PRINTF2("Read error %d,%d", read, errno);
    }
    else if(0 < lBytesRead)
    {
        VIM_DBG_LABEL_DATA("Read POS data", pbBuf, lBytesRead);
    }
    return lBytesRead;
}

static VIM_ERROR_PTR mal_nfc_vas_Activate( const char* pszWalletId, const char *pszDynamicConfigFileName )
{
    // RDD 091122 JIRA_PIRWAL_229 Issues when decrypt error seen without fail display.
   //VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
   VIM_ERROR_PTR eRet = VIM_ERROR_NOT_FOUND;
    uint8_t aucOutData[1024] = { 0x00, };
   rawData sOutRaw = { aucOutData, 0, sizeof(aucOutData) };
   rawData sInWalletId;
   rawData sInDynamicData;

   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataString( &sInWalletId, pszWalletId ) );

   if( pszDynamicConfigFileName != NULL )
   {
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_PRINTF1( "reading dynamic config from file '%s'", pszDynamicConfigFileName );
#endif
      VIM_ERROR_RETURN_ON_FAIL( mal_nfc_SetRawDataStringFromFile( &sInDynamicData, pszDynamicConfigFileName ) );
   }

   if (mal_nfc_CheckPendingPOSMessage())
   {
      VIM_DBG_MESSAGE("Returning early with POS CANCEL (1)");
      return VIM_ERROR_POS_CANCEL;
   }

#if( _NFC_DEBUG_LEVEL > _NFC_DEBUG_LEVEL_MIN )
   VIM_DBG_MESSAGE( "Calling: NFC_VAS_Activate " );
#endif

   // synchronous call. returns after 'PollTime' defined in json config. actual read has no overall user timeout, so keep re-establishing new read
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   //VIM_DBG_LABEL_STRING( "Dynamic data in", (VIM_TEXT *)sInDynamicData.buffer);
   //VIM_DBG_PRINTF1( "in sOutRaw buffer ptr = 0x%x", sOutRaw.buffer );
#endif
   VasStatus eResp = NFC_VAS_Activate(&sInWalletId, (pszDynamicConfigFileName!=NULL)?&sInDynamicData:NULL, &sOutRaw);

   if (eResp == VAS_TIME_OUT)
   {
       //VIM_DBG_MESSAGE("NFC_VAS_Activate: Timeout");        
       return VIM_ERROR_TIMEOUT;
   }


#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_PRINTF1( "out sOutRaw buffer ptr = 0x%x", sOutRaw.buffer );
#endif

   if (eResp == VAS_OK || eResp == VAS_OK_DECRYPT_REQ )
   {
      eRet = VIM_ERROR_NONE;

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_LABEL_STRING( "Out Raw", (VIM_TEXT *)sOutRaw.buffer);
#endif

      if( mal_nfc_vas_DecryptApplePayResponse( eResp, &sInWalletId, &sOutRaw, s_szLoyaltyCardNumber, sizeof(s_szLoyaltyCardNumber) ) == VIM_ERROR_NONE )
      {
         s_eSessionType = VIM_ICC_SESSION_APPLE_VAS;
		 pceft_debug_test(pceftpos_handle.instance, (char *)"IOS Phone Tapped: Sucess");

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
         VIM_DBG_MESSAGE("NFC_VAS_Activate decrypted ApplePay VAS Successful");
#endif
      }
      else if( mal_nfc_vas_DecryptGooglePayResponse( &sInWalletId, &sOutRaw, s_szLoyaltyCardNumber, sizeof(s_szLoyaltyCardNumber) ) == VIM_ERROR_NONE )
      {
         s_eSessionType = VIM_ICC_SESSION_ANDROID_VAS;
		 pceft_debug_test(pceftpos_handle.instance, (char*)"Android Phone Tapped: Sucess");
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
         VIM_DBG_MESSAGE("NFC_VAS_Activate decrypted GooglePay Successful");
#endif
      }
      else
      {
         s_eSessionType = VIM_ICC_SESSION_UNKNOWN;
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
         VIM_DBG_MESSAGE("NFC_VAS_Activate decryption failure");
#endif
         // RDD 090623 JIRA PIRPIN-2418
         return VIM_ERROR_NOT_FOUND;
      }
   }
   else if(VIM_ERROR_NOT_INTIALIZED != s_eEventResult)
   {
      eRet = s_eEventResult;
   }
   else
   {

#define DBG_VASSTATUS(val,err) case val: VIM_DBG_PRINTF1( "ERROR VasStatus returned: " #val " returned, value=%d", val ); eRet = err; break;

       if (eResp != VAS_TIME_OUT)
       {
           VIM_DBG_MESSAGE("in activate: Non timeout Error");
           VIM_DBG_PRINTF1("Device Tapped: Fail err=%d", eResp);
       }
       else
       {
           VIM_DBG_MESSAGE("in activate: Non timeout Error");
       }
      switch( eResp )
      {
         DBG_VASSTATUS(VAS_OK, VIM_ERROR_NONE)
         DBG_VASSTATUS(VAS_DO_PAY, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_FAIL, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_ERR_CTLS_DRIVER_CLOSE, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_CANCEL, VIM_ERROR_POS_CANCEL)    // FIXME Not so sure we should translate vas cancel to pos cancel.
         DBG_VASSTATUS(VAS_TIME_OUT, VIM_ERROR_TIMEOUT) // aka user timeout
         DBG_VASSTATUS(VAS_ERR_CONFIG, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_DUMMY_CALL, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_CANCEL_NOT_ALLOWED, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_CANCEL_IRRELEVANT, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_COMM_ERR, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_INIT_ERROR, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_DO_PAY_DECRYPT_REQ, VIM_ERROR_OS)
         DBG_VASSTATUS(VAS_OK_DECRYPT_REQ, VIM_ERROR_OS)
         default:
            eRet = VIM_ERROR_OS;
#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
            VIM_DBG_PRINTF1( "UNKNOWN VasStatus RETURNED. value=0x%02X", eResp );
#endif
            break;
      }

      if (mal_nfc_CheckPendingPOSMessage())
      {
         VIM_DBG_MESSAGE("Overwriting VAS return code with POS Cancel");
         eRet = VIM_ERROR_POS_CANCEL;
      }

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
      VIM_DBG_LABEL_STRING("jsonStreamOut", (VIM_TEXT *)sOutRaw.buffer);
#endif

   }

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MAX )
   VIM_DBG_LABEL_STRING("NFC_VAS_Activate output", (VIM_TEXT *)sOutRaw.buffer);
#endif

   return eRet;
}

void mal_nfc_SetCommsFd( int iCommsFd )
{
   s_iCommsFd = iCommsFd;
   VIM_DBG_PRINTF1( "mal_nfc_SetCommsFd: hdl = %d", s_iCommsFd );
}



static VIM_BOOL bBreakFromThread;


// JIRA PIRPIN_2165 Option to not use  threads if 1 sec timeout
void *mal_nfc_Thread_CancelOnRxPending( void *arg )
{
    if (s_iCommsFd >= 0)
    {
        // if main thread has terminated VAS read (e.g. phone has been tapped), then exit       
        if (!s_bEnableCancel)
        {
            VIM_DBG_MESSAGE("CANCEL THREAD - main thread NFC read completed");
            bBreakFromThread = VIM_TRUE;
        }
        // or if there's data waiting to be received (NOTE: doesn't actually READ the data from the rx queue)       
        else if (!bBreakFromThread && mal_nfc_CheckPendingPOSMessage())
        {
            s_eEventResult = VIM_ERROR_POS_CANCEL;
            bBreakFromThread = VIM_TRUE;
            VIM_DBG_MESSAGE("CANCEL THREAD - Break from NFC: POS event");
        }
        else if (s_bWallyEnabled)
        {
            VIM_ERROR_PTR eRet = Wally_SessionStateMachine();
            if (VIM_ERROR_NONE != eRet)
            {
                s_eEventResult = eRet;
                bBreakFromThread = VIM_TRUE;
                if (ERR_LOYALTY_CARD_SUCESSFUL == eRet)
                {
                    VIM_DBG_MESSAGE("CANCEL THREAD - Break from NFC: Wally rewards data was received");
                }
                else if (ERR_QR_RECV_SUCCESSFUL == eRet)
                {
                    VIM_DBG_MESSAGE("CANCEL THREAD - Break from NFC: Wally event detected");
                }
                else
                {
                    VIM_DBG_MESSAGE("CANCEL THREAD - Break from NFC: Wally error");
                }
            }
        }
    } 
    else   
    {      
        VIM_DBG_MESSAGE( "CANCEL THREAD - **** error opening UART handle, terminating thread ****" );   
    }   
    //VIM_DBG_MESSAGE( "CANCEL THREAD - cancel thread exiting" );   
    return NULL;
}


// RDD 260523 JIRA PIRPIN 2165 re-write and remove most of the useless code including all the pthread nonsense (which was crashing the tenminal)
VIM_ERROR_PTR mal_nfc_DoVasActivation
(
   const char *pszWalletId,
   const char *pszTerminalUpdateConfig,
   const char *pszVasUpdateConfig,
   const char *pszVasDynamicConfig,
   void *(* pfThreadFunc)(void *),
   VIM_BOOL bWallyEnabled
)
{
   static VIM_BOOL fFirstTime;

   VIM_ERROR_PTR eRet = VIM_ERROR_NONE;

   if( mal_nfc_CheckPendingPOSMessage())
   {
      VIM_DBG_MESSAGE("Returning early with POS CANCEL (2)");
      return VIM_ERROR_POS_CANCEL;
   }

   s_bWallyEnabled = bWallyEnabled;

   s_eEventResult = VIM_ERROR_NOT_INTIALIZED;
   vim_mem_clear( s_szLoyaltyCardNumber, VIM_SIZEOF(s_szLoyaltyCardNumber) );

   s_eSessionType = VIM_ICC_SESSION_UNKNOWN;

   VIM_ERROR_RETURN_ON_NULL( pszWalletId );
   VIM_ERROR_RETURN_ON_FAIL( mal_nfc_Open() );

   if (!fFirstTime)
   {
       fFirstTime = VIM_TRUE;
       if (pszTerminalUpdateConfig)
       {
           eRet = mal_nfc_terminal_UpdateConfig(pszTerminalUpdateConfig);
       }
       if (pszVasUpdateConfig)
       {
           mal_nfc_vas_UpdateConfig(pszWalletId, pszVasUpdateConfig);
       }
       VIM_DBG_MESSAGE("*** Configuration used for activation ***");
   }

   s_bEnableCancel = VIM_TRUE;   
   bBreakFromThread = VIM_FALSE;   
   mal_nfc_Thread_CancelOnRxPending(VIM_NULL);   
   if (bBreakFromThread)   
   {   
       VIM_DBG_MESSAGE("*** Cancel routine said to cancel ***");       
       VIM_ERROR_RETURN(VIM_ERROR_POS_CANCEL);       
   }   
   eRet = mal_nfc_vas_Activate(pszWalletId, pszVasDynamicConfig);   
   s_bEnableCancel = VIM_FALSE;
   mal_nfc_Close();
   VIM_ERROR_RETURN( eRet );
}

VIM_ERROR_PTR mal_nfc_GetVasCardNumber( char *pszDestination, VIM_SIZE ulBufferSize, VIM_ICC_SESSION_TYPE* peSessionType )
{
   VIM_ERROR_RETURN_ON_NULL( pszDestination );
   VIM_ERROR_RETURN_ON_NULL( peSessionType );

   // clear output
   vim_mem_clear( pszDestination, ulBufferSize );

   // copy saved card number
   VIM_ERROR_RETURN_ON_FAIL( vim_strncpy( pszDestination, s_szLoyaltyCardNumber, ulBufferSize ) );

   *peSessionType = s_eSessionType;
   
   VIM_DBG_VAR(*peSessionType);
   return VIM_ERROR_NONE;
}


typedef struct WAL_UART
{
   VIM_BOOL in_use;
   int os_handle;
   struct com_ConnectHandle *adkcom_handle;
   char port_name[20];    // to have ability to re-open port
   tcflag_t os_baud; // same
   VIM_UINT32 number_baud; // numeric representation of the baud rate (for ADKCOM)
} WAL_UART;

typedef struct WAL_LINKLY
{
  VIM_PCEFTPOS_PARAMETERS parameters;
  VIM_UART_PROTOCOL const* uart_protocol_methods;
  VIM_UART_HANDLE uart_handle;
  VIM_NET_HANDLE ethernet_handle;
  VIM_NET_HANDLE wifi_handle;
  VIM_NET_HANDLE tcp_handle;
} WAL_LINKLY;


// RDD 111220 added for JIRA PIRPIN-956 add session type to XML report post TXN - txn init post J8 kills original save
VIM_ICC_SESSION_TYPE mal_nfc_GetLastSessionType(void)
{
    VIM_DBG_VAR(s_eSessionType);
    return s_eSessionType;
}


VIM_ERROR_PTR mal_nfc_GetCommsFd( VIM_BOOL is_tcp, VIM_PCEFTPOS_HANDLE *pceftpos_handle, int *plCommsFd )
{
   WAL_LINKLY *psWalLinkly = NULL;

   VIM_ERROR_RETURN_ON_NULL( plCommsFd );
   VIM_ERROR_RETURN_ON_NULL( pceftpos_handle );

   *plCommsFd = -1;
   s_bIsTcp = is_tcp;

   psWalLinkly = (WAL_LINKLY *)pceftpos_handle->instance;

   if( is_tcp )
   {
      VIM_NET *psVimNet = NULL;

      VIM_DBG_MESSAGE( "*** tcp comms ***" );

      VIM_ERROR_RETURN_ON_NULL( psWalLinkly->tcp_handle.instance_ptr );

      psVimNet = psWalLinkly->tcp_handle.instance_ptr;

      VIM_ERROR_RETURN_ON_NULL(psVimNet->interface_handle.instance_ptr);

      *plCommsFd = psVimNet->interface_handle.instance_ptr->os_handle;
      VIM_DBG_PRINTF1( "*** TCP os handle = %d", *plCommsFd );
   }
   else
   {
      WAL_UART *psWalUart = NULL;
      enum com_ErrorCodes com_errno;

      VIM_DBG_MESSAGE( "*** uart comms ***" );

      psWalUart = (WAL_UART *)psWalLinkly->uart_handle.instance;

      VIM_ERROR_RETURN_ON_NULL( psWalUart );

      // use ADK com handle to get OS fd value. check it's not null first (shouldn't be)
      VIM_ERROR_RETURN_ON_NULL( psWalUart->adkcom_handle );

      // get the OS fd value
      *plCommsFd = com_ConnectGetFD( psWalUart->adkcom_handle, &com_errno );
      VIM_DBG_PRINTF1( "*** uart os handle = %d", *plCommsFd );
   }

   if( *plCommsFd == -1 )
   {
      // error, handle not found
      VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
   }

#if( _NFC_DEBUG_LEVEL >= _NFC_DEBUG_LEVEL_MIN )
   VIM_DBG_PRINTF1( "mal_nfc_GetCommsFd found comms fd = %d", *plCommsFd );
#endif

   return VIM_ERROR_NONE;
}

VIM_BOOL mal_nfc_IsJ8_active( void )
{
    return s_bEnableCancel;
}

