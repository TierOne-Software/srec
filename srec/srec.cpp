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

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

#include "srec.h"
#include "crc32.h"

namespace tierone::srec {

// Convert a std::string to a hex string
std::string ASCIIToHexString(const std::string &buffer) {
	// Pre-allocate result string for better performance
	std::string result;
	result.reserve(buffer.size() * 2);
	
	std::stringstream ss;
	ss << std::uppercase << std::hex << std::setfill('0');
	for (const auto &c : buffer) {
		ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(c));
	}
	// Convert the string stream to a std::string and return it
	return ss.str();
}

// Convert a binary file to a Srecord file
void convert_bin_to_srec(std::ifstream &input, SrecFile &sfile, const bool want_checksum) {
	// Get the max number the Srecord can store
	unsigned int bytes_to_read = sfile.max_data_bytes_per_record();
	// Buffer to store data from input file
	std::vector<uint8_t> buffer(bytes_to_read);

	// CRC32 checksum
	unsigned int sum = 0;

	// Read input file and write to Srecord file
	while (input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())) || input.gcount() > 0) {
		// resize buffer in case the last read was less than the 'bytes_to_read'
		buffer.resize(static_cast<size_t>(input.gcount()));

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
		throw std::ios_base::failure("Error opening output file: " + tempfilename);
	}

	// Convert crc32 to byte vector
	std::vector<uint8_t> crc32bytes;
	crc32bytes.push_back(static_cast<uint8_t>((sum >> 24) & 0xFF));
	crc32bytes.push_back(static_cast<uint8_t>((sum >> 16) & 0xFF));
	crc32bytes.push_back(static_cast<uint8_t>((sum >> 8) & 0xFF));
	crc32bytes.push_back(static_cast<uint8_t>(sum & 0xFF));
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

void convert_srec_to_bin(const std::string &input_file, const std::string &output_file) {
	std::ifstream input(input_file);
	std::ofstream output(output_file, std::ios::binary);

	if (!input.is_open()) {
		throw std::ios_base::failure("Failed to open input file");
	}

	if (!output.is_open()) {
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
				//std::cerr << "Failed to parse hex data: " << e.what() << std::endl;
				throw std::ios_base::failure("Failed to parse hex data: " + std::string(e.what()));
			}
		}
		output.flush();
	}

	input.close();
	output.close();
}

// Parse an S-record string and return an Srec objec
SrecFile::SrecFile(const std::string &file_name, SrecFile::AddressSize address_size, unsigned int start_address)
	: filename(file_name),
	  address(start_address),
	  exec_address(start_address),
	  address_size_bits(address_size)
{
	file.open(filename, std::ios::trunc | std::ios::in | std::ios::out);
}

SrecFile::~SrecFile() {
	file.close();
}

void SrecFile::close() {
	file.flush();
	file.close();
}

bool SrecFile::is_open() {
	return file.is_open();
}

unsigned int SrecFile::max_data_bytes_per_record() const {
	switch (address_size_bits) {
		case AddressSize::BITS16:
			return 255 - 1 - 4 - 1; // max data - byte_count -  address_width - checksum
		case AddressSize::BITS24:
			return 255 - 1 - 6 - 1; // max data - byte_count -  address_width - checksum
		case AddressSize::BITS32:
			return 255 - 1 - 8 - 1; // max data - byte_count -  address_width - checksum
		default:
			return 0;
	}
}

// Write record data (S1/S2/S3) to file
void SrecFile::write_record_payload(const std::vector<uint8_t> &buffer) {
	if (!this->file.is_open()) {
		throw SrecFileException("File is not open", this->filename);
	}
	
	// Check security limits
	if (record_count >= MAX_RECORD_COUNT) {
		throw SrecValidationException(
			"Maximum record count exceeded",
			SrecValidationException::ValidationError::DATA_TOO_LARGE
		);
	}
	
	// Check if adding this buffer would cause address overflow
	if (buffer.size() > 0 && address > UINT32_MAX - buffer.size()) {
		throw SrecAddressException(address + buffer.size(), UINT32_MAX);
	}

	// Create the record type based on the address size
	std::unique_ptr<Srec> record_type;
	switch (address_size_bits) {
		case AddressSize::BITS16:
			record_type = std::make_unique<Srec1>(address, buffer);
			break;
		case AddressSize::BITS24:
			record_type = std::make_unique<Srec2>(address, buffer);
			break;
		case AddressSize::BITS32:
			record_type = std::make_unique<Srec3>(address, buffer);
			break;
		default:
			throw SrecValidationException("Invalid address size", SrecValidationException::ValidationError::INVALID_FORMAT);
	}
	// Write the record to the file
	this->file << record_type->toString() << std::endl;
	this->file.flush();

	// Update the record count and address
	this->record_count++;
	this->address += buffer.size();
}

// Write record count (S5/S6) to file
void SrecFile::write_record_count() {
	if (!this->file.is_open()) {
		throw SrecFileException("File is not open", this->filename);
	}

	if (this->record_count > 0xFFFFFF) {
		throw SrecValidationException(
			"Record count exceeds maximum of 16777215 (0xFFFFFF)",
			SrecValidationException::ValidationError::DATA_TOO_LARGE
		);
	}

	// Create the record type based on the record count
	std::unique_ptr<Srec> record;
	if (this->record_count <= 0xFFFF) {
		record = std::make_unique<Srec5>(this->record_count);
	} else {
		record = std::make_unique<Srec6>(this->record_count);
	}

	// Write the record to the file
	this->file << record->toString() << std::endl;
	this->file.flush();
}

// Write record termination (S7/S8/S9) to file
void SrecFile::write_record_termination() {
	if (!this->file.is_open()) {
		throw SrecFileException("File is not open", this->filename);
	}

	std::unique_ptr<Srec> record;
	switch (address_size_bits) {
		case AddressSize::BITS16:
			record = std::make_unique<Srec9>(exec_address);
			break;
		case AddressSize::BITS24:
			record = std::make_unique<Srec8>(exec_address);
			break;
		case AddressSize::BITS32:
			record = std::make_unique<Srec7>(exec_address);
			break;
		default:
			throw SrecValidationException("Invalid address size", SrecValidationException::ValidationError::INVALID_FORMAT);
	}
	// Write the record to the file
	this->file << record->toString() << std::endl;
	this->file.flush();
}

void SrecFile::write_header(const std::vector<std::string> &header_data) {
	if (!this->file.is_open()) {
		throw SrecFileException("File is not open", this->filename);
	}

	// Write the header data to the file
	for (const std::string &line : header_data) {
		std::string hexStr = ASCIIToHexString(line);
		auto record = Srec0(hexStr);
		this->file << record.toString() << std::endl;
		this->file.flush();
	}
}

void SrecFile::write_header(const std::vector<uint8_t> &header_data) {
	if (!this->file.is_open()) {
		throw SrecFileException("File is not open", this->filename);
	}

	// Write the header data to the file
	auto record = Srec0(header_data);
	this->file << record.toString() << std::endl;
	this->file.flush();
}

} // namespace tierone::srec
