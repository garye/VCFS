#
# Makefile for vcfs 
#

CC=gcc
COPT= -g -O2

CFLAGS=$(COPT)

VCFS_SRCS=cvs_cmds.c vcfs_fh.c vcfs_nfs.c vcfs.c utils.c
VCFS_OBJS=cvs_cmds.o vcfs_fh.o vcfs_nfs.o vcfs.o utils.o cvstool_proc.o cvstool_svc.o cvstool_xdr.o
OTHER_OBJS=nfsproto_xdr.o nfsproto_svr.o

OTHERS = nfsproto.h nfsproto_svr.c nfsproto_xdr.c

TOOL_OBJS=cvstool.o cvstool_clnt.o cvstool_xdr.o

default: vcfs cvstool
	@echo

vcfs: $(VCFS_OBJS) $(OTHER_OBJS)
	$(CC) $(COPT) $(VCFS_OBJS) $(OTHER_OBJS) -o vcfsd

cvstool: $(TOOL_OBJS)
	$(CC) $(COPT) $(TOOL_OBJS) -o cvstool

clean_vcfs: 
	rm -f $(VCFS_OBJS) $(OTHER_OBJS) vcfsd

clean_cvstool:
	rm -f $(TOOL_OBJS) cvstool

clean: clean_vcfs clean_cvstool






