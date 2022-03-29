from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals
from builtins import super
from builtins import open
from builtins import str
from builtins import int
from builtins import zip
from builtins import range
from future import standard_library
standard_library.install_aliases()
import os
import six
import hashlib
import logging

from collections import OrderedDict

from echolib.messages.syntax import parse, DescriptionError

LANGUAGES = ["cpp", "python"]

default_language = "cpp"

logger = logging.getLogger("echomsg")

def set_default_language(language):
    if language in LANGUAGES:
        global default_language
        default_language = language

def remove_duplicates(seq):
    seen = set()
    seen_add = seen.add
    return [x for x in seq if not (x in seen or seen_add(x))]


class Type(object):

    def __init__(self, name, hash):
        self._name = name
        self._hash = hash

    def get_name(self):
        return self._name

    def get_container(self, language=None):
        return self._name

    def get_default(self, language=None):
        return None

    def get_reader(self, language=None):
        return None

    def get_writer(self, language=None):
        return None

    def get_hash(self):
        return self._hash

class ExternalType(Type):     
    def __init__(self, name, container, default = None, reader = None, writer = None):
        super(ExternalType, self).__init__(name, name)
        self._default = default
        self._container = container
        self._reader = reader
        self._writer = writer

    def get_container(self, language=None):
        language = language if language else default_language
        if isinstance(self._container, dict):
            return self._container.get(language, None)
        return self._container

    def get_default(self, language=None):
        language = language if language else default_language
        if isinstance(self._default, dict):
            return self._default.get(language, None)
        return self._default

    def get_reader(self, language=None):
        language = language if language else default_language
        if isinstance(self._reader, dict):
            return self._reader.get(language, None)
        return self._reader

    def get_writer(self, language=None):
        language = language if language else default_language
        if isinstance(self._writer, dict):
            return self._writer.get(language, None)
        return self._writer

class Source(object):

    def __init__(self, code):
        self.code = code

    def __str__(self):
        return self.code


def formatConstant(value, language="cpp"):
    if isinstance(value, Source):
        return str(value)
    if value is None and language == "cpp":
        return ""
    if value is None and language == "python":
        return "None"
    elif isinstance(value, bool):
        if language == "cpp":
            return "true" if value else "false"
        if language == "python":
            return "True" if value else "False"
    if isinstance(value, six.integer_types):
        return str(value)
    if isinstance(value, float):
        return "%.f" % (value)
    elif isinstance(value, six.string_types):
        return "\"%s\"" % value
    return str(value)

class MessagesRegistry(object):

    def __init__(self):
        self.enums = OrderedDict()
        self.types = OrderedDict()
        self.add_type(ExternalType("short", {"python" : "int", "cpp": "int16_t"}, 0))
        self.add_type(ExternalType("int", {"python" : "int", "cpp": "int32_t"}, 0))
        self.add_type(ExternalType("long", {"python" : "echolib.long", "cpp": "int64_t"}, 0))
        self.add_type(ExternalType("float", {"python" : "float", "cpp": "float"}, 0.0))
        self.add_type(ExternalType("double", {"python" : "echolib.double", "cpp": "double"}, 0.0))
        self.add_type(ExternalType("bool", {"python" : "bool", "cpp": "bool"}, False))
        self.add_type(ExternalType("char", {"python" : "echolib.char", "cpp": "char"}, '\0'))
        self.add_type(ExternalType("string", {"python" : "str", "cpp": "std::string"}, ""))

        self.add_type(ExternalType("timestamp", {
            "python" : "datetime.datetime",
            "cpp": "std::chrono::system_clock::time_point"
        }))

        self.add_type(ExternalType("header", {
            "python" : "echolib.Header",
            "cpp": "echolib::Header"
        }, default={
            "python" : Source("echolib.Header()"),
            "cpp": Source("echolib::Header()")
        }))

        self.add_type(ExternalType("array", {
            "python" : "numpy.ndarray",
            "cpp": "echolib::Array"
        }, default={
            "python" : Source("numpy.zeros((0,))"),
            "cpp": Source("echolib::Array()")
        }))

        self.add_type(ExternalType("tensor", {
            "python" : "numpy.ndarray",
            "cpp": "echolib::Array"
        }, default={
            "python" : Source("numpy.zeros((0,))"),
            "cpp": Source("echolib::Tensor()")
        }))

        self.structs = OrderedDict()
        self.messages = []
        self.namespace = None
        self.files = []
        self.sources = {l : [] for l in LANGUAGES}
        self.sources["cpp"].append("vector")
        self.sources["cpp"].append("chrono")
        self.sources["cpp"].append("echolib/datatypes.h")
        self.sources["cpp"].append("echolib/array.h")
        self.sources["python"].append("echolib")
        self.sources["python"].append("datetime")
        self.sources["python"].append("numpy")
        
    def get_sources(self, language = None):
        language = language if language else default_language
        return self.sources[language]

    def add_enum(self, name, values):
        
        typehash = hashlib.md5()
        for v in values:
            typehash.update(v.encode('utf-8'))
        self.add_type(Type(name, typehash.hexdigest()))
        self.enums[name] = values

    def add_struct(self, name, fields):
        typehash = hashlib.md5()
        for k, v in fields.items():
            t = v['type']
            if not t in self.types:
                raise RuntimeError('Unknown type: ' + t)
            typehash.update(self.types[t].get_hash().encode('utf-8'))

        self.add_type(Type(name, typehash.hexdigest()))
        self.structs[name] = fields

    def add_message(self, name, fields):
        self.add_struct(name, fields)
        self.messages.append(name)

    def add_type(self, type):
        if type.get_name() in self.types:
            raise RuntimeError('Name already taken: ' + type.get_name())
        self.types[type.get_name()] = type

