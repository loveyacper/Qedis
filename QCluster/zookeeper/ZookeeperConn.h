#ifndef BERT_ZOOKEEPERCONN_H
#define BERT_ZOOKEEPERCONN_H

#include "../QClusterInterface.h"

namespace qedis
{

class ZookeeperConn : public QClusterConn
{
public:
    ZookeeperConn(const std::shared_ptr<StreamSocket>& c);

    bool ParseMessage(const char*& data, size_t len) override;
    void OnConnect() override;
    void RunForMaster(int setid, const std::string& val) override;

private:
    // last zxid, session id & password
    // my qedis-node name
private:
    int _GetXid() const;
    mutable int xid_;
};

} // end namespace qedis

#endif //endif BERT_ZOOKEEPERCONN_H

