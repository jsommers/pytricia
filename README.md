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

The ``insert`` method can also be used to add prefixes/values to a PyTricia object.  This method returns ``None``.

    >>> pyt.insert("10.2.0.0/16", "c")

The ``insert`` method can optionally accept three parameters.




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

The numbers below are based on running the program ``perftest.py`` (in the repo) against snapshots of py-radix and pysubnettree from May 1, 2015.  All tests were run in Python 2.7.6 on a Linux 3.13 kernel system (Ubuntu 14.04 server) which has 12 cores (Intel Xeon E5645 2.4GHz) and was very lightly loaded at the time of the test.

    $ python perftest.py 
    Average execution time for PyTricia: 0.884083390236
    Average execution time for radix: 1.07317659855
    Average execution time for subnet: 0.969525814056

-----

Copyright (c) 2012-2015  Joel Sommers.  All rights reserved.
