#pragma once

#include <unity.h>

namespace httpadv
{
    inline namespace v1
    {
        namespace TestSupport
        {
            using SuiteHook = void (*)();
            using SuiteRunner = int (*)();

            void SetActiveSuiteHooks(SuiteHook setUpHook, SuiteHook tearDownHook);
            SuiteHook GetActiveSuiteSetUp();
            SuiteHook GetActiveSuiteTearDown();
            int RunConsolidatedSuite(const char *suiteName, SuiteRunner runner, SuiteHook setUpHook = nullptr, SuiteHook tearDownHook = nullptr);
        }
    }
}