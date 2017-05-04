#pragma once
#ifndef __TESTUTIL_H__
#define __TESTUTIL_H__

#include <string>
#include <memory>
#include <atomic>
#include <future>
#include <unordered_set>
#include <ios>

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
    template <typename T>
    bool WaitForTask(std::future<T>& task, uint32_t timeoutMS)
    {
        auto status = task.wait_for(std::chrono::milliseconds(timeoutMS));
        return (std::future_status::ready == status);
    }

    // Get the size of a given file
    // Throw exception if filename is empty string, or failed to open file.
    size_t GetFileSize(const std::string & filename);

    // Search a set of strings from a text file starting from 'startPos'
    // Throw exception if filename is empty string, or if searchSet if empty,
    // or if given file cannot be opened.
    // Return true if all strings are found, return false if any of them not found.
    // The items found are removed from searchSet.
    bool SearchStrings(const std::string& filename,
        std::streampos startPos,
        std::unordered_set<std::string>& searchSet);
}

#endif // __TESTUTIL_H__
