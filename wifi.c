#include <incs.h>

#ifndef _WIN32
#include <svcmgr/svc_net.h>
#include <string.h>
#include <errno.h>
#endif


#define XML_SSID_START "<SSID>"
#define XML_SSID_END   "</SSID>"

#define XML_PSK_START "<PSK>"
#define XML_PSK_END   "</PSK>"

static VIM_TEXT const WifiSupFile[] = VIM_FILE_PATH_LOCAL "wificfg.conf";
static VIM_TEXT const WifiNetFile[] = VIM_FILE_PATH_LOCAL "NET_WIFI.xml";

VIM_TEXT const WifiSSIDCfg[] = VIM_FILE_PATH_LOCAL "wifissid.conf";
VIM_TEXT const WifiPWDCfg[] = VIM_FILE_PATH_LOCAL "wifipwd.conf";

static VIM_TEXT const WifiSSIDFile[] = VIM_FILE_PATH_LOGS "wifissid.conf";
static VIM_TEXT const WifiSSID_SSD_File[] = VIM_FILE_PATH_CWD "wifissid.conf";		// Load encrypted file from SSD 

static VIM_TEXT const WifiPWDFile[] = VIM_FILE_PATH_LOGS "wifipwd.conf";
static VIM_TEXT const WifiPWD_SSD_File[] = VIM_FILE_PATH_CWD "wifipwd.conf";



VIM_CHAR* DecryptFromFile(const VIM_CHAR* FilePath);
VIM_ERROR_PTR SubUpdatedFieldData(const VIM_CHAR* DestFilePath, const VIM_CHAR* StartTag, const VIM_CHAR* EndTag, VIM_CHAR* NewTagData);


#ifdef _WIN32

VIM_ERROR_PTR CommWifiInit(VIM_CHAR* ErrMsg)
{
	return VIM_ERROR_SYSTEM;
}

#endif

#define IP_ADDR_ELEMENTS 4

VIM_ERROR_PTR IP_StrToHex(VIM_UINT8 *Dest, VIM_CHAR *Src)		// convert to array of 4 x UINT8   
{
	VIM_UINT8 *DestPtr = (VIM_UINT8 *)Dest;
	VIM_CHAR *StartPtr = Src;
	VIM_SIZE n;

	for (n = 0; n < IP_ADDR_ELEMENTS; n++)
	{
		VIM_CHAR AdrElement[3 + 1];
		VIM_CHAR *EndPtr = StartPtr;
		vim_mem_clear(AdrElement, VIM_SIZEOF(AdrElement));
		while (VIM_ISDIGIT(*EndPtr))
		{
			EndPtr += 1;
		}
		vim_mem_copy(AdrElement, StartPtr, (VIM_SIZE)(EndPtr - StartPtr));
		*DestPtr++ = asc_to_bin(AdrElement, VIM_MIN(vim_strlen(AdrElement), VIM_SIZEOF(AdrElement) - 1));
		StartPtr = ++EndPtr;
	}
	return VIM_ERROR_NONE;
}

#define PING_COUNT 3



#ifdef _WIN32

VIM_ERROR_PTR WifiStatus(VIM_BOOL SilentMode, VIM_UINT8 Useage)
{
	if (SilentMode)
		return VIM_ERROR_NONE;

	VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Reading Status", "Please Wait..."));
	vim_event_sleep(500);
	VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Status", "Not Connected"));
	vim_event_sleep(2000);
	return VIM_ERROR_NOT_FOUND;
}



static VIM_ERROR_PTR WifiGetCfg(VIM_CHAR *Buffer)
{
	return VIM_ERROR_NONE;
}

static VIM_ERROR_PTR BTGetCfg(VIM_CHAR *Buffer)
{
	return VIM_ERROR_NONE;
}

VIM_BOOL BT_Available(void)
{
	VIM_UINT8 default_ip[4];
	VIM_CHAR szIP[20] = { 0 };
	VIM_CHAR *local_ip = "192.168.0.58";

	IP_StrToHex(default_ip, local_ip);

	default_ip[3] = 0x01;

	vim_sprintf(szIP, "%d.%d.%d.%d", default_ip[0], default_ip[1], default_ip[2], default_ip[3]);

	VIM_DBG_PRINTF1("Ping to %s received 3 packets", szIP);	
	return VIM_FALSE;
}


VIM_ERROR_PTR BTStatus(VIM_BOOL SilentMode, VIM_UINT8 Useage)
{
	if (SilentMode)
		return VIM_ERROR_NONE;

	VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Reading Status", "Please Wait..."));
	vim_event_sleep(500);

	if (BT_Available())
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "BT Status", "Connected"));
		vim_event_sleep(2000);
	}
	return VIM_ERROR_NOT_FOUND;
}


#else


