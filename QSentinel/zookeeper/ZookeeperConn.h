#ifndef BERT_ZOOKEEPERCONN_H
#define BERT_ZOOKEEPERCONN_H

#include <list>
#include <map>
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

private:
    void _RunForMaster(int setid, const std::string& val);
    bool _ProcessHandshake(const prime_struct& rsp);
    bool _ProcessResponse(const ReplyHeader& header, iarchive* ia);
    bool _ProcessWatchEvent(const ReplyHeader& header, iarchive* ia);
    bool _GetSiblings(const std::string& parent);
    bool _ExistsAndWatch(const std::string& sibling);
    void _InitPingTimer();
    bool _IsMaster() const;
    bool _SendPacket(const RequestHeader& h, struct oarchive* oa, const std::string* = nullptr);
    void _OnNodeDelete(const std::string& node);

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
    };
    std::list<Request> pendingRequests_;

    timeval lastPing_;

    // my node & seq
    std::string node_;
    int seq_;
    // nodes in my set
    std::map<int, std::string> siblings_;

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

