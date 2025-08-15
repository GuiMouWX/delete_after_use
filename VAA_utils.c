#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
#include "incs.h"
#include "VAA_Utils.h"


#define VAA_DEBUG_FILE_PATH  VIM_FILE_PATH_LOCAL "VAA_DEBUG.log"      // Avoid all the VIM warnings ....
#define VAA_DEBUG_LINUX_PATH  "flash/VAA_DEBUG.log"      // Ugh

#define MIN_LINE_LEN 5  
#define MAX_VAA_DEBUG_LINES 50


static VIM_SIZE VAA_GetFileSize(const char* pszFilename)
{
	VIM_SIZE size = 0;
	vim_file_get_size(pszFilename, VIM_NULL, &size);	
    return size;
}



VIM_BOOL VAA_CheckDebug(void)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    VIM_SIZE ulFileSize = VAA_GetFileSize(VAA_DEBUG_FILE_PATH);

    if (0 < ulFileSize)
    {
        VIM_DBG_PRINTF1("!!!!!!!!!!!!!!!!! Pending debug data: %d bytes", ulFileSize );
        FILE* fp = fopen(VAA_DEBUG_LINUX_PATH, "r");
        if (fp)
        {
            VIM_CHAR *pszMalloc = NULL;
            VIM_INT16 Count = MAX_VAA_DEBUG_LINES;   // Avoid debug SPAM
            size_t ulLen = 0;

            //Lock the debug file (thread safety)
            flockfile(fp);
            while( (getline(&pszMalloc, &ulLen, fp) != -1) && Count-- )
            {
                VIM_CHAR* pszLine = pszMalloc;
                ulLen = vim_strlen(pszLine);
                if (pszLine && (ulLen > MIN_LINE_LEN))
                {
                    VIM_BOOL fImportant = VIM_FALSE;
                    VIM_INT8 vaa_id = 0;

                    VAA_StringTrimEnd(pszLine);

                    // RDD - add support to get Z1, Z2 etc. debug so we can actually tell which VAA is sending it ....
                    if (*pszLine == '!')
                    {
                        fImportant = VIM_TRUE;
                        pszLine++;
                    }

                    vaa_id = *pszLine - '0';
                    pszLine++;

                    if (fImportant)
                    {
                        pceft_debug_vaa(vaa_id, pszLine);
                    }
                    else
                    {
                        pceft_debug_vaa_test(vaa_id, pszLine);
                    }
                }
                else
                {
                    DBG_MESSAGE("Invalid VAA Z debug line");
                }
                FREE_MALLOC(pszMalloc);
            }

            //Clear file
            fp = freopen(VAA_DEBUG_LINUX_PATH, "w", fp);
            fflush(fp);
            //Unlock the debug file (thread safety)
            funlockfile(fp);
            fclose(fp);
        }
        else // RDD - Causes debug spam as this is called very frequently. Woolies would have to pay by the mB to have their EFT logs being filled with this garbage.
        {
			VIM_DBG_PRINTF1("!!!!!!!!!!!!!!!!! Unable to open debug data: %d bytes", ulFileSize);
			DBG_POINT;
        }
    }

    return fSuccess;
}


static VIM_BOOL VAA_SanitiseItemMiddle(cJSON* pCJ, const char* pszKey, VIM_UINT32 ulLeaveFirst, VIM_UINT32 ulLeaveLast)
{
    VIM_BOOL fSanitised = VIM_TRUE;

    cJSON* pCJItem = cJSON_GetObjectItem(pCJ, pszKey);
    const char* pszValue = cJSON_GetStringValue(pCJItem);
    VIM_UINT32 ulLen = pszValue ? vim_strlen(pszValue) : 0;

    if ((ulLeaveFirst + ulLeaveLast) < ulLen)
    {
        char szMasked[128 + 1] = { 0 };
        vim_strncpy(szMasked, pszValue, sizeof(szMasked) - 1);
        vim_mem_set(&szMasked[ulLeaveFirst], '*', ulLen - ulLeaveFirst - ulLeaveLast);
        fSanitised = (NULL != cJSON_SetValuestring(pCJItem, szMasked));
    }

    return fSanitised;
}

static VIM_BOOL VAA_SanitiseItem(cJSON* pCJ, const char* pszKey)
{
    return VAA_SanitiseItemMiddle(pCJ, pszKey, 0, 0);
}

