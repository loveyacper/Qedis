#include "QList.h"
#include "QStore.h"
#include "QClient.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;


QObject  CreateListObject()
{
    QObject  list(QType_list);
    list.value = std::make_shared<QList>();

    return std::move(list);
}

static QError  push(const vector<QString>& params, UnboundedBuffer& reply, ListPosition pos, bool createIfNotExist = true)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyError(err, reply);
            return err;
        }
        else if (createIfNotExist)
        {
            QObject  list(QType_list);
            list.value = std::make_shared<QList>();

            value = QSTORE.SetValue(params[1], list);
        }
        else
        {
            ReplyError(err, reply);
            return err;
        }
    }

    const PLIST& list = value->CastList();
    bool mayReady = list->empty();
    for (size_t i = 2; i < params.size(); ++ i)
    {
        if (pos == ListPosition::head)
            list->push_front(params[i]);
        else
            list->push_back(params[i]);
    }
    
    if (mayReady && !list->empty())
    {
        QSTORE.ServeClient(params[1], list);
    }
    
    FormatInt(static_cast<long>(list->size()), reply);
    return   QError_ok;
}


static QError  GenericPop(const QString& key, ListPosition pos, QString& result)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(key, value, QType_list);
    if (err != QError_ok)
    {
        return  err;
    }
    
    const PLIST&  list = value->CastList();
    if (list->empty())
    {
        QSTORE.DeleteKey(key);
        return QError_notExist;
    }

    if (pos == ListPosition::head)
    {
        result = std::move(list->front());
        list->pop_front();
    }
    else
    {
        result = std::move(list->back());
        list->pop_back();
    }
    
    if (list->empty())
    {
        QSTORE.DeleteKey(key);
    }
    
    return   QError_ok;
}

QError  lpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition::head);
}

QError  rpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition::tail);
}

QError  lpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition::head, false);
}

QError  rpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition::tail, false);
}

QError  lpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    QString  result;
    
    QError   err = GenericPop(params[1], ListPosition::head, result);
    switch (err)
    {
        case QError_ok:
            FormatBulk(result.c_str(), result.size(), reply);
            break;
            
        default:
            ReplyError(err, reply);
            break;
    }
    
    return  err;
}

QError  rpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    QString  result;
    
    QError   err = GenericPop(params[1], ListPosition::tail, result);
    switch (err)
    {
        case QError_ok:
            FormatBulk(result.c_str(), result.size(), reply);
            break;
            
        default:
            ReplyError(err, reply);
            break;
    }
    
    return  err;
}

static bool  _BlockClient( QClient* client, const QString& key, uint64_t timeout, ListPosition pos, const QString* dstList = 0)
{
    auto now = ::Now();
    
    if (timeout > 0)
        timeout += now;
    else
        timeout = std::numeric_limits<uint64_t>::max();
    
    return  QSTORE.BlockClient(key, client, timeout, pos, dstList);

}

static QError  _GenericBlockedPop(vector<QString>::const_iterator keyBegin,
                                  vector<QString>::const_iterator keyEnd,
                                  UnboundedBuffer& reply,
                                  ListPosition  pos, long timeout,
                                  const QString* target = nullptr,
                                  bool withKey = true)
{
    for (auto it(keyBegin); it != keyEnd; ++ it)
    {
        QString  result;
        QError   err = GenericPop(*it, pos, result);
        
        switch (err)
        {
            case QError_ok:
                if (withKey)
                {
                    PreFormatMultiBulk(2, reply);
                    FormatBulk(it->c_str(), it->size(), reply);
                }
                FormatBulk(result.c_str(), result.size(), reply);
                
                if (target)
                {
                    // fuck, the target process
                }
                return  err;
                
            case QError_type:
                ReplyError(err, reply);
                return  err;
                
            case QError_notExist:
                break;
                
            default:
                assert(!!!"Unknow error");
        }
    }
    
    // Do NOT block if in transaction
    if (QClient::Current()->IsFlagOn(ClientFlag_multi))
    {
        FormatNull(reply);
        return QError_ok;
    }
    
    // Put client to the wait-list
    for (auto it(keyBegin); it != keyEnd; ++ it)
    {
        _BlockClient(QClient::Current(), *it, timeout, pos, target);
    }
    
    return  QError_ok;
}

QError  blpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    long  timeout;
    if (!TryStr2Long(params.back().c_str(),
                     params.back().size(),
                     timeout))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    timeout *= 1000;
    
    return  _GenericBlockedPop(++ params.begin(), -- params.end(),
                               reply, ListPosition::head, timeout);
}

