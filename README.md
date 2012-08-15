# pytricia: an IP address lookup module for Python

Pytricia is a new python module to store IP prefixes in a patricia tree. 
It's based on Dave Plonka's modified patricia tree code, and has three things 
to recommend it over related modules (including py-radix and SubnetTree): 

 1. it's faster,
 1. it works in Python 3, and 
 1. there are a few nicer library features for manipulating the structure.

Some performance numbers to back up claim 1 will be included in the distro
in the future.

Pytricia is released under terms of the GPLv2.

# Building 

Building pytricia is done in the standard pythonic way: 

    python setup.py build
    python setup.py install

This code is beta quality at present, maybe.  Lots more testing needs to be done.
Documentation will happen as I have time.

-----

Copyright (c) 2012  Joel Sommers.  All rights reserved.


