#include <stdexcept>
#include <system_error>

extern "C" {
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}

#include "FileTracer.h"

namespace EndpointLog {


FileTracer::FileTracer(
    const std::string& filepath,
    bool createIfNotExists
    ) :
    m_filepath(filepath),
    m_fd(-1)
{
    if (filepath.empty()) {
        throw std::invalid_argument("FileTracer: invalid empty filepath parameter");
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
FileTracer::WriteLog(
    const std::string& msg
    )
{
    auto bytesleft = msg.size();
    while(bytesleft) {
        auto rtn = write(m_fd, msg.c_str()+msg.size()-bytesleft, bytesleft);
        if (-1 == rtn) {
            if (EINTR != errno) {
                throw std::system_error(errno, std::system_category(),
                                        "write() " + m_filepath + " failed");
            }
        }
        else {
            bytesleft -= rtn;
        }
    }
}


} // namespace
