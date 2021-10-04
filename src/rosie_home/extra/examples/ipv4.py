# Find all the ipv4 addresses in files given on the command line
#
# EXAMPLE: python ipv4.py ../../test/resolv.conf 

import sys, rosie

engine = rosie.engine()
engine.import_package('net')
ipv4 = engine.compile('net.ipv4')

for f in sys.argv[1:]:
    matches = ipv4.search(open(f, 'r').read())
    for m in matches:
        print(m)


