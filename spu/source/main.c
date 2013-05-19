#include <spu_intrinsics.h>
#include <spu_mfcio.h>

#define TAG 1

#include "spustr.h"

extern void spu_thread_exit(uint32_t);

/* The effective address of the input structure */
uint64_t spu_ea;
/* A copy of the structure sent by ppu */
spustr_t spu __attribute__((aligned(16)));

/* wait for dma transfer to be finished */
static void wait_for_completion(void) {
	mfc_write_tag_mask(1<<TAG);
	spu_mfcstat(MFC_TAG_UPDATE_ALL);
}

static void send_response(uint32_t x) {
	spu.response = x;
	spu.sync = 1;
	/* send response to ppu variable */
	uint64_t ea = spu_ea + ((uint32_t)&spu.response) - ((uint32_t)&spu);
	mfc_put(&spu.response, ea, 4, TAG, 0, 0);
	/* send sync to ppu variable with fence (this ensures sync is written AFTER response) */
	ea = spu_ea + ((uint32_t)&spu.sync) - ((uint32_t)&spu);
	mfc_putf(&spu.sync, ea, 4, TAG, 0, 0);
}

void calc(spucommand_t *command, uint32_t *data)
{

   float       y0 = command->yvalue;
   uint32_t    i;

   for (i=0; i<command->width; i++)
   {
      float x0 = command->start + ((command->end - command->start) / command->width) * i;

      uint32_t    r = 0x0;

      float x=0.0;
      float y=0.0;

      while (x*x + y*y < 4.0 && r < 0x00ffffff)
      {
         float xtemp = x*x - y*y + x0;
         y = 2*x*y + y0;

         x = xtemp;

         r += 0x00010101;
      }

      *data++ = r;
   }
}

void calc_vector(spucommand_t *command, uint32_t *data)
{
   int   i,j;
   vector float   y0;
   vector float   four = spu_splats((float)4.0);
   vector float   x0;
   vector float   x0d;

   for (j=0; j<4; j++)
   {
      float x1 = ((command->end - command->start) / command->width);
      float xs = command->start + x1*j;
      // Load into x0
      x0 = spu_insert(xs, x0, j);

      x1 *= 4;
      x0d = spu_insert(x1, x0d, j);
   }

   y0 = spu_splats(command->yvalue);

   // we are going to do 4 calculations at the same time.
   for (i=0; i<command->width/4; i++)
   {
      vector unsigned int r = spu_splats((unsigned int)0x0);

      vector float x = spu_splats((float)0.0);
      vector float y = spu_splats((float)0.0);
      vector unsigned int rv = spu_splats((unsigned int)0);
      vector unsigned int use = spu_splats((unsigned int)0xffffffff);

      int depth=0;
      while (depth++ < 255)
      {
         vector float xtemp = x*x - y*y + x0;
         y = 2*x*y + y0;

         x = xtemp;

         vector float d = x*x + y*y;

         vector unsigned int n = spu_cmpgt(four, d);

         /* use starts as 0xffff, dus normaal nemen we r altijd */
         rv = spu_sel(rv, r, use);

         /* pas use aan, afhankelijk van n */
         use = spu_and(n, use);

         r += spu_splats((unsigned int)0x00010101);
      }

      *(vector unsigned int*)data = rv;
      data+=4;

      x0 += x0d;


   }

}

/* -------------------------------------------------------------------- */
int main(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
	/* get data structure */
	spu_ea = arg1;
	mfc_get(&spu, spu_ea, sizeof(spustr_t), TAG, 0, 0);
	wait_for_completion();

   uint32_t data[1920] __attribute__((aligned(16)));    // Max resolution is supposed to be 1920x1080.

   while (1)
   {
      /* blocking wait for signal notification */
      spu_read_signal1();

      /* Retrieve the command using DMA */
      uint64_t ea = spu.command_ea;
      spucommand_t command;
      mfc_get(&command, ea, sizeof(spucommand_t), TAG, 0, 0);
      wait_for_completion();

      /* If the command is to quit, break out of the loop */
      if (command.cmd == CMD_QUIT) break;

      uint32_t t = spu_read_decrementer();

      calc_vector(&command, data);

      t = t - spu_read_decrementer();

      /* write the result back */
      mfc_put(data, command.dest_ea, command.width*sizeof(uint32_t), TAG, 0, 0);
      /* no wait for completion here, as it's done after response message,
        and the sync element is send with a fence so we're sure our array
        share is written back before sync */

      /* send the response message */
      send_response(t);
      wait_for_completion();
   }

	/* properly exit the thread */
	spu_thread_exit(0);
	return 0;
}
