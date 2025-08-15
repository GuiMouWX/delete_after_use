#include <stdio.h>

#include <unistd.h>

#include "incs.h"
#include "mal_nfc.h"

#define WALLY_REQ_FILENAME     "flash/Wally_Req.json"
#define WALLY_RSP_FILENAME     "flash/Wally_Resp.json"

#define WALLY_SESSION_QR_NAME  "SessionQR.png"
#define WALLY_SESSION_QR_PATH  "flash/" WALLY_SESSION_QR_NAME
static const char* EDPData = VIM_FILE_PATH_LOCAL "Wally.data";

#ifndef WALLY_WALLET_REMOVE_TIMEOUT_SEC
#define WALLY_WALLET_REMOVE_TIMEOUT_SEC     (5 * 60)
#endif

#ifndef WALLY_WALLET_REMOVE_VALIDATE_ID
#define WALLY_WALLET_REMOVE_VALIDATE_ID     0   // 0 = do not validate session on remove, 1 = validate
#endif

#define DROP_PAD_CENTER_X 159
#define DROP_PAD_CENTER_Y 321

typedef enum WALLY_SESSION_STATE
{
    SESSION_IDLE = 0,
    SESSION_AWAITING_QR_CODE,
    SESSION_AWAITING_REWARDS_DATA,
} WALLY_SESSION_STATE;

//#ifdef _DEBUG
static const VIM_CHAR* l_apszSessionStates[] =
{
    "SESSION_IDLE",
    "SESSION_AWAITING_QR_CODE",
    "SESSION_AWAITING_REWARDS_DATA",
};
//#endif
#define CHANGE_SESSION_STATE(x) do { if(l_sSessionData.eState != (x)) { VIM_DBG_PRINTF2("Wally Session State change: %s -> %s", l_apszSessionStates[l_sSessionData.eState], l_apszSessionStates[(x)]); l_sSessionData.eState = (x); } } while(0)

typedef struct CUSTOMER_PAYLOAD
{
    VIM_CHAR szRewardsId[100];
    VIM_BOOL fUseRewardsPoints;
} CUSTOMER_PAYLOAD;

typedef struct SESSION_DATA
{
    VIM_BOOL            fEnableNFC;
    WALLY_SESSION_STATE eState;
    VIM_CHAR            szQRCodeContent[256 + 1];
    VIM_CHAR            szPaymentSessionId[UUID_LEN + 1];
    VIM_CHAR            szImagePath[256 + 1];
    VIM_CHAR            szB64Path[256 + 1];
    VIM_RTC_UPTIME      ullTransactionTimer; //Timer for response from Wally Application
    VIM_RTC_UPTIME      ullQrTimer;          //Timer for QR Code rotation
    VIM_RTC_UPTIME      ullSessionTimer;     //Timer for Session rotation
    CUSTOMER_PAYLOAD    sCustomer;
    VIM_CHAR            szPAD_data[WOW_MAX_PAD_STRING_LEN];
    VIM_BOOL            fNeedDisplayRefreshNoPos;
    VIM_BOOL            fNeedDisplayRefresh;
    VIM_BOOL            fNFCLoyaltyRead;
} SESSION_DATA;

static VIM_CHAR *Wally_Get_PR_categories( void );
static SESSION_DATA l_sSessionData;
//JSON Items
static cJSON* l_pCJReceiptData;
const char* EDPTime = VIM_FILE_PATH_LOCAL "WallyTimes.data";

// NFC configuration files
static const char* const terminal_file = VIM_FILE_PATH_LOCAL "terminal.json";
static const char* const wowpass_file = VIM_FILE_PATH_LOCAL "wowpass.json";
static const char* const dynamic_file = VIM_FILE_PATH_LOCAL "dynamic.json";

VIM_BOOL Wally_HasRewardsId(void)
{
    return (0 < vim_strlen(l_sSessionData.sCustomer.szRewardsId));
}

static VIM_BOOL Wally_HasNFCRead(void)
{
    return l_sSessionData.fNFCLoyaltyRead;
}

static void Wally_SetNFCRead(void)
{
    l_sSessionData.fNFCLoyaltyRead = TRUE;
}

/*
VIM_BOOL Wally_HasPaymentSessionId(void)
{
    pceft_debug(pceftpos_handle.instance, "Wally_HasPaymentSessionId is ");
    pceft_debug(pceftpos_handle.instance, l_sSessionData.szPaymentSessionId);

    return (0 < vim_strlen(l_sSessionData.szPaymentSessionId));

}
*/
void Wally_AddPadDataToSession( VIM_CHAR *pszPadData )
{
    if ( pszPadData && ( vim_strlen(pszPadData) < sizeof(l_sSessionData.szPAD_data) ) )
    {
        vim_strcpy( l_sSessionData.szPAD_data, pszPadData );
    }
}

#if 0
//This can be used to build the common format message - not yet implemented in Wally App
static cJSON* Wally_BuildFileRequest(const VIM_CHAR* pszCommand, cJSON* pCJRequestPayload)
{
    cJSON* pCJFileRequest = cJSON_CreateObject();
    cJSON* pCJMeta        = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_FALSE;

    if (pszCommand && pCJRequestPayload)
    {
        char szTerminalID[8 + 1] = { 0 };
        static VIM_UINT32 s_ulMessageID = 0;

        vim_strncpy(szTerminalID, terminal.terminal_id, sizeof(szTerminalID) - 1);

        fSuccess = VIM_TRUE;

        fSuccess &= VAA_AddString(pCJMeta,      "Command",    pszCommand,        VIM_TRUE);
        fSuccess &= VAA_AddNumber(pCJMeta,      "MessageId",  ++s_ulMessageID,   VIM_TRUE);
        fSuccess &= VAA_AddString(pCJMeta,      "TerminalId", szTerminalID,      VIM_TRUE);

        fSuccess &= VAA_AddItem(pCJFileRequest, "meta",       pCJMeta,           VIM_TRUE);
        fSuccess &= VAA_AddItem(pCJFileRequest, "request",    pCJRequestPayload, VIM_TRUE);
    }
    
    if(!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "EDP: Failed to build file request for '%s'", SAFE_STR(pszCommand));
        FREE_JSON(pCJRequestPayload);
        FREE_JSON(pCJMeta);
        FREE_JSON(pCJFileRequest);
    }

    return pCJFileRequest;
}
#else
//This uses the existing 'flat' interface to Wally App
static cJSON* Wally_BuildFileRequest(const VIM_CHAR* pszCommand, cJSON* pCJRequestPayload)
{
    VIM_BOOL fSuccess = VIM_FALSE;

    if (pszCommand && pCJRequestPayload)
    {
        char szTerminalID[8 + 1] = { 0 };
        static VIM_UINT32 s_ulMessageID = 0;

        vim_strncpy(szTerminalID, terminal.terminal_id, sizeof(szTerminalID) - 1);

        fSuccess = VIM_TRUE;
        fSuccess &= VAA_AddString(pCJRequestPayload, "cmd",        pszCommand,      VIM_TRUE);
        fSuccess &= VAA_AddNumber(pCJRequestPayload, "MessageId",  ++s_ulMessageID, VIM_TRUE);
        fSuccess &= VAA_AddString(pCJRequestPayload, "TerminalId", szTerminalID,    VIM_TRUE);
    }

    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "EDP: Failed to build file request for '%s'", SAFE_STR(pszCommand));
        FREE_JSON(pCJRequestPayload);
    }

    return pCJRequestPayload;
}
#endif

//The store ID for Everyday Pay requires 110 to be appended to the end for some reason
static const VIM_CHAR* Wally_GetStoreID()
{
    static VIM_CHAR s_szWallyLaneID[7 + 1] = { 0 };
    vim_snprintf(s_szWallyLaneID, sizeof(s_szWallyLaneID), "%s110", VAA_GetStoreID());
    return s_szWallyLaneID;
}

// Returned cJSON pointer needs to be freed after use
static cJSON* Wally_GenerateCreateStartSessionRequest(void)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;
    fSuccess &= VAA_AddString(pCJRequest, "laneId",   VAA_GetLaneID(),    VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "storeId",  Wally_GetStoreID(), VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "location", VAA_GetLocation(), VIM_TRUE);
    if (0 < TERM_QR_QR_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLiveQR",             TERM_QR_QR_TIMEOUT + 10, VIM_TRUE);
    if (0 < TERM_QR_PS_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLivePaymentSession", TERM_QR_PS_TIMEOUT * 2,  VIM_TRUE);
    if (0 < TERM_QR_REWARDS_POLL) fSuccess &= VAA_AddIntAsString(pCJRequest, "pollRewardsPeriod",        TERM_QR_REWARDS_POLL,    VIM_FALSE);
    if (TERM_QR_POLLBUSTER)       fSuccess &= VAA_AddBool       (pCJRequest, "pollbuster",               TERM_QR_POLLBUSTER,      VIM_FALSE);
    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "EDP: Failed to create request payload for 'StartSession'");
        FREE_JSON(pCJRequest);
        return NULL;
    }

    return Wally_BuildFileRequest("StartSession", pCJRequest);
}

// Returned cJSON pointer needs to be freed after use
static cJSON* Wally_GenerateRefreshQrRequest(void)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;

    if (0 < TERM_QR_QR_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLiveQR",      TERM_QR_QR_TIMEOUT + 10, VIM_TRUE);
    if (0 < TERM_QR_REWARDS_POLL) fSuccess &= VAA_AddIntAsString(pCJRequest, "pollRewardsPeriod", TERM_QR_REWARDS_POLL,    VIM_FALSE);
    if (TERM_QR_POLLBUSTER)       fSuccess &= VAA_AddBool       (pCJRequest, "pollbuster",        TERM_QR_POLLBUSTER,      VIM_FALSE);

    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "EDP: Failed to create request payload for 'RefreshQR'");
        FREE_JSON(pCJRequest);
        return NULL;
    }

    return Wally_BuildFileRequest("RefreshQR", pCJRequest);
}

static cJSON* Wally_GenerateCancelRequest(void)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    return Wally_BuildFileRequest("Cancel", pCJRequest);
}

static cJSON* Wally_GenerateDeleteSessionRequest(const char *pszPaymentSessionId, const char *pszDeleteReason)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;

    fSuccess &= VAA_AddString(pCJRequest, "paymentSessionId", pszPaymentSessionId, VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "deleteReason", pszDeleteReason, VIM_FALSE);

    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "EDP: Failed to create request payload for 'Delete'");
        FREE_JSON(pCJRequest);
        return NULL;
    }

    return Wally_BuildFileRequest("Delete", pCJRequest);
}

