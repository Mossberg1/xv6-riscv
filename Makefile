K=kernel
U=user

OBJS = \
	$K/core/entry.o \
	$K/core/start.o \
	$K/drivers/console.o \
	$K/lib/printf.o \
	$K/drivers/uart.o \
	$K/memory/kalloc.o \
	$K/sync/spinlock.o \
	$K/lib/string.o \
	$K/core/main.o \
	$K/memory/vm.o \
	$K/core/proc.o \
	$K/core/swtch.o \
	$K/core/trampoline.o \
	$K/core/trap.o \
	$K/syscall/syscall.o \
	$K/syscall/sysproc.o \
	$K/fs/bio.o \
	$K/fs/fs.o \
	$K/fs/log.o \
	$K/sync/sleeplock.o \
	$K/fs/file.o \
	$K/fs/pipe.o \
	$K/core/exec.o \
	$K/syscall/sysfile.o \
	$K/core/kernelvec.o \
	$K/drivers/plic.o \
	$K/drivers/virtio_disk.o \
	$K/drivers/virtio_gpu.o

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-elf-'; \
	elif riscv64-none-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-none-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64
MIN_QEMU_VERSION = 7.2

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -Wno-unknown-attributes -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -march=rv64gc
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding
CFLAGS += -fno-common -nostdlib
CFLAGS += -fno-builtin-strncpy -fno-builtin-strncmp -fno-builtin-strlen -fno-builtin-memset
CFLAGS += -fno-builtin-memmove -fno-builtin-memcmp -fno-builtin-log -fno-builtin-bzero
CFLAGS += -fno-builtin-strchr -fno-builtin-exit -fno-builtin-malloc -fno-builtin-putc
CFLAGS += -fno-builtin-free
CFLAGS += -fno-builtin-memcpy -Wno-main
CFLAGS += -fno-builtin-printf -fno-builtin-fprintf -fno-builtin-vprintf
CFLAGS += -I$K/include -I$U/include -I.
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

$K/kernel: $(OBJS) $K/kernel.ld
	$(LD) $(LDFLAGS) -T $K/kernel.ld -o $K/kernel $(OBJS) 
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

$K/%.o: $K/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$K/%.o: $K/%.S
	$(CC) -march=rv64gc -g -c -o $@ $<

tags:
	find kernel user -name '*.[cS]' | xargs etags

$U/%.o: $U/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

ULIB = \
	$U/lib/ulib.o \
	$U/lib/usys.o \
	$U/lib/printf.o \
	$U/lib/umalloc.o

_%: %.o $(ULIB) $U/user.ld
	$(LD) $(LDFLAGS) -T $U/user.ld -o $@ $< $(ULIB)
	$(OBJDUMP) -S $@ > $*.asm
	$(OBJDUMP) -t $@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $*.sym

$U/lib/usys.S : $U/usys.pl
	perl $U/usys.pl > $U/lib/usys.S

$U/lib/usys.o : $U/lib/usys.S
	$(CC) $(CFLAGS) -c -o $U/lib/usys.o $U/lib/usys.S

$U/_forktest: $U/tests/forktest.o $(ULIB)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 \
	-o $U/_forktest $U/tests/forktest.o $U/lib/ulib.o $U/lib/usys.o

mkfs/mkfs: mkfs/mkfs.c $K/include/fs.h $K/include/param.h
	gcc -Wno-unknown-attributes -I. -I$K/include -o mkfs/mkfs mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

UPROGS=\
	$U/bin/_cat\
	$U/bin/_echo\
	$U/tests/_forktest\
	$U/bin/_grep\
	$U/init/_init\
	$U/bin/_kill\
	$U/bin/_ln\
	$U/bin/_ls\
	$U/bin/_mkdir\
	$U/bin/_rm\
	$U/bin/_sh\
	$U/tests/_stressfs\
	$U/tests/_usertests\
	$U/tests/_grind\
	$U/bin/_wc\
	$U/tests/_zombie\
	$U/tests/_logstress\
	$U/tests/_forphan\
	$U/tests/_dorphan\

fs.img: mkfs/mkfs README $(UPROGS)
	cd user && ../mkfs/mkfs ../fs.img ../README $(patsubst $U/%,%,$(UPROGS))

-include $(shell find kernel user -name '*.d')

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	mkfs/mkfs .gdbinit $U/lib/usys.S fs.img
	find $K $U -type f -name "*.o" -delete
	find $K $U -type f -name "*.d" -delete
	find $K $U -type f -name "*.asm" -delete
	find $K $U -type f -name "*.sym" -delete
	rm -f $K/kernel $(UPROGS)

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 3
endif

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS)
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

QEMUOPTS += -device virtio-gpu-device,bus=virtio-mmio-bus.1
QEMUOPTS += -device virtio-keyboard-device,bus=virtio-mmio-bus.2
QEMUOPTS += -device virtio-mouse-device,bus=virtio-mmio-bus.3
QEMUOPTS += -display cocoa
QEMUOPTS += -serial mon:stdio

qemu: check-qemu-version $K/kernel fs.img
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

qemu-gdb: $K/kernel .gdbinit fs.img
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

print-gdbport:
	@echo $(GDBPORT)

QEMU_VERSION := $(shell $(QEMU) --version | head -n 1 | sed -E 's/^QEMU emulator version ([0-9]+\.[0-9]+)\..*/\1/')
check-qemu-version:
	@if [ "$(shell echo "$(QEMU_VERSION) >= $(MIN_QEMU_VERSION)" | bc)" -eq 0 ]; then \
		echo "ERROR: Need qemu version >= $(MIN_QEMU_VERSION)"; \
		exit 1; \
	fi
