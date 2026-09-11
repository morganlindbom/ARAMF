// DocumentTemplate.h

#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QString>

struct DocumentSection {
    QString id;
    int level = 1;
    int order = 0;
    QString titleSv;
    QString titleEn;
    QString guidanceSv;
    QString guidanceEn;
    bool required = true;
    QString parentId;
};

struct DocumentTemplate {
    QString id;
    QString documentType;
    int version = 1;
    QList<DocumentSection> sections;
};

namespace DocumentTemplates {
DocumentTemplate thesis();
DocumentTemplate report();
QStringList validate(const DocumentTemplate& document);
QList<DocumentSection> tableOfContents(const DocumentTemplate& document);
QJsonObject toJson(const DocumentTemplate& document);
QJsonObject manifest(bool thesisEnabled, const QString& thesisMode, const QString& thesisSourceId,
                     bool reportEnabled,
                     const QString& reportMode, const QString& reportSourceId,
                     const QString& thesisLanguage = QStringLiteral("sv"),
                     const QString& reportLanguage = QStringLiteral("sv"));
}
