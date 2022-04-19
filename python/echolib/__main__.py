from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from future import standard_library
standard_library.install_aliases()
import os
import argparse
import sys

def main():
    from echolib import router

    parser = argparse.ArgumentParser()
    parser.add_argument("-v", "--verbose", action="store_true", default=False)
    parser.add_argument("bind", nargs='?', default=os.environ.get("ECHOLIB_SOCKET", "/tmp/echo.sock"))

    try:
        args = parser.parse_args()
    except argparse.ArgumentError:
        parser.print_help()
        sys.exit(-1)

    try:
        router(args.bind, args.verbose)
    except KeyboardInterrupt:
        pass

if __name__ == "__main__":
    main()
    