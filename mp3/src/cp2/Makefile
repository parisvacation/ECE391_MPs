# Based on xv6-riscv Makefile; see LICENSE.

TOOLPREFIX=riscv64-unknown-elf-
QEMU=qemu-system-riscv64

CC=$(TOOLPREFIX)gcc
AS=$(TOOLPREFIX)as
LD=$(TOOLPREFIX)ld
OBJCOPY=$(TOOLPREFIX)objcopy
OBJDUMP=$(TOOLPREFIX)objdump

CORE_OBJS = \
	start.o \
	halt.o \
	string.o \
	trapasm.o \
	intr.o \
	plic.o \
	timer.o \
	thread.o \
	thrasm.o \
	ezheap.o \
	io.o \
	device.o \
	uart.o \
	virtio.o \
	vioblk.o \
	console.o \
	excp.o \
	memory.o \
	syscall.o \
	process.o \
	kfs.o \
	elf.o \
	# Add more object files here

CFLAGS = -Wall -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -mcmodel=medany -fno-pie -no-pie -march=rv64g -mabi=lp64d
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -fno-asynchronous-unwind-tables
CFLAGS += -I. # -DDEBUG -DTRACE

QEMUOPTS = -global virtio-mmio.force-legacy=false
QEMUOPTS += -machine virt -bios none -kernel $< -m 8M -nographic
QEMUOPTS += -serial mon:stdio
QEMUOPTS += -drive file=kfs.raw,id=blk0,if=none,format=raw
QEMUOPTS += -device virtio-blk-device,drive=blk0
QEMUOPTS += -serial pty
QEMUOPTS += -monitor pty

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

all: kernel.elf

kernel.elf: $(CORE_OBJS) main.o companion.o
	$(LD) -T kernel.ld -o $@ $^



run-kernel: kernel.elf
	$(QEMU) $(QEMUOPTS)

debug-kernel: kernel.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

clean:
	if [ -f companion.o ]; then cp companion.o companion.o.save; fi
	rm -rf *.o *.elf *.asm /memory_test_kern/*.o
	if [ -f companion.o.save ]; then mv companion.o.save companion.o; fi

# This will load the trek file into your kernel memory, via kernel.ld
# `mkcomp.sh`, as well as the documentation, contain discussion
companion.o:
	if [ -f ../user/trek ]; then sh ./mkcomp.sh ../user/trek; fi
	if ! [ -f ../user/trek ]; then sh ./mkcomp.sh; fi


# Test for virtual memory

test_illegal_write.elf: $(CORE_OBJS) ./memory_test_kern/test_illegal_write.o companion.o
	$(LD) -T kernel.ld -o $@ $^

test_illegal_read.elf: $(CORE_OBJS) ./memory_test_kern/test_illegal_read.o companion.o
	$(LD) -T kernel.ld -o $@ $^

test_memory_1.elf: $(CORE_OBJS) ./memory_test_kern/test_memory_1_main.o companion.o
	$(LD) -T kernel.ld -o $@ $^

test_memory_2.elf: $(CORE_OBJS) ./memory_test_kern/test_memory_2_main.o companion.o
	$(LD) -T kernel.ld -o $@ $^

test_memory_3.elf: $(CORE_OBJS) ./memory_test_kern/test_memory_3_main.o companion.o
	$(LD) -T kernel.ld -o $@ $^


run-illwrite: test_illegal_write.elf
	$(QEMU) $(QEMUOPTS)

debug-illwrite: test_illegal_write.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

run-illread: test_illegal_read.elf
	$(QEMU) $(QEMUOPTS)

debug-illread: test_illegal_read.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

run-vm1: test_memory_1.elf
	$(QEMU) $(QEMUOPTS)

debug-vm1: test_memory_1.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

run-vm2: test_memory_2.elf
	$(QEMU) $(QEMUOPTS)

debug-vm2: test_memory_2.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

run-vm3: test_memory_3.elf
	$(QEMU) $(QEMUOPTS)

debug-vm3: test_memory_3.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)