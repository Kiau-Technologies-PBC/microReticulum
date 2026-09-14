#pragma once

#include <microReticulum/Interface.h>
#include <microReticulum/Bytes.h>
#include <microReticulum/Type.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <stdint.h>

// Frames arbitrary-length Reticulum packets over a byte-oriented serial link
// using SLIP (RFC 1055) framing, so packet boundaries survive an
// unstructured UART/USB-serial stream. Unlike LoRaInterface/UDPInterface,
// serial has no natural datagram boundary and no onboard radio/network
// hardware is required -- this works over any HardwareSerial (a spare UART,
// or the board's USB CDC port), which makes it a good fit for a bare board
// like an STM32 Nucleo with no radio wired up.
class SerialInterface : public RNS::InterfaceImpl {

public:
	// SLIP protocol bytes (RFC 1055)
	static constexpr uint8_t SLIP_END     = 0xC0;
	static constexpr uint8_t SLIP_ESC     = 0xDB;
	static constexpr uint8_t SLIP_ESC_END = 0xDC;
	static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

public:
#ifdef ARDUINO
	SerialInterface(HardwareSerial& serial, unsigned long baud = 115200, const char* name = "SerialInterface");
#endif
	// No-hardware constructor: start() will fail (nothing to open), but this
	// lets slip_encode/slip_decode_byte be exercised in a host-side unit test.
	SerialInterface(const char* name = "SerialInterface");
	virtual ~SerialInterface();

	virtual bool start();
	virtual void stop();
	virtual void loop();

	// Encode one packet for transmission over the wire. Public/static (along
	// with slip_decode_byte below) so the framing logic can be exercised in
	// a host-side unit test without any serial hardware.
	static RNS::Bytes slip_encode(const RNS::Bytes& data);

	// Feed one received byte into the SLIP decoder. Returns true and fills
	// out_frame when a complete frame has just been decoded; otherwise
	// returns false and out_frame is left untouched. Stateful across calls
	// on a given instance -- construct a fresh instance (or call stop()+
	// start()) to reset decode state.
	bool slip_decode_byte(uint8_t byte, RNS::Bytes& out_frame);

private:
	virtual bool send_outgoing(const RNS::Bytes& data);
	void on_incoming(const RNS::Bytes& data);

private:
	RNS::Bytes _rx_buffer;
	bool _rx_escaped = false;

#ifdef ARDUINO
	HardwareSerial* _serial = nullptr;
	unsigned long _baud = 115200;
#endif

};
