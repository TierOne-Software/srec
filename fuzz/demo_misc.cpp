// Demos:
//  1. convert_stream(buffer_size=0): loops writing empty records until MAX_RECORD_COUNT.
//  2. convert_srec_to_bin lax hex parsing: "1Z" silently becomes byte 0x01 instead of an error.
#include "srec/srec.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>

int main() {
	// --- Demo 2 first (fast): lax hex parse
	{
		std::ofstream ofs("/tmp/lax.srec");
		ofs << "S10500001Z00\n"; // count=5, addr=0000, data="1Z", checksum="00" (all bogus)
		ofs.close();
		try {
			tierone::srec::convert_srec_to_bin("/tmp/lax.srec", "/tmp/lax.bin");
			std::ifstream ifs("/tmp/lax.bin", std::ios::binary);
			int b = ifs.get();
			std::cout << "[lax-parse] input data field \"1Z\" converted to byte 0x" << std::hex << b
			          << " with NO error (expected: parse error)\n";
		} catch (const std::exception &e) {
			std::cout << "[lax-parse] rejected: " << e.what() << "\n";
		}
	}

	// --- Demo 1: buffer_size=0 loops
	{
		std::istringstream input("HELLO");
		auto t0 = std::chrono::steady_clock::now();
		try {
			tierone::srec::SrecStreamConverter::convert_stream(
				input, "/tmp/zero.srec", tierone::srec::SrecFile::AddressSize::BITS32,
				0, false, nullptr, /*buffer_size=*/0);
			std::cout << "[buf-0] returned normally (unexpected)\n";
		} catch (const std::exception &e) {
			auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - t0).count();
			std::ifstream ifs("/tmp/zero.srec");
			size_t lines = 0;
			std::string l;
			while (std::getline(ifs, l)) ++lines;
			std::cout << "[buf-0] threw after " << dt << " ms, output file has " << lines
			          << " lines for a 5-byte input: " << e.what() << "\n";
		}
	}
	return 0;
}
