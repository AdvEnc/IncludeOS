#################################################
#              IncludeOS makefile               #
#################################################

ifndef INCLUDEOS_INSTALL
	INCLUDEOS_INSTALL=$(HOME)/IncludeOS_install
	#$(error IncludeOS installation folder not set!)
endif
# shorter name
INSTALL = $(INCLUDEOS_INSTALL)

STACK_PROTECTOR_VALUE=$(shell awk 'BEGIN{print int(rand()*65536)}')
# stackrealign is needed to guarantee 16-byte stack alignment for SSE
# the compiler seems to be really dumb in this regard, creating a misaligned stack left and right
CAPABS_COMMON = -mstackrealign -fstack-protector-all -fno-omit-frame-pointer -msse3 -D_STACK_GUARD_VALUE_=$(STACK_PROTECTOR_VALUE)

# Various global defines
# * NO_DEBUG Disables  output from the debug macro
# * OS_TERMINATE_ON_CONTRACT_VIOLATION provides classic assert-like output from Expects / Ensures
# * _GNU_SOURCE enables POSIX-extensions in newlib, such as strnlen. ("everything newlib has", ref. cdefs.h)
CAPABS = $(CAPABS_COMMON) -O2 -DNO_DEBUG=1 -DOS_TERMINATE_ON_CONTRACT_VIOLATION -D_GNU_SOURCE
WARNS   = -Wall -Wextra #-pedantic
DEBUG_OPTS = -ggdb3


# External Libraries
###################################################
LIBC_OBJ = $(INSTALL)/newlib/libc.a
LIBG_OBJ = $(INSTALL)/newlib/libg.a
LIBM_OBJ = $(INSTALL)/newlib/libm.a

LIBGCC = $(INSTALL)/libgcc/libgcc.a
LIBCXX = $(INSTALL)/libcxx/libc++.a libc++abi.a


INC_NEWLIB=$(INSTALL)/newlib/include

# Compiler/Linker
###################################################
CC  = clang-3.8
CPP = clang++-3.8
# Set defaults if not defined
ifndef LD_INC
	LD_INC = ld
endif
ifndef AR_INC
	AR_INC = ar
endif

# Compiler options
CCOPTS = -target i686-elf
CPPOPTS = -target i686-elf

INCLUDES =  -I../api/sys -I$(INSTALL)/libcxx/include  -I$(INC_NEWLIB)  -Iinclude -I../api -I../mod/GSL/include #
CINCLUDES =  -I../api/sys -I$(INC_NEWLIB) -Iinclude -I../api  #

CCOPTS	+= $(CAPABS) $(WARNS) -c -m32 -march=i686 $(CINCLUDES)
CPPOPTS += $(CAPABS) $(WARNS) -c -m32 -std=c++14 $(INCLUDES) -D_LIBCPP_HAS_NO_THREADS=1 -DOS_VERSION="\"$(shell git describe --dirty)\""

LDOPTS = -nostdlib -melf_i386 -N --eh-frame-hdr --script=linker.ld #-flto

# Objects
###################################################
CXXABI     = $(shell find ./crt/cxxabi -type f -name "*.cpp")
CXXABI_OBJ = $(CXXABI:.cpp=.o)

OS_OBJECTS = kernel/kernel_start.o kernel/syscalls.o \
		kernel/interrupts.o kernel/os.o kernel/cpuid.o \
		kernel/irq_manager.o kernel/pci_manager.o \
		kernel/terminal.o kernel/terminal_disk.o \
		kernel/vga.o util/memstream.o util/async.o \
		crt/c_abi.o crt/string.o crt/quick_exit.o crt/cxx_abi.o  crt/mman.o \
		hw/ide.o hw/pit.o hw/pic.o hw/pci_device.o hw/cpu_freq_sampling.o \
		hw/serial.o hw/apic.o hw/apic_asm.o hw/cmos.o\
		virtio/virtio.o virtio/virtio_queue.o virtio/virtionet.o \
		net/ethernet.o net/inet_common.o net/ip4/arp.o net/ip4/ip4.o \
		net/tcp.o net/tcp_connection.o net/tcp_connection_states.o \
		net/ip4/icmpv4.o net/ip4/udp.o net/ip4/udp_socket.o \
		net/dns/dns.o net/dns/client.o net/dhcp/dh4client.o \
		net/ip6/ip6.o net/ip6/icmp6.o net/ip6/udp6.o net/ip6/ndp.o \
		net/packet.o net/buffer_store.o \
		fs/disk.o fs/filesystem.o fs/mbr.o fs/path.o \
		fs/ext4.o fs/fat.o fs/fat_async.o fs/fat_sync.o fs/memdisk.o \
		virtio/block.o # virtio/console.o

CRTI_OBJ = crt/crti.o
CRTN_OBJ = crt/crtn.o

# Header dependencies (rebuild if header changed)
###################################################
OS_DEPS = $(OS_OBJECTS:.o=.d)

.PHONY: memdisk all debug stripped clean

# Complete OS build
###################################################
# A complete build includes:
# - a bootloader
# - an OS library for the service to link against

all: bootloader libc++abi.a os.a
	@echo "\n>>> Built OS-library. Install to '"$(INSTALL)"' using 'make install'"

#stripped: LDOPTS += -s #strip all
stripped: CAPABS += -Oz
stripped: all test

# Build like "all" but with debugging output (i.e. the 'debug'-macro) enabled
debug-info: CAPABS += -UNO_DEBUG
debug-info: CAPABS += -DGSL_THROW_ON_CONTRACT_VIOLATION
debug-info: all test