static cJSON* VAA_DuplicateSanitisedJson(const cJSON* psCJ)
{
    cJSON* psCJSanitised = cJSON_Duplicate(psCJ, VIM_TRUE);

    if (QC_IsInstalled(VIM_FALSE))
    {
        //Qwikcilver Sensitive Fields
        cJSON* pCJRequestCard  = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(psCJSanitised, "request" ), "Cards"), 0);
        cJSON* pCJResponseCard = cJSON_GetArrayItem(cJSON_GetObjectItem(cJSON_GetObjectItem(psCJSanitised, "response"), "Cards"), 0);

        VAA_SanitiseItem(pCJRequestCard,  "CardPin");
        VAA_SanitiseItem(pCJResponseCard, "CardPin");

        VAA_SanitiseItemMiddle(pCJRequestCard,  "CardNumber", 6, 4);
        VAA_SanitiseItemMiddle(pCJResponseCard, "CardNumber", 6, 4);
    }

    //Add other fields to be sanitised for debug

    return psCJSanitised;
}

static void VAA_DebugErrorListJson(const cJSON* psCJ, const char* pszLabel)
{
    cJSON* psCJ_errorlist = cJSON_GetObjectItem(cJSON_GetObjectItem(psCJ, "errors"), "errorlist");
    VIM_UINT32 ulIdx, ulCount = cJSON_GetArraySize(psCJ_errorlist);
    for (ulIdx = 0; ulIdx < ulCount; ++ulIdx)
    {
        cJSON* psCJ_error = cJSON_GetArrayItem(psCJ_errorlist, ulIdx);
        char szLabel[64 + 1] = { 0 };
        snprintf(szLabel, sizeof(szLabel), "Received %s Error (%d/%d): ", SAFE_STR(pszLabel), ulIdx + 1, ulCount);
        VAA_DebugJson(szLabel, psCJ_error);
    }
}

void VAA_DebugJson(const VIM_CHAR* pszLabel, const cJSON* psCJ)
{
    cJSON* psCJSanitised = VAA_DuplicateSanitisedJson(psCJ);
    if (psCJSanitised)
    {
        VIM_CHAR* pszPrint = cJSON_PrintUnformatted(psCJSanitised);
        FREE_JSON(psCJSanitised);

        if (pszPrint)
        {
            if (pszLabel)
            {
                VIM_SIZE ulMallocSize = vim_strlen(pszLabel) + 2 + vim_strlen(pszPrint) + 1;
                VIM_CHAR* pszOut = (VIM_CHAR*)malloc(ulMallocSize);
                if (pszOut)
                {
                    snprintf(pszOut, ulMallocSize, "%s: %s", pszLabel, pszPrint);
                    VIM_DBG_MESSAGE(pszOut);
                }
                else
                {
                    pceft_debug(pceftpos_handle.instance, "Failed to malloc JSON debug buffer");
                }
                FREE_MALLOC(pszOut);
            }
            else
            {
                VIM_DBG_MESSAGE(pszPrint);
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Failed to create JSON string");
        }
        FREE_MALLOC(pszPrint);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to get sanitised JSON");
    }
}

VIM_BOOL VAA_WriteJSONRequestFile(const VIM_CHAR* pszRequestFileName, const cJSON* pCJRequest)
{
    VIM_BOOL fSuccess = VIM_FALSE;

    if (pszRequestFileName)
    {
        if (pCJRequest)
        {
            VIM_CHAR* pszFileData = cJSON_PrintUnformatted(pCJRequest);
            if (pszFileData)
            {
                VIM_SIZE ulBytesToWrite = vim_strlen(pszFileData);

                //pceft_debugf_test(pceftpos_handle.instance, "Writing %s file: %s", pszRequestFileName, pszFileData);

                if( vim_file_set( pszRequestFileName, pszFileData, ulBytesToWrite ) == VIM_ERROR_NONE )
                {
                    //VAA_DebugJson(pszRequestFileName, pCJRequest);
                    fSuccess = VIM_TRUE;
                }
                else
                {
                    pceft_debugf(pceftpos_handle.instance, "Failed to write %s request. Expected %u bytes to be written", pszRequestFileName, ulBytesToWrite);
                }
            }
            else
            {
                pceft_debug(pceftpos_handle.instance, "Failed to convert JSON to string");
            }
            FREE_MALLOC(pszFileData);
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "No JSON request to send");
        }
    }

    if (!fSuccess)
    {
        VAA_DeleteFile(pszRequestFileName);
    }

    return fSuccess;
}