def processValue(value):
    if "numeric" in value:
        try:
            return int(value["numeric"])
        except ValueError:
            return float(value["numeric"])
    elif "bool" in value:
        return bool(value["bool"])
    else:
        return str(value["string"])

def processFields(fields):
    result = OrderedDict()
    for field in fields:
        name = field["name"]
        result[name] = {"type": field["type"], "default" : None}
        result[name]["array"] = "array" in field
        if result[name]["array"]:
            result[name]["array"] = True
            if "length" in field["array"]:
                result[name]["length"] = int(field["array"]["length"])
#        if "properties" in field:
#           result[name]["properties"]
        if "default" in field:
           result[name]["default"] = processValue(field["default"])
    return result


def parseFile(msgfile, registry, searchpath=[], include=True):

    if os.path.isabs(msgfile):
        if not os.path.exists(msgfile) or not os.path.isfile(msgfile):
            raise IOError("File '%s' does not exist", msgfile)
    else:
        base = msgfile
        msgfile = None
        for p in searchpath:
            candidate = os.path.join(p, base)
            if os.path.exists(candidate) and os.path.isfile(candidate):
                msgfile = candidate
                break

    if msgfile in registry.files:
        logger.info("%s already processed, ignoring.", msgfile)
        return

    registry.files.append(msgfile)

    if not msgfile:
        raise IOError("File '%s' does not exist in search path" % base)

    try:
        with open(msgfile, "r") as handle:
            messages = parse(handle.read())
    except DescriptionError as e:
        raise DescriptionError(msgfile, e.line, e.column, e.message)

    try:

        for e in messages:
            name = e[0]
            if name == 'namespace':
                registry.namespace = e["name"]
            if name == 'import':
                parseFile(e["name"], registry, searchpath, False)
            if name == 'include':
                properties = {a["name"] : a["value"] for a in e.get("properties", [])}
                parseFile(e["name"], registry, searchpath, True)
            if name == 'external':
                containers = {l: e["name"] for l in LANGUAGES}
                defaults = {}
                readers = {}
                writers = {}
                declared = []
                for l in e.get("languages", []):
                    if not l["language"] in LANGUAGES:
                        raise RuntimeError("Unknown language {}".format(l["language"]))
                    if l["language"] in declared:
                        raise RuntimeError("Duplicate declaration {}".format(l["language"]))
                    declared.append(l["language"])
                    containers[l["language"]] = l["container"] # Override container name
                    if l.get("default", None):
                        defaults[l["language"]] = Source(l["default"])
                    if l.get("sources", None):
                        registry.sources[l["language"]].extend(l["sources"])
                    if l.get("read", None):
                        readers[l["language"]] = Source(l["read"])
                        writers[l["language"]] = Source(l["write"])

                registry.add_type(ExternalType(e["name"], containers, defaults, readers, writers))
            if name == 'enumerate':
                values = {v["name"]: k for v, k in zip(e["values"], range(len(e["values"])))}
                registry.add_enum(e["name"], values)
            if name == 'structure':
                fields = processFields(e["fields"])
                registry.add_struct(e["name"], fields)
            if name == 'message':
                fields = processFields(e["fields"])
                registry.add_message(e["name"], fields)

        registry.sources = {k: remove_duplicates(l) for k, l in registry.sources.items()}

    except RuntimeError as ex:
        logger.exception(ex)
        raise DescriptionError(msgfile, 0, 0, str(ex))

