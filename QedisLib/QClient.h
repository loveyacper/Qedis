#ifndef BERT_QCLIENT_H
#define BERT_QCLIENT_H

#include "StreamSocket.h"
#include "QCommon.h"
#include "QString.h"

#include "QStat.h"
#include <set>

enum  ParseCmdState
{
    InitState,
    ProcessMultiBulkState,
    ProcessDollarState,
    ProcessArglenState,
    ProcessArgState,
    ReadyState,
};

class DB;

class QClient: public StreamSocket
{
private:
    HEAD_LENGTH_T _HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen);
    void _HandlePacket(AttachedBuffer& buf);

public:
    QClient();
    bool SelectDB(int db);
    static QClient*  Current() {  return s_pCurrentClient; }

    // pubsub
    std::size_t Subscribe(const std::string& channel)
    {
        return  m_channels.insert(channel).second ? 1 : 0;
    }

    std::size_t UnSubscribe(const std::string& channel)
    {
        return m_channels.erase(channel);
    }

    std::size_t PSubscribe(const std::string& channel)
    {
        return  m_patternChannels.insert(channel).second ? 1 : 0;
    }

    std::size_t PUnSubscribe(const std::string& channel)
    {
        return m_patternChannels.erase(channel);
    }

    const std::set<std::string>&    GetChannels() const { return m_channels; }
    const std::set<std::string>&    GetPatternChannels() const { return m_patternChannels; }
    std::size_t     ChannelCount() const { return m_channels.size(); }
    std::size_t     PatternChannelCount() const { return m_patternChannels.size(); }

private:
    void        _Reset();

    ParseCmdState  m_state;
    int   m_multibulk;
    int   m_paramLen;
    
    std::vector<QString> m_params;
    UnboundedBuffer  m_reply;

    int   m_db;

    std::set<std::string>  m_channels;
    std::set<std::string>  m_patternChannels;
    
    QStat  m_stat;
    static  QClient*  s_pCurrentClient;
};

#endif

