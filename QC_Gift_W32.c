// RDD 031220 Add New File for Qwikcilver GiftCard Processing 

#include <incs.h>


#define BARCODE_MAX_LEN 32


// RDD 031220 Stub functions : message processing via QC rails

extern void SetResponseTimeStats(VIM_UINT64 elapsed_ticks);


static VIM_ERROR_PTR QC_RespCodeToAS2805(int lQCRespCode, VIM_CHAR* pcDestBuf, VIM_UINT32 ulDestSize)
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
            break; // JIRA

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
            pszAS2805RespCode = "00"; eRet = ERR_WOW_ERC;
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


VIM_CHAR *QC_EntryMode(transaction_t *transaction)
{
    static VIM_CHAR EntryMode = 'K';
    if( txn.card_state == card_capture_new_gift )
    {
        if(( txn.card.type == card_mag) || (transaction_get_status(transaction, st_pos_sent_swiped_pan)))
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


VIM_BOOL QC_GetCommonPadTags(transaction_t* psTxn, char* pszPadBuf, VIM_UINT32 ulBufSize)
{
    VIM_CHAR szTenderID[2 + 1] = { 0 };
    VIM_CHAR szEntryMode[1 + 1] = { 0 };

    if (!psTxn || !pszPadBuf)
        return VIM_FALSE;

    vim_snprintf(szTenderID, sizeof(szTenderID), "%2.2d", PcEFTBin(psTxn));

    VIM_DBG_PRINTF1( "Tender ID returned = %s", szTenderID)

    QC_AppendToPAD(szTenderID, pszPadBuf, ulBufSize, "TND");
    QC_AppendToPAD(QC_EntryMode( psTxn ), pszPadBuf, ulBufSize, "ENT" );

    return VIM_TRUE;
}

static VIM_BOOL QC_ParsePADData(transaction_t* psTxn)
{
    VIM_BOOL fSuccess = VIM_TRUE;
    char szPADCard[256 + 1] = { 0 };
    char szPAD[999 - 6 + 1] = { 0 };
    VIM_SIZE ulPadLen = 0;

    //Populate PAD data
    fSuccess &= QC_GetCommonPadTags(psTxn, szPAD, sizeof(szPAD));

    ulPadLen = vim_strlen(szPAD);
    vim_snprintf(psTxn->u_PadData.ByteBuff, sizeof(psTxn->u_PadData.ByteBuff), "%3.3dWWG%3.3d%s", ulPadLen + 6, ulPadLen, szPAD);

    return fSuccess;
}



VIM_ERROR_PTR QC_GetSAFCount(VIM_SIZE* pulTotalCount, VIM_SIZE* pulReversalCount, VIM_SIZE* pulAdviceCount)
{
    *pulTotalCount = 0;
    *pulReversalCount = 0;
    *pulAdviceCount = 0;
    return VIM_ERROR_NONE;
}

// Latest OTP we have is stored in this buffer.
static VIM_CHAR l_szOTP[32];

const VIM_CHAR* QC_GetOTP(void)
{
    return l_szOTP;
}


VIM_ERROR_PTR QC_SAFUpload(VIM_SIZE ulMaxSends)
{
    VIM_ERROR_RETURN(VIM_ERROR_NOT_IMPLEMENTED);
}



VIM_ERROR_PTR QC_SAFClear(void)
{
    VIM_ERROR_RETURN(VIM_ERROR_NOT_IMPLEMENTED);
}



VIM_ERROR_PTR QC_CheckForAuthChallengeRequest(void)
{
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR QC_Logon(VIM_BOOL fWaitForResponse, VIM_BOOL delete )
{
    return VIM_FALSE;
}






VIM_ERROR_PTR QC_CreateSendMsg(transaction_t *transaction)
{
    // Get Passcode from txn.u_PadData.ByteBuff : PCD00XYYYYYY then remove it from the PAD as don't want to send back to POS !!
    // Get POS Txn Ref from txn.pos_reference ( use first 8 bytes only as R10 sends ref of 8 with trailing spaces ..)
    // Get amount from txn.amout_total
    // Get PAN from txn.card.data.manual.pan and length from txn.card.data.manual.pan_length ( though always 19 for QC )

        // RDD receipt needs a stan which has no relevance to QC so just use the current value
    transaction->stan = terminal.stan;


    VIM_DBG_STRING(transaction->u_PadData.ByteBuff);
    // Create Json file which will get sent by QC application - Comms connection errors returned from here
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR QC_ReceiveParseResp(transaction_t *transaction)
{
    // Wait for response file to be updated and parse into transaction structure
    // Get rid of debug only as Donna likes to test release ver.
    // Simulate Finsim behavior of Cents components as response code to test offline etc. 
    VIM_CHAR Cents[2 + 1] = { 0,0,0 };
    vim_sprintf( Cents, "%2.2d", transaction->amount_total % 100 );
    // RDD 050221 - add balance for Ashan
    transaction->LedgerBalance = 5000;
    transaction->ClearedBalance = 4000;

    if (transaction->type == tt_gift_card_activation)
    {
        if(transaction->amount_total % 100)
            QC_RespCodeToAS2805(10015, transaction->host_rc_asc, sizeof(transaction->host_rc_asc));
        else
            QC_RespCodeToAS2805(00000, transaction->host_rc_asc, sizeof(transaction->host_rc_asc));
    }
    else
    {
        //QC_RespCodeToAS2805(10030, transaction->host_rc_asc, sizeof(transaction->host_rc_asc));
        QC_RespCodeToAS2805(00000, transaction->host_rc_asc, sizeof(transaction->host_rc_asc));
    }

    QC_ParsePADData(transaction);

    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR QC_Transaction( transaction_t *transaction, VIM_BOOL isReversal )
{
    VIM_RTC_UPTIME start_ticks, end_ticks;            
    vim_rtc_get_uptime(&start_ticks);

    VIM_ERROR_RETURN_ON_FAIL( QC_CreateSendMsg( transaction ));


    VIM_ERROR_RETURN_ON_FAIL(QC_ReceiveParseResp( transaction ));

    // RDD 170513 Get the lapsed time for the stats
    vim_rtc_get_uptime(&end_ticks);

    if (end_ticks > start_ticks)
    {
        VIM_DBG_NUMBER(end_ticks - start_ticks);
        SetResponseTimeStats(end_ticks - start_ticks);
    }
    return VIM_ERROR_NONE;
}

#ifndef PCEFT_CARD_DATA_LEN
#define PCEFT_CARD_DATA_LEN 60
#endif

VIM_ERROR_PTR QC_ProcessBarcode(transaction_t *transaction, VIM_CHAR *CardData )
{
    VIM_CHAR Barcode[PCEFT_CARD_DATA_LEN +1];
    VIM_CHAR *Ptr = CardData;
    VIM_CHAR EntryMode = 0;       // RDD 221220 - default to 'B' as NCR actually won't send 'B' or 'K' in input data.
    VIM_SIZE n,Len = 0;

    vim_mem_clear(Barcode, VIM_SIZEOF(Barcode));
    // RDD - fixed length field - could have anyhting in it.
    vim_strncpy( Barcode, Ptr, PCEFT_CARD_DATA_LEN );

    transaction_set_status(&txn, st_pos_sent_pci_pan);  // always when PAN is supplied
    transaction_set_status(&txn, st_electronic_gift_card);

    for (n = 0; n < PCEFT_CARD_DATA_LEN; n++ )
    {
        // known Issue with wex barcodes
        if (!VIM_ISDIGIT(Barcode[n]))
        {
            if (n == 0 && ( Barcode[n] == 'B' || Barcode[n] == 'K'))
            {
                EntryMode = Barcode[n];
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
        Ptr = &Barcode[1];
    else
    {
        EntryMode = 'B';
        Ptr = &Barcode[0];
    }

    VIM_DBG_STRING(Ptr);
    if ((Len = VIM_MIN(PCEFT_CARD_DATA_LEN, Len)) < WOW_GIFT_CARD_LEN_MIN)
    {
        txn.error = VIM_ERROR_POS_CANCEL;
        return txn.error;
    }
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

    txn.card.data.manual.expiry_mm[0] = '1';
    txn.card.data.manual.expiry_mm[1] = '2';
    txn.card.data.manual.expiry_yy[0] = '3';
    txn.card.data.manual.expiry_yy[1] = '0';

    return VIM_ERROR_NONE;
}

VIM_BOOL QC_IsInstalled( VIM_BOOL fReport )
{
    return VIM_TRUE;
}


VIM_ERROR_PTR QC_ClearReversal(void)
{
    DBG_POINT;
    return VIM_ERROR_NONE;
}