cJSON* VAA_ReadJSONFile(const VIM_CHAR* pszResponseFileName)
{
	cJSON* pCJResponse = NULL;
	VIM_ERROR_PTR res;
	VIM_UINT32 ulFileSize = 0;
	VIM_CHAR pszResponseBuffer[5000];

	vim_mem_clear(pszResponseBuffer, VIM_SIZEOF(pszResponseBuffer));
	if ((res = vim_file_get(pszResponseFileName, (void *)pszResponseBuffer, &ulFileSize, VIM_SIZEOF(pszResponseBuffer))) == VIM_ERROR_NONE)
	{
		VIM_DBG_PRINTF2("Read %s file:", pszResponseFileName, pszResponseBuffer);
		VIM_DBG_STRING(pszResponseBuffer);
		pCJResponse = cJSON_Parse(pszResponseBuffer);
		VAA_DebugErrorListJson(pCJResponse, pszResponseFileName);
	}
	return pCJResponse;
}

// RDD 100223 JIRA PIRPIN-2218 v734-4 QC Double Debit Issue - caused by breakout of wait loop

#if 0
cJSON* VAA_WaitForJSONResponse(const VIM_CHAR* pszResponseFileName, VIM_UINT32 ulTimeoutS)
{
	cJSON* pCJResponse = NULL;
	VIM_RTC_UPTIME start, now, end;
	VIM_BOOL exists = VIM_FALSE;
	VIM_SIZE len = 0;

	VIM_DBG_PRINTF2("-------- Waiting for %s Response: %d Secs -------", pszResponseFileName, ulTimeoutS);

	vim_rtc_get_uptime(&start);
	end = start + (ulTimeoutS * 1000);

	VIM_DBG_NUMBER(ulTimeoutS);
	vim_event_sleep(100);
	do
	{
		vim_rtc_get_uptime(&now);
		{
			vim_file_exists(pszResponseFileName, &exists);
			//VIM_DBG_VAR(exists);
		}
		if (exists)
		{
			VIM_DBG_PRINTF1("-------- File Written After %d mS -------", now-start);

			vim_file_get_size(pszResponseFileName, VIM_NULL, &len);
			VIM_DBG_PRINTF2("--------Reading %s [%d bytes] -------", pszResponseFileName, len);
			pCJResponse = VAA_ReadJSONFile(pszResponseFileName);
			if (pCJResponse)
			{
				VIM_DBG_PRINTF1("-------- Got response After %d mS -------", now-start);
			}
			else
			{
				VIM_DBG_PRINTF1("-------- Read Error After %d mS -------", now-start);
			}
			return pCJResponse;
			break;
		}
		else
		{
			VAA_CheckDebug();
			vim_event_sleep(100);
			//continue;
		}

		if ((IS_R10_POS)||(vim_file_is_present("rtm_security_token")))
		{
			if (QC_CheckForAuthChallengeRequest() != VIM_ERROR_NONE)       //  RDD - this should not be here in a supposedly generic function - TODO : move to somewhere more targeted.
			{
				VIM_DBG_PRINTF1("-------- Fatal Error After %d mS -------", now-start);
				break;
			}
		}

	} while (now < end);

	VIM_DBG_MESSAGE("-------- Timed Out or Error Waiting for Resp -------");
	return NULL;
}
#else
cJSON* VAA_WaitForJSONResponse(const VIM_CHAR* pszResponseFileName, VIM_UINT32 ulTimeoutS, VIM_BOOL QwikCilver )
{
    cJSON* pCJResponse = NULL;
    VIM_RTC_UPTIME now, end;

    VIM_DBG_PRINTF1("-------- Waiting for Response from QC VAA: %d Secs -------", ulTimeoutS);

    vim_rtc_get_uptime(&end);
    end += ulTimeoutS * 1000;

    VIM_DBG_NUMBER(ulTimeoutS);
    do
    {
        vim_rtc_get_uptime(&now);

        pCJResponse = VAA_ReadJSONFile(pszResponseFileName);

        if (QwikCilver)
        {
            QC_CheckForAuthChallengeRequest();
        }
        VAA_CheckDebug();
        vim_event_sleep(100);

        if (pCJResponse)
        {
            VIM_DBG_PRINTF1("-------- Got response After %d mS -------", end - now);
            return pCJResponse;
        }

    } while (now < end);

    pceft_debug(pceftpos_handle.instance, "Timed Out Waiting for Resp from QC");
    return pCJResponse;
}
#endif

