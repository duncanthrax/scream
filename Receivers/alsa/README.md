# scream-alsa

scream-alsa is a scream receiver using ALSA as audio output.

## Compile

You need asound headers in advance.

```shell
$ sudo yum install alsa-lib-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libasound2-dev # Debian, Ubuntu, etc.
```

Run `make` command.

## Usage

```shell
$ scream-alsa
```

If your machine has more than one network interface, you may need to
set the interface name which receives scream packets.

```shell
$ scream-alsa -i eth0
```

If you experience excessive underruns under normal operating conditions,
lower the process niceness; if it still underruns, raise the default
output start threshold (1960) with `-t`:

```shell
$ scream-alsa -t 7840
```

Run with `-v` to dump ALSA PCM setup information.

Run with `env LIBASOUND_DEBUG=1` to debug ALSA problems.
