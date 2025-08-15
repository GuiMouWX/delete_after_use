/** \file keyLoader.c
  * \brief Keyloader
  * \todo
  * \note
  */


#include <incs.h>
#include "keyloader.h"

VIM_DBG_SET_FILE("TZ");
//#define FINSIM_TEST_HOST


static KEYLOADER_COMMS keyloaderComms =
{
    {
        KEYLOADER_PORT_NUMBER,
        KEYLOADER_COMMS_BAUD_RATE,
        KEYLOADER_COMMS_DATA_BITS,
        KEYLOADER_COMMS_STOP_BITS,
        KEYLOADER_COMMS_PARITY,
        KEYLOADER_COMMST_FLOW_CONTROL,
        VIM_UART_OVER_USB_NO //MGC 220219 - UART OVER USB init missing here ( RDD - get rid of magic number for something meaningful here )
    },
    &vim_uart_protocol_stx_dle_etx_lrc,
{ (void *)VIM_NULL },
{ (void *)VIM_NULL },
(void *)VIM_NULL
};

/*static KEYLOADER_COMMS keyloaderUSBComms =
{
    {
        KEYLOADER_USB_PORT_NUMBER,
        KEYLOADER_COMMS_BAUD_RATE,
        KEYLOADER_COMMS_DATA_BITS,
        KEYLOADER_COMMS_STOP_BITS,
        KEYLOADER_COMMS_PARITY,
        KEYLOADER_COMMST_FLOW_CONTROL
    },
    &vim_uart_protocol_stx_dle_etx_lrc,
{ (void *)VIM_NULL },
{ (void *)VIM_NULL },
(void *)VIM_NULL
};*/



/*----------------------------------------------------------------------------*/
//static VIM_UART_HANDLE      keyLoaderHandle;
static VIM_BOOL             keyloadingDone = VIM_FALSE;
static VIM_BOOL             keyLoaderSignedOn = VIM_FALSE;
static VIM_UINT8            skTCU_modulus[2][256];
static VIM_UINT8            skTCU_exponent[2][256];
static VIM_UINT16           skTCU_modulus_size[2];
static VIM_UINT16           skTCU_exponent_size[2];
// RDD 060714 SPIRA:7567 - keep a global error and don't allow commands to be completed out of sequence
static VIM_ERROR_PTR		ErrorCode = VIM_ERROR_NONE;
/*----------------------------------------------------------------------------*/

typedef struct KEYLOADER_COMMAND_RECORD
{
  VIM_UINT16 command_code;
  VIM_ERROR_PTR (*handler)
  (
    VIM_BUFFER_CONST* request_buffer,
    VIM_BUFFER* response_buffer
  );
} KEYLOADER_COMMAND_RECORD;

/*----------------------------------------------------------------------------*/


