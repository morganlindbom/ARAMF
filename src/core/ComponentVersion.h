#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <optional>

struct ProductVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    QString toString() const;
    static bool parse(const QString& text, ProductVersion* result, QString* error = nullptr);
};

// This is intentionally not semantic versioning.  The fields are stable
// component identity plus explicit approval/revision metadata.
struct ComponentVersion
{
    int approvedRelease = 0;
    int parent = 0;
    int item = 0;
    int revision = 0;

    QString toString() const;
    static bool parse(const QString& text, ComponentVersion* result, QString* error = nullptr);
    bool isValid(QString* error = nullptr) const;
    ComponentVersion incrementedRevision(QString* error = nullptr) const;
};

struct ReleaseComponent
{
    QString id;
    QString displayName;
    ComponentVersion version;
    bool releaseRequired = true;
    bool enabled = true;
    QString legacyVersion;
};

struct ReleaseApproval
{
    QString approvalId;
    QString componentId;
    QString componentVersion;
    int targetRelease = 0;
    QString productVersion;
    QString authority;
    QString approvedAt;
    QString evidenceReference;
};

struct ReleaseReadinessSummary
{
    bool targetRequested = false;
    int targetRelease = 0;
    int totalRequired = 0;
    int approved = 0;
    int remaining = 0;
    int percentage = 0;
    bool readyForApproval = false;
    bool generationAllowed = true;
    QList<QString> blockingComponents;
};

class ReleaseReadinessService final
{
public:
    static ReleaseReadinessSummary evaluate(const QList<ReleaseComponent>& components,
                                            const std::optional<int>& targetRelease);
};

class ReleaseManagementService final : public QObject
{
    Q_OBJECT

public:
    explicit ReleaseManagementService(QObject* parent = nullptr);

    static ProductVersion productVersion();
    static QList<ReleaseComponent> defaultComponents();
    static QJsonObject componentVersionSnapshot(const QList<ReleaseComponent>& components);
    static QString defaultStatePath();
    static void setStatePathForTests(const QString& path);

    QList<ReleaseComponent> components() const { return components_; }
    QList<ReleaseApproval> approvalHistory() const { return approvalHistory_; }
    ReleaseReadinessSummary readiness(const std::optional<int>& targetRelease) const;

    bool reload(QString* error = nullptr);
    bool save(QString* error = nullptr) const;
    bool approveComponentForRelease(const QString& componentId,
                                    int targetRelease,
                                    const QString& authority,
                                    QString* error = nullptr);

signals:
    void registryChanged();

private:
    QString statePath() const;
    QList<ReleaseComponent> components_;
    QList<ReleaseApproval> approvalHistory_;
};
