//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsPcapFilter.h"
#include "tsArgs.h"


//----------------------------------------------------------------------------
// Define command line filtering options.
//----------------------------------------------------------------------------

void ts::PcapFilter::defineArgs(Args& args)
{
    args.option(u"first-packet", 0, Args::POSITIVE);
    args.help(u"first-packet",
         u"Filter packets starting at the specified number. "
         u"The packet numbering counts all captured packets from the beginning of the file, starting at 1. "
         u"This is the same value as seen on Wireshark in the leftmost column.");

    args.option<cn::microseconds>(u"first-timestamp");
    args.help(u"first-timestamp",
         u"Filter packets starting at the specified timestamp in micro-seconds from the beginning of the capture. "
         u"This is the same value as seen on Wireshark in the \"Time\" column (in seconds).");

    args.option(u"first-date", 0, Args::STRING);
    args.help(u"first-date", u"date-time",
         u"Filter packets starting at the specified date. Use format YYYY/MM/DD:hh:mm:ss.mmm.");

    args.option(u"last-packet", 0, Args::POSITIVE);
    args.help(u"last-packet",
         u"Filter packets up to the specified number. "
         u"The packet numbering counts all captured packets from the beginning of the file, starting at 1. "
         u"This is the same value as seen on Wireshark in the leftmost column.");

    args.option<cn::microseconds>(u"last-timestamp");
    args.help(u"last-timestamp",
         u"Filter packets up to the specified timestamp in micro-seconds from the beginning of the capture. "
         u"This is the same value as seen on Wireshark in the \"Time\" column (in seconds).");

    args.option(u"last-date", 0, Args::STRING);
    args.help(u"last-date", u"date-time",
         u"Filter packets up to the specified date. Use format YYYY/MM/DD:hh:mm:ss.mmm.");

    args.option(u"vlan-id", 0, Args::UINT32, 0, Args::UNLIMITED_COUNT);
    args.help(u"vlan-id",
              u"Filter packets from the specified VLAN id. "
              u"This option can be specified multiple times. "
              u"In that case, the values define nested VLAN ids, from the outer to inner VLAN.");
}


//----------------------------------------------------------------------------
// Get a date option and return it as micro-seconds since Unix epoch.
//----------------------------------------------------------------------------

cn::microseconds ts::PcapFilter::getDate(Args& args, const ts::UChar* arg_name, cn::microseconds def_value)
{
    Time date;
    const ts::UString str(args.value(arg_name));
    if (str.empty()) {
        return def_value;
    }
    else if (!date.decode(str, Time::ALL)) {
        args.error(u"invalid date \"%s\", use format \"YYYY/MM/DD:hh:mm:ss.mmm\"", str);
        return def_value;
    }
    else if (date < Time::UnixEpoch) {
        args.error(u"invalid date %s, must be after %s", str, Time::UnixEpoch);
        return def_value;
    }
    else {
        return cn::duration_cast<cn::microseconds>(cn::milliseconds(date - ts::Time::UnixEpoch));
    }
}


//----------------------------------------------------------------------------
// Load command line filtering options.
//----------------------------------------------------------------------------

bool ts::PcapFilter::loadArgs(Args& args)
{
    args.getIntValue(_opt_first_packet, u"first-packet", 0);
    args.getIntValue(_opt_last_packet, u"last-packet", std::numeric_limits<size_t>::max());
    args.getChronoValue(_opt_first_time_offset, u"first-timestamp", cn::microseconds::zero());
    args.getChronoValue(_opt_last_time_offset, u"last-timestamp", cn::microseconds::max());
    _opt_first_time = getDate(args, u"first-date", cn::microseconds::zero());
    _opt_last_time = getDate(args, u"last-date", cn::microseconds::max());

    std::vector<uint32_t> ids;
    args.getIntValues(ids, u"vlan-id");
    _opt_vlans.clear();
    for (uint32_t id : ids) {
        _opt_vlans.push_back({ETHERTYPE_NULL, id});
    }

    return true;
}


//----------------------------------------------------------------------------
// Protocol filters.
//----------------------------------------------------------------------------

void ts::PcapFilter::setProtocolFilterTCP()
{
    _protocols.clear();
    _protocols.insert(IP_SUBPROTO_TCP);
}

void ts::PcapFilter::setProtocolFilterUDP()
{
    _protocols.clear();
    _protocols.insert(IP_SUBPROTO_UDP);
}

void ts::PcapFilter::setProtocolFilter(const std::set<uint8_t>& protocols)
{
    _protocols = protocols;
}

