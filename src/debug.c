#define VIM_DBG_DISABLE
#include <Incs.h>
VIM_DBG_SET_FILE("TW");

#if 1
typedef struct VIM_DBG *VIM_DBG_PTR;

typedef struct VIM_DBG_HANDLE
{
  /*@only@*//*@null@*/VIM_DBG_PTR instance;
}VIM_DBG_HANDLE;
#endif
/*----------------------------------------------------------------------------*/
typedef struct VIM_DBG
{
  VIM_UINT32 reserved;  
} VIM_DBG;

/*----------------------------------------------------------------------------*/
static VIM_DBG vim_drv_dbg={0};
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_dbg_open
(
  VIM_DBG_HANDLE *handle_ptr
)
{
  VIM_UINT16 status; 

  handle_ptr->instance=&vim_drv_dbg;

  /* return with no error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_dbg_close
(
  VIM_DBG_PTR self
)
{
  /* return with no error */ 
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_drv_dbg_putchar
(
  VIM_DBG_PTR self,
  VIM_TEXT source
)
{ 
  static VIM_TEXT buffer[100];
  static VIM_SIZE buffer_index=0;
  /* check on null pointer */ 
  VIM_ERROR_RETURN_ON_NULL(self);

  if (source=='\n')
  {
    /* insert a carraige return before each line feed */
    VIM_ERROR_RETURN_ON_FAIL(vim_drv_dbg_putchar(self,'\r'));
  }

  
  /* call ingenico  library function to send one byte over RS232 */
  if(buffer_index+1>=VIM_SIZEOF(buffer))
  {
    vim_pceftpos_debug(pceftpos_handle.instance,buffer);
    buffer_index=0;
  }
  buffer[buffer_index++]=source;
  if(source=='\n')
  {
    vim_pceftpos_debug(pceftpos_handle.instance,buffer);
    buffer_index=0;
  }

  /* return with no error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
void vim_drv_trace(/*@null@*/ const char* text)
{
  return;
}
void vim_drv_abort(void)
{
	for(;;);
}
VIM_ERROR_PTR vim_drv_dbg_get(const char *key, char *buf, VIM_INT32 size)
{
 return VIM_ERROR_NONE;
}

