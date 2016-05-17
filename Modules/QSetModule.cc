#include "QSetModule.h"
#include "QSet.h"
#include "QStore.h"
#include "QGlobRegex.h"

using namespace qedis;

QError skeys(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;
    QError err = QSTORE.GetValueByType(params[1], value, QType_set);
    if (err != QError_ok) 
    {  
        if (err == QError_notExist)
            FormatNull(reply);
        else
            ReplyError(err, reply);

        return err;
    }

    std::vector<const QString* > res;
    const PSET& set = value->CastSet();
    for (const auto& k : *set)
    {
        if (glob_match(params[2], k))
        {
            res.push_back(&k);
        }
    }

    PreFormatMultiBulk(res.size(), reply);
    for (auto v : res)
    {
        FormatSingle(*v, reply);
    }

    return   QError_ok;
}


