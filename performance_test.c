#include <stdio.h>
#include <stdlib.h>
#include <time.h> // For clock_gettime and struct timespec
#include <string.h> // For memset

// --- Define a VERY large struct to stress the memory bus ---
// Let's make it roughly 4 Megabytes.
#define BIG_ARRAY_SIZE 500000 // 500,000 integers

typedef struct {
    int data[BIG_ARRAY_SIZE];
    double more_data[BIG_ARRAY_SIZE];
    char name[128];
} BigStruct;

// --- Functions to be tested ---
// The 'volatile' keyword is a hint to the compiler to not optimize this function away.
// We also return the result to ensure the work isn't completely eliminated.
volatile int process_by_value(BigStruct s) {
    return s.data[0] + s.data[BIG_ARRAY_SIZE - 1];
}

volatile int process_by_reference(const BigStruct *s_ptr) {
    return s_ptr->data[0] + s_ptr->data[BIG_ARRAY_SIZE - 1];
}

int main(void) {
    printf("Setting up the test...\n");

    // Use malloc to allocate the struct on the heap.
    // A struct this large might cause a stack overflow if declared locally in main.
    BigStruct *my_struct_ptr = malloc(sizeof(BigStruct));
    if (my_struct_ptr == NULL) {
        perror("Failed to allocate memory");
        return 1;
    }
    // Initialize it
    memset(my_struct_ptr, 0, sizeof(BigStruct));
    my_struct_ptr->data[0] = 10;
    my_struct_ptr->data[BIG_ARRAY_SIZE - 1] = 20;

    // Increase iterations significantly
    const long long ITERATIONS = 50000; // Fifty thousand calls

    // A variable to store results to prevent the compiler from optimizing out the calls
    int result_sink = 0;

    // Use a high-resolution timer
    struct timespec start, end;
    double time_spent;

    printf("Struct size: %.2f MB\n", sizeof(BigStruct) / (1024.0 * 1024.0));
    printf("Iterations: %lld\n\n", ITERATIONS);

    // --- Test Pass-by-Value ---
    printf("Testing Pass-by-Value (this should take a moment)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start); // Start high-res timer

    for (long long i = 0; i < ITERATIONS; i++) {
        // We pass the dereferenced struct, which creates a copy
        result_sink += process_by_value(*my_struct_ptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end); // Stop high-res timer
    time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double time_spent_value = time_spent;
    printf("Time taken: %f seconds\n\n", time_spent_value);


    // --- Test Pass-by-Reference ---
    printf("Testing Pass-by-Reference (this should be very fast)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (long long i = 0; i < ITERATIONS; i++) {
        result_sink += process_by_reference(my_struct_ptr);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double time_spent_ref = time_spent;
    printf("Time taken: %f seconds\n\n", time_spent_ref);
    
    // --- Analysis ---
    printf("--- Analysis ---\n");
    printf("Size of BigStruct: %zu bytes\n", sizeof(BigStruct));
    printf("Size of a pointer to BigStruct: %zu bytes\n", sizeof(BigStruct *));
    
    if (time_spent_ref > 0) {
        printf("Pass-by-value was %.2f times slower than pass-by-reference.\n", time_spent_value / time_spent_ref);
    } else {
        printf("Pass-by-reference was too fast to measure a meaningful ratio.\n");
    }

    // Free the allocated memory
    free(my_struct_ptr);
    // Use result_sink to show the compiler it's needed
    if (result_sink == 12345) printf("Magic number!\n");

    return 0;
}