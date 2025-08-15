#define ADK_COM

#include <string>
#include <vector>

#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>

#include <svc.h>
#include <time.h>

#include <adk_com.h>

#include <unistd.h>
#include <html/gui.h>	// ADK DirectGUI
#include <com/libcom.h> // ADK Communication
#include <sysinfo/sysinfo.h> // ADK Log
#include <expat/expat.h>

#include <log/liblog2.h> // ADK logging

#include <vim.h>
#include <incs.h>

//extern "C" VIM_ERROR_PTR WifiRunFromParams(void);

void logInit(void);
//==============================================================================
// DEFINES
//==============================================================================


#define NWIF_DEFAULT_HANDLE 1811

typedef struct VIM_NET_INTERFACE
{
    int os_handle;
    com_ConnectHandle *adkcom_handle_ptr;      // for ref. only as contents are hidden from App layer
    VIM_UINT8 ip_address[4];		        
    VIM_UINT16 port;
    VIM_BOOL UseUDP;
    VIM_BOOL socket_error_closed;
} VIM_NET_INTERFACE;

VIM_NET_INTERFACE conn1;

static int nwif_handle = NWIF_DEFAULT_HANDLE;

static VIM_BOOL ppp_connected;

#define APPNAME                         "82000"
 LibLogHandle LogHandle;
//VIM_CHAR ErrMsg[100] = { 0 };


#define CFG_NET_PATH                    "flash/"
#define CFG_NET_PPP_PROF_NAME           "PPP_NET.xml"


#define FILENAMESIZE 40



//==============================================================================
// STRUCT
//==============================================================================

struct comData
{
    char netProfile[FILENAMESIZE];
    char ethbtProfile[FILENAMESIZE];
    char rs232btProfile[FILENAMESIZE];
    char usbbtProfile[FILENAMESIZE];
    char tcp4gProfile[FILENAMESIZE];
};

struct btDevList
{
    char addr[20];
    char name[64];
};

//==============================================================================
// GLOBALS
//==============================================================================

static struct comData g_comData;

struct btDevList *g_pBTDevList = NULL;
int g_iBTDevMax = 0;
char g_btName[64];

//==============================================================================
//==============================================================================
// FUNCTIONS


using namespace std;
using namespace vfigui;
using namespace vfisysinfo;

void guiAlert(const char *cpTitle, const char *cpMsg1, const char *cpMsg2, const char *cpMsg3, int iWaitSec)
{
    map<string, string> value;
    int rc;

    value["title"] = cpTitle;
    if (cpMsg1)
        value["msg1"] = cpMsg1;
    if (cpMsg2)
        value["msg2"] = cpMsg2;
    if (cpMsg3)
        value["msg3"] = cpMsg3;

    rc = uiInvokeURL(value, "alarm.html");
    LOGF_INFO(LogHandle, "uiInvokeURL(alarm.html) rc=%d", rc);

    if (iWaitSec > 0)
        sleep(iWaitSec);
}

//==============================================================================
// 
//==============================================================================
VIM_ERROR_PTR netAttach( VIM_CHAR *ErrorMsg )
{
    //const char *cpTitle = "Net Attach";
    //char msg[32];
    int count = 0;
    enum com_ErrorCodes iComErrno;
    int rc;

    //guiAlert(cpTitle, "Network Profile:", g_comData.netProfile + 11, NULL, 0);
    LOGF_INFO(LogHandle, "netAttach");

    // Get current status to check if already attached
    iComErrno = COM_ERR_NONE;
    rc = com_GetNetworkStatus(g_comData.netProfile, &iComErrno);
    LOGF_INFO(LogHandle,  "com_GetNetworkStatus rc=%d err=%s", rc, strerror(iComErrno));
  
	{
		// Perform attach network
		iComErrno = COM_ERR_NONE;
        //com_DetachNetwork(g_comData.netProfile, &iComErrno);
		rc = com_AttachNetwork(g_comData.netProfile, &iComErrno);
        LOGF_INFO(LogHandle, "g_comData.netProfile = %s", g_comData.netProfile);
		LOGF_INFO(LogHandle, "com_AttachNetwork rc=%d err=%d", rc, iComErrno);
		LOGF_INFO(LogHandle,  "CCCCCCCCCCCCCCCCCCCCCCC com_AttachNetwork rc=%d err=%d CCCCCCCCCCCCCCCCCCC", rc, iComErrno);

		// Get Network status until no error or timeout
		do
		{
			rc = com_GetNetworkStatus(g_comData.netProfile, &iComErrno);
			LOGF_INFO(LogHandle,  "com_GetNetworkStatus rc=%d err=%d", rc, iComErrno);
			// RDD : iComErrno should be zero if no error and rc determines status
			if (iComErrno)
			{
				VIM_DBG_STRING(com_GetErrorString(iComErrno));
				vim_strcpy(ErrorMsg, com_GetErrorString(iComErrno));
				VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
			}
			usleep(100000);
		} while ((++count < 3) && (rc != COM_NETWORK_STATUS_START));

	}

    return (rc == COM_NETWORK_STATUS_START) ? VIM_ERROR_NONE : VIM_ERROR_TIMEOUT;
}

