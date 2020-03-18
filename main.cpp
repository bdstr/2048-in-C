#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <time.h>

extern "C"
{
#include "./SDL-2.0.7/include/SDL.h"
#include "./SDL-2.0.7/include/SDL_main.h"
}

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 1024

#define UP 0
#define DOWN 1
#define LEFT 2
#define RIGHT 3

#define ANIMATION_SPEED 5
#define DESIRED_FPS 60

struct Tile
{
	int value;
	int destination_value;
	int current_x;
	int current_y;
	int destination_x;
	int destination_y;
	bool was_merged;
};

//------------- FUNKCJE Z SZABLONU

// narysowanie napisu txt na powierzchni screen, zaczynajac od punktu (x, y)
// charset to bitmapa 128x128 zawierajaca znaki
void DrawString(SDL_Surface *screen, int x, int y, const char *text, SDL_Surface *charset)
{
	int px, py, c;
	SDL_Rect s, d;
	s.w = 14;
	s.h = 24;
	d.w = 14;
	d.h = 24;
	while (*text)
	{
		c = *text & 255;
		px = (c % 16) * 14;
		py = (c / 16) * 24;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 14;
		text++;
	};
};

// narysowanie na ekranie screen powierzchni sprite w punkcie (x, y)
// (x, y) to punkt srodka obrazka sprite na ekranie
void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y)
{
	SDL_Rect dest;
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
};

// rysowanie pojedynczego pixela
void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color)
{
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
	*(Uint32 *)p = color;
};

// rysowanie linii o dlugosci l w pionie (gdy dx = 0, dy = 1)
// blad poziomie (gdy dx = 1, dy = 0)
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color)
{
	for (int i = 0; i < l; i++)
	{
		DrawPixel(screen, x, y, color);
		x += dx;
		y += dy;
	};
};

// rysowanie prostokata o dlugosci bokow l i k
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k, Uint32 outlineColor, Uint32 fillColor)
{
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
	for (i = y + 1; i < y + k - 1; i++)
		DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
};

//------------- KONIEC FUNKCJI Z SZABLONU

// Alokacja pamieci dla kafelkow
Tile *init(int field_size)
{
	int tiles_quantity = 6 * 6;
	struct Tile *tile = (struct Tile *)malloc(tiles_quantity * sizeof(struct Tile));

	return tile;
}

// Menu wyboru wielkosci planszy
int field_size_choice_menu(SDL_Renderer *renderer, SDL_Texture *scrtex, SDL_Surface *screen, SDL_Surface *charset, SDL_Event *event, int foregroud, int backgroud)
{
	int field_size = 4;
	bool menu_quit = false;
	char *buff = (char *)malloc(sizeof(char) * 40);

	printf("Tworzenie graficznego menu...\n");
	while (!menu_quit)
	{
		DrawRectangle(screen, screen->w / 2 - strlen(buff) * 14 / 2 - 30, (SCREEN_HEIGHT / 2) - 90, strlen(buff) * 14 + 60, 180, foregroud, backgroud);
		for (int i = 0; i < 4; i++)
		{
			//podswietlenie wybranego rekordu
			if (field_size == i + 3)
			{
				DrawRectangle(screen, screen->w / 2 - strlen(buff) * 14 / 2 - 30, (SCREEN_HEIGHT / 2) - 94 + (i + 1) * 30, strlen(buff) * 14 + 60, 30, backgroud, foregroud);
			}
			sprintf(buff, "PLANSZA %d X %d", i + 3, i + 3);
			DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) - 90 + (i + 1) * 30, buff, charset);
		}
		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(event))
		{
			switch (event->type)
			{
			case SDL_KEYDOWN:
				if (event->key.keysym.sym == SDLK_RETURN)
				{
					menu_quit = true;
				}
				else if (event->key.keysym.sym == SDLK_UP)
				{
					field_size = (field_size == 3 ? 3 : --field_size);
				}
				else if (event->key.keysym.sym == SDLK_DOWN)
				{
					field_size = (field_size == 6 ? 6 : ++field_size);
				}
				break;
			}
		}
	}
	free(buff);
	return field_size;
}

// Sprawdzanie czy sasiaduja ze soba dwa pola o tej samej wartosci
bool check_if_move_can_be_done(int field_size, Tile *tile)
{
	for (int i = 0; i < field_size - 1; i++)
	{
		for (int j = 0; j < field_size - 1; j++)
		{
			if (tile[i * field_size + j].value == tile[i * field_size + j + 1].value)
			{
				return true;
			}
			if (tile[i * field_size + j].value == tile[(i + 1) * field_size + j].value)
			{
				return true;
			}
		}
	}
	for (int i = 0; i < field_size - 1; i++)
	{
		if (tile[i * field_size + field_size - 1].value == tile[(i + 1) * field_size + field_size - 1].value)
		{
			return true;
		}
		if (tile[(field_size - 1) * field_size + i].value == tile[(field_size - 1) * field_size + i + 1].value)
		{
			return true;
		}
	}
	return false;
}

// Sprawdzanie czy jest na planszy wolne pole
bool check_if_is_empty_field(int field_size, Tile *tile)
{
	for (int i = 0; i < field_size; i++)
	{
		for (int j = 0; j < field_size; j++)
		{
			if (tile[i * field_size + j].value == 0)
				return true;
		}
	}
	return false;
}

// Losowanie wolnego miejsca na planszy i umieszczenie tam nowego kafelka
void generate_new_tile(int field_size, Tile *tile, int starting_x, int starting_y, int *points)
{
	if (check_if_is_empty_field(field_size, tile) == true)
	{
		srand(time(NULL));
		bool end = false;
		*points += 2;
		while (end != true)
		{
			int y = rand() % field_size;
			int x = rand() % field_size;
			if (tile[field_size * y + x].value == 0)
			{
				tile[field_size * y + x].value = 2;
				tile[field_size * y + x].destination_value = 2;
				tile[field_size * y + x].current_x = starting_x + (x * 128);
				tile[field_size * y + x].current_y = starting_y + (y * 128);
				tile[field_size * y + x].destination_x = starting_x + (x * 128);
				tile[field_size * y + x].destination_y = starting_y + (y * 128);
				end = true;
			}
		}
		printf("Wygenerowano kafelek!\n");
	}
}

