-- Wireshark dissector for Scream audio packets
-- Drop this file into one of Wireshark's plugin folders and restart Wireshark.

local scream_proto = Proto("scream", "Scream Audio")
local default_udp_port = 4010

local band = bit32 and bit32.band or bit.band
local bor = bit32 and bit32.bor or bit.bor
local lshift = bit32 and bit32.lshift or bit.lshift

local f_sample_rate_marker = ProtoField.uint8("scream.sample_rate_marker", "Sample Rate Marker", base.DEC)
local f_sample_rate = ProtoField.uint32("scream.sample_rate", "Sample Rate", base.DEC, nil, nil, "Derived playback sample rate in Hz")
local f_bits_per_sample = ProtoField.uint8("scream.bits_per_sample", "Bits Per Sample", base.DEC)
local f_channels = ProtoField.uint8("scream.channels", "Channels", base.DEC)
local f_channel_map = ProtoField.uint16("scream.channel_map", "Channel Map", base.HEX)
local f_channel_list = ProtoField.string("scream.channel_list", "Channel Roles")
local f_payload = ProtoField.bytes("scream.payload", "PCM Payload")

scream_proto.fields = {
    f_sample_rate_marker,
    f_sample_rate,
    f_bits_per_sample,
    f_channels,
    f_channel_map,
    f_channel_list,
    f_payload
}

local HEADER_SIZE = 5

local channel_roles = {
    [0]  = "Front Left",
    [1]  = "Front Right",
    [2]  = "Front Center",
    [3]  = "Low Frequency",
    [4]  = "Back Left",
    [5]  = "Back Right",
    [6]  = "Front Left of Center",
    [7]  = "Front Right of Center",
    [8]  = "Back Center",
    [9]  = "Side Left",
    [10] = "Side Right",
    [11] = "Top Center",
    [12] = "Top Front Left",
    [13] = "Top Front Center",
    [14] = "Top Front Right",
    [15] = "Top Back Left",
    [16] = "Top Back Center",
    [17] = "Top Back Right"
}

local function decode_sample_rate(marker)
    local base = band(marker, 0x80) ~= 0 and 44100 or 48000
    local multiplier = band(marker, 0x7F)
    if multiplier == 0 then
        -- Fallback: treat zero multiplier as one frame worth of the base rate.
        multiplier = 1
    end
    return base * multiplier
end

local function describe_channels(mask)
    if mask == 0 then
        return "Unknown"
    end
    local list = {}
    for bit_index = 0, 17 do
        if band(mask, lshift(1, bit_index)) ~= 0 then
            list[#list + 1] = channel_roles[bit_index] or ("Bit " .. bit_index)
        end
    end
    return table.concat(list, ", ")
end

local function looks_like_scream(buffer)
    if buffer:len() < HEADER_SIZE then
        return false
    end

    local marker = buffer(0, 1):uint()
    local multiplier = band(marker, 0x7F)
    if multiplier == 0 or multiplier > 64 then
        return false
    end

    local bits = buffer(1, 1):uint()
    if bits ~= 16 and bits ~= 24 and bits ~= 32 then
        return false
    end

    local channels = buffer(2, 1):uint()
    if channels == 0 or channels > 8 then
        return false
    end

    return true
end

function scream_proto.dissector(buffer, pinfo, tree)
    if not looks_like_scream(buffer) then
        return 0
    end

    local marker = buffer(0, 1):uint()
    local bits = buffer(1, 1):uint()
    local channels = buffer(2, 1):uint()
    local channel_map = bor(buffer(3, 1):uint(), lshift(buffer(4, 1):uint(), 8))
    local sample_rate = decode_sample_rate(marker)
    local channel_desc = describe_channels(channel_map)

    pinfo.cols.protocol = "SCREAM"
    pinfo.cols.info = string.format("PCM %d-bit %d ch @ %d Hz (%s)", bits, channels, sample_rate, channel_desc)

    local subtree = tree:add(scream_proto, buffer())
    local marker_item = subtree:add(f_sample_rate_marker, buffer(0, 1))
    marker_item:append_text(string.format(" (base %d Hz x %d)", band(marker, 0x80) ~= 0 and 44100 or 48000, band(marker, 0x7F)))

    subtree:add(f_sample_rate, buffer(0, 1), sample_rate)
    subtree:add(f_bits_per_sample, buffer(1, 1))
    subtree:add(f_channels, buffer(2, 1))

    local channel_item = subtree:add(f_channel_map, buffer(3, 2))
    channel_item:append_text(" [" .. channel_desc .. "]")
    subtree:add(f_channel_list, buffer(3, 2), channel_desc)

    if buffer:len() > HEADER_SIZE then
        subtree:add(f_payload, buffer(HEADER_SIZE))
    end
end

local udp_port_table = DissectorTable.get("udp.port")
local current_udp_port = nil

local function register_udp_port(port)
    if current_udp_port and current_udp_port > 0 then
        udp_port_table:remove(current_udp_port, scream_proto)
    end
    if port and port > 0 then
        udp_port_table:add(port, scream_proto)
        current_udp_port = port
        return
    end
    current_udp_port = nil
end

local prefs_ok = false
if Pref and Pref.uint then
    prefs_ok = pcall(function()
        scream_proto.prefs = {
            udp_port = Pref.uint("UDP port", default_udp_port, "UDP port to decode automatically. Set to 0 to rely solely on the heuristic registration."),
        }
    end)
end

if prefs_ok then
    function scream_proto.init()
        register_udp_port(scream_proto.prefs.udp_port or default_udp_port)
    end

    function scream_proto.prefs_changed()
        register_udp_port(scream_proto.prefs.udp_port or default_udp_port)
    end
else
    function scream_proto.init()
        register_udp_port(default_udp_port)
    end
end

register_udp_port(default_udp_port)

local function scream_heur_dissector(buffer, pinfo, tree)
    if not looks_like_scream(buffer) then
        return false
    end
    scream_proto.dissector(buffer, pinfo, tree)
    return true
end

scream_proto:register_heuristic("udp", scream_heur_dissector)
