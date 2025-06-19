/*******************************************************************
*
* C-CRUSH: THE DEFINITIVE POLISHED EDITION (v8.0)
*
* This is the final, production-quality version of the game.
* It features a robust, persistent UI with large, distinct ASCII art,
* a complete and correct game logic pipeline, and comprehensive
* defensive coding. This is the culmination of our journey.
*
* Final Features:
* - New, highly-distinct ASCII art for all candies and specials.
* - Flawless, constant-width grid rendering with vertical spacing.
* - Correct activation logic for ALL special candies in ALL match types.
* - Player-centric special candy creation.
* - Intro, Level Complete, and Game Over screens.
* - A complete, multi-level game flow with procedural generation.
*
* How to Compile (macOS/Linux):
*   clang -Wall -Wextra -O2 -o ccrush final_game.c
*
*******************************************************************/

// Standard Libraries
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

// POSIX-specific Libraries
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

// --- Game Configuration ---
#define BOARD_WIDTH 8
#define BOARD_HEIGHT 8
#define NUM_CANDY_TYPES 5
#define EMPTY_TYPE 0
#define MIN_TERM_ROWS 28
#define MIN_TERM_COLS 28
#define CASCADE_DELAY_US 200000
#define ASCII_ART_HEIGHT 2
#define CELL_WIDTH 7

// --- ANSI Color & Control Codes ---
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_POS(r,c) printf("\x1b[%zu;%zuH", (size_t)(r), (size_t)(c))
#define HIDE_CURSOR  "\x1b[?25l"
#define SHOW_CURSOR  "\x1b[?25h"

#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_WHITE   "\x1b[37m"
#define COLOR_BOLD    "\x1b[1m"
#define CURSOR_COLOR  "\x1b[47;30m"

// --- Data Structures ---
typedef enum { SPECIAL_NONE, SPECIAL_STRIPED_H, SPECIAL_STRIPED_V, SPECIAL_BOMB } SpecialType;
typedef struct { int type; SpecialType special; } Candy;
typedef enum { STATE_SHOW_INTRO, STATE_PLAYING_LEVEL, STATE_SELECTING_SWAP_DIR, STATE_PROCESSING, STATE_LEVEL_COMPLETE, STATE_GAME_OVER_FINAL, STATE_QUIT } GameMode;

typedef struct {
    Candy board[BOARD_HEIGHT][BOARD_WIDTH];
    int score;
    int movesLeft;
    GameMode mode;
    size_t cursor_r, cursor_c;
    size_t selected_r, selected_c;
    char message[128];
    int currentLevel;
    int targetScore;
} GameState;

// --- Global State ---
struct termios orig_termios;

// --- Prototypes ---
void handleFatalError(const char *msg);
void enableRawMode();
void disableRawMode();
void getTerminalSize(int *rows, int *cols);
void startNewGame(GameState *gs);
void loadLevel(GameState *gs, int level);
void display(const GameState *gs);
void displayIntro();
void displayGame(const GameState *gs);
void displayLevelComplete(const GameState *gs);
void displayGameOver(const GameState *gs);
void processInput(GameState *gs);
void updateGame(GameState *gs, size_t r2, size_t c2);
void findAndMarkMatches(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void createSpecials(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH], int move_r, int move_c);
void activateSpecials(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void activateBomb(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH], int target_type);
int clearCandies(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]);
void applyGravityAndRefill(GameState *gs);

// --- Main Function ---
int main(void) {
    int term_rows, term_cols;
    getTerminalSize(&term_rows, &term_cols);
    if (term_rows < MIN_TERM_ROWS || term_cols < MIN_TERM_COLS) {
        fprintf(stderr, "Terminal too small. Please resize to at least %d rows by %d columns.\n", MIN_TERM_ROWS, MIN_TERM_COLS);
        return 1;
    }
    enableRawMode();
    GameState gameState = {0};
    gameState.mode = STATE_SHOW_INTRO;
    srand(time(NULL));
    while (gameState.mode != STATE_QUIT) {
        display(&gameState);
        processInput(&gameState);
    }
    CURSOR_POS(1, 1);
    printf(CLEAR_SCREEN);
    printf("Thanks for playing C-Crush!\n");
    return 0;
}