//==============================================================================
// 
//==============================================================================
int netDetach(void)
{
    //const char *cpTitle = "Net Detach";
    char msg[32];
    enum com_ErrorCodes iComErrno;
    int rc;

    //guiAlert(cpTitle, "Network Profile:", g_comData.netProfile + 11, NULL, 0);

    // Get current status to check if already detached
    iComErrno = COM_ERR_NONE;
    rc = com_GetNetworkStatus(g_comData.netProfile, &iComErrno);
    LOGF_INFO(LogHandle, "com_GetNetworkStatus rc=%d err=%d", rc, iComErrno);

    if (rc == COM_NETWORK_STATUS_STOP)
    {
        sprintf(msg, "com_GetNetworkStatus() %d", rc);
        //guiAlert(cpTitle, msg, "Already disconnected", NULL, 2);
        return 0;
    }

    // Perform detach
    iComErrno = COM_ERR_NONE;
    rc = com_DetachNetwork(g_comData.netProfile, &iComErrno);
    LOGF_INFO(LogHandle,"com_DetachNetwork rc=%d err=%d", rc, iComErrno);

    sprintf(msg, "com_DetachNetwork() %d", rc);
    //guiAlert(cpTitle, msg, com_GetErrorString(iComErrno), NULL, 2);

    return 0;
}

//==============================================================================
// 
//==============================================================================
int btGetDevList(char *cpTitle, int bScan)
{
    vfiipc::JSObject jsObj;
    char cpData[8192];
    char msg[32];
    char cpElem[20];
    enum com_ErrorCodes iComErrno;
    int i, rc;

    memset(cpData, 0, sizeof(cpData));

    if (bScan)
    {
       // guiAlert(cpTitle, "Searching for", "Devices...", NULL, 0);

        // Perform Bluetooth scan
        rc = com_WirelessScan(COM_WIRELESS_TYPE_BT, cpData, sizeof(cpData), &iComErrno);
        LOGF_INFO(LogHandle, "com_WirelessScan() rc=%d err=%d", rc, iComErrno);

        sprintf(msg, "com_WirelessScan() %d", rc);
        //guiAlert(cpTitle, msg, com_GetErrorString(iComErrno), NULL, 1);
    }
    else
    {
       // guiAlert(cpTitle, "Getting Paired", "Devices...", NULL, 0);

        // Get list of paired devices
        rc = com_GetDevicePropertyString(COM_PROP_BT_PAIRING_STATUS, cpData, sizeof(cpData), &iComErrno);
        LOGF_INFO(LogHandle, "BT_PAIRING_STATUS rc=%d err=%d", rc, iComErrno);

        sprintf(msg, "Get(BT_PAIRING_STATUS) %d", rc);
        //guiAlert(cpTitle, msg, com_GetErrorString(iComErrno), NULL, 1);
    }

    rc = jsObj.load(std::string(cpData));
    LOGF_INFO(LogHandle, "jsObj.load rc=%d", rc);
    if (rc == 0)
        return -1;

    rc = jsObj.exists(COM_BT_COUNT);
    LOGF_INFO(LogHandle, "jsObj.exists(COM_BT_COUNT) rc=%d", rc);
    if (rc == 0)
        return -1;
    g_iBTDevMax = jsObj(COM_BT_COUNT).getInt();
    LOGF_INFO(LogHandle, "COM_BT_COUNT=%d", g_iBTDevMax);

    // If no devices found
    if (g_iBTDevMax == 0)
    {
        //guiAlert(cpTitle, "No Devices", "Found", NULL, 2);
        return -1;
    }

    g_pBTDevList = new struct btDevList[g_iBTDevMax];
    if (g_pBTDevList == NULL)
    {
        sprintf(msg, "Error malloc() %d", errno);
        //guiAlert(cpTitle, msg, NULL, NULL, 2);
        return -1;
    }

    for (i = 0; i < g_iBTDevMax; i++)
    {
        sprintf(cpElem, "BT%d_ADDR", i);
        if (jsObj.exists(cpElem))
        {
            strcpy(g_pBTDevList[i].addr, jsObj(cpElem).getStringP()->data());
            LOGF_INFO(LogHandle, "[%s][%s]", cpElem, g_pBTDevList[i].addr);
        }

        sprintf(cpElem, "BT%d_NAME", i);
        if (jsObj.exists(cpElem))
        {
            strcpy(g_pBTDevList[i].name, jsObj(cpElem).getStringP()->data());
            LOGF_INFO(LogHandle, "[%s][%s]", cpElem, g_pBTDevList[i].name);
        }
    }

    return 0;
}

//==============================================================================
// 
//==============================================================================
int guiConfirmPair(char *cpTitle, int idx)
{
    map<string, string> value;
    int rc;

    value["title"] = cpTitle;
    value["name"] = g_pBTDevList[idx].name;
    value["addr"] = g_pBTDevList[idx].addr;

    // Display confirm pair URL
    rc = uiInvokeURL(value, "confirm-pair.html");
    LOGF_INFO(LogHandle, "uiInvokeURL(confirm-pair.html) rc=%d", rc);

    if (rc == 100)
        return 0;
    else
        return -1;
}

//==============================================================================
// 
//==============================================================================
int guiDevListMenu(char *cpTitle)
{
    static struct UIMenuEntry *pMenu = NULL;
    char msg[32];
    int iOption;
    int i;

    if (pMenu == NULL)
    {
        pMenu = new struct UIMenuEntry[g_iBTDevMax];
        if (pMenu == NULL)
        {
            sprintf(msg, "Error malloc() %d", errno);
            //guiAlert(cpTitle, msg, NULL, NULL, 2);
            return -1;
        }
    }

    for (i = 0; i < g_iBTDevMax; i++)
    {
        pMenu[i].text = g_pBTDevList[i].name;
        pMenu[i].value = i + 1;
        pMenu[i].options = 0;
    }

    // Display list of devices menu
    iOption = uiMenu("sub-menu", cpTitle, pMenu, g_iBTDevMax, 0);
    LOGF_INFO(LogHandle, "uiMenu option=%d", iOption);

    if (iOption >= 1 && iOption <= g_iBTDevMax)
    {
        return (iOption - 1);
    }
    else
    {
        // Free memory only when BACK is selected, or when there is error in menu
        delete[] pMenu;
        pMenu = NULL;

        return -1;
    }
}

