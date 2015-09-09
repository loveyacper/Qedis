#ifndef BERT_QCONFIG_H
#define BERT_QCONFIG_H

#include "QString.h"

struct QConfig
{
    bool      daemonize;
    QString   pidfile;
    
    unsigned short  port;
    
    int       timeout;
    
    QString   loglevel;
    QString   logdir;  // the log directory, differ from redis
    
    int       databases;

    // @ rdb
    // save seconds changes
    int       saveseconds;
    int       savechanges;
    bool      rdbcompression;   // yes
    bool      rdbchecksum;      // yes
    QString   rdbfilename;      // dump.rdb
    QString   rdbdir;           // ./
    
    int       maxclients;       // 10000
    
    bool      appendonly;       // no
    QString   appendfilename;   // appendonly.aof
    int       appendfsync;      // no, everysec, always
    
    int       slowlogtime;      // 1000 microseconds
    int       slowlogmaxlen;    // 128
    
    int       hz;               // 10  [1,500]
    
    QString   includefile;      // the template config
    
    QConfig();

    bool      CheckArgs() const;
};

extern  QConfig  g_config;

extern bool  LoadQedisConfig(const char* cfgFile, QConfig& cfg);

#endif

