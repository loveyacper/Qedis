#include <unistd.h>
#include <sys/time.h>
#include "ZookeeperContext.h"
#include "proto.h"
#include "zookeeper.jute.h"
#include "net/Connection.h"
#include "net/Buffer.h"
#include "util/Util.h"

#include <iostream>

#include "proto.h"

static void deserialize_prime_response(struct prime_struct *req, char* buffer)
{
     int offset = 0;
     memcpy(&req->len, buffer + offset, sizeof(req->len));
     offset = offset +  sizeof(req->len);

     req->len = ntohl(req->len);
     memcpy(&req->protocolVersion, buffer + offset, sizeof(req->protocolVersion));
     offset = offset +  sizeof(req->protocolVersion);

     req->protocolVersion = ntohl(req->protocolVersion);
     memcpy(&req->timeOut, buffer + offset, sizeof(req->timeOut));
     offset = offset +  sizeof(req->timeOut);

     req->timeOut = ntohl(req->timeOut);
     memcpy(&req->sessionId, buffer + offset, sizeof(req->sessionId));
     offset = offset +  sizeof(req->sessionId);

     req->sessionId = htonll(req->sessionId);
     memcpy(&req->passwd_len, buffer + offset, sizeof(req->passwd_len));
     offset = offset +  sizeof(req->passwd_len);

     req->passwd_len = ntohl(req->passwd_len);
     memcpy(req->passwd, buffer + offset, sizeof(req->passwd));
}

static void serialize_prime_connect(struct connect_req *req, char* buffer)
{
    int offset = 0;
    req->protocolVersion = htonl(req->protocolVersion);
    memcpy(buffer + offset, &req->protocolVersion, sizeof(req->protocolVersion));
    offset = offset +  sizeof(req->protocolVersion);

    req->lastZxidSeen = htonll(req->lastZxidSeen);
    memcpy(buffer + offset, &req->lastZxidSeen, sizeof(req->lastZxidSeen));
    offset = offset +  sizeof(req->lastZxidSeen);

    req->timeOut = htonl(req->timeOut);
    memcpy(buffer + offset, &req->timeOut, sizeof(req->timeOut));
    offset = offset +  sizeof(req->timeOut);

    req->sessionId = htonll(req->sessionId);
    memcpy(buffer + offset, &req->sessionId, sizeof(req->sessionId));
    offset = offset +  sizeof(req->sessionId);

    req->passwd_len = htonl(req->passwd_len);
    memcpy(buffer + offset, &req->passwd_len, sizeof(req->passwd_len));
    offset = offset +  sizeof(req->passwd_len);

    memcpy(buffer + offset, req->passwd, sizeof(req->passwd)); 
}


const int kTimeout = 10 * 1000; // ms

