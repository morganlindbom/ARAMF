#include "ComponentVersion.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <limits>

namespace {

QStringList parts(const QString& text)
{
    return text.trimmed().split(QLatin1Char('.'), Qt::KeepEmptyParts);
}

bool nonNegativePart(const QString& value, int* result)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < 0) return false;
    if (result) *result = parsed;
    return true;
}

QString& statePathOverride()
{
    static QString path;
    return path;
}

QJsonArray approvalsToJson(const QList<ReleaseApproval>& approvals)
{
    QJsonArray result;
    for (const auto& approval : approvals) {
        result.append(QJsonObject{
            {QStringLiteral("approvalId"), approval.approvalId},
            {QStringLiteral("componentId"), approval.componentId},
            {QStringLiteral("componentVersion"), approval.componentVersion},
            {QStringLiteral("targetRelease"), approval.targetRelease},
            {QStringLiteral("productVersion"), approval.productVersion},
            {QStringLiteral("authority"), approval.authority},
            {QStringLiteral("approvedAt"), approval.approvedAt},
            {QStringLiteral("evidenceReference"), approval.evidenceReference}});
    }
    return result;
}

} // namespace

QString ProductVersion::toString() const
{
    return QStringLiteral("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

bool ProductVersion::parse(const QString& text, ProductVersion* result, QString* error)
{
    const auto values = parts(text);
    if (values.size() != 3) {
        if (error) *error = QStringLiteral("Product version must contain MAJOR.MINOR.PATCH.");
        return false;
    }
    ProductVersion parsed;
    if (!nonNegativePart(values.at(0), &parsed.major)
        || !nonNegativePart(values.at(1), &parsed.minor)
        || !nonNegativePart(values.at(2), &parsed.patch)) {
        if (error) *error = QStringLiteral("Product version fields must be non-negative integers.");
        return false;
    }
    if (result) *result = parsed;
    return true;
}

QString ComponentVersion::toString() const
{
    return QStringLiteral("%1.%2.%3.%4")
        .arg(approvedRelease).arg(parent).arg(item).arg(revision);
}

bool ComponentVersion::parse(const QString& text, ComponentVersion* result, QString* error)
{
    const auto values = parts(text);
    if (values.size() != 4) {
        if (error) *error = QStringLiteral("Component version must contain APPROVED_RELEASE.PARENT.ITEM.REVISION.");
        return false;
    }
    ComponentVersion parsed;
    if (!nonNegativePart(values.at(0), &parsed.approvedRelease)
        || !nonNegativePart(values.at(1), &parsed.parent)
        || !nonNegativePart(values.at(2), &parsed.item)
        || !nonNegativePart(values.at(3), &parsed.revision)) {
        if (error) *error = QStringLiteral("Component version fields must be non-negative integers.");
        return false;
    }
    if (result) *result = parsed;
    return true;
}

bool ComponentVersion::isValid(QString* error) const
{
    if (approvedRelease < 0 || parent < 0 || item < 0 || revision < 0) {
        if (error) *error = QStringLiteral("Component version fields must be non-negative.");
        return false;
    }
    return true;
}

ComponentVersion ComponentVersion::incrementedRevision(QString* error) const
{
    if (!isValid(error) || revision == std::numeric_limits<int>::max()) {
        if (error && error->isEmpty()) *error = QStringLiteral("Component revision cannot be incremented further.");
        return *this;
    }
    ComponentVersion result = *this;
    ++result.revision;
    return result;
}

ReleaseReadinessSummary ReleaseReadinessService::evaluate(
    const QList<ReleaseComponent>& components,
    const std::optional<int>& targetRelease)
{
    ReleaseReadinessSummary result;
    result.generationAllowed = true;
    if (!targetRelease.has_value() || targetRelease.value() <= 0) return result;

    result.targetRequested = true;
    result.targetRelease = targetRelease.value();
    for (const auto& component : components) {
        if (!component.enabled || !component.releaseRequired) continue;
        ++result.totalRequired;
        if (component.version.approvedRelease >= result.targetRelease) ++result.approved;
        else result.blockingComponents.append(component.id);
    }
    result.remaining = result.totalRequired - result.approved;
    result.percentage = result.totalRequired == 0
        ? 100
        : qRound(100.0 * result.approved / result.totalRequired);
    result.readyForApproval = result.remaining == 0;
    return result;
}

ProductVersion ReleaseManagementService::productVersion()
{
    // Keep this separate from ComponentVersion: it is the ARAMF product
    // version, not a page/component approval identity.
    return ProductVersion{0, 0, 0};
}

QList<ReleaseComponent> ReleaseManagementService::defaultComponents()
{
    struct Entry { const char* id; const char* name; int parent; int item; };
    static const Entry entries[] = {
        {"project.overview", "Project / What is the project?", 1, 0},
        {"project.file-worker", "Project / Project file, path & Worker", 1, 1},
        {"project.modules-templates", "Project / Project modules & templates", 1, 2},
        {"project.academic", "Project / Academic", 1, 3},
        {"project.languages", "Project / Which languages are used?", 1, 4},
        {"project.frameworks", "Project / Which frameworks / SDKs are used?", 1, 5},
        {"project.development-tools", "Project / Which development tools are used?", 1, 6},
        {"project.platforms", "Project / Where does the project run?", 1, 7},
        {"project.hardware-architecture", "Project / Which hardware / architecture is used?", 1, 8},
        {"project.build-delivery", "Project / How it is built, tested and delivered", 1, 9},
        {"ai.agents", "AI / Which AI agents are used?", 2, 1},
        {"ai.responsibilities", "AI / What may AI work on?", 2, 2},
        {"ai.autonomy", "AI / How autonomous may AI be?", 2, 3},
        {"ai.integration", "AI / Which ARAMF systems should AI use?", 2, 4},
        {"resources.inventory", "Resources / Which resources belong to the project?", 3, 1},
        {"resources.authority", "Resources / Which sources are authoritative?", 3, 2},
        {"resources.policy", "Resources / How should AI use the resources?", 3, 3},
        {"rules.selection", "Rules / Which rules should apply?", 4, 1},
        {"rules.routing", "Rules / How should rules be routed?", 4, 2},
        {"memory.capture", "Memory / What should ARAMF remember?", 5, 1},
        {"memory.maintenance", "Memory / How should project memory be maintained?", 5, 2},
        {"release.overview", "Release / Version & release management", 6, 0},
        {"release.product-version", "Release / Product version", 6, 1},
        {"release.component-versions", "Release / Page & component versions", 6, 2},
        {"release.schema-compatibility", "Release / Schema & compatibility", 6, 3},
        {"release.readiness", "Release / Release readiness", 6, 4},
        {"release.approval-history", "Release / Approval & release history", 6, 5},
        {"generate.review", "Generate / Review", 7, 1},
        {"generate.generate", "Generate / Generate", 7, 2},
        {"generate.verify", "Generate / Verify", 7, 3},
        {"generate.finalize", "Generate / Finalize", 7, 4},
        {"update.review", "Update / Review Framework Knowledge", 8, 1},
        {"update.apply", "Update / Apply Framework Knowledge", 8, 2},
        {"update.improvement-backlog", "Update / ARAMF Improvement Backlog", 8, 3}
    };
    QList<ReleaseComponent> result;
    for (const auto& entry : entries) {
        result.append(ReleaseComponent{
            QString::fromUtf8(entry.id), QString::fromUtf8(entry.name),
            ComponentVersion{0, entry.parent, entry.item, 0}, true, true, {}});
    }
    return result;
}

QJsonObject ReleaseManagementService::componentVersionSnapshot(const QList<ReleaseComponent>& components)
{
    QJsonObject result;
    for (const auto& component : components) result.insert(component.id, component.version.toString());
    return result;
}

QString ReleaseManagementService::defaultStatePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base.isEmpty() ? QDir::tempPath() : base).filePath(QStringLiteral("release-management.json"));
}

