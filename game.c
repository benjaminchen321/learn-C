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
#define BOMB_TYPE (NUM_CANDY_TYPES + 1)

// --- Data Structures ---
typedef enum {
    SPECIAL_NONE, SPECIAL_STRIPED_H, SPECIAL_STRIPED_V, SPECIAL_BOMB
} SpecialType;

typedef struct {
    int type;
    SpecialType special;
} Candy;

// --- Function Prototypes ---
void populateBoard(Candy board[BOARD_HEIGHT][BOARD_WIDTH]);
void displayBoard(const Candy board[BOARD_HEIGHT][BOARD_WIDTH]);
bool isValidSwap(Candy board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2);
void swapCandies(Candy board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2);
void findAndMarkMatches(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void activateSpecials(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
int clearCandies(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void applyGravityAndRefill(Candy board[BOARD_HEIGHT][BOARD_WIDTH]);
void copyBoard(Candy dest[BOARD_HEIGHT][BOARD_WIDTH], const Candy src[BOARD_HEIGHT][BOARD_WIDTH]);

// --- Main Game Loop ---
int main(void) {
    Candy gameBoard[BOARD_HEIGHT][BOARD_WIDTH];
    int score = 0;
    int movesLeft = 25;
    srand(time(NULL));

    populateBoard(gameBoard);

    while (movesLeft > 0) {
        printf("\n--- Score: %d | Moves Left: %d ---\n", score, movesLeft);
        displayBoard(gameBoard);

        int r1, c1, r2, c2;
        printf("Enter swap (row1 col1 row2 col2): ");
        if (scanf("%d %d %d %d", &r1, &c1, &r2, &c2) != 4) {
            printf("Invalid input. Try again.\n");
            while (getchar() != '\n');
            continue;
        }

        if (!isValidSwap(gameBoard, r1, c1, r2, c2)) {
            printf("--> Invalid Move! Swap must be adjacent and create a match.\n");
            continue;
        }

        movesLeft--;
        swapCandies(gameBoard, r1, c1, r2, c2);

        int turnScore = 0;
        int totalCleared;
        do {
            bool clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
            
            findAndMarkMatches(gameBoard, clear_map);
            activateSpecials(gameBoard, clear_map);
            totalCleared = clearCandies(gameBoard, clear_map);
            
            if (totalCleared > 0) {
                turnScore += totalCleared;
                printf("\nCleared %d candies!\n", totalCleared);
                applyGravityAndRefill(gameBoard);
                printf("Board after gravity and refill:\n");
                displayBoard(gameBoard);
            }
        } while (totalCleared > 0);
        
        score += turnScore;
        if (turnScore > 0) printf("Scored %d points this turn!\n", turnScore);
    }

    printf("\n--- GAME OVER ---\nFinal Score: %d\n", score);
    return 0;
}

// --- Function Definitions ---

void populateBoard(Candy board[BOARD_HEIGHT][BOARD_WIDTH]) {
    bool clear_map[BOARD_HEIGHT][BOARD_WIDTH];
    do {
        // BUGFIX: clear_map MUST be reset to all false on each iteration.
        // memset is the standard C function to fill a block of memory with a value.
        // Here, we fill it with 0, which is equivalent to 'false'.
        memset(clear_map, 0, sizeof(clear_map));

        for (int r = 0; r < BOARD_HEIGHT; r++) {
            for (int c = 0; c < BOARD_WIDTH; c++) {
                board[r][c].type = rand() % NUM_CANDY_TYPES + 1;
                board[r][c].special = SPECIAL_NONE;
            }
        }
        findAndMarkMatches(board, clear_map);
        int initial_matches = 0;
        for(int r=0; r<BOARD_HEIGHT; r++) for(int c=0; c<BOARD_WIDTH; c++) if(clear_map[r][c]) initial_matches++;
        if (initial_matches == 0) break;
    } while (true);
}

void displayBoard(const Candy board[BOARD_HEIGHT][BOARD_WIDTH]) {
    printf("   ");
    for(int i = 0; i < BOARD_WIDTH; i++) printf(" %d ", i);
    printf("\n-----------------------------\n");
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        printf("%d |", r);
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (board[r][c].type == EMPTY_TYPE) printf(" . ");
            else if (board[r][c].special == SPECIAL_BOMB) printf(" B ");
            else {
                char s = (board[r][c].special == SPECIAL_STRIPED_H) ? '-' : (board[r][c].special == SPECIAL_STRIPED_V) ? '|' : ' ';
                printf("%d%c ", board[r][c].type, s);
            }
        }
        printf("|\n");
    }
    printf("-----------------------------\n");
}

void copyBoard(Candy dest[BOARD_HEIGHT][BOARD_WIDTH], const Candy src[BOARD_HEIGHT][BOARD_WIDTH]) {
    memcpy(dest, src, sizeof(Candy) * BOARD_HEIGHT * BOARD_WIDTH);
}

bool isValidSwap(Candy board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2) {
    if (!(abs(r1 - r2) + abs(c1 - c2) == 1)) return false;

    Candy tempBoard[BOARD_HEIGHT][BOARD_WIDTH];
    copyBoard(tempBoard, board);
    swapCandies(tempBoard, r1, c1, r2, c2);

    if (tempBoard[r1][c1].special == SPECIAL_BOMB || tempBoard[r2][c2].special == SPECIAL_BOMB) return true;

    bool temp_clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    findAndMarkMatches(tempBoard, temp_clear_map);

    for(int r=0; r<BOARD_HEIGHT; r++) for(int c=0; c<BOARD_WIDTH; c++) if(temp_clear_map[r][c]) return true;
    
    return false;
}

void swapCandies(Candy board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2) {
    Candy temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;
}

void findAndMarkMatches(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    // Horizontal matches
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH - 2; ) {
            if (board[r][c].type == EMPTY_TYPE) { c++; continue; }
            int match_type = board[r][c].type;
            int match_len = 1;
            while (c + match_len < BOARD_WIDTH && board[r][c + match_len].type == match_type) {
                match_len++;
            }

            if (match_len >= 3) {
                for (int i = 0; i < match_len; i++) clear_map[r][c + i] = true;
                if (match_len == 5) {
                    board[r][c].special = SPECIAL_BOMB;
                } else if (match_len == 4) {
                    board[r][c].special = SPECIAL_STRIPED_V;
                }
            }
            c += match_len;
        }
    }
    // Vertical matches
    for (int c = 0; c < BOARD_WIDTH; c++) {
        for (int r = 0; r < BOARD_HEIGHT - 2; ) {
            if (board[r][c].type == EMPTY_TYPE) { r++; continue; }
            int match_type = board[r][c].type;
            int match_len = 1;
            while (r + match_len < BOARD_HEIGHT && board[r + match_len][c].type == match_type) {
                match_len++;
            }

            if (match_len >= 3) {
                for (int i = 0; i < match_len; i++) clear_map[r + i][c] = true;
                if (match_len == 5) {
                    board[r][c].special = SPECIAL_BOMB;
                } else if (match_len == 4) {
                    board[r][c].special = SPECIAL_STRIPED_H;
                }
            }
            r += match_len;
        }
    }
}

