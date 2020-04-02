# scream-pulse

scream-pulse is a scream receiver using pulseaudio as audio output.

## Compile

You need pulseaudio headers in advance.

```shell
$ sudo yum install pulseaudio-libs-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libpulse-dev # Debian, Ubuntu, etc.
```

Run `make` command.

## Usage

```shell
$ scream-pulse
```

This starts the Scream client in multicast mode.
If your machine has more than one network interface, you may need to
set the interface name which receives scream packets.

```shell
$ scream-pulse -i eth0
```

### IVSHMEM (Shared memory) mode

Make sure to have read permission for the shared memory device and execute

```shell
$ scream-pulse -m /dev/shm/scream-ivshmem
```
