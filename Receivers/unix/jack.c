//
// A Scream receiver JACK client
//
// (c) 2020 Oleksandr Dunayevskyy <oleksandr.dunayevskyy@gmail.com>
//

#include <jack/jack.h>
#include <soxr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jack.h"



typedef jack_default_audio_sample_t ringbuffer_element_t;
#define RINGBUFFER_SIZE 8192
struct ringbuffer_t
{
  ringbuffer_element_t elements[RINGBUFFER_SIZE];
  uint32_t read_pos;
  uint32_t write_pos;
};


uint32_t ringbuffer_mask(uint32_t val)
{
  return val & (RINGBUFFER_SIZE - 1u);
}

void ringbuffer_init(struct ringbuffer_t *rb)
{
  rb->read_pos = 0u;
  rb->write_pos = 0u;
}

uint32_t ringbuffer_size(struct ringbuffer_t *rb)
{
  return rb->write_pos - rb->read_pos;
}

int ringbuffer_empty(struct ringbuffer_t *rb)
{
  return ringbuffer_size(rb) == 0u;
}

int ringbuffer_full(struct ringbuffer_t *rb)
{
  return ringbuffer_size(rb) == RINGBUFFER_SIZE;
}

void ringbuffer_push(struct ringbuffer_t *rb, ringbuffer_element_t val)
{
  if (!ringbuffer_full(rb))
  {
    rb->elements[ringbuffer_mask(rb->write_pos++)] = val;
  }
  else
  {
    fprintf(stderr, "ringbuffer overflow\n");
  }
  
}

ringbuffer_element_t ringbuffer_pop(struct ringbuffer_t *rb)
{
  if (!ringbuffer_empty(rb))
  {
    return rb->elements[ringbuffer_mask(rb->read_pos++)];
  }
  else
  {
    fprintf(stderr, "ringbuffer underflow\n");
    return (ringbuffer_element_t)0;
  }
  
}


static struct jack_output_data
{
  jack_client_t       *client;
  jack_port_t         **output_ports;   ///< number of ports actually used == rf->channels
  jack_default_audio_sample_t **buffers;
  receiver_format_t   receiver_format;
  uint32_t            sample_rate;      ///< source sample rate
  soxr_t              soxr;
  jack_default_audio_sample_t *resample_buffer;
  size_t              resample_buffer_size;
  struct ringbuffer_t rb;
}
jo_data;



static int init_resampler();
static int init_channels();
static int connect_ports();
static int process_source_data();

// JACK realtime process callback
int jack_process(jack_nframes_t nframes, void *arg);



int jack_output_init(char *stream_name)
{
  jack_status_t status;

  // init receiver format to track changes
  jo_data.receiver_format.sample_rate = 0;
  jo_data.receiver_format.sample_size = 0;
  jo_data.receiver_format.channels = 2;
  jo_data.receiver_format.channel_map = 0x0003;

  jo_data.soxr = NULL;
  jo_data.resample_buffer = NULL;
  jo_data.resample_buffer_size = 0;

  ringbuffer_init(&jo_data.rb);


  jo_data.client = jack_client_open(stream_name, JackNullOption, &status, NULL);
  if (jo_data.client == NULL)
  {
    fprintf(stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) 
    {
      fprintf(stderr, "Unable to connect to JACK server\n");
    }
    return 1;
  }
    
  if (status & JackServerStarted)
  {
    fprintf(stderr, "JACK server started\n");
  }
  
  if (status & JackNameNotUnique)
  {
    const char *client_name = jack_get_client_name(jo_data.client);
    fprintf(stderr, "unique name `%s' assigned\n", client_name);
  }

  jack_set_process_callback(jo_data.client, jack_process, 0);

  return 0;
}




int jack_output_send(receiver_data_t *data)
{
  receiver_format_t *rf = &data->format;

  if (memcmp(&jo_data.receiver_format, rf, sizeof(receiver_format_t)))
  {
    // audio format changed, reconfigure
    memcpy(&jo_data.receiver_format, rf, sizeof(receiver_format_t));

    jo_data.sample_rate = ((rf->sample_rate >= 128) ? 44100 : 48000) * (rf->sample_rate % 128);

    printf(
      "Switched sample rate %"PRIu32", sample size %u and %u channels\n",
       jo_data.sample_rate,
       jo_data.receiver_format.sample_size,
       rf->channels);

    printf("JACK sample rate %" PRIu32 "\n", jack_get_sample_rate(jo_data.client));


    if (init_resampler())
    {
      return 1;
    }



    if (jack_deactivate(jo_data.client))
    {
      fprintf(stderr, "cannot deactivate client");
      return 1;
    }

    if (init_channels())
    {
      return 1;
    }
    
    // activating JJACK client - jack_process() callback will start running now
    if (jack_activate(jo_data.client))
    {
      fprintf(stderr, "cannot activate client");
      return 1;
    }

    if (connect_ports())
    {
      return 1;
    }
  }


  if (process_source_data(data))
  {
    return 1;
  }
  
  return 0;
}




