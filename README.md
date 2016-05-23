# pytricia: an IP address lookup module for Python

Pytricia is a new python module to store IP prefixes in a patricia tree. 
It's based on Dave Plonka's modified patricia tree code, and has three things 
to recommend it over related modules (including py-radix and SubnetTree): 

 1. it's faster (see below),
 1. it works in Python 3, and 
 1. there are a few nicer library features for manipulating the structure.

Pytricia is released under terms of the GPLv2.

# Building 

Building pytricia is done in the standard pythonic way: 

    python setup.py build
    python setup.py install

This code is beta quality at present but has been tested on OS X 10.11 and Ubuntu 14.04 (both 64 bit) and Python 2.7.6 and Python 3.5.1.

# Examples

Create a pytricia object and load a couple prefixes into it:

    >>> import pytricia
    >>> pyt = pytricia.PyTricia()
    >>> pyt["10.0.0.0/8"] = 'a'
    >>> pyt["10.1.0.0/16"] = 'b'
    >>> len(pyt)
    2
    >>> 

The ``PyTricia`` class takes an optional parameter, which is the maximum number of bits to consider when constructing the trie.  By default, the number of bits is 32.  For IPv6, you can set this value higher (up to 128):

    >>> import pytricia
    >>> pyt = pytricia.PyTricia(128)
    >>> pyt["fe80::/64"] = 'a'
    >>> pyt["dead::/32"] = 'b'
    >>> len(pyt)
    2
    >>> 

IP prefixes and addresses can be expressed in a few different ways:
  * The most obvious way is as a string (as in the examples above).  
  * For IPv4, an integer may also be used (just for an address, not a prefix, somewhat obviously).
  * A bytes object may also be used, with a length of 4 bytes (IPv4) or 16 bytes (IPv6).  As with using an int for IPv4, this option is mostly useful for expressing an individual address, not a prefix.
  * For Python 3.4 and later, an address or network using the ``ipaddress`` module can also be used.  In particular, ``IPv4Address`` and ``IPv4Network`` objects can be used, as well as ``IPv6Address`` and ``IPv4Network``.

The ``insert`` method can also be used to add prefixes/values to a PyTricia object.  This method returns ``None``.

    >>> pyt.insert("10.2.0.0/16", "c")

The ``insert`` method can optionally accept three parameters, where the first parameter is an address, the second parameter is the prefix length, and the third parameter is some object to be associated with the network prefix:

    >>> import pytricia
    >>> from ipaddress import IPv6Address, IPv6Network
    >>> pyt = pytricia.PyTricia(128)
    >>> pyt.insert(IPv6Address("2001:218:200e:abc::1"), 56, "hello!")
    >>> pyt.insert(IPv6Network("2001:218:200e::/56"), "halo!")    
    >>> pyt.insert(bytes([10,0,1,0]), 24, "ip?")
    >>> pyt.keys()
    ['10.0.1.0/24', '2001:218:200e::/56', '2001:218:200e:abc::1/56']
    >>> 

Use standard dictionary-like access to do longest prefix match lookup:

    >>> pyt["10.0.0.0/8"]
    a
    >>> pyt["10.1.0.0/16"]
    b
    >>> pyt["10.1.0.0/24"]
    b

Alternatively, use the ``get`` method:

    >>> pyt.get("10.1.0.0/16")
    'b'
    >>> pyt.get("10.1.0.0/24")
    'b'
    >>> pyt.get("10.1.0.0/32")
    'b'
    >>> pyt.get("10.0.0.0/24")
    'a'

If you want access to the key instead (i.e., the longest matching prefix), use ``get_key``:

    >>> pyt.get_key("10.1.0.0/16")
    '10.1.0.0/16'
    >>> pyt.get_key("10.1.0.0/24")
    '10.1.0.0/16'
    >>> pyt.get_key("10.1.0.0/32")
    '10.1.0.0/16'
    >>> pyt.get_key("10.0.0.0/24")
    '10.0.0.0/8'

The ``del`` operator works as it does with Python dictionaries (and there is also a ``delete`` method that works similarly):

    >>> del pyt["10.0.0.0/8"]
    >>> pyt.get("10.1.0.0/16")
    'b'
    >>> del pyt["10.0.0.0/8"]
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    KeyError: "Prefix doesn't exist."
    >>> pyt.delete("10.2.0.0/16")
    >>>

``PyTricia`` objects can be iterated or coerced into a list:

    >>> list(pyt)
    ['10.0.0.0/8', '10.1.0.0/16']

