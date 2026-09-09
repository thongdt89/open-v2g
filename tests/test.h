#ifndef OPENV2G_TEST_H
#define OPENV2G_TEST_H

#include <stdio.h>

#define TEST_ASSERT(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            (void)fprintf(stderr, "Assertion failed at %s:%d: %s\n",           \
                          __FILE__, __LINE__, #condition);                       \
            return 1;                                                            \
        }                                                                        \
    } while (0)

#endif
