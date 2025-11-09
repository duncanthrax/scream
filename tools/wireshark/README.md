# Scream Wireshark Dissector

This folder contains a Wireshark Lua dissector tailored to the Scream virtual audio driver. It documents the UDP packet format emitted by the driver and explains how to deploy, customize, and troubleshoot the dissector when analysing captures.

## Features

- Highlights Scream traffic in the packet list with a concise `PCM` summary.
- Decodes the five-byte header (rate marker, sample size, channel count, channel mask) and exposes the PCM payload for further inspection.
- Displays human-readable speaker roles derived from the Windows channel mask.
- Provides an optional heuristic UDP registration so the dissector claims packets even when another protocol shares the port.

## Packet format

Each UDP datagram emitted by the Scream driver carries five bytes of header followed by PCM audio data. The default multicast destination is `239.255.77.77:4010`, but the dissector works for any UDP port you attach it to.

```
Offset  Size  Description
------  ----  ----------------------------------------------------------
0       1     Sample rate marker
1       1     Bits per sample (16, 24 or 32 are currently used)
2       1     Number of interleaved channels (1–8)
3       2     Channel map (little-endian Windows speaker mask)
5       N     PCM payload (interleaved, little-endian samples)
```

### Sample rate marker

The first byte encodes both the base sampling family and a multiplier:

- If bit 7 is **unset**, the base rate is 48 kHz. The lower 7 bits hold the multiplier (`1 → 48 kHz`, `2 → 96 kHz`, `4 → 192 kHz`, …).
- If bit 7 is **set**, the base rate is 44.1 kHz. The lower 7 bits hold the multiplier (`1 → 44.1 kHz`, `2 → 88.2 kHz`, `4 → 176.4 kHz`, …).

The driver guarantees that the multiplier is never zero for valid streams.

### Channel map

The two-byte channel mask follows the Windows speaker mask layout (`KS
digital`), for example:

- `0x0001` – Front Left
- `0x0002` – Front Right
- `0x0003` – Stereo (Front Left + Front Right)
- `0x060F` – 5.1 surround (Front L/R/C, Low Frequency, Back L/R)

The dissector expands the mask into human-readable role names when it is known.

## Wireshark dissector

`scream.lua` registers itself automatically for UDP port 4010 and exposes a heuristic UDP dissector so Scream frames can be identified even when another protocol claims the port.

### Installation

1. Copy `scream.lua` into one of Wireshark's personal or global plugin folders.
   - Windows: `%APPDATA%\Wireshark\plugins`
   - macOS: `~/Library/Application Support/Wireshark/Plug-ins`
   - Linux: `~/.local/lib/wireshark/plugins/<version>`
2. Restart Wireshark. You should now see packets labeled `SCREAM` with decoded header information.
3. When the protocol preferences are available, change or disable the static UDP binding under **Edit → Preferences → Protocols → Scream Audio**. Set the port to `0` to rely solely on the heuristic registration. Older builds keep the default value (4010).
4. You can still use **Analyze → Decode As…** to pin the dissector to any additional ports when reviewing captures.

### Usage tips

- In the packet list, the dissector prints the format as `PCM 24-bit 2 ch @ 48000 Hz (Front Left, Front Right)` for quick triage.
- The packet details pane contains the original marker bytes as well as the derived speaker roles; expand the protocol node to verify masks or troubleshoot channel routing.

### Development & testing

- The Lua script targets Wireshark 3.4+; earlier builds may lack the preference API or heuristic registration.

## Troubleshooting

- Ensure captures include the raw UDP payload (for live captures, disable any codec offloading).
- If the dissector shows "Unknown" channel roles, the mask contains speaker positions that are outside the standard Windows assignments. The raw hexadecimal value is still displayed for manual analysis.
- The dissector does not attempt to validate or render the PCM data. Use an audio player or another receiver for playback if you need to monitor the audio stream.
