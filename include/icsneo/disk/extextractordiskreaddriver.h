#ifndef __EXTEXTRACTORDISKREADDRIVER_H__
#define __EXTEXTRACTORDISKREADDRIVER_H__

#ifdef __cplusplus

#include "icsneo/disk/diskreaddriver.h"
#include <limits>
#include <chrono>

namespace icsneo {

namespace Disk {

/**
 * A disk read driver which uses the extended extractor command set to read from the disk
 */
class ExtExtractorDiskReadDriver : public ReadDriver {
public:
	std::pair<uint32_t, uint32_t> getBlockSizeBounds() const override {
		static_assert(SectorSize <= std::numeric_limits<uint32_t>::max(), "Incorrect sector size");
		static_assert(SectorSize >= std::numeric_limits<uint32_t>::min(), "Incorrect sector size");
		return { static_cast<uint32_t>(SectorSize), static_cast<uint32_t>(MaxSize) };
	}

private:
	static constexpr const uint32_t MaxSize = Disk::SectorSize * 512;
	static constexpr const std::chrono::seconds CacheTime = std::chrono::seconds(1);
	static constexpr const uint8_t HeaderLength = 7;

	std::array<uint8_t, MaxSize> cache;
	uint64_t cachePos = 0;
	std::chrono::time_point<std::chrono::steady_clock> cachedAt;

	Access getPossibleAccess() const override { return Access::EntireCard; }

	optional<uint64_t> readLogicalDiskAligned(Communication& com, device_eventhandler_t report,
		uint64_t pos, uint8_t* into, uint64_t amount, std::chrono::milliseconds timeout) override;

	optional<uint64_t> attemptReadLogicalDiskAligned(Communication& com, device_eventhandler_t report,
		uint64_t pos, uint8_t* into, uint64_t amount, std::chrono::milliseconds timeout);
};

} // namespace Disk

} // namespace icsneo

#endif // __cplusplus
#endif // __EXTEXTRACTORDISKREADDRIVER_H__