CC = gcc -std=c99
CFLAGS = -Wpedantic -Wall -Wextra
OPTFLAGS = -O3

AR = ar
ARFLAGS = rvs

INCDIR = ./include
SRCDIR = ./src
BINDIR = ./bin
LIBDIR = ./lib
TESTDIR = ./test

INCLUDES = -I $(INCDIR)
LDFLAGS = -L $(LIBDIR)
LDLIBS = -lutil

TARGET = $(BINDIR)/generafile 	\
		 $(BINDIR)/farm 		\
		 $(BINDIR)/collector

.PHONY: all clean

# pattern lib .c -> .o
$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -fPIC -c $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

# first rule
all: $(TARGET)

$(BINDIR)/generafile: $(SRCDIR)/generafile.c
	$(CC) $< -o $@ $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/farm: $(SRCDIR)/masterworker.c $(LIBDIR)/libutil.a
	$(CC) $< -o $@ $(INCLUDES) $(LDFLAGS) $(LDLIBS) $(CFLAGS) $(OPTFLAGS)

$(BINDIR)/collector: $(SRCDIR)/collector.c
	$(CC) $< -o $@ $(INCLUDES) $(CFLAGS) $(OPTFLAGS)

$(LIBDIR)/libutil.a: $(SRCDIR)/libutil.o
	$(AR) $(ARFLAGS) $@ $^

clean:
	rm -f $(TARGET)

cleanall: clean
	find . \( -name "*.a" -o -name "*.o" \) -exec rm -f {} \;