// Tworzenie nowej gry, ustawianie poczatkowych wartosci zmiennych
void new_game(Tile *tile, Tile *tile_to_undo, int field_size, int *starting_x, int *starting_y, double *world_time, bool *end_game, int *points, bool *first_move_occured, int *undo_variables)
{
	printf("Zerowanie zmiennych...\n");
	*world_time = 0;
	*end_game = false;
	*first_move_occured = false;
	*points = 0;

	//[0] - end_game  [1] - first_move_occured  [2] - points
	printf("Zerowanie tablicy undo...\n");
	undo_variables[0] = (*end_game == true ? 1 : 0);
	undo_variables[1] = (*first_move_occured == true ? 1 : 0);
	undo_variables[0] = *points;

	printf("Ustawianie punktow poczatkowych...\n");
	*starting_x = (SCREEN_WIDTH / 2) - (field_size % 2 == 0 ? 64 : 0) - 128 * (field_size > 4 ? 2 : 1);
	*starting_y = (SCREEN_HEIGHT / 2) - (field_size % 2 == 0 ? 64 : 0) - 128 * (field_size > 4 ? 2 : 1);

	printf("Ustawianie poczatkowych wartosci kafelkow...\n");
	for (int y = 0; y < field_size; y++)
	{
		for (int x = 0; x < field_size; x++)
		{
			tile[field_size * y + x].value = 0;
			tile[field_size * y + x].destination_value = 0;
			tile[field_size * y + x].current_x = *starting_x + (x * 128);
			tile[field_size * y + x].current_y = *starting_y + (y * 128);
			tile[field_size * y + x].destination_x = *starting_x + (x * 128);
			tile[field_size * y + x].destination_y = *starting_y + (y * 128);
		}
	}

	for (int y = 0; y < field_size; y++)
	{
		for (int x = 0; x < field_size; x++)
		{
			tile_to_undo[field_size * y + x].value = 0;
			tile_to_undo[field_size * y + x].destination_value = 0;
			tile_to_undo[field_size * y + x].current_x = *starting_x + (x * 128);
			tile_to_undo[field_size * y + x].current_y = *starting_y + (y * 128);
			tile_to_undo[field_size * y + x].destination_x = *starting_x + (x * 128);
			tile_to_undo[field_size * y + x].destination_y = *starting_y + (y * 128);
		}
	}

	printf("Generowanie poczatkowych kafelkow...\n");
	generate_new_tile(field_size, tile, *starting_x, *starting_y, points);
	generate_new_tile(field_size, tile, *starting_x, *starting_y, points);
}

