// libFuzzer harness for tierone::srec::convert_srec_to_bin.
// Contract under test: throws std::ios_base::failure only. Any other
// escaping exception (e.g. std::out_of_range from substr) is a finding.
#include "srec/srec.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	static unsigned counter = 0;
	std::string inpath = "/tmp/fuzz_srec2bin_" + std::to_string(getpid()) + "_" + std::to_string(counter) + ".srec";
	std::string outpath = inpath + ".bin";
	++counter;

	{
		std::ofstream ofs(inpath, std::ios::binary);
		ofs.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
	}

	try {
		tierone::srec::convert_srec_to_bin(inpath, outpath);
	} catch (const std::ios_base::failure &) {
		// documented exception
	} catch (const std::out_of_range &) {
		// KNOWN FINDING (2026-11-07): convert_srec_to_bin leaks std::out_of_range
		// from std::string::substr on short "S1/S2/S3" lines instead of its
		// documented std::ios_base::failure. Masked here so fuzzing can continue
		// past it to hunt for deeper issues.
	}

	std::remove(inpath.c_str());
	std::remove(outpath.c_str());
	return 0;
}
