//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------
//!
//!  @file
//!  TCP connection using TLV messages.
//!
//----------------------------------------------------------------------------

#pragma once
#include "tsTCPConnection.h"
#include "tstlvProtocol.h"
#include "tstlvMessageFactory.h"
#include "tstlvMessage.h"
#include "tstlvLogger.h"

namespace ts::tlv {
    //!
    //! TCP connection using TLV messages.
    //! @ingroup libtscore net tlv
    //! @tparam SAFETY The required type of thread-safety.
    //!
    template <ThreadSafety SAFETY>
    class Connection: public ts::TCPConnection
    {
        TS_NOBUILD_NOCOPY(Connection);
    public:
        //!
        //! Reference to superclass.
        //!
        using SuperClass = ts::TCPConnection;

        //!
        //! Generic definition of the mutex for this class.
        //!
        using MutexType = typename ThreadSafetyMutex<SAFETY>::type;

        //!
        //! Constructor.
        //! @param [in] protocol The incoming messages are interpreted
        //! according to this protocol. The reference is kept in this object.
        //! @param [in] auto_error_response When an invalid message is
        //! received, the corresponding error message is automatically
        //! sent back to the sender when @a auto_error_response is true.
        //! @param [in] max_invalid_msg When non-zero, the connection is
        //! automatically disconnected when the number of consecutive
        //! invalid messages has reached this value.
        //!
        explicit Connection(const Protocol& protocol, bool auto_error_response = true, size_t max_invalid_msg = 0);

        //!
        //! Serialize and sendMessage a TLV message.
        //! @param [in] msg The message to sendMessage.
        //! @param [in,out] report Where to report errors.
        //! @return True on success, false on error.
        //!
        bool sendMessage(const Message& msg, Report& report);

        //!
        //! Serialize and sendMessage a TLV message.
        //! @param [in] msg The message to sendMessage.
        //! @param [in,out] logger Where to report errors and messages.
        //! @return True on success, false on error.
        //!
        bool sendMessage(const Message& msg, Logger& logger);

        //!
        //! Receive a TLV message.
        //! Wait for the message, deserialize it and validate it.
        //! Process invalid messages and loop until a valid message is received.
        //! @param [out] msg A safe pointer to the received message.
        //! @param [in] abort If non-zero, invoked when I/O is interrupted
        //! (in case of user-interrupt, return, otherwise retry).
        //! @param [in,out] report Where to report errors.
        //! @return True on success, false on error.
        //!
        bool receiveMessage(MessagePtr& msg, const AbortInterface* abort, Report& report);

        //!
        //! Receive a TLV message.
        //! Wait for the message, deserialize it and validate it.
        //! Process invalid messages and loop until a valid message is received.
        //! @param [out] msg A safe pointer to the received message.
        //! @param [in] abort If non-zero, invoked when I/O is interrupted
        //! (in case of user-interrupt, return, otherwise retry).
        //! @param [in,out] logger Where to report errors and messages.
        //! @return True on success, false on error.
        //!
        bool receiveMessage(MessagePtr& msg, const AbortInterface* abort, Logger& logger);

        //!
        //! Get invalid incoming messages processing.
        //! @return True if, when an invalid message is received, the corresponding
        //! error message is automatically sent back to the sender.
        //!
        bool getAutoErrorResponse() const {return _auto_error_response;}

        //!
        //! Set invalid incoming messages processing.
        //! @param [in] on When an invalid message is received, the corresponding
        //! error message is automatically sent back to the sender when @a on is true.
        //!
        void setAutoErrorResponse(bool on) { _auto_error_response = on; }

        //!
        //! Get invalid message threshold.
        //! @return When non-zero, the connection is automatically disconnected
        //! when the number of consecutive invalid messages has reached this value.
        //!
        size_t getMaxInvalidMessages() const {return _max_invalid_msg;}

        //!
        //! Set invalid message threshold.
        //! @param [in] n When non-zero, the connection is automatically disconnected
        //! when the number of consecutive invalid messages has reached this value.
        //!
        void setMaxInvalidMessages(size_t n) {_max_invalid_msg = n;}

    protected:
        // Inherited from TCPConnection
        virtual void handleConnected(Report&) override;