// Poruszanie sie kafelkow
void move(int direction, int field_size, Tile *tile, int starting_x, int starting_y, bool *end_game, int *points, bool *first_move_occured)
{
	if (*first_move_occured == false)
		*first_move_occured = true;
	bool any_move_occured = false;
	for (int i = 0; i < field_size * field_size; i++)
		tile[i].was_merged = false;

	if (direction == UP)
	{
		for (int i = 1; i < field_size; i++)
		{
			for (int j = 0; j < field_size; j++)
			{
				int y = i;
				bool end = false;
				while (!end)
				{
					if (tile[field_size * y + j].value > 0 && (y - 1) >= 0 && tile[field_size * (y - 1) + j].was_merged == false)
					{
						if (tile[field_size * (y - 1) + j].value == 0)
						{
							tile[field_size * (y - 1) + j].value = tile[field_size * y + j].value;
							tile[field_size * (y - 1) + j].destination_value = tile[field_size * y + j].destination_value;
							tile[field_size * y + j].value = 0;
							tile[field_size * y + j].destination_value = 0;

							tile[field_size * (y - 1) + j].current_x = tile[field_size * y + j].current_x;
							tile[field_size * (y - 1) + j].current_y = tile[field_size * y + j].current_y;
							tile[field_size * (y - 1) + j].destination_x = starting_x + (j * 128);
							tile[field_size * (y - 1) + j].destination_y = starting_y + ((y - 1) * 128);

							any_move_occured = true;
						}
						else if (tile[field_size * (y - 1) + j].value == tile[field_size * y + j].value)
						{
							tile[field_size * (y - 1) + j].destination_value = 2 * tile[field_size * (y - 1) + j].value;
							tile[field_size * y + j].value = 0;
							tile[field_size * (y - 1) + j].was_merged = true;
							*points += tile[field_size * (y - 1) + j].destination_value;
							end = true;

							tile[field_size * (y - 1) + j].current_x = tile[field_size * y + j].current_x;
							tile[field_size * (y - 1) + j].current_y = tile[field_size * y + j].current_y;
							tile[field_size * (y - 1) + j].destination_x = starting_x + (j * 128);
							tile[field_size * (y - 1) + j].destination_y = starting_y + ((y - 1) * 128);

							any_move_occured = true;
						}
					}
					else
					{
						end = true;
					}
					y--;
				}
			}
		}
	}
	else if (direction == DOWN)
	{
		for (int i = field_size - 2; i >= 0; i--)
		{
			for (int j = 0; j < field_size; j++)
			{
				int y = i;
				bool end = false;
				while (!end)
				{
					if (tile[field_size * y + j].value > 0 && (y + 1) < field_size && tile[field_size * (y + 1) + j].was_merged == false)
					{
						if (tile[field_size * (y + 1) + j].value == 0)
						{
							tile[field_size * (y + 1) + j].value = tile[field_size * y + j].value;
							tile[field_size * (y + 1) + j].destination_value = tile[field_size * y + j].destination_value;
							tile[field_size * y + j].value = 0;
							tile[field_size * y + j].destination_value = 0;

							tile[field_size * (y + 1) + j].current_x = tile[field_size * y + j].current_x;
							tile[field_size * (y + 1) + j].current_y = tile[field_size * y + j].current_y;
							tile[field_size * (y + 1) + j].destination_x = starting_x + (j * 128);
							tile[field_size * (y + 1) + j].destination_y = starting_y + ((y + 1) * 128);

							any_move_occured = true;
						}
						else if (tile[field_size * (y + 1) + j].value == tile[field_size * y + j].value)
						{
							tile[field_size * (y + 1) + j].destination_value = 2 * tile[field_size * (y + 1) + j].value;
							tile[field_size * y + j].value = 0;
							tile[field_size * (y + 1) + j].was_merged = true;
							*points += tile[field_size * (y + 1) + j].destination_value;
							end = true;

							tile[field_size * (y + 1) + j].current_x = tile[field_size * y + j].current_x;
							tile[field_size * (y + 1) + j].current_y = tile[field_size * y + j].current_y;
							tile[field_size * (y + 1) + j].destination_x = starting_x + (j * 128);
							tile[field_size * (y + 1) + j].destination_y = starting_y + ((y + 1) * 128);

							any_move_occured = true;
						}
					}
					else
					{
						end = true;
					}
					y++;
				}
			}
		}
	}
	else if (direction == LEFT)
	{
		for (int i = 0; i < field_size; i++)
		{
			for (int j = 1; j < field_size; j++)
			{
				int x = j;
				bool end = false;
				while (!end)
				{
					if (tile[field_size * i + x].value > 0 && (x - 1) >= 0 && tile[field_size * i + x - 1].was_merged == false)
					{
						if (tile[field_size * i + x - 1].value == 0)
						{
							tile[field_size * i + x - 1].value = tile[field_size * i + x].value;
							tile[field_size * i + x - 1].destination_value = tile[field_size * i + x].destination_value;
							tile[field_size * i + x].value = 0;
							tile[field_size * i + x].destination_value = 0;

							tile[field_size * i + x - 1].current_x = tile[field_size * i + x].current_x;
							tile[field_size * i + x - 1].current_y = tile[field_size * i + x].current_y;
							tile[field_size * i + x - 1].destination_x = starting_x + ((x - 1) * 128);
							tile[field_size * i + x - 1].destination_y = starting_y + (i * 128);

							any_move_occured = true;
						}
						else if (tile[field_size * i + x - 1].value == tile[field_size * i + x].value)
						{
							tile[field_size * i + x - 1].destination_value = 2 * tile[field_size * i + x - 1].value;
							tile[field_size * i + x].value = 0;
							tile[field_size * i + x - 1].was_merged = true;
							*points += tile[field_size * i + x - 1].destination_value;
							end = true;

							tile[field_size * i + x - 1].current_x = tile[field_size * i + x].current_x;
							tile[field_size * i + x - 1].current_y = tile[field_size * i + x].current_y;
							tile[field_size * i + x - 1].destination_x = starting_x + ((x - 1) * 128);
							tile[field_size * i + x - 1].destination_y = starting_y + (i * 128);

							any_move_occured = true;
						}
					}
					else
					{
						end = true;
					}
					x--;
				}
			}
		}
	}
	else if (direction == RIGHT)
	{
		for (int i = 0; i < field_size; i++)
		{
			for (int j = field_size - 2; j >= 0; j--)
			{
				int x = j;
				bool end = false;
				while (!end)
				{
					if (tile[field_size * i + x].value > 0 && (x + 1) < field_size && tile[field_size * i + x + 1].was_merged == false)
					{
						if (tile[field_size * i + x + 1].value == 0)
						{
							tile[field_size * i + x + 1].value = tile[field_size * i + x].value;
							tile[field_size * i + x + 1].destination_value = tile[field_size * i + x].destination_value;
							tile[field_size * i + x].value = 0;
							tile[field_size * i + x].destination_value = 0;

							tile[field_size * i + x + 1].current_x = tile[field_size * i + x].current_x;
							tile[field_size * i + x + 1].current_y = tile[field_size * i + x].current_y;
							tile[field_size * i + x + 1].destination_x = starting_x + ((x + 1) * 128);
							tile[field_size * i + x + 1].destination_y = starting_y + (i * 128);

							any_move_occured = true;
						}
						else if (tile[field_size * i + x + 1].value == tile[field_size * i + x].value)
						{
							tile[field_size * i + x + 1].destination_value = 2 * tile[field_size * i + x + 1].value;
							tile[field_size * i + x].value = 0;
							tile[field_size * i + x + 1].was_merged = true;
							*points += tile[field_size * i + x + 1].destination_value;
							end = true;

							tile[field_size * i + x + 1].current_x = tile[field_size * i + x].current_x;
							tile[field_size * i + x + 1].current_y = tile[field_size * i + x].current_y;
							tile[field_size * i + x + 1].destination_x = starting_x + ((x + 1) * 128);
							tile[field_size * i + x + 1].destination_y = starting_y + (i * 128);

							any_move_occured = true;
						}
					}
					else
					{
						end = true;
					}
					x++;
				}
			}
		}
	}

	if (any_move_occured == true)
	{
		generate_new_tile(field_size, tile, starting_x, starting_y, points);
	}

	for (int i = 0; i < field_size; i++)
	{
		for (int j = 0; j < field_size; j++)
		{
			printf("[%i] ", tile[field_size * i + j].destination_value);
		}
		printf("\n");
	}
	printf("\n");
}

// Zapisywanie ustawienia kafelkow do przywrocenia
void save_to_undo(Tile *tile, Tile *tile_to_undo, int field_size, bool *end_game, int *points, bool *first_move_occured, int *undo_variables)
{
	//[0] - end_game  [1] - first_move_occured  [2] - points
	printf("Zapisywanie tablicy undo...\n");
	undo_variables[0] = (*end_game == true ? 1 : 0);
	undo_variables[1] = (*first_move_occured == true ? 1 : 0);
	undo_variables[2] = *points;

	printf("Zapisywanie kafelkow do undo...\n");
	for (int y = 0; y < field_size; y++)
	{
		for (int x = 0; x < field_size; x++)
		{
			tile_to_undo[field_size * y + x].value = tile[field_size * y + x].value;
			tile_to_undo[field_size * y + x].destination_value = tile[field_size * y + x].destination_value;
			tile_to_undo[field_size * y + x].current_x = tile[field_size * y + x].current_x;
			tile_to_undo[field_size * y + x].current_y = tile[field_size * y + x].current_y;
			tile_to_undo[field_size * y + x].destination_x = tile[field_size * y + x].destination_x;
			tile_to_undo[field_size * y + x].destination_y = tile[field_size * y + x].destination_y;
		}
	}
}

