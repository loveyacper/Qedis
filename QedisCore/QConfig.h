#ifndef BERT_QCONFIG_H
#define BERT_QCONFIG_H

#include <map>
#include <vector>
#include "QString.h"

namespace qedis
{

enum BackEndType
{
    BackEndNone = 0,
    BackEndLeveldb = 1,
    BackEndRocksdb = 2,
    BackEndMax = 3,
};

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

    int backend; // enum BackEndType
    QString backendPath; 
    int backendHz; // the frequency of dump to backend

    // rocksdb
    size_t write_buffer_size;
    int level0_file_num_compaction_trigger;
    int level0_slowdown_writes_trigger;
    int level0_stop_writes_trigger;
    int max_write_buffer_number;
    int min_write_buffer_number_to_merge;
    int max_background_jobs;
    uint32_t max_subcompactions;
    int max_open_files;
    bool enable_pipelined_write;
    size_t max_log_file_size;
    uint64_t max_manifest_file_size;
    bool unordered_write; // Since RocksDB 6.3
    bool two_write_queues; // Since RocksDB 6.3

    // cluster
    bool enableCluster = false;
    std::vector<QString> centers;
    int setid = -1; // sharding set id
    
    QConfig();

    bool CheckArgs() const;
    bool CheckPassword(const QString& pwd) const;
};

extern  QConfig  g_config;

extern bool  LoadQedisConfig(const char* cfgFile, QConfig& cfg);

}

#endif