static VIM_ERROR_PTR keyLoaderSignon( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSignoff( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderGetSerialNo( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetPPID( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetSSKMAN_PKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetPKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetSKPPTMS( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );
static VIM_ERROR_PTR keyLoaderSetVasKey( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer );

#ifdef _DEBUG

void K_MESSAGE(VIM_CHAR *Message)
{
    VIM_DBG_MESSAGE(Message);
}

#else

void K_MESSAGE(VIM_CHAR *Message)
{


    if (vim_adkgui_is_active()) {
        display_screen(DISP_IDLE, VIM_PCEFTPOS_KEY_NONE, Message);
    }
    else
    {
        VIM_GRX_VECTOR offset;
        //Create a debug file
        offset.x = offset.y = 0;
        vim_lcd_clear();
    }

}

#endif


/**
 * determine the value of a base64 encoding character
 *
 * @param base64char the character of which the value is searched
 * @return the value in case of success (0-63), -1 on failure
 */
int _base64_char_value(char base64char)
 {
    if (base64char >= 'A' && base64char <= 'Z')
        return base64char-'A';
    if (base64char >= 'a' && base64char <= 'z')
        return base64char-'a'+26;
    if (base64char >= '0' && base64char <= '9')
        return base64char-'0'+2*26;
    if (base64char == '+')
        return 2*26+10;
    if (base64char == '/')
        return 2*26+11;
    return -1;
}

/*----------------------------------------------------------------------------*/
int _base64_decode_triple(char quadruple[4], /*@out@*/VIM_UINT8 *result)
 {
    int i, triple_value, bytes_to_decode = 3, only_equals_yet = 1;
    int char_value[4];
    int ret = 0;

    for (i=0; i<4; i++)
    {
        char_value[i] = _base64_char_value(quadruple[i]);
    }
    /* check if the characters are valid */
    for (i=3; i>=0; i--)
    {
        if (char_value[i]<0)
        {
            if (only_equals_yet && quadruple[i]=='=')
            {
                /* we will ignore this character anyway, make it something
                * that does not break our calculations */
                char_value[i]=0;
                bytes_to_decode--;
                continue;
            }
            return ret;
        }
        /* after we got a real character, no other '=' are allowed anymore */
        only_equals_yet = 0;
    }

    /* if we got "====" as input, bytes_to_decode is -1 */
    if (bytes_to_decode < 0)
        bytes_to_decode = 0;

    /* make one big value out of the partial values */
    triple_value = char_value[0];
    triple_value *= 64;
    triple_value += char_value[1];
    triple_value *= 64;
    triple_value += char_value[2];
    triple_value *= 64;
    triple_value += char_value[3];

    /* break the big value into bytes */
    for (i=bytes_to_decode; i<3; i++)
        triple_value /= 256;
    for (i=bytes_to_decode-1; i>=0; i--)
    {
        result[i] = triple_value%256;
        triple_value /= 256;
    }

    return bytes_to_decode;
}


/**
 * decode base64 encoded data
 *
 * @param source the encoded data (zero terminated)
 * @param target pointer to the target buffer
 * @param targetlen length of the target buffer
 * @return length of converted data on success, -1 otherwise
 */
VIM_UINT16 vim_tcu_decodeB64( VIM_UINT8 const *source, VIM_UINT16 targetlen, VIM_UINT8 *target )
//size_t base64_decode(char *source, unsigned char *target, size_t targetlen)
 {
    VIM_UINT8 *src, *tmpptr;
    VIM_UINT8   quadruple[4];
    VIM_UINT8   tmpresult[3];
    VIM_INT16   i, tmplen = 3;
    VIM_UINT16  converted = 0;
    VIM_UINT8   data[2048];

	// RDD 301112 - Need this to work really !
	vim_mem_clear( data, VIM_SIZEOF(data) );

    /* concatinate '===' to the source to handle unpadded base64 data */
    src = (VIM_UINT8 *)&data[0];

    vim_strncpy((VIM_CHAR *)src, (VIM_CHAR *)source, vim_strlen((VIM_CHAR *)source));
    vim_strncat((VIM_CHAR *)src, "====", 4);
    tmpptr = src;

    /* convert as long as we get a full result */
    while (tmplen == 3)
    {
        /* get 4 characters to convert */
        for (i=0; i<4; i++)
        {
            /* skip invalid characters - we won't reach the end */
            while (*tmpptr != '=' && _base64_char_value(*tmpptr)<0)
                tmpptr++;

            quadruple[i] = *(tmpptr++);
        }

        /* convert the characters */
        tmplen = _base64_decode_triple((char *)quadruple, (VIM_UINT8 *)&tmpresult[0]);

        /* check if the fit in the result buffer */
        if (targetlen < tmplen)
        {
            return 0;
        }

        /* put the partial result in the result buffer */
        vim_mem_copy(target, tmpresult, tmplen);
        target += tmplen;
        targetlen -= tmplen;
        converted += tmplen;
    }

    return converted;
}
/*----------------------------------------------------------------------------*/
/*- END OF BASE 64 Routines                                                  -*/
/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR keyLoaderTask(  VIM_TASK_CONTEXT_PTR context_ptr,  VIM_UNUSED(VIM_RTC_UPTIME* ,next_event))
{
    VIM_ERROR_PTR error;
    VIM_UINT8 buffer[500];
	VIM_UINT8 *Ptr = buffer;
    VIM_SIZE receive_size;

	//DBG_POINT;
    /* Look for STX */
    VIM_DBG_POINT;
    if ((error = vim_uart_expect_byte(keyloaderComms.uartHandle.instance, VIM_UINT8_STX, 500)) == VIM_ERROR_NONE)
    {
        VIM_DBG_MESSAGE("STX received");
    }

    /* Check for timeout */
    if (VIM_ERROR_TIMEOUT == error )
    {
        /* If no bytes have been received then return without error */
        VIM_DBG_POINT;
        return VIM_ERROR_NONE;
    }

    /* Check for unexpected first byte */
	vim_mem_clear(&buffer, sizeof(buffer));

	if(VIM_ERROR_MISMATCH == error)
    {
#if 0
        /* Flush the rest of the message */
        vim_uart_receive(keyloaderComms.uartHandle.instance, buffer, &receive_size, VIM_SIZEOF(buffer), 100);
        VIM_DBG_VAR(buffer);
        VIM_ERROR_RETURN_ON_FAIL(vim_uart_flush(keyloaderComms.uartHandle.instance));
         /* If unexpect byte has been received then return without error */
        return VIM_ERROR_NONE;
#else
		VIM_ERROR_RETURN_ON_FAIL( vim_uart_expect_byte(keyloaderComms.uartHandle.instance, VIM_UINT8_STX, 0 ));
#endif
	}

	/* Process and validate BCC */
     //VIM_DBG_MESSAGE("ATTEMPT UART RECEIVE");
    /* Recieve the message using the selected protocol */
    error = keyloaderComms.protocolMethods->receive(keyloaderComms.uartHandle.instance,
                                                    Ptr, &receive_size, VIM_SIZEOF(buffer),
                                                    KEYLOADER_TIMEOUT_INTERCHARACTER);

    /* Send ACK or NAK appropriately */
    if( VIM_ERROR_NONE != error )
    {
        /* Send NAK */
        K_MESSAGE("RX ERR: Sending NAK");
        VIM_ERROR_RETURN_ON_FAIL(vim_uart_send_byte(keyloaderComms.uartHandle.instance, VIM_UINT8_NAK));
        /* Flush the rest of the message */
         //VIM_DBG_MESSAGE("Flushing message");
        VIM_ERROR_RETURN_ON_FAIL(vim_uart_flush(keyloaderComms.uartHandle.instance));
        /* Return without error */
        return VIM_ERROR_NONE;
    }


    /* Send ACK */

	K_MESSAGE("RX OK: Sending ACK");
     //VIM_DBG_MESSAGE("Sending ACK");
    VIM_ERROR_RETURN_ON_FAIL(vim_uart_send_byte(keyloaderComms.uartHandle.instance, VIM_UINT8_ACK));


    /* Process the message */
    VIM_ERROR_RETURN_ON_FAIL(keyLoaderGettingMessage(buffer, receive_size));


    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
/*-   This is only here for my testing as I wanted a ui and there wasnt one  -*/
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/

#define DISPLAY_ESCAPE_STR                    "\x1B"
#define DISPLAY_ESCAPE_STR_FONT_SMALL         DISPLAY_ESCAPE_STR"1"
#define DISPLAY_ESCAPE_STR_FONT_MEDIUM        DISPLAY_ESCAPE_STR"2"
#define DISPLAY_ESCAPE_STR_FONT_LARGE         DISPLAY_ESCAPE_STR"3"
#define DISPLAY_ESCAPE_STR_ALIGNMENT_LEFT     DISPLAY_ESCAPE_STR"L"
#define DISPLAY_ESCAPE_STR_ALIGNMENT_RIGHT    DISPLAY_ESCAPE_STR"R"
#define DISPLAY_ESCAPE_STR_INVERSE_LINE       DISPLAY_ESCAPE_STR"I"
#define DISPLAY_ESCAPE_STR_ENTRY_POINT        DISPLAY_ESCAPE_STR"E"
#define DISPLAY_ESCAPE_STR_ALIGNMENT_BASE     DISPLAY_ESCAPE_STR"B"
#define DISPLAY_ESCAPE_STR_BITMAP_ARROW       DISPLAY_ESCAPE_STR"A"
#define DISPLAY_ESCAPE_STR_BITMAP_LOGO        DISPLAY_ESCAPE_STR"O"
/*----------------------------------------------------------------------------*/
#define DISPLAY_ESCAPE_CHR                    '\x1B'
#define DISPLAY_ESCAPE_CHR_FONT_SMALL         '1'
#define DISPLAY_ESCAPE_CHR_FONT_MEDIUM        '2'
#define DISPLAY_ESCAPE_CHR_FONT_LARGE         '3'
#define DISPLAY_ESCAPE_CHR_ALIGNMENT_LEFT     'L'
#define DISPLAY_ESCAPE_CHR_ALIGNMENT_RIGHT    'R'
#define DISPLAY_ESCAPE_CHR_INVERSE_LINE       'I'
#define DISPLAY_ESCAPE_CHR_ENTRY_POINT        'E'
#define DISPLAY_ESCAPE_CHR_ALIGNMENT_BASE     'B'
#define DISPLAY_ESCAPE_CHR_BITMAP_ARROW       'A'
#define DISPLAY_ESCAPE_CHR_BITMAP_LOGO        'O'
typedef enum DISPLAY_ALIGNMENT
{
  DISPLAY_ALIGNMENT_LEFT,
  DISPLAY_ALIGNMENT_CENTER,
  DISPLAY_ALIGNMENT_RIGHT
}DISPLAY_ALIGNMENT;


/*static*/ VIM_ERROR_PTR onScreen(const VIM_TEXT *text, VIM_INT8 line, VIM_BOOL init)
{
  VIM_TEXT const* font_ptr=DISPLAY_ESCAPE_STR_FONT_LARGE;
  VIM_CHAR line_buf[90];
  VIM_CHAR space_buf[90];
  VIM_UINT8 i;
  VIM_UINT8 j;
  VIM_GRX_VECTOR screen_size;
  VIM_GRX_VECTOR text_size;
  VIM_GRX_VECTOR offset;
  VIM_BOOL inverse_display = VIM_FALSE;
  DISPLAY_ALIGNMENT align = DISPLAY_ALIGNMENT_CENTER;
  VIM_BOOL base_align = VIM_FALSE;
  VIM_BOOL logo_display = VIM_FALSE;
  VIM_BOOL first_align = VIM_FALSE;
  VIM_BOOL second_align = VIM_FALSE;
//  static const char           *file1 = VIM_FILE_PATH_LOCAL "message.pcx";
  VIM_GRX_CANVAS_PTR canvas_ptr;
//  VIM_UINT8                   buffer[200000];
//  VIM_UINT32                  bufferSize=200000;


  VIM_ERROR_RETURN_ON_FAIL(vim_lcd_get_canvas(&canvas_ptr));
/*
  if (init)
  {
        vim_file_get(file1,buffer,&bufferSize, VIM_SIZEOF(buffer));
        offset.x =0;
        offset.y = 55;
        vim_grx_canvas_put_pcx_bitmap( canvas_ptr, offset, buffer, bufferSize);
  }
*/
  /* draw some white on red text */
  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_background_colour(canvas_ptr,vim_grx_colour_red));
  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_set_foreground_colour(canvas_ptr,vim_grx_colour_white));

  vim_mem_set(space_buf, ' ', sizeof(space_buf));
  space_buf[sizeof(space_buf) - 1] = 0;

  VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_size_in_pixels(canvas_ptr,&screen_size));

  offset=vim_grx_vector(0,200 + line * 10);


  for (i=0,j=0; ; i++)
  {
    if ((text[i] == '\n') || (text[i] == 0) || (second_align))
    {
      if ( logo_display == VIM_TRUE )
      {
#if 0
        VIM_GRX_BITMAP const * logo;
        VIM_ERROR_RETURN_ON_FAIL(logo_get(&logo));
        vim_grx_canvas_put_bitmap_at_pixel(canvas_ptr, offset, logo);
#endif
        /* Reset logo display flag */
        logo_display = VIM_FALSE;
/*        offset.y += (logo->size.y-4);*/
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
          case DISPLAY_ALIGNMENT_CENTER:
            offset.x = (screen_size.x - text_size.x) / 2;
            break;

          case DISPLAY_ALIGNMENT_RIGHT:
            offset.x = screen_size.x - text_size.x;
            break;

          case DISPLAY_ALIGNMENT_LEFT:
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
          font_ptr= DISPLAY_ESCAPE_STR_FONT_LARGE;
        first_align = VIM_FALSE;
        second_align = VIM_FALSE;
       //VIM_DBG_VAR(first_align);
       //VIM_DBG_VAR(second_align);
      }
      else
      {
        font_ptr= DISPLAY_ESCAPE_STR_FONT_LARGE;
      }
      align = DISPLAY_ALIGNMENT_CENTER;
      offset.x = 0;

      j=0;
      if (text[i] == 0)
        break;
    }
    else
    {
      if (text[i] == DISPLAY_ESCAPE_CHR)
      {
        i++;
        switch (text[i])
        {
          case DISPLAY_ESCAPE_CHR_FONT_SMALL:
            font_ptr= DISPLAY_ESCAPE_STR_FONT_SMALL;
            break;

          case DISPLAY_ESCAPE_CHR_FONT_MEDIUM:
            font_ptr= DISPLAY_ESCAPE_STR_FONT_MEDIUM;
            break;

          case DISPLAY_ESCAPE_CHR_FONT_LARGE:
            font_ptr= DISPLAY_ESCAPE_STR_FONT_LARGE;

            break;

          case DISPLAY_ESCAPE_CHR_ALIGNMENT_LEFT:
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
            align = DISPLAY_ALIGNMENT_LEFT;
            break;

          case DISPLAY_ESCAPE_CHR_ALIGNMENT_RIGHT:
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
            align = DISPLAY_ALIGNMENT_RIGHT;
            break;

          case DISPLAY_ESCAPE_CHR_INVERSE_LINE:
            inverse_display = VIM_TRUE;
            break;

          case DISPLAY_ESCAPE_CHR_ALIGNMENT_BASE:
            base_align = VIM_TRUE;
            break;

          case DISPLAY_ESCAPE_CHR_ENTRY_POINT:
            line_buf[j] = 0;
            VIM_ERROR_RETURN_ON_FAIL(vim_grx_canvas_get_text_size(canvas_ptr,line_buf,&text_size));
            VIM_ERROR_RETURN(VIM_ERROR_NOT_IMPLEMENTED);

          case DISPLAY_ESCAPE_CHR_BITMAP_ARROW:
            break;

          case DISPLAY_ESCAPE_CHR_BITMAP_LOGO:
            logo_display = VIM_TRUE;
            break;
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


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR RunKeyload( void )
{
	// RDD 050412 SET RSA if keys loaded
	terminal.logon_state = rsa_logon;

    K_MESSAGE("Open UART");
	vim_event_sleep( 1000 );

	// RDD 021117 Added for VX690 Project
	if( IsTerminalType( "VX690" ))
	{
		VIM_DBG_MESSAGE( "Setting Keyloader Port to Native USB" );
		keyloaderComms.uartParameters.port = KEYLOADER_USB_PORT_NUMBER;
	}

    /* Open the specified serial port */
    if( vim_uart_new( &keyloaderComms.uartHandle, &keyloaderComms.uartParameters ) != VIM_ERROR_NONE )
	{
	    K_MESSAGE("Open UART Failed");
		vim_event_sleep( 2000 );
	}

     VIM_DBG_MESSAGE("CREATE POLLING TASK");
    /* Create a task to poll the serial port */

	VIM_ERROR_RETURN_ON_FAIL(vim_uart_flush(keyloaderComms.uartHandle.instance));

	K_MESSAGE("Keyloader Waiting...");


    vim_task_new(&keyloaderComms.taskHandle, keyLoaderTask, keyloaderComms.contextPtr, VIM_TASK_PRIORITY_POS);
    while(keyloadingDone != VIM_TRUE)
    {
		if(ErrorCode!=VIM_ERROR_NONE)
		{
			K_MESSAGE("Keyload Fail\n\rReboot and Start Again");
		}
        vim_event_sleep(100);
    }
    vim_task_delete(keyloaderComms.taskHandle.instance);
    vim_uart_delete(keyloaderComms.uartHandle.instance);
    if (vim_tcu_generate_new_batch_key() != VIM_ERROR_NONE)
    {
        DBG_MESSAGE("Gen new Batch Key failed");
    }
    else
    {
        DBG_MESSAGE("Gen new Batch Key OK");
    }
    //offset.x = offset.y = 0;
    K_MESSAGE("Keyload - Completed");
    return VIM_ERROR_NONE;
}


VIM_ERROR_PTR keyLoad(void)
{
    //VIM_GRX_VECTOR offset;
    /* check if initial keys have been downloaded into terminal */
    /* we check the ppid for the terminal and if we have one we */
    /* can assume that the keys are loaded   */
#ifndef _DEBUG
	VIM_DES_BLOCK ppid;
    if( VIM_ERROR_NONE == vim_tcu_get_ppid( tcu_handle.instance_ptr, &ppid ))
    {
      if( vim_mem_compare( &ppid, "\x00\x00\x00\x00\x00\x00\x00\x00", 8 ) != VIM_ERROR_NONE )
      {
		DBG_MESSAGE( "PPID detected - Skip keyload");
        return VIM_ERROR_NONE;
      }
    }
	else
	{
		VIM_DBG_VAR( ppid );
	}
#endif

#ifdef _DEBUG
	DBG_MESSAGE( "Load test Keys" );
	VIM_ERROR_RETURN_ON_FAIL( WOW_load_test_keys () );	//BD Don't want test keys on debug version,only WIN32??
	//RDD - i need test keys for DEBUG on PinPad too so i can test with same config on both platforms easily.
	return VIM_ERROR_NONE;
#endif

	VIM_ERROR_RETURN_ON_FAIL(RunKeyload());
	return VIM_ERROR_NONE;
}



/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR keyLoaderGettingMessage(VIM_UINT8 * request, VIM_SIZE request_size)
{
    VIM_UINT8 response[1024];	// RDD - guess at maximum response size
    VIM_SIZE response_size;
    VIM_ERROR_PTR error = VIM_ERROR_NONE;
    VIM_UINT8 retries=0;

    /* Default the response size to zero*/
    response_size = 0;

    VIM_DBG_MESSAGE("keyLoaderProcessMessage");

    /* If ACKing, pass the message to the legacy_pos module */
    VIM_ERROR_RETURN_ON_FAIL(keyLoaderProcessMessage(request,
      request_size, response, &response_size, VIM_SIZEOF(response)));

    /* If a response was passed back, send it back to the keyloader */
    if( response_size > 0 )
    {
        /* Send the message back to the keyloader */
        VIM_ERROR_RETURN_ON_FAIL(keyloaderComms.protocolMethods->send(
            keyloaderComms.uartHandle.instance, &response, response_size));

        //Wait for the ack
        retries = 0;
        while((retries++ < 3 ) && (error != VIM_ERROR_NONE ))
        {
            error = vim_uart_expect_byte(keyloaderComms.uartHandle.instance, VIM_UINT8_ACK, 2000);

            /* Check for timeout */
            if (VIM_ERROR_TIMEOUT == error )
            {
                 VIM_DBG_MESSAGE("---Timeout waiting for ack");
            }

            /* Check for unexpected first byte */
            if(VIM_ERROR_MISMATCH == error)
            {
                 //VIM_DBG_MESSAGE("---Did not get an ack");
                /* Flush the rest of the message */
                VIM_ERROR_RETURN_ON_FAIL(vim_uart_flush(keyloaderComms.uartHandle.instance));
                /* resend */
                VIM_ERROR_RETURN_ON_FAIL(keyloaderComms.protocolMethods->send(
                  keyloaderComms.uartHandle.instance, &response, response_size));
            }
            if (error == VIM_ERROR_NONE )
            {
                VIM_DBG_MESSAGE("-- Got the ack --");
                retries = 0;
            }
        }
    }

    if (retries > 3 )
    {
        return VIM_ERROR_TIMEOUT;
    }
    else
    {
        /* Return without error */
        return VIM_ERROR_NONE;
    }
}


/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_buffer_const_get_ascii_as_hex /* We are going from ascii representation of hex digits to binary representation of hex digits (halving the number of bytes) */
(
  VIM_BUFFER_CONST * self_ptr,
  /*@out@*/void * dest_ptr,
  VIM_SIZE dest_size
)
{
  /* Initialise the destionation ptr */
  vim_mem_clear(dest_ptr, dest_size);

  /* Return if the expected size is too large */
  VIM_ERROR_RETURN_ON_INDEX_OVERFLOW( self_ptr->buffer_index + dest_size*2,
    self_ptr->buffer_size );

  /* Get the whole ascii buffer in hex */
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_ascii_to_hex(dest_ptr,
    (VIM_CHAR *)self_ptr->buffer_ptr+self_ptr->buffer_index, dest_size));

  /* Increment the current position in the buffer by the whole size of the
    field before conversion */
  self_ptr->buffer_index += dest_size*2;

  return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
VIM_ERROR_PTR vim_buffer_add_hex_as_ascii
(
  VIM_BUFFER * self_ptr,
  void const * source_ptr,
  VIM_SIZE source_size
)
{
  /* Return if the expected size is too large */
  VIM_ERROR_RETURN_ON_INDEX_OVERFLOW( self_ptr->buffer_index + source_size*2,
    self_ptr->buffer_size);

  /* Add the whole hex buffer as ascii */
  VIM_ERROR_RETURN_ON_FAIL(vim_utl_hex_to_ascii((VIM_CHAR *)self_ptr->buffer_ptr+
    self_ptr->buffer_index, source_ptr, source_size*2));

  /* Increment the current position in the buffer by the whole size of the
    field after conversion */
  self_ptr->buffer_index += source_size*2;

  return VIM_ERROR_NONE;
}


/*----------------------------------------------------------------------------*/

VIM_ERROR_PTR keyLoaderProcessMessage(  VIM_UINT8 const * message_data,
                                        VIM_SIZE        message_data_size,
                     /*@null@*//*@out@*/VIM_UINT8       * response,
                     /*@null@*//*@out@*/VIM_SIZE        * response_size,
                                        VIM_SIZE        response_size_max
                                     )
{
  static KEYLOADER_COMMAND_RECORD const keyLoaderCommandTable[]=
  {
    /* Initialisation processing functions */
    { VIM_KEYLOADER_COMMAND_CODE_SIGNON,        keyLoaderSignon },
    { VIM_KEYLOADER_COMMAND_CODE_SIGNOFF,       keyLoaderSignoff },
    { VIM_KEYLOADER_COMMAND_GET_SERIAL_NO,      keyLoaderGetSerialNo },
    { VIM_KEYLOADER_COMMAND_SET_PPID,           keyLoaderSetPPID },
    { VIM_KEYLOADER_COMMAND_SET_KTCU,           keyLoaderSetKTCU },
    { VIM_KEYLOADER_COMMAND_SET_SSKMAN_PKTCU,   keyLoaderSetSSKMAN_PKTCU },
    { VIM_KEYLOADER_COMMAND_SET_PKTCU_EXPONENT, keyLoaderSetPKTCU },
    { VIM_KEYLOADER_COMMAND_SET_SKPPTMS,        keyLoaderSetSKPPTMS },
    { VIM_KEYLOADER_COMMAND_SET_VAS_KEY,        keyLoaderSetVasKey },

  };
  /* Index to iterate through command table */
  VIM_UINT8 command_table_index;
  /* Buffer to hold the message */
  VIM_BUFFER_CONST request_buffer;
  /* Buffer to hold the response*/
  VIM_BUFFER response_buffer;
  /* Error returned from handler */
  VIM_ERROR_PTR error;
  VIM_UINT8 command;

  *response_size = 0;

  /* Display the message request header */
  VIM_DBG_DATA(message_data, message_data_size);

  /* Initialise the buffer to please splint */
  vim_mem_clear(&response, *response_size);

  /* Create a buffer structure to allow unpacking of messages */
  VIM_ERROR_RETURN_ON_FAIL(vim_buffer_const_open(&request_buffer,
    message_data, message_data_size));

  /* Create a buffer structure to allow packing of messages */
  VIM_ERROR_RETURN_ON_FAIL(vim_buffer_start(&response_buffer,
    response, response_size_max));


 /* Extract the command */
 request_buffer.buffer_size += 2;
  VIM_ERROR_RETURN_ON_FAIL(vim_buffer_const_get_ascii_as_hex(&request_buffer,
    &command, VIM_SIZEOF(command)));

 //VIM_DBG_VAR(command);

  /* Add the command code to the response message */
  VIM_ERROR_RETURN_ON_FAIL(vim_buffer_add_hex_as_ascii(&response_buffer,
    &command, VIM_SIZEOF(command)));


  VIM_DBG_DATA(response_buffer.buffer_ptr, VIM_SIZEOF(response_buffer.buffer_index));

  /* Iterate through the command table to determine which request code we have received */
  for(command_table_index = 0;
      command_table_index < VIM_LENGTH_OF(keyLoaderCommandTable);
      command_table_index++)
  {
    VIM_UINT8 table_command;

    /* Get the current command from the command table */
    table_command = keyLoaderCommandTable[command_table_index].command_code;

    /* Check if the current command is that which we have received */
    if( command != table_command )
      continue;

    /* Display that a hit was made in the command code list */
     //VIM_DBG_MESSAGE("HIT MATCH IN COMMAND CODE TABLE");

    /* Process the request */
    error = keyLoaderCommandTable[command_table_index].handler(&request_buffer,
      &response_buffer);

	// SPIRA:7567
	ErrorCode = error;

    VIM_ERROR_RETURN_ON_FAIL(error);


    /* Copy the response to the response (OUT) parameter */
    vim_mem_copy(response, response_buffer.buffer_ptr, response_buffer.buffer_index);

    /* Verify the contents of the response buffer */
    VIM_DBG_DATA(response_buffer.buffer_ptr, response_buffer.buffer_size);

    /* Point the response size to the size of the buffer */
    *response_size = response_buffer.buffer_size;

    /* Return without error */
    return VIM_ERROR_NONE;
  }

  /* We have received a function code that we do not recognise. Return this
  to the Keyloader */

  return VIM_ERROR_NOT_IMPLEMENTED;
}


/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR keyLoaderSignon( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    /* Keyloader signon, the first step in the process */
    KEYLOADER_SIGNON_COMMAND    *command = (KEYLOADER_SIGNON_COMMAND*)request_buffer->buffer_ptr;
    KEYLOADER_SIGNON_RESPONSE   *response = (KEYLOADER_SIGNON_RESPONSE*)response_buffer->buffer_ptr;
    VIM_SIZE                    bufferSize;
    VIM_UINT8                   buffer[500];
    static const char           *keyloaderFile = VIM_FILE_PATH_LOCAL "wow_keyloader";
    VIM_BOOL                    exists;

    VIM_DBG_MESSAGE("keyLoaderSignon");

    //Clean out the buffer
    vim_mem_set(skTCU_modulus, 0, VIM_SIZEOF(skTCU_modulus));
    vim_mem_set(skTCU_exponent, 0, VIM_SIZEOF(skTCU_modulus));
    skTCU_exponent_size[0] = 0;
    skTCU_modulus_size[0] = 0;
    skTCU_exponent_size[1] = 0;
    skTCU_modulus_size[1] = 0;

    //Fill out the error response in case we drop out
    vim_mem_copy(response->responseCode, "02", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, command->command, VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);

    keyLoaderSignedOn = VIM_FALSE;

    if ( keyLoaderSignedOn == VIM_TRUE )
    {
        //We are already logged on
		K_MESSAGE("Already Signed ON");
        vim_mem_copy(response->responseCode, "03", VIM_SIZEOF(response->responseCode));
    }
    else
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_file_exists(keyloaderFile,&exists));
        if(exists)
        {
            /* Read the file*/
            //The file exists

            VIM_ERROR_RETURN_ON_FAIL(vim_file_get(keyloaderFile,buffer,&bufferSize, VIM_SIZEOF(buffer)));
            if (vim_mem_compare(buffer, command->currentPassword, VIM_SIZEOF(command->currentPassword)) == VIM_ERROR_NONE)
            {
                //We have already got the new password and it matches the current password
                vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));
                VIM_ERROR_RETURN_ON_FAIL(vim_file_set(keyloaderFile,(void *)command->newPassword, VIM_SIZEOF(command->newPassword)));

				K_MESSAGE("Signed N OK");
                keyLoaderSignedOn = VIM_TRUE;
				// SPIRA:7567
				ErrorCode = VIM_ERROR_NONE;
            }
            else
            {
                //the password is not correct
                vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
                keyLoaderSignedOn = VIM_FALSE;
            }
        }
        else
        {
            //We save the password as the file does not exist so we accept the password
            VIM_ERROR_RETURN_ON_FAIL(vim_file_set(keyloaderFile,(void *)command->newPassword, VIM_SIZEOF(command->newPassword)));
            vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));
            keyLoaderSignedOn = VIM_TRUE;

        }
    }

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);
    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR keyLoaderSignoff( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    /* Just sign off the keyloader */
     //VIM_DBG_MESSAGE("keyLoaderSignoff");

    //Clean out the buffer
    vim_mem_set(skTCU_modulus, 0, VIM_SIZEOF(skTCU_modulus));
    vim_mem_set(skTCU_exponent, 0, VIM_SIZEOF(skTCU_modulus));
    skTCU_exponent_size[0] = 0;
    skTCU_modulus_size[0] = 0;
    skTCU_exponent_size[1] = 0;
    skTCU_modulus_size[1] = 0;


	// SPIRA:7567
	VIM_ERROR_RETURN_ON_FAIL( ErrorCode );


    //There is no response for a sign off
    keyLoaderSignedOn = VIM_FALSE;
    keyloadingDone = VIM_TRUE;
    response_buffer->buffer_size = 0;

    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderGetSerialNo( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_GETSERIAL_NUMBER_COMMAND      *command = (KEYLOADER_GETSERIAL_NUMBER_COMMAND*)request_buffer->buffer_ptr;
    KEYLOADER_GETSERIAL_NUMBER_RESPONSE     *response = (KEYLOADER_GETSERIAL_NUMBER_RESPONSE*)response_buffer->buffer_ptr;
    VIM_UINT8                               serialNo[12];
    VIM_UINT8                               ptid[9];

     //VIM_DBG_MESSAGE("keyLoaderGetSerialNo");

    //Fill out the error respons ein case we drop out
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, command->command, VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);

    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
        VIM_DBG_MESSAGE("Keyloader Not signed on")
		ErrorCode = VIM_ERROR_SYSTEM;
        return VIM_ERROR_SYSTEM;
    }
	DBG_POINT;
    VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_raw_serial_number( (VIM_TEXT*)serialNo, VIM_SIZEOF(serialNo) ) );
   //VIM_DBG_VAR(serialNo);
    VIM_ERROR_RETURN_ON_FAIL( vim_terminal_get_physical_terminal_id( (VIM_TEXT*)ptid, VIM_SIZEOF(ptid) ) );
   //VIM_DBG_VAR(ptid);

    vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->ptid, ptid, VIM_SIZEOF(response->ptid));
    vim_mem_copy(response->serialNo, serialNo, VIM_SIZEOF(response->serialNo));

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command) \
                    + VIM_SIZEOF(response->serialNo) + VIM_SIZEOF(response->ptid);

    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderSetPPID( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_SETPPID_COMMAND      command; //= (KEYLOADER_SETPPID_COMMAND*)request_buffer->buffer_ptr;
    KEYLOADER_SETPPID_RESPONSE     *response = (KEYLOADER_SETPPID_RESPONSE*)response_buffer->buffer_ptr;

    VIM_DBG_MESSAGE("keyLoaderSetPPID");
    /* Clear the structure */
    vim_mem_clear(&command, VIM_SIZEOF(command));


	// SPIRA:7567
	VIM_ERROR_RETURN_ON_FAIL( ErrorCode );
    //As this is only for a single byte value

    command.acquirer = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;

    VIM_ERROR_RETURN_ON_FAIL(vim_buffer_const_get_ascii_as_hex(request_buffer,
      &command.ppid.bytes, VIM_SIZEOF(command.ppid)));

    //Fill out the error respons ein case we drop out
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, "A1", VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);


    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
         VIM_DBG_MESSAGE("Keyloader Not signed on");

        return VIM_ERROR_NONE;
    }

    VIM_DBG_VAR(command.acquirer);

    if ( command.acquirer == 1 )
    {
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_sponsor(tcu_handle.instance_ptr,SPONSOR));
        VIM_DBG_VAR( command.ppid );
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_ppid(tcu_handle.instance_ptr, &command.ppid ));
        /* test go get ppid */
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_get_ppid(tcu_handle.instance_ptr, &command.ppid));

		K_MESSAGE("PPID Loaded OK");
		VIM_DBG_MESSAGE( "PPID loaded OK" );
        VIM_DBG_VAR( command.ppid );
        vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));
    }
    else
    {
      VIM_ERROR_RETURN( VIM_ERROR_OS );
    }

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);
    VIM_DBG_VAR(response_buffer->buffer_size);

    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderSetKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_SETSKTCU_COMMAND     command;
    KEYLOADER_SETSKTCU_RESPONSE    *response = (KEYLOADER_SETSKTCU_RESPONSE*)response_buffer->buffer_ptr;
    VIM_UINT8                      key[600];
    VIM_UINT16                     outB64KeySize=0;

    VIM_DBG_MESSAGE("keyLoaderSetKTCU");
    /* Clear the structure */
    vim_mem_clear(&command, VIM_SIZEOF(command));


	// SPIRA:7567
	VIM_ERROR_RETURN_ON_FAIL( ErrorCode );


    //As this is only for a single byte value
    command.acquirer = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;
    //Read if we are doing the modulus or exponent
    command.modulus = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;


    //Fill out the error response in case we drop out due to an error
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, "A2", VIM_SIZEOF(response->command));


    //Don't do anything unless we are signed on
    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
        VIM_DBG_MESSAGE("Keyloader Not signed on")
        return VIM_ERROR_SYSTEM;
    }

    // RDD - no need to process acquirer code as only one acquirer
   VIM_DBG_VAR(command.acquirer);

    //Convert the key from B64 format to a block of data
    //Move the data ptr to the start of the key data
    request_buffer->buffer_ptr+=request_buffer->buffer_index;
    VIM_DBG_DATA(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2);
    outB64KeySize = vim_tcu_decodeB64(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2, &key[0]);
    VIM_DBG_DATA(key, outB64KeySize);

    //Save the key data for the modulus or exponent
    if ( command.modulus == 1 )
    {
        VIM_DBG_MESSAGE("MODULUS");
        vim_mem_copy(&skTCU_modulus[command.acquirer][0], key, outB64KeySize);
        skTCU_modulus_size[command.acquirer] = outB64KeySize;

    }
    else
    {
        VIM_DBG_MESSAGE("EXPONENT");
        vim_mem_copy(&skTCU_exponent[command.acquirer][0], key, outB64KeySize);
        skTCU_exponent_size[command.acquirer] = outB64KeySize;
    }

    if (skTCU_exponent_size[command.acquirer] > 0  && skTCU_modulus_size[command.acquirer] > 0 )
    {
        VIM_DBG_MESSAGE( "Set skTCU:===========" );
        VIM_DBG_DATA(skTCU_modulus[command.acquirer],skTCU_modulus_size[command.acquirer] );
        VIM_DBG_DATA(skTCU_exponent[command.acquirer], skTCU_exponent_size[command.acquirer]);
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skTCU(tcu_handle.instance_ptr, &skTCU_modulus[command.acquirer][0], &skTCU_exponent[command.acquirer][0], skTCU_modulus_size[command.acquirer], skTCU_exponent_size[command.acquirer]));
        vim_mem_clear(&skTCU_modulus[command.acquirer][0], VIM_SIZEOF(skTCU_modulus[command.acquirer]));
        vim_mem_clear(&skTCU_exponent[command.acquirer][0], VIM_SIZEOF(skTCU_exponent[command.acquirer]));

		VIM_DBG_MESSAGE( "Set skTCU:=========== DONE !" );

		K_MESSAGE("skTCU Loaded OK");

        skTCU_exponent_size[command.acquirer]= 0;
        skTCU_modulus_size[command.acquirer] = 0;
    }
    //Set our return code to ok
    vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);
   //VIM_DBG_VAR(response_buffer->buffer_size);

    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderSetSSKMAN_PKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_SSKMAN_PKTCU_COMMAND      command;
    KEYLOADER_SSKMAN_PKTCU_RESPONSE     *response = (KEYLOADER_SSKMAN_PKTCU_RESPONSE*)response_buffer->buffer_ptr;
    VIM_UINT8                           key[600];
    VIM_UINT16                          outB64KeySize=0;


    VIM_DBG_MESSAGE("keyLoaderSetSSKMAN_PKTCU");
    /* Clear the structure */
    vim_mem_clear(&command, VIM_SIZEOF(command));


	// SPIRA:7567
	VIM_ERROR_RETURN_ON_FAIL( ErrorCode );

    //As this is only for a single byte value
    command.acquirer = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;
    //Read if we are doing the modulus or exponent
    command.modulus = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;


    //Fill out the error response in case we drop out due to an error
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, "A3", VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);


    //Don't do anything unless we are signed on
    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
         //VIM_DBG_MESSAGE("Keyloader Not signed on")
        return VIM_ERROR_NONE;
    }

    // RDD - no need to process acquirer code as only one acquirer
   //VIM_DBG_VAR(command.acquirer);

    //Convert the key from B64 format to a block of data
    //Move the data ptr to the start of the key data
    request_buffer->buffer_ptr+=request_buffer->buffer_index;
    VIM_DBG_DATA(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2);
    outB64KeySize = vim_tcu_decodeB64(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2, &key[0]);
    VIM_DBG_DATA(key, outB64KeySize);


    //Save the key data for the modulus or exponent
    if ( command.modulus == 1 )
    {
        VIM_DBG_MESSAGE("MODULUS");
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skMAN_pkTCU_in_parts(tcu_handle.instance_ptr, (void const *)&key[0], VIM_TRUE, outB64KeySize ));
    }
    else
    {
        VIM_DBG_MESSAGE("EXPONENT");
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skMAN_pkTCU_in_parts(tcu_handle.instance_ptr, (void const *)&key[0], VIM_FALSE, outB64KeySize ));
    }

    //Set our return code to ok
    vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);
    VIM_DBG_VAR(response_buffer->buffer_size);
    VIM_DBG_MESSAGE( " Set SKmanSKtcu DONE ! " );

	K_MESSAGE("sKmanSkTCU Loaded OK");

    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderSetPKTCU( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_PKTCU_COMMAND      command;
    KEYLOADER_PKTCU_RESPONSE     *response = (KEYLOADER_PKTCU_RESPONSE*)response_buffer->buffer_ptr;
    VIM_UINT8                    key[600];
    VIM_UINT16                   outB64KeySize=0;


    VIM_DBG_MESSAGE("keyLoaderSetPKTCU");
    /* Clear the structure */
    vim_mem_clear(&command, VIM_SIZEOF(command));


	// SPIRA:7567
	VIM_ERROR_RETURN_ON_FAIL( ErrorCode );

    //As this is only for a single byte value
    command.acquirer = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;
    //Read if we are doing the modulus or exponent
    command.modulus = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;


    //Fill out the error response in case we drop out due to an error
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, "A4", VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);


    //Don't do anything unless we are signed on
    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
         //VIM_DBG_MESSAGE("Keyloader Not signed on")
        return VIM_ERROR_SYSTEM;
    }

    // RDD - no need to process acquirer code as only one acquirer
    VIM_DBG_VAR(command.acquirer);

    //Convert the key from B64 format to a block of data
    //Move the data ptr to the start of the key data
    request_buffer->buffer_ptr+=request_buffer->buffer_index;
    VIM_DBG_DATA(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2);
    outB64KeySize = vim_tcu_decodeB64(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2, &key[0]);
    VIM_DBG_DATA(key, outB64KeySize);

    //Save the key data for the modulus or exponent
    if ( command.modulus == 1 )
    {
         VIM_DBG_MESSAGE("MODULUS - NOT SUPPORTED");
    }
    else
    {
        VIM_DBG_MESSAGE("EXPONENT");
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_pkTCU(tcu_handle.instance_ptr, (void const *)&key[0], outB64KeySize ));
        //Set our return code to ok
        vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));
    }


    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);
    VIM_DBG_MESSAGE( " Set PKtcu DONE ! " );
	K_MESSAGE("pkTCU Loaded OK");

    return VIM_ERROR_NONE;
}
/*----------------------------------------------------------------------------*/
static VIM_ERROR_PTR keyLoaderSetSKPPTMS( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
    KEYLOADER_SKTMSPP_COMMAND           command;
    KEYLOADER_SKTMSPP_RESPONSE          *response = (KEYLOADER_SKTMSPP_RESPONSE*)response_buffer->buffer_ptr;
    VIM_UINT8                           key[600];
    VIM_UINT16                          outB64KeySize;


     //VIM_DBG_MESSAGE("keyLoaderSetSKPPTMS");
    /* Clear the structure */
    vim_mem_clear(&command, VIM_SIZEOF(command));

    //Read if we are doing the modulus or exponent
    command.modulus = request_buffer->buffer_ptr[request_buffer->buffer_index] - 0x30;
    request_buffer->buffer_index += 1;


    //Fill out the error response in case we drop out due to an error
    vim_mem_copy(response->responseCode, "01", VIM_SIZEOF(response->responseCode));
    vim_mem_copy(response->command, "A5", VIM_SIZEOF(response->command));
    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);


    //Don't do anything unless we are signed on
    if ( keyLoaderSignedOn != VIM_TRUE )
    {
        //We are not signed on
         //VIM_DBG_MESSAGE("Keyloader Not signed on")
        return VIM_ERROR_NONE;
    }

    //Set the TCU to be our acquirer
     //VIM_DBG_MESSAGE("LFD Key");
    VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_sponsor(tcu_handle.instance_ptr,"LFD"));

    //Convert the key from B64 format to a block of data
    //Move the data ptr to the start of the key data
    request_buffer->buffer_ptr+=request_buffer->buffer_index;
    VIM_DBG_DATA(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2);
    outB64KeySize = vim_tcu_decodeB64(request_buffer->buffer_ptr, request_buffer->buffer_size-request_buffer->buffer_index-2, &key[0]);
    VIM_DBG_DATA(key, outB64KeySize);

    //Save the key data for the modulus or exponent
    if ( command.modulus == 1 )
    {
         //VIM_DBG_MESSAGE("MODULUS");
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skTMSPP(tcu_handle.instance_ptr, (void const *)&key[0], (void const *)VIM_NULL, outB64KeySize, 0 ));
    }
    else
    {
         //VIM_DBG_MESSAGE("EXPONENT");
        VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skTMSPP(tcu_handle.instance_ptr, (void const *)VIM_NULL, (void const *)&key[0], 0, outB64KeySize ));
    }

    //Set our return code to ok
    vim_mem_copy(response->responseCode, "00", VIM_SIZEOF(response->responseCode));

    response_buffer->buffer_size = VIM_SIZEOF(response->responseCode) + VIM_SIZEOF(response->command);

    return VIM_ERROR_NONE;
}

