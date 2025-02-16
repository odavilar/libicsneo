#include "icsneo/communication/ethernetpacketizer.h"
#include "icsneo/platform/optional.h"
#include "gtest/gtest.h"

using namespace icsneo;

#define MAC_SIZE (6)
static const uint8_t correctDeviceMAC[MAC_SIZE] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
static const uint8_t correctHostMAC[MAC_SIZE] = {0x12, 0x23, 0x34, 0x45, 0x56, 0x67};

class EthernetPacketizerTest : public ::testing::Test {
protected:
	// Start with a clean instance of the packetizer for every test
	void SetUp() override {
		onError = [](APIEvent::Type, APIEvent::Severity) {
			// Unless caught by the test, the packetizer should not throw errors
			EXPECT_TRUE(false);
		};
		packetizer.emplace([this](APIEvent::Type t, APIEvent::Severity s) {
			onError(t, s);
		});
		memcpy(packetizer->deviceMAC, correctDeviceMAC, MAC_SIZE);
		memcpy(packetizer->hostMAC, correctHostMAC, MAC_SIZE);
	}

	void TearDown() override {
		packetizer.reset();
	}

	optional<EthernetPacketizer> packetizer;
	device_eventhandler_t onError;
};

TEST_F(EthernetPacketizerTest, DownSmallSinglePacket)
{
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 1u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x09, 0x00, // 9 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
	}));
}

TEST_F(EthernetPacketizerTest, DownSmallMultiplePackets)
{
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	packetizer->inputDown({ 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee });
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 1u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x12, 0x00, // 18 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee
	}));
}

TEST_F(EthernetPacketizerTest, DownSmallMultiplePacketsOverflow)
{
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	packetizer->inputDown({ 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee });
	packetizer->inputDown(std::vector<uint8_t>(1480)); // Near the max
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 2u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x12, 0x00, // 18 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee
	}));
	std::vector<uint8_t> bigOutput({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0xc8, 0x05, // 1480 bytes
		0x01, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
	});
	bigOutput.resize(1480 + 24);
	EXPECT_EQ(output.back().size(), bigOutput.size());
	EXPECT_EQ(output.back(), bigOutput);
}

TEST_F(EthernetPacketizerTest, DownOverflowSmallMultiplePackets)
{
	packetizer->inputDown(std::vector<uint8_t>(1486)); // Near the max, not enough room for the next packet
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	packetizer->inputDown({ 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee });
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 2u);
	std::vector<uint8_t> bigOutput({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0xce, 0x05, // 1486 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
	});
	bigOutput.resize(1486 + 24);
	EXPECT_EQ(output.front().size(), bigOutput.size());
	EXPECT_EQ(output.front(), bigOutput);
	EXPECT_EQ(output.back(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x12, 0x00, // 18 bytes
		0x01, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee
	}));
}

TEST_F(EthernetPacketizerTest, DownBigSmallSmall)
{
	packetizer->inputDown(std::vector<uint8_t>(1480)); // Near the max, enough room for the next packet
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	packetizer->inputDown({ 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee }); // Not enough room for this one
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 2u);
	std::vector<uint8_t> bigOutput({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0xd1, 0x05, // 1486 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
	});
	bigOutput.resize(1480 + 24);
	bigOutput.insert(bigOutput.end(), { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	EXPECT_EQ(output.front().size(), bigOutput.size());
	EXPECT_EQ(output.front(), bigOutput);
	EXPECT_EQ(output.back(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x9, 0x00, // 9 bytes
		0x01, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee
	}));
}

TEST_F(EthernetPacketizerTest, DownJumboSmallSmall)
{
	packetizer->inputDown(std::vector<uint8_t>(3000)); // Two full packets plus 20 bytes
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	packetizer->inputDown({ 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee });
	const auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 3u);
	std::vector<uint8_t> bigOutput({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0xd2, 0x05, // 1490 bytes
		0x00, 0x00, // packet number
		0x01, 0x01, // first piece, version 1
	});
	bigOutput.resize(1490 + 24); // Full packet
	EXPECT_EQ(output.front(), bigOutput);
	std::vector<uint8_t> bigOutput2({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0xd2, 0x05, // 1490 bytes
		0x00, 0x00, // packet number
		0x00, 0x01, // mid piece, version 1
	});
	bigOutput2.resize(1490 + 24); // Full packet
	EXPECT_EQ(output[1], bigOutput2);
	EXPECT_EQ(output.back(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x26, 0x00, // 38 bytes
		0x00, 0x00, // packet number
		0x02, 0x01, // last piece, version 1
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
		0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee
	}));
}

TEST_F(EthernetPacketizerTest, PacketNumberIncrement)
{
	packetizer->inputDown({ 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	auto output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 1u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x09, 0x00, // 9 bytes
		0x00, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
	}));

	packetizer->inputDown({ 0x12, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 1u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x09, 0x00, // 9 bytes
		0x01, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x12, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
	}));

	packetizer->inputDown({ 0x13, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99 });
	output = packetizer->outputDown();
	ASSERT_EQ(output.size(), 1u);
	EXPECT_EQ(output.front(), std::vector<uint8_t>({
		0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
		0x12, 0x23, 0x34, 0x45, 0x56, 0x67,
		0xca, 0xb1,
		0xaa, 0xaa, 0x55, 0x55,
		0x09, 0x00, // 9 bytes
		0x02, 0x00, // packet number
		0x03, 0x01, // first and last piece, version 1
		0x13, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
	}));
}