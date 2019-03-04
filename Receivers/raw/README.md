# scream-raw

scream-raw is a scream receiver using stdout as audio output.

## Compile

Run `make` command.

## Usage

```shell
$ scream-raw
```

If your machine has more than one network interface, you may need to
set the interface name which receives scream packets.

```shell
$ scream-raw -i eth0
```


