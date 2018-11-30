#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_MULTICAST_GROUP "239.255.77.77"
#define DEFAULT_PORT 4010

#define MAX_SO_PACKETSIZE 1154
#define CHANNELS 2

#define SNDCHK(call, ret) { \
  if (ret < 0) {            \
    alsa_error(call, ret);  \
    return -1;              \
  }                         \
}

static void alsa_error(const char *msg, int r)
{
  fprintf(stderr, "%s: %s\n", msg, snd_strerror(r));
}

static void show_usage(const char *arg0)
{
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s [-u] [-v] [-p <port>] [-i <iface>] [-g <group>] [-t <target_latency_ms>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional. Default is to use\n");
  fprintf(stderr, "         multicast with group address 239.255.77.77, port 4010.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -u          : Use unicast instead of multicast.\n");
  fprintf(stderr, "         -v          : Verbose operation.\n");
  fprintf(stderr, "         -p <port>   : Use <port> instead of default port 4010.\n");
  fprintf(stderr, "                       Applies to both multicast and unicast.\n");
  fprintf(stderr, "         -i <iface>  : Use local interface <iface>. Either the IP\n");
  fprintf(stderr, "                       or the interface name can be specified. In\n");
  fprintf(stderr, "                       multicast mode, uses this interface for IGMP.\n");
  fprintf(stderr, "                       In unicast, binds to this interface only.\n");
  fprintf(stderr, "         -g <group>  : Multicast group address. Multicast mode only.\n");
  fprintf(stderr, "         -t <latency>: Target latency in milliseconds. Defaults to 50ms.\n");
  fprintf(stderr, "\n");
  exit(1);
}

static in_addr_t get_interface(const char *name)
{
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
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

static int setup_alsa(snd_pcm_t **psnd, snd_pcm_format_t format, unsigned int rate, int verbosity, unsigned int target_latency_ms)
{
  int ret;
  const char *device = "default";
  int soft_resample = 1;
  unsigned int latency = target_latency_ms * 1000;

  ret = snd_pcm_open(psnd, device, SND_PCM_STREAM_PLAYBACK, 0);
  SNDCHK("snd_pcm_open", ret);

  ret = snd_pcm_set_params(*psnd, format, SND_PCM_ACCESS_RW_INTERLEAVED,
                           CHANNELS, rate, soft_resample, latency);
  SNDCHK("snd_pcm_set_params", ret);

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

static int write_frames(snd_pcm_t *snd, unsigned char buf[MAX_SO_PACKETSIZE-2], int total_frames, int bytes_per_sample)
{
  int i = 0;
  int ret;
  snd_pcm_sframes_t written;

  while (i < total_frames) {
    written = snd_pcm_writei(snd, &buf[i * bytes_per_sample * CHANNELS], total_frames - i);
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

int main(int argc, char *argv[])
{
  int sockfd;
  ssize_t n;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  snd_pcm_t *snd;
  unsigned char buf[MAX_SO_PACKETSIZE];
  int opt;
  int verbosity = 0;
  int samples;
  snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
  unsigned int bytes_per_sample = 2;
  unsigned int rate = 44100;
  unsigned char cur_server_rate = 0;
  unsigned char cur_server_size = 0;

  // Command line options
  int use_unicast       = 0;
  char *multicast_group = NULL;
  in_addr_t interface   = INADDR_ANY;
  uint16_t port         = DEFAULT_PORT;
  int target_latency_ms = 50;


  while ((opt = getopt(argc, argv, "i:g:p:t:vuh")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    case 'p':
      port = atoi(optarg);
      if (!port) show_usage(argv[0]);
      break;
    case 'u':
      use_unicast = 1;
      break;
    case 'g':
      multicast_group = strdup(optarg);
      break;
    case 't':
      target_latency_ms = atoi(optarg);
      break;
    case 'v':
      verbosity += 1;
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = use_unicast ? interface : htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  if (!use_unicast) {
    imreq.imr_multiaddr.s_addr = inet_addr(multicast_group ? multicast_group : DEFAULT_MULTICAST_GROUP);
    imreq.imr_interface.s_addr = interface;

    setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
               (const void *)&imreq, sizeof(struct ip_mreq));
  }

  if (setup_alsa(&snd, format, rate, verbosity, target_latency_ms) == -1) {
    return -1;
  }

  for (;;) {
    n = recvfrom(sockfd, &buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
    if (n < 2) continue;
    
    // Change rate/size?
    if (cur_server_rate != buf[0] || cur_server_size != buf[1]) {
        cur_server_rate = buf[0];
        cur_server_size = buf[1];

        rate = ((cur_server_rate >= 128) ? 44100 : 48000) * (cur_server_rate % 128);
        switch (cur_server_size) {
          case 16: format = SND_PCM_FORMAT_S16_LE; bytes_per_sample = 2; break;
          case 24: format = SND_PCM_FORMAT_S24_3LE; bytes_per_sample = 3; break;
          case 32: format = SND_PCM_FORMAT_S32_LE; bytes_per_sample = 4; break;
          default:
            if (verbosity > 0)
              printf("Unsupported sample size %hhu, not playing until next format switch.\n", cur_server_size);
            rate = 0;
        }

        if (rate) {
          close_alsa(snd);
          if (setup_alsa(&snd, format, rate, verbosity, target_latency_ms) == -1) {
            if (verbosity > 0)
              printf("Unable to set up ALSA with sample rate %u and sample size %hhu, not playing until next format switch.\n", rate, cur_server_size);
            snd = NULL;
            rate = 0;
          }
          else {
            if (verbosity > 0)
              printf("Switched format to sample rate %u and sample size %hhu.\n", rate, cur_server_size);
          }
        }
    }
    if (!rate) continue;

    samples = (n - 2) / (bytes_per_sample * CHANNELS);
    write_frames(snd, &buf[2], samples, bytes_per_sample);
  }
}
