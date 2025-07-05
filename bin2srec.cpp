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

#include "argparse.hpp"

#include "srec/srec.h"
#include "srec/crc32.h"


int main(int argc, char *argv[]) {
	std::string outputfilename;
	std::string inputfilename;

	// Define arguments
	argparse::ArgumentParser parser("bin2srec");
	parser.add_argument("-i", "--input")
		.help("Input file name");
	parser.add_argument("-o", "--output")
		.help("Output file name")
		.default_value("output.srec");
	parser.add_argument("-b", "--addrbits")
		.help("Address bits, 16, 24, or 32")
		.default_value(32)
		.nargs(1)
		.scan<'i', int>();
	parser.add_argument("-c", "--checksum")
		.help("Add a CRC32 checksum as the first S0 record")
		.default_value(false)
		.implicit_value(true);

	// Parse arguments
	try {
		parser.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Parsing command line arguments failed" << std::endl;
		std::cerr << err.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	// Check if input file is specified
	try {
		inputfilename = parser.get<std::string>("--input");
	} catch (const std::exception &err) {
		std::cerr << "No input file specified: " << err.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	// Check if output file is specified
	try {
		outputfilename = parser.get<std::string>("--output");
	} catch (const std::exception &err) {
		std::cerr << "Error getting output filename: " << err.what() << std::endl;
		std::cerr << parser;
		return 1;
	}

	// Open input file
	std::ifstream input(inputfilename, std::ios::binary);
	if (!input.is_open()) {
		std::cerr << "Error opening input file" << std::endl;
		return 1;
	}

	// Get address size
	tierone::srec::SrecFile::AddressSize addrsize;
	switch (parser.get<int>("--addrbits")) {
		case 16:
			addrsize = tierone::srec::SrecFile::AddressSize::BITS16;
			break;
		case 24:
			addrsize = tierone::srec::SrecFile::AddressSize::BITS24;
			break;
		case 32:
			addrsize = tierone::srec::SrecFile::AddressSize::BITS32;
			break;
		default:
			std::cerr << "Invalid address size" << std::endl;
			return 1;
	}

	// Open output file
	tierone::srec::SrecFile sfile(outputfilename, addrsize);
	if (!sfile.is_open()) {
		std::cerr << "Error opening output file" << std::endl;
		return 1;
	}

	try {
		tierone::srec::convert_bin_to_srec(input, sfile, parser.get<bool>("--checksum"));
	} catch (const std::exception &err) {
		std::cerr << "Error converting binary file: " << err.what() << std::endl;
	}

	return 0;
}

