// RDD 031220 Add New File for Qwikcilver GiftCard Processing 
#include <incs.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "cJSON_local.h"

#define QC_APP_FNAME "82002.exe"


#define QC_VER_FNAME VIM_FILE_PATH_LOCAL "Qwikcilver.ver"

#define QC_REQ_FNAME VIM_FILE_PATH_LOCAL "Qwikcilver_Req.json"
#define QC_RSP_FNAME VIM_FILE_PATH_LOCAL "Qwikcilver_Resp.json"

#define QC_0600_REQ_FILE    "Qwikcilver_0600_req.txt"
#define QC_0600_RESP_FILE   "Qwikcilver_0600_resp.txt"
#define QC_0610_NOTIFY_FILE "Qwikcilver_0610_notify.txt"


#define QC_0600_REQ_FILE_PATH    VIM_FILE_PATH_LOCAL QC_0600_REQ_FILE
#define QC_0600_RESP_FILE_PATH   VIM_FILE_PATH_LOCAL QC_0600_RESP_FILE
#define QC_0610_NOTIFY_FILE_PATH VIM_FILE_PATH_LOCAL QC_0610_NOTIFY_FILE

//#define QC_TEST_TERMINAL_ID "Linkly:Dev:POS:01"

#define BARCODE_IIN_LEN_A   13
#define BARCODE_IIN_LEN_B   11
#define QC_CARD_DATA_LEN 40 // From PCEFTPOS PinPad Dev guide
#define PAD_META_LEN        6

// Container JSON Keys
#define JSON_KEY_PA_META       "meta"
#define JSON_KEY_PA_REQUEST    "request"
#define JSON_KEY_PA_RESPONSE   "response"
#define JSON_KEY_PA_ERRORS     "errors"
#define JSON_KEY_PA_ERROR_LIST "errorlist"


// Latest OTP we have is stored in this buffer.
static VIM_CHAR l_szOTP[32];

extern VIM_SIZE PcEFTBin(const transaction_t* transaction);
extern VIM_BOOL IsReturnsCard(transaction_t* transaction);


#if 1

VIM_BOOL QC_IsInstalled(VIM_BOOL fReport)
{
    static VIM_INT8 s_cQCAppInstalled = -1;
    if (0 > s_cQCAppInstalled)
    {
        if (VAA_FileExists(QC_APP_FNAME))
        {
            if( fReport )
                pceft_debug(pceftpos_handle.instance, "Qwikcilver present");
            s_cQCAppInstalled = 1;
        }
        else
        {
            if (fReport)
                pceft_debug(pceftpos_handle.instance, "Qwikcilver not present");
            s_cQCAppInstalled = 0;
        }
    }
    return (1 == s_cQCAppInstalled) ? VIM_TRUE : VIM_FALSE;
}

#endif


VIM_BOOL QC_HasCreds(void)
{
	return(vim_file_is_present("QC_Creds.bak"));
}


static VIM_BOOL QC_WriteJSONRequestFile(const cJSON* pCJRequest)
{
	vim_file_delete(QC_RSP_FNAME);
    return VAA_WriteJSONRequestFile(QC_REQ_FNAME, pCJRequest);
}

/*
static cJSON* QC_ReadJSONResponseFile(void)
{
    cJSON* pCJ = VAA_ReadJSONResponseFile(QC_RSP_FNAME);
    remove(QC_REQ_FNAME);
    return pCJ;
}
*/

static cJSON* QC_WaitForResponse(void)
{
    cJSON* pCJResponse = NULL;
    //if (QC_IsInstalled(VIM_FALSE))
    if (1)
    {
        //static VIM_BOOL fFirst = VIM_TRUE;

        // RDD 300523 JIRA PIRPIN-2401 Increase timeout to 30 secs and decrease REST timeout in 82002 json config to 5 secs
        pCJResponse = VAA_WaitForJSONResponse( QC_RSP_FNAME, 30, VIM_TRUE );
        remove(QC_REQ_FNAME);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Qwikcilver app not installed - aborting wait for response");
    }
    return pCJResponse;
}


static void QC_SendAndForget(const cJSON* pCJRequest)
{
    if (QC_WriteJSONRequestFile(pCJRequest))
    {
        pceft_debug(pceftpos_handle.instance, "Wrote QC request");
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to write QC request");
    }
}


static cJSON* QC_SendAndReceive(const cJSON* pCJRequest)
{
    cJSON* pCJResponse = NULL;

    if (QC_WriteJSONRequestFile(pCJRequest))
    {
        pCJResponse = QC_WaitForResponse();
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to write QC request");
    }

    return pCJResponse;
}

static VIM_BOOL QC_AddMetaData(cJSON* psCJFileRequest, const VIM_CHAR* pszCommand )
{
    VIM_BOOL fSuccess = VIM_FALSE;

    if (psCJFileRequest && pszCommand)
    {
        char szTerminalID[8 + 1] = { 0 };
        static VIM_UINT32 s_ulMessageID = 0;
        cJSON* psCJMeta = cJSON_CreateObject();

        vim_strncpy(szTerminalID, terminal.terminal_id, 8);

        fSuccess = VIM_TRUE;
        fSuccess &= VAA_AddString(psCJMeta, "Command",    pszCommand,       VIM_TRUE);
        fSuccess &= VAA_AddNumber(psCJMeta, "MessageId",  ++s_ulMessageID,  VIM_TRUE);
        fSuccess &= VAA_AddString(psCJMeta, "TerminalId", szTerminalID,     VIM_TRUE);
        fSuccess &= VAA_AddString(psCJMeta, "HostId",     terminal.host_id, VIM_FALSE);

        // RDD 190721 JIRA PIRPIN-1123 : Additional info to go into every request header
        fSuccess &= VAA_AddString(psCJMeta, "MerchantName", terminal.configuration.merchant_name, VIM_TRUE);
        fSuccess &= VAA_AddString(psCJMeta, "MerchantID", terminal.merchant_id, VIM_TRUE);
        {
            VIM_CHAR StoreID[5 + 1] = { 0,0,0,0,0 };    // RDD Fomat XNNNN X=L,A,F,0 ( Live Gp, ALH, EG Gp, WOW ) NNNN = 4 digit Store ID
            vim_mem_copy(StoreID, &terminal.merchant_id[10], 5 );
            fSuccess &= VAA_AddString(psCJMeta, "StoreID", StoreID, VIM_TRUE);
        }
#if 0
        if( vim_strlen(TERM_QC_USERNAME) && vim_strlen(TERM_QC_PASSWORD) )
        {
            pceft_debug_test(pceftpos_handle.instance, "QC Setting up LFD Creds");
            fSuccess &= VAA_AddString(psCJMeta, "UserName", TERM_QC_USERNAME, VIM_TRUE);            // Optional for testing only and backup
            fSuccess &= VAA_AddString(psCJMeta, "Password", TERM_QC_PASSWORD, VIM_TRUE);
        }
        else
        {
            pceft_debug_test(pceftpos_handle.instance, "QC No LFD Creds");
        }
#endif
        fSuccess &= VAA_AddItem(psCJFileRequest, "meta", psCJMeta, VIM_TRUE);
    }
    else
    {
        VIM_DBG_MESSAGE("Null parameter");
    }

    return fSuccess;
}

