#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_WIDTH 8
#define BOARD_HEIGHT 8
#define NUM_CANDY_TYPES 4

// NEW: Function Prototype
// This tells the compiler, "Hey, a function named displayBoard exists.
// It takes a 2D int array of this specific size as an argument and returns nothing (void)."
// This is necessary because the compiler reads the file from top to bottom.
// Without this, when main() tries to call displayBoard(), the compiler wouldn't know it exists yet.
void displayBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]);
int findAndFlagMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]);

int main(void) {
    int gameBoard[BOARD_HEIGHT][BOARD_WIDTH];
    srand(time(NULL));

    // Populating the board (same as before)
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            gameBoard[row][col] = rand() % NUM_CANDY_TYPES + 1;
        }
    }

    // --- Displaying the board ---
    // REFACTORED: All the display logic is now in a function call.
    printf("Welcome to C-Crush!\n\n");
    printf("Original Board:\n");
    displayBoard(gameBoard);

    int matchesFound = findAndFlagMatches(gameBoard);

    printf("\nFound %d matches.\n", matchesFound);
    printf("Board with flagged matches (negative numbers):\n");
    displayBoard(gameBoard);

    return 0;
}

// NEW: Function Definition
// Here is the actual code for the function.
void displayBoard(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            printf("%d ", board[row][col]);
        }
        printf("\n");
    }
}

int findAndFlagMatches(int board[BOARD_HEIGHT][BOARD_WIDTH]) {
    int matchBoard[BOARD_HEIGHT][BOARD_WIDTH] = {0};
    int matches = 0;

    // Check for horizontal matches
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH - 2; col++) {
            int candy = board[row][col];
            if (candy == board[row][col + 1] && candy == board[row][col + 2]) {
                matchBoard[row][col] = 1;     
                matchBoard[row][col + 1] = 1; 
                matchBoard[row][col + 2] = 1; 
            }
        }
    }

    // Check for vertical matches
    for (int col = 0; col < BOARD_WIDTH; col++) {
        for (int row = 0; row < BOARD_HEIGHT - 2; row++) {
            int candy = board[row][col];
            if (candy == board[row + 1][col] && candy == board[row + 2][col]) {
                matchBoard[row][col] = 1;     
                matchBoard[row + 1][col] = 1; 
                matchBoard[row + 2][col] = 1; 
            }
        }
    }

    // Flag matches in the original board
    for (int row = 0; row < BOARD_HEIGHT; row++) {
        for (int col = 0; col < BOARD_WIDTH; col++) {
            if (matchBoard[row][col] == 1) {
                matches++;
                board[row][col] = -abs(board[row][col]);
            }
        }
    }

    return matches;
}