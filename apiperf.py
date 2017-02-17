#
# This file is part of Pytricia.
# Joel Sommers <jsommers@colgate.edu>
# 
# Pytricia is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# Pytricia is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public License
# along with Pytricia.  If not, see <http://www.gnu.org/licenses/>.
#

from __future__ import print_function
import pytricia
import sys

pyt = pytricia.PyTricia(64)

if sys.version_info.major == 3:
    from ipaddress import IPv4Network, IPv6Network
    ip4 = IPv4Network('10.0.1.2/16', strict=False)
    ip6 = IPv6Network('fe80:beef::/64')
    ip4_bytes = ip4.network_address.packed
    ip6_bytes = ip6.network_address.packed

def do_insert():
    pyt.insert('10.0.1.2/16', 'abc')    
    pyt.insert('fe80:beef::/64', 'abc')

def do_indexassign():
    pyt['10.0.1.2/16'] = 'abc'
    pyt['fe80:beef::/64'] = 'abc'

def do_insertstr():
    pyt.insert('10.0.1.2/16', 'abc')
    pyt.insert('fe80:beef::/64', 'abc')

def do_insertstr_separate_prefix():
    pyt.insert('10.0.1.2', 16, 'abc')
    pyt.insert('fe80:beef::', 64, 'abc')

def do_insertbytes():
    pyt.insert(ip4_bytes, 16, 'abc')
    pyt.insert(ip6_bytes, 64, 'abc')

def do_insertipaddr():
    pyt.insert(ip4, 'abc')
    pyt.insert(ip6, 'abc')

def main():
    from timeit import Timer
    iterations = 10000000

    expts = ['do_insert', 'do_indexassign', 'do_insertstr', 'do_insertstr_separate_prefix']
    if sys.version_info.major == 3:
        expts.append('do_insertbytes')
        expts.append('do_insertipaddr')

    for fn in expts:
        t = Timer("{}()".format(fn), "from __main__ import {}".format(fn))
        v = t.timeit(number=iterations)
        v = v / float(iterations)
        print ("{:.08f}s: average execution time for {}".format(v, fn))

if __name__ == '__main__':
    main()
