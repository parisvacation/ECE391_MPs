#!/bin/bash

cd ../user

make clean || exit 1

make all || exit 1

cd ../util

make clean || exit 1

make all || exit 1

./mkfs ../kern/kfs.raw ../user/bin/init0 ../user/bin/init1 ../user/bin/init2 ../user/bin/trek ../user/bin/syscall_test_case \
        ../user/bin/test_memory_1 ../user/bin/test_memory_2 ../user/bin/test_memory_3 || exit 1

cd ../kern