static cJSON* Wally_GenerateDoPaymentRequest(void)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;

    VIM_CHAR szAmountString[10 + 1] = { 0 };
    VIM_CHAR szRRN[12 + 1] = { 0 };
    VIM_CHAR szTxnRef[3 + 12 + 1] = { 0 };
    VIM_CHAR szSTAN[6 + 1] = { 0 };
    VIM_CHAR *pszPrCats = Wally_Get_PR_categories();

    //Prepare data
    vim_snprintf(szAmountString, VIM_SIZEOF(szAmountString), "%u", (VIM_UINT32)txn.amount_total);
    vim_mem_copy(szRRN, txn.rrn, VIM_SIZEOF(szRRN) - 1);
    vim_strcpy(szTxnRef, TERM_QR_MERCH_REF_PREFIX);
    vim_mem_copy(&szTxnRef[vim_strlen(szTxnRef)], txn.pos_reference, sizeof(szTxnRef) - vim_strlen(szTxnRef) - 1);
    vim_snprintf(szSTAN, VIM_SIZEOF(szSTAN), "%06u", terminal.stan);
    terminal_increment_stan();

    //Add elements to JSON request
    fSuccess &= VAA_AddString(pCJRequest, "merchantReferenceId", szTxnRef,                   VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "grossAmount",         szAmountString,             VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "maxUses",             "1",                        VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "location",            VAA_GetLocation(),          VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "laneId",              VAA_GetLaneID(),            VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "storeId",             Wally_GetStoreID(),         VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "rrn",                 szRRN,                      VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "terminalId",          VAA_GetTerminalID(),        VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "stan",                szSTAN,                     VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "paymentSessionId",    txn.edp_payment_session_id, VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "productRestrictCats", pszPrCats,                  VIM_FALSE);
    fSuccess &= VAA_AddString(pCJRequest, "idempotencyKey",      VAA_GenerateUUID(),         VIM_TRUE);

    FREE_MALLOC(pszPrCats); 

    if (0 < TERM_QR_PS_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLivePayment", TERM_QR_PS_TIMEOUT, VIM_TRUE);
    if (0 < TERM_QR_QR_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLiveQR",      TERM_QR_QR_TIMEOUT, VIM_TRUE);
    if (0 < TERM_QR_RESULTS_POLL) fSuccess &= VAA_AddIntAsString(pCJRequest, "pollResultPeriod",  TERM_QR_RESULTS_POLL, VIM_FALSE);

    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "EDP: Failed to create request payload for 'DigitalPayment'");
        FREE_JSON(pCJRequest);
        return NULL;
    }

    return Wally_BuildFileRequest("DigitalPayment", pCJRequest);
}

static cJSON *Wally_GenerateDoRefundRequest(void)
{
    cJSON *pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;

    VIM_CHAR szAmountString[10 + 1] = { 0 };
    VIM_CHAR szRRN[12 + 1] = { 0 };
    VIM_CHAR szTxnRef[3 + 12 + 1] = { 0 };
    VIM_CHAR szSTAN[6 + 1] = { 0 };
    VIM_CHAR *pszPrCats = Wally_Get_PR_categories();

    // Prepare data
    vim_snprintf(szAmountString, VIM_SIZEOF(szAmountString), "%u", (VIM_UINT32)txn.amount_total);
    vim_mem_copy(szRRN, txn.rrn, VIM_SIZEOF(szRRN) - 1);
    vim_strcpy(szTxnRef, TERM_QR_MERCH_REF_PREFIX);
    vim_mem_copy(&szTxnRef[vim_strlen(szTxnRef)], txn.pos_reference, sizeof(szTxnRef) - vim_strlen(szTxnRef) - 1);
    vim_snprintf(szSTAN, VIM_SIZEOF(szSTAN), "%06u", terminal.stan);
    terminal_increment_stan();

    // Add elements to JSON request
    fSuccess &= VAA_AddString(pCJRequest, "merchantReferenceId", szTxnRef,                   VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "grossAmount",         szAmountString,             VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "maxUses",             "1",                        VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "location",            VAA_GetLocation(),          VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "laneId",              VAA_GetLaneID(),            VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "storeId",             Wally_GetStoreID(),         VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "rrn",                 szRRN,                      VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "terminalId",          VAA_GetTerminalID(),        VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "stan",                szSTAN,                     VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "paymentSessionId",    txn.edp_payment_session_id, VIM_TRUE);
    fSuccess &= VAA_AddString(pCJRequest, "productRestrictCats", pszPrCats,                  VIM_FALSE);
    fSuccess &= VAA_AddString(pCJRequest, "idempotencyKey",      VAA_GenerateUUID(),         VIM_TRUE);

    FREE_MALLOC(pszPrCats); 

    if (0 < TERM_QR_PS_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLivePayment", TERM_QR_PS_TIMEOUT, VIM_TRUE);
    if (0 < TERM_QR_QR_TIMEOUT)   fSuccess &= VAA_AddIntAsString(pCJRequest, "timeToLiveQR",      TERM_QR_QR_TIMEOUT, VIM_TRUE);
    if (0 < TERM_QR_RESULTS_POLL) fSuccess &= VAA_AddIntAsString(pCJRequest, "pollResultPeriod",  TERM_QR_RESULTS_POLL, VIM_FALSE);

    if (!fSuccess)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "EDP: Failed to create request payload for 'DigitalRefund'");
        FREE_JSON(pCJRequest);
        return NULL;
    }

    // FIXME I'm assuming DigitalPayment becomes DigitalRefund for refunds, and otherwise they are the same. May not turn out to be true.
    return Wally_BuildFileRequest("DigitalRefund", pCJRequest);
}

static VIM_BOOL Wally_WriteJSONRequestFile(const cJSON* pCJRequest)
{
    VAA_DeleteFile(WALLY_RSP_FILENAME);
    return VAA_WriteJSONRequestFile(WALLY_REQ_FILENAME, pCJRequest);
}
VIM_BOOL Wally_WriteJSONRequestOfRegistrytoFile(const cJSON* pCJRequest)
{
    VAA_DeleteFile(EDPTime);
    return VAA_WriteJSONRequestFile(EDPTime, pCJRequest);
}

void Wally_BuildJSONRequestOfRegistrytoFIle(void)
{
    cJSON* pCJRequest = cJSON_CreateObject();
    VIM_BOOL fSuccess = VIM_TRUE;
    fSuccess &= VAA_AddNumber(pCJRequest, "QR_REVERSAL_TIMER", TERM_QR_REVERSALTIMER, VIM_TRUE);
    fSuccess &= VAA_AddNumber(pCJRequest, "QR_UPDATE_PAY_TIMEOUT", TERM_QR_UPDATEPAY_TIMEOUT, VIM_TRUE);
    Wally_WriteJSONRequestOfRegistrytoFile(pCJRequest);
    FREE_JSON(pCJRequest);
}
static cJSON* Wally_ReadJSONResponseFile(void)
{
    cJSON* pCJ = VAA_ReadJSONFile(WALLY_RSP_FILENAME);
    if (pCJ)
    {
        VAA_DeleteFile(WALLY_REQ_FILENAME);
    }
    return pCJ;
}

static cJSON* Wally_WaitForResponse(void)
{
    cJSON* pCJ = VAA_WaitForJSONResponse(WALLY_RSP_FILENAME, TERM_QR_WALLY_TIMEOUT, VIM_FALSE);
    if (pCJ)
    {
        VAA_DeleteFile(WALLY_REQ_FILENAME);
    }
    return pCJ;
}

static VIM_BOOL Wally_RespIsSuccess(cJSON* psCJ)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (psCJ)
    {
        cJSON_bool fParsedSuccess = FALSE;
        cJSON* psCJResponse = cJSON_GetObjectItem(psCJ, "response");
        if (cJSON_GetBoolItemValue(psCJResponse, "success", &fParsedSuccess))
        {
            fSuccess = fParsedSuccess ? VIM_TRUE : VIM_FALSE;
            pceft_debugf_test(pceftpos_handle.instance, "Wally response %s", SUCCESS_STR(fSuccess));
        }
        else
        {
            pceft_debug_test(pceftpos_handle.instance, "Wally response not successful (missing 'success' flag)");
        }
    }
    else
    {
        //Null / no response
    }
    return fSuccess;
}

static void Wally_DeleteSessionQrFile(void)
{
    VAA_DeleteFile(WALLY_SESSION_QR_PATH);
}

static void Wally_Display_ProcessingNow(void)
{
    display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, "QrProcessing.gif");
}

static VIM_BOOL Wally_Display_SendQrToPOS(void)
{
    VIM_BOOL fResult = VIM_FALSE;
    VIM_CHAR szPAD[256] = { 0 };
    VIM_UINT32 ulLen = 0;

    VIM_DBG_PRINTF1("Sending QR Code '%s' to POS", l_sSessionData.szQRCodeContent);

    ulLen = vim_strlen(l_sSessionData.szQRCodeContent);
    vim_snprintf(szPAD, sizeof(szPAD), "QRC%03u%s", ulLen, l_sSessionData.szQRCodeContent);

    if (!VAA_IsNullOrEmpty(l_sSessionData.szImagePath))
    {
        ulLen = vim_strlen(l_sSessionData.szImagePath);
        vim_snprintf(szPAD + vim_strlen(szPAD), sizeof(szPAD) - vim_strlen(szPAD), "DAT%03u""TAG003QRI""PTH%03u%s", ulLen + 9 + 6, ulLen, l_sSessionData.szImagePath);
    }
    if (!VAA_IsNullOrEmpty(l_sSessionData.szB64Path))
    {
        ulLen = vim_strlen(l_sSessionData.szB64Path);
        vim_snprintf(szPAD + vim_strlen(szPAD), sizeof(szPAD) - vim_strlen(szPAD), "DAT%03u""TAG003Q64""PTH%03u%s", ulLen + 9 + 6, ulLen, l_sSessionData.szB64Path);
    }

    if (!VAA_IsNullOrEmpty(szPAD))
    {
        fResult = (VIM_ERROR_NONE == pceftpos_custom_display("", "", VIM_PCEFTPOS_GRAPHIC_DATA, PCEFT_INPUT_NONE, VIM_PCEFTPOS_KEY_CANCEL, szPAD));
    }
    return fResult;
}

static VIM_BOOL Wally_Display_SessionQrOverlay(const VIM_CHAR* pszBackgroundImage)
{
    VIM_BOOL fResult = VIM_FALSE;
    if (!TERM_QR_DISABLE_FROM_IMAGE)
    {
        if (!VAA_IsNullOrEmpty(pszBackgroundImage))
        {
            VIM_UINT32 ulWidth = 0, ulHeight = 0;
            if (VAA_ValidatePngFile(WALLY_SESSION_QR_PATH, &ulWidth, &ulHeight) && (0 < ulWidth) && (0 < ulHeight))
            {
                char szLeft[3 + 1] = { 0 }, szTop[3 + 1] = { 0 };
                VIM_INT32 lLeft = DROP_PAD_CENTER_X - ulWidth / 2;
                VIM_INT32 lTop = DROP_PAD_CENTER_Y - ulHeight / 2;
                if (0 > lLeft) lLeft = 0;
                if (0 > lTop)  lTop = 0;
                vim_snprintf(szLeft, sizeof(szLeft), "%u", lLeft);
                vim_snprintf(szTop, sizeof(szLeft), "%u", lTop);
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Display QR image (w/h) %d/%d at (%s,%s)", ulWidth, ulHeight, szLeft, szTop);
                fResult = (VIM_ERROR_NONE == display_screen(DISP_IMAGES_AND_OVERLAY, VIM_PCEFTPOS_KEY_NONE, pszBackgroundImage, "../../../" WALLY_SESSION_QR_PATH, szLeft, szTop));
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Invalid PNG file '%s' (%ux%u)", WALLY_SESSION_QR_PATH, ulWidth, ulHeight);
            }
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Invalid background image");
        }
    }
    else
    {
        pceft_debug_test(pceftpos_handle.instance, "Skipping QR from image as configured");
    }
    return fResult;
}

