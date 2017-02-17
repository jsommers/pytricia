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
import radix
import SubnetTree
import random

if sys.version_info.major == 3:
    xrange = range

def build_tree_subnet(s):
    n = 0
    for i in xrange(256):
        for j in xrange(256):
            # for k in xrange(256):
            # prefix='.'.join([str(i),str(j),str(k),"0"])+'/16'
            prefix='.'.join([str(i),str(j),"0.0"])+'/16'
            s[prefix] = n
            n += 1

def lookups_subnet(s, n=100000):
    for i in xrange(n):
        random.randint(0,255)
        addr='.'.join([str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255))])
        val = s[addr]

def runtestsubnet():
    s = SubnetTree.SubnetTree()
    build_tree_subnet(s)
    lookups_subnet(s)

def build_tree_radix(r):
    n = 0
    for i in xrange(256):
        for j in xrange(256):
            prefix='.'.join([str(i),str(j),"0.0"])
            node = r.add(network=prefix, masklen=24)
            node.data["n"] = n
            n += 1

def lookups_radix(r, n=100000):
    for i in xrange(n):
        random.randint(0,255)
        addr='.'.join([str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255))])
        node = r.search_exact(addr)

def runtestradix():
    r = radix.Radix()
    build_tree_radix(r)
    lookups_radix(r)

def build_tree(pt):
    n = 0
    for i in xrange(256):
        for j in xrange(256):
            # for k in xrange(256):
            # prefix='.'.join([str(i),str(j),str(k),"0"])+'/16'
            prefix='.'.join([str(i),str(j),"0.0"])+'/16'
            pt[prefix] = n
            n += 1

def lookups(pt, n=100000):
    for i in xrange(n):
        random.randint(0,255)
        addr='.'.join([str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255)),str(random.randint(0,255))])
        val = pt[addr]

def runtestpt():
    pt = pytricia.PyTricia(32)    
    build_tree(pt)
    lookups(pt)

def main():
    from timeit import Timer

    t = Timer("runtestpt()", "from __main__ import runtestpt")
    v = t.timeit(number=10)
    v = v / 10.0
    print ("Average execution time for PyTricia:",v)

    t = Timer("runtestradix()", "from __main__ import runtestradix")
    v = t.timeit(number=10)
    v = v / 10.0
    print ("Average execution time for radix:",v)

    t = Timer("runtestsubnet()", "from __main__ import runtestsubnet")
    v = t.timeit(number=10)
    v = v / 10.0
    print ("Average execution time for subnet:",v)

if __name__ == '__main__':
    main()
