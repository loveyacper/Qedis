#include <iostream>
#include <unistd.h>
#include "ZookeeperConn.h"
#include "Log/Logger.h"
#include "QCommon.h"

#include "zookeeper.jute.h"
#include "proto.h"

// from zookeeper.c
static int deserialize_prime_response(struct prime_struct* req, const char* buffer)
{
    int offset = 0; 

    memcpy(&req->len, buffer + offset, sizeof(req->len));
    offset += sizeof(req->len); 
    req->len = ntohl(req->len); 

    memcpy(&req->protocolVersion, buffer + offset, sizeof(req->protocolVersion)); 
    offset += sizeof(req->protocolVersion);
    req->protocolVersion = ntohl(req->protocolVersion); 

    memcpy(&req->timeOut, buffer + offset, sizeof(req->timeOut)); 
    offset += sizeof(req->timeOut);
    req->timeOut = ntohl(req->timeOut);

    memcpy(&req->sessionId, buffer + offset, sizeof(req->sessionId)); 
    offset += sizeof(req->sessionId); 
    req->sessionId = htonll(req->sessionId); 

    memcpy(&req->passwd_len, buffer + offset, sizeof(req->passwd_len)); 
    offset += sizeof(req->passwd_len); 
    req->passwd_len = ntohl(req->passwd_len); 

    memcpy(req->passwd, buffer + offset, sizeof(req->passwd)); 
    return 0;
}
          
const int kTimeout = 15 * 1000; // ms
const int PING_XID = -2;

namespace qedis
{
    
const std::string ZookeeperConn::kSessionFile = "zk.session";

    
ZookeeperConn::ZookeeperConn(const std::shared_ptr<StreamSocket>& c, int setId, const std::string& addr) :
    QClusterConn(c),
    xid_(0),
    setId_(setId),
    addr_(addr),
    state_(State::kNone),
    pingTimer_(nullptr)
{
    lastPing_.tv_sec = 0;
    lastPing_.tv_usec = 0;
    sessionInfo_.passwd[0] = '\0';
}

ZookeeperConn::~ZookeeperConn()
{
}

bool ZookeeperConn::ParseMessage(const char*& data, size_t len)
{
    switch (state_)
    {
    case State::kHandshaking:
        {
            if (len < HANDSHAKE_RSP_SIZE)
                return true;

            struct prime_struct rsp;
            deserialize_prime_response(&rsp, data);
            data += HANDSHAKE_RSP_SIZE;
        
            if (_ProcessHandshake(rsp))
                RunForMaster(setId_, addr_);
            else
                return false;
        }
        break;

    case State::kConnected:
        {
            if (len < 4)
                return true;

            int thisLen = *(int*)data;
            thisLen = ntohl(thisLen);
            std::cout << "thisLen " << thisLen << ", recv Len " << len << std::endl;
            if (sizeof(thisLen) + thisLen > len)
                return true;

            struct ReplyHeader hdr; // xid zxid err
            struct iarchive *ia = create_buffer_iarchive(const_cast<char* >(data) + 4, thisLen); 
            deserialize_ReplyHeader(ia, "hdr", &hdr);

            // update zxid
            if (hdr.zxid > 0)
                sessionInfo_.lastSeenZxid = hdr.zxid;
            else
                std::cout << "zxid " << hdr.zxid << std::endl;

            if (!_ProcessResponse(hdr, ia))
                return false;

            data += thisLen + sizeof(thisLen);
        }
        break;

    default:
        assert (false);
        data += len;
        break;
    }

    return true;
}

void ZookeeperConn::OnConnect()
{
    // But this is in net thread, I miss ananas!
    assert (state_ == State::kNone);

    std::cout << __PRETTY_FUNCTION__ << std::endl;
    DBG << __PRETTY_FUNCTION__;

    {
        auto del = [this](FILE* fp) {
            ::fclose(fp);
            ::unlink(this->kSessionFile.c_str());
        };

        std::unique_ptr<FILE, decltype(del)> _(fopen(kSessionFile.data(), "rb"), del);
        FILE* const fp = _.get();

        if (fp)
            fread(&sessionInfo_, sizeof sessionInfo_, 1, fp);
    }

    char buffer_req[HANDSHAKE_REQ_SIZE];
    int len = sizeof(buffer_req);
    
    struct connect_req req; 
    req.protocolVersion = 0; 
    req.sessionId = sessionInfo_.sessionId;
    req.passwd_len = sizeof(req.passwd);
    req.timeOut = kTimeout;
    req.lastZxidSeen = sessionInfo_.lastSeenZxid;
    memcpy(req.passwd, sessionInfo_.passwd, req.passwd_len);
                    
    StackBuffer<HANDSHAKE_REQ_SIZE + 4> buf;
    buf << htonl(len)
        << htonl(req.protocolVersion)
        << htonll(req.lastZxidSeen)
        << htonl(req.timeOut)
        << htonll(req.sessionId)
        << htonl(req.passwd_len);

    if (req.passwd_len > 0)
        buf.PushData(req.passwd, req.passwd_len);

    auto s = sock_.lock();
    if (s)
    {
        state_ = State::kHandshaking;
        s->SendPacket(buf);
    }
}

void ZookeeperConn::OnDisconnect()
{
    if (pingTimer_)
        TimerManager::Instance().KillTimer(pingTimer_);
}

static struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f, {"world", "anyone"}}};
static struct ACL _READ_ACL_UNSAFE_ACL[] = {{0x01, {"world", "anyone"}}};
static struct ACL _CREATOR_ALL_ACL_ACL[] = {{0x1f, {"auth", ""}}};
struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};
struct ACL_vector ZOO_READ_ACL_UNSAFE = { 1, _READ_ACL_UNSAFE_ACL};
struct ACL_vector ZOO_CREATOR_ALL_ACL = { 1, _CREATOR_ALL_ACL_ACL};

    
static std::string MakeParentNode(int setid)
{
    std::string path("/servers/set-");
    path += std::to_string(setid); 

    return path;
}

