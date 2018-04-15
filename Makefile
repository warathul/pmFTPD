#
# pmFTPD
# (c) 1999 Philipp Knirsch
#
# pmFTPD comes with complete free source code. pmFTPD is available under
# the terms of the GNU General Public License.
#
# For further details see file COPYING. 
#
# File:         Makefile
# Purpose:      Makefile for the whole server
# Author:       Philipp Knirsch
# Created:      10.2.1999
# Updated:      25.2.1999
#
# Doku:         To handle the whole project we use a simple makefile to
#               only compile necessary code.
#



.SUFFIXES:
.SUFFIXES: .c .o



#
# Object files to be compiled.
#
OBJECTS = pmFTPD.o pmNetwork.o pmCommand.o pmConfig.o\
	  pmAuth.o pmAuthOracle.o pmAuthDB2.o snprintf.o



#
# Compile in support for Oracle. Please set the ORACLE_HOME correctly.
#
#ORACLE_HOME = /home/oracle/orac
#CFORACLE = -DORACLE -I$(ORACLE_HOME)/rdbms/demo
#LDORACLE = -L$(ORACLE_HOME)/lib -L$(ORACLE_HOME)/rdbms/lib $(ORACLE_HOME)/lib/libclntsh.so -lcommon -lsql -lcore4 -lnlsrtl3

#
# Compile in support for DB2. Please set the DB2_HOME correctly.
#
#DB2_HOME = /home/db2inst1/sqllib
#CFDB2 = -DDB2 -I$(DB2_HOME)/include
#LDDB2 = -L/home/db2inst1/sqllib/lib -ldb2



#
# Misc definitions, inlcudes and libraries for various systems. SGI e.g.
# doesn't have a snprintf() function. That really s***s by the way.
#
CFMISC = -DHAVE_SNPRINTF -DHAVE_VSNPRINTF
LDMISC = -lcrypt



#
# General UNIX configuration
#
CC = cc
CFLAGS = -O $(CFORACLE) $(CFDB2) $(CFMISC)
LDFLAGS = $(LDORACLE) $(LDDB2) $(LDMISC)



all: pmFTPD

pmFTPD: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o pmFTPD

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -rf core *.o pmFTPD
