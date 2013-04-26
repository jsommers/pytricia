import unittest
import pytricia

def dumppyt(t):
    print ("\nDumping Pytricia")
    for x in t.keys():
        print ("\t {}: {}".format(x,t[x]))
    
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
      
    def testBasic(self):
        pyt = pytricia.PyTricia()
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.1.0.0/16"] = 'b'
        # dumppyt(pyt)

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

        self.assertListEqual(['10.0.0.0/8','10.1.0.0/16'], pyt.keys())

    def testMoreComplex(self):
        pyt = pytricia.PyTricia()
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.1.0/24"] = 'c'
        pyt["0.0.0.0/0"] = 'default route'

        # dumppyt(pyt)
        # FIXME: make some more tests
        self.assertFalse(pyt.has_key('1.0.0.0/8'))

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
        # self.assertListEqual(['0.0.0.0/0', '10.0.0.0/8','10.1.0.0/16','10.0.1.0/24'], list(iter(pyt)))

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

    def testIteration3(self):
        pyt = pytricia.PyTricia()
        pyt["10.1.0.0/16"] = 'b'
        pyt["10.0.0.0/8"] = 'a'
        pyt["10.0.1.0/24"] = 'c'
        x = iter(pyt)
        del pyt["10.0.1.0/24"]
        self.assertIsNotNone(next(x))
        self.assertIsNotNone(next(x))
        self.assertRaises(StopIteration, next, x)
        self.assertRaises(StopIteration, next, x)


# tests should cover:
# get w,w/o default, has_key, keys, in, [] access, [] assigment


if __name__ == '__main__':
    unittest.main()