// --- Core Game Logic ---

void startNewGame(GameState *gs) {
    if (!gs) return;
    loadLevel(gs, 1);
}

void loadLevel(GameState *gs, int level) {
    if (!gs) return;
    gs->currentLevel = level;
    gs->score = 0;
    gs->targetScore = 100 + (level - 1) * 75;
    int moves = 25 - ((level - 1) / 2);
    gs->movesLeft = (moves < 10) ? 10 : moves;
    gs->mode = STATE_PLAYING_LEVEL;
    gs->cursor_r = BOARD_HEIGHT / 2;
    gs->cursor_c = BOARD_WIDTH / 2;
    snprintf(gs->message, sizeof(gs->message), "Level %d! Get %d points.", gs->currentLevel, gs->targetScore);
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

void display(const GameState *gs) {
    if (!gs) return;
    switch (gs->mode) {
        case STATE_SHOW_INTRO:         displayIntro(); break;
        case STATE_PLAYING_LEVEL:
        case STATE_SELECTING_SWAP_DIR:
        case STATE_PROCESSING:         displayGame(gs); break;
        case STATE_LEVEL_COMPLETE:     displayLevelComplete(gs); break;
        case STATE_GAME_OVER_FINAL:    displayGameOver(gs); break;
        case STATE_QUIT:               break;
    }
}

void displayIntro() {
    CURSOR_POS(1, 1);
    printf(CLEAR_SCREEN);
    printf(COLOR_BOLD "--- WELCOME TO C-CRUSH! ---\n\n" COLOR_RESET);
    CURSOR_POS(4, 5);
    printf(COLOR_YELLOW "--- HOW TO PLAY ---\n" COLOR_RESET);
    CURSOR_POS(6, 5);
    printf("W, A, S, D or Arrow Keys : Move the cursor\n");
    CURSOR_POS(7, 5);
    printf("SPACE                      : Select a candy to swap\n");
    CURSOR_POS(8, 5);
    printf("W, A, S, D or Arrow Keys : Choose direction to swap\n");
    CURSOR_POS(9, 5);
    printf("Q                          : Quit the game at any time\n");
    CURSOR_POS(12, 5);
    printf(COLOR_YELLOW "--- SPECIAL CANDIES ---\n" COLOR_RESET);
    CURSOR_POS(14, 5);
    printf("Match 4 -> Striped Candy: Clears a row or column.\n");
    CURSOR_POS(15, 5);
    printf("Match 5 or T/L -> Color Bomb: Swap to clear all of one color.\n");
    CURSOR_POS(16, 5);
    printf("Match Bomb in a line -> Explodes in a 3x3 area.\n");
    CURSOR_POS(17, 5);
    printf("Match Bomb + Bomb -> Clears the entire board!\n");
    CURSOR_POS(19, 1);
    printf(COLOR_BOLD "PRESS ANY KEY TO START...\n" COLOR_RESET);
    fflush(stdout);
}

void displayGame(const GameState *gs) {
    if (!gs) return;
    const char* candy_colors[] = {COLOR_RESET, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA};
    // --- NEW ASCII ART: Distinct, blocky patterns ---
    const char* ascii_art[NUM_CANDY_TYPES + 1][ASCII_ART_HEIGHT] = {
        {"     ", "     "}, // EMPTY
        {"█████", "█████"}, // Solid Block
        {"█ █ █", " █ █ "}, // Checkered
        {"VVVVV", "VVVVV"}, // Wavy
        {"/\\/\\/", "\\/\\/\\"}, // Jagged
        {" O O ", "O O O"}  // Circles
    };
    // --- NEW ASCII ART FOR SPECIALS ---
    const char* special_art[4][ASCII_ART_HEIGHT] = {
        {"", ""}, // None
        {"=====", "====="}, // Striped H
        {"|||||", "| | |"}, // Striped V
        {" / \\ ", "( B )"}  // Bomb
    };

    CURSOR_POS(1, 1);
    printf(CLEAR_SCREEN);
    printf(COLOR_BOLD "----------------------- C-CRUSH -----------------------\n" COLOR_RESET);
    CURSOR_POS(2, 1);
    printf("\nScore: %-5d / %-5d | Moves Left: %-5d | (Q to Quit)\n\n", gs->score, gs->targetScore, gs->movesLeft);
    CURSOR_POS(3, 1);
    printf("\n\n--------------------------------------------------------\n");
    
    size_t board_start_row = 7;
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t art_line = 0; art_line < ASCII_ART_HEIGHT; art_line++) {
            CURSOR_POS(board_start_row + (r * (ASCII_ART_HEIGHT + 1)) + art_line, 1);
            for (size_t c = 0; c < BOARD_WIDTH; c++) {
                bool is_cursor_on = (r == gs->cursor_r && c == gs->cursor_c);
                bool is_selected = (gs->mode == STATE_SELECTING_SWAP_DIR && r == gs->selected_r && c == gs->selected_c);
                
                if (is_cursor_on || is_selected) printf(CURSOR_COLOR);

                const Candy *candy = &gs->board[r][c];
                char left_bracket = ' ', right_bracket = ' ';
                if (is_cursor_on) {
                    left_bracket = (gs->mode == STATE_PLAYING_LEVEL) ? '>' : '{';
                    right_bracket = (gs->mode == STATE_PLAYING_LEVEL) ? '<' : '}';
                } else if (is_selected) {
                    left_bracket = '{'; right_bracket = '}';
                }
                
                printf("%c", left_bracket);
                if (candy->type == EMPTY_TYPE) {
                    printf("     ");
                } else {
                    // Use special art if available, otherwise use normal art
                    const char* art_to_use = (candy->special != SPECIAL_NONE) ? 
                                             special_art[candy->special][art_line] : 
                                             ascii_art[candy->type][art_line];
                    printf("%s%s%s", candy_colors[candy->type], art_to_use, COLOR_RESET);
                }
                
                if (is_cursor_on || is_selected) printf(CURSOR_COLOR);
                printf("%c", right_bracket);
                if (is_cursor_on || is_selected) printf(COLOR_RESET);
            }
        }
    }

    size_t bottom_ui_row = board_start_row + (BOARD_HEIGHT * (ASCII_ART_HEIGHT + 1));
    CURSOR_POS(bottom_ui_row, 1);
    printf("--------------------------------------------------------\n");
    CURSOR_POS(bottom_ui_row + 1, 1);
    printf("%s\n", gs->message);
    fflush(stdout);
}

