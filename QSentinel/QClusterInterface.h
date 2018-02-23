#ifndef BERT_CLUSTERINTERFACE_H
#define BERT_CLUSTERINTERFACE_H

#if QEDIS_CLUSTER

#include <vector>
#include <set>
#include <string>
#include <unordered_map>
#include <functional>

struct SocketAddr;

namespace ConnectionTag
{
   const int kSlaveClient = 3;
}

namespace qedis
{

class QClusterConn
{ 
public:
    using MasterInitCallback = std::function<void (const std::vector<SocketAddr>&)>;
    using MigrationCallback = std::function<void (const std::unordered_map<SocketAddr, std::set<int>>& )>;

    virtual ~QClusterConn()
    {
    }

    void SetOnBecomeMaster(MasterInitCallback cb)
    {
        onBecomeMaster_ = std::move(cb);
    }

    void SetOnMigration(MigrationCallback cb)
    {
        onMigration_ = std::move(cb);
    }

    void SetOnBecomeSlave(std::function<void (const std::string& )> cb)
    {
        onBecomeSlave_ = std::move(cb);
    }

public:
    virtual bool ParseMessage(const char*& data, size_t len) = 0;
    virtual void OnConnect() = 0;
    virtual void OnDisconnect() = 0;
    virtual void UpdateShardData() = 0;

protected:
    MasterInitCallback onBecomeMaster_;
    MigrationCallback onMigration_;
    std::function<void (const std::string& )> onBecomeSlave_;

};

} // end namespace qedis

#endif // endif QEDIS_CLUSTER

#endif // endif BERT_CLUSTERINTERFACE_H

