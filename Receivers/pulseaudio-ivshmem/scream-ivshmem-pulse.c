// Scream-IVSHMEM receiver for Pulseaudio.
// Author: Marco Martinelli - https://github.com/martinellimarco

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s <ivshmem device path>\n", arg0);
  fprintf(stderr, "\n");
  exit(1);
}

static void * open_mmap(const char *shmfile) {
  struct stat st;
  if (stat(shmfile, &st) < 0)  {
    fprintf(stderr, "Failed to stat the shared memory file: %s", shmfile);
    exit(2);
  }

  int shmFD = open(shmfile, O_RDWR, (mode_t)0600);
  if (shmFD < 0) {
    fprintf(stderr, "Failed to open the shared memory file: %s", shmfile);
    exit(3);
  }

  void * map = mmap(0, st.st_size, PROT_READ, MAP_SHARED, shmFD, 0);
  if (map == MAP_FAILED) {
    fprintf(stderr, "Failed to map the shared memory file: %s", shmfile);
    close(shmFD);
    exit(4);
  }

  return map;
}

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

int main(int argc, char*argv[]) {
  if (argc != 2) {
    show_usage(argv[0]);
  }
  
  unsigned char * mmap = open_mmap(argv[1]);
  
  int error;

  pa_simple *s;
  pa_sample_spec ss;
  pa_channel_map channel_map;

  unsigned char cur_sample_rate = 0;
  unsigned char cur_sample_size = 0;
  unsigned char cur_channels = 2;
  uint16_t cur_channel_map = 0x0003;

  // map to stereo, it's the default number of channels
  pa_channel_map_init_stereo(&channel_map);

  // Start with base default format, will switch to actual format later
  ss.format = PA_SAMPLE_S16LE;
  ss.rate = 44100;
  ss.channels = cur_channels;
  s = pa_simple_new(NULL,
    "Scream",
    PA_STREAM_PLAYBACK,
    NULL,
    "Audio",
    &ss,
    &channel_map,
    NULL,
    NULL
  );
  if (!s) {
    printf("Unable to connect to PulseAudio.\n");
    goto BAIL;
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

    if ( cur_sample_rate != header->sample_rate
      || cur_sample_size != header->sample_size
      || cur_channels != header->channels
      || cur_channel_map != header->channel_map) {
      
      cur_sample_rate = header->sample_rate;
      cur_sample_size = header->sample_size;
      cur_channels = header->channels;
      cur_channel_map = header->channel_map;

      ss.channels = cur_channels;

      ss.rate = ((cur_sample_rate >= 128) ? 44100 : 48000) * (cur_sample_rate % 128);
      switch (cur_sample_size) {
        case 16: ss.format = PA_SAMPLE_S16LE; break;
        case 24: ss.format = PA_SAMPLE_S24LE; break;
        case 32: ss.format = PA_SAMPLE_S32LE; break;
        default:
          printf("Unsupported sample size %hhu, not playing until next format switch.\n", cur_sample_size);
          ss.rate = 0;
          continue;
      }

      if (cur_channels == 0 || cur_channel_map == 0) {
        continue;
      }
      else if (cur_channels == 1) {
        pa_channel_map_init_mono(&channel_map);
      }
      else if (cur_channels == 2) {
        pa_channel_map_init_stereo(&channel_map);
      }
      else {
        pa_channel_map_init(&channel_map);
        channel_map.channels = cur_channels;
        // k is the key to map a windows SPEAKER_* position to a PA_CHANNEL_POSITION_*
        // it goes from 0 (SPEAKER_FRONT_LEFT) up to 10 (SPEAKER_SIDE_RIGHT) following the order in ksmedia.h
        // the SPEAKER_TOP_* values are not used
        int k = -1;
        for (int i=0; i<cur_channels; i++) {
          for (int j = k+1; j<=10; j++) {// check the channel map bit by bit from lsb to msb, starting from were we left on the previous step
            if ((cur_channel_map >> j) & 0x01) {// if the bit in j position is set then we have the key for this channel
              k = j;
              break;
            }
          }
          // map the key value to a pulseaudio channel position
          switch (k) {
            case  0: channel_map.map[i] = PA_CHANNEL_POSITION_LEFT; break;
            case  1: channel_map.map[i] = PA_CHANNEL_POSITION_RIGHT; break;
            case  2: channel_map.map[i] = PA_CHANNEL_POSITION_CENTER; break;
            case  3: channel_map.map[i] = PA_CHANNEL_POSITION_LFE; break;
            case  4: channel_map.map[i] = PA_CHANNEL_POSITION_REAR_LEFT; break;
            case  5: channel_map.map[i] = PA_CHANNEL_POSITION_REAR_RIGHT; break;
            case  6: channel_map.map[i] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER; break;
            case  7: channel_map.map[i] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER; break;
            case  8: channel_map.map[i] = PA_CHANNEL_POSITION_REAR_CENTER; break;
            case  9: channel_map.map[i] = PA_CHANNEL_POSITION_SIDE_LEFT; break;
            case 10: channel_map.map[i] = PA_CHANNEL_POSITION_SIDE_RIGHT; break;
            default:
              // center is a safe default, at least it's balanced. This shouldn't happen, but it's better to have a fallback
              printf("Channel %i coult not be mapped. Falling back to 'center'.\n", i);
              channel_map.map[i] = PA_CHANNEL_POSITION_CENTER;
          }
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
          printf("Channel %i mapped to %s\n", i, channel_name);
        }
      }
      // this is for extra safety
      if (!pa_channel_map_valid(&channel_map)) {
        printf("Invalid channel mapping, falling back to CHANNEL_MAP_WAVEEX.\n");
        pa_channel_map_init_extend(&channel_map, cur_channels, PA_CHANNEL_MAP_WAVEEX);
      }
      if (!pa_channel_map_compatible(&channel_map, &ss)){
        printf("Incompatible channel mapping.\n");
        ss.rate = 0;
      }

      if (ss.rate > 0) {
        if (s) pa_simple_free(s);
        s = pa_simple_new(NULL,
          "Scream",
          PA_STREAM_PLAYBACK,
          NULL,
          "Audio",
          &ss,
          &channel_map,
          NULL,
          NULL
        );
        if (s) {
          printf("Switched format to sample rate %u, sample size %hhu and %u channels.\n", ss.rate, cur_sample_size, cur_channels);
        }
        else {
          printf("Unable to open PulseAudio with sample rate %u, sample size %hhu and %u channels, not playing until next format switch.\n", ss.rate, cur_sample_size, cur_channels);
          ss.rate = 0;
        }
      }
    }
    if (!ss.rate) continue;
    if (pa_simple_write(s, buf, header->chunk_size, &error) < 0) {
      printf("pa_simple_write() failed: %s\n", pa_strerror(error));
      goto BAIL;
    }
  }

  BAIL:
  if (s) pa_simple_free(s);
  return 0;
};