//==============================================================================
// 
//==============================================================================
int guiConfirmSSP(char *cpCode)
{
    map<string, string> value;
    int rc;

    LOGF_INFO(LogHandle, "guiConfirmSSP() code[%s]", cpCode);

    value["code"] = cpCode;

    // Display confirm SSP with numeric comparison
    rc = uiInvokeURL(value, "confirm-ssp.html");
    LOGF_INFO(LogHandle, "uiInvokeURL(confirm-ssp.html) rc=%d", rc);

    if (rc == 100)
        return 1;
    else
        return 0;
}

//==============================================================================
// 
//==============================================================================
void comNetCB(enum com_NetworkEvent iEvent, enum com_NetworkType iNetType,
    const void *vData, void *vPriv, enum com_ErrorCodes iComErrno)
{
    struct com_networkEventData nwData;
    int choice;
    int rc;
    pthread_cond_t *pcond;

    LOGF_INFO( LogHandle, "comNetCB() evt=%d tid=%lu", iEvent, pthread_self());

    memcpy(&nwData, vData, sizeof(nwData));

    switch (iEvent)
    {
    case COM_EVENT_BT_CONFIRM_PAIR_REQUEST: // 11

        choice = guiConfirmSSP(nwData.data1);

        // Confirm SSP with numeric comparison
        rc = com_SetDevicePropertyInt(COM_PROP_BT_CONFIRM_PAIR, choice, &iComErrno);
        LOGF_INFO(LogHandle, "COM_PROP_BT_CONFIRM_PAIR rc=%d err=%d", rc, iComErrno);

        break;

    case COM_EVENT_BT_PAIR_DONE: // 12

        //guiAlert("BT Pair", "Pairing Done", NULL, NULL, 1);

        if (vPriv != NULL)
        {
            //guiAlert("BT Discoverable", g_btName, "Terminal is", "Discoverable...", 0);
        }
        break;

    case COM_EVENT_BT_DISCOV_TO_ELAPSED: // 13

        pcond = (pthread_cond_t *)vPriv;

        // End of BT discoverable, unlock mutex
        rc = pthread_cond_signal(pcond);
        LOGF_INFO(LogHandle, "pthread_cond_signal() rc=%d", rc);

        break;

    default:
        LOGF_INFO(LogHandle,  "Unhandled Event Detected : %d", iEvent);
        break;
    }
}

//==============================================================================
// 
//==============================================================================


int btScanPair(void)
{
#if 0

    char *cpTitle = "BT Scan/Pair";
    enum com_ErrorCodes iComErrno;
    char msg[32];
    int idx, rc;

    LOGF_INFO(LogHandle, "btScanPair() tid=%lu", pthread_self());

    rc = btGetDevList(cpTitle, 1);
    if (rc == -1)
        return -1;

    // Loop scan results until BACK is selected
    while (1)
    {
        idx = guiDevListMenu(cpTitle);
        if (idx == -1)
            break;

        rc = guiConfirmPair(cpTitle, idx);
        if (rc == -1)
            continue;

        //guiAlert(cpTitle, "Pairing...", NULL, NULL, 0);

        // Set callback function for SSP with numeric comparison
        iComErrno = COM_ERR_NONE;
        rc = com_NetworkSetCallback(comNetCB, NULL, &iComErrno);
        LOGF_INFO(LogHandle, "com_NetworkSetCallback() rc=%d err=%d", rc, iComErrno);

        // Set using callback for SSP with numeric comparison
        iComErrno = COM_ERR_NONE;
        rc = com_SetDevicePropertyInt(COM_PROP_BT_SSP_CONFIRMATION, 1, &iComErrno);
        LOGF_INFO(LogHandle, "COM_PROP_BT_SSP_CONFIRMATION rc=%d err=%d", rc, iComErrno);

        // Perform pairing
        iComErrno = COM_ERR_NONE;
        rc = com_BTPair(g_pBTDevList[idx].addr, COM_BT_PAIR_AUTO, NULL, &iComErrno);
        LOGF_INFO(LogHandle, "com_BTPair rc=%d err=%d", rc, iComErrno);

        sprintf(msg, "com_BTPair() %d", rc);
        //guiAlert(cpTitle, msg, com_GetErrorString(iComErrno), NULL, 10);

        // Reset network callback
        iComErrno = COM_ERR_NONE;
        rc = com_NetworkSetCallback(NULL, NULL, &iComErrno);
        LOGF_INFO(LogHandle, "com_NetworkSetCallback() rc=%d err=%d", rc, iComErrno);
        iComErrno = COM_ERR_NONE;
        rc = com_SetDevicePropertyInt(COM_PROP_BT_SSP_CONFIRMATION, 0, &iComErrno);
        LOGF_INFO(LogHandle, "COM_PROP_BT_SSP_CONFIRMATION rc=%d err=%d", rc, iComErrno);
    }

    delete[] g_pBTDevList;
    g_pBTDevList = NULL;
#endif
    return 0;
}

