#!/usr/bin/python3

# usage: copy-asset.py <file> <output>
# <file> is the input file
# <output> is the output file

import shutil
from sys import argv

file = argv[1]
output = argv[2]

shutil.copyfile(file, output)
