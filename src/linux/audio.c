/*
Copyright (C) 1997-2008 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

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
#include "../cpu/zspc/zspc.h"

#ifdef __LIBAO__
static pthread_t audio_thread;
static pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t audio_wait = PTHREAD_COND_INITIALIZER;
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
static short ao_buffer1[1070];
static short ao_buffer2[2048];
static unsigned int ao_samples = 0;

void SoundWrite_ao()
{
  if (!pthread_mutex_trylock(&audio_mutex))
  {
    zspc_flush_samples();
    ao_samples = zspc_sample_count();
    //printf("Samples Ready: %u\n", ao_samples);
    memcpy(ao_buffer2, ao_buffer1, ao_samples*sizeof(short));
    zspc_set_output(ao_buffer1, sizeof(ao_buffer1)/sizeof(short));

    pthread_cond_broadcast(&audio_wait); //Send signal
    pthread_mutex_unlock(&audio_mutex);
  }
  else
  {
    pthread_cond_broadcast(&audio_wait); //Send signal
  }
}

static void *SoundThread_ao(void *useless)
{
  for (;;)
  {
    unsigned int samples;
    pthread_mutex_lock(&audio_mutex);

    //The while() is there to prevent error codes from breaking havoc
     while (!ao_samples)
     {
       pthread_cond_wait(&audio_wait, &audio_mutex); //Wait for signal
     }

     samples = ao_samples;
     ao_samples = 0;
     pthread_mutex_unlock(&audio_mutex);

     ao_play(audio_device, (char*)ao_buffer2, samples*2);
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
  }

  //ao_option driver_options = { "buf_size", "32768", 0 };

  audio_device = ao_open_live(driver_id, &driver_format, 0);
  if (audio_device)
  {
    ao_info *di = ao_driver_info(driver_id);
    printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, driver_format.channels, driver_format.rate);

    memset(ao_buffer1, 0, sizeof(ao_buffer1));
    memset(ao_buffer2, 0, sizeof(ao_buffer2));
    zspc_set_output(ao_buffer1, sizeof(ao_buffer1)/sizeof(short));
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
  SDL_LockAudio();
  if (dsp_sample_count) //Lets have less memset()s
  {
    sdl_audio_buffer_tail += dsp_samples_pull((short *)(sdl_audio_buffer+sdl_audio_buffer_tail), (sdl_audio_buffer_len-sdl_audio_buffer_tail)/2)*2;
  }
  SDL_UnlockAudio();
*/
/*
  extern unsigned int T36HZEnabled;

  SDL_LockAudio();
  while (dsp_sample_count && (sdl_audio_buffer_fill < sdl_audio_buffer_len))
  {
    short *dest = (short *)(sdl_audio_buffer+sdl_audio_buffer_tail);
    size_t pull = 512;
    if (T36HZEnabled)
    {
      memset(dest, 0, pull);
    }
    else
    {
      pull = dsp_samples_pull(dest, 256)*2;
    }

    sdl_audio_buffer_fill += pull;
    sdl_audio_buffer_tail += pull;
    if (sdl_audio_buffer_tail >= sdl_audio_buffer_len) { sdl_audio_buffer_tail = 0; }
  }
  SDL_UnlockAudio();
*/
}

static void SoundUpdate_sdl(void *userdata, unsigned char *stream, int len)
{
  size_t extra = 0;
  if (len > sdl_audio_buffer_tail)
  {
    extra = len-sdl_audio_buffer_tail;
    len = sdl_audio_buffer_tail;
  }
  if (len)
  {
    memcpy(stream, sdl_audio_buffer, len);
    sdl_audio_buffer_tail -= len;
    memmove(sdl_audio_buffer, sdl_audio_buffer+len, sdl_audio_buffer_tail);
  }
  if (extra)
  {
    memset(stream+len, 0, extra);
  }
/*
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
*/
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

