// Minimal STM32 Nucleo-WL55JC1 bring-up smoke test.
//
// No radio, no filesystem -- just a loopback Interface pair (same pattern as
// test/test_rns_loopback) proving Reticulum, Transport, and the packet path
// initialize and keep running on real hardware. Watch the serial monitor at
// 115200 baud: a periodic announce should loop back and be reported by
// ExampleAnnounceHandler every ~10 seconds with no reset/crash.

#include <microReticulum.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

class InInterface : public RNS::InterfaceImpl {
public:
	InInterface(const char *name = "InInterface") : RNS::InterfaceImpl(name) {
		_OUT = false;
		_IN = true;
	}
	virtual ~InInterface() {
		_name = "(deleted)";
	}
	// Incoming-only interface; send_outgoing is never called on this side.
	virtual bool send_outgoing(const RNS::Bytes &data) { return true; }
};

class OutInterface : public RNS::InterfaceImpl {
public:
	OutInterface(RNS::Interface& in_interface, const char *name = "OutInterface") : RNS::InterfaceImpl(name), _in_interface(in_interface) {
		_OUT = true;
		_IN = false;
	}
	virtual ~OutInterface() {
		_name = "(deleted)";
	}
	virtual bool send_outgoing(const RNS::Bytes &data) {
		// Loop data straight back to InInterface to prove the packet path
		// end-to-end without any actual hardware transport.
		_in_interface.handle_incoming(data);
		InterfaceImpl::handle_outgoing(data);
		return true;
	}
private:
	RNS::Interface& _in_interface;
};

class ExampleAnnounceHandler : public RNS::AnnounceHandler {
public:
	ExampleAnnounceHandler(const char* aspect_filter = nullptr) : AnnounceHandler(aspect_filter) {}
	virtual ~ExampleAnnounceHandler() {}
	virtual void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity, const RNS::Bytes& app_data) {
		INFOF("Received announce, destination hash: %s", destination_hash.toHex().c_str());
		if (app_data) {
			INFOF("Received announce, app data: \"%s\"", app_data.toString().c_str());
		}
	}
};

RNS::Reticulum reticulum({RNS::Type::NONE});
RNS::Interface in_interface(new InInterface());
RNS::Interface out_interface(new OutInterface(in_interface));
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

double last_announce = 0.0;

void setup() {
#ifdef ARDUINO
	Serial.begin(115200);
	delay(2000); // give the USB CDC / serial monitor time to attach
	Serial.print("Hello from Nucleo-WL55JC1 on PlatformIO!\n");
#endif

	RNS::loglevel(RNS::LOG_INFO);

	HEAD("Registering loopback Interface instances with Transport...", RNS::LOG_INFO);
	RNS::Transport::register_interface(in_interface);
	RNS::Transport::register_interface(out_interface);

	HEAD("Creating Reticulum instance...", RNS::LOG_INFO);
	reticulum = RNS::Reticulum();
	reticulum.transport_enabled(false);
	reticulum.start();

	HEAD("Creating Identity and Destination instances...", RNS::LOG_INFO);
	identity = RNS::Identity();
	destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "smoketest");
	destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	RNS::Transport::register_announce_handler(announce_handler);

	HEAD("RNS Ready!", RNS::LOG_INFO);
}

void loop() {
	reticulum.loop();

	if ((RNS::Utilities::OS::time() - last_announce) > 10) {
		destination.announce();
		last_announce = RNS::Utilities::OS::time();
	}
}