static void Wally_Display_LoyaltyPrompt(VIM_BOOL fSendToPOS)
{
    pceft_debugf_test(pceftpos_handle.instance, "EDP Display Loyalty '%s'", SAFE_STR(l_sSessionData.szQRCodeContent));
    if (!VAA_IsNullOrEmpty(l_sSessionData.szQRCodeContent))
    {
        const VIM_CHAR* pszBackground = l_sSessionData.fEnableNFC ? "QrJ8.png" : "QrIdle.png";

        if (TERM_QR_SEND_QR_TO_POS && fSendToPOS)
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "calling Wally_Display_SendQrToPOS");
            Wally_Display_SendQrToPOS();
        }

        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "Displaying QrOverlay");
        if (!Wally_Display_SessionQrOverlay(pszBackground))
        {
            display_screen(DISP_IMAGES_AND_QR, VIM_PCEFTPOS_KEY_NONE, pszBackground, l_sSessionData.szQRCodeContent, "68", "230", "QrBranding.png", "131", "293");
        }
    }
    else if(l_sSessionData.fEnableNFC)
    {
        display_screen_cust_pcx(DISP_IMAGES, "Label", WOW_PRESENT_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
    }
    else
    {
        Wally_Display_ProcessingNow();
    }
}

static void Wally_vas_error_call_back(VIM_ERROR_PTR eError)
{
    if (eError != VIM_ERROR_INVALID_DATA) {
        DBG_POINT;
        VIM_DBG_MESSAGE("Wally_vas_error_call_back");
        if (!Wally_Display_SessionQrOverlay("QrNfcError.png"))
        {
            display_screen(DISP_IMAGES_AND_QR, VIM_PCEFTPOS_KEY_NONE, "QrNfcError.png", l_sSessionData.szQRCodeContent, "68", "230", "QrBranding.png", "131", "293");
        }
        vim_sound(VIM_TRUE);
        VIM_ERROR_IGNORE(vim_event_sleep(2000)); // wait 2 secs

        if (!terminal.abort_transaction)
        {
            l_sSessionData.fNeedDisplayRefreshNoPos = VIM_TRUE;
        }
    }
    else
    {
        // Comms failure contacting EDP host - enable transaction cancel from POS
        vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_CANCEL);
    }
}

VIM_ERROR_PTR Wally_SessionFunc_ResetSessionData(VIM_BOOL fEnableNFC, VIM_BOOL fDeleteSession)
{
    VIM_DBG_MESSAGE("Resetting Wally Session Data");
    CHANGE_SESSION_STATE(SESSION_IDLE);
    VIM_ERROR_PTR eRet;
    //End previous session/restart Wally state
    if(fDeleteSession && !VAA_IsNullOrEmpty(l_sSessionData.szPaymentSessionId))
        eRet = Wally_DeleteSession(l_sSessionData.szPaymentSessionId, NULL, VIM_FALSE);
    else 
        eRet = Wally_Cancel();

    //Reset session data
    vim_mem_clear(&l_sSessionData, sizeof(l_sSessionData));
    l_sSessionData.fEnableNFC = fEnableNFC;
    l_sSessionData.fNeedDisplayRefreshNoPos = VIM_TRUE;
    Wally_DeleteSessionQrFile();
    return eRet;
}

static VIM_ERROR_PTR Wally_SessionFunc_SendGenerateQRRequest(VIM_BOOL fCreateSession)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    const VIM_CHAR* pszLabel = fCreateSession ? "Create Payment Session"                  : "Refresh QR";
    cJSON* pCJRequest        = fCreateSession ? Wally_GenerateCreateStartSessionRequest() : Wally_GenerateRefreshQrRequest();

    VIM_DBG_PRINTF1("Wally_SendGenerateQRRequest: %s", pszLabel);

    if (pCJRequest)
    {
        if (Wally_WriteJSONRequestFile(pCJRequest))
        {
            VIM_DBG_PRINTF1("Successfully wrote Wally %s data to Request File", pszLabel);
            CHANGE_SESSION_STATE(SESSION_AWAITING_QR_CODE);

            l_sSessionData.ullTransactionTimer = VAA_GetTimer(TERM_QR_WALLY_TIMEOUT);
            l_sSessionData.ullQrTimer          = VAA_GetTimer(TERM_QR_QR_TIMEOUT);
            if (fCreateSession)
            {
                l_sSessionData.ullSessionTimer = VAA_GetTimer(TERM_QR_PS_TIMEOUT);
            }

            eRet = VIM_ERROR_NONE;
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Failed to Write Wally Request file");
        }
        FREE_JSON(pCJRequest);
    }
    else
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "Failed to generate %s Request", pszLabel);
    }

    return eRet;
}

static VIM_ERROR_PTR Wally_SessionFunc_AwaitQRResponse(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    cJSON* pCJ = Wally_ReadJSONResponseFile();
    if (Wally_RespIsSuccess(pCJ))
    {
        cJSON* pCJ_Response = cJSON_GetObjectItem(pCJ, "response");
        VIM_CHAR szPaymentSessionId[UUID_LEN + 1] = { 0 };
        VIM_CHAR szQRCodeContent[256 + 1] = { 0 };
        VIM_CHAR szImagePath[256 + 1] = { 0 };
        VIM_CHAR szB64Path[256 + 1] = { 0 };

        //Payment Session ID is only in Start Payment Session Response; not Refresh QR Code. Make optional
        if (cJSON_GetStringItemValue(pCJ_Response, "paymentSessionId", szPaymentSessionId, sizeof(szPaymentSessionId)))
        {
            VIM_DBG_PRINTF1("Parsed EverydayPay Payment Session ID '%s'", szPaymentSessionId);
            vim_mem_clear(l_sSessionData.szPaymentSessionId, sizeof(l_sSessionData.szPaymentSessionId));
            vim_strncpy(l_sSessionData.szPaymentSessionId, szPaymentSessionId, sizeof(l_sSessionData.szPaymentSessionId) - 1);
        }
        //Clear QR Content - should be present in all cases
        vim_mem_clear(l_sSessionData.szQRCodeContent, sizeof(l_sSessionData.szQRCodeContent));
        if (cJSON_GetStringItemValue(pCJ_Response, "content", szQRCodeContent, sizeof(szQRCodeContent)))
        {
            VIM_DBG_PRINTF1("Parsed QR Code content '%s'", szQRCodeContent);
            vim_strncpy(l_sSessionData.szQRCodeContent, szQRCodeContent, sizeof(l_sSessionData.szQRCodeContent) - 1);
        }
        //QR POS asset image path
        vim_mem_clear(l_sSessionData.szImagePath, sizeof(l_sSessionData.szImagePath));
        if (cJSON_GetStringItemValue(pCJ_Response, "qrImagePath", szImagePath, sizeof(szImagePath)))
        {
            VIM_DBG_PRINTF1("Parsed QR image path '%s'", szImagePath);
            vim_strncpy(l_sSessionData.szImagePath, szImagePath, sizeof(l_sSessionData.szImagePath) - 1);
        }
        //Base64 QR POS asset image path
        vim_mem_clear(l_sSessionData.szB64Path, sizeof(l_sSessionData.szB64Path));
        if (cJSON_GetStringItemValue(pCJ_Response, "base64Path", szB64Path, sizeof(szB64Path)))
        {
            VIM_DBG_PRINTF1("Parsed QR image path '%s'", szB64Path);
            vim_strncpy(l_sSessionData.szB64Path, szB64Path, sizeof(l_sSessionData.szB64Path) - 1);
        }

        if (!VAA_IsNullOrEmpty(l_sSessionData.szPaymentSessionId) && !VAA_IsNullOrEmpty(l_sSessionData.szQRCodeContent))
        {
            pceft_debug_test(pceftpos_handle.instance, "Successfully Parsed QR Response");

            l_sSessionData.fNeedDisplayRefresh = VIM_TRUE;
            CHANGE_SESSION_STATE(SESSION_AWAITING_REWARDS_DATA);
            eRet = ERR_QR_RECV_SUCCESSFUL;
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "QR Response did not contain Payment Session ID and QR Code content");
            eRet = VIM_ERROR_INVALID_DATA;
        }
    }
    else if (pCJ)
    {
        pceft_debug_test(pceftpos_handle.instance, "Unsuccessful response; restarting states");
        eRet = VIM_ERROR_INVALID_DATA;
    }
    else if (VAA_IsTimerExpired(l_sSessionData.ullTransactionTimer))
    {
        VIM_DBG_MESSAGE("Hit Await QR Response timeout!");
        eRet = VIM_ERROR_TIMEOUT;
    }
    else
    {
        //Keep looping in this state
        eRet = VIM_ERROR_NONE;
    }

    if(pCJ)
    {
        //Tidyup
        VAA_DeleteFile(WALLY_RSP_FILENAME);
        FREE_JSON(pCJ);
    }

    return eRet;
}

static VIM_ERROR_PTR Wally_SessionFunc_AwaitRewardsId(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    cJSON* pCJ = Wally_ReadJSONResponseFile();
    if (Wally_RespIsSuccess(pCJ))
    {
        cJSON* pCJ_Response = cJSON_GetObjectItem(pCJ, "response");

        if (cJSON_GetStringItemValue(pCJ_Response, "rewardsId", l_sSessionData.sCustomer.szRewardsId, sizeof(l_sSessionData.sCustomer.szRewardsId)) && (0 < vim_strlen(l_sSessionData.sCustomer.szRewardsId)))
        {
            cJSON_bool fUsePoints = 0;
            VIM_DBG_PRINTF1("Parsed EverydayPay Rewards ID '%s'", l_sSessionData.sCustomer.szRewardsId);

            if (cJSON_GetBoolItemValue(pCJ_Response, "usePoints", &fUsePoints))
            {
                VIM_DBG_PRINTF1("Parsed EverydayPay Use Points '%s'", BOOL_STR(fUsePoints));
                l_sSessionData.sCustomer.fUseRewardsPoints = fUsePoints ? VIM_TRUE : VIM_FALSE;
            }

            CHANGE_SESSION_STATE(SESSION_IDLE);
            eRet = ERR_LOYALTY_CARD_SUCESSFUL;
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "QR Response did not contain Rewards ID");
            eRet = VIM_ERROR_NONE; // Keep Looping
        }
    }
    else if (pCJ)
    {
        pceft_debug_test(pceftpos_handle.instance, "Unsuccessful response; restarting states");
        eRet = VIM_ERROR_INVALID_DATA;
    }
    else if (VAA_IsTimerExpired(l_sSessionData.ullSessionTimer))
    {
        pceft_debug_test(pceftpos_handle.instance, "Payment Session Timer expired, creating new payment session");
        eRet = Wally_SessionFunc_SendGenerateQRRequest(VIM_TRUE);
    }
    else if (VAA_IsTimerExpired(l_sSessionData.ullQrTimer))
    {
        pceft_debug_test(pceftpos_handle.instance, "QR Timer expired, rotating QR Code");
        eRet = Wally_SessionFunc_SendGenerateQRRequest(VIM_FALSE);
    }
    else
    {
        //Keep looping in this state
        eRet = VIM_ERROR_NONE;
    }

    if ( pCJ )
    {
        //Tidyup
        VAA_DeleteFile(WALLY_RSP_FILENAME);
        FREE_JSON(pCJ);
    }

    return eRet;
}

// This function needs to load and parse "./flash/terminal.json" to get the PollTime
// poll timeout, and not return 'timeout' as an error, until that limit is hit.

#ifdef PPBUSYFIX

