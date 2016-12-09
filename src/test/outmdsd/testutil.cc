#include <stdexcept>
#include <system_error>
#include <iomanip>
#include <sstream>

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