void ReleaseManagementService::setStatePathForTests(const QString& path)
{
    statePathOverride() = path;
}

ReleaseManagementService::ReleaseManagementService(QObject* parent)
    : QObject(parent), components_(defaultComponents())
{
    reload(nullptr);
}

QString ReleaseManagementService::statePath() const
{
    return statePathOverride().isEmpty() ? defaultStatePath() : statePathOverride();
}

ReleaseReadinessSummary ReleaseManagementService::readiness(const std::optional<int>& targetRelease) const
{
    return ReleaseReadinessService::evaluate(components_, targetRelease);
}

bool ReleaseManagementService::reload(QString* error)
{
    if (error) error->clear();
    components_ = defaultComponents();
    approvalHistory_.clear();
    QFile file(statePath());
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("Release management state is malformed: %1").arg(parseError.errorString());
        return false;
    }
    const auto root = document.object();
    const auto storedComponents = root.value(QStringLiteral("components")).toArray();
    for (const auto& value : storedComponents) {
        const auto object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        auto it = std::find_if(components_.begin(), components_.end(), [&id](const ReleaseComponent& component) { return component.id == id; });
        if (it == components_.end()) continue;
        ComponentVersion parsed;
        QString parseErrorText;
        if (!ComponentVersion::parse(object.value(QStringLiteral("version")).toString(), &parsed, &parseErrorText)) {
            if (error) *error = QStringLiteral("Invalid component version for %1: %2").arg(id, parseErrorText);
            return false;
        }
        // Parent/item are stable component identity.  Persisted release state
        // may update approval/revision values, but it must never renumber the
        // canonical identity derived from the component ID.
        parsed.parent = it->version.parent;
        parsed.item = it->version.item;
        it->version = parsed;
        it->releaseRequired = object.value(QStringLiteral("releaseRequired")).toBool(it->releaseRequired);
        it->enabled = object.value(QStringLiteral("enabled")).toBool(it->enabled);
        it->legacyVersion = object.value(QStringLiteral("legacyVersion")).toString();
    }
    for (const auto& value : root.value(QStringLiteral("approvalHistory")).toArray()) {
        const auto object = value.toObject();
        ReleaseApproval approval{
            object.value(QStringLiteral("approvalId")).toString(),
            object.value(QStringLiteral("componentId")).toString(),
            object.value(QStringLiteral("componentVersion")).toString(),
            object.value(QStringLiteral("targetRelease")).toInt(0),
            object.value(QStringLiteral("productVersion")).toString(),
            object.value(QStringLiteral("authority")).toString(),
            object.value(QStringLiteral("approvedAt")).toString(),
            object.value(QStringLiteral("evidenceReference")).toString()};
        if (!approval.approvalId.isEmpty() && approval.targetRelease > 0) approvalHistory_.append(approval);
    }
    for (auto& component : components_) {
        int approvedRelease = 0;
        int authoritativeRevision = 0;
        bool hasAuthoritativeRevision = false;
        for (const auto& approval : approvalHistory_) {
            if (approval.componentId != component.id) continue;
            ComponentVersion approvedVersion;
            if (!ComponentVersion::parse(approval.componentVersion, &approvedVersion)
                || approvedVersion.parent != component.version.parent
                || approvedVersion.item != component.version.item) continue;
            approvedRelease = std::max(approvedRelease, approval.targetRelease);
            authoritativeRevision = std::max(authoritativeRevision, approvedVersion.revision);
            hasAuthoritativeRevision = true;
        }
        if (approvedRelease == 0) {
            component.version.approvedRelease = 0;
            component.version.revision = 0;
        } else {
            component.version.approvedRelease = approvedRelease;
            component.version.revision = hasAuthoritativeRevision ? authoritativeRevision : 0;
        }
    }
    emit registryChanged();
    return true;
}

