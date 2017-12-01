# Makefile for LC3 simulator assignment

# Change linux to osx on OX-X
# 1. make sure sed is in your path (used in ./fixPath) (ok for dept machines)
# 2. Set GCC to the the C compiler you use             (ok for dept machines)

OS		= linux

# List of files
# 'MY_SRC' are the files completed by the student in this/previous assignments
# .o files omitted from OBJS are provided in the archive LIB

C_HEADERS	= Debug.h field.h hardware.h install.h lc3.h logic.h symbol.h util.h
MY_SRC		=                                            logic.c 
OBJS		= Debug.o                    install.o       logic.o 

EXE		= mysim
LIB		= P8.a
SUBMISSION	= mysim.tar

# Compiler and loader commands and flags
GCC		= gcc
DEFINES         = -DSTACK_OPS -DDEBUG
GCC_FLAGS	= -g -std=c11 -Wall -c $(DEFINES)
LD_FLAGS	= -g -std=c11 -Wall

# Compile .c files to .o files
.c.o:
	$(GCC) $(GCC_FLAGS) $<

# Target is the executable
$(EXE): ${C_HEADERS} $(OBJS) $(LIB)
	$(GCC) $(LD_FLAGS) $(OBJS) $(LIB) -o $(EXE)

install.c: install.c.MASTER
	./fixPath install.c mysim-tk

# Clean up the directory
clean:
	rm -f install.c mysim-tk *.o *~ $(EXE) $(SUBMISSION)

#Create tar file for assignment checkin
submission: $(MY_SRC)
	tar -cvf $(SUBMISSION) $(MY_SRC)
