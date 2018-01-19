#if QEDIS_CLUSTER

#include <unistd.h>
#include "ZookeeperConn.h"
#include "Log/Logger.h"
#include "QCommon.h"

#include "zookeeper.jute.h"
#include "proto.h"

static int deserialize_prime_response(struct prime_struct* req, AttachedBuffer& buffer)
{
    buffer >> req->len;
    req->len = ntohl(req->len); 

    buffer >> req->protocolVersion;
    req->protocolVersion = ntohl(req->protocolVersion); 

    buffer >> req->timeOut;
    req->timeOut = ntohl(req->timeOut);

    buffer >> req->sessionId;
    req->sessionId = htonll(req->sessionId); 

    buffer >> req->passwd_len;
    req->passwd_len = ntohl(req->passwd_len); 

    memcpy(req->passwd, buffer.ReadAddr(), sizeof(req->passwd)); 
    return 0;
}
          
          
const int kTimeout = 15 * 1000; // ms

namespace qedis
{
    
    
ZookeeperConn::ZookeeperConn(const std::shared_ptr<StreamSocket>& c, int setId, const std::string& addr) :
    xid_(0),
    setId_(setId),
    addr_(addr),
    state_(State::kNone),
    pingTimer_(nullptr),
    sock_(c)
{
    lastPing_.tv_sec = 0;
    lastPing_.tv_usec = 0;
    sessionInfo_.passwd[0] = '\0';
    sessionFile_ = "zk.session" + addr;
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
            AttachedBuffer buffer(const_cast<char* >(data), len);
            deserialize_prime_response(&rsp, buffer);
            data += sizeof(int) + rsp.len;
        
            if (!_ProcessHandshake(rsp))
                return false;
        }
        break;

    case State::kConnected:
        {
            if (len < 4)
                return true;

            int thisLen = *(int*)data;
            thisLen = ntohl(thisLen);
            if (sizeof(thisLen) + thisLen > len)
                return true;

            struct ReplyHeader hdr;
            struct iarchive *ia = create_buffer_iarchive(const_cast<char* >(data) + 4, thisLen); 
            deserialize_ReplyHeader(ia, "hdr", &hdr);

            // update zxid
            if (hdr.zxid > 0)
                sessionInfo_.lastSeenZxid = hdr.zxid;

            if (!_ProcessResponse(hdr, ia))
                return false;

            data += thisLen + sizeof(thisLen);
        }
        break;

    default:
        assert (false);
        break;
    }

    return true;
}

void ZookeeperConn::OnConnect()
{
    assert (state_ == State::kNone);

    {
        auto del = [this](FILE* fp) {
            ::fclose(fp);
            ::unlink(this->sessionFile_.c_str());
        };

        std::unique_ptr<FILE, decltype(del)> _(fopen(sessionFile_.data(), "rb"), del);
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

static struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f,
                                                 {const_cast<char*>("world"),
                                                  const_cast<char*>("anyone")
                                                 }
                                           }};
struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};

    
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

void ZookeeperConn::_RunForMaster(int setid, const std::string& value)
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

    const bool resumedSession = (sessionInfo_.sessionId == rsp.sessionId);
    if (resumedSession)
        DBG << "resume session Id " << rsp.sessionId;
    else
        DBG << "new session Id " << rsp.sessionId;

    sessionInfo_.sessionId = rsp.sessionId;
    memcpy(sessionInfo_.passwd, rsp.passwd, rsp.passwd_len);

    std::unique_ptr<FILE, decltype(fclose)*> _(fopen(sessionFile_.data(), "wb"), fclose);
    FILE* fp = _.get();
    fwrite(&sessionInfo_, sizeof sessionInfo_, 1, fp);

    state_ = State::kConnected;

    _InitPingTimer();
                
    if (resumedSession)
    {
        // My node must exists
        if (!_GetSiblings(MakeParentNode(setId_)))
            return false;
    }
    else
    {
        // Create my node
        _RunForMaster(setId_, addr_);
    }

    return true;
}

