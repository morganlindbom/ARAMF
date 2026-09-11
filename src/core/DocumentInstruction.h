#pragma once

#include <QJsonObject>
#include <QStringList>

struct DocumentInstruction {
    QString id;
    QString documentType;
    int version = 1;
    QString purpose;
    QStringList rules;
};

namespace DocumentInstructions {
DocumentInstruction thesis();
DocumentInstruction report();
const DocumentInstruction& forType(const QString& documentType);
QStringList validate(const DocumentInstruction& instruction);
QJsonObject toJson(const DocumentInstruction& instruction);
}
