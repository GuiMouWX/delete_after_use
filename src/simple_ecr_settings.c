#include <incs.h>

VIM_DBG_SET_FILE("TT");
static VIM_SIMPLE_ECR_PARAMETERS simple_ecr_settings;

/* For last pos command structure */
#define TAG_LAST_POS_RESPONSE_PENDING   (VIM_UINT8 *)"\xC0"
#define TAG_LAST_POS_CMD_CODE           (VIM_UINT8 *)"\xC1"
#define TAG_LAST_POS_SUB_CODE           (VIM_UINT8 *)"\xC2"

/* For pceft link layer structure */
#define TAG_PCEFT_LL_PORT               (VIM_UINT8 *)"\xC0"
#define TAG_PCEFT_LL_BAUD               (VIM_UINT8 *)"\xC1"
#define TAG_PCEFT_LL_DATA_BITS          (VIM_UINT8 *)"\xC2"
#define TAG_PCEFT_LL_STOP_BITS          (VIM_UINT8 *)"\xC3"
#define TAG_PCEFT_LL_PARITY             (VIM_UINT8 *)"\xC4"
#define TAG_PCEFT_LL_FLOW_CONTROL       (VIM_UINT8 *)"\xC5"
#define TAG_PCEFT_LL_PROTOCOL           (VIM_UINT8 *)"\xC6"

/* For base pceft settings values */
#define TAG_PCEFT_DEVICE_CODE           (VIM_UINT8 *)"\xC0"

#define TAG_PCEFT_LINK_LAYER      (VIM_UINT8 *)"\xF4"
#define TAG_PCEFT_DIALOG          (VIM_UINT8 *)"\xF5"
#define TAG_PCEFT_PRINTER         (VIM_UINT8 *)"\xF6"

