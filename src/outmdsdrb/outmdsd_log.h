#pragma once
#ifndef __OUTMDSD_LOG_H__
#define __OUTMDSD_LOG_H__

#include <string>

void InitLogger(const std::string& logFilePath, bool createIfNotExist);

void SetLogLevel(const std::string & level);


#endif // __OUTMDSD_LOG_H__
