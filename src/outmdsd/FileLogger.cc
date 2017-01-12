#include <stdexcept>
#include <system_error>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include "FileLogger.h"

namespace EndpointLog {


FileLogger::FileLogger(
    const std::string& filepath,
    bool createIfNotExists
    ) :
    m_filepath(filepath),
    m_fd(-1)
{
    if (filepath.empty()) {
        throw std::invalid_argument("FileLogger: invalid empty filepath parameter");
    }

    int rtn = 0;
    if (createIfNotExists) {
        rtn = open(filepath.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    }
    else {
        rtn = open(filepath.c_str(), O_WRONLY | O_APPEND);
    }

    if (-1 == rtn) {
        throw std::system_error(errno, std::system_category(), "open " + filepath + " failed");
    }
    m_fd = rtn;
}

void
FileLogger::WriteLog(
    const std::string& msg
    )
{
    int rtn = 0;
    while(-1 == (rtn = write(m_fd, msg.c_str(), msg.size())) && EINTR == errno) {}
    if (-1 == rtn) {
        throw std::system_error(errno, std::system_category(),
                                "write() " + m_filepath + " failed");
    }
}


} // namespace
