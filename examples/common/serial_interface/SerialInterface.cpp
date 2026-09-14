#include "SerialInterface.h"

#include <microReticulum/Transport.h>
#include <microReticulum/Log.h>

using namespace RNS;

#ifdef ARDUINO
SerialInterface::SerialInterface(HardwareSerial& serial, unsigned long baud /*= 115200*/, const char* name /*= "SerialInterface"*/) :
	RNS::InterfaceImpl(name), _serial(&serial), _baud(baud) {

	_IN = true;
	_OUT = true;
	_bitrate = baud;
	_HW_MTU = RNS::Type::Reticulum::MTU;
}
#endif

SerialInterface::SerialInterface(const char* name /*= "SerialInterface"*/) : RNS::InterfaceImpl(name) {
	_IN = true;
	_OUT = true;
	_HW_MTU = RNS::Type::Reticulum::MTU;
}

/*virtual*/ SerialInterface::~SerialInterface() {
	stop();
}

/*virtual*/ bool SerialInterface::start() {
	_online = false;

#ifdef ARDUINO
	if (!_serial) {
		ERROR("SerialInterface: no HardwareSerial instance provided");
		return false;
	}
	_serial->begin(_baud);
	_online = true;
	INFOF("SerialInterface: online at %lu baud", _baud);
#else
	ERROR("SerialInterface: only supported under Arduino");
#endif

	return _online;
}

/*virtual*/ void SerialInterface::stop() {
	_online = false;
	_rx_buffer.clear();
	_rx_escaped = false;
}

/*virtual*/ void SerialInterface::loop() {
	if (!_online) {
		return;
	}

#ifdef ARDUINO
	while (_serial->available() > 0) {
		uint8_t byte = (uint8_t)_serial->read();
		Bytes frame;
		if (slip_decode_byte(byte, frame)) {
			on_incoming(frame);
		}
	}
#endif
}

/*virtual*/ bool SerialInterface::send_outgoing(const Bytes& data) {
	DEBUGF("%s.send_outgoing: data: %s", toString().c_str(), data.toHex().c_str());
	bool success = true;

	try {
		if (_online) {
			TRACEF("SerialInterface: sending %lu bytes...", data.size());
			Bytes framed = slip_encode(data);
#ifdef ARDUINO
			_serial->write(framed.data(), framed.size());
#endif
			TRACE("SerialInterface: sent bytes");
		}

		// Perform post-send housekeeping
		InterfaceImpl::handle_outgoing(data);
	}
	catch (const std::exception& e) {
		ERRORF("Could not transmit on %s. The contained exception was: %s", toString().c_str(), e.what());
		success = false;
	}

	return success;
}

void SerialInterface::on_incoming(const Bytes& data) {
	DEBUGF("%s.on_incoming: data: %s", toString().c_str(), data.toHex().c_str());
	// Pass received data on to transport
	InterfaceImpl::handle_incoming(data);
}

/*static*/ Bytes SerialInterface::slip_encode(const Bytes& data) {
	Bytes framed;
	framed.append(SLIP_END);

	const uint8_t* src = data.data();
	for (size_t i = 0; i < data.size(); i++) {
		uint8_t b = src[i];
		if (b == SLIP_END) {
			framed.append(SLIP_ESC);
			framed.append(SLIP_ESC_END);
		}
		else if (b == SLIP_ESC) {
			framed.append(SLIP_ESC);
			framed.append(SLIP_ESC_ESC);
		}
		else {
			framed.append(b);
		}
	}

	framed.append(SLIP_END);
	return framed;
}

bool SerialInterface::slip_decode_byte(uint8_t byte, Bytes& out_frame) {
	if (byte == SLIP_END) {
		// SLIP allows (and senders here emit) a leading/trailing END on every
		// frame, so an empty accumulated buffer just means "between frames"
		// rather than a zero-length packet -- skip it instead of delivering.
		bool have_frame = _rx_buffer.size() > 0;
		if (have_frame) {
			out_frame = _rx_buffer;
		}
		_rx_buffer.clear();
		_rx_escaped = false;
		return have_frame;
	}

	if (_rx_escaped) {
		_rx_escaped = false;
		if (byte == SLIP_ESC_END) {
			_rx_buffer.append(SLIP_END);
		}
		else if (byte == SLIP_ESC_ESC) {
			_rx_buffer.append(SLIP_ESC);
		}
		else {
			// Malformed escape sequence -- pass the byte through as-is
			_rx_buffer.append(byte);
		}
	}
	else if (byte == SLIP_ESC) {
		_rx_escaped = true;
	}
	else {
		_rx_buffer.append(byte);
	}

	return false;
}
