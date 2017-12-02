#include <cassert>
#include "ProxyLog.h"
#include "ClientConn.h"

#include "ClusterManager.h"
#include "QedisManager.h"
#include "QedisConn.h"

#include "net/EventLoop.h"

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

        proto_.SetParams(params);
        proto_.SetParam(std::string(ptr, ptr + consumed));

        ptr += consumed;
        parseRet = ParseResult::ok;
    }
    
    if (parseRet != ParseResult::ok) 
    { 
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }
    
    const auto& params = proto_.GetParams();
    if (params.size() <= 1)
    {
        // TODO filter command
        static const std::string kError = "-ERR No such command\r\n";
        hostConn_->SendPacket(kError.data(), kError.size());
    }
    else
    {
        const std::string host = ClusterManager::Instance().GetServer(params[1]);
        if (host.empty())
        {
            // TODO error message
            static const std::string kError = "-ERR Server not ready\r\n";
            hostConn_->SendPacket(kError.data(), kError.size());
        }
        else
        {
            // Forwart request to Qedis `host`
            const auto& rawReq = proto_.GetRawRequest();
            QedisManager::Instance().GetConnection(host)
                .Then([rawReq, conn = hostConn_](ananas::Try<QedisConn* >&& qconn) {
                    try {
                        QedisConn* qc = qconn;
                        auto rspFuture = qc->ForwardRequest(rawReq);
                        rspFuture.OnTimeout(std::chrono::seconds(3), [conn]() {
                                const std::string kTimeout = "-ERR Server response timeout\r\n";
                                conn->SendPacket(kTimeout.data(), kTimeout.size());
                            }, conn->GetLoop()
                        );
                        return rspFuture;
                    }
                    catch (const std::exception& e) {
                        ERR(g_logger) << "GetServer with exception " << e.what();
                        static const std::string kError = "-ERR Server not alive\r\n";
                        return ananas::MakeReadyFuture<std::string>(kError);
                    }
                })
                .Then([conn](const std::string& reply) {
                    DBG(g_logger) << "Send reply " << reply;
                    conn->SendPacket(reply.data(), reply.size());
                })
                .OnTimeout(std::chrono::seconds(3), [conn]() {
                        const std::string kTimeout = "-ERR Internal server connect timeout\r\n";
                        conn->SendPacket(kTimeout.data(), kTimeout.size());
                    }, hostConn_->GetLoop()
                );
        }
    }

    proto_.Reset();

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

