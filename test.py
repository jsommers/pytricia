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

import unittest
import pytricia
import socket
import struct
import sys

def dumppyt(t):
    print ("\nDumping Pytricia")
    for x in t.keys():
        print ("\t",x,t[x])
    
class PyTriciaTests(unittest.TestCase):
    def testInit(self):
        with self.assertRaises(ValueError) as cm:
            t = pytricia.PyTricia('a')
        self.assertIsInstance(cm.exception, ValueError)

        with self.assertRaises(ValueError) as cm:
            t = pytricia.PyTricia(-1)
        self.assertIsInstance(cm.exception, ValueError)

        self.assertIsInstance(pytricia.PyTricia(1), pytricia.PyTricia)
        self.assertIsInstance(pytricia.PyTricia(128), pytricia.PyTricia)

        with self.assertRaises(ValueError) as cm:
            t = pytricia.PyTricia(129)
        self.assertIsInstance(cm.exception, ValueError)

        t = pytricia.PyTricia(64, socket.AF_INET6)
        self.assertIsInstance(t, pytricia.PyTricia)

        with self.assertRaises(ValueError) as cm:
            t = pytricia.PyTricia(64, socket.AF_INET6+1)

      
    def testBasic(self):
        pyt = pytricia.PyTricia()
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.1.0.0/16"] = 'b'

        self.assertEqual(pyt["10.0.0.0/8"], 'a')
        self.assertEqual(pyt["10.1.0.0/16"], 'b')
        self.assertEqual(pyt["10.0.0.0"], 'a')
        self.assertEqual(pyt["10.1.0.0"], 'b')
        self.assertEqual(pyt["10.1.0.1"], 'b')
        self.assertEqual(pyt["10.0.0.1"], 'a')

        self.assertTrue('10.0.0.0' in pyt)
        self.assertTrue('10.1.0.0' in pyt)
        self.assertTrue('10.0.0.1' in pyt)
        self.assertFalse('9.0.0.0' in pyt)
        self.assertFalse('0.0.0.0' in pyt)

        self.assertTrue(pyt.has_key('10.0.0.0/8'))
        self.assertTrue(pyt.has_key('10.1.0.0/16'))
        self.assertFalse(pyt.has_key('10.2.0.0/16'))
        self.assertFalse(pyt.has_key('9.0.0.0/8'))
        self.assertFalse(pyt.has_key('10.0.0.1'))

        self.assertTrue(pyt.has_key('10.0.0.0/8'))
        self.assertTrue(pyt.has_key('10.1.0.0/16'))
        self.assertFalse(pyt.has_key('10.2.0.0/16'))
        self.assertFalse(pyt.has_key('9.0.0.0/8'))
        self.assertFalse(pyt.has_key('10.0.0.0'))

        self.assertListEqual(sorted(['10.0.0.0/8','10.1.0.0/16']), sorted(pyt.keys()))

    def testNonStringKey(self):
        pyt = pytricia.PyTricia()

        # insert as string
        pyt['10.1.2.3/24'] = 'abc'

        # lookup as string
        for i in range(256):
            self.assertEqual(pyt['10.1.2.{}'.format(i)], 'abc')

        # lookup as bytes (or, ugh, another str in python2)
        b = socket.inet_aton('10.1.2.3') 
        self.assertEqual(pyt[b], 'abc')

        # bytes in py3k.  python2 stinks.
        if sys.version_info.major == 3 and sys.version_info.minor >= 4:
            i = b[0] * 2**24 + b[1] * 2**16 + b[2] * 2**8 
            for j in range(256):
                self.assertEqual(pyt[i+j], 'abc')

        if sys.version_info.major == 2:
            # longs only exist in python2.  it stinks.
            b = struct.unpack('4b',socket.inet_aton('10.1.2.3'))
            i = b[0] * 2**24 + b[1] * 2**16 + b[2] * 2**8 
            self.assertEqual(pyt[long(i)], 'abc')

            # i, = struct.unpack('i', b)
            for j in range(256):
                self.assertEqual(pyt[i+j], 'abc')

        # lookup as str
        self.assertEqual(pyt['10.1.2.99'], 'abc')

        # lookup as ipaddress objects
        if sys.version_info.major == 3 and sys.version_info.minor >= 4:
            import ipaddress
            ipaddr = ipaddress.IPv4Address("10.1.2.47")
            self.assertEqual(pyt[ipaddr], 'abc')

            ipnet = ipaddress.IPv4Network("10.1.2.0/24")
            self.assertEqual(pyt[ipnet], 'abc')

            with self.assertRaises(KeyError) as cm:
                ipaddr = ipaddress.IPv6Address("fe01::1")
                self.assertIsNone(pyt[ipaddr])

            with self.assertRaises(KeyError) as cm:
                ipnet = ipaddress.IPv6Network("fe01::1/64", strict=False)
                self.assertIsNone(pyt[ipnet])

        with self.assertRaises(KeyError) as cm:
            pyt["fe01::1/64"]

        with self.assertRaises(KeyError) as cm:
            pyt["fe01::1"]

        with self.assertRaises(ValueError) as cm:
            pyt[""]

        with self.assertRaises(ValueError) as cm:
            pyt["apple"]

        with self.assertRaises(KeyError) as cm:
            pyt[2**65] 

    def testMoreComplex(self):
        pyt = pytricia.PyTricia()
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.1.0/24"] = 'c'
        pyt["0.0.0.0/0"] = 'default route'


        self.assertEqual(pyt['10.0.0.1/32'], 'a')
        self.assertEqual(pyt['10.0.0.1'], 'a')

        self.assertFalse(pyt.has_key('1.0.0.0/8'))
        # with 0.0.0.0/0, everything should be 'in'
        for i in range(256):
            self.assertTrue('{}.2.3.4'.format(i) in pyt)
            # default for all but 10.0.0.0/8
            if i != 10:
                self.assertEqual(pyt['{}.2.3.4'.format(i)], 'default route')
        
    def testDelete(self):
        pyt = pytricia.PyTricia(64)
        pyt.insert("fe80:abcd::0/96", "xyz")
        pyt.insert("fe80:beef::0", 96, "abc")
        self.assertEqual(pyt.get("fe80:abcd::0/96"), "xyz")
        self.assertEqual(pyt.get("fe80:beef::0/96"), "abc")
        pyt.delete("fe80:abcd::/96")
        pyt.delete("fe80:beef::/96")
        with self.assertRaises(KeyError) as cm:
            pyt.delete("fe80:abcd::/96")
        with self.assertRaises(KeyError) as cm:
            pyt.delete("fe80:beef::/96")
        self.assertEqual(len(pyt),0)

    def testInsertRemove(self):
        pyt = pytricia.PyTricia()

        pyt['10.0.0.0/8'] = list(range(10))
        self.assertListEqual(['10.0.0.0/8'], pyt.keys())
        pyt.delete('10.0.0.0/8')
        self.assertListEqual([], pyt.keys())
        self.assertFalse(pyt.has_key('10.0.0.0/8'))

        pyt['10.0.0.0/8'] = list(range(10))
        self.assertListEqual(['10.0.0.0/8'], pyt.keys())
        pyt.delete('10.0.0.0/8')
        self.assertListEqual([], pyt.keys())
        self.assertFalse(pyt.has_key('10.0.0.0/8'))

        pyt['10.0.0.0/8'] = list(range(10))
        self.assertListEqual(['10.0.0.0/8'], pyt.keys())
        del pyt['10.0.0.0/8']
        self.assertListEqual([], pyt.keys())
        self.assertFalse(pyt.has_key('10.0.0.0/8'))

        with self.assertRaises(KeyError) as cm:
            t = pytricia.PyTricia()
            pyt['10.0.0.0/8'] = list(range(10))
            t.delete('10.0.0.0/9')
        self.assertIsInstance(cm.exception, KeyError)

    def testIp6(self):
        pyt = pytricia.PyTricia(128)
        addrstr = "fe80::0/32" 
        pyt[addrstr] = "hello, ip6"
        self.assertEqual(pyt["fe80::1"], "hello, ip6")

        if sys.version_info.major == 3 and sys.version_info.minor >= 4:
            from ipaddress import IPv6Address, IPv6Network
            addr = IPv6Address("fe80::1")
            xnet = IPv6Network("fe80::1/32", strict=False)
            self.assertEqual(pyt[addr], 'hello, ip6')
            self.assertEqual(pyt[xnet], 'hello, ip6')

    def testIteration(self):
        pyt = pytricia.PyTricia()
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.0.1.0/24"] = 'c'
        pyt["0.0.0.0/0"] = 'default route'
        self.assertListEqual(sorted(['0.0.0.0/0', '10.0.0.0/8','10.1.0.0/16','10.0.1.0/24']), sorted(list(pyt.__iter__())))

    def testIteration2(self):
        pyt = pytricia.PyTricia()
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.0.1.0/24"] = 'c'
        x = iter(pyt)
        self.assertIsNotNone(next(x))
        self.assertIsNotNone(next(x))
        self.assertIsNotNone(next(x))
        self.assertRaises(StopIteration, next, x)
        self.assertRaises(StopIteration, next, x)

    def testMultipleIter(self):
        pyt = pytricia.PyTricia()
        pyt["10.0.0.0/8"] = 0
        for i in range(10):
            self.assertListEqual(['10.0.0.0/8'], list(pyt))
            self.assertListEqual(['10.0.0.0/8'], list(pyt.keys()))

    def testInsert(self):
        pyt = pytricia.PyTricia()
        val = pyt.insert("10.0.0.0/8", "a")
        self.assertIs(val, None)
        self.assertEqual(len(pyt), 1)
        self.assertEqual(pyt["10.0.0.0/8"], "a")
        self.assertIn("10.0.0.1", pyt)

    def testInsert2(self):
        pyt = pytricia.PyTricia()
        val = pyt.insert("10.0.0.0", 8, "a")
        self.assertIs(val, None)
        self.assertEqual(len(pyt), 1)
        self.assertEqual(pyt["10.0.0.0/8"], "a")
        self.assertIn("10.0.0.1", pyt)

    def testInsert3(self):
        pyt = pytricia.PyTricia(128)
        val = pyt.insert("fe80::aebc:32ff:fec2:b659/64", "a")
        self.assertIs(val, None)
        self.assertEqual(len(pyt), 1)
        self.assertEqual(pyt["fe80::aebc:32ff:fec2:b659/64"], "a")
        self.assertIn("fe80::aebc:32ff:fec2:b659", pyt)

    def testInsert4(self):
        pyt = pytricia.PyTricia(64)
        with self.assertRaises(ValueError) as cm:
            val = pyt.insert("fe80::1") # should raise an exception

    def testGet(self):
        pyt = pytricia.PyTricia()
        pyt.insert("10.0.0.0/8", "a")
        self.assertEqual(pyt.get("10.0.0.0/8", "X"), "a")
        self.assertEqual(pyt.get("11.0.0.0/8", "X"), "X")

    def testGet2(self):
        pyt = pytricia.PyTricia(64)
        pyt.insert("fe80:abcd::0/96", "xyz")
        pyt.insert("fe80:beef::0", 96, "abc")
        self.assertEqual(pyt.get("fe80:abcd::0/96"), "xyz")
        self.assertEqual(pyt.get("fe80:beef::0/96"), "abc")

        addrlist = sorted([ x for x in pyt.keys() ])
        self.assertEqual(addrlist, ['fe80:abcd::/96', 'fe80:beef::/96'])

    def testGet3(self):
        if sys.version_info.major == 3 and sys.version_info.minor >= 4:
            from ipaddress import IPv6Address, IPv6Network

            pyt = pytricia.PyTricia(128)
            
            pyt.insert(IPv6Network('2001:218:200e::/56'), "def")
            pyt.insert(IPv6Network("fe80:abcd::0/96"), "xyz")
            pyt.insert(IPv6Address("fe80:beef::"), 96, "abc")

            addrlist = sorted([ x for x in pyt.keys() ])
            self.assertEqual(addrlist, ['2001:218:200e::/56', 'fe80:abcd::/96', 'fe80:beef::/96'])

            self.assertEqual(pyt.get("fe80:abcd::0/96"), "xyz")
            self.assertEqual(pyt.get("fe80:beef::0/96"), "abc")
            self.assertEqual(pyt.get(IPv6Network("fe80:abcd::0/96")), "xyz")
            self.assertEqual(pyt.get(IPv6Network("fe80:beef::0/96")), "abc")

    def testGetKey(self):
        pyt = pytricia.PyTricia()
        pyt.insert("10.0.0.0/8", "a")
        self.assertEqual(pyt.get_key("10.0.0.0/8"), "10.0.0.0/8")
        self.assertEqual(pyt.get_key("10.42.42.42"), "10.0.0.0/8")
        self.assertIsNone(pyt.get_key("11.0.0.0/8"))
        pyt.insert("10.42.0.0/16", "b")
        self.assertEqual(pyt.get_key("10.42.42.42"), "10.42.0.0/16")

    def testGetKeyIP6(self):
        pyt = pytricia.PyTricia(128)
        pyt.insert("2001:db8:10::/48", "a")
        self.assertEqual(pyt.get_key("2001:db8:10::/48"), "2001:db8:10::/48")
        self.assertEqual(pyt.get_key("2001:db8:10:42::1"), "2001:db8:10::/48")
        self.assertIsNone(pyt.get_key("2001:db8:11::/48"))
        pyt.insert("2001:db8:10:42::/64", "b")
        self.assertEqual(pyt.get_key("2001:db8:10:42::1"), "2001:db8:10:42::/64")

    def testChildren(self):
        pyt = pytricia.PyTricia()
        pyt.insert("42.0.0.0/8", "0")
        pyt.insert("42.100.0.0/16", "1")
        pyt.insert("10.0.0.0/8", "a")
        pyt.insert("10.100.0.0/16", "b")
        pyt.insert("10.100.100.0/24", "c")
        pyt.insert("10.101.0.0/16", "d")
        self.assertListEqual(sorted(["10.100.0.0/16", "10.100.100.0/24", "10.101.0.0/16"]), sorted(pyt.children("10.0.0.0/8")))
        self.assertListEqual(["10.100.100.0/24"], pyt.children("10.100.0.0/16"))
        self.assertListEqual([], pyt.children("10.100.100.0/24"))
        with self.assertRaises(KeyError) as cm:
            pyt.children("10.42.42.0/24")
        self.assertIsInstance(cm.exception, KeyError)

    def testChildrenIp6(self):
        pyt = pytricia.PyTricia(128)
        pyt.insert("2001:db8:42::/48", "0")
        pyt.insert("2001:db8:42:100::/64", "1")
        pyt.insert("2001:db8:10::/48", "a")
        pyt.insert("2001:db8:10:100::/64", "b")
        pyt.insert("2001:db8:10:100:100::/96", "c")
        pyt.insert("2001:db8:10:101::/64", "d")
        self.assertListEqual(sorted(["2001:db8:10:100::/64", "2001:db8:10:100:100::/96", "2001:db8:10:101::/64"]), sorted(pyt.children("2001:db8:10::/48")))
        self.assertListEqual(["2001:db8:10:100:100::/96"], pyt.children("2001:db8:10:100::/64"))
        self.assertListEqual([], pyt.children("2001:db8:10:100:100::/96"))
        with self.assertRaises(KeyError) as cm:
            pyt.children("2001:db8:10:42:42::/96")
        self.assertIsInstance(cm.exception, KeyError)

    def testParent(self):
        pyt = pytricia.PyTricia()
        pyt.insert("10.0.0.0/8", "a")
        pyt.insert("10.100.0.0/16", "b")
        pyt.insert("10.100.100.0/24", "c")
        self.assertIsNone(pyt.parent("10.0.0.0/8"))
        self.assertEqual("10.0.0.0/8", pyt.parent("10.100.0.0/16"))
        self.assertEqual("10.100.0.0/16", pyt.parent("10.100.100.0/24"))
        with self.assertRaises(KeyError) as cm:
            pyt.parent("10.42.42.0/24")
        self.assertIsInstance(cm.exception, KeyError)

    def testParentIP6(self):
        pyt = pytricia.PyTricia(128)
        pyt.insert("2001:db8:10::/48", "a")
        pyt.insert("2001:db8:10:100::/64", "b")
        pyt.insert("2001:db8:10:100:100::/96", "c")
        self.assertIsNone(pyt.parent("2001:db8:10::/48"))
        self.assertEqual("2001:db8:10::/48", pyt.parent("2001:db8:10:100::/64"))
        self.assertEqual("2001:db8:10:100::/64", pyt.parent("2001:db8:10:100:100::/96"))
        with self.assertRaises(KeyError) as cm:
            pyt.parent("2001:db8:42:42::/64")
        self.assertIsInstance(cm.exception, KeyError)

    def testExceptions(self):
        pyt = pytricia.PyTricia(32)
        with self.assertRaises(ValueError) as cm:
            pyt.insert("1.2.3/24", "a")

        with self.assertRaises(KeyError) as cm:
            pyt["1.2.3.0/24"] 

        with self.assertRaises(ValueError) as cm:
            pyt["1.2.3/24"] 

        with self.assertRaises(ValueError) as cm:
            pyt.get("1.2.3/24")

        with self.assertRaises(ValueError) as cm:
            pyt.delete("1.2.3/24")

        with self.assertRaises(KeyError) as cm:
            pyt.delete("1.2.3.0/24")

        self.assertFalse(pyt.has_key('1.2.3.0/24'))
        with self.assertRaises(ValueError) as cm:
            pyt.has_key('1.2.3/24')

        self.assertFalse('1.2.3.0/24' in pyt)

    def testRaw(self):
        pyt = pytricia.PyTricia(32, socket.AF_INET, True)
        prefixes = [
            (b'\x01\x02\x00\x00', 16),
            (b'\x01\x02\x02\x00', 24),
            (b'\x01\x02\x03\x00', 24),
            (b'\x01\x02\x03\x04', 32)
        ]
        values = ["A", "B", "C", "D"]
        for prefix, value in zip(prefixes, values):
            pyt.insert(prefix, value)

        with self.assertRaises(ValueError) as cm:
            pyt.insert((b'invalid', 24), "Z")
        self.assertEqual(pyt.get_key((b'\x01\x02\x02\x02', 30)), (b'\x01\x02\x02\x00', 24))
        self.assertListEqual(sorted(pyt.keys()), sorted(prefixes))
        self.assertEqual(pyt.parent((b'\x01\x02\x03\x04', 32)), (b'\x01\x02\x03\x00', 24))
        self.assertListEqual(list(pyt.children((b'\x01\x02\x03\x00', 24))), [(b'\x01\x02\x03\x04', 32)])
        self.assertListEqual(sorted(list(pyt)), sorted(prefixes))

    def testRawIP6(self):
        pyt = pytricia.PyTricia(128, socket.AF_INET6, True)
        prefixes = [
            (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x00\x00', 96+16),
            (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x02\x00', 96+24),
            (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x00', 96+24),
            (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x04', 96+32)
        ]
        values = ["A", "B", "C", "D"]
        for prefix, value in zip(prefixes, values):
            pyt.insert(prefix, value)

        self.assertEqual(pyt.get_key((b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x02\x02', 96+30)), (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x02\x00', 96+24))
        self.assertListEqual(sorted(pyt.keys()), sorted(prefixes))
        self.assertEqual(pyt.parent((b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x04', 96+32)), (b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x00', 96+24))
        self.assertListEqual(list(pyt.children((b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x00', 96+24))), [(b'\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\xAA\xBB\xCC\xDD\x01\x02\x03\x04', 96+32)])
        self.assertListEqual(sorted(list(pyt)), sorted(prefixes))

if __name__ == '__main__':
    unittest.main()