VIM_ERROR_PTR WifiStatus( VIM_BOOL SilentMode, VIM_UINT8 WifiUsage )
{
    int res;
    VIM_CHAR Buffer[200];
    struct netIfconfig config;
	VIM_KBD_CODE key;

	vim_mem_clear( Buffer, VIM_SIZEOF( Buffer ));
	if ( !WifiUsage )
	{
		if (!SilentMode)
		{
			VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Not Required", "" ));
			vim_event_sleep(2000);
		}
		return VIM_ERROR_NOT_FOUND;
	}

	if (!SilentMode)
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Checking WiFi", "Please Wait..."));
	}

    if ((res = net_interfaceUp("wlan0", NET_IP_V4)) != 0)
    {
		pceft_debugf(pceftpos_handle.instance, "Wifi Error:<%s> restarting ...", strerror(errno));
		net_wifiStart(1);

		if ((res = net_interfaceUp("wlan0", NET_IP_V4)) != 0)		
		{				
			pceft_debugf(pceftpos_handle.instance, "Unable to start Wifi:<%s>", strerror(errno));		
			return VIM_ERROR_NOT_FOUND;
		}
    }
	config = net_interfaceStatus("wlan0");
	vim_sprintf(Buffer, "Wifi connected\nlocal ip: %s\n", config.local_ip);
	pceft_debug(pceftpos_handle.instance, Buffer);

	if (!SilentMode)
	{
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Status:", Buffer);
		vim_kbd_read(&key, TIMEOUT_INACTIVITY);
	}
    return VIM_ERROR_NONE;
}

/* @return 0 when the pairing was successful, or -1 on failure with errno
* @li ENOENT Bluetooth SW not found
* @li ENODEV Bluetooth not available
* @li EINVAL Invalid parameters
* @li ECOMM Communication error with Bluetooth daemon
* @li EIO Communication error with Bluetooth device
* @li EEXIST The Bluetooth device is already paired
* @li ENOMEM Allocation failed
*/

// RDD 090422 Bring Network up and ping the defaul IP addr.
VIM_BOOL BT_Available(void)
{
	struct netIfconfig config;
	VIM_UINT8 default_ip[4];
	VIM_CHAR szIP[20] = { 0 };
	struct netPingInfo PingInfo;

	config = net_interfaceStatus(NET_PAN_DEFAULT);

	IP_StrToHex(default_ip, config.local_ip);

	default_ip[3] = 0x01;

	vim_sprintf(szIP, "%d.%d.%d.%d", default_ip[0], default_ip[1], default_ip[2], default_ip[3]);

	VIM_DBG_POINT;

	PingInfo = net_ping(szIP, PING_COUNT);

	VIM_DBG_POINT;

	if (!PingInfo.nreceived)
	{
		VIM_DBG_PRINTF1( "net Ping failed: <%s> ", strerror(errno) );
	}
	else
	{
		VIM_DBG_PRINTF2( "Ping to %s received %d packets", szIP, PingInfo.nreceived);
		return VIM_TRUE;
	}
	VIM_DBG_POINT;
	return VIM_FALSE;
}



VIM_ERROR_PTR BTStatus(VIM_BOOL SilentMode, VIM_UINT8 BTUsage)
{
	int res;
	VIM_CHAR Buffer[200];
	struct netIfconfig config;
	struct netBluetoothDevList paired_device;
	VIM_KBD_CODE key;

	vim_mem_clear(Buffer, VIM_SIZEOF(Buffer));
	if (!BTUsage)
	{
		if (!SilentMode)
		{
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "BT Not Required", ""));
			vim_event_sleep(2000);
		}
		return VIM_ERROR_NOT_FOUND;
	}

	if (!SilentMode)
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Checking Bluetooth", "Please Wait..."));
	}

	if ((res = net_interfaceUp(NET_PAN_DEFAULT, NET_IP_V4)) != 0)
	{
		pceft_debugf(pceftpos_handle.instance, "Net BT IF Up:<%s> re-pairing ...", strerror(errno));
		VIM_CHAR AddressBuffer[500];
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Pairing Required", "Put V400m on Base\n and Retry #BT"));
		vim_event_sleep(2000);

		adk_com_init(VIM_TRUE, VIM_FALSE, AddressBuffer);

		// For Base 
		if(( res = net_bluetooth_autoUsbPairing( AddressBuffer, VIM_SIZEOF(AddressBuffer))) != 0)
		{
			if (errno == EEXIST)
			{
				pceft_debugf(pceftpos_handle.instance, "<%s>", strerror(errno));
			}
			else
			{
				pceft_debugf(pceftpos_handle.instance, "Unable to pair BT:<%s>", strerror(errno));
				VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
			}
		}
		else
		{
			pceft_debugf(pceftpos_handle.instance, "Pairing successful with %s", AddressBuffer );
		}


		if ((res = net_interfaceUp(NET_PAN_DEFAULT, NET_IP_V4)) != 0)
		{
			pceft_debugf(pceftpos_handle.instance, "net_interfaceUp Error:<%s> after sucessful pairing", strerror(errno));
			return VIM_ERROR_NOT_FOUND;
		}
	}
	else
	{
		pceft_debugf(pceftpos_handle.instance, "net_interfaceUp Sucessful");
	}
#if 0
	if ((res = net_bluetoothConnectPAN(NET_BTBASE_DEFAULT)) != 0)
	{
		pceft_debugf(pceftpos_handle.instance, "net_bluetoothConnectPAN Error:<%s> after sucessful pairing", strerror(errno));
		return VIM_ERROR_NOT_FOUND;
	}
	else
	{
		pceft_debugf(pceftpos_handle.instance, "net_bluetoothConnectPAN Sucessful");
	}