void VAA_DeleteFile(const VIM_CHAR* pszFileName)
{
    pceft_debugf_test(pceftpos_handle.instance, "Deleting %s", SAFE_STR(pszFileName));
    if(pszFileName)
    {
        vim_file_delete(pszFileName);
    }
}

VIM_CHAR* VAA_GenerateUUID(void)
{
    static VIM_CHAR szUUID[37] = { 0 };     // MUST be 'static', we are sharing its address with the caller.
    uuid_t binuuid;

    // From stack-overflow (modified): https://stackoverflow.com/questions/51053568/generating-a-random-uuid-in-c

    vim_mem_clear(szUUID, VIM_SIZEOF(szUUID));

    //Generate a UUID. We're not done yet, though,
    //for the UUID generated is in binary format
    //(hence the variable name). We must 'unparse'
    //binuuid to get a usable 36-character string.

    uuid_generate_random(binuuid);

    // Produces a UUID string at uuid consisting of lower-case letters.
    uuid_unparse_lower(binuuid, szUUID);

    VIM_DBG_PRINTF1("UUID = %s", szUUID);

    return szUUID;
}

VIM_RTC_UPTIME VAA_GetTimer(VIM_UINT32 ulTimeoutInSecs)
{
    VIM_RTC_UPTIME now;

    vim_rtc_get_uptime(&now);

    return (now + (ulTimeoutInSecs * 1000));
}

VIM_BOOL VAA_IsTimerExpired(VIM_RTC_UPTIME ullTimer)
{
    VIM_RTC_UPTIME now;

    vim_rtc_get_uptime(&now);
    return ((ullTimer != 0) && (now > ullTimer));
}

const VIM_CHAR* VAA_GetLaneID(void)
{
    static VIM_CHAR s_szLaneID[3 + 1] = { 0 };
    vim_mem_clear(s_szLaneID, VIM_SIZEOF(s_szLaneID));

    if (8 > vim_strlen(terminal.terminal_id))
    {
        VIM_DBG_MESSAGE("TID too short!");    // Should not happen
        return NULL;
    }

    vim_strncpy(s_szLaneID, &terminal.terminal_id[5], 3);
    VIM_DBG_PRINTF1("Lane ID = [%s]", s_szLaneID);

    return s_szLaneID;
}

const VIM_CHAR* VAA_GetStoreID(void)
{
    static VIM_CHAR s_szStoreID[4 + 1] = { 0 };
    vim_mem_clear(s_szStoreID, VIM_SIZEOF(s_szStoreID));

    if (8 > vim_strlen(terminal.terminal_id))
    {
        VIM_DBG_MESSAGE("TID too short!");    // Should not happen
        return NULL;
    }

    vim_strncpy(s_szStoreID, &terminal.terminal_id[1], 4);
    VIM_DBG_PRINTF1("Store ID = [%s]", s_szStoreID);

    return s_szStoreID;
}

const VIM_CHAR* VAA_GetTerminalID(void)
{
    static VIM_CHAR s_szTerminalId[8 + 1] = { 0 };
    vim_mem_clear(s_szTerminalId, VIM_SIZEOF(s_szTerminalId));

    if (8 > vim_strlen(terminal.terminal_id))
    {
        VIM_DBG_MESSAGE("TID too short!");    // Should not happen
        return NULL;
    }

    vim_strncpy(s_szTerminalId, terminal.terminal_id, 8);
    VIM_DBG_PRINTF1("Terminal ID = [%s]", s_szTerminalId);

    return s_szTerminalId;
}

const VIM_CHAR* VAA_GetLocation(void)
{
    static const VIM_CHAR* s_pszNoLocation = "<No Location Configured>";
    return VAA_IsNullOrEmpty(TERM_QR_LOCATION) ? s_pszNoLocation : TERM_QR_LOCATION;
}

