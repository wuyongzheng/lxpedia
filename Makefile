CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lz

all: lxp-makedb test-encoding

lxp-makedb: lxp-makedb.o lxp-util.o
test-encoding: test-encoding.o lxp-util.o
