// Minimal STM32 Nucleo-WL55JC1 bring-up smoke test.
//
// No filesystem -- just a loopback Interface pair (same pattern as
// test/test_rns_loopback) proving Reticulum, Transport, and the packet path
// initialize and keep running on real hardware. Watch the serial monitor at
// 115200 baud: a periodic announce should loop back and be reported by
// ExampleAnnounceHandler every ~10 seconds with no reset/crash.
//
// Also brings up the chip's onboard sub-GHz LoRa radio (construct + begin())
// as a standalone step, independent of the loopback Reticulum wiring above --
// this is NOT yet a working LoRa Interface (nothing transmits/receives
// Reticulum traffic over it). It only proves the radio hardware itself
// initializes correctly on this board. Wiring it into a real Interface (the
// way examples/common/lora_interface/LoRaInterface.{h,cpp} does for
// ESP32/nRF52 boards with external SX126x/SX127x modules) is the next step.

#include <microReticulum.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <RadioLib.h>

// No pins to configure -- the radio is on-die, signals route internally.
STM32WLx radio = new STM32WLx_Module();

// RF switch control table for the Nucleo-WL55JC1 specifically (per RadioLib's
// own STM32WLx examples). Other STM32WL boards/standalone modules may wire
// their RF switch differently and need a different table.
static const uint32_t rfswitch_pins[] = {PC3, PC4, PC5, RADIOLIB_NC, RADIOLIB_NC};
static const Module::RfSwitchMode_t rfswitch_table[] = {
	{STM32WLx::MODE_IDLE,  {LOW,  LOW,  LOW}},
	{STM32WLx::MODE_RX,    {HIGH, HIGH, LOW}},
	{STM32WLx::MODE_TX_LP, {HIGH, HIGH, HIGH}},
	{STM32WLx::MODE_TX_HP, {HIGH, LOW,  HIGH}},
	END_OF_MODE_TABLE,
};
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
#ifdef ARDUINO
		// Plain Serial output, not RNS::Log -- RNS_LOG_LEVEL is compiled down
		// to ERROR-only on this build (256KB flash is tight once the radio is
		// linked in), so INFO-level library log macros are stripped entirely.
		Serial.print("Announce: ");
		Serial.println(destination_hash.toHex().c_str());
		if (app_data) {
			Serial.print("  app data: ");
			Serial.println(app_data.toString().c_str());
		}
#endif
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
	Serial.print("Hello from Nucleo-WL55JC1!\n");
#endif

	// RNS_LOG_LEVEL is compiled down to ERROR-only on this build (256KB
	// flash is tight once the radio is linked in), so the runtime level set
	// here only affects what's left (errors/critical) -- HEAD/INFO calls
	// below don't compile at all. Progress below is reported via plain
	// Serial output instead so it survives regardless.
	RNS::loglevel(RNS::LOG_INFO);

#ifdef ARDUINO
	Serial.print("Init radio...\n");
	// Must be set before begin() so the radio knows which TX modes (LP/HP)
	// are wired up on this board.
	radio.setRfSwitchTable(rfswitch_pins, rfswitch_table);
	// EU868, default BW/SF/CR; 1.7V TCXO is what the Nucleo-WL55JC1 uses.
	int radio_state = radio.begin(868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 1.7, false);
	if (radio_state == RADIOLIB_ERR_NONE) {
		Serial.print("Radio OK.\n");
	} else {
		Serial.print("Radio init failed, code ");
		Serial.println(radio_state);
	}
#endif

#ifdef ARDUINO
	Serial.print("Registering interfaces...\n");
#endif
	RNS::Transport::register_interface(in_interface);
	RNS::Transport::register_interface(out_interface);

#ifdef ARDUINO
	Serial.print("Starting Reticulum...\n");
#endif
	reticulum = RNS::Reticulum();
	reticulum.transport_enabled(false);
	reticulum.start();

#ifdef ARDUINO
	Serial.print("Creating identity/destination...\n");
#endif
	identity = RNS::Identity();
	destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "smoketest");
	destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

	RNS::Transport::register_announce_handler(announce_handler);

#ifdef ARDUINO
	Serial.print("Ready!\n");
#endif
}

void loop() {
	reticulum.loop();

	if ((RNS::Utilities::OS::time() - last_announce) > 10) {
		destination.announce();
		last_announce = RNS::Utilities::OS::time();
	}
}
