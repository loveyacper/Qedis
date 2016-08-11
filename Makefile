all:
	cd leveldb && make && cd -
	mkdir -p bin && ln -s -f ../backends/leveldb/libleveldb.so.1.18 bin/libleveldb.so.1
	mkdir -p build && cd build && cmake .. && make
clean:
	cd build && make clean
