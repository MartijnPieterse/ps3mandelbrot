#include <ppu-lv2.h>
#include <io/pad.h>
#include <malloc.h>

#include "rsxutil.h"

#include <cmath>

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
      

      for(i = 0; i < buffer->height; i++)
      {
         for(j = 0; j < buffer->width; j++)
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

private:

   padInfo     m_padinfo;
   padData     m_paddata;

   bool        m_startPressed;
};

int main(s32 argc, const char* argv[])
{

   MandelBrot         mandel;
   RSXClass          *rsx = new RSXClass();
   PadClass          *pad = new PadClass();


   // Ok, everything is setup. Now for the main loop.

   while(!pad->startPressed())
   {
      pad->check();

      rsx->WaitFlip();

      mandel.Render(rsx->getCurrentBuffer());
      rsx->Flip();

      mandel.Move(pad->stickLH()*0.1, pad->stickLV()*0.1);
      mandel.Zoom(1.0);
   }

   delete rsx;
   delete pad; 

   return 0;

}
