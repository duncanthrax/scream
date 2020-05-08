#include "pcap.h"

static pcap_t *handle;
static int (*pcap_output_callback)(receiver_data_t* receiver_data);

int init_pcap(const char* interface_name, int port, char* multicast_group) {
  struct bpf_program fp;             /* The compiled filter expression */
  bpf_u_int32 mask;                  /* The netmask of our sniffing device */
  bpf_u_int32 net;                   /* The IP of our sniffing device */
  char filter_exp[PCAP_ERRBUF_SIZE]; /* Large enough */
  char errbuf[PCAP_ERRBUF_SIZE];     /* Error buffer for calls into libpcap */

  handle = pcap_open_live(interface_name, PCAP_BUFSIZ, 1, 1000, errbuf);
  if (handle == NULL) {
      // If you have the hard requirement of having to run the receiver as non-root due to pulse/alsa uid/gid issues:
      //    setcap cap_net_raw,cap_net_admin=eip ./scream
    fprintf(stderr, "libpcap couldn't open device %s: %s\n", interface_name, errbuf);
    return 1;
  }

  snprintf(filter_exp, PCAP_ERRBUF_SIZE, "udp port %d", port);
  if (pcap_lookupnet(interface_name, &net, &mask, errbuf) == -1) {
    fprintf(stderr, "WARN: libpcap couldn't get netmask for device %s (often okay to ignore, especially for macvtap)\n", interface_name);
    net = 0;
    mask = 0;
  }

  if (pcap_datalink(handle) != DLT_EN10MB) {
    fprintf(stderr, "libpcap device %s doesn't provide Ethernet headers, not supported\n", interface_name);
    return 2;
  }

  if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
    fprintf(stderr, "libpcap parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
    return 3;
  }
  if (pcap_setfilter(handle, &fp) == -1) {
    fprintf(stderr, "libpcap install filter %s: %s\n", filter_exp, pcap_geterr(handle));
    return 4;
  }
}

void pcap_callback(u_char *args, const struct pcap_pkthdr *header, const u_char *pkg)
{
  const struct sniff_ethernet *ethernet;  /* The ethernet header */
  const struct sniff_ip *ip;              /* The IP header */
  const struct sniff_udp *udp;            /* The UDP header */
  u_char* payload;                        /* Packet payload */

  int size_ip;
  int size_udp;
  int size_payload;

  /* define ethernet header */
  ethernet = (struct sniff_ethernet*)(pkg);

  /* define/compute ip header offset */
  ip = (struct sniff_ip*)(pkg + SIZE_ETHERNET);
  size_ip = IP_HL(ip) * 4;

  if (size_ip < 20) {
    fprintf(stderr, "pcap captured a packet with invalid IP header length of %u\n", size_ip);
    return;
  }

  /* define/compute udp header offset */
  udp = (struct sniff_udp*)(pkg + SIZE_ETHERNET + size_ip);
  size_udp = ntohs(udp->uh_ulen);

  /* define/compute udp payload (daragram) offset */
  payload = (u_char *)(pkg + SIZE_ETHERNET + size_ip + 8);

  /* compute udp payload (datagram) size */
  size_payload = ntohs(ip->ip_len) - (size_ip + 8);

  if (size_payload == 0 || size_udp == 0) {
    fprintf(stderr, "pcap captured a udp packet with (size_payload=%d, size_udp=%d)\n", size_payload, size_ip);
    return;
  }

  receiver_data_t receiver_data = {0};
  if (size_payload < HEADER_SIZE) {
    fprintf(stderr, "WARN: received packet shorter than %d\n", HEADER_SIZE);
    return;
  }

  receiver_data.format.sample_rate = payload[0];
  receiver_data.format.sample_size = payload[1];
  receiver_data.format.channels = payload[2];
  receiver_data.format.channel_map = (payload[4] << 8u) | payload[3];
  receiver_data.audio_size = size_payload - HEADER_SIZE;
  receiver_data.audio = &payload[5];

  int ret = pcap_output_callback(&receiver_data);
  if (ret != 0) {
    fprintf(stderr, "WARN: output function failed with %d\n", ret);
  }
}

int run_pcap(int (*output_function)(receiver_data_t*))
{
  pcap_output_callback = output_function;
  return pcap_loop(handle, -1, pcap_callback, NULL);
}