void displayLevelComplete(const GameState *gs) {
    if (!gs) return;
    displayGame(gs);
    CURSOR_POS(BOARD_HEIGHT * (ASCII_ART_HEIGHT + 1) + 6, 1);
    printf(COLOR_GREEN COLOR_BOLD "--- LEVEL %d COMPLETE! ---\n" COLOR_RESET, gs->currentLevel);
    CURSOR_POS(BOARD_HEIGHT * (ASCII_ART_HEIGHT + 1) + 7, 1);
    printf("Press any key to continue to the next level...\n");
    fflush(stdout);
}

void displayGameOver(const GameState *gs) {
    if (!gs) return;
    displayGame(gs);
    CURSOR_POS(BOARD_HEIGHT * (ASCII_ART_HEIGHT + 1) + 6, 10);
    printf(COLOR_RED COLOR_BOLD "--- GAME OVER ---\n" COLOR_RESET);
    CURSOR_POS(BOARD_HEIGHT * (ASCII_ART_HEIGHT + 1) + 7, 10);
    printf("You did not reach the target score. Press any key to return to the main menu.\n");
    fflush(stdout);
}

void processInput(GameState *gs) {
    if (!gs) return;
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) handleFatalError("read");
    if (c == '\x1b') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': c = 'w'; break; case 'B': c = 's'; break;
                case 'C': c = 'd'; break; case 'D': c = 'a'; break;
            }
        }
    }
    if (c == '\0') return;
    if (c == 'q' || c == 'Q') {
        gs->mode = STATE_QUIT;
        return;
    }
    switch (gs->mode) {
        case STATE_SHOW_INTRO: startNewGame(gs); break;
        case STATE_PLAYING_LEVEL:
            snprintf(gs->message, sizeof(gs->message), "Use WASD/Arrows to move. SPACE to select.");
            switch (c) {
                case 'w': if (gs->cursor_r > 0) gs->cursor_r--; break;
                case 's': if (gs->cursor_r < BOARD_HEIGHT - 1) gs->cursor_r++; break;
                case 'a': if (gs->cursor_c > 0) gs->cursor_c--; break;
                case 'd': if (gs->cursor_c < BOARD_WIDTH - 1) gs->cursor_c++; break;
                case ' ': gs->selected_r = gs->cursor_r; gs->selected_c = gs->cursor_c; gs->mode = STATE_SELECTING_SWAP_DIR; break;
            }
            break;
        case STATE_SELECTING_SWAP_DIR:
            snprintf(gs->message, sizeof(gs->message), "Selected (%zu, %zu). Choose swap direction or SPACE to cancel.", gs->selected_r, gs->selected_c);
            int dr = 0, dc = 0;
            bool direction_chosen = true;
            switch (c) {
                case 'w': dr = -1; break; case 's': dr = 1; break;
                case 'a': dc = -1; break; case 'd': dc = 1; break;
                case ' ': gs->mode = STATE_PLAYING_LEVEL; direction_chosen = false; break;
                default: direction_chosen = false; break;
            }
            if (direction_chosen) {
                size_t r2 = (size_t)((int)gs->selected_r + dr);
                size_t c2 = (size_t)((int)gs->selected_c + dc);
                if (r2 < BOARD_HEIGHT && c2 < BOARD_WIDTH) updateGame(gs, r2, c2);
                else gs->mode = STATE_PLAYING_LEVEL;
            }
            break;
        case STATE_LEVEL_COMPLETE: loadLevel(gs, gs->currentLevel + 1); break;
        case STATE_GAME_OVER_FINAL: gs->mode = STATE_SHOW_INTRO; break;
        case STATE_PROCESSING: case STATE_QUIT: break;
        default: gs->mode = STATE_PLAYING_LEVEL; break;
    }
}

