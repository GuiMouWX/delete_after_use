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


VIM_DBG_SET_FILE("T7");
#define ESCAPE_STR     "\x1B"
#define ESCAPE_CHAR    '\x1B'

/** Escape sequences. Include anywhere on a line to modify the way it looks. Default is center alignment, large font, inverse off */

#define FONT_SMALL      ESCAPE_STR"1"
#define FONT_MEDIUM     ESCAPE_STR"2"
#define FONT_LARGE      ESCAPE_STR"3"
#define LEFT_ALIGN      ESCAPE_STR"L"
#define RIGHT_ALIGN     ESCAPE_STR"R"
#define INVERSE_LINE    ESCAPE_STR"I"
#define ENTRY_POINT     ESCAPE_STR"E"
#define BASE_ALIGN      ESCAPE_STR"B"
#define BITMAP_ARROW    ESCAPE_STR"A"
#define BITMAP_LOGO     ESCAPE_STR"O"

#define ESC_FIELD             'F'
#define ESC_FNT_SMALL         '1'
#define ESC_FNT_MEDIUM        '2'
#define ESC_FNT_LARGE         '3'
#define ESC_ALIGN_LEFT        'L'
#define ESC_ALIGN_RIGHT       'R'
#define ESC_INVERSE           'I'
#define ESC_ENTRY             'E'
#define ESC_ALIGN_BASE        'B'
#define ESC_BITMAP_ARROW      'A'
#define ESC_BITMAP_LOGO       'O'

#define FIELD(field_name)    ESCAPE_STR "F" #field_name ESCAPE_STR

#define PROMPT_BACK_CLEAR_OK_ENTER FONT_MEDIUM"BACK/CLR   OK/ENTER"
#define PROMPT_CANCEL_RETRY        FONT_MEDIUM"CANCEL        RETRY"
#define PROMPT_CLEAR_OK           FONT_MEDIUM "CLEAR            OK"
#define PROMPT_OK                 FONT_MEDIUM"                  OK"
#define PROMPT_YES_NO             "YES OR NO"
#define PROMPT_OK_CANCEL          "OK OR CANCEL"

#define IDLE_FILENAME			"IdleStatus.ui"

#define MAX_MENU_SIZE 20

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
    { "Button1", VIM_KBD_CODE_SOFT1 },
    { "Button2", VIM_KBD_CODE_SOFT2 },
    { "Button3", VIM_KBD_CODE_SOFT3 },
    { "", VIM_KBD_CODE_NONE }
};

const key_map_t cheque_savings_keys[] =
{
    { "Button1", VIM_KBD_CODE_SOFT1 },
    { "Button2", VIM_KBD_CODE_SOFT2 },
    { "", VIM_KBD_CODE_NONE }
};

const key_map_t cheque_credit_keys[] =
{
    { "Button1", VIM_KBD_CODE_SOFT1 },
    { "Button2", VIM_KBD_CODE_SOFT3 },
    { "", VIM_KBD_CODE_NONE }
};

const key_map_t savings_credit_keys[] =
{
    { "Button1", VIM_KBD_CODE_SOFT2 },
    { "Button2", VIM_KBD_CODE_SOFT3 },
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
    { "Button1", VIM_KBD_CODE_SOFT1 },
    { "Button2", VIM_KBD_CODE_SOFT2 },
    { "Button3", VIM_KBD_CODE_SOFT3 },
    { "ButtonPrev", VIM_KBD_CODE_RFUNC0 },
    { "ButtonNext", VIM_KBD_CODE_RFUNC1 },
    { "", VIM_KBD_CODE_NONE }
};


const key_map_t cheque_keys[] =
{
    { "Button1", VIM_KBD_CODE_KEY1 },
    { "", VIM_KBD_CODE_NONE }
};



