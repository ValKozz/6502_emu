#include <iostream>
#include <fstream>
#include <vector>

#include "cpu.h"

using namespace std;

int main(int argc, char **argv) {
	if (argc < 2) {
		cout << "Usage: 6502_emu <filename.bin>" << endl;
		return 0;
	}

	u8 buffer;
	vector<u8> prog;
	ifstream file(argv[1], ios::binary | ios::in);
	if (!file.is_open()) {
		cout << "Error opening " << argv[1] << ". Does it exist?" << endl;
		return 1;
	}

	file.seekg(0, ios::end);
	streampos fsize = file.tellg();
	file.seekg(0, ios::beg);
	prog.resize(fsize);

	for (int i = 0; i < fsize; i++) {
		file.read((char*)&buffer, sizeof(buffer));
		prog.push_back(buffer);
	}
	file.close();

	CPU _6502(1);
	_6502.load_prog(prog, prog.size());
	_6502.run();

	return 0;
}
