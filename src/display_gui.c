/**
 * \file display.c
 *
 * \brief
 * Handles all terminal displays
 *
 * \details
 * Displays the terminal and POS screens for various states of the transaction
 *
 * \author
 * Eftsolutions Australia
 */


#include "incs.h"

VIM_BOOL use_vim_screen_functions = VIM_FALSE;
static VIM_BOOL pos_displayed;
extern VIM_CHAR *GetLogoFile(VIM_UINT32 CardIndex);

// RDD - txn type is needed to build the top line of the displey in most of the stages
extern VIM_CHAR type[20];

const key_map_t yes_no_keys[] =
{
    { "Yes", VIM_KBD_CODE_OK },
{ "No", VIM_KBD_CODE_CLR },
{ "ButtonPrev", VIM_KBD_CODE_RFUNC0 },
{ "ButtonNext", VIM_KBD_CODE_RFUNC1 },
};

const key_map_t cheque_savings_credit_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
{ "Button2", VIM_KBD_CODE_KEY2 },
{ "Button3", VIM_KBD_CODE_KEY3 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t cheque_savings_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
{ "Button2", VIM_KBD_CODE_KEY2 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t cheque_credit_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
{ "Button2", VIM_KBD_CODE_KEY3 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t savings_credit_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY2 },
{ "Button2", VIM_KBD_CODE_KEY3 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t sale_cash_other_keys[] =
{
    { "Button1", VIM_KBD_CODE_SOFT1 },
{ "Button2", VIM_KBD_CODE_SOFT2 },
{ "Button3", VIM_KBD_CODE_SOFT3 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t sale_other_keys[] =
{
    { "Button1", VIM_KBD_CODE_SOFT1 },
{ "Button2", VIM_KBD_CODE_SOFT3 },
{ "", VIM_KBD_CODE_NONE }
};

const key_map_t app_select_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
{ "Button2", VIM_KBD_CODE_KEY2 },
{ "Button3", VIM_KBD_CODE_KEY3 },
{ "ButtonPrev", VIM_KBD_CODE_HASH },
{ "ButtonNext", VIM_KBD_CODE_ASTERISK },
{ "", VIM_KBD_CODE_NONE }
};


const key_map_t cheque_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
{ "", VIM_KBD_CODE_NONE }
};
const key_map_t qr_confirm_keys[] =
{
    { "Button1", VIM_KBD_CODE_CANCEL },
};
// RDD 230819 JIRA WWBP-684 Use function key when not connected to register
const key_map_t function_key[] =
{ {"", VIM_KBD_CODE_HASH } };


const screen_t screens_adk[] =
{
    /***** Adam K - WOW Screens according to terminal screen library spec v1.0 *****/
    /* Designed to use .ui screen labels. Formatting is defined in each individual label in the .ui file, so each FIELD esc seq is used to point to a different label. */


    {
        DISP_IMAGES,
        VIM_NULL,
        FIELD(Image)"%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "images.html"
    },
    {
        DISP_EDP_REMOVE,
        VIM_NULL,
        FIELD(Image)"%s"
        FIELD(Button1)"Remove Everyday Pay",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "QR_connected.html",
        qr_confirm_keys
    },
    {
        DISP_IMAGES_AND_QR,
        VIM_NULL,
        FIELD(Image)"%s" \
        FIELD(szQrData)"%s" \
        FIELD(leftPixelQR)"%s" \
        FIELD(topPixelQR)"%s" \
        FIELD(BrandingImage)"%s"\
        FIELD(leftPixelBrand)"%s"\
        FIELD(topPixelBrand)"%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "images_QR.html"
    },

    {
        DISP_IMAGES_AND_OVERLAY,
        VIM_NULL,
        FIELD(Image)"%s" \
        FIELD(OverlayImage)"%s"\
        FIELD(leftPixelOverlay)"%s"\
        FIELD(topPixelOverlay)"%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "images_Overlay.html"
    },

    {
        DISP_BLANK,
        VIM_NULL,
        "%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING
    },

    {
        DISP_IDLE_PCEFT,
        VIM_NULL,
        FIELD(Label) "%s"\
        FIELD(Status)"%s",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		IDLE_PCEFT_ADK,
        function_key
    },

    {
        DISP_IDLE,
        VIM_NULL,
        FIELD(Label) "%s\n",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        IDLESTATUS_ADK
    },


    {
        DISP_SWIPE_OR_INSERT,
        "SWIPE OR\nINSERT CARD",
        FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
        FIELD(AppName)" "FIELD(AccountType)" "\
        FIELD(Label)"Swipe or\nInsert Card",
        VIM_PCEFTPOS_GRAPHIC_CARD,
        "Transaction.html"
    },

    {
        DISP_PROCESSING_WAIT,
        "PROCESSING\n"\
        "PLEASE WAIT",
        FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
        FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
        FIELD(Label)"%s\nPlease Wait",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "Transaction.html"
    },

	// Standalone only
    {
        DISP_CONNECTING_WAIT,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
        FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
        FIELD(Label)"%s\nPlease Wait",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "Transaction.html"
    },

	{
		DISP_SENDING_WAIT,
		VIM_NULL,
		//"SENDING\n"
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Sending\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.html"
	},

	{
		DISP_RECEIVING_WAIT,
		VIM_NULL,
		//"RECEIVING\n"
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Receiving\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.html"
	},

	{
		DISP_DISCONNECTING_WAIT,
		VIM_NULL,
		//"DISCONNECTING\n"
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Disconnecting\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.html"
	},

    {
        DISP_SELECT_ACCOUNT,
        "SELECT ACCOUNT\n"\
        "CHQ, SAV OR CR",
    FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
    FIELD(AppName)"%s"\
    FIELD(Label)"Select Account\n"
    FIELD(Button1)" Cheque or Press 1"\
    FIELD(Button2)" Savings or Press 2"\
    FIELD(Button3)" Credit or Press 3",
    VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
    "Acc3ButtonRed.html",
    cheque_savings_credit_keys

    },

    {
        DISP_SELECT_ACCOUNT_CHQ_SAV,
        "SELECT ACCOUNT\n"\
        "CHQ OR SAV",
    FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
    FIELD(AppName)"%s"\
    FIELD(Label)"Select Account\n"\
    FIELD(Button1)" Cheque or Press 1"\
    FIELD(Button2)" Savings or Press 2",
    VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
    "Acc2Button.html",
    cheque_savings_keys

    },
      {
          DISP_SWITCH_TO_INTEGRATED_MODE,
          VIM_NULL,
          FIELD(Label)"\n"\
          "SWITCH TO\n"\
    "INTEGRATED MODE?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
      },
  {
      DISP_SWITCH_TO_STANDALONE_MODE,
      VIM_NULL,
      FIELD(Label)"\n"\
      "SWITCH TO\n"\
    "STANDALONE MODE?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_INTERNAL_PRINTER,
      VIM_NULL,
      FIELD(Label)"\n"\
      "INTERNAL PRINTER?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_WINDOWS_PRINTER,
      VIM_NULL,
      FIELD(Label)"\n"\
      "USE WINDOWS\n"\
    "DEFAULT PRINTER?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_INTERNAL_MODEM,
      VIM_NULL,
      FIELD(Label)"\n"\
      "INTERNAL MODEM?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_JOURNAL_TXN,
      VIM_NULL,
      FIELD(Label)"\n"\
      "JOURNAL TXNS?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_PRINT_SECOND_RECEIPT,
      VIM_NULL,
      FIELD(Label)"\n"\
      "PRINT SECOND\n"\
    "RECEIPT?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },
  {
      DISP_STANDALONE_SWIPE_START_TXN,
      VIM_NULL,
      FIELD(Label)"\n"\
      "SWIPE START\n"\
    "TRANSACTIONS?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
  },


    {
        DISP_QUERY_CARD_SELECT_ACCOUNT,
        "SELECT ACCOUNT\n"\
        "CHQ, SAV OR CR",
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(AppName)"%s"\
    FIELD(Button1)"Cheque"\
    FIELD(Button2)"Savings"\
    FIELD(Button3)"Credit"\
    FIELD(Label)"Select Account\n",
    VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
    "Menu3Button.html",
    cheque_savings_credit_keys,

    /* Traditional text-based display */
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(AppName)"%s"\
    FIELD(Label)"  SELECT ACCOUNT    \n"
    FONT_SMALL"                      \n"
    FONT_SMALL"                      \n"
    FONT_SMALL"                      \n"
    FONT_SMALL " Cheque Savings Credit\n"
    },
    {
        DISP_REMOVE_CARD_A,
        VIM_NULL,
        FIELD(Label)"",
        VIM_PCEFTPOS_GRAPHIC_REMOVE,
        "images.html"
    },

    {
        DISP_REMOVE_CARD_B,
        "REMOVE CARD",		// RDD 050312: SPIRA IN4852 - Keep POS in txn mode by sending remove card message repeatedly
        FIELD(Label)"REMOVE CARD",
        VIM_PCEFTPOS_GRAPHIC_REMOVE,
        "A_Default.html"
    },


{
    DISP_KEY_PIN_OR_ENTER,
    "KEY PIN OR ENTER",
    FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
    FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
    FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PIN,
    "PinEntry.html"
},

{
    DISP_TRANSACTION_APPROVED,
    "APPROVED",
    FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
    FIELD(AppName)"%s" FIELD(AccountType)"%s"\
    FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_FINISHED,
    "Transaction.html"
},

{
    DISP_TRANSACTION_APPROVED_BAL_CLR,
    VIM_NULL,
    FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
    FIELD(Label)"Approved"\
    FIELD(Bal) "BAL: " FIELD(BalAmount)"%s\n"\
    FIELD(Clr) "CLR: "FIELD(ClrAmount)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Transaction.html"
},

    {
        DISP_TRANSACTION_DECLINED_BAL_CLR,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(Label)"\n%s\n"\
        FIELD(Bal) "" FIELD(BalAmount)"\n"\
        FIELD(Clr) "CLR: "FIELD(ClrAmount)"%s\n",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "Transaction.html"
    },

    {
        DISP_APPROVED_WITH_SIG,
        "SIGNATURE OK?",
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(AppName)"%s"FIELD(AccountType)"%s"\
        FIELD(Label)"VERIFY SIGNATURE\n"\
        "CORRECT?\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
        "Transaction.html"
    },

    {
        DISP_DECLINED,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(AppName)"%s"FIELD(AccountType)"%s"\
        FIELD(Label)"%s",
        VIM_PCEFTPOS_GRAPHIC_FINISHED,
        "Transaction.html"
    },

    {
        DISP_CARD_NUMBER,
        "ENTER CARD NUMBER\nON PINPAD",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"Card Number:\n",
        VIM_PCEFTPOS_GRAPHIC_DATA,
        "EntryPoint.html"
    },

    {
        DISP_EXPIRY,
        "ENTER EXPIRY <MMYY>\nON PINPAD",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"Expiry <MMYY>\n",
        VIM_PCEFTPOS_GRAPHIC_DATA,
        "EntryPoint.html"
    },

    {
        DISP_SETTLE_DATE,
        "KEY SETTLE DATE\nON PINPAD <DDMM>",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"Key Date (DDMM) or\n [Enter] For Last",
        VIM_PCEFTPOS_GRAPHIC_DATA,
        "EntryPoint.html"
    },

    {
        DISP_QUERY_EXPIRY,
        "ENTER EXPIRY <MMYY>\nON PINPAD",
        FIELD(TransactionType)" " FIELD(Amount)" "\
        FIELD(Label)"Expiry <MMYY>\n",
        VIM_PCEFTPOS_GRAPHIC_DATA,
        "EntryPoint.html"
    },

    {
        DISP_CONTACTLESS_PAYMENT,
        "PRESENT CARD",
        FIELD(CTLS_status)"%s\n"\
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"%s",
        VIM_PCEFTPOS_GRAPHIC_ALL,
        "ContactlessPurchaseScreen.html"
    },

    {
        DISP_CONTACTLESS_TEST,
        "TAP CARD",
        FIELD(CTLS_status)"%s\n"\
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"Present Card",
        VIM_PCEFTPOS_GRAPHIC_ALL,
        "ContactlessPurchaseScreen.html"
    },


    {
        DISP_GENERIC_ENTRY_POINT,
        "%s",
        FIELD(Label) "%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "EntryPoint.html"
    },


    {
        DISP_KEY_PIN,
        "KEY PIN",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(AppName)"%s" FIELD(AccountType)"%s"\
        FIELD(Label)"%s\n",
        VIM_PCEFTPOS_GRAPHIC_PIN,
        "PinEntry.html"
    },

    {
        DISP_WAIT,
        "ALLOW MANUAL\nKEY ENTRY",
        //FIELD(Image)"%s"
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(AppName)" "FIELD(AccountType)" "\
        FIELD(Label)"Please Wait",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
        "Transaction.html"
    },

    {
        DISP_KEY_DATA,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(Label)"%s",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "EntryPoint.html"
    },

    {
        DISP_SELECT_OPTION,
        VIM_NULL,
        FIELD(TransactionType)" " FIELD(Amount)" "\
        FIELD(AppName)" "\
        FIELD(Button1)"Sale Amount"\
        FIELD(Button2)"Sale Amount + Cash out"\
        FIELD(Button3)"Other Amount"\
        FIELD(Label)"%s %s\n\n"\
        "Select Option:\n",
        VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
        "Menu3Button.html",
        sale_cash_other_keys,

        /* Traditional text-based display */
        FIELD(TransactionType)" " FIELD(Amount)" "\
        " "\
        "%s %s\n"\
        "     SELECT OPTION:    \n"
        FONT_SMALL"  Sale    Sale    Other \n"
        FONT_SMALL" Amount  Amount  Amount \n"
        FONT_SMALL"           +            \n"
        FONT_SMALL"        Cash out        \n",
    },

    {
        DISP_AMOUNT_ENTRY,
        VIM_NULL,
        FIELD(TransactionType)" " FIELD(Amount)" "\
        FIELD(AppName)"%s"FIELD(AccountType)"%s"\
        FIELD(Label)"%s\n",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "EntryPointA.html",
        0,
        /* Traditional text-based display */
        "\n"\
        "%s AMOUNT?\n"\
        RIGHT_ALIGN""ENTRY_POINT"\n"
    },


    {
        DISP_END_PRESWIPE,
        VIM_NULL,
        FIELD(Label) FONT_LARGE"\n"\
        FONT_LARGE"Your Selection\n"\
            FONT_LARGE"Has Been\n"
            FONT_LARGE"Accepted\n",
            VIM_PCEFTPOS_GRAPHIC_PROCESSING,
            "A_Default.html"
   },

    {
        DISP_CARD_READ_SUCCESSFUL,
        VIM_NULL,
        FIELD(Label)"Deposit\nCard Read\nSuccessful",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "A_Default.html"
    },

    {
        DISP_SYSTEM_ERROR,
        VIM_NULL,
        FIELD(Label)"%s\n"\
        "System Error\n",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "A_Default.html"
    },

    {
        DISP_WAITING_FOR_CONNECTION,
        VIM_NULL,
        "Connect PinPad\nTo Register",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING
    },

    {
        DISP_LOGON_SUCCESSFUL,
        "WOW\nLOGON SUCCESSFUL",
        "Logon Successful",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "A_Default.html"
    },

    {
        DISP_SETTLEMENT_APPROVED,
        "%s",
        //FIELD(Image)"%s"
        FIELD(TransactionType)" " FIELD(Amount) " "\
        FIELD(AppName)" "FIELD(AccountType)" "\
        FIELD(Label)"%s",
        VIM_PCEFTPOS_GRAPHIC_FINISHED,
            "Transaction.html"
    },
    {
        DISP_CARD_REJECTED,
        VIM_NULL,
        //FIELD(Image)"%s"
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(AppName)"%s"FIELD(AccountType)"%s"\
        FIELD(Label)"%s",
        VIM_PCEFTPOS_GRAPHIC_CARD,
        "Transaction.html"
    },
    {
        DISP_CANCELLED,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
            FIELD(AppName)"%s"FIELD(AccountType)"%s"\
            FIELD(Label)"%s",
            VIM_PCEFTPOS_GRAPHIC_FINISHED,
            "Transaction.html"
    },
    {
        DISP_CARD_MUST_BE_INSERTED,
        "CARD MUST BE\nINSERTED",
        FIELD(TransactionType)"%s\n"  FIELD(Amount)"%s\n"\
        FIELD(AppName)" "FIELD(AccountType)" "\
        FIELD(Label)"Card Must Be\nInserted"\
        " ",
        VIM_PCEFTPOS_GRAPHIC_INSERT,
        "Transaction.html"
    },

    {
        DISP_PS_CARD_MUST_BE_INSERTED,
        VIM_NULL,
        FIELD(Label)"Card Must Be\nInserted",
        VIM_PCEFTPOS_GRAPHIC_INSERT,
        "A_Default.html"
    },

    {
        DISP_UNABLE_TO_PROCESS_CARD,
        "CARD REJECTED\nINVALID CARD NO",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(AppName)" "FIELD(AccountType)" "\
        FIELD(Label)"Card Rejected\nInvalid Card No"\
        " ",
        VIM_PCEFTPOS_GRAPHIC_FINISHED,
        "Transaction.html"
    },

    {
        DISP_APPROVED_TXN_ONLY,
        "APPROVED",
        "%s\n"\
        "Approved",
        VIM_PCEFTPOS_GRAPHIC_FINISHED,
        "A_Default.html"
    },

  {
        DISP_FUNCTION,
        "ENTER PASSWORD\nON PINPAD",
        FIELD(TransactionType)" " FIELD(Amount) " "\
        FIELD(Label) "%s\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
        "EntryPoint.html"
  },

  {
      DISP_ENTER_SAF_LIMIT,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount) " "\
      FIELD(Label) "Enter SAF Limit:\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.html"
  },

  {
      DISP_SAF_FUNCTIONS,
      VIM_NULL,
      FIELD(Label) "SAF Control\n"
      "Functions",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_DIAGNOSTIC_FUNCTIONS,
      VIM_NULL,
      FIELD(Label) "Diagnostic\n"
      "Functions",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

    // RDD 040714 added
  {
      DISP_DIAG_RECORD,
      VIM_NULL,
      FIELD(Label) "%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },




  {
      DISP_MAINTENANCE_FUNCTIONS,
      VIM_NULL,
      FIELD(Label) "Terminal\n"
      "Maintenance",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_EFTPOS_VERSION,
      VIM_NULL,
      FIELD(Label)"%s\nApplication %06d  \nContactless DRV \n%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_TERMINAL_ID,
      VIM_NULL,
      FIELD(Label)"Terminal ID\n"
      "%8.8s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_FILLING_SAF,
      VIM_NULL,
      FIELD(Label)"Writing %3.3s\n"\
      "Txns to SAF",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_MERCHANT_ID,
      VIM_NULL,
      FIELD(Label)"Merchant ID\n"
      "%15.15s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PPID,
      VIM_NULL,
      FIELD(Label)"PPID\n"
      "%16.16s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_SAF_TRANS,
      VIM_NULL,
      FIELD(Label)"Stored TXNS:\n"
      "%s\n%s\n%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_DELETING_FILES,
      VIM_NULL,
      "Deleted All\n"\
      "Config Files",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_ENTER_MERCHANT_PASSWORD,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label) "Enter Merchant\n"
    "Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_CARD_READ_STATISTICS,
      VIM_NULL,
      "Swipe Card\n"
    "Good Read   %3d\n"
    "Bad Read    %3d\n"
    "Total Read  %3d\n",
    VIM_PCEFTPOS_GRAPHIC_CARD
  },

  {
      DISP_POS_FME,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"POS-FME\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.html"
  },

  {
      DISP_NII,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"NII\n"\
    "%d",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.html"
  },

  {
      DISP_PROTECTED_FUNCTIONS,
      VIM_NULL,
      FIELD(Label)"Protected\n"
      "Functions",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PRINTING_PLEASE_WAIT,
      VIM_NULL,
      FIELD(Label)"\n"\
      "Printing\n"\
    "Please Wait",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_CONFIRM_APPLICATION,
      "PRESS ENTER\n"\
      "ON THE PINPAD",
    FIELD(Label)"CONFIRM\n"\
    "\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_PASSWORD_INVALID,
      VIM_NULL,
      FIELD(Label)"Password Invalid\n"\
      "Retry Password?\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_INVALID_DATE,
      "REJECTED\nINVALID DATE",
      FIELD(Label)"Rejected\nInvalid Date\n"\
      "%2.2s/%2.2s\n",
    VIM_PCEFTPOS_GRAPHIC_FINISHED
  },

  {
      DISP_RESET_MEMORY,
      VIM_NULL,
      FIELD(Label)"Reset Memory?\n"\
      "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_CONNECTING,
      VIM_NULL,
      FIELD(Label)"Connecting to\n"\
      "%s\n"\
    "Please Wait",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_CARD_READ_ERROR,
      "CARD READ FAILED",
      FIELD(Label)"Card Read Failed\n%s",
      VIM_PCEFTPOS_GRAPHIC_FINISHED
  },

  {
      DISP_BAD_CHIP_READ_REMOVE_CARD,
      "CARD READ FAILED\nREMOVE CARD",
      FIELD(Label)"Card Read Failed\nRemove Card",
      VIM_PCEFTPOS_GRAPHIC_REMOVE
  },

  {
      DISP_INIT_PLEASE_WAIT,
      "WOW\n"\
      "INITIALISING",
    FIELD(Label)"Initialising\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_INVALID_PIN_N_TRIES_REMAINING,
      VIM_NULL,
      FIELD(Label)"%s\n"
      "Invalid PIN\n"
    "%d Tries Left\n"
    "\n"
    PROMPT_CANCEL_RETRY,
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PRINTER_ERROR,
      "PRINTER ERROR\nPRINTER OFFLINE",
      FIELD(Label)"Printer Error\n"\
      "Printer Offline\n"\
    "\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PRINTER_OUT_OF_PAPER,
      "PRINTER ERROR\nOUT OF PAPER",
      FIELD(Label)"Printer Error\n"\
      "Out of Paper\n"\
    "\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_INVALID_ACCOUNT,
      VIM_NULL,
      FIELD(Label)"Invalid Account\n"\
      "Selected"\
    "\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_SENDING_REVERSAL,
      "PROCESSING\nPLEASE WAIT",
      FIELD(Label)"Sending\nReversal",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PROCESSING_TERMINAL_ONLY,
      VIM_NULL,
      FIELD(Label)"\nProcessing\n"\
      "Please Wait",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_CONFIRM_PAN,
      VIM_NULL,
      FIELD(Label)"Confirm PAN?\n"\
      "%s\n"\
    "%s\n\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_ENTER_AMOUNT,
      VIM_NULL,
      FIELD(TransactionType) "%s" FIELD(Amount) "%s"\
      FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPointAN.html"
  },
 
  {
      DISP_ACTIVATION_AMT,
      VIM_NULL,
      FIELD(TransactionType) "%s" FIELD(Amount) "%s"\
      FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
    DISP_TERMINAL_CONFIG_TERMINAL_ID,
    VIM_NULL,
    FIELD(TransactionType) "" FIELD(Amount) ""\
    FIELD(Label)"Enter Terminal ID:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPointAN.html"
  },

  {
      DISP_IDLE_LOCKED,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"Locked\n"\
    "Enter Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
    DISP_CHANGE_TIME,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"Enter Time HHMM:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_CHANGE_DATE,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"Date:\n"\
    "DD/MM/YYYY\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_PW_CHANGE_MENU,
      VIM_NULL,
      FIELD(Label)"Password Change:\n"\
      "1-TRAN\n"\
    "2-RFND\n"\
    "3-VOID\n"\
    "4-SETL \n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_PW_VERIFY_OLD,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"%s Type Password\n"\
    "Enter Old Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_PW_ENTER_NEW,
      VIM_NULL,
      FIELD(TransactionType) " " FIELD(Amount) " "\
      FIELD(Label)"%s Type Password\n"\
    "Enter New Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_TXN_ACCEPTED,
      VIM_NULL,
      FIELD(Label)"Transaction\n"\
      "Accepted\n",
    VIM_PCEFTPOS_GRAPHIC_FINISHED
  },

  {
      DISP_GET_APPROVAL_CODE,
      "ENTER AUTH NUMBER:",
      FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
      FIELD(Label)"Enter Auth No",
      VIM_PCEFTPOS_GRAPHIC_DATA,
      "EntryPoint.html"
  },

  {
      DISP_CONFIRM_YESORNO,
      VIM_NULL,
      FIELD(Label)"%s\n"\
      "%s\n"\
      PROMPT_YES_NO,
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_CONFIRM_OKORCANCEL,
      VIM_NULL,
      FIELD(Label)"%s\n"\
      "%s\n"\
    PROMPT_OK_CANCEL,
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_PIN_PRESS_ENTER,
      "PRESS ENTER\nTO CONFIRM",
      FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
    FIELD(Label)"Press Enter\nTo Confirm\n",
    VIM_PCEFTPOS_GRAPHIC_PIN,
    "Transaction.html"
  },

  {
      DISP_TERMINAL_CONFIG_MERCHANT_ID,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label) "Merchant ID\n"\
    "%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPointAN.html"
  },

  {
      DISP_TERMINAL_CONFIG_TRANSSEND_ID,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"TRAN$END ID:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_QUERY_CARD_NUMBER,
      "ENTER CARD NUMBER\nON PINPAD",
      FIELD(TransactionType)"%s" FIELD(Amount)"%s"\
      FIELD(Label)"Card Number:\n",
    VIM_PCEFTPOS_GRAPHIC_CARD,
    "EntryPoint.html"
  },

  {
      DISP_SHIFT_TOTALS_PRINTING,
      "SHIFT TOTALS\nPRINTING",
      FIELD(Label)"Shift Totals\n"\
      "PRINTING",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_PROCESSING,
      "PROCESSING\nPLEASE WAIT",
      FIELD(Label)"%s\n"\
      "Please Wait\n"\
    "%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },


  {
      DISP_EMV_DEBUG,
      VIM_NULL,
      FIELD(Label)"%s EMV Debug?\n"\
      "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_TMS_CONFIG,
      VIM_NULL,
      FIELD(Label)"\n\n"\
      "RTM Config\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_DISABLE_TMS,
      VIM_NULL,
      FIELD(Label)"Disable RTM?\n"\
      "[ENTER]= YES\n"\
    "[CLEAR]=  NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "YesNo.html",
    yes_no_keys
  },

  {
      DISP_TMS_PHONE,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"TMS Phone No:\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_TMS_SHA,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"TMS SHA:\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_TMS_NII,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"TMS NII:\n"\
    "%d",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

  {
      DISP_TMS_LINE_SPEED,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"TMS Line Speed:"\
    "[1] = 1200 BPS\n"\
    "[2] = 2400 BPS\n"\
    "[3] = 33600 BPS\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

    {
        DISP_SAF_UPLOADING,
        "PROCESSING\nPLEASE WAIT", // RDD 090312: SPIRA 5057: NO POS DISPLAY FOR UPLOADING ADVICES
        "Sending Advice\n"\
        "%s %s\n",
          VIM_PCEFTPOS_GRAPHIC_PROCESSING,
          "Transaction.html"
    },

  {
      DISP_USE_MAG_STRIPE,
      "SWIPE CARD",
      FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
      FIELD(AppName)" "FIELD(AccountType)" "\
    FIELD(Label)"Swipe Card",
    VIM_PCEFTPOS_GRAPHIC_CARD,
    "Transaction.html"
  },

  {
      DISP_I3070_USB_RS232,
      VIM_NULL,
      FIELD(Label)"%s\n"\
      "%s\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_CLEAR_REVERSAL,
      VIM_NULL,
      FIELD(Label)"Clear Reversal?\n"\
      "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_REVERSAL_APPROVED,
      VIM_NULL,
      FIELD(Label)"Reversal Approved\n"\
      "And Cleared",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_REVERSAL_DECLINED,
      VIM_NULL,
      FIELD(Label)"Reversal Not\n"
      "Uploaded",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_MEMORY_RESET,
      VIM_NULL,
      FIELD(Label)"Memory Reset",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_SCREEN_NEW_ENTER_MERCHANT_PASSWORD,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(Label)"New Merchant\n"
    "Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

    {
        DISP_CUSTOMER_COPY_CONFIRM,
        VIM_NULL,
        "\nCUSTOMER COPY?\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION
    },


  {
      DISP_SCREEN_VERIFY_ENTER_MERCHANT_PASSWORD,
      VIM_NULL,
      FIELD(Label)"Verify Merchant\n"
      "Password",
    VIM_PCEFTPOS_GRAPHIC_VERIFY
  },

  {
      DISP_SCREEN_MERCHANT_PASSWORD_CHANGED,
      VIM_NULL,
      FIELD(Label)"Merchant Password\n"
      "Changed",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_CLEAR_SAF,
      VIM_NULL,
      FIELD(Label)"Clear%s SAF\n"\
      "Transactions?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_CLEAR_CONFIG,
      VIM_NULL,
      FIELD(Label)"Delete Config\n"\
      "Files?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },


{
    DISP_SAF_APPROVED,
    VIM_NULL,
    FIELD(Label)"SAF Transactions\n"\
    "Cleared",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
},

  {
      DISP_SAF_DECLINED,
      VIM_NULL,
      FIELD(Label)"SAF Transactions\n"\
      "Not Uploaded",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_SELECT_APPLICATION,
      "SELECT APPLICATION\n ON PINPAD",
      FIELD(Label)"",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_RESET_ROC,
      VIM_NULL,
      FIELD(Label)"Reset ROC?\n"\
      "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_CONTACTLESS_NOT_READY,
      "CONTACTLESS READER\nUNAVAILABLE",
      FIELD(Label)"Contactless Reader\nUnavailable",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_HOST_ID,
      VIM_NULL,
      FIELD(Label)"Current Host ID:\n"\
      "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_DELETE_REVERSAL,
      VIM_NULL,
      FIELD(Label)"Delete Reversal?\n"\
      "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "YesNo.html",
    yes_no_keys
  },

  {
      DISP_REVERSAL_DECLINED_AND_CLEARED,
      VIM_NULL,
      "Reversal Cleared",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
         DISP_PLEASE_INSERT_CARD,
        "PLEASE INSERT CARD",
         FIELD(TransactionType) "%s" FIELD(Amount) "%s"\
        FIELD(AppName) " " FIELD(AccountType) " "\
        FIELD(Label)"Please Insert Card",
        VIM_PCEFTPOS_GRAPHIC_CARD,
        "Transaction.html"
  },

  {
      DISP_INITISALISE_191,
      "INITIALISE 191",
      FIELD(Label)"\nInitialise 191",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_INITISALISE_192,
      "INITIALISE 192",
      FIELD(Label)"\nInitialise 192",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_INITISALISE_193,
      "INITIALISE 193",
      FIELD(Label)"\nInitialise 193",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_LOADING_CPAT,
      "LOADING CPAT\nBLOCK %3.3d",
      "\nLoading CPAT\nBlock %3.3d",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_LOADING_EPAT,
      "LOADING EPAT\nBLOCK %3.3d",
      "\nLoading EPAT\nBlock %3.3d",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_LOADING_PKT,
      "LOADING PKT\nBLOCK %3.3d",
      "\nLoading PKT\nBlock %3.3d",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },
  {
      DISP_LOADING_FCAT,
      "LOADING FCAT\nBLOCK %3.3d",
      "\nLoading FCAT\nBlock %3.3d",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_PS_ENTER_AMOUNT,
      VIM_NULL,
      FIELD(TransactionType)" " FIELD(Amount)" "\
      FIELD(AppName)" "\
    FIELD(Button1)"Sale Amount"\
    FIELD(Button2)"Other Amount"\
    FIELD(Label)"%s %s\n\n"\
    "Select Option:\n",
    VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
    "Menu2Button.html",
    sale_other_keys
  },

  {
      DISP_TOKEN_LOOKUP_RESULT,
      VIM_NULL,
      FIELD(Label)"Card:\n"\
      "%s\n"\
    "Card Token:\n"\
    "%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"A_Default.html",
  },

  {
      DISP_TOKEN_LOOKUP_PROCESSING,
      "PROCESSING\nPLEASE WAIT",
      FIELD(Label)"Token Lookup\n\n"\
      "Processing\n\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_APP_SELECT_2,
      "SELECT APPLICATION\nON PINPAD",
      FIELD(TransactionType) " " FIELD(Amount) " "
      FIELD(AppName) " "
      FIELD(Label) "%s"\
      FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu2Button.html",
    cheque_savings_keys
  },

    // VN JIRA WWBP-319 Invalid keys pressed in Application selection results in 'Operator Cancel'
  {
      DISP_APP_SELECT_3,
      "SELECT APPLICATION\nON PINPAD",
      FIELD(TransactionType) " " FIELD(Amount) " "
      FIELD(AppName) " "
      FIELD(Label) "%s"\
      FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s"\
    FIELD(Button3) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
          "Menu3ButtonRed.html",
          app_select_keys
  },
  {
      DISP_APP_SELECT_1,
      "SELECT APPLICATION\nON PINPAD",
      //FIELD(TransactionType) " " FIELD(Amount) " "
      //FIELD(AppName) " "
      FIELD(Label) "%s"\
      FIELD(Button1) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu1Button.html",
    cheque_keys
  },

    //first %s is the number, second is the appname
  {
      DISP_APP_SELECT,
      "SELECT APPLICATION\nON PINPAD",
      FIELD(Label) "%s"\
      FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s"\
    FIELD(Button3) "%s %s"\
    FIELD(ButtonPrev) "Prev"\
    FIELD(ButtonNext) "Next",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "ExtendedMenu.html",
    app_select_keys
  },

  {
      DISP_CTLS_INIT,
      "CTLS READER INIT\nPLEASE WAIT",
      FIELD(Label) "CTLS Reader Init\nPlease Wait\n%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_TMS_GENERAL_SCREEN,
      "%s\n%s",
      FIELD(Label) "%s\n%s"\
      FIELD(Status)"",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    IDLE_FILENAME_ADK
  },
  {
      DISP_PIN_OK,
      "PIN OK\nVERIFIED OFFLINE",
      FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label)"PIN OK\n",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "Transaction.html"
  },
  {
      DISP_INVALID_PIN_RETRY,
      "INCORRECT PIN\nRETRY",   // RDD - send offline Pin Retry notification to POS
      FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label)"Incorrect PIN\nRetry",
      VIM_PCEFTPOS_GRAPHIC_PIN,
      "Transaction.html"
  },

    // 240818 RDD JIRA WWBP-118 TCPIP PCEFT Client setup
    {
        DISP_TCP_HOSTNAME,
        VIM_NULL,
        FIELD(TransactionType) "PcEFTPOS Client IP"\
        FIELD(Label) "Current Client IP:\n%s\nEdit IP Address\nor press ENTER",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "EntryPointIP.html"
    },

    {
        DISP_WIFI_SSID,
        VIM_NULL,
        FIELD(TransactionType) "WiFi SSID"\
        FIELD(Label) "Key New SSID\nor press [ENTER]",
          VIM_PCEFTPOS_GRAPHIC_PROCESSING,
          "EntryPointAN.html"
    },

    {
        DISP_WIFI_PASSWORD,
        VIM_NULL,
        FIELD(TransactionType) "WiFi Password"\
        FIELD(Label) "Key New Password\nor press [ENTER]",
          VIM_PCEFTPOS_GRAPHIC_PROCESSING,
          "EntryPointAN.html"
    },

    {
        DISPLAY_SCREEN_PCEFT_PROTOCOL,
        VIM_NULL,
        "PCEFTPOS PROTOCOL\n"\
        LEFT_ALIGN"1 STX/VLI/CRC\n"\
        LEFT_ALIGN"2 STX/ETX/LRC\n"\
        LEFT_ALIGN"3 TCP\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
    },


  {
      DISP_TCP_PORT,
      VIM_NULL,
      FIELD(TransactionType) "TCP PORT"\
      FIELD(Label) "CURRENT PORT\n%s\n\nNEW PORT?",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },

      
  // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
  {
      DISP_BCD_ENTRY,
      VIM_NULL,//"KEY BARCODE\nON PINPAD
      FIELD(TransactionType)"" FIELD(Amount) ""\
      FIELD(Label) "Enter Barcode\nNumber",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "EntryPoint.html"
  },


  {
      DISP_PCD_ENTRY,
      VIM_NULL,
      FIELD(TransactionType)"%s" FIELD(Amount) "%s"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label) "Enter Passcode\nNumber",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.html"
  },


  {
      DISP_TECHNICAL_FALLBACK,
      DISP_STR_TECH_FALLBACK ,
      FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
      FIELD(AppName)" " FIELD(AccountType) " "\
      FIELD(Label)"Please Wait For\nAssistance",
      VIM_PCEFTPOS_GRAPHIC_QUESTION,
     "Transaction.html"
  },
  {
      DISP_PRINT_BALANCE,
      "REFER CUSTOMER TO\nPINPAD TO PRINT RCPT",
      FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
      FIELD(AppName)" " FIELD(AccountType) " "\
      FIELD(Label)"Balance Enquiry\nPrint Receipt?\nEnter=YES CLR=NO",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "Transaction.html"
  },
  {
      DISP_GET_ANSWER,
      VIM_NULL,
      FIELD(Label)"%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_PRESS_ENTER_TO_CONFIRM,
      "PRESS ENTER\nON THE PINPAD",
      FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label)"Press Enter\nTo Confirm\n",
      VIM_PCEFTPOS_GRAPHIC_PIN,
      "Transaction.html"
  },

  {
      DISP_BUSINESS_DIV_MENU,
      VIM_NULL,
      FIELD(Label)"Choose Business\nDivision:\n"\
      LEFT_ALIGN"%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

    {
        DISP_PASSWORD,
        VIM_NULL,
        FIELD(TransactionType)"%s" FIELD(Amount) "%s"\
        FIELD(Label) "Enter Password:\n",
    //		FIELD(Label) "Enter Function\n",
        VIM_PCEFTPOS_GRAPHIC_QUESTION,
         "EntryPoint.html"
    },


  {
      DISP_ENTER_PASSWORD,
      "%s\nPASSWORD",
      "\n"\
      "\n"\
    "PASSWORD\n"\
    ""RIGHT_ALIGN""ENTRY_POINT"\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
      DISP_SELECTION_APP_BLOCKED,
      "SELECTION\nBLOCKED",
      FIELD(Label)"Selection\nBlocked",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_CARD_MUST_BE_SWIPED,
      "CARD\nMUST BE SWIPED",
      FIELD(Label)"Card\nMust Be Swiped",
      VIM_PCEFTPOS_GRAPHIC_CARD,
      "A_Default.html"
  },

  {
      DISP_CARD_ERROR_EXPIRED_DATE,
      "EXP. DATE ERROR\nCONTACT ISSUER",
      FIELD(Label)"Exp. Date Error\nContact Issuer",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_PIN_TRY_LIMIT_EXCEEDED,
      "PIN TRY LIMIT\nEXCEEDED",
      FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
      FIELD(AppName)" " FIELD(AccountType)" "\
      FIELD(Label)"PIN Try Limit\nExceeded",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "Transaction.html"
  },

  {
      DISP_TXN_NOT_ACCEPTED,
      "TRANSACTION\nNOT ACCEPTED",
      FIELD(Label)"Transaction\nNot Accepted",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "A_Default.html"
  },

  {
      DISP_WOW_PROCESSING,
      "%s\nWOW PROCESSING",
      FIELD(Label)"%s\nProcessing",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_CARD_DATA_ERROR,
      "CARD REJECTED\nCARD DATA ERROR",
      FIELD(Label)"Card Rejected\nCard Data Error\n%s",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "A_Default.html"
  },

  {
      DISP_CANCELLED_PENDING,
      VIM_NULL,
      FIELD(Label)"Cancelled\n%s",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "A_Default.html"
  },

    {
        DISP_GENERIC_00,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC0_SS
    },

    {
        DISP_GENERIC_01,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC1_SS
    },
    {
        DISP_GENERIC_02,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC2_SS
    },
    {
        DISP_GENERIC_03,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC3_SS
    },
    {
        DISP_GENERIC_04,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC4_SS
    },
    {
        DISP_GENERIC_05,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC5_SS
    },
    {
        DISP_GENERIC_06,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC6_SS
    },
    {
        DISP_GENERIC_07,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC7_SS
    },
    {
        DISP_GENERIC_08,
        VIM_NULL,
        "",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        GENERIC8_SS
    },


  {
      DISP_USE_DIFFERENT_CARD,      
      "PLEASE USE ANOTHER CARD",
      FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
      FIELD(AppName)" " FIELD(AccountType)" \n"\
      FIELD(Label)"Please Retry With\nAnother Card",
      VIM_PCEFTPOS_GRAPHIC_FINISHED,
      "Transaction.html"
  },


  {
      DISP_TMS_CONFIRMATION,
      VIM_NULL,
      FIELD(Label)"DO TMS LOGON?\n"\
      "ENTER=YES, CLR=NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_REBOOT_CONFIRMATION,
      VIM_NULL,
      FIELD(Label)"REBOOT PINPAD?\n"\
      "ENTER=YES, CLR=NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

    {
        DISP_WINZ_CARD_ACCEPTED,
        "WINZ CARD ACCEPTED",
        FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
        FIELD(AppName)" " FIELD(AccountType)" "\
        FIELD(Label)"WINZ Card\nAccepted\n",
        VIM_PCEFTPOS_GRAPHIC_FINISHED,
        "Transaction.html"
    },

  {
      DISP_NO_REPEATS_FOUND,
      VIM_NULL,
      FIELD(Label)"NO REPEAT\nSAF FOUND",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "A_Default.html"
  },

  {
      DISP_PRESENT_ONE_CARD,
      "SWIPE OR INSERT\nONE CARD",
      FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
      FIELD(AppName)" "FIELD(AccountType)" "\
      FIELD(Label)"Swipe or Insert\nOnecard",
      VIM_PCEFTPOS_GRAPHIC_CARD,
      "Transaction.html"
  },

  {
      DISP_GET_ROC,
      VIM_NULL,
      "PRE-AUTH RRN?",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_ROC_NUMBER_ENQUIRY,
      VIM_NULL,
      "PRE-AUTH RRN?",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },



  {
      DISP_GET_APPROVAL_CODE_STANDALONE,
      VIM_NULL,
      "\n"\
      "ENTER AUTH NO\n"\
    "\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },


    {
        DISP_CCV_DATA_ENTRY,
        "ENTER CVV\nON TERMINAL",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"
        FIELD(Label)"Enter CVV:\n",
        VIM_PCEFTPOS_GRAPHIC_PIN,
        "EntryPoint.html"
    },

    {
        DISP_CCV_OPT_DATA_ENTRY,
        "CVV OR ENTER\nON TERMINAL",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"
        FIELD(Label)"Key CVV\nor Press Enter",
        VIM_PCEFTPOS_GRAPHIC_PIN,
        "EntryPoint.html"
    },


      {
          DISP_INVALID_CCV,
          "INVALID CVV",
          "\n"\
          "INVALID CVV\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
      },


  {
      DISP_GET_ROC_STANDALONE,
      VIM_NULL,
    FIELD(TransactionType)"COMPLETION"\
    FIELD(Label)"PRE-AUTH RRN?\n",
    VIM_PCEFTPOS_GRAPHIC_ALL,
    "EntryPoint.html"
  },


    {
        DISP_CONTACTLESS_ONECARD,
        "PRESENT ONE CARD",
        FIELD(CTLS_status)"%s\n"\
        FIELD(Label)"Present or swipe\nyour Onecard now",
    VIM_PCEFTPOS_GRAPHIC_ALL,
    "OneCard.html"
    },

    {
        DISP_CONTACTLESS_DISCOUNT_CARD,
        "PRESENT DISCOUNT\nCARD",
        FIELD(CTLS_status)\
        FIELD(Label)"Present or swipe\nLoyalty card now",
    VIM_PCEFTPOS_GRAPHIC_ALL,
    "OneCard.html"
    },

    {
        DISP_PASS_WALLET,
        "Select and Present\nWOW Phone Pass",
        FIELD(CTLS_status)\
        FIELD(Label)"Select and Present\nWOW Phone Pass",
    VIM_PCEFTPOS_GRAPHIC_ALL,
    "OneCard.html"
    },

    {
        DISP_SELECT_PASS,
        "Please Select\nWOW Phone Pass",
        FIELD(CTLS_status)\
        FIELD(Label)"Please Select\nWOW Phone Pass",
    VIM_PCEFTPOS_GRAPHIC_ALL,
    "OneCard.html"
    },


    {
        DISP_CONTACTLESS_DISABLED,
        "ALL CTLS AIDs\nDISABLED IN EPAT",
        FIELD(Label)"All CTLS AIDs\nDISABLED IN EPAT",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING
    },

    {
        DISP_INIT101_PLEASE_WAIT,
        "WOW INIT 101\n"\
        "PLEASE WAIT",
    FIELD(Label)"\nInit 101\n"\
    "Please Wait",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
    },

  {
      DISP_REBOOT2,
      "REMOTE REBOOT\nIN TWO SECONDS",
      FIELD(Label)"REMOTE REBOOT\nIN TWO SECONDS",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_REBOOT1,
      "REMOTE REBOOT\nIN ONE SECOND",
      FIELD(Label)"REMOTE REBOOT\nIN ONE SECONDS",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_RESET_STATS,
      "RESET STATS FILE",
      FIELD(Label)"Reset Stats File",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_DEBUG_STATS,
      "OUTPUT STATS FILE",
      FIELD(Label)"Output Stats File",
      VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

    {
        DISP_ENTER_EGC,
        "ENTER GIFT CARD\n NUMBER ON PINPAD",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
        FIELD(Label)"Gift Card Number:\n",
    VIM_PCEFTPOS_GRAPHIC_PIN,
    "EntryPoint.html"
    },

    {
        DISP_VERSIONS,
        VIM_NULL,
        FIELD(Label)"SOFTWARE\nVERSIONS:\n\n\nOS:%s\nEOS:%s\nCTLS:%s\nEFT APP:%d",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING
    },

   {
        DISP_ENTER_EGC_SW,
        "KEY eGIFTCARD NUMBER",
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Key Giftcard Number",
      VIM_PCEFTPOS_GRAPHIC_PIN,
      "EntryPoint.html"
   },


      {
      DISP_ENTER_EGC_SW_NO_ENTRY,
        "SWIPE GIFTCARD OR\nKEY eGIFTCARD NUMBER",
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Swipe Giftcard or\nKey Giftcard Number",
      VIM_PCEFTPOS_GRAPHIC_PIN,
      "Transaction.html"
      },

          {
      DISP_AVALON_ALL,
      "Scan/Swipe or Enter\nGiftCard number",    // RDD 021220 as detailed in POS Spec v1.4 021220
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Swipe Card or Enter\neGiftCard number",
      VIM_PCEFTPOS_GRAPHIC_DATA,
      "Transaction.html"
          },

          {
      DISP_GEN_TXN,
      VIM_NULL,
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label)"%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "Transaction.html"
          },



  {
      DISP_SCANNING_BATCH_PLEASE_WAIT,
      "SCANNING BATCH\nPLEASE WAIT",
      FIELD(Label)"\n"\
      "SCANNING BATCH\n"\
    "PLEASE WAIT\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "A_Default.html"
  },

    {
        DISP_VERIFY_AUTH_ENQUIRY,
        VIM_NULL,
        FIELD(Label)"%s\n"\
        "VERIFY AUTH\n"\
    "CORRECT?\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "A_Default.html"
    },

{
    DISP_YES_NO,
    VIM_NULL,
    FIELD(Label) "%s"\
    FIELD(LeftNavText) "No"\
    FIELD(RightNavText) "Yes",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "YesNo.html",
    yes_no_keys,
},



      {
          DISP_TERMINAL_CONFIG_PABX_ACCESS_CODE,
          VIM_NULL,
          "\nPABX NO:\n",
          VIM_PCEFTPOS_GRAPHIC_QUESTION,
          "EntryPoint.html"
      },

  {
      DISP_TERMINAL_CONFIG_TMS_TELEPHONE_NO,
      VIM_NULL,
      "\nHOST PHONE NO:\n",
      VIM_PCEFTPOS_GRAPHIC_QUESTION,
      "EntryPoint.html"
  },

  {
      DISP_TMS_PACKET_LEN,
      VIM_NULL,
      "\nTMS MAX BLOCK LEN:\n",
      VIM_PCEFTPOS_GRAPHIC_QUESTION,
      "EntryPoint.html"
  },

      {
          DISP_TEST_PACKET_LEN,
          VIM_NULL,
          "\nADD DE60 BYTES:\n",
          VIM_PCEFTPOS_GRAPHIC_QUESTION,
          "EntryPoint.html"
      },


  {
      DISP_TERMINAL_CONFIG_DIAL,
      VIM_NULL,
      "\n"\
      LEFT_ALIGN"DIAL TYPE:\n"\
    LEFT_ALIGN"%c\n"\
    LEFT_ALIGN"[1]=TONE\n"\
    LEFT_ALIGN"[2]=PULSE\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_TERMINAL_CONFIG_BLIND,
      VIM_NULL,
      "\n"\
      LEFT_ALIGN"BLIND DIAL:\n"\
    LEFT_ALIGN"%s\n"\
    LEFT_ALIGN"[1]=ON\n"\
    LEFT_ALIGN"[2]=OFF\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },

  {
      DISP_TERMINAL_CONFIG_QUICK,
      VIM_NULL,
      "\n"\
      LEFT_ALIGN"QUICK DIAL:\n"\
    LEFT_ALIGN"%s\n"\
    LEFT_ALIGN"[1]=ON\n"\
    LEFT_ALIGN"[2]=OFF\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },


    {
        DISP_TERMINAL_CONFIG_HEADER_TYPE,
        VIM_NULL,
        "\n"\
        LEFT_ALIGN"HEADER_TYPE: [%c]\n"\
    LEFT_ALIGN"[0]=NO HEADER\n"\
    LEFT_ALIGN"[1]=CLNP\n"\
    LEFT_ALIGN"[2]=TPDU\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
    },

    {
        DISPLAY_SCREEN_SHUTTING_DOWN,
        VIM_NULL,
        "SHUTTING DOWN\n%s\n\n"\
        "PRESS ANY KEY\n"\
    "TO CANCEL\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    },

};



const screen_t *screens = VIM_NULL;


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_initialize(void)
{
	VIM_DBG_MESSAGE("display_initialize");
    VIM_ERROR_RETURN_ON_FAIL(vim_lcd_open());
    use_vim_screen_functions = VIM_TRUE;
    VIM_DBG_MESSAGE("New display with ADK");
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR FileAuthError(VIM_CHAR *FileName)
{
	VIM_CHAR ErrText[100];
	VIM_GRX_VECTOR offset = { 0,0 };
	vim_sprintf(ErrText, "%s Invalid", FileName);
	pceft_debug(pceftpos_handle.instance, ErrText);
	vim_lcd_put_text_at_cell(offset, ErrText);
	terminal_clear_status(ss_tms_configured);
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR find_display_index(VIM_UINT32 id, VIM_UINT32 *display_index)
{
    VIM_UINT32 i, num_screens;
    num_screens = VIM_LENGTH_OF(screens_adk);
    for (i = 0; i < num_screens; i++)
    {
        if (screens[i].screen_id == id)
            break;
    }
    if (i >= num_screens)
    {
        VIM_DBG_MESSAGE("CANT FIND SCREEN");
        VIM_DBG_VAR(id);
        return VIM_ERROR_SYSTEM;
    }
    *display_index = i;
    return VIM_ERROR_NONE;
}



VIM_ERROR_PTR display_screen_pos_display( VIM_UINT32 screens_index, VIM_PCEFTPOS_KEYMASK pos_keymask, VIM_VA_LIST ap )
{
    VIM_CHAR pos_text[100], pos_line_1[21], pos_line_2[21];
    VIM_SIZE pos_length, line_1_length, line_2_length;

    //DBG_POINT;
    if (IS_STANDALONE)
    {
        VIM_ERROR_RETURN(VIM_ERROR_NONE);
    }
    vim_mem_clear( pos_text, VIM_SIZEOF( pos_text ) );
    vim_mem_clear( pos_line_1, VIM_SIZEOF(pos_line_1) );
    vim_mem_clear( pos_line_2, VIM_SIZEOF(pos_line_2) );

   // VIM_DBG_NUMBER(screens_index);
   // VIM_DBG_VAR(pos_displayed);

    if((screens_adk[screens_index].pceft_pos_text != VIM_NULL) && (get_operation_initiator() != it_pinpad) && (!pos_displayed ))
    {
        vim_vsnprintf(pos_text, VIM_SIZEOF(pos_text), screens_adk[screens_index].pceft_pos_text, ap);

        vim_mem_clear(pos_line_1, VIM_SIZEOF(pos_line_1));
        vim_mem_clear(pos_line_2, VIM_SIZEOF(pos_line_2));

        // RDD - if there is a newline in the string then split the string into the two lines
        line_1_length = vim_strcspn(pos_text, "\n\0");
        pos_length = vim_strlen(pos_text);

        if (pos_length > line_1_length)
            line_2_length = pos_length - line_1_length - 1;
        else
            line_2_length = 0;

        vim_mem_copy(pos_line_1, pos_text, VIM_MIN(line_1_length, 20));
        vim_mem_copy(pos_line_2, &pos_text[line_1_length + 1], MIN(line_2_length, 20));

        // RDD 080212 - Ensure that only POS initiated txns send data to the POS

        if ((vim_strlen(pos_line_1) || vim_strlen(pos_line_2)) && (terminal.initator == it_pceftpos))
            //if(( vim_strlen(pos_line_1) || vim_strlen(pos_line_2) ))
        {
            VIM_ERROR_PTR res;

            // RDD added 171011 for PCEFT Cancel Key Support
            VIM_DBG_STRING(pos_line_1);
            VIM_DBG_STRING(pos_line_2);
            if ((res = pceft_pos_display(pos_line_1, pos_line_2, pos_keymask, screens_adk[screens_index].graphic)) != VIM_ERROR_NONE)
            {
                DBG_MESSAGE("****** PcEft Display Error ******");
                // RDD 151013 SPIRA 6957: NZ v418 Pilot issue - PinPad - PcEft Client COMMS died due to Offline Pin Issue
                // RDD 251013 - appears to crash the PinPad if cancel key pressed
                //res = pceft_settings_reconfigure( VIM_TRUE );
            }
            VIM_ERROR_RETURN_ON_FAIL(res);
        }
    }

    // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
    else if (pos_keymask)
    {
        VIM_DBG_MESSAGE("Keymask POS display1");
        VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, pos_keymask));
        //last_keymask_set = keymask;
    }


    return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/



/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR adkgui_display_terminal(const VIM_TEXT *text, const VIM_TEXT* fileName, const key_map_t* key_mappings)
{
    //VIM_ERROR_RETURN_ON_FAIL(vim_lcd_set_backlight(100));

    // prevent screen saver from activating for
    // screen_saver_start_delay_secs seconds
    //vim_rtc_get_uptime(&terminal.last_screen_saver_idle_time_ms);
    //terminal.last_screen_saver_idle_time_ms += eft_fleet_tms_params.screen_saver_start_delay_secs * 1000;
    /* Set the screen saver status as to be initialized */
    //VIM_ERROR_RETURN_ON_FAIL(set_screen_saver_status(SCREEN_SAVER_STATUS_YET_TO_START));
    //VIM_ERROR_RETURN_ON_FAIL(vim_screen_create_screen(screen_ui_script, no_key_mappings));

    return vim_adkgui_display_terminal("Label", text, fileName, key_mappings);
}



VIM_BOOL is_approved( VIM_CHAR const *rc )
{
	// RDD 230916 v548-0 SPIRA:8931 Txns which return Balance were displaying "Approved" if txn was approved online but print failed
	if(( txn.error == ERR_WOW_ERC ) || ( txn.error == VIM_ERROR_NONE ))
	{
		if( !vim_strncmp( rc, "00", 2 )) return VIM_TRUE;
		if( !vim_strncmp( rc, "08", 2 )) return VIM_TRUE;
	}
	return VIM_FALSE;
}



VIM_ERROR_PTR  display_result
(
  VIM_ERROR_PTR error,
  VIM_CHAR const *asc_error,
  VIM_CHAR const *line1,
  VIM_CHAR const *line2,
  VIM_PCEFTPOS_KEYMASK pos_key_type,    
  VIM_SIZE MinDisplayTimeMs
)
{

  VIM_CHAR pos_line_1[21], pos_line_2[21], line_1_length, line_2_length, pos_length;
  error_t error_data;

  VIM_PCEFTPOS_GRAPHIC pos_display_graphic = VIM_PCEFTPOS_GRAPHIC_FINISHED;

  VIM_CHAR CardName[MAX_APPLICATION_LABEL_LEN+1];
  VIM_CHAR account_string_short[3+1];

  // QR-code payment txns display their own approved / declined screen; block the normal one.
  if ( TERM_QR_WALLET && IsEverydayPay(&txn) )
  {
      // This has fixed 3 sec display time.
      return Wally_DisplayResult();
  }


  VIM_DBG_PRINTF1("----------- display_result %d mS--------------", MinDisplayTimeMs );

  get_acc_type_string_short(txn.account, account_string_short);
  vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );
  GetCardName( &txn, CardName );
  DBG_STRING( CardName );

  error_code_lookup(error, asc_error, &error_data);

  VIM_DBG_ERROR(error);

  pos_displayed = VIM_FALSE;

  VIM_DBG_STRING(error_data.terminal_line1);
  VIM_DBG_STRING(error_data.terminal_line2);

  /*Split the pos string into 2 lines*/
  vim_mem_clear(pos_line_1, VIM_SIZEOF(pos_line_1));
  vim_mem_clear(pos_line_2, VIM_SIZEOF(pos_line_2));


  line_1_length = vim_strcspn(error_data.terminal_line2, "\n\0");
  pos_length = vim_strlen(error_data.terminal_line2);


  if (pos_length > line_1_length)
	line_2_length = pos_length - line_1_length - 1;
  else
	line_2_length = 0;

  vim_mem_copy(pos_line_1, error_data.terminal_line2, VIM_MIN(line_1_length, 20));
  vim_mem_copy(pos_line_2, &error_data.terminal_line2[line_1_length+1], MIN(line_2_length, 20));
  //VIM_DBG_STRING(pos_line_1);
  //VIM_DBG_STRING(pos_line_2);

  switch(error_data.params)
  {
		case no_data:
			// Deal with the pos display first
			if(vim_strlen(error_data.terminal_line2) )
			{
				// RDD added 171011 for PCEFT Cancel Key Support
				VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(pos_line_1, pos_line_2, pos_key_type, pos_display_graphic ));
				pos_displayed = VIM_TRUE;
			}
			// RDD - intentional drop through to PinPad screen display only
		case no_pos_display:
			display_screen(error_data.screen_id, pos_key_type, error_data.terminal_line1, pos_key_type, pos_display_graphic );
			pos_displayed = VIM_TRUE;
			break;

		case txn_type:
			display_screen(error_data.screen_id, pos_key_type, "LOGON", error_data.terminal_line1); /*placeholder for now, need to sort out screens for these*/
			pos_displayed = VIM_FALSE;
			break;

		default:
		case display_line:

		// RDD 100512 SPIRA:5442 Need specific file related data to be sent to display
		if( vim_strlen( line1 ) ) vim_strcpy( error_data.terminal_line1, line1 );
		// intentional drop through...


		case txn_line:
			{
				if(( CardIndex(&txn) == WINZ_CARD ) && (is_approved(asc_error) ) && ( txn.type == tt_sale ))
				{
					display_screen( DISP_WINZ_CARD_ACCEPTED, pos_key_type, type, AmountString(txn.amount_total) );
				    pos_displayed = VIM_TRUE;
					break;
				}

				//VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(error_data.terminal_line1, error_data.terminal_line2, pos_key_type, pos_display_graphic ));

				// RDD 260713 SPIRA:6782 - Below change is wrong. We don't want to ever display the balance lines unless the balance is >= 0
				if((( txn.LedgerBalance >= 0 )||( txn.ClearedBalance >= 0 ))&&(is_approved(asc_error)))
				{
					VIM_CHAR AscLedgerBal[16], AscClearedBal[16];
					if( txn.type == tt_balance )
					{
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display( "REFER CUSTOMER TO", "PINPAD TO OBTAIN BAL", pos_key_type, pos_display_graphic ));
						pos_displayed = VIM_TRUE;
					}
					else
					{
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display( "APPROVED", "", VIM_PCEFTPOS_KEY_NONE, pos_display_graphic ));
						pos_displayed = VIM_TRUE;
					}
					vim_strcpy( AscLedgerBal, AmountString1(txn.LedgerBalance) );  //BD Display zero balance
					vim_strcpy( AscClearedBal, AmountString1(txn.ClearedBalance) ); //BD Display zero balance
					display_screen( DISP_TRANSACTION_APPROVED_BAL_CLR, pos_key_type, type, AmountString(txn.amount_total), AscLedgerBal, AscClearedBal );
					VIM_ERROR_IGNORE(vim_event_sleep(5000)); // wait 5 secs - ( RDD  220113 - allow abort but don't generate an error at this stage )
				}
				else
				{
					if(( txn.type == tt_logon ) || ( txn.type == tt_settle ) || ( txn.type == tt_presettle ) || ( txn.type == tt_query_card ))
						vim_strcpy( type, " " );
					if(( error_data.screen_id == DISP_CANCELLED )||( error_data.screen_id == DISP_CARD_REJECTED )||( error_data.screen_id == DISP_DECLINED ))
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(pos_line_1, pos_line_2, pos_key_type, pos_display_graphic ));
                    DBG_STRING(CardName);
                    {
                        VIM_CHAR Display[100] = { 0 };
						if ((txn.amount_surcharge) && (is_approved_txn(&txn)))
                            vim_sprintf(Display, "%s\nInc Surcharge %s", error_data.terminal_line1, AmountString(txn.amount_surcharge));
                        else
                            vim_strcpy(Display, error_data.terminal_line1);

                        VIM_ERROR_RETURN_ON_FAIL(display_screen(error_data.screen_id, pos_key_type, type, AmountString(txn.amount_total), CardName, account_string_short, Display ));
                    }
				}
			}
			break;
  }

#if 0
  // RDD 090212 - Fix so that we have more control over the min POS display time if a keymask was set
  if( MinDisplayTimeMs )
  {
	  if(( pos_key_type != VIM_PCEFTPOS_KEY_NONE ) && ( is_integrated_mode() ))
	  {
		  VIM_PCEFTPOS_KEYMASK key;
		  pceft_pos_wait_key(&key, MinDisplayTimeMs);

		  if( IS_INTEGRATED() )
			  VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, 0));
	  }
	  else
	  {
		  VIM_ERROR_IGNORE( vim_event_sleep(MinDisplayTimeMs));
	  }
  }
#endif

  /* return without error */
  // RDD 110914 v433 moved to after receipt printing

  // RDD 110215 v541 - this needs to be moved to after receipt printing
  if( MinDisplayTimeMs )
  {
	  vim_kbd_sound_invalid_key ();
	  VIM_ERROR_IGNORE( vim_event_sleep(MinDisplayTimeMs));
  }

  VIM_DBG_PRINTF1("----------- end display_result %d mS--------------", MinDisplayTimeMs);
  return VIM_ERROR_NONE;
}


VIM_BOOL IsIdleDisplay( VIM_CHAR *UIFileName )
{
	VIM_SIZE len = vim_strlen( IDLE_FILENAME );
	if( !vim_strncmp( UIFileName, IDLE_FILENAME, len ) )
		return VIM_TRUE;

	len = vim_strlen(IDLE_FILENAME_ADK);
	if (!vim_strncmp(UIFileName, IDLE_FILENAME_ADK, len))
		return VIM_TRUE;

	return VIM_FALSE;
}





// RDD 110215 v421
static VIM_SIZE LastScreen;
//static VIM_TEXT *last_image_filename;


VIM_ERROR_PTR display_screen_cust_pcx
(
  VIM_UINT32 screen_id,
  VIM_TEXT *controlName,
  VIM_TEXT *image_file_name,
  VIM_PCEFTPOS_KEYMASK pos_keymask,
    ...
)
{
  const VIM_CHAR *terminal_text;
  //VIM_CHAR term_text[256];
  VIM_UINT32 screens_index;
  VIM_BOOL ErrorDisplay = VIM_FALSE;
  VIM_SIZE PosLen = 0;
  VIM_VA_LIST ap;

  //VIM_DBG_STRING("display_screen_cust_pcx");
  //VIM_DBG_MESSAGE("----------- display_screen_cust_pcx --------------");

  // already displaying this screen - skip an extra display in offline contactless sales
  if((screen_id == LastScreen) && ( screen_id == DISP_PROCESSING_WAIT ))
  {
	  VIM_DBG_WARNING( "----Skip cust Screen----" );
	  return VIM_ERROR_NONE;
  }

#if 0
  if( screen_id == LastScreen && !vim_strcmp( last_image_filename, image_file_name ))
  {
	  VIM_DBG_WARNING("----Skip cust Screen----");
	  return VIM_ERROR_NONE;
  }
  last_image_filename = image_file_name;
  LastScreen = screen_id;
#endif

  //VIM_DBG_NUMBER( screen_id );
  // RDD 191214 - modifiy here in case we're in standalone lite mode
  if( IS_STANDALONE )	
  {
	  terminal.initator = it_pinpad;
  }

  if( pos_keymask == VIM_PCEFTPOS_KEY_ERROR )
  {
	  pos_keymask = VIM_PCEFTPOS_KEY_OK;
	  ErrorDisplay = VIM_TRUE;
  }

  if ((IS_V400m) && (terminal.logon_state == logged_on))
  {
      showStatusBar();
  }


  pos_displayed = VIM_FALSE;

  /* Find index in screens array */
  VIM_ERROR_RETURN_ON_FAIL(find_display_index(screen_id, &screens_index));

  /* Send pos display */
  /* if the transaction happend in idle, don't display in the generic pos */
  // RDD 191218 - re-wrote to form generic function
  VIM_VA_START(ap, pos_keymask);  
  VIM_ERROR_WARN_ON_FAIL(display_screen_pos_display(screens_index, pos_keymask, ap));  
  VIM_VA_END(ap);
  /* Do terminal display */
  terminal_text = screens[screens_index].terminal_text;
  if (!use_vim_screen_functions && (screens[screens_index].optional_traditional_terminal_text != VIM_NULL))
  {
	  terminal_text = screens[screens_index].optional_traditional_terminal_text;
  }

  if (terminal_text != VIM_NULL)
  {


    // RDD 191218 Required for use with graphics displays. Set in display_initialize

    if (vim_adkgui_is_active())
    {
        VIM_CHAR *ptr = VIM_NULL;
		VIM_CHAR image[64] = {0};
		vim_strcpy(image, image_file_name);
		//VIM_DBG_STRING(image);
		if ((vim_strchr(image, '.', &ptr) == VIM_ERROR_NONE) && ptr) {
			vim_strncpy(ptr + 1, IMAGE_FORMAT_ADK, VIM_SIZEOF(IMAGE_FORMAT_ADK));
		}
		display_screen(screen_id, pos_keymask, image);
	}

	// RDD 150212: Error Beep if cancelled for any reason
	//if(( screens_index == DISP_CANCELLED )||( screens_index == DISP_CARD_REJECTED ))
	// RDD 161214 - this looks like a bug as the index does not forllow the array member at all
#if 0
	if( screens_index == DISP_CANCELLED )
#else
	if(( screen_id == DISP_CANCELLED ) && ( IS_INTEGRATED ))
#endif
	{
		vim_kbd_sound_invalid_key();
	}

	if(( PosLen && PosLen <= 24 ) && ( terminal.initator == it_pceftpos ))
	{
		 VIM_PCEFTPOS_KEYMASK key;

		 // If there was an error then beep immediatly THEN do the wait KEY
		 if(( ErrorDisplay ) || ( txn.error == VIM_ERROR_POS_CANCEL ) || ( txn.error == VIM_ERROR_USER_CANCEL ))
		 {
			 if( !ErrorDisplay )
			 {
				 if( IS_INTEGRATED )				  
				 {
					 VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK));
				 }
			 }
			// DBG_MESSAGE( "!!!! WAIT 2 SECONDS !!!!" );
			 pceft_pos_wait_key( &key, TWO_SECOND_DELAY );
		 }
	}
  }
  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR ui_display_screen
(
    VIM_UINT32 screen_id,
    VIM_PCEFTPOS_KEYMASK pos_keymask,
    VIM_VA_LIST ap
)
{
    const VIM_CHAR *terminal_text;
    VIM_CHAR term_text[256];
    //VIM_CHAR pos_line_1[21], pos_line_2[21], line_1_length, line_2_length, pos_length;
    VIM_UINT32 screens_index;
    VIM_BOOL ErrorDisplay = VIM_FALSE;
    VIM_SIZE PosLen = 0;

    //VIM_EVENT_FLAG_PTR events[1];
    VIM_BOOL default_event = VIM_FALSE;
    VIM_EVENT_FLAG_PTR icc_event_ptr = &default_event;

    // RDD 110215 v421
    //static VIM_SIZE LastScreen;
    //VIM_DBG_MESSAGE("----------- display_screen --------------");


    // already displaying this screen - skip an extra display in offline contactless sales
    if ((screen_id == LastScreen) && (screen_id == DISP_PROCESSING_WAIT))
    {
        // RDD130619 - temporarily get rid of this
       // VIM_DBG_WARNING("----Skip Screen----");
        //return VIM_ERROR_NONE;
    }
    LastScreen = screen_id;

    //VIM_DBG_NUMBER( screen_id );


    if (pos_keymask == VIM_PCEFTPOS_KEY_ERROR)
    {
        pos_keymask = VIM_PCEFTPOS_KEY_OK;
        ErrorDisplay = VIM_TRUE;
    }

    pos_displayed = VIM_FALSE;

    /* Find index in screens array */
    VIM_ERROR_RETURN_ON_FAIL(find_display_index(screen_id, &screens_index));
    //VIM_DBG_NUMBER(screens_index);

    /* Send pos display */
    // RDD 191218 - re-wrote to form generic function
    VIM_ERROR_WARN_ON_FAIL(display_screen_pos_display(screens_index, pos_keymask, ap));

    /* Do terminal display */
	//VIM_DBG_NUMBER(screens_index);
    terminal_text = screens[screens_index].terminal_text;
    if( !use_vim_screen_functions && (screens[screens_index].optional_traditional_terminal_text != VIM_NULL))
    {
        terminal_text = screens[screens_index].optional_traditional_terminal_text;
    }

    if (terminal_text != VIM_NULL)
    {
		//VIM_DBG_POINT;
        VIM_ERROR_IGNORE(vim_vsnprintf(term_text, VIM_SIZEOF(term_text), terminal_text, ap));
		//VIM_DBG_STRING(term_text);
	    if(1)
        {
            // html
            // VN TODO  file integrity check
			//VIM_DBG_MESSAGE("ADK Display");

            if(( card_chip == txn.card.type) && ( txn.error != VIM_ERROR_EMV_CARD_REMOVED ))
            {
                VIM_DBG_WARNING(" IIIIIIIIIIIII UI Display Screen - Setup ICC Handle IIIIIIIIIIIIII");
                VIM_ERROR_IGNORE( vim_icc_get_removed_flag( icc_handle.instance_ptr, &icc_event_ptr ));
                //events[0] = icc_event_ptr;
            }

            if (screens[screens_index].screen_file != 0) {
				//VIM_DBG_STRING(screens[screens_index].screen_file);
                VIM_ERROR_RETURN_ON_FAIL( adkgui_display_terminal( term_text, screens[screens_index].screen_file, screens[screens_index].keys));

            }
            else
            {
				VIM_DBG_MESSAGE("A_Default.html");
                adkgui_display_terminal(term_text, "A_Default.html", VIM_NULL);
            }

           // DBG_POINT;
            if( *icc_event_ptr )
            {
                DBG_POINT;
                VIM_DBG_ERROR(txn.error);
                if (/* txn.error != VIM_ERROR_EMV_CARD_REMOVED */1)
                {
                    VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_REMOVED);
                }
                else
                {
                    VIM_DBG_WARNING("Card Removed Err Already Reported");
                }
            }

        }

        // RDD 150212: Error Beep if cancelled for any reason
        //if(( screens_index == DISP_CANCELLED )||( screens_index == DISP_CARD_REJECTED ))
        // RDD 161214 - this looks like a bug as the index does not forllow the array member at all

        if ((screen_id == DISP_CANCELLED) && (IS_INTEGRATED))
        {
            vim_kbd_sound_invalid_key();
        }

        if ((PosLen && PosLen <= 24) && (terminal.initator == it_pceftpos))
        {
            VIM_PCEFTPOS_KEYMASK key;

            // If there was an error then beep immediatly THEN do the wait KEY
            if ((ErrorDisplay) || (txn.error == VIM_ERROR_POS_CANCEL) || (txn.error == VIM_ERROR_USER_CANCEL))
            {
                if ( !ErrorDisplay )
                {
                    if( IS_INTEGRATED )
                    {
                        VIM_ERROR_RETURN_ON_FAIL( vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK) );
                    }
                }
                VIM_DBG_MESSAGE("!!!! WAIT 2 SECONDS !!!!");
                pceft_pos_wait_key(&key, TWO_SECOND_DELAY);
            }
        }
    }
	//VIM_DBG_MESSAGE("ui_display_screen DONE");
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_screen
(
    VIM_UINT32 screen_id,
    VIM_PCEFTPOS_KEYMASK pos_keymask,
    ...
)
{
    VIM_VA_LIST ap;

    VIM_VA_START(ap, pos_keymask);
   // VIM_DBG_PRINTF1(">>>>>>>>>>>> Display Screen: %d", screen_id );

    VIM_ERROR_RETURN_ON_FAIL(ui_display_screen(screen_id, pos_keymask, ap));
    VIM_VA_END(ap);
    return VIM_ERROR_NONE;
}

void pressAnyKey( void )
{
    VIM_PCEFTPOS_KEYMASK key_code = VIM_PCEFTPOS_KEY_NO;

    pceft_pos_wait_key( &key_code, THREE_SECOND_DELAY );
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR get_yes_no
(
  VIM_UINT32 timeout
)
{
  VIM_KBD_CODE key_code;
  while(1)
  {
    /* wait for key or touch */
      if( vim_adkgui_is_active() )
      {
          //VIM_EVENT_FLAG_PTR events[1];
          VIM_DBG_POINT;
          VIM_ERROR_RETURN_ON_FAIL( vim_kbd_read( &key_code, timeout ));
      }

    VIM_DBG_VAR( key_code );

    switch(key_code)
    {
      case VIM_KBD_CODE_OK:
         return VIM_ERROR_NONE;

      case VIM_KBD_CODE_CANCEL:
         VIM_ERROR_RETURN(VIM_ERROR_USER_CANCEL);

      case VIM_KBD_CODE_CLR:
        VIM_ERROR_RETURN(VIM_ERROR_USER_BACK);

      default:
        /*vim_kbd_invalid_key_sound();*/
        vim_kbd_sound_invalid_key();
        break;
    }
  }
}

static VIM_ERROR_PTR adkgui_display_list_select
(
    VIM_TEXT const *text,
    VIM_SIZE item_index,
    VIM_SIZE* selected_ptr,
    VIM_TEXT const * const items[],
    VIM_SIZE count,
    VIM_MILLISECONDS timeout,
    VIM_BOOL secret_item,
    VIM_BOOL numbered_items
)
{
    VIM_ERROR_PTR error;
    VIM_EVENT_FLAG_PTR events[2];
    VIM_BOOL default_event = VIM_FALSE;
    VIM_EVENT_FLAG_PTR icc_event_ptr = &default_event;
    VIM_BOOL allow_cancel = VIM_FALSE;

    /* check if the caller supplied a pointer for outputing the selected item */
    if (selected_ptr == VIM_NULL)
    {
        /* make sure selected_ptr points to something valid */
        selected_ptr = &item_index;
    }

    VIM_DBG_STRING("adkgui_display_list_select");
    VIM_DBG_VAR(count);
    VIM_DBG_VAR(item_index);

    // RDD 191218 Don't know what this is....
    //terminal.last_status_string = VIM_NULL;

    // RDD Todo - put back pos cancel support
    //events[0] = ecr_quest_pos_get_cancel_flag_addr();

    if (card_chip == txn.card.type)
    {
        //    if(vim_icc_new(&icc_handle,VIM_ICC_SLOT_CUSTOMER)==VIM_ERROR_NONE)
        VIM_ERROR_IGNORE(vim_icc_get_removed_flag(icc_handle.instance_ptr, &icc_event_ptr));
    }
    events[1] = icc_event_ptr;

    error = vim_adkgui_menu_dialog(
        text,
        item_index,
        selected_ptr,
        items,
        count,
        timeout,
        secret_item,
        numbered_items,
        allow_cancel,
        "A_Menu.html",
        events,
        2
    );

    //	VIM_ERROR_IGNORE(vim_icc_close(icc_handle.instance_ptr));
    //  icc_handle.instance_ptr=VIM_NULL;

    /* EMV Card is removed */
    if (*icc_event_ptr)
        return VIM_ERROR_EMV_CARD_REMOVED;

    // check events
    //if (*events[0])
    //{
    //    // RDD Todo - put back pos cancel support
    //    //ecr_quest_pos_clear_cancel_flag();
    //    return VIM_ERROR_POS_CANCEL;
    //}
    if (*selected_ptr > 0)
        *selected_ptr = *selected_ptr - 1;  // make it zero based
    return error;
}



VIM_ERROR_PTR display_list_select
(
    VIM_TEXT const *title,
    VIM_SIZE item_index,
    VIM_SIZE* selected_ptr,
    VIM_TEXT const * const items[],
    VIM_SIZE count,
    VIM_MILLISECONDS timeout,
    VIM_BOOL secret_item,
    VIM_BOOL numbered_items
)
{
    VIM_ERROR_RETURN_ON_FAIL( adkgui_display_list_select( title, item_index, selected_ptr, items, count, timeout, secret_item, numbered_items ));
    return  VIM_ERROR_NONE;
}


VIM_ERROR_PTR display_menu_select_zero_based
(
    VIM_TEXT const *text,
    VIM_SIZE item_index,
    VIM_SIZE* selected_ptr,
    VIM_TEXT const * const items[],
    VIM_SIZE count,
    VIM_MILLISECONDS timeout,
    VIM_BOOL secret_item,
    VIM_BOOL numbered_items,
    VIM_BOOL allow_exit,
    VIM_BOOL CheckCard
)
{
    VIM_ERROR_PTR error = VIM_ERROR_NONE;
    VIM_EVENT_FLAG_PTR events[2];
    VIM_BOOL default_event = VIM_FALSE;
    VIM_CHAR *Filename;
    //VIM_SIZE event_count = 0;

    VIM_EVENT_FLAG_PTR icc_event_ptr = &default_event;
    VIM_EVENT_FLAG_PTR pos_event_ptr = &default_event;

    /* check if the caller supplied a pointer for outputing the selected item */
    if (selected_ptr == VIM_NULL)
    {
        /* make sure selected_ptr points to something valid */
        selected_ptr = &item_index;
    }

    VIM_DBG_STRING("display_menu_select_zero_based");
    VIM_DBG_NUMBER(count);
	VIM_DBG_NUMBER(item_index);

    switch (count)
    {
        case 1:
            Filename = "Menu1Button.html";
            break;
        case 2:
            Filename = "Menu2Button.html";
            break;
        case 3:
            Filename = "Menu3Button.html";
            break;
        default:
            Filename = "A_Menu.html";
            break;
    }

    //terminal.last_status_string = VIM_NULL;

        if (card_chip == txn.card.type)
        {
            //    if(vim_icc_new(&icc_handle,VIM_ICC_SLOT_CUSTOMER)==VIM_ERROR_NONE)
            VIM_ERROR_IGNORE(vim_icc_get_removed_flag(icc_handle.instance_ptr, &icc_event_ptr));
        }
        events[0] = icc_event_ptr;
        // RDD 060619 - Menu with "prev" and "next" not supported by 3 button html file

        // Ask to monitor POS Cancel during PIN entry
        VIM_ERROR_IGNORE(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pos_event_ptr));
        events[1] = pos_event_ptr;

           error = vim_adkgui_menu_dialog(
                text,
                item_index,
                selected_ptr,
                items,
                count,
                timeout,
                secret_item,
                numbered_items,
                allow_exit,
                Filename,
                events,
                2);

        /* EMV Card is removed */
        if (*icc_event_ptr)
        {
            VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_REMOVED);
        }

        // check events
        if (*pos_event_ptr)
        {
            VIM_ERROR_RETURN(VIM_ERROR_POS_CANCEL);
        }


        VIM_DBG_ERROR(error);
        VIM_DBG_NUMBER(*selected_ptr);


    VIM_DBG_NUMBER(*selected_ptr);
    VIM_ERROR_RETURN(error);
}



#define EMV_APP_LIST_MAX    25
#define EMV_APP_LIST_TITLE  "SELECT APPLICATION"

VIM_ERROR_PTR display_menu_select_gui
(
    VIM_TEXT const *title,
    VIM_SIZE* item_index,
    VIM_SIZE* selected_ptr,
    VIM_TEXT const * const items[],
    VIM_SIZE count,
    VIM_MILLISECONDS timeout,
    VIM_BOOL secret_item,
    VIM_BOOL numbered_items,
    VIM_BOOL allow_exit,
    VIM_BOOL CheckCard
)
{

    //vim_tch_flush();
    VIM_DBG_WARNING("-------display_menu_select_gui-------");

    /* check if the caller supplied a pointer for outputing the selected item */
    if (selected_ptr == VIM_NULL)
    {
        /* make sure selected_ptr points to something valid */
        selected_ptr = item_index;
    }

    while (1)
    {
        VIM_ERROR_PTR result;
        {

            if(( result = display_menu_select_zero_based(EMV_APP_LIST_TITLE, 0, selected_ptr, items, count, ENTRY_TIMEOUT, VIM_FALSE, VIM_TRUE, VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE)
            {
                VIM_DBG_NUMBER( *selected_ptr );
                VIM_DBG_ERROR( result );

                if (*selected_ptr <= count)
                {
                    *selected_ptr -= 1;
                    VIM_ERROR_RETURN(VIM_ERROR_NONE);
                }
                else
                {
                    VIM_DBG_WARNING("Invalid Selection");
                    continue;
                }
            }
        }

        if( result == VIM_ERROR_TIMEOUT)
        {
            VIM_DBG_WARNING("---------- Process Timeout -----------");
            if( allow_exit )
            {
                //DBG_POINT;
                return result;
            }
            else
            {
                ERROR_TONE;
                continue;
            }
        }

        if (result != VIM_ERROR_NONE && result != VIM_ERROR_TIMEOUT)
        {
            VIM_DBG_ERROR(result);
            return result;
        }
    }
}






static void MenuInitItemList( VIM_CHAR **ItemPtr, S_MENU *MenuPtr, VIM_SIZE MenuSize )
{
	VIM_SIZE Item;
	for( Item=0; Item< MenuSize; Item++, MenuPtr++ )
		*ItemPtr++ = (VIM_CHAR *)&MenuPtr->MenuItem;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR MenuRun( S_MENU *MenuList, VIM_SIZE MenuLen, const VIM_CHAR *MenuTitle, VIM_BOOL SecretItem, VIM_BOOL NumberedItems )
{
	VIM_CHAR *items[ MAX_MENU_SIZE ];
	//VIM_ERROR_PTR res = VIM_ERROR_NONE;
	//VIM_SIZE ItemIndex = 0;
	VIM_SIZE SelectedIndex = 0;

	if( MenuLen > MAX_MENU_SIZE ) return VIM_ERROR_OVERFLOW;

	MenuInitItemList( (VIM_CHAR **)items, MenuList, MenuLen );

    if( vim_adkgui_is_active() )
    {
        VIM_ERROR_RETURN_ON_FAIL(display_menu_select_zero_based( MenuTitle, MenuLen, &SelectedIndex, (VIM_TEXT const **)items, MenuLen, ENTRY_TIMEOUT, SecretItem, NumberedItems, VIM_TRUE, VIM_FALSE ));
        if( SelectedIndex )
            SelectedIndex -= 1;
    }

    VIM_DBG_NUMBER(SelectedIndex);
    // RDD 130215 SPIRA:8388 - crash if 0 selected and less than 10 menu items...

	if( SelectedIndex <= MenuLen )
		VIM_ERROR_RETURN_ON_FAIL( MenuList[SelectedIndex].MenuFunction() );
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR adkgui_pin_entry(
    VIM_TCU_PTR self_ptr,
    VIM_TCU_PIN_ENTRY_PARAMETERS const * parameters_ptr,
    VIM_SIZE *pin_size,
    VIM_UINT32 id,
    ...
)
{
    VIM_CHAR term_text[512];
    VIM_UINT32 display_index;
    VIM_VA_LIST ap;
    const VIM_TEXT default_file_name[] = "A_Default.html";
    const VIM_TEXT *file_name;
    const key_map_t* key_mapping;
    //VIM_DBG_NUMBER(id);
    VIM_ERROR_PTR error;
    VIM_EVENT_FLAG_PTR events[2];
    VIM_BOOL default_event = VIM_FALSE;
    VIM_EVENT_FLAG_PTR icc_event_ptr = &default_event;
    VIM_EVENT_FLAG_PTR pos_event_ptr = &default_event;
    VIM_SIZE event_count = 0;

    pos_displayed = VIM_FALSE;
    VIM_ERROR_RETURN_ON_FAIL(find_display_index(id, &display_index));

    VIM_VA_START(ap, id);    
    VIM_ERROR_WARN_ON_FAIL(display_screen_pos_display(display_index, VIM_PCEFTPOS_KEY_CANCEL, ap));
    VIM_VA_END(ap);

    if (screens[display_index].terminal_text == VIM_NULL)
    {
        VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
    }

    VIM_VA_START(ap, id);
    vim_vsnprintf(term_text, VIM_SIZEOF(term_text), screens[display_index].terminal_text, ap);
    VIM_VA_END(ap);

    if( screens[display_index].screen_file != 0 )
    {
        file_name = screens[display_index].screen_file;
        key_mapping = screens[display_index].keys;
    }
    else
    {
        file_name = default_file_name;
        key_mapping = VIM_NULL;
    }

    //terminal.last_status_string = VIM_NULL;

    // Ask to monitor POS Cancel during PIN entry
    VIM_ERROR_IGNORE(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pos_event_ptr));
    events[0] = pos_event_ptr;
    event_count = 1;

    if (card_chip == txn.card.type)
    {
        VIM_DBG_WARNING(" IIIIIIIIIIIII Setup ICC Handle IIIIIIIIIIIIII");
        VIM_ERROR_IGNORE( vim_icc_get_removed_flag( icc_handle.instance_ptr, &icc_event_ptr ));
        events[1] = icc_event_ptr;
        event_count = 2;
    }

    VIM_DBG_WARNING("Enter vim_adkgui_pin_entry");
    error = vim_adkgui_pin_entry(self_ptr,
        parameters_ptr,
        term_text,
        file_name,
        key_mapping,
        events,
        event_count,
        pin_size);

    VIM_DBG_WARNING("Exit vim_adkgui_pin_entry");
    //	VIM_ERROR_IGNORE(vim_icc_close(icc_handle.instance_ptr));
    //  icc_handle.instance_ptr=VIM_NULL;

    /* EMV Card is removed */
    if (*icc_event_ptr)
    {
        VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_REMOVED);
    }

    // check events
    if (*pos_event_ptr)
    {
        VIM_ERROR_RETURN(VIM_ERROR_POS_CANCEL);
    }
    return error;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR adkgui_data_entry(char *inbuf, kbd_options_t const* options, VIM_UINT32 id, VIM_PCEFTPOS_KEYMASK pos_keymask, VIM_VA_LIST ap)
{
    VIM_UINT8 min, max;
    VIM_BOOL MaskInput = VIM_FALSE;

    VIM_ERROR_PTR error;
    VIM_CHAR term_text[512];
    VIM_UINT32 display_index;
    const VIM_TEXT default_file_name[] = "A_Default.html";
    const VIM_TEXT *file_name;
    VIM_SIZE OtherEvents = 1;

    VIM_BOOL CheckPOS = (pos_keymask == VIM_PCEFTPOS_KEY_NONE) ? VIM_FALSE : VIM_TRUE;
    VIM_EVENT_FLAG_PTR events[2];
    VIM_BOOL default_event = VIM_FALSE;
    VIM_EVENT_FLAG_PTR icc_event_ptr = &default_event;
    VIM_EVENT_FLAG_PTR pos_event_ptr = &default_event;

    VIM_DBG_MESSAGE("------------------ DisplayGui data entry ------------------");

    // check for POS only if necessary
    if ((IS_INTEGRATED) && (CheckPOS))
    {
        OtherEvents += 1;
    }

    if (options->entry_options & KBD_OPT_IP)
    {
        min = 12;
        max = 12;
    }
    else if (options->entry_options & (KBD_OPT_EXPIRY))
    {
        min = 4;
        max = 4;
    }
    else
    {
        min = options->min;
        max = options->max;
    }

    /* Find index in screens array */
    VIM_ERROR_RETURN_ON_FAIL(find_display_index(id, &display_index));
    //VIM_DBG_NUMBER(screens_index);

    DBG_POINT;
    /* Send pos display */
    // RDD 191218 - re-wrote to form generic function
	// Change to warning in case going from standalone -> integrated and POS I/F not yet setup. we need acces to fn menu.
    VIM_ERROR_WARN_ON_FAIL(display_screen_pos_display(display_index, pos_keymask, ap));

    vim_vsnprintf(term_text, sizeof(term_text), screens[display_index].terminal_text, ap);

	DBG_POINT;
    // RDD 110619 Added to mask input like for function menu and manual entry
    if (options->entry_options & KBD_OPT_MASKED)
    {
        MaskInput = VIM_TRUE;
        file_name = PASSWORD_FILENAME_ADK;
    }
    else
    {
        if (screens[display_index].screen_file != 0)
        {
            file_name = screens[display_index].screen_file;
        }
        else
        {
            file_name = default_file_name;
        }
    }
    //terminal.last_status_string = VIM_NULL;

    // Ask to monitor POS Cancel during PIN entry
    //events[0] = ecr_quest_pos_get_cancel_flag_addr();

    if (card_chip == txn.card.type)
    {
        //    if(vim_icc_new(&icc_handle,VIM_ICC_SLOT_CUSTOMER)==VIM_ERROR_NONE)
        VIM_ERROR_IGNORE(vim_icc_get_removed_flag(icc_handle.instance_ptr, &icc_event_ptr));
    }
    events[0] = icc_event_ptr;

    if (IS_INTEGRATED)
    {
        // Ask to monitor POS Cancel during PIN entry
        DBG_POINT;
        VIM_ERROR_IGNORE(vim_pceftpos_get_message_received_flag(pceftpos_handle.instance, &pos_event_ptr));
        events[1] = pos_event_ptr;
    }

#ifndef _DEBUG
	//vim_adkgui_set_data_entry();
#endif
    VIM_DBG_PRINTF1("timeout = %d",options->timeout);
    error = vim_adkgui_data_entry(min, max, MaskInput, options->timeout, inbuf,
        term_text,
        file_name,
        events,
        OtherEvents);

    VIM_DBG_ERROR(error);
    VIM_DBG_STRING(inbuf);

#ifndef _DEBUG
	//vim_adkgui_unset_data_entry();
#endif

    //	VIM_ERROR_IGNORE(vim_icc_close(icc_handle.instance_ptr));
    //  icc_handle.instance_ptr=VIM_NULL;

    /* EMV Card is removed */
    if (*icc_event_ptr)
    {
        VIM_ERROR_RETURN(VIM_ERROR_EMV_CARD_REMOVED);
    }
    else if ((*pos_event_ptr)&&( IS_INTEGRATED ))
    {
        VIM_ERROR_RETURN(VIM_ERROR_POS_CANCEL);
    }
    // check events
    //if (*events[0])
    //{
    //    //ecr_quest_pos_clear_cancel_flag();
    //    return VIM_ERROR_POS_CANCEL;
    //}
    VIM_ERROR_RETURN( error );
}



// RDD 201218 Create a generic function to handle display and data entry in all platforms

VIM_ERROR_PTR display_data_entry(
    VIM_CHAR *entry_buf,
    kbd_options_t const *entry_opts,
    VIM_UINT32 screen_id,
    VIM_PCEFTPOS_KEYMASK pos_keymask,
    ...)
{
    VIM_ERROR_PTR error = VIM_ERROR_NONE;
    VIM_VA_LIST ap;

    if (vim_adkgui_is_active())
    {
        VIM_VA_START(ap, pos_keymask);
        VIM_DBG_VAR(entry_opts->entry_options);
        VIM_DBG_VAR(entry_opts->max);
        VIM_DBG_VAR(entry_opts->min);
        error = adkgui_data_entry(entry_buf, entry_opts, screen_id, pos_keymask, ap);
        VIM_VA_END(ap);
        VIM_DBG_POINT;
    }

    // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
    if ((error != VIM_ERROR_NONE)&&( txn.type != tt_query_card_openpay))
    {
        display_result(error, "", "", "", VIM_PCEFTPOS_KEY_NONE, TWO_SECOND_DELAY);
    }
    VIM_ERROR_RETURN(error);
}




// RDD 131218 Added for P400 Project ADK Requires Pin Entry Callback

// handle 1,2,3,<Enter> situation

// RDD 131218 Callback from vim_adk_gui
// handle 1,2,3,<Enter> situation
VIM_ERROR_PTR vim_adkgui_pinentry_underflow_cb(void)
{
    // display_screen(SCREEN_PIN_INVALID);
    // vim_event_sleep(3000);
    vim_kbd_sound_invalid_key();
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR InitGUI()
{
    screens = screens_adk;
    VIM_ERROR_RETURN_ON_NULL(screens);
    {
        vim_adkgui_init();
    }
    return VIM_ERROR_NONE;
}


void display_get_image_format( VIM_CHAR *format, VIM_UINT8 *len )
{
    *len = VIM_SIZEOF(IMAGE_FORMAT_ADK) - 1;
    vim_mem_copy(format, IMAGE_FORMAT_ADK, *len);
}


VIM_ERROR_PTR display_callback_text(VIM_CHAR *TextBuffer, VIM_BOOL DataEntry )
{
    VIM_DBG_VAR(DataEntry);

    if (!DataEntry)
    {
        VIM_DBG_PRINTF1( ">>>>>>>>>>>> Display Text: %s", TextBuffer );
        return display_screen( DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, TextBuffer, "" );
    }
    else
    {
        return display_screen( DISP_GENERIC_ENTRY_POINT, VIM_PCEFTPOS_KEY_NONE, TextBuffer, "" );
    }
}