QError  brpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    long  timeout;
    if (!TryStr2Long(params.back().c_str(),
                     params.back().size(),
                     timeout))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    timeout *= 1000;
    
    return  _GenericBlockedPop(++ params.begin(), -- params.end(),
                               reply, ListPosition::tail, timeout);
}

QError  lindex(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatNull(reply);
        return  err;
    }
    
    const PLIST&    list   = value->CastList();
    long idx;
    if (!TryStr2Long(params[2].c_str(), params[2].size(), idx))
    {
        ReplyError(QError_nan, reply);
        return  QError_nan;
    }
    
    const int size = static_cast<int>(list->size());
    if (idx < 0)
        idx += size;
    
    if (idx < 0 || idx >= size)
    {
        FormatNull(reply);
        return  QError_ok;
    }
    
    const QString* result = 0;
    
    if (2 * idx < size)
    {
        QList::const_iterator it = list->begin();
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    else
    {
        QList::const_reverse_iterator  it = list->rbegin();
        idx = size - 1 - idx;
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    
    FormatSingle(result->c_str(), result->size(), reply);
    return   QError_ok;
}


QError  lset(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyError(QError_notExist, reply);
        return  err;
    }
    
    const PLIST&    list = value->CastList();
    long idx;
    if (!TryStr2Long(params[2].c_str(), params[2].size(), idx))
    {
        ReplyError(QError_param, reply);
        return  QError_notExist;
    }
    
    const int size = static_cast<int>(list->size());
    if (idx < 0)
        idx += size;
    
    if (idx < 0 || idx >= size)
    {
        FormatNull(reply);
        return  QError_ok;
    }
    
    QString* result = 0;
    
    if (2 * idx < size)
    {
        QList::iterator it = list->begin();
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    else
    {
        QList::reverse_iterator it = list->rbegin();
        idx = size - 1 - idx;
        while (idx -- > 0)
        {
            ++ it;
        }
        result = &*it;
    }
    
    *result = params[3];
    
    FormatOK(reply);
    return   QError_ok;
}


QError  llen(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        if (err == QError_type)
        {
            ReplyError(err, reply);
        }
        else
        {
            Format0(reply);
        }

        return  err;
    }
    
    const PLIST&    list   = value->CastList();
    FormatInt(static_cast<long>(list->size()), reply);
    return   QError_ok;
}

static void Index2Iterator(long start, long end,
                           QList&  list,
                           QList::iterator& beginIt,
                           QList::iterator& endIt)
{
    assert (start >= 0 && end >= 0 && start <= end);
    
    beginIt = list.begin();
    while (start -- > 0)   ++ beginIt;

    endIt  = list.begin();
    while (end -- > 0)  ++ endIt;
}

static size_t GetRange(long start, long end,
                       QList&  list,
                       QList::iterator& beginIt,
                       QList::iterator& endIt)
{
    size_t   rangeLen = 0;
    if (start > end)
    {
        beginIt = endIt = list.end();  // empty
    }
    else if (start != 0 || end + 1 != static_cast<long>(list.size()))
    {
        rangeLen = end - start + 1;
        Index2Iterator(start, end, list, beginIt, endIt);
    }
    else
    {
        rangeLen= list.size();
        beginIt = list.begin();
        endIt   = -- list.end();  // entire list
    }
    
    return rangeLen;
}


QError  ltrim(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return  err;
    }
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_param, reply);
        return err;
    }
    
    const PLIST&    list   = value->CastList();
    AdjustIndex(start, end, list->size());
    
    QList::iterator beginIt, endIt;
    GetRange(start, end, *list, beginIt, endIt);
    
    if (beginIt != list->end())
    {
        assert (endIt != list->end());
        list->erase(list->begin(), beginIt);
        list->erase(++ endIt, list->end());
    }
    
    FormatOK(reply);
    return   QError_ok;
}

QError  lrange(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        ReplyError(err, reply);
        return  err;
    }
    
    long start, end;
    if (!Strtol(params[2].c_str(), params[2].size(), &start) ||
        !Strtol(params[3].c_str(), params[3].size(), &end))
    {
        ReplyError(QError_param, reply);
        return err;
    }
    
    const PLIST&    list   = value->CastList();
    AdjustIndex(start, end, list->size());
    
    QList::iterator beginIt, endIt;
    size_t rangeLen = GetRange(start, end, *list, beginIt, endIt);
    
    PreFormatMultiBulk(rangeLen, reply);
    
    if (beginIt != list->end())
    {
        assert (endIt != list->end());

        while (true)
        {
            FormatBulk(beginIt->c_str(), beginIt->size(), reply);
            if (beginIt == endIt)
                break;
            
            ++ beginIt;
        }
    }
    
    return   QError_ok;
}