//==============================================================================
// 
//==============================================================================
int btRemovePair(void)
{
#if 0
    char *cpTitle = "BT Remove Pair";
    enum com_ErrorCodes iComErrno;
    char msg[32];
    int idx, rc;

    rc = btGetDevList(cpTitle, 0);
    if (rc == -1)
        return -1;

    // Loop scan results until BACK is selected
    while (1)
    {
        idx = guiDevListMenu(cpTitle);
        if (idx == -1)
            break;

        rc = guiConfirmPair(cpTitle, idx);
        if (rc == -1)
            continue;

        //guiAlert(cpTitle, "Removing Pair...", NULL, NULL, 0);

        // Perform remove pair
        iComErrno = COM_ERR_NONE;
        rc = com_BTUnPair(g_pBTDevList[idx].addr, &iComErrno);
        LOGF_INFO(LogHandle, "com_BTUnPair rc=%d err=%d", rc, iComErrno);

        sprintf(msg, "com_BTUnPair() %d", rc);
        //guiAlert(cpTitle, msg, com_GetErrorString(iComErrno), NULL, 2);

        if (iComErrno == COM_ERR_NONE)
        {
            // Update list after successful remove pair
            rc = btGetDevList(cpTitle, 0);
            if (rc == -1)
                break;
        }
    }

    delete[] g_pBTDevList;
    g_pBTDevList = NULL;
#endif
    return 0;
}

//==============================================================================
// 
//==============================================================================
int btDiscoverable(void)
{
    //char *cpTitle = "BT Discoverable";
    enum com_ErrorCodes iComErrno;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int rc;

    LOGF_INFO(LogHandle, "btDiscoverable() tid=%lu", pthread_self());

   // guiAlert(cpTitle, "<Name>", "Terminal is", "Discoverable...", 0);

    // Set callback function for BT notifications
    iComErrno = COM_ERR_NONE;
    rc = com_NetworkSetCallback(comNetCB, &cond, &iComErrno);
    LOGF_INFO(LogHandle, "com_NetworkSetCallback() rc=%d err=%d", rc, iComErrno);

    // Set using callback for SSP with numeric comparison
    iComErrno = COM_ERR_NONE;
    rc = com_SetDevicePropertyInt(COM_PROP_BT_SSP_CONFIRMATION, 1, &iComErrno);
    LOGF_INFO(LogHandle, "COM_PROP_BT_SSP_CONFIRMATION rc=%d err=%d", rc, iComErrno);

    // Set BT discoverable
    iComErrno = COM_ERR_NONE;
    rc = com_SetDevicePropertyInt(COM_PROP_BT_DISCOVERY_TIMEOUT, 120, &iComErrno);
    LOGF_INFO(LogHandle, "COM_PROP_BT_DISCOVERY_TIMEOUT rc=%d err=%d", rc, iComErrno);

    // Initialize mutex 
    rc = pthread_mutex_init(&mutex, NULL);
    rc = pthread_cond_init(&cond, NULL);
    rc = pthread_mutex_lock(&mutex);
    LOGF_INFO(LogHandle, "pthread_mutex_lock() rc=%d", rc);

    // Set BT discoverable
    iComErrno = COM_ERR_NONE;
    rc = com_SetDevicePropertyInt(COM_PROP_BT_DISCOVERABLE, 1, &iComErrno);
    LOGF_INFO(LogHandle, "COM_PROP_BT_DISCOVERABLE rc=%d err=%d", rc, iComErrno);

    // Get local BT name
    memset(g_btName, 0, sizeof(g_btName));
    iComErrno = COM_ERR_NONE;
    rc = com_GetDevicePropertyString(COM_PROP_BT_LOCAL_NAME, g_btName, sizeof(g_btName), &iComErrno);
    LOGF_INFO(LogHandle, "COM_PROP_BT_LOCAL_NAME name[%s] rc=%d err=%d", g_btName, rc, iComErrno);

    //guiAlert(cpTitle, g_btName, "Terminal is", "Discoverable...", 0);

    // Use mutex lock to wait for BT discoverable to end
    rc = pthread_cond_wait(&cond, &mutex);
    LOGF_INFO(LogHandle, "pthread_cond_wait() rc=%d", rc);

    // Destroy mutex
    rc = pthread_mutex_unlock(&mutex);
    LOGF_INFO(LogHandle, "pthread_mutex_unlock() rc=%d", rc);
    rc = pthread_mutex_destroy(&mutex);
    rc = pthread_cond_destroy(&cond);

    // Reset network callback
    iComErrno = COM_ERR_NONE;
    rc = com_NetworkSetCallback(NULL, NULL, &iComErrno);
    LOGF_INFO(LogHandle, "com_NetworkSetCallback() rc=%d err=%d", rc, iComErrno);
    iComErrno = COM_ERR_NONE;
    rc = com_SetDevicePropertyInt(COM_PROP_BT_SSP_CONFIRMATION, 0, &iComErrno);
    LOGF_INFO(LogHandle, "COM_PROP_BT_SSP_CONFIRMATION rc=%d err=%d", rc, iComErrno);

    return 0;
}


VIM_ERROR_PTR CommWifiInit(VIM_CHAR* ErrMsg)
{
    VIM_ERROR_PTR res;

    vim_sprintf(g_comData.netProfile, "%sNET_WIFI.xml", CFG_NET_PATH);

    if ((res = netAttach(ErrMsg)) != VIM_ERROR_NONE)
    {
        LOGF_INFO(LogHandle, "netAttach (Wifi) Failed:%s", ErrMsg);
    }
    else
    {
        LOGF_INFO(LogHandle, "netAttach (Wifi) Success:%s", ErrMsg);
    }
    VIM_ERROR_RETURN(res);
}

