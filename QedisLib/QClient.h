#ifndef BERT_QCLIENT_H
#define BERT_QCLIENT_H

#include "StreamSocket.h"
#include "QCommon.h"
#include "QString.h"

#include "QSlowLog.h"
#include "QReplication.h"
#include <unordered_set>

enum  class ParseCmdState : std::int8_t
{
    Init,
    MultiBulk,
    Arglen,
    Arg,
    Ready,
};

enum ClientFlag
{
    ClientFlag_multi = 0x1,
    ClientFlag_dirty = 0x1 << 1,
    ClientFlag_wrongExec = 0x1 << 2,
    ClientFlag_master = 0x1 << 3,
};

class DB;
struct QSlaveInfo;

class QClient: public StreamSocket
{
private:
    BODY_LENGTH_T _HandlePacket(AttachedBuffer& buf);

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

    const std::unordered_set<QString>&  GetChannels() const { return m_channels; }
    const std::unordered_set<QString>&  GetPatternChannels() const { return m_patternChannels; }
    std::size_t     ChannelCount() const { return m_channels.size(); }
    std::size_t     PatternChannelCount() const { return m_patternChannels.size(); }
    
    bool  WaitFor(const QString& key, const QString* target = nullptr);
    
    const std::unordered_set<QString>  WaitingKeys() const { return m_waitingKeys; }
    void  ClearWaitingKeys()    {  m_waitingKeys.clear(), m_target.clear(); }
    const QString&  GetTarget() const { return m_target; }
    
    void    SetName(const QString& name) { m_name = name; }
    const   QString&    GetName() const { return m_name; }
    
    void         SetSlaveInfo();
    QSlaveInfo*  GetSlaveInfo() const { return m_slaveInfo.get(); }
    
    static void  AddCurrentToMonitor();
    static void  FeedMonitors(const std::vector<QString>& params);

private:
    BODY_LENGTH_T    _ProcessInlineCmd(const char* , size_t);
    void    _Reset();

    ParseCmdState  m_state;
    int   m_multibulk;
    int   m_paramLen;
    
    std::vector<QString> m_params;
    UnboundedBuffer  m_reply;

    int   m_db;

    std::unordered_set<QString>  m_channels;
    std::unordered_set<QString>  m_patternChannels;
    
    unsigned m_flag;
    std::unordered_set<QString>  m_watchKeys;
    std::vector<std::vector<QString> > m_queueCmds;
    
    // blocked list
    std::unordered_set<QString> m_waitingKeys;
    QString m_target;
    
    // slave info from master view
    std::unique_ptr<QSlaveInfo>  m_slaveInfo;
    
    // name
    std::string m_name;
    
    static  QClient*  s_pCurrentClient;
    static  std::set<std::weak_ptr<QClient>, std::owner_less<std::weak_ptr<QClient> > > s_monitors;
};

#endif