const screen_t screens[] =
{
	{
		DISP_BRANDING,
		VIM_NULL,
		"",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		GEN_BRAND1_UI_FILE
	},

	{
		DISP_SCREENSAVER2,
		VIM_NULL,
		"",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		GEN_BRAND2_UI_FILE
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
		FIELD(Label) "%s\n"\
		FIELD(Status)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		IDLE_PCEFT
	},

	{
		DISP_IDLE,
		VIM_NULL,
		FIELD(Label) "%s\n"\
		FIELD(Status)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		IDLESTATUS
	},

	{
		DISP_IDLE2,
		VIM_NULL,
		FIELD(Label) "%s\n"\
		FIELD(Status)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		IDLESTATUS1
	},


	{
		DISP_SWIPE_OR_INSERT,
		"SWIPE OR\nINSERT CARD",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Swipe or\nInsert Card",
		VIM_PCEFTPOS_GRAPHIC_CARD,
		"Transaction.ui"
	},

	{
		DISP_PROCESSING_WAIT,
#ifdef _XPRESS
        VIM_NULL,
#else
		"PROCESSING\n"\
		"PLEASE WAIT",
#endif
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"%s\nPlease Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.ui"
	},


	{
		DISP_SENDING_WAIT,
		VIM_NULL,
		//"SENDING\n"\
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Sending\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.ui"
	},

	{
		DISP_RECEIVING_WAIT,
		VIM_NULL,
		//"RECEIVING\n"\
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Receiving\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.ui"
	},

	{
		DISP_DISCONNECTING_WAIT,
		VIM_NULL,
		//"DISCONNECTING\n"\
		//"PLEASE WAIT",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"Disconnecting\n"\
		"Please Wait\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.ui"
	},

	{
		DISP_SELECT_ACCOUNT,
		"SELECT ACCOUNT\n"\
		"CHQ, SAV OR CR",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s"\
		FIELD(Button1)"Cheque or Press 1"\
		FIELD(Button2)"Savings or Press 2"\
		FIELD(Button3)"Credit or Press 3"\
		FIELD(Label)"Select Account\n",
		VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
		"Menu3Button.ui",
		cheque_savings_credit_keys,

		/* Traditional text-based display */
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		"%s"\
		FIELD(Label)"  SELECT ACCOUNT     \n"
		FONT_SMALL"                       \n"
		FONT_SMALL"                       \n"
		FONT_SMALL"                       \n"
		FONT_SMALL " Cheque Savings Credit\n"
	},

	{
		DISP_SELECT_ACCOUNT_CHQ_SAV,
		"SELECT ACCOUNT\n"\
		"CHQ OR SAV",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s"\
		FIELD(Label)"Select Account\n"\
		FIELD(Button1)"Cheque or Press 1"\
		FIELD(Button2)"Savings or Press 2",
		VIM_PCEFTPOS_GRAPHIC_ACCOUNT,
		"Menu2Button.ui",
		cheque_savings_keys,

		/* Traditional text-based display */
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s"\
		FIELD(Label)"  SELECT ACCOUNT     \n"
		FONT_SMALL"                       \n"
		FONT_SMALL"                       \n"
		FONT_SMALL"                       \n"
		FONT_SMALL " Cheque Savings       \n"
	},
	  {
    DISP_SWITCH_TO_INTEGRATED_MODE, 
    VIM_NULL,
    "\n"\
    "SWITCH TO\n"\
    "INTEGRATED MODE?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_SWITCH_TO_STANDALONE_MODE, 
    VIM_NULL,
    "\n"\
    "SWITCH TO\n"\
    "STANDALONE MODE?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_INTERNAL_PRINTER,
    VIM_NULL,
    "\n"\
    "INTERNAL PRINTER?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_WINDOWS_PRINTER,
    VIM_NULL,
    "\n"\
    "USE WINDOWS\n"\
    "DEFAULT PRINTER?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_INTERNAL_MODEM,
    VIM_NULL,
    "\n"\
    "INTERNAL MODEM?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_JOURNAL_TXN,
    VIM_NULL,
    "\n"\
    "JOURNAL TXNS?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_PRINT_SECOND_RECEIPT,
    VIM_NULL,
    "\n"\
    "PRINT SECOND\n"\
    "RECEIPT?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  {
    DISP_STANDALONE_SWIPE_START_TXN,
    VIM_NULL,
    "\n"\
    "SWIPE START\n"\
    "TRANSACTIONS?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
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
		"Menu3Button.ui",
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
		"RemoveCardA.ui"
	},

	{
		DISP_REMOVE_CARD_B,
		"REMOVE CARD",		// RDD 050312: SPIRA IN4852 - Keep POS in txn mode by sending remove card message repeatedly
		FIELD(Label)"",
		VIM_PCEFTPOS_GRAPHIC_REMOVE,
		"RemoveCardB.ui"
	},


#if 1
	{
		DISP_KEY_PIN_OR_ENTER,
		"KEY PIN OR ENTER",
		FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"%s\n"\
		LEFT_ALIGN""ENTRY_POINT,
		VIM_PCEFTPOS_GRAPHIC_PIN,
		"PINOrEnter.ui"
	},
#else
	{
		DISP_KEY_PIN_OR_ENTER,
		"KEY PIN OR ENTER",
		FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
		FIELD(Label)"%s\n"\
		LEFT_ALIGN""ENTRY_POINT,
		VIM_PCEFTPOS_GRAPHIC_PIN,
		//"PINOrEnter.ui"
		"EntryPoint.ui"
	},
#endif
	{
		DISP_TRANSACTION_APPROVED,
#ifdef _XPRESS
        VIM_NULL,
#else
		"APPROVED",
#endif
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s"\
		FIELD(Label)"\n%s\n",
		VIM_PCEFTPOS_GRAPHIC_FINISHED,
		"Transaction.ui"
	},

	{
		DISP_TRANSACTION_APPROVED_BAL_CLR,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(Label)"\nApproved\n"\
		FIELD(Bal) "BAL: " FIELD(BalAmount)"%s\n"\
		FIELD(Clr) "CLR: "FIELD(ClrAmount)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Approved.ui"
		//"Balance.ui"
	},

	{
		DISP_TRANSACTION_DECLINED_BAL_CLR,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(Label)"\n%s\n"\
		FIELD(Bal) "" FIELD(BalAmount)"\n"\
		FIELD(Clr) "CLR: "FIELD(ClrAmount)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Approved.ui"
		//"Transaction.ui"
		//"Balance.ui"
	},

	{
		DISP_APPROVED_WITH_SIG,
		"SIGNATURE OK?",
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(AppName)"%s"FIELD(AccountType)"%s"\
		FIELD(Label)"Approved With\n"\
		"Signature\n",
		VIM_PCEFTPOS_GRAPHIC_QUESTION,
		"Transaction.ui"
	},

	{
		DISP_DECLINED,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(AppName)"%s"FIELD(AccountType)"%s"\
		FIELD(Label)"%s",
		VIM_PCEFTPOS_GRAPHIC_FINISHED,
		"Transaction.ui"
	},

	{
		DISP_CARD_NUMBER,
		"ENTER CARD NUMBER\nON PINPAD",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(Label)"Card Number:\n",
		VIM_PCEFTPOS_GRAPHIC_DATA,
		"EntryPoint.ui"
	},

	{
		DISP_EXPIRY,
		"ENTER EXPIRY <MMYY>\nON PINPAD",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(Label)"Expiry <MMYY>\n",
		VIM_PCEFTPOS_GRAPHIC_DATA,
		"EntryPoint.ui"
	},

	{
		DISP_SETTLE_DATE,
		"KEY SETTLE DATE\nON PINPAD <DDMM>",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(Label)"Key Date <DDMM>\n(Enter For Current)",
		VIM_PCEFTPOS_GRAPHIC_DATA,
		"EntryPoint.ui"
	},

	{
		DISP_QUERY_EXPIRY,
		"ENTER EXPIRY <MMYY>\nON PINPAD",
		FIELD(TransactionType)" " FIELD(Amount)" "\
		FIELD(Label)"Expiry <MMYY>\n",
		VIM_PCEFTPOS_GRAPHIC_DATA,
		"EntryPoint.ui"
	},

	{
		DISP_CONTACTLESS_PAYMENT,
		"PRESENT CARD",
		FIELD(CTLS_Status)"%s\n"\
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(Label)"Present Card",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"ContactlessPurchaseScreen.ui"
	},

		{
		DISP_CONTACTLESS_TEST,
		"TAP CARD",
		FIELD(CTLS_Status)"%s\n"\
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(Label)"Present Card",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"ContactlessPurchaseScreen.ui"
	},

      {
          DISP_GENERIC_ENTRY_POINT,
          "%s",
           FIELD(Label) "%s",
          VIM_PCEFTPOS_GRAPHIC_PROCESSING,
          "EntryPoint.ui"
      },


	{
		DISP_KEY_PIN,
		"KEY PIN",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(AppName)"%s" FIELD(AccountType)"%s"\
		FIELD(Label)"%s\n"\
		LEFT_ALIGN""ENTRY_POINT,
		VIM_PCEFTPOS_GRAPHIC_PIN,
		"PINOrEnter.ui"
	},

	{
		DISP_WAIT,
		"ALLOW MANUAL\nKEY ENTRY",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Please Wait",
		VIM_PCEFTPOS_GRAPHIC_QUESTION,
		"Transaction.ui"
	},
	
	{
		DISP_KEY_DATA,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(Label)"%s",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"EntryPoint.ui"
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
		"Menu3Button.ui",
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
		FIELD(Label)"%s\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"EntryPoint.ui",
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
		VIM_PCEFTPOS_GRAPHIC_PROCESSING
	},

	{
		DISP_CARD_READ_SUCCESSFUL,
		VIM_NULL,
		FIELD(TransactionType)"Deposit" FIELD(Amount) " "\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Card Read\nSuccessful",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		"Transaction.ui"
	},

	{
		DISP_SYSTEM_ERROR,
		VIM_NULL,
		FIELD(Label)"%s\n"\
		"System Error\n",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING
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
		VIM_PCEFTPOS_GRAPHIC_PROCESSING
	},

	{
		DISP_SETTLEMENT_APPROVED,
		"%s",
		FIELD(TransactionType)" " FIELD(Amount) " "\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"%s",
		VIM_PCEFTPOS_GRAPHIC_FINISHED,
		"Transaction.ui"
	},
	{
		DISP_CARD_REJECTED,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(AppName)"%s"FIELD(AccountType)"%s"\
		FIELD(Label)"%s",
		VIM_PCEFTPOS_GRAPHIC_CARD,
		"Transaction.ui"
	},
	{
		DISP_CANCELLED,
		VIM_NULL,
		FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
		FIELD(AppName)"%s"FIELD(AccountType)"%s"\
		FIELD(Label)"%s",
		VIM_PCEFTPOS_GRAPHIC_FINISHED,
		"Transaction.ui"
	},
	{
		DISP_CARD_MUST_BE_INSERTED,
		"CARD MUST BE\nINSERTED",
		FIELD(TransactionType)"%s\n"  FIELD(Amount)"%s\n"\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Card Must Be\nInserted"\
		" ",
		VIM_PCEFTPOS_GRAPHIC_INSERT,
		"Transaction.ui"
	},
	
	{
		DISP_PS_CARD_MUST_BE_INSERTED,
		VIM_NULL,
		FIELD(Label)"Card Must Be\nInserted",
		VIM_PCEFTPOS_GRAPHIC_INSERT
	},

	{
		DISP_UNABLE_TO_PROCESS_CARD,
		"CARD REJECTED\nINVALID CARD NO",
		FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Card Rejected\nInvalid Card No"\
		" ",
		VIM_PCEFTPOS_GRAPHIC_FINISHED,
		"Transaction.ui"
	},
	
	{
		DISP_APPROVED_TXN_ONLY,
		"APPROVED",
		"%s\n"\
		"Approved",
		VIM_PCEFTPOS_GRAPHIC_FINISHED
	},

  {
    DISP_FUNCTION,
    "ENTER PASSWORD\nON PINPAD",
    FIELD(TransactionType)" " FIELD(Amount) " "\
    FIELD(Label) "%s\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	  "EntryPoint.ui"
  },

  {
    DISP_ENTER_SAF_LIMIT,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount) " "\
    FIELD(Label) "Enter SAF Limit:\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	  "EntryPoint.ui"
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
    FIELD(Label)"STORED TXNS:\n"
    "%s\n%s\n%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  
  {
    DISP_DELETING_FILES,
    VIM_NULL,
    FONT_LARGE"Deleted All\n"\
    FONT_LARGE"Config Files",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  
  {
    DISP_ENTER_MERCHANT_PASSWORD,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label) "Enter Merchant\n"
    "Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.ui"
  },

  {
    DISP_CARD_READ_STATISTICS,
    VIM_NULL,
    FONT_LARGE "\n"
    FONT_LARGE LEFT_ALIGN "Swipe Card\n"
    FONT_LARGE LEFT_ALIGN ENTRY_POINT "Good Read   %3d\n"
    FONT_LARGE LEFT_ALIGN ENTRY_POINT "Bad Read    %3d\n"
    FONT_LARGE LEFT_ALIGN ENTRY_POINT "Total Read  %3d\n",
    VIM_PCEFTPOS_GRAPHIC_CARD
  },
  
  {
    DISP_POS_FME,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"POS-FME\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	"EntryPoint.ui"
  },

  {
    DISP_NII,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"NII\n"\
    "%d",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	"EntryPoint.ui"
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
    "EFT LOGON",
    FIELD(Label)"EFT Logon\nPlease Wait",
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
    "ENTER AMOUNT\nON PINPAD",
    FIELD(TransactionType) "%s" FIELD(Amount) "%s"\
    FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_ACTIVATION_AMT, 
    VIM_NULL,
    FIELD(TransactionType) "%s" FIELD(Amount) "%s"\
    FIELD(Label)"%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_TERMINAL_CONFIG_TERMINAL_ID,
    VIM_NULL,
    FIELD(TransactionType) "" FIELD(Amount) ""\
    FIELD(Label)"Enter Terminal ID:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_IDLE_LOCKED,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"Locked\n"\
    "Enter Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_CHANGE_TIME,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"Enter Time HHMM:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },

  {
    DISP_CHANGE_DATE,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"Date:\n"\
    "DD/MM/YYYY\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
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
	"EntryPoint.ui"
  },
  
  {
    DISP_PW_ENTER_NEW,
    VIM_NULL,
    FIELD(TransactionType) " " FIELD(Amount) " "\
    FIELD(Label)"%s Type Password\n"\
    "Enter New Password\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
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
    FIELD(Label)"Enter Auth\n"\
	"Number\n"\
    ENTRY_POINT"\n",
    VIM_PCEFTPOS_GRAPHIC_DATA,
	"EntryPoint.ui"
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
    "Transaction.ui"
  },

  {
    DISP_TERMINAL_CONFIG_MERCHANT_ID,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(Label) "Enter Merchant ID\n"\
    "%s\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },

  {
    DISP_TERMINAL_CONFIG_TRANSSEND_ID,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(Label)"TRAN$END ID:\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },

  {
    DISP_QUERY_CARD_NUMBER,
    "ENTER CARD NUMBER\nON PINPAD",
    FIELD(TransactionType)"%s" FIELD(Amount)"%s"\
    FIELD(Label)"Card Number:\n",
    VIM_PCEFTPOS_GRAPHIC_CARD,
	"EntryPoint.ui"
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
    "YesNo.ui",
    yes_no_keys
  },
  
  {
    DISP_TMS_PHONE,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(Label)"TMS Phone No:\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_TMS_SHA,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(Label)"TMS SHA:\n"\
    "%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
  },
  
  {
    DISP_TMS_NII,
    VIM_NULL,
    FIELD(TransactionType)" " FIELD(Amount)" "\
    FIELD(Label)"TMS NII:\n"\
    "%d",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"EntryPoint.ui"
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
	"EntryPoint.ui"
  },

    {
    DISP_SAF_UPLOADING, 
    "PROCESSING\nPLEASE WAIT", // RDD 090312: SPIRA 5057: NO POS DISPLAY FOR UPLOADING ADVICES
#ifdef _DEBUG
    FONT_LARGE"Sending Advice\n"\
    FONT_SMALL"%s %s\n",
#else
    FONT_LARGE"Sending\n"\
    FONT_LARGE"Advice\n",
#endif
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
    DISP_USE_MAG_STRIPE,
    "SWIPE CARD",
    FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
    FIELD(AppName)" "FIELD(AccountType)" "\
    FIELD(Label)"Swipe Card",
    VIM_PCEFTPOS_GRAPHIC_CARD,
    "Transaction.ui"
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
	"EntryPoint.ui"
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
  
#if 0
  {
    DISP_CLEAR_SAF,
    VIM_NULL,
    "\n"\
    "CLEAR SAF\n"\
    "TRANSACTIONS?\n"\
    "[ENTER] = YES\n"\
    "[CLEAR] = NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "YesNo.ui",
    yes_no_keys
  },
#endif

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
    "MAKE SELECTION\n ON PINPAD",
    FIELD(Label)"",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },
  
  {
    DISP_RESET_ROC,
    VIM_NULL,
    FIELD(Label)"Reset RRN?\n"\
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
    "YesNo.ui",
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
    "Transaction.ui"
  },

  {
    DISP_INITISALISE_191,
    "INITIALISE 191", 
    FONT_LARGE"\nInitialise 191",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  {
    DISP_INITISALISE_192,
    "INITIALISE 192", 
    FONT_LARGE"\nInitialise 192",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  {
    DISP_INITISALISE_193,
    "INITIALISE 193", 
    FONT_LARGE"\nInitialise 193",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  {
    DISP_LOADING_CPAT,
    "LOADING CPAT\nBLOCK %3.3d", 
        FONT_LARGE"\nLoading CPAT\nBlock %3.3d",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  {
    DISP_LOADING_EPAT,
    "LOADING EPAT\nBLOCK %3.3d", 
    FONT_LARGE"\nLoading EPAT\nBlock %3.3d",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },    
  {
    DISP_LOADING_PKT,
    "LOADING PKT\nBLOCK %3.3d", 
    FONT_LARGE"\nLoading PKT\nBlock %3.3d",    
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },
  {
     DISP_LOADING_FCAT,
     "LOADING FCAT\nBLOCK %3.3d", 
     FONT_LARGE"\nLoading FCAT\nBlock %3.3d",    
     VIM_PCEFTPOS_GRAPHIC_PROCESSING
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
		"Menu2Button.ui",
        sale_other_keys
  },
  
  {
      DISP_TOKEN_LOOKUP_RESULT, 
      VIM_NULL,
      FIELD(Label)LEFT_ALIGN"Card:\n"\
      LEFT_ALIGN"%s\n"\
      LEFT_ALIGN"Card Token:\n"\
      RIGHT_ALIGN"%s\n",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "DefaultScreen1.ui",
  },
  
  {
      DISP_TOKEN_LOOKUP_PROCESSING, 
      "PROCESSING\nPLEASE WAIT",
      FIELD(Label)"Token Lookup\n\n"\
      "Processing\n\n",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },


  {
    DISP_APP_SELECT_1,
    "SELECT APPLICATION\nON PINPAD",
    //FIELD(TransactionType) " " FIELD(Amount) " "
    //FIELD(AppName) " "
    FIELD(Label) "%s"\
    FIELD(Button1) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu1Button.ui",
    cheque_keys
  },


  {
    DISP_APP_SELECT_2,
    "SELECT APPLICATION\nON PINPAD",
    //FIELD(TransactionType) " " FIELD(Amount) " "
    //FIELD(AppName) " "
    FIELD(Label) "%s"\
    FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu2Button.ui",
    cheque_savings_keys
  },

  {
    DISP_APP_SELECT_2,
    "SELECT APPLICATION\nON PINPAD",
    //FIELD(TransactionType) " " FIELD(Amount) " "
    //FIELD(AppName) " "
    FIELD(Label) "%s"\
    FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu2Button.ui",
    cheque_savings_keys
  },


  {
    DISP_APP_SELECT_3,
    "SELECT APPLICATION\nON PINPAD",
    //FIELD(TransactionType) " " FIELD(Amount) " "
	//FIELD(AppName) " "
    FIELD(Label) "%s"\
    FIELD(Button1) "%s %s"\
    FIELD(Button2) "%s %s"\
    FIELD(Button3) "%s %s",         
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "Menu3Button.ui",
    cheque_savings_credit_keys
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
    "ExtendedMenu.ui",
    app_select_keys
  },
  
  {
    DISP_CTLS_INIT,
    "CTLS READER INIT\nPLEASE WAIT",
    FIELD(Label) "CTLS Reader Init\nPlease Wait\n%s",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
    DISP_TMS_GENERAL_SCREEN,
    "%s\n%s",
    FIELD(Label) "%s\n%s"\
    FIELD(Status)"",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    IDLE_FILENAME
  },
  {
    DISP_PIN_OK,
    "PIN OK\nVERIFIED OFFLINE\n",
    FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
	FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
	FIELD(Label)"PIN OK\n",
    VIM_PCEFTPOS_GRAPHIC_FINISHED,    
    "Transaction.ui"
  },
  {
    DISP_INVALID_PIN_RETRY,
    VIM_NULL,//"INCORRECT PIN\nRETRY",
    FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
	FIELD(AppName)" "FIELD(AccountType)" "\
    FIELD(Label)"Incorrect PIN\nRetry\n",
    VIM_PCEFTPOS_GRAPHIC_PIN,
	"Transaction.ui"
  },

            // 240818 RDD JIRA WWBP-118 TCPIP PCEFT Client setup
    {
        DISP_TCP_HOSTNAME,
        VIM_NULL,
        FIELD(TransactionType) "PcEFTPOS Client IP"\
        FIELD(Label) "Current Client IP:\n%s\nKey New IP\nor <ENTER>?",
        VIM_PCEFTPOS_GRAPHIC_PROCESSING,
        "EntryPoint.ui"
    },

	{
		DISP_WIFI_SSID,
		VIM_NULL,
		FIELD(TransactionType) "WiFi SSID"\
		FIELD(Label) "Key New SSID\nor press [ENTER]",
		  VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		  "EntryPoint.ui"
	},
	{
		DISP_WIFI_PASSWORD,
		VIM_NULL,
		FIELD(TransactionType) "WiFi Password"\
		FIELD(Label) "Key New Password\nor press [ENTER]",
		  VIM_PCEFTPOS_GRAPHIC_PROCESSING,
		  "EntryPoint.ui"
	},



  {
      DISP_TCP_PORT,
      VIM_NULL,
      FIELD(TransactionType) "TCP PORT"\
      FIELD(Label) "CURRENT PORT\n%s\n\nNEW PORT?",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.ui"
  },



  // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
  {
      DISP_BCD_ENTRY,
      VIM_NULL,// "KEY BARCODE\nON PINPAD" ,
      FIELD(TransactionType)"" FIELD(Amount) ""\
      FIELD(Label) "Enter Barcode\nNumber",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.ui"
  },

  {
      DISP_PCD_ENTRY,
      "KEY PASS CODE\nON PINPAD",
      FIELD(TransactionType)"" FIELD(Amount) ""\
      FIELD(Label) "Enter Passcode\nNumber",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING,
    "EntryPoint.ui"
  },

		  {
	  DISP_OTP_ENTRY,
	  "KEY OTP\nOR <ENTER>",
	  FIELD(TransactionType)"%s" FIELD(Amount) "%s"\
	  FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
	  FIELD(Label) "Enter OTP\nor <Enter>",
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
	"Transaction.ui"
  },
  {
	DISP_PRINT_BALANCE,
	"REFER CUSTOMER TO\nPINPAD TO PRINT RCPT",
	FIELD(TransactionType)"%s" FIELD(Amount)"%s\n"\
	FIELD(AppName)" " FIELD(AccountType) " "\
	FIELD(Label)"Balance Enquiry\nPrint Receipt?\nEnter=YES CLR=NO",
	VIM_PCEFTPOS_GRAPHIC_PROCESSING,
	"Transaction.ui"
  },
  {
	DISP_GET_ANSWER,
	VIM_NULL,
	FIELD(Label)"%s",
	VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
    DISP_PRESS_ENTER_TO_CONFIRM,
    "PRESS ENTER\nON THE PINPAD",
	FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
	FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
	FIELD(Label)"Press Enter\nTo Confirm\n",
	VIM_PCEFTPOS_GRAPHIC_PIN,
    "Transaction.ui"
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
		"EntryPoint.ui"
	},


  {
    DISP_ENTER_PASSWORD,
    "%s\nPASSWORD",
    "\n"\
    RIGHT_ALIGN"\n"\
    LEFT_ALIGN"PASSWORD\n"\
    FONT_MEDIUM""RIGHT_ALIGN""ENTRY_POINT"\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
  DISP_SELECTION_APP_BLOCKED,
  "SELECTION\nBLOCKED",
  FIELD(Label)"Selection\nBlocked",
  VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
  DISP_CARD_MUST_BE_SWIPED,
  "CARD\nMUST BE SWIPED",
  FIELD(Label)"Card\nMust Be Swiped",
  VIM_PCEFTPOS_GRAPHIC_CARD
  },

  {
  DISP_CARD_ERROR_EXPIRED_DATE,
  "EXP. DATE ERROR\nCONTACT ISSUER",
  FIELD(Label)"Exp. Date Error\nContact Issuer",
  VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
    DISP_PIN_TRY_LIMIT_EXCEEDED,
    "PIN TRY LIMIT\nEXCEEDED",
	FIELD(TransactionType)"%s" FIELD(Amount) " %s\n"\
	FIELD(AppName)" " FIELD(AccountType)" "\
	FIELD(Label)"PIN Try Limit\nExceeded",
	VIM_PCEFTPOS_GRAPHIC_FINISHED,
    "Transaction.ui"
  },

  {
  DISP_TXN_NOT_ACCEPTED,
  "TRANSACTION\nNOT ACCEPTED",
  FIELD(Label)"Transaction\nNot Accepted",
  VIM_PCEFTPOS_GRAPHIC_FINISHED
  },

  {
    DISP_WOW_PROCESSING, 
    "%s\nWOW PROCESSING",
    FIELD(Label)"%s\nProcessing",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
    DISP_CARD_DATA_ERROR, 
    "CARD REJECTED\nCARD DATA ERROR",
    FIELD(Label)"Card Rejected\nCard Data Error\n%s",
    VIM_PCEFTPOS_GRAPHIC_FINISHED
  },

  {
    DISP_CANCELLED_PENDING, 
    VIM_NULL,
    FIELD(Label)"Cancelled\n%s",
    VIM_PCEFTPOS_GRAPHIC_FINISHED
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
		FIELD(Label)"Please Retry With\nAnother Card",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING
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
		"Transaction.ui"
	},

  {
		DISP_NO_REPEATS_FOUND,
		VIM_NULL,
		FIELD(Label)"NO REPEAT\nSAF FOUND",
		VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

  {
        DISP_PRESENT_ONE_CARD, 
	    "SWIPE OR INSERT\nONE CARD",
		FIELD(TransactionType)"%s" FIELD(Amount)" %s\n"\
		FIELD(AppName)" "FIELD(AccountType)" "\
		FIELD(Label)"Swipe or Insert\nOnecard",
		VIM_PCEFTPOS_GRAPHIC_CARD,
		"Transaction.ui"
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
	"EntryPoint.ui"
	},

    {
        DISP_CCV_OPT_DATA_ENTRY,
        "CVV OR ENTER\nON TERMINAL",
        FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"
        FIELD(Label)"Key CVV\nor Press Enter",
        VIM_PCEFTPOS_GRAPHIC_PIN,
        "EntryPoint.ui"
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
	FIELD(TransactionType)"%s\n"\
    FIELD(Label)"PRE-AUTH RRN?",
    VIM_PCEFTPOS_GRAPHIC_DATA,
	"EntryPoint.ui"
  },


	{
		DISP_CONTACTLESS_ONECARD,
		"PRESENT ONE CARD",
		FIELD(CTLS_Status)"%s\n"\
		FIELD(Label)"Present or swipe\nyour Onecard now",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"OneCard.ui"
	},

	{
		DISP_CONTACTLESS_DISCOUNT_CARD,
		"PRESENT DISCOUNT\nCARD",
		FIELD(CTLS_Status)\
		FIELD(Label)"Present or swipe\nLoyalty card now",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"OneCard.ui"
	},

	{
		DISP_PASS_WALLET,
		"Select and Present\nWOW Phone Pass",
		FIELD(CTLS_Status)\
		FIELD(Label)"Select and Present\nWOW Phone Pass",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"OneCard.ui"
	},

	{
		DISP_SELECT_PASS,
		"Please Select\nWOW Phone Pass",
		FIELD(CTLS_Status)\
		FIELD(Label)"Please Select\nWOW Phone Pass",
		VIM_PCEFTPOS_GRAPHIC_ALL,
		"OneCard.ui"
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
		"EntryPoint.ui"
	},

	{
    DISP_VERSIONS,
    VIM_NULL,
    FIELD(Label)"SOFTWARE\nVERSIONS:\n\n\nOS:%s\nEOS:%s\nCTLS:%s\nEFT APP:%d",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },  

    {               
        DISP_ENTER_EGC_SW,
        "SWIPE GIFTCARD OR\nKEY EGIFTCARD NUMBER",           
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Swipe Giftcard or\nKey Giftcard Number",
      VIM_PCEFTPOS_GRAPHIC_PIN,      
      "EntryPoint.ui"
    },

  {
      DISP_ENTER_EGC_SW_NO_ENTRY,
      "SWIPE GIFTCARD OR\nKEY EGIFTCARD NUMBER",
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Swipe Giftcard or\nKey Giftcard Number",
      VIM_PCEFTPOS_GRAPHIC_DATA,
      "Transaction.ui"
  },

  {
      DISP_AVALON_ALL,
      "Scan/Swipe or Enter\nGiftCard number",    // RDD 021220 as detailed in POS Spec v1.4 021220
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(Label)"Swipe Card or Enter\neGiftCard number",
      VIM_PCEFTPOS_GRAPHIC_DATA,
      "Transaction.ui"
  },


  {
      DISP_GEN_TXN,
      VIM_NULL,  
      FIELD(TransactionType)"%s " FIELD(Amount)" %s\n"\
      FIELD(AppName)"%s" FIELD(AccountType)"%s\n"\
      FIELD(Label)"%s",
      VIM_PCEFTPOS_GRAPHIC_PROCESSING,
      "Transaction.ui"
  },





  {
    DISP_SCANNING_BATCH_PLEASE_WAIT,
    "SCANNING BATCH\nPLEASE WAIT",
    "\n"\
    "SCANNING BATCH\n"\
    "PLEASE WAIT\n",
    VIM_PCEFTPOS_GRAPHIC_PROCESSING
  },

    {
    DISP_VERIFY_AUTH_ENQUIRY,
	VIM_NULL,
    "%s\n"\
    "VERIFY AUTH\n"\
    "CORRECT?\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION
  },



#if 1
    {
    DISP_YES_NO,
   VIM_NULL,
    FIELD(Label)"%s",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	"YesNo1.ui",
    yes_no_keys,
  },
#else
      
	{
    DISP_YES_NO,
   VIM_NULL,
    FIELD(Label) "%s"\
    FIELD(Button1) "YES"\
    FIELD(Button2) "NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	"YesNo.ui",
    yes_no_keys,
  },

#endif
        
	{
    DISP_YES_NO,
   VIM_NULL,
    FIELD(Label) "%s"\
    FIELD(Button1) "YES"\
    FIELD(Button2) "NO",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
	"YesNo.ui",
    yes_no_keys,
  },



      {
      DISP_TERMINAL_CONFIG_PABX_ACCESS_CODE,
    VIM_NULL,    
    "\nPABX NO:\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.ui"
  },

  {
      DISP_TERMINAL_CONFIG_TMS_TELEPHONE_NO,
    VIM_NULL,    
    "\nHOST PHONE NO:\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.ui"
  },

  {
      DISP_TMS_PACKET_LEN,
    VIM_NULL,    
    "\nTMS MAX BLOCK LEN:\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.ui"
  },

	  {
      DISP_TEST_PACKET_LEN,
    VIM_NULL,    
    "\nADD DE60 BYTES:\n",
    VIM_PCEFTPOS_GRAPHIC_QUESTION,
    "EntryPoint.ui"
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


    
    
#define arrow_width   16
#define arrow_height  11
static unsigned char arrow_bits[] = 
{
  0x0f, 0xf0,
  0x0f, 0xf0,
  0x0f, 0xf0,
  0x0f, 0xf0,
  0x7f, 0xfe,
  0x3f, 0xfc,
  0x1f, 0xf8,
  0x0f, 0xf0, 
  0x07, 0xe0,
  0x03, 0xc0,
  0x01, 0x80
};        
VIM_GRX_BITMAP arrow_bitmap = {
{ arrow_width, arrow_height},
  (arrow_width+7)/8,
  VIM_LENGTH_OF(arrow_bits),
  arrow_bits,
  VIM_NULL
};

#define logo_width   128
#define logo_height  30
static unsigned char logo_bits[] = 
{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xF7, 0xEF,
  0x8F, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xF7, 0xEF,
  0xDF, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8C,
  0xD9, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8C,
  0xD9, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8C,
  0xD9, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xF1, 0x8C,
  0xD9, 0x8C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xF1, 0x8F,
  0xD9, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8F,
  0x99, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8C,
  0x19, 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xC1, 0x8C,
  0x19, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xC1, 0x8C,
  0x1F, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0xC1, 0x8C,
  0x0F, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
VIM_GRX_BITMAP logo2_bitmap = {
{ logo_width, logo_height},
  (logo_width+7)/8,
  VIM_LENGTH_OF(logo_bits),
  logo_bits,
  VIM_NULL
};


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_initialize(void)
{  
  static VIM_GRX_FONT_NAME const font_table_64[]=
  {
    {DISPLAY_FONT_SMALL,&vim_grx_font_ascii_6x10_bold},
    {DISPLAY_FONT_MEDIUM,&vim_grx_font_ascii_6x10_bold},
    {DISPLAY_FONT_LARGE,&vim_grx_font_ascii_7x13_bold}
  };
  
  static VIM_GRX_FONT_NAME const font_table_128[]=
  {
    {DISPLAY_FONT_SMALL,&vim_grx_font_ascii_6x21_bold},
    {DISPLAY_FONT_MEDIUM,&vim_grx_font_ascii_6x21_bold},
    {DISPLAY_FONT_LARGE,&vim_grx_font_ascii_7x26_bold}
  };
  
  VIM_GRX_VECTOR screen_size;
  VIM_GRX_CANVAS_PTR canvas_ptr;

  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_open());

  /* get the canvas associated with the lcd */
  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_canvas(&canvas_ptr));

  /* get the size of the screen */
  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_size_in_pixels(&screen_size));
  /* check if the screen has a vertical height of 64 */

  use_vim_screen_functions = VIM_FALSE;

  if(screen_size.y==64)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_font_names(canvas_ptr,font_table_64,VIM_LENGTH_OF(font_table_64)));
  }
  else if (screen_size.y < 320)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_font_names(canvas_ptr,font_table_128,VIM_LENGTH_OF(font_table_128)));
  }
  else
  {
	  // Probably a colour graphical display, use new screen display functions
	  use_vim_screen_functions = VIM_TRUE;
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR display_terminal_lock(const char *text)
{
	if (use_vim_screen_functions)
	{
		return vim_screen_display_terminal("Label", text);
	}
	else
	{
	  VIM_TEXT const* font_ptr=DISPLAY_FONT_LARGE;
	  char line_buf[90];
	  char space_buf[90];
	  VIM_UINT8 i;
	  VIM_UINT8 j;
	  VIM_GRX_VECTOR screen_size;
	  VIM_GRX_VECTOR text_size;
	  VIM_GRX_VECTOR offset;
	  VIM_BOOL inverse_display = VIM_FALSE;
	  alignment_t align = align_center;
	  VIM_BOOL base_align = VIM_FALSE;
	  VIM_BOOL arrow_display = VIM_FALSE;
	  VIM_BOOL logo_display = VIM_FALSE;
	  VIM_BOOL first_align = VIM_FALSE;
	  VIM_BOOL second_align = VIM_FALSE;

	  VIM_GRX_CANVAS_PTR canvas_ptr;

	  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_canvas(&canvas_ptr));

	  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_clear(canvas_ptr));

	  vim_mem_set(space_buf, ' ', sizeof(space_buf));
	  space_buf[sizeof(space_buf) - 1] = 0;

	  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_size_in_pixels(canvas_ptr,&screen_size));

	  offset=vim_grx_vector(0,0);
      VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_font_by_name(canvas_ptr, DISPLAY_FONT_LARGE));

	  for (i=0,j=0; ; i++)
	  {
		if ((text[i] == '\n') || (text[i] == 0) || (second_align))
		{
		  if ( logo_display == VIM_TRUE )
		  {
			if( logo_picture.bitmap_ptr == VIM_NULL || logo_picture.image_length == 0)
			{
			  vim_grx_canvas_put_bitmap_at_pixel(canvas_ptr, offset, &logo2_bitmap);
			  /* Reset logo display flag */
			  logo_display = VIM_FALSE;
			  offset.y += (logo_height-4);
			}
			else
			{
			  vim_grx_canvas_put_bitmap_at_pixel(canvas_ptr, offset, &logo_picture);
			   /* Reset logo display flag */
			   logo_display = VIM_FALSE;
			   offset.y += (logo_picture.size.y-4);
			}
		  }
		  else if ( arrow_display == VIM_TRUE )
		  {
			VIM_UINT8 n = 0;
			while ( n<3 )
			{
			  offset.x = n*40+15;
			  n++;
			  vim_grx_canvas_put_bitmap_at_pixel(canvas_ptr, offset, &arrow_bitmap);
			}
			/* Reset arrow display flag */
			arrow_display = VIM_FALSE;
			offset.y += arrow_height;
		  }
		  else
		  {
			line_buf[j] = 0;
			VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_font_by_name(canvas_ptr,font_ptr));
			if (j==0)
			{ 
			  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_text_size(canvas_ptr," ",&text_size));
			}
			else  
			{
			  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_text_size(canvas_ptr,line_buf,&text_size));
			}
	        
			switch (align)
			{
			  case align_center:
				offset.x = (screen_size.x - text_size.x) / 2;
				break;
	            
			  case align_right:
				offset.x = screen_size.x - text_size.x;
				break;
	            
			  case align_left:
				offset.x = 0;
				break;
			}
			if(base_align)
			{
			  offset.y = screen_size.y - text_size.y;
			}

			VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_invert(canvas_ptr,inverse_display));

			if (inverse_display)
			{
			  /* Blackout the entire line first for inverse lines */
			  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_put_text_at_pixel(canvas_ptr,vim_grx_vector(0,offset.y),space_buf));
			}
			VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_put_text_at_pixel(canvas_ptr,offset,line_buf));
			/* Reset alignment etc. back to default */
			inverse_display = VIM_FALSE;

			offset.y += text_size.y;
		  }
	      
		  if (first_align)
		  {
			if (second_align)
			  offset.y -= text_size.y;
			else
			  font_ptr= DISPLAY_FONT_LARGE;
			first_align = VIM_FALSE;
			second_align = VIM_FALSE;
			//VIM_DBG_VAR(first_align);
			//VIM_DBG_VAR(second_align);
		  }
		  else
		  {
			font_ptr= DISPLAY_FONT_LARGE;
		  }
		  align = align_center;
		  offset.x = 0;
	      
		  j=0;
		  if (text[i] == 0)
			break;
		}
		else
		{
		  if (text[i] == ESCAPE_CHAR)
		  {
			i++;
			switch (text[i])
			{
			  case ESC_FNT_SMALL:
				font_ptr= DISPLAY_FONT_SMALL;
				break;

			  case ESC_FNT_MEDIUM:
				font_ptr= DISPLAY_FONT_MEDIUM;
				break;

			  case ESC_FNT_LARGE:
				font_ptr= DISPLAY_FONT_LARGE;

				break;

			  case ESC_ALIGN_LEFT:
				if (first_align == VIM_FALSE)
				{
				  first_align = VIM_TRUE;
				}
				else
				{
				  second_align = VIM_TRUE;
				  i -= 3;
				  //VIM_DBG_VAR(second_align);
				  continue;
				}
				align = align_left;
				break;

			  case ESC_ALIGN_RIGHT:
				if (first_align == VIM_FALSE)
				{
				  first_align = VIM_TRUE;
				}
				else
				{
				  second_align = VIM_TRUE;
				  i -= 3;
				  //VIM_DBG_VAR(second_align);
				  continue;
				}
				align = align_right;
				break;

			  case ESC_INVERSE:
				inverse_display = VIM_TRUE;
				break;

			  case ESC_ALIGN_BASE:
				base_align = VIM_TRUE;
				break;

			  case ESC_ENTRY:
				line_buf[j] = 0;
				VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_text_size(canvas_ptr,line_buf,&text_size));
				data_entry_set_cursor(text_size.y, offset.y, font_ptr, align);
				break;

			  case ESC_BITMAP_ARROW:
				arrow_display = VIM_TRUE;
				break;
	            
			  case ESC_BITMAP_LOGO:
				logo_display = VIM_TRUE;
				break;

			  case ESC_FIELD  :
				  i++;
				  while ((text[i] != '\0') && (text[i] != ESCAPE_CHAR))
				  {
					  i++;
				  }

				  continue;

			}
		  }
		  else
		  {
			line_buf[j] = text[i];
			j++;
		  }
		}
	  }

	  return VIM_ERROR_NONE;
	}
}

 

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_terminal(const char *text)
{
  VIM_ERROR_PTR error1;
  VIM_ERROR_PTR error2;
  VIM_ERROR_PTR error3;
  VIM_SEMAPHORE_TOKEN_HANDLE token_handle;
  /* lock the display */
  error1=vim_lcd_lock_update(&token_handle);
  /* generate the display */
  if(( error2=display_terminal_lock(text)) != VIM_ERROR_NONE)
  {
	  return error2;
  }
  /* unlock the display */
  error3=vim_lcd_unlock_update(token_handle.instance);
  /* catch any unahndled errors and return them to the caller*/
  VIM_ERROR_RETURN_ON_FAIL(error1);
  VIM_ERROR_RETURN_ON_FAIL(error2);
  VIM_ERROR_RETURN_ON_FAIL(error3);
  /* return without error */
  return VIM_ERROR_NONE;  
}
/*----------------------------------------------------------------------------*/


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

  get_acc_type_string_short(txn.account, account_string_short);    
  vim_mem_clear( CardName, MAX_APPLICATION_LABEL_LEN+1 );
  GetCardName( &txn, CardName );
  DBG_STRING( CardName );

  VIM_DBG_MESSAGE(" ------------ display_result -----------");
   
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

