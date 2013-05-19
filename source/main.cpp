#include <ppu-lv2.h>
#include <io/pad.h>
#include <malloc.h>

#include <sys/spu.h>

#include "rsxutil.h"

#include <cmath>

#define DEBUG
#include "debug.hpp"

#define MAX_BUFFERS (2)

// -----------------------------------------------------------------------
class MandelBrot
{
public:
   // --------------------------------------------------------------------
   MandelBrot()
   : m_x1(-2.0),
     m_x2( 1.0),
     m_y1(-1.5),
     m_y2( 1.5)
   {
   }

   // --------------------------------------------------------------------
   ~MandelBrot()
   {
   }

   // --------------------------------------------------------------------
   void Render(rsxBuffer *buffer)
   {
      s32 i, j;

      float xstep = (m_x2 - m_x1) / buffer->width;
      float ystep = (m_y2 - m_y1) / buffer->height;
      

      for(i = 0; i < buffer->height; i+=4)
      {
         for(j = 0; j < buffer->width; j+=4)
         {
            float x = m_x1 + xstep * j;
            float y = m_y1 + ystep * i;
            s32 color = mandelb(x, y);
            buffer->ptr[i * buffer->width + j] = iter2color(color);
         }
      }
   }

   // --------------------------------------------------------------------
   void Zoom(float p)
   {
      float delta = (m_x2 - m_x1) * (1.0 - p) * 0.5;
      m_x1 += delta;
      m_x2 -= delta;


      delta = (m_y2 - m_y1) * (1.0 - p) * 0.5;
      m_y1 += delta;
      m_y2 -= delta;
   }

   // --------------------------------------------------------------------
   void Move(float xmove, float ymove)
   {
      float delta = (m_x2 - m_x1) * xmove;
      m_x1 += delta;
      m_x2 += delta;

      delta = (m_y2 - m_y1) * ymove;
      m_y1 += delta;
      m_y2 += delta;
   }

   float get_x1(void) { return m_x1; }
   float get_x2(void) { return m_x2; }
   float get_y1(void) { return m_y1; }
   float get_y2(void) { return m_y2; }

private:
   // --------------------------------------------------------------------
   // 255 must be black
   s32 iter2color(s32 iter)
   {
      s32 r = 0;

      if (iter == 255) return 0;

      // Fill Red
      r += iter << 16;

      // Fill Green
      r += (((iter & 0x0f) << 4) + ((iter & 0xf0) >> 4)) << 8;

      // Fill Blue
      r += sin(iter / 255.0) * 127 + 127;

      return r;
   }

   // --------------------------------------------------------------------
   s32 mandelb(float x0, float y0)
   {
      float x=0;
      float y=0;

      s32 iteration = 0;
      s32 max_iteration = 255;

      while ( x*x + y*y < 2*2  &&  iteration < max_iteration )
      {
         float xtemp = x*x - y*y + x0;
         y = 2*x*y + y0;

         x = xtemp;

         iteration = iteration + 1;
      }

     return iteration;
   }

   float m_x1;
   float m_x2;
   float m_y1;
   float m_y2;
};

// -----------------------------------------------------------------------
// --------------- RSXClass ----------------------------------------------
// -----------------------------------------------------------------------
class RSXClass
{
public:
   // --------------------------------------------------------------------
   RSXClass()
   : m_host_addr(NULL),
     m_currentBuffer(0)
   {
      /* 
       * Allocate a 32Mb buffer, alligned to a 1Mb boundary                          
       * to be our shared IO memory with the RSX. 
       */
      m_host_addr = memalign (1024*1024, HOST_SIZE);
      m_context = initScreen (m_host_addr, HOST_SIZE); // rsxutil.c

      getResolution(&m_width, &m_height); // rsxutil.c

      for (int i=0; i < MAX_BUFFERS; i++)
      {
         makeBuffer( &m_buffers[i], m_width, m_height, i);
      }

      debugPrintf("Screen size = %d, %d\n", m_width, m_height);

      flip(m_context, MAX_BUFFERS - 1);
   }

   // --------------------------------------------------------------------
   ~RSXClass()
   {
      gcmSetWaitFlip(m_context);
      for (int i=0; i < MAX_BUFFERS; i++)
      {
         rsxFree(m_buffers[i].ptr);
      }

      rsxFinish(m_context, 1);
      free(m_host_addr);
   }

   // --------------------------------------------------------------------
   void WaitFlip()
   {
      waitFlip();
   }

   // --------------------------------------------------------------------
   void Flip()
   {
      flip(m_context, m_buffers[m_currentBuffer].id); // Flip buffer onto screen
      m_currentBuffer = (m_currentBuffer+1)%MAX_BUFFERS;
   }