#endif
	config = net_interfaceStatus(NET_PAN_DEFAULT);
	paired_device = net_bluetoothPairedList();


	if (BT_Available())
	{
		vim_sprintf(Buffer, "BT connected\nlocal ip: %s\nPaired Device:%s\nPing Host Passed", config.local_ip, paired_device.device->name);
	}
	else
	{
		vim_sprintf(Buffer, "BT connected\nlocal ip: %s\nPaired Device:%s\nPing Host Failed:\n Check Ethernet Cable", config.local_ip, paired_device.device->name);
	}

	pceft_debug(pceftpos_handle.instance, Buffer);
	if (!SilentMode)
	{
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "BT Status", Buffer);
	}

	vim_kbd_read(&key, TIMEOUT_INACTIVITY);
	return VIM_ERROR_NONE;
}





static VIM_ERROR_PTR WifiGetCfg(VIM_CHAR *Buffer)
{
    struct netWifiInfo info;

    vim_strcpy(info.ssid, "");
    info.signalStrength = 0;
    info = net_wifiInfo("wlan0");

    if (info.signalStrength)
    {
        vim_sprintf(Buffer, "ssid: %s\nsignal: %d%%", info.ssid, info.signalStrength);
    }
    else
    {
        vim_sprintf(Buffer, "ssid: ????\nError:%s", info.ssid, strerror(errno));
    }
    return VIM_ERROR_NONE;
}


#endif


/** /@param[in] filePath pathname of the file to be created.If NULL or "" then NET_WPA_SUPPLICANT_DEFAULT_CONFIG_FILE is used.
* @param[in] ssid network name as an ASCII string with double quotation, or as hex string.
* @param[in] psk 64 hex - digit passkey string, or 8 to 63 ASCII characters passphrase string with double quotation.
* @param[in] key_mgt Key management to be used.Allowable values are WPA - PSK and /or WPA - EAP.If "" or NULL, then NET_WPA_SUPPLICANT_DEFAULT_KEY_MANAGEMENT will be used
* @param[in] pairwise Pairwise cipher to be used.Allowable values are CCMPand /or TKIP.If "" or NULL, then NET_WPA_SUPPLICANT_DEFAULT_PAIRWISE will be used.
* @param[in] group Group cipher to be used.Allowable values are CCMPand /or TKIP.If "" or NULL, then NET_WPA_SUPPLICANT_DEFAULT_GROUP will be used.
* @param[in] proto Protocol to be used.Allowable values are RSN, WPA2, and /or WPA.If "" or NULL, then NET_WPA_SUPPLICANT_DEFAULT_PROTO will be used.
**/

#ifdef _WIN32
static int net_defSupplicantFileCreateExt(char* filePath, char* ssid, char* psk, char* key_mgmt, char* pairwise, char* group, char* proto)
{
	return 0;
}
#endif

/*
* Start WIFI supplicant according to parameters stored in the default file
* Supplicant configuration file has to follow PCI - compliant security settings :
*network = {
*proto = RSN or WPA2
* group = CCMP
* pairwise = CCMP
* key_mgmt = WPA - PSK or WPA - EAP or both
* }
*/

VIM_ERROR_PTR WifiSetConfig(VIM_CHAR* ssid, VIM_CHAR* psk, VIM_CHAR* protocol)
{
	VIM_INT16 ret = 0;
	VIM_CHAR Buffer[100] = { 0 };

	VIM_DBG_MESSAGE("WWWWWWWWWWWWW Configure WIFI WWWWWWWWWWWWWWW");

	if (!vim_strcmp(protocol, "WPA"))
	{
		VIM_DBG_MESSAGE("WPA");
		ret = net_defSupplicantFileCreateExt(VIM_NULL, ssid, psk, "WPA-PSK", "TKIP", "TKIP", protocol );
	}
	else if (!vim_strcmp(protocol, "WPA2"))
	{
		VIM_DBG_MESSAGE("WPA2");
		ret = net_defSupplicantFileCreateExt(VIM_NULL, ssid, psk, "WPA-PSK", "CCMP", "CCMP", protocol);
	}
	else if (!vim_strcmp(protocol, "WPA2-EAP"))
	{
		VIM_DBG_MESSAGE("WPA2-EAP");
		ret = net_defSupplicantFileCreateExt(VIM_NULL, ssid, psk, "WPA-EAP", "CCMP", "CCMP", "WPA2");
	}
	else
	{
		VIM_DBG_PRINTF3("SSID:%s PWD:%s PROT:%s", ssid, psk, protocol );
		VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
	}

	if (!ret)
	{
		vim_sprintf(Buffer, "Sucessful Wifi Config: SSID=%s, Protocol=%s", ssid, protocol);
	}
	else
	{
		vim_sprintf(Buffer, "Failed Wifi Config: SSID=%s, Protocol=%s return=%d Error=%s", ssid, protocol, ret, strerror(errno));
	}
	pceft_debug(pceftpos_handle.instance, Buffer );
	return VIM_ERROR_NONE;
}





