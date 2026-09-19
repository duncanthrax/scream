/* Exercise the real output code without connecting to an audio server. */
#include "pulseaudio.h"

int verbosity = 0;
static pa_sample_spec opened_spec;
static pa_buffer_attr opened_attr;
static unsigned int opens;
static int fake_stream;

#define CHECK(expr) do { \
  if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
    exit(1); \
  } \
} while (0)

pa_simple *pa_simple_new(const char *server, const char *name,
                        pa_stream_direction_t direction, const char *device,
                        const char *stream_name, const pa_sample_spec *spec,
                        const pa_channel_map *map, const pa_buffer_attr *attr,
                        int *error)
{
  (void)server; (void)name; (void)direction; (void)device;
  (void)stream_name; (void)error;
  CHECK(pa_sample_spec_valid(spec));
  CHECK(pa_channel_map_compatible(map, spec));
  opened_spec = *spec;
  opened_attr = *attr;
  ++opens;
  return (pa_simple *)&fake_stream;
}

void pa_simple_free(pa_simple *stream) { (void)stream; }

int pa_simple_write(pa_simple *stream, const void *data, size_t bytes, int *error)
{
  (void)stream; (void)data; (void)bytes; (void)error;
  return 0;
}

static void check_attributes(unsigned int target_ms, unsigned int max_ms)
{
  CHECK(opened_attr.tlength == pa_usec_to_bytes(target_ms * 1000u, &opened_spec));
  CHECK(opened_attr.maxlength == pa_usec_to_bytes(max_ms * 1000u, &opened_spec));
  CHECK(opened_attr.tlength <= opened_attr.maxlength);
  CHECK(opened_attr.prebuf == (uint32_t)-1);
  CHECK(opened_attr.minreq == (uint32_t)-1);
}

int main(void)
{
  static const receiver_format_t formats[] = {
    {1, 32, 2, 3},       /* 48 kHz stereo: original regression */
    {1, 24, 2, 3},
    {130, 16, 1, 4},     /* 88.2 kHz mono */
    {2, 24, 6, 0x3f},    /* 96 kHz 5.1 */
    {4, 32, 8, 0x63f},   /* 192 kHz 7.1 */
    {129, 16, 2, 3}      /* switch back down */
  };
  unsigned char pcm[1152] = {0};
  for (unsigned int max_ms = 100; max_ms <= 200; max_ms += 100) {
    CHECK(pulse_output_init(50, max_ms, NULL, "test") == 0);
    check_attributes(50, max_ms);
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
      receiver_data_t data = {0};
      data.format = formats[i];
      data.audio = pcm;
      data.audio_size = sizeof(pcm);
      unsigned int before = opens;
      CHECK(pulse_output_send(&data) == 0);
      CHECK(opens == before + 1);
      check_attributes(50, max_ms);
      CHECK(pulse_output_send(&data) == 0);
      CHECK(opens == before + 1);
    }
  }
  return 0;
}
