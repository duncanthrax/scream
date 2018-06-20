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

#define MULTICAST_TARGET "239.255.77.77"
#define MULTICAST_PORT 4010
#define MAX_SO_PACKETSIZE 1764
#define BYTES_PER_SAMPLE 2
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
  fprintf(stderr, "Usage: %s [-v] [-i interface_name_or_address] [-t target_latency_ms]\n",
	  arg0);
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

static int setup_alsa(snd_pcm_t *pcm, unsigned int rate, int verbosity, unsigned int target_latency_ms)
{
  int ret;
  int soft_resample = 1;
  unsigned int latency = target_latency_ms * 1000;

  ret = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                           CHANNELS, rate, soft_resample, latency);
  SNDCHK("snd_pcm_set_params", ret);

  if (verbosity > 0) {
    return dump_alsa_info(pcm);
  }

  return 0;
}

static int write_frames(snd_pcm_t *snd, unsigned char buf[MAX_SO_PACKETSIZE], int total_frames)
{
  int i = 0;
  int ret;
  snd_pcm_sframes_t written;

  while (i < total_frames) {
    written = snd_pcm_writei(snd, &buf[i * BYTES_PER_SAMPLE * CHANNELS], total_frames - i);
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
  int sockfd, ret;
  ssize_t n;
  struct sockaddr_in servaddr;
  struct ip_mreq imreq;
  snd_pcm_t *snd;
  unsigned char buf[MAX_SO_PACKETSIZE];
  in_addr_t interface = INADDR_ANY;
  int opt;
  int verbosity = 0;
  unsigned int rate = 44100;
  int samples;
  int target_latency_ms = 50;

  while ((opt = getopt(argc, argv, "i:t:v")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
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

  const char *device = "default";
  ret = snd_pcm_open(&snd, device, SND_PCM_STREAM_PLAYBACK, 0);
  SNDCHK("snd_pcm_open", ret);

  if (setup_alsa(snd, rate, verbosity, target_latency_ms) == -1) {
    return -1;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  memset((void *)&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(MULTICAST_PORT);
  bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

  imreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_TARGET);
  imreq.imr_interface.s_addr = interface;

  setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
             (const void *)&imreq, sizeof(struct ip_mreq));

  for (;;) {
    n = recvfrom(sockfd, &buf, MAX_SO_PACKETSIZE, 0, NULL, 0);
    samples = n / (BYTES_PER_SAMPLE * CHANNELS);
    write_frames(snd, buf, samples);
  }
}