# Build with debugging symbols (OBS: Dramatically increases binary size)
debug: CCOPTS  += $(DEBUG_OPTS)
debug: CPPOPTS += $(DEBUG_OPTS)
debug: OBJ_LIST += $(LIBG_OBJ)
debug: CAPABS += -O0
debug: all test

# Build with debugging symbols + debugging ouput, i.e. "debug" + "debug-info"
debug-all: CAPABS += -UNO_DEBUG
debug-all: CCOPTS  += $(DEBUG_OPTS)
debug-all: CPPOPTS += $(DEBUG_OPTS)
debug-all: OBJ_LIST += $(LIBG_OBJ)
debug-all: CAPABS += -O0
debug-all: all test


silent: CPPOPTS += -DNO_INFO=1
silent: all test

# Test service
###################################################
LIBS_OBJ = os.a $(LIBCXX) os.a $(LIBC_OBJ) $(LIBM_OBJ) $(LIBGCC)


CRTBEGIN_OBJ = $(CRTI_OBJ) $(INSTALL)/crt/crtbegin.o
CRTEND_OBJ   = $(INSTALL)/crt/crtend.o $(CRTN_OBJ)

TEST_OBJ = debug/test_service.o util/service_name.o

test_service: CPPOPTS += -DSERVICE_NAME="\"Test Service\""
test_service: $(TEST_OBJ) $(CRTI_OBJ) $(CRTN_OBJ)
	@echo "\n>> Linking test service"
	touch smalldisk
	$(LD_INC) -v $(LDOPTS) $(CRTBEGIN_OBJ) $(TEST_OBJ) $(LIBS_OBJ) $(CRTEND_OBJ) -o debug/$@

test_ipv6: CPPOPTS += -DSERVICE_NAME="\"IPv6 Test Service\""
test_ipv6: debug/test_ipv6.o $(CRTI_OBJ) $(CRTN_OBJ)
	@echo "\n>> Linking test IPv6 service"
	$(LD_INC) -v $(LDOPTS) $(CRTBEGIN_OBJ) debug/test_ipv6.o $(LIBS_OBJ) $(CRTEND_OBJ) -o debug/$@

test_disk: CPPOPTS += -DSERVICE_NAME="\"Virtio-disk Test\""
test_disk: debug/test_disk.o $(CRTI_OBJ) $(CRTN_OBJ) memdisk
	@echo "\n>> Linking disk test"
	touch smalldisk
	$(LD_INC) -v $(LDOPTS) $(CRTBEGIN_OBJ) debug/test_disk.o $(LIBS_OBJ) $(CRTEND_OBJ) memdisk.o -o debug/$@

test_tcp: CPPOPTS += -DSERVICE_NAME="\"TCP Test Service\""
test_tcp: debug/test_tcp.o $(CRTI_OBJ) $(CRTN_OBJ)
	@echo "\n>> Linking test TCP service"
	$(LD_INC) -v $(LDOPTS) $(CRTBEGIN_OBJ) debug/test_tcp.o $(LIBS_OBJ) $(CRTEND_OBJ) -o debug/$@

test: test_service

# OS libraries
###################################################
# OS
libc++abi.a: $(CXXABI_OBJ)
	$(AR_INC) rcs $@ $(CXXABI_OBJ)

os.a: $(OS_OBJECTS)
	@echo "\n>> Building the OS library"
	$(AR_INC) rcs $@ $(OS_OBJECTS)

install: $(CRTI_OBJ) $(CRTN_OBJ)
	@echo "\n>> Installing..."
	cp bootloader $(INSTALL)
	cp os.a $(INSTALL)
	cp libc++abi.a $(INSTALL)/libcxx
	cp crt/crti.o $(INSTALL)/crt
	cp crt/crtn.o $(INSTALL)/crt
	cp -r ../api $(INSTALL)/
	cp linker.ld $(INSTALL)/
	# Memdisk related
	mkdir -p $(INSTALL)/memdisk
	cp memdisk/* $(INSTALL)/memdisk/
	# Seed related
	cp util/service_name.cpp $(INSTALL)
	cp seed/Makefile $(INSTALL)/Makeseed
	# Eexternal dependencies related
	mkdir -p $(INSTALL)/mod/
	cp -r ../mod/GSL $(INSTALL)/mod
	@echo "\nDone!"

# Makefile recipes
###################################################
%.o: %.c
	$(CC) -MMD $(CCOPTS) -o $@ $<

%.o: %.cpp
	@echo "\n"
	$(CPP) -MMD $(CPPOPTS) -o $@ $<

# AS-assembled object files
%.o: %.s
	@echo "\n"
	$(CPP) $(CPPOPTS) -x assembler-with-cpp $< -o $@

%.o: %.asm
	nasm -f elf $< -o $@

# Bootloader
###################################################
bootloader: boot/bootloader.asm boot/disk_read_lba.asm
	@echo "\n>> Assembling bootloader"
	nasm -f bin -o bootloader $<

# Disk image as a section
###################################################
memdisk: memdisk.asm
	@echo "\n>> Assembling memdisk"
	nasm -f elf -o memdisk.o $<

# Etags
###################################################
etags:
	@echo "\n>> Updating emacs tags "
	- find ../ -name "*.[hc]pp" | grep -v ".*\#.*" | etags -


# Cleanup
###################################################
clean_tests:
	$(RM) debug/*.o debug/*.d debug/*.img debug/test_service debug/test_ipv6 debug/test_tcp

clean: clean_tests
	$(RM) $(OS_OBJECTS) $(CXXABI_OBJ) $(OS_DEPS)
	$(RM) os.a
	$(RM) libc++abi.a
	$(RM) bootloader memdisk.o

-include $(OS_DEPS)
