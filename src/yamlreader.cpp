#include "yamlreader.h"

#include <yaml.h>

#include <memory>

namespace
{

// Aliases can make a small document huge (every alias is a copy here).
const int maxNodes = 100000;
const int maxDepth = 64;

class Converter
{
public:
    Converter(yaml_document_t *document, const QString &name) : document(document), name(name) {}

    Result<QVariant> convert(const yaml_node_t *node, int depth)
    {
        if (++nodes > maxNodes || depth > maxDepth)
            return fail(node, "the document is too large or too deeply nested");

        switch (node->type)
        {
            case YAML_SCALAR_NODE:
                return scalar(node);

            case YAML_SEQUENCE_NODE:
            {
                QVariantList list;
                for (auto item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++)
                {
                    auto value = convert(yaml_document_get_node(document, *item), depth + 1);
                    if (!value)
                        return value;
                    list.append(*value);
                }
                return list;
            }

            case YAML_MAPPING_NODE:
            {
                QVariantMap map;
                for (auto pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++)
                {
                    auto keyNode = yaml_document_get_node(document, pair->key);
                    if (keyNode->type != YAML_SCALAR_NODE)
                        return fail(keyNode, "a key has to be a scalar");
                    auto key = text(keyNode);
                    if (map.contains(key))
                        return fail(keyNode, QString("\"%1\" is there twice").arg(key));

                    auto value = convert(yaml_document_get_node(document, pair->value), depth + 1);
                    if (!value)
                        return value;
                    map.insert(key, *value);
                }
                return map;
            }

            default:
                return QVariantMap();
        }
    }

private:
    yaml_document_t *document;
    QString name;
    int nodes = 0;

    std::unexpected<Error> fail(const yaml_node_t *node, const QString &problem) const
    {
        return Err(Error::Parse, QString("%1:%2: %3").arg(name).arg(node->start_mark.line + 1).arg(problem));
    }

    static QString text(const yaml_node_t *node)
    {
        return QString::fromUtf8(reinterpret_cast<const char *>(node->data.scalar.value),
                                 qsizetype(node->data.scalar.length));
    }

    static QVariant scalar(const yaml_node_t *node)
    {
        auto value = text(node);
        if (node->data.scalar.style != YAML_PLAIN_SCALAR_STYLE)
            return value;

        if (value == "true" || value == "false")
            return value == "true";
        bool isInteger = false;
        qlonglong integer = value.toLongLong(&isInteger);
        // not "+1", " 1" (toLongLong() takes them)
        if (isInteger && !value.isEmpty() && (value[0].isDigit() || value[0] == '-'))
            return integer;
        return value;
    }
};

}  // namespace

Result<QVariant> readYaml(const QByteArray &yaml, const QString &name)
{
    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
        return Err(Error::Failed, "Out of memory.");
    std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> parserGuard(&parser, yaml_parser_delete);
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char *>(yaml.constData()),
                                 size_t(yaml.size()));
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);

    yaml_document_t document;
    if (!yaml_parser_load(&parser, &document))
        return Err(Error::Parse, QString("%1:%2: %3")
                                     .arg(name)
                                     .arg(parser.problem_mark.line + 1)
                                     .arg(QString::fromUtf8(parser.problem ? parser.problem : "invalid YAML")));
    std::unique_ptr<yaml_document_t, decltype(&yaml_document_delete)> documentGuard(&document,
                                                                                     yaml_document_delete);

    auto root = yaml_document_get_root_node(&document);
    if (!root)
        return QVariantMap();
    return Converter(&document, name).convert(root, 0);
}