// Przywracanie poprzedniego ustawienia kafelkow
void load_undo(Tile *tile, Tile *tile_to_undo, int field_size, bool *end_game, int *points, bool *first_move_occured, int *undo_variables)
{
	printf("Cofanie ruchu...\n");
	//[0] - end_game  [1] - first_move_occured  [2] - points
	printf("Przywracanie tablicy undo...\n");
	*end_game = undo_variables[0];
	*first_move_occured = undo_variables[1];
	*points = undo_variables[2];

	printf("Przywracanie kafelkowz undo...\n");
	for (int y = 0; y < field_size; y++)
	{
		for (int x = 0; x < field_size; x++)
		{
			tile[field_size * y + x].value = tile_to_undo[field_size * y + x].value;
			tile[field_size * y + x].destination_value = tile_to_undo[field_size * y + x].destination_value;
			tile[field_size * y + x].current_x = tile_to_undo[field_size * y + x].current_x;
			tile[field_size * y + x].current_y = tile_to_undo[field_size * y + x].current_y;
			tile[field_size * y + x].destination_x = tile_to_undo[field_size * y + x].destination_x;
			tile[field_size * y + x].destination_y = tile_to_undo[field_size * y + x].destination_y;
		}
	}
}

// Zapisywanie obecnego stanu gry do pliku
void save_game_to_file(Tile *tile, int field_size, bool *end_game, int *points, double *world_time, int *starting_x, int *starting_y)
{
	printf("Zapisywanie stanu gry...\n");
	printf("Otwieranie pliku...\n");

	FILE *save_file;
	save_file = fopen("./save/save", "a");

	time_t rawtime;
	struct tm *timeinfo;
	char name[40];
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(name, 80, "%F_%X", timeinfo);

	printf("Tworzenie rekordu o dacie %s...\n", name);
	fprintf(save_file, "%s ", name);

	printf("Zapisywanie zmiennych do pliku...\n");
	fprintf(save_file, "%d ", field_size);
	fprintf(save_file, "%d ", (*end_game == true ? 1 : 0));
	fprintf(save_file, "%d ", *points);
	fprintf(save_file, "%.1lf ", *world_time);
	fprintf(save_file, "%d ", *starting_x);
	fprintf(save_file, "%d ", *starting_y);

	printf("Zapisywanie kafelkow do pliku...\n");
	for (int y = 0; y < field_size; y++)
	{
		for (int x = 0; x < field_size; x++)
		{
			fprintf(save_file, "%d ", tile[field_size * y + x].value);
			fprintf(save_file, "%d ", tile[field_size * y + x].destination_value);
			fprintf(save_file, "%d ", tile[field_size * y + x].current_x);
			fprintf(save_file, "%d ", tile[field_size * y + x].current_y);
			fprintf(save_file, "%d ", tile[field_size * y + x].destination_x);
			fprintf(save_file, "%d ", tile[field_size * y + x].destination_y);
		}
	}
	fprintf(save_file, "\n");
	fclose(save_file);
}

// Ladowanie stanu gry z pliku
void load_game_from_file(Tile *tile, int *field_size, bool *end_game, int *points, bool *first_move_occured, double *world_time, int *starting_x, int *starting_y, Tile *tile_to_undo, int *undo_variables, SDL_Renderer *renderer, SDL_Texture *scrtex, SDL_Surface *screen, SDL_Surface *charset, SDL_Event *event, int foregroud_color, int backgroud_color)
{
	printf("Przywracanie stanu gry...\n");
	bool menu_quit = false;
	int line_to_read = 1;
	printf("Otwieranie pliku...\n");

	FILE *read_file;
	read_file = fopen("./save/save", "r");
	if (read_file == NULL)
	{
		printf("Nie mozna otworzyc pliku!\n");
		return;
	}

	printf("Alokacja pamieci dla nazw zapisow...\n");
	char *buff = (char *)malloc(sizeof(char) * 40);
	char *line = (char *)malloc(sizeof(char) * 1000);
	int records_quantity = 0;

	char *names[99];
	names[records_quantity] = (char *)malloc(sizeof(char) * 40);

	printf("Czytanie rekordow...\n");
	while (fscanf(read_file, "%s %[^\n]\n", names[records_quantity], line) != EOF)
	{
		printf("Nazwa rekordu w pliku: %s\n", names[records_quantity]);
		records_quantity++;
		names[records_quantity] = (char *)malloc(sizeof(char) * 40);
	};

	printf("Tworzenie graficznego menu...\n");
	while (!menu_quit)
	{
		DrawRectangle(screen, screen->w / 2 - strlen(buff) * 14 / 2 - 30, (SCREEN_HEIGHT / 2) - 30 * (1 + records_quantity / 2), strlen(buff) * 14 + 60, 60 + records_quantity * 30, foregroud_color, backgroud_color);
		for (int i = 0; i < records_quantity; i++)
		{
			//podswietlenie wybranego rekordu
			if (line_to_read == i + 1)
			{
				DrawRectangle(screen, screen->w / 2 - strlen(buff) * 14 / 2 - 30, (SCREEN_HEIGHT / 2) - 30 * (1 + records_quantity / 2) - 4 + (i + 1) * 30, strlen(buff) * 14 + 60, 30, backgroud_color, foregroud_color);
			}
			sprintf(buff, "%d. %s", i + 1, names[i]);
			DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * (1 + records_quantity / 2) + (i + 1) * 30, buff, charset);
		}
		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(event))
		{
			switch (event->type)
			{
			case SDL_KEYDOWN:
				if (event->key.keysym.sym == SDLK_RETURN)
				{
					menu_quit = true;
				}
				else if (event->key.keysym.sym == SDLK_UP)
				{
					line_to_read = (line_to_read == 1 ? 1 : --line_to_read);
				}
				else if (event->key.keysym.sym == SDLK_DOWN)
				{
					line_to_read = (line_to_read == records_quantity ? records_quantity : ++line_to_read);
				}
				break;
			}
		}
	}

	fclose(read_file);
	read_file = fopen("./save/save", "r");

	int field_size_temp;
	int end_game_temp;
	int points_temp;
	double world_time_temp;
	int starting_x_temp;
	int starting_y_temp;
	printf("Pobieranie danych z rekordu %d...\n", line_to_read);
	for (int i = 1; i <= line_to_read; i++)
	{
		if (i == line_to_read)
		{
			//Jesli fscanf pobral 5 zmiennych program kontynuuje
			if (fscanf(read_file, "%s %d %d %d %lf %d %d", names[i - 1], &field_size_temp, &end_game_temp, &points_temp, &world_time_temp, &starting_x_temp, &starting_y_temp) == 7)
			{
				printf("field_size: %i; end_game: %i; points: %i; world_time: %lf\n", field_size_temp, end_game_temp, points_temp, world_time_temp);
				*field_size = field_size_temp;

				printf("Tworzenie nowej gry...\n");
				// Najpierw tworzy nowa gre zeby zaalokowac odpowiednia ilosc pamieci
				new_game(tile, tile_to_undo, *field_size, starting_x, starting_y, world_time, end_game, points, first_move_occured, undo_variables);

				printf("Przywracanie zapisanych zmiennych...\n");
				// Potem przywraca zapisane w pliku wartosci
				*end_game = (end_game_temp == 1 ? true : false);
				*first_move_occured = false;
				*points = points_temp;
				*world_time = world_time_temp;
				*starting_x = starting_x_temp;
				*starting_y = starting_y_temp;

				for (int y = 0; y < *field_size; y++)
				{
					for (int x = 0; x < *field_size; x++)
					{
						int return_value = 0;
						int value_temp, destination_value_temp, current_x_temp, current_y_temp, destination_x_temp, destination_y_temp;
						return_value += fscanf(read_file, "%d ", &value_temp);
						return_value += fscanf(read_file, "%d ", &destination_value_temp);
						return_value += fscanf(read_file, "%d ", &current_x_temp);
						return_value += fscanf(read_file, "%d ", &current_y_temp);
						return_value += fscanf(read_file, "%d ", &destination_x_temp);
						return_value += fscanf(read_file, "%d ", &destination_y_temp);

						tile[*field_size * y + x].value = value_temp;
						tile[*field_size * y + x].destination_value = destination_value_temp;
						tile[*field_size * y + x].current_x = current_x_temp;
						tile[*field_size * y + x].current_y = current_y_temp;
						tile[*field_size * y + x].destination_x = destination_x_temp;
						tile[*field_size * y + x].destination_y = destination_y_temp;

						printf("[%i %i] ", tile[*field_size * y + x].value, tile[*field_size * y + x].destination_value);
					}
					printf("\n");
				}

				printf("Zaladowano zapis %s!\n", names[line_to_read - 1]);
			}
			else
			{
				printf("Wystapil blad przy ladowaniu danych!");
			}
		}
		else
		{
			if (fscanf(read_file, "%[^\n]\n", line) == EOF)
			{
				printf("Wystapil blad przy ladowaniu danych!");
			}
		}
	}
	fclose(read_file);
	free(buff);
	free(line);
}

