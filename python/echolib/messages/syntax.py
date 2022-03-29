from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from builtins import map
from builtins import super
from future import standard_library
standard_library.install_aliases()
from pyparsing import *

class DescriptionError(Exception):
    
    def __init__(self, file, line, column, message):
        super().__init__()
        self._file = file
        self._line = line
        self._column = column
        self._message = message

    def __str__(self):
        return "{} (line: {}, col: {}): {}".format(self._file, self._line, self._column, self._message)

    @property
    def file(self):
        return self._file

    @property
    def line(self):
        return self._line

    @property
    def column(self):
        return self._column

    @property
    def message(self):
        return self._message

def description_syntax():

    def make_keyword(kwd_str, kwd_value):
        return Keyword(kwd_str).setParseAction(replaceWith(kwd_value))

    TRUE = make_keyword("true", True)
    FALSE = make_keyword("false", False)
    NULL = make_keyword("null", None)

    LBRACK, RBRACK, LBRACE, RBRACE, COLON, SEMICOLON, EQUALS, POINT, LANGLE, RANGLE = map(
        Suppress, "[]{}:;=.()")

    Exponent = CaselessLiteral('E')

    MessageFile = dblQuotedString().setParseAction(removeQuotes)
    StringLiteral = dblQuotedString().setParseAction(removeQuotes)

    PlusMinus = Literal('+') | Literal('-')
    Number = Word(nums)
    IntegerValue = Combine( Optional(PlusMinus) + Number )
    FloatValue = Combine( IntegerValue +
                       Optional( POINT + Optional(Number) ) +
                       Optional( Exponent + IntegerValue )
                     )
    BooleanValue = Or( [TRUE | FALSE])

    LanguageName = Word(alphanums)
    FieldName = Word(alphanums + "_")
    FieldType = Word(alphanums)
    PropertyName = Word(alphanums + "_")
    EnumerateConst = Word(nums)
    EnumerateValue = Group(Word(alphanums + "_").setResultsName("name"))
    EnumerateName = Word(alphanums + "_")
    StructureName = Word(alphanums + "_")
    ArrayLength = Word(nums)
    
    Value = Group(Or([FloatValue.setResultsName("numeric") | StringLiteral.setResultsName("string") | BooleanValue.setResultsName("bool")]))
    PositionalProperty = Value.setResultsName("value")
    KeywordProperty = Group(PropertyName.setResultsName("name") + EQUALS + Value.setResultsName("value"))
    PropertyList = Group(LANGLE +
        (Group(KeywordProperty + ZeroOrMore(Suppress(COLON) + KeywordProperty).setResultsName("kwargs")) ^
        (Group(PositionalProperty + ZeroOrMore(Suppress(COLON) + PositionalProperty)).setResultsName("args") + ZeroOrMore(Suppress(COLON) + KeywordProperty).setResultsName("kwargs"))) + RANGLE)

    ExternalLanguage = Group(Literal("language") + LanguageName.setResultsName("language") 
            + StringLiteral.setResultsName("container") 
            + Optional(Literal("from") + OneOrMore(StringLiteral).setResultsName("sources"))
            + Optional(Literal("default") + StringLiteral.setResultsName("default"))
            + Optional(Literal("read") + StringLiteral.setResultsName("read") 
                + Literal("write") + StringLiteral.setResultsName("write"))
            ) + SEMICOLON

    ExternalLanguageList = Group(LANGLE + ZeroOrMore(ExternalLanguage) + RANGLE)

    Field = Group(FieldType.setResultsName("type") + Optional(Group(LBRACK +
        Optional(ArrayLength).setResultsName("length") + RBRACK).setResultsName("array")) +
        FieldName.setResultsName("name") + Optional(PropertyList.setResultsName("properties")) + Optional(EQUALS + Value.setResultsName("default")) + SEMICOLON)
    FieldList = Group(
        LBRACE + ZeroOrMore(Field) + RBRACE).setResultsName('fields')

    Enumerate = Group(Literal("enumerate") + EnumerateName.setResultsName("name") + LBRACE +
                      Group(delimitedList(EnumerateValue)).setResultsName('values') + RBRACE)

    Include = Group(
        Literal("include") + MessageFile.setResultsName('name') + Optional(PropertyList.setResultsName("properties")))  + SEMICOLON

    Import = Group(
        Literal("import") + MessageFile.setResultsName('name')) + SEMICOLON

    External = Group(
        Literal("external") + StructureName.setResultsName('name') + ExternalLanguageList.setResultsName("languages")) + SEMICOLON

    Structure = Group(
        Literal("structure") + StructureName.setResultsName('name') + FieldList)
    Message = Group(
        Literal("message") + StructureName.setResultsName('name') + FieldList)

    Namespace = Group(
        Literal("namespace") + Word(alphanums + ".").setResultsName('name') + SEMICOLON)
    Messages = Optional(
        Namespace) + ZeroOrMore(Or([Enumerate | Include | Import | External | Structure | Message]))
    Messages.ignore(pythonStyleComment)

    return Messages

def parse(text):

    syntax = description_syntax()

    try:
        return syntax.parseString(text, True)
    except ParseException as e:
        raise DescriptionError("<input>", e.lineno, e.col, e.msg)
