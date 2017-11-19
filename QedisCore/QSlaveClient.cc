#include "QSlaveClient.h"
#include "QConfig.h"
#include "QCommon.h"
#include "Log/Logger.h"

namespace qedis
{

void QSlaveClient::OnConnect()
{
    std::string cmd = BuildInlineRequest("slaveof ", g_config.ip, std::to_string(g_config.port));
    INF << "Send to slave cmd " << cmd;

    SendPacket(cmd.data(), cmd.size());
    
    auto wk = std::weak_ptr<QSlaveClient>(std::static_pointer_cast<QSlaveClient>(this->shared_from_this()));
    Timer* timer = TimerManager::Instance().CreateTimer();
    timer->Init(3 * 1000, 1);
    timer->SetCallback([wk]() {
        auto me = wk.lock();
        if (me) {
            USR << "OnTimer close " << me->GetPeerAddr().ToString();
            me->OnError();
        }
    });
    TimerManager::Instance().AsyncAddTimer(timer);
}
    
PacketLength QSlaveClient::_HandlePacket(const char* msg, std::size_t len)
{
    return static_cast<PacketLength>(len);
}

}

