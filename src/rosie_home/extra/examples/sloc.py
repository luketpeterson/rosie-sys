#  -*- coding: utf-8; -*-
#  -*- Mode: Python; -*-                                                   
# 
#  sloc.py
# 
#  © Copyright Jamie A. Jennings 2018, 2019.
#  LICENSE: MIT License (https://opensource.org/licenses/mit-license.html)
#
# DEPENDENCIES:
#   - A Rosie installation in /usr/local or other standard place
#   - pip install rosie
#
# USAGE:
#   python sloc.py "--" ../../src/lua/*.lua

from __future__ import unicode_literals, print_function
import os, sys
import rosie

def print_usage():
    print("Usage: " + sys.argv[0] + " <comment_start> [files ...]")

if len(sys.argv) < 2:
    print_usage();
    sys.exit(-1)

comment_start = sys.argv[1]
if os.path.isfile(comment_start) or len(comment_start) > 2:
    print('Error: first argument should be the comment character, e.g. "#" or "--"')
    print_usage()
    sys.exit(-1)

# The pattern source_line is defined below as "not looking at
# optional-whitespace followed by either the start of a comment or the
# end of the line".  So it will match non-comment, non-blank lines.

engine = rosie.engine()
source_line = engine.compile('!{[:space:]* "' + comment_start + '"/$}')

def count(f):
    count = 0
    for line in f:
        if line and source_line.match(line): count += 1
    return count

for f in sys.argv[2:]:
    label = f + ": " if f else ""
    print(label.ljust(36) + str(count(open(f, 'r'))).rjust(4) + " non-comment, non-blank lines")



    



                             
