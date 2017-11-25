#ifndef BERT_ZKRESPONSE_H
#define BERT_ZKRESPONSE_H

#include "zookeeper.jute.h"
#include "proto.h"

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

#endif //BERT_ZKRESPONSE_H