QError  linsert(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        Format0(reply);
        return err;
    }
    
    bool before = false;
    if (params[2] == "before")
        before = true;
    else if (params[2] == "after")
        before = false;
    else
    {
        ReplyError(QError_param, reply);
        return QError_param;
    }
    
    const PLIST&    list = value->CastList();
    QList::iterator it = std::find(list->begin(), list->end(), params[3]);
    if (it == list->end())
    {
        FormatInt(-1, reply);
        return QError_notExist;
    }
    
    if (before)
        list->insert(it, params[4]);
    else
        list->insert(++ it, params[4]);
    
    FormatInt(static_cast<long>(list->size()), reply);
    
    return   QError_ok;
}


QError  lrem(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        Format0(reply);
        return err;
    }
    
    long count;
    if (!Strtol(params[2].c_str(), params[2].size(), &count))
    {
        ReplyError(QError_param, reply);
        return err;
    }
    
    const PLIST&    list = value->CastList();
    
    ListPosition  start = ListPosition::head;
    
    if (count < 0)
    {
        count = -count;
        start = ListPosition::tail;
    }
    else if (count == 0)
    {
        count = list->size(); // remove all elements equal to param[3]
    }
    
    long resultCount = 0;
    if (start == ListPosition::head)
    {
        QList::iterator it = list->begin();
        while (it != list->end() && resultCount < count)
        {
            if (*it == params[3])
            {
                list->erase(it ++);
                ++ resultCount;
            }
            else
            {
                ++ it;
            }
        }
    }
    else
    {
        QList::reverse_iterator it = list->rbegin();
        while (it != list->rend() && resultCount < count)
        {
            if (*it == params[3])
            {
                list->erase((++it).base()); // Effective STL, item 28
                ++ resultCount;
            }
            else
            {
                ++ it;
            }
        }
    }

    FormatInt(resultCount, reply);
    return   QError_ok;
}

static QError  _GenericRpoplpush(const vector<QString>& params, UnboundedBuffer& reply, bool block = false)
{
    QObject* src;
    
    QError err = QSTORE.GetValueByType(params[1], src, QType_list);
    if (err != QError_ok)
    {
        if (!block)
        {
            FormatNull(reply);
            return err;
        }
        else
        {
            ;
        }
    }
    
    QObject* dst;
    
    err = QSTORE.GetValueByType(params[2], dst, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyError(err, reply);
            return err;
        }
        QObject  dstObj(QType_list);
        dstObj.value = std::make_shared<QList>();
        dst = QSTORE.SetValue(params[2], dstObj);
    }
    
    const PLIST& srclist = src->CastList();
    const PLIST& dstlist = dst->CastList();
    
    dstlist->splice(dstlist->begin(), *srclist, (++ srclist->rbegin()).base());
    
    FormatBulk(dstlist->begin()->c_str(),
               dstlist->begin()->size(),
               reply);
    
    return   QError_ok;
}

QError  rpoplpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    QObject* src;
    
    QError err = QSTORE.GetValueByType(params[1], src, QType_list);
    if (err != QError_ok)
    {
        FormatNull(reply);
        return err;
    }
    
    QObject* dst;
    
    err = QSTORE.GetValueByType(params[2], dst, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyError(err, reply);
            return err;
        }
        QObject  dstObj(QType_list);
        dstObj.value = std::make_shared<QList>();
        dst = QSTORE.SetValue(params[2], dstObj);
    }
    
    const PLIST& srclist = src->CastList();
    const PLIST& dstlist = dst->CastList();
    
    assert (!srclist->empty());
    
    dstlist->splice(dstlist->begin(), *srclist, (++ srclist->rbegin()).base());
    
    FormatBulk(dstlist->begin()->c_str(),
               dstlist->begin()->size(),
               reply);
    
    return   QError_ok;
}

QError  brpoplpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    // check timeout format
    long  timeout;
    if (!TryStr2Long(params.back().c_str(),
                     params.back().size(),
                     timeout))
    {
        ReplyError(QError_nan, reply);
        return QError_nan;
    }
    
    timeout *= 1000;
    
    // check target list
    QObject* dst;
    
    QError  err = QSTORE.GetValueByType(params[2], dst, QType_list);
    if (err != QError_ok)
    {
        if (err != QError_notExist)
        {
            ReplyError(err, reply);
            return err;
        }
    }
    
    auto dstKeyIter = -- (-- params.end());
    return  _GenericBlockedPop(++ params.begin(), dstKeyIter,
                               reply, ListPosition::tail, timeout, &*dstKeyIter, false);
}
