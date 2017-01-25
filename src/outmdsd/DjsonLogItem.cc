#include <algorithm>

#include "DjsonLogItem.h"
#include "IdMgr.h"

using namespace EndpointLog;

const char*
DjsonLogItem::GetData()
{
    if (m_djsonData.empty()) {
        if (m_schemaAndData.empty()) {
            ComposeSchemaAndData();
        }
        ComposeFullData();
    }

    return m_djsonData.c_str();
}

IdMgr&
DjsonLogItem::GetIdMgr()
{
    static IdMgr m;
    return m;
}

void
DjsonLogItem::ComposeSchemaAndData()
{
    std::ostringstream strm;

    ComposeSchema(strm);
    ComposeDataValue(strm);

    // free m_svlist capacity
    std::vector<ItemInfo> tmpv;
    tmpv.swap(m_svlist);

    m_schemaAndData = strm.str();
}

void
DjsonLogItem::ComposeSchema(
    std::ostringstream& strm
    )
{
    IdMgr::value_type_t cachedInfo;

    // The fields order are preserved. So schema with same names/types
    // but in different order will be treated different schemas.
    auto key = GetSchemaCacheKey();
    if (GetIdMgr().GetItem(key, cachedInfo)) {
        strm << cachedInfo.first << "," << cachedInfo.second;
    }
    else {
        auto schemaArray = ComposeSchemaArray();
        auto schemaId = GetIdMgr().FindOrInsert(key, schemaArray);
        strm << schemaId << "," << schemaArray;
    }
}

std::string
DjsonLogItem::GetSchemaCacheKey() const
{
    std::string tmpstr;
    for(const auto & item : m_svlist) {
        tmpstr.append(item.name).append(item.type);
    }
    return tmpstr;
}

std::string
DjsonLogItem::ComposeSchemaArray() const
{
    std::ostringstream strm;
    strm << "[";
    for (size_t i = 0; i < m_svlist.size();  i++) {
        strm << "[\"" << m_svlist[i].name << "\",\"" << m_svlist[i].type << "\"]";
        if (i != (m_svlist.size()-1)) {
            strm << ",";
        }
    }
    strm << "],";
    return strm.str();
}

void
DjsonLogItem::ComposeDataValue(
    std::ostringstream& strm
    ) const
{
    strm << "[";
    for (size_t i = 0; i < m_svlist.size();  i++) {
        strm << m_svlist[i].value;
        if (i != (m_svlist.size()-1)) {
            strm << ",";
        }
    }
    strm << "]";
}

void
DjsonLogItem::ComposeFullData()
{
    auto tag = GetTag();
    size_t len = 2 + m_source.size() + 2 + tag.size() + 1 + m_schemaAndData.size() + 1;
    auto lenstr = std::to_string(len);

    m_djsonData.reserve(len + lenstr.size() + 1);
    m_djsonData = lenstr;
    m_djsonData.append("\n[\"").append(m_source).append("\",").append(tag).append(",").append(m_schemaAndData).append("]");
}