void activateSpecials(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    bool changed;
    do {
        changed = false;
        for (int r = 0; r < BOARD_HEIGHT; r++) {
            for (int c = 0; c < BOARD_WIDTH; c++) {
                if (clear_map[r][c]) {
                    if (board[r][c].special == SPECIAL_STRIPED_H) {
                        for (int i = 0; i < BOARD_WIDTH; i++) if (!clear_map[r][i]) { clear_map[r][i] = true; changed = true; }
                    } else if (board[r][c].special == SPECIAL_STRIPED_V) {
                        for (int i = 0; i < BOARD_HEIGHT; i++) if (!clear_map[i][c]) { clear_map[i][c] = true; changed = true; }
                    }
                }
            }
        }
    } while (changed); // Loop until a full pass makes no new changes
}

int clearCandies(Candy board[BOARD_HEIGHT][BOARD_WIDTH], bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    int cleared_count = 0;
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            if (clear_map[r][c]) {
                board[r][c].type = EMPTY_TYPE;
                board[r][c].special = SPECIAL_NONE;
                cleared_count++;
            }
        }
    }
    return cleared_count;
}

void applyGravityAndRefill(Candy board[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (int c = 0; c < BOARD_WIDTH; c++) {
        int write_row = BOARD_HEIGHT - 1;
        for (int r = BOARD_HEIGHT - 1; r >= 0; r--) {
            if (board[r][c].type != EMPTY_TYPE) {
                if (r != write_row) board[write_row][c] = board[r][c];
                write_row--;
            }
        }
        while (write_row >= 0) {
            board[write_row][c].type = rand() % NUM_CANDY_TYPES + 1;
            board[write_row][c].special = SPECIAL_NONE;
            write_row--;
        }
    }
}