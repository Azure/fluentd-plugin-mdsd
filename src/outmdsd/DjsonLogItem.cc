#include "DjsonLogItem.h"

using namespace EndpointLog;

const char*
DjsonLogItem::GetData() const
{
    if (m_djsonData.empty()) {
        ComposeFullData();
    }
    return m_djsonData.c_str();
}

void
DjsonLogItem::ComposeFullData() const
{
    auto tag = GetTag();
    size_t len = 2 + m_source.size() + 2 + tag.size() + 1 + m_schemaAndData.size() + 1;
    auto lenstr = std::to_string(len);

    m_djsonData.reserve(len + lenstr.size() + 1);
    m_djsonData = lenstr;
    m_djsonData.append("\n[\"").append(m_source).append("\",").append(tag).append(",").append(m_schemaAndData).append("]");
}
