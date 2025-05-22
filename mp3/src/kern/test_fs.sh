#/bin/bash
rm -rf companion.o
cd ../user
make clean && make
cd ../util
make clean && make
./mkfs ../kern/kfs.raw \
        ../user/bin/trek ../user/bin/rogue ../user/bin/zork ../user/bin/hello\
        ../user/bin/t_read ../user/bin/to_write
cd ../kern
./mkcomp.sh kfs.raw
make run-fs
rm -f kfs.raw companion.o
#make clean