#include <incs.h>
VIM_ERROR_PTR term_host_gprs_attach(comms_params_t *params, VIM_UINT32 connect_timeout)
{
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR term_host_comms_connect(comms_params_t *params, VIM_UINT32 connect_timeout)
{

  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR term_host_comms_transmit
(
  void const* source,
  VIM_SIZE source_size
)
{
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR term_host_comms_receive
(
  void * destination, 
  VIM_SIZE * destination_size,
  VIM_SIZE destination_max_size, 
  VIM_MILLISECONDS timeout
)
{
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR term_host_comms_disconnect(void)
{
 
  return VIM_ERROR_NONE;
}


