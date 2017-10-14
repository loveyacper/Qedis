#ifndef BERT_ZOOKEEPERCONN_H
#define BERT_ZOOKEEPERCONN_H

#include <list>
#include <map>
#include "../QClusterInterface.h"

struct prime_struct;
struct ReplyHeader;
struct iarchive;

namespace qedis
{

class ZookeeperConn : public QClusterConn
{
public:
    ZookeeperConn(const std::shared_ptr<StreamSocket>& c, int setId, const std::string& addr);

    bool ParseMessage(const char*& data, size_t len) override;
    void OnConnect() override;
    void RunForMaster(int setid, const std::string& val) override;

private:
    bool _ProcessHandshake(const prime_struct& rsp);
    bool _ProcessResponse(const ReplyHeader& header, iarchive* ia);
    bool _GetSiblings(const std::string& parent);
    bool _ExistsAndWatch(const std::string& sibling);

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
    } sessionInfo_;
#pragma pack()

    // ? zk_cli doesn't persist this field
    int64_t lastSeenZxid_ {0};

    static const std::string kSessionFile;
};

} // end namespace qedis

#endif //endif BERT_ZOOKEEPERCONN_H

