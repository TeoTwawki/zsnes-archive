#define __STDC_CONSTANT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cpu/zspc/zspc.h"

#define NUMCONV_BT32
#include "numconv.h"

#ifdef __WIN32__
#include <dsound.h>

typedef bool adev_t;

static LPDIRECTSOUND8 lpds = 0;
static LPDIRECTSOUNDBUFFER hdspribuf = 0;    //primary direct sound buffer
static LPDIRECTSOUNDBUFFER hdsbuf = 0;       //secondary direct sound buffer (stream buffer)
static int ds_buffer_size = 0;               //size in bytes of the direct sound buffer
static int ds_write_offset = 0;              //offset of the write cursor in the direct sound buffer
static int ds_min_free_space = 0;            //if the free space is below this value get_space() will return 0

static HRESULT ds_error(HRESULT res)
{
#ifdef DEBUG
  switch (res)
  {
    case DS_OK: puts("Okay"); break;
    case DSERR_BUFFERLOST: puts("Buffer Lost"); break;
    case DSERR_INVALIDCALL: puts("Invalid Call"); break;
    case DSERR_INVALIDPARAM: puts("Invalid Param"); break;
    case DSERR_PRIOLEVELNEEDED: puts("Priority Needed"); break;
    case DSERR_BADFORMAT: puts("Bad Format"); break;
    case DSERR_OUTOFMEMORY: puts("Out of Memory"); break;
    case DSERR_UNSUPPORTED: puts("Unsupported"); break;
    default: puts("Unknown Error"); break;
  }
#endif
  return(res);
}

static void ds_initialize()
{
  WAVEFORMATEX wfx;
  DSBUFFERDESC dsbpridesc;
  DSBUFFERDESC dsbdesc;

  ds_error(DirectSoundCreate8(0, &lpds, 0));
  ds_error(lpds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_EXCLUSIVE));

  memset(&wfx, 0, sizeof(wfx));
  wfx.wFormatTag = WAVE_FORMAT_PCM;
  wfx.nChannels = 2;
  wfx.nSamplesPerSec = 32000;
  wfx.wBitsPerSample = 16;
  wfx.nBlockAlign = wfx.nChannels * (wfx.wBitsPerSample >> 3);
  wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

  //Fill in primary sound buffer descriptor
  memset(&dsbpridesc, 0, sizeof(DSBUFFERDESC));
  dsbpridesc.dwSize = sizeof(DSBUFFERDESC);
  dsbpridesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

  //Fill in the secondary sound buffer (=stream buffer) descriptor
  memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
  dsbdesc.dwSize = sizeof(DSBUFFERDESC);
  dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 //Better position accuracy
                  | DSBCAPS_GLOBALFOCUS         //Allows background playing
                  | DSBCAPS_CTRLVOLUME;         //volume control enabled

  dsbdesc.dwBufferBytes = wfx.nChannels * wfx.nSamplesPerSec * (16>>3);
  dsbdesc.lpwfxFormat = &wfx;
  ds_buffer_size = dsbdesc.dwBufferBytes;

  //create primary buffer and set its format
  ds_error(lpds->CreateSoundBuffer(&dsbpridesc, &hdspribuf, 0));
  ds_error(hdspribuf->SetFormat((WAVEFORMATEX *)&wfx));

  // now create the stream buffer
  if (ds_error(lpds->CreateSoundBuffer(&dsbdesc, &hdsbuf, 0)) != DS_OK)
  {
    if (dsbdesc.dwFlags & DSBCAPS_LOCHARDWARE)
    {
      // Try without DSBCAPS_LOCHARDWARE
      dsbdesc.dwFlags &= ~DSBCAPS_LOCHARDWARE;
      ds_error(lpds->CreateSoundBuffer(&dsbdesc, &hdsbuf, 0));
    }
  }

  ds_write_offset = 0;
  ds_min_free_space = wfx.nBlockAlign;
}

static int ds_write_buffer(unsigned char *data, int len)
{
  HRESULT res;
  LPVOID lpvPtr1; 
  DWORD dwBytes1; 
  LPVOID lpvPtr2; 
  DWORD dwBytes2; 

  // Lock the buffer
  res = ds_error(hdsbuf->Lock(ds_write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0));
  // If the buffer was lost, restore and retry lock.
  if (res == DSERR_BUFFERLOST)
  {
    hdsbuf->Restore();
    res = ds_error(hdsbuf->Lock(ds_write_offset, len, &lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0));
  }

  if (SUCCEEDED(res))
  {
    // Write to pointers without reordering.
    memcpy(lpvPtr1, data, dwBytes1);
    if (lpvPtr2) { memcpy(lpvPtr2, data+dwBytes1, dwBytes2); }
    ds_write_offset += dwBytes1+dwBytes2;
    if (ds_write_offset >= ds_buffer_size) { ds_write_offset = dwBytes2; }

   // Release the data back to DirectSound.
    res = ds_error(hdsbuf->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2));
    if (SUCCEEDED(res))
    {
      // Success.
      DWORD status;
      ds_error(hdsbuf->GetStatus(&status));
      if (!(status & DSBSTATUS_PLAYING))
      {
        ds_error(hdsbuf->Play(0, 0, DSBPLAY_LOOPING));
      }
      return(dwBytes1+dwBytes2);
    }
  }
  // Lock, Unlock, or Restore failed.
  return(0);
}

