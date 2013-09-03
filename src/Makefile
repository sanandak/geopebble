CWPROOT=/home/user/su

CC=$(CROSS_COMPILE)gcc
LINK = $(CC)
CFLAGS = -Wall -DLINUX -ggdb -I$(CWPROOT)/include -I../lib
LDFLAGS = -L../lib -lgputils -lm -lrt -pthread # for shm & sem
OBJS = gp_params.o get_gpsStatus.o gp_fns.o


all: geopebble gp_sched gp_gps gp_store gp_base
clean: 
	rm -f *.o geopebble gp_sched gp_store gp_gps gp_base

geopebble: geopebble.o $(OBJS)
	$(LINK) $^ $(LDFLAGS) -o $@

gp_base: gp_base.o $(OBJS)
	$(LINK) $^ $(LDFLAGS) -o $@

gp_qc: gp_qc.o $(OBJS)
	$(LINK) $^ -L../../SU/lib -lcwp -I../../SU/include $(LDFLAGS) -o $@

gp_sched: gp_sched.o $(OBJS)
	$(LINK) $^ $(LDFLAGS) -lm -o $@

gp_store: gp_store.o $(OBJS)
	$(LINK) $^ $(LDFLAGS) -o $@
gp_gps: gp_gps.o gp_gps_parse.o  $(OBJS)
	$(LINK) $^ $(LDFLAGS) -o $@

#testFileMap: testFileMap.o $(OBJS)
#	$(LINK) $^ $(LDFLAGS) -L$(CWPROOT)/lib -lsu -lpar -lcwp -o $@


