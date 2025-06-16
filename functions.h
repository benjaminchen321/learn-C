// functions.h
#ifndef FUNCTIONS_H
#define FUNCTIONS_H

// Define the large struct here so both files know about it.
#define BIG_ARRAY_SIZE 500000

typedef struct {
    int data[BIG_ARRAY_SIZE];
    double more_data[BIG_ARRAY_SIZE];
    char name[128];
} BigStruct;

// Function declarations (prototypes)
int process_by_value(BigStruct s);
int process_by_reference(const BigStruct *s_ptr);

#endif // FUNCTIONS_H