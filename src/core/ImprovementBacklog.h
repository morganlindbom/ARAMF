#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

struct ImprovementBacklogProjectIdentity
{
    QString projectId;
    QString projectName;
    QString projectPath;
};

class ImprovementBacklogService final : public QObject
{
    Q_OBJECT
public:
    explicit ImprovementBacklogService(QObject* parent = nullptr);

    static void setPathForTests(const QString& path);
    static void clearPathForTests();
    static void setWriteFailureForTests(bool enabled);

    QString backlogPath() const;
    bool ensure(QString* error = nullptr) const;
    QList<QJsonObject> items(QString* error = nullptr) const;

    bool report(const QString& projectRoot,
                const QString& title,
                const QString& observation,
                const QString& expected = {},
                const QString& area = {},
                const QStringList& evidence = {},
                const QString& task = {},
                const QString& agent = {},
                QJsonObject* result = nullptr,
                QString* error = nullptr) const;

    ImprovementBacklogProjectIdentity projectIdentity(const QString& projectRoot) const;
    bool reportWithIdentity(const ImprovementBacklogProjectIdentity& identity,
                            const QString& title,
                            const QString& observation,
                            const QString& expected = {},
                            const QString& area = {},
                            const QStringList& evidence = {},
                            const QString& task = {},
                            const QString& agent = {},
                            QJsonObject* result = nullptr,
                            QString* error = nullptr) const;

    bool normalizeProjectIdentity(const ImprovementBacklogProjectIdentity& identity,
                                  QString* error = nullptr) const;

    bool triage(const QString& id,
                const QString& action,
                const QString& duplicateOf = {},
                const QString& priority = {},
                QString* error = nullptr) const;

    bool setStatus(const QString& id, const QString& status, QString* error = nullptr) const;

    bool removeItem(const QString& id, QString* error = nullptr) const;

private:
    bool read(QJsonObject* store, QString* error) const;
    bool write(const QJsonObject& store, QString* error) const;
};
