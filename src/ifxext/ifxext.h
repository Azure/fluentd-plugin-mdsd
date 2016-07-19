#ifndef __IFXEXT__H__
#define __IFXEXT__H__

#include <string>

uint32_t MdmStartup();
void MdmCleanup();

class Mdm0D {
public:
    Mdm0D(const std::string & monitoringAccount,
          const std::string & metricNamespace,
          const std::string & metricName,
          bool addDefaultDim = false
          );

    long LogValue(int64_t rawData);

    // timestampUtc: number of ticks (100ns) for UTC time
    // since base time is 1601/01/01 00:00:00.
    long LogValueAtTime(uint64_t timestampUtc,
                        int64_t rawData);

private:
    class MeasureMetric0D* m_0d;
};

class Mdm1D {
public:
    Mdm1D(const std::string & monitoringAccount,
          const std::string & metricNamespace,
          const std::string & metricName,
          const std::string & dimName,
          bool addDefaultDim = false
          );

    long LogValue(int64_t rawData,
                  const std::string& dimValue);

    long LogValueAtTime(uint64_t timestampUtc,
                        int64_t rawData,
                        const std::string& dimValue);

private:
    class MeasureMetric1D* m_1d;
};

class Mdm2D {
public:
    Mdm2D(const std::string & monitoringAccount,
          const std::string & metricNamespace,
          const std::string & metricName,
          const std::string & dimName1,
          const std::string & dimName2,
          bool addDefaultDim = false
          );

    long LogValue(int64_t rawData,
                  const std::string& dimValue1,
                  const std::string& dimValue2);

    long LogValueAtTime(uint64_t timestampUtc,
                        int64_t rawData,
                        const std::string& dimValue1,
                        const std::string& dimValue2);

private:
    class MeasureMetric2D* m_2d;
};


#endif // __IFXEXT__H__