static VIM_SIZE QC_GetLengthFromPADTag(const VIM_CHAR* pszTag)
{
    VIM_SIZE ulLen = 0;
    if (pszTag)
    {
        if (6 <= vim_strlen(pszTag))
        {
            VIM_CHAR szLen[3 + 1] = { 0 };
            vim_mem_copy(szLen, pszTag + 3, 3);
            ulLen = vim_atol(szLen);
        }
    }
    return ulLen;
}

static VIM_ERROR_PTR QC_RemoveTagFromPAD(transaction_t* psTxn, VIM_CHAR* pszTag)
{
    VIM_ERROR_PTR eResult = VIM_ERROR_SYSTEM;
    if (psTxn && pszTag)
    {
        //Ensure that pszTag is pointing to a tag in the transaction PAD buffer
        if (pszTag >= psTxn->u_PadData.ByteBuff && pszTag < psTxn->u_PadData.ByteBuff + sizeof(psTxn->u_PadData.ByteBuff) - 6)
        {
            VIM_SIZE ulLen = QC_GetLengthFromPADTag(pszTag) + 6;
            if ((pszTag + ulLen) <= (psTxn->u_PadData.ByteBuff + sizeof(psTxn->u_PadData.ByteBuff)))
            {
                VIM_SIZE ulRemainingLen = vim_strlen(pszTag + ulLen);
                //Shift remaining PAD data over the top of the tag we want to remove
                vim_mem_copy(pszTag, pszTag + ulLen, ulRemainingLen);
                //Zero the now artifact data
                vim_mem_set(pszTag + ulRemainingLen, 0x00, ulLen);
                eResult = VIM_ERROR_NONE;
            }
            else
            {
                eResult = VIM_ERROR_OVERFLOW;
            }
        }
        else
        {
            eResult = VIM_ERROR_INVALID_DATA;
        }
    }
    else
    {
        eResult = VIM_ERROR_NULL;
    }
    return(eResult);
}

static VIM_ERROR_PTR QC_ExtractPassCodeFromPAD(transaction_t* psTxn, VIM_CHAR* pszBuffer, VIM_SIZE ulSize)
{
    VIM_ERROR_PTR eResult = VIM_ERROR_SYSTEM;
    if (psTxn && pszBuffer)
    {
        char* pszPADPasscode = NULL;

        VIM_DBG_LABEL_STRING("PAD Before", psTxn->u_PadData.ByteBuff);
        //Find the Passcode data in the PAD buffer
        if ((VIM_ERROR_NONE == vim_strstr(psTxn->u_PadData.ByteBuff, "PCD", &pszPADPasscode)) &&
            (NULL != pszPADPasscode) &&
            (6 < vim_strlen(pszPADPasscode)))
        {
            //Get the length of Passcode data
            VIM_SIZE ulLen = QC_GetLengthFromPADTag(pszPADPasscode);
            if ((ulLen <= vim_strlen(pszPADPasscode)) && (ulSize > ulLen))
            {
                //Copy the passcode into the external buffer
                vim_mem_copy(pszBuffer, pszPADPasscode + 6, ulLen);
                pszBuffer[ulLen] = '\0';
                //Remove the PAD tag (and it's container WWO tag if present)
                if (0 == vim_strncmp(pszPADPasscode - 6, "WWO", 3))
                    pszPADPasscode -= 6;
                eResult = QC_RemoveTagFromPAD(psTxn, pszPADPasscode);
            }
            else
            {
                eResult = VIM_ERROR_OVERFLOW;
            }
        }
        else
        {
            eResult = VIM_ERROR_NOT_FOUND;
        }
    }
    else
    {
        eResult = VIM_ERROR_NULL;
    }

    VIM_DBG_LABEL_STRING("PAD After", psTxn->u_PadData.ByteBuff);

    return(eResult);
}


static VIM_ERROR_PTR QC_ExtractPAN(const transaction_t* psTxn, VIM_CHAR* psPAN, VIM_UINT32 ulBufSize)
{
    VIM_CHAR* pcDelim = NULL;

    if (!psTxn || !psPAN || (0 == ulBufSize))
        return VIM_ERROR_NULL;

    vim_mem_set(psPAN, 0x00, ulBufSize);

    switch (psTxn->card.type)
    {
    case card_manual:
    case card_manual_pos: vim_strncpy(psPAN, psTxn->card.data.manual.pan, ulBufSize - 1); break;
    case card_mag:        vim_strncpy(psPAN, psTxn->card.data.mag.track2, ulBufSize - 1); break;
    default:
        pceft_debugf(pceftpos_handle.instance, "Unsupported card type %d", psTxn->card.type);
        return VIM_ERROR_INVALID_DATA;
    }

    // RDD 290621 JIRA PIRPIN-1105  Full track2 should be sent if available
    if ((VIM_ERROR_NONE == vim_strchr(psPAN, '=', &pcDelim)) && pcDelim)
    {
        *pcDelim = '\0';
    }

    return VIM_ERROR_NONE;
}

static VIM_BOOL QC_AddTransactionRequestData(cJSON* psCJFileRequest, transaction_t* psTxn)
{
    VIM_BOOL fSuccess = VIM_FALSE;
    if (psCJFileRequest && psTxn)
    {
        cJSON* psCJRequest = cJSON_CreateObject();
        cJSON* psCards = cJSON_CreateArray();
        cJSON* psCard = cJSON_CreateObject();

        VIM_CHAR szT2[MAX_TRACK2_LENGTH + 1];
        VIM_CHAR szCardNum[WOW_PAN_LEN_MAX + 1] = { 0 };
        VIM_CHAR szPasscode[32 + 1] = { 0 };
        VIM_CHAR szTxnRef[8 + 1] = { 0 };

        const VIM_CHAR* pszCurrencyCode = WOW_NZ_PINPAD ? "NZD" : "AUD";
        double dAmount = ((double)psTxn->amount_total) / 100;

        // RDD 300621 JIRA PIRPIN-1105 : Track2 data needs also to be added if available
        fSuccess = VIM_TRUE;

        vim_mem_clear(szT2, VIM_SIZEOF(szT2));
        if (psTxn->card.type == card_mag)
        {
            // RDD 010721 QC requested that we add start/end Sentinels (stripped by vim)
            VIM_CHAR *pszT2 = &szT2[0];
            *pszT2++ = ';';
            vim_mem_copy(pszT2, psTxn->card.data.mag.track2, psTxn->card.data.mag.track2_length);
            pszT2 += psTxn->card.data.mag.track2_length;
            *pszT2 = '?';

            // Create card data
            fSuccess &= VAA_AddString(psCard, "TrackData", szT2, VIM_TRUE);
            pceft_debugf_test(pceftpos_handle.instance, "QC write TrackData(T2) :%s", szT2);
        }
        // RDD 300621 - leave the barcode out for now as entering the barcode from the spreadsheet appears to result in 10838 error from QC.
        // I'm leaving this with Vinay to analyse for now and it doesn't affect bulk activations anyway I think.
        // RDD 140721 This issue now fixed at QC end and just test as OK so use the code...
#if 1
        else if ((psTxn->card.type == card_manual) && (vim_strlen(psTxn->card.data.manual.barcode) > WOW_PAN_LEN_MAX))
        {
            fSuccess &= VAA_AddString(psCard, "TrackData", psTxn->card.data.manual.barcode, VIM_TRUE);
            pceft_debugf_test(pceftpos_handle.instance, "QC write TrackData(BC) :%s", psTxn->card.data.manual.barcode);
        }
#endif 
        QC_ExtractPAN(psTxn, szCardNum, sizeof(szCardNum));
        QC_ExtractPassCodeFromPAD(psTxn, szPasscode, sizeof(szPasscode));
        vim_strncpy(szTxnRef, psTxn->pos_reference, sizeof(szTxnRef) - 1);

        fSuccess = VIM_TRUE;

        // Create card data
        fSuccess &= VAA_AddString(psCard, "CardNumber", szCardNum, VIM_TRUE);
        fSuccess &= VAA_AddString(psCard, "CardPin", szPasscode, VIM_FALSE);
        fSuccess &= VAA_AddNumber(psCard, "Amount", dAmount, VIM_FALSE);
        fSuccess &= VAA_AddString(psCard, "CurrencyCode", pszCurrencyCode, VIM_FALSE);

        // Add card data to array
        fSuccess &= cJSON_AddItemToArray(psCards, psCard);


        // Populate Request payload

        fSuccess &= VAA_AddNumber(psCJRequest, "NumberOfCards", 1, VIM_TRUE);
        fSuccess &= VAA_AddItem(psCJRequest, "Cards", psCards, VIM_TRUE);
        fSuccess &= VAA_AddString(psCJRequest, "BusinessReferenceNumber", szTxnRef, VIM_TRUE);
        fSuccess &= VAA_AddString(psCJRequest, "Notes", psTxn->rrn, VIM_TRUE);

        //Add request data to main JSON object
        fSuccess &= VAA_AddItem(psCJFileRequest, "request", psCJRequest, VIM_TRUE);
    }
    else
    {
        VIM_DBG_MESSAGE("Null parameter");
    }

    if (!fSuccess)
    {
        pceft_debug(pceftpos_handle.instance, "Failed to populate Request Payload Data");
    }

    return fSuccess;
}

