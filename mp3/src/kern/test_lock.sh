#/bin/bash
cd ../user
make clean
make 
cp bin/init_lock_test bin/runme

cd ../util
mak clean
make 
./mkfs ../kern/kfs.raw ../user/bin/runme ../user/bin/to_write

cd ../kern
make clean
make run-kernel