/*----------------------------------------------------------------------------*/

static VIM_ERROR_PTR keyLoaderSetVasKey( VIM_BUFFER_CONST* request_buffer, VIM_BUFFER* response_buffer )
{
     //VIM_DBG_MESSAGE("keyLoaderSetVasKey");
    return VIM_ERROR_NOT_IMPLEMENTED;
}
/*----------------------------WIN32 SIMULATOR TEST KEYS------------------------------------------------*/

// FIXME if I leave this as RDD has committed it, I get KVC errors, upon logon. Reverting with this change, I don't see KVC errors and logon succeeds.
// RDD 081020 Trello #202 - use Verifone test keys instead of Ingenico - don't change this. If you can't logon call Donna and ask her to setup your TID to use Verifone keys.
#if 0

static VIM_DES_BLOCK const test_wow_ppid =
{
    /* RDD - valid test ppid for wow */
    { 0x05,0x52,0x62,0x20,0x08,0x08,0x00,0x77 }
};

/* TEST PinPad Private Key exponant - does not work on production host */
VIM_UINT8 const skTCUexp[SKTCU_PKSP_KI_MAX_LEN] = {
0x3A,0xD6,0xB8,0x57 ,0x8D,0x25,0x1C,0xFD ,0x70,0x62,0x13,0x90 ,0x61,0xA0,0x11,0x95 ,0xA3,0x08,0xEB,0x68 ,0x89,0x68,0x14,0x34 ,0x8E,0xCE,0x2B,0x57 ,0x55,0x82,0xCE,0x71,
0x53,0x0F,0x5A,0x2B ,0xFD,0xEB,0xC8,0x8F ,0xA9,0xD1,0x33,0xE1 ,0x23,0x2A,0x45,0x1E ,0x38,0xAC,0x9C,0xFE ,0x34,0x91,0x45,0x66 ,0x5A,0xE2,0x86,0x73 ,0xF6,0x8F,0xF5,0x25,
0x4F,0xC3,0xFA,0xC1 ,0xF0,0x99,0x76,0x15 ,0x4B,0x0B,0x40,0x64 ,0x83,0x88,0x69,0xC6 ,0xEA,0x28,0x5D,0x66 ,0xCB,0xFE,0x78,0xD7 ,0xE3,0xF9,0x5D,0x10 ,0x42,0xD7,0x04,0x06,
0x3F,0x85,0x8F,0x20 ,0xAC,0xE6,0xA1,0xA3 ,0x60,0x55,0xED,0x16 ,0x85,0x5B,0x45,0x1B ,0x22,0xCD,0xC9,0x4E ,0xD5,0x9C,0xFB,0x91
};