// /servers/set-{setid}/qedis(ip:port)-xxxseq
static std::string MakeNodePath(int setid, const std::string& addr)
{
    std::string path(MakeParentNode(setid));
    path += "/qedis(" + addr + ")-";

    return path;
}

static int GetNodeSeq(const std::string& path)
{
    // /servers/set-{setid}/qedis(ip:port)-xxxseq
    auto pos = path.find_last_of('-');
    if (pos == std::string::npos)
        return -1;

    std::string number(path.substr(pos + 1));
    return std::stoi(number);
}

static std::string GetNodeAddr(const std::string& path)
{
    // /servers/set-{setid}/qedis(ip:port)-xxxseq
    auto start = path.find_first_of('(');
    auto end = path.find_first_of(')');
    if (start == std::string::npos ||
        end == std::string::npos)
        return std::string();

    return path.substr(start + 1, end - start - 1);
}

void ZookeeperConn::RunForMaster(int setid, const std::string& value)
{
    INF << __FUNCTION__ << ", setid " << setid << ", value " << value;

    struct oarchive* oa = create_buffer_oarchive();
    struct RequestHeader h = { STRUCT_INITIALIZER (xid , _GetXid()), STRUCT_INITIALIZER (type ,ZOO_CREATE_OP) };
    int rc = serialize_RequestHeader(oa, "header", &h);

    if (rc < 0) return;

    std::string path(MakeNodePath(setid, addr_));

    struct CreateRequest req;
    req.path = const_cast<char* >(path.data());
    req.data.buff = const_cast<char* >(value.data());
    req.data.len = static_cast<int32_t>(value.size());
    req.flags = ZOO_SEQUENCE | ZOO_EPHEMERAL;
    req.acl = ZOO_OPEN_ACL_UNSAFE;
    rc = rc < 0 ? rc : serialize_CreateRequest(oa, "req", &req);

    _SendPacket(h, oa);
}

int ZookeeperConn::_GetXid() const
{
    return ++ xid_; 
}
    
bool ZookeeperConn::_ProcessHandshake(const prime_struct& rsp)
{
    if (sessionInfo_.sessionId && sessionInfo_.sessionId != rsp.sessionId)
    {
        DBG << "expired, new session " << rsp.sessionId;
        return false;
    }

    DBG << "new session Id " << rsp.sessionId;

    sessionInfo_.sessionId = rsp.sessionId;
    memcpy(sessionInfo_.passwd, rsp.passwd, rsp.passwd_len);

    std::unique_ptr<FILE, decltype(fclose)*> _(fopen(kSessionFile.data(), "wb"), fclose);
    FILE* fp = _.get();
    fwrite(&sessionInfo_, sizeof sessionInfo_, 1, fp);

    state_ = State::kConnected;

    _InitPingTimer();
    return true;
}

void ZookeeperConn::_InitPingTimer()
{
    assert (!pingTimer_);
    pingTimer_ = TimerManager::Instance().CreateTimer();
    pingTimer_->Init(kTimeout / 2);
    pingTimer_->SetCallback([this]() {
        INF << "send ping ";

        struct oarchive *oa = create_buffer_oarchive();
        struct RequestHeader h = { STRUCT_INITIALIZER(xid, PING_XID), STRUCT_INITIALIZER (type , ZOO_PING_OP) };

        serialize_RequestHeader(oa, "header", &h); 
        gettimeofday(&this->lastPing_, nullptr);

        _SendPacket(h, oa);
    });

    TimerManager::Instance().AsyncAddTimer(pingTimer_);
}

bool ZookeeperConn::_IsMaster() const
{
    if (siblings_.empty())
        return false;

    auto me = siblings_.find(seq_);
    assert (me != siblings_.end());

    return me == siblings_.begin();
}

