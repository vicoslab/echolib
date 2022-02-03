
from __future__ import absolute_import

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