static VIM_ERROR_PTR WifiConfig(void)
{
	VIM_KBD_CODE key;
    VIM_CHAR Buffer[200];
    vim_mem_clear( Buffer, VIM_SIZEOF( Buffer ));

    VIM_ERROR_RETURN_ON_FAIL( display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Reading Config", "Please Wait..."));

    WifiGetCfg(Buffer);

	VIM_ERROR_RETURN_ON_FAIL( display_screen( DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Config", Buffer ));
	vim_kbd_read(&key, TIMEOUT_INACTIVITY);

    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR WifiStat( void )
{
	return (WifiStatus(VIM_FALSE, 2));
}

extern VIM_PCEFTPOS_PARAMETERS pceftpos_settings;

static VIM_DES_BLOCK iv =
{ {
		0xb4,0x93,0x1a,0x8d,0x89,0xAB,0x22,0x90
} };

static const VIM_DES2_KEY ssk11 =
{ {
  {{0x14,0x83,0x00,0x01,0xA3,0xAd,0xa2,0x02}},
  {{0x02,0x4f,0x42,0x44,0xad,0x12,0x7f,0x6d}}
} };

static const VIM_DES2_KEY ssk22 =
{ {
  {{0x15,0x84,0x01,0x02,0xA4,0xAe,0xa3,0x03}},
  {{0x03,0x4e,0x40,0x42,0xae,0x11,0x7d,0x66}}
} };

#if 0
static VIM_DES_KEY ssk[2] =
{ 
	{{0x14,0x83,0x00,0x01,0xA3,0xAd,0xa2,0x02}},{{0x02,0x4f,0x42,0x44,0xad,0x12,0x7f,0x6d}}
};
#endif

// Rolled Key.
VIM_DES2_KEY ssk33;

const char b64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


VIM_INT16 b64invs[] = { 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58,
	59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5,
	6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
	21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42,
	43, 44, 45, 46, 47, 48, 49, 50, 51 };



VIM_SIZE b64_decoded_size(const VIM_CHAR* in)
{
	VIM_SIZE len, ret, i;

	if (in == NULL)
		return 0;

	len = strlen(in);
	ret = len / 4 * 3;

	for (i = len; i-- > 0; ) {
		if (in[i] == '=') {
			ret--;
		}
		else {
			break;
		}
	}

	return ret;
}


void b64_generate_decode_table()
{
	int    inv[80];
	size_t i;

	vim_mem_set(inv, -1, sizeof(inv));
	for (i = 0; i < sizeof(b64chars) - 1; i++) {
		inv[b64chars[i] - 43] = i;
	}
}


int b64_isvalidchar(char c)
{
	if (c >= '0' && c <= '9')
		return 1;
	if (c >= 'A' && c <= 'Z')
		return 1;
	if (c >= 'a' && c <= 'z')
		return 1;
	if (c == '+' || c == '/' || c == '=')
		return 1;
	return 0;
}


VIM_INT16 b64_decode(const VIM_CHAR* in, VIM_UINT8* out, size_t *outlen)
{
	size_t len, i, j;
	int v;

	if (in == NULL || out == NULL)
		return 0;

	len = vim_strlen(in);
	if (*outlen < b64_decoded_size(in) || len % 4 != 0)
		return 0;

	for (i = 0; i < len; i++) {
		if (!b64_isvalidchar(in[i])) {
			return 0;
		}
	}

	for (i = 0, j = 0; i < len; i += 4, j += 3) {
		v = b64invs[in[i] - 43];
		v = (v << 6) | b64invs[in[i + 1] - 43];
		v = in[i + 2] == '=' ? v << 6 : (v << 6) | b64invs[in[i + 2] - 43];
		v = in[i + 3] == '=' ? v << 6 : (v << 6) | b64invs[in[i + 3] - 43];

		out[j] = (v >> 16) & 0xFF;
		if (in[i + 2] != '=')
			out[j + 1] = (v >> 8) & 0xFF;
		if (in[i + 3] != '=')
			out[j + 2] = v & 0xFF;
	}
	*outlen = i;
	return 1;
}




size_t b64_encoded_size(size_t inlen)
{
	size_t ret;

	ret = inlen;
	if (inlen % 3 != 0)
		ret += 3 - (inlen % 3);
	ret /= 3;
	ret *= 4;

	return ret;
}


char *b64_encode(const VIM_UINT8* in, size_t len)
{
	char* out;
	size_t  elen, i, j, v;

	if (in == NULL || len == 0)
		return NULL;

	elen = b64_encoded_size(len);
	out = malloc(elen + 1);
	out[elen] = '\0';

	for (i = 0, j = 0; i < len; i += 3, j += 4) {
		v = in[i];
		v = i + 1 < len ? v << 8 | in[i + 1] : v << 8;
		v = i + 2 < len ? v << 8 | in[i + 2] : v << 8;

		out[j] = b64chars[(v >> 18) & 0x3F];
		out[j + 1] = b64chars[(v >> 12) & 0x3F];
		if (i + 1 < len) {
			out[j + 2] = b64chars[(v >> 6) & 0x3F];
		}
		else {
			out[j + 2] = '=';
		}
		if (i + 2 < len) {
			out[j + 3] = b64chars[v & 0x3F];
		}
		else {
			out[j + 3] = '=';
		}
	}

	return out;
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR ofb_encrypt_using_ssk(VIM_CHAR *source_ptr, VIM_CHAR const *FilePath )
{
	VIM_UINT8 ClearData[64] = { 0 };
	VIM_UINT8 EncData[64] = { 0 };
	VIM_CHAR EncDataL[65] = { 0 };//buffer to include the length in 1st byte
	VIM_SIZE Len = vim_strlen(source_ptr);
	VIM_SIZE Len_Unpadded = Len;
	VIM_UINT8 Rem = Len % 8;
	VIM_CHAR* Ptr = VIM_NULL;

	// Round up to neares 8 bytes 9 DES Block )
	if (Rem)
		Len += (8 - Rem);

	vim_strcpy((VIM_CHAR *)ClearData, source_ptr);

	VIM_ERROR_RETURN_ON_FAIL(vim_des2_owf(&ssk33, &ssk11, &ssk22));

	VIM_ERROR_RETURN_ON_FAIL(vim_des2_ofb_encrypt(EncData,
		ClearData,
		Len,
		&ssk33,
		&iv));

	VIM_DBG_DATA(EncData, Len);

	Ptr = b64_encode(EncData, Len);
	vim_strcpy(&EncDataL[1], Ptr);
	EncDataL[0] = Len_Unpadded;  //set the length
	vim_file_set(FilePath, EncDataL, vim_strlen(EncDataL));
	free(Ptr);
	/* return without error */
	return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/


VIM_ERROR_PTR ofb_decrypt_using_ssk(VIM_CHAR* Output, VIM_CHAR const *EncDatab64 )
{
	VIM_UINT8 EncData[64] = { 0 };
	VIM_UINT8 ClearData[64] = { 0 };
	VIM_SIZE EncDataLen = VIM_SIZEOF(EncData);
	VIM_SIZE Len_Unpadded = EncDatab64[0];  //read the actual length
	++EncDatab64;  //move the pointer to actual data
	b64_decode(EncDatab64, (VIM_UINT8*)EncData, &EncDataLen);

	VIM_ERROR_RETURN_ON_FAIL(vim_des2_owf(&ssk33, &ssk11, &ssk22));

	VIM_ERROR_RETURN_ON_FAIL(vim_des2_ofb_encrypt(ClearData, EncData, EncDataLen, &ssk33, &iv));

	VIM_DBG_STRING((VIM_CHAR *)ClearData);
	vim_strncpy(Output, (VIM_CHAR *)ClearData, EncDataLen);
	Output[Len_Unpadded] = '\0';  //terminate any potential junks
	return VIM_ERROR_NONE;
}


// Load Encrypted Password from SSD
VIM_ERROR_PTR WifiLoadEncryptedTag(const VIM_CHAR* FilePath, const VIM_CHAR* TagStart, const VIM_CHAR* TagEnd)
{
	VIM_ERROR_RETURN_ON_FAIL(SubUpdatedFieldData(WifiNetFile, TagStart, TagEnd, DecryptFromFile(FilePath)));
	// Grab the contents of the Network file
	return VIM_ERROR_NONE;
}



VIM_ERROR_PTR WifiRunFromParams(void)
{
	VIM_ERROR_PTR res;
	VIM_CHAR ErrMsg[100] = { 0 };
	VIM_CHAR DisplayBuf[100] = { 0 };

	vim_sprintf(DisplayBuf, "Connecting to Wifi\nPlease Wait...");
	display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
	pceft_debug(pceftpos_handle.instance, DisplayBuf);
	vim_event_sleep(1000);

	if (vim_file_is_present(WifiSSIDCfg))	
	{
		if (WifiLoadEncryptedTag(WifiSSIDCfg, XML_SSID_START, XML_SSID_END) != VIM_ERROR_NONE)
		{
			vim_sprintf(DisplayBuf, "Custom SSID Invalid\nUsing Default Value");
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
		}
		else
		{
			vim_sprintf(DisplayBuf, "Wifi SSID Decrypted from File - Success");
		}
	}
	else
	{
		vim_sprintf(DisplayBuf, "No Custom SSID found\nUsing Default Value");
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "" );
	}
	pceft_debug(pceftpos_handle.instance, DisplayBuf);

	if (vim_file_is_present(WifiPWDCfg))
	{
		if (WifiLoadEncryptedTag(WifiPWDCfg, XML_PSK_START, XML_PSK_END) != VIM_ERROR_NONE)
		{
			vim_sprintf(DisplayBuf, "Custom PSK Invalid\nUsing Default Value");
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
		}
		else
		{
			vim_sprintf(DisplayBuf, "Wifi PSK Decrypted from File - Success");
		}
	}
	else
	{
		vim_sprintf(DisplayBuf, "No Custom PSK found\nUsing Default Value");
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
	}
	pceft_debug(pceftpos_handle.instance, DisplayBuf);

	if ((res = CommWifiInit(ErrMsg)) == VIM_ERROR_NONE)
	{
		VIM_DBG_MESSAGE("WIFI OK WIFI OK WIFI OK WIFI OK WIFI OK WIFI OK WIFI OK WIFI OK WIFI OK ");
		if (TERM_USE_WIFI != 1) {
			TERM_USE_WIFI = 2;
		}
		vim_sprintf(DisplayBuf, "Wifi Connected - Success");
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
		pceft_debug(pceftpos_handle.instance, DisplayBuf);
		vim_sound(VIM_FALSE);
		vim_event_sleep(200);
		vim_sound(VIM_FALSE);
		vim_event_sleep(200);
		vim_sound(VIM_FALSE);
		vim_event_sleep(2000);
	}
	else
	{
		VIM_DBG_MESSAGE("WIFI FAILED WIFI FAILED WIFI FAILED WIFI FAILED WIFI FAILED WIFI FAILED WIFI FAILED ");
		vim_sprintf(DisplayBuf, "Wifi connect Failed\nError:\n%s", ErrMsg);
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, DisplayBuf, "");
		pceft_debug(pceftpos_handle.instance, DisplayBuf);
		vim_event_sleep(5000);
	}
	VIM_ERROR_RETURN(res);
}



VIM_CHAR* DecryptFromFile(const VIM_CHAR *FilePath)
{
	VIM_SIZE Len = 0;
	VIM_CHAR encData[64] = { 0 };
	static VIM_CHAR decData[64] = { 0 };

	if (vim_file_get(FilePath, encData, &Len, VIM_SIZEOF(encData)) == VIM_ERROR_NONE)
	{
		VIM_DBG_MESSAGE("Sucessfully Read Wifi PSK from SSD");
		if( ofb_decrypt_using_ssk( decData, encData ) == VIM_ERROR_NONE )
		{
			//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Decrypt", "Success");
			VIM_DBG_MESSAGE("Sucessfully Decrypted Wifi PSK from SSD");
			//vim_event_sleep(1000);
			return decData;
		}
		else
		{
			display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Decrypt", "Failed");
		}
	}
	else
	{
		display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "File Read", "Failed");
	}
	vim_event_sleep(2000);
	return VIM_NULL;
}


// RDD 191022 Substitute a Tag Value in a file for a given value - XML or JSON format
VIM_ERROR_PTR SubUpdatedFieldData(const VIM_CHAR* DestFilePath, const VIM_CHAR* StartTag, const VIM_CHAR *EndTag, VIM_CHAR* NewTagData )
{
	VIM_CHAR FileBuffer1[1024] = { 0 };
	VIM_CHAR FileBuffer2[1024] = { 0 };
	VIM_SIZE Len = 0;
	VIM_CHAR* Ptr = VIM_NULL;
	VIM_CHAR* Ptr2 = VIM_NULL;

	VIM_ERROR_RETURN_ON_NULL(NewTagData);

	VIM_ERROR_RETURN_ON_FAIL(vim_file_get(DestFilePath, FileBuffer1, &Len, VIM_SIZEOF(FileBuffer1)));
	VIM_ERROR_RETURN_ON_FAIL(vim_strstr(FileBuffer1, StartTag, &Ptr));
	Ptr += vim_strlen(StartTag);

	// Get a copy of the rest of the file
	VIM_ERROR_RETURN_ON_FAIL(vim_strstr(FileBuffer1, EndTag, &Ptr2));
	Ptr2 += vim_strlen(EndTag);		

	vim_strcpy(FileBuffer2, Ptr2);

	// Wipe the current buffer
	vim_mem_clear(Ptr, vim_strlen(Ptr));	

	// Copy over the new tag data
	vim_strcpy(Ptr, NewTagData);
	Ptr += vim_strlen(NewTagData);
	
	// Add the end tag
	vim_strcat(Ptr, EndTag);
	Ptr += vim_strlen(EndTag);
	//vim_strncat(Ptr, "\n\r", vim_strlen("\n\r"));
	
	// Add any other original tags
	vim_strncat(Ptr, FileBuffer2, vim_strlen(FileBuffer2));		
	
	// Write the new file contents
	VIM_ERROR_RETURN_ON_FAIL( vim_file_set(DestFilePath, FileBuffer1, vim_strlen(FileBuffer1)));
	return VIM_ERROR_NONE;
}






VIM_ERROR_PTR WifiKeyedPwd(void)
{
	kbd_options_t options;
	VIM_CHAR TempPwd[64] = { 0 };

	options.timeout = ENTRY_TIMEOUT;
	options.min = 8;
	options.max = 32;
	options.entry_options = KBD_OPT_ALPHA | KBD_OPT_ALLOW_BLANK | KBD_OPT_DISP_ORIG;
	options.CheckEvents = VIM_FALSE;

	vim_strcpy(TempPwd, TERM_WIFI_PWD);

	VIM_ERROR_RETURN_ON_FAIL( display_data_entry( TempPwd, &options, DISP_WIFI_PASSWORD, VIM_PCEFTPOS_KEY_NONE, 0 ));

	vim_file_set( WifiPWDFile, TempPwd, vim_strlen(TempPwd) );

	VIM_ERROR_RETURN_ON_FAIL( WifiLoadEncryptedTag(WifiPWDFile, XML_PSK_START, XML_PSK_END));
	WifiRunFromParams();

	return VIM_ERROR_NONE;
}

extern VIM_BOOL GetAnswer(VIM_CHAR* PinPadText);


VIM_ERROR_PTR SaveFileToFlash(void)
{
	if (GetAnswer("Insert Flash Drive\ninto USBC port\nand Press [ENTER]") == VIM_TRUE)
	{
		if (vim_file_copy(WifiSSIDCfg, WifiSSID_SSD_File) == VIM_ERROR_NONE)
		{
			if (vim_file_copy(WifiPWDCfg, WifiPWD_SSD_File) == VIM_ERROR_NONE)
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "File Transfer Success", "Remove Flash Drive"));
			}
			else
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "File Transfer Failed", "Remove Flash Drive"));
			}
		}
		else
		{
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "SSID Transfer Failed", "Remove Flash Drive"));
		}
		vim_event_sleep(2000);
	}
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR WifiFlashDriveUpPwd(void)
{
	return SaveFileToFlash();
}


