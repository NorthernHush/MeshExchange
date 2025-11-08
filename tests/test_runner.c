#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple test framework
typedef struct {
    const char *name;
    void (*func)(void);
    int passed;
} test_t;

static test_t *tests = NULL;
static int test_count = 0;
static int tests_passed = 0;

void register_test(const char *name, void (*func)(void)) {
    tests = realloc(tests, sizeof(test_t) * (test_count + 1));
    tests[test_count].name = name;
    tests[test_count].func = func;
    tests[test_count].passed = 0;
    test_count++;
}

void run_tests(void) {
    printf("Running %d tests...\n\n", test_count);

    for (int i = 0; i < test_count; i++) {
        printf("Running test: %s\n", tests[i].name);

        // Simple try-catch simulation
        if (setjmp(test_env) == 0) {
            tests[i].func();
            tests[i].passed = 1;
            tests_passed++;
            printf("✓ PASSED\n");
        } else {
            printf("✗ FAILED\n");
        }
        printf("\n");
    }

    printf("Results: %d/%d tests passed\n", tests_passed, test_count);

    if (tests_passed == test_count) {
        printf("All tests passed!\n");
        exit(0);
    } else {
        printf("Some tests failed!\n");
        exit(1);
    }
}

#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
            longjmp(test_env, 1); \
        } \
    } while (0)

#define TEST_ASSERT_STR_EQ(a, b) TEST_ASSERT(strcmp(a, b) == 0)
#define TEST_ASSERT_INT_EQ(a, b) TEST_ASSERT(a == b)
#define TEST_ASSERT_NULL(a) TEST_ASSERT(a == NULL)
#define TEST_ASSERT_NOT_NULL(a) TEST_ASSERT(a != NULL)

// Simple jump buffer for test failures
#include <setjmp.h>
static jmp_buf test_env;

int main(int argc, char *argv[]) {
    // Register all tests here
    // This will be populated by including test files

    run_tests();
    return 0;
}