namespace qedis
{
    
ZookeeperContext::ZookeeperContext(ananas::Connection* c) :
    conn_(c),
    xid_(0),
    state_(ZookeeperContext::State::kNone)
{
    sessionInfo_.passwd[0] = 0;

    sessionFile_ = "zk.session" + c->Peer().ToString();
}

ZookeeperContext::~ZookeeperContext()
{
}

bool ZookeeperContext::ParseMessage(const char*& data, ananas::PacketLen_t len)
{
    switch (state_)
    {
    case State::kHandshaking:
        {
            if (len < HANDSHAKE_RSP_SIZE)
                return true;

            auto zkrsp = NewResponse<prime_struct>();
            deserialize_prime_response(zkrsp.get(), const_cast<char* >(data));
            data += sizeof(int) + zkrsp->len;
            std::cout << "skip kHandshaking " << zkrsp->len << std::endl;

            pendingReq_.front().promise.SetValue(AnyCast<void>(zkrsp));
            pendingReq_.pop();
        }
        break;

    case State::kConnected:
        {
            assert (len >= 4);
            int thisLen = *(int*)data;
            thisLen = ntohl(thisLen);
            if (sizeof thisLen + thisLen > len)
            {
                std::cout << "thisLen is " << thisLen << ", only len " << len << std::endl;
                return true; // not enough
            }
                
            std::cout << "thisLen is " << thisLen << ", and len " << len << std::endl;
            
            struct ReplyHeader hdr;
            struct iarchive *ia = create_buffer_iarchive(const_cast<char* >(data) + 4, thisLen); 
            deserialize_ReplyHeader(ia, "hdr", &hdr);

            if (hdr.zxid > 0)
                sessionInfo_.lastZxidSeen = hdr.zxid;

            if (!_ProcessResponse(hdr, ia))
                return false;

            data += thisLen + sizeof thisLen;
            return true;
        }
        break;
    default:
        assert (false);
        break;
    }

    return true;
}
            
bool ZookeeperContext::_ProcessResponse(const ReplyHeader& hdr, iarchive* ia)
{
    if (hdr.err != 0)
    {
        std::cout << "TODO hdr.err " << hdr.err << std::endl;
        //return false;
    }

    if (hdr.xid == WATCHER_EVENT_XID)
    {
        std::cout << "TODO WATCHER_EVENT_XID\n";
        return false;
        //return _ProcessWatchEvent(hdr, ia);
    }

    if (pendingReq_.empty())
    {
        std::cerr << "Can not find request " << hdr.xid << std::endl;
        return false;
    }

    auto& req = pendingReq_.front();
    ANANAS_DEFER
    {
        if (hdr.err)
            std::cout << "Error " << hdr.err << " with " << req.path << std::endl;

        pendingReq_.pop();
    };

    if (req.xid != hdr.xid)
    {
        std::cerr << "Req xid " << req.xid << ", recv wrong order response " << hdr.xid << std::endl;
        return false;
    }

    switch (req.type)
    {
    case ZOO_PING_OP:
        {
            timeval now;
            gettimeofday(&now, nullptr);

            auto rsp = NewResponse<long>();
            *rsp = now.tv_sec * 1000;
            *rsp += now.tv_usec  / 1000;

            req.promise.SetValue(AnyCast<void>(rsp));
        }
        break;

    case ZOO_CREATE_OP:
        {
            CreateResponse crsp;
            if (deserialize_CreateResponse(ia, "rsp", &crsp) != 0)
            {
                std::cout << "deserialize_CreateResponse failed\n";
                return false;
            }

            ANANAS_DEFER
            {
                deallocate_CreateResponse(&crsp);
            };

            auto rsp = NewResponse<CreateRsp>();
            *rsp = Convert(crsp);

            req.promise.SetValue(AnyCast<void>(rsp));
        }
        break;

    case ZOO_GETCHILDREN2_OP:
        {
            GetChildren2Response grsp;
            if (deserialize_GetChildren2Response(ia, "rsp", &grsp) != 0)
            {
                std::cout << "deserialize_GetChildren2Response failed\n";
                return false;
            }

            ANANAS_DEFER
            {
                deallocate_GetChildren2Response(&grsp);
            };

            auto rsp = NewResponse<ChildrenRsp>();
            *rsp = Convert(req.path, grsp);
            req.promise.SetValue(AnyCast<void>(rsp));
        }
        break;

    case ZOO_GETDATA_OP:
        {
            GetDataResponse drsp;
            if (deserialize_GetDataResponse(ia, "rsp", &drsp) != 0)
            {
                std::cout << "deserialize_GetDataResponse failed\n";
                return false;
            }

            ANANAS_DEFER
            {
                deallocate_GetDataResponse(&drsp);
            };

            auto rsp = NewResponse<DataRsp>();
            *rsp = Convert(req.path, drsp);

            req.promise.SetValue(AnyCast<void>(rsp));
        }
        break;

    default:
        std::cout << "TODO req type " << req.type << std::endl;
        break;
    }

    return true;
}
    
void ZookeeperContext::ProcessPing(long lastPing, ZkResponse now)
{
    auto rsp = AnyCast<long>(now);
    int millseconds = *rsp - lastPing;
    if (millseconds > 1)
        std::cout << "ProcessPing used millseconds:" << millseconds << std::endl;
}

bool ZookeeperContext::ProcessHandshake(const ZkResponse& rsp)
{
    auto hrsp = AnyCast<prime_struct>(rsp);
    if (sessionInfo_.sessionId && sessionInfo_.sessionId != hrsp->sessionId)
    {
        std::cout << "expired old session " << sessionInfo_.sessionId << ", new session " << hrsp->sessionId << std::endl;
        return false;
    }

    resumed_ = (sessionInfo_.sessionId == hrsp->sessionId);
    if (resumed_)
        std::cout << "resume session Id " << hrsp->sessionId << std::endl;
    else
        std::cout << "new session Id " << hrsp->sessionId << std::endl;

    // record this sessionInfo;
    sessionInfo_.sessionId = hrsp->sessionId;
    memcpy(sessionInfo_.passwd, hrsp->passwd, hrsp->passwd_len);

    std::unique_ptr<FILE, decltype(fclose)*> _(fopen(sessionFile_.data(), "wb"), fclose);
    FILE* fp = _.get();
    assert (fp);
    fwrite(&sessionInfo_, sizeof sessionInfo_, 1, fp);

    assert (state_ == State::kHandshaking);
    state_  = State::kConnected;

    std::cout << "recv ProcessHandshake sid " << hrsp->sessionId << std::endl;
    return true;
}

ananas::Future<ZkResponse> ZookeeperContext::DoHandshake()
{
    {
        auto del = [this](FILE* fp) {
            ::fclose(fp);
            ::unlink(sessionFile_.c_str());
        };

        std::unique_ptr<FILE, decltype(del)> _(fopen(sessionFile_.data(), "rb"), del);
        FILE* const fp = _.get();

        if (fp)
            fread(&sessionInfo_, sizeof sessionInfo_, 1, fp);
    }

    struct connect_req req; 
    memset(&req, 0, sizeof req);
    req.timeOut = kTimeout;
    req.sessionId = sessionInfo_.sessionId;
    req.lastZxidSeen = sessionInfo_.lastZxidSeen;
    req.passwd_len = sizeof(req.passwd);
    memcpy(req.passwd, sessionInfo_.passwd, req.passwd_len);
                    
    char buffer[HANDSHAKE_REQ_SIZE];
    int len = HANDSHAKE_REQ_SIZE;
    len = htonl(len);
    serialize_prime_connect(&req, buffer);
    
    struct ananas::SliceVector v;
    v.PushBack(&len, sizeof len);
    v.PushBack(buffer, HANDSHAKE_REQ_SIZE);
    conn_->SendPacket(v);

    state_ = State::kHandshaking;

    return _PendingRequest(0, 0);
}

ananas::Future<ZkResponse> ZookeeperContext::Ping()
{
    struct oarchive *oa = create_buffer_oarchive();
    struct RequestHeader h = { STRUCT_INITIALIZER(xid, PING_XID), STRUCT_INITIALIZER (type , ZOO_PING_OP) };

    serialize_RequestHeader(oa, "header", &h);

    _SendPacket(oa);
    return _PendingRequest(ZOO_PING_OP, PING_XID);
}

static struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f, {"world", "anyone"}}};
struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};

