/*----------------------------------------------------------------------------*/
/* Header file introduced 23/8/10                                             */
/* For inclusion into 'shared' libaries.                                      */
/*----------------------------------------------------------------------------*/

#define REG_CURRENT 0  /* Values to force register accesses to specified bank */
#define REG_USER    1                                        /* ... or system */
#define REG_SVC     2
#define REG_FIQ     3
#define REG_IRQ     4
#define REG_ABT     5
#define REG_UNDEF   6

#define MEM_SYSTEM       0                         /* sources for memory read */
#define MEM_INSTRUCTION  1
#define MEM_DATA         2
