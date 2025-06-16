// functions.c
#include "functions.h"

// The compiler optimizes this file by itself, but it can't
// change how it's CALLED from main.c.

int process_by_value(BigStruct s) {
    // This copy operation is what we want to measure.
    return s.data[0] + s.data[BIG_ARRAY_SIZE - 1];
}

int process_by_reference(const BigStruct *s_ptr) {
    return s_ptr->data[0] + s_ptr->data[BIG_ARRAY_SIZE - 1];
}