all:
	@if [ ! -d "leveldb" ]; then echo "leveldb not present. Fetching leveldb-1.18 from internet..."; curl -s -L -O https://github.com/google/leveldb/archive/v1.18.tar.gz; tar xzvf v1.18.tar.gz; rm -f v1.18.tar.gz; mv leveldb-1.18 leveldb; fi
	cd leveldb && make && cd -
	mkdir -p bin
	mkdir -p build && cd build && cmake .. && make 

clean:
	@if [ -d "build" ]; then cd build && make clean && cd -; fi
cleanall:clean
	@if [ -d "leveldb" ]; then cd leveldb && make clean && cd -; rm -rf leveldb; fi
