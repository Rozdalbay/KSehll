#pragma once

#ifndef KSHELL_TEST_H
#define KSHELL_TEST_H

#include <iostream>
#include <string>
#include <vector>
#include <functional>

#define TEST_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            std::wcerr << L"  FAIL: " << msg << L"\n"; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            std::wcerr << L"  FAIL: " << msg << L"\n"; \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(expr) TEST_ASSERT(expr, L"")
#define TEST_ASSERT_FALSE(expr) TEST_ASSERT(!(expr), L"")

struct TestCase
{
    std::wstring name;
    std::function<bool()> func;
};

inline int runTests(const std::vector<TestCase>& tests)
{
    int passed = 0;
    int failed = 0;

    for (const auto& test : tests)
    {
        std::wcout << L"Running: " << test.name << L"... ";
        if (test.func())
        {
            std::wcout << L"PASS\n";
            ++passed;
        }
        else
        {
            std::wcout << L"FAIL\n";
            ++failed;
        }
    }

    std::wcout << L"\nResults: " << passed << L" passed, " << failed << L" failed, "
               << (passed + failed) << L" total\n";
    return failed;
}

#endif // KSHELL_TEST_H