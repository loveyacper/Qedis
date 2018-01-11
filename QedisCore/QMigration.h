#ifndef BERT_QMIGRATION_H
#define BERT_QMIGRATION_H

#include <deque>
#include "QClient.h"

namespace ConnectionTag
{
const int kMigrateClient = 4;
}

namespace qedis
{

enum class MigrateState
{
    None,
    Processing,
    Timeout,
    Done,
};

struct MigrationItem
{
    SocketAddr dst;
    int dstDb = 0;
    time_t timeout = 0;
    bool copy = false;
    bool replace = false;
    std::vector<QString> keys;
    std::weak_ptr<StreamSocket> client;

    MigrateState state = MigrateState::None;
};

class QMigrateClient;

class QMigrationManager
{
    friend class QMigrateClient;
public:
    static QMigrationManager& Instance();
    
    void Add(const MigrationItem& item);
    void Add(MigrationItem&& item);

    void InitMigrationTimer();
    void LoopCheck();

    std::shared_ptr<QMigrateClient> GetConnection(const SocketAddr& dst);
    void OnConnectMigrateFail(const SocketAddr& dst);
    void OnConnect(QMigrateClient* );

private:
    QMigrationManager() { }

    std::unordered_map<SocketAddr, std::deque<MigrationItem> > items_;
    std::unordered_map<SocketAddr, std::weak_ptr<QMigrateClient> > conns_;

    std::unordered_set<SocketAddr> pendingConnect_;
};

class QMigrateClient : public StreamSocket
{
public:
    void OnConnect() override;

private:
    PacketLength _HandlePacket(const char* msg, std::size_t len) override;
    int readyRsp_ = 0;
};

} // end namespace qedis

#endif