ananas::Future<ZkResponse> ZookeeperContext::CreateNode(bool empher,
                                                        bool seq,
                                                        const std::string* data,
                                                        const std::string* pathPrefix)
{
    struct oarchive *oa = create_buffer_oarchive();
    struct RequestHeader h = { STRUCT_INITIALIZER (xid , _GetXid()), STRUCT_INITIALIZER (type ,ZOO_CREATE_OP) };
    int rc = serialize_RequestHeader(oa, "header", &h);
    if (rc < 0)
    {
        close_buffer_oarchive(&oa, 1);
        return ananas::MakeExceptionFuture<ZkResponse>(std::runtime_error("serialize_RequestHeader failed when try to create node"));
    }

    struct CreateRequest req;
    req.path = const_cast<char* >(pathPrefix->data());
    req.data.buff = const_cast<char* >(data->data());
    req.data.len = static_cast<int32_t>(data->size());
    req.acl = ZOO_OPEN_ACL_UNSAFE;
    req.flags = 0;
    if (empher) req.flags |= ZOO_SEQUENCE;
    if (empher) req.flags |= ZOO_EPHEMERAL;

    rc = rc < 0 ? rc : serialize_CreateRequest(oa, "req", &req);

    _SendPacket(oa);
    return _PendingRequest(h.type, h.xid);
}

void ZookeeperContext::ProcessCreateNode(const ZkResponse& rsp)
{
    auto crsp = AnyCast<CreateRsp>(rsp);
    std::cout << "Create Node " << crsp->path << std::endl;
}

ananas::Future<ZkResponse> ZookeeperContext::GetChildren2(const std::string& parent, bool watch)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER(type ,ZOO_GETCHILDREN2_OP)};
    struct GetChildren2Request req;
    req.path = const_cast<char* >(parent.data());
    req.watch = watch ? 1 : 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetChildren2Request(oa, "req", &req);

    _SendPacket(oa);
    return _PendingRequest(h.type, h.xid, &parent);
}

void ZookeeperContext::ProcessGetChildren2(const ZkResponse& rsp)
{
    auto grsp = AnyCast<ChildrenRsp>(rsp);
    std::cout << "GetChildren2 for " << grsp->parent << std::endl;

    for (const auto& c : grsp->children)
    {
        std::cout << "child: " << c << std::endl;
    }
}

ananas::Future<ZkResponse> ZookeeperContext::GetData(const std::string& node, bool watch)
{
    struct oarchive* oa = create_buffer_oarchive();

    struct RequestHeader h = { STRUCT_INITIALIZER( xid, _GetXid()), STRUCT_INITIALIZER(type ,ZOO_GETDATA_OP)};
    struct GetDataRequest req;
    req.path = const_cast<char* >(node.data());
    req.watch = watch ? 1 : 0;
    
    int rc = serialize_RequestHeader(oa, "header", &h); 
    rc = rc < 0 ? rc : serialize_GetDataRequest(oa, "req", &req);

    _SendPacket(oa);
    return _PendingRequest(h.type, h.xid, &node);
}

void ZookeeperContext::ProcessGetData(const ZkResponse& rsp)
{
    auto drsp = AnyCast<DataRsp>(rsp);
    std::cout << "GetData " << drsp->path << " : " << drsp->data << std::endl;
}


int ZookeeperContext::_GetXid() const
{
    return ++ xid_; 
}
    
ananas::Future<ZkResponse> ZookeeperContext::_PendingRequest(int type, int xid, const std::string* path)
{
    struct Request req;
    req.type = type;
    req.xid = xid;
    if (path) req.path = *path;

    auto fut = req.promise.GetFuture();
    pendingReq_.push(std::move(req));

    return fut;
}

void ZookeeperContext::_SendPacket(struct oarchive* oa)
{
    int totalLen = htonl(get_buffer_len(oa));

    struct ananas::SliceVector v;
    v.PushBack(&totalLen, sizeof totalLen);
    v.PushBack(get_buffer(oa), get_buffer_len(oa));

    conn_->SendPacket(v);
    close_buffer_oarchive(&oa, 1);
}

} // end namespace qedis
