#include <incs.h>


// RDD 011115 SPIRA:8836 Reason Code reflects specific Comms error

  /* VIM_ERROR_PTR,						ASC_RC, PINPAD DISP,				POS DISP ,						POS RESPONSE TEXT,   COMM_ERROR_FLAG,      SCREEN_ID,          PARAMS  */
const error_t terminal_errors[] =
{
	// RDD 090816 v560-8 SPIRA:9016 was MO and needs to be M0 to trigger overnight reboot
    {VIM_ERROR_SYSTEM                   ,    "M0", "Declined\nSystem Error",                    "DECLINED\nSYSTEM ERROR", "SYSTEM ERROR",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_MISMATCH                 ,    "M1", "Declined\nSystem Error",                    "DECLINED\nSYSTEM ERROR", "SYSTEM ERROR",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_TMS_AUTHENTICATE_FAILURE,     "M2", "Application\nRejected",                        "CANCELLED\nAPP REJECTED", "SYSTEM ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_AS2805_FORMAT            ,     "M3", "Message Error\nCancelled",                    "CANCELLED\nMESSAGE FORMAT", "MESSAGE ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NULL                     ,    "M4", "Cancelled\nNULL Field",                        "CANCELLED\nMSG FORMAT ERR",        "MSG FORMAT ERR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_PDU                      ,    "M5", "Cancelled\nMsg Format Error",              "CANCELLED\nMESSAGE FORMAT ERROR",        "MSG FORMAT ERROR",VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NONE                        ,     "00", "Approved",                                    "APPROVED",                                    "APPROVED",            VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {VIM_ERROR_ALREADY_INTIALIZED       ,"00", "Aready Initialised",                                    "APPROVED",                                    "APPROVED",            VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
	{ERR_MOTO_ECOM_DISABLED				,	"T8", "Cancelled\nMOTO Not Enabled",					"Contact Help Desk",					"UNABLE TO PROCESS",	VIM_FALSE, DISP_CANCELLED, txn_line },
	{ERR_LOGON_SUCCESSFUL                ,     "00", "Logon\nSuccessful",                        "WOW LOGON\nSUCCESSFULL",                    "APPROVED",            VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_SETTLE_SUCCESSFUL                ,     "00", "Settlement\nApproved",                    "SETTLEMENT\nWOW APPROVED",                    "APPROVED",            VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_PRESETTLE_SUCCESSFUL            ,     "00", "Pre-Settlement\nApproved",                "PRE-SETTLEMENT\nWOW APPROVED",                "APPROVED",            VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_ONE_CARD_SUCESSFUL                ,     "00", "Thank you for\nusing your Onecard",                "",                                            "APPROVED ONECARD",    VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_LOYALTY_CARD_SUCESSFUL            ,     "00", "Loyalty Card\nRead OK",                        "",                                            "BARCODE READ",        VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_LOGON_FAILED_08                ,     "08", "Declined\nSystem Error",                    "DECLINED\nSYSTEM ERROR",                    "SYSTEM ERROR",        VIM_FALSE,            DISP_DECLINED, txn_line},
    {ERR_NOT_TMS_ON                        ,     "TA", "Cancelled\nRTM Logon Required",                "CANCELLED\nRTM LOGON REQUIRED",            "RTM LOGON REQD",VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CASHOUT_DISABLED               ,    "AQ", "Cancelled\nNo Cash-Out",                     "CANCELLED\nNO CASH-OUT",                    "NO CASH-OUT",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_ACQ_PREAUTH_NOT_ALLOWED        ,    "AP", "Cancelled\nNo Pre Auth",                     "CANCELLED\nNO CASH-OUT",                    "NO PRE-AUTH",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_MANUAL_PAN_DISABLED            ,    "MC", "Cancelled\nNo Manual Entry",                "CARD REJECTED\nNO MANUAL CARD",            "NO MANUAL CARD",    VIM_FALSE,            DISP_CARD_REJECTED, txn_line},
    {ERR_PCI_REBOOT_PENDING               ,    "DG", "Daily Reboot\nPlease Wait",                   "DAILY REBOOT\nPLEASE WAIT",                    "DAILY REBOOT",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_REFUNDS_DISABLED               ,    "AR", "Cancelled\nNo Refund",                   "CARD REJECTED\nNO REFUND",                    "NO REFUND",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_OVER_REFUND_LIMIT_T               ,    "EG", "Transaction Refund\nLimit Exceeded",                   "REJECTED\nNO REFUND",                    "OVER REFUND LIMIT",        VIM_FALSE,            DISP_CANCELLED, txn_line},    
    {ERR_OVER_REFUND_LIMIT_D               ,    "EG", "Daily Refund\nLimit Exceeded",                   "REJECTED\nNO REFUND",                    "OVER REFUND LIMIT",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TRACK2_MISMATCH                ,     "TY", "Card Rejected\nNot Accepted Here",                "CARD REJECTED\nNOT ACCEPTED HERE",                "NOT ACCEPTED",        VIM_FALSE,            DISP_CARD_REJECTED, txn_line}, // JIRA PIRPIN-2174
    {ERR_LOYALTY_MISMATCH                ,     "TZ", "Loyalty Card\nNot Recognised",                "LOYALTY CARD\nNOT RECOGNISED",                "CARD UNKNOWN",        VIM_FALSE,            DISP_CARD_REJECTED, txn_line},  
    {ERR_CARD_EXPIRED                   ,    "TQ", "Card Rejected\nCard Expired",                "CARD REJECTED\nCARD EXPIRED",                "CARD EXPIRED",    VIM_FALSE,            DISP_CARD_REJECTED, txn_line},
    {ERR_INVALID_TRACK2,                     "TY", "Card Read Failed",                            "CARD READ FAILED",                            "CARD READ FAILED",    VIM_FALSE,            DISP_CARD_REJECTED, txn_line},
    {ERR_MCR_BLACKLISTED_BIN,                "TY", "Card Read Error\nPlease Retry",               "CARD READ FAILED",                            "CARD READ FAILED",    VIM_FALSE,            DISP_CARD_REJECTED, txn_line},
    {ERR_LUHN_CHECK_FAILED              ,    "TC", "Card Rejected\nInvalid Card No",             "CARD REJECTED\nINVALID CARD NO",            "INVALID CARD NO",VIM_FALSE,        DISP_CARD_REJECTED, txn_line},
    {ERR_CARD_REJECTED_NO_BAL           ,    "FE", "Card Rejected\nNo Bal Enq",                    "CARD REJECTED\nNO BAL ENQ",                "COMPLETE",           VIM_FALSE,        DISP_CARD_REJECTED, txn_line},
    {VIM_ERROR_EMV_APPLICATION_BLOCKED    ,  "EJ", "Card Rejected\nContact Issuer",                "CARD REJECTED\nCONTACT ISSUER",            "CONTACT ISSUER",       VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERR_NO_CASHOUT                        , "TE", "Card Rejected\nNo Cash Out",                "CARD REJECTED\nNO CASH OUT",                "NO CASH OUT",            VIM_FALSE,        DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_CARD_BLOCKED         ,    "E3", "Cancelled\nCard Blocked",                    "CANCELLED\nCARD BLOCKED",                    "CARD BLOCKED",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_ALL_APPS_BLOCKED     ,    "E4", "Cancelled\nApp Blocked",                     "CANCELLED\nAPP BLOCKED",                    "APP BLOCKED", VIM_FALSE,        DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_CARD_ERROR           ,    "E5", "Cancelled\nCard Error",                          "CANCELLED\nCARD ERROR",                    "CARD ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_AID_NOT_SUPPORTED              ,    "E6", "Cancelled\nCard Not Accepted",                 "CANCELLED\nCARD NOT ACCEPTED",                "CARD NOT ACCEPTED",  VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERR_EMV_CONFIG_ERROR               ,    "E7", "Cancelled\nEMV Config Error",                "CANCELLED\nEMV CONFIG ERROR",                "EMV CONFIG ERROR",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_KERNEL_ERROR         ,    "E7", "Cancelled\nEMV Config Error",                "CANCELLED\nEMV CONFIG ERROR",        "EMV CONFIG ERROR",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_MSG                    ,    "E8", "Cancelled\nInvalid Message",                 "CANCELLED\nINVALID MESSAGE",        "INVALID MESSAGE",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_TRACK2_MISMATCH      ,    "E9", "Cancelled\nEMV Data Mismatch",                "CANCELLED\nEMV DATA MISMATCH",        "EMV DATA MISMATCH",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_EMV_PIN_TRIES_EXCEEDED         ,    "EA", "Cancelled\nPIN Tries Exceeded",                "CANCELLED\nPIN TRIES EXCEEDED",        "PIN TRIES EXCEEDED", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_ALL_APPS_BLOCKED     ,    "EC", "Cancelled\nInvalid Card",                    "CANCELLED\nINVALID CARD",        "INVALID CARD",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_PRODUCT                ,    "ED", "Cancelled\nInvalid Product",                 "CANCELLED\nINVALID PRODUCT",        "INVALID PRODUCT",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_EMV_FALLBACK_NOT_ALLOWED       ,    "EE", "Cancelled\nSwipe Not Allowed",                 "CANCELLED\nSWIPE NOT ALLOWED",        "SWIPE NOT ALLOWED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CHIP_READ_ERROR                ,    "EH", "Cancelled\nChip Read Error",                 "CANCELLED\nCHIP READ ERROR",        "CHIP READ ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_SET_MANAGER_PW                 ,    "EI", "Cancelled\nSet Password",                    "CANCELLED\nSET PASSWORD",        "SET MANAGER PASSWORD",  VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERR_TIP_OVER_MAX                   ,    "MT", "Cancelled\nTip Over Max %",                  "CANCELLED\nTIP OVER MAX %",        "TIP OVER MAX %",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CASH_OVER_MAX                   ,   "MU", "Cancelled\nCashout Over Max",                  "CANCELLED\nCASHOUT OVER MAX",        "CASH OVER MAX",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {MSG_ERR_FORMAT_ERR                 ,    "XB", "Cancelled\nMsg Format Error",                  "CANCELLED\nMSG FORMAT ERROR",        "MESSAGE FORMAT ERROR",  VIM_FALSE,        DISP_CANCELLED, txn_line}, // RDD 251012 SPIRA:6220 Georgia asked for this !
    {VIM_ERROR_OVERFLOW                 ,    "M0", "Cancelled\nSystem Error 2",                  "CANCELLED\nSYSTEM ERROR 2",        "SYSTEM ERROR 2",  VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERROR_CNP                          ,    "M1", "Cancelled\nCNP Error",                       "CANCELLED\nCNP ERROR",        "CNP ERROR",          VIM_FALSE,            DISP_CANCELLED, txn_line},
    {MSG_ERR_STAN_MISMATCH              ,    "M2", "Cancelled\nSTAN Mismatched",                 "CANCELLED\nSTAN MISMATCHED",        "STAN MISMATCHED",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {MSG_ERR_CATID_MISMATCH             ,    "M3", "Cancelled\nTerm ID Mismatch",                "CANCELLED\nTERM ID MISMATCH",        "TERM ID MISMATCH",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_WOW_COMMS_ERROR                ,     "M4",    "Cancelled",        "CANCELLED",                    "COMMS ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_BUFFER_TOO_SMALL               ,    "N0", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",        "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_FILE_SYSTEM_ERROR              ,    "N1", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",        "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_RECORD_DOESNT_EXIST            ,    "N5", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",        "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_TLV                    ,    "N6", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",        "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TAG_NOT_FOUND                  ,    "N7", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",        "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_TOTAL_AMOUNT           ,    "NA", "Cancelled\nZero Amount",                     "CANCELLED\nZERO AMOUNT",        "ZERO AMOUNT",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_ROC_NO_APPR                 ,    "NR", "Cancelled\nNo RRN No Appr",                  "CANCELLED\nNO RRN NO APPR",        "NO RRN NO APPR",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TRANSMISSION_FAILURE           ,    "X1", "Cancelled\nComms Error",                     "CANCELLED\nCOMMS ERROR",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_HOST_RECV_TIMEOUT              ,    "X0", "Cancelled\nReceive Timeout",                 "CANCELLED\nRECEIVE TIMEOUT",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_POS_NOT_SUPPORTED              ,    "X0", "Cancelled\nComms Error",                     "CANCELLED\nCOMMS ERROR",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_POS_PROTOCOL_ERROR             ,    "X2", "Cancelled\nComms Error",                     "CANCELLED\nCOMMS ERROR",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_POS_RSP_INVALID                ,    "X3", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",       "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_POS_COMMS_ERROR                ,    "X4", "Cancelled\nComms Error",                     "CANCELLED\nCOMMS ERROR",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_CONNECT_TIMEOUT          ,    "X5", "Cancelled\nComms Error",                     "CANCELLED\nCOMMS ERROR",        "COMMS ERROR",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_POS_REQUEST_INVALID      ,    "XG", "Cancelled\nInvalid Software",                "CANCELLED\nINVALID REQUEST",        "INVALID REQUEST",VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_PRINTER                  ,    "PF", "Cancelled\nPrint Failed",                    "CANCELLED\nPRINT FAILED",        "PRINT FAILED",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_PAPER                    ,     "PF", "Cancelled\nNo Printer Paper",                "CANCELLED\nNO PAPER",            "NO PAPER",                VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_POS_REQ_INVALID                ,    "PR", "Cancelled\nInvalid Request",                 "CANCELLED\nINVALID REQUEST",        "POS REQUEST INVALID",VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERR_POWER_FAIL                     ,    "Q5", "Cancelled\nPower Fail",                      "CANCELLED\nPOWER FAIL",        "POWER FAIL",         VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CANNOT_PROCESS_EPAN            ,     "R1", "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",            "CANNOT PROCESS EPAN", VIM_FALSE,        DISP_CANCELLED, txn_line},
    {VIM_ERROR_EMV_CARD_REMOVED         ,    "RC", "Cancelled\nCard Removed",                    "CANCELLED\nCARD REMOVED",        "CARD REMOVED",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_RECORD_DOESNT_EXIST            ,    "RN", "Cancelled\nRRN Not Found",                   "CANCELLED\nRRN NOT FOUND",        "RRN NOT FOUND",      VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_ROC_COMPLETED                    ,    "RN", "Cancelled\nRRN Completed",                   "CANCELLED\nRRN COMPLETED",        "RRN COMPLETED",      VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_ROC_EXPIRED                    ,    "RP", "Cancelled\nRRN Expired",                       "CANCELLED\nRRN EXPIRED",        "RRN EXPIRED",      VIM_FALSE,                DISP_CANCELLED, txn_line},
    {ERR_INVALID_AUTH_NO                ,    "RO", "Cancelled\nInvalid Auth No",                 "CANCELLED\nINVALID AUTH NO",        "INVALID AUTH NO",    VIM_FALSE,        DISP_CANCELLED, txn_line},
    {ERR_INVALID_ROC                    ,    "RV", "Cancelled\nInvalid RRN",                     "CANCELLED\nINVALID RRN",        "INVALID RRN",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_PCEFTPOS_NOT_CONNECTED   ,    "RT", "Cancelled\nNo EFT Server",                   "CANCELLED\nNO EFT SERVER",        "NO EFT SERVER",      VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_MODEM                    ,    "R1", "Cancelled\nModem Error",                     "CANCELLED\nMODEM ERROR",        "MODEM ERROR",        VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_DIAL_TONE             ,    "R2", "Cancelled\nNo Dial Tone",                    "CANCELLED\nNO DIAL TONE",        "NO DIAL TONE",       VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_ANSWER                ,    "R3", "Cancelled\nNo Answer",                       "CANCELLED\nNO ANSWER",        "NO ANSWER",          VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_DIAL_BUSY                ,    "R4", "Cancelled\nNumber Busy",                     "CANCELLED\nNUMBER BUSY",        "NUMBER BUSY",        VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_NO_HOST_NO                     ,    "R5", "Cancelled\nNo Host Number",                  "CANCELLED\nNO HOST NUMBER",        "NO HOST PHONE NUMBER", VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_DISCONNECTED             ,    "R6", "Cancelled\nLine Disconnected",                "CANCELLED\nLINE DISCONNECTED",        "LINE DISCONNECTED",  VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_CARRIER               ,    "R7", "Cancelled\nNo Carrier",                      "CANCELLED\nNO CARRIER",        "NO CARRIER",         VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_EFT_SERVER            ,    "R8", "Cancelled\nNo EFT Server",					"CANCELLED\nNO EFT SERVER", "NO EFT SERVER",      VIM_TRUE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_PHONE_LINE            ,    "R9", "Cancelled\nNo Line",						"CANCELLED\nNO LINE", "NO LINE",      VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_NO_LINK_SETUP_EFT_SERVER       ,    "RT", "Cancelled\nNo EFT Server",        "CANCELLED\nNO EFT SERVER", "NO EFT SERVER",      VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_PHONE_LINE                  ,    "R9", "Cancelled\nNo Phone Line",                   "CANCELLED\nNO PHONE LINE",        "NO PHONE LINE",      VIM_TRUE,            DISP_CANCELLED, txn_line},
    {ERR_INTERRUPTED_BY_POS             ,    "SD", "Cancelled\nPOS Interrupt",                   "CANCELLED\nPOS INTERRUPT",        "INTERRUPTED BY POS", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_POS_GET_STATUS_OK              ,    "T0", "Ready",                                        "READY",             "READY",               VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_PP_BUSY                        ,    "BY", "Pinpad Busy",                                "PINPAD BUSY",             "PINPAD BUSY",               VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_POS_GET_STATUS_APPROVED        ,    "00", "",                                            "",                                 "TRANSACTION ACCEPTED", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_DUPLICATE_TXN               ,    "T1", "Cancelled\nNo Last Txn",                     "CANCELLED\nNO LAST TXN",        "NO LAST TXN",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_DUPLICATE_RECEIPT           ,    "T2", "Cancelled\nNo Duplicate Receipt",            "CANCELLED\nNO DUPLICATE RECEIPT",     "NO DUPLICATE RECEIPT", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_ACCOUNT                ,    "TZ", "Cancelled\nInvalid Account",                 "CANCELLED\nINVALID ACCOUNT",        "INVALID ACCOUNT",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_LAST_SETTLEMENT             ,    "T4", "Cancelled\nNo Last Settlement",            "CANCELLED\nNO LAST SETTLMENT",        "NO LAST SETTLEMENT", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CARD_NOT_ACCEPTED              ,    "TA", "Cancelled\nCard Not Accepted",             "CANCELLED\nCARD NOT ACCEPTED",        "CARD NOT ACCEPTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_INVALID_TENDER                 ,    "TA", "Invalid Card\nUse Gift Card Tender",         "USE GIFT CARD TENDER",        "CARD NOT ACCEPTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_FILE_DOWNLOAD                  ,    "TD", "Cancelled\nFile Load Error",                 "CANCELLED\nFILE LOAD ERROR",        "FILE LOAD ERROR",    VIM_FALSE,            DISP_CANCELLED, display_line},
    {ERR_NOT_LOGGED_ON                  ,    "TF", "Cancelled\nRSA Logon Required",                  "CANCELLED\nLOGON REQUIRED",        "LOGON REQUIRED",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TERMINAL_LOCKED                ,    "TF", "Cancelled\nLogon Required",                  "CANCELLED\nLOGON REQUIRED",        "LOGON REQUIRED",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_TIMEOUT                  ,    "TI", "Cancelled\nOperator Timeout",                "CANCELLED\nOPERATOR TIMEOUT",        "OPERATOR TIMEOUT",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_SIGNATURE_DECLINED             ,    "TL", "Declined\nSignature Error",                    "DECLINED\nSIGNATURE ERROR","SIGNATURE ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_POS_CANCEL               ,    "TM", "Cancelled\nBy Operator",                       "CANCELLED\nBY OPERATOR",     "OPERATOR CANCELLED",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_USER_CANCEL              ,    "TM", "Cancelled\nBy Customer",                       "CANCELLED\nBY CUSTOMER",     "CUSTOMER CANCELLED", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_RETRY_OF_TIP                ,    "TR", "Cancelled\nNo Retry of Tip",                 "CANCELLED\nNO RETRY OF TIP",        "NO RETRY OF TIP",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_SAF_PENDING                    ,    "TU", "Cancelled\nSAF Pending",                     "CANCELLED\nSAF PENDING",        "SAF PENDING",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_REVERSAL_PENDING               ,    "TV", "Cancelled\nReversal Pending",                "CANCELLED\nREVERSAL PENDING",        "REVERSAL PENDING",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_CPAT_MATCH                  ,    "TX", "Cancelled\nCard Not Accepted",                 "CANCELLED\nCARD NOT ACCEPTED",        "CARD NOT ACCEPTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
  {VIM_ERROR_EMV_UNABLE_TO_PROCESS    ,      "TX", "Cancelled\nUnable to Process",                "CANCELLED\nUNABLE TO PROCESS",        "UNABLE TO PROCESS",  VIM_FALSE,     DISP_CANCELLED, txn_line},
  {VIM_ERROR_ECR_REQUEST_NOT_SUPPORTED,      "T8", "Cancelled\nUnable To Process",                "CANCELLED\nUNABLE TO PROCESS",      "UNABLE TO PROCESS",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_PASSWORD_MISMATCH              ,    "TG", "Cancelled\nPassword Mismatch",                "CANCELLED\nPASSWORD MISMATCH",        "PASSWRD MISMATCH",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_SAF_FULL                    ,     "TP", "Cancelled\nPinpad Full",                    "CANCELLED\nPINPAD FULL",        "PINPAD FULL",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_ALREADY_VOIDED                    ,     "VR", "Cancelled\nTxn Already Void",                "CANCELLED\nTXN ALREADY VOID",            "TXN ALREADY VOID", VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_VOID_ON_DEBIT               ,    "VD", "Cancelled\nNo Void On Debit",                "CANCELLED\nNO VOID ON DEBIT",        "NO VOID ON DEBIT",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_ALREADY_VOIDED                 ,    "VR", "Cancelled\nTxn Already Void",                "CANCELLED\nTXN ALREADY VOID",        "TXN ALREADY VOID",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    { VIM_ERROR_TMS_MAC                  ,    "WS", "RTM Cancelled\nInvalid SHA1",                "RTM CANCELLED\nINVALID SHA1",        "RTM INVALID SHA1",   VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_RESPONSE                    ,     "X0",  "Cancelled\nNo Response",                    "CANCELLED",                    "NO RESPONSE",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NO_LINK                    ,     "RA",    "Cancelled\nNo Link",                        "CANCELLED",                    "COMMS ERROR",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_MAC_ERROR                      ,    "X7",  "Cancelled\nSystem Error",                    "CANCELLED\nSYSTEM ERROR",  "SYSTEM ERROR",       VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_KVC_ERROR                        ,     "X8",    "Logon Failed\nKVC System Error",            "KVC SYSTEM ERROR",                    "LOGON FAILED",        VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CNP_ERROR                      ,    "XG", "Logon Failed\nInvalid Software",            "LOGON FAILED",     "INVALID SOFTWARE",          VIM_FALSE,            DISP_DECLINED, txn_line},
    {ERR_KEYED_NOT_ALLOWED              ,    "XD", "Cancelled\nNo Manual Entry",                 "CANCELLED\nNO MANUAL ENTRY",        "NO MANUAL ENTRY",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TXN_TYPE_NOT_ON_CARD           ,    "XE", "Cancelled\nTxn Not Supported",               "CANCELLED\nTXN NOT SUPPORTED",        "TXN NOT SUPPORTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_TXN_NOT_SUPPORTED              ,    "XG", "Cancelled\nTxn Not Supported",               "CANCELLED\nTXN NOT SUPPORTED",        "TXN NOT SUPPORTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_NOT_IMPLEMENTED          ,    "XG", "Cancelled\nTxn Not Supported",               "CANCELLED\nTXN NOT SUPPORTED",        "TXN NOT SUPPORTED",  VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_NO_KEYS_LOADED                 ,    "XT", "Cancelled\nConfig Required",                 "CANCELLED\nCONFIG REQUIRED",        "CONFIG REQUIRED",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_CONFIGURATION_REQUIRED         ,    "XT", "Cancelled\nConfig Required",                 "CANCELLED\nCONFIG REQUIRED",        "CONFIG REQUIRED",    VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_BATCH_FULL                     ,    "XU", "Cancelled\nBatch Full",                      "CANCELLED\nBATCH FULL",        "BATCH FULL",         VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_EMV_CARD_APPROVED              ,    "Y1", "Approved",                        "APPROVED",        "APPROVED",           VIM_FALSE,            DISP_CANCELLED, txn_line}, /* EMV Y1 - offline approved */
    {ERR_UNABLE_TO_GO_ONLINE_APPROVED   ,    "Y3", "Approved",                        "APPROVED",        "APPROVED",           VIM_FALSE,            DISP_CANCELLED, txn_line}, /* EMV Y3 - EMV unable to go online approved */
    {ERR_EMV_CARD_DECLINED              ,    "Z1", "Declined\nContact Issuer",    "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",     VIM_FALSE,            DISP_DECLINED, txn_line}, /* EMV Z1 - offline declined */
    {VIM_ERROR_PICC_EMV_DDA_FAILURE     ,    "Z1", "Declined\nContact Issuer",    "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",     VIM_FALSE,            DISP_DECLINED, txn_line},
    {VIM_ERROR_PICC_EMV_AUTHENTICATION_FAILURE,"Z1", "Declined\nContact Issuer",  "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",     VIM_FALSE,            DISP_DECLINED, txn_line},
    {ERR_UNABLE_TO_GO_ONLINE_DECLINED   ,    "Z3", "Declined\nContact Issuer",    "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",       VIM_FALSE,            DISP_DECLINED, txn_line}, /* EMV Z3 - unable to go online declined */
    {ERR_HOST_APPROVED_CARD_DECLINED    ,    "Z4", "Declined\nContact Issuer",    "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",     VIM_FALSE,            DISP_DECLINED, txn_line}, /* EMV host approved, card declined */
        // RDD 160713 SPIRA:6773 Reversal Response was not waited for after this failure for offline PIN cards because this flag had not been cleared
        // which in turn caused the command check to exit with VIM_ERROR_NONE, which in turn caused the reversal to be cleared.
    {VIM_ERROR_EMV_SECOND_GENAC_FAILED  ,    "Z4", "Declined\nContact Issuer",    "DECLINED\nCONTACT ISSUER",  "CONTACT ISSUER",     VIM_FALSE,            DISP_DECLINED, txn_line}, /* EMV host approved, card declined */
    {ERR_QC_AUTH_FAILURE               ,    "GS",	"Store Check Fail\n( QC Gift Cards)",	    "QC LOGON FAILED",	    "QC LOGON FAILED",	    VIM_FALSE,			DISP_CANCELLED, txn_line },
	{ERR_RTM_AUTH_FAILURE               ,    "GR",	"Cancelled\nRTM Error1",	    "CANCELLED\nRTM ERROR1",	    "QC RTM ERROR1",	    VIM_FALSE,			DISP_CANCELLED, txn_line },
	{ERR_RTM_AUTH_FAILURE2               ,   "G6",	"Cancelled\nRTM Error2",	    "CANCELLED\nRTM ERROR2",	    "QC RTM ERROR2",	    VIM_FALSE,			DISP_CANCELLED, txn_line },
	{ERR_SIGNATURE_APPROVED_OFFLINE     ,    "08", "Approved",            "APPROVED",        "APPROVED",           VIM_FALSE,            DISP_TRANSACTION_APPROVED, txn_line},
    {VIM_ERROR_PICC_COLLISION           ,    "TS", "Cancelled\nMultiple Cards",                  "CANCELLED\nMULTIPLE CARDS",        "MULTIPLE CARDS",     VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_PRESWIPE_INCOMPLETE            ,    "32", "",                    "",        "INCOMPLETE",         VIM_FALSE,            DISP_CANCELLED, txn_line},
    {ERR_PRESWIPE_COMPLETE              ,    "30", "",                    "",        "COMPLETE",           VIM_FALSE,            DISP_CANCELLED, txn_line},
    {VIM_ERROR_PICC_CARD_DECLINED       ,    "Z1", "Declined",                        "DECLINED",        "CONTACT ISSUER",     VIM_FALSE,            DISP_CANCELLED, txn_line}, /* Contactless transaction declined and doesn't need fallback */
    {ERR_CPAT_NO_DOCK                    ,     "Y0",    "Card\nMust Be Swiped",        "CARD\nMUST BE SWIPED",                    "UNABLE TO PROCESS",    VIM_FALSE,            DISP_CANCELLED, txn_line},
};


const error_t host_errors[] =
																								// RDD 111215 POS TEXT Max 16 chars or CRASH ! because 24 byte print limit incl. 6 byte STAN + space + NULL
{
  /* VIM_ERROR_PTR,		ASC_RC, PINPAD DISP ,						POS DISPLAY,				POS RESPONSE TEXT,      COMM_ERROR_FLAG,      SCREEN_ID,          PARAMS  */
	{ERR_WOW_ERC,		"XX",	"Declined\nSystem Error",		"DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"F0",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID PRODUCT",	"INVALID PRODUCT",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"F1",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID VOLUME DATA","INVALID VOLUME DATA",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"F2",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID PRICE",		"INVALID PRICE",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"F3",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID PRODUCT AMT","INVALID PRODUCT AMT",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"F4",	"Cancelled\nSystem Error",		"CANCELLED\nMISSING PRODUCT ID","MISSING PRODUCT ID",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"F5",	"Cancelled\nSystem Error",		"CANCELLED\nTRAN BALANCE ERROR","TRAN BALANCE ERROR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"FD",	"Cancelled\nNo Discount",		"CANCELLED\nNO DISCOUNT",		"NO DISCOUNT",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"FE",	"Card Rejected\nNo Bal Enq",	"CARD REJECTED\nNO BAL ENQ",	"NO BALANCE",			VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"FL",	"Cancelled\nNo Loyalty",		"CANCELLED\nNO LOYALTY",		"NO LOYALTY",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"FR",	"Card Rejected\nNo Refund",		"CARD REJECTED\nNO REFUND",		"NO REFUND",			VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"FS",	"Cancelled\nNo Split Tender",	"CANCELLED\nNO SPLIT TENDER",	"NO SPLIT TENDER",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"FV",	"Voucher Entry\nNot Allowed",	"VOUCHER ENTRY\nNOT ALLOWED",	"NO VOUCHER ENTRY",		VIM_FALSE,			DISP_CANCELLED, txn_line},

    // RDD 300321 Add Some New codes for already activated gift cards
    {ERR_WOW_ERC,		"GU",	"Gift Card\nNot Activated",	    "GIFT CARD\nNOT ACTIVATED",	"NOT ACTIVATED",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GA",	"Gift Card\nAlready Active",	"GIFT CARD\nALREADY ACTIVE",	"ALREADY ACTIVE",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GI",	"Gift Card\nInvalid Activation",	"GIFT CARD\nINVALID",	"INVALID ACTIVATION",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GB",	"Gift Card\nInsufficient Balance",	"GIFT CARD\nBALANCE EXCEEDED",	"BALANCE EXCEEDED",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GH",	"Gift Card\nExceeds Max Amount",	"GIFT CARD\nEXCEEDS MAX AMT",	"AMOUNT EXCEEDED",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GD",	"Gift Card\nDeactivated",	    "GIFT CARD\nDEACTIVATED",	    "CARD DEACTIVATED",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"GP",	"Declined\nIncorrect Passcode",  "DECLINED\nINCORRECT PASSCODE",  "PASSCODE ERROR",	    VIM_FALSE,			DISP_DECLINED, txn_line}, /*different for full emv*/
    // RDD 200223 JIRA PIRPIN-2232 aDD NEW eRROR
    {ERR_WOW_ERC,		"GX",	"Card Deactivated\nZero Balance Check",  "DECLINED\nZERO BALANCE CHECK",  "ZERO BALANCE",	    VIM_FALSE,			DISP_DECLINED, txn_line}, 

    {ERR_WOW_ERC,		"T0",	"",			"",	"",		    VIM_FALSE,			DISP_DECLINED, no_data},
	{ERR_WOW_ERC,		"T3",	"Declined\nBank Unavailable",	"DECLINED\nBANK UNAVAILABLE",	"BANK UNAVAILABLE",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"T4",	"Cancelled\nSystem Error",		"CANCELLED\nSYSTEM ERROR",		"INVALID SAVED TXN",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"T5",	"Cancelled\nSystem Error",		"CANCELLED\nSYSTEM ERROR",      "INVALID TRAN REF NO",  VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"T6",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID TRAN DATE", "INVALID TRAN DATE",    VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"T7",	"Cancelled\nSystem Error",		"CANCELLED\nINVALID TRAN TIME", "INVALID TRAN TIME",    VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"T8",	"Cancelled\nSystem Error",		"CANCELLED\nSYSTEM ERROR",      "UNABLE TO PROCESS",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"R2",	"Cancelled\nSystem Error",		"CANCELLED\nSYSTEM ERROR",      "TOKEN UNAVAILABLE",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"T9",	"Mobile Pay Declined\nPlease Use Card","MOBILE PAY DECLINED\nPLEASE USE CARD", "RETRY WITH CARD", VIM_FALSE,	DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"TA",	"Cancelled\nTMS Logon Required","CANCELLED\nTMS LOGON REQUIRED","RTM LOGON REQUIRED",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TB",	"Card Rejected\nCard Invalid",	"CARD REJECTED\nCARD INVALID",  "CARD INVALID",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TT",	"Declined\nPlease Insert Card","DECLINED\nPLEASE INSERT CARD", "INSERT CARD",           VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"TD",	"",								"",								"",						VIM_FALSE,			DISP_BLANK, no_data},
	{ERR_WOW_ERC,		"TE",	"Card Rejected\nNo Cash Out",	"CARD REJECTED\nNO CASH OUT",	"NO CASH OUT",			VIM_FALSE,		    DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"TF",	"Cancelled\nLogon Reqrd",		"CANCELLED\nLOGON REQURD",      "WOW LOGON REQUIRED",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TG",	"Cancelled\nComms Error",		"CANCELLED\nCOMMS ERROR",       "DISPLAY ERROR",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TH",	"Cancelled\nComms Error",		"CANCELLED\nCOMMS ERROR",       "PRINTER ERROR",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TI",	"Cancelled\nOperator Timeout",	"CANCELLED\nOPERATOR TIMEOUT",  "OPERATOR TIMEOUT",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TK",	"",								"",								"",				        VIM_FALSE,			DISP_BLANK, no_data},
	{ERR_WOW_ERC,		"TM",	"Cancelled\nBy Operator",		"CANCELLED\nBY OPERATOR",		"OPERATOR CANCEL",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TP",	"Cancelled\nPinpad Full",		"CANCELLED\nPINPAD FULL",		"PINPAD FULL",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TQ",	"Card Rejected\nCard Expired",	"CANCELLED\nCARD EXPIRED",      "CARD EXPIRED",			VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"TU",	"Cancelled\nSAF Pending",		"CANCELLED\nSAF PENDING",       "SAF PENDING",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TV",	"Cancelled\nReversal Pending",	"CANCELLED\nREVERSAL PENDING",  "REVERSAL PENDING",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TW",	"Card Rejected\nNo Deposit",	"CARD REJECTED\nNO DEPOSIT",	"NO DEPOSIT",			VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"TX",	"Cannot Process",				"CANNOT PROCESS",		        "UNABLE TO PROCESS",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"TY",	"Card Rejected\nNot Accepted Here",	"CARD REJECTED\nNOT ACCEPTED HERE",	"CARD UNKNOWN",	VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"TZ",	"Cancelled\nInvalid Account",	"CANCELLED\nINVALID ACCOUNT",   "INVALID ACCOUNT",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"RT",	"Cancelled\nNo Response",		"CANCELLED\nNO RESPONSE",       "NO RESPONSE",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"X3",	"FVV System Error",				"FVV SYSTEM ERROR",		        "FVV SYSTEM ERROR",		VIM_FALSE,			DISP_BLANK, no_data},
	{ERR_WOW_ERC,		"X4",	"System Error",					"SYSTEM ERROR",		            "SYSTEM ERROR",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"X6",	"System Error",					"SYSTEM ERROR",		            "SYSTEM ERROR",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"X7",	"System Error",					"SYSTEM ERROR",		            "SYSTEM ERROR",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"X8",	"KVC System Error",				"KVC SYSTEM ERROR",		        "KVC SYSTEM ERROR",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,       "XB",   "Cancelled\nMsg Format Error",  "CANCELLED\nMSG FORMAT ERROR",  "MESSAGE FORMAT ERROR",  VIM_FALSE,		DISP_CANCELLED, txn_line}, // RDD 251012 SPIRA:6220 Georgia asked for this !
	{ERR_WOW_ERC,		"XC",	"System Error",					"SYSTEM ERROR",		            "SYSTEM ERROR",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"XD",	"Card Invalid",					"CARD INVALID",		            "CARD INVALID",			VIM_FALSE,			DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"XE",	"Card Rejected\nInvalid Month",	"CARD REJECTED\nINVALID MONTH", "INVALID MONTH",		VIM_FALSE,		DISP_CARD_REJECTED, txn_line},
	{ERR_WOW_ERC,		"XG",	"Software Error",				"SOFTWARE ERROR",		        "INVALID SOFTWARE",		VIM_FALSE,			DISP_BLANK, txn_type},
	{ERR_WOW_ERC,		"XK",	"",								"",								"NO DUPLICATE PRESENT",	VIM_FALSE,			DISP_BLANK, txn_line},
	{ERR_WOW_ERC,		"RD",	"Card Read Failed",				"CARD READ FAILED",		        "CARD READ FAILED",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"RE",	"Server Error W1",				"SERVER ERROR W1",		        "",						VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"R0",	"",								"",								"NO SAF PRINT DATA",	VIM_FALSE,			DISP_BLANK, txn_line},
	{ERR_WOW_ERC,		"RP",	"Declined",						"DECLINED\nEXCEED PIN TRIES",	"EXCEED PIN TRIES",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"RT",	"Cancelled",					"CANCELLED",		            "COMMS ERROR",			VIM_FALSE,			DISP_CANCELLED, txn_line},

	{ERR_WOW_ERC,		"L5",	"Declined\nRetry PIN",		    "DECLINED\nRETRY PIN",			"PIN ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"OJ",	"Re-Key Odometer",				"RE-KEY ODOMETER",				"ODOMETER ERROR",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"OD",	"Declined\nOver Daily Volume",	"DECLINED\nOVER DAILY VOLUME",	"OVER DAILY VOLUME",	VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"OM",	"Monthly Volume\nExceeded",     "DECLINED\nOVER MONTHLY VOLUME","OVER MONTHLY VOLUME",	VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"OP",	"Per Transaction\nVolume Exceeded","DECLINED\nOVER PER TXN VOLUME","OVER PER TXN VOLUME",	VIM_FALSE,		DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"DX",	"Declined\nProd XX Not Allowed","DECLINED\nPROD XX NOT ALLOWED","PRODUCT NOT ALLOWED",	VIM_FALSE,			DISP_DECLINED, txn_line},

  /* VIM_ERROR_PTR, ASC_RC, PINPAD DISP,							POS DISPLAY,				POS RESPONSE TEXT,      COMM_ERROR_FLAG,      SCREEN_ID,          PARAMS  */
    {ERR_WOW_ERC,       "WS",   "RTM Cancelled\nInvalid SHA1",  "RTM CANCELLED\nINVALID SHA1",  "RTM INVALID SHA1",     VIM_FALSE,          DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"W0",	"",						"",								"",								VIM_FALSE,			DISP_TRANSACTION_APPROVED, txn_line},
	{ERR_WOW_ERC,		"W1",	"Requested File\nNot Found",	"REQUESTED FILE\nNOT FOUND",	"RTM FILE NOT FOUND",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W2",	"RTM Server\nSystem Down",		"RTM SERVER\nSYSTEM DOWN",		"RTM SRV SYSTEM ERROR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W3",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W4",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W5",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W6",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W7",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W8",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"W9",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"WA",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"WB",	"RTM Server\nSetup Error",		"WebLFD Server\nNo Response",	"LFD SERVER NO RESP",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"WC",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"WD",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"WE",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"WF",	"LFD CFG File\nError",		    "CFG FILE\nFORMAT ERR",		    "CFG FORMAT ERR",	    VIM_FALSE,			DISP_CANCELLED, txn_line},
    {ERR_WOW_ERC,		"WZ",	"RTM Server\nSetup Error",		"RTM SERVER\nSETUP ERROR",		"RTM SERVER SETUP ERR",	VIM_FALSE,			DISP_CANCELLED, txn_line},

  /* VIM_ERROR_PTR,		ASC_RC, PINPAD DISP,     POS DISPLAY,     POS RESPONSE TEXT,      COMM_ERROR_FLAG,      SCREEN_ID,          PARAMS  */
	{ERR_WOW_ERC,		"00",	"Approved",		                    "APPROVED",		            "APPROVED",				VIM_FALSE,			DISP_TRANSACTION_APPROVED, txn_line}, /*different for gift card*/
	{ERR_WOW_ERC,		"01",	"Declined\nContact Issuer",		    "DECLINED\nCONTACT ISSUER",	"CONTACT BANK",		    VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"02",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"03",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"04",   "Declined\nContact Bank",         "DECLINED\nCONTACT BANK",     "CALL SUPERVISOR",      VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"05",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"06",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"07",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"08",	"Approved",		    "APPROVED",		            "APPROVED",				VIM_FALSE,			DISP_TRANSACTION_APPROVED, txn_line},
    {ERR_WOW_ERC,		"09",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"10",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"12",	"Declined\nInvalid",		    "DECLINED\nINVALID",		    "INVALID TRANSACTION",	VIM_FALSE,			DISP_DECLINED, txn_line},
	//{ERR_WOW_ERC,		"13",	"Declined\nInvalid Amount",		    "DECLINED\nINVALID AMOUNT",	"INVALID AMOUNT",		VIM_FALSE,			DISP_DECLINED, txn_line_bal_clr}, /*different for gift card*/
	{ERR_WOW_ERC,		"13",	"Declined\nInvalid Amount",		    "DECLINED\nINVALID AMOUNT",	"INVALID AMOUNT",		VIM_FALSE,			DISP_DECLINED, txn_line}, /*RDD 020113 SPIRA:6488 */
	{ERR_WOW_ERC,		"14",	"Declined\nInvalid Card Number",		    "DECLINED\nINVALID CARD NUMBER",  "CARD NUMBER INVALID",	VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"15",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"16",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"17",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"18",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"19",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"20",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"21",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"22",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"23",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"24",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"25",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"26",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"27",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"28",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"29",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"30",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"31",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"32",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"33",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"34",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"35",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"36",	"Declined\nBank Unavailable",		    "DECLINED\nBANK UNAVAILABLE",	"NO BANK DO MANUAL",	VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"37",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"38",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"39",	"Declined\nNo Credit Account",		    "DECLINED\nNO CREDIT ACCOUNT",	"NO CREDIT ACCOUNT",	VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"40",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"41",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"42",	"Declined\nNo Account",	        "DECLINED\nNO ACCOUNT",		"NO ACCOUNT",			VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"43",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"44",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"45",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"46",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"47",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"48",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"49",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"50",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
//	{ERR_WOW_ERC,		"51",	"Declined\nContact Bank",		    "DECLINED\nCONTACT BANK",		"CONTACT BANK",			VIM_FALSE,			DISP_DECLINED, txn_line_bal_clr}, /*different for gift card*/
	{ERR_WOW_ERC,		"51",	"Declined\nContact Bank",		    "DECLINED\nCONTACT BANK",		"CONTACT BANK",			VIM_FALSE,			DISP_DECLINED, txn_line}, /*RDD 020113 SPIRA:6488 */
	{ERR_WOW_ERC,		"52",	"Declined\nNo Cheque Account",		    "DECLINED\nNO CHEQUE ACCOUNT",	"NO CHEQUE ACCOUNT",	VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"53",	"Declined\nNo Savings Account",		    "DECLINED\nNO SAVINGS ACCOUNT",	"NO SAVINGS ACCOUNT",	VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"54",	"Declined\nExpired Card",		    "DECLINED\nEXPIRED CARD",		"EXPIRED CARD",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"55",	"Declined\nIncorrect PIN",		    "DECLINED\nINCORRECT PIN",    "PIN ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line}, /*different for full emv*/
    {ERR_WOW_ERC,		"56",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"57",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"58",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"59",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"60",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"61",	"Declined\nOver Card Limit",		    "DECLINED\nOVER CARD LIMIT",	"OVER CARD LIMIT",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"62",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"63",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"64",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	// RDD 161214 added for Paypass3 - this error code now accociated with activity limit
	{ERR_WOW_ERC,		"65",	"Declined\nOver Card Limit",				"DECLINED\nOVER CARD LIMIT",	"OVER CARD LIMIT",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"66",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"67",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"68",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"69",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"70",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"71",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"72",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"73",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"74",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"75",	"Declined\nExceed PIN Tries",		    "DECLINED\nEXCEED PIN TRIES",	"EXCEED PIN TRIES",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"77",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"78",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"79",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"80",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"81",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"82",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"83",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"84",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"85",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"86",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"87",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"88",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"89",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"90",	"Declined\nSystem Error",		    "DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"91",	"Declined\nBank Unavailable",		    "DECLINED\nBANK UNAVAILABLE",	"NO BANK DO MANUAL",	VIM_FALSE,			DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"92",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	// RDD 100415 SPIRA:8583 - changed from 93 to 77 as 93 is already used in E-commerce
    {ERR_WOW_ERC,		"94",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
    {ERR_WOW_ERC,		"95",   "Declined\nSystem Error",         "DECLINED\nSYSTEM ERROR",     "SYSTEM ERROR",         VIM_FALSE,          DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"96",	"Declined\nRetry",			"DECLINED\nRETRY",		    "SYSTEM MALFUNCT'N",    VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"97",	"Settlement\nTotals Reset",		"SETTLEMENT\nTOTALS RESET",		    "TOTALS RESET",						VIM_FALSE,			DISP_DECLINED, txn_line}, /*wrong screen*/
	{ERR_WOW_ERC,		"98",	"Declined\nSystem Error",			"DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"99",	"Declined\nSystem Error",			"DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",			VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"Q3",	"Declined\nRefund Limit",			"DECLINED\nREFUND LIMIT",		"OVER REFUND LIMIT",	VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"Q5",	"Cancelled\nPower Fail",		"CANCELLED\nPOWER FAIL",		"POWER FAIL",			VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"Q6",	"Cancelled\nNo Manual Card",		"CANCELLED\nNO MANUAL CARD",   "NO MANUAL CARD",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"RI",	"Cancelled\nItems Not Allowed",		"CANCELLED\nITEMS NOT ALLOWED",   "ITEMS NOT ALLOWED",		VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"N0",	"Declined\nCPAT Sequence Error",			"DECLINED\nCPAT SEQUENCE ERROR",	"CPAT SEQUENCE ERROR",	VIM_FALSE,			DISP_CANCELLED, txn_line},
	{ERR_WOW_ERC,		"N1",	"",			        "",					"LOGON SUCCESSFUL",		VIM_FALSE,			DISP_LOGON_SUCCESSFUL, txn_line}, /*wrong screen*/
	{ERR_WOW_ERC,		"N2",	"Declined\nPinpad ID Error",			"DECLINED\nPINPAD ID ERROR",  "PINPAD ID ERROR",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"CU",	"Declined\nInvalid Currency",			"DECLINED\nINVALID CURRENCY",  "INVALID CURRENCY",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"N3",	"",										"",								"APPROVED",		VIM_FALSE,			DISP_LOGON_SUCCESSFUL, txn_line}, /*wrong screen*/
	{ERR_WOW_ERC,		"Y1",	"",					"",		            "",						VIM_FALSE,			DISP_TRANSACTION_APPROVED, txn_line}, /*mapped to 00 or 08 by pinpad*/
	{ERR_WOW_ERC,		"Y3",	"",					"",		            "",						VIM_FALSE,			DISP_CANCELLED, txn_line}, /*mapped to 00 or 08 by pinpad*/
	{ERR_WOW_ERC,		"Z1",	"Declined\nContact Issuer",			"DECLINED\nCONTACT ISSUER",   "CONTACT ISSUER",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"Z3",	"Declined\nContact Issuer",			"DECLINED\nCONTACT ISSUER",   "CONTACT ISSUER",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"Z4",	"Declined\nContact Issuer",			"DECLINED\nCONTACT ISSUER",   "CONTACT ISSUER",		VIM_FALSE,			DISP_DECLINED, txn_line},
	{ERR_WOW_ERC,		"XX",	"Declined\nSystem Error",			"DECLINED\nSYSTEM ERROR",		"SYSTEM ERROR",	        VIM_FALSE,			DISP_CANCELLED, txn_line}
	// RDD - put the old WBC terminal errors here until we replace them with the respecive WOW codes

};

VIM_ERROR_DEFINE("WQ",err_wow_erc);
VIM_ERROR_DEFINE("T2",err_acq_preauth_not_allowed);
VIM_ERROR_DEFINE("VD",err_co_not_allowed);
VIM_ERROR_DEFINE("VI",err_no_void_on_debit);
VIM_ERROR_DEFINE("T3",err_buffer_too_small);
VIM_ERROR_DEFINE("T4",err_card_expired);
VIM_ERROR_DEFINE("T5",err_comms_not_supported);
VIM_ERROR_DEFINE("T6",err_configuration_required);
VIM_ERROR_DEFINE("T7",err_connection_dropped);
VIM_ERROR_DEFINE("T8",err_cpat_rejected);
VIM_ERROR_DEFINE("T9",err_dial_busy);
VIM_ERROR_DEFINE("TA",err_emv_card_declined);
VIM_ERROR_DEFINE("TB",err_fallback_mode);
VIM_ERROR_DEFINE("TC",err_file_system_error);
VIM_ERROR_DEFINE("TD",err_gprs_network_error);
VIM_ERROR_DEFINE("WR",err_no_cashout);
VIM_ERROR_DEFINE("TF",err_host_erc);
VIM_ERROR_DEFINE("TG",err_idle);
VIM_ERROR_DEFINE("TH",err_invalid_account);
VIM_ERROR_DEFINE("TI",err_invalid_pin_length);
VIM_ERROR_DEFINE("TJ",err_invalid_proof);
VIM_ERROR_DEFINE("TK",err_invalid_tlv);
VIM_ERROR_DEFINE("TL",err_ip_connection_error);
VIM_ERROR_DEFINE("TM",err_keyed_not_allowed);
VIM_ERROR_DEFINE("TN",err_kvc_mismatch);
VIM_ERROR_DEFINE("TO",err_logon_successful);
VIM_ERROR_DEFINE("TP",err_luhn_check_failed);
VIM_ERROR_DEFINE("TQ",err_merch_blocked_card);
VIM_ERROR_DEFINE("TR",err_merch_rejected);
VIM_ERROR_DEFINE("TS",err_modem);
VIM_ERROR_DEFINE("TT",err_no_answer);
VIM_ERROR_DEFINE("TU",err_no_carrier);
VIM_ERROR_DEFINE("TV",err_no_cashout_allowed);
VIM_ERROR_DEFINE("TW",err_no_cpat_match);
VIM_ERROR_DEFINE("TX",err_no_dial_tone);
VIM_ERROR_DEFINE("TY",err_no_duplicate_receipt);
VIM_ERROR_DEFINE("TZ",err_no_duplicate_txn);
VIM_ERROR_DEFINE("UT", err_manual_pan_disabled);
VIM_ERROR_DEFINE("DG", err_pci_reboot_pending);
VIM_ERROR_DEFINE("U0",err_no_link);
VIM_ERROR_DEFINE("U1",err_no_sim_card);
VIM_ERROR_DEFINE("U2",err_not_logged_on);
VIM_ERROR_DEFINE("U3",err_out_of_range);
VIM_ERROR_DEFINE("U4",err_pdu);
VIM_ERROR_DEFINE("U5",err_power_fail);
VIM_ERROR_DEFINE("U6",err_preauth_disabled);
VIM_ERROR_DEFINE("U7",err_record_doesnt_exist);
VIM_ERROR_DEFINE("U8",err_refunds_disabled);
VIM_ERROR_DEFINE("U9",err_reversal_pending);
VIM_ERROR_DEFINE("UA",err_saf_pending);
VIM_ERROR_DEFINE("UB",err_secret_item_selected);
VIM_ERROR_DEFINE("UC",err_security_error);
VIM_ERROR_DEFINE("UD",err_signature_declined);
VIM_ERROR_DEFINE("UE",err_software_component_missing);
VIM_ERROR_DEFINE("UF",err_tag_not_found);
VIM_ERROR_DEFINE("UG",err_mcr_failed_blacklist);
VIM_ERROR_DEFINE("UH",err_txn_type_not_on_card);
VIM_ERROR_DEFINE("UI",err_unable_to_go_online_declined);
VIM_ERROR_DEFINE("UJ",err_user_back);
VIM_ERROR_DEFINE("UK",err_zero_pin);
VIM_ERROR_DEFINE("UL",msg_err_catid_mismatch);
VIM_ERROR_DEFINE("UM",msg_err_format_err);
VIM_ERROR_DEFINE("UN",msg_err_stan_mismatch);
VIM_ERROR_DEFINE("UO",err_password_mismatch);
VIM_ERROR_DEFINE("UP",err_settlement_reqd);
VIM_ERROR_DEFINE("UQ",err_offline_disabled);
VIM_ERROR_DEFINE("UR",err_void_disabled);
VIM_ERROR_DEFINE("US",err_adjustment_disabled);
VIM_ERROR_DEFINE("DG", err_pci_reboort_pending);
VIM_ERROR_DEFINE("UU",err_cashout_disabled);
VIM_ERROR_DEFINE("UV",err_invalid_tip_amount);
VIM_ERROR_DEFINE("UW",err_invalid_offline_amount);
VIM_ERROR_DEFINE("UX",err_invalid_total_amount);
VIM_ERROR_DEFINE("UY",err_invalid_track2);
VIM_ERROR_DEFINE("UZ",err_invalid_track1);
VIM_ERROR_DEFINE("V0",err_already_voided);
VIM_ERROR_DEFINE("V1",err_already_adjusted);
VIM_ERROR_DEFINE("V2",err_no_acquirer);
VIM_ERROR_DEFINE("V3",err_no_batch_totals);
VIM_ERROR_DEFINE("V4",err_batch_totals);
VIM_ERROR_DEFINE("V5",err_amex_acquirer_not_found);
VIM_ERROR_DEFINE("V6",err_moto_ecom_disabled);
VIM_ERROR_DEFINE("V7",err_bad_tables);
VIM_ERROR_DEFINE("V8",err_emv_tran);
VIM_ERROR_DEFINE("V9",err_no_roc_no_appr);
VIM_ERROR_DEFINE("VA",err_roc_completed);
VIM_ERROR_DEFINE("VB",err_roc_expired);
VIM_ERROR_DEFINE("VC",err_efb_not_allowed);
VIM_ERROR_DEFINE("VE",err_sale_with_tip_not_allowed);
VIM_ERROR_DEFINE("VF",err_tip_over_max);
VIM_ERROR_DEFINE("VG",err_cash_over_max);	// RDD 010814 ALH added
VIM_ERROR_DEFINE("WS",err_file_download);
VIM_ERROR_DEFINE("VH",err_no_retry_of_tip);
VIM_ERROR_DEFINE("WT",err_cannot_process_epan);
VIM_ERROR_DEFINE("VJ",err_mute_card);
VIM_ERROR_DEFINE("VK",err_card_removed);
VIM_ERROR_DEFINE("VL",err_aid_not_supported);
VIM_ERROR_DEFINE("VM",err_emv_config_error);
VIM_ERROR_DEFINE("VN",err_invalid_msg);
VIM_ERROR_DEFINE("VO",err_emv_pin_tries_exceeded);
VIM_ERROR_DEFINE("VP",err_invalid_product);
VIM_ERROR_DEFINE("VQ",err_emv_fallback_not_allowed);
VIM_ERROR_DEFINE("VR",err_track2_mismatch);
VIM_ERROR_DEFINE("VS",err_over_refund_limit_d);
VIM_ERROR_DEFINE("GY", err_over_refund_limit_t);
VIM_ERROR_DEFINE("VT",err_icc_read_error);
VIM_ERROR_DEFINE("VU",err_set_manager_pw_for_refund);
VIM_ERROR_DEFINE("VV",err_transmission_failure);
VIM_ERROR_DEFINE("VW",err_pos_not_supported);
VIM_ERROR_DEFINE("VX",err_pos_protocol_error);
VIM_ERROR_DEFINE("VY",err_pos_rsp_invalid);
VIM_ERROR_DEFINE("VZ",err_pos_comms_error);
VIM_ERROR_DEFINE("W0",err_host_recv_timeout);
VIM_ERROR_DEFINE("W1",err_pos_req_invalid);
VIM_ERROR_DEFINE("W2",err_no_host_phone_number);
VIM_ERROR_DEFINE("W3",err_no_eft_server);
VIM_ERROR_DEFINE("W4",err_no_link_setup_eft_server);
VIM_ERROR_DEFINE("W5",err_no_phone_line);
VIM_ERROR_DEFINE("W6",err_interrupted_by_pos);
VIM_ERROR_DEFINE("W7",err_pos_get_status_ok);
VIM_ERROR_DEFINE("W8",err_no_last_settlement);
VIM_ERROR_DEFINE("W9",err_card_not_accepted);
VIM_ERROR_DEFINE("WA",err_no_response);
VIM_ERROR_DEFINE("WB",err_mac_error);
VIM_ERROR_DEFINE("WC",err_cnp_error);
VIM_ERROR_DEFINE("WD",err_txn_not_supported);
VIM_ERROR_DEFINE("WE",err_batch_full);
VIM_ERROR_DEFINE("WF",err_emv_card_approved);
VIM_ERROR_DEFINE("WG",err_unable_to_go_online_approved);
VIM_ERROR_DEFINE("WH",err_host_approved_card_declined);
VIM_ERROR_DEFINE("WI",err_not_tms_on);
VIM_ERROR_DEFINE("WJ",err_invalid_roc);
VIM_ERROR_DEFINE("WK",err_invalid_auth_no);
VIM_ERROR_DEFINE("WL",err_invalid_amount);
VIM_ERROR_DEFINE("WM",err_no_keys_loaded);
VIM_ERROR_DEFINE("WN",err_signature_approved_offline);
VIM_ERROR_DEFINE("WO",err_terminal_locked);
VIM_ERROR_DEFINE("WP",err_cnp);
VIM_ERROR_DEFINE("WU",err_preswipe_incomplete);
VIM_ERROR_DEFINE("WV",err_preswipe_complete);
VIM_ERROR_DEFINE("WW",err_card_rejected_no_bal);
VIM_ERROR_DEFINE("WX",err_wow_comms_error);
VIM_ERROR_DEFINE("WY",err_settle_successful);
VIM_ERROR_DEFINE("WZ",err_presettle_successful);
VIM_ERROR_DEFINE("X0",err_logon_failed);
VIM_ERROR_DEFINE("X1",err_card_expired_date);
VIM_ERROR_DEFINE("X2",err_kvc_error);
VIM_ERROR_DEFINE("X3",err_pos_get_status_approved);
VIM_ERROR_DEFINE("X4",err_onecard_successful);
VIM_ERROR_DEFINE("X5",err_loyalty_mismatch);
VIM_ERROR_DEFINE("X6",err_cpat_no_dock);
VIM_ERROR_DEFINE("X7",err_pp_busy);
VIM_ERROR_DEFINE("X8",err_loyalty_card_successful);
VIM_ERROR_DEFINE("X9", err_qr_recv_successful);
VIM_ERROR_DEFINE("GS", err_qc_auth_failure);
VIM_ERROR_DEFINE("GR", err_rtm_auth_failure);
VIM_ERROR_DEFINE("G6", err_rtm_auth_failure2);
VIM_ERROR_DEFINE("GZ", err_invalid_tender);


error_t ipc_error_data;


VIM_ERROR_PTR ascii_to_vim_error(char const *asc_error)
{
    VIM_UINT32 i;

    for( i = 0; i < ARRAY_LENGTH(terminal_errors) - 1; i++ )
    {
        if( !vim_strncmp( terminal_errors[i].ascii_code, asc_error, 2 ))
        {
            return terminal_errors[i].error_code;
        }
    }
    return VIM_ERROR_SYSTEM;
}



// RDD - Does a search on the RC data
VIM_ERROR_PTR host_error_lookup(char const *error, error_t *error_data)
{
  VIM_UINT32 i;

  *error_data = host_errors[0];

  for( i=0; i < ARRAY_LENGTH(host_errors); i++ )
  {
    if (vim_mem_compare(host_errors[i].ascii_code, error, VIM_SIZEOF(host_errors[i].ascii_code)) == VIM_ERROR_NONE)
    {
      *error_data = host_errors[i];
	  if( !vim_strncmp( host_errors[i].ascii_code, "DX", 2 ))
	  {
		  vim_mem_copy( &ipc_error_data, &host_errors[i], VIM_SIZEOF(error_t) );
		  vim_sprintf( ipc_error_data.terminal_line1, "Product %2.2d\nNot Allowed", txn.u_PadData.PadData.InvalidProductID );
		  vim_sprintf( ipc_error_data.terminal_line2, "DECLINED\nPROD %2.2d NOT ALLOWED", txn.u_PadData.PadData.InvalidProductID );
		  vim_sprintf( ipc_error_data.pos_text, "PROD %2.2d NOT ALLOWED", txn.u_PadData.PadData.InvalidProductID );
		  *error_data = ipc_error_data;
	  }
	  return VIM_ERROR_NONE;
    }
  }

  // RDD 150212: If there was an unknown host response then copy it over the XX so we see it
  if( vim_strlen( error ) )
  {
	  vim_strcpy( error_data->ascii_code, error );
	  return VIM_ERROR_NONE;
  }
  /*no error found, return failure and we drop back to terminal error lookup.*/
  return VIM_ERROR_NOT_FOUND;
}



// RDD - Does a search on the error code
VIM_ERROR_PTR terminal_error_lookup( VIM_ERROR_PTR error, error_t *error_data, VIM_BOOL report_error )
{
  VIM_UINT32 i;
  //VIM_CHAR buffer[150];
  VIM_DBG_POINT;
  VIM_DBG_ERROR( error );

  //RDD - point the error data ptr to the default "SYSTEM ERROR" in case the code can't be matched
  *error_data = terminal_errors[0];

  for( i=0; i < ARRAY_LENGTH(terminal_errors)-1; i++ )
  {
	  if( terminal_errors[i].error_code == error )
	  {
		*error_data = terminal_errors[i];
		return VIM_ERROR_NONE;
	  }
  }
  {
	  // RDD 190315 - this can cause crashes so just return VIM_ERROR_SYSTEM
#if 0
	  // RDD 230513 SPIRA:6742 Make Less MO SYTEM ERRORS by copying text from actual VIM_ERROR_XXX ( just the XXX part)
	  static error_t unlisted_vim_error;
	  vim_mem_copy( &unlisted_vim_error, error_data, VIM_SIZEOF( error_t ));
	  if( error->name != VIM_NULL )
		vim_sprintf( unlisted_vim_error.pos_text, "%.20s", &error->name[10] );
	  *error_data = unlisted_vim_error;
#endif
  }
  // Error not found so report it
#if 0
  if( report_error )
	pceft_debug_error(pceftpos_handle.instance, "System Error:", error );
#endif
  VIM_ERROR_RETURN( VIM_ERROR_NOT_FOUND );
}

// RDD - does a search on the RC data and then on the error code (if no match found for RC)
VIM_ERROR_PTR error_code_lookup(VIM_ERROR_PTR error, VIM_CHAR const* asc_error, error_t *error_data)
{
	// RDD 270412 - ensure that this is clear first.
	vim_mem_clear( error_data, VIM_SIZEOF(error_t) );

	if(( error == VIM_ERROR_NONE ) || ( error == ERR_WOW_ERC ))
	{
		if(( host_error_lookup( asc_error, error_data )) == VIM_ERROR_NONE )
		{
			return VIM_ERROR_NONE;
		}
	}
	return( terminal_error_lookup( error, error_data, VIM_TRUE ) );
}