//==============================================================================
// Initialize ADK com
//==============================================================================
VIM_ERROR_PTR ADKcomInit()
{
    enum com_ErrorCodes iComErrno;
    int iCtr;
    int rc;
    if (LogHandle == 0) {
        logInit();
    }

    LOGF_INFO(LogHandle, "CCCCCCCCCCCCCCCC Com Init CCCCCCCCCCCCCCC");

    /* initialize COM */
    for (iCtr = 0; iCtr < 30; iCtr++)
    {
        iComErrno = COM_ERR_NONE;
        rc = com_Init(&iComErrno);
        LOGF_INFO(LogHandle, "com_Init rc=%d err=%d ", rc, iComErrno);
        usleep(1000000);
        // If successful, or failed due to other than comdaemon not yet ready or already initialised
        if (rc == 0 || iComErrno != COM_ERR_DAEMON_COM || iComErrno == COM_ERR_ALREADY_INIT)
        {
            LOGF_INFO(LogHandle, "CCCCCCCCCCCCCCCC Success CCCCCCCCCCCCCCC");
            break;
        }
        else
        {
            LOGF_INFO(LogHandle, "com_Init failed, icomerrno = %d", iComErrno);
        }
    }

    if ((rc == -1) && (iComErrno != COM_ERR_ALREADY_INIT))
    {
        if (iComErrno == COM_ERR_DAEMON_COM)
        {
            VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);
        }
        else
        {
            VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
        }
    }
    return VIM_ERROR_NONE;
}

//==============================================================================
// Initialize ADK com
//==============================================================================
VIM_ERROR_PTR comInit(int UseBT, int InitWifi, VIM_CHAR *ErrMsg )
{
    enum com_ErrorCodes iComErrno;
	VIM_ERROR_PTR res = VIM_ERROR_NONE;
    int iParam;
    int rc;

    memset(&g_comData, 0, sizeof(struct comData));

    VIM_ERROR_RETURN_ON_FAIL(ADKcomInit());
    /* check if BT terminal */
    iComErrno = COM_ERR_NONE;
    iParam = 0;
    rc = com_GetDevicePropertyInt(COM_PROP_SUPP_INTERFACES, &iParam, &iComErrno);

    LOGF_INFO(LogHandle, "COM_PROP_SUPP_INTERFACES rc=%d val=0x%08x err=%d", rc, iParam, iComErrno);
    LOGF_INFO(LogHandle,  "Interfaces: %d ", iParam);

    // Attach different Networks for BT ( Integrated ) or ( GPRS ) Standalone
    // RDD 130122 JIRA PIRPIN-1995
    if ((InitWifi)&&(iParam & COM_WIFI))
    {
        VIM_ERROR_RETURN_ON_FAIL(CommWifiInit(ErrMsg));
		//WifiRunFromParams();
    }
    else
    {
        LOGF_INFO(LogHandle, "----------- No Wifi ----------");
        //VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
    }

    if (UseBT)
    {
        if (iParam & COM_BLUETOOTH)
        {
            LOGF_INFO(LogHandle,  "CCCCCCCCCCCCCCCC Configuring Network for BT CCCCCCCCCCCCCCC");

            vim_sprintf(g_comData.netProfile, "%sNET_ETHBT.xml", CFG_NET_PATH);
            //sprintf(g_comData.ethbtProfile, "%sCONN_RS232BT.xml", CFG_NET_PATH);
            //sprintf(g_comData.rs232btProfile, "%sCONN_ETHBT.xml", CFG_NET_PATH);
            //sprintf(g_comData.usbbtProfile, "%sCONN_USBBT.xml", CFG_NET_PATH);

			if(( res = netAttach(ErrMsg)) != VIM_ERROR_NONE)
			{
				LOGF_INFO(LogHandle,   "netAttach (BT) Failed:%s", ErrMsg );
			}
			else
			{
				LOGF_INFO(LogHandle,  "netAttach (BT) Success:%s", ErrMsg);
			}
        }
        else
        {
            LOGF_INFO(LogHandle,  "CCCCCCCCCCCCCCCC Error No BT CCCCCCCCCCCCCCC");
            VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
        }
    }
    if( 1 )
    {
        if (iParam & COM_FEATURE1_4G_LTE)
        {
            LOGF_INFO(LogHandle,  "CCCCCCCCCCCCCCCC 4G LTE Terminal CCCCCCCCCCCCCCC");
            sprintf(g_comData.netProfile, "%sPPP_NET.xml", CFG_NET_PATH);
            sprintf(g_comData.tcp4gProfile, "%sTCPIP_EFT.xml", CFG_NET_PATH);
        }
        else
        {
            LOGF_INFO(LogHandle,  "CCCCCCCCCCCCCCCC Not a 4G Terminal CCCCCCCCCCCCCCC");
        }
    }
    return VIM_ERROR_NONE;
}

//==============================================================================
//
//==============================================================================
void comDeinit(void)
{
    enum com_ErrorCodes iComErrno;
    int rc;

    iComErrno = COM_ERR_NONE;
    rc = com_Destroy(&iComErrno);
    LOGF_INFO(LogHandle, "com_Destroy rc=%d err=%d", rc, iComErrno);
}

