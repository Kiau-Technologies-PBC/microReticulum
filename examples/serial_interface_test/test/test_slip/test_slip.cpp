#include <unity.h>

#include <vector>

#include "SerialInterface.h"

using namespace RNS;

void setUp(void) {}
void tearDown(void) {}

void test_slip_encode_wraps_with_end_bytes() {
	uint8_t raw[] = {0x01, 0x02, 0x03};
	Bytes payload(raw, sizeof(raw));

	Bytes framed = SerialInterface::slip_encode(payload);

	TEST_ASSERT_EQUAL_UINT32(payload.size() + 2, framed.size());
	TEST_ASSERT_EQUAL_UINT8(SerialInterface::SLIP_END, framed.data()[0]);
	TEST_ASSERT_EQUAL_UINT8(SerialInterface::SLIP_END, framed.data()[framed.size() - 1]);
	for (size_t i = 0; i < payload.size(); i++) {
		TEST_ASSERT_EQUAL_UINT8(payload.data()[i], framed.data()[i + 1]);
	}
}

void test_slip_roundtrip_escapes_special_bytes() {
	// Payload deliberately contains raw SLIP_END and SLIP_ESC bytes to prove
	// the escaping round-trips correctly rather than corrupting/truncating
	// the frame.
	uint8_t raw[] = {0x00, SerialInterface::SLIP_END, 0xFF, SerialInterface::SLIP_ESC, 0x7E};
	Bytes payload(raw, sizeof(raw));

	Bytes framed = SerialInterface::slip_encode(payload);

	SerialInterface iface;
	Bytes decoded;
	bool got_frame = false;
	for (size_t i = 0; i < framed.size(); i++) {
		if (iface.slip_decode_byte(framed.data()[i], decoded)) {
			got_frame = true;
		}
	}

	TEST_ASSERT_TRUE(got_frame);
	TEST_ASSERT_TRUE(decoded == payload);
}

void test_slip_decodes_back_to_back_frames() {
	uint8_t raw1[] = {0x11, 0x22};
	uint8_t raw2[] = {0x33, SerialInterface::SLIP_END, 0x44};
	Bytes payload1(raw1, sizeof(raw1));
	Bytes payload2(raw2, sizeof(raw2));

	Bytes framed;
	framed.append(SerialInterface::slip_encode(payload1));
	framed.append(SerialInterface::slip_encode(payload2));

	SerialInterface iface;
	std::vector<Bytes> received;
	for (size_t i = 0; i < framed.size(); i++) {
		Bytes decoded;
		if (iface.slip_decode_byte(framed.data()[i], decoded)) {
			received.push_back(decoded);
		}
	}

	TEST_ASSERT_EQUAL_UINT32(2, received.size());
	TEST_ASSERT_TRUE(received[0] == payload1);
	TEST_ASSERT_TRUE(received[1] == payload2);
}

int runUnityTests(void) {
	UNITY_BEGIN();
	RUN_TEST(test_slip_encode_wraps_with_end_bytes);
	RUN_TEST(test_slip_roundtrip_escapes_special_bytes);
	RUN_TEST(test_slip_decodes_back_to_back_frames);
	return UNITY_END();
}

int main(void) {
	return runUnityTests();
}
