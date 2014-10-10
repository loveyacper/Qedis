#include "IntSet.h"

using std::size_t;

size_t IntSet::m_maxElem = 512;

Encoding  IntSet::_TestEncoding(int64_t  val)
{
    if (val > std::numeric_limits<int>::max() || val < std::numeric_limits<int>::min())
        return Encoding64;
    else if (val > std::numeric_limits<short>::max() || val < std::numeric_limits<short>::min())
        return Encoding32;
    else
        return Encoding16;
}

bool IntSet::InsertValue(int64_t  val)
{
    Encoding newEnc = _TestEncoding(val);

    if (m_encoding == Encoding0)
        m_encoding = newEnc;

    if (newEnc <= m_encoding)
    {
        switch (m_encoding)
        {
        case  Encoding16:
            if (m_set16 == NULL)
                m_set16.Reset(new IntSetImpl<int16_t>());

            return m_set16->Size() < m_maxElem && m_set16->InsertValue(val);

        case  Encoding32:
            if (m_set32 == NULL)
                m_set32.Reset(new IntSetImpl<int32_t>());

            return m_set32->Size() < m_maxElem && m_set32->InsertValue(val);

        case  Encoding64:
            if (m_set64 == NULL)
                m_set64.Reset(new IntSetImpl<int64_t>());

            return m_set64->Size() < m_maxElem && m_set64->InsertValue(val);

        default:
            assert (false);
        }
    }
    else
    {
        if (m_encoding == Encoding16)
        {
            if (m_set16->Size() >= m_maxElem)
                return false;

            switch (newEnc)
            {
            case Encoding32:
                m_set32.Reset(new IntSetImpl<int32_t>());
                m_set16->MoveTo(*m_set32);
                m_set32->InsertValue(val);
                break;

            case Encoding64:
                m_set64.Reset(new IntSetImpl<int64_t>());
                m_set16->MoveTo(*m_set64);
                m_set64->InsertValue(val);
                break;

            default:
                assert (!!!"new enc should be 32 or 64");
            }
        }
        else if (m_encoding == Encoding32)
        {
            if (m_set32->Size() >= m_maxElem)
                return false;

            assert (newEnc == Encoding64);
            m_set64.Reset(new IntSetImpl<int64_t>());
            m_set32->MoveTo(*m_set64);

            m_set64->InsertValue(val);
        }
        else
        {
            assert (!!!"m_encoding is abnormal, should be 16 or 32");
        }

        m_encoding = newEnc;
    }

    return  true;
}

bool IntSet::Exist(int64_t val) const
{
    if (m_encoding == Encoding16)
        return m_set16->Exist(val);
    else if (m_encoding == Encoding32)
        return m_set32->Exist(val);
    else if (m_encoding == Encoding64)
        return m_set64->Exist(val);

    return false;
}

size_t IntSet::Size() const
{
    if (m_encoding == Encoding16)
        return m_set16->Size();
    else if (m_encoding == Encoding32)
        return m_set32->Size();
    else if (m_encoding == Encoding64)
        return m_set64->Size();

    return 0;
}

std::size_t IntSet::EraseValue(int64_t val) const
{
    if (m_encoding == Encoding16)
        return m_set16->EraseValue(val);
    else if (m_encoding == Encoding32)
        return m_set32->EraseValue(val);
    else if (m_encoding == Encoding64)
        return m_set64->EraseValue(val);

    return 0;
}

void IntSet::Print() const
{
    if (m_encoding == Encoding16)
        m_set16->Print();
    else if (m_encoding == Encoding32)
        m_set32->Print();
    else if (m_encoding == Encoding64)
        m_set64->Print();
}
    
int64_t IntSet::operator[] (int idx) const
{
    switch (m_encoding)
    {
    case Encoding16:
        return  (*m_set16)[idx];

    case Encoding32:
        return  (*m_set32)[idx];

    case Encoding64:
        return  (*m_set64)[idx];

    default:
        assert (false);
    }

    return 0;
}
    

