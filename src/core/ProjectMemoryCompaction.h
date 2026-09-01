#pragma once

#include <QJsonObject>
#include <QObject>

class ProjectMemoryCompaction final : public QObject
{
    Q_OBJECT
public:
    explicit ProjectMemoryCompaction(QObject* parent = nullptr);

    static int reviewThreshold(const QString& projectRoot, QString* error = nullptr);
    static bool reviewDue(const QString& projectRoot, QString* error = nullptr);
    QJsonObject dryRun(const QString& projectRoot, QString* error = nullptr) const;
    bool compact(const QString& projectRoot, bool approveCandidates = false,
                 QJsonObject* result = nullptr, QString* error = nullptr) const;
    QJsonObject applicableKnowledge(const QString& projectRoot, QString* error = nullptr) const;
};
