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

#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cinttypes>
#include <cstddef>
#include <limits>

#include "srec_exceptions.h"

namespace tierone::srec {

/**
 * @brief Convert ASCII string to hexadecimal representation
 * @param buffer Input ASCII string to convert
 * @return Hexadecimal string representation (uppercase)
 * @example ASCIIToHexString("Hello") returns "48656C6C6F"
 */
std::string ASCIIToHexString(const std::string &buffer);




/**
 * @brief Base class for all Motorola S-record types
 * 
 * This abstract class provides the common interface and functionality
 * for all S-record types. Each S-record consists of a type field,
 * count field, address/data field, and checksum.
 * 
 * @note This class is thread-safe for reading operations only.
 *       Writing operations are not thread-safe.
 */
class Srec {
public:
	/**
	 * @brief S-record type enumeration
	 */
	enum class Type {
		S0, ///< Header record
		S1, ///< Data record with 16-bit address
		S2, ///< Data record with 24-bit address  
		S3, ///< Data record with 32-bit address
		S5, ///< Count record (16-bit)
		S6, ///< Count record (24-bit)
		S7, ///< Termination record with 32-bit address
		S8, ///< Termination record with 24-bit address
		S9  ///< Termination record with 16-bit address
	};

	/**
	 * @brief Get the S-record type
	 * @return The S-record type (S0, S1, S2, etc.)
	 */
	Type getType() const {
		return type;
	}

	/**
	 * @brief Construct S-record with specified type
	 * @param record_type The type of S-record to create
	 */
	explicit Srec(Type record_type) : type(record_type) {};
	virtual ~Srec() = default;

	/**
	 * @brief Get the character representation of the S-record type
	 * @return Character '0'-'9' corresponding to the S-record type
	 */
	char getTypeChar () const {
		switch (type) {
			case Type::S0:
				return '0';
			case Type::S1:
				return '1';
			case Type::S2:
				return '2';
			case Type::S3:
				return '3';
			case Type::S5:
				return '5';
			case Type::S6:
				return '6';
			case Type::S7:
				return '7';
			case Type::S8:
				return '8';
			case Type::S9:
				return '9';
			default:
				return '0';
		}
	}

	/**
	 * @brief Calculate the checksum for the record
	 * 
	 * Computes the one's complement checksum of the record data.
	 * The checksum includes the byte count and all address/data bytes.
	 * 
	 * @param data Vector of bytes including address and/or data
	 * @return Computed checksum as std::byte
	 */
	std::byte checksum(const std::vector<uint8_t> &data) const {
		unsigned long sum = data.size() + 1; // data + byte_count
		for (const auto byte : data) {
			sum += byte;
		}
		return ~static_cast<std::byte>(sum & 0xFF);
	}

	/**
	 * @brief Get the record data as a byte vector
	 * 
	 * Returns the combined address and data bytes for this record.
	 * The format depends on the specific S-record type:
	 * - S0: 2-byte address (0x0000) + header data
	 * - S1/S2/S3: address bytes + payload data
	 * - S5/S6: count bytes
	 * - S7/S8/S9: execution address bytes
	 * 
	 * @return Vector of bytes containing address and/or data
	 * @note This is a pure virtual function implemented by derived classes
	 */
	virtual std::vector<uint8_t> getRecordData() = 0;

