#include<stdio.h>
#include<inttypes.h>
#include "ROM.cpp"
#include "Display8.cpp"


class CHIP8 {

public:

	uint8_t* memory;			//the memory
	uint8_t* V; 					//the registers
	uint16_t I;						//address register
	uint16_t SP;					//Stack Pointer; Stack begins at 0xEA0
	uint16_t PC;					//Program Counter; starts at 0x200
	uint8_t delay;					//delay timer
	uint8_t sound;					//sound timer
	uint8_t* display;				//Display Buffer; in memory at 0xF00

	Display8 display8;

	bool draw = false;


	void initialise(ROM rom) {
		memory = new uint8_t[1024*4](); //the () at the end initializes everything to zero using black magic
		V = new uint8_t[16]();
		SP = 0xEA0;
		PC = 0x200;
		display = &memory[0xF00];

		for(int i = PC; i < PC + rom.size; ++i) {
			memory[i] = rom.romContents[i-PC];
			//D_start
			//printf("i: %i, PC: %i, memory[i]: %02x , romContents[i-PC]: %02x \n", i, PC, memory[i], rom.romContents[i-PC]);
			//D_end
		}

		display8.initialise();

	} //end initialise


	void freememory() {
		delete[] memory;
	}



	void Op6() { //6XNN sets V[X] to 0xNN
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		V[x] = nn;

		PC+=2;
	}

	void OpA() { //ANNN sets I to 0xNNN
		int n = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		int nnn = (n << 8) + nn; //should calculate the correct NNN

		I = nnn;

		PC+=2;
	}

	void OpD() {
		/*
		 * DXYN = Draw(V[X], V[Y], N), draws sprite with a height of N pixels stored at I to coordinates V[X], V[Y].
		 * Pixels get Flipped (XOR), if an active pixel gets flipped to zero V[0xF] gets set to 1, otherwise V[0xF] gets set to 0.
		 */

		int x = (memory[PC] & 0x0f);
		int y = (memory[PC + 1] & 0xf0);
		int n = (memory[PC + 1] & 0x0f);

		bool Vf = 0;

		//bytes to be accessed are at display[V[X] + (64/8)*V[Y] + (64/8)*i]
		//and at memory[I+i]

		int coord;
		int bit;

		for(int i = 0; i < n; ++i) {
			coord = V[x] + 8*V[y] + 8*i;
			for(int j = 7; j >= 0; --j) {
				bit = (1 << j);
				if((memory[I+i] & bit)) { //only have to do something if the bit is set
					if((display[coord] & bit)) {
						Vf = 1;
					} //end if display is set
					display[coord] = (display[coord]^bit); // the ^ means XOR
				} //end if I is set
			} //end for j
		} //end for i

		V[0xf] = Vf;

		PC += 2;
		draw = true;

	} //end OpD



	bool runOP() {

		bool success = true;
		uint8_t opcode = memory[PC]; //load the current opcode

		uint8_t msb = (opcode & 0xf0) >> 4; //grab the 4 most significant bits, and shift them to the right to look at them

		switch(msb) {

		case 0x06:
			Op6();
			break;

		case 0x0a:
			OpA();
			break;

		case 0x0d:
			OpD();
			break;

		default:
			printf("Opcode %02x not implemented yet\n", opcode);
			success = false;

		} //end switch

		return success;
	} //end runOP


	void runEmu() {
		while(runOP()) {
			if(draw) {
				display8.draw(display);
				draw = false;
			}
		}
	}


	void d_printMemory() {
		for(int i = 0; i < 16; ++i) {
			for(int j = 0; j < 256; ++j) {
				printf("%02x ",  memory[j+(i*256)]);
			}
			printf("\n");
		}
	} //end d_printMemory

	void d_printRegisters() {
		for(int i = 0; i < 16; i++)
			printf("V[%i]: %02x\n", i, V[i]);
		printf("I: %02x\n", I);
	}

};
