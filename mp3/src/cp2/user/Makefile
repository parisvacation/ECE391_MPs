# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

ULIB_OBJS = \
	string.o \
	io.o \
	syscall.o \
	start.o

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I.

all: bin/trek bin/init0 bin/init1 bin/init2 bin/syscall_test_case bin/test_memory_1 bin/test_memory_2 bin/test_memory_3

bin/init0: $(ULIB_OBJS) init0.o
	$(LD) -T user.ld -o $@ $^

bin/init1: $(ULIB_OBJS) init1.o
	$(LD) -T user.ld -o $@ $^

bin/init2: $(ULIB_OBJS) init2.o
	$(LD) -T user.ld -o $@ $^

bin/syscall_test_case: $(ULIB_OBJS) syscall_test_case.o
	$(LD) -T user.ld -o $@ $^

bin/test_memory_1: $(ULIB_OBJS) test_memory_1.o
	$(LD) -T user.ld -o $@ $^

bin/test_memory_2: $(ULIB_OBJS) test_memory_2.o
	$(LD) -T user.ld -o $@ $^

bin/test_memory_3: $(ULIB_OBJS) test_memory_3.o
	$(LD) -T user.ld -o $@ $^
	
clean:
	rm -rf *.o *.elf *.asm bin/init0 bin/init1 bin/init2 bin/syscall_test_case bin/test_memory_1 bin/test_memory_2 bin/test_memory_3
