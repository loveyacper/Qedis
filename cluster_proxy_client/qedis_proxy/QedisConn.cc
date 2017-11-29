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
    hostConn_->SendPacket(buf.data(), buf.size());

    QedisConn::Request req;
    req.request = params;

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
#if 0
    std::cout << "--- Request: [ ";
    for (const auto& arg : req.request)
        std::cout << arg << " ";
    std::cout << "]\n--- Response --\n";
    std::cout << proto_.GetParam() << "\n";
#endif

    req.promise.SetValue(proto_.GetParam());
    pending_.pop();

    proto_.Reset();

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

