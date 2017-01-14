#pragma once

#ifndef __ENDPOINT_DJSONLOGITEM_H__
#define __ENDPOINT_DJSONLOGITEM_H__

#include <string>
#include <sstream>
#include "LogItem.h"

namespace EndpointLog {

class IdMgr;

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
private:
    struct ItemInfo
    {
        std::string name;
        std::string type;
        std::string value;

        ItemInfo(std::string n, std::string t, std::string v) :
            name(std::move(n)),
            type(std::move(t)),
            value(std::move(v))
        {}
    };

    struct CompItemInfo
    {
        bool operator()(const ItemInfo & x, const ItemInfo & y) { return x.name < y.name; }
    };

public:
    DjsonLogItem(std::string source)
        :LogItem(),
        m_source(std::move(source))
    {
    }

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

    void AddData(std::string name, bool value)
    {
        m_svlist.emplace_back(std::move(name), "FT_BOOL", value? "true" : "false");
    }

    void AddData(std::string name, int32_t value)
    {
        m_svlist.emplace_back(std::move(name), "FT_INT32", std::to_string(value));
    }

    void AddData(std::string name, uint32_t value)
    {
        m_svlist.emplace_back(std::move(name), "FT_INT64", std::to_string(value));
    }

    void AddData(std::string name, int64_t value)
    {
        m_svlist.emplace_back(std::move(name), "FT_INT64", std::to_string(value));
    }

    void AddData(std::string name, double value)
    {
        // because std::to_string(double) only prints 6 deciman digits, don't use it
        std::ostringstream strm;
        strm << value;
        m_svlist.emplace_back(std::move(name), "FT_DOUBLE", strm.str());
    }

    void AddData(std::string name, uint64_t seconds, uint32_t nanoseconds)
    {
        std::ostringstream strm;
        strm << "[" << seconds << "," << nanoseconds << "]";
        m_svlist.emplace_back(name, "FT_TIME", strm.str());
    }

    void AddData(std::string name, const char* value)
    {
        if (!value) {
            throw std::invalid_argument("DjsonLogItem::AddData: unexpected NULl for const char* parameter.");
        }
        AddData(name, std::string(value));
    }

    void AddData(std::string name, std::string value)
    {
        std::string tmpstr;
        tmpstr.reserve(2 + value.size());
        tmpstr.append(1, '"').append(value).append(1, '"');
        m_svlist.emplace_back(std::move(name), "FT_STRING", std::move(tmpstr));
    }

private:
    static IdMgr& GetIdMgr();

    void ComposeSchemaAndData() const;

    void ComposeSchema(std::ostringstream& strm) const;
    std::string GetSchemaCacheKey() const;
    std::string ComposeSchemaArray() const;
    void ComposeDataValue(std::ostringstream& strm) const;

    void ComposeFullData() const;

private:
    std::string m_source;
    mutable std::string m_schemaAndData;
    mutable std::vector<ItemInfo> m_svlist; // contain schema and value info
    mutable std::string m_djsonData;
};

} // namespace

#endif // __ENDPOINT_DJSONLOGITEM_H__