void updateGame(GameState *gs, size_t r2, size_t c2) {
    if (!gs) return;
    gs->mode = STATE_PROCESSING;
    snprintf(gs->message, sizeof(gs->message), "Checking move...");
    size_t r1 = gs->selected_r;
    size_t c1 = gs->selected_c;
    Candy c1_pre_swap = gs->board[r1][c1];
    Candy c2_pre_swap = gs->board[r2][c2];
    bool is_bomb_bomb_move = (c1_pre_swap.special == SPECIAL_BOMB && c2_pre_swap.special == SPECIAL_BOMB);
    bool is_bomb_move = is_bomb_bomb_move || (c1_pre_swap.special == SPECIAL_BOMB || c2_pre_swap.special == SPECIAL_BOMB);
    if (!is_bomb_move) {
        GameState tempState;
        memcpy(&tempState, gs, sizeof(GameState));
        Candy temp = tempState.board[r1][c1];
        tempState.board[r1][c1] = tempState.board[r2][c2];
        tempState.board[r2][c2] = temp;
        bool temp_clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
        findAndMarkMatches(&tempState, temp_clear_map);
        bool is_valid_move = false;
        for(size_t r=0; r<BOARD_HEIGHT; r++) for(size_t c=0; c<BOARD_WIDTH; c++) if(temp_clear_map[r][c]) is_valid_move = true;
        if (!is_valid_move) {
            snprintf(gs->message, sizeof(gs->message), "Invalid move! No match formed.");
            gs->mode = STATE_PLAYING_LEVEL;
            return;
        }
    }
    gs->movesLeft--;
    gs->board[r1][c1] = c2_pre_swap;
    gs->board[r2][c2] = c1_pre_swap;
    int turnScore = 0;
    int totalCleared;
    bool first_pass = true;
    do {
        if (!first_pass) {
            displayGame(gs);
            usleep(CASCADE_DELAY_US);
        }
        bool clear_map[BOARD_HEIGHT][BOARD_WIDTH] = {false};
        if (first_pass && is_bomb_move) {
            if (is_bomb_bomb_move) {
                snprintf(gs->message, sizeof(gs->message), "DOUBLE BOMB! Board cleared!");
                for(size_t r=0; r<BOARD_HEIGHT; r++) for(size_t c=0; c<BOARD_WIDTH; c++) clear_map[r][c] = true;
            } else {
                snprintf(gs->message, sizeof(gs->message), "BOMB! Clearing all of that type...");
                int target_type = EMPTY_TYPE;
                size_t bomb_final_r, bomb_final_c;
                if (c1_pre_swap.special == SPECIAL_BOMB) {
                    bomb_final_r = r2; bomb_final_c = c2;
                    target_type = c2_pre_swap.type;
                } else {
                    bomb_final_r = r1; bomb_final_c = c1;
                    target_type = c1_pre_swap.type;
                }
                clear_map[bomb_final_r][bomb_final_c] = true;
                if (target_type != EMPTY_TYPE) activateBomb(gs, clear_map, target_type);
            }
        } else {
            snprintf(gs->message, sizeof(gs->message), "Processing matches...");
            findAndMarkMatches(gs, clear_map);
            createSpecials(gs, clear_map, first_pass ? (int)r2 : -1, first_pass ? (int)c2 : -1);
            activateSpecials(gs, clear_map);
        }
        totalCleared = clearCandies(gs, clear_map);
        if (totalCleared > 0) {
            snprintf(gs->message, sizeof(gs->message), "Cleared %d candies! Gravity...", totalCleared);
            displayGame(gs);
            usleep(CASCADE_DELAY_US);
            turnScore += totalCleared;
            applyGravityAndRefill(gs);
        }
        first_pass = false;
    } while (totalCleared > 0);
    gs->score += turnScore;
    if (turnScore > 0) snprintf(gs->message, sizeof(gs->message), "Scored %d points that turn!", turnScore);
    if (gs->score >= gs->targetScore) gs->mode = STATE_LEVEL_COMPLETE;
    else if (gs->movesLeft <= 0) gs->mode = STATE_GAME_OVER_FINAL;
    else gs->mode = STATE_PLAYING_LEVEL;
}

