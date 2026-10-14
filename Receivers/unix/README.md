# scream

scream is a Scream audio receiver using Pulseaudio, ALSA or stdout as audio output.

## Compile

Compilation is done using CMake.

If Pulseaudio or ALSA headers are found, support for those will be compiled in (see below for distro packages to install). If Pulseaudio is compiled in, it will be the default output, otherwise ALSA will be the default. If both Pulseaudio and ALSA are not compiled in, raw/stdout will be the default.

```shell
$ mkdir build && cd build
$ cmake ..
$ make
```

### Pulseaudio

```shell
$ sudo yum install pulseaudio-libs-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libpulse-dev # Debian, Ubuntu, etc.
```

### ALSA

```shell
$ sudo yum install alsa-lib-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libasound2-dev # Debian, Ubuntu, etc.
```

## Usage

You can see the accepted options by using the -h (help) option.

```shell
$ scream -h
```

### Network mode

```shell
$ scream
```

This starts the Scream client in multicast mode, using the default audio output.
Unicast mode is also supported, and can be used by passing the -u option. If your machine has more than one network interface, you may need to set the interface name which receives scream packets.

```shell
$ scream -i eth0
```

### libpcap mode

This starts the Scream client in libpcap (sniffer) mode. This mode is mostly useful if you are able to the UDP
multicast/unicast transmission in `wireshark`/`tcpdump` but unable to have it be delivered to the
user-space Scream client.

```shell
$ scream -P -i macvtap0
```

If you have the hard requirement of having to run the receiver as non-root due to `pulseaudio`/`alsa`
uid/gid issues, you can do the following:

```shell
# setcap cap_net_raw,cap_net_admin=eip ./scream
```

### IVSHMEM (Shared memory) mode

Make sure to have read permission for the shared memory device and execute

```shell
$ scream -m /dev/shm/scream-ivshmem
```

### PulseAudio output

Use `-t` to request a target buffer duration (default 50 ms) and `-l` to
request a maximum buffer duration (default 100 ms), for example:

```shell
$ scream -o pulse -t 20 -l 100
```

Both byte limits are recalculated for the active sample rate, sample size
and channel count. With `-v`, the receiver reports requested buffer values
at stream creation. The server may negotiate different values; these requests
are not a guarantee of end-to-end latency. The simple PulseAudio API does not
expose negotiated buffer attributes. Inspect the stream on the server to
verify effective buffering.

### ALSA output

If you experience excessive underruns under normal operating conditions,
lower the process niceness; if it still underruns, raise the default
target latency (50 ms) with `-t`:

```shell
$ scream -o alsa -t 100
```

Note that audio hardware typically has small buffers that result in a
latency lower than the target latency.

Run with `-v` to dump ALSA PCM setup information.

Run with `env LIBASOUND_DEBUG=1` to debug ALSA problems.
