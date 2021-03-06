#################################################
#          IncludeOS SERVICE makefile           #
#################################################

# IncludeOS location
ifndef INCLUDEOS_INSTALL
	INCLUDEOS_INSTALL=$(HOME)/IncludeOS_install
endif

ifneq ($(DISK),)
MEMDISK=memdisk.o
endif

# Shorter name
INSTALL = $(INCLUDEOS_INSTALL)

# Compiler and linker options
###################################################
CAPABS_COMMON = -mstackrealign -fstack-protector-all -msse3
WARNS  = -Wall -Wextra #-pedantic
DEBUG_OPTS = -ggdb3 -v

OPTIONS = $(WARNS) $(CAPABS) $(EXTRA_FLAGS)

# External Libraries
###################################################
LIBC_OBJ = $(INSTALL)/newlib/libc.a
LIBG_OBJ = $(INSTALL)/newlib/libg.a
LIBM_OBJ = $(INSTALL)/newlib/libm.a

LIBGCC = $(INSTALL)/libgcc/libgcc.a
LIBCXX = $(INSTALL)/libcxx/libc++.a $(INSTALL)/libcxx/libc++abi.a


INC_NEWLIB=$(INSTALL)/newlib/include
INC_LIBCXX=$(INSTALL)/libcxx/include

CC = clang-3.8 -target i686-elf
CPP = clang++-3.8 -target i686-elf
ifndef LD_INC
	LD_INC = ld
endif

INCLUDES = -I$(INC_LIBCXX) -I$(INSTALL)/api/sys -I$(INC_NEWLIB) -I$(INSTALL)/api -I$(INSTALL)/mod/GSL/include $(LOCAL_INCLUDES)

all: CAPABS  =  $(CAPABS_COMMON) -O2
debug: CAPABS = $(CAPABS_COMMON) -O0
stripped: CAPABS = $(CAPABS_COMMON) -Oz

CPPOPTS = $(CAPABS) $(WARNS) -c -m32 -std=c++14 $(INCLUDES) -D_LIBCPP_HAS_NO_THREADS=1 -D_GNU_SOURCE
LDOPTS = -nostdlib -melf_i386 -N --script=$(INSTALL)/linker.ld

# Objects
###################################################

CRTBEGIN_OBJ = $(INSTALL)/crt/crtbegin.o
CRTEND_OBJ = $(INSTALL)/crt/crtend.o
CRTI_OBJ = $(INSTALL)/crt/crti.o
CRTN_OBJ = $(INSTALL)/crt/crtn.o

# Full link list
OBJS  = $(FILES:.cpp=.o) .service_name.o

LIBS =  $(INSTALL)/os.a $(LIBCXX) $(INSTALL)/os.a $(LIBC_OBJ) $(LIBM_OBJ) $(LIBGCC)

OS_PRE = $(CRTBEGIN_OBJ) $(CRTI_OBJ)
OS_POST = $(CRTEND_OBJ) $(CRTN_OBJ)

DEPS = $(OBJS:.o=.d)

# Complete build
###################################################
# A complete build includes:
# - a "service", to be linked with OS-objects (OS included)
.PHONY: all stripped debug debug-info debug-all memdisk service

all: service

stripped: LDOPTS  += -S #strip all
stripped: CPPOPTS += -Oz
stripped: service

# Build like "all" but with debugging output (i.e. the 'debug'-macro) enabled
debug-info: CAPABS += -UNO_DEBUG
debug-info: service

# Build with debugging symbols (OBS: Dramatically increases binary size)
debug: CCOPTS  += $(DEBUG_OPTS)
debug: CPPOPTS += $(DEBUG_OPTS)
debug: OBJ_LIST += $(LIBG_OBJ)
debug: CAPABS += -O0
debug: service

# Build with debugging symbols + debugging ouput, i.e. "debug" + "debug-info"
debug-all: CAPABS += -UNO_DEBUG
debug-all: CCOPTS  += $(DEBUG_OPTS)
debug-all: CPPOPTS += $(DEBUG_OPTS)
debug-all: OBJ_LIST += $(LIBG_OBJ)
debug-all: CAPABS += -O0
debug-all: service

# Disk image as a section
###################################################
memdisk:
	@echo "\n>> Creating memdisk"
	python $(INSTALL)/memdisk/memdisk.py --file $(INSTALL)/memdisk/memdisk.asm $(DISK)
	@echo "\n>> Assembling memdisk"
	nasm -f elf $(INSTALL)/memdisk/memdisk.asm -o $(MEMDISK)

# Service
###################################################
service.o: service.cpp
	@echo "\n>> Compiling the service"
	$(CPP) $(CPPOPTS) -o $@ $<

.service_name.o: $(INSTALL)/service_name.cpp
	$(CPP) $(CPPOPTS) -DSERVICE_NAME="\"$(SERVICE_NAME)\"" -o $@ $<

# Link the service with the os
service: $(OBJS) $(LIBS)
# build memdisk when IMAGES is set
ifneq ($(MEMDISK),)
	@echo "\n>> Creating memdisk"
	python $(INSTALL)/memdisk/memdisk.py --file $(INSTALL)/memdisk/memdisk.asm $(DISK)
	@echo "\n>> Assembling memdisk"
	nasm -f elf $(INSTALL)/memdisk/memdisk.asm -o $(MEMDISK)
endif
	@echo "\n>> Linking service with OS"
	$(LD_INC) $(LDOPTS) $(OS_PRE) $(OBJS) $(LIBS) $(OS_POST) $(MEMDISK) -o $(SERVICE)
	@echo "\n>> Building" $(SERVICE).img
	$(INSTALL)/vmbuild $(INSTALL)/bootloader $(SERVICE)

# Object files
###################################################

# Runtime
crt%.o: $(INSTALL)/crt/crt%.s
	@echo "\n>> Assembling C runtime:" $@
	$(CPP) $(CPPOPTS) -x assembler-with-cpp $<

# General C++-files to object files
%.o: %.cpp
	@echo "\n>> Compiling $<..."
	$(CPP) $(CPPOPTS) -o $@ $<

%.o: %.c
	@echo "\n>> Compiling $<..."
	$(CC) $(WARNS) $(CAPABS) $(EXTRA_FLAGS) -c -m32 -std=c11 -I$(INSTALL)/api/sys -I$(INC_NEWLIB) -o $@ $<

# AS-assembled object files
%.o: %.s
	@echo "\n>> Assembling GNU 'as' files"
	$(CPP) $(CPPOPTS) -x assembler-with-cpp $<

# Cleanup
###################################################
clean:
	$(RM) $(OBJS) $(DEPS) $(SERVICE)
	$(RM) $(SERVICE).img
	$(RM) .service_name.o

-include $(DEPS)
