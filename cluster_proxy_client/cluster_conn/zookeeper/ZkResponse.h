#ifndef BERT_ZKRESPONSE_H
#define BERT_ZKRESPONSE_H

#include "zookeeper.jute.h"
#include "proto.h"

using ZkResponse = std::shared_ptr<void>;

template <typename T>
inline
std::shared_ptr<T> NewResponse()
{
    return std::make_shared<T>();
}

template <typename T>
inline std::shared_ptr<T> AnyCast(const ZkResponse& rsp)
{
    return std::static_pointer_cast<T>(rsp);
}

struct CreateRsp
{
    std::string path;
};

inline
CreateRsp Convert(const CreateResponse& rsp)
{
    CreateRsp crsp;
    crsp.path = rsp.path;
    return crsp;
}

struct ChildrenRsp
{
    std::string parent;
    std::vector<std::string> children;
    struct Stat stat;
};

inline
ChildrenRsp Convert(const std::string& parent, const GetChildren2Response& children)
{
    ChildrenRsp rsp;
    rsp.parent = parent;

    for (int i = 0; i < children.children.count; ++ i)
    {
        rsp.children.push_back(children.children.data[i]);
    }

    rsp.stat = children.stat;
    return rsp;
}

struct DataRsp
{
    std::string path;
    std::string data;
    struct Stat stat;
};

inline
DataRsp Convert(const std::string& path, const GetDataResponse& rsp)
{
    DataRsp drsp;
    drsp.path = path;
    drsp.data.assign(rsp.data.buff, rsp.data.len);
    drsp.stat = rsp.stat;
    return drsp;
}

#if 0
union ZkResponse
{
    prime_struct handshakeRsp;
    long recvPing;
    CreateResponse createRsp;

    struct 
    {
        struct buffer parent;
        GetChildren2Response children;
    } child2Rsp;

    struct {
        struct buffer path;
        GetDataResponse data;
    } dataRsp;
};
#endif

#endif //BERT_ZKRESPONSE_H