// --- The Corrected Logic Pipeline Functions ---

void findAndMarkMatches(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    if (!gs || !clear_map) return;
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE) { c++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (c + match_len < BOARD_WIDTH && gs->board[r][c + match_len].type == match_type) match_len++;
            if (match_len >= 3) {
                for (size_t i = 0; i < match_len; i++) clear_map[r][c + i] = true;
            }
            c += match_len;
        }
    }
    for (size_t c = 0; c < BOARD_WIDTH; c++) {
        for (size_t r = 0; r < BOARD_HEIGHT - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE) { r++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (r + match_len < BOARD_HEIGHT && gs->board[r + match_len][c].type == match_type) match_len++;
            if (match_len >= 3) {
                for (size_t i = 0; i < match_len; i++) clear_map[r + i][c] = true;
            }
            r += match_len;
        }
    }
}

void createSpecials(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH], int move_r, int move_c) {
    if (!gs || !clear_map) return;
    bool h_matches[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    bool v_matches[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    unsigned char h_len[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    unsigned char v_len[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE || !clear_map[r][c]) { c++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (c + match_len < BOARD_WIDTH && gs->board[r][c + match_len].type == match_type && clear_map[r][c+match_len]) match_len++;
            if (match_len >= 3) {
                h_len[r][c] = match_len;
                for (size_t i = 0; i < match_len; i++) h_matches[r][c + i] = true;
            }
            c += match_len;
        }
    }
    for (size_t c = 0; c < BOARD_WIDTH; c++) {
        for (size_t r = 0; r < BOARD_HEIGHT - 2; ) {
            if (gs->board[r][c].type == EMPTY_TYPE || !clear_map[r][c]) { r++; continue; }
            int match_type = gs->board[r][c].type;
            size_t match_len = 1;
            while (r + match_len < BOARD_HEIGHT && gs->board[r + match_len][c].type == match_type && clear_map[r+match_len][c]) match_len++;
            if (match_len >= 3) {
                v_len[r][c] = match_len;
                for (size_t i = 0; i < match_len; i++) v_matches[r + i][c] = true;
            }
            r += match_len;
        }
    }
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            bool is_h = h_matches[r][c];
            bool is_v = v_matches[r][c];
            bool is_move_spot = (r == (size_t)move_r && c == (size_t)move_c);
            if (is_h && is_v) {
                if (is_move_spot || gs->board[r][c].special == SPECIAL_NONE) {
                    gs->board[r][c].special = SPECIAL_BOMB;
                    clear_map[r][c] = false;
                }
            } else if (h_len[r][c] >= 5 || v_len[r][c] >= 5) {
                if (is_move_spot || gs->board[r][c].special == SPECIAL_NONE) {
                    gs->board[r][c].special = SPECIAL_BOMB;
                    clear_map[r][c] = false;
                }
            } else if (h_len[r][c] == 4) {
                if (is_move_spot || gs->board[r][c].special == SPECIAL_NONE) {
                    gs->board[r][c].special = SPECIAL_STRIPED_V;
                    clear_map[r][c] = false;
                }
            } else if (v_len[r][c] == 4) {
                if (is_move_spot || gs->board[r][c].special == SPECIAL_NONE) {
                    gs->board[r][c].special = SPECIAL_STRIPED_H;
                    clear_map[r][c] = false;
                }
            }
        }
    }
}

