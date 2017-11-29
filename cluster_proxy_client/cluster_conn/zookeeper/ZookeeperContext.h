#ifndef BERT_ZOOKEEPERCONTEXT_H
#define BERT_ZOOKEEPERCONTEXT_H

#include <queue>
#include <string>
#include "future/Future.h"
#include "net/Typedefs.h"
#include "ZkResponse.h"

struct oarchive;
struct iarchive;

namespace ananas
{
class Connection;
}

namespace qedis
{

class ZookeeperContext final
{
public:
    explicit
    ZookeeperContext(ananas::Connection* c);
    ~ZookeeperContext();

    bool ParseMessage(const char*& data, ananas::PacketLen_t len);

    bool IsResumed() const { return resumed_; }

    // requests
    ananas::Future<ZkResponse> DoHandshake();
    bool ProcessHandshake(const ZkResponse& rsp);

    // Ping
    ananas::Future<ZkResponse> Ping();
    void ProcessPing(long lastPing, ZkResponse now); // Unit: millseconds

    // Create node
    ananas::Future<ZkResponse> CreateNode(bool empher, bool seq,
                                          const std::string* data = nullptr,
                                          const std::string* pathPrefix = nullptr);
    void ProcessCreateNode(const ZkResponse& rsp);

    // Get children2
    ananas::Future<ZkResponse> GetChildren2(const std::string& parent, bool watch = false);
    void ProcessGetChildren2(const ZkResponse& rsp);

    // Get data 
    ananas::Future<ZkResponse> GetData(const std::string& node, bool watch = false);
    void ProcessGetData(const ZkResponse& rsp);

private:

    void _SendPacket(struct oarchive* oa);

    ananas::Connection* const conn_;

    int _GetXid() const;
    mutable int xid_;
    
    int lastZxid_;

    struct Request {
        ananas::Promise<ZkResponse> promise;
        int type;
        int xid;
        std::string path;
        
        Request() : type(-1), xid(-1) { }

        Request(const Request& ) = delete;
        void operator= (const Request & ) = delete;

        Request(Request&& r) {
            _Move(std::move(r));
        }

        Request& operator= (Request&& r) {
            return _Move(std::move(r));
        }
    private:
        Request& _Move(Request&& r) {
            if (&r != this) {
                promise = std::move(r.promise);
                type = r.type;
                xid = r.xid;
                path = std::move(r.path);
                r.type = r.xid = -1;
            }
            return *this;
        }
    };

    ananas::Future<ZkResponse> _PendingRequest(int type, int xid, const std::string* path = nullptr);

    std::queue<Request> pendingReq_;

    enum class State
    {
        kNone,
        kHandshaking,
        kConnected,
    } state_;

    bool _ProcessResponse(const ReplyHeader& hdr, iarchive* ia);

#pragma pack(1)
    struct SessionInfo
    {
        int64_t sessionId{0};
        char passwd[16];
        int64_t lastZxidSeen {0}; // zk_cli doesn't persist this field
    } sessionInfo_;
#pragma pack()
    std::string sessionFile_;
    bool resumed_ {false};
};

} // end namespace qedis

#endif //endif BERT_ZOOKEEPERCONN_H

