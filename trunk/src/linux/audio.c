/*
Copyright (C) 1997-2007 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

http://www.zsnes.com
http://sourceforge.net/projects/zsnes
https://zsnes.bountysource.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "../gblhdr.h"
#include <stdbool.h>

#ifdef __LIBAO__
#include <ao/ao.h>
#include <pthread.h>
#include <signal.h>
#endif

#include "../asm_call.h"
#include "../cfg.h"
#include "cpu/spc.h"
unsigned int BufferSizeB, BufferSizeW;
short DSPBuffer[1280];

#ifdef __LIBAO__
static pthread_t audio_thread;
static pthread_mutex_t audio_mutex;
static pthread_cond_t audio_wait;
static ao_device *audio_device = 0;
static volatile size_t samples_waiting = false;
#endif

unsigned char *sdl_audio_buffer = 0;
int sdl_audio_buffer_len = 0, sdl_audio_buffer_fill = 0;
int sdl_audio_buffer_head = 0, sdl_audio_buffer_tail = 0;
unsigned char sound_sdl = false;

int SoundEnabled = 1;
unsigned char PrevStereoSound;
unsigned int PrevSoundQuality;

static const int freqtab[7] = { 8000, 11025, 22050, 44100, 16000, 32000, 48000 };
#define RATE freqtab[SoundQuality = ((SoundQuality > 6) ? 1 : SoundQuality)]


#ifdef __LIBAO__

void SoundWrite_ao()
{
  if (!pthread_mutex_lock(&audio_mutex))
  {
    samples_waiting += dsp_sample_count;
    if (samples_waiting > 1280)
    {
      //pthread_cond_wait(&audio_wait, &audio_mutex); //Wait for signal
    }
    pthread_mutex_unlock(&audio_mutex);
  }
}

static void *SoundThread_ao(void *useless)
{
  static unsigned int buffer_loc = 0;
  unsigned int play_amount;
  for (;;)
  {
    pthread_cond_broadcast(&audio_wait); //Send signal
    pthread_mutex_lock(&audio_mutex);

    if (samples_waiting > 256) { play_amount = 256; }
    else { play_amount =  samples_waiting; }
    samples_waiting -= play_amount;
    pthread_mutex_unlock(&audio_mutex);

    if (buffer_loc + play_amount >= dsp_buffer_size)
    {
      unsigned int current_amount = dsp_buffer_size - buffer_loc;
      ao_play(audio_device, (char *)(dsp_samples_buffer+buffer_loc), current_amount*sizeof(short));
      play_amount -= current_amount;
      buffer_loc = 0;
    }
    if (play_amount)
    {
      ao_play(audio_device, (char *)(dsp_samples_buffer+buffer_loc), play_amount*sizeof(short));
      buffer_loc += play_amount;
    }
  }
  return(0);
}

static int SoundInit_ao()
{
  int driver_id = ao_driver_id(libAoDriver);
  if (driver_id < 0) { driver_id = ao_default_driver_id(); }

  ao_sample_format driver_format;
  driver_format.bits = 16;
  driver_format.channels = StereoSound+1;
  driver_format.rate = freqtab[SoundQuality = ((SoundQuality > 6) ? 1 : SoundQuality)];
  driver_format.byte_format = AO_FMT_LITTLE;

  if (audio_device)
  {
    ao_close(audio_device);
  }
  else
  {
    if (pthread_create(&audio_thread, 0, SoundThread_ao, 0))
    {
      puts("pthread_create() failed.");
    }
    else if (pthread_mutex_init(&audio_mutex, 0))
    {
      puts("pthread_mutex_init() failed.");
    }
    else if (pthread_cond_init(&audio_wait, 0))
    {
      puts("pthread_cond_init() failed.");
    }
  }

  //ao_option driver_options = { "buf_size", "32768", 0 };

  audio_device = ao_open_live(driver_id, &driver_format, 0);
  if (audio_device)
  {
    ao_info *di = ao_driver_info(driver_id);
    printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, driver_format.channels, driver_format.rate);
  }
  else
  {
    SoundEnabled = 0;
    puts("Audio Open Failed");
    return(false);
  }
  return(true);
}

#endif

void SoundWrite_sdl()
{
/*
  extern unsigned char DSPDisable;
  extern unsigned T36HZEnabled;

  DSP_buf = DSPBuffer;
  // Process sound
  BufferSizeB = 256;
  BufferSizeW = 512;

  DSP_count = BufferSizeB/2;
  // take care of the things we left behind last time
  SDL_LockAudio();
  while (sdl_audio_buffer_fill < sdl_audio_buffer_len)
  {
    short *p = (short*)&sdl_audio_buffer[sdl_audio_buffer_tail];

    if (soundon && !DSPDisable) { dsp_run(); }

    if (T36HZEnabled)
    {
      memset(p, 0, BufferSizeW);
    }
    else
    {
      short *d = DSPBuffer, *end_d = DSPBuffer+BufferSizeB;

      memcpy(p, d, BufferSizeW);
    }

    sdl_audio_buffer_fill += BufferSizeW;
    sdl_audio_buffer_tail += BufferSizeW;
    if (sdl_audio_buffer_tail >= sdl_audio_buffer_len) { sdl_audio_buffer_tail = 0; }
  }
  SDL_UnlockAudio();
*/
}

