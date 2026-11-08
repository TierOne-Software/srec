/*
* Copyright 2025 TierOne Software
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

#include "argparse.hpp"
#include "srec/srec.h"
#include "srec/crc32.h"

static int run(int argc, char *argv[]) {

	// Define arguments
	argparse::ArgumentParser program("sreccheck");
	program.add_argument("file").help("SREC file to check");
	program.add_argument("-v", "--verbose").help("Verbose mode").default_value(false).implicit_value(true);

	// Parse arguments
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Parsing command line arguments failed" << '\n';
		std::cerr << err.what() << '\n';
		std::cerr << program;
		return 1;
	}

	// Check if file is specified
	std::string srecfilename;
	try {
		srecfilename = program.get<std::string>("file");
	} catch (const std::exception &err) {
		std::cerr << "No file specified" << '\n';
		std::cerr << err.what() << '\n';
		std::cerr << program;
		return 1;
	}

	// Open file
	std::ifstream srecfile(srecfilename);
	if (!srecfile.is_open()) {
		std::cerr << "Failed to open file" << '\n';
		return 1;
	}
	srecfile.close();

	unsigned long found_crc = 0;
	unsigned long sum = 0; // calculated CRC

	// Parse the S-record file with the library parser. This validates hex
	// digits, byte counts, and record checksums; any error is reported and
	// exits non-zero instead of crashing on malformed input.
	try {
		tierone::srec::SrecStreamParser::parse_file(
			srecfilename,
			[&](const tierone::srec::SrecStreamParser::ParsedRecord &record) {
				using tierone::srec::Srec;
				if (record.type == Srec::Type::S0) {
					// The CRC32 is stored as the first four data bytes of the S0 record
					if (record.data.size() >= 4) {
						found_crc = (static_cast<unsigned long>(record.data[0]) << 24) |
						            (static_cast<unsigned long>(record.data[1]) << 16) |
						            (static_cast<unsigned long>(record.data[2]) << 8) |
						            static_cast<unsigned long>(record.data[3]);
					}
				} else if (record.type == Srec::Type::S1 ||
				           record.type == Srec::Type::S2 ||
				           record.type == Srec::Type::S3) {
					sum = tierone::srec::xcrc32(record.data.data(),
					                            static_cast<unsigned long>(record.data.size()),
					                            static_cast<unsigned int>(sum));
				}
				return true;
			});
	} catch (const std::exception &err) {
		std::cerr << "Invalid S-record file: " << err.what() << '\n';
		return 1;
	}

	// Print results, if verbose flag is set
	if (program.get<bool>("verbose")) {
		std::cout << "Found CRC:       0x" << std::uppercase << std::hex << found_crc << '\n';
		std::cout << "Calculated CRC:  0x" << std::uppercase << std::hex << sum << '\n';
	}
	if (found_crc == sum) {
		if (program.get<bool>("verbose")) {
			std::cout << "CRC matches" << '\n';
		}
		return 0;
	} else {
		std::cout << "CRC does not match" << '\n';
		if (program.get<bool>("verbose")) {
			std::cerr << "CRC check failed. Expected: " << std::hex << found_crc << ", got 0x" << sum << '\n';
		}
		return 1;
	}
}

int main(int argc, char *argv[]) {
	try {
		return run(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Unexpected error: " << err.what() << '\n';
		return 1;
	}
}
