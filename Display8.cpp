#include <SDL2/SDL.h>


class Display8 {
public:

	const int SCREEN_WIDTH = 64;
	const int SCREEN_HEIGHT = 32;

	const int SCALE_FACTOR = 16;

	SDL_Window* window;
	SDL_Renderer* renderer;



	void initialise() {
		if(SDL_Init(SDL_INIT_VIDEO) >= 0) {
			SDL_CreateWindowAndRenderer(SCREEN_WIDTH * SCALE_FACTOR, SCREEN_HEIGHT * SCALE_FACTOR, SDL_WINDOW_SHOWN, &window, &renderer);

			SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF); //set initial color to black

			SDL_RenderClear(renderer);
		}
	}

	void draw(uint8_t* display) {
		int cellbytes = (SCREEN_WIDTH * SCREEN_HEIGHT) / 8;

		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

		SDL_Rect pixel;
		pixel.w = SCALE_FACTOR;
		pixel.h = SCALE_FACTOR;

		int bit;
		int pixnum;

		for(int i = 0; i < cellbytes; ++i) {
			for(int j = 7; j >= 0; --j) {
				bit = (1 << j);
				//bit to be drawn is at pixel number i*8 + (7-j)
				//thus coordinates are x: (pixel number) % 64, y: (pixel number) / 64

				pixnum = i*8 + (7-j);

				if(display[i] & bit) {
					pixel.x = pixnum % SCREEN_WIDTH;
					pixel.y = pixnum / SCREEN_WIDTH;
					SDL_RenderFillRect(renderer, &pixel);

				}
			}
		}

		SDL_RenderPresent(renderer);

		//D_Start
		SDL_Delay(2000);
		//D_End

	}


};