//BD				if((( txn.LedgerBalance )||( txn.ClearedBalance ))&&(is_approved(asc_error)))
				//if((( txn.LedgerBalance >= 0 )||( txn.ClearedBalance >= 0 ) || gift_card() )&&(is_approved(asc_error)))
				{
					VIM_CHAR AscLedgerBal[16], AscClearedBal[16];
#if 1
					vim_strcpy(AscLedgerBal, AmountString1(txn.LedgerBalance));  //BD Display zero balance
					vim_strcpy(AscClearedBal, AmountString1(txn.ClearedBalance)); //BD Display zero balance
					display_screen(DISP_TRANSACTION_APPROVED_BAL_CLR, pos_key_type, type, AmountString(txn.amount_total), AscLedgerBal, AscClearedBal);
#endif
					if( txn.type == tt_balance )
					{
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display( "REFER CUSTOMER TO", "PINPAD TO OBTAIN BAL", pos_key_type, pos_display_graphic ));
						pos_displayed = VIM_TRUE;
					}
					else
					{
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display( "APPROVED", "", pos_key_type, pos_display_graphic ));
						pos_displayed = VIM_TRUE;
					}
#if 0
					vim_strcpy( AscLedgerBal, AmountString1(txn.LedgerBalance) );  //BD Display zero balance
					vim_strcpy( AscClearedBal, AmountString1(txn.ClearedBalance) ); //BD Display zero balance
					display_screen( DISP_TRANSACTION_APPROVED_BAL_CLR, pos_key_type, type, AmountString(txn.amount_total), AscLedgerBal, AscClearedBal );	