VIM_BOOL VAA_GetUUIDWithHyphens(const VIM_CHAR* pszUUID, VIM_CHAR* pszOutUUIDWithHyphens, VIM_SIZE ulOutSize)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (pszUUID && pszOutUUIDWithHyphens)
    {
        vim_mem_clear(pszOutUUIDWithHyphens, ulOutSize);
        if (UUID_LEN < ulOutSize)
        {
            //Ensure we don't double up our hyphens, by ensuring they've been removed from the input
            VIM_CHAR szNoHyphens[UUID_NO_HYPHEN_LEN + 1] = { 0 };
            fSuccess = VAA_GetUUIDNoHyphens(pszUUID, szNoHyphens, sizeof(szNoHyphens));
            if (fSuccess)
            {
                VIM_CHAR* pszOut = pszOutUUIDWithHyphens;
                VIM_CHAR* pszIn  = szNoHyphens;

#define COPY_UUID_DATA(out, in, len) do { vim_mem_copy(out, in, len); out += len; in += len; } while (0)
                COPY_UUID_DATA(pszOut, pszIn, 8);
                *pszOut++ = '-';
                COPY_UUID_DATA(pszOut, pszIn, 4);
                *pszOut++ = '-';
                COPY_UUID_DATA(pszOut, pszIn, 4);
                *pszOut++ = '-';
                COPY_UUID_DATA(pszOut, pszIn, 4);
                *pszOut++ = '-';
                COPY_UUID_DATA(pszOut, pszIn, 12);
#undef COPY_UUID_DATA

                VIM_DBG_PRINTF4("Add UUID Hyphens: '%s'(%u) -> '%s'(%u)", pszUUID, vim_strlen(pszUUID), pszOutUUIDWithHyphens, vim_strlen(pszOutUUIDWithHyphens))
                fSuccess = (UUID_LEN == vim_strlen(pszOutUUIDWithHyphens));
                if (!fSuccess)
                {
                    pceft_debugf(pceftpos_handle.instance, "Unexpected GUID output length (got %u, expected %u)", vim_strlen(pszOutUUIDWithHyphens), UUID_LEN);
                }
            }
            else
            {
                VIM_DBG_PRINTF1("Failed Initial attempt to remove hyphens from '%s'", pszUUID);
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "UUID convert Overflow");
        }
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Null parameter");
    }
    return fSuccess;
}

VIM_BOOL VAA_GetUUIDNoHyphens(const VIM_CHAR* pszUUID, VIM_CHAR* pszOutUUIDNoHyphens, VIM_SIZE ulOutSize)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (pszUUID && pszOutUUIDNoHyphens)
    {
        vim_mem_clear(pszOutUUIDNoHyphens, ulOutSize);
        if (UUID_NO_HYPHEN_LEN < ulOutSize)
        {
            VIM_CHAR* pszOut = pszOutUUIDNoHyphens;
            VIM_SIZE i = 0, written = 0;
            VIM_SIZE ulInLen = vim_strlen(pszUUID);
            for (i = 0; (i < ulInLen) && (written < ulOutSize); ++i)
            {
                VIM_CHAR c = pszUUID[i];
                if (c == '-')
                {
                    //Skip Hyphens
                }
                else
                {
                    *pszOut++ = c;
                    written++;
                }
            }
            VIM_DBG_PRINTF4("Remove UUID Hyphens: '%s'(%u) -> '%s'(%u)", pszUUID, vim_strlen(pszUUID), pszOutUUIDNoHyphens, vim_strlen(pszOutUUIDNoHyphens))
            fSuccess = (UUID_NO_HYPHEN_LEN == vim_strlen(pszOutUUIDNoHyphens));
            if (!fSuccess)
            {
                pceft_debugf(pceftpos_handle.instance, "Unexpected GUID output length (got %u, expected %u)", vim_strlen(pszOutUUIDNoHyphens), UUID_NO_HYPHEN_LEN);
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "UUID convert Overflow");
        }
    }
    return fSuccess;
}

VIM_BOOL VAA_IsWhiteSpace(VIM_CHAR c)
{
    switch (c)
    {
    case '\r':
    case '\n':
    case ' ':
    case '\t':
        return VIM_TRUE;
    }
    return VIM_FALSE;
}

VIM_CHAR* VAA_StringTrimEnd(VIM_CHAR* pszString)
{
    if (pszString)
    {
        VIM_SIZE ulLen = vim_strlen(pszString);
        while (0 < ulLen && VAA_IsWhiteSpace(pszString[ulLen - 1]))
        {
            pszString[--ulLen] = '\0';
        }
    }
    return pszString;
}

VIM_BOOL VAA_IsNullOrEmpty(const VIM_CHAR* pszString)
{
    return !pszString || ('\0' == *pszString);
}

