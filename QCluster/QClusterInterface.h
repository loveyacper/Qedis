#ifndef BERT_CLUSTERINTERFACE_H
#define BERT_CLUSTERINTERFACE_H

#if QEDIS_CLUSTER

//#include <memory>
#include "StreamSocket.h"

namespace qedis
{

class QClusterConn
{ 
public:
    QClusterConn(const std::shared_ptr<StreamSocket>& c) : sock_(c)
    {
    }

    virtual ~QClusterConn()
    {
    }

protected:
    std::weak_ptr<StreamSocket> sock_;

public:
    virtual bool ParseMessage(const char*& data, size_t len) = 0;
    virtual void OnConnect() = 0;
    virtual void RunForMaster(int setid, const std::string& val) = 0;
};

} // end namespace qedis

#endif // endif QEDIS_CLUSTER

#endif // endif BERT_CLUSTERINTERFACE_H

