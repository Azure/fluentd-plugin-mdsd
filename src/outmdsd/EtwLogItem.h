#pragma once
#ifndef __ENDPOINT_ETWLOGITEM_H__
#define __ENDPOINT_ETWLOGITEM_H__

#include <string>

#include "DjsonLogItem.h"

namespace EndpointLog {

// This class contains ETW data sent to mdsd DJSON socket (e.g. from MDM MetricsExtension)
class EtwLogItem : public DjsonLogItem
{
public:
    EtwLogItem(
        std::string source,
        std::string guid,
        int32_t eventId) : DjsonLogItem(std::move(source))
    {
        AddData("GUID", std::move(guid));
        AddData("EventId", eventId);
    }
};

} // namespace

#endif // __ENDPOINT_ETWLOGITEM_H__