VIM_BOOL VAA_IsJsonNullOrEmpty(const cJSON* pCJ)
{
    if (!pCJ)
        return VIM_TRUE;

    if (cJSON_IsNull(pCJ) || cJSON_IsInvalid(pCJ))
        return VIM_TRUE;

    if (cJSON_IsArray(pCJ) || cJSON_IsObject(pCJ))
        return (0 >= cJSON_GetArraySize(pCJ));

    if (cJSON_IsString(pCJ) || cJSON_IsRaw(pCJ))
        return VAA_IsNullOrEmpty(cJSON_GetStringValue(pCJ));

    //Assume other types are not empty (i.e. bool, number)

    return VIM_FALSE;
}

VIM_BOOL VAA_AddBool(cJSON* psCJ, const VIM_CHAR* pszLabel, VIM_BOOL fBool, VIM_BOOL fMandatory)
{
    if (!psCJ || VAA_IsNullOrEmpty(pszLabel))
    {
        VIM_DBG_PRINTF4("%s util_AddBool(%p, %s, %s)", fMandatory ? "FAILED mandatory" : "Skipping", psCJ, SAFE_STR(pszLabel), BOOL_STR(fBool));
        return !fMandatory;
    }
    return (NULL != cJSON_AddBoolToObject(psCJ, pszLabel, fBool));
}

VIM_BOOL VAA_AddString(cJSON* psCJ, const VIM_CHAR* pszLabel, const VIM_CHAR* pszString, VIM_BOOL fMandatory)
{
    if (!psCJ || VAA_IsNullOrEmpty(pszLabel) || VAA_IsNullOrEmpty(pszString))
    {
        VIM_DBG_PRINTF4("%s QC_AddString(%p, %s, %s)", fMandatory ? "FAILED mandatory" : "Skipping", psCJ, SAFE_STR(pszLabel), SAFE_STR(pszString));
        return !fMandatory;
    }
    return (NULL != cJSON_AddStringToObject(psCJ, pszLabel, pszString));
}

VIM_BOOL VAA_AddNumber(cJSON* psCJ, const VIM_CHAR* pszLabel, double dNumber, VIM_BOOL fMandatory)
{
    if (!psCJ || VAA_IsNullOrEmpty(pszLabel))
    {
        VIM_DBG_PRINTF4("%s QC_AddNumber(%p, %s, %f)", fMandatory ? "FAILED mandatory" : "Skipping", psCJ, SAFE_STR(pszLabel), dNumber);
        return !fMandatory;
    }
    return (NULL != cJSON_AddNumberToObject(psCJ, pszLabel, dNumber));
}

VIM_BOOL VAA_AddItem(cJSON* psCJ, const VIM_CHAR* pszLabel, cJSON* psCJItem, VIM_BOOL fMandatory)
{
    if (!psCJ || VAA_IsNullOrEmpty(pszLabel) || !psCJItem)
    {
        VIM_DBG_PRINTF4("%s QC_AddItem(%p, %s, %p)", fMandatory ? "FAILED mandatory" : "Skipping", psCJ, SAFE_STR(pszLabel), psCJItem);
        return !fMandatory;
    }
    return (VIM_FALSE != cJSON_AddItemToObject(psCJ, pszLabel, psCJItem));
}

VIM_BOOL VAA_AddIntAsString(cJSON* psCJ, const VIM_CHAR* pszLabel, VIM_INT32 lNumber, VIM_BOOL fMandatory)
{
    VIM_CHAR szNumber[40 + 1] = { 0 };
    if (!psCJ || VAA_IsNullOrEmpty(pszLabel))
    {
        VIM_DBG_PRINTF4("%s QC_AddNumber(%p, %s, %d)", fMandatory ? "FAILED mandatory" : "Skipping", psCJ, SAFE_STR(pszLabel), lNumber);
        return !fMandatory;
    }
    snprintf(szNumber, sizeof(szNumber), "%d", lNumber);
    return VAA_AddString(psCJ, pszLabel, szNumber, fMandatory);
}

