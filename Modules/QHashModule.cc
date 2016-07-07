#include "QHashModule.h"
#include "QHash.h"
#include "QStore.h"
#include "QGlobRegex.h"

using namespace qedis;

QError hgets(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_hash);
    if (err != QError_ok) 
    {  
        ReplyError(err, reply);
        return err;
    }
    
    const PHASH& hash= value->CastHash();
    std::vector<const QString* > res;
    for (const auto& kv : *hash)
    {
        if (glob_match(params[2], kv.first))
        {
            res.push_back(&kv.first);
            res.push_back(&kv.second);
        }
    }

    PreFormatMultiBulk(res.size(), reply);
    for (auto v : res)
    {
        FormatBulk(*v, reply);
    }

    return   QError_ok;
}

