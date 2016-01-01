#ifndef BERT_QCLIENT_H
#define BERT_QCLIENT_H

#include "StreamSocket.h"
#include "QCommon.h"
#include "QString.h"

#include "QSlowLog.h"
#include "QReplication.h"
#include <unordered_set>

namespace qedis
{

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
    
    void OnConnect() override;
    
    bool SelectDB(int db);
    static QClient*  Current();
    
    //multi
    void SetFlag(unsigned flag) { flag_ |= flag; }
    void ClearFlag(unsigned flag) { flag_ &= ~flag; }
    bool IsFlagOn(unsigned flag) { return flag_ & flag; }
    void FlagExecWrong() { if (IsFlagOn(ClientFlag_multi)) SetFlag(ClientFlag_wrongExec);   }
    
    bool Watch(const QString& key);
    void UnWatch();
    bool NotifyDirty(const QString& key);
    bool Exec();
    void ClearMulti();

    // pubsub
    std::size_t Subscribe(const QString& channel)
    {
        return  channels_.insert(channel).second ? 1 : 0;
    }

    std::size_t UnSubscribe(const QString& channel)
    {
        return channels_.erase(channel);
    }

    std::size_t PSubscribe(const QString& channel)
    {
        return  patternChannels_.insert(channel).second ? 1 : 0;
    }

    std::size_t PUnSubscribe(const QString& channel)
    {
        return patternChannels_.erase(channel);
    }

    const std::unordered_set<QString>&  GetChannels() const { return channels_; }
    const std::unordered_set<QString>&  GetPatternChannels() const { return patternChannels_; }
    std::size_t     ChannelCount() const { return channels_.size(); }
    std::size_t     PatternChannelCount() const { return patternChannels_.size(); }
    
    bool  WaitFor(const QString& key, const QString* target = nullptr);
    
    const std::unordered_set<QString>  WaitingKeys() const { return waitingKeys_; }
    void  ClearWaitingKeys()    {  waitingKeys_.clear(), target_.clear(); }
    const QString&  GetTarget() const { return target_; }
    
    void    SetName(const QString& name) { name_ = name; }
    const   QString&    GetName() const { return name_; }
    
    void         SetSlaveInfo();
    QSlaveInfo*  GetSlaveInfo() const { return slaveInfo_.get(); }
    
    static void  AddCurrentToMonitor();
    static void  FeedMonitors(const std::vector<QString>& params);
    
    void    SetAuth() { auth_ = true; }
    
    void    RewriteCmd(std::vector<QString>& params) { params_.swap(params); }

private:
    BODY_LENGTH_T    _ProcessInlineCmd(const char* , size_t);
    void    _Reset();

    ParseCmdState  state_;
    int   multibulk_;
    int   paramLen_;
    
    std::vector<QString> params_;
    UnboundedBuffer  reply_;

    int   db_;

    std::unordered_set<QString>  channels_;
    std::unordered_set<QString>  patternChannels_;
    
    unsigned flag_;
    std::unordered_set<QString>  watchKeys_;
    std::vector<std::vector<QString> > queueCmds_;
    
    // blocked list
    std::unordered_set<QString> waitingKeys_;
    QString target_;
    
    // slave info from master view
    std::unique_ptr<QSlaveInfo>  slaveInfo_;
    
    // name
    std::string name_;
    
    // auth
    bool  auth_;
    
    static  QClient*  s_pCurrentClient;
    static  std::set<std::weak_ptr<QClient>, std::owner_less<std::weak_ptr<QClient> > > s_monitors;
};
    
}

#endif

