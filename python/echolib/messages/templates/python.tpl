# This is an autogenerated file, do not modify!

{% for import in registry.get_sources() %}
import {{ import }}
{% endfor %}

def enum(name, enums):
    reverse = dict((value, key) for key, value in enums.items())
    enums["str"] = staticmethod(lambda x: reverse[x])
    return type(name, (), enums)

def enum_conversion(enum, obj):
    if isinstance(obj, int):
        return obj
    if isinstance(obj, str):
        return getattr(enum, obj)
    return 0

{% for name, values in registry.enums.items() %}
{{ name }} = enum("{{ name }}", { {% for k, v in values.items() %}'{{k}}' : {{v}}{% if not loop.last %}, {% endif %}{% endfor %} })

echolib.registerType({{ name }}, lambda x: x.readInt(), lambda x, o: x.writeInt(o), lambda x: enum_conversion({{ name }}, x))

{% endfor %}

{% for name, type in registry.types.items() %}
{%- if type.get_reader() and type.get_writer() %}
echolib.registerType({{ type.get_container() }}, {{ type.get_reader() }}, {{ type.get_writer() }} )
{% endif -%}
{% endfor %}

{% for name, fields in registry.structs.items() %}
class {{ name }}(object):
    def __init__(self, 
        {%- for k, v in fields.items() -%}{%- set defval = v["default"] if not v["default"] is none else registry.types[v["type"]].get_default() -%}
        {%- if not loop.first %},
        {%- endif -%}{%- if v['array'] and v['length'] is none -%}
        {{ k }} = None
        {%- elif v['array'] and not v['length'] is none -%}
        {{ k }} = None
        {%- elif not defval is none -%}
        {{ k }} = {{ defval|constant }}
        {%- else -%}
        {{ k }} = None
        {%- endif -%}
        {%- endfor -%}
        ):
        {% for k, v in fields.items() -%}
        {%- set defval = v["default"] if not v["default"] is none else registry.types[v["type"]].get_default() -%}
        {%- if v['array'] and v['length'] is none %}
        if {{ k }} is None:
            self.{{ k }} = []
        else:
            self.{{ k }} = {{ k }}
        {%- elif v['array'] and not v['length'] is none %}
        if {{ k }} is None:
            self.{{ k }} = []
        else:
            self.{{ k }} = {{ k }}
        {%- elif not defval is none %}
        self.{{ k }} = {{ k }}
        {%- else %}
        if {{ k }} is None:
            self.{{ k }} = {{ registry.types[v["type"]].get_container() }}()
        else:
            self.{{ k }} = {{ k }}
        {%- endif -%}
        {%- endfor %}
        pass

    @staticmethod
    def read(reader):
        dst = {{ name }}()
        {% for k, v in fields.items() -%}
        {% if v['array'] -%}
        dst.{{ k }} = echolib.readList({{ registry.types[v["type"]].get_container() }}, reader)
        {% else %}
        dst.{{ k }} = echolib.readType({{ registry.types[v["type"]].get_container() }}, reader)
        {% endif %}
        {% endfor %}
        return dst

    @staticmethod
    def write(writer, obj):
        {% for k, v in fields.items() -%}
        {% if v['array'] -%}
        echolib.writeList({{ registry.types[v["type"]].get_container() }}, writer, obj.{{ k }})
        {% else %}
        echolib.writeType({{ registry.types[v["type"]].get_container() }}, writer, obj.{{ k }})
        {% endif %}
        {% endfor %}
        pass

echolib.registerType({{ name }}, {{ name }}.read, {{ name }}.write)

{% endfor %}

{% for name in registry.messages %}
{% set metadata = registry.types[name] %}
class {{ name }}Subscriber(echolib.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = echolib.MessageReader(message)
            return {{ name }}.read(reader)

        super({{ name }}Subscriber, self).__init__(client, alias, "{{ metadata.get_hash() }}", lambda x: callback(_read(x)))


class {{ name }}Publisher(echolib.Publisher):

    def __init__(self, client, alias):
        super({{ name }}Publisher, self).__init__(client, alias, "{{ metadata.get_hash() }}")

    def send(self, obj):
        writer = echolib.MessageWriter()
        {{ name }}.write(writer, obj)
        super({{ name }}Publisher, self).send(writer)

{% endfor %}