#ifndef BERT_QEDISCONN_H
#define BERT_QEDISCONN_H

#include <queue>
#include <string>
#include <vector>
#include "net/Connection.h"
#include "future/Future.h"
#include "ClientProtocol.h"

// Build redis request from multiple strings, use inline protocol 
template <typename... Args>
std::string BuildQedisRequest(Args&& ...);

template <typename S>
std::string BuildQedisRequest(S&& s)
{
    return std::string(std::forward<S>(s)) + "\r\n";
}

template <typename H, typename... T>
std::string BuildQedisRequest(H&& head, T&&... tails)
{
    std::string h(std::forward<H>(head));
    return h + " " + BuildQedisRequest(std::forward<T>(tails)...);
}

class QedisConn final
{
public:
    explicit
    QedisConn(ananas::Connection* conn);

    ananas::Future<std::string> 
    ForwardRequest(const std::vector<std::string>& params);

    // Qedis get command
    //ananas::Future<std::pair<ResponseType, std::string> > Get(const std::string& key);
    // Qedis set command
    //ananas::Future<std::pair<ResponseType, std::string> > Set(const std::string& key, const std::string& value);

    // Parse response
    ananas::PacketLen_t OnRecv(ananas::Connection* conn, const char* data, ananas::PacketLen_t len);

    //static void PrintResponse(const std::pair<ResponseType, std::string>& info);

private:
    ananas::Connection* hostConn_;

    struct Request
    {
        std::vector<std::string> request;
        ananas::Promise<std::string> promise;

        Request() = default;

        Request(Request const& ) = delete;
        void operator= (Request const& ) = delete;

        Request(Request&& ) = default;
        Request& operator= (Request&& ) = default;
    };

    std::queue<Request> pending_;

    ClientProtocol proto_;
};

#endif

