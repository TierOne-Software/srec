#ifndef SREC_H_
#define SREC_H_

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cinttypes>
#include <cstddef>

std::string ASCIIToHexString(const std::string &buffer);


// Base class for Srecords
class Srec {
public:
	enum class Type {
		S0, S1, S2, S3, S5, S6, S7, S8, S9
	};

	Type getType() const {
		return type;
	}

	explicit Srec(Type type) : type(type) {};
	virtual ~Srec() = default;

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
		}
		return '0';
	}

	// Calculate the checksum for the record
	// We expect the data to be a vector of bytes which can
	// include an address and/or data.
	std::byte checksum(const std::vector<uint8_t> &data) const {
		unsigned long sum = data.size() + 1; // data + byte_count
		for (const auto byte : data) {
			sum += byte;
		}
		return ~static_cast<std::byte>(sum & 0xFF);
	}

	// Get the record data
	// We expect the data to be a vector of bytes which can
	// include an address and/or data.
	// This is a pure virtual function and must be implemented
	// by the derived class.
	virtual std::vector<uint8_t> getRecordData() = 0;

	// Convert the record to a string.
	// We expect the data to be a vector of bytes which can
	// include an address and/or data.
	virtual std::string toString() {
		std::vector<uint8_t> data = getRecordData();

		if (data.size() > 255) {
			throw std::invalid_argument("Data size exceeds maximum");
		}

		std::stringstream ss;
		ss << 'S' << getTypeChar()
		   << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << (data.size() + 1/*data + checksum*/);
		for (const auto byte : data) {
			ss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(byte);
		}
		ss << std::setfill('0') << std::setw(2) << std::uppercase << std::hex << static_cast<unsigned int>(checksum(data));
		return ss.str();
	}

private:
	Type type;
};

// S0 record
// Contains the header information for the file
// The header information is a series of ASCII characters
// This class handles adding the address field to the header
class Srec0 final : public Srec {
	std::vector<uint8_t> header;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	explicit Srec0(const std::vector<uint8_t> &header) : Srec(Srec::Type::S0), header(header) {};
	explicit Srec0(const std::string &header) : Srec(Srec::Type::S0) {
		for (const auto c : header) {
			this->header.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec0(const char *header, const size_t length) : Srec(Srec::Type::S0) {
		for (size_t i = 0; i < length; ++i) {
			this->header.push_back(header[i]);
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

// S1 record
// Contains 16-bit address and data
class Srec1 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	Srec1(unsigned int address, const std::vector<uint8_t> &data) : Srec(Srec::Type::S1), address(address), data(data) {};
	Srec1(unsigned int address, const std::string &data) : Srec(Srec::Type::S1), address(address) {
		for (const auto c : data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec1(unsigned int address, const unsigned char *data, const size_t length) : Srec(Srec::Type::S1), address(address) {
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(data[i]);
		}
	};
	~Srec1() final = default;

	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

// S2 record
// Contains 24-bit address and data
class Srec2 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 3; //in bytes

	Srec2(unsigned int address, const std::vector<uint8_t> &data) : Srec(Srec::Type::S2), address(address), data(data) {};
	Srec2(unsigned int address, const std::string &data) : Srec(Srec::Type::S2), address(address) {
		for (const auto c : data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec2(unsigned int address, const unsigned char *data, const size_t length) : Srec(Srec::Type::S2), address(address) {
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(data[i]);
		}
	};
	~Srec2() final = default;

	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 16) & 0xFF);
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

// S3 record
// Contains 32-bit address and data
class Srec3 final : public Srec {
	unsigned int address;
	std::vector<uint8_t> data;
public:
	static constexpr size_t ADDRESS_SIZE = 4; //in bytes

	Srec3(unsigned int address, const std::vector<uint8_t> &data) : Srec(Srec::Type::S3), address(address), data(data) {};
	Srec3(unsigned int address, const std::string &data) : Srec(Srec::Type::S3), address(address) {
		for (const auto c : data) {
			this->data.push_back(static_cast<uint8_t>(c));
		}
	};
	Srec3(unsigned int address, const unsigned char *data, const size_t length) : Srec(Srec::Type::S3), address(address) {
		for (size_t i = 0; i < length; ++i) {
			this->data.push_back(data[i]);
		}
	};
	~Srec3() final = default;

	std::vector<uint8_t> getData() const {
		return data;
	}

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 24) & 0xFF);
		record.push_back((address >> 16) & 0xFF);
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		record.insert(record.end(), data.begin(), data.end());
		return record;
	}
};

// S5 record
// Contains 16-bit count
// This record is used to count the number of S1, S2, and S3 records
// in the file
class Srec5 final : public Srec {
	unsigned int count;
public:
	explicit Srec5(unsigned int count) : Srec(Srec::Type::S5), count(count) {
		if (count > 0xFFFF) {
			throw std::invalid_argument("Count exceeds maximum");
		}
	};
	~Srec5() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((count >> 8) & 0xFF);
		record.push_back(count & 0xFF);
		return record;
	}
};

// S6 record
// Contains 24-bit count
// This record is used to count the number of S1, S2, and S3 records
// in the file
class Srec6 final : public Srec {
	unsigned int count;
public:
	explicit Srec6(unsigned int count) : Srec(Srec::Type::S6), count(count) {
		if (count > 0xFFFFFF) {
			throw std::invalid_argument("Count exceeds maximum");
		}
	};
	~Srec6() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((count >> 16) & 0xFF);
		record.push_back((count >> 8) & 0xFF);
		record.push_back(count & 0xFF);
		return record;
	}
};

