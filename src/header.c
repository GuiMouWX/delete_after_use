
#include <incs.h>
VIM_DBG_SET_FILE("TD");
#define COMMS_MODE_PSTN_ISDN        1
#define COMMS_MODE_PSTN_NIST        2
#define COMMS_MODE_PSTN_HIST        3
#define COMMS_MODE_GPRS             4
#define COMMS_MODE_ETHERNET         5
#define COMMS_MODE_PSTN_ISDN_1200   11
#define COMMS_MODE_PSTN_NIST_1200   12
#define COMMS_MODE_PSTN_HIST_1200   13
#define COMMS_MODE_PSTN_ISDN_V32    21
#define COMMS_MODE_PSTN_NIST_V32    22
#define COMMS_MODE_PSTN_HIST_V32    23


typedef struct clnp_header_t
{
  VIM_UINT8 id;
  VIM_UINT8 length;
  VIM_UINT8 version;
  VIM_UINT8 lifetime;
  VIM_UINT8 type;
  VIM_UINT8 message_length[2];
  VIM_UINT8 checksum[2];
  VIM_UINT8 destination_length;
  VIM_UINT8 destination[5];
  VIM_UINT8 source_length;
  VIM_UINT8 source[5];
}clnp_header_t;

VIM_ERROR_PTR header_get_type
(
  comms_params_t const*params,
  header_type_t* header_type_ptr
)
{
  *header_type_ptr= header_none;

  DBG_MESSAGE( "Header Type" );
#if 1 /*  TODO: Add header */
  if (comms_internal_mode())
  {
    switch (params->comms_type[0])
    {
      case COMMS_MODE_PSTN_ISDN:
      case COMMS_MODE_PSTN_ISDN_1200:
      case COMMS_MODE_PSTN_ISDN_V32:
        *header_type_ptr=header_clnp_identified;
        break;
        
      case COMMS_MODE_PSTN_HIST:
      case COMMS_MODE_PSTN_HIST_1200:
      case COMMS_MODE_PSTN_HIST_V32:
// RDD need TPDU header for GPRS
#if 0
	  case COMMS_MODE_GPRS:
        *header_type_ptr= header_clnp_non_identified;
        break;
#else
        *header_type_ptr= header_clnp_non_identified;
        break;

	  case COMMS_MODE_GPRS:
#endif


      default:
        *header_type_ptr= header_none;
    }
  }
#endif  
  DBG_VAR( *header_type_ptr );
  return VIM_ERROR_NONE;
}

VIM_ERROR_PTR header_get_size
(
  comms_params_t const *params, 
  VIM_SIZE* header_size_ptr
)
{
  header_type_t header_type;
  *header_size_ptr=0;

  VIM_ERROR_RETURN_ON_FAIL(header_get_type(params,&header_type));

  /*  TODO: Add TPDU */
  switch (header_type)
  {
    case header_clnp_identified:
    case header_clnp_non_identified:
      *header_size_ptr=sizeof(clnp_header_t);
      return VIM_ERROR_NONE;
      
    case header_none:
      return VIM_ERROR_NONE;

    default:
      VIM_ERROR_RETURN(VIM_ERROR_SYSTEM);
  }
}

VIM_ERROR_PTR header_set
(
  comms_params_t const *params, 
  VIM_UINT8 *header, 
  VIM_SIZE message_size,
  VIM_SIZE* header_size_ptr,
  VIM_SIZE header_max_size
)
{
  clnp_header_t *clnp;
  header_type_t header_type;
  VIM_UINT32 argent_id = 0;

  DBG_MESSAGE( "Header Set" );

  VIM_ERROR_RETURN_ON_FAIL(header_get_size(params,header_size_ptr));

  VIM_ERROR_RETURN_ON_FAIL(header_get_type(params,&header_type));

  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(*header_size_ptr,header_max_size);
  switch (header_type)
  {
    case header_clnp_identified:
/*  TODO:      argent_id = terminal.argent_id; */
    case header_clnp_non_identified:
      clnp = (clnp_header_t *)header;
      clnp->id = 0x81;
      clnp->length = sizeof(clnp_header_t);
      clnp->version = 1;
      clnp->lifetime = 20;
      clnp->type = 0x3C;
      clnp->message_length[0] = (message_size + clnp->length) / 256;
      clnp->message_length[1] = (message_size + clnp->length) % 256;
      clnp->checksum[0] = 0;
      clnp->checksum[1] = 0;
      clnp->destination_length = sizeof(clnp->destination);
      clnp->destination[0] = 0x49;
      asc_to_hex(params->sha, &clnp->destination[1], 8);
      clnp->source_length = sizeof(clnp->source);
      argent_id = (argent_id*256) + 1;
      clnp->source[0] = 0x49;
      clnp->source[1] = (VIM_UINT8)(argent_id >> 24);
      clnp->source[2] = (VIM_UINT8)(argent_id >> 16);
      clnp->source[3] = (VIM_UINT8)(argent_id >> 8);
      clnp->source[4] = (VIM_UINT8) argent_id;
      break;

    default:
      break;
  }
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR header_validate
(
  comms_params_t const *comms_parameters_ptr, 
  void const *message_ptr, 
  VIM_SIZE message_size,
  VIM_ERROR_PTR error
)
{
  header_type_t header_type;
  clnp_header_t *clnp;
  VIM_UINT32 header_size;
  /* get the expected header type */
  VIM_ERROR_RETURN_ON_FAIL(header_get_type(comms_parameters_ptr,&header_type));
  
  /* get the expected header length */
  VIM_ERROR_RETURN_ON_FAIL(header_get_size(comms_parameters_ptr,&header_size));
#if 0
  /* ensure that the message is large enough to contain the header */
  VIM_ERROR_RETURN_ON_FALSE(header_size<=message_size,VIM_ERROR_PDU);
#else
  //VIM_DBG_VAR(message_size);
  //VIM_DBG_VAR(header_size);
  //VIM_DBG_ERROR(error);
  if (message_size < header_size)
    return VIM_ERROR_PDU;
#endif
  //VIM_DBG_VAR(header_type);
  /* perform validation based on header type */
  switch (header_type)
  {
    case header_clnp_identified:
    case header_clnp_non_identified:
    {
      VIM_SIZE message_size_in_header;
      clnp = (clnp_header_t *)message_ptr;
      /* Verify the length indicator */
      message_size_in_header = (clnp->message_length[0] * 256) + clnp->message_length[1];
      if (message_size_in_header != message_size)
      {
       //VIM_DBG_VAR(message_size_in_header);
       //VIM_DBG_VAR(message_size);
       //VIM_DBG_VAR((*clnp));
        VIM_ERROR_RETURN(VIM_ERROR_CLNP_HEADER_LENGTH);
      }
      //VIM_DBG_DATA(clnp->source,6);
      /* Check for cnp error */
      if (VIM_ERROR_NONE == vim_mem_compare(clnp->source, "\x49\xFF\xFF\xFF\xFF", 5))
      {
        bcd_to_asc(((VIM_UINT8*) (message_ptr)) + header_size, txn.cnp_error, 4);
        return ERROR_CNP;
      }
      break;
    }

    case header_none:
      if (VIM_ERROR_PDU == error)
      {
        bcd_to_asc(((VIM_UINT8*) (message_ptr)) + header_size, txn.cnp_error, 4);
        return ERROR_CNP;
      }
      break;
      
    default:
      break;
  }

  return VIM_ERROR_NONE;
}