The ``in`` operator can be used to test whether a prefix is contained in the ``PyTricia`` object, or whether an individual address is "covered" by a prefix:

    >>> '10.0.0.0/8' in pyt
    True
    >>> '10.2.0.0/8' in pyt
    True
    >>> '192.168.0.0/16' in pyt
    False
    >>> '192.168.0.0' in pyt
    False
    >>> '10.1.2.3' in pyt
    True
    >>> 

The ``has_key`` method is also implement, but it's important to note that the behavior of ``in`` differs from ``has_key``.  The ``has_key`` method checks for an *exact match* of a network prefix.  The ``in`` operator checks whether the left-hand operand (i.e., an IP address) is contained within one of the prefixes in the ``PyTricia`` object.  The ``get`` method and the indexing operation (``[]``) (each described above) have lookup behavior similar like the ``in`` operator --- they do *not* search for an exact match, but rather for the most closely matching prefix.  For example:

    >>> pyt.has_key('10.1.0.0/16')
    True
    >>> pyt.has_key('10.1.0.0')
    False
    >>> pyt.has_key('10.0.0.0/8')
    True
    >>> pyt.has_key('10.0.0.0')
    False
    >>> pyt.has_key('10.0.0.0/12')
    False
    >>> '10.0.0.0/12' in pyt
    True
    >>> '10.0.0.0' in pyt
    True
    >>> '10.0.0.0/8' in pyt
    True
    >>> 

It is also possible to find the ``parent`` and ``children`` of a given prefix in the tree.  Similarly to the ``has_key`` method, the prefix must be present as an exact match in the tree.  For instance:

    >>> pyt.parent('10.1.0.0/16')
    '10.0.0.0/8'
    >>> pyt.parent('10.0.0.0/8')
    None
    >>> pyt["10.1.1.0/24"] = 'c'
    >>> pyt.children('10.0.0.0/8')
    ['10.1.0.0/16', '10.1.1.0/24']
    >>> pyt.children('10.1.0.0/16')
    ['10.1.1.0/24']
    >>> pyt.children('10.1.1.0/24')
    []
    >>> pyt.parent('10.1.42.0/24')
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    KeyError: Prefix doesn't exist.

If you want to get the longest matching prefix for arbitrary prefixes, you should use ``get_key``, not ``parent``.

A ``PyTricia`` object is *almost* like a dictionary, but not quite.   You can extract the keys, but not the values:

    >>> pyt.keys()
    ['10.0.0.0/8', '10.1.0.0/16']
    >>> pyt.values()
    Traceback (most recent call last):
      File "<stdin>", line 1, in <module>
    AttributeError: 'pytricia.PyTricia' object has no attribute 'values'

As with a dictionary, you can iterate over a ``PyTricia`` object.  Currently, there's no ``items()``-like method for iterating over both keys and values; you can just iterate over keys (network prefixes).

    >> for prefix in pyt:
    ...     print (prefix,pyt[prefix])
    ... 
    10.0.0.0/8 a
    10.1.0.0/16 b
    >>> 

# Performance

For API usage, the usual Python advice applies: using indexing is the fastest method for insertion, lookup, and removal.  See the ``apiperf.py`` script in the repo for some comparative numbers.  For Python 3, using ``ipaddress``-module objects is the slowest.  There's a price to pay for the convenience, unfortunately.

The numbers below are based on running the program ``perftest.py`` (in the repo) against snapshots of py-radix and pysubnettree from February 2, 2016.  All tests were run in Python 2.7.6 and 3.4.3 on a Linux 3.13 kernel system (Ubuntu 14.04 server) which has 12 cores (Intel Xeon E5645 2.4GHz) and was very lightly loaded at the time of the test.

    $ python perftest.py 
    Average execution time for PyTricia: 0.902257204056
    Average execution time for radix: 1.09275889397
    Average execution time for subnet: 0.984920787811

    $ python3 perftest.py 
    Average execution time for PyTricia: 1.0562857019998773
    Average execution time for radix: 1.306612914499965
    Average execution time for subnet: 1.1982004833000246

# Acknowledgments

This software is based up on work supported by the National Science Foundation under Grant No. CNS-1054985.  Any opinions, findings, and conclusions or recommendations expressed in this material are those of the author(s) and do not necessarily reflect the views of the National Science Foundation.

-----

Copyright (c) 2012-2016  Joel Sommers.  All rights reserved.