// Zapisywanie po skonczonej grze wyniku gry do pliku
bool save_to_rank(int field_size, int points, double world_time, SDL_Renderer *renderer, SDL_Texture *scrtex, SDL_Surface *screen, SDL_Surface *charset, SDL_Event *event, int foregroud, int backgroud)
{
	bool menu_quit = false;
	char *buff = (char *)malloc(sizeof(char) * 37);
	char *name = (char *)malloc(sizeof(char) * 30);
	name[0] = '\0';

	printf("Wyswietlanie pola do wpisania imienia...\n");
	while (!menu_quit)
	{
		DrawRectangle(screen, screen->w / 2 - 31 * 14 / 2 - 40, (SCREEN_HEIGHT / 2) + 63, 31 * 14 + 80, 50, foregroud, foregroud);

		sprintf(buff, "Imie: %s", name);
		DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) + 63, buff, charset);

		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(event))
		{
			switch (event->type)
			{
			case SDL_KEYDOWN:
				if ((event->key.keysym.sym >= 'a' && event->key.keysym.sym <= 'z') || (event->key.keysym.sym >= '0' && event->key.keysym.sym <= '9'))
				{
					if (strlen(buff) < 30)
					{
						char letter[2] = "";
						if (event->key.keysym.sym >= 97 && event->key.keysym.sym <= 122)
						{
							letter[0] = event->key.keysym.sym - 32;
						}
						else
						{
							letter[0] = event->key.keysym.sym;
						}
						printf("%i  %s\n", event->key.keysym.sym, letter);
						strcat(name, letter);
						printf("Imie: %s\n", name);
					}
				}
				else if (event->key.keysym.sym == SDLK_BACKSPACE)
				{
					name[strlen(name) - 1] = '\0';
				}
				else if (event->key.keysym.sym == SDLK_RETURN)
				{
					DrawRectangle(screen, screen->w / 2 - 31 * 14 / 2 - 40, (SCREEN_HEIGHT / 2) + 63, 31 * 14 + 80, 50, foregroud, foregroud);
					menu_quit = true;
					printf("Zapisywanie wyniku gry...\n");
					printf("Otwieranie pliku...\n");

					FILE *save_file;
					save_file = fopen("./save/rank", "a");

					printf("Zapisywanie do pliku...\n");
					fprintf(save_file, "%s ", name);
					fprintf(save_file, "%d ", field_size);
					fprintf(save_file, "%d ", points);
					fprintf(save_file, "%.1lf\n", world_time);
					fclose(save_file);
					free(buff);
					return true;
				}
				else if (event->key.keysym.sym == SDLK_ESCAPE)
				{
					DrawRectangle(screen, screen->w / 2 - 31 * 14 / 2 - 40, (SCREEN_HEIGHT / 2) + 63, 31 * 14 + 80, 50, foregroud, foregroud);
					menu_quit = true;
					free(buff);
					return false;
				}
				break;
			}
		}
	}
	free(buff);
	return false;
}

