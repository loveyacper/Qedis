#include <iostream>
#include <sys/time.h>
#include "ZookeeperProxyConn.h"
#include "net/Connection.h"
#include "net/EventLoop.h"

ZookeeperProxyConn::ZookeeperProxyConn(ananas::Connection* c) :
    conn_(c)
{
    ctx_.reset(new qedis::ZookeeperContext(c));
}
    
ZookeeperProxyConn::~ZookeeperProxyConn()
{
}

bool ZookeeperProxyConn::OnData(const char*& data, size_t len)
{
    return ctx_->ParseMessage(data, len);
}

void ZookeeperProxyConn::OnConnect()
{
    ctx_->DoHandshake()
        .Then([ctx = ctx_.get(), c = conn_](const ZkResponse& rsp) mutable {
            if (!ctx->ProcessHandshake(rsp.handshakeRsp)) {
                c->ActiveClose();
                return (qedis::ZookeeperContext*)nullptr;
            }
            std::cout << "DoHandshake succ\n";

            return ctx;
        })
        .Then([](qedis::ZookeeperContext* ctx) mutable {
            std::vector<ananas::Future<ZkResponse> > futures;
            if (ctx && !ctx->IsResumed()) {
                // register me
                // TODO set data
                const std::string data = "127.0.0.1:3679";
                const std::string path = "/proxy/qedis_proxy_";
                auto fut = ctx->CreateNode(true, true, &data, &path); // tmp seq for me // 先获取set列表，再获取set下的qedis列表
                futures.emplace_back(std::move(fut));

                auto fut = ctx->GetChildren2("/servers");
                futures.emplace_back(std::move(fut));
            }

            return ananas::WhenAll(std::begin(futures), std::end(futures));
        })
        .Then([ctx = ctx_.get()](const std::vector<ananas::Try<ZkResponse> >& rsps) mutable {
            std::cout << "GetChildren2 response\n";
            if (!rsps.empty())
                ctx->ProcessGetChildren2(rsps.back());
            return ctx;
        }) //  Store Qedis info
        .Then([conn = conn_](qedis::ZookeeperContext* ctx) {
            conn->GetLoop()->ScheduleAfter<ananas::kForever>(
                std::chrono::seconds(7),
                [conn, ctx]() {
                    timeval now;
                    ::gettimeofday(&now, nullptr);
                    long lastPing = now.tv_sec * 1000;
                    lastPing += now.tv_usec / 1000;
                    auto processPing = std::bind(&qedis::ZookeeperContext::ProcessPing, ctx, lastPing, std::placeholders::_1);
                    ctx->Ping().Then(processPing);
            });

            return conn->GetLoop();
        }) // Init Ping Timer
        .Then([](ananas::EventLoop* loop) {
            if (loop->Listen("127.0.0.1", 6379, [](ananas::Connection* c) {}))
                std::cout << "Listen succ\n";
        })
        .OnTimeout(std::chrono::seconds(3), []() {
                std::cout << "OnTimeout handshake\n";
                ananas::EventLoop::ExitApplication();
            }, conn_->GetLoop()
        );
}

void ZookeeperProxyConn::OnDisconnect()
{
}