    private:
        const Protocol& _protocol;
        bool            _auto_error_response = false;
        size_t          _max_invalid_msg = 0;
        size_t          _invalid_msg_count = 0;
        MutexType       _send_mutex {};
        MutexType       _receive_mutex {};
    };
}


//----------------------------------------------------------------------------
// Template definitions.
//----------------------------------------------------------------------------

// Constructor.
template <ts::ThreadSafety SAFETY>
ts::tlv::Connection<SAFETY>::Connection(const Protocol& protocol, bool auto_error_response, size_t max_invalid_msg) :
    ts::TCPConnection(),
    _protocol(protocol),
    _auto_error_response(auto_error_response),
    _max_invalid_msg(max_invalid_msg)
{
}

// Invoked when connection is established.
// With MSVC, we get a bogus warning:
// warning C4505: 'ts::tlv::Connection<ts::Mutex>::handleConnected': unreferenced local function has been removed
TS_PUSH_WARNING()
TS_MSC_NOWARNING(4505)
template <ts::ThreadSafety SAFETY>
void ts::tlv::Connection<SAFETY>::handleConnected(Report& report)
{
    SuperClass::handleConnected(report);
    _invalid_msg_count = 0;
}
TS_POP_WARNING()

// Serialize and sendMessage a TLV message.
template <ts::ThreadSafety SAFETY>
bool ts::tlv::Connection<SAFETY>::sendMessage(const Message& msg, Report& report)
{
    tlv::Logger logger(Severity::Debug, &report);
    return sendMessage(msg, logger);
}

// Serialize and sendMessage a TLV message.
template <ts::ThreadSafety SAFETY>
bool ts::tlv::Connection<SAFETY>::sendMessage(const Message& msg, Logger& logger)
{
    logger.log(msg, u"sending message to " + peerName());

    ByteBlockPtr bbp(new ByteBlock);
    Serializer serial(bbp);
    msg.serialize(serial);

    std::lock_guard<MutexType> lock(_send_mutex);
    return SuperClass::send(bbp->data(), bbp->size(), logger.report());
}

// Receive a TLV message (wait for the message, deserialize it and validate it)
template <ts::ThreadSafety SAFETY>
bool ts::tlv::Connection<SAFETY>::receiveMessage(MessagePtr& msg, const AbortInterface* abort, Report& report)
{
    tlv::Logger logger(Severity::Debug, &report);
    return receiveMessage(msg, abort, logger);
}

// Receive a TLV message (wait for the message, deserialize it and validate it)
template <ts::ThreadSafety SAFETY>
bool ts::tlv::Connection<SAFETY>::receiveMessage(MessagePtr& msg, const AbortInterface* abort, Logger& logger)
{
    const bool has_version(_protocol.hasVersion());
    const size_t header_size(has_version ? 5 : 4);
    const size_t length_offset(has_version ? 3 : 2);

    // Loop until a valid message is received
    for (;;) {
        ByteBlock bb(header_size);

        // Receive complete message
        {
            std::lock_guard<MutexType> lock(_receive_mutex);

            // Read message header
            if (!SuperClass::receive(bb.data(), header_size, abort, logger.report())) {
                return false;
            }

            // Get message length and read message payload
            const size_t length = GetUInt16(bb.data() + length_offset);
            bb.resize(header_size + length);
            if (!SuperClass::receive(bb.data() + header_size, length, abort, logger.report())) {
                return false;
            }
        }

        // Analyze the message
        MessageFactory mf(bb.data(), bb.size(), _protocol);
        if (mf.errorStatus() == tlv::OK) {
            _invalid_msg_count = 0;
            mf.factory(msg);
            if (msg != nullptr) {
                logger.log(*msg, u"received message from " + peerName());
            }
            return true;
        }

        // Received an invalid message
        _invalid_msg_count++;

        // Send back an error message if necessary
        if (_auto_error_response) {
            MessagePtr resp;
            mf.buildErrorResponse(resp);
            if (!sendMessage(*resp, logger.report())) {
                return false;
            }
        }

        // If invalid message max has been reached, break the connection
        if (_max_invalid_msg > 0 && _invalid_msg_count >= _max_invalid_msg) {
            logger.report().error(u"too many invalid messages from %s, disconnecting", peerName());
            disconnect(logger.report());
            return false;
        }
    }
}
