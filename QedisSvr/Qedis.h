//
//  qedis.h
//
//  Created by Bert Young on 16-1-22.
//  Copyright (c) 2016年 Bert Young. All rights reserved.
//

#include "QString.h"
#include "Server.h"

#define QEDIS_VERSION "1.0.0"

class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
    bool  ParseArgs(int ac, char* av[]);
    const qedis::QString& GetConfigName() const { return cfgFile_; }

private:
    std::shared_ptr<StreamSocket> _OnNewConnection(int fd, int tag) override;
    bool    _Init() override;
    bool    _RunLogic() override;
    void    _Recycle() override;
    
    qedis::QString cfgFile_;
    unsigned short port_;
    qedis::QString logLevel_;
    
    qedis::QString master_;
    unsigned short masterPort_;

#if QEDIS_CLUSTER
    // cluster
    size_t clusterIndex_ = 0;
#endif
    
    static const unsigned kRunidSize;
};
