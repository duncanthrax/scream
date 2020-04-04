#include "alsa.h"

#define SNDCHK(call, ret) { \
  if (ret < 0) {            \
    alsa_error(call, ret);  \
    return -1;              \
  }                         \
}

static struct alsa_output_data {
  snd_pcm_chmap_t *channel_map;
  snd_pcm_t *snd;

  receiver_format_t receiver_format;
  unsigned int rate;
  unsigned int bytes_per_sample;

  int latency;
  char *alsa_device;
} ao_data;

void alsa_error(const char *msg, int r)
{
  fprintf(stderr, "%s: %s\n", msg, snd_strerror(r));
}

int dump_alsa_info(snd_pcm_t *pcm)
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

int setup_alsa(snd_pcm_t **psnd, snd_pcm_format_t format, unsigned int rate, unsigned int target_latency_ms, const char *output_device, int channels, snd_pcm_chmap_t **channel_map)
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

int alsa_output_init(int latency, char *alsa_device)
{
  // init receiver format to track changes
  ao_data.receiver_format.sample_rate = 0;
  ao_data.receiver_format.sample_size = 0;
  ao_data.receiver_format.channels = 2;
  ao_data.receiver_format.channel_map = 0x0003;

  ao_data.latency = latency;
  ao_data.alsa_device = alsa_device;

  ao_data.channel_map = malloc(sizeof(snd_pcm_chmap_t) + MAX_CHANNELS*sizeof(unsigned int));
  ao_data.channel_map->channels = 2;
  ao_data.channel_map->pos[0] = SND_CHMAP_FL;
  ao_data.channel_map->pos[1] = SND_CHMAP_FR;

  // Start with base default format, rate and channels. Will switch to actual format later
  if (setup_alsa(&ao_data.snd, SND_PCM_FORMAT_S16_LE, 44100, latency, alsa_device, ao_data.receiver_format.channels, &ao_data.channel_map) == -1) {
    return 1;
  }

  return 0;
}

int alsa_output_send(receiver_data_t *data)
{
  snd_pcm_format_t format;

  receiver_format_t *rf = &data->format;

  if (memcmp(&ao_data.receiver_format, rf, sizeof(receiver_format_t))) {
    // audio format changed, reconfigure
    memcpy(&ao_data.receiver_format, rf, sizeof(receiver_format_t));

    ao_data.rate = ((rf->sample_rate >= 128) ? 44100 : 48000) * (rf->sample_rate % 128);
    switch (rf->sample_size) {
      case 16: format = SND_PCM_FORMAT_S16_LE; ao_data.bytes_per_sample = 2; break;
      case 24: format = SND_PCM_FORMAT_S24_3LE; ao_data.bytes_per_sample = 3; break;
      case 32: format = SND_PCM_FORMAT_S32_LE; ao_data.bytes_per_sample = 4; break;
      default:
        if (verbosity > 0)
          fprintf(stderr, "Unsupported sample size %hhu, not playing until next format switch.\n", rf->sample_size);
        ao_data.rate = 0;
    }

    ao_data.channel_map->channels = rf->channels;
    if (rf->channels == 1) {
      ao_data.channel_map->pos[0] = SND_CHMAP_MONO;
    }
    else {
      // k is the key to map a windows SPEAKER_* position to a PA_CHANNEL_POSITION_*
      // it goes from 0 (SPEAKER_FRONT_LEFT) up to 10 (SPEAKER_SIDE_RIGHT) following the order in ksmedia.h
      // the SPEAKER_TOP_* values are not used
      int k = -1;
      for (int i=0; i<rf->channels; i++) {
        for (int j = k+1; j<=10; j++) {// check the channel map bit by bit from lsb to msb, starting from were we left on the previous step
          if ((rf->channel_map >> j) & 0x01) {// if the bit in j position is set then we have the key for this channel
            k = j;
            break;
          }
        }
        // map the key value to a ALSA channel position
        switch (k) {
          case  0: ao_data.channel_map->pos[i] = SND_CHMAP_FL; break;
          case  1: ao_data.channel_map->pos[i] = SND_CHMAP_FR; break;
          case  2: ao_data.channel_map->pos[i] = SND_CHMAP_FC; break;
          case  3: ao_data.channel_map->pos[i] = SND_CHMAP_LFE; break;
          case  4: ao_data.channel_map->pos[i] = SND_CHMAP_RL; break;
          case  5: ao_data.channel_map->pos[i] = SND_CHMAP_RR; break;
          case  6: ao_data.channel_map->pos[i] = SND_CHMAP_FLC; break;
          case  7: ao_data.channel_map->pos[i] = SND_CHMAP_FRC; break;
          case  8: ao_data.channel_map->pos[i] = SND_CHMAP_RC; break;
          case  9: ao_data.channel_map->pos[i] = SND_CHMAP_SL; break;
          case 10: ao_data.channel_map->pos[i] = SND_CHMAP_SR; break;
          default:
            // center is a safe default, at least it's balanced. This shouldn't happen, but it's better to have a fallback
            if (verbosity) {
              fprintf(stderr, "Channel %i could not be mapped. Falling back to 'center'.\n", i);
            }
            ao_data.channel_map->pos[i] = SND_CHMAP_FC;
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
          fprintf(stderr, "Channel %i mapped to %s\n", i, channel_name);
        }
      }
    }

    if (ao_data.rate) {
      close_alsa(ao_data.snd);
      if (setup_alsa(&ao_data.snd, format, ao_data.rate, ao_data.latency, ao_data.alsa_device, rf->channels, &ao_data.channel_map) == -1) {
        if (verbosity > 0)
          fprintf(stderr, "Unable to set up ALSA with sample rate %u, sample size %hhu and %u channels, not playing until next format switch.\n", ao_data.rate, rf->sample_size, rf->channels);
        ao_data.snd = NULL;
        ao_data.rate = 0;
      }
      else {
        if (verbosity > 0)
          fprintf(stderr, "Switched format to sample rate %u, sample size %hhu and %u channels.\n", ao_data.rate, rf->sample_size, rf->channels);
      }
    }

  }

  if (!ao_data.rate) return 0;

  int ret;
  snd_pcm_sframes_t written;

  int i = 0;
  int samples = (data->audio_size) / (ao_data.bytes_per_sample * rf->channels);
  while (i < samples) {
    written = snd_pcm_writei(ao_data.snd, &data->audio[i * ao_data.bytes_per_sample * rf->channels], samples - i);
    if (written < 0) {
      ret = snd_pcm_recover(ao_data.snd, written, 0);
      SNDCHK("snd_pcm_recover", ret);
      return 0;
    } else if (written < samples - i) {
      if (verbosity) fprintf(stderr, "Writing again after short write %ld < %d\n", written, samples - i);
    }
    i += written;
  }

  return 0;
}
