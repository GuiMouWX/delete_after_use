#include <incs.h>

#if 0
VIM_DBG_SET_FILE("UV");
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_get_file_name
(
  VIM_UINT8 file_id, 
  VIM_TEXT *file_name_ptr, 
  VIM_SIZE file_name_max_size
)
{  
  VIM_ERROR_RETURN_ON_FAIL(vim_snprintf(file_name_ptr, file_name_max_size, FILE_NAME_TEMPLATE, file_id));
  return VIM_ERROR_NONE;  
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_open
(
  table_t * table_ptr,
  VIM_UINT8 table_id
)
{
  /* validate parameters */
  VIM_ERROR_RETURN_ON_NULL(table_ptr);
  /* get the name of the file */
  VIM_ERROR_RETURN_ON_FAIL(table_get_file_name(table_id, table_ptr->file_name, sizeof(table_ptr->file_name)));
  /* save the table id */
  table_ptr->table_id = table_id;
  /* check the header type */
  if (table_id == FILE_WBLT_TABLE)
  {
    /* get the header record */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(table_ptr->file_name,&table_ptr->header.wblt,VIM_SIZEOF(table_ptr->header.wblt),0));
    /* get the record size */
    VIM_ERROR_RETURN_ON_FAIL(vim_utl_bcd_to_size(table_ptr->header.wblt.record_length,VIM_SIZEOF(table_ptr->header.wblt.record_length)*2,&table_ptr->record_size));
    /* get the record count */
    VIM_ERROR_RETURN_ON_FAIL(vim_utl_bcd_to_size(table_ptr->header.wblt.record_count,VIM_SIZEOF(table_ptr->header.wblt.record_count)*2,&table_ptr->record_count));
  }
  else
  {
    /* get the header record */
    VIM_ERROR_RETURN_ON_FAIL(vim_file_get_bytes(table_ptr->file_name,&table_ptr->header.general,VIM_SIZEOF(table_ptr->header.general),0));
    /* get the record size */
    VIM_ERROR_RETURN_ON_FAIL(vim_utl_bcd_to_size(table_ptr->header.general.record_length,VIM_SIZEOF(table_ptr->header.general.record_length)*2,&table_ptr->record_size));
    /* get the record count */
    VIM_ERROR_RETURN_ON_FAIL(vim_utl_bcd_to_size(table_ptr->header.general.record_count,VIM_SIZEOF(table_ptr->header.general.record_count)*2,&table_ptr->record_count));
  }
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_read_record
(
  table_t *table_ptr, 
  VIM_SIZE record_index, 
  table_record_t *record_ptr
)
{
  VIM_SIZE record_offset;
  /* validate parameters */
  VIM_ERROR_RETURN_ON_NULL(table_ptr);
  VIM_ERROR_RETURN_ON_NULL(record_ptr);  
  VIM_ERROR_RETURN_ON_SIZE_OVERFLOW(record_index+1, table_ptr->record_count);

  /* compute the offset of the record within the file */  
  record_offset = ((record_index+1) * (table_ptr->record_size));

  /* Get the record */
  VIM_ERROR_RETURN_ON_NULL(vim_file_get_bytes(table_ptr->file_name, record_ptr,table_ptr->record_size,record_offset));

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_update_record
(
  VIM_UINT8 table_id, 
  VIM_SIZE record_index, 
  table_record_t *record_ptr
)
{
  VIM_SIZE record_offset;
  table_t table;
  /* open the table */
  VIM_ERROR_RETURN_ON_FAIL(table_open(&table, table_id));
  /* compute the offset of the record within the table */
  record_offset = ((record_index+1) * table.record_size);

  /* Write new record to the file */
  /* todo:(DAC) make this power off safe */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_set_bytes(table.file_name,record_ptr,table.record_size,record_offset));

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
bool table_exists(VIM_UINT8 table_id)
{
  char file_name[30];
  VIM_BOOL exists;
  /* get the name of the table */
  table_get_file_name(table_id, file_name, sizeof(file_name));

  /* check if the file exists */
  if (VIM_ERROR_NONE!=vim_file_exists(file_name,&exists))
  {
    return VIM_FALSE;
  }
  return exists;
}
/*----------------------------------------------------------------------------*/
/* Setup the default initial data for a table */
VIM_ERROR_PTR table_set_default_data(VIM_UINT8 table_id)
{
  VIM_UINT32 record_length, record_count;
  VIM_UINT8 default_buffer[500], apn_length;
  table_header_t *header;
  table_record_t *record;
  char file_name[30];
  bool gprs_enabled = GPRS_CAPABLE;

  vim_mem_clear(default_buffer, sizeof(default_buffer));
  table_get_file_name(table_id, file_name, sizeof(file_name));

  header = (table_header_t *)default_buffer;
  if (table_id == FILE_WBLT_TABLE)
    bin_to_bcd(TABLE_DEFAULT_VERSION, header->wblt.version_number, sizeof(header->wblt.version_number)*2);
  else if(table_id < FILE_CONFIG_TABLE || table_id > FILE_ICC_TABLE)
    bin_to_bcd(TABLE_OTHERS_DEFAULT_VERSION, header->general.version_number, sizeof(header->general.version_number)*2);
  else
    bin_to_bcd(TABLE_DEFAULT_VERSION, header->general.version_number, sizeof(header->general.version_number)*2);

  record_count = 0;
  switch (table_id)
  {
    case FILE_CONFIG_TABLE:
      record_length = sizeof(config_record_t);
      record = (table_record_t *)&default_buffer[(record_count+1)*record_length];
      /* Default general data record */
      record->config.record_type[0] = CONFIG_RECORD_GENERAL;
      record->config.data.general.connection_timeout = 45;
      record->config.data.general.transaction_timeout = 46;
      record->config.data.general.non_financial_timeout = 10;
      record->config.data.general.input_timeout = 30;
      record_count++;
      
      /* Default comms record */
      record = (table_record_t *)&default_buffer[record_length*2];
      record->config.record_type[0] = CONFIG_RECORD_COMMS;
      vim_mem_copy(record->config.data.comms.apn, "\xC1\x0A"NAB_DEFAULT_APN, 12);
      record_count++;
      break;

    case FILE_ACQUIRER_TABLE:
      record_length = sizeof(acquirer_record_t);
      record = (table_record_t *)&default_buffer[(record_count+1)*record_length];
      record->acquirer.record_type[0] = 1;
      record->acquirer.acquirer_index[0] = NAB_ACQUIRER;
      record->acquirer.institution_name_idx[0] = 1;
      vim_mem_copy(record->acquirer.aiic, NAB_DEFAULT_AIIC, 6);
      record->acquirer.keyset[0] = 1;
      vim_mem_copy(record->acquirer.app_list, NAB_DEFAULT_APP_LIST, 2);
      record_count++;
      break;

    case FILE_COMMUNICATION_TABLE:
      record_length = sizeof(comms_record_t);
      record = (table_record_t *)&default_buffer[(record_count+1)*record_length];

      /* NAB GPRS record */
      if (gprs_enabled)
      {
        record->comms.record_type[0] = COMMS_RECORD_COMMS;
        record->comms.data.params.acquirer_idx[0] = NAB_ACQUIRER;
        record->comms.data.params.access_idx[0] = 1;
        vim_mem_set(record->comms.data.params.host_phone, ' ', 5);
        record->comms.data.params.comms_type[0] = COMMS_MODE_GPRS;
        vim_mem_copy(record->comms.data.params.sha, NAB_DEFAULT_SHA, 8);
        vim_mem_copy(record->comms.data.params.ip, NAB_DEFAULT_IP, 4);
        vim_mem_copy(record->comms.data.params.port, NAB_DEFAULT_PORT, 2);
        vim_mem_clear(record->comms.data.params.user_id, sizeof(record->comms.data.params.user_id));
        vim_mem_clear(record->comms.data.params.password, sizeof(record->comms.data.params.password));
        record->comms.data.params.apn_idx[0] = 0;
        record_count++;
        record = (table_record_t *)&default_buffer[(record_count+1)*record_length];
      }

      /* NAB dialup record */
      record->comms.record_type[0] = COMMS_RECORD_COMMS;
      record->comms.data.params.acquirer_idx[0] = NAB_ACQUIRER;
      record->comms.data.params.access_idx[0] = gprs_enabled ? 2 : 1;
      vim_mem_copy(record->comms.data.params.host_phone, NAB_DEFAULT_PHONE, 5);
      record->comms.data.params.comms_type[0] = COMMS_MODE_PSTN_NIST;
      vim_mem_set(record->comms.data.params.sha, ' ', 8);
      vim_mem_clear(record->comms.data.params.ip, 4);
      vim_mem_clear(record->comms.data.params.port, 2);
      record->comms.data.params.apn_idx[0] = 0;
      record_count++;

      /* TMS gprs comms record */
      record = (table_record_t *)&default_buffer[(record_count+1)*record_length];
      record->comms.record_type[0] = COMMS_RECORD_COMMS;
      record->comms.data.params.acquirer_idx[0] = TMS_ACQUIRER;
      record->comms.data.params.access_idx[0] = 1;
      vim_mem_set(record->comms.data.params.host_phone, ' ', 5);
      record->comms.data.params.comms_type[0] = 4;
      vim_mem_set(record->comms.data.params.sha, ' ', 8);
      vim_mem_copy(record->comms.data.params.ip, TMS_DEFAULT_IP, 4);
      vim_mem_copy(record->comms.data.params.port, TMS_DEFAULT_PORT, 2);
      vim_mem_copy(record->comms.data.params.user_id, TMS_DEFAULT_USER_ID, sizeof(TMS_DEFAULT_USER_ID)-1);
      vim_mem_copy(record->comms.data.params.password, TMS_DEFAULT_PASSWORD, sizeof(TMS_DEFAULT_PASSWORD)-1);
      record->comms.data.params.apn_idx[0] = 1;
      record_count++;

      /* Apn record for tms */
      record = (table_record_t *)&default_buffer[(record_count+1)*record_length];
      record->comms.record_type[0] = COMMS_RECORD_APN;
      record->comms.data.apn_details.apn_idx[0] = 1;
      vim_mem_set(record->comms.data.apn_details.apn, ' ', sizeof(record->comms.data.apn_details.apn));
      vim_mem_copy(record->comms.data.apn_details.apn, TMS_DEFAULT_APN, sizeof(TMS_DEFAULT_APN));
      apn_length = record->comms.data.apn_details.apn[1];
      /* Userid and password in apn record */
      vim_mem_copy(&record->comms.data.apn_details.apn[2 + apn_length], TMS_DEFAULT_USER_ID, sizeof(TMS_DEFAULT_USER_ID)-1);
      vim_mem_copy(&record->comms.data.apn_details.apn[2 + apn_length + 30], TMS_DEFAULT_PASSWORD, sizeof(TMS_DEFAULT_PASSWORD)-1);
      record_count++;
      /* END TMS TESTING */
      break;

    default:
      record_length = sizeof(table_header_t);
      break;
  }

  
  if (table_id == FILE_WBLT_TABLE)
  {
    bin_to_bcd(record_length, header->wblt.record_length, sizeof(header->wblt.record_length)*2);
    bin_to_bcd(record_count, header->wblt.record_count, sizeof(header->wblt.record_count)*2);
    vim_mem_clear(header->wblt.filler, sizeof(header->wblt.filler));
  }
  else
  {
    bin_to_bcd(record_length, header->general.record_length, sizeof(header->general.record_length)*2);
    bin_to_bcd(record_count, header->general.record_count, sizeof(header->general.record_count)*2);
    header->general.delimeter = 0;
  }
  vim_file_set(file_name, default_buffer, record_length*(record_count+1));

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Check if the table record provided is matched by the key provided */
bool table_check_key(VIM_UINT8 table, table_key_t *key, table_record_t *record)
{
  VIM_UINT8 i, digit;

  switch (table)
  {
    case FILE_CONFIG_TABLE:
      return (record->config.record_type[0] == key->config.record_type[0]);

    case FILE_ACQUIRER_TABLE:
      return (record->acquirer.acquirer_index[0] == key->acquirer.index[0]);

    case FILE_CARD_NAME_TABLE:
      if (record->card_name.record_type[0] == key->card_name.record_type[0])
      {
        if (record->card_name.record_type[0] == 1)
          return (record->card_name.data.name.card_name_idx[0] == key->card_name.index[0]);
        if (record->card_name.record_type[0] == 2)
          return (record->card_name.data.group.group_idx[0] == key->card_name.index[0]);
      }
      return FALSE;

    case FILE_INST_NAME_TABLE:
      if (record->institution_name.record_type[0] == 1)
        return (record->institution_name.inst_name_idx[0] == key->inst_name.index[0]);
      return FALSE;

    case FILE_CPAT:
      if (record->cpat.record_type[0] == 1)
      {
        /* Now check the prefix */
        for (i=0; i < sizeof(key->cpat.card_prefix); i++)
        {
          if (i % 2) 
          {
            digit = record->cpat.card_prefix[i/2] & 0x0F;
          }
          else
          {
            digit = (record->cpat.card_prefix[i/2] & 0xF0) >> 4;
          }
          if (digit != 0x0F)  
          {
            /* F matches anything */
            digit += '0';
            if (key->cpat.card_prefix[i] != digit)
            {
              break;
            }
          }
        }
        
        if (i == sizeof(key->cpat.card_prefix))
        {
          return TRUE;
        }
      }
      return FALSE;

    case FILE_COMMUNICATION_TABLE:
      if (record->comms.record_type[0] != key->comms.record_type[0])
        return FALSE;
      if (key->comms.record_type[0] == COMMS_RECORD_COMMS) /* comms record must match acquirer index and access index too */
      {
        if (key->comms.record_idx[0] != record->comms.data.params.acquirer_idx[0])
          return FALSE;
        if (key->comms.access_idx[0] != record->comms.data.params.access_idx[0])
          return FALSE;
      }
      if (key->comms.record_type[0] == COMMS_RECORD_APN) /* APN record must match apn index too */
        return (key->comms.record_idx[0] == record->comms.data.apn_details.apn_idx[0]);
      return TRUE;

    case FILE_APPLICATION_ID_TABLE:
      if (record->appid.record_type[0] != key->appid.record_type[0])
        return FALSE;
      if (vim_mem_compare(record->appid.data.aid.aid_value, &key->appid.AID.value,key->appid.AID.size)!=VIM_ERROR_NONE)
        return FALSE;
      return TRUE;


    case FILE_ICC_TABLE:
      if (record->icc.record_type[0] != key->icc.record_type[0])
        return FALSE;

      if (key->icc.record_type[0] == ICC_RECORD_DATA)
      {
        if (vim_mem_compare(record->icc.detail.data.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_DDOL)
      {
        if (vim_mem_compare(record->icc.detail.ddol.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_TDOL)
      {
        if (vim_mem_compare(record->icc.detail.tdol.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_TDOL)
      {
        if (vim_mem_compare(record->icc.detail.tdol.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_CONTACTLESS)
      {
        if (vim_mem_compare(record->icc.detail.data_contactless.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_DDOL_CONTACTLESS)
      {
        if (vim_mem_compare(record->icc.detail.ddol_contactless.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_TDOL_CONTACTLESS)
      {
        if (vim_mem_compare(record->icc.detail.tdol_contactless.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      else if (record->icc.record_type[0] == ICC_RECORD_UDOL_CONTACTLESS)
      {
        if (vim_mem_compare(record->icc.detail.udol_contactless.rid, key->icc.rid, VIM_SIZEOF(key->icc.rid))!=VIM_ERROR_NONE)
          return FALSE;
        return TRUE;
      }
      /* unknown record type */
      return FALSE;
  }
  return FALSE;
}
/*----------------------------------------------------------------------------*/
/* Search any table for the record referenced by the key value provided. returns index found if successful or negative error code if it fails */
VIM_ERROR_PTR table_search_old
(
  VIM_UINT8 table_id, 
  table_key_t *key, 
  table_record_t *record_ptr,
  VIM_SIZE * record_index_ptr
)
{
  VIM_UINT32 record_index=0;
  table_t table={0};
  table_record_t record={0};

  /* init */
  if( record_index_ptr != VIM_NULL )
    *record_index_ptr = 0x00;
  if( record_ptr != VIM_NULL )
    vim_mem_clear( record_ptr, VIM_SIZEOF(table_record_t));
  
  /* check if the caller supplied a record buffer */
  if(record_ptr==VIM_NULL)  
  {
    /* use a local record buffer if no record buffer is supplied */
    record_ptr=&record;
  }

  /* get table details */
  VIM_ERROR_RETURN_ON_FAIL(table_open(&table,table_id));
  /* iterate through table records */
  for (record_index=0; record_index < table.record_count; record_index++)
  {
    /* the next record from the table */
    VIM_ERROR_RETURN_ON_FAIL(table_read_record(&table, record_index, record_ptr));
    /* Check if the key matches that from the table */
    if (table_check_key(table_id, key, record_ptr))
    {
      /* check if the caller supplied an index pointer address */
      if(record_index_ptr!=VIM_NULL)
      {
        /* return the record index to the caller */
        *record_index_ptr=record_index;
      }
      return VIM_ERROR_NONE;
    }
  }

  VIM_DBG_MESSAGE("NO TABLE ENTRY");
  VIM_DBG_VAR((*key));

  VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
}
/* improve performance of table search */
VIM_ERROR_PTR table_search
(
  VIM_UINT8 table_id, 
  table_key_t *key, 
  table_record_t *record_ptr,
  VIM_SIZE * record_index_ptr
)
{
  VIM_UINT32 record_index=0;
  table_t table={0};
  table_record_t record={0};
  VIM_SIZE record_size;
  VIM_FILE_HANDLE table_file_handle;
  VIM_ERROR_PTR error;

  /* init */
  if( record_index_ptr != VIM_NULL )
    *record_index_ptr = 0x00;
  if( record_ptr != VIM_NULL )
    vim_mem_clear( record_ptr, VIM_SIZEOF(table_record_t));
  
  /* check if the caller supplied a record buffer */
  if(record_ptr==VIM_NULL)  
  {
    /* use a local record buffer if no record buffer is supplied */
    record_ptr=&record;
  }

  /* get table details */
  VIM_ERROR_RETURN_ON_FAIL(table_open(&table,table_id));

  /* no record, return error */
  if( table.record_count == 0 )
    return VIM_ERROR_NOT_FOUND;

  /* open file */
  vim_mem_clear( &table_file_handle, VIM_SIZEOF(VIM_FILE_HANDLE));
  error = vim_file_open (&table_file_handle, table.file_name, VIM_FALSE, VIM_TRUE, VIM_FALSE);
  if( error != VIM_ERROR_NONE )
  {
    vim_file_close (table_file_handle.instance);
    VIM_ERROR_RETURN( error );
  }
  /* read first index record and ingnore it */
  error = vim_file_read (table_file_handle.instance, record_ptr, &record_size, table.record_size);
  if( error != VIM_ERROR_NONE )
  {
    VIM_ERROR_RETURN_ON_FAIL( vim_file_close (table_file_handle.instance));
    VIM_ERROR_RETURN( error );
  }
  
  for (record_index=0; record_index < table.record_count; record_index++)
  {
    /* read next record */
    error = vim_file_read (table_file_handle.instance, record_ptr, &record_size, table.record_size);
    if( error != VIM_ERROR_NONE )
    {
      VIM_ERROR_RETURN_ON_FAIL( vim_file_close (table_file_handle.instance));
      VIM_ERROR_RETURN( error );
    }
    if( record_size != table.record_size )
    {
      VIM_ERROR_RETURN_ON_FAIL( vim_file_close (table_file_handle.instance));
      VIM_ERROR_RETURN( VIM_ERROR_SYSTEM );
    }
    /* Check if the key matches that from the table */
    if (table_check_key(table_id, key, record_ptr))
    {
      /* check if the caller supplied an index pointer address */
      if(record_index_ptr!=VIM_NULL)
      {
        /* return the record index to the caller */
        *record_index_ptr=record_index;
      }
      /* close file before return */
      vim_file_close( table_file_handle.instance );
      return VIM_ERROR_NONE;
    }
  }
  
  VIM_DBG_MESSAGE("NO TABLE ENTRY");
  VIM_DBG_VAR((*key));

  /* close file */
  VIM_ERROR_RETURN_ON_FAIL( vim_file_close( table_file_handle.instance ));

  VIM_ERROR_RETURN(VIM_ERROR_NOT_FOUND);
}

/*----------------------------------------------------------------------------*/
/** Read the receive timeout from config table. Return the timeout value in milliseconds */
VIM_MILLISECONDS table_get_rx_timeout(void)
{
  VIM_UINT32 timeout;
  table_key_t key;
  table_record_t record;
  /* set a default time out */
  timeout = RECEIVE_TIMEOUT;
  /* set the search key */
  key.config.record_type[0] = CONFIG_RECORD_GENERAL;
  /* search the config file for the general record */
  if (table_search(FILE_CONFIG_TABLE, &key, &record,VIM_NULL) ==VIM_ERROR_NONE)
  {
    /* extract the timeout from the general record */
    timeout = record.config.data.general.transaction_timeout * 1000;
  }

  return timeout;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_get_connection_timeout
(
  VIM_MILLISECONDS* timeout_ptr
)
{
  table_key_t key;
  table_record_t record;
  *timeout_ptr=((VIM_MILLISECONDS)30000);

  if (table_search(FILE_CONFIG_TABLE, &key, &record,VIM_NULL) ==VIM_ERROR_NONE)
  {
    *timeout_ptr = record.config.data.general.connection_timeout * 1000;
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_get_apn(char apn[23], VIM_UINT8 apn_index)
{
  table_key_t key={0};
  table_record_t record={0};

  key.comms.record_idx[0] = apn_index;
  key.comms.record_type[0] = COMMS_RECORD_APN;
  /* todo:(DAC) fix magic number */  
  vim_mem_clear(apn, 23);
  
  if (table_search(FILE_COMMUNICATION_TABLE, &key, &record,VIM_NULL)==VIM_ERROR_NONE)
  {
    DBG_MESSAGE("GOT COMMS APN");
    vim_mem_copy(apn, &record.comms.data.apn_details.apn[2], MIN(record.comms.data.apn_details.apn[1], 22));
    DBG_STRING(apn);
  }
  else
  {
    DBG_MESSAGE("NO COMMS APN TRY GENERAL");
    key.config.record_type[0] = CONFIG_RECORD_COMMS;
    VIM_ERROR_RETURN_ON_FAIL(table_search(FILE_CONFIG_TABLE, &key, &record,VIM_NULL));
    vim_mem_copy(apn, &record.config.data.comms.apn[2], MIN(record.config.data.comms.apn[1], 22));
    VIM_DBG_VAR(record.config.data.comms.apn);
    VIM_DBG_STRING(apn);
  }
  
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Retrieve the userid and password as NULL terminated strings */
VIM_ERROR_PTR table_get_userid_password(comms_params_t *params, char user_string[31], char password_string[11])
{
  VIM_UINT8 apn_length;
  table_record_t record={0};
  table_key_t key={0};
  vim_mem_clear(user_string, 31);
  vim_mem_clear(password_string, 31);

  /* Check if there is a separate APN record containing the userid and password */
  if (params->apn_idx[0] != 0)
  {
    key.comms.record_idx[0] = params->apn_idx[0];
    key.comms.record_type[0] = COMMS_RECORD_APN;
    if (table_search(FILE_COMMUNICATION_TABLE, &key, &record,VIM_NULL) ==VIM_ERROR_NONE)
    {
      apn_length = record.comms.data.apn_details.apn[1];
      /* APN is ASN.1 encoded, then followed by 30 bytes userid, 10 bytes password (not ASN.1 encoded) */
      vim_mem_copy(user_string, &record.comms.data.apn_details.apn[2 + apn_length], strlen_space_padded(&record.comms.data.apn_details.apn[2 + apn_length], 30));
      vim_mem_copy(password_string, &record.comms.data.apn_details.apn[2 + apn_length + 30], strlen_space_padded(&record.comms.data.apn_details.apn[2 + apn_length + 30], 10));
      return VIM_ERROR_NONE;
    }
  }

  /* No APN record, retrieve from regular record */
  vim_mem_copy(user_string, params->user_id, strlen_space_padded(params->user_id, sizeof(params->user_id)));
  vim_mem_copy(password_string, params->password, strlen_space_padded(params->password, sizeof(params->password)));
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_set_apn(char apn[23], VIM_UINT8 apn_index)
{
  VIM_SIZE record_index=0;
  table_key_t key={0};
  table_record_t record={0};
  VIM_TLV_LIST tags={0};
  VIM_UINT8 tag_buffer[VIM_SIZEOF(record.comms.data.apn_details.apn)]={0};

  /* create a tlv buffer for the APN */
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_create(&tags, tag_buffer, sizeof(tag_buffer)));
  /* store the APN as TLV */
  VIM_ERROR_RETURN_ON_FAIL(vim_tlv_add_bytes(&tags, (VIM_UINT8*)TAG_APN, apn, vim_strlen(apn)));

  /* set the search key to look for an index within the apn records */
  key.comms.record_type[0] = COMMS_RECORD_APN;
  key.comms.record_idx[0] = apn_index;

  /* search for the APN record in the communications table */
  if (table_search(FILE_COMMUNICATION_TABLE, &key, &record,&record_index)==VIM_ERROR_NONE)
  {
    DBG_MESSAGE("SET COMMS APN");
    /* update the apn component of the reocrd */
    vim_mem_copy(record.comms.data.apn_details.apn, tags.pstart, MIN(tags.total_length, sizeof(record.comms.data.apn_details.apn)));
    /* commit the updated record back to the file*/
    VIM_ERROR_RETURN_ON_FAIL(table_update_record(FILE_COMMUNICATION_TABLE, record_index, &record));
  }
  else
  {
    DBG_MESSAGE("NO COMMS APN TRY GENERAL");
    /* set the search key */
    key.config.record_type[0] = CONFIG_RECORD_COMMS;

    /* search the config table */
    VIM_ERROR_RETURN_ON_FAIL(table_search(FILE_CONFIG_TABLE, &key, &record,&record_index));

    DBG_MESSAGE("SET GENERAL APN");
    /* update the apn component of the reocrd */
    vim_mem_copy(record.config.data.comms.apn, tags.pstart, MIN(tags.total_length, sizeof(record.config.data.comms.apn)));
    /* commit the updated record back to the file*/
    VIM_ERROR_RETURN_ON_FAIL(table_update_record(FILE_CONFIG_TABLE, record_index, &record));
  }

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/* Update the version number in a table but leave the rest of the contents in-tact */
VIM_ERROR_PTR table_set_version
(
  VIM_UINT8 table_id, 
  VIM_UINT32 version
)
{
  table_t table;
  VIM_SIZE header_size;

  /* get the table information */
  VIM_ERROR_RETURN_ON_FAIL(table_open(&table,table_id));
  /* determine the table format */
  if (table_id == FILE_WBLT_TABLE)
  {
    /* Get the size of the header */
    header_size=VIM_SIZEOF(table.header.wblt.version_number);
    /* convert the version to bcd and store it in the header */
    bin_to_bcd(version, table.header.wblt.version_number, header_size*2);
  }
  else
  {
    /* Get the size of the header */
    header_size=VIM_SIZEOF(table.header.general.version_number);
    /* convert the version to bcd and store it in the header */
    bin_to_bcd(version, table.header.general.version_number, header_size*2);
  }
  
  /* commit the header record */
  VIM_ERROR_RETURN_ON_FAIL(vim_file_set_bytes(table.file_name,&table.header,header_size,0));
  
  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_get_institution_name(VIM_UINT8 host, char inst_name[13])
{
  VIM_SIZE name_size;
  table_key_t key;
  table_record_t record;

  /* set the search key */
  key.inst_name.index[0] = host;
  /* search the insitution name file */
  if (table_search(FILE_INST_NAME_TABLE, &key, &record,VIM_NULL)==VIM_ERROR_NONE)
  {
    /* get the length of the host name */    
    /* todo:(DAC) fix magic numbers */
    name_size = strlen_space_padded(record.institution_name.inst_name_short, 12);
    /* copy the host name */    
    vim_mem_copy(inst_name, record.institution_name.inst_name_short, name_size);
    /* null terminate the host name */
    inst_name[name_size] = 0;
  }
  else
  {
    /* set a default host name */
    /* todo:(DAC) fix magic numbers */
    VIM_ERROR_RETURN_ON_FAIL(vim_strncpy(inst_name, "NAB", 13));
  }

  /* return without error */
  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR table_get_record
(
  VIM_UINT8 table_id, 
  VIM_SIZE record_index, 
  table_record_t *record_ptr
)
{
  table_t table;
  /* validate parameters */  
  VIM_ERROR_RETURN_ON_NULL(record_ptr);
  /* get table information */
  VIM_ERROR_RETURN_ON_FAIL(table_open(&table, table_id));
  /* read the record */
  VIM_ERROR_RETURN_ON_FAIL(table_read_record(&table,record_index,record_ptr));
  /* return without error */
  return VIM_ERROR_NONE;
}

#endif
