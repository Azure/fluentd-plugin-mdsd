#pragma once

#ifndef __ENDPOINT_DJSONLOGITEM_H__
#define __ENDPOINT_DJSONLOGITEM_H__

#include <string>
#include <sstream>
#include "LogItem.h"

namespace EndpointLog {

// Each DjsonLogItem contains a DJSON-formatted item.
// The item includes
//   - message length in bytes
//   - new line char
//   - a string containing the JSON payload
// The JSON payload includes 5 elements:
//   - source, a string
//   - message id, uint64_t.
//   - schema id, uint64_t.
//   - schema array.
//   - data array.
//
// Example item:
// 110
// ["syslog",53,3,[["timestamp","FT_TIME"],["message","FT_STRING"]],[[1475129808,541868180],"This is a message"]]
//
class DjsonLogItem : public LogItem
{
public:
    /// Construct a new object.
    /// source: source of the DJSON item
    /// schemaAndData: include schema id, schema array, data array.
    DjsonLogItem(std::string source, std::string schemaAndData) 
        : LogItem(),
        m_source(std::move(source)),
        m_schemaAndData(std::move(schemaAndData))
    {
    }

    ~DjsonLogItem() {}

    DjsonLogItem(const DjsonLogItem & other) = default;
    DjsonLogItem(DjsonLogItem&& other) = default;

    DjsonLogItem& operator=(const DjsonLogItem & other) = default;
    DjsonLogItem& operator=(DjsonLogItem&& other) = default;

    // Return full DJSON-formatted string
    const char* GetData() const override;

private:
    void ComposeFullData() const;

private:
    std::string m_source;
    std::string m_schemaAndData;
    mutable std::string m_djsonData;
};

} // namespace

#endif // __ENDPOINT_DJSONLOGITEM_H__