static VIM_ERROR_PTR Wally_PollJ8QrOnly(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
    VIM_INT32 lTimeoutMs = (TERM_QR_WALLY_TIMEOUT > 0) ? TERM_QR_WALLY_TIMEOUT : 30000;       // 30 sec t/o default (@QR_WALLY_TIMEOUT may override).
    VIM_RTC_UPTIME end;

    if ( lTimeoutMs < 1000 ) lTimeoutMs *= 1000;	// timeout should be in millisec

    vim_rtc_get_uptime( &end );
    end += lTimeoutMs;

    // Note, this function must NEVER return VIM_ERROR_NONE. This function is meant to behave exactly
    // like 'Wally_PollJ8QrNFC', so if it did return VIM_ERROR_NONE, the caller would assume that
    // the NFC has provided rewards-data - and we are not enabling the NFC, in this path.
    while ( eRet == VIM_ERROR_NONE )
    {
        VIM_RTC_UPTIME now;

        usleep( 10*1000 ); // 10 microseconds OS sleep to reduce CPU loading

        vim_rtc_get_uptime( &now );
        if ( now >= end )
        {
            eRet = VIM_ERROR_TIMEOUT;
        }
        else if (mal_nfc_CheckPendingPOSMessage())
        {
            eRet = VIM_ERROR_POS_CANCEL;
        }
        else
        {
            eRet = Wally_SessionStateMachine();
            vim_event_sleep(100);	// Try to ease up the reading of file
        }
    }
    return eRet;
}

#else

static VIM_ERROR_PTR Wally_PollJ8QrOnly(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
    VIM_INT32 lTimeoutMs = 30000;       // 30 sec t/o default (PollTime in terminal.json file may override).
    VIM_RTC_UPTIME end;
    cJSON* pCJ = VAA_ReadJSONFile(terminal_file);

    if (pCJ)
    {
        int lTemp = 0;
        cJSON* pCJ_TC = cJSON_GetObjectItem(pCJ, "Terminal_Configuration");

        if (pCJ_TC)
        {
            cJSON* pCJ_Term = cJSON_GetObjectItem(pCJ_TC, "Terminal");

            if (pCJ_Term)
            {
                cJSON_GetIntItemValue(pCJ_Term, "PollTime", &lTemp);
                if (lTemp > 0)
                {
                    VIM_DBG_PRINTF2("Override default PollTime %d with terminal.json value %d", lTimeoutMs, lTemp);
                    lTimeoutMs = lTemp;
                }
            }
        }
        FREE_JSON(pCJ);
    }

    vim_rtc_get_uptime(&end);
    end += lTimeoutMs;

    // Note, this function must NEVER return VIM_ERROR_NONE. This function is meant to behave exactly
    // like 'Wally_PollJ8QrNFC', so if it did return VIM_ERROR_NONE, the caller would assume that
    // the NFC has provided rewards-data - and we are not enabling the NFC, in this path.
    while (eRet == VIM_ERROR_NONE)
    {
        VIM_RTC_UPTIME now;

        usleep(10 * 1000); // 10 microseconds OS sleep to reduce CPU loading

        vim_rtc_get_uptime(&now);
        if (now >= end)
        {
            eRet = VIM_ERROR_TIMEOUT;
        }
        else if (mal_nfc_CheckPendingPOSMessage())
        {
            eRet = VIM_ERROR_POS_CANCEL;
        }
        else
        {
            eRet = Wally_SessionStateMachine();
        }
    }

    return eRet;
}


#endif

static VIM_ERROR_PTR Wally_PollJ8QrNFC(void)
{
    return mal_nfc_DoVasActivation(WALLET_WOW_EDR, terminal_file, wowpass_file, dynamic_file, mal_nfc_Thread_CancelOnRxPending, VIM_TRUE);
}

VIM_ERROR_PTR Wally_HandleJ8Session(VIM_EVENT_FLAG_PTR* pfEventList, VIM_BOOL fEnableNFC)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    VIM_DBG_PRINTF2("Wally_HandleJ8Session %p, EnableNFC=%s", pfEventList, BOOL_STR(fEnableNFC));

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else
    {
        VIM_BOOL bLoop = VIM_TRUE;

        Wally_SessionFunc_ResetSessionData(fEnableNFC, VIM_TRUE);

        // comms_fd is the handle we use to detect POS messages (cancellation e.g.)
        VIM_DBG_PRINTF1("comms_fd = %d", comms_fd);
        mal_nfc_SetCommsFd(comms_fd);

        while (bLoop)
        {
            if(l_sSessionData.fNeedDisplayRefreshNoPos || l_sSessionData.fNeedDisplayRefresh)
            {
                Wally_Display_LoyaltyPrompt(l_sSessionData.fNeedDisplayRefresh);
                l_sSessionData.fNeedDisplayRefreshNoPos = l_sSessionData.fNeedDisplayRefresh = VIM_FALSE;
            }
            if (SESSION_IDLE == l_sSessionData.eState)
            {
                Wally_SessionFunc_SendGenerateQRRequest(VIM_TRUE);
            }

            // blocks main thread until something happens - returns on:
            // - (NFC Enabled) VAS tap successful
            // - POS msg received (could be a cancel, or a heartbeat)
            // - (NFC Enabled) VAS tap read error
            // - (NFC Enabled) VAS read timeout. Timeout defined (in msec) in 'PollTime' parameter in terminal.json
            // - Wally Loyalty info received
            // Note: this drives the Wally Session State Machine
            eRet = fEnableNFC ? Wally_PollJ8QrNFC() : Wally_PollJ8QrOnly();

            if (Wally_HasRewardsId())
            {
                SaveLoyaltyNumber(l_sSessionData.sCustomer.szRewardsId, vim_strlen(l_sSessionData.sCustomer.szRewardsId), VIM_ICC_SESSION_UNKNOWN);
                VIM_DBG_PRINTF1("Wally: SaveLoyaltyNumber [%s]", l_sSessionData.sCustomer.szRewardsId);
                eRet = ERR_LOYALTY_CARD_SUCESSFUL;
                bLoop = VIM_FALSE;
            }
            else if (eRet == VIM_ERROR_NONE)
            {
                VIM_CHAR szLoyaltyData[128] = { 0 };

                // card read success
                mal_nfc_GetVasCardNumber(szLoyaltyData, VIM_SIZEOF(szLoyaltyData), &txn.session_type);
                if (0 < vim_strlen(szLoyaltyData))
                {
                    Wally_SetNFCRead();
                    SaveLoyaltyNumber(szLoyaltyData, vim_strlen(szLoyaltyData), VIM_ICC_SESSION_UNKNOWN);
                    VIM_DBG_PRINTF1("NFC VAS: SaveLoyaltyNumber [%s]", szLoyaltyData);

                    eRet = ERR_LOYALTY_CARD_SUCESSFUL;
                }
                else
                {
                    eRet = VIM_ERROR_MISMATCH;
                }
                bLoop = VIM_FALSE;
            }
            else
            {
                VIM_DBG_PRINTF4("%s error = %d (%s,%s)", fEnableNFC ? "mal_nfc_DoVasActivation" : "Wally_PollJ8QrOnly", eRet, SAFE_STR(eRet->id), SAFE_STR(eRet->name));

                if ((VIM_ERROR_POS_CANCEL != eRet) && (VIM_ERROR_TIMEOUT != eRet) && (ERR_QR_RECV_SUCCESSFUL != eRet))
                {
                    // Handle EDP host comms adn card read errors
                    // If card read error call callback to display an error to user, then re-instate the VAS read again
                    Wally_vas_error_call_back(eRet);
                }

                if (VIM_ERROR_INVALID_DATA == eRet)
                {
                    // Non-recoverable EDP host comms error. Session has been reset at this point.
                    bLoop = VIM_FALSE;
                }
                else
                {
                    // check events and return if interrupt event occurred
                    eRet = card_get_check_events(pfEventList);
                    if (VIM_ERROR_NONE != eRet)
                    {
                        VIM_DBG_ERROR(eRet);
                        bLoop = VIM_FALSE;
                        Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_FALSE);
                    }
                    else
                    {
                        // else not a POS cancellation or comms gone down, probably a heartbeat msg, loop around again, reinstate the VAS read
                        //Send a heartbeat to the client so it doesn't think we're dead
                        pceft_pos_send_heartbeat(VIM_TRUE);
                    }
                }
            }
        }
    }

    VIM_ERROR_RETURN(eRet);
}

VIM_ERROR_PTR Wally_CheckPaymentSession(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    VIM_DBG_MESSAGE("Wally_CheckPaymentSession");

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else if(!Wally_HasRewardsId())
    {
        eRet = VIM_ERROR_NONE;

        if (!Wally_HasNFCRead())
        {
            VIM_RTC_UPTIME ulTimer = VAA_GetTimer(3);
            VIM_DBG_MESSAGE("Running state machine for a short time");
            while (!Wally_HasRewardsId() && !VAA_IsTimerExpired(ulTimer))
            {
                //vim_event_sleep(100);
                Wally_SessionFunc_AwaitRewardsId();
            }

            if (Wally_HasRewardsId())
            {
                VIM_DBG_MESSAGE("Got rewards ID, updating loyalty file");
                SaveLoyaltyNumber(l_sSessionData.sCustomer.szRewardsId, vim_strlen(l_sSessionData.sCustomer.szRewardsId), VIM_ICC_SESSION_UNKNOWN);
                eRet = ERR_LOYALTY_CARD_SUCESSFUL;
            }
            else
            {
                VIM_DBG_MESSAGE("Failed to get rewards ID");
                Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_TRUE);
            }
        }
        else
        {
            /*
            * NFC rewards tap - delete payment session as per the required flow.
            */
            Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_TRUE);
        }
    }
    else
    {
        VIM_DBG_MESSAGE("Already have EDP Rewards ID, no need to poll again");
    }

    return eRet;
}

VIM_ERROR_PTR Wally_Cancel(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    VIM_DBG_MESSAGE("Wally_Cancel");

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else
    {
        cJSON* pCJRequest = Wally_GenerateCancelRequest();

        if (Wally_WriteJSONRequestFile(pCJRequest))
        {
            //Keep synchronous - should be quick
            Wally_WaitForResponse();
            VIM_DBG_MESSAGE("We don't care about the response to a cancel");
            eRet = VIM_ERROR_NONE;
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Failed to send Cancel request to Wally App");
        }
        FREE_JSON(pCJRequest);
    }

    return eRet;
}

VIM_ERROR_PTR Wally_DeleteSession(const VIM_TEXT *pszPaymentSessionId, const VIM_TEXT *pszDeleteReason, VIM_BOOL fDisplayLoading)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    VIM_DBG_PRINTF2("Wally_DeleteSession '%s', '%s'", SAFE_STR(pszPaymentSessionId), SAFE_STR(pszDeleteReason));

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else
    {
        cJSON* pCJRequest = NULL;

        if(fDisplayLoading)
            Wally_Display_ProcessingNow();

        pCJRequest = Wally_GenerateDeleteSessionRequest(pszPaymentSessionId, pszDeleteReason);
        if (Wally_WriteJSONRequestFile(pCJRequest))
        {
            cJSON* pCJResponse = Wally_WaitForResponse();
            if (Wally_RespIsSuccess(pCJResponse))
            {
                pceft_debugf_test(pceftpos_handle.instance, "Delete session '%s' successful", SAFE_STR(pszPaymentSessionId));
                eRet = VIM_ERROR_NONE;
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Delete session '%s' failed", SAFE_STR(pszPaymentSessionId));
                eRet = VIM_ERROR_INVALID_DATA;
            }
            VAA_DeleteFile(WALLY_RSP_FILENAME);
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Failed to send Delete request to Wally App");
        }
        FREE_JSON(pCJRequest);
    }
    return eRet;
}