   // --------------------------------------------------------------------
   rsxBuffer* getCurrentBuffer(void)
   {
      return &m_buffers[m_currentBuffer];
   }

private:
   gcmContextData*   m_context;
   void*             m_host_addr;
   rsxBuffer         m_buffers[MAX_BUFFERS];
   u16               m_width;
   u16               m_height;
   int               m_currentBuffer;

};

// -----------------------------------------------------------------------
// --------------- PadClass ----------------------------------------------
// -----------------------------------------------------------------------
class PadClass
{
public:
   // --------------------------------------------------------------------
   PadClass()
   : m_startPressed(false)
   {
      ioPadInit(1);  // Waarom niet MAX_PADS??

      m_paddata.ANA_L_H = 128;
      m_paddata.ANA_L_V = 128;
      m_paddata.ANA_R_V = 128;
      m_paddata.BTN_START = 0;
   }

   // --------------------------------------------------------------------
   ~PadClass()
   {
      ioPadEnd();
   }

   // --------------------------------------------------------------------
   void check(void)
   {
      // Check the pads.
      ioPadGetInfo(&m_padinfo);
      for (int i=0; i < 1; i++)
      {
         if(m_padinfo.status[i])
         {
            if (!ioPadGetData(i, &m_paddata))
            {
               if(m_paddata.BTN_START)
               {
                  m_startPressed = true;
               }
            }
         }
      }
   }

   // --------------------------------------------------------------------
   bool startPressed()
   {
      return m_startPressed;
   }

   // --------------------------------------------------------------------
   float stickLH(void)
   {
      return ((m_paddata.ANA_L_H & 0xff) / 128.0 - 1.0);
   }

   // --------------------------------------------------------------------
   float stickLV(void)
   {
      return ((m_paddata.ANA_L_V & 0xff) / 128.0 - 1.0);
   }

   // --------------------------------------------------------------------
   float stickRV(void)
   {
      return ((m_paddata.ANA_R_V & 0xff) / 128.0 - 1.0);
   }

private:

   padInfo     m_padinfo;
   padData     m_paddata;

   bool        m_startPressed;
};

// -----------------------------------------------------------------------
// --------------- SpuClass ----------------------------------------------
// -----------------------------------------------------------------------
extern const u8 spu_bin_end[];
extern const u8 spu_bin[];
extern const u32 spu_bin_size;
#define ptr2ea(x) ((u64)(void *)(x))
#include "spustr.h"
class SpuClass
{
public:
   // --------------------------------------------------------------------
   SpuClass()
   {
      s32   r;

      // Utilize all 6 SPUs
      debugPrintf("Initializing 6 SPU's\n");
      r = sysSpuInitialize(6, 0);
      debugPrintf("Return value: %d\n", r);

      debugPrintf("Address of spu target: %8x %8x %8x\n", spu_bin, spu_bin_end, spu_bin_size);

      // Load binary image
      debugPrintf("Loading ELF image.\n");
      r = sysSpuImageImport(&m_image, spu_bin, 0);
      debugPrintf("Return value: %d\n", r);

      // Creat thread group...
      debugPrintf("About to create SpuThreadGroup\n");
      sysSpuThreadGroupAttribute grpattr = { 7+1, ptr2ea("mygroup"), 0, 0 };
      r = sysSpuThreadGroupCreate(&m_group_id, 6, 100, &grpattr);
      debugPrintf("Return value: %d\n", r);
      debugPrintf("Group Id: %d\n", m_group_id);


      // To be calculated array...
      m_array = (uint32_t*)memalign(16, 24*sizeof(uint32_t));
      m_command = (spucommand_t*)memalign(16, 6*sizeof(spucommand_t));

      // Create all 6 SPU's
      m_spu = (spustr_t *)memalign(16, 6*sizeof(spustr_t));
      sysSpuThreadAttribute attr = { ptr2ea("mythread"), 8+1, SPU_THREAD_ATTR_NONE };
      sysSpuThreadArgument arg[6];
      for (int i=0; i<6; i++)
      {
         m_spu[i].id = -1;
         m_spu[i].rank = i;
         m_spu[i].count = 6;
         m_spu[i].sync = 0;
         m_spu[i].array_ea = ptr2ea(m_array);
         m_spu[i].command_ea = ptr2ea(&m_command[i]);
         arg[i].arg0 = ptr2ea(&m_spu[i]);

         sysSpuThreadInitialize(&m_spu[i].id, m_group_id, i, &m_image, &attr, &arg[i]);
         sysSpuThreadSetConfiguration(m_spu[i].id, SPU_SIGNAL1_OVERWRITE|SPU_SIGNAL2_OVERWRITE);
      }

      sysSpuThreadGroupStart(m_group_id);

   }


