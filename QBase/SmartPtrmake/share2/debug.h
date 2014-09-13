#pragma once

           // std::cerr << __FUNCTION__ << "\n";  \
#undef   DECLARE_CLASS

#define  DECLARE_CLASS(_name_)  \
    class _name_ {  \
        public:     \
        _name_() {  \
        }   \
        _name_(const _name_& ) {  \
        }   \
        ~_name_() { \
        }   \
    };

#undef  DECLARE_BASE_CLASS
#define  DECLARE_BASE_CLASS(_name_)  \
    class _name_ {  \
        public:     \
        _name_() {  \
            std::cerr << __FUNCTION__ << "\n";  \
        }   \
        virtual ~_name_() { \
            std::cerr << __FUNCTION__<<"\n";    \
        }   \
    };

#undef  DECLARE_SON_CLASS
#define  DECLARE_SON_CLASS(_son_, _base_)  \
    class _son_ : public _base_ {  \
        public:     \
        _son_() {  \
            std::cerr << __FUNCTION__ << "\n";  \
        }   \
        virtual ~_son_() { \
            std::cerr << __FUNCTION__<<"\n";    \
        }   \
    };
