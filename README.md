# TierOne SREC library and utilities

This repository contains a library and utilities for manipulating Motorola S-record files.

## SREC Format Overview

The Motorola S-record format is a widely used file format for storing binary data in a text-based, hexadecimal format. It was originally developed by Motorola for programming microcontrollers and EPROMs. Each line in an S-record file is called a record and follows a specific structure:

- **S-record structure**:
  - Type field (2 chars): Starts with 'S' followed by a digit (0-9) indicating the record type.
  - Count field (2 chars): Hexadecimal byte count of the remaining fields.
  - Address field (4-8 chars): Memory address where data should be loaded.
  - Data field (variable): The actual data in hexadecimal format.
  - Checksum (2 chars): One's complement of the least significant byte of the sum of all bytes.

- **Record types**:
  - S0: Header record (containing metadata, not program data)
  - S1/S2/S3: Data records with 16-bit, 24-bit, or 32-bit addresses
  - S5/S6: Record count records (16-bit or 24-bit)
  - S7/S8/S9: Termination records with 32-bit, 24-bit, or 16-bit addresses

## Library

The library can be found under the 'srec' directory and provides:

- Classes for each S-record type (Srec0, Srec1, etc.)
- SrecFile class for reading/writing S-record files
- CRC32 calculation for file verification
- Uses C++17 features

## Utilities

### bin2srec

This utility converts a binary file to an S-record file.

Usage:
```
bin2srec -i <input file> -o <output file> -b <address_bits> --checksum
```

Arguments:
- `-i, --input`: Input binary file
- `-o, --output`: Output SREC file (defaults to "output.srec")
- `-b, --addrbits`: Address size in bits (16, 24, or 32)
- `-c, --checksum`: Add a CRC32 checksum as the first S0 record

Example:
```
bin2srec -i input.bin -o output.srec -b 16 --checksum
```

### srec2bin

This utility converts an S-record file to a binary file.

Usage:
```
srec2bin -i <input file> -o <output file>
```

Arguments:
- `-i, --input`: Input SREC file
- `-o, --output`: Output binary file

Example:
```
srec2bin -i input.srec -o output.bin
```

### sreccheck

This utility checks the CRC32 of an S-record file.
The checksum is expected to be the first S0 line of the file.

Usage:
```
sreccheck <input file> [--verbose]
```

Arguments:
- `file`: SREC file to check
- `-v, --verbose`: Show detailed information about the CRC check

Example:
```
sreccheck input.srec --verbose
sreccheck input.srec
```

## Building

The project uses CMake as its build system. To build the utilities:

```bash
# Build for host platform
cmake -B build_host -S . -G Ninja
cmake --build build_host
```

For cross-compiling to ARM:

```bash
# NOTE: replace with you're own toolchain and toolchain file.
# Set the cross-compiler location
export CROSS_COMPILER=/path/to/armv7-eabihf--musl--stable-2023.11-1

# Build for ARM target
cmake -DCMAKE_TOOLCHAIN_FILE=toolchainfile.cmake -B build_target -S . -G Ninja
cmake --build build_target
```

## Testing

The library and utilities include comprehensive tests:

### Unit Tests

Unit tests are implemented using the Catch2 framework and test all components of the SREC library:

```bash
ctest --test-dir build_host
```

### Integration Tests

A roundtrip test script is provided that verifies the full workflow:
1. Creates random binary data
2. Converts it to SREC format using bin2srec
3. Verifies the SREC checksum using sreccheck
4. Converts back to binary using srec2bin
5. Compares the result with the original data

To run the integration tests:

```bash
cmake --test-dir build_host -R roundtrip_test
```

## License

Licensed under the Apache License, Version 2.0. See [LICENSE-2.0.txt](LICENSE-2.0.txt) for the full license text.


## Contributing

We welcome contributions to TierOne SREC! Here's how you can help:

### Submitting Changes

1. Fork the repository on GitHub
2. Create a feature branch from `master`
3. Make your changes with appropriate tests
4. Ensure all tests pass and code follows the existing style
5. Submit a pull request with a clear description of your changes

### Development Guidelines

- Follow the existing code style and naming conventions
- Add tests for new features or bug fixes
- Update documentation for API changes
- Use modern C++ features appropriately
- Ensure compatibility with supported compilers

### Reporting Issues

Please report bugs, feature requests, or questions by opening an issue on GitHub.

## Copyright

Copyright 2025 TierOne Software. All rights reserved.