static VIM_ERROR_PTR Wally_HandleLegacyTransactionResponse(const cJSON* pCJStatusDetail, const char* pszResult)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    cJSON* pCJCreditCards = cJSON_GetObjectItem(pCJStatusDetail, "creditCards");
    cJSON* pCJCreditCard = cJSON_GetArrayItem(pCJCreditCards, 0);

    //Parse External Service Code
    if (cJSON_GetStringItemValue(pCJCreditCard, "externalServiceCode", txn.host_rc_asc, sizeof(txn.host_rc_asc)))
    {
        cJSON* pCJReceiptData = cJSON_GetObjectItem(pCJCreditCard, "receiptData");
        VIM_CHAR szCardSuffix[32 + 1] = { 0 };

        if (2 != vim_strlen(txn.host_rc_asc))
        {
            VIM_DBG_PRINTF1("Invalid response code [%s]; faking it", txn.host_rc_asc);
            vim_strcpy(txn.host_rc_asc, 0 == vim_strcmp(pszResult, "APPROVED") ? "00" : "01");
        }

        //Parse Card Suffix
        if (cJSON_GetStringItemValue(pCJReceiptData, "cardSuffix", szCardSuffix, sizeof(szCardSuffix)) && (0 < vim_strlen(szCardSuffix)))
        {
            cJSON* pCJExtendedData = cJSON_GetObjectItem(pCJCreditCard, "extendedTransactionData");
            VIM_CHAR szCardBin[32 + 1] = { 0 };
            int i, lArraySize = cJSON_GetArraySize(pCJExtendedData);

            //Parse Card BIN from extended data
            for (i = 0; i < lArraySize; ++i)
            {
                cJSON* pCJExtendedDatum = cJSON_GetArrayItem(pCJExtendedData, i);
                VIM_CHAR szField[32 + 1] = { 0 };
                if (cJSON_GetStringItemValue(pCJExtendedDatum, "field", szField, sizeof(szField)) && (0 == vim_strcmp(szField, "bin")))
                {
                    cJSON_GetStringItemValue(pCJExtendedDatum, "value", szCardBin, sizeof(szCardBin));
                    break;  // Stop looping once we've found the bin-range.
                }
            }

            if (0 < vim_strlen(szCardBin))
            {
                // Dummy PAN = first-six (bin), then 6 nines, then last-four, for BIN range lookup, and receipt-printing.
                VIM_CHAR szDummyPan[30] = { 0 };
                vim_strcpy(szDummyPan, szCardBin);
                vim_strcat(szDummyPan, "999999");
                vim_strcat(szDummyPan, szCardSuffix);

                vim_strcpy(txn.card.pci_data.ePan, szDummyPan);

                card_name_search(&txn, &txn.card);

                VIM_DBG_MESSAGE("Wally response parsed successfully");
                eRet = VIM_ERROR_NONE;
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debug_test(pceftpos_handle.instance, "Wally response missing card bin");
                eRet = VIM_ERROR_INVALID_DATA;
            }
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Wally response missing card suffix");
            eRet = VIM_ERROR_INVALID_DATA;
        }
    }
    else
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "Wally response Missing resp code - set up network error / decline.");
        eRet = VIM_ERROR_INVALID_DATA;
    }
    return eRet;
}

static const cJSON* Wally_FindMatchingResponse(const cJSON* pCJSubTransactions, const VIM_CHAR* pszLinkIdKey, const VIM_CHAR* pszInstrumentLinkId, const VIM_CHAR* pszResponseArrayKey, const cJSON** ppCJSubTransaction, VIM_BOOL fZ0Missing)
{
    if (pCJSubTransactions && pszLinkIdKey && pszInstrumentLinkId && pszResponseArrayKey)
    {
        VIM_INT32 i, j, lSubTranCount = cJSON_GetArraySize(pCJSubTransactions);
        for (i = 0; i < lSubTranCount; ++i)
        {
            const cJSON* pCJSubTransaction = cJSON_GetArrayItem(pCJSubTransactions, i);
            const cJSON* pCJResponses = cJSON_GetObjectItem(pCJSubTransaction, pszResponseArrayKey);
            VIM_INT32 lResponseCount = cJSON_GetArraySize(pCJResponses);
            for (j = 0; j < lResponseCount; ++j)
            {
                const cJSON* pCJPaymentResponse = cJSON_GetArrayItem(pCJResponses, j);
                const VIM_CHAR* pszPaymentResponseLinkId = VAA_GetJsonStringValue(pCJPaymentResponse, pszLinkIdKey);
                if (pszPaymentResponseLinkId && (0 == vim_strcmp(pszInstrumentLinkId, pszPaymentResponseLinkId)))
                {
                    if (ppCJSubTransaction)
                        *ppCJSubTransaction = pCJSubTransaction;
                    VIM_DBG_PRINTF3("Found matching Payment Response for '%s'='%s' in '%s'", SAFE_STR(pszLinkIdKey), SAFE_STR(pszInstrumentLinkId), SAFE_STR(pszResponseArrayKey));
                    return pCJPaymentResponse;
                }
            }
        }
    }
    else
    {
        VIM_DBG_MESSAGE("Null parameter");
    }

    pceft_debugf_test(pceftpos_handle.instance, "Failed to find matching SubTransaction for '%s'='%s'in '%s'", SAFE_STR(pszLinkIdKey), SAFE_STR(pszInstrumentLinkId), SAFE_STR(pszResponseArrayKey));
    return NULL;
}

static const cJSON* Wally_GetMatchingResponse(const cJSON* pCJInstrument, const cJSON* pCJInstrumentTransaction, const cJSON* pCJSubTransactions, const VIM_CHAR* pszLinkRefKey, const VIM_CHAR* pszResponseArrayKey, const cJSON** ppCJSubTransaction, VIM_BOOL fZ0Missing)
{
    //Matching Primarily by refundTransactionRef, then by paymentTransactionRef and finally by paymentInstrumentId
    const cJSON* pCJSubTransactionResponse = NULL;
    const VIM_CHAR* pszPaymentInstIdKey = "paymentInstrumentId";
    const VIM_CHAR* pszTranLinkRef      = VAA_GetJsonStringValue(pCJInstrumentTransaction, pszLinkRefKey);
    const VIM_CHAR* pszPaymentInstId    = VAA_GetJsonStringValue(pCJInstrument,            pszPaymentInstIdKey);

    pCJSubTransactionResponse                                 = Wally_FindMatchingResponse(pCJSubTransactions, pszLinkRefKey,       pszTranLinkRef,   pszResponseArrayKey, ppCJSubTransaction, fZ0Missing);
    if (!pCJSubTransactionResponse) pCJSubTransactionResponse = Wally_FindMatchingResponse(pCJSubTransactions, pszPaymentInstIdKey, pszPaymentInstId, pszResponseArrayKey, ppCJSubTransaction, fZ0Missing);

    if (!pCJSubTransactionResponse && fZ0Missing)
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "Failed to get matching %s Subtransaction data for instrument transaction ('%s'='%s', '%s'='%s')",
            SAFE_STR(pszResponseArrayKey),
            SAFE_STR(pszLinkRefKey),       SAFE_STR(pszTranLinkRef),
            SAFE_STR(pszPaymentInstIdKey), SAFE_STR(pszPaymentInstId));
    }

    return pCJSubTransactionResponse;
}

static cJSON* Wally_PopulateSubTransaction(cJSON* pCJOut, const cJSON* pCJInstrument, const cJSON* pCJInstrumentTransaction, const cJSON* pCJSubTransaction, const cJSON* pCJSubTransactionPaymentResponse)
{
    if (pCJOut)
    {
        VIM_CHAR szCardSuffix[6 + 1] = { 0 };
        VIM_CHAR szCardPrefix[8 + 1] = { 0 };

        const cJSON* pCJReceiptData      = cJSON_GetObjectItem(pCJSubTransactionPaymentResponse, "receiptData");
        const cJSON* pCJExtendedTranData = cJSON_GetObjectItem(pCJSubTransactionPaymentResponse, "extendedTransactionData");
        const cJSON* pCJInstrumentParent = pCJInstrument;
        const cJSON* pCJExternalServiceCodeParent = pCJSubTransactionPaymentResponse ? pCJSubTransactionPaymentResponse : pCJSubTransaction;
        const char* pszInstrumentTypeKey = "instrumentType";
        VIM_INT32 i, lExtendedTranArraySize = cJSON_GetArraySize(pCJExtendedTranData);

        if (!pCJReceiptData)
            pCJReceiptData = cJSON_GetObjectItem(pCJSubTransaction, "receiptData");

        //Find where the paymentInstrumentId/instrumentType objects are - depends on response type
        if (!cJSON_HasObjectItem(pCJInstrument, "paymentInstrumentId"))
        {
            const cJSON* pCJBasketError = cJSON_GetArrayItem(cJSON_GetObjectItem(pCJSubTransaction, "basketValidationErrors"), 0);
            if (cJSON_HasObjectItem(pCJBasketError, "paymentInstrumentId"))
            {
                pCJInstrumentParent = pCJBasketError;
                pszInstrumentTypeKey = "paymentInstrumentType";
            }
            else if (cJSON_HasObjectItem(pCJSubTransaction, "paymentInstrumentId"))
            {
                pCJInstrumentParent = pCJSubTransaction;
            }
            else if (cJSON_HasObjectItem(pCJSubTransactionPaymentResponse, "paymentInstrumentId"))
            {
                pCJInstrumentParent = pCJSubTransactionPaymentResponse;
            }
            else
            {
                VIM_DBG_MESSAGE("Couldn't find paymentInstrumentId");
            }
        }
        VAA_CopyCjsonItem      (pCJOut, "paymentInstrumentId", pCJInstrumentParent,                       VIM_TRUE);
        VAA_CopyCjsonItemRename(pCJOut, "instrumentType",      pCJInstrumentParent, pszInstrumentTypeKey, VIM_TRUE);

        VAA_CopyCjsonItem(pCJOut, "type",                  pCJInstrumentTransaction, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "amount",                pCJInstrumentTransaction, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "status",                pCJInstrumentTransaction, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "paymentTransactionRef", pCJInstrumentTransaction, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "refundTransactionRef",  pCJInstrumentTransaction, VIM_TRUE);

        VAA_CopyCjsonItem(pCJOut, "scheme",     pCJReceiptData, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "cardSuffix", pCJReceiptData, VIM_TRUE);

        VAA_CopyCjsonItem(pCJOut, "externalServiceCode",    pCJExternalServiceCodeParent, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "externalServiceMessage", pCJExternalServiceCodeParent, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "errorCode",              pCJExternalServiceCodeParent, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "errorDetail",            pCJExternalServiceCodeParent, VIM_TRUE);
        VAA_CopyCjsonItem(pCJOut, "errorMessage",           pCJExternalServiceCodeParent, VIM_TRUE);

        //Get data from Extended Tran Data Array
        for (i = 0; i < lExtendedTranArraySize; ++i)
        {
            const cJSON* pCJExtendedTranItem = cJSON_GetArrayItem(pCJExtendedTranData, i);
            const VIM_CHAR* pszField = VAA_GetJsonStringValue(pCJExtendedTranItem, "field");
            const VIM_CHAR* pszValue = VAA_GetJsonStringValue(pCJExtendedTranItem, "value");

            if (pszField && pszValue)
            {
                if (0 == vim_strcmp(pszField, "bin"))
                {
                    vim_strncpy(szCardPrefix, pszValue, sizeof(szCardPrefix) - 1);
                }
                else if (0 == vim_strcmp(pszField, "stan"))
                {
                    VAA_AddString(pCJOut, "stan", pszValue, VIM_FALSE);
                }
                else if (0 == vim_strcmp(pszField, "rrn"))
                {
                    VAA_AddString(pCJOut, "rrn", pszValue, VIM_FALSE);
                }
            }
        }

        //Build up masked PAN
        cJSON_GetStringItemValue(pCJReceiptData, "cardSuffix", szCardSuffix, sizeof(szCardSuffix));

        if (VAA_IsNullOrEmpty(szCardPrefix) || VAA_IsNullOrEmpty(szCardSuffix))
        {
            pceft_debugf_test(pceftpos_handle.instance, "Not enough card data to build masked PAN; prefix='%s', suffix='%s'", szCardPrefix, szCardSuffix);
        }
        else if (MAX_PAN_LENGTH < (vim_strlen(szCardPrefix) + vim_strlen(szCardSuffix)))
        {
            pceft_debugf_test(pceftpos_handle.instance, "Card data too long to build masked PAN; prefix='%s', suffix='%s'", szCardPrefix, szCardSuffix);
        }
        else
        {
            VIM_CHAR szMaskedPan[MAX_PAN_LENGTH + 1] = { 0 };

            vim_strcpy(szMaskedPan, szCardPrefix);
            vim_mem_set(&szMaskedPan[vim_strlen(szMaskedPan)], '9', MAX_PAN_LENGTH - vim_strlen(szCardPrefix) - vim_strlen(szCardSuffix));
            vim_strcat(szMaskedPan, szCardSuffix);

            VAA_AddString(pCJOut, "maskedPan", szMaskedPan, VIM_TRUE);
        }

        VAA_DebugJson("Populated Receipt SubTransaction Item", pCJOut);
    }
    else
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "Null populate subTran out Parameter");
    }
    return pCJOut;
}

