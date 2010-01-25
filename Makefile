CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lz

all: loki-makedb

loki-makedb: loki-makedb.o util.o
