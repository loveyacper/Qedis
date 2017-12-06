#include <algorithm>
#include <iostream>
#include "QedisConn.h"

QedisConn::QedisConn(ananas::Connection* conn) :
    hostConn_(conn)
{
}

static 
std::string BuildRequest(const std::vector<std::string>& params)
{
    assert (!params.empty());

    std::string req;
    for (const auto& e : params)
    {
        req += e + " ";
    }

    req.pop_back();
    req += "\r\n";

    return req;
}

ananas::Future<std::string>
QedisConn::ForwardRequest(const std::vector<std::string>& params)
{
    std::string buf = BuildRequest(params);
    return ForwardRequest(buf);
}

ananas::Future<std::string>
QedisConn::ForwardRequest(const std::string& rawReq)
{
    hostConn_->SendPacket(rawReq.data(), rawReq.size());

    QedisConn::Request req;

    auto fut = req.promise.GetFuture();
    pending_.push(std::move(req));

    return fut;
}
    
ananas::PacketLen_t QedisConn::OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    const char* ptr = data;
    auto parseRet = proto_.Parse(ptr, ptr + len);
    if (parseRet == ParseResult::error)
    {
        conn->ActiveClose();
        return 0;
    }
    else if (parseRet != ParseResult::ok) 
    {
        // wait
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }

    assert (parseRet == ParseResult::ok);
        
    auto& req = pending_.front();
    req.promise.SetValue(proto_.GetContent());
    pending_.pop();

    proto_.Reset();

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

