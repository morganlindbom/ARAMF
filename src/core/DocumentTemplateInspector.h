#pragma once

#include "DocumentTemplate.h"
#include "ProjectModel.h"

struct TemplateInspectionResult {
    QString format;
    QString capability;
    bool formatRecognized = false;
    bool structurallyParsed = false;
    bool exists = false;
    QString sourcePath;
    QString fileName;
    qint64 fileSize = 0;
    QString modifiedTime;
    QString contentHash;
    bool changed = false;
    QList<DocumentSection> sections;
    QStringList warnings;
};

namespace DocumentTemplateInspector {
TemplateInspectionResult inspect(const ProjectResource& resource, const QString& projectPath = QString());
}