//0xaa,0x1a,0x61,0xbd,0x3e,0x1d,0xd4,0xe3,0x0d,0xa2,0x3a,0x34,0x3e,0xcc, bb 1c 87 a0 cd 18 6a be da ea ab a3 d4 59 8b b2 65 f8 83 ad 48 ed 40 22 ee 7b aa 02 44 bb 51 78 09 d3 af 22 63 02 58 d8 62 6b 76 d8 90 6b ab 62 00 b3 04 ca 73 66 5e 36 8a 6a d3 98 a7 38 c3 a9 95 c1 65 42 90 9f 57 64 3b 76 80 1e fb 53 f3 f5 04 3c c4 62 0f b5 59 d2 f5 24 a6 71 8f f6 4c 27 73 8d 25 0a 09 ca 34 c9 f4 c9


VIM_UINT8 const wow_test_SKman_PKtcu[]=
{
  0x58,0x85,0x66,0xD2,0x70,0xF2,0x2E,0xAC,0xF7,0x4C,0xD9,0x74,0xD9,0xC1,0x3B,0x23,0x6C,0x45,0x64,0xE2,0xB5,0xD1,0x6E,0xCC,0x08,0x0A,0xD2,0xFF,0x01,0x40,0x83,0x7D,
  0xA6,0xCA,0x1D,0x61,0xB5,0x79,0x47,0x92,0x7A,0x69,0x09,0xCB,0x2D,0x66,0x02,0x7E,0x14,0x63,0xB9,0x0F,0x9F,0x45,0x05,0x88,0x9F,0x55,0x74,0x0A,0xC0,0x8E,0x82,0x61,
  0x31,0x5C,0xFD,0xC4,0xF8,0xEF,0xF8,0xBB,0xBF,0x96,0x5B,0xB2,0xF1,0xBC,0x4E,0xFD,0x2D,0xFC,0x59,0xA4,0xD0,0x06,0x16,0xAF,0xBA,0x70,0x39,0x62,0xFB,0x59,0xEF,0x5A,
  0x49,0x84,0xFF,0xFD,0x04,0xEC,0x32,0x37,0x6F,0xEE,0x59,0x5F,0x02,0x84,0x9E,0x19,0x55,0x65,0x8F,0xFA,0xAD,0x85,0x14,0xA8,0xD7,0x7D,0xB1,0x3D,0xF7,0x1F,0x9C,0xEB,
  0x10,0x62,0x46,0xA6,0x39,0x26,0x3B,0xE0,0x47,0xFE,0x30,0xC2,0x98,0x5D,0x7E,0xDF,0xC3,0xA6,0x8C,0x9E,0xEA,0xE1,0xC6,0xF9,0x34,0x25,0x75,0xFE,0x34,0xC0,0xF7,0x7E,
  0xC9,0x5D,0x2D,0x17,0x65,0x6D,0xD8,0x0E,0x09,0xDA,0x62,0xF2,0xBF,0x01,0x22,0x69,0xC5,0x75,0xB7,0x29,0x8C,0x03,0x0B,0x4A,0x04,0x4E,0xA0,0xAE,0x05,0xB7,0x27,0x4D,
  0x4B,0x4C,0x92,0xFD,0xB7,0xBB,0xAD,0xF4,0x92,0xB2,0xF3,0x81,0xD6,0xFF,0xEC,0xA3,0xD8,0x3A,0x42,0xFB,0x8B,0xC7,0x96,0xDB,0x27,0x9F,0x87,0x3A,0x19,0x4A,0xE2,0x2D,
  0xF5,0x71,0x14,0x5A,0xBC,0xCE,0xC6,0xA5,0xD3,0xF2,0xD4,0xA4,0x49,0x3C,0xFE,0x79,0x44,0xBE,0xA9,0xFD,0x75,0xA8,0x27,0x9C,0x36,0x4B,0xC1,0x0D,0xF5,0x9F,0x4D,0xED
};

