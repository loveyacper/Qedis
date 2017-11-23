#ifndef BERT_ZKRESPONSE_H
#define BERT_ZKRESPONSE_H

#include "zookeeper.jute.h"
#include "proto.h"

union ZkResponse
{
    prime_struct handshakeRsp;
    long recvPing;
    CreateResponse createRsp;
    GetChildren2Response child2Rsp;
    GetDataResponse dataRsp;
};

#endif //BERT_ZKRESPONSE_H

