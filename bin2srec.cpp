#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "argparse.hpp"

#include "srec/srec.h"
#include "srec/crc32.h"

void convert_bin_to_srec(std::ifstream &input, SrecFile &sfile, bool want_checksum);
void write_checksum(const SrecFile &srecfile, unsigned int sum);

// Convert a binary file to a Srecord file
void convert_bin_to_srec(std::ifstream &input, SrecFile &sfile, const bool want_checksum) {
	// Get the max number the Srecord can store
	unsigned int bytes_to_read = sfile.max_data_bytes_per_record();
	// Buffer to store data from input file
	std::vector<uint8_t> buffer(bytes_to_read);

	// CRC32 checksum
	unsigned int sum = 0;

	// Read input file and write to Srecord file
	while (input.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || input.gcount() > 0) {
		// resize buffer in case the last read was less than the 'bytes_to_read'
		buffer.resize(input.gcount());

		sfile.write_record_payload(buffer);
		sum = xcrc32(buffer.data(), buffer.size(), sum);
		buffer.resize(bytes_to_read);
	}

	// Write record count and termination
	sfile.write_record_count();
	sfile.write_record_termination();

	sfile.close();
	input.close();

	// Write checksum if requested as the first line in the Srecord file
	if (want_checksum) {
		write_checksum(sfile, sum);
	}
}

void write_checksum(const SrecFile &srecfile, const unsigned int sum) {
	// Open a temp file
	std::string tempfilename = srecfile.getFilename() + ".tmp";
	SrecFile sfile(tempfilename, srecfile.addrsize());
	if (!sfile.is_open()) {
		std::cerr << "Error opening output file" << std::endl;
		throw std::ios_base::failure("Error opening output file: " + tempfilename);
	}

	// Convert crc32 to byte vector
	std::vector<uint8_t> crc32bytes;
	crc32bytes.push_back((sum >> 24) & 0xFF);
	crc32bytes.push_back((sum >> 16) & 0xFF);
	crc32bytes.push_back((sum >> 8) & 0xFF);
	crc32bytes.push_back(sum & 0xFF);
	crc32bytes.push_back(0); // null

	// Write header
	sfile.write_header(crc32bytes);
	sfile.close();

	// Now append the original file to the temp file
	std::ifstream ifs(srecfile.getFilename(), std::ios::binary);
	std::ofstream ofs(tempfilename, std::ios::binary | std::ios::app);
	ofs << ifs.rdbuf();
	ifs.close();
	ofs.close();

	// Rename the temp file to the original file
	std::rename(tempfilename.c_str(), srecfile.getFilename().c_str());
}

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
	SrecFile::AddressSize addrsize;
	switch (parser.get<int>("--addrbits")) {
		case 16:
			addrsize = SrecFile::AddressSize::BITS16;
			break;
		case 24:
			addrsize = SrecFile::AddressSize::BITS24;
			break;
		case 32:
			addrsize = SrecFile::AddressSize::BITS32;
			break;
		default:
			std::cerr << "Invalid address size" << std::endl;
			return 1;
	}

	// Open output file
	SrecFile sfile(outputfilename, addrsize);
	if (!sfile.is_open()) {
		std::cerr << "Error opening output file" << std::endl;
		return 1;
	}

	convert_bin_to_srec(input, sfile, parser.get<bool>("--checksum"));

	return 0;
}

