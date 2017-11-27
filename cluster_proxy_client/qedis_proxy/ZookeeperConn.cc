#include <iostream>
#include <sys/time.h>

#include "net/Connection.h"
#include "net/EventLoop.h"
#include "util/Util.h"

#include "ZookeeperConn.h"
#include "ProxyConfig.h"
#include "ClusterManager.h"

ZookeeperConn::ZookeeperConn(ananas::Connection* c) :
    conn_(c)
{
    ctx_.reset(new qedis::ZookeeperContext(c));
}
    
ZookeeperConn::~ZookeeperConn()
{
}

bool ZookeeperConn::OnData(const char*& data, size_t len)
{
    return ctx_->ParseMessage(data, len);
}

ananas::Try<qedis::ZookeeperContext* >
ZookeeperConn::_ProcessHandshake(const ZkResponse& rsp) 
{
    if (ctx_->ProcessHandshake(rsp))
    {
        std::cout << "DoHandshake succ\n";
        return ananas::Try<qedis::ZookeeperContext* >(ctx_.get());
    }
    else
    {
        conn_->ActiveClose();
        auto exptr = std::make_exception_ptr(std::runtime_error("ProcessHandshake failed"));
        return ananas::Try<qedis::ZookeeperContext* >(exptr);
    }
}

ananas::Future<std::vector<ananas::Try<ZkResponse>>>
ZookeeperConn::_RegisterAndGetServers(ananas::Try<qedis::ZookeeperContext* >&& tctx)
{
    std::vector<ananas::Future<ZkResponse> > futures;
    try
    {
        qedis::ZookeeperContext* ctx = tctx;
                    
        if (!ctx->IsResumed())
        {
            const std::string& data = qedis::g_config.bindAddr;
            const std::string& path = qedis::ProxyConfig::kProxyPrefixPath;
            auto fut = ctx->CreateNode(true, true, &data, &path);
            futures.emplace_back(std::move(fut));
        }

        // 3. get qedis server's set and watch the set's list
        auto fut = ctx->GetChildren2(qedis::ProxyConfig::kQedisSetsPath, true);
        futures.emplace_back(std::move(fut));
    }
    catch (const std::exception& e)
    {
        std::cout << "OnConnect exception " << e.what() << std::endl;
    }

    return ananas::WhenAll(std::begin(futures), std::end(futures));
}

ananas::Future<std::vector<ananas::Try<ZkResponse>>>
ZookeeperConn::_GetShardingInfo(const std::vector<ananas::Try<ZkResponse>>& rsps)
{
    std::vector<ananas::Future<ZkResponse> > futures;
    if (!rsps.empty())
    {
        const ZkResponse& sets = rsps.back();
        ctx_->ProcessGetChildren2(sets);

        auto grsp = AnyCast<ChildrenRsp>(sets);
        for (const auto& child : grsp->children)
        {
            std::cout << "Try get data " << qedis::ProxyConfig::kQedisSetsPath + "/" + child << std::endl;
            auto fut = ctx_->GetData(qedis::ProxyConfig::kQedisSetsPath + "/" + child, true);
            futures.emplace_back(std::move(fut));
        }
    }

    return ananas::WhenAll(std::begin(futures), std::end(futures));
}

// /servers/set-1
static int GetSetID(const std::string& path)
{
    auto pos = path.find_last_of('-');
    if (pos == std::string::npos)
        return -1;

    std::string number(path.substr(pos + 1));
    return std::stoi(number);
}

bool ZookeeperConn::_ProcessShardingInfo(const std::vector<ananas::Try<ZkResponse>>& vrsp)
{
    for (const auto& rsp : vrsp)
    {
        ctx_->ProcessGetData(rsp);

        //ZkResponse rsp2 = rsp;
        auto drsp = AnyCast<DataRsp>(rsp.Value());
        
        // /servers/set-1
        int setid = GetSetID(drsp->path);
        if (setid < 0)
        {
            std::cout << "Wrong setid " << setid << std::endl;
            return false;
        }
            
        std::cout << "Path = " << drsp->path << ", setid " << setid << std::endl;
        std::cout << "Value = " << drsp->data << std::endl;
        
        std::vector<std::string> shardings = ananas::SplitString(drsp->data, ',');
        ClusterManager::Instance().AddShardingInfo(setid, shardings);
    }

    return true;
}
    