// Odczytywanie i wyswietlanie rankingu wynikow gry z pliku
void show_rank(SDL_Renderer *renderer, SDL_Texture *scrtex, SDL_Surface *screen, SDL_Surface *charset, SDL_Event *event, int foregroud, int backgroud)
{
	printf("Ladowanie rankingu...\n");
	bool menu_quit = false;
	int field_size_to_show = 4;
	bool sort_by_points = true;

	printf("Otwieranie pliku...\n");
	FILE *read_file;
	read_file = fopen("./save/rank", "r");
	if (read_file == NULL)
	{
		printf("Nie mozna otworzyc pliku!\n");
		return;
	}

	printf("Alokacja pamieci dla zapisow...\n");
	char *buff = (char *)malloc(sizeof(char) * 50);
	char *temp_name = (char *)malloc(sizeof(char) * 50);
	int temp_field_size;
	int temp_points;
	double temp_time;

	int records_quantity;
	char *rank[30];

	//sortowanie rankingu
	records_quantity = 0;
	while (fscanf(read_file, "%s %d %d %lf\n", temp_name, &temp_field_size, &temp_points, &temp_time) != EOF)
	{
		if (temp_field_size == field_size_to_show)
		{
			rank[records_quantity] = (char *)malloc(sizeof(char) * 50);
			records_quantity++;
		}
	}
	fclose(read_file);

	printf("Ilosc rekordow: %d\n", records_quantity);
	printf("Czytanie rekordow...\n");
	int last_max_points = 100000;
	double last_min_time = 100000;
	for (int i = 0; i < 30 && i < records_quantity; i++)
	{
		read_file = fopen("./save/rank", "r");
		int max_points = 0;
		int min_time = 0;
		while (fscanf(read_file, "%s %d %d %lf\n", temp_name, &temp_field_size, &temp_points, &temp_time) != EOF)
		{
			if (sort_by_points == true && temp_points > max_points && temp_points < last_max_points && field_size_to_show == temp_field_size)
			{
				max_points = temp_points;
				sprintf(rank[i], "%s  %d  %.1lf", temp_name, temp_points, temp_time);
			}
			else if (sort_by_points == false && temp_time > min_time && temp_time < last_min_time && field_size_to_show == temp_field_size)
			{
				min_time = temp_time;
				sprintf(rank[i], "%s  %d  %.1lf", temp_name, temp_points, temp_time);
			}
		}
		last_min_time = min_time;
		last_max_points = max_points;
		fclose(read_file);
	}
	//===
	for (int i = 0; i < 30 && i < records_quantity; i++)
	{
		printf("%s\n", rank[i]);
	}

	printf("Wyswietlanie rankingu...\n");
	while (!menu_quit)
	{
		DrawRectangle(screen, screen->w / 2 - 40 * 14 / 2 - 29, (SCREEN_HEIGHT / 2) - 30 * 16, 40 * 14 + 60, 60 + 30 * 30, foregroud, backgroud);

		//podswietlenie wyboru wielkosci
		DrawRectangle(screen, screen->w / 2 - 12 * 14 / 2 + (field_size_to_show - 3) * 14 * 4, (SCREEN_HEIGHT / 2) - 30 * 16, 3 * 14, 26, backgroud, foregroud);

		DrawString(screen, screen->w / 2 - 12 * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * 16, "3X3", charset);
		DrawString(screen, screen->w / 2 - 4 * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * 16, "4X4", charset);
		DrawString(screen, screen->w / 2 + 4 * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * 16, "5X5", charset);
		DrawString(screen, screen->w / 2 + 12 * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * 16, "6X6", charset);

		for (int i = 0; i < records_quantity; i++)
		{
			sprintf(buff, "%d. %s", i + 1, rank[i]);
			DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) - 30 * (1 + records_quantity / 2) + (i + 1) * 30, buff, charset);
		}
		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(event))
		{
			switch (event->type)
			{
			case SDL_KEYDOWN:
				if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_ESCAPE)
				{
					menu_quit = true;
				}
				else if (event->key.keysym.sym == SDLK_LEFT)
				{
					field_size_to_show = (field_size_to_show == 3 ? 3 : --field_size_to_show);
				}
				else if (event->key.keysym.sym == SDLK_RIGHT)
				{
					field_size_to_show = (field_size_to_show == 6 ? 6 : ++field_size_to_show);
				}
				else if (event->key.keysym.sym == SDLK_p)
				{
					sort_by_points = true;
				}
				else if (event->key.keysym.sym == SDLK_t)
				{
					sort_by_points = false;
				}

				//sortowanie rankingu
				records_quantity = 0;
				read_file = fopen("./save/rank", "r");
				while (fscanf(read_file, "%s %d %d %lf\n", temp_name, &temp_field_size, &temp_points, &temp_time) != EOF)
				{
					if (temp_field_size == field_size_to_show)
					{
						rank[records_quantity] = (char *)malloc(sizeof(char) * 50);
						records_quantity++;
					}
				}
				fclose(read_file);

				printf("Ilosc rekordow: %d\n", records_quantity);
				printf("Czytanie rekordow...\n");
				int last_max_points = 100000;
				double last_min_time = 100000;
				for (int i = 0; i < 30 && i < records_quantity; i++)
				{
					read_file = fopen("./save/rank", "r");
					int max_points = 0;
					int min_time = 0;
					while (fscanf(read_file, "%s %d %d %lf\n", temp_name, &temp_field_size, &temp_points, &temp_time) != EOF)
					{
						if (sort_by_points == true && temp_points > max_points && temp_points < last_max_points && field_size_to_show == temp_field_size)
						{
							max_points = temp_points;
							sprintf(rank[i], "%s  %d  %.1lf", temp_name, temp_points, temp_time);
						}
						else if (sort_by_points == false && temp_time > min_time && temp_time < last_min_time && field_size_to_show == temp_field_size)
						{
							min_time = temp_time;
							sprintf(rank[i], "%s  %d  %.1lf", temp_name, temp_points, temp_time);
						}
					}
					last_min_time = min_time;
					last_max_points = max_points;
					fclose(read_file);
				}
				//===
				for (int i = 0; i < 30 && i < records_quantity; i++)
				{
					printf("%s\n", rank[i]);
				}
			}
		}
	}
	free(buff);
	free(temp_name);
}

