# libyaml 0.2.5 (https://github.com/yaml/libyaml, MIT license, see License), the parser and the loader only:
# the sources of the release without the emitter (emitter.c, dumper.c, writer.c).
INCLUDEPATH += $$PWD

DEFINES += YAML_DECLARE_STATIC YAML_VERSION_MAJOR=0 YAML_VERSION_MINOR=2 YAML_VERSION_PATCH=5 \
    YAML_VERSION_STRING=\\\"0.2.5\\\"

HEADERS += $$PWD/yaml.h $$PWD/yaml_private.h
SOURCES += $$PWD/api.c $$PWD/loader.c $$PWD/parser.c $$PWD/reader.c $$PWD/scanner.c
