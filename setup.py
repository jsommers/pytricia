from distutils.core import setup, Extension
setup(name="pytricia", 
      version="0.9.1",
      description="An efficient IP address storage and lookup module for Python.",
      author="Joel Sommers",
      author_email="jsommers@acm.org",
      url="https://github.com/jsommers/pytricia",
      # download_url="http://cs.colgate.edu/~jsommers/downloads/pytricia-0.1.tar.gz",
      keywords=['patricia tree','IP addresses'],
      classifiers=[
              "Programming Language :: Python :: 2",
              "Programming Language :: Python :: 3",
              "Development Status :: 4 - Beta",
              "Intended Audience :: Developers",
              "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",
              "Operating System :: OS Independent",
              "Topic :: Software Development :: Libraries :: Python Modules",
              "Topic :: Internet :: Log Analysis",
              "Topic :: Scientific/Engineering",
      ],
      ext_modules=[
         Extension("pytricia", ["pytricia.c","patricia.c"]),
         ],
      long_description='''
Pytricia is a Python module to store IP prefixes in a
patricia tree. It's based on Dave Plonka's modified patricia tree
code, and has three things to recommend it over related modules
(including py-radix and SubnetTree):

 * it's faster,

 * it works nicely in Python 3, and

 * there are a few nicer library features for manipulating the structure.

See the github repo for documentation and some performance numbers: <https://github.com/jsommers/pytricia>.

Pytricia is released under terms of the GPLv2.
'''
)
