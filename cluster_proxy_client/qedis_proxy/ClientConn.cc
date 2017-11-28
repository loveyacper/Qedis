#include <cassert>
#include "ProxyLog.h"
#include "ClientConn.h"

#define CRLF "\r\n"

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
    const char* const end = data + len;
    const char* ptr = data;

    auto parseRet = proto_.ParseRequest(ptr, end);
    if (parseRet == ParseResult::error)
    {   
        if (!proto_.IsInitialState())
        {
            ERR(g_logger) << "ParseError for " << data;
            // error protocol
            hostConn_->ActiveClose();
            return 0;
        }

        // try inline command
        std::vector<std::string> params;
        auto consumed = ProcessInlineCmd(ptr, len, params); 
        if (consumed == 0)
            return 0;

        ptr += consumed;
        proto_.SetParams(params);
        parseRet = ParseResult::ok;
    }
    else if (parseRet != ParseResult::ok) 
    { 
        // wait
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }
    
    assert (parseRet == ParseResult::ok);

#if 0
    // TODO handle packet 
    // 1. calc hash
    qedisMgr.Forward(params)
        .Then([](const QedisResponse& rsp) {
            // rsp:  : +-  *   $

        });
    const auto& params = proto_.GetParams(); 
    if (params.empty()) 
        return static_cast<ananas::PacketLen_t>(ptr - data);
#endif
    proto_.Reset();

    // pseudo reply
    {
        std::string reply = "-ERR ";
        reply += hostConn_->Peer().ToString();
        reply += "\r\n";
        hostConn_->SendPacket(reply.data(), reply.size());
    }

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

// helper
static size_t FormatBulk(const char* str, size_t len, std::string* reply)
{
    size_t oldSize = reply->size();
    (*reply) += '$';

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, len);
    reply->append(val, tmp);

    if (str && len > 0)
        reply->append(str, len);
        
    reply->append(CRLF, 2);
    return reply->size() - oldSize;
}

