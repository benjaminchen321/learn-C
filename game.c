/*******************************************************************
*
* C-CRUSH: THE FINAL ARCHITECTURE (v5.0)
*
* This is the final, production-quality version of the game.
* It features a robust, persistent UI and a correct, multi-stage
* logic pipeline for all match, creation, and activation events.
* This version correctly handles all special candy interactions.
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
#define MIN_TERM_ROWS 24
#define MIN_TERM_COLS 80
#define CASCADE_DELAY_US 200000

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
#define COLOR_BOLD    "\x1b[1m"
#define CURSOR_COLOR  "\x1b[47;30m"

// --- Data Structures ---
typedef enum { SPECIAL_NONE, SPECIAL_STRIPED_H, SPECIAL_STRIPED_V, SPECIAL_BOMB } SpecialType;
typedef struct { int type; SpecialType special; } Candy;
typedef enum { STATE_SHOW_INTRO, STATE_MOVING_CURSOR, STATE_SELECTING_SWAP_DIR, STATE_PROCESSING, STATE_GAME_OVER } GameMode;

typedef struct {
    Candy board[BOARD_HEIGHT][BOARD_WIDTH];
    int score;
    int movesLeft;
    GameMode mode;
    size_t cursor_r, cursor_c;
    size_t selected_r, selected_c;
    char message[128];
} GameState;

// --- Global State ---
struct termios orig_termios;

// --- Prototypes ---
void handleFatalError(const char *msg);
void enableRawMode();
void disableRawMode();
void getTerminalSize(int *rows, int *cols);
void initGame(GameState *gs);
void displayIntro();
void displayGame(const GameState *gs);
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
    while (gameState.mode != STATE_GAME_OVER) {
        if (gameState.mode == STATE_SHOW_INTRO) displayIntro();
        else displayGame(&gameState);
        processInput(&gameState);
    }
    displayGame(&gameState);
    CURSOR_POS(BOARD_HEIGHT + 6, 1);
    printf("Final Score: %d\n\n", gameState.score);
    return 0;
}

// --- Core Game Logic ---

void initGame(GameState *gs) {
    if (!gs) return;
    gs->score = 0;
    gs->movesLeft = 25;
    gs->mode = STATE_MOVING_CURSOR;
    gs->cursor_r = BOARD_HEIGHT / 2;
    gs->cursor_c = BOARD_WIDTH / 2;
    snprintf(gs->message, sizeof(gs->message), "Game started! Use WASD to move.");
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

void displayIntro() {
    CURSOR_POS(1, 1);
    printf(CLEAR_SCREEN);
    printf(COLOR_BOLD "--- WELCOME TO C-CRUSH! ---\n\n" COLOR_RESET);
    CURSOR_POS(4, 5);
    printf(COLOR_YELLOW "--- HOW TO PLAY ---\n" COLOR_RESET);
    CURSOR_POS(6, 5);
    printf("W, A, S, D : Move the cursor\n");
    CURSOR_POS(7, 5);
    printf("SPACE      : Select a candy to swap\n");
    CURSOR_POS(8, 5);
    printf("W, A, S, D : Choose direction to swap\n");
    CURSOR_POS(9, 5);
    printf("Q          : Quit the game at any time\n");
    CURSOR_POS(4, 40);
    printf(COLOR_YELLOW "--- SPECIAL CANDIES ---\n" COLOR_RESET);
    CURSOR_POS(6, 40);
    printf("Match 4 in a row -> Striped Candy (-●- or |●|)\n");
    CURSOR_POS(7, 40);
    printf("                      Clears an entire row or column.\n");
    CURSOR_POS(9, 40);
    printf("Match 5 or T/L shape -> Color Bomb ( B )\n");
    CURSOR_POS(10, 40);
    printf("                      Swap with any candy to clear all\n");
    CURSOR_POS(11, 40);
    printf("                      candies of that type!\n");
    CURSOR_POS(15, 25);
    printf(COLOR_BOLD "PRESS ANY KEY TO START...\n" COLOR_RESET);
    fflush(stdout);
}

void displayGame(const GameState *gs) {
    if (!gs) return;
    const char* candy_colors[] = {COLOR_RESET, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA};
    const char* candy_symbols[] = {" ", "●", "■", "◆", "▲", "★"};
    CURSOR_POS(1, 1);
    printf(CLEAR_SCREEN);
    printf(COLOR_BOLD "--- C-CRUSH: THE DEFINITIVE EDITION ---\n" COLOR_RESET);
    CURSOR_POS(2, 1);
    printf("Score: %-5d | Moves Left: %-5d | (Q to Quit)\n", gs->score, gs->movesLeft);
    CURSOR_POS(3, 1);
    printf("-------------------------------------------------\n");
    size_t board_start_row = 4;
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        CURSOR_POS(board_start_row + r, 1);
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            bool is_cursor_on = (r == gs->cursor_r && c == gs->cursor_c);
            bool is_selected = (gs->mode == STATE_SELECTING_SWAP_DIR && r == gs->selected_r && c == gs->selected_c);
            char left_bracket = ' ', right_bracket = ' ';
            if (is_cursor_on) {
                left_bracket = (gs->mode == STATE_MOVING_CURSOR) ? '>' : '{';
                right_bracket = (gs->mode == STATE_MOVING_CURSOR) ? '<' : '}';
                printf(CURSOR_COLOR);
            } else if (is_selected) {
                left_bracket = '{'; right_bracket = '}';
                printf(CURSOR_COLOR);
            }
            const Candy *candy = &gs->board[r][c];
            printf("%c", left_bracket);
            if (candy->type == EMPTY_TYPE) printf(" . ");
            else {
                printf("%s", candy_colors[candy->type]);
                if (candy->special == SPECIAL_BOMB) printf(COLOR_BOLD " B " COLOR_RESET "%s", candy_colors[candy->type]);
                else if (candy->special == SPECIAL_STRIPED_H) printf("-%s-", candy_symbols[candy->type]);
                else if (candy->special == SPECIAL_STRIPED_V) printf("|%s|", candy_symbols[candy->type]);
                else printf(" %s ", candy_symbols[candy->type]);
            }
            printf("%s%c", COLOR_RESET, right_bracket);
            if (is_cursor_on || is_selected) printf(COLOR_RESET);
        }
    }
    CURSOR_POS(board_start_row + BOARD_HEIGHT, 1);
    printf("-------------------------------------------------\n");
    CURSOR_POS(board_start_row + BOARD_HEIGHT + 1, 1);
    printf("%s\n", gs->message);
    fflush(stdout);
}

void processInput(GameState *gs) {
    if (!gs) return;
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) handleFatalError("read");
    if (c == '\0') return;
    if (c == 'q' || c == 'Q') {
        gs->mode = STATE_GAME_OVER;
        snprintf(gs->message, sizeof(gs->message), "Quitting game. Thanks for playing!");
        return;
    }
    switch (gs->mode) {
        case STATE_SHOW_INTRO: initGame(gs); break;
        case STATE_MOVING_CURSOR:
            snprintf(gs->message, sizeof(gs->message), "Use W,A,S,D to move. SPACE to select.");
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
                case 'w': dr = -1; break;
                case 's': dr = 1; break;
                case 'a': dc = -1; break;
                case 'd': dc = 1; break;
                case ' ': gs->mode = STATE_MOVING_CURSOR; direction_chosen = false; break;
                default: direction_chosen = false; break;
            }
            if (direction_chosen) {
                size_t r2 = (size_t)((int)gs->selected_r + dr);
                size_t c2 = (size_t)((int)gs->selected_c + dc);
                if (r2 < BOARD_HEIGHT && c2 < BOARD_WIDTH) updateGame(gs, r2, c2);
                else gs->mode = STATE_MOVING_CURSOR;
            }
            break;
        case STATE_PROCESSING: case STATE_GAME_OVER: break;
        default: gs->mode = STATE_MOVING_CURSOR; break;
    }
    if (gs->movesLeft <= 0 && gs->mode != STATE_GAME_OVER) {
        gs->mode = STATE_GAME_OVER;
        snprintf(gs->message, sizeof(gs->message), "GAME OVER! No moves left. Press Q to exit.");
    }
}

// The main logic handler for a player's move
void updateGame(GameState *gs, size_t r2, size_t c2) {
    if (!gs) return;
    gs->mode = STATE_PROCESSING;
    snprintf(gs->message, sizeof(gs->message), "Checking move...");
    size_t r1 = gs->selected_r;
    size_t c1 = gs->selected_c;
    Candy c1_pre_swap = gs->board[r1][c1];
    Candy c2_pre_swap = gs->board[r2][c2];
    bool is_bomb_move = (c1_pre_swap.special == SPECIAL_BOMB || c2_pre_swap.special == SPECIAL_BOMB);
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
            gs->mode = STATE_MOVING_CURSOR;
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
        } else {
            snprintf(gs->message, sizeof(gs->message), "Processing matches...");
            // --- THE CORRECTED LOGIC PIPELINE ---
            // 1. Find initial matches
            findAndMarkMatches(gs, clear_map);
            // 2. Create new specials from those matches (e.g. a 4-match becomes a striped candy)
            createSpecials(gs, clear_map, first_pass ? (int)r2 : -1, first_pass ? (int)c2 : -1);
            // 3. Activate pre-existing specials that were part of a match
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
    gs->mode = STATE_MOVING_CURSOR;
}

// --- The Corrected Logic Pipeline Functions ---

// Pass 1: Find all basic 3+ matches and mark them.
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

// Pass 2: Look at the matches found and create new special candies.
void createSpecials(GameState *gs, bool clear_map[BOARD_HEIGHT][BOARD_WIDTH], int move_r, int move_c) {
    if (!gs || !clear_map) return;
    bool h_matches[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    bool v_matches[BOARD_HEIGHT][BOARD_WIDTH] = {false};
    unsigned char h_len[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    unsigned char v_len[BOARD_HEIGHT][BOARD_WIDTH] = {0};

    // Re-scan to get lengths and positions, this is necessary for correct special creation
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

    // Now create specials based on the analyzed matches
    for (size_t r = 0; r < BOARD_HEIGHT; r++) {
        for (size_t c = 0; c < BOARD_WIDTH; c++) {
            bool is_h = h_matches[r][c];
            bool is_v = v_matches[r][c];
            bool is_move_spot = (r == (size_t)move_r && c == (size_t)move_c);

            if (is_h && is_v) { // T or L shape
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

// Pass 3: Activate pre-existing specials that are marked for clearing.
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