VIM_ERROR_PTR WifiCopyFileToLogs(void)
{
	if (vim_file_copy(WifiSSIDCfg, WifiSSIDFile) == VIM_ERROR_NONE)
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enc SSID", "Copied to VHQ"));
	}
	else
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enc SSID", "Copy Failed"));
	}
	vim_event_sleep(2000);
	if (vim_file_copy(WifiPWDCfg, WifiPWDFile) == VIM_ERROR_NONE)
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enc PWD", "Copied to VHQ"));
	}
	else
	{
		VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Enc PWD", "Copy Failed"));
	}
	vim_event_sleep(2000);
	return VIM_ERROR_NONE;
}


VIM_ERROR_PTR WifiFlashDrivePwd(void)
{
	if (GetAnswer("Insert Flash Drive\ninto USBC port\nand Press [ENTER]") == VIM_TRUE)
	{

		if (WifiLoadEncryptedTag(WifiSSID_SSD_File, XML_SSID_START, XML_SSID_END) == VIM_ERROR_NONE)
		{
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Sucessfully Updated", "Wifi SSID"));
			vim_event_sleep(1000);
			if (WifiLoadEncryptedTag(WifiPWD_SSD_File, XML_PSK_START, XML_PSK_END) == VIM_ERROR_NONE)
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Sucessfully Updated", "Wifi Password"));
				vim_event_sleep(2000);
				WifiRunFromParams();
				return VIM_ERROR_NONE;
			}
			else
			{
				VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Failed to Update", "Wifi Password"));
			}
		}
		else
		{
			VIM_ERROR_RETURN_ON_FAIL(display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Failed to Update", "Wifi SSID"));
		}
	}	
	vim_event_sleep(2000);
	return VIM_ERROR_USER_BACK;
}




