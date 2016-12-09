#pragma once
#ifndef __TESTUTIL_H__
#define __TESTUTIL_H__

#include <string>
#include <memory>
#include <atomic>

namespace TestUtil
{
    inline std::string EndOfTest() { return "ENDOFTEST"; }
    std::string GetCurrDir();

    bool IsFileExists(const std::string & filepath);
    bool RemoveFileIfExists(const std::string & filepath);

    std::string GetTimeNow();
    std::string GetErrnoStr(int errnum);
}

#endif // __TESTUTIL_H__
