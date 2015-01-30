################################################################################
##   Smack namespace makefile
##
##   Copyright (C) 2014 Samsung Electronics Co.
##
##   This program is free software; you can redistribute it and/or
##   modify it under the terms of the GNU General Public License
##   as published by the Free Software Foundation; either version 2
##   of the License, or (at your option) any later version.
##
##   This program is distributed in the hope that it will be useful,
##   but WITHOUT ANY WARRANTY; without even the implied warranty of
##   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##   GNU General Public License for more details.
##
##   You should have received a copy of the GNU General Public License
##   along with this program; if not, write to the Free Software
##   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
##
## Author: Michal Witanowski <m.witanowski@samsung.com>
################################################################################

CC=gcc
CFLAGS=-Wall -O0 -g
TEST_CASES=$(patsubst %.c,%,$(wildcard smack_ns_tc_*.c))

all: smack_ns_launch $(TEST_CASES)

launch: smack_ns_launch.c common.h ../smack_common.h
	$(CC) $(CFLAGS) smack_ns_launch.c -o smack_ns_launch

test_common.o: test_common.c test_common.h common.h ../smack_common.h
	$(CC) $(CFLAGS) -c test_common.c -c

$(TEST_CASES): test_common.o
	$(CC) $(CFLAGS) $@.c test_common.o -o $@

clean:
	rm -f smack_ns_launch $(TEST_CASES) test_common.o
