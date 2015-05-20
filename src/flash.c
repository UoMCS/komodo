/******************************************************************************/
/*                Name:           flash.c                                     */
/*                Version:        1.4.1                                       */
/*                Date:           06/01/2008 (spelling fix only)              */
/*                Flash (aaaahh)                                              */
/*                                                                            */
/*============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include "serial.h"
#include "flash.h"

int check_ack(void);
int read_mem(unsigned int address, unsigned int length, char * destination);
void printtable (uchar * tabledata);
int chararr2int (uchar * arr);

# define DOWNLOAD 0
# define VERIFY   1

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* ELF definitions                                                            */

#define PROGBITS   1
#define SYMTAB     2
#define DYNSYM    11

#define SHF_WRITE  1
#define SHF_ALLOC  2
#define SHF_EXEC   4

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* ELF structures                                                             */

typedef struct                 /* Programme or section table info from header */
  {
  unsigned int offset;
  unsigned int count;
  unsigned int entry_size;
  }
  tabledef;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef struct section_list_name          /* Record for listing section info. */
  {
  struct section_list_name  *pNext;
  unsigned int               number;
  unsigned int               name;
  unsigned int               type;
  unsigned int               flags;
  unsigned int               addr;
  unsigned int               offset;
  unsigned int               size;
  unsigned int               link;
  unsigned int               info;
  unsigned int               align;
  unsigned int               entsize;
  unsigned int               count;
  }
  section_list;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

typedef int boolean;

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

const SECTION_LIST_SIZE = sizeof(section_list);
/* ELF Global data                                                            */
int class, encoding;