VIM_ERROR_PTR QC_CreateSendMsg(transaction_t* psTxn)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    const VIM_CHAR* pszCommand = NULL;

    VIM_DBG_MESSAGE("QC_CreateSendMsg");
	
	VIM_ERROR_RETURN_ON_NULL(psTxn);

	// RDD 160522 MB Says we need to ensure that the EFT SAF is empty prior to sending 0600 via CDS ( due to no creds )
	if (QC_HasCreds() == VIM_FALSE)
	{
		if ((eRet = ReversalAndSAFProcessing(MAX_BATCH_SIZE, VIM_TRUE, VIM_TRUE, VIM_FALSE)) != VIM_ERROR_NONE)
		{
			// If SAF upload fails allow only activations
			if(( psTxn->type != tt_gift_card_activation ) && ( eRet == VIM_ERROR_TIMEOUT ))
			{
				pceft_debug(pceftpos_handle.instance, "QC No Creds and EFT SAF upload failed but activation so continue");
			}
			else
			{
				pceft_debug(pceftpos_handle.instance, "QC No Creds and EFT SAF upload failed - exit");
				VIM_ERROR_RETURN( eRet );
			}
		}
	}

    if( IsReturnsCard(psTxn) && (psTxn->type == tt_gift_card_activation ))
    {
        VIM_SIZE pulTotalCount = 0, pulReversalCount = 0, pulAdviceCount = 0;
      
        // Only allow returns card activations if Online and no real SAFs pending - otherwise reject them 
        QC_GetSAFCount(&pulTotalCount, &pulReversalCount, &pulAdviceCount);
        
        if (pulTotalCount)
        {
            if (QC_SAFUpload(pulTotalCount) != VIM_ERROR_NONE)
            {
                pceft_debugf(pceftpos_handle.instance, "Failed to upload %d pending SAF - decline Returns Activation as offline", pulTotalCount);
                VIM_ERROR_RETURN(ERR_NO_RESPONSE);
            }
        }
#if 0
        if (QC_Logon(VIM_TRUE, VIM_FALSE) != VIM_ERROR_NONE)
        {
            VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);
        }
#endif
    }

    // RDD receipt needs a stan which has no relevance to QC so just use the current value
    psTxn->stan = terminal.stan;

    switch (psTxn->type)
    {
        case tt_gift_card_activation: pszCommand = "Activate"; break;
        case tt_sale:                 pszCommand = "Redeem";   break;
        case tt_balance:              pszCommand = "Balance";  break;
        case tt_refund:               pszCommand = "Reload";   break;
        default:                                               break;
    }

    if (pszCommand)
    {
        cJSON* psCJFileRequest = cJSON_CreateObject();
        if (QC_AddMetaData(psCJFileRequest, pszCommand))
        {
            if (QC_AddTransactionRequestData(psCJFileRequest, psTxn))
            {
                if (QC_WriteJSONRequestFile(psCJFileRequest))
                {
                    //Set the response code to X0 as default and mark transaction as sent
                    transaction_set_status(psTxn, st_message_sent);
                    pceft_debugf(pceftpos_handle.instance, "QC Created json Req: %s Amt=%s", pszCommand, AmountString(psTxn->amount_total));
                    eRet = VIM_ERROR_NONE;
                }
                else
                {
                    pceft_debug(pceftpos_handle.instance, "QC Failed to write file");
                }
            }
            else
            {
                pceft_debug(pceftpos_handle.instance, "QC Failed to add payload");
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "QC Failed to add metadata");
        }

        FREE_JSON(psCJFileRequest);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "QC Unsupported transaction type");
        eRet = VIM_ERROR_INVALID_DATA;
    }

    return(eRet);
}

// RDD Complete List 
#if 0
00000	Success
10018	Transaction failed.
10744	Authorization Token Expired.
10863	Invalid request.
10197	Amount is incorrect.
10008	Authorization failed.
10743	Invalid Authorization Token.
10128	Access denied for performing the transaction.
10064	Invalid batch.
10019	Transaction Id check failed.
10121	Invalid TerminalID.
10135	POS Auth failed.
10123	Invalid username or password.
10126	Merchant Outlet Auth failed.
10129	Merchant auth failed.
10136	The user has been disabled.
10012	Transaction failed.
10838	Validation Failed.
10313	Invalid Idempotency Key.
10748	idempotencyKey exceeds max length.
10312	Invalid Idempotent Request - Parameters Mismatch or Validity Period Passed.
10004	Either Card Number or Card Pin is Incorrect.
10361	Invalid currency code.
10066	Could not read card.
10065	Card format is incorrect.
10193	CardNumber does not match with TrackData CardNumber.
10026	Card Program not supported.
10058	Card program group inactive.
10638	Program parameters not available.
10143	Card Pin is mandatory.
10119	Your card has been temporarily deactivated for the day due to multiple attempts with invalid PIN.Please retry tomorrow.
10086	Either card number or card pin is incorrect.
10001	Card expired.
10256	Execution of custom procedure for {0} transaction failed.
10010	Balance is insufficient.
10368	Currency conversion not allowed.
10117	Conversion rate is not configured for this currency.
10363	This currency is not supported by the merchant.
10362	This currency is not supported by the issuer.
10103	Card Program Group does not exists.
10015	Card already active.
10021	Card deactivated.
10029	Card not activated.
10348	Card is reserved.
10002	Card is deactivated.
10249	Card not allocated to the current outlet.Cannot be activated / issued.
10317	Card activation not allowed.
10314	Either fixed denomination value is invalid or not set.
10094	Validation failed.Amount is not part of initial values.
10036	Amount less than min load limit.
10038	Amount more than max load limit.
10116	Only integer amount is allowed.
10096	Merchant not authorized to accept this card.
10030	Card deactivated.Zero balance.
10075	Card already reissued.
10027	Card is deactivated.
10082	Card deactivated.It was lost or stolen.Please pick up the card.
10022	Amount more than max reload limit.
10035	Amount less than min reload limit.
10067	Amount not multiple of reload unit.
10353	Cannot process reverse transaction.
10320	Redemption not allowed out of specified time range.
10257	Redemption amount is greater than total bill amount.
10297	Total Bill Amount is not in the accepted range.
10296	Total Bill Amount should be passed to proceed with the redeem transaction.
10115	Entered amount is greater than the configured decimal places.
10251	Only one time redemption allowed for this card.
10034	Amount less than min redeem limit.
10037	Amount more than max redeem limit.
#endif


