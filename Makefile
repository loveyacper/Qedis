all:
	@if [ ! -d "leveldb" ]; then echo "leveldb not present. Fetching leveldb-1.18 from internet..."; curl -s -L -O https://github.com/google/leveldb/archive/v1.18.tar.gz; tar xzvf v1.18.tar.gz; rm -f v1.18.tar.gz; mv leveldb-1.18 leveldb; fi
	cd leveldb && make && cd -
	mkdir -p bin && ln -s -f ../leveldb/libleveldb.so.1.18 bin/libleveldb.so.1
	@if [ ! -d "rocksdb" ]; then echo "rocksdb not present. Fetching rocksdb v6.3.6 from internet..."; curl -s -L -O https://github.com/facebook/rocksdb/archive/v6.3.6.tar.gz; tar xzvf v6.3.6.tar.gz; rm -f v6.3.6.tar.gz; mv rocksdb-6.3.6 rocksdb; fi
	cd rocksdb && make shared_lib && cd -
	cp ./rocksdb/librocksdb.* ./bin/
	mkdir -p build && cd build && cmake .. && make 

clean:
	@if [ -d "build" ]; then cd build && make clean && cd -; fi
cleanall:clean
	@if [ -d "leveldb" ]; then cd leveldb && make clean && cd -; rm -rf leveldb; fi

cleanbuild:
	rm -rf ./build; rm -rf ./bin/*
	@if [ -d "rocksdb" ]; then rm -rf rocksdb; fi
	@if [ -d "leveldb" ]; then rm -rf leveldb; fi