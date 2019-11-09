// Scream-IVSHMEM receiver for ALSA.
// Author: Marco Martinelli - https://github.com/martinellimarco

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <sys/time.h>
#include <sys/resource.h>

#define MAX_CHANNELS 8

#define SNDCHK(call, ret) { \
  if (ret < 0) {            \
    alsa_error(call, ret);  \
    return -1;              \
  }                         \
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

static void alsa_error(const char *msg, int r)
{
  fprintf(stderr, "%s: %s\n", msg, snd_strerror(r));
}

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s <ivshmem device path> [-v] [-t <target_latency_ms>] [-o <output_sound_device>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         -v          : Verbose operation.\n");
  fprintf(stderr, "         -g <group>  : Multicast group address. Multicast mode only.\n");
  fprintf(stderr, "         -t <latency>: Target latency in milliseconds. Defaults to 50ms.\n");
  fprintf(stderr, "         -o <device>: Set output soundcard. Default is \"default\".\n");
  fprintf(stderr, "\n");
  exit(1);
}

static int dump_alsa_info(snd_pcm_t *pcm)
{
  int ret;
  snd_output_t *log;

  ret = snd_output_stdio_attach(&log, stderr, 0);
  SNDCHK("snd_output_stdio_attach", ret);

  ret = snd_pcm_dump(pcm, log);
  SNDCHK("snd_pcm_dump", ret);

  ret = snd_output_close(log);
  SNDCHK("snd_output_close", ret);

  return 0;
}

static int setup_alsa(snd_pcm_t **psnd, snd_pcm_format_t format, unsigned int rate, int verbosity, unsigned int target_latency_ms, const char *output_device, int channels, snd_pcm_chmap_t **channel_map)
{
  int ret;
  int soft_resample = 1;
  unsigned int latency = target_latency_ms * 1000;

  ret = snd_pcm_open(psnd, output_device, SND_PCM_STREAM_PLAYBACK, 0);
  SNDCHK("snd_pcm_open", ret);

  ret = snd_pcm_set_params(*psnd, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                           channels, rate, soft_resample, latency);
  SNDCHK("snd_pcm_set_params", ret);

  ret = snd_pcm_set_chmap(*psnd, *channel_map);
  if (ret == -ENXIO) { // snd_pcm_set_chmap returns -ENXIO if device does not support channel maps at all
    if (channels > 2) { // but it's relevant only above 2 channels
      fprintf(stderr, "Your device doesn't support channel maps. Channels may be in the wrong order.\n");
      // TODO ALSA has a fixed channel order and we have the source channel_map.
      // It's possible to reorder the channels in software. Maybe a place to start is the remap_data function in aplay.c
    }
  }
  else {
    SNDCHK("snd_pcm_set_chmap", ret);
  }

  if (verbosity > 0) {
    return dump_alsa_info(*psnd);
  }

  return 0;
}

static int close_alsa(snd_pcm_t *snd) {
  int ret;
  if (!snd) return 0;
  ret = snd_pcm_close(snd);
  SNDCHK("snd_pcm_close", ret);
  return 0;
}