#endif
					VIM_ERROR_IGNORE(vim_event_sleep(5000)); // wait 5 secs - ( RDD  220113 - allow abort but don't generate an error at this stage )
				}
				else
				{
					if(( txn.type == tt_logon ) || ( txn.type == tt_settle ) || ( txn.type == tt_presettle ) || ( txn.type == tt_query_card ))
						vim_strcpy( type, " " );
#if 1
					{
						VIM_CHAR Display[100] = { 0 };
						if ((txn.amount_surcharge) && (is_approved_txn(&txn)))
							vim_sprintf(Display, "%s\nInc Surcharge %s", error_data.terminal_line1, AmountString(txn.amount_surcharge));
						else
							vim_strcpy(Display, error_data.terminal_line1);

						VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(error_data.screen_id, "Logo", GetLogoFile(terminal.card_name[txn.card_name_index].CardNameIndex), pos_key_type, type, AmountString(txn.amount_total), CardName, account_string_short, Display));
					}
#endif

					if(( error_data.screen_id == DISP_CANCELLED )||( error_data.screen_id == DISP_CARD_REJECTED )||( error_data.screen_id == DISP_DECLINED ))
						VIM_ERROR_RETURN_ON_FAIL( pceft_pos_display(pos_line_1, pos_line_2, pos_key_type, pos_display_graphic ));
					DBG_STRING( CardName );