static VIM_ERROR_PTR QC_RespCodeToAS2805(int lQCRespCode, VIM_CHAR* pcDestBuf, VIM_UINT32 ulDestSize, transaction_t* psTxn)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    if (pcDestBuf && (2 <= ulDestSize))
    {
        const char* pszAS2805RespCode;

        switch (lQCRespCode)
        {
            case 00000:
            pszAS2805RespCode = "00"; eRet = VIM_ERROR_NONE; 
            break; //Transaction approved

            case 10030:
                pszAS2805RespCode = "GX"; eRet = ERR_WOW_ERC;
                break; // JIRA PIRPIN-2232

            case 10002:
            case 10021:
            case 10027:
            case 10082:
                pszAS2805RespCode = "GD"; eRet = ERR_WOW_ERC;    
                break; //Card was deactivated

            case 10029:
                pszAS2805RespCode = "GU"; eRet = ERR_WOW_ERC;
                break; //Card requires activation



            case 10008:
            pszAS2805RespCode = "05"; eRet = ERR_WOW_ERC;    
            break; //Error

            case 10015:
            pszAS2805RespCode = "GA"; eRet = ERR_WOW_ERC;    
            break; //Already Active - card was activated previously

            case 10064:
            case 10121:
            case 10123:
            case 10249:
            case 10638:
            case 10863:
            pszAS2805RespCode = "12"; eRet = ERR_WOW_ERC;    
            break; //Invalid Data

            case 10037:
            case 10038:
            case 10022:
			// RDD 221221 JIRA PIRPIN-1368 
			case 20006:
            pszAS2805RespCode = "GH"; eRet = ERR_WOW_ERC;    
            break; //Exceeeds Redeem/reload limit

            case 10034:
            case 10035:
            case 10036:
            case 10067:
            case 10094:
            case 10115:
            case 10116:
            case 10117:
            case 10197:
            case 10257:
            case 10296:
            case 10297:
            case 10314:
            case 10361:
            case 10362:
            case 10363:
            case 10368:
            pszAS2805RespCode = "13"; eRet = ERR_WOW_ERC;    
            break; //Invalid Amount

            case 10004:
            case 10065:
            case 10136:
            case 10193:
            pszAS2805RespCode = "14"; eRet = ERR_WOW_ERC;    
            break; //Invalid Card

            case 10353:
            pszAS2805RespCode = "21"; eRet = ERR_WOW_ERC;    
            break; //System Error

            case 10010:
            pszAS2805RespCode = "GB"; eRet = ERR_WOW_ERC;    
            break; //Contact Issuer


            case 10001:
            pszAS2805RespCode = "54"; eRet = ERR_WOW_ERC;    
            break; //Expired

            case 10086:
            case 10838:
            case 10143:
            pszAS2805RespCode = "GP"; eRet = ERR_WOW_ERC;    
            break; //Invalid PIN

            case 10096:
            pszAS2805RespCode = "58"; eRet = ERR_WOW_ERC;    
            break; //System Error

            case 10119:
            pszAS2805RespCode = "75"; eRet = ERR_WOW_ERC;    
            break; //Card blocked for 24hrs - exceed PIN retries

            case 10312:
            case 10313:
            case 10743:
            case 10744:
            case 10748:
            pszAS2805RespCode = "96"; eRet = ERR_WOW_ERC;    
            break; //Security Error

            case 10012:
            pszAS2805RespCode = "99"; eRet = ERR_WOW_ERC;    
            break; //System Error

            // RDD 230421 - Robbie added this to denote Offline Approved so it needs to be mapped !
            case 77777:
                // RDD 010623 JIRA PIRPIN-2413 don;t allow offline for returns card activation
                if ( IsReturnsCard(psTxn) )
                {
                    pszAS2805RespCode = "X0"; eRet = ERR_HOST_RECV_TIMEOUT;
                    pceft_debug(pceftpos_handle.instance, "Returns Card Activation - remove this SAF");
                    // RDD 020623 - Safe to Wipe the SAF as the prior QC logon will have uploaded any pending SAFs so only the returns card activation should be there. 
                    QC_SAFClear();
                }
                else
                {
                    pszAS2805RespCode = "00"; eRet = ERR_WOW_ERC;
                }
                break; //System Error

            default:
            pszAS2805RespCode = "01"; eRet = ERR_WOW_ERC;    
            break; //Declined

        }
        vim_mem_set(pcDestBuf, 0x00, ulDestSize);
        vim_strncpy(pcDestBuf, pszAS2805RespCode, 2);

        pceft_debugf(pceftpos_handle.instance, "QC RC '%d' to AS2805 '%s'", lQCRespCode, pszAS2805RespCode);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Invalid parameters to QC resp code conversion");
    }
    return eRet;
}

#if 0
static VIM_BOOL QC_AppendToPAD(const VIM_CHAR* pszValue, VIM_CHAR* pszPADBuf, VIM_UINT32 ulBufSize, const VIM_CHAR* pszPADTag)
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
#endif


VIM_CHAR *QC_EntryMode(transaction_t *transaction)
{
    static VIM_CHAR EntryMode = 'K';
    if (txn.card_state == card_capture_new_gift)
    {
        if ((txn.card.type == card_mag) || (transaction_get_status(transaction, st_pos_sent_swiped_pan)))
        {
            EntryMode = 'S';
        }
        else if (transaction_get_status(transaction, st_pos_sent_keyed_pan))
        {
            EntryMode = 'K';
        }
        else
        {
            EntryMode = 'B';
        }
    }
    else if (IsBGC())
    {
        EntryMode = 'B';
    }
    return &EntryMode;
}






static VIM_BOOL QC_ParsePADData(transaction_t* psTxn)
{
    VIM_BOOL fSuccess = VIM_TRUE;
    char szPAD[999 - 6 + 1] = { 0 };
    VIM_SIZE ulPadLen = 0;

    //Populate PAD data
    fSuccess &= QC_GetCommonPadTags(psTxn, szPAD, sizeof(szPAD));

    ulPadLen = vim_strlen(szPAD);
    vim_snprintf(psTxn->u_PadData.ByteBuff, sizeof(psTxn->u_PadData.ByteBuff), "%3.3dWWG%3.3d%s", ulPadLen + 6, ulPadLen, szPAD);
    return fSuccess;
}



