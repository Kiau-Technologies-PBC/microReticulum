// STM32 Nucleo-WL55JC1 node communicating over SerialInterface (SLIP-framed
// UART) instead of the loopback pair used in ../nucleo_smoketest.
//
// Flash this sketch to two Nucleo-WL55JC1 boards and cross-wire Serial1
// TX<->RX (and a common GND) between them. Both boards periodically announce
// a destination; each should see the other's announce reported by
// ExampleAnnounceHandler over its `Serial` (ST-LINK VCP) debug log.
//
// No radio, no filesystem -- just proves real inter-board Reticulum traffic
// over the one interface every bare Nucleo already has: a UART.

#include <SerialInterface.h>
#include <microReticulum.h>

#ifdef ARDUINO
#include <Arduino.h>
// The stock Nucleo-WL55JC1 variant only pre-declares one HardwareSerial
// (`Serial`, USART2 on PA2/PA3 -> the ST-LINK VCP) -- there's no built-in
// `Serial1`. USART1 is wired to D0/D1 (PB7=RX, PB6=TX) on the Nucleo-64
// header, so instantiate a second port there for Reticulum traffic and
// leave `Serial` free for the debug log.
HardwareSerial Serial1(PB7, PB6);
#endif

const char* fruits[] = {"Peach", "Quince", "Date", "Tangerine", "Pomelo", "Carambola", "Grape"};

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
RNS::Interface serial_interface({RNS::Type::NONE});
RNS::Identity identity({RNS::Type::NONE});
RNS::Destination destination({RNS::Type::NONE});
RNS::HAnnounceHandler announce_handler(new ExampleAnnounceHandler());

double last_announce = 0.0;

void reticulum_announce() {
	if (destination) {
		destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
	}
}

void setup() {
#ifdef ARDUINO
	Serial.begin(115200);
	delay(2000); // give the ST-LINK VCP / serial monitor time to attach
	Serial.print("Hello from Nucleo-WL55JC1 (SerialInterface node) on PlatformIO!\n");
#endif

	RNS::loglevel(RNS::LOG_INFO);

	HEAD("Registering SerialInterface with Transport...", RNS::LOG_INFO);
#ifdef ARDUINO
	serial_interface = new SerialInterface(Serial1, 115200);
#else
	serial_interface = new SerialInterface();
#endif
	RNS::Transport::register_interface(serial_interface);
	serial_interface.start();

	HEAD("Creating Reticulum instance...", RNS::LOG_INFO);
	reticulum = RNS::Reticulum();
	reticulum.transport_enabled(false);
	reticulum.probe_destination_enabled(true);
	reticulum.start();

	HEAD("Creating Identity and Destination instances...", RNS::LOG_INFO);
	identity = RNS::Identity();
	destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "serial_node");
	destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	RNS::Transport::register_announce_handler(announce_handler);

	HEAD("RNS Ready!", RNS::LOG_INFO);
}

void loop() {
	// Reticulum::loop() calls Interface::loop() on every registered
	// interface internally, so SerialInterface's RX polling happens here too.
	reticulum.loop();

	if ((RNS::Utilities::OS::time() - last_announce) > 10) {
		reticulum_announce();
		last_announce = RNS::Utilities::OS::time();
	}
}
