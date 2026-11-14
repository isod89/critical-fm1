#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# CRITICAL's installer page: web/installer.tmpl.html with web/fwpkg.js and the
# application area inlined: one file, opens from the disk.   mkpage.py AREA VERSION OUT.html
import base64, sys
area, version, out = sys.argv[1], sys.argv[2], sys.argv[3]
t = open("web/installer.tmpl.html").read()
js = open("web/fwpkg.js").read()
t = t.replace("__FWPKG__", js).replace("__VERSION__", version)
t = t.replace("__AREA__", base64.b64encode(open(area, "rb").read()).decode())
open(out, "w").write(t)
print(out, len(t), "bytes")