static VIM_BOOL Wally_IsRestrictedItem(const cJSON* pCJ)
{
    VIM_BOOL fRestrictedItem = FALSE;
    const char* pszErrorCode = cJSON_GetStringValue(cJSON_GetObjectItem(pCJ, "errorCode"));
    if (!VAA_IsNullOrEmpty(pszErrorCode))
    {
        fRestrictedItem = (0 == vim_strncmp(pszErrorCode, "RP-", 3));
    }
    return fRestrictedItem;
}

static VIM_ERROR_PTR Wally_HandleTransactionResponse(const cJSON* pCJInstruments, cJSON* pCJSubTransactions, const char* pszResult, const char* pszRollback)
{
    cJSON* pCJReceiptDataSubTranArray = NULL;
    VIM_SIZE i, j, lInstrumentCount = cJSON_GetArraySize(pCJInstruments);
    VIM_BOOL fRestrictedItem = FALSE;
    VIM_INT32 lRecordCount = 0;

    FREE_JSON(l_pCJReceiptData);
    l_pCJReceiptData = cJSON_CreateObject();
    pCJReceiptDataSubTranArray = cJSON_AddArrayToObject(l_pCJReceiptData, "subTransactions");

    cJSON_AddStringToObject(l_pCJReceiptData, "rollback", pszRollback);

    //Create mapping between instruments[i].transactions[j] and subTransactions[n].(payment|refund)Responses[m]
    for (i = 0; i < lInstrumentCount; ++i)
    {
        const cJSON* pCJInstrument = cJSON_GetArrayItem(pCJInstruments, i);
        const cJSON* pCJInstrumentTransactions = cJSON_GetObjectItem(pCJInstrument, "transactions");
        VIM_INT32 lInstrumentTransactionCount = cJSON_GetArraySize(pCJInstrumentTransactions);
        for (j = 0; j < lInstrumentTransactionCount; ++j)
        {
            const cJSON* pCJInstrumentTransaction = cJSON_GetArrayItem(pCJInstrumentTransactions, j);
            const cJSON* pCJSubTransaction = NULL;
            const cJSON* pCJSubTransactionPaymentResponse = Wally_GetMatchingResponse(pCJInstrument, pCJInstrumentTransaction, pCJSubTransactions, "paymentTransactionRef", "paymentResponses", &pCJSubTransaction, VIM_TRUE);
            const cJSON* pCJSubTransactionRefundResponse  = Wally_GetMatchingResponse(pCJInstrument, pCJInstrumentTransaction, pCJSubTransactions, "refundTransactionRef",  "refundResponses",  &pCJSubTransaction, VIM_FALSE);
            cJSON* pCJNewSubTran = cJSON_CreateObject();

            if (pCJSubTransactionPaymentResponse || pCJSubTransactionRefundResponse)
            {
                //Populate up subtransaction with payment info, then override with refund info
                //This populates card data etc which may be missing from the refund info alone
                Wally_PopulateSubTransaction(pCJNewSubTran, pCJInstrument, pCJInstrumentTransaction, pCJSubTransaction, pCJSubTransactionPaymentResponse);
                if (pCJSubTransactionRefundResponse)
                {
                    VIM_DBG_MESSAGE("Refund detected, updating subtransaction from refund sub tran data");
                    Wally_PopulateSubTransaction(pCJNewSubTran, pCJInstrument, pCJInstrumentTransaction, pCJSubTransaction, pCJSubTransactionRefundResponse);
                }
            }

            if (!VAA_IsJsonNullOrEmpty(pCJNewSubTran))
            {
                ++lRecordCount;
                cJSON_AddItemToArray(pCJReceiptDataSubTranArray, pCJNewSubTran);
                if (Wally_IsRestrictedItem(pCJNewSubTran))
                    fRestrictedItem = TRUE;
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Failed to populate subtransaction for Instrument Transaction (%d, %d)", i, j);
                FREE_JSON(pCJNewSubTran);
            }
        }
    }

    //Run through subtransactions for any error scenarios that don't create mapping
    for (i = 0; i < cJSON_GetArraySize(pCJSubTransactions); ++i)
    {
        const cJSON* pCJSubTransaction = cJSON_GetArrayItem(pCJSubTransactions, i);
        if ((!cJSON_HasObjectItem(pCJSubTransaction, "paymentResponses") && !cJSON_HasObjectItem(pCJSubTransaction, "refundResponses")) ||
            lRecordCount == 0)
        {
            cJSON* pCJNewSubTran = cJSON_CreateObject();
            Wally_PopulateSubTransaction(pCJNewSubTran, NULL, NULL, pCJSubTransaction, NULL);
            if (!VAA_IsJsonNullOrEmpty(pCJNewSubTran))
            {
                cJSON_AddItemToArray(pCJReceiptDataSubTranArray, pCJNewSubTran);
                if (Wally_IsRestrictedItem(pCJNewSubTran))
                    fRestrictedItem = TRUE;
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Failed to populate subtransaction for error subtran (%d)", i);
                FREE_JSON(pCJNewSubTran);
            }
        }
    }

    VAA_DebugJson("Populated EDP Receipt data", l_pCJReceiptData);

    if (pszResult && (0 == vim_strcmp(pszResult, "APPROVED")))
    {
        vim_strcpy(txn.host_rc_asc, "00");
        txn.error = VIM_ERROR_NONE;
        if (fRestrictedItem)
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "WARNING!!! EDP Transaction approved, but also contains restricted items! This shouldn't happen");
        }
    }
    else
    {
        vim_strcpy(txn.host_rc_asc, fRestrictedItem ? "RI" : "01");
        txn.error = ERR_WOW_ERC;
    }

    return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR Wally_ParsePaymentResponse(const cJSON* pCJ)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NULL;
    if (pCJ)
    {
        cJSON* pCJResponse = cJSON_GetObjectItem(pCJ, "response");
        VIM_CHAR szResult[128 + 1] = { 0 };

        if (cJSON_GetStringItemValue(pCJResponse, "transactionStatus", szResult, sizeof(szResult)))
        {
            cJSON* pCJStatusDetail    = cJSON_GetObjectItem(pCJResponse, "statusDetail");
            cJSON* pCJInstruments     = cJSON_GetObjectItem(pCJResponse, "instruments");
            cJSON* pCJSubTransactions = cJSON_GetObjectItem(pCJResponse, "subTransactions");

            if (pCJStatusDetail)
            {
                pceft_debug_test(pceftpos_handle.instance, "Legacy Wally transaction data detected");
                eRet = Wally_HandleLegacyTransactionResponse(pCJStatusDetail, szResult);
            }
            else if(pCJInstruments && pCJSubTransactions)
            {
                pceft_debug_test(pceftpos_handle.instance, "New Format Wally transaction data detected");
                eRet = Wally_HandleTransactionResponse(pCJInstruments, pCJSubTransactions, szResult, VAA_GetJsonStringValue(pCJResponse, "rollback"));
            }
            else {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debug_test(pceftpos_handle.instance, "Missing Wally transaction data");
                eRet = VIM_ERROR_INVALID_DATA;
            }
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Wally Transaction response invalid (or missing transactionStatus)");
            eRet = VIM_ERROR_INVALID_DATA;
        }
    }
    return eRet;
}

VIM_ERROR_PTR Wally_DisplayResult()
{
    // Display result and wait for a keypress.
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    if (0 == vim_strcmp(txn.host_rc_asc, "00"))
    {
        eRet = display_screen(DISP_IMAGES_AND_OVERLAY, VIM_PCEFTPOS_KEY_NONE, "QrPaid.png", "QrPaidOverlay.gif", "88", "177");
    }
    else
    {
        const char* pszImage;
        if (0 == vim_strcmp(txn.host_rc_asc, "RA"))
            pszImage = "QrPayErrorNW.png";
        else if (0 == vim_strcmp(txn.host_rc_asc, "RI"))
            pszImage = "QrPayErrorItem.png";
        else
            pszImage = "QrPayError.png";

        eRet = display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, pszImage);
        //vim_event_sleep(THREE_SECOND_DELAY);
    }
    vim_event_sleep(THREE_SECOND_DELAY);
    return eRet;
}

static void add_PR_category( VIM_CHAR *pszResultString, VIM_CHAR *pszCategory, int iBufferSize )
{
    // Check for NULL string pointers, and enough room for both strings, plus a comma, plus a null-terminator.
    if ( pszResultString && pszCategory &&
         ( vim_strlen(pszResultString) + vim_strlen(pszCategory) + 2 < iBufferSize ) )
    {
        if ( vim_strlen( pszResultString ) )
        {
            // Already have some data, so append a comma first.
            vim_strcat( pszResultString, "," );
        }

        vim_strcat( pszResultString, pszCategory );
    }
    else
    {
        VIM_DBG_PRINTF3( "Pointer %p or %p NULL, or %d buffer is too small", pszResultString, pszCategory, iBufferSize );
    }
}

