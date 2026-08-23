// FrameworkKnowledge.h

#pragma once

#include <QObject>
#include <QStringList>

class ProjectModel;

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
    QString origin = QStringLiteral("project");
    QString originProjectId;
    QString originalKnowledgeId;
    QString promotedAt;
};

class FrameworkKnowledgeService final : public QObject
{
    Q_OBJECT

public:
    explicit FrameworkKnowledgeService(QObject* parent = nullptr);

    // Test-only storage seam; production leaves this unset and always uses
    // AramfPaths::programRoot()/ARAMF_DATA.
    static void setGlobalLibraryPathForTests(const QString& path);
    static void clearGlobalLibraryPathForTests();

    bool ensureFile(const QString& projectRoot, QString* error = nullptr) const;
    QString propose(const QString& projectRoot,
                    const QString& title,
                    const QString& lesson,
                    const QStringList& scopes,
                    const QStringList& evidence,
                    bool portable = true,
                    QString* error = nullptr) const;
    QString proposeApprovedByAdministrator(const QString& projectRoot,
                                           const QString& title,
                                           const QString& lesson,
                                           const QStringList& scopes,
                                           const QStringList& evidence,
                                           const QString& administrator,
                                           bool portable = true,
                                           QString* error = nullptr) const;
    bool approve(const QString& projectRoot,
                 const QString& candidateId,
                 const QString& approvalSource,
                 QString* error = nullptr) const;
    bool markMoreEvidence(const QString& projectRoot,
                          const QString& entryId,
                          QString* error = nullptr) const;
    bool supersede(const QString& projectRoot,
                   const QString& entryId,
                   const QString& replacementId,
                   QString* error = nullptr) const;
    QString globalLibraryPath() const;
    QString legacyGlobalLibraryPath() const;
    QString legacyExecutableGlobalLibraryPath() const;
    bool ensureGlobalLibrary(QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> builtInEntries(QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> globalEntries(QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> approvedGlobalEntries(const QStringList& scopes = {},
                                                         QString* error = nullptr) const;
    bool promoteToGlobal(const QString& projectRoot,
                         const QString& entryId,
                         QString* error = nullptr) const;
    bool supersedeGlobal(const QString& entryId,
                         const QString& replacementId,
                         QString* error = nullptr) const;
    bool seedProject(const QString& projectRoot,
                     const ProjectModel* model = nullptr,
                     QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> entries(const QString& projectRoot,
                                           QString* error = nullptr) const;
    QList<FrameworkKnowledgeEntry> approvedEntries(const QString& projectRoot,
                                                   const QStringList& scopes = {},
                                                   QString* error = nullptr) const;
    // Resolves the read-only effective catalog from built-in, global, and
    // project layers without copying any entry between stores.
    QList<FrameworkKnowledgeEntry> effectiveKnowledgeForProject(const QString& projectRoot,
                                                                 QString* error = nullptr) const;
    bool adoptKnowledgeForProject(const QString& projectRoot,
                                  const QStringList& entryIds,
                                  QString* error = nullptr) const;
};
