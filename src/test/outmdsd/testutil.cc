#include <stdexcept>
#include <system_error>
#include <iomanip>
#include <sstream>
#include <fstream>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
}

#include "testutil.h"

std::string
TestUtil::GetCurrDir()
{
    char buf[1024];
    if (!getcwd(buf, sizeof(buf))) {
        throw std::system_error(errno, std::system_category(), "getcwd failed");
    }
    return buf;
}

bool
TestUtil::IsFileExists(
    const std::string & filepath
    )
{
    if (filepath.empty()) {
        throw std::invalid_argument("IsFileExists(): invalid, empty file path is given.");
    }

    struct stat sb;
    auto rtn = stat(filepath.c_str(), &sb);
    return (0 == rtn);
}

bool
TestUtil::RemoveFileIfExists(
    const std::string & filepath
    )
{
    if (!TestUtil::IsFileExists(filepath)) {
        return false;
    }
    if (unlink(filepath.c_str())) {
        throw std::system_error(errno, std::system_category(), "unlink(" + filepath + ")");
    }
    return true;
}

std::string
TestUtil::GetTimeNow()
{
    struct timeval tv;
    (void) gettimeofday(&tv, 0);
    
    struct tm zulu;
    (void) gmtime_r(&(tv.tv_sec), &zulu);

    char buf[128];
    auto rtn = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &zulu);
    if (0 == rtn) {
        throw std::runtime_error("strftime() failed");
    }
    auto usec = static_cast<unsigned long>(tv.tv_usec);
    std::ostringstream strm;
    strm << buf << "." << std::setfill('0') << std::setw(6) << usec << "0Z";
    return strm.str();
}

std::string
TestUtil::GetErrnoStr(int errnum)
{
    char errorstr[256];
    char* errRC = strerror_r(errnum, errorstr, sizeof(errorstr));
    return std::string(errRC);
}

bool
TestUtil::WaitForTask(
    std::future<void>& task,
    uint32_t timeoutMS
    )
{
    auto status = task.wait_for(std::chrono::milliseconds(timeoutMS));
    return (std::future_status::ready == status);
}

size_t
TestUtil::GetFileSize(
    const std::string & filename
    )
{
    if (filename.empty()) {
        throw std::invalid_argument("filename arg cannot be empty string");
    }

    std::ifstream fin(filename);
    if (!fin.is_open()) {
        throw std::runtime_error("cannot open file '" + filename + "'");
    }
    fin.seekg(0, fin.end);
    auto sz = fin.tellg();
    fin.close();
    return sz;
}

bool
TestUtil::SearchStrings(
    const std::string& filename,
    std::streampos startPos,
    std::unordered_set<std::string>& searchSet
    )
{
    if (filename.empty()) {
        throw std::invalid_argument("SearchStrings: filename arg cannot be empty string");
    }
    if (searchSet.empty()) {
        throw std::invalid_argument("SearchStrings: searchSet cannot be empty");
    }

    std::ifstream fin(filename);
    if (!fin.is_open()) {
        throw std::runtime_error("SearchStrings: cannot open file '" + filename + "'");
    }

    fin.seekg(startPos);

    std::string line;
    while(getline(fin, line)) {
        std::string foundStr;
        bool found = false;
        for (const auto & str : searchSet) {
            if (line.find(str) != std::string::npos) {
                foundStr = str;
                found = true;
                break;
            }
        }

        if (found) {
            searchSet.erase(foundStr);
        }
    }
    fin.close();

    return (searchSet.empty() == true);
}
