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

from setuptools import setup, Extension, find_packages
setup(name="pytricia", 
      version="1.2.0",
      description="An efficient IP address storage and lookup module for Python.",
      author="Joel Sommers",
      author_email="jsommers@acm.org",
      url="https://github.com/jsommers/pytricia",
      include_package_data=True,
      packages=find_packages(),
      keywords=['patricia tree','IP addresses'],
      license_files=["COPYING*"],
      classifiers=[
              "Programming Language :: Python :: 2",
              "Programming Language :: Python :: 3",
              "Intended Audience :: Developers",
              "Operating System :: OS Independent",
              "Topic :: Software Development :: Libraries :: Python Modules",
              "Topic :: Internet :: Log Analysis",
              "Topic :: Scientific/Engineering",
      ],
      ext_modules=[
         Extension("pytricia", ["pytricia.c","patricia.c"],
                        # extra_compile_args = ["-g", "-O0"]  # Enable debug info, disable optimization
                   ),
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

Pytricia is released under terms of the GNU Lesser General Public License,
version 3.
'''
)