/* ELF routines */
int  elf_test(FILE*);
int  dismember(FILE*, int);
void do_header(FILE*, tabledef*, tabledef*, unsigned int*, unsigned int*);
void do_section_table(FILE*, tabledef*, unsigned int, section_list**);
int  do_binary(FILE*, section_list*, unsigned int, int);
void delete_section_list(section_list**);
int  send_block(FILE*,  unsigned int, unsigned int, unsigned int);
int  check_block(FILE*, unsigned int, unsigned int, unsigned int);
unsigned int get_item(FILE*, int);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int main(int argc, char *argv[])
{
uchar tabledata[16 * ENTRY_SIZE];
unsigned char input[16];
unsigned int start, length, count, count2;
FILE *file;
int hex;
char *hex_string;
int comp;
int temp;
char *commandstrings[] = { "--help", "-h", "-?", "-s", NULL };

  //            0         1     2     3  
interface_type = SERIAL;
portname = PORTNAME;
count = 1;
while (argv[count])
  {
  if ('-' != *argv[count])
    {
    g_print("Unknown option %s\n", argv[count]);
    exit(1);                                                // @@
    }
    comp = 0;
    while (g_strcasecmp(commandstrings[comp], argv[count]))
      if (!commandstrings[comp])
        {
        g_print("Unknown option  %s\n", argv[count]);
        exit(1);
        }
      else
        comp++;

    switch (comp)
      {
      case 0:
      case 1:
      case 2:                //help
        g_print("\n Flash (aaaaahhhhh) Version " VERSION " (" DATE ")\n"
                 " Copyright " YEAR " University of Manchester \n"
                 " Authors: Charlie Brej & Jim Garside\n"
                 " Bug reports and feature requests: komodo@cs.man.ac.uk\n"
                 " Website: http://www.cs.man.ac.uk/teaching/electronics/komodo\n"
                 "\n"
                 " Usage: flash [options]\n"
                 "\n"
                 "   --help\n"
                 "   -h\n"
                 "   -?              display this help message\n"
                 "   -s [port]       run through serial port Default:"
                 PORTNAME "\n");
        exit(0);
      case 3:                //serial
        interface_type = SERIAL;
        if (argv[count + 1])
          if ('-' != *argv[count + 1])
            portname = argv[++count];
        break;
      default:
        g_print("Command parameter error!");
        exit(0);
      }
    count++;
  }

g_print("\nSerial Setup    :%d", serial_setup(100));     /* Setup serial port */

read_pipe  = serial_FD;
write_pipe = serial_FD;

g_print("\nFlash ROM programmer v0.2 (c) 2003 Manchester University");
board_sendchar(0xFE);                   /* Magic word to establish bona fides */
board_sendchar(0xA5);
board_sendchar(0x1B);
board_sendchar(0x1E);
input[0] = 0; input[1] = 0; input[2] = 0; input[3] = 0;

do                                     /* Read reply until message recognised */
  {                              /*  (not intended to be particularly robust) */
  input[0] = input[1];
  input[1] = input[2];
  input[2] = input[3];
  board_getchar(&input[3]);
  }
  while (input[0] != 0xFE || input[1] != 0xE1
      || input[2] != 0x90 || input[3] != 0x0D);

  while (1)                                                      /* Main loop */
    {
    g_print("\n>");                                                 /* Prompt */
    if (EOF == scanf("%s", input)) exit(0);
    hex = ('h' == input[1]) || ('H' == input[1]);
    switch (input[0])
      {
      case 'H':
      case 'h':
      case '?':
        g_print("\n FLASH (aaaaahhhhh) v0.2 (c) 2003 Manchester University"
            "\n R Filename StartAddress Length - Read to file (\"@\" => stdout) from Flash"
            "\n W Filename StartAddress Length - Write from file (\"@\" => stdout) to Flash"
            "\n P                              - Ping board"
            "\n H                              - Help Menu"
            "\n T                              - Show free memory areas"
            "\n E EraseAddress                 - Erase memory areas"
            "\n L LockAddress                  - Lock memory areas"
            "\n C CheckAddress                 - Lock Check memory areas"
            "\n I                              - Identity"
            "\n D Filename                     - Download ELF file"
            "\n V Filename                     - Verify from ELF file"
            "\n ^C                             - Quit");
        break;

      case 'R':
      case 'r':
        g_print("\nFilename>");      scanf("%s", input);
        if ('@' == *input) file = stdout;
        else               file = fopen(input, "w");

        if (file == NULL) g_print("\nBad Filename\a");
        else
          {
          g_print("Start address>"); scanf("%x", &start);
          g_print("Length>");        scanf("%x", &length);
          g_print("\nReading:%s, at %x size %x\n", input, start, length);
          board_sendchar('R');                           /* Send read command */
          board_sendbN(start, 4);
          board_sendbN(length, 4);

          if (hex) hex_string = "%02x ";             /* Set up format strings */
          else     hex_string = "%c";

          while ((count = MIN(length, 16)))
            {
            if (count != board_getchararray(count, input))
              {
              if (file != stdout) fclose(file);
              g_print("\nTimeout!!!!!\a");
              break;
              }
            for (count2 = 0; count2 < count; count2++)
              fprintf(file, hex_string, input[count2]);

            if (hex)                                        /* ASCII dump too */
              {
              fprintf(file, "   ; ");
              for (count2 = 0; count2 < count; count2++)
                {
                if ((input[count2] >= 0x20) && (input[count2] <= 0x7E))
                  fprintf(file, "%c", input[count2]);
                else
                  fprintf(file, ".");
                }
              fprintf(file, "\n");
              }

            board_sendchar('A');
            length -= count;
            if (file != stdout) g_print(".");
            fflush(stdout);
            }

          if (file != stdout) fclose(file);
          g_print("\nSuccess");
          }
        break;

      case 'W':
      case 'w':
        g_print("\nFilename>");      scanf("%s", input);
        if ('@' == *input) file = stdin;
        else               file = fopen(input, "r");

        if (file == NULL) g_print("\nBad Filename\a");
        else
          {
          g_print("Start address>"); scanf("%x", &start);
          g_print("Length>");        scanf("%x", &length);
          g_print("\nWriting:%s, at %x size %x\n", input, start, length);
          board_sendchar('W');                          /* Send write command */
          board_sendbN(start, 4);
          board_sendbN(length, 4);

          while ((count = MIN(length, 16)))
            {
            for (count2 = 0; count2 < count; count2++)
              if (hex)
                {
                fscanf(file, "%x", &temp);
                input[count2] = temp;
                }
              else
                fscanf(file, "%c", &input[count2]);

            board_sendchararray(count, input);

            if (!check_ack())
              {
              if (file != stdin) fclose(file);
              break;
              }

            length -= count;
            g_print(".");                               /* Progress indicator */
//          if (hex)     // Can't work out what this was for - not much though!
//            if (!(length & 0x3f0)) g_print("\n %06d  ", length);
            fflush(stdout);             /* Ensure progress printing displayed */
            }                        // end while

          if (file != stdin) fclose(file);
          g_print("\nSuccess");
          }
        break;

      case 'D':                                          /* Download ELF file */
      case 'd':
        {
        int error;

        g_print("\nFilename>");      scanf("%s", input);
        file = fopen(input, "r");
        if (file == NULL) g_print("\nBad Filename\a");
        else
          {
          if (!elf_test(file))
            g_print("\"%s\" is not an ELF file\n", input);
          else
            {
            g_print("Loading: %s\n", input);
            error = dismember(file, DOWNLOAD);
            if (error) g_print("\nFailed"); else g_print("\nSuccess");
            }
          fclose(file);
          }
        }
        break;

      case 'V':                                       /* Verify from ELF file */
      case 'v':
        {
        int error;

        g_print("\nFilename>");      scanf("%s", input);
        file = fopen(input, "r");
        if (file == NULL) g_print("\nBad Filename\a");
        else
          {
          if (!elf_test(file))
            g_print("\"%s\" is not an ELF file\n", input);
          else
            {
            g_print("Verifying: %s\n", input);
            error = dismember(file, VERIFY);
            if (error) g_print("\nFailed"); else g_print("\nSuccess");
            }
          fclose(file);
          }
        }
        break;

      case 'E':
      case 'e':
        g_print("\nErase address>"); scanf("%x", &start);
        board_sendchar('E');
        board_sendbN(start, 4);
        if (check_ack()) g_print("\nSuccess");
        break;

      case 'P':
      case 'p':
        board_sendchar('P');
        if (check_ack()) g_print("\nSuccess");
        else fclose(file);      // @@ Errr... which file, exactly?? @@
        break;

      case 'T':
      case 't':
        read_mem(TABLE, length = 16 * ENTRY_SIZE, tabledata);
        printtable(tabledata);
        break;

      case 'L':
      case 'l':
        g_print("Lock address>"); scanf("%x", &start);
        board_sendchar('L');
        board_sendbN(start, 4);
        if (check_ack()) g_print("\nSuccess");
        break;

      case 'C':
      case 'c':
        g_print("Check address>"); scanf("%x", &start);
        board_sendchar('C');
        board_sendbN(start, 4);
        if (1 != board_getchar(input)) g_print("\nTimeout!!!!!\a");
        else
          switch (input[0])
            {
            case 'L': g_print("\nLocked");     break;
            case 'N': g_print("\nNot locked"); break;
            default:  g_print("\nBad Ack!");   break;
            }
        break;

      case 'I':
      case 'i':
        {
        unsigned char serial_no[4];

        board_sendchar('R');
        board_sendbN(SERIAL_NO, 4);
        board_sendbN(4, 4);
        if (board_getchararray(4, serial_no) != 4)
          g_print("\nTimeout!!!!!\a");
        else
          {
          board_sendchar('A');
          g_print("\nBoard serial number:  %02X%02X%02X%02X",
                    serial_no[3],  serial_no[2], serial_no[1],  serial_no[0]);

          board_sendchar('I');
          if ((1!=board_getchar(&input[0])) || (1!=board_getchar(&input[1])))
            g_print("\nTimeout!!!!!\a");
          else
            {
            g_print("\nFlash ROM type:       ");

            switch (input[0])                   /* Case of: manufacturer code */
              {
              case 0x01:
                g_print("AMD  ");
                switch (input[1])
                  {
                  case 0xC4: g_print("Am29LV160D  (Top boot block)");    break;
                  case 0x49: g_print("Am29LV160D  (Bottom boot block)"); break;
                  default:   g_print("Part No:  %02X", input[1]);        break;
                  }
                break;

              case 0x1F:
                g_print("Atmel  ");
                switch (input[1])
                  {
                  case 0xC0: g_print("AT49BV16X4");  break;
                  case 0xC2: g_print("AT49BV16X4T"); break;
                  default:   g_print("Part No:  %02X", input[1]); break;
                  }
                break;

              case 0x20:
                g_print("ST  ");
                switch (input[1])
                  {
                  case 0xC4: g_print("M29W160DT"); break;
                  case 0x49: g_print("M29W160DB"); break;
                  default:   g_print("Part No:  %02X", input[1]); break;
                  }
                break;

              default:
                g_print("Manufacturer code:  %02X  part code:  %02X",
                         input[0], input[1]);
                break;
              }                        // end switch
            }                          // end else
          }                            // end else
        }                              // end declaration scope
        break;

      default:
        g_print("\nUnknown command");
        break;

      }                              // end switch (interpreting params )
    }                                // end while
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* This is used to parse the boot table.  It checks each entry and reports on */
/* the valid ones.  It also finds any FPGA configuration files used.          */
/* A sorted list of `used' areas of memory is also printed.                   */

void printtable(uchar * tabledata)
{
used_list blocks[50];                    /* Saves creating and disposing them */
         /* 50 = 16 boots, each with 2 FPGAs + boot table + dummy first block */
unsigned int k;

void insert_block(unsigned int start, unsigned int end)
{   /* Build a linked list (in array) of memory areas used in ascending order */
int i, j;

i = blocks[0].next;                               /* First (0) is dummy block */
j = 0;

while ((i > 0) && (start > blocks[i].start))
  {
  j = i;
  i = blocks[i].next;
  }

if ((i < 0) || (start != blocks[i].start))                 /* Don't duplicate */
  {                            /*  Insert if end of list or new start address */
  blocks[k].start = start;
  blocks[k].end   = end;
  blocks[k].next  = i;
  blocks[j].next  = k;
  k++;
  }

return;
}


int fpga_print(unsigned int fpga_base, char * name)
  {
  int count, okay;
  unsigned int j;
  uchar fpga_header[FPGA_HEADER_SIZE];

  if (okay = read_mem(fpga_base, FPGA_HEADER_SIZE, fpga_header))
    {
    if ((fpga_header[0] == 'F') && (fpga_header[1] == 'P')
     && (fpga_header[2] == 'G') && (fpga_header[3] == 'A'))
      {
      j = fpga_base + chararr2int(&fpga_header[FPGA_START])
                    + chararr2int(&fpga_header[FPGA_LENGTH]);
      g_print("\n%s file from  %08X  to  %08X", name, fpga_base, j);
      g_print("\n%s", &fpga_header[FPGA_MESSAGE]);
      insert_block(fpga_base, j);
      }
    }
  return okay;
  }


int count, i, j, highest_boot;

k = 1;
blocks[0].next = -1;                      /* Initialise list of `used' blocks */

for (count = 0; count < 16; count++)
  {
  g_print("\nBoot %2d", count);
  if ('C' == tabledata[count * ENTRY_SIZE]
   && 'O' == tabledata[count * ENTRY_SIZE + 1]
   && 'D' == tabledata[count * ENTRY_SIZE + 2]
   && 'E' == tabledata[count * ENTRY_SIZE + 3])
    {
    highest_boot = count;                    /* Find highest used boot number */
    tabledata[(count+1) * ENTRY_SIZE - 1]; /* Just in case terminator missing */
    i = count * ENTRY_SIZE + LCD_TEXT;
    while (tabledata[i] != '\0') i++;                     /* Skip LCD message */
    if (((i+1) % ENTRY_SIZE) != 0)   /* i.e. not at end of space (v. likely!) */
      g_print("\n%s", &tabledata[i + 1]);              /* Print comment field */
    i = chararr2int(tabledata + (count * ENTRY_SIZE) + ROM_START);
    j = chararr2int(tabledata + (count * ENTRY_SIZE) + ROM_LENGTH);
    g_print("\nBase address: %08X", i);
    g_print("\n      length: %08X", j);
    if (chararr2int(tabledata + (count * ENTRY_SIZE) + RAM_LENGTH) != 0)
      g_print("\nCopies: %08X bytes to internal RAM from ROM offset: %08X",
       (chararr2int(tabledata + (count*ENTRY_SIZE)+RAM_LENGTH)+3) & 0xFFFFFFFC,
        chararr2int(tabledata + (count * ENTRY_SIZE) + RAM_START));

    if ((chararr2int(tabledata + (count * ENTRY_SIZE) + FLAGS) & FLAG_RAM_EXEC) != 0)
      g_print("\n executes at absolute address: %08X",
                  chararr2int(tabledata + (count * ENTRY_SIZE) + START_OFFSET));
    else
      g_print("\n executes at ROM offset: %08X",
              i + chararr2int(tabledata + (count * ENTRY_SIZE) + START_OFFSET));

    insert_block(i, i + j);                 /* Add `used' area to linked list */

    fpga_print(chararr2int(&tabledata[(count * ENTRY_SIZE) + SPARTAN_START]),
               "Spartan");
    fpga_print(chararr2int(&tabledata[(count * ENTRY_SIZE) + VIRTEX_START]),
               "Virtex");

    g_print("\n");
    }
  else
    g_print(" unused");
  }

insert_block(TABLE, TABLE + ((highest_boot+1) * ENTRY_SIZE)); /*Boot table too*/

g_print("\n\nUsed areas:   Start      End");
i = blocks[0].next;
while (i > 0)
  {
  g_print("\n            %08X  %08X", blocks[i].start, blocks[i].end);
  i = blocks[i].next;
  }

return;
}

/******************************************************************************/
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Convert an unsigned char array to an integer                               */
/*                                                                            */

int chararr2int(uchar * arr)
{
return (arr[0] | arr[1] << 8 | arr[2] << 16 | arr[3] << 24);
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Accept an `A'ck and indicate status                                        */

int check_ack(void)
{
char input[1];
boolean okay;

okay = FALSE;
if (1 != board_getchar(input)) g_print("\nTimeout!!!!!\a");
else
  if ('A' != input[0])         g_print("\nBad Ack!!!!!\a");
  else okay = TRUE;

return okay;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int read_mem(unsigned int address, unsigned int length, char * destination)
{
unsigned int start, count;
int okay;

board_sendchar('R');
board_sendbN(address, 4);
board_sendbN(length, 4);
start = 0;
while ((count = MIN(length, 16)))
  {
  okay = (count == board_getchararray(count, destination + start));
  if (okay)
    {
    board_sendchar('A');
    length -= count;
    start += count;
    }
  else
    {
    g_print("\nTimeout!!!!!\a");
    break;
    }
  }

return okay;
}

/*                                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int elf_test(FILE *fHandle)
{
return (fgetc(fHandle) == 0x7F) && (fgetc(fHandle) == 'E') &&
       (fgetc(fHandle) ==  'L') && (fgetc(fHandle) == 'F');
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int dismember(FILE *fHandle, int action)
{
tabledef programme, section_table;
unsigned int section_names, shstrndx, size, lowest_addr, highest_addr;
section_list *sections;
section_list *p1, *p2;
int error;

rewind(fHandle);

sections = NULL;                         /* (Will be) list of section records */

do_header(fHandle, &programme, &section_table, &section_names, &shstrndx);
do_section_table(fHandle, &section_table, section_names, &sections);

size         = 0;
lowest_addr  = 0xFFFFFFFF;
highest_addr = 0x00000000;             /* (Will end up as next free address.) */

for (p1 = sections; p1 != NULL; p1 = p1->pNext)    /* Chain down section list */
  if ((p1->flags & SHF_ALLOC) != 0)                       /* Loadable section */
    {
    size = size + p1->size;
    if (p1->addr < lowest_addr) lowest_addr = p1->addr;
    if ((p1->addr + p1->size) > highest_addr) highest_addr = p1->addr+p1->size;
    }

g_print("Loadable length 0x%08X between limits 0x%08X and 0x%08X (inclusive)\n\n",
        size, lowest_addr, highest_addr - 1);

error = FALSE;
for (p1 = sections; p1 != NULL; p1 = p1->pNext)    /* Chain down section list */
  {
  if (p1->type == PROGBITS)                           /* Find binary sections */
    {                                                     /*  and output them */
    if ((p1->flags & SHF_ALLOC) != 0)                     /* Loadable section */
      error = do_binary(fHandle, p1, section_names, action) || error;
    g_print("\n");
    }

  if ((action == DOWNLOAD) && error) break;        /* Keep going if verifying */
  }

delete_section_list(&sections);

return error;
}

/*----------------------------------------------------------------------------*/
/* Pull file header apart                                                     */

void do_header(FILE *fHandle, tabledef *prog, tabledef *sect,
               unsigned int *section_names, unsigned int *shstrndx)
{
unsigned int ftype, proc, OS_ABI;

fseek(fHandle, 4, SEEK_SET);                                     /* Strip ELF */

class    = get_item(fHandle, 1);
encoding = get_item(fHandle, 1);
get_item(fHandle, 1);                                          /* ABI version */
OS_ABI   = get_item(fHandle, 1);

fseek(fHandle, 16, SEEK_SET);                           /* Strip whole ident. */

ftype = get_item(fHandle, 2);
proc =  get_item(fHandle, 2);

if (proc != 0x28) g_print("** WARNING - NOT AN ARM BINARY **\n");

get_item(fHandle, 4);get_item(fHandle, 4);	// Discard ELF version & entry address

prog->offset = get_item(fHandle, 4);
sect->offset = get_item(fHandle, 4);
get_item(fHandle, 4);				// Discard flags

get_item(fHandle, 2);                                 /* Header size - fixed? */
prog->entry_size = get_item(fHandle, 2);
prog->count      = get_item(fHandle, 2);
sect->entry_size = get_item(fHandle, 2);
sect->count      = get_item(fHandle, 2);
*shstrndx = get_item(fHandle, 2);

fseek(fHandle, sect->offset + sect->entry_size * (*shstrndx) + 4 * 4, SEEK_SET);

*section_names = get_item(fHandle, 4);    /* Pointer to header string section */

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Build a list of sections                                                   */

void do_section_table(FILE *fHandle, tabledef *sect, unsigned int section_names,
                      section_list **sect_list)
{
unsigned int i, j;
section_list *pLast, *pNew;

*sect_list = NULL;                                 /* Initialise list pointer */
pLast      = *sect_list;

fseek(fHandle, sect->offset, SEEK_SET);                       /* Strip ident. */

for (i = 0; i < sect->count; i++)
  {
  pNew = (section_list*) malloc(SECTION_LIST_SIZE);         /* Make new entry */
  pNew->pNext  = NULL;
  pNew->number = i;

  if (pLast == NULL) *sect_list = pNew;                     /* Link new entry */
  else             pLast->pNext = pNew;
  pLast = pNew;

  pNew->name    = get_item(fHandle, 4);             /* Offset to section name */
  pNew->type    = get_item(fHandle, 4);
  pNew->flags   = get_item(fHandle, 4);
  pNew->addr    = get_item(fHandle, 4);
  pNew->offset  = get_item(fHandle, 4);
  pNew->size    = get_item(fHandle, 4);
  pNew->link    = get_item(fHandle, 4);
  pNew->info    = get_item(fHandle, 4);
  pNew->align   = get_item(fHandle, 4);
  pNew->entsize = get_item(fHandle, 4);

  if (pNew->entsize == 0) pNew->count = 0;
  else                    pNew->count = pNew->size / pNew->entsize;
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int do_binary(FILE *fHandle, section_list *pSection, unsigned int section_names,
              int action)
{
int error;

g_print("Section %2d: ", pSection->number);
switch (action)
  {
  case DOWNLOAD:
    error = send_block( fHandle, pSection->offset, pSection->addr, pSection->size);
    break;

  case VERIFY:
    error = check_block(fHandle, pSection->offset, pSection->addr, pSection->size);
    break;

  default: break;
  }

return error;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

void delete_section_list(section_list **pList)
{
section_list *pNext, *pThis;

pNext = *pList;                                              /* Point to list */
*pList = NULL;                                               /* Chop off list */

while (pNext != NULL)                                  /* While records exist */
  {
  pThis = pNext;                                            /* Keep a pointer */
  pNext = pNext->pNext;                                /* Move down the chain */
  free(pThis);                                       /* Free record behind us */
  }

return;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int send_block(FILE *fHandle, unsigned int fPos,
        unsigned int address, unsigned int count)
{
unsigned char buffer[16];
unsigned int i, block_length;
int error;

error = FALSE;

fseek(fHandle, fPos, SEEK_SET);

g_print("Downloading to address %08X, length %08X\n", address, count);

board_sendchar('W');                                    /* Send write command */
board_sendbN(address, 4);
board_sendbN(count, 4);

while ((block_length = MIN(count, 16)) != 0)
  {
  for (i = 0; i < block_length; i++)              /* Buffer next set of bytes */
    buffer[i] = get_item(fHandle, 1);

  board_sendchararray(block_length, buffer);                   /* Send buffer */

//for (i = 0; i < block_length; i++) g_print("%02X ", buffer[i]); g_print("\n ");

  if (!check_ack()) {error = TRUE; break;}
  count = count - block_length;
  g_print(".");                                         /* Progress indicator */
  fflush(stdout);                       /* Ensure progress printing displayed */
  }                                                           /* end of while */

return error;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

int check_block(FILE *fHandle, unsigned int fPos,
         unsigned int address, unsigned int count)
{
unsigned char c, buffer[16];
unsigned int i, block_length;
int fail, error;

error = FALSE;

fseek(fHandle, fPos, SEEK_SET);

g_print("Checking address %08X, length %08X\n", address, count);

board_sendchar('R');                                     /* Send read command */
board_sendbN(address, 4);
board_sendbN(count, 4);

while ((block_length = MIN(count, 16)) != 0)
  {
  fail = (block_length != board_getchararray(block_length, buffer));
                                                               /* Fill buffer */
  if (!fail) board_sendchar('A');
  else { g_print("\nTimeout!!!!!\a"); error = TRUE; break; }

  for (i = 0; i < block_length; i++)              /* Buffer next set of bytes */
    {
    c = get_item(fHandle, 1);
    if (c != buffer[i])
      {
      g_print("\nAddress: %08X reads: %02X - should read: %02X",
                          address, buffer[i], c);
      error = TRUE;
      }
    address++;                                      /* Keep address `current' */
    }
  count = count - block_length;
  g_print(".");                                         /* Progress indicator */
  fflush(stdout);                       /* Ensure progress printing displayed */
  }                                                           /* end of while */

return error;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
/* Get an item of `size' bytes from the current position in `fHandle'         */
/* The global variable `encoding' indicated endianness (2 = big, else little) */

unsigned int get_item(FILE *fHandle, int size)
{
unsigned int i, j;

if (encoding == 2)                                              /* Big endian */
  for (i = 0, j = 0; i < size; i++) { j = (j << 8) | fgetc(fHandle); }
else                                                 /*  else assume sensible */
  for (i = 0, j = 0; i < size; i++) { j = j | fgetc(fHandle) << (8 * i); }

return j;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

/******************************************************************************/

/*                                end of flash.c                              */
/*============================================================================*/
