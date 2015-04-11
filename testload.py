from __future__ import print_function

import unittest
import pytricia
import gzip

def dumppyt(t):
    print ("\nDumping Pytricia")
    for x in t.keys():
        print ("\t {}: {}".format(x,t[x]))

class PyTriciaLoadTest(unittest.TestCase):
    def testLoaditUp(self):
        pyt = pytricia.PyTricia()

        # load routeviews prefix -> AS mappings; all of them.
        print ("loading routeviews data")
        inf = gzip.GzipFile('routeviews-rv2-20141202-1200.pfx2as.gz', 'r')
        for line in inf:
            ipnet,prefix,asn = line.split()
            network = '{}/{}'.format(ipnet.decode(), prefix.decode())
            pyt[network] = asn.decode()
            # x = pyt.insert(network, asn) # segfault?!
        inf.close()

        # verify that everything was stuffed into pyt correctly
        print ("verifying routeviews data")
        inf = gzip.GzipFile('routeviews-rv2-20141202-1200.pfx2as.gz', 'r')
        for line in inf:
            ipnet,prefix,asn = line.split()
            asn = asn.decode()
            network = '{}/{}'.format(ipnet.decode(), prefix.decode())
            self.assertTrue(network in pyt)
            self.assertTrue(ipnet.decode() in pyt)
            self.assertEqual(pyt[network], asn)
        inf.close()

        # dump everything out...
        dumppyt(pyt)

if __name__ == '__main__':
    print ("This is a fairly long-running test.  Be patient.")
    unittest.main()
