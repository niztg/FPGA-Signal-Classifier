INSTALL := C:/intelFPGA/QUARTUS_Lite_V23.1

MAIN := main.c
HDRS := address_map.h kiss_fft.h kiss_fft_guts.h kiss_fftr.h helper/data_processing.h helper/signal_analysis.h helper/vga.h
SRCS := main.c kiss_fft.c kiss_fftr.c helper/data_processing.c helper/signal_analysis.c helper/vga.c
OBJS := $(SRCS:.c=.o)

SHELL := cmd.exe
.SUFFIXES:

# DE1-SoC
JTAG_INDEX_SoC := 2
JTAG_INDEX_Lite := 1

# Tool locations
COMPILER := $(INSTALL)/fpgacademy/AMP/cygwin64/home/compiler/bin
CC := $(COMPILER)/riscv32-unknown-elf-gcc.exe
LD := $(CC)
OD := $(COMPILER)/riscv32-unknown-elf-objdump.exe
NM := $(COMPILER)/riscv32-unknown-elf-nm.exe

# Programmer / terminal
QP_PROGRAMMER := quartus_pgm.exe
HW_DE1-SoC := "$(INSTALL)/fpgacademy/Computer_Systems/DE1-SoC/DE1-SoC_Computer/niosVg/DE1_SoC_Computer.sof"
HW_DE10-Lite := "$(INSTALL)/fpgacademy/Computer_Systems/DE10-Lite/DE10-Lite_Computer/niosVg/DE10_Lite_Computer.sof"

SYS_FLAG_CABLE_SoC := -c "DE-SoC [USB-1]"
SYS_FLAG_CABLE_Lite := -c "USB-Blaster [USB-0]"

# Flags
USERCCFLAGS := -g -O1 -ffunction-sections -fverbose-asm -fno-inline -gdwarf-2
USERLDFLAGS := -Wl,--defsym=__stack_pointer$$=0x4000000 -Wl,--defsym -Wl,JTAG_UART_BASE=0xff201000
ARCHCCFLAGS := -march=rv32im_zicsr -mabi=ilp32
ARCHLDFLAGS := -march=rv32im_zicsr -mabi=ilp32
CCFLAGS := -Wall -c -I. -Ihelper $(USERCCFLAGS) $(ARCHCCFLAGS)
LDFLAGS := $(USERLDFLAGS) $(ARCHLDFLAGS)
LIBS := -lm

TARGET := $(basename $(MAIN)).elf

.PHONY: compile clean symbols objdump DETECT_DEVICES DE1-SoC DE10-Lite TERMINAL GDB_SERVER GDB_CLIENT

compile: $(TARGET)

$(TARGET): $(OBJS)
	@if exist "$@" del /Q "$@"
	@echo Linking $@
	$(LD) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

%.o: %.c $(HDRS)
	@echo Compiling $<
	$(CC) $(CCFLAGS) -o $@ $<

symbols: $(TARGET)
	$(NM) -p $(TARGET)

objdump: $(TARGET)
	$(OD) -d -S $(TARGET)

clean:
	@if exist "$(subst /,\,$(TARGET))" del /Q "$(subst /,\,$(TARGET))"
	@for %%f in ($(OBJS)) do @if exist "$(subst /,\,$$f)" del /Q "$(subst /,\,$$f)"

DETECT_DEVICES:
	$(QP_PROGRAMMER) $(SYS_FLAG_CABLE_SoC) --auto

DE1-SoC:
	$(QP_PROGRAMMER) $(SYS_FLAG_CABLE_SoC) -m jtag -o "P;$(HW_DE1-SoC)@$(JTAG_INDEX_SoC)"

DE10-Lite:
	$(QP_PROGRAMMER) $(SYS_FLAG_CABLE_Lite) -m jtag -o "P;$(HW_DE10-Lite)@$(JTAG_INDEX_Lite)"

TERMINAL:
	nios2-terminal.exe --instance 0

GDB_SERVER:
	ash-riscv-gdb-server.exe --device 02D120DD --gdb-port 2454 --instance 1 --probe-type USB-Blaster-2 --transport-type jtag --auto-detect true

GDB_CLIENT:
	riscv32-unknown-elf-gdb.exe -silent -ex "target remote:2454" -ex "set $$mstatus=0" -ex "set $$mtvec=0" -ex "load" -ex "set $$pc=_start" -ex "info reg pc" "$(TARGET)"