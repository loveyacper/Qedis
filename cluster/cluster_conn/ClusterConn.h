#ifndef BERT_CLUSTERCONN_H
#define BERT_CLUSTERCONN_H

namespace qedis
{

class ClusterConn
{ 
public:
    virtual ~ClusterConn()
    {
    }

public:
    virtual bool OnData(const char*& data, size_t len) = 0;
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
};

} // end namespace qedis

#endif // endif BERT_CLUSTERCONN_H