	/**
	 * @brief Convert the S-record to its string representation
	 * 
	 * Creates the complete S-record string in Motorola format:
	 * S<type><count><address/data><checksum>
	 * 
	 * Example: "S1131000010203E3" for S1 record with 3 data bytes at address 0x1000
	 * 
	 * @return Formatted S-record string (uppercase hexadecimal)
	 * @throws SrecValidationException if record data exceeds 254 bytes
	 */
	virtual std::string toString() {
		std::vector<uint8_t> data = getRecordData();

		if (data.size() > 254) { // 255 - 1 for checksum
			throw SrecValidationException(
				"Record data size exceeds maximum of 254 bytes",
				SrecValidationException::ValidationError::DATA_TOO_LARGE
			);
		}

		// Pre-allocate string buffer for better performance
		// Format: S + type + 2-digit count + data bytes (2 chars each) + 2-digit checksum
		const size_t estimated_size = 1 + 1 + 2 + (data.size() * 2) + 2;
		std::string result;
		result.reserve(estimated_size);
		
		std::stringstream ss;
		ss << 'S' << getTypeChar()
		   << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(data.size() + 1/*data + checksum*/);
		for (const auto byte : data) {
			ss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(byte);
		}
		ss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(checksum(data));
		return ss.str();
	}

private:
	Type type;
};

/**
 * @brief S0 Header Record
 * 
 * Contains header information for the S-record file.
 * The header data is typically ASCII text describing the file contents.
 * Always uses a 2-byte address field (0x0000).
 * 
 * @note This is the first record in an S-record file
 */
class Srec0 final : public Srec {
	std::vector<uint8_t> header;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	explicit Srec0(const std::vector<uint8_t> &header_data) : Srec(Srec::Type::S0), header(header_data) {};
	explicit Srec0(const std::string &header_data) : Srec(Srec::Type::S0) {
		for (const auto c : header_data) {
			this->header.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec0(const char *header_data, const size_t length) : Srec(Srec::Type::S0) {
		for (size_t i = 0; i < length; ++i) {
			this->header.push_back(static_cast<uint8_t>(header_data[i]));
		}
	};
	~Srec0() final = default;

	std::vector<uint8_t> getRecordData() override {
		// prepend 2 zero bytes to the header
		// this is the address field
		std::vector<uint8_t> record;
		record.push_back(0);
		record.push_back(0);
		record.insert(record.end(), header.begin(), header.end());

		return record;
	}
};

/**
 * @brief S1 Data Record with 16-bit Address
 * 
 * Contains data with a 16-bit address field (0x0000-0xFFFF).
 * Used for programs that fit within 64KB of address space.
 * 
 * @note Address range: 0x0000 to 0xFFFF
 */
class Srec1 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	Srec1(unsigned int addr, const std::vector<uint8_t> &record_data) : Srec(Srec::Type::S1), address(addr), data(record_data) {
		if (addr > 0xFFFF) {
			throw SrecAddressException(addr, 0xFFFF);
		}
	};
	Srec1(unsigned int addr, const std::string &record_data) : Srec(Srec::Type::S1), address(addr) {
		if (addr > 0xFFFF) {
			throw SrecAddressException(addr, 0xFFFF);
		}
		for (const auto c : record_data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec1(unsigned int addr, const unsigned char *record_data, const size_t length) : Srec(Srec::Type::S1), address(addr) {
		if (addr > 0xFFFF) {
			throw SrecAddressException(addr, 0xFFFF);
		}
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(record_data[i]);
		}
	};
	~Srec1() final = default;

	/**
	 * @brief Get the data payload
	 * @return Copy of the data bytes (excluding address)
	 */
	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

/**
 * @brief S2 Data Record with 24-bit Address
 * 
 * Contains data with a 24-bit address field (0x000000-0xFFFFFF).
 * Used for programs that require up to 16MB of address space.
 * 
 * @note Address range: 0x000000 to 0xFFFFFF
 */
class Srec2 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 3; //in bytes

	Srec2(unsigned int addr, const std::vector<uint8_t> &record_data) : Srec(Srec::Type::S2), address(addr), data(record_data) {
		if (addr > 0xFFFFFF) {
			throw SrecAddressException(addr, 0xFFFFFF);
		}
	};
	Srec2(unsigned int addr, const std::string &record_data) : Srec(Srec::Type::S2), address(addr) {
		if (addr > 0xFFFFFF) {
			throw SrecAddressException(addr, 0xFFFFFF);
		}
		for (const auto c : record_data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec2(unsigned int addr, const unsigned char *record_data, const size_t length) : Srec(Srec::Type::S2), address(addr) {
		if (addr > 0xFFFFFF) {
			throw SrecAddressException(addr, 0xFFFFFF);
		}
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(record_data[i]);
		}
	};
	~Srec2() final = default;

	/**
	 * @brief Get the data payload
	 * @return Copy of the data bytes (excluding address)
	 */
	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

/**
 * @brief S3 Data Record with 32-bit Address
 * 
 * Contains data with a 32-bit address field (0x00000000-0xFFFFFFFF).
 * Used for programs that require the full 32-bit address space.
 * 
 * @note Address range: 0x00000000 to 0xFFFFFFFF
 */
class Srec3 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 4; //in bytes

	Srec3(unsigned int addr, const std::vector<uint8_t> &record_data) : Srec(Srec::Type::S3), address(addr), data(record_data) {};
	Srec3(unsigned int addr, const std::string &record_data) : Srec(Srec::Type::S3), address(addr) {
		for (const auto c : record_data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec3(unsigned int addr, const unsigned char *record_data, const size_t length) : Srec(Srec::Type::S3), address(addr) {
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(record_data[i]);
		}
	};
	~Srec3() final = default;

	/**
	 * @brief Get the data payload
	 * @return Copy of the data bytes (excluding address)
	 */
	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 24) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

/**
 * @brief S5 Count Record with 16-bit Count
 * 
 * Contains the count of S1, S2, and S3 data records in the file.
 * Uses a 16-bit count field (0x0000-0xFFFF).
 * 
 * @note Maximum count: 65535 records
 */
class Srec5 final : public Srec {
	unsigned int count;
public:
	explicit Srec5(unsigned int record_count) : Srec(Srec::Type::S5), count(record_count) {
		if (record_count > 0xFFFF) {
			throw std::invalid_argument("Count exceeds maximum");
		}
	};
	~Srec5() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(count & 0xFF));
		return record;
	}
};

/**
 * @brief S6 Count Record with 24-bit Count
 * 
 * Contains the count of S1, S2, and S3 data records in the file.
 * Uses a 24-bit count field (0x000000-0xFFFFFF).
 * 
 * @note Maximum count: 16777215 records
 */
class Srec6 final : public Srec {
	unsigned int count;
public:
	explicit Srec6(unsigned int record_count) : Srec(Srec::Type::S6), count(record_count) {
		if (record_count > 0xFFFFFF) {
			throw std::invalid_argument("Count exceeds maximum");
		}
	};
	~Srec6() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((count >> 16) & 0xFF));
		record.push_back(static_cast<uint8_t>((count >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(count & 0xFF));
		return record;
	}
};

/**
 * @brief S7 Termination Record with 32-bit Start Address
 * 
 * Specifies the execution start address for the program.
 * Uses a 32-bit address field and terminates the S-record file.
 * 
 * @note This is the final record in files using S3 data records
 */
class Srec7 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 4; //in bytes

	explicit Srec7(unsigned int addr) : Srec(Srec::Type::S7), address(addr) {};
	~Srec7() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 24) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		return record;
	}
};

