// Differential roundtrip harness: random bytes -> SREC -> bytes.
// Any data loss or corruption in the writer/reader pair is a finding.
#include "srec/srec.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

static std::string fname(const char *tag, unsigned counter) {
	return std::string("/tmp/fuzz_rt_") + tag + "_" + std::to_string(getpid()) + "_" + std::to_string(counter);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size < 2) {
		return 0;
	}
	static unsigned counter = 0;
	const uint8_t flags = data[0];
	++data;
	--size;

	const bool want_checksum = flags & 1;
	tierone::srec::SrecFile::AddressSize addr_size =
		(flags & 2) ? tierone::srec::SrecFile::AddressSize::BITS32
		: (flags & 4) ? tierone::srec::SrecFile::AddressSize::BITS24
		: tierone::srec::SrecFile::AddressSize::BITS16;

	std::string srecpath = fname("out", counter);
	std::string binpath = fname("back", counter);
	++counter;

	try {
		std::istringstream input(std::string(reinterpret_cast<const char *>(data), size));
		tierone::srec::SrecStreamConverter::convert_stream(input, srecpath, addr_size, 0, want_checksum,
			nullptr, 64);
		tierone::srec::convert_srec_to_bin(srecpath, binpath);
	} catch (const std::exception &) {
		std::remove(srecpath.c_str());
		std::remove(binpath.c_str());
		return 0; // writer/reader rejected the input; not a finding
	}

	// Compare roundtripped bytes with original
	std::ifstream back(binpath, std::ios::binary);
	std::vector<uint8_t> got((std::istreambuf_iterator<char>(back)), std::istreambuf_iterator<char>());
	if (got.size() != size || (size > 0 && std::memcmp(got.data(), data, size) != 0)) {
		__builtin_trap(); // data corrupted by the roundtrip
	}

	std::remove(srecpath.c_str());
	std::remove(binpath.c_str());
	return 0;
}
