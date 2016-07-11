#include "ifxext.h"
#include <IfxMetrics.h>
#include <Windows.h>

// debug purpose. todo: remove this later.
static const char* modTag = "IFXRuby";

Mdm0D::Mdm0D(
    const std::string & monitoringAccount,
    const std::string & metricNamespace,
    const std::string & metricName,
    bool addDefaultDim
)
{
    m_0d = MeasureMetric0D::Create(monitoringAccount.c_str(),
                                   metricNamespace.c_str(),
                                   metricName.c_str(),
                                   addDefaultDim);
}

long
Mdm0D::LogValue(
    int64_t rawData
    )
{
    auto r = m_0d->LogValue(static_cast<LONG64>(rawData));
    return static_cast<long>(r);
}

long
Mdm0D::LogValueAtTime(
    uint64_t timestampUtc,
    int64_t rawData
    )
{
    auto r = m_0d->LogValue(static_cast<ULONG64>(timestampUtc), static_cast<LONG64>(rawData));
    return static_cast<long>(r);
}

Mdm1D::Mdm1D(
    const std::string & monitoringAccount,
    const std::string & metricNamespace,
    const std::string & metricName,
    const std::string & dimName,
    bool addDefaultDim
)
{
    TraceDebug((1, "%s: Mdm1D: Account='%s' Namespace='%s' MetricName='%s' DimName='%s'",
                modTag, monitoringAccount.c_str(),  metricNamespace.c_str(),
                metricName.c_str(), dimName.c_str()));

    m_1d = MeasureMetric1D::Create(monitoringAccount.c_str(),
                                   metricNamespace.c_str(),
                                   metricName.c_str(),
                                   dimName.c_str(),
                                   addDefaultDim);
}

long
Mdm1D::LogValue(
    int64_t rawData,
    const std::string& dimValue
    )
{
    TraceDebug((1, "%s: Mdm1D: LogValue: rawData='%lld' dimValue='%s'",
                modTag, (long long)rawData, dimValue.c_str()));
    auto r = m_1d->LogValue(static_cast<LONG64>(rawData), dimValue.c_str());
    return static_cast<long>(r);
}

long
Mdm1D::LogValueAtTime(
   uint64_t timestampUtc,
   int64_t rawData,
   const std::string& dimValue
   )
{
    auto r = m_1d->LogValue(static_cast<ULONG64>(timestampUtc), static_cast<LONG64>(rawData), dimValue.c_str());
    return static_cast<long>(r);
}

Mdm2D::Mdm2D(
    const std::string & monitoringAccount,
    const std::string & metricNamespace,
    const std::string & metricName,
    const std::string & dimName1,
    const std::string & dimName2,
    bool addDefaultDim
    )
{
    TraceDebug((1, "%s: Mdm2D: Account='%s' Namespace='%s' MetricName='%s' DimName1='%s' DimName2='%s'",
                modTag, monitoringAccount.c_str(),  metricNamespace.c_str(),
                metricName.c_str(), dimName1.c_str(), dimName2.c_str()));

    m_2d = MeasureMetric2D::Create(monitoringAccount.c_str(),
                                   metricNamespace.c_str(),
                                   metricName.c_str(),
                                   dimName1.c_str(),
                                   dimName2.c_str(),
                                   addDefaultDim);
}

long
Mdm2D::LogValue(
    int64_t rawData,
    const std::string& dimValue1,
    const std::string& dimValue2
    )
{
    TraceDebug((1, "%s: Mdm2D: LogValue: rawData='%lld' dimValue1='%s' dimValue2='%s'",
                modTag, (long long)rawData, dimValue1.c_str(), dimValue2.c_str()));
    auto r = m_2d->LogValue(static_cast<LONG64>(rawData), dimValue1.c_str(), dimValue2.c_str());
    return static_cast<long>(r);
}

long
Mdm2D::LogValueAtTime(
    uint64_t timestampUtc,
    int64_t rawData,
    const std::string& dimValue1,
    const std::string& dimValue2
    )
{
    TraceDebug((1, "%s: Mdm2D: LogValue: TimeStamp='%llu', rawData='%lld' dimValue1='%s' dimValue2='%s'",
            modTag, (unsigned long long)timestampUtc, (long long)rawData, dimValue1.c_str(), dimValue2.c_str()));

    auto r = m_2d->LogValue(static_cast<ULONG64>(timestampUtc), static_cast<LONG64>(rawData),
                            dimValue1.c_str(), dimValue2.c_str());
    return static_cast<long>(r);
}