// Examines txn PAD data to generate product-restriction categories info.
// If not NULL, the returned pointer needs to be freed after use.
static VIM_CHAR *Wally_Get_PR_categories( void )
{
    VIM_CHAR *pszRetVal = NULL;

#define PR_MALLOC_BUFF_SIZE    1000

    VIM_DBG_PRINTF1( "PAD data length %d", vim_strlen(l_sSessionData.szPAD_data) );

    if ( vim_strlen(l_sSessionData.szPAD_data) > 0 )
    {
        VIM_CHAR *pszBsk = NULL;

        pszRetVal = (VIM_CHAR *)malloc( PR_MALLOC_BUFF_SIZE );
        if ( !pszRetVal )
        {
            VIM_DBG_MESSAGE( "Malloc error!" );     // Should not happen.
            return NULL;
        }

        vim_mem_clear( pszRetVal, PR_MALLOC_BUFF_SIZE );

        // PAD data exists - parse it.
        VIM_DBG_PRINTF1( "PAD data: [%s]", l_sSessionData.szPAD_data );

        if ( ( vim_strstr( l_sSessionData.szPAD_data, "BSK", &pszBsk ) == VIM_ERROR_NONE ) &&
             ( pszBsk != NULL ) )
        {
            int iBskSize = 0;

            // Got BSK container - check for product-restriction data inside it.
            pszBsk += 3;    // Step past 'BSK' to get to the length.
            iBskSize = VAA_get_PAD_size( pszBsk );

            // If we don't have at least 12 chars in BSK, we can't possibly have any PR categories.
            // We need 6 for PRD+len, another 6 for PID+len, and at least 1 for product-id (probably 2).
            if ( iBskSize > 12 )
            {
                // Parse only the BSK payload to find our PR data.
                char *pszBasketEnd = pszBsk + iBskSize;     // Used to ensure we don't search beyond BSK.
                char *pszProduct = NULL;

                if ( vim_strstr( pszBsk, "PRD", &pszProduct ) == VIM_ERROR_NONE )
                {
                    VIM_CHAR *pszProdId = NULL;

                    while ( ( pszProduct != NULL ) && ( pszProduct < pszBasketEnd ) )
                    {
                        VIM_ERROR_PTR err = vim_strstr( pszProduct, "PID", &pszProdId );
                        
                        if ( err == VIM_ERROR_NONE )
                        {
                            if ( pszProdId )
                            {
                                int iProdIdLen = 0;
                                VIM_CHAR szProdCat[100] = { 0 };    // Any upper limit to this? Surely 100 is enough?
                                
                                pszProdId += 3;    // Step past 'PID' to get to the id-length.
                                iProdIdLen = VAA_get_PAD_size( pszProdId );
                                pszProdId += 3;     // Step past the length field.

                                // Copy the product-id string (typically numeric, but we don't assume),
                                // into a null-terminated string buffer, and add it to the cJSON fragment.

                                vim_strncpy( szProdCat, pszProdId, iProdIdLen );
                                VIM_DBG_PRINTF1( "Adding PR category [%s]", szProdCat );

                                add_PR_category( pszRetVal, szProdCat, PR_MALLOC_BUFF_SIZE );

                                // Advance pszProduct pointer to next PRD (start after PID info)
                                if ( vim_strstr( pszProdId + iProdIdLen, "PRD", &pszProduct ) != VIM_ERROR_NONE )
                                {
                                    DBG_MESSAGE( err->name );   // Abort, on vim_strstr error - should not happen
                                    pszProduct = NULL;          // Exit the loop.
                                }
                            }
                            else
                            {
                                DBG_MESSAGE( "No PID found, look for next PRD" );
                                // Advance pszProduct pointer (start after prior PRD+length).
                                if ( vim_strstr( pszProduct + 6, "PRD", &pszProduct ) != VIM_ERROR_NONE )
                                {
                                    DBG_MESSAGE( err->name );   // Abort, on vim_strstr error - should not happen
                                    pszProduct = NULL;          // Exit the loop.
                                }
                            }
                        }
                        else
                        {
                            DBG_MESSAGE( err->name );   // Abort, on vim_strstr error - should not happen
                            pszProduct = NULL;          // Exit the loop.
                        }
                    }
                }
            }
            else
            {
                DBG_MESSAGE( "BSK too small to contain any product-restrictions." );
                VIM_DBG_PRINTF1( "iBskSize = %d", iBskSize );
            }
        }
        else
        {
            DBG_MESSAGE( "No BSK found" );
        }
    }

    return pszRetVal;
}


VIM_ERROR_PTR Wally_DoTransaction(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    VIM_DBG_MESSAGE("Wally_DoTransaction");

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else
    {
        cJSON* pCJRequest = NULL;

        Wally_Display_ProcessingNow();

        FREE_JSON(l_pCJReceiptData);

        // Indicate that this is a qr-code transaction.
        txn.card.type = card_qr;
        txn.card.pci_data.eTokenFlag = VIM_FALSE;       // PAN, not token.
        txn.print_receipt = VIM_TRUE;
        vim_mem_clear(txn.host_rc_asc, sizeof(txn.host_rc_asc));

        // Delete any leftover Wally request and response files
        VAA_DeleteFile(WALLY_RSP_FILENAME);
        VAA_DeleteFile(WALLY_REQ_FILENAME);

        switch(txn.type)
        {
        case tt_sale:   pCJRequest = Wally_GenerateDoPaymentRequest(); break;
        case tt_refund: pCJRequest = Wally_GenerateDoRefundRequest();  break;
        default:
            pceft_debug_test(pceftpos_handle.instance, "Unexpected EDP Transaction Type");
            break;
        }
        
        if (pCJRequest)
        {
            pceft_debug_test(pceftpos_handle.instance, "EDPTiming: Sending transaction request to Wally");
            if (Wally_WriteJSONRequestFile(pCJRequest))
            {
                cJSON* pCJ  = Wally_WaitForResponse();
                if (pCJ)
                {
                    pceft_debug_test(pceftpos_handle.instance, "EDPTiming: Received transaction response from Wally");
                    eRet = Wally_ParsePaymentResponse(pCJ);
                    FREE_JSON(pCJ);
                    VAA_DeleteFile(WALLY_RSP_FILENAME);
                }
                else
                {
                    pceft_debug_test(pceftpos_handle.instance, "EDPTiming: Wally Transaction response timed out");
                    eRet = VIM_ERROR_TIMEOUT;
                }
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debug_test(pceftpos_handle.instance, "Failed to send Transaction request to Wally App");
                eRet = VIM_ERROR_SYSTEM;
            }
            FREE_JSON(pCJRequest);
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Failed to Create EverydayPay Transaction request");
            eRet = VIM_ERROR_INVALID_DATA;
        }
        if (VIM_ERROR_NONE != eRet)
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Error receiving/parsing Wally transaction response");
            if (2 != vim_strlen(txn.host_rc_asc))
            {
                pceft_debug_test(pceftpos_handle.instance, "Setting response code 'RA'");
                vim_strcpy(txn.host_rc_asc, "RA");
            }
        }
    }

    txn.error = eRet;
    return txn.error;
}


VIM_ERROR_PTR Wally_SessionStateMachine(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    if (!TERM_QR_WALLET)
    {
        pceft_debug_test(pceftpos_handle.instance, "Everyday Pay Wallet not enabled - exiting immediately");
        eRet = VIM_ERROR_NOT_INTIALIZED;
    }
    else
    {
        VAA_CheckDebug();

        switch (l_sSessionData.eState)
        {
        case SESSION_IDLE:                  eRet = VIM_ERROR_NONE;                      break; //If we're here, something's gone wrong along the way. Wally response timeout?
        case SESSION_AWAITING_QR_CODE:      eRet = Wally_SessionFunc_AwaitQRResponse(); break;
        case SESSION_AWAITING_REWARDS_DATA: eRet = Wally_SessionFunc_AwaitRewardsId();  break;
        default:
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "Unexpected state %u", l_sSessionData.eState);
            break;
        }

        if ((VIM_ERROR_NONE == eRet) || (ERR_LOYALTY_CARD_SUCESSFUL == eRet) || (ERR_QR_RECV_SUCCESSFUL == eRet))
        {
            //Expected state
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "Retrieved error '%s' when in Wally session state %d. Resetting session", eRet->name, l_sSessionData.eState);
            Wally_SessionFunc_ResetSessionData(l_sSessionData.fEnableNFC, VIM_TRUE);
        }
    }

    return eRet;
}

VIM_ERROR_PTR Wally_GetPaymentSessionIdFromPAD(const VIM_CHAR* pszInPADData, VIM_CHAR* pszOutPaymentSessionId, VIM_UINT32 ulBufSize)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NULL;

    VIM_DBG_PRINTF3("Wally_GetPaymentSessionIdFromPAD(%s, %x, %u)", SAFE_STR(pszInPADData), pszOutPaymentSessionId, ulBufSize);

    if (pszOutPaymentSessionId)
    {
        vim_mem_set(pszOutPaymentSessionId, 0x00, ulBufSize);
    }

    if (pszInPADData)
    {
        VIM_CHAR* Ptr = NULL;
        if (VIM_ERROR_NONE == vim_strstr(pszInPADData, "WWE", &Ptr))
        {
            if (VIM_ERROR_NONE == vim_strstr(pszInPADData, "PSI", &Ptr))
            {
                VIM_CHAR szLen[3 + 1] = { 0 };
                VIM_UINT32 ulLen = 0;

                Ptr += 3;
                vim_mem_copy(szLen, Ptr, 3);
                ulLen = vim_atol(szLen);
                if ((UUID_LEN == ulLen) || (UUID_NO_HYPHEN_LEN == ulLen))
                {
                    if (pszOutPaymentSessionId)
                    {
                        VIM_CHAR szPaymentSessionId[UUID_LEN + 1] = { 0 };
                        Ptr += 3;
                        vim_mem_copy(szPaymentSessionId, Ptr, ulLen);
                        if (VAA_GetUUIDWithHyphens(szPaymentSessionId, pszOutPaymentSessionId, ulBufSize))
                        {
                            VIM_DBG_PRINTF1("Successfully retrieved Payment Session ID '%s' from PAD data", pszOutPaymentSessionId);
                            eRet = VIM_ERROR_NONE;
                        }
                        else
                        {
                            VIM_DBG_PRINTF1("Failed to add hyphens to '%s'", szPaymentSessionId);
                            eRet = VIM_ERROR_INVALID_DATA;
                        }
                    }
                    else
                    {
                        VIM_DBG_MESSAGE("Found valid Payment Session ID in PAD data");
                        eRet = VIM_ERROR_NONE;
                    }
                }
                else
                {
                    VIM_DBG_PRINTF3("Unexpected length (%u) for Everyday Pay Payment Session ID (expecting %u or %u)", ulLen, UUID_LEN, UUID_NO_HYPHEN_LEN);
                    eRet = VIM_ERROR_INVALID_DATA;
                }
            }
            else
            {
                VIM_DBG_MESSAGE("Everyday Pay PAD tag does not contain Payment Session ID tag");
                eRet = VIM_ERROR_INVALID_DATA;
            }
        }
        else
        {
            VIM_DBG_MESSAGE("PAD data does not contain Everyday Pay container tag");
            eRet = VIM_ERROR_INVALID_DATA;
        }
    }
    else
    {
        VIM_DBG_MESSAGE("Null parameter");
    }

    return eRet;
}

