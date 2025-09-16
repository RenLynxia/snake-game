#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Definisi konstanta untuk jendela dan game
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define CELL_SIZE 20
#define GAME_WIDTH (WINDOW_WIDTH / CELL_SIZE)
#define GAME_HEIGHT (WINDOW_HEIGHT / CELL_SIZE)

// Warna
#define WHITE_COLOR 255, 255, 255, 255
#define BLACK_COLOR 0, 0, 0, 255
#define RED_COLOR 255, 0, 0, 255
#define GREEN_COLOR 0, 255, 0, 255
#define DARK_GREEN_COLOR 0, 204, 0, 255
#define GRID_COLOR 50, 50, 50, 255

// Struktur untuk koordinat
struct coordinate {
    int x;
    int y;
};

// Variabel global
int length;
int score;
int life;
int direction;
int game_state; // 0: Menu, 1: Playing, 2: Game Over, 3: Difficulty Menu
int difficulty_multiplier;
Uint32 game_speed;

struct coordinate head, food;
struct coordinate body[1000];

// SDL variables
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
Mix_Chunk* eat_sound = NULL;
Mix_Chunk* game_over_sound = NULL;

// Prototipe fungsi
void setup();
void draw_game();
void input(SDL_Event e);
void logic();
void generate_food();
void game_over();
void draw_text(const char* text, int x, int y, SDL_Color color, int center);
void draw_menu();
void handle_menu_input(SDL_Event e);
void draw_game_over_screen();
void draw_difficulty_menu();
void handle_difficulty_input(SDL_Event e);

// Inisialisasi SDL, SDL_ttf, dan SDL_mixer
int init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    if (TTF_Init() == -1) {
        printf("SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("SDL_mixer could not initialize! Mix_Error: %s\n", Mix_GetError());
        return 0;
    }

    window = SDL_CreateWindow("Game Ular", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }

    // Memuat font
    font = TTF_OpenFont("arial.ttf", 7);
    if (font == NULL) {
        font = TTF_OpenFont("font.ttf", 7);
    }
    if (font == NULL) {
        printf("Failed to load font! TTF_Error: %s\n", TTF_GetError());
        printf("Pastikan Anda memiliki file 'arial.ttf' atau 'font.ttf' di folder yang sama.\n");
        return 0;
    }

    // Memuat efek suara
    eat_sound = Mix_LoadWAV("eat.wav");
    if (eat_sound == NULL) {
        printf("Failed to load eat sound! Mix_Error: %s\n", Mix_GetError());
    }

    game_over_sound = Mix_LoadWAV("gameover.wav");
    if (game_over_sound == NULL) {
        printf("Failed to load game over sound! Mix_Error: %s\n", Mix_GetError());
    }

    return 1;
}

// Menutup SDL
void close_sdl() {
    if (eat_sound) Mix_FreeChunk(eat_sound);
    if (game_over_sound) Mix_FreeChunk(game_over_sound);
    Mix_CloseAudio();
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, char* argv[]) {
    if (!init_sdl()) {
        return 1;
    }

    game_state = 0; // Mulai di layar menu
    int quit = 0;
    SDL_Event e;
    Uint32 last_tick = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (game_state == 0) {
                handle_menu_input(e);
            } else if (game_state == 1) {
                input(e);
            } else if (game_state == 2) {
                if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    quit = 1;
                }
            } else if (game_state == 3) {
                handle_difficulty_input(e);
            }
        }

        if (game_state == 1) {
            Uint32 current_tick = SDL_GetTicks();
            if (current_tick - last_tick > game_speed) {
                logic();
                last_tick = current_tick;
            }
        }

        // Gambar berdasarkan status game
        if (game_state == 0) {
            draw_menu();
        } else if (game_state == 1) {
            draw_game();
        } else if (game_state == 2) {
            draw_game_over_screen();
        } else if (game_state == 3) {
            draw_difficulty_menu();
        }
    }

    close_sdl();
    return 0;
}

