from __future__ import print_function

import unittest
import pytricia
import gzip

def dumppyt(t):
    print ("\nDumping Pytricia")
    for x in t.keys():
        print ("\t {}: {}".format(x,t[x]))

class PyTriciaLoadTest(unittest.TestCase):
    _files = ['routeviews-rv2-20160202-1200.pfx2as.gz', 'routeviews-rv6-20160202-1200.pfx2as.gz']

    def testLoaditUp(self):
        pyt = pytricia.PyTricia(128)

        # load routeviews prefix -> AS mappings; all of them.
        for f in PyTriciaLoadTest._files:
            print ("loading routeviews data from {}".format(f))
            with gzip.GzipFile(f, 'r') as inf:
                for line in inf:
                    ipnet,prefix,asn = line.split()
                    network = '{}/{}'.format(ipnet.decode(), prefix.decode())
                    pyt[network] = asn.decode()

        # verify that everything was stuffed into pyt correctly
        for f in PyTriciaLoadTest._files:
            print ("verifying routeviews data from {}".format(f))
            with gzip.GzipFile(f, 'r') as inf:
                ipnet,prefix,asn = line.split()
                asn = asn.decode()
                network = '{}/{}'.format(ipnet.decode(), prefix.decode())
                self.assertTrue(network in pyt)
                self.assertTrue(ipnet.decode() in pyt)
                self.assertEqual(pyt[network], asn)

        # dump everything out...
        # dumppyt(pyt)

        print("removing everything.")
        netlist = [ n for n in pyt.keys() ]
        for net in netlist:
            del pyt[net]

        self.assertEqual(len(pyt), 0)


if __name__ == '__main__':
    unittest.main()
