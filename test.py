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

    def testIteration(self):
        pyt = pytricia.PyTricia()
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.0.1.0/24"] = 'c'
        pyt["0.0.0.0/0"] = 'default route'
        self.assertListEqual(sorted(['0.0.0.0/0', '10.0.0.0/8','10.1.0.0/16','10.0.1.0/24']), sorted(list(pyt.__iter__())))
        # self.assertListEqual(sorted(['0.0.0.0/0', '10.0.0.0/8','10.1.0.0/16','10.0.1.0/24']), sorted(list(iter(pyt))))

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

    def testGet(self):
        pyt = pytricia.PyTricia()
        pyt.insert("10.0.0.0/8", "a")
        self.assertEqual(pyt.get("10.0.0.0/8", "X"), "a")
        self.assertEqual(pyt.get("11.0.0.0/8", "X"), "X")

if __name__ == '__main__':
    unittest.main()

