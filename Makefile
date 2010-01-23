CC=gcc
CFLAGS=-Wall
LDFLAGS=-lz

all: loki-makedb

loki-makedb: loki-makedb.o util.o
