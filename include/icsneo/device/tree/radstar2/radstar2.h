#ifndef __RADSTAR2_H_
#define __RADSTAR2_H_

#ifdef __cplusplus

#include "icsneo/device/device.h"
#include "icsneo/device/devicetype.h"
#include "icsneo/device/tree/radstar2/radstar2settings.h"

namespace icsneo {

class RADStar2 : public Device {
public:
	// Serial numbers start with RS
	// USB PID is 0x0005, standard driver is FTDI
	// Ethernet MAC allocation is 0x05, standard driver is Raw
	ICSNEO_FINDABLE_DEVICE(RADStar2, DeviceType::RADStar2, "RS");

	static const std::vector<Network>& GetSupportedNetworks() {
		static std::vector<Network> supportedNetworks = {
			Network::NetID::HSCAN,
			Network::NetID::MSCAN,

			Network::NetID::LIN,

			Network::NetID::OP_Ethernet1,
			Network::NetID::OP_Ethernet2
		};
		return supportedNetworks;
	}

protected:
	RADStar2(neodevice_t neodevice, const driver_factory_t& makeDriver) : Device(neodevice) {
		initialize<RADStar2Settings>(makeDriver);
	}

	virtual void setupPacketizer(Packetizer& packetizer) override {
		Device::setupPacketizer(packetizer);
		packetizer.disableChecksum = true;
		packetizer.align16bit = false;
	}

	virtual void setupEncoder(Encoder& encoder) override {
		Device::setupEncoder(encoder);
		encoder.supportCANFD = true;
	}

	virtual void setupDecoder(Decoder& decoder) override {
		Device::setupDecoder(decoder);
		decoder.timestampResolution = 10; // Timestamps are in 10ns increments instead of the usual 25ns
	}

	void setupSupportedRXNetworks(std::vector<Network>& rxNetworks) override {
		for(auto& netid : GetSupportedNetworks())
			rxNetworks.emplace_back(netid);
	}

	// The supported TX networks are the same as the supported RX networks for this device
	void setupSupportedTXNetworks(std::vector<Network>& txNetworks) override { setupSupportedRXNetworks(txNetworks); }
};

}

#endif // __cplusplus

#endif