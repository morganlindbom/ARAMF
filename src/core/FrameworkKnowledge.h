// FrameworkKnowledge.h

#pragma once

#include <QObject>
#include <QStringList>

struct FrameworkKnowledgeEntry
{
    QString id;
    QString title;
    QString lesson;
    QString status = QStringLiteral("candidate");
    QString reviewStatus = QStringLiteral("more-evidence");
    QStringList scopes;
    QStringList evidence;
    bool portable = true;
    QString createdAt;
    QString approvedAt;
    QString approvalSource;
    QString supersededBy;
};

class FrameworkKnowledgeService final : public QObject
{
    Q_OBJECT

public:
    explicit FrameworkKnowledgeService(QObject* parent = nullptr);

    bool ensureFile(const QString& projectRoot, QString* error = nullptr) const;
    QString propose(const QString& projectRoot,
                    const QString& title,
                    const QString& lesson,
                    const QStringList& scopes,
                    const QStringList& evidence,
                    bool portable = true,
                    QString* error = nullptr) const;
    bool approve(const QString& projectRoot,
                 const QString& candidateId,
                 const QString& approvalSource,
                 QString* error = nullptr) const;
    bool supersede(const QString& projectRoot,
                   const QString& entryId,
                   const QString& replacementId,
                   QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> entries(const QString& projectRoot,
                                           QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> approvedEntries(const QString& projectRoot,
                                                   const QStringList& scopes = {},
                                                   QString* error = nullptr) const;
};