/* PinPad Public Key */
VIM_UINT8 const pkTCUmod[SKTCU_PKSP_KI_MAX_LEN] = {
0xC6,0xB7,0xB5,0x26, 0x43,0xBD,0x2B,0x2F, 0xAE,0xBA,0x5B,0x31, 0x43,0x61,0x41,0xF8, 0xC0,0x97,0x52,0xD4, 0x11,0x0D,0x84,0x65, 0x18,0x15,0xFB,0x08, 0x77,0xB5,0xDB,0x90,
0x69,0x56,0x70,0xAE, 0x75,0x62,0x39,0x6E, 0xC8,0x3F,0x30,0x1E, 0x10,0xF5,0x2C,0x36, 0xE7,0x83,0x38,0x9E, 0xA5,0x9C,0x87,0x3E, 0x8C,0x9D,0xF5,0x3F, 0x8E,0x67,0x68,0x3A,
0xC5,0x49,0x0C,0xC1, 0xCF,0xB5,0x90,0x8E, 0x66,0xDA,0xEA,0x8F, 0x62,0x69,0x45,0x62, 0x67,0x44,0xF9,0xC1, 0xF0,0xDB,0xE4,0x8A, 0xC7,0xBB,0x7C,0x81, 0x11,0x22,0x2C,0x6D,
0xC9,0x6A,0xD5,0x5E, 0xDC,0xB3,0xB5,0xD4, 0xDF,0x0B,0x4C,0xDD, 0x5C,0x0E,0x9D,0x21, 0x9F,0x1B,0x7D,0xA5, 0x38,0x73,0x97,0x67
};



