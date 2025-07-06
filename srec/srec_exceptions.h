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

#include <exception>
#include <string>
#include <sstream>

namespace tierone::srec {

/**
 * Base exception class for all SREC-related errors
 */
class SrecException : public std::exception {
protected:
    mutable std::string message_;
    
public:
    explicit SrecException(const std::string& msg) : message_(msg) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

/**
 * Exception thrown when parsing S-record format fails
 */
class SrecParseException : public SrecException {
private:
    size_t line_number_;
    size_t column_;
    
public:
    SrecParseException(const std::string& msg, size_t line = 0, size_t col = 0)
        : SrecException(msg), line_number_(line), column_(col) {
        
        if (line_number_ > 0) {
            std::ostringstream oss;
            oss << message_;
            oss << " at line " << line_number_;
            if (column_ > 0) {
                oss << ", column " << column_;
            }
            message_ = oss.str();
        }
    }
    
    size_t getLineNumber() const { return line_number_; }
    size_t getColumn() const { return column_; }
};

/**
 * Exception thrown for file I/O related errors
 */
class SrecFileException : public SrecException {
private:
    std::string filename_;
    
public:
    SrecFileException(const std::string& msg, const std::string& filename = "")
        : SrecException(msg), filename_(filename) {
        
        if (!filename_.empty()) {
            message_ = message_ + " (file: " + filename_ + ")";
        }
    }
    
    const std::string& getFilename() const { return filename_; }
};

/**
 * Exception thrown when validation fails (checksum, format, etc.)
 */
class SrecValidationException : public SrecException {
public:
    enum class ValidationError {
        CHECKSUM_MISMATCH,
        INVALID_FORMAT,
        INVALID_ADDRESS,
        DATA_TOO_LARGE,
        INVALID_RECORD_TYPE,
        USER_CANCELLED
    };
    
private:
    ValidationError error_type_;
    
public:
    SrecValidationException(const std::string& msg, ValidationError error_type)
        : SrecException(msg), error_type_(error_type) {}
    
    ValidationError getErrorType() const { return error_type_; }
};

/**
 * Exception thrown when address ranges are invalid or overflow
 */
class SrecAddressException : public SrecValidationException {
private:
    uint32_t address_;
    uint32_t max_address_;
    
public:
    SrecAddressException(uint32_t addr, uint32_t max_addr)
        : SrecValidationException(
            createMessage(addr, max_addr),
            ValidationError::INVALID_ADDRESS
          ),
          address_(addr),
          max_address_(max_addr) {}
    
    uint32_t getAddress() const { return address_; }
    uint32_t getMaxAddress() const { return max_address_; }
    
private:
    static std::string createMessage(uint32_t addr, uint32_t max_addr) {
        std::ostringstream oss;
        oss << "Address 0x" << std::hex << addr 
            << " exceeds maximum allowed address 0x" << max_addr;
        return oss.str();
    }
};

} // namespace tierone::srec