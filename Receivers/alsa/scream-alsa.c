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
#define TYPICAL_SAMPLES_PER_PACKET (980 / (BYTES_PER_SAMPLE * CHANNELS))

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
  fprintf(stderr, "Usage: %s [-v] [-i interface_name_or_address] [-t alsa_output_start_threshold]\n",
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

static void dump_alsa_info(snd_pcm_t *pcm)
{
  snd_output_t *log;
  snd_output_stdio_attach(&log, stderr, 0);
  snd_pcm_dump(pcm, log);
  snd_output_close(log);
}

static int setup_alsa(snd_pcm_t *pcm, unsigned int rate, unsigned int channels, int start_threshold, int verbosity)
{
  int ret;
  snd_pcm_hw_params_t *hw;
  snd_pcm_sw_params_t *sw;

  snd_pcm_hw_params_alloca(&hw);

  ret = snd_pcm_hw_params_any(pcm, hw);
  SNDCHK("snd_pcm_hw_params_any", ret);

  ret = snd_pcm_hw_params_set_rate_resample(pcm, hw, 1);
  SNDCHK("snd_pcm_hw_params_set_rate_resample", ret);

  ret = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
  SNDCHK("snd_pcm_hw_params_set_access", ret);

  ret = snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
  SNDCHK("snd_pcm_hw_params_set_format", ret);

  ret = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
  SNDCHK("snd_pcm_hw_params_set_rate_near", ret);

  ret = snd_pcm_hw_params_set_channels(pcm, hw, channels);
  SNDCHK("snd_pcm_hw_params_set_channels", ret);

  ret = snd_pcm_hw_params(pcm, hw);
  SNDCHK("hw_params", ret);

  ret = snd_pcm_prepare(pcm);
  SNDCHK("snd_pcm_prepare", ret);

  snd_pcm_sw_params_alloca(&sw);

  ret = snd_pcm_sw_params_current(pcm, sw);
  SNDCHK("snd_pcm_sw_params_current", ret);

  // Avoid underruns
  ret = snd_pcm_sw_params_set_start_threshold(pcm, sw, start_threshold);
  SNDCHK("snd_pcm_sw_params_set_start_threshold", ret);

  ret = snd_pcm_sw_params(pcm, sw);
  SNDCHK("snd_pcm_sw_params", ret);

  if (verbosity > 0) {
    dump_alsa_info(pcm);
  }

  return 0;
}

static int write_frames(snd_pcm_t *snd, void *pcm, int num_frames)
{
  int ret;
  snd_pcm_sframes_t f;

  f = snd_pcm_writei(snd, pcm, num_frames);
  if (f < 0) {
    ret = snd_pcm_recover(snd, f, 0);
    SNDCHK("snd_pcm_recover", ret);
    return 0;
  }
  if (f < num_frames) {
    fprintf(stderr, "Short write %ld\n", f);
  }
  return num_frames;
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
  int start_threshold = TYPICAL_SAMPLES_PER_PACKET * 8;

  while ((opt = getopt(argc, argv, "i:t:v")) != -1) {
    switch (opt) {
    case 'i':
      interface = get_interface(optarg);
      break;
    case 't':
      start_threshold = atoi(optarg);
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

  if (setup_alsa(snd, rate, CHANNELS, start_threshold, verbosity) == -1) {
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
    write_frames(snd, &buf, samples);
  }
}
