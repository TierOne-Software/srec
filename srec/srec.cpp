#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <memory>

#include "srec.h"

// Convert a std::string to a hex string
std::string ASCIIToHexString(const std::string &buffer) {
	std::stringstream ss;
	ss << std::uppercase << std::hex << std::setfill('0');
	for (const auto &c : buffer) {
		ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(c));
	}
	// Convert the string stream to a std::string and return it
	return ss.str();
}

// Parse an S-record string and return an Srec objec
SrecFile::SrecFile(const std::string &filename, SrecFile::AddressSize address_size, unsigned int address)
	: filename(filename),
	  address(address),
	  exec_address(address),
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
	}
	return 0;
}

// Write record data (S1/S2/S3) to file
void SrecFile::write_record_payload(const std::vector<uint8_t> &buffer) {
	if (!this->file.is_open()) {
		throw std::ios_base::failure("File is not open: " + this->filename);
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
		throw std::ios_base::failure("File is not open: " + this->filename);
	}

	if (this->record_count > 0xFFFFFF) {
		throw std::out_of_range("Record count must be less than 0xFFFFFF");
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
		throw std::ios_base::failure("File is not open: " + this->filename);
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
	}
	// Write the record to the file
	this->file << record->toString() << std::endl;
	this->file.flush();
}

void SrecFile::write_header(const std::vector<std::string> &header_data) {
	if (!this->file.is_open()) {
		throw std::ios_base::failure("File is not open: " + this->filename);
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
		throw std::ios_base::failure("File is not open: " + this->filename);
	}

	// Write the header data to the file
	auto record = Srec0(header_data);
	this->file << record.toString() << std::endl;
	this->file.flush();
}