#if 0
					{
                        VIM_CHAR Display[100] = { 0 };
                        if (( txn.amount_surcharge)&&( is_approved_txn(&txn )))
                            vim_sprintf(Display, "%s\nInc Surcharge %s", error_data.terminal_line1, AmountString(txn.amount_surcharge));
                        else
                            vim_strcpy(Display, error_data.terminal_line1);

                        VIM_ERROR_RETURN_ON_FAIL(display_screen_cust_pcx(error_data.screen_id, "Logo", GetLogoFile(terminal.card_name[txn.card_name_index].CardNameIndex), pos_key_type, type, AmountString(txn.amount_total), CardName, account_string_short, Display ));
                    }
#endif
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

		  if( is_integrated_mode() )
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
#if 0
  if( MinDisplayTimeMs )
	pceft_pos_display_close();
#else
  // RDD 300315 SPIRA:????
  // RDD 151116 SPIRA:9088 Some display results flashing on screen
  if(( MinDisplayTimeMs ) /* && ( is_standalone_mode() */)
  {
	  vim_kbd_sound_invalid_key ();
	  VIM_ERROR_IGNORE( vim_event_sleep(MinDisplayTimeMs)); 
  }
#endif
  return VIM_ERROR_NONE;
}


VIM_BOOL IsIdleDisplay( VIM_CHAR *UIFileName )
{
	VIM_SIZE len = vim_strlen( IDLE_FILENAME );
	if( !vim_strncmp( UIFileName, IDLE_FILENAME, len ) )
		return VIM_TRUE;
	return VIM_FALSE;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_list_items
(
  VIM_TEXT const *title,
  VIM_TEXT const * const items[],
  VIM_SIZE item_count,
  VIM_SIZE item_selected,
  VIM_BOOL numbered_items
)
{
  VIM_SIZE items_per_screen;
  VIM_SIZE item_index;
  VIM_SIZE menu_offset;
  VIM_GRX_VECTOR screen;
  VIM_TEXT line_buf[600];

  VIM_ERROR_IGNORE(vim_lcd_set_font_by_name(DISPLAY_FONT_SMALL));
  vim_lcd_get_size_in_cells(&screen);
  items_per_screen = screen.y - 2;
  //VIM_DBG_VAR_ENDIAN(items_per_screen);
  menu_offset = (item_selected / items_per_screen) * items_per_screen;
  /* Display header inverted */
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf(line_buf,sizeof(line_buf),INVERSE_LINE FONT_MEDIUM "%s\n",title));
  //VIM_DBG_VAR_ENDIAN(item_selected);
  /* Display the current list of items */
  for (item_index=0; item_index < items_per_screen; item_index++)
  {
    VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf),FONT_MEDIUM LEFT_ALIGN));
    if ((item_index+menu_offset) < item_count)
    {
      if (numbered_items)
      {
        VIM_TEXT number[5];
        VIM_ERROR_RETURN_ON_FAIL(vim_snprintf(number, VIM_SIZEOF(number), "%2d ", item_index+menu_offset+1));
        VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf),number));
      }
      if ((item_index+menu_offset) == item_selected)
      {
        VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf),INVERSE_LINE));
      }
      VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf), items[item_index+menu_offset]));
    }
    VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf),"\n"));
  }
  VIM_ERROR_RETURN_ON_FAIL(vim_string_append(line_buf, sizeof(line_buf),PROMPT_BACK_CLEAR_OK_ENTER));

	if (use_vim_screen_functions)
    vim_screen_create_screen("DefaultScreen.ui", no_key_mappings);

  VIM_ERROR_RETURN_ON_FAIL(display_terminal(line_buf));
  /* display_arrow(line.y,offset.y+line.y-1); */
  return VIM_ERROR_NONE;
}



