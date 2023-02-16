#
# To compile, type "make"
# To remove files, type "make clean" or "make realclean"
#
# If you want optimization, add -O2 to CFLAGS
CFLAGS := -g -Wall -Werror
LOADLIBES := -lm -lpthread -lpopt
TARGETS := server client_simple client fileset
PLOT_FILES := plot-threads.out plot-requests.out plot-cachesize.out \
	      plot-threads.pdf plot-requests.pdf plot-cachesize.pdf
FILESET := fileset_dir fileset_dir.idx

# Make sure that 'all' is the first target
all: depend $(TARGETS)

clean:
	rm -rf core *.o $(TARGETS) $(PLOT_FILES) run-*.out server-*.log

realclean: clean
	rm -rf *~ *.bak .depend *.log TAGS $(FILESET)

tags:
	etags *.c *.h

server: server.o server_thread.o request.o common.o

client_simple: client_simple.o common.o
client: client.o common.o

fileset: fileset.o common.o

depend:
	$(CC) -MM *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
