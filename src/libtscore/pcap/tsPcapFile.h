//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  Pcap and pcapng file.
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsReport.h"
#include "tsMemory.h"
#include "tsTime.h"
#include "tsIPPacket.h"
#include "tsPcap.h"

namespace ts {
    //!
    //! Read a pcap or pcapng capture file format.
    //! @ingroup libtscore net
    //!
    //! This is the type of files which is created by Wireshark.
    //! This class reads a pcap or pcapng file and extracts IP frames (IPv4 or IPv6).
    //! All metadata and all other types of frames are ignored.
    //!
    //! @see https://tools.ietf.org/pdf/draft-gharris-opsawg-pcap-02.pdf (PCAP)
    //! @see https://datatracker.ietf.org/doc/draft-gharris-opsawg-pcap/ (PCAP tracker)
    //! @see https://tools.ietf.org/pdf/draft-tuexen-opsawg-pcapng-04.pdf (PCAP-ng)
    //! @see https://datatracker.ietf.org/doc/draft-tuexen-opsawg-pcapng/ (PCAP-ng tracker)
    //!
    class TSCOREDLL PcapFile
    {
        TS_NOCOPY(PcapFile);
    public:
        //!
        //! Default constructor.
        //!
        PcapFile() = default;

        //!
        //! Destructor.
        //!
        virtual ~PcapFile();

        //!
        //! Open the file for read.
        //! @param [in] filename File name. If empty or "-", use standard input.
        //! @param [in,out] report Where to report errors.
        //! @return True on success, false on error.
        //!
        virtual bool open(const fs::path& filename, Report& report);

        //!
        //! Check if the file is open.
        //! @return True if the file is open, false otherwise.
        //!
        bool isOpen() const { return _in != nullptr; }

        //!
        //! Get the file name.
        //! @return The file name as specified in open().
        //! If the standard input is used, return "standard input".
        //!
        fs::path fileName() const { return _name; }

        //!
        //! Read the next IP packet, IPv4 or IPv6, headers included.
        //! Skip intermediate metadata and other types of packets.
        //!
        //! @param [out] packet Received IP packet.
        //! @param [out] vlans Stack of VLAN encapsulation from which the packet is extracted.
        //! @param [out] timestamp Capture timestamp in microseconds since Unix epoch or -1 if none is available.
        //! @param [in,out] report Where to report error.
        //! @return True on success, false on error.
        //!
        virtual bool readIP(IPPacket& packet, VLANIdStack& vlans, cn::microseconds& timestamp, Report& report);

        //!
        //! Get the number of captured packets so far.
        //! This includes all packets, not only IP packets.
        //! This value is the number of the last returned packet, as seen in the left-most column in Wireshark interface.
        //! @return The number of captured packets so far.
        //!
        uint64_t packetCount() const { return _packet_count; }

        //!
        //! Check if the end of file (or other error) has been reached.
        //! @return True on end of file or error.
        //!
        bool endOfFile() const { return _error; }

        //!
        //! Get the number of valid captured IP packets so far.
        //! @return The number of valid captured IP packets so far.
        //!
        uint64_t ipPacketCount() const { return _ip_packet_count; }

        //!
        //! Get the total file size in bytes so far.
        //! @return The total file size in bytes so far.
        //!
        uint64_t fileSize() const { return _file_size; }

        //!
        //! Get the total size in bytes of captured packets so far.
        //! This includes all packets, including link-layer headers when present.
        //! @return The total size in bytes of captured packets so far.
        //!
        uint64_t totalPacketsSize() const { return _packets_size; }

        //!
        //! Get the total size in bytes of valid captured IP packets so far.
        //! This includes all IP headers but not link-layer headers when present.
        //! @return The total size in bytes of valid captured IP packets so far.
        //!
        uint64_t totalIPPacketsSize() const { return _ip_packets_size; }

        //!
        //! Get the capture timestamp of the first packet in the file.
        //! @return Capture timestamp in microseconds since Unix epoch or -1 if none is available.
        //!
        cn::microseconds firstTimestamp() const { return _first_timestamp; }

