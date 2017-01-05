#pragma once
#ifndef __TESTUTIL_H__
#define __TESTUTIL_H__

#include <string>
#include <memory>
#include <atomic>
#include <future>

namespace TestUtil
{
    inline std::string EndOfTest() { return "ENDOFTEST"; }

    inline std::string CreateMsg(int index)
    {
        return std::string("TestMsg-") + std::to_string(index);
    }

    std::string GetCurrDir();

    bool IsFileExists(const std::string & filepath);
    bool RemoveFileIfExists(const std::string & filepath);

    std::string GetTimeNow();
    std::string GetErrnoStr(int errnum);

    // wait for a task for a max amount of time. then check its status
    // Return true if finished within timeoutMS, false if otherwise.
    bool WaitForTask(std::future<void>& task, uint32_t timeoutMS);
}

#endif // __TESTUTIL_H__
