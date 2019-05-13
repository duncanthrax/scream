// Scream-IVSHMEM receiver for ALSA.
// Author: Marco Martinelli - https://github.com/martinellimarco

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

struct shmheader {
  uint32_t magic;
  uint16_t write_idx;
  uint8_t  offset;
  uint16_t max_chunks;
  uint32_t chunk_size;
  uint8_t  sample_rate;
  uint8_t  sample_size;
  uint8_t  channels;
  uint16_t channel_map;
};

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s  <ivshmem device path> [-v]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         -v          : Verbose operation.\n");
  fprintf(stderr, "\n");
  exit(1);
}

static void * open_mmap(const char *shmfile) {
  struct stat st;
  if (stat(shmfile, &st) < 0)  {
    fprintf(stderr, "Failed to stat the shared memory file: %s\n", shmfile);
    exit(2);
  }

  int shmFD = open(shmfile, O_RDONLY);
  if (shmFD < 0) {
    fprintf(stderr, "Failed to open the shared memory file: %s\n", shmfile);
    exit(3);
  }

  void * map = mmap(0, st.st_size, PROT_READ, MAP_SHARED, shmFD, 0);
  if (map == MAP_FAILED) {
    fprintf(stderr, "Failed to map the shared memory file: %s\n", shmfile);
    close(shmFD);
    exit(4);
  }

  return map;
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    show_usage(argv[0]);
  }
  
  unsigned char * mmap = open_mmap(argv[1]);

  int opt;
  int verbosity = 0;
  int samples;

  unsigned int bytes_per_sample = 2;
  unsigned int rate = 44100;
  unsigned char cur_sample_rate = 0;
  unsigned char cur_sample_size = 0;
  unsigned char cur_channels = 2;
  uint16_t cur_channel_map = 0x0003;
  
  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
    case 'v':
      verbosity += 1;
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (1+optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  struct shmheader *header = (struct shmheader*)mmap;
  uint16_t read_idx = header->write_idx;

  for (;;) {
    if (header->magic != 0x11112014) {
      while (header->magic != 0x11112014) {
        usleep(10000);//10ms
      }
      read_idx = header->write_idx;
    }
    if (read_idx == header->write_idx) {
      usleep(10000);//10ms
      continue;
    }
    if (++read_idx == header->max_chunks) {
      read_idx = 0;
    }
    unsigned char *buf = &mmap[header->offset+header->chunk_size*read_idx];
    
    // Change rate/size?
    if ( cur_sample_rate != header->sample_rate
      || cur_sample_size != header->sample_size
      || cur_channels != header->channels
      || cur_channel_map != header->channel_map) {
      
      cur_sample_rate = header->sample_rate;
      cur_sample_size = header->sample_size;
      cur_channels = header->channels;
      cur_channel_map = header->channel_map;

        rate = ((cur_sample_rate >= 128) ? 44100 : 48000) * (cur_sample_rate % 128);
        switch (cur_sample_size) {
          case 16:  bytes_per_sample = 2; break;
          case 24:  bytes_per_sample = 3; break;
          case 32:  bytes_per_sample = 4; break;
          default:
            if (verbosity > 0) {
              fprintf(stderr, "Unsupported sample size %hhu, not playing until next format switch.\n", cur_sample_size);
            }
            rate = 0;
        }

        if (cur_channels > 2) {
          int k = -1;
          for (int i=0; i<cur_channels; i++) {
            for (int j = k+1; j<=10; j++) {// check the channel map bit by bit from lsb to msb, starting from were we left on the previous step
              if ((cur_channel_map >> j) & 0x01) {// if the bit in j position is set then we have the key for this channel
                k = j;
                break;
              }
            }
            if (verbosity > 0) {
              const char *channel_name;
              switch (k) {
                case  0: channel_name = "Front Left"; break;
                case  1: channel_name = "Front Right"; break;
                case  2: channel_name = "Front Center"; break;
                case  3: channel_name = "LFE / Subwoofer"; break;
                case  4: channel_name = "Rear Left"; break;
                case  5: channel_name = "Rear Right"; break;
                case  6: channel_name = "Front-Left Center"; break;
                case  7: channel_name = "Front-Right Center"; break;
                case  8: channel_name = "Rear Center"; break;
                case  9: channel_name = "Side Left"; break;
                case 10: channel_name = "Side Right"; break;
                default:
                  channel_name = "Unknown. Setted to Center.";
              }
              fprintf(stderr, "Channel %i is %s\n", i, channel_name);
            }
          }
        }
    }
    if (!rate) continue;

    samples = (header->chunk_size) / (bytes_per_sample * cur_channels);
    fwrite(buf, bytes_per_sample*cur_channels, samples, stdout);
 
  }
}
