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

// ============================================================================
// Streaming API Implementation
// ============================================================================

uint8_t SrecStreamParser::hex_char_to_byte(char c) {
	if (c >= '0' && c <= '9') {
		return static_cast<uint8_t>(c - '0');
	} else if (c >= 'A' && c <= 'F') {
		return static_cast<uint8_t>(c - 'A' + 10);
	} else if (c >= 'a' && c <= 'f') {
		return static_cast<uint8_t>(c - 'a' + 10);
	} else {
		throw SrecParseException("Invalid hex character: " + std::string(1, c));
	}
}

uint8_t SrecStreamParser::parse_hex_byte(const std::string &hex_str, size_t offset) {
	if (offset + 1 >= hex_str.size()) {
		throw SrecParseException("Incomplete hex byte at offset " + std::to_string(offset));
	}
	uint8_t high = hex_char_to_byte(hex_str[offset]);
	uint8_t low = hex_char_to_byte(hex_str[offset + 1]);
	return static_cast<uint8_t>((high << 4) | low);
}

Srec::Type SrecStreamParser::char_to_type(char type_char) {
	switch (type_char) {
		case '0': return Srec::Type::S0;
		case '1': return Srec::Type::S1;
		case '2': return Srec::Type::S2;
		case '3': return Srec::Type::S3;
		case '5': return Srec::Type::S5;
		case '6': return Srec::Type::S6;
		case '7': return Srec::Type::S7;
		case '8': return Srec::Type::S8;
		case '9': return Srec::Type::S9;
		default:
			throw SrecParseException("Invalid S-record type: " + std::string(1, type_char));
	}
}

SrecStreamParser::ParsedRecord SrecStreamParser::parse_line(const std::string &line, 
                                                           size_t line_number,
                                                           bool validate_checksum) {
	ParsedRecord record{};
	record.line_number = line_number;
	record.checksum_valid = false;

	// Skip empty lines and comments
	if (line.empty() || line[0] != 'S') {
		throw SrecParseException("Invalid S-record format", line_number);
	}

	// Minimum length: S + type + count (2 chars) + checksum (2 chars) = 6 chars
	if (line.length() < 6) {
		throw SrecParseException("S-record too short", line_number);
	}

	try {
		// Parse type
		record.type = char_to_type(line[1]);

		// Parse byte count
		uint8_t byte_count = parse_hex_byte(line, 2);
		
		// Validate record length
		// Total length = 'S' + type + count (2 chars) + (byte_count * 2 chars per byte)
		size_t expected_length = 4 + (byte_count * 2);
		if (line.length() != expected_length) {
			std::ostringstream oss;
			oss << "S-record length mismatch: expected " << expected_length 
			    << ", got " << line.length() 
			    << " (byte_count=" << static_cast<unsigned int>(byte_count) << ")"
			    << " at line " << line_number;
			throw SrecParseException(oss.str(), line_number);
		}

		// Parse address based on record type
		size_t address_bytes = 0;
		size_t data_start_offset = 4;
		
		switch (record.type) {
			case Srec::Type::S0:
				address_bytes = 2;
				record.address = (parse_hex_byte(line, 4) << 8) | parse_hex_byte(line, 6);
				data_start_offset = 8;
				break;
			case Srec::Type::S1:
			case Srec::Type::S9:
				address_bytes = 2;
				record.address = (parse_hex_byte(line, 4) << 8) | parse_hex_byte(line, 6);
				data_start_offset = 8;
				break;
			case Srec::Type::S2:
			case Srec::Type::S8:
				address_bytes = 3;
				record.address = (parse_hex_byte(line, 4) << 16) | 
				                (parse_hex_byte(line, 6) << 8) | 
				                parse_hex_byte(line, 8);
				data_start_offset = 10;
				break;
			case Srec::Type::S3:
			case Srec::Type::S7:
				address_bytes = 4;
				record.address = (parse_hex_byte(line, 4) << 24) | 
				                (parse_hex_byte(line, 6) << 16) |
				                (parse_hex_byte(line, 8) << 8) | 
				                parse_hex_byte(line, 10);
				data_start_offset = 12;
				break;
			case Srec::Type::S5:
				address_bytes = 2;
				record.address = (parse_hex_byte(line, 4) << 8) | parse_hex_byte(line, 6);
				data_start_offset = 8;
				break;
			case Srec::Type::S6:
				address_bytes = 3;
				record.address = (parse_hex_byte(line, 4) << 16) | 
				                (parse_hex_byte(line, 6) << 8) | 
				                parse_hex_byte(line, 8);
				data_start_offset = 10;
				break;
		}

		// Parse data bytes
		size_t data_bytes = byte_count - address_bytes - 1; // -1 for checksum
		record.data.reserve(data_bytes);
		
		for (size_t i = 0; i < data_bytes; ++i) {
			uint8_t byte_val = parse_hex_byte(line, data_start_offset + (i * 2));
			record.data.push_back(byte_val);
		}

		// Parse checksum
		size_t checksum_offset = data_start_offset + (data_bytes * 2);
		record.checksum = parse_hex_byte(line, checksum_offset);

		// Validate checksum if requested
		if (validate_checksum) {
			uint32_t sum = byte_count;
			
			// Add address bytes to sum
			for (size_t i = 0; i < address_bytes; ++i) {
				sum += parse_hex_byte(line, 4 + (i * 2));
			}
			
			// Add data bytes to sum
			for (uint8_t data_byte : record.data) {
				sum += data_byte;
			}
			
			uint8_t calculated_checksum = static_cast<uint8_t>(~sum & 0xFF);
			record.checksum_valid = (calculated_checksum == record.checksum);
			
			if (!record.checksum_valid) {
				std::ostringstream oss;
				oss << "Checksum validation failed on line " << line_number
				    << ": expected 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(2) 
				    << static_cast<unsigned int>(calculated_checksum)
				    << ", got 0x" << std::setw(2) << static_cast<unsigned int>(record.checksum);
				throw SrecValidationException(
					oss.str(),
					SrecValidationException::ValidationError::CHECKSUM_MISMATCH
				);
			}
		} else {
			record.checksum_valid = true; // Assume valid when not validating
		}

	} catch (const SrecException &) {
		throw; // Re-throw our exceptions
	} catch (const std::exception &e) {
		throw SrecParseException("Parse error on line " + std::to_string(line_number) + 
		                        ": " + std::string(e.what()), line_number);
	}

	return record;
}