VIM_ERROR_PTR WifiSettings(void)
{
	kbd_options_t options;

	// RDD 080520 - Added feature to use a pre-loaded .Ini file if LFD @WIFI
#if 0
	if (terminal.configuration.use_wifi == '3')
	{
		display_screen(DISP_IDLE_PCEFT, VIM_PCEFTPOS_KEY_NONE, "Reading file based\nWifi Parameters...", "");
		pceftpos_settings.tcp_parameters.UseWifi = VIM_TRUE;
		vim_event_sleep(5000);
		return VIM_ERROR_NONE;
	}
#endif

	options.timeout = FIVE_MIN_TIMEOUT;		// Can take a while to enter all the data
	options.min = 8;
	options.max = 32;
	options.entry_options = KBD_OPT_ALPHA | KBD_OPT_ALLOW_BLANK | KBD_OPT_DISP_ORIG;
	options.CheckEvents = VIM_FALSE;

	VIM_DBG_STRING(TERM_WIFI_SSID);

	VIM_ERROR_RETURN_ON_FAIL(display_data_entry(TERM_WIFI_SSID, &options, DISP_WIFI_SSID, VIM_PCEFTPOS_KEY_NONE, 0));
	VIM_DBG_STRING(TERM_WIFI_SSID);

	// Encrypt and save WIFI parameters
	ofb_encrypt_using_ssk(TERM_WIFI_SSID, WifiSSIDCfg);
	// Modify the Net XML file with the new password

	VIM_ERROR_RETURN_ON_FAIL(display_data_entry(TERM_WIFI_PWD, &options, DISP_WIFI_PASSWORD, VIM_PCEFTPOS_KEY_NONE, 0));
	VIM_DBG_STRING(TERM_WIFI_PWD);

	// Encrypt and save WIFI parameters
	ofb_encrypt_using_ssk(TERM_WIFI_PWD, WifiPWDCfg);
	// Modify the Net XML file with the new password
	VIM_ERROR_RETURN_ON_FAIL(WifiLoadEncryptedTag(WifiPWDCfg, XML_PSK_START, XML_PSK_END));

	//pceftpos_settings.tcp_parameters.UseWifi = VIM_TRUE;

	if (WifiRunFromParams() != VIM_ERROR_NONE)
	{
		VIM_ERROR_RETURN(VIM_ERROR_USER_BACK);
	}

	if (GetAnswer("Save Encrypted Password\nto Flash?\nEnter=YES Clr=NO") == VIM_TRUE)
	{
		SaveFileToFlash();
	}
	terminal_save();

	return VIM_ERROR_NONE;
}