/**
 * @brief S8 Termination Record with 24-bit Start Address
 * 
 * Specifies the execution start address for the program.
 * Uses a 24-bit address field and terminates the S-record file.
 * 
 * @note This is the final record in files using S2 data records
 */
class Srec8 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 3; //in bytes

	explicit Srec8(unsigned int addr) : Srec(Srec::Type::S8), address(addr) {
		if (addr > 0xFFFFFF) {
			throw SrecAddressException(addr, 0xFFFFFF);
		}
	};
	~Srec8() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		return record;
	}
};

/**
 * @brief S9 Termination Record with 16-bit Start Address
 * 
 * Specifies the execution start address for the program.
 * Uses a 16-bit address field and terminates the S-record file.
 * 
 * @note This is the final record in files using S1 data records
 */
class Srec9 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	explicit Srec9(unsigned int addr) : Srec(Srec::Type::S9), address(addr) {
		if (addr > 0xFFFF) {
			throw SrecAddressException(addr, 0xFFFF);
		}
	};
	~Srec9() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
		record.push_back(static_cast<uint8_t>(address & 0xFF));
		return record;
	}
};


/**
 * @brief S-record file writer and manager
 * 
 * Provides functionality to create and write S-record files with proper
 * formatting, validation, and security checks. Supports all standard
 * address sizes (16-bit, 24-bit, 32-bit).
 * 
 * @note Files are opened in truncate mode and will overwrite existing content
 * @warning This class is not thread-safe
 */
