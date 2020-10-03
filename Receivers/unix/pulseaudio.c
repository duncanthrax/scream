#include "pulseaudio.h"

static struct pulse_output_data {
  pa_simple *s;
  pa_sample_spec ss;
  pa_channel_map channel_map;
  pa_buffer_attr buffer_attr;

  receiver_format_t receiver_format;
  int latency;
  char *sink;
  char *stream_name;
} po_data;

int pulse_output_init(int latency, char *sink, char *stream_name)
{
  int error;

  // set application icon
  setenv("PULSE_PROP_application.icon_name", "audio-card", 0);

  // map to stereo, it's the default number of channels
  pa_channel_map_init_stereo(&po_data.channel_map);

  // Start with base default format, rate and channels. Will switch to actual format later
  po_data.ss.format = PA_SAMPLE_S16LE;
  po_data.ss.rate = 44100;
  po_data.ss.channels = 2;

  // init receiver format to track changes
  po_data.receiver_format.sample_rate = 0;
  po_data.receiver_format.sample_size = 0;
  po_data.receiver_format.channels = 2;
  po_data.receiver_format.channel_map = 0x0003;

  po_data.latency = latency;
  po_data.sink = sink;
  po_data.stream_name = stream_name;

  // set buffer size for requested latency
  po_data.buffer_attr.maxlength = (uint32_t)-1;
  po_data.buffer_attr.tlength = pa_usec_to_bytes((pa_usec_t)po_data.latency * 1000u, &po_data.ss);
  po_data.buffer_attr.prebuf = (uint32_t)-1;
  po_data.buffer_attr.minreq = (uint32_t)-1;
  po_data.buffer_attr.fragsize = (uint32_t)-1;

  po_data.s = pa_simple_new(NULL,
    "Scream",
    PA_STREAM_PLAYBACK,
    po_data.sink,
    po_data.stream_name,
    &po_data.ss,
    &po_data.channel_map,
    &po_data.buffer_attr,
    &error
  );
  if (!po_data.s) {
    fprintf(stderr, "Unable to connect to PulseAudio. %s\n", pa_strerror(error));
    return 1;
  }

  return 0;
}

void pulse_output_destroy()
{
  if (po_data.s)
    pa_simple_free(po_data.s);
}

int pulse_output_send(receiver_data_t *data)
{
  int error;

  receiver_format_t *rf = &data->format;

  if (memcmp(&po_data.receiver_format, rf, sizeof(receiver_format_t))) {
    // audio format changed, reconfigure
    memcpy(&po_data.receiver_format, rf, sizeof(receiver_format_t));

    po_data.ss.channels = rf->channels;
    po_data.ss.rate = ((rf->sample_rate >= 128) ? 44100 : 48000) * (rf->sample_rate % 128);
    switch (rf->sample_size) {
      case 16: po_data.ss.format = PA_SAMPLE_S16LE; break;
      case 24: po_data.ss.format = PA_SAMPLE_S24LE; break;
      case 32: po_data.ss.format = PA_SAMPLE_S32LE; break;
      default:
        printf("Unsupported sample size %hhu, not playing until next format switch.\n", rf->sample_size);
        po_data.ss.rate = 0;
    }

    if (rf->channels == 1) {
      pa_channel_map_init_mono(&po_data.channel_map);
    }
    else if (rf->channels == 2) {
      pa_channel_map_init_stereo(&po_data.channel_map);
    }
    else {
      pa_channel_map_init(&po_data.channel_map);
      po_data.channel_map.channels = rf->channels;
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
        // map the key value to a pulseaudio channel position
        switch (k) {
          case  0: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_LEFT; break;
          case  1: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_RIGHT; break;
          case  2: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_CENTER; break;
          case  3: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_LFE; break;
          case  4: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_REAR_LEFT; break;
          case  5: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_REAR_RIGHT; break;
          case  6: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER; break;
          case  7: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER; break;
          case  8: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_REAR_CENTER; break;
          case  9: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_SIDE_LEFT; break;
          case 10: po_data.channel_map.map[i] = PA_CHANNEL_POSITION_SIDE_RIGHT; break;
          default:
            // center is a safe default, at least it's balanced. This shouldn't happen, but it's better to have a fallback
            printf("Channel %i could not be mapped. Falling back to 'center'.\n", i);
            po_data.channel_map.map[i] = PA_CHANNEL_POSITION_CENTER;
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
            channel_name = "Unknown. Set to Center.";
        }
        printf("Channel %i mapped to %s\n", i, channel_name);
      }
    }
    // this is for extra safety
    if (!pa_channel_map_valid(&po_data.channel_map)) {
      printf("Invalid channel mapping, falling back to CHANNEL_MAP_WAVEEX.\n");
      pa_channel_map_init_extend(&po_data.channel_map, rf->channels, PA_CHANNEL_MAP_WAVEEX);
    }
    if (!pa_channel_map_compatible(&po_data.channel_map, &po_data.ss)){
      printf("Incompatible channel mapping.\n");
      po_data.ss.rate = 0;
    }

    if (po_data.ss.rate > 0) {
      // sample spec has changed, so the playback buffer size for the requested latency must be recalculated as well
      po_data.buffer_attr.tlength = pa_usec_to_bytes((pa_usec_t)po_data.latency * 1000, &po_data.ss);

      if (po_data.s) pa_simple_free(po_data.s);
      po_data.s = pa_simple_new(NULL,
        "Scream",
        PA_STREAM_PLAYBACK,
        po_data.sink,
        po_data.stream_name,
        &po_data.ss,
        &po_data.channel_map,
        &po_data.buffer_attr,
        NULL
      );
      if (po_data.s) {
        printf("Switched format to sample rate %u, sample size %hhu and %u channels.\n", po_data.ss.rate, rf->sample_size, rf->channels);
      }
      else {
        printf("Unable to open PulseAudio with sample rate %u, sample size %hhu and %u channels, not playing until next format switch.\n", po_data.ss.rate, rf->sample_size, rf->channels);
        po_data.ss.rate = 0;
      }
    }
  }

  if (!po_data.ss.rate) return 0;
  if (pa_simple_write(po_data.s, data->audio, data->audio_size, &error) < 0) {
    fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
    pulse_output_destroy();
    return 1;
  }
  return 0;
}
