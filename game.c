#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

// --- Game Configuration ---
#define BOARD_WIDTH 8
#define BOARD_HEIGHT 8
#define NUM_CANDY_TYPES 5
#define EMPTY_TYPE 0

// --- ANSI Color Codes ---
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_BOLD    "\x1b[1m"

// --- Data Structures ---
typedef enum {
    SPECIAL_NONE, SPECIAL_STRIPED_H, SPECIAL_STRIPED_V, SPECIAL_BOMB
} SpecialType;

typedef struct {
    int type;
    SpecialType special;
} Candy;

typedef struct {
    Candy board[BOARD_HEIGHT][BOARD_WIDTH];
    int score;
    int movesLeft;
} GameState;

typedef enum { DIR_H, DIR_V } MatchDirection;

typedef struct {
    unsigned char id;
    unsigned char len;
    MatchDirection dir;
} MatchInfo;

// --- Function Prototypes ---
void initGame(GameState *gs);
void displayBoard(const GameState *gs);
bool isValidSwap(GameState *gs, size_t r1, size_t c1, size_t r2, size_t c2);
void findAndMarkMatches(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void activateSpecials(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
int clearCandies(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void applyGravityAndRefill(GameState *gs);
void copyGameState(GameState *dest, const GameState *src);

// --- Main Game Loop ---
int main(void) {
    GameState gameState;
    srand(time(NULL));
    initGame(&gameState);

    while (gameState.movesLeft > 0) {
        printf("\n--- Score: %d | Moves Left: %d ---\n", gameState.score, gameState.movesLeft);
        displayBoard(&gameState);

        printf("Enter swap (row1 col1 row2 col2): ");
        int r1, c1, r2, c2;
        if (scanf("%d %d %d %d", &r1, &c1, &r2, &c2) != 4) {
            printf("Invalid input format. Please enter 4 numbers.\n");
            while (getchar() != '\n'); // Clear invalid input buffer
            continue;
        }

        // Security: Check if input coordinates are within bounds
        if (r1 < 0 || r1 >= BOARD_HEIGHT || c1 < 0 || c1 >= BOARD_WIDTH ||
            r2 < 0 || r2 >= BOARD_HEIGHT || c2 < 0 || c2 >= BOARD_WIDTH) {
            printf("--> Coordinates out of bounds (0-7).\n");
            continue;
        }

        if (!isValidSwap(&gameState, r1, c1, r2, c2)) {
            printf("--> Invalid Move! Swap must be adjacent and create a match.\n");
            continue;
        }

        gameState.movesLeft--;
        // Direct swap on the main board after validation
        Candy temp = gameState.board[r1][c1];
        gameState.board[r1][c1] = gameState.board[r2][c2];
        gameState.board[r2][c2] = temp;

        int turnScore = 0;
        int totalCleared;
        do {
            bool clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
            findAndMarkMatches(&gameState, clear_map);
            activateSpecials(&gameState, clear_map);
            totalCleared = clearCandies(&gameState, clear_map);
            
            if (totalCleared > 0) {
                turnScore += totalCleared;
                printf("\nCleared %d candies!\n", totalCleared);
                applyGravityAndRefill(&gameState);
                printf("Board after gravity and refill:\n");
                displayBoard(&gameState);
            }
        } while (totalCleared > 0);
        
        gameState.score += turnScore;
        if (turnScore > 0) printf("Scored %d points this turn!\n", turnScore);
    }

    printf("\n--- GAME OVER ---\nFinal Score: %d\n", gameState.score);
    return 0;
}

void findAndMarkMatches(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    MatchInfo match_map[BOARD_HEIGHT][BOARD_WIDTH] = {0}; // Zeros mean no match
    unsigned char match_id_counter = 1;

    // --- Pass 1 & 2: Find all H and V matches and record their info ---
    // Horizontal
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE) { c++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (c + match_len < BOARD_WIDTH && gs->board[r][c + match_len].type == match_type) match_len++;
            
            if (match_len >= 3) {
                for (size_t i = 0; i < match_len; i++) {
                    match_map[r][c + i].id = match_id_counter;
                    match_map[r][c + i].len = match_len;
                    match_map[r][c + i].dir = DIR_H;
                }
                match_id_counter++;
            }
            c += match_len;
        }
    }
    // Vertical
    for (size_t c = 0; c < BOARD_WIDTH; c++) {
        for (size_t r = 0; r < BOARD_HEIGHT - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE) { r++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (r + match_len < BOARD_HEIGHT && gs->board[r + match_len][c].type == match_type) match_len++;

            if (match_len >= 3) {
                // Check for intersections. If a candy is already part of an H match,
                // it's an intersection. We'll handle this in the final pass.
                for (size_t i = 0; i < match_len; i++) {
                    // Don't overwrite H-match info, just mark it for clearing
                    if (match_map[r + i][c].id == 0) { // Only if no H-match exists
                        match_map[r + i][c].id = match_id_counter;
                        match_map[r + i][c].len = match_len;
                        match_map[r + i][c].dir = DIR_V;
                    }
                }
                match_id_counter++;
            }
            r += match_len;
        }
    }

    // --- Pass 3: Final consolidation and special creation ---
    bool special_placed[match_id_counter]; // Track if a special has been made for a match ID
    memset(special_placed, 0, sizeof(special_placed));

    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            if (match_map[r][c].id > 0) {
                clear_map[r][c] = true; // Mark for clearing by default
                
                unsigned char current_id = match_map[r][c].id;
                unsigned char current_len = match_map[r][c].len;
                
                // If we haven't already placed a special for this match
                if (!special_placed[current_id]) {
                    bool is_intersection = false;
                    // Check if this candy is part of an H and V match
                    // This is a simplified check; a full check would be more complex
                    if (match_map[r][c].dir == DIR_V) {
                        for(size_t i=0; i<BOARD_WIDTH; i++) {
                            if(match_map[r][i].id > 0 && match_map[r][i].dir == DIR_H) {
                                is_intersection = true;
                                break;
                            }
                        }
                    }

                    if (is_intersection) {
                        gs->board[r][c].special = SPECIAL_BOMB;
                        clear_map[r][c] = false;
                        special_placed[current_id] = true;
                    } else if (current_len == 5) {
                        gs->board[r][c].special = SPECIAL_BOMB;
                        clear_map[r][c] = false;
                        special_placed[current_id] = true;
                    } else if (current_len == 4) {
                        gs->board[r][c].special = (match_map[r][c].dir == DIR_H) ? SPECIAL_STRIPED_V : SPECIAL_STRIPED_H;
                        clear_map[r][c] = false;
                        special_placed[current_id] = true;
                    }
                }
            }
        }
    }
}