// Menyiapkan kondisi awal game
void setup() {
    length = 5;
    score = 0;
    life = 3;
    direction = SDL_SCANCODE_RIGHT;
    head.x = GAME_WIDTH / 2;
    head.y = GAME_HEIGHT / 2;
    generate_food();
    game_state = 1; // Pindah ke status bermain
}

// Menggambar semua elemen game ke jendela
void draw_game() {
    // Menggambar latar belakang
    SDL_SetRenderDrawColor(renderer, BLACK_COLOR);
    SDL_RenderClear(renderer);

    // Menggambar grid
    SDL_SetRenderDrawColor(renderer, GRID_COLOR);
    for (int i = 0; i < GAME_WIDTH; i++) {
        SDL_RenderDrawLine(renderer, i * CELL_SIZE, 0, i * CELL_SIZE, WINDOW_HEIGHT);
    }
    for (int i = 0; i < GAME_HEIGHT; i++) {
        SDL_RenderDrawLine(renderer, 0, i * CELL_SIZE, WINDOW_WIDTH, i * CELL_SIZE);
    }

    // Menggambar makanan
    SDL_Rect food_rect = { food.x * CELL_SIZE, food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_SetRenderDrawColor(renderer, RED_COLOR);
    SDL_RenderFillRect(renderer, &food_rect);

    // Menggambar kepala ular
    SDL_Rect head_rect = { head.x * CELL_SIZE, head.y * CELL_SIZE, CELL_SIZE, CELL_SIZE };
    SDL_SetRenderDrawColor(renderer, GREEN_COLOR);
    SDL_RenderFillRect(renderer, &head_rect);

    // Menggambar tubuh ular
    for (int i = 0; i < length - 1; i++) {
        SDL_Rect body_rect = { body[i].x * CELL_SIZE, body[i].y * CELL_SIZE, CELL_SIZE, CELL_SIZE };
        SDL_SetRenderDrawColor(renderer, DARK_GREEN_COLOR);
        SDL_RenderFillRect(renderer, &body_rect);
    }

    // Tampilkan skor
    char score_text[20];
    sprintf(score_text, "Skor: %d", score);
    draw_text(score_text, WINDOW_WIDTH - 150, 20, (SDL_Color){WHITE_COLOR}, 0);

    // Tampilkan render
    SDL_RenderPresent(renderer);
}

// Menggambar teks
void draw_text(const char* text, int x, int y, SDL_Color color, int center) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) return;

    SDL_Rect rect;
    rect.w = surface->w;
    rect.h = surface->h;
    rect.x = x;
    rect.y = y;

    if (center) {
        rect.x = (WINDOW_WIDTH - rect.w) / 2;
    }

    SDL_RenderCopy(renderer, texture, NULL, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

// Menggambar layar menu utama
void draw_menu() {
    SDL_SetRenderDrawColor(renderer, BLACK_COLOR);
    SDL_RenderClear(renderer);
    draw_text("Game Ular", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 - 100, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan S untuk Mulai", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 + 50, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan Q untuk Keluar", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 + 100, (SDL_Color){WHITE_COLOR}, 1);
    SDL_RenderPresent(renderer);
}

// Menangani input menu utama
void handle_menu_input(SDL_Event e) {
    if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_S:
                game_state = 3; // Pindah ke menu kesulitan
                break;
            case SDL_SCANCODE_Q:
                exit(0);
                break;
        }
    }
}

// Menggambar menu kesulitan
void draw_difficulty_menu() {
    SDL_SetRenderDrawColor(renderer, BLACK_COLOR);
    SDL_RenderClear(renderer);
    draw_text("Pilih Tingkat Kesulitan", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 - 100, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan M untuk Mudah", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 + 50, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan N untuk Normal", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 + 100, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan H untuk Sulit", (WINDOW_WIDTH - 200) / 2, WINDOW_HEIGHT / 2 + 150, (SDL_Color){WHITE_COLOR}, 1);
    SDL_RenderPresent(renderer);
}

// Menangani input menu kesulitan
void handle_difficulty_input(SDL_Event e) {
    if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_M:
                game_speed = 150; // Mudah
                difficulty_multiplier = 5;
                setup();
                break;
            case SDL_SCANCODE_N:
                game_speed = 100; // Normal
                difficulty_multiplier = 10;
                setup();
                break;
            case SDL_SCANCODE_H:
                game_speed = 50; // Sulit
                difficulty_multiplier = 20;
                setup();
                break;
        }
    }
}


