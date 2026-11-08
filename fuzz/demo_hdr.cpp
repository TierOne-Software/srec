// Demo: write_header(vector<string>) double-hex-encodes header data.
#include "srec/srec.h"
#include <fstream>
#include <iostream>

int main() {
	// Header as ASCII string
	{
		tierone::srec::SrecFile sf("/tmp/hdr_str.srec", tierone::srec::SrecFile::AddressSize::BITS32);
		sf.write_header(std::vector<std::string>{"TEST"});
		sf.close();
	}
	// Same header as raw bytes
	{
		tierone::srec::SrecFile sf("/tmp/hdr_bytes.srec", tierone::srec::SrecFile::AddressSize::BITS32);
		sf.write_header(std::vector<uint8_t>{'T', 'E', 'S', 'T'});
		sf.close();
	}
	std::ifstream f1("/tmp/hdr_str.srec"), f2("/tmp/hdr_bytes.srec");
	std::string l1, l2;
	std::getline(f1, l1);
	std::getline(f2, l2);
	std::cout << "write_header({\"TEST\"})        -> " << l1 << "\n";
	std::cout << "write_header({'T','E','S','T'}) -> " << l2 << "\n";
	std::cout << "expected both                  -> S007000054455354XX\n";
	return 0;
}
