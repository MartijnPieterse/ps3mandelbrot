/*
 * History:
 * 2013-05-03: Creation (Martijn Pieterse)
 *
 */

#include <ppu-lv2.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>

#include <io/pad.h>
#include "rsxutil.h"

#define MAX_BUFFERS 2

/*
 * Calculate the Mandelbrot
 */



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

void drawFrame(rsxBuffer *buffer, float mCx, float mCy, float mSx, float mSy)
{
   s32 i, j;

   for(i = 0; i < buffer->height; i+=1)
   {
      for(j = 0; j < buffer->width; j+=1)
      {
         float x = (mSx / buffer->width) * (j - (buffer->width/2))  + mCx;
         float y = (mSy / buffer->height) * (i - (buffer->height/2)) + mCy;
         s32 color = mandelb(x, y);
         buffer->ptr[i * buffer->width + j] = iter2color(color);
//         buffer->ptr[(i+1) * buffer->width + j] = iter2color(color);
//         buffer->ptr[(i+1) * buffer->width + (j+1)] = iter2color(color);
//         buffer->ptr[i * buffer->width + (j+1)] = iter2color(color);
      }
   }
}

int main(s32 argc, const char* argv[])
{
   gcmContextData    *context;
   void              *host_addr = NULL;
   rsxBuffer          buffers[MAX_BUFFERS];
   int                currentBuffer = 0;
   u16                width;
   u16                height;
   float mCx = -0.5;  // Center X
   float mCy = 0.0;  // Center Y
   float mSx = 3.5;  // Size X * 2
   float mSy = 3.5;  // Size Y * 2
        
   /* 
    * Allocate a 32Mb buffer, alligned to a 1Mb boundary                          
    * to be our shared IO memory with the RSX. 
    */
   host_addr = memalign (1024*1024, HOST_SIZE);
   context = initScreen (host_addr, HOST_SIZE); // rsxutil.c
   ioPadInit(1);  // Waarom niet MAX_PADS??

   getResolution(&width, &height); // rsxutil.c

   for (int i=0; i < MAX_BUFFERS; i++)
   {
      makeBuffer( &buffers[i], width, height, i);
   }

   flip(context, MAX_BUFFERS - 1);

   // Ok, everything is setup. Now for the main loop.
   padInfo            padinfo;
   padData            paddata;
   paddata.ANA_L_H = 128;
   paddata.ANA_L_V = 128;
   paddata.ANA_R_V = 128;
   float moves = 0.3;
   while(1)
   {

      // Check the pads.
      ioPadGetInfo(&padinfo);
      for (int i=0; i < 1; i++)
      {
         if(padinfo.status[i])
         {
            if (!ioPadGetData(i, &paddata))
            {
                                   
               if(paddata.BTN_START)
               {
                  goto end;
               }


               float delta = 0.0;
               delta = moves * ((paddata.ANA_L_H & 0xff) / 256.0 - 0.5);
               mCx += delta;
               delta = moves * ((paddata.ANA_L_V & 0xff) / 256.0 - 0.5);
               mCy += delta;

               if ((paddata.ANA_R_V & 0xff) < 64)
               {
                  mSx *= 0.95;
                  mSy *= 0.95;
                  moves *= 0.95;
               }
               if ((paddata.ANA_R_V & 0xff) > 192)
               {
                  mSx *= 1.05;
                  mSy *= 1.05;
                  moves *= 1.05;
               }
            }

         }
                        
      }


      waitFlip(); // Wait for the last flip to finish, so we can draw to the old buffer
      drawFrame(&buffers[currentBuffer], mCx, mCy, mSx, mSy); // Draw into the unused buffer
      flip(context, buffers[currentBuffer].id); // Flip buffer onto screen

      currentBuffer = (currentBuffer+1)%MAX_BUFFERS;

   }
  
end:
   gcmSetWaitFlip(context);
   for (int i=0; i < MAX_BUFFERS; i++)
   {
      rsxFree(buffers[i].ptr);
   }

   rsxFinish(context, 1);
   free(host_addr);
   ioPadEnd();
        
   return 0;
}
