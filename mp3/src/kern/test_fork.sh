#/bin/bash
cd ../user
make clean
make 
cp bin/init_trek_rule30 bin/runme

cd ../util
make clean
make
./mkfs ../kern/kfs.raw ../user/bin/runme ../user/bin/trek ../user/bin/rule30 ../user/bin/fib

cd ../kern
make clean
make run-kernel