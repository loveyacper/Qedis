#ifndef BERT_INTSET_H
#define BERT_INTSET_H

#include <vector>
#include <cassert>
#include <algorithm>
#include <limits>
#include <iostream>
#include <cassert>
#include "SmartPtr/UniquePtr.h"
#include "QSet.h"

// small set, elem number is small, or will be slow
template <typename T>
class IntSetImpl
{
public:
    typedef typename std::vector<T>::iterator  iterator;
    typedef typename std::vector<T>::const_iterator  const_iterator;
    typedef typename std::vector<T>::value_type  value_type;
    typedef typename std::vector<T>::reference   reference;
    typedef typename std::vector<T>::const_reference   const_reference;
    typedef typename std::vector<T>::difference_type  difference_type;
    typedef typename std::vector<T>::pointer  pointer ;

    iterator begin() { return m_values.begin(); }
    iterator end()   { return m_values.end();   }
    const_iterator begin() const { return m_values.begin(); }
    const_iterator end()   const { return m_values.end();   }

    reference  operator[](int idx)
    {
        return  m_values[idx];
    }

    const_reference operator[](int idx) const
    {
        return  m_values[idx];
    }

    void Print() const
    {
        for (const_iterator it(begin()); it != end(); ++ it)
            std::cout << *it << "  ";
            
        std::cout << std::endl;
    }


    bool InsertValue(int64_t val)
    {
        assert (val <= std::numeric_limits<T>::max() && val >= std::numeric_limits<T>::min());

        if (Exist(val))
            return false;

        m_values.push_back(static_cast<T>(val));
        std::sort(m_values.begin(), m_values.end()); // TODO : effective

        return true;
    }

    std::size_t EraseValue(int64_t val)
    {
        iterator  it = std::lower_bound(begin(), end(), static_cast<T>(val));

        if (it == end())
            return  0;

        std::size_t nDeleted = 0;
        while (*it == static_cast<T>(val))
        {
            ++ nDeleted;
            it = m_values.erase(it);
        }

        return nDeleted;
    }

    bool Exist(int64_t val) const
    {
        if (val > std::numeric_limits<T>::max() || 
            val < std::numeric_limits<T>::min())
            return false;

        return  std::binary_search(begin(), end(), static_cast<T>(val));
    }


    void MoveTo(QSet& set)
    {
        for (iterator it(begin()); it != end(); ++ it) 
        {
            char strVal[64];
            snprintf(strVal, sizeof strVal, "%ld", *it);
            set.insert(strVal);
        }

        m_values.clear();
    }

    template <typename U>
    void MoveTo(IntSetImpl<U>& bigSet)
    {
        assert (sizeof (T) < sizeof (U));
        for (iterator it(begin()); it != end(); ++ it) 
        {
            bigSet.InsertValue(*it);
        }

        m_values.clear();
    }

    std::size_t Size() const {  return m_values.size(); }
    
private:
    std::vector<T>   m_values;
};

enum Encoding
{
    Encoding0  = 0,
    Encoding16 = 1,
    Encoding32 = 2,
    Encoding64 = 3,
};


class  IntSet
{
private:
    UniquePtr<IntSetImpl<int64_t> > m_set64;
    UniquePtr<IntSetImpl<int32_t> > m_set32;
    UniquePtr<IntSetImpl<int16_t> > m_set16;
    Encoding             m_encoding;

    static std::size_t m_maxElem; // TODO from config

public:
    static Encoding  _TestEncoding(int64_t  val);

    IntSet() : m_encoding(Encoding0)
    {
    }
   

    bool InsertValue(int64_t  val);

    bool Exist(int64_t val) const;

    std::size_t Size() const;

    std::size_t EraseValue(int64_t val) const;

    void Print() const;

    void swap(IntSet& set)
    {
        std::swap(m_encoding, set.m_encoding);
        m_set16.swap(set.m_set16);
        m_set32.swap(set.m_set32);
        m_set64.swap(set.m_set64);
    }

    int64_t  GetValue(int  idx) const { return this->operator[](idx); }

    int64_t operator[] (int idx) const;
};
    
#endif

