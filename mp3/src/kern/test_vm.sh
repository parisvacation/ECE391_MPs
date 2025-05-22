#/bin/bash
./mkfs.sh

echo "------------------------"
echo "Now running test for illegal write"
echo "------------------------"
make run-illwrite
echo "------------------------"


echo "------------------------"
make run-illread
echo "------------------------"

echo "------------------------"
make run-vm1
echo "------------------------"
echo "------------------------"
make run-vm2
echo "------------------------"
echo "------------------------"
make run-vm3
echo "------------------------"
#make clean