//==============================================================================
// Display menu
//==============================================================================
#if 0
#define MAINMENUMAX                     6
int guiMainMenu(void)
{
    struct UIMenuEntry pMenu[MAINMENUMAX];
    char menuTexts[MAINMENUMAX][20] = {
        "BT Scan/Pair",
        "BT Remove Pair",
        "BT Discoverable",
        "Net Attach",
        "Net Detach",
        "Restart"
    };
    int iOption;
    int i;

    for (i = 0; i < MAINMENUMAX; i++)
    {
        pMenu[i].text = menuTexts[i];
        pMenu[i].value = i + 1;
        pMenu[i].options = 0;
    }

    iOption = uiMenu("main-menu", "ADK-BT DEMO", pMenu, MAINMENUMAX, 0);
    LOGF_INFO(LogHandle, "uiMenu option=%d", iOption);

    switch (iOption)
    {
    case UI_ERR_CONNECTION_LOST:
        return -1;

    case 1:
        btScanPair();
        break;
    case 2:
        btRemovePair();
        break;
    case 3:
        btDiscoverable();
        break;
    case 4:
        netAttach(ErrMsg);
        break;
    case 5:
        netDetach();
        break;
    case 6:
        sysReboot();
        break;
    }

    return 0;

}
#endif
//==============================================================================
// Initialize ADK gui
//==============================================================================
void guiInit(void)
{
    // "Down" on last menu option takes us to first option, and vice versa
    uiSetPropertyInt(UI_PROP_CIRCULAR_MENU, 1);

    // Feedback for user when button is touched.
    uiSetPropertyInt(UI_PROP_TOUCH_ACTION_BEEP, 1);

    // Set bigger font
    uiSetPropertyInt(UI_PROP_DEFAULT_FONT_SIZE, 4);

    // Return to previous menu after 10 seconds of idle
    //uiSetPropertyInt(UI_PROP_TIMEOUT, 10000); -> moved to iDisplayMenu

    return;
}

//==============================================================================
//
//==============================================================================
void guiDeinit(void)
{
    // return to default:0
    uiSetPropertyInt(UI_PROP_CIRCULAR_MENU, 0);

    // return to default:0
    uiSetPropertyInt(UI_PROP_TOUCH_ACTION_BEEP, 0);

    return;
}

//==============================================================================
// Initialize ADK log
//==============================================================================
void logInit(void)
{
    const char *cpVer = log_getVersion();

    //LOGAPI_SET_VERBOSITY(LOGAPI_VERB_FILE_LINE | LOGAPI_VERB_PID);

    LogHandle = LOGAPI_INIT(APPNAME);
    //LOGAPI_SETLEVEL(LOGAPI_TRACE);
    //LOGAPI_SETOUTPUT(LOGAPI_ALL);

    LOGF_INFO( LogHandle, "adklog ver[%s]", cpVer );
}

//==============================================================================
//
//==============================================================================
void logDeinit(void)
{
    LOGAPI_DEINIT(LogHandle);
}


//==============================================================================
//
//==============================================================================


#if 0
void launchADK()
{
    run("SYSLOG.OUT", 0);
    run("GUIPRTSERVER.OUT", 0);
    run("COMDAEMON.VSA", 0);
    SVC_WAIT(100);	// just to give things a chance to get started
}
#endif



void conn_status_callback(enum com_ConnectionEvent event, enum com_ConnectionType type, const void *data, void *priv, enum com_ErrorCodes iComErrno)
{
    (void)event;
    (void)priv;
    
	VIM_CHAR ErrMsg[100] = { 0 };
	vim_strcpy(ErrMsg, com_GetErrorString(iComErrno));

	switch (event) 
	{
		case COM_EVENT_CONNECTION_ESTABLISHED:
        LOGF_INFO(LogHandle,  "Connection established!");
        break;

		case COM_EVENT_CONNECTION_NEXT:
        LOGF_INFO(LogHandle,  "Trying new connection");
        break;

		case COM_EVENT_CONNECTION_FAILED:
        LOGF_INFO(LogHandle,  "Connection failed %s", ErrMsg );
        break;

		case COM_EVENT_PROFILE_FAILED:
        LOGF_INFO(LogHandle,  "Entire profile failed %s", ErrMsg );
        break;

		default:
        LOGF_INFO(LogHandle,  "Other Error %s", ErrMsg );
        break;
    }
}


