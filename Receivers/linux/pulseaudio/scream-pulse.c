#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define DEFAULT_PORT 4010

#define HEADER_SIZE 5
#define MAX_SO_PACKETSIZE 1152+HEADER_SIZE

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s [-u] [-p <port>] [-i <iface>] [-g <group>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional. Default is to use\n");
  fprintf(stderr, "         multicast with group address 239.255.77.77, port 4010.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -u                        : Use unicast instead of multicast.\n");
  fprintf(stderr, "         -p <port>                 : Use <port> instead of default port 4010.\n");
  fprintf(stderr, "                                     Applies to both multicast and unicast.\n");
  fprintf(stderr, "         -i <iface>                : Use local interface <iface>. Either the IP\n");
  fprintf(stderr, "                                     or the interface name can be specified. In\n");
  fprintf(stderr, "                                     multicast mode, uses this interface for IGMP.\n");
  fprintf(stderr, "                                     In unicast, binds to this interface only.\n");
  fprintf(stderr, "         -g <group>                : Multicast group address. Multicast mode only.\n");
  fprintf(stderr, "         -m <ivshmem device path>  : Use shared memory device.\n");
  fprintf(stderr, "         -t <latency>              : Target latency in milliseconds. Defaults to 50ms.\n");
  fprintf(stderr, "\n");
  exit(1);
}

enum receiver_type {
    Unicast, Multicast, SharedMem
};

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

typedef struct rctx_network {
  int sockfd;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  unsigned char buf[MAX_SO_PACKETSIZE];
} rctx_network_t;

typedef struct rctx_shmem {
  unsigned char* mmap;
  uint16_t read_idx;
} rctx_shmem_t;

typedef struct receiver_format {
  unsigned char sample_rate;
  unsigned char sample_size;
  unsigned char channels;
  uint16_t channel_map;
} receiver_format_t;

typedef struct receiver_data {
  receiver_format_t format;
  unsigned int audio_size;
  unsigned char* audio;
} receiver_data_t;


