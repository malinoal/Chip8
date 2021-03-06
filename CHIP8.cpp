#include<stdlib.h>
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

	bool success = true;

	SDL_Event event;

	bool* KEYS;

	char* d_keychars = "0000abcdefghijklmnopqrstuvwxyz";

	const int FPS = 600;				//Desired Speed, determines CPU-Cycles/Second
	const unsigned int TICKS_PER_FRAME = 1000 / FPS;
	uint32_t timer = 0;
	uint32_t timeDelta = 0;



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

		initFonts();

		KEYS = new bool[322](); //There are apparently 322 different Keys registered by SDL

		timer = SDL_GetTicks();

	} //end initialise


	void freememory() {
		delete[] memory;
	}

	int NNN() { //calculates the NNN for OPs of the form XNNN
		int n = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		int nnn = (n << 8) + nn; //should calculate the correct NNN

		return nnn;
	}

	void savePCtoStack() {
		uint16_t high = (PC & 0xff00) >> 8;
		uint16_t low = (PC & 0x00ff);
		SP++;
		memory[SP] = high;
		SP++;
		memory[SP] = low;
	}

	void retrievePCfromStack() {
		uint16_t low = memory[SP--];
		uint16_t high = (memory[SP--]) << 8;

		uint16_t full = 0;
		full = (full | low);
		full = (full | high);

		PC = full;
	}

	void Op0() { //either clears the screen (E0) or returns from a subroutine (EE)
		int nn = memory[PC+1];

		switch(nn) {
		case 0xe0:
			for(int i = 0; i < 0x100; ++i) display[i] = 0;
			draw = true;
			PC+=2;
			break;

		case 0xee:
			retrievePCfromStack();
			//D_Start
			//printf("Returned from subroutine to %04x\n", PC);
			//D_End
			break;

		default:
			printf("ERROR: OpCode 00%02x does not exist.\n", nn);
			success = false;
		}

	}

	void Op1() { //1NNN: Jumps to NNN
		int nnn = NNN();
		PC = nnn;

		//D_Start
		//printf("Jumped to %04x \n", nnn);
		//D_End
	}


	void Op2() { //2NNN calls subroutine at NNN
		int nnn = NNN();

		//D_Start
		//printf("PC before Jump to subroutine: %04x\n", PC);
		//printf("PC saved on stack should be that +2, so: %04x\n", PC+2);
		//D_End

		/*
		 * Problem: The PC is a 16 bit number, memory only saves 8 bit numbers
		 * Solution: Write functions to save PC on stack and retrieve it
		 */

		//SP++; //increase Stack pointer so as to not overwrite what has been there previously		//OBSOLETE
		//memory[SP] = (PC+2); //save where to continue when subroutine has finished				//OBSOLETE

		PC += 2;
		savePCtoStack();

		PC = nnn; //Jump to subroutine

		//D_Start
		//printf("Called Subroutine at %03x \n", nnn);
		//printf("top of stack is now: %04x\n", memory[SP]);										//OBSOLETE as PC takes two places on Stack
		//D_End
	}

	void Op3() { //3XNN skips next instruction if V[X] == NN
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		if(V[x] == nn) PC += 4;
		else PC += 2;

		//D_Start
		//printf("Checked if V[%01x] is %i. (It's %i)\n", x, nn, V[x]);
		//D_End
	}

	void Op4() { //4XNN skips next instruction if V[X] != NN
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		if(V[x] != nn) PC += 4;
		else PC += 2;

		//D_Start
		//printf("Checked if V[%01x] is NOT %i. (It's %i)\n", x, nn, V[x]);
		//D_End
	}

	void Op5() { //5XY0 skips next instruction if V[x] == V[y]
		int x = (memory[PC] & 0x0f);
		int y = ((memory[PC+1] & 0xf0) >> 4);

		if(V[x] == V[y])
			PC+=4;
		else
			PC+=2;
	}

	void Op6() { //6XNN sets V[X] to 0xNN
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		V[x] = nn;

		PC+=2;


		//D_Start
		//printf("Set V[%02x] to %02x\n", x, nn);
		//D_End
	}

	void Op7() { //7XNN: V[X]+=NN
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		V[x] += nn;

		PC+=2;

		//D_Start
		//printf("Increased V[%02x] by %02x\n", x, nn);
		//D_End
	}

	void Op8() {
		/*
		 * 8XYZ
		 * Math Function, does a variety of things with V[X] and V[Y] dependent on Z
		 * Descriptions for the cases taken from wikipedia (https://en.wikipedia.org/wiki/CHIP-8)
		 */
		int x = (memory[PC] & 0x0f);
		int y = ((memory[PC+1] & 0xf0) >> 4);
		int z = (memory[PC+1] & 0x0f);

		switch(z) {

		case 0x00: //Sets VX to the value of VY.
			V[x] = V[y];
			break;

		case 0x01: //Sets VX to VX or VY. (Bitwise OR operation)
			V[x] = V[x] | V[y];
			break;

		case 0x02: //Sets VX to VX and VY. (Bitwise AND operation)
			V[x] = V[x] & V[y];
			break;

		case 0x03: //Sets VX to VX xor VY.
			V[x] = V[x] ^ V[y];
			break;

		case 0x04: //Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't.
			{uint16_t result = V[x] + V[y];
			if(result & 0xf0)
				V[0xf] = 1;
			else
				V[0xf] = 0;
			V[x] = (result &0x0f);} //The {} are neccessary to prevent result from appearing in other cases (even though it isn't used, the compiler doesn't like it)
			break;

		case 0x05: //VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
			if(V[y] > V[x])
				V[0xf] = 0;
			else
				V[0xf] = 1;
			V[x] -= V[y];
			break;

		case 0x06: //Stores the least significant bit of VX in VF and then shifts VX to the right by 1.
			V[0xf] = (V[x] & 1);
			V[x] = V[x] >> 1;
			break;

		case 0x07: //Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
			if(V[x] > V[y])
				V[0xf] = 0;
			else
				V[0xf] = 1;
			V[x] = V[y] - V[x];
			break;

		case 0x0e: //Stores the most significant bit of VX in VF and then shifts VX to the left by 1.
			V[0xf] = (V[x] & 0x80);
			V[x] = V[x] << 1;
			break;

		default:
			printf("ERROR: OpCode 8XY%01x does not exist", z);
			success = false;

		}

		PC += 2;
	} //end Op8

	void Op9() { //9XY0 Skips next instruction if V[X] != V[Y]
		int x = (memory[PC] & 0x0f);
		int y = ((memory[PC+1] & 0xf0) >> 4);

		if(V[x] != V[y])
			PC += 4;
		else
			PC += 2;

	}

	void OpA() { //ANNN sets I to 0xNNN
		int nnn = NNN();

		I = nnn;

		PC+=2;

		//D_Start
		//printf("Set I to %02x\n", nnn);
		//D_End
	}

	void OpB() { //BNNN Jumps to NNN+V[0]
		int nnn = NNN();

		PC = nnn+V[0];

		//D_Start
		//printf("Jumped to 0x04\n", PC);
		//D_End
	}

	void OpC() { //CXNN sets V[X] to ((random number) & NN)
		int x = (memory[PC] & 0x0f);
		int nn = memory[PC+1];

		V[x] = ((rand() % 256) & nn);

		PC+=2;
	}

	/*void OpDBACK() { 										//OBSOLETE
		/*
		 * DXYN = Draw(V[X], V[Y], N), draws sprite with a height of N pixels stored at I to coordinates V[X], V[Y].
		 * Pixels get Flipped (XOR), if an active pixel gets flipped to zero V[0xF] gets set to 1, otherwise V[0xF] gets set to 0.
		 *
		 * TODO: Drawing is buggy, not sure if the problem is with OpD or with the Display's draw function
		 /

		int x = (memory[PC] & 0x0f);
		int y = ((memory[PC + 1] & 0xf0)) >> 4;
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

					//D_Start
					//printf("display[coord]: %02x, memory[I+i]: %02x\n", display[coord], memory[I+i]);
					//D_End

				} //end if I is set
			} //end for j
		} //end for i

		V[0xf] = Vf;

		PC += 2;
		draw = true;

		//D_Start
		printf("drew a sprite\n");
		//D_End

	} */ //end OpD

	void OpD() { //re-implements OpD in an attempt to fix bugs
		/*
		 * DXYN = Draw(V[X], V[Y], N), draws sprite with a height of N pixels stored at I to coordinates V[X], V[Y].
		 * Pixels get Flipped (XOR), if an active pixel gets flipped to zero V[0xF] gets set to 1, otherwise V[0xF] gets set to 0.
		 */

		int x = V[(memory[PC] & 0x0f)];
		int y = V[((memory[PC + 1] & 0xf0)) >> 4];
		int n = (memory[PC + 1] & 0x0f);

		uint8_t* sprite;
		int spritebit;
		uint8_t sourcebit;
		uint8_t* destbyte_p;
		uint8_t destbyte;
		uint8_t destmask;
		uint8_t destbit;

		V[0xf] = 0; //sets V[F] in case no collision takes place

		for(int i = 0; i < n; ++i) {
			sprite = &memory[I+i]; //a pointer to the sprite to be drawn
			spritebit = 7; //which bit of the current row we are currently at
			for(int j = x; j < (x+8); ++j) {

				sourcebit = (*sprite >> spritebit) & 0x1; //Isolate the bit to be drawn

				if(sourcebit) { //only have to do something if the sourcebit is set

					destbyte_p = &display[(i+y)*(64/8) + (j/8)]; //creates a pointer to the byte on the screen to be drawn on

					destbyte = *destbyte_p; //gets the current contents of the screen at that byte

					destmask = (0x80 >> (j%8)); //creates a mask that has only a bit set where it's currently relevant

					destbit = destbyte & destmask; //applies the mask to the destination byte in order to determine the destination bit

					sourcebit = sourcebit << (7 - (j%8)); //shifts the sourcebit to be at the same position as the destination bit

					if(sourcebit & destbit) //if both are set
						V[0xf] = 1; 		//set V[F] to 1

					destbit ^= sourcebit; //do the XOR

					destbyte = (destbyte & ~destmask) | destbit; //apllies the XOR to the actual byte, the ~ inverts the mask

					*destbyte_p = destbyte; //applies the changes, so far we've been working with a copy

				}

				spritebit--; //Do the next bit
			}
		}

		PC+=2; //increase Program Counter
		draw = true; //tells emu to redraw the screen

		//D_Start
		//printf("drew a sprite at the locations in V[%01x] and V[%01x]\n", (memory[PC] & 0x0f) , ((memory[PC + 1] & 0xf0)) >> 4);
		//D_End
	}

	void OpE() { //Skips instructions dependent on whether or not the key in V[X] is pressed
		int x = (memory[PC] & 0x0f);
		switch(memory[PC+1]) {

		case 0x9e: //Skip if Key is pressed
			if(KEYS[V[x]+4])
				PC+=4;
			else
				PC+=2;
			break;

		case 0xa1: //Skip if Key is not pressed
			if(!KEYS[V[x]+4])
				PC+=4;
			else
				PC+=2;
			break;

		default:
			printf("ERROR: Opcode EX%02x does not exist", memory[PC+1]);
			success = false;
		}

		//D_Start
		//printf("Checked if Key number %i (%c) was pressed\n", V[x]+4, d_keychars[V[x]+4]);
		//D_End

	}

	void FX07() { //Sets V[X] to the value of the delay timer
		int x = (memory[PC] & 0x0f);
		V[x] = delay;

		//D_Start
		//printf("Set V[%02x] to the delay timer (%02x)\n", x, delay);
		//D_End
	}

	void FX0A() { //waits for the next keypress, then stores it in V[X]
		int x = (memory[PC] & 0x0f);
		bool waiting = true;
		while(waiting) {
			if(SDL_PollEvent(&event)) {
				if(event.type == SDL_KEYDOWN) {
					V[x] = event.key.keysym.scancode - 4;
					waiting = false;
				}
			}
		}
	}

	void FX15() { //Sets the delay timer to V[X]
		int x = (memory[PC] & 0x0f);
		delay = V[x];

		//D_Start
		//printf("Delay Timer set to %02x\n", V[x]);
		//D_End
	}

	void FX18() { //Sets sound timer to V[x]
		int x = (memory[PC] & 0x0f);
		sound = V[x];

		//D_Start
		//printf("Sound Timer set to %02x\n", V[x]);
		//D_End
	}

	void FX1E() { //I += V[x]
		int x = (memory[PC] & 0x0f);
		I += V[x];

		//D_Start
		//printf("Added V[%02x] to I\n", x);
		//D_End
	}

	void FX29() { //Sets I to the location of the sprite for the character in VX. Characters 0-F are represented by a 4x5 font.
		int x = (memory[PC] & 0x0f);
		I = V[x] * 5;

		//D_Start
		//printf("Set I to the location of %02x\n", V[x]);
		//D_End
	}

	void FX33() { //Stores V[X] as a Binary Coded Decimal in memory at I
		int x = (memory[PC] & 0x0f);
		int value = V[x];
		int hundreds, tens, ones;
		ones = value % 10;
		value /= 10;
		tens = value % 10;
		hundreds = value / 10;

		memory[I] = hundreds;
		memory[I+1] = tens;
		memory[I+2] = ones;

		//D_Start
		//printf("Stored %02x as a Binary Coded Decimal in memory at I\n", V[x]);
		//D_End
	}

	void FX55() { //Stores V[0] to V[X] (including V[X]) in memory starting at address I
		int x = (memory[PC] & 0x0f);
		for(int i = 0; i <= x; ++i)
			memory[I+i] = V[i];
	}

	void FX65() { //Fills V[0] to V[X] (including V[X]) with values from memory starting at address I
		int x = (memory[PC] & 0x0f);
		for(int i = 0; i <= x; ++i) {
			V[i] = memory[I+i];
		}
	}

	void OpF() { //Function Code, FXYY does something to V[X] dependent on code YY
		switch(memory[PC+1]) {

		case 0x07:
			FX07();
			break;

		case 0x0a:
			FX0A();
			break;

		case 0x15:
			FX15();
			break;

		case 0x18:
			FX18();
			break;

		case 0x1e:
			FX1E();
			break;

		case 0x29:
			FX29();
			break;

		case 0x33:
			FX33();
			break;

		case 0x55:
			FX55();
			break;

		case 0x65:
			FX65();
			break;

		default:
			printf("Function code FX%02x not implemented yet\n", memory[PC+1]);
			success = false;
		}

		PC += 2;

	} //end OpF



	bool runOP() {

		//bool success = true;
		uint8_t opcode = memory[PC]; //load the current opcode

		uint8_t msb = (opcode & 0xf0) >> 4; //grab the 4 most significant bits, and shift them to the right to look at them

		switch(msb) {

		case 0x00:
			Op0();
			break;

		case 0x01:
			Op1();
			break;

		case 0x02:
			Op2();
			break;

		case 0x03:
			Op3();
			break;

		case 0x04:
			Op4();
			break;

		case 0x06:
			Op6();
			break;

		case 0x07:
			Op7();
			break;

		case 0x08:
			Op8();
			break;

		case 0x09:
			Op9();
			break;

		case 0x0a:
			OpA();
			break;

		case 0x0c:
			OpC();
			break;

		case 0x0d:
			OpD();
			break;

		case 0x0e:
			OpE();
			break;

		case 0x0f:
			OpF();
			break;

		default:
			printf("Opcode %02x not implemented yet\n", opcode);
			success = false;

		} //end switch

		/*
		 * Start of Timer Implementation
		 */
		if(delay > 0) delay--;

		if(sound > 0) {
			sound--;
			std::cout << '\a'; //creates an alert sound
		}


		timeDelta = SDL_GetTicks() - timer;

		if (timeDelta < TICKS_PER_FRAME) SDL_Delay(TICKS_PER_FRAME - timeDelta);

		timer = SDL_GetTicks();




		//D_Start
		//printf("Executed OpCode %04x, at Point %04x\n", opcode, PC);
		//D_end

		return success;
	} //end runOP


	void runEmu() {
		while(runOP()) {
			if(draw) {
				display8.draw(display);
				draw = false;
			}
			detectKeyboard();
		}
	}

	void detectKeyboard() {
		while(SDL_PollEvent(&event)) {
			switch(event.type) {

			case SDL_QUIT:
				success = false;
				break;

			case SDL_KEYDOWN:
				KEYS[event.key.keysym.scancode] = true;
				//D_Start
				//printf("Key number %i pressed\n", event.key.keysym.scancode);
				//D_End
				break;

			case SDL_KEYUP:
				KEYS[event.key.keysym.scancode] = false;
				//D_Start
				//printf("Key number %i no longer pressed\n", event.key.keysym.scancode);
				//D_End
				break;
			}
		}
	}


	void initFonts() { //Characters 0-F represented by a 4x5 font.

		/*
		 * Template:
		 * 0b00000000
		 * 0b00000000
		 * 0b00000000
		 * 0b00000000
		 * 0b00000000
		 */

		uint8_t fontsprites[] = { //copied from kpmiller at https://github.com/kpmiller/emulator101/blob/master/Chip8/font4x5.c
				//0 at 0
				    0b01100000,
				    0b10010000,
				    0b10010000,
				    0b10010000,
				    0b01100000,

				//1 at 5
				    0b01100000,
				    0b00100000,
				    0b00100000,
				    0b00100000,
				    0b01110000,

				//2 at 10
				    0b11100000,
				    0b00010000,
				    0b00110000,
				    0b01100000,
				    0b11110000,
				//3 at 15
				    0b11100000,
				    0b00010000,
				    0b01100000,
				    0b00010000,
				    0b11100000,
				//etc.
				    0b10100000,
				    0b10100000,
				    0b11100000,
				    0b00100000,
				    0b00100000,

				    0b11110000,
				    0b10000000,
				    0b11110000,
				    0b00010000,
				    0b11110000,

				    0b10000000,
				    0b10000000,
				    0b11110000,
				    0b10010000,
				    0b11110000,

				    0b11110000,
				    0b00010000,
				    0b00100000,
				    0b00100000,
				    0b00100000,

				    0b11110000,
				    0b10010000,
				    0b11110000,
				    0b10010000,
				    0b11110000,

				    0b11110000,
				    0b10010000,
				    0b11110000,
				    0b00010000,
				    0b00010000,

				    0b01100000,
				    0b10010000,
				    0b11110000,
				    0b10010000,
				    0b10010000,

				    0b10000000,
				    0b10000000,
				    0b11110000,
				    0b10010000,
				    0b11110000,

				    0b11110000,
				    0b10000000,
				    0b10000000,
				    0b10000000,
				    0b11110000,

				    0b11100000,
				    0b10010000,
				    0b10010000,
				    0b10010000,
				    0b11100000,

				    0b11110000,
				    0b10000000,
				    0b11100000,
				    0b10000000,
				    0b11110000,

				    0b11110000,
				    0b10000000,
				    0b11100000,
				    0b10000000,
					0b10000000,
		};

		for(int i = 0; i < 16*5; ++i) memory[i] = fontsprites[i];
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
		printf("I: %04x\n", I);
		printf("PC: %04x\n", PC);
		printf("SP: %04x\n", SP);
		printf("top of stack: %04x\n", memory[SP]);
	}

};
