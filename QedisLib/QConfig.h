#ifndef BERT_QCONFIG_H
#define BERT_QCONFIG_H

#include <map>
#include "QString.h"

namespace qedis
{

struct QConfig
{
    bool      daemonize;
    QString   pidfile;
    
    QString   ip;
    unsigned short  port;
    
    int       timeout;
    
    QString   loglevel;
    QString   logdir;  // the log directory, differ from redis
    
    int       databases;
    
    // auth
    QString   password;
    
    std::map<QString, QString>   aliases;

    // @ rdb
    // save seconds changes
    int       saveseconds;
    int       savechanges;
    bool      rdbcompression;   // yes
    bool      rdbchecksum;      // yes
    QString   rdbfullname;      // ./dump.rdb
    
    int       maxclients;       // 10000
    
    bool      appendonly;       // no
    QString   appendfilename;   // appendonly.aof
    int       appendfsync;      // no, everysec, always
    
    int       slowlogtime;      // 1000 microseconds
    int       slowlogmaxlen;    // 128
    
    int       hz;               // 10  [1,500]
    
    QString   masterIp;
    unsigned short masterPort;  // replication
    QString   masterauth;
    
    QString   runid;

    QString   includefile;      // the template config

    std::vector<QString>  modules; // modules

    // use redis as cache, level db as backup
    uint64_t maxmemory; // default 2GB
    int maxmemorySamples; // default 5
    bool noeviction; // default true
    
    QConfig();

    bool CheckArgs() const;
    bool CheckPassword(const QString& pwd) const;
};

extern  QConfig  g_config;

extern bool  LoadQedisConfig(const char* cfgFile, QConfig& cfg);

}

#endif