/*----------------------------------------------------------------------------*/
static const char *ecr_settings_filename = VIM_FILE_PATH_LOCAL "ecr_cfg.dat";
/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_UART_PARAMETERS structure */
static VIM_MAP const simple_ecr_settings_uart_map[]=
{
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PORT,VIM_UART_PARAMETERS,port,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_BAUD,VIM_UART_PARAMETERS,baud_rate,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_DATA_BITS,VIM_UART_PARAMETERS,data_bits,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PARITY,VIM_UART_PARAMETERS,parity,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_STOP_BITS,VIM_UART_PARAMETERS,stop_bits,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_FLOW_CONTROL,VIM_UART_PARAMETERS,flow_control,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_END
};
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/* TLV mappings for VIM_PCEFTPOS_PARAMETERS structure */
static VIM_MAP simple_ecr_settings_map[]=
{
  VIM_MAP_DEFINE_SUB_MAP(TAG_PCEFT_LINK_LAYER,VIM_SIMPLE_ECR_PARAMETERS,uart_parameters,simple_ecr_settings_uart_map),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_LL_PROTOCOL,VIM_SIMPLE_ECR_PARAMETERS,protocol,&vim_map_accessor_integer_signed,VIM_NULL),
  VIM_MAP_DEFINE_FIELD(TAG_PCEFT_DEVICE_CODE,VIM_SIMPLE_ECR_PARAMETERS,device_code,&vim_map_accessor_char_string,VIM_NULL),    
  VIM_MAP_DEFINE_END
};
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_settings_set_defaults(VIM_SIMPLE_ECR_PARAMETERS *settings)
{
  /* set default physical layer parameters of the simple_ecr connection */
  /* set the default serial port */
  settings->uart_parameters.port = 1;
  /* set the default baud rate */
  settings->uart_parameters.baud_rate = VIM_UART_BAUD_9600;
  /* set the default data bits */
  settings->uart_parameters.data_bits = VIM_UART_DATA_BITS_8;
  /* set the default stop bits */
  settings->uart_parameters.stop_bits = VIM_UART_STOP_BITS_1;
  /* set the default parity */
  settings->uart_parameters.parity = VIM_UART_PARITY_NONE;
  /* set the default flow control */
  settings->uart_parameters.flow_control = VIM_UART_FLOW_CONTROL_NONE;

  /* set the default data link layer parameters of the simple_ecr connection */
  settings->protocol = VIM_SIMPLE_ECR_PROTOCOL_STX_DLE_ETX_LRC;
  /* set the default simple_ecr dialog parameters  */
  vim_mem_set(&settings->dialog, ' ', sizeof(settings->dialog));

  /* set the default simple_ecr printer parameters  */
  settings->printer.auto_print = '0';
  settings->printer.cut_receipt = '0';
  settings->printer.journal_receipt = '0';

  /* set the device code */  
  settings->device_code = '0';

  /* set the bank code */  
  settings->bank_code = 'N';

  /* set the hardware code */
  settings->hardware_code= '0';

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_settings_save(void)
{
  VIM_UINT8 buffer[200];
  VIM_TLV_LIST tlv_list;

  /* create a tlv buffer */
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tlv_list, buffer,VIM_SIZEOF(buffer)));

  /* convert the structure to TLV */  
  VIM_ERROR_RETURN_ON_FAIL(vim_map_to_tlv(simple_ecr_settings_map,&tlv_list,&simple_ecr_settings));
  
  /* save the file */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_set(ecr_settings_filename, buffer, tlv_list.total_length));

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_close(void)
{
  VIM_SIMPLE_ECR_PTR temp_instance;

  /* move the instance to a temporary variable */
  temp_instance = simple_ecr_handle.instance;
 //VIM_ 

  /* set the global instance to VIM_NULL */
  simple_ecr_handle.instance = VIM_NULL;

  /* destroy the instance */
  if( temp_instance != VIM_NULL )
    VIM_ERROR_RETURN_ON_FAIL(vim_simple_ecr_delete(temp_instance));
  
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_settings_reconfigure(void)
{
  /* check if there is already a simple_ecr instance */
  if (simple_ecr_handle.instance!=VIM_NULL)
  {
#if 0
    VIM_PCEFTPOS_PTR temp_instance;
    /* move the instance to a temporary variable */
    temp_instance=simple_ecr_handle.instance;
   //VIM_ 

    /* set the global instance to VIM_NULL */
    simple_ecr_handle.instance=VIM_NULL;

    /* destroy the instance */
    VIM_ERROR_RETURN_ON_FAIL(vim_simple_ecr_delete(temp_instance));
#endif
    VIM_ERROR_RETURN_ON_FAIL(simple_ecr_close());
  }
 //VIM_ 

  /* generate a new simple_ecr session */  
  VIM_ERROR_RETURN_ON_FAIL(vim_simple_ecr_new(&simple_ecr_handle,&simple_ecr_settings));
 //VIM_ 

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR simple_ecr_settings_load(void)
{
  VIM_BOOL exists;

  /* populate simple_ecr settings structure with defaults */
  VIM_ERROR_RETURN_ON_FAIL(simple_ecr_settings_set_defaults(&simple_ecr_settings));
  /* check if there is simple_ecr configuration file */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(ecr_settings_filename,&exists));
  if(exists)
  {
    VIM_SIZE buffer_size;
    VIM_UINT8 buffer[200];
    VIM_TLV_LIST tlv_list;
    /* read the file into a buffer */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get(ecr_settings_filename, buffer,&buffer_size, VIM_SIZEOF(buffer)));
    /* create a tlv buffer */
    VIM_ERROR_RETURN_ON_FAIL(vim_tlv_open(&tlv_list, buffer,buffer_size,buffer_size,VIM_FALSE));
    /* convert the buffer from tlv into a c struture */
    VIM_ERROR_RETURN_ON_FAIL(vim_map_from_tlv(simple_ecr_settings_map,&simple_ecr_settings,&tlv_list))
  }

  //VIM_DBG_MAP(simple_ecr_settings,simple_ecr_settings_map);
//VIM_ 
  /* reconfigure simple_ecr */ 
  VIM_ERROR_RETURN_ON_FAIL(simple_ecr_settings_reconfigure());
//VIM_ 

  return VIM_ERROR_NONE;
}
