#!/usr/bin/env python3
"""Squashes the RapidJSON headers into the single header thirdparty/rapidjson.h.

usage: amalgamate_rapidjson.py <rapidjson checkout> [output]

Local includes ("...") are inlined recursively, system includes (<...>) are kept.
A header that was already inlined is replaced by a marker.
"""

import os
import re
import sys

# top level headers, in this order
HEADERS = [
    "rapidjson.h", "allocators.h", "document.h", "encodedstream.h", "encodings.h", "filereadstream.h",
    "filewritestream.h", "fwd.h", "istreamwrapper.h", "memorybuffer.h", "memorystream.h", "ostreamwrapper.h",
    "pointer.h", "prettywriter.h", "reader.h", "schema.h", "stream.h", "stringbuffer.h", "writer.h",
]

# configuration inserted in front of the include guard of rapidjson.h
PRELUDE = """
#define RAPIDJSON_ASSERT_THROWS

#ifdef __ARM_NEON__
#define RAPIDJSON_NEON
#endif

"""

INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"[^\n]*\n', re.MULTILINE)


class Amalgamator:
    def __init__(self, include_dir):
        self.include_dir = os.path.realpath(include_dir)
        self.included = set()

    def inline(self, path, name):
        path = os.path.realpath(path)
        if path in self.included:
            body = "// already included"
        else:
            self.included.add(path)
            with open(path, encoding="utf-8") as f:
                content = f.read()
            if name == "rapidjson.h" and os.path.dirname(path) == self.include_dir:
                content = content.replace("#ifndef RAPIDJSON_RAPIDJSON_H_", PRELUDE + "#ifndef RAPIDJSON_RAPIDJSON_H_", 1)
            directory = os.path.dirname(path)
            body = INCLUDE_RE.sub(lambda m: self.include(directory, m), content)
        return "\n// Begin file: {0}\n{1}\n// End file:{0}\n\n".format(name, body)

    def include(self, directory, match):
        path = os.path.join(directory, match.group(1))
        # e.g. the '#include "rapidjson/..."' example in a doc comment
        if not os.path.isfile(path):
            return match.group(0)
        return self.inline(path, match.group(1))

    def run(self):
        out = "".join(self.inline(os.path.join(self.include_dir, h), h) for h in HEADERS)
        return out.rstrip("\n") + "\n"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    include_dir = os.path.join(sys.argv[1], "include", "rapidjson")
    output = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "rapidjson.h")
    text = Amalgamator(include_dir).run()
    # the original amalgamation was written with CRLF line endings, keep them to keep diffs readable
    with open(output, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(text)


if __name__ == "__main__":
    main()
