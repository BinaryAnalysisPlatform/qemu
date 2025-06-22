#!/usr/bin/env python3
"""
This just does:
sed -i 's/->base/->__base/g' <file>
sed -i 's/ProtobufCMessage base;/ProtobufCMessage __base;/g' <file>
"""

import sys
import re

if len(sys.argv) != 6 or sys.argv[3] != "-o":
    print("usage: fix_proto_src.py frame.piqi.pb-c.c frame.piqi.pb-c.h -o frame.piqi.pb-c-fixed.c frame.piqi.pb-c-fixed.h")
    exit(1)

for (in_file, out_file) in zip(sys.argv[1:3], sys.argv[4:6]):
    with open(in_file, "r") as i:
        contents = i.read()
    contents = contents.replace("->base", "->__base")
    contents = contents.replace("ProtobufCMessage base;", "ProtobufCMessage __base;")
    contents = re.sub(r'".*frame.piqi.pb-c.h"', '"frame.piqi.pb-c-patched.h"', contents)
    with open(out_file, "w") as o:
        o.write(contents)