bool ZookeeperConn::_ProcessResponse(const ReplyHeader& hdr, iarchive* ia)
{
    if (pendingRequests_.empty())
    {
        ERR << "Can not find request " << hdr.xid;
        return false;
    }

    const Request& req = pendingRequests_.front();
    QEDIS_DEFER
    {
        pendingRequests_.pop_front();
    };

    if (req.xid != hdr.xid)
    {
        ERR << "wrong req xid " << req.xid << ", wrong order response " << hdr.xid;
        return false;
    }

    std::cout << "req.type " << req.type << std::endl;
    switch (req.type)
    {
    case ZOO_PING_OP:
        {
            timeval now;
            gettimeofday(&now, nullptr);
            int microseconds = (now.tv_sec - lastPing_.tv_sec) * 1000000;
            microseconds += (now.tv_usec - lastPing_.tv_usec);
            INF<< "recv ping used microseconds " << microseconds;
        }
        break;

    case ZOO_CREATE_OP:
        {
            CreateResponse rsp;
            if (deserialize_CreateResponse(ia, "rsp", &rsp) != 0)
            {
                ERR << "deserialize_CreateResponse failed";
                return false;
            }

            QEDIS_DEFER
            {
                deallocate_CreateResponse(&rsp);
            };

            assert (node_.empty());
            node_ = rsp.path;
            seq_ = GetNodeSeq(node_);
            if (seq_ < 0)
            {
                ERR << "Wrong node seq " << seq_ << " for node " << node_;
                return false;
            }

            DBG << "my node seq " << seq_ << " for my node " << node_ << ", addr " << GetNodeAddr(node_);
            if (!_GetSiblings(MakeParentNode(setId_)))
                return false;
        }
        break;

    case ZOO_GETCHILDREN2_OP:
        {
            GetChildren2Response rsp;
            if (deserialize_GetChildren2Response(ia, "rsp", &rsp) != 0)
            {
                ERR << "deserialize_GetChildren2Response failed";
                return false;
            }

            QEDIS_DEFER
            {
                deallocate_GetChildren2Response(&rsp);
            };

            siblings_.clear();
            for (int i = 0; i < rsp.children.count; ++ i)
            {
                const std::string& node = rsp.children.data[i];
                int seq = GetNodeSeq(node);

                std::cout << "Get sibling " << node << std::endl;
                siblings_.insert({seq, node});
            }

            auto me = siblings_.find(seq_);
            assert (me != siblings_.end());
            if (me == siblings_.begin())
            {
                if (onBecomeMaster_)
                    onBecomeMaster_();
            }
            else
            {
                // monitor the node bigger than me
                auto brother = -- me; // I'll watch you
                _ExistsAndWatch(MakeParentNode(setId_) + "/" + brother->second);
            }
        }
        break;

    case ZOO_EXISTS_OP:
        {
            if (hdr.err == ZNONODE)
            {
                // check if I am the master
                //_ExistsAndWatch(MakeParentNode(setId_) + "/" + sibling->second);
                const std::string siblingName = req.path.substr(req.path.find_last_of('/') + 1);
                const int seq = GetNodeSeq(siblingName);
                siblings_.erase(seq);
                if (_IsMaster())
                {
                    if (onBecomeMaster_)
                        onBecomeMaster_();
                }
                else
                {
                    auto me = siblings_.find(seq_);
                    assert (me != siblings_.begin());
                    assert (me != siblings_.end());

                    auto brother = -- me; // I'll watch you
                    _ExistsAndWatch(MakeParentNode(setId_) + "/" + brother->second);
                }
            }
            else
            {
                assert (hdr.err == ZOK);
                ExistsResponse rsp;
                if (deserialize_ExistsResponse(ia, "rsp", &rsp) != 0)
                {
                    ERR << "deserialize_ExistsResponse failed";
                    return false;
                }

                QEDIS_DEFER
                {
                    deallocate_ExistsResponse(&rsp);
                };

                DBG << "Exists response version " << rsp.stat.version;
                if (onBecomeSlave_)
                {
                    std::string master = GetNodeAddr(siblings_.begin()->second);
                    if (master.empty())
                        return false;

                    onBecomeSlave_(master);
                }
            }
        }
        break;

    default:
        break;
    }

    return true;
}

bool ZookeeperConn::_GetSiblings(const std::string& parent)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER (type ,ZOO_GETCHILDREN2_OP)};
    struct GetChildren2Request req;
    req.path = const_cast<char* >(parent.data());
    req.watch = 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetChildren2Request(oa, "req", &req);

    if (!_SendPacket(h, oa, &parent))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_ExistsAndWatch(const std::string& sibling)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER (type ,ZOO_EXISTS_OP)};
    struct ExistsRequest req;
    req.path = const_cast<char* >(sibling.data());
    req.watch = 1;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_ExistsRequest(oa, "req", &req);

    if (!_SendPacket(h, oa, &sibling))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_SendPacket(const RequestHeader& h, struct oarchive* oa, const std::string* v)
{
    auto s = sock_.lock();
    if (!s)
        return false;
        
    int totalLen = htonl(get_buffer_len(oa));
    s->SendPacket(&totalLen, sizeof totalLen);
    s->SendPacket(get_buffer(oa), get_buffer_len(oa));

    Request r;
    r.xid = h.xid;
    r.type = h.type;
    if (v) r.path = *v;

    pendingRequests_.emplace_back(std::move(r));
   
    close_buffer_oarchive(&oa, 1);
    return true;
}

} // end namespace qedis