// Wyswietlanie informacji o skonczonej grze
bool end_game_screen(int field_size, int points, double world_time, SDL_Renderer *renderer, SDL_Texture *scrtex, SDL_Surface *screen, SDL_Surface *charset, SDL_Event *event, int foregroud, int backgroud)
{
	bool menu_quit = false;
	char *buff = (char *)malloc(sizeof(char) * 40);

	printf("Tworzenie okna konca gry...\n");
	while (!menu_quit)
	{
		//buff zawira najdluzszy tekst aby okno bylo odpowiednio szerokie
		sprintf(buff, "N - nowa gra   R - zapisz wynik");
		DrawRectangle(screen, screen->w / 2 - strlen(buff) * 14 / 2 - 40, (SCREEN_HEIGHT / 2) - 97, strlen(buff) * 14 + 80, 180, backgroud, foregroud);

		sprintf(buff, "KONIEC GRY!");
		DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) - 57, buff, charset);
		sprintf(buff, "Punkty: %d Czas: %.1f", points, world_time);
		DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) - 27, buff, charset);
		sprintf(buff, "N - nowa gra   R - zapisz wynik");
		DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) + 3, buff, charset);
		sprintf(buff, "ESC - wyjscie");
		DrawString(screen, screen->w / 2 - strlen(buff) * 14 / 2, (SCREEN_HEIGHT / 2) + 33, buff, charset);

		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		while (SDL_PollEvent(event))
		{
			switch (event->type)
			{
			case SDL_KEYDOWN:
				if (event->key.keysym.sym == SDLK_n)
				{
					menu_quit = true;
					free(buff);
					return true;
				}
				else if (event->key.keysym.sym == SDLK_r)
				{
					if (save_to_rank(field_size, points, world_time, renderer, scrtex, screen, charset, event, foregroud, backgroud) == true)
					{
						printf("Zapisano wynik do rankingu!\n");
					}
					else
					{
						printf("Nie udalo sie zapisac wyniku do rankingu!\n");
					}
				}
				else if (event->key.keysym.sym == SDLK_ESCAPE)
				{
					menu_quit = true;
					free(buff);
					return false;
				}
				break;
			}
		}
	}
	free(buff);
	return false;
}

// MAIN