// S7 record
// Contains 32-bit address and execution address
// This record is used to specify the execution start address
// of the program
class Srec7 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 4; //in bytes

	explicit Srec7(unsigned int address) : Srec(Srec::Type::S7), address(address) {};
	~Srec7() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 24) & 0xFF);
		record.push_back((address >> 16) & 0xFF);
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		return record;
	}
};

// S8 record
// Contains 24-bit address and execution address
// This record is used to specify the execution start address
// of the program
class Srec8 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 3; //in bytes

	explicit Srec8(unsigned int address) : Srec(Srec::Type::S8), address(address) {};
	~Srec8() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 16) & 0xFF);
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		return record;
	}
};

// S9 record
// Contains 16-bit address and execution address
// This record is used to specify the execution start address
// of the program
class Srec9 final : public Srec {
	unsigned int address;
public:
	static constexpr size_t ADDRESS_SIZE = 2; //in bytes

	explicit Srec9(unsigned int address) : Srec(Srec::Type::S9), address(address) {};
	~Srec9() final = default;

	std::vector<uint8_t> getRecordData() override {
		std::vector<uint8_t> record;
		record.push_back((address >> 8) & 0xFF);
		record.push_back(address & 0xFF);
		return record;
	}
};


class SrecFile {
public:
	enum class AddressSize {
		BITS16, BITS24, BITS32
	};

private:
	std::string filename;
	std::fstream file;

	unsigned int address; // current address
	unsigned int exec_address; // execution address
	AddressSize address_size_bits;

	unsigned int record_count{0};


public:
	SrecFile(const std::string &filename, AddressSize address_size, unsigned int address = 0);
    ~SrecFile();
    void close();
	bool is_open();
	unsigned int max_data_bytes_per_record() const;

	void write_header(const std::vector<std::string> &header_data);
	void write_header(const std::vector<uint8_t> &header_data);
	void write_record_payload(const std::vector<uint8_t> &buffer);
	void write_record_count();
	void write_record_termination();

	std::string getFilename() const {
		return filename;
	}

	AddressSize addrsize() const {
		return address_size_bits;
	}
};

#endif /* SREC_H_ */
