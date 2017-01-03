#pragma once
#ifndef __ENDPOINT_DATAREADER_H__
#define __ENDPOINT_DATAREADER_H__

#include <memory>
#include <atomic>
#include <string>
#include "LogItemPtr.h"

namespace EndpointLog {

template<typename T> class ConcurrentMap;
class SocketClient;

/// This class implements a socket data reader. The data to be read
/// are expected to be a series of either '<tagstr>\n' or '<tagstr>:<status-id>\n'.
///
/// If a shared cache is given, the item whose key equals to <tagstr> will be removed
/// from dataCache.
///
/// Once started, DataReader will run in an infinite loop until told to stop.
///
class DataReader {
public:
    /// Constructor
    /// <param name="sockClient">socket client</param>
    /// <param name="dataCache">shared cache for backup data. Can be NULL.</param>
    DataReader(const std::shared_ptr<SocketClient> & sockClient,
        const std::shared_ptr<ConcurrentMap<LogItemPtr>> & dataCache);

    ~DataReader();

    // DataReader is not copyable but movable
    DataReader(const DataReader& other) = delete;
    DataReader& operator=(const DataReader& other) = delete;

    DataReader(DataReader&& other) = default;
    DataReader& operator=(DataReader&& other) = default;

    /// Run the reading process in an infinite loop until told to stop.
    void Run();

    /// Notify the reader to stop.
    void Stop();

    /// Get total number of tags read. For testability.
    size_t GetNumTagsRead() const { return m_nTagsRead; }

private:
    /// Try to read data from the socket. Process the data if any.
    /// <param name="partialData">To save partial data that are not processed yet</param>
    /// Return true if valid data (including 0-byte) are read, return false otherwise.
    bool DoRead(std::string& partialData);

    /// Define interruption point in the loop.
    void InterruptPoint() const;

    /// Process data read from the socket.
    /// <param name='str'> data string to be processed </param>
    /// Return the partial unprocessed string.
    std::string ProcessData(const std::string & str);

    /// Process each item of the read data. An item is a substring delimited by '\n'
    /// in the read data. The item doesn't contain the '\n'.
    /// Each item is in either of the following formats
    /// - <tagstr>
    /// - <tagstr>:<otherstr>
    void ProcessItem(const std::string& item);

    void ProcessTag(const std::string & tag);
    void ProcessTag(const std::string & tag, const std::string & ackStatus);

private:
    std::shared_ptr<SocketClient> m_socketClient;
    std::shared_ptr<ConcurrentMap<LogItemPtr>> m_dataCache;

    std::atomic<bool> m_stopRead{false};    /// flag to stop further reading.

    std::atomic<size_t> m_nTagsRead{0}; /// number of tags read. for testability.
};

} // namespace

#endif // __ENDPOINT_DATAREADER_H__