static int write_frames(snd_pcm_t *snd, unsigned char *buf, int total_frames, int bytes_per_sample, int channels)
{
  int i = 0;
  int ret;
  snd_pcm_sframes_t written;

  while (i < total_frames) {
    written = snd_pcm_writei(snd, &buf[i * bytes_per_sample * channels], total_frames - i);
    if (written < 0) {
      ret = snd_pcm_recover(snd, written, 0);
      SNDCHK("snd_pcm_recover", ret);
      return 0;
    } else if (written < total_frames - i) {
      fprintf(stderr, "Writing again after short write %ld < %d\n", written, total_frames - i);
    }
    i += written;
  }
  return 0;
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

  snd_pcm_t *snd;
  int opt;
  int verbosity = 0;
  int samples;
  snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
  unsigned int bytes_per_sample = 2;
  unsigned int rate = 44100;
  unsigned char cur_sample_rate = 0;
  unsigned char cur_sample_size = 0;
  unsigned char cur_channels = 2;
  uint16_t cur_channel_map = 0x0003;

  snd_pcm_chmap_t *channel_map;
  channel_map = malloc(sizeof(snd_pcm_chmap_t) + MAX_CHANNELS*sizeof(unsigned int));
  channel_map->channels = 2;
  channel_map->pos[0] = SND_CHMAP_FL;
  channel_map->pos[1] = SND_CHMAP_FR;

  // Command line options
  int target_latency_ms = 50;
  char *output_device ="default";

  while ((opt = getopt(argc, argv, "t:vo:")) != -1) {
    switch (opt) {
    case 't':
      target_latency_ms = atoi(optarg);
      break;
    case 'v':
      verbosity += 1;
      break;
    case 'o':
      output_device = strdup(optarg);
      break;
    default:
      show_usage(argv[0]);
    }
  }

  if (1+optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  // Opportunistic call to renice us, so we can keep up under
  // higher load conditions. This may fail when run as non-root.
  setpriority(PRIO_PROCESS, 0, -11);

  if (setup_alsa(&snd, format, rate, verbosity, target_latency_ms, output_device, cur_channels, &channel_map) == -1) {
    return -1;
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

        rate = ((cur_sample_rate >= 128) ? 44100 : 48000) * (cur_sample_rate % 128);
        switch (cur_sample_size) {
          case 16: format = SND_PCM_FORMAT_S16_LE; bytes_per_sample = 2; break;
          case 24: format = SND_PCM_FORMAT_S24_3LE; bytes_per_sample = 3; break;
          case 32: format = SND_PCM_FORMAT_S32_LE; bytes_per_sample = 4; break;
          default:
            if (verbosity > 0)
              printf("Unsupported sample size %hhu, not playing until next format switch.\n", cur_sample_size);
            rate = 0;
        }

        channel_map->channels = cur_channels;
        if (cur_channels == 1) {
          channel_map->pos[0] = SND_CHMAP_MONO;
        }
        else {
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
              case  0: channel_map->pos[i] = SND_CHMAP_FL; break;
              case  1: channel_map->pos[i] = SND_CHMAP_FR; break;
              case  2: channel_map->pos[i] = SND_CHMAP_FC; break;
              case  3: channel_map->pos[i] = SND_CHMAP_LFE; break;
              case  4: channel_map->pos[i] = SND_CHMAP_RL; break;
              case  5: channel_map->pos[i] = SND_CHMAP_RR; break;
              case  6: channel_map->pos[i] = SND_CHMAP_FLC; break;
              case  7: channel_map->pos[i] = SND_CHMAP_FRC; break;
              case  8: channel_map->pos[i] = SND_CHMAP_RC; break;
              case  9: channel_map->pos[i] = SND_CHMAP_SL; break;
              case 10: channel_map->pos[i] = SND_CHMAP_SR; break;
              default:
                // center is a safe default, at least it's balanced. This shouldn't happen, but it's better to have a fallback
                if (verbosity > 0) {
                  printf("Channel %i coult not be mapped. Falling back to 'center'.\n", i);
                }
                channel_map->pos[i] = SND_CHMAP_FC;
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
              printf("Channel %i mapped to %s\n", i, channel_name);
            }
          }
        }

        if (rate) {
          close_alsa(snd);
          if (setup_alsa(&snd, format, rate, verbosity, target_latency_ms, output_device, cur_channels, &channel_map) == -1) {
            if (verbosity > 0)
              printf("Unable to set up ALSA with sample rate %u, sample size %hhu and %u channels, not playing until next format switch.\n", rate, cur_sample_size, cur_channels);
            snd = NULL;
            rate = 0;
          }
          else {
            if (verbosity > 0)
              printf("Switched format to sample rate %u, sample size %hhu and %u channels.\n", rate, cur_sample_size, cur_channels);
          }
        }
    }
    if (!rate) continue;

    samples = (header->chunk_size) / (bytes_per_sample * cur_channels);
    write_frames(snd, buf, samples, bytes_per_sample, cur_channels);
  }
}
