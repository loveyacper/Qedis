#include <algorithm>
#include <iostream>
#include "QedisConn.h"

QedisConn::QedisConn(ananas::Connection* conn) :
    hostConn_(conn)
{
}

// temp
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
    
#if 0
ananas::Future<std::pair<ResponseType, std::string> >
QedisContext::Get(const std::string& key)
{
    // Qedis inline protocol request
    std::string req_buf = BuildQedisRequest("get", key);
    hostConn_->SendPacket(req_buf.data(), req_buf.size());

    QedisContext::Request req;
    req.request.push_back("get");
    req.request.push_back(key);

    auto fut = req.promise.GetFuture();
    pending_.push(std::move(req));

    return fut;
}
    
ananas::Future<std::pair<ResponseType, std::string> >
QedisContext::Set(const std::string& key, const std::string& value)
{
    // Qedis inline protocol request
    std::string req_buf = BuildQedisRequest("set", key, value);
    hostConn_->SendPacket(req_buf.data(), req_buf.size());

    QedisContext::Request req;
    req.request.push_back("set");
    req.request.push_back(key);
    req.request.push_back(value);

    auto fut = req.promise.GetFuture();
    pending_.push(std::move(req));

    return fut;
}
#endif


ananas::PacketLen_t QedisConn::OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len)
{
    const char* ptr = data;
    auto parseRet = proto_.Parse(ptr, ptr + len);
    if (parseRet == CParseResult::error)
    {
        conn->ActiveClose();
        return 0;
    }
    else if (parseRet != CParseResult::ok) 
    {
        // wait
        return static_cast<ananas::PacketLen_t>(ptr - data); 
    }

    assert (parseRet == CParseResult::ok);
        
    auto& req = pending_.front();
    std::cout << "--- Request: [ ";
    for (const auto& arg : req.request)
        std::cout << arg << " ";
    std::cout << "]\n--- Response --\n";
    std::cout << proto_.GetParam() << "\n";

    req.promise.SetValue(proto_.GetParam());
    pending_.pop();

    proto_.Reset();

    return static_cast<ananas::PacketLen_t>(ptr - data);
}