static in_addr_t get_interface(const char *name)
{
  int sockfd = socket(AF_INET,SOCK_DGRAM,0);
  struct ifreq ifr;
  in_addr_t addr = inet_addr(name);
  struct if_nameindex *ni;
  int i;

  if (addr != INADDR_NONE) {
    return addr;
  }

  if (strlen(name) >= sizeof(ifr.ifr_name)) {
    fprintf(stderr, "Too long interface name: %s\n\n", name);
    goto error_exit;
  }
  strcpy(ifr.ifr_name, name);

  sockfd = socket(AF_INET,SOCK_DGRAM,0);
  if (ioctl(sockfd, SIOCGIFADDR, &ifr) != 0) {
    fprintf(stderr, "Invalid interface: %s\n\n", name);
    goto error_exit;
  }
  close(sockfd);
  return ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr;

error_exit:
  ni = if_nameindex();
  fprintf(stderr, "Available interfaces:\n");
  for (i = 0; ni[i].if_name != NULL; i++) {
    strcpy(ifr.ifr_name, ni[i].if_name);
    if (ioctl(sockfd, SIOCGIFADDR, &ifr) == 0) {
      fprintf(stderr, "  %-10s (%s)\n", ni[i].if_name, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    }
  }
  exit(1);
}

static void* init_network(enum receiver_type receiver_mode, in_addr_t interface, int port, char* multicast_group)
{
  rctx_network_t* ctx = malloc(sizeof(rctx_network_t));
  ctx->sockfd = socket(AF_INET,SOCK_DGRAM,0);

  memset((void *)&(ctx->servaddr), 0, sizeof(ctx->servaddr));
  ctx->servaddr.sin_family = AF_INET;
  ctx->servaddr.sin_addr.s_addr = (receiver_mode == Unicast) ? interface : htonl(INADDR_ANY);
  ctx->servaddr.sin_port = htons(port);
  bind(ctx->sockfd, (struct sockaddr *)&ctx->servaddr, sizeof(ctx->servaddr));

  if (receiver_mode == Multicast) {
    ctx->imreq.imr_multiaddr.s_addr = inet_addr(multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP);
    ctx->imreq.imr_interface.s_addr = interface;

    setsockopt(ctx->sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
              (const void *)&ctx->imreq, sizeof(struct ip_mreq));
  }

  return ctx;
}

static void* init_shmem(char* shmem_device_file)
{
  rctx_shmem_t* ctx = malloc(sizeof(rctx_shmem_t));

  struct stat st;
  if (stat(shmem_device_file, &st) < 0)  {
    fprintf(stderr, "Failed to stat the shared memory file: %s\n", shmem_device_file);
    exit(2);
  }

  int shmFD = open(shmem_device_file, O_RDONLY);
  if (shmFD < 0) {
    fprintf(stderr, "Failed to open the shared memory file: %s\n", shmem_device_file);
    exit(3);
  }

  ctx->mmap = mmap(0, st.st_size, PROT_READ, MAP_SHARED, shmFD, 0);
  if (ctx->mmap == MAP_FAILED) {
    fprintf(stderr, "Failed to map the shared memory file: %s\n", shmem_device_file);
    close(shmFD);
    exit(4);
  }

  struct shmheader *header = (struct shmheader*)ctx->mmap;
  ctx->read_idx = header->write_idx;

  return ctx;
}

void rcv_network(void* network_ctx, receiver_data_t* receiver_data)
{
  rctx_network_t* ctx = (rctx_network_t*)network_ctx;
  ssize_t n = 0;

  while (n < HEADER_SIZE) {
    n = recvfrom(ctx->sockfd, ctx->buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
  }
  receiver_data->format.sample_rate = ctx->buf[0];
  receiver_data->format.sample_size = ctx->buf[1];
  receiver_data->format.channels = ctx->buf[2];
  receiver_data->format.channel_map = (ctx->buf[3] << 8) | ctx->buf[4];
  receiver_data->audio_size = n - HEADER_SIZE;
  receiver_data->audio = &ctx->buf[5];
}

void rcv_shmem(void* shmem_ctx, receiver_data_t* receiver_data)
{
  rctx_shmem_t* ctx = (rctx_shmem_t*)shmem_ctx;
  struct shmheader *header = (struct shmheader*)ctx->mmap;

  int valid = 0;
  do {
    if (header->magic != 0x11112014) {
      while (header->magic != 0x11112014) {
        usleep(10000);//10ms
      }
      ctx->read_idx = header->write_idx;
      continue;
    }
    if (ctx->read_idx == header->write_idx) {
      usleep(10000);//10ms
      continue;
    }
    if (header->channels == 0 || header->channel_map == 0)
      continue;

    valid = 1;
  } while (!valid);

  if (++ctx->read_idx == header->max_chunks) {
    ctx->read_idx = 0;
  }

  receiver_data->format.sample_rate = header->sample_rate;
  receiver_data->format.sample_size = header->sample_size;
  receiver_data->format.channels = header->channels;
  receiver_data->format.channel_map = header->channel_map;

  receiver_data->audio_size = header->chunk_size;
  receiver_data->audio = &ctx->mmap[header->offset+header->chunk_size*ctx->read_idx];
}


int main(int argc, char*argv[]) {
  int error;

  // function pointer definition for receiver
  void (*receiver_rcv_fn)(void* ctx, receiver_data_t* receiver_data);
  void *receiver_ctx;
  receiver_data_t receiver_data;

  pa_simple *s;
  pa_sample_spec ss;
  pa_channel_map channel_map;
  pa_buffer_attr buffer_attr;

  int opt;
  unsigned char cur_sample_rate = 0;
  unsigned char cur_sample_size = 0;
  unsigned char cur_channels = 2;
  unsigned char cur_channel_map_lsb = 0x03; // stereo
  unsigned char cur_channel_map_msb = 0;
  uint16_t cur_channel_map = (cur_channel_map_msb << 8) | cur_channel_map_lsb;

  // Command line options
  enum receiver_type receiver_mode = Multicast;

  char *multicast_group = NULL;
  char *ivshmem_device  = NULL;
  int target_latency_ms = 50;
  in_addr_t interface   = INADDR_ANY;
  uint16_t port         = DEFAULT_PORT;

  while ((opt = getopt(argc, argv, "i:g:p:m:t:uh")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    case 'p':
      port = atoi(optarg);
      if (!port) show_usage(argv[0]);
      break;
    case 'u':
      receiver_mode = Unicast;
      break;
    case 'g':
      multicast_group = strdup(optarg);
      break;
    case 'm':
      receiver_mode = SharedMem;
      ivshmem_device = strdup(optarg);
      break;
    case 't':
      target_latency_ms = atoi(optarg);
      if (target_latency_ms < 0) show_usage(argv[0]);
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  // Opportunistic call to renice us, so we can keep up under
  // higher load conditions. This may fail when run as non-root.
  setpriority(PRIO_PROCESS, 0, -11);

  // set application icon
  setenv("PULSE_PROP_application.icon_name", "audio-card", 0);

  // map to stereo, it's the default number of channels
  pa_channel_map_init_stereo(&channel_map);

  // Start with base default format, will switch to actual format later
  ss.format = PA_SAMPLE_S16LE;
  ss.rate = 44100;
  ss.channels = cur_channels;

  // set buffer size for requested latency
  buffer_attr.maxlength = (uint32_t)-1;
  buffer_attr.tlength = pa_usec_to_bytes((pa_usec_t)target_latency_ms * 1000u, &ss);
  buffer_attr.prebuf = (uint32_t)-1;
  buffer_attr.minreq = (uint32_t)-1;
  buffer_attr.fragsize = (uint32_t)-1;

  s = pa_simple_new(NULL,
    "Scream",
    PA_STREAM_PLAYBACK,
    NULL,
    "Audio",
    &ss,
    &channel_map,
    &buffer_attr,
    &error
  );
  if (!s) {
    fprintf(stderr, "Unable to connect to PulseAudio. %s\n", pa_strerror(error));
    goto BAIL;
  }

  // initialize receiver
  switch (receiver_mode) {
    case SharedMem:
      printf("Starting IVSHMEM receiver\n");
      receiver_ctx = init_shmem(ivshmem_device);
      receiver_rcv_fn = rcv_shmem;
      break;
    case Unicast:
    case Multicast:
    default:
      printf("Starting %s receiver\n", receiver_mode == Unicast ? "unicast" : "multicast");
      receiver_ctx = init_network(receiver_mode, interface, port, multicast_group);
      receiver_rcv_fn = rcv_network;
      break;
  }


  for (;;) {
    receiver_rcv_fn(receiver_ctx, &receiver_data);

    if (cur_sample_rate != receiver_data.format.sample_rate ||
        cur_sample_size != receiver_data.format.sample_size ||
        cur_channels != receiver_data.format.channels ||
        cur_channel_map != receiver_data.format.channel_map) {
      cur_sample_rate = receiver_data.format.sample_rate;
      cur_sample_size = receiver_data.format.sample_size;
      cur_channels = receiver_data.format.channels;
      cur_channel_map = receiver_data.format.channel_map;

      ss.channels = cur_channels;

      ss.rate = ((cur_sample_rate >= 128) ? 44100 : 48000) * (cur_sample_rate % 128);
      switch (cur_sample_size) {
        case 16: ss.format = PA_SAMPLE_S16LE; break;
        case 24: ss.format = PA_SAMPLE_S24LE; break;
        case 32: ss.format = PA_SAMPLE_S32LE; break;
        default:
          printf("Unsupported sample size %hhu, not playing until next format switch.\n", cur_sample_size);
          ss.rate = 0;
      }

      if (cur_channels == 1) {
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
        // sample spec has changed, so the playback buffer size for the requested latency must be recalculated as well
        buffer_attr.tlength = pa_usec_to_bytes((pa_usec_t)target_latency_ms * 1000, &ss);

        if (s) pa_simple_free(s);
        s = pa_simple_new(NULL,
          "Scream",
          PA_STREAM_PLAYBACK,
          NULL,
          "Audio",
          &ss,
          &channel_map,
          &buffer_attr,
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
    if (pa_simple_write(s, receiver_data.audio, receiver_data.audio_size, &error) < 0) {
      fprintf(stderr, "pa_simple_write() failed: %s\n", pa_strerror(error));
      goto BAIL;
    }
  }

  BAIL:
    if (s) pa_simple_free(s);
    return 1;
};