void activateSpecials(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    if (!gs || !clear_map) return;
    bool changed_in_pass;
    do {
        changed_in_pass = false;
        for (size_t r = 0; r < BOARD_HEIGHT; r++) {
            for (size_t c = 0; c < BOARD_WIDTH; c++) {
                if (clear_map[r][c] && gs->board[r][c].special != SPECIAL_NONE) {
                    if (gs->board[r][c].special == SPECIAL_STRIPED_H) {
                        for (size_t i = 0; i < BOARD_WIDTH; i++) if (!clear_map[r][i]) { clear_map[r][i] = true; changed_in_pass = true; }
                    } else if (gs->board[r][c].special == SPECIAL_STRIPED_V) {
                        for (size_t i = 0; i < BOARD_HEIGHT; i++) if (!clear_map[i][c]) { clear_map[i][c] = true; changed_in_pass = true; }
                    } else if (gs->board[r][c].special == SPECIAL_BOMB) {
                        for (int dr = -1; dr <= 1; dr++) {
                            for (int dc = -1; dc <= 1; dc++) {
                                size_t nr = r + dr;
                                size_t nc = c + dc;
                                if (nr < BOARD_HEIGHT && nc < BOARD_WIDTH) {
                                    if (!clear_map[nr][nc]) {
                                        clear_map[nr][nc] = true;
                                        changed_in_pass = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } while (changed_in_pass);
}

void activateBomb(const GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH], int target_type) {
    if (!gs || !clear_map || target_type == EMPTY_TYPE) return;
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            if (gs->board[r][c].type == target_type) {
                clear_map[r][c] = true;
            }
        }
    }
}

int clearCandies(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH]) {
    if (!gs || !clear_map) return 0;
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
    if (!gs) return;
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

// --- System & Terminal Utility Functions ---
void handleFatalError(const char *msg) {
    disableRawMode();
    perror(msg);
    exit(1);
}
void disableRawMode() {
    printf(SHOW_CURSOR);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) perror("tcsetattr");
}
void enableRawMode() {
    printf(HIDE_CURSOR);
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) handleFatalError("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) handleFatalError("tcsetattr");
}
void getTerminalSize(int *rows, int *cols) {
    if (!rows || !cols) return;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        *rows = 24; *cols = 80;
    } else {
        *cols = ws.ws_col; *rows = ws.ws_row;
    }
}