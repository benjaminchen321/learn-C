#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <math.h> // For abs()

#define BOARD_WIDTH 8
#define BOARD_HEIGHT 8
#define NUM_CANDY_TYPES 4
#define EMPTY_SPACE 0

// Function Prototypes
void populateBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]); // NEW
void displayBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]);
bool isValidSwap(int board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2); // NEW
void swapCandies(int board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2); // NEW
int findAndFlagMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]);
void removeMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]);
void applyGravityAndRefill(int board[BOARD_HEIGHT][BOARD_WIDTH]); // RENAMED

int main(void) {
    int gameBoard[BOARD_HEIGHT][BOARD_WIDTH];
    int score = 0;
    srand(time(NULL));

    populateBoard(gameBoard);

    // The main interactive game loop
    while (true) {
        printf("\n--- Score: %d ---\n", score);
        displayBoard(gameBoard);

        // Get user input
        int r1, c1, r2, c2;
        printf("Enter swap coordinates (row1 col1 row2 col2): ");
        int result = scanf("%d %d %d %d", &r1, &c1, &r2, &c2);

        // Input validation
        if (result != 4 || r1 < 0 || r1 >= BOARD_HEIGHT || c1 < 0 || c1 >= BOARD_WIDTH ||
            r2 < 0 || r2 >= BOARD_HEIGHT || c2 < 0 || c2 >= BOARD_WIDTH) {
            printf("Invalid input. Please enter 4 numbers between 0 and 7.\n");
            // Clear the input buffer to prevent an infinite loop on bad text input
            while (getchar() != '\n');
            continue; // Skip to the next loop iteration
        }

        // Check for a valid move
        if (!isValidSwap(gameBoard, r1, c1, r2, c2)) {
            printf("Invalid swap. Must be adjacent and create a match.\n");
            continue;
        }

        // Perform the swap
        swapCandies(gameBoard, r1, c1, r2, c2);
        printf("\nPerforming swap...\n");
        displayBoard(gameBoard);

        // Handle cascades
        int turnScore = 0;
        while (true) {
            int matchesFound = findAndFlagMatches(gameBoard);
            if (matchesFound == 0) {
                break;
            }
            turnScore += matchesFound;
            removeMatches(gameBoard);
            applyGravityAndRefill(gameBoard);
            printf("\nCascade! Board after clearing and refill:\n");
            displayBoard(gameBoard);
        }
        score += turnScore;
        printf("You scored %d points this turn!\n", turnScore);
    }

    return 0;
}

// NEW: Extracted board population into its own function
void populateBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    do {
        for (int row = 0; row < BOARD_HEIGHT; row++) {
            for (int col = 0; col < BOARD_WIDTH; col++) {
                board[row][col] = rand() % NUM_CANDY_TYPES + 1;
            }
        }
    // Ensure the initial board has at least one match to start.
    // In a real game, you'd ensure NO matches exist to start.
    // For our test program, this guarantees we can see cascades.
    } while (findAndFlagMatches(board) == 0);

    // Now clear the flags for the actual start of the game
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            board[row][col] = abs(board[row][col]);
        }
    }
}

void displayBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    printf("   ");
    for(int i = 0; i < BOARD_WIDTH; i++) printf(" %d ", i);
    printf("\n-----------------------------\n");
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        printf("%d |", row);
        for (int col = 0; col < BOARD_WIDTH; col++) {
            printf(" %d ", board[row][col]);
        }
        printf("|\n");
    }
    printf("-----------------------------\n");
}

// NEW: Checks if a swap is legal.
bool isValidSwap(int board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2) {
    // 1. Check if candies are adjacent
    int row_diff = abs(r1 - r2);
    int col_diff = abs(c1 - c2);
    if (!((row_diff == 1 && col_diff == 0) || (row_diff == 0 && col_diff == 1))) {
        return false; // Not adjacent
    }

    // 2. Temporarily perform the swap
    swapCandies(board, r1, c1, r2, c2);

    // 3. Check if the swap creates a match
    int matches = findAndFlagMatches(board);

    // 4. Swap back to restore the original board state
    swapCandies(board, r1, c1, r2, c2);
    
    // 5. Unflag any candies that were flagged during the check
    for (int r = 0; r < BOARD_HEIGHT; r++) {
        for (int c = 0; c < BOARD_WIDTH; c++) {
            board[r][c] = abs(board[r][c]);
        }
    }

    return matches > 0;
}

// NEW: Simple function to swap two candies on the board.
void swapCandies(int board[BOARD_HEIGHT][BOARD_WIDTH], int r1, int c1, int r2, int c2) {
    int temp = board[r1][c1];
    board[r1][c1] = board[r2][c2];
    board[r2][c2] = temp;
}

// RENAMED and UPDATED from previous step's challenge
void applyGravityAndRefill(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (int col = 0; col < BOARD_WIDTH; col++) {
        int write_row = BOARD_HEIGHT - 1;
        for (int read_row = BOARD_HEIGHT - 1; read_row >= 0; read_row--) {
            if (board[read_row][col] != EMPTY_SPACE) {
                board[write_row][col] = board[read_row][col];
                write_row--;
            }
        }
        while (write_row >= 0) {
            board[write_row][col] = rand() % NUM_CANDY_TYPES + 1; // Refill with random
            write_row--;
        }
    }
}

// findAndFlagMatches and removeMatches are unchanged from the previous step.
// (You can copy them from your last file).
// I'll include them here for completeness.

int findAndFlagMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    int matchBoard[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    int matches = 0;

    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH - 2; col++) {
            int candy = abs(board[row][col]);
            if (candy != EMPTY_SPACE && candy == abs(board[row][col + 1]) && candy == abs(board[row][col + 2])) {
                matchBoard[row][col] = 1; matchBoard[row][col + 1] = 1; matchBoard[row][col + 2] = 1;
            }
        }
    }
    for (int col = 0; col < BOARD_WIDTH; col++) {
        for (int row = 0; row < BOARD_HEIGHT - 2; row++) {
            int candy = abs(board[row][col]);
            if (candy != EMPTY_SPACE && candy == abs(board[row + 1][col]) && candy == abs(board[row + 2][col])) {
                matchBoard[row][col] = 1; matchBoard[row + 1][col] = 1; matchBoard[row + 2][col] = 1;
            }
        }
    }
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            if (matchBoard[row][col] == 1 && board[row][col] > 0) {
                matches++;
                board[row][col] = -board[row][col];
            }
        }
    }
    return matches;
}

void removeMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            if (board[row][col] < 0) {
                board[row][col] = EMPTY_SPACE;
            }
        }
    }
}