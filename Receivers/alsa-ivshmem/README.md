# scream-ivshmem-alsa

scream-ivshmem-alsa is a scream receiver using ALSA as audio output.

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
$ scream-ivshmem-alsa /dev/shm/scream-ivshmem
```

If you experience excessive underruns under normal operating conditions,
lower the process niceness; if it still underruns, raise the default
target latency (50 ms) with `-t`:

```shell
$ scream-ivshmem-alsa /dev/shm/scream-ivshmem -t 100
```

Note that audio hardware typically has small buffers that result in a
latency lower than the target latency.

Run with `-v` to dump ALSA PCM setup information.

Run with `env LIBASOUND_DEBUG=1` to debug ALSA problems.
