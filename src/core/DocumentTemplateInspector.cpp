#include "DocumentTemplateInspector.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDir>

namespace {
QString capabilityFor(const QString& format)
{
    if (format == QStringLiteral("md") || format == QStringLiteral("html") || format == QStringLiteral("htm") || format == QStringLiteral("tex")) return QStringLiteral("text-structured");
    if (format == QStringLiteral("docx") || format == QStringLiteral("dotx") || format == QStringLiteral("odt")) return QStringLiteral("office-structured");
    if (format == QStringLiteral("pdf")) return QStringLiteral("fixed-layout");
    if (format == QStringLiteral("txt")) return QStringLiteral("opaque");
    return QStringLiteral("unsupported");
}

void parseMarkdown(const QString& text, QList<DocumentSection>& sections)
{
    const auto lines = text.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    int order = 0;
    for (const auto& line : lines) {
        const auto match = QRegularExpression(QStringLiteral("^(#{1,6})[ \\t]+(.+?)\\s*$")).match(line);
        if (!match.hasMatch()) continue;
        const int level = match.captured(1).size();
        const QString title = match.captured(2).trimmed();
        sections.append({QStringLiteral("heading-%1").arg(++order), level, order, title, title, {}, {}, false, {}});
    }
}

void parseHtml(const QString& text, QList<DocumentSection>& sections)
{
    QRegularExpression expression(QStringLiteral("<h([1-6])(?:\\s[^>]*)?>(.*?)</h\\1>"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
    auto iterator = expression.globalMatch(text);
    int order = 0;
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        const QString title = match.captured(2).remove(QRegularExpression(QStringLiteral("<[^>]+>"))).simplified();
        sections.append({QStringLiteral("heading-%1").arg(++order), match.captured(1).toInt(), order, title, title, {}, {}, false, {}});
    }
}

void parseLatex(const QString& text, QList<DocumentSection>& sections)
{
    QRegularExpression expression(QStringLiteral("\\\\(chapter|section|subsection|subsubsection)\\s*\\{([^}]*)\\}"));
    auto iterator = expression.globalMatch(text);
    int order = 0;
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        const int level = match.captured(1) == QStringLiteral("chapter") ? 1 : match.captured(1) == QStringLiteral("section") ? 2 : match.captured(1) == QStringLiteral("subsection") ? 3 : 4;
        const QString title = match.captured(2).trimmed();
        sections.append({QStringLiteral("heading-%1").arg(++order), level, order, title, title, {}, {}, false, {}});
    }
}
}

namespace DocumentTemplateInspector {
TemplateInspectionResult inspect(const ProjectResource& resource, const QString& projectPath)
{
    TemplateInspectionResult result;
    result.sourcePath = resource.location;
    const QString location = QDir::isAbsolutePath(resource.location) || projectPath.isEmpty() ? resource.location : QDir(projectPath).filePath(resource.location);
    const QFileInfo info(location);
    result.fileName = info.fileName();
    result.format = info.suffix().toLower();
    result.capability = capabilityFor(result.format);
    result.formatRecognized = result.capability != QStringLiteral("unsupported");
    result.exists = info.exists() && info.isFile();
    result.fileSize = result.exists ? info.size() : 0;
    result.modifiedTime = result.exists ? info.lastModified().toUTC().toString(Qt::ISODate) : QString();
    if (!result.formatRecognized) result.warnings << QStringLiteral("File extension is unsupported");
    if (!result.exists) {
        result.warnings << QStringLiteral("Custom template source file is missing");
        return result;
    }
    QFile file(location);
    if (!file.open(QIODevice::ReadOnly)) {
        result.warnings << QStringLiteral("Custom template source could not be read");
        return result;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) hash.addData(file.read(64 * 1024));
    result.contentHash = QString::fromLatin1(hash.result().toHex());
    if (result.format == QStringLiteral("md") || result.format == QStringLiteral("html") || result.format == QStringLiteral("htm") || result.format == QStringLiteral("tex")) {
        file.seek(0);
        const QString text = QString::fromUtf8(file.readAll());
        if (result.format == QStringLiteral("md")) parseMarkdown(text, result.sections);
        else if (result.format == QStringLiteral("html") || result.format == QStringLiteral("htm")) parseHtml(text, result.sections);
        else parseLatex(text, result.sections);
        result.structurallyParsed = true;
        if (result.sections.isEmpty()) result.warnings << QStringLiteral("No supported headings were found");
    } else if (result.capability == QStringLiteral("office-structured")) {
        result.warnings << QStringLiteral("Office structure inspection is unavailable; source is preserved without fabricated headings");
    } else if (result.capability == QStringLiteral("fixed-layout")) {
        result.warnings << QStringLiteral("PDF is fixed-layout and structurally opaque");
    } else if (result.capability == QStringLiteral("opaque")) {
        result.warnings << QStringLiteral("Plain text structure is not inferred");
    }
    return result;
}
}
