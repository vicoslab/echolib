
from __future__ import absolute_import

import os, sys
from ignition import mergevars
from ignition.plugin import Plugin

class Mapping(Plugin):

	def __init__(self):
		super(Plugin, self).__init__()

	def on_program_init(self, program, **kwargs):
		mapping = kwargs.get("remap", {})
		if not type(mapping) is dict:
			return
		maplist = []
		for k, v in mapping.items():
			maplist.append("%s=%s" % (k, v))

		program.environment = mergevars({"ECHOLIB_MAP" : ";".join(maplist)}, program.environment)

