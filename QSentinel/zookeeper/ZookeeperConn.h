#ifndef BERT_ZOOKEEPERCONN_H
#define BERT_ZOOKEEPERCONN_H

#include <list>
#include <map>
#include <unordered_map>
#include <set>

#include "StreamSocket.h"
#include "../QClusterInterface.h"

struct prime_struct;
struct RequestHeader;
struct ReplyHeader;
struct iarchive;
struct oarchive;

class Timer;

namespace qedis
{

class ZookeeperConn : public QClusterConn
{
public:
    ZookeeperConn(const std::shared_ptr<StreamSocket>& c, int setId, const std::string& addr);
    ~ZookeeperConn();

    bool ParseMessage(const char*& data, size_t len) override;
    void OnConnect() override;
    void OnDisconnect() override;
    void UpdateShardData() override;

private:
    void _RunForMaster(int setid, const std::string& val);
    bool _ProcessHandshake(const prime_struct& rsp);
    bool _ProcessResponse(const ReplyHeader& header, iarchive* ia);
    bool _ProcessWatchEvent(const ReplyHeader& header, iarchive* ia);
    bool _GetSiblings(const std::string& parent, std::function<bool (void*)> cb);
    bool _GetData(const std::string& node, bool watch, std::function<bool (void*)> cb);
    bool _SetData(const std::string& node, const std::string& data, int ver, std::function<bool (void*)> cb);
    bool _Exists(const std::string& sibling, bool watch, std::function<bool (void*)> cb);

    void _InitPingTimer();
    bool _IsMaster() const;

    bool _SendPacket(const RequestHeader& h,
                     struct oarchive* oa,
                     std::function<bool (void*)> cb = std::function<bool (void* )>(),
                     const std::string* = nullptr);

    void _OnNodeDelete(const std::string& node);
    bool _OnNodeCreate(void* );
    bool _OnGetMySiblings(void* );
    bool _OnMySetData(void* );
    bool _OnWatchBrother(void* );
    bool _ProcessData(const std::string& data);

    std::vector<SocketAddr> _CollectSlaveAddr() const;

    int _GetXid() const;
    mutable int xid_;

    const int setId_;
    const std::string addr_;

    enum class State
    {
        kNone,
        kHandshaking,
        kConnected,
    } state_;

    struct Request
    {
        int type;
        int xid;
        std::string path;
        std::function<bool (void* )> cb;
    };
    std::list<Request> pendingRequests_;

    timeval lastPing_;

    // my node & seq
    std::string node_;
    int seq_;
    // nodes in my set
    std::map<int, std::string> siblings_;

    // master info
    // my set shards
    std::set<int> shards_;
    std::set<int> migratingShards_;
    int version_;

#pragma pack(1)
    struct SessionInfo
    {
        int64_t sessionId{0};
        char passwd[16];
        // ? zk_cli doesn't persist this field
        int64_t lastSeenZxid {0};
    } sessionInfo_;
#pragma pack()

    std::string sessionFile_;
    Timer* pingTimer_;
    std::weak_ptr<StreamSocket> sock_;
};

} // end namespace qedis

#endif //endif BERT_ZOOKEEPERCONN_H

