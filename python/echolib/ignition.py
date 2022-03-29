
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from builtins import super
from future import standard_library
standard_library.install_aliases()
import os, sys
from ignition.program import mergevars
from ignition.plugin import Plugin

class Mapping(Plugin):

    def __init__(self):
        super().__init__()

    def on_program_init(self, program):
        mapping = program.auxiliary.get("remap", {})
        if not type(mapping) is dict:
            return
        maplist = []
        for k, v in mapping.items():
            maplist.append("%s=%s" % (k, v))

        program.environment["ECHOLIB_MAP"] = ";".join(maplist)
