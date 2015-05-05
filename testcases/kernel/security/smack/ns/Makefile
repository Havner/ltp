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

top_srcdir		?= ../../../../..

include $(top_srcdir)/include/mk/testcases.mk

INSTALL_TARGETS	:= *.sh
INSTALL_TARGETS	+= smack_ns_tc_*
INSTALL_TARGETS	+= mount_test.img

CFLAGS		+= -g -O0
LDLIBS		+= -lcap
LDFLAGS		:= -Wl,--as-needed $(LDLIBS)
TEST_CASES	:= $(patsubst $(abs_srcdir)/%.c,%,$(wildcard $(abs_srcdir)/smack_ns_tc_*.c))
MAKE_TARGETS	:= smack_ns_launch
MAKE_TARGETS	+= $(TEST_CASES)
RM		+= -r

smack_ns_launch: smack_ns_launch.c smack_ns_common.h ../smack_common.h
	$(CC) $(CFLAGS) $< -o $@

test_common.o: test_common.c test_common.h smack_ns_common.h ../smack_common.h
	$(CC) $(CFLAGS) -c test_common.c -o $@

smack_ns_tc_%: smack_ns_tc_%.c test_common.o
	$(CC) $(CFLAGS) $< test_common.o -o $@ $(LDFLAGS)

include $(top_srcdir)/include/mk/generic_leaf_target.mk
