#include "ZookeeperConn.h"
#include "Log/Logger.h"

#include "zookeeper.jute.h"
#include "proto.h"

namespace qedis
{

ZookeeperConn::ZookeeperConn(const std::shared_ptr<StreamSocket>& c) :
    QClusterConn(c),
    xid_(0)
{
}

bool ZookeeperConn::ParseMessage(const char*& data, size_t len)
{
    data += len;
    return true;
}

#define HANDSHAKE_REQ_SIZE 44
/* connect request */
struct connect_req { 
    int32_t protocolVersion; 
    int64_t lastZxidSeen; 
    int32_t timeOut; 
    int64_t sessionId; 
    int32_t passwd_len; 
    char passwd[16]; 
};

void ZookeeperConn::OnConnect()
{
    // TODO
    // prime the connection
    // send last zxid, send old-session-id&password

    char buffer_req[HANDSHAKE_REQ_SIZE];
    int len = sizeof(buffer_req);
    
    struct connect_req req; 
    req.protocolVersion = 0; 
    req.sessionId = 0;//zh->client_id.client_id; 
    req.passwd_len = sizeof(req.passwd);//
    req.timeOut = 15 * 1000; 
    req.lastZxidSeen = 0;//zh->last_zxid;
                    
    StackBuffer<HANDSHAKE_REQ_SIZE + 4> buf;
    buf << htonl(len)
        << htonl(req.protocolVersion)
        << req.lastZxidSeen
        << htonl(req.timeOut)
        << req.sessionId
        << htonl(req.passwd_len);

    if (req.passwd_len)
        buf.PushData(req.passwd, req.passwd_len);

    auto s = sock_.lock();
    assert (s);
    s->SendPacket(buf);
        
    // just for test!
    RunForMaster(1, "zkvalue");
}

#if 0
const int ZOO_PERM_READ = 1 << 0;
const int ZOO_PERM_WRITE = 1 << 1;
const int ZOO_PERM_CREATE = 1 << 2;
const int ZOO_PERM_DELETE = 1 << 3;
const int ZOO_PERM_ADMIN = 1 << 4;
const int ZOO_PERM_ALL = 0x1f;
struct Id ZOO_ANYONE_ID_UNSAFE = {"world", "anyone"};
struct Id ZOO_AUTH_IDS = {"auth", ""};
#endif
static struct ACL _OPEN_ACL_UNSAFE_ACL[] = {{0x1f, {"world", "anyone"}}};
static struct ACL _READ_ACL_UNSAFE_ACL[] = {{0x01, {"world", "anyone"}}};
static struct ACL _CREATOR_ALL_ACL_ACL[] = {{0x1f, {"auth", ""}}};
struct ACL_vector ZOO_OPEN_ACL_UNSAFE = { 1, _OPEN_ACL_UNSAFE_ACL};
struct ACL_vector ZOO_READ_ACL_UNSAFE = { 1, _READ_ACL_UNSAFE_ACL};
struct ACL_vector ZOO_CREATOR_ALL_ACL = { 1, _CREATOR_ALL_ACL_ACL};

void ZookeeperConn::RunForMaster(int setid, const std::string& value)
{
    INF << __FUNCTION__ << ", setid " << setid << ", value " << value;

    struct oarchive* oa = create_buffer_oarchive();
    struct RequestHeader h = { STRUCT_INITIALIZER (xid , _GetXid()), STRUCT_INITIALIZER (type ,ZOO_CREATE_OP) };
    int rc = serialize_RequestHeader(oa, "header", &h);

    if (rc < 0) return;

    // /servers/set-{setid}/qedis-xxx
    std::string path("/servers/set-");
    path += std::to_string(setid); 
    path += "/qedis-";

    struct CreateRequest req;
    req.path = const_cast<char* >(path.data());
    req.data.buff = const_cast<char* >(value.data());
    req.data.len = static_cast<int32_t>(value.size());
    req.flags = ZOO_SEQUENCE; // TODO
    req.acl = ZOO_OPEN_ACL_UNSAFE; // TODO ACL
    rc = rc < 0 ? rc : serialize_CreateRequest(oa, "req", &req);

    auto s = sock_.lock();
    assert (s);
    // send length first
    int totalLen = htonl(get_buffer_len(oa));
    s->SendPacket(&totalLen, sizeof totalLen);
    // send data body
    s->SendPacket(get_buffer(oa), get_buffer_len(oa));

    close_buffer_oarchive(&oa, 1);
}

int ZookeeperConn::_GetXid() const
{
    return ++ xid_; 
}

} // end namespace qedis

