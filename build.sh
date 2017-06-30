#!/bin/bash

ARG=$1

if [ $# -lt 1 ]; then
    ARG="all"
fi

BASEPATH=$(cd `dirname $0`; pwd)


case $ARG in
    all)
        if [ ! -d "leveldb" ]; then
            echo "leveldb not present. Fetching leveldb-1.18 from internet..."
            curl -s -L -O https://github.com/google/leveldb/archive/v1.18.tar.gz
            tar xzvf v1.18.tar.gz
            rm -f v1.18.tar.gz
            mv leveldb-1.18 leveldb
        fi
        cd leveldb && make && cd -
        mkdir -p bin && ln -s -f ../leveldb/libleveldb.so.1.18 bin/libleveldb.so.1
        mkdir -p build && cd build && cmake .. && make
        ;;
    clean)
        cd build && make clean
        ;;
    cleanall)
        (cd leveldb && make clean && cd -)
        rm -rf leveldb
        (cd build && make clean && cd -)
        rm -rf build bin
        ;;
    help|*)
        echo "Usage:"
        echo "$0 help:     view help info."
        echo "$0 all:      build all target"
        echo "$0 install:  install framework"
        echo "$0 cleanall: remove all temp file"
        ;;
esac

