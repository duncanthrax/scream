# scream-ivshmem-pulse

scream-ivshmem-pulse is a scream receiver using pulseaudio as audio output and IVSHMEM to share the ring buffer between guest and host.

## Compile

You need pulseaudio headers in advance.

```shell
$ sudo yum install pulseaudio-libs-devel # Redhat, CentOS, etc.
or
$ sudo apt-get install libpulse-dev # Debian, Ubuntu, etc.
```

Run `make` command.

## Usage

Launch your VM, make sure to have read permission for the shared memory device and execute

```shell
$ scream-ivshmem-pulse /dev/shm/scream-ivshmem
```