static VIM_BOOL Wally_AppendPADTag(VIM_CHAR* pszDest, VIM_SIZE ulDestSize, const VIM_CHAR* pszTag, const VIM_CHAR* pszValue)
{
    VIM_BOOL fSuccess = FALSE;
    VIM_DBG_PRINTF4("Wally_AppendPADTag(%s, %u, %s, %s)", SAFE_STR(pszDest), ulDestSize, SAFE_STR(pszTag), SAFE_STR(pszValue));
    if (pszDest && pszTag && pszValue)
    {
        if (3 == vim_strlen(pszTag))
        {
            VIM_SIZE ulValueLen = vim_strlen(pszValue);
            VIM_SIZE ulOffset   = vim_strlen(pszDest);
            if ((ulOffset + 6 + ulValueLen) < ulDestSize)
            {
                fSuccess = (VIM_ERROR_NONE == vim_snprintf(&pszDest[ulOffset], ulDestSize - ulOffset, "%s%03u%s", pszTag, ulValueLen, pszValue));
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "PAD Data '%s':'%s' will exceed destination buffer size (%u)", pszTag, pszValue, ulDestSize);
            }
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "Invalid tag length '%s'", pszTag);
        }
    }
    else
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "Invalid parameter");
    }
    return fSuccess;
}

VIM_BOOL Wally_GetSessionPADData(VIM_CHAR* pszPAD, VIM_SIZE ulPADSize)
{
    VIM_BOOL fSuccess = FALSE;
    if (pszPAD)
    {
        vim_mem_set(pszPAD, 0x00, ulPADSize);
        if (SESSION_IDLE == l_sSessionData.eState)
        {
            if (Wally_HasRewardsId())
            {
                VIM_CHAR szPaymentSessionIdNoHyphens[UUID_NO_HYPHEN_LEN + 1] = { 0 };

                fSuccess = VAA_GetUUIDNoHyphens(l_sSessionData.szPaymentSessionId, szPaymentSessionIdNoHyphens, sizeof(szPaymentSessionIdNoHyphens));
                fSuccess &= Wally_AppendPADTag(pszPAD, ulPADSize, "PSI", szPaymentSessionIdNoHyphens);
                //fSuccess &= Wally_AppendPADTag(pszPAD, ulPADSize, "USE", l_sSessionData.sCustomer.fUseRewardsPoints ? "1" : "0"); //TODO: This can't fit in the response yet (limited to 50 bytes by VIM)
            }
            else
            {
                pceft_debug_test(pceftpos_handle.instance, "QR Code not scanned");
            }
        }
        else
        {
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debug_test(pceftpos_handle.instance, "Wally state machine not at idle");
        }
    }
    else
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debug_test(pceftpos_handle.instance, "Invalid parameter");
    }
    return fSuccess;
}

const VIM_CHAR* Wally_GetEdpRollback(void)
{
    return VAA_GetJsonStringValue(l_pCJReceiptData, "rollback");
}

VIM_SIZE Wally_GetEdpSubTransactionCount(void)
{
    return cJSON_GetArraySize(cJSON_GetObjectItem(l_pCJReceiptData, "subTransactions"));
}

const cJSON* Wally_GetEdpSubTransaction(VIM_SIZE ulIdx, transaction_t* psTxn)
{
    cJSON* pCJSubTransaction = NULL;

    pCJSubTransaction = cJSON_GetArrayItem(cJSON_GetObjectItem(l_pCJReceiptData, "subTransactions"), ulIdx);
    if (pCJSubTransaction)
    {
        if (psTxn)
        {
            const VIM_CHAR* pszResponseCode  = VAA_GetJsonStringValue(pCJSubTransaction, "externalServiceCode");
            const VIM_CHAR* pszType          = VAA_GetJsonStringValue(pCJSubTransaction, "type");
            const VIM_CHAR* pszStan          = VAA_GetJsonStringValue(pCJSubTransaction, "stan");
            const VIM_CHAR* pszCardType      = VAA_GetJsonStringValue(pCJSubTransaction, "instrumentType");
            const VIM_CHAR* pszCardMaskedPan = VAA_GetJsonStringValue(pCJSubTransaction, "maskedPan");
            double dAmount = 0;

            txn_do_init(psTxn);

            if ( VAA_IsNullOrEmpty( pszResponseCode ) )
            {
                if (Wally_IsRestrictedItem(pCJSubTransaction))
                {
                    DBG_MESSAGE("Converting RC 'RP-*' to 'RI'");
                    vim_strcpy(psTxn->host_rc_asc, "RI");
                }
            }

            psTxn->type = tt_sale;
            if (pszType)
            {
                if (0 == vim_strcmp(pszType, "REFUND"))
                {
                    VIM_DBG_MESSAGE("Subtransaction 'type' is refund");
                    psTxn->type = tt_refund;
                }
                else if (0 == vim_strcmp(pszType, "PAYMENT"))
                {
                    VIM_DBG_MESSAGE("Subtransaction 'type' is sale");
                }
                else
                {
                    // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                    pceft_debugf_test(pceftpos_handle.instance, "Unknown subTransaction 'type' (%s), defaulting to sale", pszType);
                }
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debug_test(pceftpos_handle.instance, "No subTransaction 'type', defaulting to sale");
            }

            psTxn->card.type = card_qr;
            psTxn->card.pci_data.eTokenFlag = VIM_FALSE;       // PAN, not token.

            psTxn->account = account_credit;
            if (pszCardType && (0 == vim_strcmp(pszCardType, "GIFT_CARD")))
            {
                psTxn->account = account_savings;
            }

            if (pszResponseCode)
            {
                if (2 == vim_strlen(pszResponseCode))
                {
                    vim_strcpy(psTxn->host_rc_asc, pszResponseCode);
                }
                else
                {
                    // Only set up host_rc_asc if it hasn't been set up already.
                    if ( vim_strlen(psTxn->host_rc_asc) != 2 )
                    {
                        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                        pceft_debugf_test(pceftpos_handle.instance, "Invalid subtran response code '%s'. Setting '01'", pszResponseCode);
                        vim_strcpy(psTxn->host_rc_asc, "01");
                    }
                }
                psTxn->error = APPROVED_RESPONSE_CODE(psTxn->host_rc_asc) ? VIM_ERROR_NONE : ERR_WOW_ERC;
            }
            else
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Missing subtran response code '%s'. Setting 'RA'", psTxn->host_rc_asc);
                vim_strcpy(psTxn->host_rc_asc, "RA");
                psTxn->error = VIM_ERROR_NO_LINK;
            }

            cJSON_GetNumberItemValue(pCJSubTransaction, "amount", &dAmount);
            psTxn->amount_purchase = (VIM_UINT64)(dAmount * 100);
            psTxn->amount_total = psTxn->amount_purchase;

            if (pszStan)
            {
                psTxn->stan = vim_atol(pszStan);
            }

            if (pszCardMaskedPan)
            {
                vim_strncpy(psTxn->card.pci_data.ePan, pszCardMaskedPan, sizeof(psTxn->card.pci_data.ePan) - 1);
                card_name_search(psTxn, &psTxn->card);
            }
        }
    }
    else
    {
        VIM_DBG_PRINTF1("Unable to find EDP payment Response %u", ulIdx);
    }

    return pCJSubTransaction;
}

#define EDP_APP_FNAME "82003.exe"


VIM_BOOL EDP_IsInstalled(VIM_BOOL fReport)
{
    static VIM_INT8 s_cEDPAppInstalled = -1;
    if (0 > s_cEDPAppInstalled)
    {
        if (VAA_FileExists(EDP_APP_FNAME))
        {
            if (fReport)
                pceft_debug_test(pceftpos_handle.instance, "EDP exe present");
            s_cEDPAppInstalled = 1;
        }
        else
        {
            if (fReport)
            {
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debug_test(pceftpos_handle.instance, "EDP exe not present");
            }
            s_cEDPAppInstalled = 0;
        }
    }
    return (1 == s_cEDPAppInstalled) ? VIM_TRUE : VIM_FALSE;
}

void Wally_UpdateConfig(void)
{
    VIM_SIZE FileSize = VIM_SIZEOF(TERM_BRAND);
    VIM_FILE_HANDLE file_handle;
    VIM_BOOL Exists = VIM_FALSE;
    VIM_ERROR_PTR res;
    vim_file_exists(EDPData, &Exists);
    if (Exists)
    {
        vim_file_delete(EDPData);
    }
    if ((res = vim_file_open(&file_handle, EDPData, VIM_TRUE, VIM_TRUE, VIM_TRUE)) == VIM_ERROR_NONE)
    {
        res = vim_file_write(file_handle.instance, TERM_BRAND, FileSize);
        vim_file_close(file_handle.instance);
    }
    if (res != VIM_ERROR_NONE)
    {
        pceft_debug_test(pceftpos_handle.instance, "Failed to add Banner string into wally.data");
        vim_file_delete(EDPData);
    }
    Wally_BuildJSONRequestOfRegistrytoFIle();
}

VIM_ERROR_PTR Wally_RewardsRemoveDialogue(const VIM_CHAR* psSessionId)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_INVALID_DATA;

//    pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", psSessionId, SAFE_STR(l_sSessionData.szPaymentSessionId));

#if WALLY_WALLET_REMOVE_VALIDATE_ID == 1
    if (vim_strcmp(l_sSessionData.szPaymentSessionId, psSessionId) == 0)
#else
    if (!VAA_IsNullOrEmpty(psSessionId))
#endif
    {
        VIM_EVENT_FLAG_PTR pceftpos_event_ptr;
        VIM_EVENT_FLAG_PTR event_list[1];
        VIM_KBD_CODE key;
        VIM_MILLISECONDS userTimeoutMs = (WALLY_WALLET_REMOVE_TIMEOUT_SEC * 1000);
        VIM_ERROR_PTR res;

        display_screen_cust_pcx(DISP_EDP_REMOVE, "Label", EDP_THANKS_PASS_SCREEN, VIM_PCEFTPOS_KEY_CANCEL);
        vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pceftpos_event_ptr);
        event_list[0] = pceftpos_event_ptr;
        eRet = VIM_ERROR_SYSTEM;
        if ((res = vim_adkgui_kbd_touch_or_other_read(&key, userTimeoutMs, event_list, 1)) == VIM_ERROR_NONE)
        {
            if (key == VIM_KBD_CODE_CANCEL)
            {
                // User Pressed the CANCEL key
                // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "", psSessionId);
                display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, "QrRemovingEDP.gif");
                eRet = Wally_SessionFunc_ResetSessionData(VIM_FALSE, VIM_TRUE);
                if (eRet == VIM_ERROR_NONE)
                {
                    // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                    pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "Succeeded", psSessionId);
                    display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_NONE, "QrEDPRemoved.png");
                }
                else
                {
                    // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
                    pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "Failed", psSessionId);
                    display_screen(DISP_IMAGES, VIM_PCEFTPOS_KEY_YES, "QrPayErrorNW.png");
                }
            }
            else if (*event_list)
            {
                // Incoming message from POS
                eRet = VIM_ERROR_POS_CANCEL;
                pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "Canceled by POS", psSessionId);
            }
        }
        else if (res == VIM_ERROR_TIMEOUT)
        {
            eRet = VIM_ERROR_USER_IDLE;
            // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
            pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "Canceled by User inactivity", psSessionId);
        }
    }
    else 
    {
        // RDD 28/04/23 JIRA PIRPIN-1731 migrated 
        pceft_debugf_test(pceftpos_handle.instance, "Removing EDP wallet %s: Payment Session ID=%s", "Failed. Invalid Session ID", SAFE_STR(psSessionId));
    }
    return eRet;
}