static bool ds_play(adev_t, char *samples_buffer, size_t samples_count)
{
  unsigned char *data = (unsigned char *)samples_buffer;
  int samples_outputted, samples_remaining;

  samples_remaining = samples_count;
  for (;;)
  {
    DWORD play_offset;
    int space, len = samples_remaining;

    // make sure we have enough space to write data
    ds_error(hdsbuf->GetCurrentPosition(&play_offset, 0));
    space = ds_buffer_size-(ds_write_offset-play_offset);
    if (space > ds_buffer_size) { space -= ds_buffer_size; } // ds_write_offset < play_offset
    if (space < len) { len = space; }

    samples_outputted = ds_write_buffer((unsigned char *)data, len);

    data += samples_outputted;
    samples_remaining -= samples_outputted;
  }
  return(true);
}

void ds_shutdown()
{
  if (hdsbuf) { hdsbuf->Release(); }
  if (hdspribuf) { hdspribuf->Release(); }
  if (lpds) { lpds->Release(); }
}

adev_t ds_init()
{
  return(true);
}

#define ao_initialize ds_initialize
#define ao_shutdown ds_shutdown
#define ao_init ds_init
#define ao_play ds_play

#else
#include <ao/ao.h>

typedef ao_device *adev_t;

adev_t ao_init()
{
  static ao_device *dev = 0;
  if (!dev)
  {
    struct ao_sample_format format = { 16, 32000, 2, AO_FMT_LITTLE };
    dev = ao_open_live(ao_default_driver_id(), &format, NULL);

    if (dev)
    {
      ao_info *di = ao_driver_info(dev->driver_id);
      printf("\nAudio Opened.\nDriver: %s\nChannels: %u\nRate: %u\n\n", di->name, format.channels, format.rate);
    }
  }

  return(dev);
}

#endif //End of OS specific code


struct header_t
{
  char    tag[33];
  uint8_t marker[3];
  uint8_t marker2;
  uint8_t pc[2];
  uint8_t a, x, y, statflags, stack;
  uint8_t reserved[2];
  char    song[32];
  char    game[32];
  char    dumper[16];
  char    comment[32];
  uint8_t date[4];
  uint8_t reserved2[7];
  uint8_t len_secs[4];
  uint8_t fade_msec[3];
  char    author[32];
  uint8_t mute_mask;
  uint8_t emulator;
  uint8_t reserved3[46];
};

static void read_spcfile(const char *fname, struct header_t *header)
{
  FILE *fp = fopen(fname, "rb");
  if (fp)
  {
    uint8_t spcdata[67000]; //We should adjust this later for the max possible size;
    size_t size = fread(spcdata, 1, sizeof(spcdata), fp);
    memcpy(header, spcdata, sizeof(struct header_t));
    zspc_load_spc(spcdata, size);
    zspc_clear_echo();
    fclose(fp);
  }
}

static void print_spcinfo(struct header_t *header)
{
  char *emulator;
  switch (header->emulator)
  {
    case 1:
      emulator = "ZSNES";
      break;
    case 2:
      emulator = "Snes9x";
      break;
    default:
      emulator = "Unknown";
      break;
  }
  printf("Emulator: %s\n  Dumper: %s\n    Game: %s\n   Title: %s\n  Artist: %s\n  Length: %u\n\n", emulator, header->dumper, header->game, header->song, header->author, bytes_to_uint32(header->len_secs));
}

bool all_silence(int16_t *buffer, size_t len)
{
  while (len--)
  {
    if ((*buffer < -2) || (*buffer > 2)) { return(false); }
  }
  return(true);
}

void run_spc(adev_t dev, struct header_t *header)
{
  if (dev)
  {
    int16_t samples_buffer[2048];
    size_t samples_count = sizeof(samples_buffer)/sizeof(int16_t);

    size_t samples_played = 0, silence_count = 0;
    uint32_t play_secs = bytes_to_uint32(header->len_secs);

    if (!play_secs)
    {
      play_secs = ~0;
    }

    while (samples_played/(zspc_sample_rate*2) < play_secs)
    {
      zspc_play(samples_count, samples_buffer);

      if (all_silence(samples_buffer, samples_count))
      {
        silence_count++;
        if (silence_count == 20) //~2 seconds of silence
        {
          play_secs = 0;
        }
      }
      else
      {
        silence_count = 0;
      }

      if (!ao_play(dev, (char*)samples_buffer, samples_count*2))
      {
        exit(-1);
      }
      samples_played += samples_count;
    }
  }
}

int main(int argc, char *argv[])
{
  struct header_t header;
  assert(sizeof(struct header_t) == 0x100);

  ao_initialize();
  atexit(ao_shutdown);

  while (--argc)
  {
    zspc_init();
    read_spcfile(*(++argv), &header);
    print_spcinfo(&header);
    run_spc(ao_init(), &header);
  }

  return(0);
}