static VIM_BOOL QC_ParseBalance(transaction_t* psTxn, const cJSON* pCJCard)
{
    double dBalance = 0;
    VIM_BOOL fSuccess = cJSON_GetNumberItemValue(pCJCard, "Balance", &dBalance);
    // RDD 190521 JIRA PIRPIN-1070 Fix zero balances so they are displayed
    //if (fSuccess && (0 < dBalance))
    if (fSuccess)
    {
        psTxn->ClearedBalance = (VIM_INT32)(dBalance * 100);
        psTxn->LedgerBalance  = (VIM_INT32)(dBalance * 100);
        VIM_DBG_PRINTF2("Parsed QC Balances: Cleared=%d, Ledger=%d", psTxn->ClearedBalance, psTxn->LedgerBalance);
    }
    else
    {
        VIM_DBG_MESSAGE("Skipping QC Balance; element not found");
    }
    return fSuccess;
}


static VIM_BOOL QC_RespCodeIsApprovedOffline(int lRespCode)
{
    return (77777 == lRespCode);
}


static VIM_BOOL QC_RespCodeIsApproved(int lRespCode)
{
    return ((0 == lRespCode) || QC_RespCodeIsApprovedOffline(lRespCode));
}

#define SAFE_STR(x)                 ((x) ? (x) : "<NULL>")

#if 0
static VIM_ERROR_PTR QC_ParseErrors(transaction_t* psTxn, cJSON* pCJResp)
{

	cJSON* psCJ_errorlist = cJSON_GetObjectItem(cJSON_GetObjectItem(pCJResp, "errors"), "errorlist");

	VIM_UINT32 ulIdx, ulCount = cJSON_GetArraySize(psCJ_errorlist);
	
	for (ulIdx = 0; ulIdx < ulCount; ++ulIdx)
	{
			cJSON* psCJ_error = cJSON_GetArrayItem(psCJ_errorlist, ulIdx);
			char szLabel[64 + 1] = { 0 };
			snprintf(szLabel, sizeof(szLabel), "Received QC VAA Error (%d/%d): ",ulIdx + 1, ulCount);
			VAA_DebugJson(szLabel, psCJ_error);	
	}
	return VIM_ERROR_NONE;
}
#endif


static VIM_ERROR_PTR QC_ParseRespCode(transaction_t* psTxn, const cJSON* pCJResp, const cJSON* pCJCard)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    if (psTxn)
    {
        int lRespCode = -1, lCardRespCode = -1;

        cJSON_GetIntItemValue(pCJResp, "ResponseCode", &lRespCode);
        cJSON_GetIntItemValue(pCJCard, "ResponseCode", &lCardRespCode);

        eRet = QC_RespCodeToAS2805(lRespCode, psTxn->host_rc_asc, sizeof(psTxn->host_rc_asc), psTxn );
        if (QC_RespCodeIsApproved(lRespCode) && QC_RespCodeIsApproved(lCardRespCode) && ( eRet = ERR_WOW_ERC ))
        {
            if (QC_RespCodeIsApprovedOffline(lRespCode) || QC_RespCodeIsApprovedOffline(lCardRespCode))
            {
                // RDD - to get the *00 which denotes offline approved
                transaction_set_status( psTxn, st_efb );
                pceft_debug(pceftpos_handle.instance, "QC Transaction Approved OFFLINE!");
            }
            else
                pceft_debug_test(pceftpos_handle.instance, "QC Transaction Approved!");
        }
        else
        {
            //At least one of the response codes indicates an error
            pceft_debugf(pceftpos_handle.instance, "QC Txn Failed %d ( mapped to %s )", lRespCode, psTxn->host_rc_asc );

            if (0 != lCardRespCode)
            {
                //The Card Response code usually contains more detail on what the issue is
                eRet = QC_RespCodeToAS2805(lCardRespCode, psTxn->host_rc_asc, sizeof(psTxn->host_rc_asc), psTxn );
            }
            else
            {
                pceft_debug(pceftpos_handle.instance, "Main response code indicates error, but card level doesn't!");
            }

            //QC_DebugRespCodeString(pCJResp, "QC Transaction", "ResponseMessage");
            //QC_DebugRespCodeString(pCJResp, "QC Transaction", "ErrorDescription");
            //QC_DebugRespCodeString(pCJResp, "QC Card",        "ResponseMessage");
            //QC_DebugRespCodeString(pCJResp, "QC Card",        "ErrorDescription");
        }
    }
    return eRet;
}

#define JSON_KEY_RTM_RESULT          "result"
#define JSON_KEY_RTM_VALIDATION_RSP  "validationResponse"


#define MIN_LEN_0610 50

#if 0

static VIM_ERROR_PTR QC_ParseCredentials( const cJSON *pCJResp, char *pszMsg0610Response, int iRespBuffSize )
{
    VIM_ERROR_PTR eRet = VIM_ERROR_NONE;
	VIM_CHAR szRTMResponse[20];

    if ( pCJResp && pszMsg0610Response )
    {
		if (cJSON_GetStringItemValue(pCJResp, JSON_KEY_RTM_RESULT, szRTMResponse, VIM_SIZEOF(szRTMResponse)))
		{
			pceft_debugf(pceftpos_handle.instance, "Found RTM Get Creds Response:%s", szRTMResponse);

			// RDD 170622 added result field from RTM for processing by PA : if we get any response from RTM then create a response to PA
			if (vim_strcmp(szRTMResponse, "ok"))
			{
				//VIM_ERROR_RETURN(ERR_RTM_AUTH_FAILURE);
                eRet = VIM_ERROR_NONE;
            }
		}

        if ( cJSON_GetStringItemValue( pCJResp, JSON_KEY_RTM_VALIDATION_RSP, pszMsg0610Response, iRespBuffSize ) )
        {            
			VIM_DBG_PRINTF1( "Found 0610 response: %s", pszMsg0610Response );
			if (vim_strlen(pszMsg0610Response) < MIN_LEN_0610)
			{
				pceft_debugf(pceftpos_handle.instance, "RTM 0610 Response Invalid (len=%d)", vim_strlen(pszMsg0610Response));
				//eRet = ERR_RTM_AUTH_FAILURE2;
				eRet = VIM_ERROR_NONE;
			}
        }
        else
        {
			pceft_debug(pceftpos_handle.instance, "RTM 0610 Response Field Missing");
            //eRet = ERR_RTM_AUTH_FAILURE2;
            eRet = VIM_ERROR_NONE;
        }
    }
    else
    {
        eRet = VIM_ERROR_NULL;
    }
    return eRet;
}


#endif


static VIM_ERROR_PTR QC_ParseResponse(transaction_t* psTxn, cJSON* pCJRoot)
{
    VIM_ERROR_PTR eRet = ERR_NO_RESPONSE;
    if (psTxn && pCJRoot)
    {
        cJSON* pCJMeta = cJSON_GetObjectItem(pCJRoot, JSON_KEY_PA_META);

        if (pCJRoot && pCJMeta)
        {
            cJSON* pCJResp = cJSON_GetObjectItem(pCJRoot, JSON_KEY_PA_RESPONSE);
            cJSON* pCJCard = cJSON_GetArrayItem(cJSON_GetObjectItem(pCJResp, "Cards"), 0);

            if (pCJResp && pCJCard)
            {
               // QC_ParsePADData(psTxn, pCJResp, pCJCard);
                QC_ParseBalance(psTxn, pCJCard);
                eRet = QC_ParseRespCode(psTxn, pCJResp, pCJCard);
            }
            else if(pCJResp)
            {
                //pceft_debug_test(pceftpos_handle.instance, "Processing Auth Errors");
				//QC_ParseErrors(psTxn, pCJRoot);
            }
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Invalid response received from QC app");
        }
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Null Root (or Txn)");
    }
    return eRet;
}

