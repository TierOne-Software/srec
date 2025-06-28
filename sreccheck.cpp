#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

#include "argparse.hpp"
#include "srec/crc32.h"

int main(int argc, char *argv[]) {

	// Define arguments
	argparse::ArgumentParser program("sreccheck");
	program.add_argument("file").help("SREC file to check");
	program.add_argument("-v", "--verbose").help("Verbose mode").default_value(false).implicit_value(true);

	// Parse arguments
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Parsing command line arguments failed" << std::endl;
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	// Check if file is specified
	std::string srecfilename;
	try {
		srecfilename = program.get<std::string>("file");
	} catch (const std::exception &err) {
		std::cerr << "No file specified" << std::endl;
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	// Open file
	std::ifstream srecfile(srecfilename);
	if (!srecfile.is_open()) {
		std::cerr << "Failed to open file" << std::endl;
		return 1;
	}

	unsigned long found_crc = 0;
	unsigned long sum = 0; // calculated CRC
	std::string line;
	std::vector<uint8_t> buff;

	while (std::getline(srecfile, line)) {
		if (line[0] == 'S') {
			if (line[1] == '0') {
				// read the CRC, bytes 8-15
				try {
					found_crc = std::stoul(line.substr(8, 8), nullptr, 16);
				} catch (const std::out_of_range &err) {
					std::cerr << "Failed to parse CRC" << std::endl;
					std::cerr << err.what() << std::endl;
					return 1;
				}
				continue;
			}

			// determine the index to start at
			unsigned int start;
			if (line[1] == '1') {
				start = 4 /* S1 + byte count */ + 4 /* address */;
			} else if (line[1] == '2') {
				start = 4 /* S2 + byte count */ + 6 /* address */;
			} else if (line[1] == '3') {
				start = 4 /* S3 + byte count */ + 8 /* address */;
			} else {
				// not an S1/S2/S3 record, skip
				continue;
			}

			buff.clear();
			for (unsigned int i = start; i < line.size()-2/*skip checksum*/; i += 2) {
				buff.push_back(static_cast<uint8_t>(std::stoul(line.substr(i, 2), nullptr, 16)));
			}
			sum = xcrc32(buff.data(), buff.size(), sum);
		}
	}

	// Print results, if verbose flag is set
	if (program.get<bool>("verbose")) {
		std::cout << "Found CRC:       0x" << std::uppercase << std::hex << found_crc << std::endl;
		std::cout << "Calculated CRC:  0x" << std::uppercase << std::hex << sum << std::endl;
	}
	if (found_crc == sum) {
		if (program.get<bool>("verbose")) {
			std::cout << "CRC matches" << std::endl;
		}
		return 0;
	} else {
		std::cout << "CRC does not match" << std::endl;
		if (program.get<bool>("verbose")) {
			std::cerr << "CRC check failed. Expected: " << std::hex << found_crc << ", got 0x" << sum << std::endl;
		}
		return 1;
	}
}