VIM_ERROR_PTR WOW_load_test_keys( void )
{
  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_ppid(tcu_handle.instance_ptr, &test_wow_ppid));
  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skTCU(tcu_handle.instance_ptr, pkTCUmod, skTCUexp, SKTCU_PKSP_KI_MAX_LEN, SKTCU_PKSP_KI_MAX_LEN));
  VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skMAN_pkTCU( tcu_handle.instance_ptr, wow_test_SKman_PKtcu, SKMAN_PKTCU_LEN ));

  return VIM_ERROR_NONE;
}

#else

// RDD 081020 Trello #202 - use Verifone test keys instead of Ingenico
static VIM_DES_BLOCK const test_wow_ppid =
{
    {0x06,0x12,0x02,0x13,0x25,0x06,0x02,0x89}
};

// From Mimic Script
VIM_CHAR const *skTCUexp64 = "qhphvT4d1OMNojo0Psy7HIegzRhqvtrqq6PUWYuyZfiDrUjtQCLue6oCRLtReAnTryJjAljYYmt22JBrq2IAswTKc2ZeNopq05inOMOplcFlQpCfV2Q7doAe+1Pz9QQ8xGIPtVnS9SSmcY/2TCdzjSUKCco0yfTJ";
VIM_CHAR const *skTCUmod64 = "3SteGrQ0FW/yIYzVim3n9owCudd647bPqtR9RaLx375jtQoAxGkgG0QNI0ASI2/MsCU6kW1oT52OytX7GpKEy16qIQukXmw7e2fYlOQ3xq4J1nEcESGoyV/eUSILw2aVew0z8tPTi7xDA4qU6FA6ssMgMYR0gK+z";
VIM_CHAR const *SKmanPkTCUexp64= "NDX3U52e4pfPKRD78Hc9/F8Twdvi+S1Mg+Ep7Ac4GQ1tbhXyDnG1+ike3FRN4sUD3XoSPpRu1tOqKMB0oMOs1UH/Vozm4lzZfKf1my3tDErO74BQXX+c+Xf27KuYyta6KeN5bayd4jRb8R9VIqV88/SRePC7k7KTo/SmayXfVQg=";
VIM_CHAR const *SKmanPkTCUmod64= "fZU7pN8ewZYn+6jbC3uNUlwAv4P0HjX3OpodzAbR9Gi8uqrURqb3ntKUnQOED8EFR90mUAzvK6KTEDVZTFAvZkIp+IOsVaJUYCFwB4mkE4v+ETpkZeItmou6siP9gUiauzkLsKR4Bv2vN5mZx4qsHmdTay2b0Z/MvbPlKUwjjuE=";
VIM_CHAR const *pkTCUexp64 = "AQAB";