   // --------------------------------------------------------------------
   ~SpuClass()
   {
      s32 r;
      // Send Quit Command to All SPUs
      debugPrintf("Sending QUIT command...\n");
      m_command[0].cmd = CMD_QUIT;
      m_command[1].cmd = CMD_QUIT;
      m_command[2].cmd = CMD_QUIT;
      m_command[3].cmd = CMD_QUIT;
      m_command[4].cmd = CMD_QUIT;
      m_command[5].cmd = CMD_QUIT;

      /* Send signal notification to waiting spus */
      for (int i = 0; i < 6; i++)
      {
         r = sysSpuThreadWriteSignal(m_spu[i].id, 0, 1);
//         debugPrintf("Sending signal... %08x\n", r);
      }


      u32 cause, status;
      debugPrintf("Joining SPU thread group... ");

      r = sysSpuThreadGroupJoin(m_group_id, &cause, &status);
      debugPrintf("%08x\n", r);
      debugPrintf("cause=%d status=%d\n", cause, status);

      r = sysSpuImageClose(&m_image);
      debugPrintf("Closing image... %08x\n", r);

      free(m_array);
      free(m_spu);

   }

// 1 SPU = 210ms
// 2 SPU = 104ms
// 3 SPU =  70ms
// 4 SPU =  51ms
// 5 SPU =  42ms
// 6 SPU =  35ms
#define SPU_USAGE (6)
   void Calc2(rsxBuffer *buffer, float x1, float x2, float y1, float y2)
   {
      unsigned long long   t = __mftb();
      int sput = 0;

      int next_spu = 0;
      for (int i = 0; i < SPU_USAGE; i++)
      {
         m_spu[i].sync = 1;
         m_spu[i].response = 0;
      }

      for (int j = 0; j < buffer->height;)
      {
         if (m_spu[next_spu].sync != 0)
         {
            sput += m_spu[next_spu].response;
            m_spu[next_spu].sync = 0;
            m_command[next_spu].start = x1;
            m_command[next_spu].end = x2;
            m_command[next_spu].yvalue = y1 + ((y2-y1) / buffer->height) * j;
            m_command[next_spu].cmd = CMD_CALC;
            m_command[next_spu].width = buffer->width;
            m_command[next_spu].dest_ea = ptr2ea(&(buffer->ptr[j*buffer->width]));

            (void)sysSpuThreadWriteSignal(m_spu[next_spu].id, 0, 1);

            j++;
         }
         next_spu = (next_spu+1)%SPU_USAGE;
      }

      // Wait for all spus to finish.
      for (int i = 0; i < SPU_USAGE; i++)
      {
         while (m_spu[i].sync == 0);
         sput += m_spu[next_spu].response;
      }

      t = __mftb() - t;

      // 400.000 = 5000us
      // 400 = 5us
      //  80 = 1us
      t = t / 80;
      debugPrintf("tijd: %d.%06d   ", t / 1000000, t % 1000000);

      // 200ms = alle lijnen = 15893107
      // 1 lijn is 32343
      // ~490 lijnen, res = 480. Klopt.
      // Ook 80MHz?
      sput = sput / 80;
      debugPrintf("sputijd: %d.%06d\n", sput / 1000000, sput % 1000000);
   }
private:


   u32            m_group_id;
   sysSpuImage    m_image;
   uint32_t      *m_array;
   spucommand_t  *m_command;
   spustr_t      *volatile m_spu;

};


// -----------------------------------------------------------------------
// --------------- main ----------------------------------------------
// -----------------------------------------------------------------------
int main(s32 argc, const char* argv[])
{
   debugInit();

   MandelBrot         mandel;
   RSXClass          *rsx = new RSXClass();
   PadClass          *pad = new PadClass();

   SpuClass          *spu = new SpuClass();


   // Ok, everything is setup. Now for the main loop.
   while(!pad->startPressed())
   {
      pad->check();

      rsx->WaitFlip();

      //mandel.Render(rsx->getCurrentBuffer());

      spu->Calc2(rsx->getCurrentBuffer(), mandel.get_x1(), mandel.get_x2(), mandel.get_y1(), mandel.get_y2());
      rsx->Flip();

      mandel.Move(pad->stickLH()*0.1, pad->stickLV()*0.1);
      mandel.Zoom(1.0 + pad->stickRV()*0.05);
   }

   delete spu;
   delete rsx;
   delete pad; 

   debugStop();

   return 0;

}
