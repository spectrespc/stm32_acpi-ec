/* tests/unity.c */

#include "unity.h"

int unity_failures;

static int unity_count;

void unity_fail(const char *file, int line, const char *msg)
{
    printf("    FAIL %s:%d: %s\n", file, line, msg);
    unity_failures++;
}

void unity_run_test(const char *name, void (*fn)(void))
{
    int before = unity_failures;

    fn();
    unity_count++;

    printf("  [%s] %s\n", (unity_failures == before) ? "PASS" : "FAIL", name);
}

int unity_end(void)
{
    printf("Итого: %d тестов, %d ошибок\n", unity_count, unity_failures);
    return (unity_failures == 0) ? 0 : 1;
}