// --- Other Functions (Mostly unchanged, but with `const` and `size_t` where appropriate) ---

void initGame(GameState *gs) {
    gs->score = 0;
    gs->movesLeft = 25;
    bool clear_map[BOARD_HEIGHT][BOARD_WIDTH];
    do {
        memset(clear_map, 0, sizeof(clear_map));
        for (size_t r = 0; r < BOARD_HEIGHT; r++) {
            for (size_t c = 0; c < BOARD_WIDTH; c++) {
                gs->board[r][c].type = rand() % NUM_CANDY_TYPES + 1;
                gs->board[r][c].special = SPECIAL_NONE;
            }
        }
        findAndMarkMatches(gs, clear_map);
        int initial_matches = 0;
        for(size_t r=0; r<BOARD_HEIGHT; r++) for(size_t c=0; c<BOARD_WIDTH; c++) if(clear_map[r][c]) initial_matches++;
        if (initial_matches == 0) break;
    } while (true);
}

void displayBoard(const GameState *gs) {
    const char* candy_colors[] = {COLOR_RESET, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA};
    const char* candy_symbols[] = {" ", "●", "■", "◆", "▲", "★"};
    printf("   ");
    for(size_t i = 0; i < BOARD_WIDTH; i++) printf(" %zu ", i);
    printf("\n-----------------------------\n");
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        printf("%zu |", r);
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            const Candy *candy = &gs->board[r][c];
            if (candy->type == EMPTY_TYPE) {
                printf(" . ");
            } else {
                printf("%s", candy_colors[candy->type]);
                if (candy->special == SPECIAL_BOMB) printf(COLOR_BOLD " B " COLOR_RESET);
                else if (candy->special == SPECIAL_STRIPED_H) printf("-%s-", candy_symbols[candy->type]);
                else if (candy->special == SPECIAL_STRIPED_V) printf("|%s|", candy_symbols[candy->type]);
                else printf(" %s ", candy_symbols[candy->type]);
                printf(COLOR_RESET);
            }
        }
        printf("|\n");
    }
    printf("-----------------------------\n");
}

void copyGameState(GameState *dest, const GameState *src) {
    memcpy(dest, src, sizeof(GameState));
}

bool isValidSwap(GameState *gs, size_t r1, size_t c1, size_t r2, size_t c2) {
    if (!((abs((int)r1 - (int)r2) + abs((int)c1 - (int)c2)) == 1)) return false;

    GameState tempState;
    copyGameState(&tempState, gs);
    
    Candy temp = tempState.board[r1][c1];
    tempState.board[r1][c1] = tempState.board[r2][c2];
    tempState.board[r2][c2] = temp;

    if (tempState.board[r1][c1].special == SPECIAL_BOMB || tempState.board[r2][c2].special == SPECIAL_BOMB) return true;

    bool temp_clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    findAndMarkMatches(&tempState, temp_clear_map);

    for(size_t r=0; r<BOARD_HEIGHT; r++) for(size_t c=0; c<BOARD_WIDTH; c++) if(temp_clear_map[r][c]) return true;
    
    return false;
}

void activateSpecials(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    bool changed;
    do {
        changed = false;
        for (size_t r = 0; r < BOARD_HEIGHT; r++) {
            for (size_t c = 0; c < BOARD_WIDTH; c++) {
                if (clear_map[r][c]) {
                    if (gs->board[r][c].special == SPECIAL_STRIPED_H) {
                        for (size_t i = 0; i < BOARD_WIDTH; i++) if (!clear_map[r][i]) { clear_map[r][i] = true; changed = true; }
                    } else if (gs->board[r][c].special == SPECIAL_STRIPED_V) {
                        for (size_t i = 0; i < BOARD_HEIGHT; i++) if (!clear_map[i][c]) { clear_map[i][c] = true; changed = true; }
                    }
                }
            }
        }
    } while (changed);
}

int clearCandies(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    int cleared_count = 0;
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            if (clear_map[r][c]) {
                gs->board[r][c].type = EMPTY_TYPE;
                gs->board[r][c].special = SPECIAL_NONE;
                cleared_count++;
            }
        }
    }
    return cleared_count;
}

void applyGravityAndRefill(GameState *gs) {
    for (size_t c = 0; c < BOARD_WIDTH; c++) {
        size_t write_row = BOARD_HEIGHT - 1;
        for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
            if (gs->board[r][c].type != EMPTY_TYPE) {
                if ((size_t)r != write_row) gs->board[write_row][c] = gs->board[r][c];
                write_row--;
            }
        }
        while ((int)write_row >= 0) {
            gs->board[write_row][c].type = rand() % NUM_CANDY_TYPES + 1;
            gs->board[write_row][c].special = SPECIAL_NONE;
            write_row--;
        }
    }
}