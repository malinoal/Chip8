#include<stdio.h>
#include<iostream>
#include<fstream>
//#include "ROM.cpp"
#include "CHIP8.cpp"

ROM readRom(char* romPath) {

	char* romContents;

	std::ifstream rom (romPath, std::ios::in|std::ios::binary|std::ios::ate); //ate= at the end, used to calculate file size; ios= input output stream

	if(rom.is_open()) {
		std::streampos size = rom.tellg(); //sets the size to the current pointer at the rom, which should be located at the end, thus giving the correct size
		romContents = new char[size];

		rom.seekg(0, std::ios::beg); //places pointer at beginning (with an offset of 0)

		rom.read(romContents, size); //copies contents of the rom into romContents

		rom.close(); //rom no longer needed, as it is loaded entirely into memory

		ROM romout;
		romout.romContents = (unsigned char*) romContents;
		romout.size = size;

		return romout;

	} else {
		std::cout << "ROM could not be opened";
		ROM romout;
		romout.romContents = NULL;
		romout.size = NULL;
		return romout;
	}
}

int main(int argc, char* args[]) {

	ROM rom = readRom(args[1]);

	/*for(int i = 0; i < rom.size; ++i)
		printf("%02x\n", (int) rom.romContents[i]);*/ //romContents seem to be read correctly

	CHIP8 chip8;

	chip8.initialise(rom);
	chip8.runEmu();
	chip8.d_printRegisters();
	chip8.d_printMemory();
	chip8.freememory();



	delete[] rom.romContents;
	return 0;
}
