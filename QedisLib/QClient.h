#ifndef BERT_QCLIENT_H
#define BERT_QCLIENT_H

#include "StreamSocket.h"
#include "QCommon.h"
#include "QString.h"

#include "QStat.h"
#include <set>

enum  class ParseCmdState : std::int8_t
{
    Init,
    MultiBulk,
    Dollar,
    Arglen,
    Arg,
    Ready,
};

enum ClientFlag
{
    ClientFlag_multi = 0x1,
    ClientFlag_dirty = 0x1 << 1,
    ClientFlag_wrongExec = 0x1 << 2,
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
    static QClient*  Current();
    
    //multi
    void SetFlag(unsigned flag) { m_flag |= flag; }
    void ClearFlag(unsigned flag) { m_flag &= ~flag; }
    bool IsFlagOn(unsigned flag) { return m_flag & flag; }
    void FlagExecWrong() { if (IsFlagOn(ClientFlag_multi)) SetFlag(ClientFlag_wrongExec);   }
    
    bool Watch(const QString& key);
    void UnWatch();
    bool NotifyDirty(const QString& key);
    bool Exec();
    void ClearMulti();

    // pubsub
    std::size_t Subscribe(const QString& channel)
    {
        return  m_channels.insert(channel).second ? 1 : 0;
    }

    std::size_t UnSubscribe(const QString& channel)
    {
        return m_channels.erase(channel);
    }

    std::size_t PSubscribe(const QString& channel)
    {
        return  m_patternChannels.insert(channel).second ? 1 : 0;
    }

    std::size_t PUnSubscribe(const QString& channel)
    {
        return m_patternChannels.erase(channel);
    }

    const std::set<QString>&    GetChannels() const { return m_channels; }
    const std::set<QString>&    GetPatternChannels() const { return m_patternChannels; }
    std::size_t     ChannelCount() const { return m_channels.size(); }
    std::size_t     PatternChannelCount() const { return m_patternChannels.size(); }

private:
    void        _ProcessInlineCmd(const char* , size_t , BODY_LENGTH_T* );
    void        _Reset();

    ParseCmdState  m_state;
    int   m_multibulk;
    int   m_paramLen;
    
    std::vector<QString> m_params;
    UnboundedBuffer  m_reply;

    int   m_db;

    std::set<QString>  m_channels;
    std::set<QString>  m_patternChannels;
    
    unsigned m_flag;
    std::set<QString>  m_watchKeys;
    std::vector<std::vector<QString> > m_queueCmds;
    
    QStat  m_stat;
    static  QClient*  s_pCurrentClient;
};

#endif