void ts::PcapFilter::clearProtocolFilter()
{
    _protocols.clear();
}


//----------------------------------------------------------------------------
// Address filters.
//----------------------------------------------------------------------------

void ts::PcapFilter::setSourceFilter(const IPSocketAddress& addr)
{
    _source = addr;
    _bidirectional_filter = false;
}

void ts::PcapFilter::setDestinationFilter(const IPSocketAddress& addr)
{
    _destination = addr;
    _bidirectional_filter = false;
}

void ts::PcapFilter::setBidirectionalFilter(const IPSocketAddress& addr1, const IPSocketAddress& addr2)
{
    _source = addr1;
    _destination = addr2;
    _bidirectional_filter = true;
}

void ts::PcapFilter::setWildcardFilter(bool on)
{
    _wildcard_filter = on;
}

bool ts::PcapFilter::addressFilterIsSet() const
{
    const bool use_port = _protocols.empty() || _protocols.contains(IP_SUBPROTO_TCP) || _protocols.contains(IP_SUBPROTO_UDP);
    return _source.hasAddress() &&
           (!use_port || _source.hasPort()) &&
           _destination.hasAddress() &&
           (!use_port || _destination.hasPort());
}

const ts::IPSocketAddress& ts::PcapFilter::otherFilter(const IPSocketAddress& addr) const
{
    if (addr.match(_source)) {
        return _destination;
    }
    else if (addr.match(_destination)) {
        return _source;
    }
    else {
        return IPSocketAddress::AnySocketAddress4;
    }
}


//----------------------------------------------------------------------------
// Open the file, inherited method.
//----------------------------------------------------------------------------

bool ts::PcapFilter::open(const fs::path& filename, Report& report)
{
    // Invoke superclass.
    const bool ok = PcapFile::open(filename, report);
    if (ok) {
        // Reinitialize filters.
        _protocols.clear();
        _source.clear();
        _destination.clear();
        _bidirectional_filter = false;
        _wildcard_filter = true;
        _first_packet = _opt_first_packet;
        _last_packet = _opt_last_packet;
        _first_time_offset = _opt_first_time_offset;
        _last_time_offset = _opt_last_time_offset;
        _first_time = _opt_first_time;
        _last_time = _opt_last_time;
    }
    return ok;
}


//----------------------------------------------------------------------------
// Read an IPv4 packet, inherited method.
//----------------------------------------------------------------------------

bool ts::PcapFilter::readIP(IPPacket& packet, VLANIdStack& vlans, cn::microseconds& timestamp, Report& report)
{
    // Read packets until one which matches all filters.
    for (;;) {
        // Invoke superclass to read next packet.
        if (!PcapFile::readIP(packet, vlans, timestamp, report)) {
            return false;
        }

        // Check final conditions (no need to read further in the file).
        if (packetCount() > _last_packet ||
            timestamp > _last_time ||
            timeOffset(timestamp) > _last_time_offset)
        {
            return false;
        }

        // Check if the packet matches all general filters.
        if ((!_protocols.empty() && !_protocols.contains(packet.protocol())) ||
            packetCount() < _first_packet ||
            timestamp < _first_time ||
            timeOffset(timestamp) < _first_time_offset ||
            !vlans.match(_opt_vlans))
        {
            // Drop that packet.
            continue;
        }

        // Is there any unspecified field in current stream addresses (act as wildcard)?
        const IPSocketAddress src(packet.source());
        const IPSocketAddress dst(packet.destination());
        const bool unspecified = !_wildcard_filter && !addressFilterIsSet();
        bool display_filter = false;

        // Check if the IP packet belongs to the filtered session.
        // By default, _source and _destination are empty and match everything.
        if (src.match(_source) && dst.match(_destination)) {
            if (unspecified) {
                _source = src;
                _destination = dst;
                display_filter = true;
            }
        }
        else if (_bidirectional_filter && src.match(_destination) && dst.match(_source)) {
            if (unspecified) {
                _source = dst;
                _destination = src;
                display_filter = true;
            }
        }
        else {
            // Not a packet from that TCP session.
            continue;
        }

        if (display_filter) {
            report.log(_display_addresses_severity, u"selected stream %s %s %s", _source, _bidirectional_filter ? u"<->" : u"->", _destination);
        }

        report.log(2, u"packet: ip size: %'d, data size: %'d, timestamp: %'!s", packet.size(), packet.protocolDataSize(), timestamp);
        return true;
    }
}
