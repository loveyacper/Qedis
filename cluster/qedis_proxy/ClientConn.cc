#include <cassert>
#include <algorithm>

#include "ProxyLog.h"
#include "Command.h"
#include "ClientConn.h"

#include "ClusterManager.h"
#include "QedisManager.h"
#include "QedisConn.h"

#include "net/EventLoop.h"
#include "util/Util.h"

static 
ananas::PacketLen_t ProcessInlineCmd(const char* buf,
                                     size_t bytes,
                                     std::vector<std::string>& params)
{
    if (bytes < 2)
        return 0;

    std::string res;

    for (size_t i = 0; i + 1 < bytes; ++ i)
    {
        if (buf[i] == '\r' && buf[i+1] == '\n')
        {
            if (!res.empty())
                params.emplace_back(std::move(res));

            return static_cast<ananas::PacketLen_t>(i + 2);
        }

        if (isblank(buf[i]))
        {
            if (!res.empty())
            {
                params.reserve(4);
                params.emplace_back(std::move(res));
            }
        }
        else
        {
            res.reserve(16);
            res.push_back(buf[i]);
        }
    }

    return 0;
}


ClientConn::ClientConn(ananas::Connection* conn) :
    hostConn_(conn)
{
}

ananas::PacketLen_t ClientConn::OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    assert (conn == hostConn_);

    const char* const end = data + len;
    const char* ptr = data;

    auto parseRet = proto_.ParseRequest(ptr, end);
    if (parseRet == ParseResult::error)
    {   
        if (!proto_.IsInitialState())
        {
            ERR(g_logger) << "ParseError for " << data;
            hostConn_->ActiveClose();
            return 0;
        }

        // try inline command
        std::vector<std::string> params;
        auto consumed = ProcessInlineCmd(ptr, len, params); 
        if (consumed == 0)
            return 0;

        proto_.GetParams() = params;
        proto_.GetRawRequest().assign(ptr, ptr + consumed);

        ptr += consumed;
        parseRet = ParseResult::ok;
    }
    
    if (parseRet != ParseResult::ok) 
    { 
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }
    
    ANANAS_DEFER {
        proto_.Reset();
    };

    // handle request packet
    const auto& params = proto_.GetParams();
    assert (!params.empty());

    std::string cmd(params[0]);
    std::transform(params[0].begin(), params[0].end(), cmd.begin(), ::tolower);

    const CommandInfo* info = CommandTable::GetCommandInfo(cmd);
    if (!info)
    {
        const auto& e = g_errorInfo[QError_unknowCmd];
        hostConn_->SendPacket(e.errorStr, e.len);
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }

    if (info->handler)
    {
        std::string reply(info->handler(params));
        hostConn_->SendPacket(reply.data(), reply.size());
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }

    const auto& host = ClusterManager::Instance().GetServer(params[1]);
    if (host.empty())
    {
        const auto& e = g_errorInfo[QError_notready];
        hostConn_->SendPacket(e.errorStr, e.len);
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }
            
    // Forwart request to Qedis `host`
    const auto& rawReq = proto_.GetRawRequest();
    QedisManager::Instance().GetConnection(host)
        .Then([rawReq, conn = hostConn_](ananas::Try<QedisConn* >&& qconn) {
            try {
                QedisConn* qc = qconn;
                return qc->ForwardRequest(rawReq);
            }
            catch (const std::exception& e) {
                ERR(g_logger) << "GetServer with exception " << e.what();
                const auto& info = g_errorInfo[QError_dead];
                return ananas::MakeReadyFuture<std::string>(std::string(info.errorStr, info.len));
            }
        })
        .Then([conn](const std::string& reply) {
            DBG(g_logger) << "Send reply " << reply;
            conn->SendPacket(reply.data(), reply.size());
        });

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