static int init_resampler()
{
  soxr_io_spec_t io_spec;
  soxr_datatype_t in_datatype;
  switch(jo_data.receiver_format.sample_size)
  {
    case 16: in_datatype = SOXR_INT16_I; break;
    case 32: in_datatype = SOXR_INT32_I; break;
    default:
      return 1;
  }

  io_spec = soxr_io_spec(in_datatype, SOXR_FLOAT32_I);
  if (jo_data.soxr)
  {
    soxr_delete(jo_data.soxr);
  }
  jo_data.soxr = soxr_create(
    jo_data.sample_rate,
    jack_get_sample_rate(jo_data.client),
    jo_data.receiver_format.channels,
    NULL, &io_spec, NULL, NULL );
  if (!jo_data.soxr )
  {
    fprintf(stderr, "failed to initialize resampler");
    return 1;
  }
  return 0;
}


static const char *channel_index_to_name(uint8_t channel)
{
  const char *channel_name;
  switch (channel) {
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
    default: channel_name = "unknown_channel";
  }

  return channel_name;
}


static int init_channels()
{
  free(jo_data.output_ports);
  jo_data.output_ports = malloc(sizeof(jack_port_t*) * jo_data.receiver_format.channels);
  free(jo_data.buffers);
  jo_data.buffers = malloc(sizeof(jack_default_audio_sample_t*) * jo_data.receiver_format.channels);
  for (int i = 0 ; i < jo_data.receiver_format.channels; ++i)
  {
    if ( (jo_data.receiver_format.channel_map  >> i) & 1 )
    {
      const char *port_name = channel_index_to_name(i);
      jo_data.output_ports[i] = jack_port_register(jo_data.client,
              port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
      printf("registered jack port '%s' for channel %u\n",  port_name, i);
      
      if (jo_data.output_ports[i] == NULL)
      {
        fprintf(stderr, "no more JACK ports available\n");
        return 1;
      }
    }
  }
  return 0;
}


static int connect_ports()
{
  int num_output_ports = 0;
  
  const char **ports = jack_get_ports(jo_data.client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
  if (ports == NULL)
  {
    fprintf(stderr, "no physical playback ports\n");
    return 1;
  }

  // connect the receiver if there is enough output ports
  for ( ; ports[num_output_ports] && (!strstr(ports[num_output_ports],"midi")); ++num_output_ports)
  {
    ;
  }


  if (num_output_ports >= jo_data.receiver_format.channels)
  {
    for (int port = 0; port < jo_data.receiver_format.channels && ports[port] ; ++port)
    {
      if (jack_connect(jo_data.client, jack_port_name(jo_data.output_ports[port]), ports[port]))
      {
        fprintf(stderr, "cannot connect to output port %s\n", ports[port]);
      }
      else
      {
        printf("channel %u connected to output port %s\n", port, ports[port]);
      }
      
    }
  }

  jack_free(ports);
}


static int process_source_data(receiver_data_t *data)
{
  // per channel!
  size_t src_nsamples;
  size_t dst_nsamples;
  size_t idone;
  size_t odone;

  size_t resample_ideal_buf_size;
  soxr_error_t soxr_error;

  // (re)allocate a resampling buffer if needed
  src_nsamples = data->audio_size / (jo_data.receiver_format.sample_size >> 3) / jo_data.receiver_format.channels;
  dst_nsamples = src_nsamples * jack_get_sample_rate(jo_data.client) / jo_data.sample_rate + 1;
  resample_ideal_buf_size = dst_nsamples*jo_data.receiver_format.channels;
  if (jo_data.resample_buffer_size < resample_ideal_buf_size)
  {
    free(jo_data.resample_buffer);
    jo_data.resample_buffer = malloc(resample_ideal_buf_size* sizeof(jack_default_audio_sample_t));
    jo_data.resample_buffer_size = resample_ideal_buf_size;
  }

  soxr_error = soxr_process(jo_data.soxr, data->audio, src_nsamples, &idone, jo_data.resample_buffer, dst_nsamples, &odone);
  if (soxr_error != NULL)
  {
    fprintf(stderr,"soxr error : %s\n", soxr_strerror(soxr_error));
    return 1;
  }

  for (int i = 0; i < odone*jo_data.receiver_format.channels; ++i)
  {
    ringbuffer_push(&jo_data.rb, jo_data.resample_buffer[i]);
  }

  return 0;
}





int jack_process(jack_nframes_t nframes, void *arg)
{
  uint8_t channels = jo_data.receiver_format.channels;
  uint32_t total_nframes = nframes * channels;
  uint32_t read_frames = ringbuffer_size(&jo_data.rb);

  // wait until rb fills up a bit to prevent early underruns
  if (read_frames < (total_nframes + 200))
  {
    return 0;
  }

  if (total_nframes <= read_frames)
  {
    read_frames = total_nframes;
  }
  else
  {
    // JACK wants more than we can fullfill
    fprintf(stderr, "jack_process: underflow!! read_frames=%u  nframes=%u\n", read_frames, total_nframes);
  }  
  

  if (read_frames > 0)
  {
    for (int port = 0; port < channels; ++port)
    {
      jo_data.buffers[port] = jack_port_get_buffer(jo_data.output_ports[port], nframes);
    }

    // transfer samples from ringbuffer to JACK port buffers
    for (uint32_t sample = 0; sample < read_frames /channels; ++sample)
    {
      for (int port = 0; port < channels; ++port)
      {
        *jo_data.buffers[port] = ringbuffer_pop(&jo_data.rb);
        jo_data.buffers[port]++;
        
      }
    }
  }

  return 0;
}