#ifdef __cplusplus
extern "C"
#endif

	int
	main(int argc, char **argv)
{
	int t1, t2, quit, rc;
	double delta, world_time;
	SDL_Event event;
	SDL_Surface *screen, *charset;
	SDL_Surface *backgroud[4];
	SDL_Surface *tile_backgroud[12];
	SDL_Texture *scrtex;
	SDL_Window *window;
	SDL_Renderer *renderer;

	int field_size;
	int points;
	int starting_x;
	int starting_y;
	bool end_game;
	bool first_move_occured;

	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
	{
		printf("SDL_Init error: %s\n", SDL_GetError());
		return 1;
	}

	printf("Tworzenie okna...\n");
	rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer);
	if (rc != 0)
	{
		SDL_Quit();
		printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
		return 1;
	};

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	SDL_SetWindowTitle(window, "2048 by Marcin Kloczkowski 175684");

	screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	scrtex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

	// wylaczenie widocznosci kursora myszy
	SDL_ShowCursor(SDL_DISABLE);

	printf("Ladowanie pliku z czcionka...\n");
	charset = SDL_LoadBMP("./graphics/charset24x14.bmp");
	if (charset == NULL)
	{
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};
	SDL_SetColorKey(charset, true, 0x000000);

	printf("Ladowanie grafik tla...\n");
	backgroud[0] = SDL_LoadBMP("./graphics/backgroud3x3.bmp");
	backgroud[1] = SDL_LoadBMP("./graphics/backgroud4x4.bmp");
	backgroud[2] = SDL_LoadBMP("./graphics/backgroud5x5.bmp");
	backgroud[3] = SDL_LoadBMP("./graphics/backgroud6x6.bmp");
	if (backgroud[0] == NULL || backgroud[1] == NULL || backgroud[2] == NULL || backgroud[3] == NULL)
	{
		printf("SDL_LoadBMP(graphics/backgroud) error: %s\n", SDL_GetError());
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
	};

	printf("Ladowanie grafik kafelkow...\n");
	for (int i = 1; i < 12; i++)
	{
		int name_suffix = pow(2, i);
		char name[30];
		sprintf(name, "./graphics/tile%d.bmp", name_suffix);
		tile_backgroud[i] = SDL_LoadBMP(name);
		if (tile_backgroud[i] == NULL)
		{
			printf("%d. SDL_LoadBMP(/graphics/tile%d.bmp) error: %s\n", i, name_suffix, SDL_GetError());
			SDL_FreeSurface(charset);
			SDL_FreeSurface(screen);
			SDL_DestroyTexture(scrtex);
			SDL_DestroyWindow(window);
			SDL_DestroyRenderer(renderer);
			SDL_Quit();
			return 1;
		}
		printf("%d. SDL_LoadBMP(/graphics/tile%d.bmp)\n", i, name_suffix);
	}

	int backgroud_color = SDL_MapRGB(screen->format, 0xDC, 0xDC, 0xDC);
	int menu_foreground_color = SDL_MapRGB(screen->format, 0x22, 0x22, 0x22);
	int menu_background_color = SDL_MapRGB(screen->format, 0x8C, 0x8C, 0x8C);

	t1 = SDL_GetTicks();

	printf("Menu wyboru wielkosci planszy...\n");
	field_size = field_size_choice_menu(renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);

	printf("Alokowanie pamieci dla kafelkow...\n");
	Tile *tile;
	tile = init(field_size);

	Tile *tile_to_undo;
	tile_to_undo = init(field_size);
	int *undo_variables = (int *)malloc(3 * sizeof(int));

	printf("Tworzenie nowej gry...\n");
	new_game(tile, tile_to_undo, field_size, &starting_x, &starting_y, &world_time, &end_game, &points, &first_move_occured, undo_variables);

	char text[128];
	quit = 0;

	while (!quit)
	{
		t2 = SDL_GetTicks();

		// Warunek ograniczajacy pojawianie sie kolejnych klatek na co (1000 / DESIRED_FPS) milisekund
		// co daje stala liczbe fps-ow
		if (t2 - t1 >= (1000 / DESIRED_FPS))
		{

			// w tym momencie t2-t1 to czas w milisekundach,
			// jaki uplynal od ostatniego narysowania ekranu
			// delta to ten sam czas w sekundach
			delta = (t2 - t1) * 0.001;
			world_time += delta;

			t1 = t2;

			// kolor tla
			SDL_FillRect(screen, NULL, backgroud_color);

			// tekst informacyjny
			DrawRectangle(screen, 4, 4, SCREEN_WIDTH - 8, 64, menu_foreground_color, menu_background_color);
			sprintf(text, "Czas trwania = %.1lf s    Punkty: %i", world_time, points);
			DrawString(screen, screen->w / 2 - strlen(text) * 14 / 2, 10, text, charset);
			sprintf(text, "Esc - wyjscie, \030 \031 \032 \033 - ruch, U - cofniecie ruchu, N - nowa gra");
			DrawString(screen, screen->w / 2 - strlen(text) * 14 / 2, 42, text, charset);

			// grafika tla zalezna od wielkosci planszy
			if (field_size == 3)
			{
				DrawSurface(screen, backgroud[0], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
			}
			else if (field_size == 4)
			{
				DrawSurface(screen, backgroud[1], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
			}
			else if (field_size == 5)
			{
				DrawSurface(screen, backgroud[2], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
			}
			else if (field_size == 6)
			{
				DrawSurface(screen, backgroud[3], SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
			}

			int x, y;
			bool all_tiles_on_dest_position = true;
			for (int i = 0; i < field_size; i++)
			{
				for (int j = 0; j < field_size; j++)
				{
					if (tile[field_size * i + j].value > 0)
					{
						if (tile[field_size * i + j].current_x != tile[field_size * i + j].destination_x)
						{
							if (tile[field_size * i + j].current_x < tile[field_size * i + j].destination_x)
								x = tile[field_size * i + j].current_x + pow(2, ANIMATION_SPEED);
							else
								x = tile[field_size * i + j].current_x - pow(2, ANIMATION_SPEED);
							tile[field_size * i + j].current_x = x;
						}
						else
							x = tile[field_size * i + j].current_x;

						if (tile[field_size * i + j].current_y != tile[field_size * i + j].destination_y)
						{
							if (tile[field_size * i + j].current_y < tile[field_size * i + j].destination_y)
								y = tile[field_size * i + j].current_y + pow(2, ANIMATION_SPEED);
							else
								y = tile[field_size * i + j].current_y - pow(2, ANIMATION_SPEED);
							tile[field_size * i + j].current_y = y;
						}
						else
							y = tile[field_size * i + j].current_y;

						if (tile[field_size * i + j].current_x == tile[field_size * i + j].destination_x && tile[field_size * i + j].current_y == tile[field_size * i + j].destination_y)
						{
							tile[field_size * i + j].value = tile[field_size * i + j].destination_value;
						}
						else
						{
							all_tiles_on_dest_position = false;
						}

						int tile_backgroud_index = log2(tile[field_size * i + j].value);

						DrawSurface(screen, tile_backgroud[tile_backgroud_index], x, y);

						//Sprawdzanie czy nastapil koniec gry przez zdobycie 2048
						if (tile[field_size * i + j].value == 2048)
						{
							printf("KONIEC GRY!\n");
							end_game = true;
							if (end_game_screen(field_size, points, world_time, renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color) == true)
							{
								printf("Wybrano nowa gre!\n");
								field_size = field_size_choice_menu(renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);
								new_game(tile, tile_to_undo, field_size, &starting_x, &starting_y, &world_time, &end_game, &points, &first_move_occured, undo_variables);
							}
							else
							{
								printf("Wybrano wyjscie!\n");
								quit = 1;
							}
						}
					}
				}
			}

			// Wyrenderowanie obrazu
			SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
			SDL_RenderClear(renderer);
			SDL_RenderCopy(renderer, scrtex, NULL, NULL);
			SDL_RenderPresent(renderer);

			//Sprawdzanie czy nastapil koniec gry przez brak ruchu
			if (all_tiles_on_dest_position == true && end_game == false)
			{
				if (check_if_is_empty_field(field_size, tile) == false && check_if_move_can_be_done(field_size, tile) == false)
				{
					printf("KONIEC GRY!\n");
					end_game = true;
					if (end_game_screen(field_size, points, world_time, renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color) == true)
					{
						field_size = field_size_choice_menu(renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);
						new_game(tile, tile_to_undo, field_size, &starting_x, &starting_y, &world_time, &end_game, &points, &first_move_occured, undo_variables);
					}
					else
					{
						quit = 1;
					}
				}
			}

			// obsluga zdarzen (o ile jakies zaszly)
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE)
						quit = 1;
					else if (event.key.keysym.sym == SDLK_n)
					{
						field_size = field_size_choice_menu(renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);
						new_game(tile, tile_to_undo, field_size, &starting_x, &starting_y, &world_time, &end_game, &points, &first_move_occured, undo_variables);
					}
					else if (event.key.keysym.sym == SDLK_u && first_move_occured == true)
					{
						load_undo(tile, tile_to_undo, field_size, &end_game, &points, &first_move_occured, undo_variables);
					}
					else if (event.key.keysym.sym == SDLK_s)
					{
						save_game_to_file(tile, field_size, &end_game, &points, &world_time, &starting_x, &starting_y);
					}
					else if (event.key.keysym.sym == SDLK_r)
					{
						show_rank(renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);
					}
					else if (event.key.keysym.sym == SDLK_l)
					{
						load_game_from_file(tile, &field_size, &end_game, &points, &first_move_occured, &world_time, &starting_x, &starting_y, tile_to_undo, undo_variables, renderer, scrtex, screen, charset, &event, menu_foreground_color, menu_background_color);
					}
					else if (end_game == false && event.key.keysym.sym == SDLK_UP)
					{
						save_to_undo(tile, tile_to_undo, field_size, &end_game, &points, &first_move_occured, undo_variables);
						move(UP, field_size, tile, starting_x, starting_y, &end_game, &points, &first_move_occured);
					}
					else if (end_game == false && event.key.keysym.sym == SDLK_DOWN)
					{
						save_to_undo(tile, tile_to_undo, field_size, &end_game, &points, &first_move_occured, undo_variables);
						move(DOWN, field_size, tile, starting_x, starting_y, &end_game, &points, &first_move_occured);
					}
					else if (end_game == false && event.key.keysym.sym == SDLK_LEFT)
					{
						save_to_undo(tile, tile_to_undo, field_size, &end_game, &points, &first_move_occured, undo_variables);
						move(LEFT, field_size, tile, starting_x, starting_y, &end_game, &points, &first_move_occured);
					}
					else if (end_game == false && event.key.keysym.sym == SDLK_RIGHT)
					{
						save_to_undo(tile, tile_to_undo, field_size, &end_game, &points, &first_move_occured, undo_variables);
						move(RIGHT, field_size, tile, starting_x, starting_y, &end_game, &points, &first_move_occured);
					}
					break;
				case SDL_KEYUP:
					break;
				case SDL_QUIT:
					quit = 1;
					break;
				}
			}
		}
	}

	printf("Zwalnianie pamieci...\n");
	free(tile);
	free(tile_to_undo);
	free(undo_variables);
	printf("Zwalnianie powierzchni...\n");
	SDL_FreeSurface(charset);
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(scrtex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	printf("WYJSCIE\n");
	SDL_Quit();
	return 0;
};
