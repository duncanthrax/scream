/* Exercise the production backend with real PulseAudio format helpers and
 * deterministic simple-API substitutes; no server or audio device required. */
#include <assert.h>
#include <limits.h>
#include "pulseaudio.h"

int verbosity = 1;
struct pa_simple { int active; };
static struct pa_simple stream;
static pa_sample_spec requested_spec;
static pa_buffer_attr requested_attr;
static int opens, writes, frees, fail_open, fail_write;

pa_simple *pa_simple_new(const char *server, const char *name,
  pa_stream_direction_t direction, const char *device, const char *stream_name,
  const pa_sample_spec *spec, const pa_channel_map *map,
  const pa_buffer_attr *attr, int *error)
{
  (void)server; (void)name; (void)device; (void)stream_name;
  assert(direction == PA_STREAM_PLAYBACK);
  assert(pa_sample_spec_valid(spec));
  assert(pa_channel_map_valid(map));
  assert(pa_channel_map_compatible(map, spec));
  assert(error);
  opens++;
  requested_spec = *spec;
  requested_attr = *attr;
  if (fail_open) { *error = PA_ERR_CONNECTIONREFUSED; return NULL; }
  assert(!stream.active);
  stream.active = 1;
  return &stream;
}

void pa_simple_free(pa_simple *s)
{
  assert(s == &stream && stream.active);
  stream.active = 0;
  frees++;
}

int pa_simple_write(pa_simple *s, const void *data, size_t bytes, int *error)
{
  assert(s == &stream && stream.active);
  assert(data && bytes && error);
  writes++;
  if (fail_write) { *error = PA_ERR_IO; return -1; }
  return 0;
}

static void check_buffers(unsigned rate, unsigned bits, unsigned channels)
{
  assert(requested_spec.rate == rate);
  assert(requested_spec.channels == channels);
  assert(pa_sample_size(&requested_spec) == bits / 8);
  assert(requested_attr.tlength == rate * (bits / 8) * channels * 20 / 1000);
  assert(requested_attr.maxlength == rate * (bits / 8) * channels * 100 / 1000);
  assert(requested_attr.prebuf == UINT32_MAX);
  assert(requested_attr.minreq == UINT32_MAX);
  assert(requested_attr.fragsize == UINT32_MAX);
}

int main(void)
{
  unsigned char audio[24] = {0};
  receiver_data_t data = {{0}, sizeof(audio), audio};
  assert(pulse_output_init(20, 100, NULL, "test") == 0);
  check_buffers(44100, 16, 2);
  for (unsigned rate = 0; rate < 2; rate++) {
    for (unsigned bits = 16; bits <= 32; bits += 8) {
      for (unsigned channels = 1; channels <= 6; channels = channels == 1 ? 2 : channels + 4) {
        data.format = (receiver_format_t){rate ? 1 : 129, bits, channels,
          channels == 1 ? 4 : channels == 2 ? 3 : 0x3f};
        assert(pulse_output_send(&data) == 0);
        check_buffers(rate ? 48000 : 44100, bits, channels);
        int before = opens;
        assert(pulse_output_send(&data) == 0);
        assert(opens == before);
      }
    }
  }
  int before = writes;
  const receiver_format_t invalid[] = {
    {0, 16, 2, 3}, {128, 16, 2, 3}, {127, 16, 2, 3},
    {1, 8, 2, 3}, {1, 16, 0, 0}, {1, 16, 255, 0xffff}
  };
  for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
    data.format = invalid[i];
    assert(pulse_output_send(&data) == 0);
    assert(pulse_output_send(&data) == 0);
  }
  assert(writes == before);
  fail_open = 1;
  data.format = (receiver_format_t){1, 24, 2, 3};
  assert(pulse_output_send(&data) == 0);
  assert(pulse_output_send(&data) == 0);
  assert(writes == before);
  fail_open = 0;
  data.format.sample_size = 32;
  assert(pulse_output_send(&data) == 0);
  check_buffers(48000, 32, 2);
  fail_write = 1;
  assert(pulse_output_send(&data) == 1);
  pulse_output_destroy(); /* No double free after a failed write. */
  fail_write = 0;
  fail_open = 1;
  assert(pulse_output_init(20, 100, NULL, "test") == 1);
  pulse_output_destroy();
  fail_open = 0;
  assert(pulse_output_init(-1, 100, NULL, "test") == 1);
  assert(pulse_output_init(20, -1, NULL, "test") == 1);
  assert(pulse_output_init(INT_MAX, INT_MAX, NULL, "test") == 1);
  /* A limit can fit at initialization but overflow with the real format. */
  assert(pulse_output_init(20, 20000000, NULL, "test") == 0);
  data.format = (receiver_format_t){1, 32, 6, 0x3f};
  assert(pulse_output_send(&data) == 1);
  pulse_output_destroy();
  assert(opens > 18 && frees > 18);
  puts("PulseAudio buffer and error-path tests passed.");
  return 0;
}
