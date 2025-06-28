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

#include "argparse.hpp"
#include "srec/srec.h"

void convert_srec_to_bin(const std::string &input_file, const std::string &output_file) {
	std::ifstream input(input_file);
	std::ofstream output(output_file, std::ios::binary);

	if (!input.is_open()) {
		std::cerr << "Failed to open input file: " << input_file << std::endl;
		throw std::ios_base::failure("Failed to open input file");
	}

	if (!output.is_open()) {
		std::cerr << "Failed to open output file: " << output_file << std::endl;
		throw std::ios_base::failure("Failed to open output file");
	}

	std::string line;
	while (std::getline(input, line)) {
		if (line.empty()) {
			continue;
		}

		if (line[0] != 'S') {
			continue;
		}
		if (line[1] != '1' && line[1] != '2' && line[1] != '3') {
			continue;
		}
		std::string data;
		if (line[1] == '1') {
			data = line.substr(4+(Srec1::ADDRESS_SIZE*2)); // cut off S#, byte_count, and address
			data = data.substr(0, data.size() - 2); // cut off checksum
		} else if (line[1] == '2') {
			data = line.substr(4+(Srec2::ADDRESS_SIZE*2)); // cut off S#, byte_count, and address
			data = data.substr(0, data.size() - 2); // cut off checksum
		} else if (line[1] == '3') {
			data = line.substr(4+(Srec3::ADDRESS_SIZE*2));	// cut off S#, byte_count, and address
			data = data.substr(0, data.size() - 2); // cut off checksum
		}
		// walk through the data string and covert each ASCII pair to a byte
		// and write it to the output file
		// e.g. "010203" -> 0x01, 0x02, 0x03
		for (size_t i = 0; i < data.size(); i += 2) {
			try {
				auto byte = static_cast<uint8_t>(std::stoi(data.substr(i, 2), nullptr, 16));
				output.write(reinterpret_cast<const char *>(&byte), 1);
			} catch (const std::exception &e) {
				std::cerr << "Failed to parse hex data: " << e.what() << std::endl;
			}
		}
		output.flush();
	}

	input.close();
	output.close();
}

int main(int argc, char *argv[]) {

	// Define arguments
	argparse::ArgumentParser program("srec2bin");
	program.add_argument("-i", "--input")
		.help("Input file in SREC format");
	program.add_argument("-o", "--output")
		.help("Output file in binary format");

	// Parse arguments
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Parsing command line arguments failed" << std::endl;
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		return 1;
	}

	// Check if input file is specified
	if (!program.present("-i")) {
		std::cerr << "Input file is not specified" << std::endl;
		std::cerr << program;
		return 1;
	}

	// Check if output file is specified
	if (!program.present("-o")) {
		std::cerr << "Output file is not specified" << std::endl;
		std::cerr << program;
		return 1;
	}

	std::string input_file = program.get<std::string>("-i");
	std::string output_file = program.get<std::string>("-o");

	convert_srec_to_bin(input_file, output_file);
	return 0;
}

