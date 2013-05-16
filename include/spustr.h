#define CMD_QUIT (1)
#define CMD_CALC (2)

typedef struct
{
   uint32_t id;         /* spu thread id */
   uint32_t rank;       /* rank in SPU thread group (0..count-1) */
   uint32_t count;      /* number of threads in group */
   uint32_t sync;       /* sync value for response */
   uint32_t response;   /* response value */
   uint32_t array_ea;   /* effective address of data array */
   uint32_t command_ea; /* effective address of command */
   uint32_t dummy[1];   /* unused data for 16-byte multible size */
} spustr_t;


typedef struct
{
   uint32_t cmd;
   uint32_t width;      /* Line width. */
   float    start;      /* Value at start */
   float    end;        /* Value at end */
   float    yvalue;     /* Value to use for Y */
   uint32_t dest_ea;    /* destination in framebuffer */
   uint32_t dummy[2];
} spucommand_t;