VIM_ERROR_PTR QC_ReceiveParseResp(transaction_t* psTxn)
{
    VIM_ERROR_PTR eRet = ERR_NO_RESPONSE;

    VIM_DBG_MESSAGE("QC_ReceiveParseResp");
    VAA_CheckDebug();

    if (psTxn)
    {
        cJSON* pCJRoot = NULL;
        vim_strcpy(psTxn->host_rc_asc, "X0");
        pCJRoot = QC_WaitForResponse();

        if (pCJRoot)
        {
            eRet = QC_ParseResponse(psTxn, pCJRoot);
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Timed out waiting for response from QC app");
        }

        FREE_JSON(pCJRoot);

		if (txn.error != VIM_ERROR_NONE)
		{
			eRet = txn.error;
		}
		else
		{
			VIM_DBG_PRINTF1("Transaction response code = %s", psTxn->host_rc_asc);
			QC_ParsePADData(psTxn);
		}
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Invalid parameters to QC Receive Parse");
    }

    VAA_CheckDebug();
    VIM_ERROR_RETURN( eRet );
}

/*
VIM_ERROR_PTR QC_Transaction(transaction_t* psTxn)
{
    VIM_RTC_UPTIME start_ticks, end_ticks;
    vim_rtc_get_uptime(&start_ticks);

    VIM_DBG_MESSAGE("QC_Transaction");

    VIM_ERROR_RETURN_ON_FAIL(QC_CreateSendMsg(psTxn));

    // RDD 010317 - Set the repeat flag here for the reversals 
    if (reversal_present())
    {
        transaction_set_status(&reversal, st_repeat_saf);
        reversal_set(&reversal);
    }

    VIM_ERROR_RETURN_ON_FAIL(QC_ReceiveParseResp(psTxn));

    // RDD 170513 Get the lapsed time for the stats
    vim_rtc_get_uptime(&end_ticks);

    if (end_ticks > start_ticks)
    {
        VIM_DBG_NUMBER(end_ticks - start_ticks);
        SetResponseTimeStats(end_ticks - start_ticks);
    }
    return VIM_ERROR_NONE;
}
*/

static VIM_ERROR_PTR QC_ParseSuccessResponse(const cJSON* pCJResponse)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    if (pCJResponse)
    {
        cJSON* pCJMeta = cJSON_GetObjectItem(pCJResponse, "meta");

        if (pCJResponse && pCJMeta)
        {
            cJSON_bool fSuccess = VIM_FALSE;
            if (cJSON_GetBoolItemValue(pCJMeta, "Success", &fSuccess) && fSuccess)
            {
                eRet = VIM_ERROR_NONE;
            }
            else
            {
                eRet = ERR_QC_AUTH_FAILURE; 
                pceft_debug(pceftpos_handle.instance, "QC Responded with an Error");
            }
        }
        else
        {
            eRet = ERR_INVALID_MSG;
            pceft_debug(pceftpos_handle.instance, "QC Response was Invalid");
        }
    }
    else
    {
        eRet = ERR_NO_RESPONSE;
        pceft_debug(pceftpos_handle.instance, "QC App Timed Out waiting for response");
    }
    return eRet;
}



VIM_ERROR_PTR QC_Logon(VIM_BOOL fWaitForResponse, VIM_BOOL fDeleteBackedUpCredentials)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    cJSON* pCJFileRequest = cJSON_CreateObject();

    // Only want error reporting during logon.
	VIM_ERROR_WARN_ON_FAIL( vim_file_delete(QC_0600_REQ_FILE_PATH));

    if (!QC_IsInstalled(VIM_FALSE))
    {
		pceft_debug(pceftpos_handle.instance, "QC Fatal Error : Qwikcilver Not Installed");
		VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
    }

    if (QC_AddMetaData(pCJFileRequest, "Logon"))
    {
		cJSON* psCJRequest = cJSON_CreateObject();        
		VAA_AddBool(psCJRequest,    "DeleteBackedUpCredentials", fDeleteBackedUpCredentials, VIM_TRUE);
		VAA_AddItem(pCJFileRequest, "request", psCJRequest, VIM_TRUE);

        if (fWaitForResponse)
        {
            cJSON* pCJRoot = QC_SendAndReceive(pCJFileRequest);
            eRet = QC_ParseSuccessResponse(pCJRoot);
            if (VIM_ERROR_NONE == eRet)
            {
                pceft_debug(pceftpos_handle.instance, "Qwikcilver Logon successful");
            }
			else
			{
				pceft_debug(pceftpos_handle.instance, "Qwikcilver Logon Failed");
			}
            FREE_JSON(pCJRoot);
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Doing background Qwikcilver Logon; Ignoring any response");
            QC_WriteJSONRequestFile(pCJFileRequest);
            eRet = VIM_ERROR_NONE;
        }
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to add Logon metadata");
    }

    FREE_JSON(pCJFileRequest);

    return eRet;
}

// JIRA PIRPIN-2244 Issues with scanned barcodes due to incorrect offset returned by VIM layer
static VIM_CHAR Barcode[QC_CARD_DATA_LEN + 1];

VIM_ERROR_PTR QC_ProcessBarcode(transaction_t *transaction, VIM_CHAR *CardData)
{
    VIM_CHAR *Ptr = CardData;
    VIM_CHAR EntryMode = 0;       // RDD 221220 - default to 'B' as NCR actually won't send 'B' or 'K' in input data.
    VIM_SIZE n, Len = 0;

    vim_mem_clear(Barcode, VIM_SIZEOF(Barcode));
    // RDD - fixed length field - could have anyhting in it.
    vim_strncpy(Barcode, Ptr, QC_CARD_DATA_LEN);

    pceft_debugf_test(pceftpos_handle.instance, "Barcode:%s", Barcode);

    transaction_set_status(&txn, st_pos_sent_pci_pan);  // always when PAN is supplied
    transaction_set_status(&txn, st_electronic_gift_card);

    for (n = 0; n < QC_CARD_DATA_LEN; n++)
    {
        // known Issue with wex barcodes
        if (!VIM_ISDIGIT(Barcode[n]))
        {
            if( n == 0 && (Barcode[n] == 'B' ))
            {
                EntryMode = 'B';                    
                transaction_set_status(&txn, st_pos_sent_pci_pan );
            }
            else
            {
                // Get rid of spaces etc. and ensure null termination.                       
                Barcode[n] = 0;
                if (Len >= WOW_GIFT_CARD_LEN_MIN)
                    break;
            }
        }
        else
        {
            Len += 1;
        }
    }
    // Skip the leading entrymode (if there was one )
    if (EntryMode)
    {
        Ptr = &Barcode[1];
    }
    else
    {
        EntryMode = 'B';
        Ptr = &Barcode[0];
    }

    if ((Len = VIM_MIN(QC_CARD_DATA_LEN, Len)) < WOW_GIFT_CARD_LEN_MIN)
    {
        pceft_debugf_test(pceftpos_handle.instance, "QC Barcode : %s Invalid Len = %d", Ptr, Len);
        txn.error = VIM_ERROR_POS_CANCEL;
        VIM_ERROR_RETURN( txn.error );
    }

    // RDD 300621 Added for Avalon JIRA PIRPIN-1105 - non PCI so don't encrypt for PFR
    vim_mem_clear(transaction->card.data.manual.barcode, VIM_SIZEOF(transaction->card.data.manual.barcode));
    vim_strcpy(transaction->card.data.manual.barcode, Ptr);
    

    // Set this so we can report the entry mode back to POS ( no idea what possible use this could be )
    if (EntryMode == 'K')
        transaction_set_status(&txn, st_pos_sent_keyed_pan);

    // RDD ToDo - skip any leading garbarge - can't test this without new client
    while (Len > WOW_PAN_LEN_MAX)
    {
        Ptr += 1;
        Len -= 1;
    }
    VIM_DBG_STRING(Ptr);

    vim_strncpy(txn.card.data.manual.pan, Ptr, Len);
    txn.card.data.manual.pan_length = Len;

    txn.card.type = card_manual;
    txn.card_state = card_capture_new_gift;
    transaction_set_status(&txn, st_pos_sent_pci_pan);

    txn.card.data.manual.expiry_mm[0] = '0';
    txn.card.data.manual.expiry_mm[1] = '0';
    txn.card.data.manual.expiry_yy[0] = '0';
    txn.card.data.manual.expiry_yy[1] = '0';

    return VIM_ERROR_NONE;
}