VIM_BOOL VAA_CopyCjsonItemRename(cJSON* pCJDest, const VIM_CHAR* pszDestKey, const cJSON* pCJSrc, const VIM_CHAR* pszSrcKey, VIM_BOOL fMandatory)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (pszDestKey && pCJDest && pszSrcKey && pCJSrc)
    {
        cJSON* pCJItem = cJSON_GetObjectItem(pCJSrc, pszSrcKey);
        if (pCJItem)
        {
            cJSON* pCJCopy = cJSON_Duplicate(pCJItem, VIM_TRUE);
            fSuccess = cJSON_AddItemToObject(pCJDest, pszDestKey, pCJCopy);
            if (!fSuccess)
            {
                FREE_JSON(pCJCopy);
            }
        }
        else
        {
            VIM_DBG_PRINTF1("Item '%s' missing from source JSON object", pszSrcKey);
        }
    }
    else
    {
        VIM_DBG_PRINTF4("Null parameter(%s,%s,%s,%s)", SAFE_STR(pCJDest?"dest":NULL), SAFE_STR(pszDestKey), SAFE_STR(pCJSrc ?"src":NULL), SAFE_STR(pszSrcKey));
    }
    return (!fMandatory || fSuccess);
}

VIM_BOOL VAA_CopyCjsonItem(cJSON* pCJDest, const VIM_CHAR* pszKey, const cJSON* pCJSrc, VIM_BOOL fMandatory)
{
    return VAA_CopyCjsonItemRename(pCJDest, pszKey, pCJSrc, pszKey, fMandatory);
}

const VIM_CHAR* VAA_GetJsonStringValue(const cJSON* pCJ, const VIM_CHAR* pszKey)
{
    return (const VIM_CHAR*)cJSON_GetStringValue(cJSON_GetObjectItem(pCJ, pszKey));
}

VIM_BOOL VAA_AppendToPAD(const VIM_CHAR* pszValue, VIM_CHAR* pszPADBuf, VIM_UINT32 ulBufSize, const VIM_CHAR* pszPADTag)
{
    VIM_BOOL fSuccess = VIM_FALSE;

    if (pszValue && pszPADBuf && pszPADTag)
    {
        if (3 == vim_strlen(pszPADTag))
        {
            VIM_SIZE ulValueLen = vim_strlen(pszValue);
            if (0 < ulValueLen)
            {
                VIM_SIZE ulExistingLen = vim_strlen(pszPADBuf);
                VIM_CHAR szNewPAD[256 + 1] = { 0 };
                snprintf(szNewPAD, sizeof(szNewPAD), "%s%3.3d%s", pszPADTag, ulValueLen, pszValue);
                if ((ulExistingLen + vim_strlen(szNewPAD)) < ulBufSize)
                {
                    vim_snprintf(&pszPADBuf[ulExistingLen], ulBufSize - ulExistingLen, "%s", szNewPAD);
                    VIM_DBG_PRINTF1("Added PAD tag '%s'", szNewPAD);
                    fSuccess = VIM_TRUE;
                }
                else
                {
                    pceft_debugf(pceftpos_handle.instance, "PAD data overflow, discarding tag '%s'", szNewPAD);
                }
            }
            else
            {
                VIM_DBG_PRINTF1("Skipping 0 byte pad tag '%s'", pszPADTag);
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Invalid PAD Tag length");
        }
    }

    return fSuccess;
}

VIM_BOOL VAA_AppendJsonStringToPAD(const cJSON* pCJ, const VIM_CHAR* pszJsonKey, VIM_CHAR* pszPADBuf, VIM_UINT32 ulBufSize, const VIM_CHAR* pszPADTag)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (pCJ && pszJsonKey)
    {
        VIM_CHAR szValue[50 + 1] = { 0 };
        fSuccess = cJSON_GetStringItemValue(pCJ, pszJsonKey, szValue, sizeof(szValue));
        if (fSuccess)
        {
            fSuccess = VAA_AppendToPAD(szValue, pszPADBuf, ulBufSize, pszPADTag);
        }
        else
        {
            VIM_DBG_PRINTF2("Skipping PAD tag '%s'; VAA string '%s' not found", SAFE_STR(pszPADTag), pszJsonKey);
        }
    }
    else
    {
        VIM_DBG_PRINTF2("Skipping PAD tag '%s'; parent of '%s' missing", SAFE_STR(pszPADTag), SAFE_STR(pszJsonKey));
    }
    return fSuccess;
}

