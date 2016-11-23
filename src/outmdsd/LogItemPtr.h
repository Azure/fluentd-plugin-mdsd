#pragma once

#ifndef __ENDPOINT_LOGITEMPTR_H__
#define __ENDPOINT_LOGITEMPTR_H__

#include <memory>

namespace EndpointLog {

class LogItem;
using LogItemPtr = std::shared_ptr<LogItem>;

}

#endif // __ENDPOINT_LOGITEMPTR_H__
