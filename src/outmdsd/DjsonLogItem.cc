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

    // because most log items will share same schema, use unsorted key first
    auto unsortedKey = GetSchemaCacheKey();
    if (GetIdMgr().GetItem(unsortedKey, cachedInfo)) {
        strm << cachedInfo.first << "," << cachedInfo.second;
        return;
    }

    auto unsortedSchemaArray = ComposeSchemaArray();

    // sort items because they should have same schema
    CompItemInfo compItemInfo;
    std::sort(m_svlist.begin(), m_svlist.end(), compItemInfo);

    auto sortedKey = GetSchemaCacheKey();

    if (GetIdMgr().GetItem(sortedKey, cachedInfo)) {
        strm << cachedInfo.first << "," << cachedInfo.second;
        GetIdMgr().Insert(unsortedKey, std::make_pair(cachedInfo.first, unsortedSchemaArray));
    }
    else {
        auto sortedSchemaArray = ComposeSchemaArray();
        auto schemaId = GetIdMgr().FindOrInsert(sortedKey, sortedSchemaArray);

        // save to cache for unsorted key too
        GetIdMgr().Insert(unsortedKey, std::make_pair(schemaId, unsortedSchemaArray));
        strm << schemaId << "," << sortedSchemaArray;
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
