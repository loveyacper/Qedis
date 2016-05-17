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
    
    QString   includefile;      // the template config

    // TODO fix config parser to accept repeat keys
    QString   modules; // modules, seperated by space
    
    QConfig();

    bool      CheckArgs() const;
};

extern  QConfig  g_config;

extern bool  LoadQedisConfig(const char* cfgFile, QConfig& cfg);

}

#endif