VIM_BOOL VAA_AppendJsonNumberToPAD(const cJSON* pCJ, const VIM_CHAR* pszJsonKey, VIM_CHAR* pszPADBuf, VIM_UINT32 ulBufSize, const VIM_CHAR* pszPADTag, VIM_SIZE ulDecimalPlaces)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (pCJ && pszJsonKey)
    {
        double dValue = 0;
        fSuccess = cJSON_GetNumberItemValue(pCJ, pszJsonKey, &dValue);
        if (fSuccess)
        {
            char szFormat[16 + 1] = { 0 };
            char szValue[50 + 1] = { 0 };

            if (0 < ulDecimalPlaces)
                vim_snprintf(szFormat, sizeof(szFormat), "%%.%uf", ulDecimalPlaces);
            else
                vim_strncpy(szFormat, "%g", sizeof(szFormat) - 1);

            snprintf(szValue, sizeof(szValue), szFormat, dValue);
            fSuccess = VAA_AppendToPAD(szValue, pszPADBuf, ulBufSize, pszPADTag);
        }
        else
        {
            VIM_DBG_PRINTF2("Skipping PAD tag '%s'; VAA number '%s' not found", SAFE_STR(pszPADTag), pszJsonKey);
        }
    }
    else
    {
        VIM_DBG_PRINTF2("Skipping PAD tag '%s'; parent of '%s' missing", SAFE_STR(pszPADTag), SAFE_STR(pszJsonKey));
    }
    return fSuccess;
}

VIM_BOOL VAA_FileExists(const VIM_CHAR* pszFilename)
{
    return (pszFilename && (0 == access(pszFilename, F_OK)));
}

VIM_BOOL VAA_ValidatePngFile(const VIM_CHAR* pszFilename, VIM_UINT32* pulWidthPx, VIM_UINT32* pulHeightPx)
{
    VIM_BOOL fValid = VIM_FALSE;

    if (pulWidthPx) *pulWidthPx = 0;
    if (pulHeightPx) *pulHeightPx = 0;

    if (VAA_FileExists(pszFilename))
    {
		VIM_DBG_MESSAGE("Found Session QR Code");
        VIM_UINT8 aucFileHeader[24 + 1] = { 0 };
        if( vim_file_get_bytes( pszFilename, aucFileHeader, sizeof(aucFileHeader), 0) == VIM_ERROR_NONE )
        {
            const char* pszPngMagicString = "\x89PNG\x0D\x0A\x1A\x0A";
            const char* pszImageHeaderTag = "IHDR";
            //Check Magic string
            fValid = VIM_ERROR_NONE == vim_mem_compare(aucFileHeader, pszPngMagicString, 8);
            //Check Header tag
            fValid &= VIM_ERROR_NONE == vim_mem_compare(&aucFileHeader[12], pszImageHeaderTag, 4);
            if (fValid)
            {
                VIM_UINT32 ulWidth = 0, ulHeight = 0;
                //Extract Width and Height from PNG header
                ulWidth = aucFileHeader[16] << 24
                    | aucFileHeader[17] << 16
                    | aucFileHeader[18] << 8
                    | aucFileHeader[19];
                ulHeight = aucFileHeader[20] << 24
                    | aucFileHeader[21] << 16
                    | aucFileHeader[22] << 8
                    | aucFileHeader[23];

                VIM_DBG_PRINTF3("File '%s' is Valid PNG. Width=%upx, Height=%upx", pszFilename, ulWidth, ulHeight);

                if (pulWidthPx) *pulWidthPx = ulWidth;
                if (pulWidthPx) *pulHeightPx = ulHeight;
            }
            else
            {
                pceft_debugf_test(pceftpos_handle.instance, "File '%s' is not valid PNG", pszFilename);
            }
        }
        else
        {
            pceft_debugf_test(pceftpos_handle.instance, "Failed to read file '%s'", pszFilename);
        }
    }
    else
    {
        pceft_debugf_test(pceftpos_handle.instance, "Image '%s' not found", pszFilename);
    }

    return fValid;
}

int VAA_get_PAD_size( char *pszPADLen )
{
    int iRet = 0;

    if ( ( pszPADLen != NULL ) &&
         vim_char_is_numeric( *pszPADLen ) &&
         vim_char_is_numeric(*(pszPADLen+1)) &&
         vim_char_is_numeric(*(pszPADLen+2)) )
    {
        iRet =  (  *pszPADLen     - '0' ) * 100;
        iRet += (  *(pszPADLen+1) - '0' ) * 10;
        iRet += (  *(pszPADLen+2) - '0' );
    }    
    else
    {
        DBG_MESSAGE( "PAD length parse error!" );   // Should not happen unless PAD input is malformed.
    }
    return iRet;
}

