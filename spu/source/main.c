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

uint32_t calc(float x0, float y0)
{
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
   
   return r;
}

/* -------------------------------------------------------------------- */
int main(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4) {
	/* get data structure */
	spu_ea = arg1;
	mfc_get(&spu, spu_ea, sizeof(spustr_t), TAG, 0, 0);
	wait_for_completion();

   uint32_t data[1920];    // Max resolution is supposed to be 1920x1080.

   while (1)
   {
      uint32_t i;
      /* blocking wait for signal notification */
      spu_read_signal1();

      /* Retrieve the command using DMA */
      uint64_t ea = spu.command_ea;
      spucommand_t command;
      mfc_get(&command, ea, sizeof(spucommand_t), TAG, 0, 0);
      wait_for_completion();

      /* If the command is to quit, break out of the loop */
      if (command.cmd == CMD_QUIT) break;

      /* Fill the results.. */
      for (i=0; i<command.width; i++)
      {
         float p = command.start + ((command.end - command.start) / command.width) * i;
         data[i] = calc(p, command.yvalue);
      }

      /* write the result back */
      mfc_put(data, command.dest_ea, command.width*sizeof(uint32_t), TAG, 0, 0);
      /* no wait for completion here, as it's done after response message,
        and the sync element is send with a fence so we're sure our array
        share is written back before sync */

      /* send the response message */
      send_response(1);
      wait_for_completion();
   }

	/* properly exit the thread */
	spu_thread_exit(0);
	return 0;
}
