all:
	mkdir build; cd build; cmake ..; make
clean:
	cd build; make clean;
