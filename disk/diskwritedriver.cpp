#include "icsneo/disk/diskwritedriver.h"
#include <cstring>

using namespace icsneo;
using namespace icsneo::Disk;

const uint64_t WriteDriver::RetryAtomic = std::numeric_limits<uint64_t>::max();
const APIEvent::Severity WriteDriver::NonatomicSeverity = APIEvent::Severity::EventInfo;

optional<uint64_t> WriteDriver::writeLogicalDisk(Communication& com, device_eventhandler_t report, ReadDriver& readDriver,
	uint64_t pos, const uint8_t* from, uint64_t amount, std::chrono::milliseconds timeout) {
	if(amount == 0)
		return 0;

	optional<uint64_t> ret;

	const uint32_t idealBlockSize = getBlockSizeBounds().second;

	// Write from here if we need to read-modify-write a block
	// That would be the case either if we don't want some at the
	// beginning or end of the block.
	std::vector<uint8_t> alignedWriteBuffer;

	// Read to here, ideally this can be sent back to the device to
	// ensure an operation is atomic
	std::vector<uint8_t> atomicBuffer(idealBlockSize);

	pos += vsaOffset;
	const uint64_t startBlock = pos / idealBlockSize;
	const uint32_t posWithinFirstBlock = static_cast<uint32_t>(pos % idealBlockSize);
	uint64_t blocks = amount / idealBlockSize + (amount % idealBlockSize ? 1 : 0);
	if(blocks * idealBlockSize - posWithinFirstBlock < amount)
		blocks++; // We need one more block to get the last partial block's worth
	uint64_t blocksProcessed = 0;

	while(blocksProcessed < blocks && timeout >= std::chrono::milliseconds::zero()) {
		const uint64_t currentBlock = startBlock + blocksProcessed;

		uint64_t fromOffset = blocksProcessed * idealBlockSize;
		if(fromOffset < posWithinFirstBlock)
			fromOffset = 0;
		else
			fromOffset -= posWithinFirstBlock;

		const uint32_t posWithinCurrentBlock = (blocksProcessed ? 0 : posWithinFirstBlock);
		uint32_t curAmt = idealBlockSize - posWithinCurrentBlock;
		const auto amountLeft = amount - ret.value_or(0);
		if(curAmt > amountLeft)
			curAmt = static_cast<uint32_t>(amountLeft);

		auto start = std::chrono::high_resolution_clock::now();
		const auto reportFromRead = [&report, &blocksProcessed](APIEvent::Type t, APIEvent::Severity s) {
			if(t == APIEvent::Type::ParameterOutOfRange && blocksProcessed)
				t = APIEvent::Type::EOFReached;
			report(t, s);
		};
		auto bytesTransferred = readDriver.readLogicalDisk(com, reportFromRead, currentBlock * idealBlockSize,
			atomicBuffer.data(), idealBlockSize, timeout);
		timeout -= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

		if(bytesTransferred != idealBlockSize)
			break; // readLogicalDisk reports its own errors

		const bool useAlignedWriteBuffer = (posWithinCurrentBlock != 0 || curAmt != idealBlockSize);
		if(useAlignedWriteBuffer) {
			alignedWriteBuffer = atomicBuffer;
			memcpy(alignedWriteBuffer.data() + posWithinCurrentBlock, from + fromOffset, curAmt);
		}

		start = std::chrono::high_resolution_clock::now();
		bytesTransferred = writeLogicalDiskAligned(com, report, currentBlock * idealBlockSize, atomicBuffer.data(),
			useAlignedWriteBuffer ? alignedWriteBuffer.data() : (from + fromOffset), idealBlockSize, timeout);
		timeout -= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

		if(bytesTransferred == RetryAtomic) {
			// The user may want to log these events in order to see how many atomic misses they are getting
			report(APIEvent::Type::AtomicOperationRetried, APIEvent::Severity::EventInfo);
			continue;
		}

		if(!bytesTransferred.has_value() || *bytesTransferred < curAmt) {
			if(timeout < std::chrono::milliseconds::zero())
				report(APIEvent::Type::Timeout, APIEvent::Severity::Error);
			else
				report((blocksProcessed || bytesTransferred.value_or(0u) != 0u) ? APIEvent::Type::EOFReached :
					APIEvent::Type::ParameterOutOfRange, APIEvent::Severity::Error);
			break;
		}

		if(!ret)
			ret.emplace();
		*ret += std::min<uint64_t>(*bytesTransferred, curAmt);
		blocksProcessed++;
	}

	return ret;
}