// RDD 110215 v421 
static VIM_SIZE LastScreen;
VIM_TEXT *last_image_filename;


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
  VIM_CHAR term_text[256], pos_text[100]; 
  VIM_CHAR pos_line_1[21], pos_line_2[21], line_1_length, line_2_length, pos_length;
  VIM_UINT32 screens_index;
  VIM_BOOL ErrorDisplay = VIM_FALSE;
  VIM_SIZE PosLen = 0;
  VIM_VA_LIST ap;
  

  //VIM_DBG_MESSAGE("----------- display_screen_cust_pcx --------------");

  // already displaying this screen - skip an extra display in offline contactless sales
  if((screen_id == LastScreen) && ( screen_id == DISP_PROCESSING_WAIT ))
  {
	  VIM_DBG_WARNING( "----Skip cust Screen----" );
	  return VIM_ERROR_NONE;
  }



  if( screen_id == LastScreen && last_image_filename == image_file_name )
  {
	  //VIM_DBG_WARNING("----Skip cust Screen----");
	  return VIM_ERROR_NONE;
  }


  last_image_filename = image_file_name;
  LastScreen = screen_id;

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

  pos_displayed = VIM_FALSE;

  /* Find index in screens array */
  for (screens_index=0; screens_index < ARRAY_LENGTH(screens); screens_index++)
  {
    if (screens[screens_index].screen_id == screen_id)
      break;
  }
  if (screens_index >= ARRAY_LENGTH(screens))
  {
    DBG_MESSAGE("CANT FIND SCREEN");
    DBG_VAR(screen_id);
    return VIM_ERROR_SYSTEM;
  }
  
  /* Send pos display */
  /* if the transaction happend in idle, don't display in the generic pos */

  if ((screens[screens_index].pceft_pos_text != VIM_NULL) && (get_operation_initiator() != it_pinpad) && (!pos_displayed) )
  //if ((screens[screens_index].pceft_pos_text != VIM_NULL) && (!pos_displayed))
  {
    VIM_VA_START(ap, pos_keymask);

    vim_vsnprintf(pos_text, VIM_SIZEOF(pos_text), screens[screens_index].pceft_pos_text, ap);
	VIM_VA_END(ap);
	 
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
    vim_mem_copy(pos_line_2, &pos_text[line_1_length+1], MIN(line_2_length, 20));

	// RDD 080212 - Ensure that only POS initiated txns send data to the POS
	 
	if(( vim_strlen(pos_line_1) || vim_strlen(pos_line_2) ) && ( terminal.initator == it_pceftpos ))
	//if(( vim_strlen(pos_line_1) || vim_strlen(pos_line_2) ))
	{
		VIM_ERROR_PTR res;

		// RDD added 171011 for PCEFT Cancel Key Support
		if(( res = pceft_pos_display(pos_line_1, pos_line_2, pos_keymask, screens[screens_index].graphic )) != VIM_ERROR_NONE )
		{	
			  DBG_MESSAGE( "****** PcEft Display Error ******");
		    // RDD 151013 SPIRA 6957: NZ v418 Pilot issue - PinPad - PcEft Client COMMS died due to Offline Pin Issue
			// RDD 251013 - appears to crash the PinPad if cancel key pressed
			//res = pceft_settings_reconfigure( VIM_TRUE );
		}
		VIM_ERROR_RETURN_ON_FAIL( res );
	}
  }

  /* Do terminal display */
  terminal_text = screens[screens_index].terminal_text;
  if (!use_vim_screen_functions && (screens[screens_index].optional_traditional_terminal_text != VIM_NULL))
  {
	  terminal_text = screens[screens_index].optional_traditional_terminal_text;
  }

  if (terminal_text != VIM_NULL)
  {

    VIM_VA_START(ap, pos_keymask);

	VIM_ERROR_RETURN_ON_FAIL(vim_vsnprintf(term_text, sizeof(term_text), terminal_text, ap));
    VIM_VA_END(ap);

	if (use_vim_screen_functions)
	{
		if (screens[screens_index].screen_file != 0)
		{
			// RDD 140312 added to return error if screen file specified but doesn't exist
			VIM_ERROR_PTR res = VIM_ERROR_NONE;
			if(( res = vim_screen_create_screen(screens[screens_index].screen_file, screens[screens_index].keys)) != VIM_ERROR_NONE )
				return res;
		}
		else
		{
			vim_screen_create_screen("DefaultScreen.ui", no_key_mappings);
		}
	}
    
	//VIM_DBG_PRINTF2( "%s : %s", controlName, image_file_name );
	VIM_ERROR_WARN_ON_FAIL( vim_screen_set_control_image( controlName, image_file_name ));
    display_terminal(term_text);

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
					 VIM_DBG_MESSAGE("dkm1");
					 VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK));				   
				 }
			 }
			 DBG_MESSAGE( "!!!! WAIT 2 SECONDS !!!!" );
			 pceft_pos_wait_key( &key, TWO_SECOND_DELAY );
		 }
	}
  }
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
  const VIM_CHAR *terminal_text;
  VIM_CHAR term_text[512], pos_text[100]; 
  VIM_CHAR pos_line_1[21], pos_line_2[21], line_1_length, line_2_length, pos_length;
  VIM_UINT32 screens_index;
  VIM_BOOL ErrorDisplay = VIM_FALSE;
  VIM_SIZE PosLen = 0;
  VIM_VA_LIST ap;
  
  // RDD 110215 v421 
  //static VIM_SIZE LastScreen;
 // VIM_DBG_MESSAGE("----------- display_screen --------------");


  // already displaying this screen - skip an extra display in offline contactless sales
  if(( screen_id == LastScreen ) && ( screen_id == DISP_PROCESSING_WAIT ) && ( IS_INTEGRATED ))
  {
	  VIM_DBG_WARNING( "----Skip Screen----" );
	  return VIM_ERROR_NONE;
  }
  LastScreen = screen_id;

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

  pos_displayed = VIM_FALSE;

  /* Find index in screens array */
  for (screens_index=0; screens_index < ARRAY_LENGTH(screens); screens_index++)
  {
    if (screens[screens_index].screen_id == screen_id)
      break;
  }
  if (screens_index >= ARRAY_LENGTH(screens))
  {
    //DBG_MESSAGE("CANT FIND SCREEN");
    //DBG_VAR(screen_id);
    return VIM_ERROR_SYSTEM;
  }
  
  /* Send pos display */
  /* if the transaction happend in idle, don't display in the generic pos */

  if ((screens[screens_index].pceft_pos_text != VIM_NULL) && (get_operation_initiator() != it_pinpad) && (!pos_displayed) )
  //if ((screens[screens_index].pceft_pos_text != VIM_NULL) && (!pos_displayed))
  {
    VIM_VA_START(ap, pos_keymask);
    vim_vsnprintf(pos_text, VIM_SIZEOF(pos_text), screens[screens_index].pceft_pos_text, ap);
	VIM_VA_END(ap);
	 
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
    vim_mem_copy(pos_line_2, &pos_text[line_1_length+1], MIN(line_2_length, 20));

	// RDD 080212 - Ensure that only POS initiated txns send data to the POS	 
	if(( vim_strlen(pos_line_1) || vim_strlen(pos_line_2)) && ( terminal.initator == it_pceftpos ))
	{
        pceft_pos_display(pos_line_1, pos_line_2, pos_keymask, screens[screens_index].graphic);
	}
  }

  // RDD 141020 JIRA PIRPIN-891 Hack to fix OpenPay cancel issue
  else if (pos_keymask)
  {
	  VIM_DBG_MESSAGE("dkm3");
      vim_pceftpos_set_keymask(pceftpos_handle.instance, pos_keymask);
  }

  /* Do terminal display */
  terminal_text = screens[screens_index].terminal_text;
  if (!use_vim_screen_functions && (screens[screens_index].optional_traditional_terminal_text != VIM_NULL))
  {
	  terminal_text = screens[screens_index].optional_traditional_terminal_text;
  }

  if (terminal_text != VIM_NULL)
  {

	VIM_VA_START(ap, pos_keymask);
	VIM_ERROR_RETURN_ON_FAIL(vim_vsnprintf(term_text, sizeof(term_text), terminal_text, ap));
    VIM_VA_END(ap);

	if (use_vim_screen_functions)
	{
		if (screens[screens_index].screen_file != 0)
		{
			// RDD 140312 added to return error if screen file specified but doesn't exist
			VIM_ERROR_PTR res = VIM_ERROR_NONE;
			if(( res = vim_screen_create_screen(screens[screens_index].screen_file, screens[screens_index].keys)) != VIM_ERROR_NONE )
				return res;
		}
		else
		{
			vim_screen_create_screen("DefaultScreen.ui", no_key_mappings);
		}
	}
    
    display_terminal(term_text);

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
         VIM_DBG_VAR( terminal.initator );
	
		 // If there was an error then beep immediatly THEN do the wait KEY
		 if(( ErrorDisplay ) || ( txn.error == VIM_ERROR_POS_CANCEL ) || ( txn.error == VIM_ERROR_USER_CANCEL ))
		 {
			 if( !ErrorDisplay )	
			 {				  
				 if( IS_INTEGRATED )				  
				 {	
					 VIM_DBG_MESSAGE("dkm2");
					 VIM_ERROR_RETURN_ON_FAIL(vim_pceftpos_set_keymask(pceftpos_handle.instance, VIM_PCEFTPOS_KEY_OK));				   
				 }
			 }
			 DBG_MESSAGE( "!!!! WAIT 2 SECONDS !!!!" );
			 pceft_pos_wait_key( &key, TWO_SECOND_DELAY );
		 }
	}
  }
  return VIM_ERROR_NONE;
}




