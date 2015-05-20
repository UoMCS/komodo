/*----------------------------------------------------------------------------*/
/* Header file introduced 23/8/10                                             */
/* For inclusion into 'shared' libaries.                                      */
/*----------------------------------------------------------------------------*/

/* The following define possible states of the board */

#define CLIENT_STATE_CLASS_MASK     0XC0
#define CLIENT_STATE_CLASS_RESET    0X00
#define CLIENT_STATE_CLASS_STOPPED  0X40
#define CLIENT_STATE_CLASS_RUNNING  0X80
#define CLIENT_STATE_CLASS_ERROR    0XC0
#define CLIENT_STATE_RESET          0X00
#define CLIENT_STATE_BUSY           0X01
#define CLIENT_STATE_STOPPED        0X40
#define CLIENT_STATE_BREAKPOINT     0X41
#define CLIENT_STATE_WATCHPOINT     0X42
#define CLIENT_STATE_MEMFAULT       0X43
#define CLIENT_STATE_BYPROG         0X44
#define CLIENT_STATE_RUNNING        0X80
#define CLIENT_STATE_RUNNING_BL     0x81    //  @@@ NEEDS UPDATE
#define CLIENT_STATE_RUNNING_SVC    0x81
#define CLIENT_STATE_STEPPING       0X82

#define CLIENT_STATE_BROKEN         0x30