ananas::Future<std::vector<ananas::Try<ZkResponse>>>
ZookeeperConn::_GetServers(const std::vector<ananas::Try<ZkResponse>>& vrsp)
{
    std::vector<ananas::Future<ZkResponse> > futures;

    for (const auto& rsp : vrsp)
    {
        ctx_->ProcessGetData(rsp);

        auto drsp = AnyCast<DataRsp>(rsp.Value());
        
        // /servers/set-1
        auto fut = ctx_->GetChildren2(drsp->path, true);
        futures.emplace_back(std::move(fut));
    }

    return ananas::WhenAll(std::begin(futures), std::end(futures));
}

static std::string GetNodeAddr(const std::string& path)
{
    // /servers/set-{setid}/qedis(ip:port)-xxxseq
    auto start = path.find_first_of('(');
    auto end = path.find_first_of(')');
    if (start == std::string::npos ||
        end == std::string::npos)
        return std::string();

    return path.substr(start + 1, end - start - 1);
}


bool ZookeeperConn::_ProcessServerInfo(const std::vector<ananas::Try<ZkResponse>>& vrsp)
{
    for (const auto& rsp : vrsp)
    {
        ctx_->ProcessGetChildren2(rsp);

        auto crsp = AnyCast<ChildrenRsp>(rsp.Value());
            
        std::cout << "Parent& " << crsp->parent << std::endl;

        int setid = GetSetID(crsp->parent);
        for (const auto& child : crsp->children)
        {
            // qedis(127.0.0.1:6379)-0000000215
            std::string data = GetNodeAddr(child);
            std::cout << "child " << data << std::endl;
            ClusterManager::Instance().AddServerInfo(setid, data);
        }
    }

    return true;
}

void ZookeeperConn::_InitPingTimer() 
{
    auto doPing = [conn = conn_, ctx = ctx_.get()]()
                  {
                      timeval now;
                      ::gettimeofday(&now, nullptr);
                      long lastPing = now.tv_sec * 1000;
                      lastPing += now.tv_usec / 1000;
                      auto processPing = std::bind(&qedis::ZookeeperContext::ProcessPing, ctx, lastPing, std::placeholders::_1);
                      ctx->Ping().Then(processPing);
                  };

    pingId_ = conn_->GetLoop()->ScheduleAfter<ananas::kForever>(std::chrono::seconds(3), doPing);
}
    
void ZookeeperConn::OnConnect()
{
    ctx_->DoHandshake()
        .Then([me = this](const ZkResponse& rsp) mutable {
            // 1. Handshake with zookeeper
            return me->_ProcessHandshake(rsp);
        })
        .Then([me = this](ananas::Try<qedis::ZookeeperContext* >&& tctx) mutable {
            // 2. Register me and get redis sets' info
            return me->_RegisterAndGetServers(std::move(tctx));
        })
        .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& rsps) mutable {
            // 3. Get the qedis sets's sharding info
            return me->_GetShardingInfo(rsps);
        })
        .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& vrsp) mutable {
            // 4. store qedis sets' sharding info
            if (!me->_ProcessShardingInfo(vrsp)) {
                using InnerType = std::vector<ananas::Try<ZkResponse>>;
                auto exp = std::runtime_error("ProcessShardingInfo failed");
                return ananas::MakeExceptionFuture<InnerType>(exp);
            }

            // 5. get qedis server's list and watch the qedis server list
            return me->_GetServers(vrsp);
        })
        .Then([me = this](const std::vector<ananas::Try<ZkResponse> >& vrsp) mutable {
            // 6. store qedis servers' info
            return me->_ProcessServerInfo(vrsp);
        })
        .Then([me = this](bool succ) {
            // 7. Init ping timer
            if (succ) {
                me->_InitPingTimer();
            }
        })
        .OnTimeout(std::chrono::seconds(5), []() {
                std::cout << "OnTimeout handshake\n";
                ananas::EventLoop::ExitApplication();
            }, conn_->GetLoop()
        );
}

void ZookeeperConn::OnDisconnect()
{
    if (pingId_)
        conn_->GetLoop()->Cancel(pingId_);
}

