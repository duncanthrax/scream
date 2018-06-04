Scream - Virtual network sound card for Microsoft Windows
---------------------------------------------------------------
Scream is a virtual device driver for Windows that provides a
discrete sound device. Audio played through this device is
published on your local network as a PCM multicast stream.

Receivers on the network can pick up the stream and play it
through their own audio outputs. A simple server for Linux
(interfacing with PulseAudio) and one for Windows are provided.

Scream is based on Microsoft's MSVAD audio driver sample code.


Download and install
---------------------------------------------------------------
A ZIP file containing a signed x64 build is [available on the
GitHub releases page](https://github.com/duncanthrax/scream/releases).
The "installer" is a batch file that needs to be run with
administrator rights.

The build is supposed to run on all x64 versions of Windows 7
through Windows 10. 

Microsoft has [recently tightened the rules for signing kernel
drivers](https://docs.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-). These new rules apply to newer Windows 10 installations
that were not upgraded from an earlier version. If your installation
is subject to these rules, the driver will not install.
**Workaround: [Disable secure boot in BIOS](https://docs.microsoft.com/en-us/windows-hardware/manufacture/desktop/disabling-secure-boot).**
For more information, see [this issue](https://github.com/duncanthrax/scream/issues/8).


Usage
---------------------------------------------------------------
All audio played through the Scream device will be put onto
the local LAN as a multicast stream. The multicast target address
and port is always "239.255.77.77:4010". The audio is a raw PCM
stream, always 44100kHz, 16bit, stereo. It is transferred in UDP
frames with a payload size of max. 980 bytes, representing 1/180
second of audio. Delay is minimal, since all processing is done
on kernel level.

Receivers simply need to read the stream off the network and
stuff it into a local audio sink. The receiver system's kernel
should automatically do the necessary IGMP signalling, so it's
usually sufficient to just open a multicast listen socket and
start reading from it. Minimal buffering (~ 4 times the UDP
payload size) should be done to account for jitter.

Two receivers are provided: 

- Linux/Pulseaudio: Not included in the installer package. Just
type 'make' to build it.

- Windows: ScreamReader, contributed by @MrShoenel. Included in
the installer package as of version 1.2.

Both receivers can be run as unprivileged users.


Building
-------------------------------------------------------------
VS2015 and matching WDK are required. You might also have
luck with earlier (or future) VS versions, but I didn't test that.