#define WOW_PKTCU_EXP_LEN 3

VIM_ERROR_PTR WOW_load_test_keys( void )
{
	VIM_UINT16 outB64KeySize=0;

	VIM_UINT8 skTCUexp[SKTCU_PKSP_KI_MAX_LEN];
	VIM_UINT8 skTCUmod[SKTCU_PKSP_KI_MAX_LEN];
	VIM_UINT8 sKmanPkTCUexp[SKMAN_PKTCU_PART_LEN];
	VIM_UINT8 sKmanPkTCUmod[SKMAN_PKTCU_PART_LEN];
	VIM_UINT8 pkTCUexp[WOW_PKTCU_EXP_LEN];
	VIM_DES_BLOCK temp_ppid;

	vim_mem_clear(temp_ppid.bytes, VIM_SIZEOF(temp_ppid.bytes));

	// Set PPID
	// RDD 040522 - don't set default if good PPID already set in case we want to use Native PPID
	vim_tcu_get_ppid(tcu_handle.instance_ptr, &temp_ppid);
	
	if( temp_ppid.bytes[0] == 0x00)
	{
		VIM_DBG_MESSAGE("PPIDPPIDPPIDPPIDPPIDPPIDPPIDPPID Set Default PPID PPIDPPIDPPIDPPIDPPIDPPIDPPIDPPID");
		VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_ppid(tcu_handle.instance_ptr, &test_wow_ppid));
	}
	else	
	{
		VIM_DBG_MESSAGE("PPIDPPIDPPIDPPIDPPIDPPIDPPIDPPID Use Existing PPID PPIDPPIDPPIDPPIDPPIDPPIDPPIDPPID");	
		VIM_DBG_DATA(temp_ppid.bytes, VIM_SIZEOF(temp_ppid.bytes));
	}


	// Convert Sktcu - exp
    if(( outB64KeySize = vim_tcu_decodeB64( (const VIM_UINT8 *)skTCUexp64, SKTCU_PKSP_KI_MAX_LEN, skTCUexp )) != SKTCU_PKSP_KI_MAX_LEN )
	{
		DBG_MESSAGE( "Key Decode Length Error - skTCUexp64" );
		DBG_VAR( outB64KeySize );
	}
	// Convert Sktcu - mod
    if(( outB64KeySize = vim_tcu_decodeB64( (const VIM_UINT8 *)skTCUmod64, SKTCU_PKSP_KI_MAX_LEN, skTCUmod )) != SKTCU_PKSP_KI_MAX_LEN )
	{
		DBG_MESSAGE( "Key Decode Length Error - skTCUmod64" );
		DBG_VAR( outB64KeySize );
	}
	// Store the Key componants
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_set_skTCU(tcu_handle.instance_ptr, skTCUmod, skTCUexp, SKTCU_PKSP_KI_MAX_LEN, SKTCU_PKSP_KI_MAX_LEN ));

	// Convert SKManPKtcu - exp
    if(( outB64KeySize = vim_tcu_decodeB64( (const VIM_UINT8 *)SKmanPkTCUexp64, SKMAN_PKTCU_PART_LEN, sKmanPkTCUexp )) != SKMAN_PKTCU_PART_LEN )
	{
		DBG_MESSAGE( "Key Decode Length Error - SKmanPkTCUexp64" );
		DBG_VAR( outB64KeySize );
	}
	// Convert SKManPKtcu - mod
    if(( outB64KeySize = vim_tcu_decodeB64( (const VIM_UINT8 *)SKmanPkTCUmod64, SKMAN_PKTCU_PART_LEN, sKmanPkTCUmod )) != SKMAN_PKTCU_PART_LEN )
	{
		DBG_MESSAGE( "Key Decode Length Error - SKmanPkTCUmod64" );
		DBG_VAR( outB64KeySize );
	}
	// Store the Key components
	VIM_DBG_MESSAGE("EXPONENT");
	VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skMAN_pkTCU_in_parts( tcu_handle.instance_ptr, (void const *)sKmanPkTCUexp, VIM_FALSE, SKMAN_PKTCU_PART_LEN ));
    VIM_DBG_MESSAGE("MODULUS");
	VIM_ERROR_RETURN_ON_FAIL(vim_tcu_set_skMAN_pkTCU_in_parts( tcu_handle.instance_ptr, (void const *)sKmanPkTCUmod, VIM_TRUE, SKMAN_PKTCU_PART_LEN ));

	// Convert the public key exponenet
    if(( outB64KeySize = vim_tcu_decodeB64( (const VIM_UINT8 *)pkTCUexp64, WOW_PKTCU_EXP_LEN, pkTCUexp )) != WOW_PKTCU_EXP_LEN )
	{
		DBG_MESSAGE( "Key Decode Length Error - pkTCUexp64" );
		DBG_VAR( outB64KeySize );
	}
	// Store the public key exponenet
	VIM_ERROR_RETURN_ON_FAIL( vim_tcu_set_pkTCU( tcu_handle.instance_ptr, pkTCUexp, WOW_PKTCU_EXP_LEN ));
	return VIM_ERROR_NONE;
}

#endif
