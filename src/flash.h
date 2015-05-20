/* ':':':':':':':':':'@' '@':':':':':':':':':'@' '@':':':':':':':':':'| */
/* :':':':':':':':':':'@'@':':':':':':':':':':'@'@':':':':':':':':':':| */
/* ':':':': : :':':':':'@':':':':': : :':':':':'@':':':':': : :':':':'| */
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */
/*		Name:		flash.h					*/
/*		Version:	1.4.2					*/
/*		Date:		19/07/2007				*/
/*  -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=- */

#ifndef FLASH_H
#define	FLASH_H

#define uchar unsigned char

#define SERIAL_NO       0x00003FFC
#define TABLE           0x00004000


#define MAGIC_NUMBER     0               /* Boot code table entry definitions */
#define FLAGS            4
#define RAM_START        8
#define RAM_LENGTH      12
#define ROM_START       16
#define ROM_LENGTH      20
#define START_OFFSET    24
#define CPSR            28
#define SPARTAN_START   32
#define SPARTAN_LENGTH  36
#define VIRTEX_START    40
#define VIRTEX_LENGTH   44
#define LCD_TEXT        48

#define ENTRY_SIZE   0x100

#define FPGA_HEADER_SIZE 64     /* No of bytes fetched at start of FPGA defn. */
#define FPGA_START        4        /* Offset to start of FPGA definition file */
#define FPGA_LENGTH       8                      /* Length of FPGA definition */
#define FPGA_MESSAGE     12                            /* User's FPGA message */

#define FLAG_RAM_EXEC  0x08

typedef enum
{
  SERIAL,
  EMULATOR,
  FAKE
}
target_type;

target_type interface_type;


typedef struct used_listname
{
unsigned int start;
unsigned int end;
int next;
}
used_list;


#endif

/*									*/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 	*/
/*                     end of flash.h				*/
/************************************************************************/
