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

    _SendPacket(h, oa, std::bind(&ZookeeperConn::_OnNodeCreate,
                                 this,
                                 std::placeholders::_1));
}

int ZookeeperConn::_GetXid() const
{
    return ++ xid_; 
}
    
bool ZookeeperConn::_ProcessHandshake(const prime_struct& rsp)
{
    if (sessionInfo_.sessionId && sessionInfo_.sessionId != rsp.sessionId)
    {
        DBG << "Expired old session " << sessionInfo_.sessionId;
        return false;
    }

    const bool resumedSession = (sessionInfo_.sessionId == rsp.sessionId);
    if (resumedSession)
        DBG << "resume session Id:" << rsp.sessionId;
    else
        DBG << "new session Id:" << rsp.sessionId;

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
        if (!_GetSiblings(MakeParentNode(setId_), std::bind(&ZookeeperConn::_OnGetMySiblings,
                                                             this,
                                                             std::placeholders::_1)))
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
            // TODO
        case CHANGED_EVENT_DEF:
            INF << "CHANGED_EVENT_DEF";
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
        INF << "got req.type " << req.type;

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

            return req.cb(&rsp);
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

            return req.cb(&rsp);
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
                
            return req.cb(&drsp);
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

                return req.cb(&rsp);
            }
        }
        break;

    default:
        break;
    }

    return true;
}

bool ZookeeperConn::_ProcessData(const std::string& data)
{
    INF << "Got data " << data;

    // *set的数据格式是  1,3,4,7|ipport@1,4|ipport@3,7
    std::vector<QString> tmp(SplitString(data, '|'));
    switch (tmp.size()) 
    {
        case 0:
            ERR << "Why no data on my set " << setId_;
            return false;

        case 1:
        case 2:
            {
                const auto& sharding = *tmp.begin();
                shards_.clear();
                for (const auto& s : SplitString(sharding, ','))
                {
                    INF << "Add my shard " << s;
                    shards_.insert(std::stoi(s));
                }

                migratingShards_.clear();
                std::unordered_map<SocketAddr, std::set<int>> migration;
                auto it = tmp.begin();
                for (++ it; it != tmp.end(); ++ it)
                {
                    std::vector<QString> dstAndShard(SplitString(*it, '@'));
                    if (dstAndShard.size() != 2)
                    {
                        ERR << "Wrong migration format:" << *it;
                        return false;
                    }
                    else
                    {
                        INF << "Migrate " << dstAndShard[1] << " to " << dstAndShard[0];

                        SocketAddr dstAddr(dstAndShard[0]);
                        auto& shards = migration[dstAddr];

                        std::vector<QString> migrateStr(SplitString(dstAndShard[1], ','));
                        for (const auto& s : migrateStr)
                        {
                            const int shard = std::stoi(s);
                            if (shards_.count(shard))
                                shards.insert(std::stoi(s));
                            else
                                ERR << "Migration: shard " << shard << " is not on my set";
                        }

                        migratingShards_.insert(shards.begin(), shards.end());
                    }
                }

                if (onMigration_ && !migration.empty())
                    onMigration_(migration);
            }

            return true;

        default:
            return false;
    }
}

bool ZookeeperConn::_OnMySetData(void* r)
{
    const auto& drsp = *reinterpret_cast<GetDataResponse*>(r);

    version_ = drsp.stat.version;
    std::string data(drsp.data.buff, drsp.data.len);
    return _ProcessData(data);
}
    
std::vector<SocketAddr> ZookeeperConn::_CollectSlaveAddr() const
{
    assert (_IsMaster());

    std::vector<SocketAddr> slaves;
    slaves.reserve(siblings_.size());

    auto slave = siblings_.begin();
    for (++ slave; slave != siblings_.end(); ++ slave)
    {
        SocketAddr addr(GetNodeAddr(slave->second));
        slaves.push_back(addr);
    }

    return slaves;
}

bool ZookeeperConn::_GetSiblings(const std::string& parent, std::function<bool (void*)> cb)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER (type ,ZOO_GETCHILDREN2_OP)};
    struct GetChildren2Request req;
    req.path = const_cast<char* >(parent.data());
    req.watch = 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetChildren2Request(oa, "req", &req);

    if (!_SendPacket(h, oa, std::move(cb), &parent))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_GetData(const std::string& node, bool watch, std::function<bool (void*)> cb)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER(type ,ZOO_GETDATA_OP)};
    struct GetDataRequest req;
    req.path = const_cast<char* >(node.data());
    req.watch = watch ? 1 : 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetDataRequest(oa, "req", &req);

    if (!_SendPacket(h, oa, std::move(cb), &node))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_SetData(const std::string& node, const std::string& data, int ver, std::function<bool (void*)> cb)
{
    // TODO
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER(type ,ZOO_SETDATA_OP)};
    struct SetDataRequest req;
    req.path = const_cast<char* >(node.data());
    req.data.buff = const_cast<char* >(data.data());
    req.data.len = static_cast<int32_t>(data.size());
    req.version = ver;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_SetDataRequest(oa, "req", &req);

    if (!_SendPacket(h, oa, std::move(cb), &node))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_Exists(const std::string& sibling, bool watch, std::function<bool (void*)> cb)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER (type ,ZOO_EXISTS_OP)};
    struct ExistsRequest req;
    req.path = const_cast<char* >(sibling.data());
    req.watch = watch ? 1 : 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_ExistsRequest(oa, "req", &req);

    if (!_SendPacket(h, oa, std::move(cb), &sibling))
        return false;

    return rc >= 0;
}

