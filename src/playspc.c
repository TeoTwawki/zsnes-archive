#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ao/ao.h>
#include "cpu/zspc/zspc.h"

#define NUMCONV_BT16
#define NUMCONV_BT32
#include "numconv.h"

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

ao_device *ao_init()
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

bool all_silence(int16_t *buffer, size_t len)
{
  while (len--)
  {
    if ((*buffer < -2) || (*buffer > 2)) { return(false); }
  }
  return(true);
}

void run_spc(ao_device *dev, struct header_t *header)
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
