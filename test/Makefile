SOURCES := $(wildcard run-*.c)
HELPERS := $(wildcard helper*.c)
BINS := $(SOURCES:.c=)
CCANDIR := ../ccan/
BASECFLAGS := -ggdb -I $(CCANDIR) -Wall -DFASTCHECK
CFLAGS := -MMD $(BASECFLAGS)
LDLIBS := -lcrypto -lrt
CCAN_OBJS := ccan-noerr.o ccan-tal.o ccan-take.o ccan-list.o ccan-read_write_all.o ccan-htable.o ccan-isaac64.o ccan-tal-str.o ccan-str.o ccan-time.o ccan-strmap.o ccan-hash.o ccan-io-io.o ccan-io-poll.o ccan-timer.o
HELPER_OBJS := $(HELPERS:.c=.o)

VALGRIND=valgrind -q --error-exitcode=99 --track-origins=yes --leak-check=full --show-reachable=yes --suppressions=vg-suppressions

check: $(BINS)
	set -e; for b in $(BINS); do printf "[TEST] %-48s" $$b ; $(VALGRIND) ./$$b; echo OK ; done

bins: $(BINS)

update-mocks-%: %
	@set -e; trap "rm -f mocktmp.$*.*" EXIT; \
	START=`fgrep -n '/* AUTOGENERATED MOCKS START */' $* | cut -d: -f1`;\
	END=`fgrep -n '/* AUTOGENERATED MOCKS END */' $* | cut -d: -f1`; \
	if [ -n "$$START" ]; then \
	  echo $*: ; \
	  head -n $$START $* > mocktmp.$*.new; \
	  (cat mocktmp.$*.new; tail -n +$$END $*) > mocktmp.$*.test.c; \
	  if ! $(CC) $(BASECFLAGS) mocktmp.$*.test.c -o mocktmp.$*.out $(HELPER_OBJS) $(CCAN_OBJS) $(LDLIBS) 2>mocktmp.$*.err; then \
	    ./mockup.sh < mocktmp.$*.err >> mocktmp.$*.new; \
	    sed -n 's,.*Generated stub for \(.*\) .*,\t\1,p' < mocktmp.$*.new; \
          fi; \
	  tail -n +$$END $* >> mocktmp.$*.new; mv mocktmp.$*.new $*; \
	fi

$(SOURCES:%=update-mocks-%): $(CCAN_OBJS) $(HELPER_OBJS)

update-mocks: $(SOURCES:%=update-mocks-%)

$(BINS:=.o) : %.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(BINS) : $(HELPER_OBJS) $(CCAN_OBJS)

ccan-noerr.o: $(CCANDIR)/ccan/noerr/noerr.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal.o: $(CCANDIR)/ccan/tal/tal.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-take.o: $(CCANDIR)/ccan/take/take.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-list.o: $(CCANDIR)/ccan/list/list.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-read_write_all.o: $(CCANDIR)/ccan/read_write_all/read_write_all.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-hash.o: $(CCANDIR)/ccan/hash/hash.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-htable.o: $(CCANDIR)/ccan/htable/htable.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-str.o: $(CCANDIR)/ccan/str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-tal-str.o: $(CCANDIR)/ccan/tal/str/str.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-isaac64.o: $(CCANDIR)/ccan/isaac/isaac64.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-time.o: $(CCANDIR)/ccan/time/time.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-strmap.o: $(CCANDIR)/ccan/strmap/strmap.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-io-io.o: $(CCANDIR)/ccan/io/io.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-io-poll.o: $(CCANDIR)/ccan/io/poll.c
	$(CC) $(CFLAGS) -c -o $@ $<
ccan-timer.o: $(CCANDIR)/ccan/timer/timer.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BINS) *.o *.d

.PHONY:
	bins

-include *.d