bool ReleaseManagementService::save(QString* error) const
{
    if (error) error->clear();
    QJsonArray components;
    for (const auto& component : components_) {
        components.append(QJsonObject{
            {QStringLiteral("id"), component.id},
            {QStringLiteral("displayName"), component.displayName},
            {QStringLiteral("version"), component.version.toString()},
            {QStringLiteral("releaseRequired"), component.releaseRequired},
            {QStringLiteral("enabled"), component.enabled},
            {QStringLiteral("legacyVersion"), component.legacyVersion}});
    }
    const QJsonObject root{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("productVersion"), productVersion().toString()},
        {QStringLiteral("components"), components},
        {QStringLiteral("approvalHistory"), approvalsToJson(approvalHistory_)}};
    QDir().mkpath(QFileInfo(statePath()).absolutePath());
    QSaveFile file(statePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ReleaseManagementService::approveComponentForRelease(const QString& componentId,
                                                          int targetRelease,
                                                          const QString& authority,
                                                          QString* error)
{
    if (error) error->clear();
    if (targetRelease <= 0) {
        if (error) *error = QStringLiteral("Target release must be a positive integer.");
        return false;
    }
    if (authority != QStringLiteral("user") && authority != QStringLiteral("manager")) {
        if (error) *error = QStringLiteral("Only the authorized user or Manager role may approve a component.");
        return false;
    }
    auto it = std::find_if(components_.begin(), components_.end(), [&componentId](const ReleaseComponent& component) { return component.id == componentId; });
    if (it == components_.end()) {
        if (error) *error = QStringLiteral("Unknown release component: %1").arg(componentId);
        return false;
    }
    if (targetRelease < it->version.approvedRelease) {
        if (error) *error = QStringLiteral("Approved release cannot decrease from %1 to %2.")
            .arg(it->version.approvedRelease).arg(targetRelease);
        return false;
    }
    if (targetRelease == it->version.approvedRelease) return true;

    const ReleaseComponent original = *it;
    ReleaseComponent updated = original;
    updated.version.approvedRelease = targetRelease;
    const ReleaseApproval approval{
        QStringLiteral("approval-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)),
        updated.id,
        updated.version.toString(),
        targetRelease,
        productVersion().toString(),
        authority,
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate),
        QStringLiteral("explicit-authorized-approval")};
    *it = updated;
    approvalHistory_.append(approval);
    QString saveError;
    if (!save(&saveError)) {
        *it = original;
        approvalHistory_.removeLast();
        if (error) *error = saveError;
        return false;
    }
    emit registryChanged();
    return true;
}
