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
	io.o \
	device.o \
	virtio.o \
	vioblk.o \
	elf.o \
	kfs.o \
	uart.o \
	timer.o \
	thread.o \
	plic.o \
	thrasm.o \
	ezheap.o \
	console.o
	# Add the names of new objects here

ELFTEST_OBJS = \
	start.o \
	halt.o \
	string.o \
	trapasm.o \
	intr.o \
	io.o \
	device.o \
	uart.o \
	timer.o \
	thread.o \
	plic.o \
	thrasm.o \
	ezheap.o \
	console.o

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

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

all: shell.elf

shell.elf: $(CORE_OBJS) main_shell.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-shell: shell.elf
	$(QEMU) $(QEMUOPTS)

debug-shell: shell.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# You might want to add more here

clean:
	if [ -f companion.o ]; then cp companion.o companion.o.save; fi
	rm -rf *.o *.elf *.asm
	if [ -f companion.o.save ]; then mv companion.o.save companion.o; fi

companion.o:
	if [ -f ../user/bin/trek ] && [ -f ../user/bin/to_write ] && [ -f ../user/bin/t_read ]; then sh ./mkcomp.sh ../user/bin/trek ../user/bin/to_write ../user/bin/t_read; fi
	if ! [ -f ../user/bin/trek ] && ![ -f ../user/bin/to_write ] && ![ -f ../user/bin/t_read ]; then sh ./mkcomp.sh; fi

# Below are unit-tests for elf_load() function;

Elftest: Elftest.elf
	$(QEMU) -machine virt -bios none -kernel $< -m 8M -nographic -serial mon:stdio -serial pty 

main_tests_elf.o: main_tests_elf.c
	$(CC) $(CFLAGS) -c -o $@ $<
	
debug-Elftest: Elftest.elf
	$(QEMU) -machine virt -bios none -kernel $< -m 8M -nographic -serial mon:stdio -serial pty -S $(QEMUGDB)

Elftest.elf: $(ELFTEST_OBJS) elf.o main_tests_elf.o companion.o
	$(LD) -T kernel.ld -o $@ $^

# Below are test for virtio_block device
test_vioblk.elf: $(CORE_OBJS) test_vioblk.o
	$(LD) -T kernel.ld -o $@ $^
run-test-vioblk: test_vioblk.elf
	$(QEMU) $(QEMUOPTS)
run-debug-vioblk: test_vioblk.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

# Below are test for file system device
fs.elf: $(CORE_OBJS) main_tests_fs.o companion.o
	$(LD) -T kernel.ld -o $@ $^

run-fs: fs.elf
	$(QEMU) $(QEMUOPTS)

debug-fs: fs.elf
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)