S_MENU WifiSetupMenuXXX[] = {
	{ "Key Real Pwd/SSID", WifiSettings },
	{ "Pwd/SSID from USB", WifiFlashDrivePwd },
	{ "Pwd/SSID to VHQ", WifiCopyFileToLogs },
	{ "Pwd/SSID to USB", WifiFlashDriveUpPwd }
};

S_MENU WifiSetupMenu[] = {
	{ "Key Real Pwd/SSID", WifiSettings },
	{ "Pwd/SSID from USB", WifiFlashDrivePwd },
	{ "Pwd/SSID to VHQ", WifiCopyFileToLogs }
};

VIM_ERROR_PTR WifiSetupPwd(void)
{
	S_MENU *WifiSetup = (XXX_MODE) ? WifiSetupMenuXXX : WifiSetupMenu;
	VIM_SIZE Len = (XXX_MODE) ? VIM_LENGTH_OF(WifiSetupMenuXXX) : VIM_LENGTH_OF(WifiSetupMenu);
	VIM_ERROR_PTR res;

	while ((res = MenuRun(WifiSetup, Len, "Wifi Setup", VIM_FALSE, VIM_TRUE)) == VIM_ERROR_NONE);

	if (res == VIM_ERROR_USER_BACK)
	{
		res = VIM_ERROR_NONE;
	}
	return res;
}


