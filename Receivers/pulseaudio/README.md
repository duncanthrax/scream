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

If your machine has more than one network interface, you may need to
set the interface name which receives scream packets.

```shell
$ scream-pulse -i eth0
```
