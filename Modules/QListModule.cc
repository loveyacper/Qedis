#include "QListModule.h"
#include "QList.h"
#include "QStore.h"
#include <algorithm>

using namespace qedis;


QError ldel(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    QObject* value;

    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        Format0(reply);
        return err;
    }
        
    long idx;
    if (!Strtol(params[2].c_str(), params[2].size(), &idx))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
        
    const PLIST& list = value->CastList();
    const int size = static_cast<int>(list->size());
    if (idx < 0)
        idx += size;
    
    if (idx < 0 || idx >= size)
    {
        Format0(reply);
        return QError_nop;
    }
    
    if (2 * idx < size)
    {
        auto it = list->begin();
        std::advance(it, idx);
        list->erase(it);
    }
    else
    {
        auto it = list->rbegin();
        idx = size - 1 - idx;
        std::advance(it, idx);
        list->erase((++it).base());
    }

    if (list->empty())
        QSTORE.DeleteKey(params[1]);
    
    Format1(reply);
    return QError_ok;
}