void ZookeeperConn::_InitPingTimer()
{
    assert (!pingTimer_);
    pingTimer_ = TimerManager::Instance().CreateTimer();
    pingTimer_->Init(kTimeout / 2);
    pingTimer_->SetCallback([this]() {
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

bool ZookeeperConn::_ProcessWatchEvent(const ReplyHeader& hdr, iarchive* ia)
{
    struct WatcherEvent evt;
    deserialize_WatcherEvent(ia, "event", &evt);

    INF << "WatcherEventType " << evt.type
        << ", state " << evt.state
        << ", path " << evt.path;

    switch (evt.type)
    {
        case DELETED_EVENT_DEF:
            _OnNodeDelete(evt.path);
            break;

        default:
            break;
    }

    deallocate_WatcherEvent(&evt);
    return true;
}

bool ZookeeperConn::_ProcessResponse(const ReplyHeader& hdr, iarchive* ia)
{
    if (hdr.xid == WATCHER_EVENT_XID)
    {
        return _ProcessWatchEvent(hdr, ia);
    }

    // TODO process some other watcher events

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

    if (req.type != ZOO_PING_OP)
        INF << "req.type " << req.type;

    switch (req.type)
    {
    case ZOO_PING_OP:
        {
            timeval now;
            gettimeofday(&now, nullptr);
            int microseconds = (now.tv_sec - lastPing_.tv_sec) * 1000000;
            microseconds += (now.tv_usec - lastPing_.tv_usec);
            if (microseconds > 10 * 1000)
                WRN << "recv ping used microseconds " << microseconds;
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
            assert (seq_ >= 0);

            DBG << "my node seq " << seq_
                << " for my node " << node_
                << ", addr " << GetNodeAddr(node_);

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
                assert (seq >= 0);

                if (node_.empty())
                {
                    std::string addr = GetNodeAddr(node);
                    if (addr == addr_)
                    {
                        node_ = node;
                        seq_ = seq;
                        DBG << "Resumed session: my seq " << seq_
                            << " for my node " << node_
                            << ", addr " << GetNodeAddr(node_);
                    }
                }


                INF << "Get sibling " << node;
                siblings_.insert({seq, node});
            }

            auto me = siblings_.find(seq_);
            assert (me != siblings_.end());
            if (me == siblings_.begin())
            {
                // I am master!先获取节点分片信息，再执行slaveof no one等
                if (!_GetData(MakeParentNode(setId_), true))
                {
                    ERR << "_GetData failed for set " << setId_;
                    return false;
                }
            }
            else
            {
                // monitor the node bigger than me
                auto brother = -- me; // I'll watch you
                _Exists(MakeParentNode(setId_) + "/" + brother->second, true);
            }
        }
        break;

    case ZOO_GETDATA_OP:
        {
            GetDataResponse drsp;
            if (deserialize_GetDataResponse(ia, "rsp", &drsp) != 0)
            {
                ERR << "deserialize_GetDataResponse failed";
                return false;
            }

            QEDIS_DEFER
            {
                deallocate_GetDataResponse(&drsp);
            };
                
            // 获取了节点分片信息

            std::vector<SocketAddr> slaves;
            slaves.reserve(siblings_.size());

            auto me = siblings_.find(seq_);
            assert (me != siblings_.end());
            auto slave = me;
            for (++ slave; slave != siblings_.end(); ++ slave)
            {
                SocketAddr addr(GetNodeAddr(slave->second));
                slaves.push_back(addr);
            }

#if 0
            std::vector<int> shardings;
            std::unordered_map<int, std::vector<int> > migration; // dst set & shardings
            {
                // *set的数据格式是  1,3,4,7|2:1,4
                std::string data(drsp.data.buff, drsp.data.len);
                std::vector<QString> tmp(SplitString(data, '|'));
                std::vector<QString> shardingStr(SplitString(tmp[0], ','));
                auto it = tmp.begin();
                for (++ it; it != tmp.end(); ++ it)
                {
                    std::vector<QString> dstAndShardingStr(SplitString(*it, ':'));
                    assert (dstAndShardingStr.size() == 2);

                    int dstSetId = std::stoi(dstAndShardingStr[0]);
                    std::vector<QString> migrateStr(SplitString(dstAndShardingStr[1], ','));
                    std::vector<int> migrates;
                    migrates.reserve(migrateStr.size());
                    for (const auto& s : migrateStr)
                        migrates.push_back(std::stoi(s));

                    migration[dstSetId] = migrates;
                }
            }
#endif

            if (onBecomeMaster_)
                onBecomeMaster_(slaves);
        }
        break;

    case ZOO_EXISTS_OP:
        {
            if (hdr.err == ZNONODE)
            {
                _OnNodeDelete(req.path);
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

bool ZookeeperConn::_GetData(const std::string& node, bool watch)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER(type ,ZOO_GETDATA_OP)};
    struct GetDataRequest req;
    req.path = const_cast<char* >(node.data());
    req.watch = watch ? 1 : 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetDataRequest(oa, "req", &req);

    if (!_SendPacket(h, oa, &node))
        return false;

    return rc >= 0;
}


bool ZookeeperConn::_Exists(const std::string& sibling, bool watch)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER (type ,ZOO_EXISTS_OP)};
    struct ExistsRequest req;
    req.path = const_cast<char* >(sibling.data());
    req.watch = watch ? 1 : 0;
    
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

void ZookeeperConn::_OnNodeDelete(const std::string& node)
{
    const std::string siblingName = node.substr(node.find_last_of('/') + 1);
    const int seq = GetNodeSeq(siblingName);
    siblings_.erase(seq);
    if (_IsMaster())
    {
        // Though I'll be master, I must broadcast this fact to all children.
        bool succ = _GetSiblings(MakeParentNode(setId_));
        if (!succ)
            ERR << __FUNCTION__ << ", _GetSiblings failed with " << setId_;
    }
    else
    {
        auto me = siblings_.find(seq_);
        assert (me != siblings_.begin());
        assert (me != siblings_.end());

        auto brother = -- me; // I'll watch you
        _Exists(MakeParentNode(setId_) + "/" + brother->second, true);
    }
}

} // end namespace qedis

#endif
