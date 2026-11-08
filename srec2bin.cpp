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


static int run(int argc, char *argv[]) {

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
		std::cerr << "Parsing command line arguments failed" << '\n';
		std::cerr << err.what() << '\n';
		std::cerr << program;
		return 1;
	}

	// Check if input file is specified
	if (!program.present("-i")) {
		std::cerr << "Input file is not specified" << '\n';
		std::cerr << program;
		return 1;
	}

	// Check if output file is specified
	if (!program.present("-o")) {
		std::cerr << "Output file is not specified" << '\n';
		std::cerr << program;
		return 1;
	}

	std::string input_file = program.get<std::string>("-i");
	std::string output_file = program.get<std::string>("-o");

    try {
		tierone::srec::convert_srec_to_bin(input_file, output_file);
	} catch (const std::exception &err) {
		std::cerr << "Error converting SREC file: " << err.what() << '\n';
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	try {
		return run(argc, argv);
	} catch (const std::exception &err) {
		std::cerr << "Unexpected error: " << err.what() << '\n';
		return 1;
	}
}