/*----------------------------------------------------------------------------*/
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
  VIM_SIZE entered_val;
  VIM_KBD_CODE key_1;
  VIM_KBD_CODE key_2; 
  /* check if the caller supplied a pointer for outputing the selected item */
  if(selected_ptr==VIM_NULL)
  {
    /* make sure selected_ptr points to something valid */
    selected_ptr=&item_index;
  } 

  while (1)
  {
    VIM_ERROR_PTR result;
    VIM_SEMAPHORE_TOKEN_HANDLE lcd_lock;
    /* lock the display until the update is complete */
    result = vim_lcd_lock_update(&lcd_lock);
    /* make sure there were no errors while locking the display */
    if (result !=VIM_ERROR_NONE)
    {
      /* check if the lock needs to be released */
      if (lcd_lock.instance!=VIM_NULL)
      {
        /* release the lock */
        VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));    
      }
      /* return the error from the lcd lock */
      VIM_ERROR_RETURN(result);
    }
    /* display a menu */
    result=display_list_items(title,items, count,item_index,numbered_items);
    /* check for errors while displaying the menu */
    if (result !=VIM_ERROR_NONE)
    {
      /* unlock the lcd */
      VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));    
      /* return the error from the menu display call */
      VIM_ERROR_RETURN(result);
    }
    /* unlock the display */
    VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));    

    /* Wait for a key, up or down moves item_index up or down, numbers select an item */

    VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( &key_1,timeout, VIM_TRUE ));

    switch (key_1)
    {
      case VIM_KBD_CODE_KEY0:
        if (secret_item)
        {
           return ERR_SECRET_ITEM_SELECTED;
        }
        /*vim_kbd_invalid_key_sound();*/
        vim_kbd_sound_invalid_key();
        break;

      case VIM_KBD_CODE_KEY1:
      case VIM_KBD_CODE_KEY2:
      case VIM_KBD_CODE_KEY3:
      case VIM_KBD_CODE_KEY4:
      case VIM_KBD_CODE_KEY5:
      case VIM_KBD_CODE_KEY6:
      case VIM_KBD_CODE_KEY7:
      case VIM_KBD_CODE_KEY8:
      case VIM_KBD_CODE_KEY9:
        entered_val = (key_1 - VIM_KBD_CODE_KEY0);
        if ((entered_val * 10) < count) /* Check if we need to wait for a second key */
        {
          /* Wait 600ms for a second key */					
		  VIM_ERROR_RETURN_ON_FAIL( screen_kbd_touch_read_with_abort( &key_2, 60, VIM_FALSE, VIM_TRUE ));
          //VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( &key_2, 60, VIM_TRUE ));

          if ((key_2 >= VIM_KBD_CODE_KEY0) && (key_2 <= VIM_KBD_CODE_KEY9))
          {
            entered_val = (key_1 - VIM_KBD_CODE_KEY0)*10 + (key_2 - VIM_KBD_CODE_KEY0); 
          }
        }
        else
        {
          entered_val = (key_1 - VIM_KBD_CODE_KEY0);
        }
        entered_val--; /* reduce by 1 to line up with array indexes */
        if (entered_val < count)
        {
          //DBG_VAR(entered_val);
          *selected_ptr=entered_val;
          return VIM_ERROR_NONE;
        }
        /*vim_kbd_invalid_key_sound();*/
        //vim_kbd_sound_invalid_key();
        break;

      case VIM_KBD_CODE_OK: /* Enter key */
        *selected_ptr=item_index;
        return VIM_ERROR_NONE;

      case VIM_KBD_CODE_SCROLLUP:
      case VIM_KBD_CODE_SOFT2:  
        /* decrement the item index or loop around to the highest index*/
        item_index =(item_index+count-1)%count;
        break;

      case VIM_KBD_CODE_SCROLLDOWN:
      case VIM_KBD_CODE_SOFT1:
        /* increment the item index or loop around to the lowest index*/
        item_index=(item_index+1)%count;
        break;

      case VIM_KBD_CODE_CANCEL:
        VIM_ERROR_RETURN(VIM_ERROR_USER_CANCEL);

      case VIM_KBD_CODE_CLR:
        VIM_ERROR_RETURN(VIM_ERROR_USER_CANCEL);

      default:
        /*vim_kbd_invalid_key_sound();*/
        vim_kbd_sound_invalid_key();
        break;
    }
  }
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
    VIM_ERROR_RETURN_ON_FAIL( vim_screen_kbd_or_touch_read( &key_code,timeout,VIM_TRUE ));
    //VIM_DBG_DATA(&key_code, sizeof(key_code));
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

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR display_menu_select
(
  VIM_TEXT const *title,
  VIM_SIZE* item_index,
  VIM_SIZE* selected_ptr,
  VIM_TEXT *items[],
  VIM_SIZE count,
  VIM_MILLISECONDS timeout,
  VIM_BOOL secret_item,
  VIM_BOOL numbered_items,
  VIM_BOOL allow_exit,
  VIM_BOOL CheckCard
)
{
  VIM_BOOL valid_button_press = 0;
  VIM_SIZE num_buttons; /* a check in case there are < 3 buttons to display*/
  VIM_SIZE entered_val;
  VIM_KBD_CODE key_1;
  VIM_CHAR num1[8];
  VIM_CHAR num2[8];  
  VIM_CHAR num3[8];
  VIM_SIZE index1;

  //vim_tch_flush();
  
  /* check if the caller supplied a pointer for outputing the selected item */
  if(selected_ptr==VIM_NULL)
  {
    /* make sure selected_ptr points to something valid */
    selected_ptr=item_index;
  } 

  
  while (1)
  {
    VIM_ERROR_PTR result;
    VIM_SEMAPHORE_TOKEN_HANDLE lcd_lock;
    /* lock the display until the update is complete */
    result = vim_lcd_lock_update(&lcd_lock);
    /* make sure there were no errors while locking the display */
    if (result !=VIM_ERROR_NONE)
    {
      /* check if the lock needs to be released */
      if (lcd_lock.instance!=VIM_NULL)
      {
        /* release the lock */
        VIM_ERROR_RETURN_ON_FAIL(vim_lcd_unlock_update(lcd_lock.instance));    
      }
      /* return the error from the lcd lock */
      VIM_ERROR_RETURN(result);
    }

	
	// Read the buffer to get rid of buffered presses

	//while( screen_kbd_or_touch_read( &key_1, 20, VIM_TRUE, VIM_FALSE ) != VIM_ERROR_TIMEOUT );
	if (!count)
	{
		count = 1;
	}


    /* display a menu and record number of valid buttons  */
    if (count == 1) 
    {
      num_buttons = 1;
	  if( numbered_items )
		result=display_screen( DISP_APP_SELECT_1, VIM_PCEFTPOS_KEY_CANCEL, title, "1", items[0] );
	  else
		result=display_screen( DISP_APP_SELECT_1, VIM_PCEFTPOS_KEY_CANCEL, title, "", items[0] );
    }
    else if (count == 2)
    {
      num_buttons = 2;
	  if( numbered_items )
		result=display_screen( DISP_APP_SELECT_2, VIM_PCEFTPOS_KEY_CANCEL, title, "1", items[0], "2", items[1] );
	  else
		result=display_screen( DISP_APP_SELECT_2, VIM_PCEFTPOS_KEY_CANCEL, title, "", items[0], "", items[1] );
    }
    else if (count == 3)
    {
        num_buttons = 3;
        if (numbered_items)
            result = display_screen(DISP_APP_SELECT_3, VIM_PCEFTPOS_KEY_CANCEL, title, "1", items[0], "2", items[1], "3", items[2]);
        else
            result = display_screen(DISP_APP_SELECT_3, VIM_PCEFTPOS_KEY_CANCEL, title, "", items[0], "", items[1], "", items[2]);
    }
    else
    {
      num_buttons = 3;
	  index1 = (*item_index)+1;
	  vim_sprintf(num1, "%d", index1);
	  ++index1;
	  if (index1 > count)
	  	index1 = 1;
	  vim_sprintf(num2, "%d", index1);
	  ++index1;
	  if (index1 > count)
	  	index1 = 1;
	  // RDD 060814 - display 0 instead of 10
	  if( index1 == 10 )
		  vim_strcpy( num3, "0" );
	  else		
		  vim_sprintf(num3, "%d", index1);

	  if( numbered_items )
	  {
		  result=display_screen( DISP_APP_SELECT, VIM_PCEFTPOS_KEY_CANCEL, title, &num1, items[*item_index], &num2, items[((*item_index)+1)%count], &num3, items[((*item_index)+2)%count] );
	  }
	  
      else
      {
          result = display_screen( DISP_APP_SELECT, VIM_PCEFTPOS_KEY_CANCEL, title, "", items[*item_index], "", items[((*item_index) + 1) % count], "", items[((*item_index) + 2) % count]);
      }
    }

    /* check for errors while displaying the menu */
    if( result !=VIM_ERROR_NONE )
    {      
		/* unlock the lcd */      
		VIM_ERROR_RETURN_ON_FAIL( vim_lcd_unlock_update(lcd_lock.instance) );    
      
		/* return the error from the menu display call */      
		VIM_ERROR_RETURN(result);
    }
    /* unlock the display */
    VIM_ERROR_RETURN_ON_FAIL( vim_lcd_unlock_update(lcd_lock.instance) );    

    /* Wait for a key, up or down moves item_index up or down, numbers 1-3 select an item*/
   
	//timeout set here to ensure beep every 30 seconds otherwise it hangs in event wait for too long
	//result = screen_kbd_touch_read_with_abort(&key_1, 7000, VIM_TRUE );				

	// RDD 010512 SPIRA 5019 Use the function that I wrote to time slice card removal checking with mini-timeouts
	// RDD 220312 SPIRA 5159 Don't do invalid key bleep if valid selection - only at timeout intervals

	////////// RDD TEST TEST TEST - need to make this generate an error for APP Seletion but need to fix Card Removed fn for this so it only tests card removal not card presence
	

	if( CheckCard )
	{
		result = screen_kbd_touch_read_with_abort( &key_1, timeout, VIM_FALSE, VIM_TRUE );
	}
	else
	{
		result = vim_screen_kbd_or_touch_read( &key_1, timeout, VIM_TRUE );
	}

	if( result == VIM_ERROR_TIMEOUT )
	{
  	    //result=display_screen(DISP_APP_SELECT, VIM_PCEFTPOS_KEY_CANCEL, &num1, items[item_index], &num2, items[(item_index+1)%count], &num3, items[(item_index+2)%count]);
		// RDD 070912 SPIRA:6024 
		// RDD - need to exit if called from main menu so that clock can be updated. Fn will be called again until selection is made
		if( secret_item )
		{
			//DBG_POINT;
			return result;		
		}
		ERROR_TONE;
		continue;
	}

	if (result != VIM_ERROR_NONE && result != VIM_ERROR_TIMEOUT)
	{
		VIM_DBG_ERROR( result );
		return result;
	}
	
	VIM_DBG_KBD_CODE(key_1);
    switch (key_1)
    {
      case VIM_KBD_CODE_KEY1:
	  case VIM_KBD_CODE_KEY2:
	  case VIM_KBD_CODE_KEY3:
	  case VIM_KBD_CODE_KEY4:
	  case VIM_KBD_CODE_KEY5:
	  case VIM_KBD_CODE_KEY6:
	  case VIM_KBD_CODE_KEY7:
	  case VIM_KBD_CODE_KEY8:
	  case VIM_KBD_CODE_KEY9:
		entered_val = key_1 - VIM_KBD_CODE_KEY0;
		if (entered_val <= count)
		{
			*selected_ptr = --entered_val;
			return VIM_ERROR_NONE;
		}
		break;
		
	  case VIM_KBD_CODE_KEY0:
		  
#if 0
		// RDD 140616 SPIRA:8994 - zero not correctly handled for App selection
		if(( count < 10 ) && ( numbered_items ))
		{
			//ERROR_TONE;		
			continue;
		}
		*selected_ptr = 10;	// 10th Item is ArrayElement[9]		  		
		return VIM_ERROR_NONE;
#else
			*selected_ptr = 10;	// 10th Item is ArrayElement[9]		  
			return VIM_ERROR_NONE;
#endif

      case VIM_KBD_CODE_RFUNC0: /*previous*/
	  	//VIM_ERROR_IGNORE(vim_kbd_sound());
	  //case VIM_KBD_CODE_FEED:
        /* decrement the item index or loop around to the highest index*/
        *item_index =((*item_index)+count-3)%(count);
		KBD_CLICK;
        //vim_kbd_sound();
        break;
      
      case VIM_KBD_CODE_SOFT1:
		  KBD_CLICK;
	  	//VIM_ERROR_IGNORE(vim_kbd_sound());
	  	key_1 = VIM_KBD_CODE_KEY1;
      //vim_kbd_sound();
        valid_button_press = 1;
        break;
      
      case VIM_KBD_CODE_SOFT2:
		  KBD_CLICK;
        if (num_buttons >= 2)
        {
          //VIM_ERROR_IGNORE(vim_kbd_sound());
		  key_1 = VIM_KBD_CODE_KEY2;
      //vim_kbd_sound();
          valid_button_press = 1;
        }
		break;
      
      case VIM_KBD_CODE_SOFT3:
		  KBD_CLICK;
        if (num_buttons == 3)
        {
		  //VIM_ERROR_IGNORE(vim_kbd_sound());	
		  key_1 = VIM_KBD_CODE_KEY3;
        //vim_kbd_sound();
          valid_button_press = 1;
        }
        break;

	  
      case VIM_KBD_CODE_RFUNC1: /*next*/
		//VIM_ERROR_IGNORE(vim_kbd_sound());  
	  //case VIM_KBD_CODE_FUNC: - RDD - get this code from pressing the # key on 820
        /* increment the item index or loop around to the lowest index*/
        *item_index=((*item_index)+3)%(count);
		KBD_CLICK;
        //vim_kbd_sound();
        break;

      case VIM_KBD_CODE_CANCEL:
		if( allow_exit )
			return VIM_ERROR_USER_CANCEL;
	  	ERROR_TONE;
		break;

      case VIM_KBD_CODE_CLR:
		if( allow_exit )
			return VIM_ERROR_USER_BACK;
	  	ERROR_TONE;
		break;

	  case VIM_KBD_CODE_FUNC:
	  case VIM_KBD_CODE_HASH:
		  if( secret_item )
		  {
			  DBG_MESSAGE( "-------- HASH KEY PRESSED --------" );
			  config_functions(0);
			  return VIM_ERROR_NOT_FOUND;
		  }

      default:
        break;
    }
    if (valid_button_press)
    {
      entered_val = ((key_1 - VIM_KBD_CODE_KEY0) + *item_index)%count;
      if (entered_val == 0)
      	entered_val = count;
      *selected_ptr = --entered_val;
      return VIM_ERROR_NONE;
    }
  }
}