#if 0

VIM_BOOL QC_GetCommonPadTags(transaction_t* psTxn, char* pszPadBuf, VIM_UINT32 ulBufSize)
{
    VIM_CHAR szTenderID[2 + 1] = { 0 };
    VIM_CHAR szEntryMode[1 + 1] = { 0 };

    if (!psTxn || !pszPadBuf)
        return VIM_FALSE;

    vim_snprintf(szTenderID, sizeof(szTenderID), "%2.2d", PcEFTBin(psTxn));

    VIM_DBG_PRINTF1("Tender ID returned = %s", szTenderID)

        QC_AppendToPAD(szTenderID, pszPadBuf, ulBufSize, "TND");
    QC_AppendToPAD(QC_EntryMode(psTxn), pszPadBuf, ulBufSize, "ENT");

    return VIM_TRUE;
}

#else 

VIM_BOOL QC_GetCommonPadTags(transaction_t* psTxn, char* pszPadBuf, VIM_UINT32 ulBufSize)
{
    VIM_CHAR szTenderID[2 + 1] = { 0,0,0 };
    VIM_CHAR szEntryMode[1 + 1] = { 0,0 };
    VIM_CHAR EntryMode = *QC_EntryMode(psTxn);

    if (!psTxn || !pszPadBuf)
        return VIM_FALSE; 

    vim_sprintf(szTenderID, "%2.2d", PcEFTBin(psTxn));
    VIM_DBG_STRING(szTenderID);

    vim_sprintf(szEntryMode, "%c", EntryMode );
    VIM_DBG_STRING(szEntryMode);

    VAA_AppendToPAD(szTenderID, pszPadBuf, ulBufSize, "TND");
    VAA_AppendToPAD(szEntryMode, pszPadBuf, ulBufSize, "ENT");
    VIM_DBG_STRING(pszPadBuf);

    return VIM_TRUE;
}
#endif





static VIM_ERROR_PTR QC_ParseSAFCountResponse(const cJSON* pCJRoot, size_t* pulTotalCount, size_t* pulReversalCount, size_t* pulAdviceCount)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;

    if (pCJRoot && pulTotalCount && pulReversalCount && pulAdviceCount)
    {
        const cJSON* pCJMeta = cJSON_GetObjectItem(pCJRoot, "meta");
        cJSON_bool fSuccess = VIM_FALSE;
        if (cJSON_GetBoolItemValue(pCJMeta, "success", &fSuccess) && fSuccess)
        {
            const cJSON* pCJResponse = cJSON_GetObjectItem(pCJRoot, "response");

            fSuccess &= cJSON_GetUIntItemValue(pCJResponse, "SAFCount",      pulTotalCount);
            fSuccess &= cJSON_GetUIntItemValue(pCJResponse, "ReversalCount", pulReversalCount);
            fSuccess &= cJSON_GetUIntItemValue(pCJResponse, "AdviceCount",   pulAdviceCount);

            if (fSuccess)
            {
                eRet = VIM_ERROR_NONE;
                VIM_DBG_PRINTF3("Qwikcilver SAF Counts: Total=%u, Reversals=%u, Advices=%u", *pulTotalCount, *pulReversalCount, *pulAdviceCount);
            }
            else
            {
                eRet = VIM_ERROR_INVALID_DATA;
                pceft_debug(pceftpos_handle.instance, "Failed to parse SAF count elements");
            }
        }
        else
        {
            eRet = VIM_ERROR_NOT_FOUND;
            pceft_debug(pceftpos_handle.instance, "Failed to determine QC SAF counts");
        }
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "No SAF Count response data");
    }

    return eRet;
}


VIM_ERROR_PTR QC_GetSAFCount(VIM_SIZE* pulTotalCount, VIM_SIZE* pulReversalCount, VIM_SIZE* pulAdviceCount)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    size_t ulTotal = 0, ulReversals = 0, ulAdvices = 0;
    cJSON* pCJFileRequest = cJSON_CreateObject();

    VIM_DBG_MESSAGE("Getting Qwikcilver SAF count");

    if (QC_AddMetaData(pCJFileRequest, "SAF_Count"))
    {
        cJSON* pCJRoot = QC_SendAndReceive(pCJFileRequest);
        eRet = QC_ParseSAFCountResponse(pCJRoot, &ulTotal, &ulReversals, &ulAdvices);
        FREE_JSON(pCJRoot);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to add SAF count metadata");
    }

    FREE_JSON(pCJFileRequest);

    if (pulTotalCount)    *pulTotalCount    = (VIM_SIZE)ulTotal;
    if (pulReversalCount) *pulReversalCount = (VIM_SIZE)ulReversals;
    if (pulAdviceCount)   *pulAdviceCount   = (VIM_SIZE)ulAdvices;

    return eRet;
}


VIM_ERROR_PTR QC_ClearReversal(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    cJSON* pCJFileRequest = cJSON_CreateObject();

    VIM_DBG_MESSAGE("Clear QC Pending Reversal");

    if (QC_AddMetaData(pCJFileRequest, "Reversal_Clear"))
    {
        QC_SendAndForget(pCJFileRequest);
        eRet = VIM_ERROR_NONE;
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to add Reversal Delete metadata");
    }
    FREE_JSON(pCJFileRequest);
    return eRet;
}




VIM_ERROR_PTR QC_SAFUpload(VIM_SIZE ulMaxSends)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    cJSON* pCJFileRequest = cJSON_CreateObject();

    VIM_DBG_PRINTF1("Attempting upload of up to %u Qwikcilver SAF transactions", ulMaxSends);

    if (QC_AddMetaData(pCJFileRequest, "SAF_Upload"))
    {
        cJSON* pCJRequest = cJSON_CreateObject();
        if (pCJRequest)
        {
            cJSON* pCJRoot = NULL;

            VAA_AddNumber(pCJRequest, "MaxUploads", ulMaxSends, VIM_FALSE);
            cJSON_AddItemToObject(pCJFileRequest, "request", pCJRequest);

            pCJRoot = QC_SendAndReceive(pCJFileRequest);

            eRet = QC_ParseSuccessResponse(pCJRoot);
            if (VIM_ERROR_NONE == eRet)
            {
                VIM_DBG_MESSAGE("SAF Upload successful");
            }
            FREE_JSON(pCJRoot);
        }
        else
        {
            pceft_debug(pceftpos_handle.instance, "Failed to create SAF Upload JSON 'SAF' object");
        }
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to add SAF Upload metadata");
    }
    FREE_JSON(pCJFileRequest);

    return eRet;
}