void SrecStreamParser::parse_stream(std::istream &input_stream, 
                                   RecordCallback callback,
                                   bool validate_checksums) {
	std::string line;
	size_t line_number = 0;
	
	while (std::getline(input_stream, line)) {
		++line_number;
		
		// Skip empty lines
		if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
			continue;
		}
		
		// Remove trailing whitespace
		line.erase(line.find_last_not_of(" \t\r\n") + 1);
		
		try {
			ParsedRecord record = parse_line(line, line_number, validate_checksums);
			
			// Call user callback
			if (!callback(record)) {
				break; // User requested to stop parsing
			}
		} catch (const SrecParseException &e) {
			// Re-throw with line context if not already included
			if (e.getLineNumber() == 0) {
				throw SrecParseException(e.what(), line_number);
			} else {
				throw;
			}
		}
	}
	
	if (input_stream.bad()) {
		throw SrecFileException("Stream read error", "");
	}
}

void SrecStreamParser::parse_file(const std::string &filename,
                                 RecordCallback callback,
                                 bool validate_checksums) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw SrecFileException("Failed to open file", filename);
	}
	
	try {
		parse_stream(file, callback, validate_checksums);
	} catch (const SrecException &) {
		file.close();
		throw;
	}
	
	file.close();
}

void SrecStreamConverter::convert_stream(std::istream &input,
                                        const std::string &output_filename,
                                        SrecFile::AddressSize address_size,
                                        uint32_t start_address,
                                        bool want_checksum,
                                        ProgressCallback progress_callback,
                                        size_t buffer_size) {
	// Create output file
	SrecFile sfile(output_filename, address_size, start_address);
	if (!sfile.is_open()) {
		throw SrecFileException("Failed to create output file", output_filename);
	}

	// Get input stream size if possible
	size_t total_bytes = 0;
	if (input.tellg() != -1) {
		input.seekg(0, std::ios::end);
		total_bytes = static_cast<size_t>(input.tellg());
		input.seekg(0, std::ios::beg);
	}

	// Calculate optimal record size
	size_t max_record_size = sfile.max_data_bytes_per_record();
	size_t chunk_size = std::min(buffer_size, max_record_size);
	
	std::vector<uint8_t> buffer(chunk_size);
	size_t bytes_processed = 0;
	uint32_t crc_sum = 0;
	
	// Process input in chunks
	while (input.read(reinterpret_cast<char*>(buffer.data()), 
	                 static_cast<std::streamsize>(buffer.size())) || input.gcount() > 0) {
		                 
		size_t bytes_read = static_cast<size_t>(input.gcount());
		buffer.resize(bytes_read);
		
		// Write data record
		sfile.write_record_payload(buffer);
		
		// Update CRC if needed
		if (want_checksum) {
			crc_sum = xcrc32(buffer.data(), buffer.size(), crc_sum);
		}
		
		bytes_processed += bytes_read;
		
		// Call progress callback if provided
		if (progress_callback && !progress_callback(bytes_processed, total_bytes)) {
			throw SrecValidationException("Conversion aborted by user", 
			                             SrecValidationException::ValidationError::USER_CANCELLED);
		}
		
		// Reset buffer size for next iteration
		buffer.resize(chunk_size);
	}
	
	// Check for read errors
	if (input.bad()) {
		throw SrecFileException("Input stream read error", "");
	}
	
	// Write record count and termination
	sfile.write_record_count();
	sfile.write_record_termination();
	sfile.close();
	
	// Add checksum header if requested
	if (want_checksum) {
		write_checksum(sfile, crc_sum);
	}
}

} // namespace tierone::srec