static void MenuInitItemList( VIM_CHAR **ItemPtr, S_MENU *MenuPtr, VIM_SIZE MenuSize )
{
	VIM_SIZE Item;
	for( Item=0; Item< MenuSize; Item++, MenuPtr++ )
		*ItemPtr++ = (VIM_CHAR *)&MenuPtr->MenuItem;
}




VIM_ERROR_PTR vim_adkgui_pinentry_underflow_cb()
{
    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR MenuRun( S_MENU *MenuList, VIM_SIZE MenuLen, const VIM_CHAR *MenuTitle, VIM_BOOL SecretItem, VIM_BOOL NumberedItems )
{		
	VIM_CHAR *items[ MAX_MENU_SIZE ];
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
	VIM_SIZE ItemIndex = 0;		
	VIM_SIZE SelectedIndex = 0;

	if( MenuLen > MAX_MENU_SIZE ) return VIM_ERROR_OVERFLOW;

	MenuInitItemList( (VIM_CHAR **)items, MenuList, MenuLen );	
	VIM_ERROR_RETURN_ON_FAIL( display_menu_select( MenuTitle, &ItemIndex, &SelectedIndex, (VIM_TEXT **)items, MenuLen, ENTRY_TIMEOUT, SecretItem, NumberedItems, VIM_TRUE, VIM_FALSE ));
	// RDD 130215 SPIRA:8388 - crash if 0 selected and less than 10 menu items...
		
	if( SelectedIndex <= MenuLen )						
		VIM_ERROR_RETURN_ON_FAIL( MenuList[SelectedIndex].MenuFunction() );			
	return VIM_ERROR_NONE;
}




void display_get_image_format(VIM_CHAR *format, VIM_UINT8 *len) {
#ifdef _VOS2
    {
        *len = VIM_SIZEOF(IMAGE_FORMAT_ADK) - 1;
        vim_mem_copy(format, IMAGE_FORMAT_ADK, *len);
    }
#else
    {
        *len = VIM_SIZEOF(IMAGE_FORMAT_VERIX) - 1;
        vim_mem_copy(format, IMAGE_FORMAT_VERIX, *len);
    }
#endif
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

    VIM_VA_START(ap, pos_keymask);
    VIM_ERROR_RETURN_ON_FAIL( display_screen( screen_id, pos_keymask, ap ));
    VIM_VA_END(ap);

    VIM_ERROR_RETURN_ON_FAIL( data_entry(entry_buf, entry_opts));
    return VIM_ERROR_NONE;
}




VIM_ERROR_PTR display_callback_text(VIM_CHAR *TextBuffer, VIM_BOOL DataEntry )
{
    if (!DataEntry)
    {
        return display_screen( DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, TextBuffer, ""  );
    }
    else
    {
        return display_screen( DISP_GENERIC_ENTRY_POINT, VIM_PCEFTPOS_KEY_NONE, "", TextBuffer );
    }
}