int waitForData(com_ConnectHandle *handle) 
{
    unsigned int flags_out;
    enum com_ErrorCodes com_errno;

    int ret = com_Select(handle, COM_SELECT_READ, 3 * 60000, &flags_out, &com_errno);
    if (ret == -1) 
    {
        LOGF_INFO(LogHandle,  "com_Select failed with erro %s", com_GetErrorString(com_errno));
        return -1;
    }
    else if (ret == 0) 
    {
        LOGF_INFO(LogHandle,  "com_Select timed out, %s", __func__);
        return -1;
    }

    if (!(flags_out & COM_SELECT_READ)) 
    {
        LOGF_INFO(LogHandle,  "com_Select: wrong flag detected");
        return -1;
    }

    LOGF_INFO(LogHandle,  "Data has arrived. Ready to read...");
    return 0;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_nwif_tcp_disconnect(VIM_NET_INTERFACE_PTR self_ptr)
{

    LOGF_INFO(LogHandle,  "vim_drv_nwif_tcp_disconnect");

    VIM_ERROR_RETURN_ON_NULL(self_ptr);

    /* delete the instance */
    if (self_ptr->os_handle >= 0)
    {
        com_ConnectClose( self_ptr->adkcom_handle_ptr, NULL );
    }

    VIM_ERROR_RETURN_ON_FAIL(vim_mem_delete(self_ptr));
    return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_nwif_tcp_receive(VIM_NET_INTERFACE_PTR self_ptr, void *destination_ptr, VIM_SIZE *destination_size_ptr, VIM_SIZE destination_max_size)
{
    int result = 0;
    *destination_size_ptr = 0;
	enum com_ErrorCodes iComErrno;

    VIM_ERROR_RETURN_ON_NULL(self_ptr->adkcom_handle_ptr);

    /* attempt to receive bytes */
    errno = 0;

    if (!waitForData(self_ptr->adkcom_handle_ptr))
    {
        result = com_Receive(self_ptr->adkcom_handle_ptr, (void *)destination_ptr, (size_t)destination_max_size, &iComErrno);
    }

    if (result > 0)
    {
        *destination_size_ptr = result;
        VIM_DBG_LABEL_DATA("ADK tcp recv<==", destination_ptr, result);
        return VIM_ERROR_NONE;
    }
    else
    {     
		LOGF_INFO(LogHandle,  "!!!!! com_Receive error %d:%s ", result, com_GetErrorString(iComErrno));
		VIM_ERROR_RETURN(VIM_ERROR_TIMEOUT);
    }
    VIM_ERROR_RETURN(VIM_ERROR_NONE);
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_nwif_tcp_send( VIM_NET_INTERFACE_PTR self_ptr, const void *source_ptr, VIM_SIZE source_size )
{
    VIM_SIZE total_sent = 0;
    int sent;
    enum com_ErrorCodes iComErrno;
    const void *send_ptr = source_ptr;

    VIM_ERROR_RETURN_ON_NULL(self_ptr->adkcom_handle_ptr);

    // Verifone may not send large messages in one hit 
    do
    {
        if(( sent = com_Send(self_ptr->adkcom_handle_ptr, send_ptr, (size_t)source_size, &iComErrno )) < 0 )
        {
            LOGF_INFO(LogHandle,  "!!!!! ADK FATAL ERROR sent:errno in vim_drv_gprs_tcp_send %d:%s !!!!!", sent, com_GetErrorString(iComErrno) );
            VIM_ERROR_RETURN_ON_FAIL(vim_drv_nwif_tcp_disconnect(self_ptr));
            VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
        }
        LOGF_INFO(LogHandle,  "---- ADK TCPIP Sent Packet Size %d of %d OK", sent, source_size);
        total_sent += sent;

    } while (total_sent < source_size);

    return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_nwif_tcp_is_connected(VIM_NET_INTERFACE_PTR self_ptr, VIM_BOOL *connected_ptr)
{
    *connected_ptr = VIM_TRUE;
    
    //VIM_DBG_VAR(*connected_ptr);
    return VIM_ERROR_NONE;
}

VIM_ERROR_PTR adk_gprs_connect_request(VIM_NET_INTERFACE_HANDLE_PTR handle_ptr, VIM_NET_INTERFACE_CONNECT_PARAMETERS_PTR parameters_ptr)
{
	enum com_ErrorCodes iComErrno;
	
	static VIM_NET_INTERFACE_METHODS const methods =
    {
        vim_drv_nwif_tcp_is_connected,
        vim_drv_nwif_tcp_disconnect,
        vim_drv_nwif_tcp_send,
        vim_drv_nwif_tcp_receive
    };

    /* assign methods */
    handle_ptr->methods_ptr = &methods;
    /* allocate memory for a new instance */

	LOGF_INFO(LogHandle,  "ADKADKADKADKADK TCPIP connection via ADK COM ADKADKADKADKADK");

    VIM_MEM_NEW( handle_ptr->instance_ptr, VIM_NET_INTERFACE );
    vim_mem_clear( handle_ptr->instance_ptr, VIM_SIZEOF( VIM_NET_INTERFACE ));
    handle_ptr->instance_ptr->os_handle = -1;

    if(( handle_ptr->instance_ptr->adkcom_handle_ptr = com_Connect( g_comData.tcp4gProfile, conn_status_callback, NULL, 30000, &iComErrno)) == VIM_NULL )
    {
		VIM_CHAR ErrMsg[100] = {0};
		vim_strcpy(ErrMsg, com_GetErrorString(iComErrno));

        LOGF_INFO(LogHandle,  "ADK 4G Connection Failed: %s", ErrMsg );
        // Delete the instance 
        vim_drv_nwif_tcp_disconnect(handle_ptr->instance_ptr);
        return VIM_ERROR_TIMEOUT;
    }
    else
    {
        LOGF_INFO(LogHandle,   "ADK 4G Connection Completed : Ready To Send" );
        handle_ptr->instance_ptr->os_handle = nwif_handle;
    }
    return VIM_ERROR_NONE;
}
#if 0
struct com_PingInfo {
    unsigned int ntransmitted; /*!< number of transmitted packets */
    unsigned int nreceived;    /*!< number of received packets */
    unsigned long tavg;        /*!< average round trip in milliseconds*/
    unsigned long tmin;        /*!< max round trip in milliseconds*/
    unsigned long tmax;        /*!< min round trip in milliseconds*/
};
#endif




void adk_com_ppp_status( VIM_SIZE *State )
{  
    enum com_ErrorCodes iComErrno;
    int rc = com_GetNetworkStatus(g_comData.netProfile, &iComErrno);
    LOGF_INFO(LogHandle,  "com_GetNetworkStatus rc=%d err=%d", rc, iComErrno);
    *State = (VIM_SIZE)rc;

    switch (rc)        
    {
        default:
            LOGF_INFO(LogHandle,  "-------------------- ADK COM ERROR --------------------");
            break;
        case COM_NETWORK_STATUS_START:
            LOGF_INFO(LogHandle,  "-------------------- ADK COM NETWORK RUNNING --------------------");
            break;
        case COM_NETWORK_STATUS_STOP:
            LOGF_INFO(LogHandle,  "-------------------- ADK COM NETWORK STOPPED --------------------");
            break;        
    }
}


VIM_BOOL adk_com_ppp_connected(void)
{
    com_PingInfo PingInfo;
    com_ErrorCodes iErrNo;
    int rc;
    VIM_SIZE State;
    
    adk_com_ppp_status(&State);

    ppp_connected = (State == COM_NETWORK_STATUS_START) ? VIM_TRUE : VIM_FALSE;
    VIM_DBG_VAR(ppp_connected);

#if 0
    if ((conn1.adkcom_handle_ptr = com_Connect(g_comData.tcp4gProfile, conn_status_callback, NULL, 30000, NULL)) == 0)
    {
        LOGF_INFO(LogHandle,  "Connection Failed");
        // Delete the instance 
        //vim_drv_nwif_tcp_disconnect(handle_ptr->instance_ptr);
        //return VIM_ERROR_TIMEOUT;
    }
#endif

    rc = com_Ping( "10.23.122.130", COM_INTERFACE_GPRS0, 5, 5000, &PingInfo, &iErrNo );

    LOGF_INFO(LogHandle,  "Ping Errno=%d rc=%d", iErrNo, rc);
    if (!rc)
    {
        LOGF_INFO(LogHandle,  "Received:%d of 5. Ave round trip:%lu mS", PingInfo.nreceived, PingInfo.tavg);
    }
    return ppp_connected;
}



VIM_ERROR_PTR RestartBT(char *ErrMsg)
{
	VIM_ERROR_RETURN(netAttach(ErrMsg));
}


VIM_ERROR_PTR adk_com_init(int InitBT, int InitWifi, VIM_CHAR *ErrMsg )
{
    int ComStat = -1;
    com_ErrorCodes com_errno = COM_ERR_INVALID_HANDLE;
	//TERM_USE_4G = 0;

    LOGF_INFO(LogHandle,  "------------------- adk_com_init ------------------");

    logInit();

    // Kick off ADK Com - ignore error if already init cos we may call this more than once if 4G goes down
    VIM_ERROR_RETURN_ON_FAIL(comInit(InitBT, InitWifi, ErrMsg));

    LOGF_INFO(LogHandle,  "------------------- Com Init Success ------------------");
#if 0 // RDD - this was test SW so remove it for now.
	if (1)
	{
		if (!com_GetDevicePropertyInt(COM_PROP_SUPPORTED_FEATURES_2, &ComStat, &com_errno))
		{
			LOGF_INFO(LogHandle,  "CPSF2 = %d (errno = %d)", ComStat, com_errno);

			if (ComStat & COM_FEATURE2_IBEACON)
			{
				LOGF_INFO(LogHandle,  "CPSF2 = %d iBeacons Supported ( %d )", ComStat, com_errno);
			}
			if (ComStat & COM_FEATURE2_EDDYSTONE)
			{
				LOGF_INFO(LogHandle,  "CPSF2 = %d Eddystone Beacons Supported ( %d )", ComStat, com_errno);
			}
		}
		else
		{
			LOGF_INFO(LogHandle,  "COM_PROP_SUPPORTED_FEATURES_2 read FAILED: <%s> ", com_GetErrorString(com_errno));
		}
	}
#endif
    for(int i =0; i < 10; ++i)
    {
        // SIM status property for mobile devices. 0 = SIM unlocked and ready, 1 = PIN needed, 2 = PUK needed, 3 = SIM busy, read status again, 4 = SIM not present, -1 = Error)
        if (!com_GetDevicePropertyInt(COM_PROP_GSM_GET_SIM_STATUS, &ComStat, &com_errno))
        {
            LOGF_INFO(LogHandle,  "SIM Status = %d Com_errno = %d", ComStat, com_errno);
            switch (ComStat)
            {
            case 0:
                vim_strcpy(ErrMsg, "SIM Unlocked and Ready" );
				//TERM_USE_4G = 1;
				// Connect to APN ( 30 sec timeout, but configured to connect automatically on power up )
				ppp_connected = VIM_FALSE;
				VIM_ERROR_RETURN_ON_FAIL(netAttach(ErrMsg));
				ppp_connected = VIM_TRUE;
				return VIM_ERROR_NONE;
                break;

            case 1:
                vim_strcpy(ErrMsg, "SIM Locked: PIN required");
                break;

            case 2:
                vim_strcpy(ErrMsg, "SIM Locked: PUK required");
                break;

            case 3:
                vim_strcpy(ErrMsg, "SIM Busy: Try Later");
                break;

            case 4:
                vim_strcpy(ErrMsg, "SIM Missing");
                break;

            default:
                vim_strcpy(ErrMsg, com_GetErrorString(com_errno));
                break;

            }

            LOGF_INFO(LogHandle,  ErrMsg);

            if (ComStat)
            {
                VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
            }
            break;
        }
        else
        {
            vim_strcpy(ErrMsg, com_GetErrorString(com_errno));
            
            LOGF_INFO(LogHandle,  "SIM Status = %d Com_errno = %d", ComStat, com_errno);
            if (com_errno == COM_ERR_TRY_AGAIN) {
                LOGF_INFO(LogHandle, "------------------- Com SIM Status Failed : Retrying ------------------");
                sleep(1);
                continue;
            }
            LOGF_INFO(LogHandle, "------------------- Com SIM Status Failed : Exit ------------------");
            VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
        }
    }
	if (InitBT)
	{
		if (RestartBT(ErrMsg) != VIM_ERROR_NONE)
		{

		}
	}
    return VIM_ERROR_NOT_FOUND;
}





/*----------------------------------------------------------------------------*/


