/* tests/unity.h
 *
 * Минимальный самодостаточный тестовый фреймворк (подмножество API
 * Unity). Вендорится вместо FetchContent, чтобы тесты собирались
 * офлайн и без сетевых зависимостей.
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern int unity_failures;

void unity_run_test(const char *name, void (*fn)(void));
void unity_fail(const char *file, int line, const char *msg);
int  unity_end(void);

#define UNITY_BEGIN() do { unity_failures = 0; } while (0)
#define UNITY_END()   unity_end()

#define RUN_TEST(fn) unity_run_test(#fn, fn)

#define TEST_ASSERT_TRUE(cond) \
    do { if (!(cond)) unity_fail(__FILE__, __LINE__, "expected true: " #cond); } while (0)

#define TEST_ASSERT_FALSE(cond) \
    do { if ((cond)) unity_fail(__FILE__, __LINE__, "expected false: " #cond); } while (0)

#define TEST_ASSERT_EQUAL_UINT16(expected, actual) \
    do { \
        uint16_t _e = (uint16_t)(expected), _a = (uint16_t)(actual); \
        if (_e != _a) { \
            char _b[128]; \
            snprintf(_b, sizeof _b, "expected %u got %u (%s)", \
                     (unsigned)_e, (unsigned)_a, #actual); \
            unity_fail(__FILE__, __LINE__, _b); \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL_UINT32(expected, actual) \
    do { \
        uint32_t _e = (uint32_t)(expected), _a = (uint32_t)(actual); \
        if (_e != _a) { \
            char _b[128]; \
            snprintf(_b, sizeof _b, "expected %u got %u (%s)", \
                     (unsigned)_e, (unsigned)_a, #actual); \
            unity_fail(__FILE__, __LINE__, _b); \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL_HEX32(expected, actual) \
    do { \
        uint32_t _e = (uint32_t)(expected), _a = (uint32_t)(actual); \
        if (_e != _a) { \
            char _b[128]; \
            snprintf(_b, sizeof _b, "expected 0x%08X got 0x%08X (%s)", \
                     (unsigned)_e, (unsigned)_a, #actual); \
            unity_fail(__FILE__, __LINE__, _b); \
        } \
    } while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        long _e = (long)(expected), _a = (long)(actual); \
        if (_e != _a) { \
            char _b[128]; \
            snprintf(_b, sizeof _b, "expected %ld got %ld (%s)", \
                     _e, _a, #actual); \
            unity_fail(__FILE__, __LINE__, _b); \
        } \
    } while (0)

#endif /* UNITY_H */