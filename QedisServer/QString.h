#ifndef BERT_QSTRING_H
#define BERT_QSTRING_H

#include <string>

#if defined(__APPLE__)
//#include "Allocator.h"
typedef std::string  QString;
//typedef std::basic_string<char, std::char_traits<char>, Bert::Allocator<char> >  QString;

#else

typedef std::string  QString;

#endif


#endif

