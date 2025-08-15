/*----------------------------------------------------------------------------*/
#include "dbg.h"
#ifdef DEBUG_ON
#ifndef WIN32
#warning DEBUG is turned ON
#endif

/*----------------------------------------------------------------------------*/
/* handle for the debug device */
static FILE* dbg_handle=NULL;

/* mask to determine which areas of debug should be enabled */
unsigned long dbg_mask=DBG_MASK_DEFAULT;

/*----------------------------------------------------------------------------*/
static void dbg_init(void)
{
	/* open the debug device */
	dbg_handle = fopen("COM5", "rw*");
	/* set the communication parameters of the debug device */
	format
	(
		"COM5",
		115200, 
		8, 
		1, 
		NO_PARITY, 
		ODD, 
		0      
	);
	//mask_event( debug_handle, 0 );
}
/*----------------------------------------------------------------------------*/
static void dbg_putchar
(
	char source
)
{
#ifdef WIN32
	static char buffer[120];
	static size_t buffer_index=0;
	if (buffer_index>sizeof(buffer)-2)
	{
		trace(0,buffer_index,buffer);
		buffer_index=0;
	}
	if (source=='\n')
	{
		trace(0,buffer_index,buffer);
		buffer_index=0;
	}
	else
	{
		buffer[buffer_index++]=source;
	}
#else
	/* make sure the debug device has been initialised*/
	if (dbg_handle==NULL)
	{
		/* intialise the debug device */
		dbg_init();
	}
	/* check if the source character is a line feed */
	if (source=='\n')
	{
		/* insert a carraige return before each line feed */
		dbg_putchar('\r');
	}
	/* send the source character to the debug device */
  while(fwrite(&source,1,1,dbg_handle)==0)
    printf("\x1B""DEBUG DEAD");
#endif
}
/*----------------------------------------------------------------------------*/
void dbg_puts
(
	char const* source
)
{
	/* make sure the source pointer is not null */
	if (source!=NULL)
	{
		/* iterate through the source characters until a null terminator is 
		encounterd */
		while(*source)
		{
			/* send the current character from source to debug then advance to
			the next character in source*/
			dbg_putchar(*source++);
		}
	}
}
/*----------------------------------------------------------------------------*/
static void dbg_put_nibble
(
	unsigned char src
)
{
	char const hex_character[]="0123456789ABCDEF";
	/* map the least significant nibble of the source to its ascii 
	representation*/
	dbg_putchar(hex_character[src&0xF]);
}
/*----------------------------------------------------------------------------*/
static void dbg_put_byte
(
	unsigned char src
)
{
	/* print the most signifigant nibble of the byte */
	dbg_put_nibble(src>>4);
	/* print the least signifigant nibble of the byte */
	dbg_put_nibble(src&0xF);
}
/*----------------------------------------------------------------------------*/
static void dbg_data
(
	void const* source, 
	size_t source_length
)
{
	/* check if the source is null */
	if ( source!=NULL )
	{
		/* convert source to an array of bytes */
		unsigned char* source_bytes=(unsigned char*)source;  
		/* format the debug output*/
		dbg_puts("\n\t");
		/* iterate through each byte of the source data*/
		while(source_length--)
		{
			/* send the byte */
			dbg_put_byte(*source_bytes++);
			/* insert a space after each byte */
			dbg_puts(" ");
		}
		/* insert a carraige return at the end of the data*/
		dbg_puts("\n");
	}
	else
	{
		/* send null to debug to indicate that the source pointer was null */
		dbg_puts("(null)\n");
	}
}
/*----------------------------------------------------------------------------*/
void dbg_dump
(
	char const* title,
	void const* source, 
	size_t source_length
)
{
	/* print the title */
	dbg_puts(title);
	/* print the data */
	dbg_data(source,source_length);
}
/*----------------------------------------------------------------------------*/
void dbg_timestamp
(
  void
)
{
  char time_string[35];
  DATE d;

  read_date(&d);
  vim_snprintf(time_string, sizeof(time_string), "%2.2s/%2.2s/%2.2s %2.2s:%2.2s:%2.2s %lu ticks\n", d.day, d.month, d.year, d.hour, d.minute, d.second, get_tick_counter());
  dbg_puts(time_string);
}
/*----------------------------------------------------------------------------*/
void dbg_close(void)
{
  if (dbg_handle != NULL)
    fclose(dbg_handle);
  dbg_handle = NULL;
}
/*----------------------------------------------------------------------------*/
#endif

