#include "sndio.h"

static struct sio_hdl *h;
static receiver_format_t fmt;
static int started;
static unsigned long latency_ms;

int sndio_output_init(unsigned int max_latency_ms, char *dev)
{
  if (dev == NULL)
    dev = SIO_DEVANY;
  if ((h = sio_open(dev, SIO_PLAY, 0)) == NULL) {
    fprintf(stderr, "sio_open failed\n");
    return 1;
  }
  memset(&fmt, 0, sizeof(fmt));
  started = 0;
  latency_ms = max_latency_ms;
  return 0;
}

int sndio_output_send(receiver_data_t *data)
{
  receiver_format_t *rf = &data->format;
  struct sio_par p;

  if (memcmp(rf, &fmt, sizeof(fmt))) {
    if (!rf->sample_rate) return 0;

    // audio format changed, reconfigure
    if (started)
      sio_stop(h);
    sio_initpar(&p);
    p.bits = rf->sample_size;
    p.bps = SIO_BPS(p.bits);
    p.sig = p.bits > 8;
    p.le = 1;
    p.pchan = rf->channels;
    p.rate = ((rf->sample_rate >= 128) ? 44100 : 48000) * (rf->sample_rate % 128);
    p.appbufsz = p.rate * latency_ms / 1000;
    p.xrun = SIO_IGNORE;
    if (!sio_setpar(h, &p)) {
      fprintf(stderr, "sio_setpar failed\n");
      goto end;
    }
    
    memcpy(&fmt, rf, sizeof(fmt));
    sio_start(h);
    started = 1;
  }

  sio_write(h, data->audio, data->audio_size);
  if (!sio_eof(h))
    return 0;
end:
  sio_close(h);
  return 1;
}
