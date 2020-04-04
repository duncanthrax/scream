#include "raw.h"

static struct raw_output_data {
  receiver_format_t receiver_format;
  int rate;
  int bytes_per_sample;
} ro_data;

int raw_output_init()
{
  // init receiver format to track changes
  ro_data.receiver_format.sample_rate = 0;
  ro_data.receiver_format.sample_size = 0;
  ro_data.receiver_format.channels = 2;
  ro_data.receiver_format.channel_map = 0x0003;
  return 0;
}

int raw_output_send(receiver_data_t *data)
{
  receiver_format_t *rf = &data->format;

  if (memcmp(&ro_data.receiver_format, rf, sizeof(receiver_format_t))) {
    // audio format changed, reconfigure
    memcpy(&ro_data.receiver_format, rf, sizeof(receiver_format_t));

    ro_data.rate = ((rf->sample_rate >= 128) ? 44100 : 48000) * (rf->sample_rate % 128);
    switch (rf->sample_size) {
      case 16:
      case 24:
      case 32:
        break;
      default:
        if (verbosity > 0) {
          fprintf(stderr, "Unsupported sample size %hhu, not playing until next format switch.\n", rf->sample_size);
        }
        ro_data.rate = 0;
    }

    if (rf->channels > 2) {
      int k = -1;
      for (int i=0; i<rf->channels; i++) {
        for (int j = k+1; j<=10; j++) {// check the channel map bit by bit from lsb to msb, starting from were we left on the previous step
          if ((rf->channel_map >> j) & 0x01) {// if the bit in j position is set then we have the key for this channel
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

  if (!ro_data.rate) return 0;

  fwrite(data->audio, 1, data->audio_size, stdout);

  return 0;
}