static void SoundUpdate_sdl(void *userdata, unsigned char *stream, int len)
{
  int left = sdl_audio_buffer_len - sdl_audio_buffer_head;

  if (left > 0)
  {
    if (left <= len)
    {
      memcpy(stream, &sdl_audio_buffer[sdl_audio_buffer_head], left);
      stream += left;
      len -= left;
      sdl_audio_buffer_head = 0;
      sdl_audio_buffer_fill -= left;
    }

    if (len)
    {
      memcpy(stream, &sdl_audio_buffer[sdl_audio_buffer_head], len);
      sdl_audio_buffer_head += len;
      sdl_audio_buffer_fill -= len;
    }
  }
}

static int SoundInit_sdl()
{
  const int samptab[7] = { 1, 1, 2, 4, 2, 4, 4 };
  SDL_AudioSpec audiospec;
  SDL_AudioSpec wanted;

  SDL_CloseAudio();

  if (sdl_audio_buffer)
  {
    free(sdl_audio_buffer);
    sdl_audio_buffer = 0;
  }
  sdl_audio_buffer_len = 0;

  wanted.freq = RATE;
  wanted.channels = StereoSound+1;
  wanted.samples = samptab[SoundQuality] * 128 * wanted.channels;
  wanted.format = AUDIO_S16LSB;
  wanted.userdata = 0;
  wanted.callback = SoundUpdate_sdl;

  if (SDL_OpenAudio(&wanted, &audiospec) < 0)
  {
    SoundEnabled = 0;
    return(false);
  }
  SDL_PauseAudio(0);

  sdl_audio_buffer_len = audiospec.size*2;
  sdl_audio_buffer_len = (sdl_audio_buffer_len + 255) & ~255; // Align to SPCSize
  if (!(sdl_audio_buffer = malloc(sdl_audio_buffer_len)))
  {
    SDL_CloseAudio();
    puts("Audio Open Failed");
    SoundEnabled = 0;
    return(false);
  }

  sound_sdl = true;
  printf("\nAudio Opened.\nDriver: Simple DirectMedia Layer output\nChannels: %u\nRate: %u\n\n", wanted.channels, wanted.freq);
  return(true);
}


int InitSound()
{
  sound_sdl = false;
  if (!SoundEnabled)
  {
    return(false);
  }

  PrevSoundQuality = SoundQuality;
  PrevStereoSound = StereoSound;

  #ifdef __LIBAO__
  if (strcmp(libAoDriver, "sdl") && !(!strcmp(libAoDriver, "auto") && !strcmp(ao_driver_info(ao_default_driver_id())->name, "null")))
  {
    return(SoundInit_ao());
  }
  #endif
  return(SoundInit_sdl());
}

void DeinitSound()
{
  #ifdef __LIBAO__
  if (audio_device)
  {
    pthread_kill(audio_thread, SIGTERM);
    pthread_mutex_destroy(&audio_mutex);
    pthread_cond_destroy(&audio_wait);
    ao_close(audio_device);
  }
  #endif
  SDL_CloseAudio();
  if (sdl_audio_buffer) { free(sdl_audio_buffer); }
}

