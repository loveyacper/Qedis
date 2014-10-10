#include "QList.h"
#include "QStore.h"
#include "Log/Logger.h"
#include <iostream>
#include <cassert>

using namespace std;

enum ListPosition
{
    ListPosition_head,
    ListPosition_tail,
};

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
            list.value.Reset(new QList());

            value = QSTORE.SetValue(params[1], list);
        }
        else
        {
            ReplyError(err, reply);
            return err;
        }
    }

    const PLIST& list = value->CastList();
    for (size_t i = 2; i < params.size(); ++ i)
    {
        if (pos == ListPosition_head)
            list->push_front(params[i]);
        else
            list->push_back(params[i]);
    }
    
    FormatInt(static_cast<long>(list->size()), reply);
    return   QError_ok;
}


static QError  pop(const vector<QString>& params, UnboundedBuffer& reply, ListPosition pos)
{
    QObject* value;
    
    QError err = QSTORE.GetValueByType(params[1], value, QType_list);
    if (err != QError_ok)
    {
        FormatNull(reply);
        return  err;
    }
    
    const PLIST&    list   = value->CastList();
    assert (!list->empty());

    if (pos == ListPosition_head)
    {
        const QString& result = list->front();
        FormatSingle(result.c_str(), result.size(), reply);
        list->pop_front();
    }
    else
    {
        const QString& result = list->back();
        FormatSingle(result.c_str(), result.size(), reply);
        list->pop_back();
    }
    
    if (list->empty())
    {
        QSTORE.DeleteKey(params[1]);
    }
    
    return   QError_ok;
}

QError  lpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_head);
}

QError  rpush(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_tail);
}

QError  lpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_head, false);
}

QError  rpushx(const vector<QString>& params, UnboundedBuffer& reply)
{
    return push(params, reply, ListPosition_tail, false);
}

QError  lpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    return pop(params, reply, ListPosition_head);
}

QError  rpop(const vector<QString>& params, UnboundedBuffer& reply)
{
    return pop(params, reply, ListPosition_tail);
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
    if (!Str2Long(params[2].c_str(), params[2].size(), idx))
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
    if (!Str2Long(params[2].c_str(), params[2].size(), idx))
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

#if 0
static void AdjustIndex(long& start, long& end, size_t  size)
{
    if (size == 0)
    {
        end = 0, start = 1;
        return;
    }
    
    if (start < 0)  start += size;
    if (start < 0)  start = 0;
    if (end < 0)    end += size;
    
    if (start > end || start >= size)
        end = 0, start = 1;
    
    if (end >= size)  end = size - 1;
}
#endif

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
    
    ListPosition  start = ListPosition_head;
    
    if (count < 0)
    {
        count = -count;
        start = ListPosition_tail;
    }
    else if (count == 0)
    {
        count = list->size(); // remove all elements equal to param[3]
    }
    
    long resultCount = 0;
    if (start == ListPosition_head)
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
        dstObj.value.Reset(new QList());
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
