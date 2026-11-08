// libFuzzer harness for tierone::srec::SrecStreamParser.
// Contract under test: parse_line/parse_stream throw only SrecException
// subtypes. Any other escaping exception is reported as a fuzz finding.
#include "srec/srec.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string input(reinterpret_cast<const char *>(data), size);

	// Single-line parse, both checksum modes
	for (const bool validate : {false, true}) {
		try {
			auto rec = tierone::srec::SrecStreamParser::parse_line(input, 1, validate);
			volatile size_t sink = rec.data.size() + rec.address + rec.checksum + rec.line_number;
			(void)sink;
		} catch (const tierone::srec::SrecException &) {
			// expected for malformed input
		}
	}

	// Multi-line stream parse, both checksum modes
	for (const bool validate : {false, true}) {
		try {
			std::istringstream ss(input);
			tierone::srec::SrecStreamParser::parse_stream(
				ss,
				[](const tierone::srec::SrecStreamParser::ParsedRecord &r) {
					volatile size_t sink = r.data.size() + r.address + r.checksum;
					(void)sink;
					return true;
				},
				validate);
		} catch (const tierone::srec::SrecException &) {
			// expected for malformed input
		}
	}
	return 0;
}