        //!
        //! Get the capture timestamp of the last packet which was read from the file.
        //! @return Capture timestamp in microseconds since Unix epoch or -1 if none is available.
        //!
        cn::microseconds lastTimestamp() const { return _last_timestamp; }

        //!
        //! Compute the time offset from the beginning of the file of a packet timestamp.
        //! @param [in] timestamp Capture timestamp of a packet in the file.
        //! @return Time offset in microseconds of the packet from the beginning of the file.
        //!
        cn::microseconds timeOffset(cn::microseconds timestamp) const
        {
            return timestamp < cn::microseconds::zero() || _first_timestamp < cn::microseconds::zero() ? cn::microseconds::zero() : timestamp - _first_timestamp;
        }

        //!
        //! Compute the date and time from a packet timestamp.
        //! @param [in] timestamp Capture timestamp of a packet in a file.
        //! @return Corresponding date or Epoch in case of error.
        //!
        static Time ToTime(cn::microseconds timestamp)
        {
            return timestamp < cn::microseconds::zero() ? Time::Epoch : Time::UnixEpoch + timestamp;
        }

        //!
        //! Close the file.
        //! Do not reset counters, file names, etc. The last values before close() are still accessible.
        //!
        virtual void close();

    private:
        // Descriptioon of one capture interface.
        // Pcap files have only one interface, pcap-ng files may have more.
        class InterfaceDesc
        {
        public:
            InterfaceDesc() = default;
            uint16_t         link_type {LINKTYPE_UNKNOWN};
            size_t           fcs_size = 0;     // Number of Frame Cyclic Sequences bytes after each packet.
            std::intmax_t    time_units = 0;   // Time units per second.
            cn::microseconds time_offset {0};  // Offset to add to all time stamps.
        };

        bool             _error = false;          // Error was set, may be logical error, not a file error.
        std::istream*    _in = nullptr;           // Point to actual input stream.
        std::ifstream    _file {};                // Input file (when it is a named file).
        UString          _name {};                // Saved file name for messages.
        bool             _be = false;             // The file use a big-endian representation.
        bool             _ng = false;             // Pcapng format (not pcap).
        uint16_t         _major = 0;              // File format major version.
        uint16_t         _minor = 0;              // File format minor version.
        uint64_t         _file_size = 0;          // Number of bytes read so far.
        uint64_t         _packet_count = 0;       // Count of captured packets.
        uint64_t         _ip_packet_count = 0;    // Count of captured IP packets.
        uint64_t         _packets_size = 0;       // Total size in bytes of captured packets.
        uint64_t         _ip_packets_size = 0;    // Total size in bytes of captured IP packets.
        cn::microseconds _first_timestamp {-1};   // Timestamp of first packet in file.
        cn::microseconds _last_timestamp {-1};    // Timestamp of last packet in file.
        std::vector<InterfaceDesc> _if {};        // Capture interfaces by index, only one in pcap files.

        // Report an error (if fmt is not empty), set error indicator, return false.
        bool error()
        {
            _error = true;
            return false;
        }
        template <class... Args>
        bool error(Report& report, const UChar* fmt, Args&&... args)
        {
            report.error(fmt, std::forward<ArgMixIn>(args)...);
            return error();
        }

        // Read exactly "size" bytes. Return false if not enough bytes before eof.
        bool readall(uint8_t* data, size_t size, Report& report);

        // Read a file / section header, starting from a magic number which was read as big endian.
        bool readHeader(uint32_t magic, Report& report);

        // Analyze a pcap-ng interface description.
        bool analyzeNgInterface(const uint8_t* data, size_t size, Report& report);

        // Read a pcap-ng block. The 32-bit block type has already been read.
        // Start at "Block total length". Read complete block, including the two length fields.
        // Return only the block body.
        bool readNgBlockBody(uint32_t block_type, ByteBlock& body, Report& report);

        // Read 32 or 16 bits using the endianness.
        uint16_t get16(const void* addr) const { return _be ? GetUInt16BE(addr) : GetUInt16LE(addr); }
        uint32_t get32(const void* addr) const { return _be ? GetUInt32BE(addr) : GetUInt32LE(addr); }
        uint64_t get64(const void* addr) const { return _be ? GetUInt64BE(addr) : GetUInt64LE(addr); }
    };
}
