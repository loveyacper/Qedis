//
//  qedis.h
//
//  Created by Bert Young on 16-1-22.
//  Copyright (c) 2016年 Bert Young. All rights reserved.
//

#include "QString.h"
#include "Server.h"

#define QEDIS_VERSION "0.7.0"

class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
    bool  ParseArgs(int ac, char* av[]);
    const char* GetRunId() const { return runid_.get(); }
    const qedis::QString& GetConfigName() const { return cfgFile_; }

private:
    std::shared_ptr<StreamSocket>  _OnNewConnection(int fd) override;
    bool    _Init() override;
    bool    _RunLogic() override;
    void    _Recycle() override;
    
    qedis::QString cfgFile_;
    unsigned short port_;
    qedis::QString logLevel_;
    
    qedis::QString master_;
    unsigned short masterPort_;
    
    static const unsigned kRunidSize;
    std::unique_ptr<char []> runid_;
};
