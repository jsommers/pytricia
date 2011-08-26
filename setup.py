from distutils.core import setup, Extension
setup(name="pytricia", version="0.1",
      ext_modules=[
         Extension("pytricia", ["pytricia.c","patricia.c"]),
         ])