class SrecFile {
public:
	/**
	 * @brief Address size enumeration for S-record files
	 */
	enum class AddressSize {
		BITS16, ///< 16-bit addresses (S1/S9 records)
		BITS24, ///< 24-bit addresses (S2/S8 records) 
		BITS32  ///< 32-bit addresses (S3/S7 records)
	};

private:
	std::string filename;
	std::fstream file;

	unsigned int address; // current address
	unsigned int exec_address; // execution address
	AddressSize address_size_bits;

	unsigned int record_count{0};
	
	// Security limits
	static constexpr size_t MAX_FILE_SIZE = 100 * 1024 * 1024; // 100MB
	static constexpr size_t MAX_RECORD_COUNT = 1000000; // 1M records


public:
	/**
	 * @brief Construct and open an S-record file for writing
	 * @param file_name Path to the output file
	 * @param address_size Address size for data records (16/24/32-bit)
	 * @param start_address Starting address for data records (default: 0)
	 * @throws SrecFileException if file cannot be opened
	 */
	SrecFile(const std::string &file_name, AddressSize address_size, unsigned int start_address = 0);
	
	/**
	 * @brief Destructor - automatically closes the file
	 */
    ~SrecFile();
    
	/**
	 * @brief Close the S-record file and flush all data
	 */
    void close();
    
	/**
	 * @brief Check if the file is currently open
	 * @return true if file is open, false otherwise
	 */
	bool is_open();
	
	/**
	 * @brief Get maximum data bytes per record for current address size
	 * @return Maximum number of data bytes that can fit in one record
	 */
	unsigned int max_data_bytes_per_record() const;

	/**
	 * @brief Write header records (S0) from string data
	 * @param header_data Vector of header strings (converted to ASCII)
	 * @throws SrecFileException if file is not open
	 */
	void write_header(const std::vector<std::string> &header_data);
	
	/**
	 * @brief Write header record (S0) from binary data
	 * @param header_data Vector of header bytes
	 * @throws SrecFileException if file is not open
	 */
	void write_header(const std::vector<uint8_t> &header_data);
	
	/**
	 * @brief Write data record (S1/S2/S3) with payload
	 * @param buffer Data bytes to write
	 * @throws SrecFileException if file is not open
	 * @throws SrecValidationException if limits exceeded
	 * @throws SrecAddressException if address overflow
	 */
	void write_record_payload(const std::vector<uint8_t> &buffer);
	
	/**
	 * @brief Write count record (S5/S6) with current record count
	 * @throws SrecFileException if file is not open
	 * @throws SrecValidationException if count exceeds limits
	 */
	void write_record_count();
	
	/**
	 * @brief Write termination record (S7/S8/S9) with execution address
	 * @throws SrecFileException if file is not open
	 */
	void write_record_termination();

	/**
	 * @brief Get the filename of this S-record file
	 * @return Filename as provided to constructor
	 */
	std::string getFilename() const {
		return filename;
	}

	/**
	 * @brief Get the address size configuration
	 * @return Current address size setting
	 */
	AddressSize addrsize() const {
		return address_size_bits;
	}
};


/**
 * @brief Convert binary file to S-record format
 * @param input Input binary file stream
 * @param sfile Output S-record file
 * @param want_checksum Whether to include CRC32 checksum in header
 * @throws SrecFileException on file I/O errors
 */
void convert_bin_to_srec(std::ifstream &input, SrecFile &sfile, bool want_checksum);

/**
 * @brief Write CRC32 checksum as S0 header record
 * @param srecfile S-record file to modify
 * @param sum CRC32 checksum value
 * @throws std::ios_base::failure on file operations
 */
void write_checksum(const SrecFile &srecfile, unsigned int sum);

/**
 * @brief Convert S-record file to binary format
 * @param input_file Path to input S-record file
 * @param output_file Path to output binary file
 * @throws std::ios_base::failure on file I/O errors
 * @note Only processes S1, S2, and S3 data records
 */
void convert_srec_to_bin(const std::string &input_file, const std::string &output_file);

} // namespace tierone::srec