// Memproses input dari keyboard
void input(SDL_Event e) {
    if (e.type == SDL_KEYDOWN) {
        switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_UP:
                if (direction != SDL_SCANCODE_DOWN) direction = SDL_SCANCODE_UP;
                break;
            case SDL_SCANCODE_DOWN:
                if (direction != SDL_SCANCODE_UP) direction = SDL_SCANCODE_DOWN;
                break;
            case SDL_SCANCODE_LEFT:
                if (direction != SDL_SCANCODE_RIGHT) direction = SDL_SCANCODE_LEFT;
                break;
            case SDL_SCANCODE_RIGHT:
                if (direction != SDL_SCANCODE_LEFT) direction = SDL_SCANCODE_RIGHT;
                break;
            case SDL_SCANCODE_ESCAPE:
                game_over();
                break;
        }
    }
}

// Memperbarui logika game
void logic() {
    // Pindahkan tubuh ular
    for (int i = length - 1; i > 0; i--) {
        body[i] = body[i - 1];
    }
    body[0] = head;

    // Pindahkan kepala ular
    if (direction == SDL_SCANCODE_UP)
        head.y--;
    else if (direction == SDL_SCANCODE_DOWN)
        head.y++;
    else if (direction == SDL_SCANCODE_LEFT)
        head.x--;
    else if (direction == SDL_SCANCODE_RIGHT)
        head.x++;

    // Cek tabrakan dengan makanan
    if (head.x == food.x && head.y == food.y) {
        length++;
        score += difficulty_multiplier;
        generate_food();
        if (eat_sound) {
            Mix_PlayChannel(-1, eat_sound, 0);
        }
    }

    // Cek tabrakan dengan batas atau tubuh sendiri
    if (head.x < 0 || head.x >= GAME_WIDTH || head.y < 0 || head.y >= GAME_HEIGHT) {
        life--;
        if (life > 0) {
            setup();
        } else {
            game_over();
        }
    }

    // Cek tabrakan dengan tubuh sendiri
    for (int i = 1; i < length; i++) {
        if (head.x == body[i].x && head.y == body[i].y) {
            life--;
            if (life > 0) {
                setup();
            } else {
                game_over();
            }
        }
    }
}

// Menghasilkan koordinat makanan secara acak
void generate_food() {
    srand(time(NULL));
    food.x = rand() % GAME_WIDTH;
    food.y = rand() % GAME_HEIGHT;
}

// Menampilkan layar game over
void draw_game_over_screen() {
    SDL_SetRenderDrawColor(renderer, BLACK_COLOR);
    SDL_RenderClear(renderer);

    char score_text[50];
    sprintf(score_text, "Game Over! Skor Anda: %d", score);
    draw_text("Game Over", (WINDOW_WIDTH-300)/2, WINDOW_HEIGHT / 2 - 100, (SDL_Color){RED_COLOR}, 1);
    draw_text(score_text, (WINDOW_WIDTH-300)/2, WINDOW_HEIGHT / 2 + 50, (SDL_Color){WHITE_COLOR}, 1);
    draw_text("Tekan ESC untuk keluar", (WINDOW_WIDTH-300)/2, WINDOW_HEIGHT / 2 + 100, (SDL_Color){WHITE_COLOR}, 1);
    SDL_RenderPresent(renderer);
}

void game_over() {
    game_state = 2; // Pindah ke status Game Over
    if (game_over_sound) {
        Mix_PlayChannel(-1, game_over_sound, 0);
    }
}