VIM_ERROR_PTR QC_SAFClear(void)
{
    VIM_ERROR_PTR eRet = VIM_ERROR_SYSTEM;
    cJSON* pCJFileRequest = cJSON_CreateObject();

    pceft_debug(pceftpos_handle.instance, "Telling QC to clear its SAF");

    if (QC_AddMetaData(pCJFileRequest, "SAF_Clear"))
    {
        cJSON* pCJRoot = QC_SendAndReceive(pCJFileRequest);

        eRet = QC_ParseSuccessResponse(pCJRoot);
        if (VIM_ERROR_NONE == eRet)
        {
            pceft_debug(pceftpos_handle.instance, "SAF Clear successful");
        }
        FREE_JSON(pCJRoot);
    }
    else
    {
        pceft_debug(pceftpos_handle.instance, "Failed to add SAF Clear metadata");
    }
    FREE_JSON(pCJFileRequest);

    return VIM_ERROR_SYSTEM;
}

const VIM_CHAR* QC_GetOTP( void )
{
    return l_szOTP;
}

// Create the 0600-containing QC-get-credentials request.
// Put all the necessary data into the QC request file, send it, wait for a response.
static VIM_ERROR_PTR QC_PerformAuthChallenge(void)
{
    VIM_ERROR_PTR err = VIM_ERROR_NONE;
    VIM_AS2805_MESSAGE msg = { 0 };

	DBG_MESSAGE("-------------- QCQCQCQC Auth Challenge: 0600 Create QCQCQCQC -----------------");

    err = msgAS2805_create( HOST_WOW, &msg, MESSAGE_WOW_600_QC );

    if ( VIM_ERROR_NONE == err )
    {
        cJSON *pCJFileRequest = cJSON_CreateObject();

        DBG_MESSAGE( "0600 message created ok" );

        if ( pCJFileRequest )
        {
            VIM_CHAR szMsg0600[1000] = { 0 };
            VIM_RTC_UPTIME start_ticks;
            VIM_RTC_UPTIME end_ticks;
            VIM_UINT32 ulTimeoutS = TERM_QC_RESP_TIMEOUT;

            vim_rtc_get_uptime( &start_ticks );

            // Assuming ASCII-hex (not b64) for now.
            VIM_DBG_NUMBER(msg.msg_size * 2);
            VIM_DBG_DATA( szMsg0600, msg.msg_size);
            hex_to_asc( msg.message, szMsg0600, ( msg.msg_size * 2 ) );
            VIM_DBG_DATA(szMsg0600, (msg.msg_size * 2));

            if (VAA_AddString(pCJFileRequest, "0600_Msg", szMsg0600, VIM_TRUE))
            {
                DBG_MESSAGE("Writing QC Auth Challenge response file");
                if (VAA_WriteJSONRequestFile(QC_0600_RESP_FILE_PATH, pCJFileRequest))
                {
                    cJSON* pCJResp = NULL;
                    char szMsg0600Response[1000] = { 0 };
                    transaction_t my_txn;

                    DBG_MESSAGE("Waiting for QC Auth Challenge notify file");
                    // RDD 240521 JIRA PIRPIN-1072 Make This timeout configurable ( was hardcoded at 20 seconds ) 
                    pCJResp = VAA_WaitForJSONResponse(QC_0610_NOTIFY_FILE_PATH, ulTimeoutS, VIM_TRUE);
                    VIM_DBG_PRINTF1("QC Auth Challenge notify file: %s", pCJResp ? "Received" : "Timed out");

                    txn_do_init(&my_txn);
                    // RDD 300323 - remove response checking as too often wrong in nonprod. poss in prod too. 
#if 0
					if ((err = QC_ParseCredentials(pCJResp, szMsg0600Response, sizeof(szMsg0600Response))) != VIM_ERROR_NONE)
					{
						txn.error = err;
						VIM_ERROR_RETURN(err);
					}
#endif
					// RDD 200622 : Most logons to RTM are failing because of missing 0610 field in GetCreds HTTP response - ToDo - Get this fixed to restore optimum security
					if (vim_strlen(szMsg0600Response))
					{
						DBG_STRING(szMsg0600Response);

						// Send the 0610 response to the AS2805 logic for parsing.

						vim_rtc_get_uptime(&end_ticks);

						if (end_ticks > start_ticks)
						{
							VIM_DBG_NUMBER(end_ticks - start_ticks);
							SetResponseTimeStats(end_ticks - start_ticks);
						}

						asc_to_hex(szMsg0600Response, msg.message, vim_strlen(szMsg0600Response));
						msg.msg_size = vim_strlen(szMsg0600Response) / 2;

						if ((err = msgAS2805_parse(HOST_WOW, MESSAGE_WOW_600_QC, &msg, &my_txn)) == VIM_ERROR_NONE)
						{
							pceft_debug_test(pceftpos_handle.instance, "RTM Get Creds and Validate PinPad successful");
						}
						else
						{
							pceft_debug(pceftpos_handle.instance, "RTM 0610 Response Invalid (AS2805 Parse Failed)");
							err = ERR_RTM_AUTH_FAILURE2;
						}
					}
					
					DBG_VAR(err->name);
                    FREE_JSON(pCJResp);
                }
                else
                {
                    pceft_debug(pceftpos_handle.instance, "RTM Failed to write QC Auth Challenge file");
                }
            }
            else
            {
                pceft_debug( pceftpos_handle.instance, "RTM Failed to add GetCreds metadata" );
            }
        
            FREE_JSON( pCJFileRequest );
        }
        else
        {
            pceft_debug( pceftpos_handle.instance, "Failed to create Logon JSON object" );
        }
    }
    else
    {
        DBG_MESSAGE( "!!!!!!!!!!!!!!!!!! 0600 message creation failed !!!!!!!!!!!!!!!!!!!!!" );
        DBG_MESSAGE( err->name );
    }
    return err;
}


VIM_ERROR_PTR QC_CheckForAuthChallengeRequest( void )
{
    VIM_ERROR_PTR res = VIM_ERROR_SYSTEM;

	//VIM_DBG_MESSAGE("!!!!!!!!!!!!!!!!! QC: Auth Challenge Request !!!!!!!!!!!!!!!!!");

	if (vim_file_is_present(QC_0600_REQ_FILE))
    {
		VIM_DBG_MESSAGE("QC: Found Response File");
		cJSON* pCJRequest = VAA_ReadJSONFile(QC_0600_REQ_FILE_PATH);
        vim_file_delete(QC_0600_REQ_FILE_PATH);

        vim_mem_clear(l_szOTP, sizeof(l_szOTP));
        cJSON_GetStringItemValue(pCJRequest, "otp", l_szOTP, sizeof(l_szOTP));
        VIM_DBG_PRINTF2("Parsed otp='%s' from '%s'", l_szOTP, QC_0600_REQ_FILE_PATH);

        FREE_JSON(pCJRequest);

        pceft_debugf_test(pceftpos_handle.instance, "Received Challenge request with OTP '%s'. Performing Auth challenge dance", l_szOTP);

		res = QC_PerformAuthChallenge();
    }
    else
    {
		//VIM_DBG_MESSAGE("QC: No Response File yet");
		res = VIM_ERROR_NONE;
    }
    return res;
}





