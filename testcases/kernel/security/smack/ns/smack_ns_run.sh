#!/bin/bash
#
#   Copyright (C) 2014 Samsung Electronics Co.
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version 2
#   of the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
#
# Smack namespace test launcher
#
# Author: Michal Witanowski <m.witanowski@samsung.com>
#

if [[ $EUID -ne 0 ]]; then
   echo "You must be root to run this script!" 1>&2
   exit 1
fi

echo "Running test: $1..."

# root / fake root
./smack_ns_launch --uid=0 $1
./smack_ns_launch -I --uid=1000 --mapped-uid=0 $1
./smack_ns_launch -S --uid=0 $1
./smack_ns_launch -IS --uid=1000 --mapped-uid=0 $1

# non root
./smack_ns_launch --uid=1000 $1
./smack_ns_launch -I --uid=1000 --mapped-uid=5000 $1
./smack_ns_launch -S --uid=1000 $1
./smack_ns_launch -IS --uid=1000 --mapped-uid=5000 $1
