CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lz

all: lxp-makedb

lxp-makedb: lxp-makedb.o lxp-util.o