bool ZookeeperConn::_SendPacket(const RequestHeader& h,
                                struct oarchive* oa,
                                std::function<bool (void*)> cb,
                                const std::string* v)
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
    r.cb = std::move(cb);

    pendingRequests_.emplace_back(std::move(r));
   
    close_buffer_oarchive(&oa, 1);
    return true;
}

bool ZookeeperConn::_OnWatchBrother(void* r)
{
    const auto& rsp = *reinterpret_cast<ExistsResponse*>(r);

    DBG << "Exists response version " << rsp.stat.version;
    if (onBecomeSlave_)
    {
        std::string master = GetNodeAddr(siblings_.begin()->second);
        if (master.empty())
            return false;

        onBecomeSlave_(master);
    }

    return true;
}

bool ZookeeperConn::_OnNodeCreate(void* r)
{
    const CreateResponse& rsp = *reinterpret_cast<CreateResponse*>(r);
    assert (node_.empty());
    node_ = rsp.path;
    seq_ = GetNodeSeq(node_);
    assert (seq_ >= 0);

    DBG << "Create me, seq " << seq_
        << " for node " << node_
        << ", addr " << GetNodeAddr(node_);

    if (!_GetSiblings(MakeParentNode(setId_), std::bind(&ZookeeperConn::_OnGetMySiblings,
                                                         this,
                                                         std::placeholders::_1)))
        return false;

    return true;
}

bool ZookeeperConn::_OnGetMySiblings(void* r)
{
    const GetChildren2Response& rsp = *reinterpret_cast<GetChildren2Response*>(r);

    siblings_.clear();
    for (int i = 0; i < rsp.children.count; ++ i)
    {
        const std::string& node = rsp.children.data[i];
        int seq = GetNodeSeq(node);
        assert (seq >= 0);

        if (node_.empty())
        {
            std::string addr(GetNodeAddr(node));
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
        // I am master!
        if (onBecomeMaster_)
            onBecomeMaster_(_CollectSlaveAddr());

        // fetch sharding / migration
        if (!_GetData(MakeParentNode(setId_), true, std::bind(&ZookeeperConn::_OnMySetData,
                                                              this,
                                                              std::placeholders::_1)))
        {
            ERR << "GetData failed for set " << setId_;
            return false;
        }
    }
    else
    {
        // monitor the node just before than me
        auto brother = -- me; // I'll watch you
        _Exists(MakeParentNode(setId_) + "/" + brother->second,
                true,
                std::bind(&ZookeeperConn::_OnWatchBrother,
                           this,
                           std::placeholders::_1));
    }

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
        bool succ = _GetSiblings(MakeParentNode(setId_), std::bind(&ZookeeperConn::_OnGetMySiblings,
                                                                   this,
                                                                   std::placeholders::_1));
        if (!succ)
            ERR << __FUNCTION__ << ", _GetSiblings failed with " << setId_;
    }
    else
    {
        auto me = siblings_.find(seq_);
        assert (me != siblings_.begin());
        assert (me != siblings_.end());

        auto brother = -- me; // I'll watch you
        _Exists(MakeParentNode(setId_) + "/" + brother->second,
                true,
                std::bind(&ZookeeperConn::_OnWatchBrother,
                           this,
                           std::placeholders::_1));
    }
}

void ZookeeperConn::UpdateShardData()
{
    std::set<int> newShards;
    std::set_difference(shards_.begin(), shards_.end(),
                        migratingShards_.begin(), migratingShards_.end(),
                        std::inserter(newShards, newShards.begin()));

    migratingShards_.clear();
    shards_.clear();
    newShards.swap(shards_);

    std::string data(qedis::StringJoin(shards_.begin(), shards_.end(), ','));
    INF << "Set key " << MakeParentNode(setId_) << " = " << data;
    // TODO callback
    _SetData(MakeParentNode(setId_), data, version_, [](void* ) { return true; });
}

} // end namespace qedis

#endif

