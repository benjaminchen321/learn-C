// main.c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "functions.h" // Include our custom header

int main(void) {
    printf("Setting up the definitive test...\n");

    BigStruct *my_struct_ptr = malloc(sizeof(BigStruct));
    if (my_struct_ptr == NULL) {
        perror("Failed to allocate memory");
        return 1;
    }
    memset(my_struct_ptr, 0, sizeof(BigStruct));
    my_struct_ptr->data[0] = 10;
    my_struct_ptr->data[BIG_ARRAY_SIZE - 1] = 20;

    const long long ITERATIONS = 50000;
    int result_sink = 0;
    struct timespec start, end;
    double time_spent;

    printf("Struct size: %.2f MB\n", sizeof(BigStruct) / (1024.0 * 1024.0));
    printf("Iterations: %lld\n\n", ITERATIONS);

    // --- Test Pass-by-Value ---
    printf("Testing Pass-by-Value (forcing the copy)...\n");
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (long long i = 0; i < ITERATIONS; i++) {
        result_sink += process_by_value(*my_struct_ptr);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    double time_spent_value = time_spent;
    printf("Time taken: %f seconds\n\n", time_spent_value);

    // --- Test Pass-by-Reference ---
    printf("Testing Pass-by-Reference...\n");
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
    if (time_spent_ref > 1e-9) { // Check if not zero
        printf("Pass-by-value was %.2f times slower than pass-by-reference.\n", time_spent_value / time_spent_ref);
    } else {
        printf("Pass-by-reference was too fast to measure a meaningful ratio.\n");
    }

    free(my_struct_ptr);
    if (result_sink == 12345) printf("Magic number!\n");

    return 0;
}