VIM_ERROR_PTR WifiEnable(void)
{
	if(TERM_USE_WIFI != 1){
		TERM_USE_WIFI = 2;
	}
	CommsSetup(VIM_TRUE);
	//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Activation", "Enabled");
	terminal_save();
	return VIM_ERROR_NONE;
}

VIM_ERROR_PTR WifiDisable(void)
{
	TERM_USE_WIFI = 0;
	CommsSetup(VIM_TRUE);
	//display_screen(DISP_TMS_GENERAL_SCREEN, VIM_PCEFTPOS_KEY_NONE, "Wifi Activation", "Disabled");
	terminal_save();
	return VIM_ERROR_NONE;
}


S_MENU WifiMenu[] = { 	{ "Wifi Status", WifiStat },
                      	{ "Wifi Signal", WifiConfig },
						{ "Wifi Configuration", WifiSetupPwd },
						{ "Wifi Enable Check", WifiEnable },
						{ "Wifi Disable Check", WifiDisable }
};




#ifdef _WIN32



int net_wifiStart(int param)
{
    return 0;
}

VIM_ERROR_PTR WifiInit(void)
{
	return VIM_ERROR_NONE;
}

#else

#if 1
// ADK
VIM_ERROR_PTR WifiInit(void)
{
	return(WifiRunFromParams());
}
#else

// SDK
VIM_ERROR_PTR WifiInit(void)
{
	int ret;
	struct netIfconfig config;
	vim_mem_set(&config, 0, sizeof(struct netIfconfig));

	vim_strcpy(config.interface, "wlan0");
	vim_strcpy(config.suppliconf, NET_WPA_SUPPLICANT_DEFAULT_CONFIG_FILE);
	config.usedhcp = 1;
	config.activate = NET_ACTIVATE_ON;


	if ((ret = net_interfaceSet(config)) != 0)
	{
		pceft_debugf(pceftpos_handle.instance, "Wifi was unable to set configuration. ret=%d", ret);
		return VIM_ERROR_SYSTEM;
	}
	else
	{
		VIM_CHAR Buffer[200] = { 0 };

		// RDD 101022 JIRA PIRPIN-1995 New Wifi Params in default supplicant file
		WifiSetConfig(TERM_WIFI_SSID, TERM_WIFI_PWD, TERM_WIFI_PROT);
		vim_event_sleep(1000);

		if (net_wifiStart(1) != 0)
		{
			pceft_debugf(pceftpos_handle.instance, "Wifi chip failed to powerup. Error = %s", strerror(errno));
		}


		if ((ret = net_interfaceUp(NET_WIFI_DEFAULT, NET_IP_V4)) != 0)
		{
			pceft_debugf(pceftpos_handle.instance, "Wifi network failed to start. Error = %s", strerror(errno));
			vim_mem_set(&config, 0, sizeof(struct netIfconfig));
			config = net_interfaceGet(NET_WIFI_DEFAULT);
			pceft_debugf(pceftpos_handle.instance, "Sup File: %s Activate=%d", config.suppliconf, config.activate);

			ret = net_getLinkStatus(NET_WIFI_DEFAULT);
			pceft_debugf(pceftpos_handle.instance, "Wifi Error : ret = %s", strerror(errno));

			WifiGetCfg(Buffer);
			pceft_debug(pceftpos_handle.instance, Buffer);

		}
		else
		{
			pceft_debug(pceftpos_handle.instance, "Wifi started OK");
			WifiGetCfg(Buffer);
			pceft_debug(pceftpos_handle.instance, Buffer);
		}
	}
	return VIM_ERROR_NONE;
}
#endif





#endif


VIM_ERROR_PTR WifiSetup(void)
{
	if (TERM_USE_WIFI != 1) {
		TERM_USE_WIFI = 2;
	}
	//WifiInit();

	while (MenuRun(WifiMenu, VIM_LENGTH_OF(WifiMenu), "Wifi Control Panel", VIM_FALSE, VIM_TRUE) == VIM_ERROR_NONE);
	return VIM_ERROR_NONE;
}


#ifndef _WIN32

VIM_BOOL WifiCfgPresent(void)
{
    VIM_BOOL exists = VIM_FALSE;

    if( vim_file_exists( WifiSupFile, &exists ) == VIM_ERROR_NONE)
    {
        return exists;
    }
    return VIM_FALSE;
}


#endif