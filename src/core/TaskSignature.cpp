#include "TaskSignature.h"
#include "ProjectModel.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

namespace {
QStringList normalizeStrings(const QStringList& values, bool toLower = true)
{
    QStringList result;
    result.reserve(values.size());
    for (const auto& value : values) {
        QString item = value.trimmed();
        if (toLower) item = item.toLower();
        if (!item.isEmpty() && !result.contains(item)) {
            result.append(item);
        }
    }
    result.sort();
    return result;
}

QStringList normalizePaths(const QStringList& paths)
{
    QStringList result;
    result.reserve(paths.size());
    for (const auto& path : paths) {
        QString item = path.trimmed().replace('\\', '/');
        while (item.startsWith('/')) item.remove(0, 1);
        while (item.endsWith('/')) item.chop(1);
        item = QDir::cleanPath(item);
        if (!item.isEmpty() && item != QStringLiteral(".") && !result.contains(item)) {
            result.append(item);
        }
    }
    result.sort();
    return result;
}

double jaccardSimilarity(const QStringList& listA, const QStringList& listB)
{
    if (listA.isEmpty() || listB.isEmpty()) return 0.0;

    const QSet<QString> setA(listA.begin(), listA.end());
    const QSet<QString> setB(listB.begin(), listB.end());
    const int intersectionSize = (setA & setB).size();
    const int unionSize = (setA | setB).size();
    return unionSize > 0 ? static_cast<double>(intersectionSize) / static_cast<double>(unionSize) : 0.0;
}

QString inferSubsystemFromPaths(const QStringList& paths, const QStringList& scopes, const QString& task = {})
{
    const QString t = task.toLower();
    if (t.contains(QStringLiteral("ui")) || t.contains(QStringLiteral("workflow")) || t.contains(QStringLiteral("layout"))) return QStringLiteral("ui");
    if (t.contains(QStringLiteral("memory")) || t.contains(QStringLiteral("event-log")) || t.contains(QStringLiteral("decision"))) return QStringLiteral("memory");
    if (t.contains(QStringLiteral("orchestrat")) || t.contains(QStringLiteral("p2"))) return QStringLiteral("orchestration");
    if (t.contains(QStringLiteral("recertification")) || t.contains(QStringLiteral("governance"))) return QStringLiteral("governance");
    if (t.contains(QStringLiteral("release")) || t.contains(QStringLiteral("version"))) return QStringLiteral("release");
    if (t.contains(QStringLiteral("worker")) || t.contains(QStringLiteral("p0"))) return QStringLiteral("worker");
    if (t.contains(QStringLiteral("context")) || t.contains(QStringLiteral("p1"))) return QStringLiteral("context");

    for (const auto& scope : scopes) {
        const QString s = scope.toLower();
        if (s.contains(QStringLiteral("memory")) || s.contains(QStringLiteral("knowledge")) || s.contains(QStringLiteral("decision"))) return QStringLiteral("memory");
        if (s.contains(QStringLiteral("orchestrat")) || s.contains(QStringLiteral("p2"))) return QStringLiteral("orchestration");
        if (s.contains(QStringLiteral("worker")) || s.contains(QStringLiteral("p0")) || s.contains(QStringLiteral("task"))) return QStringLiteral("worker");
        if (s.contains(QStringLiteral("context")) || s.contains(QStringLiteral("p1"))) return QStringLiteral("context");
        if (s.contains(QStringLiteral("ui")) || s.contains(QStringLiteral("workflow"))) return QStringLiteral("ui");
        if (s.contains(QStringLiteral("template"))) return QStringLiteral("templates");
        if (s.contains(QStringLiteral("rule"))) return QStringLiteral("rules");
    }
    for (const auto& path : paths) {
        const QString p = path.toLower();
        if (p.contains(QStringLiteral("memory")) || p.contains(QStringLiteral("event-log")) || p.contains(QStringLiteral("decision"))) return QStringLiteral("memory");
        if (p.contains(QStringLiteral("orchestrat")) || p.contains(QStringLiteral("p2"))) return QStringLiteral("orchestration");
        if (p.contains(QStringLiteral("worker")) || p.contains(QStringLiteral("ownership"))) return QStringLiteral("worker");
        if (p.contains(QStringLiteral("context")) || p.contains(QStringLiteral("coordination"))) return QStringLiteral("context");
        if (p.contains(QStringLiteral("ui/")) || p.contains(QStringLiteral("workflow"))) return QStringLiteral("ui");
        if (p.contains(QStringLiteral("template"))) return QStringLiteral("templates");
        if (p.contains(QStringLiteral("rule"))) return QStringLiteral("rules");
        if (p.contains(QStringLiteral("cmakelists")) || p.contains(QStringLiteral("cmake"))) return QStringLiteral("build");
    }
    return QStringLiteral("general");
}
}

void TaskSignature::normalize()
{
    taskCategory = taskCategory.trimmed().toLower();
    operationType = operationType.trimmed().toLower();
    targetSubsystem = targetSubsystem.trimmed().toLower();
    languageFramework = languageFramework.trimmed().toLower();
    governanceClass = governanceClass.trimmed().toLower();
    resourceOwnershipClass = resourceOwnershipClass.trimmed().toLower();
    signatureVersion = signatureVersion.trimmed();
    if (signatureVersion.isEmpty()) signatureVersion = QStringLiteral("1.0");

    if (languageFramework.isEmpty()) languageFramework = QStringLiteral("cpp17/qt6");
    if (governanceClass.isEmpty()) governanceClass = QStringLiteral("governed-write");
    if (resourceOwnershipClass.isEmpty()) {
        resourceOwnershipClass = referencedFiles.isEmpty() ? QStringLiteral("none") : QStringLiteral("exclusive-file");
    }
    if (operationType.isEmpty()) operationType = QStringLiteral("modify");
    if (taskCategory.isEmpty()) taskCategory = QStringLiteral("implementation");

    relevantScopes = normalizeStrings(relevantScopes, true);
    referencedFiles = normalizePaths(referencedFiles);
    validationRequirements = normalizeStrings(validationRequirements, true);

    if (targetSubsystem.isEmpty()) {
        targetSubsystem = inferSubsystemFromPaths(referencedFiles, relevantScopes);
    }
}

QString TaskSignature::fingerprint() const
{
    TaskSignature copy = *this;
    copy.normalize();

    QJsonObject obj{
        {QStringLiteral("category"), copy.taskCategory},
        {QStringLiteral("governanceClass"), copy.governanceClass},
        {QStringLiteral("languageFramework"), copy.languageFramework},
        {QStringLiteral("operationType"), copy.operationType},
        {QStringLiteral("referencedFiles"), QJsonArray::fromStringList(copy.referencedFiles)},
        {QStringLiteral("relevantScopes"), QJsonArray::fromStringList(copy.relevantScopes)},
        {QStringLiteral("resourceOwnershipClass"), copy.resourceOwnershipClass},
        {QStringLiteral("subsystem"), copy.targetSubsystem},
        {QStringLiteral("validationRequirements"), QJsonArray::fromStringList(copy.validationRequirements)},
        {QStringLiteral("version"), copy.signatureVersion}
    };

    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool TaskSignature::isValid(QString* error) const
{
    if (taskCategory.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("taskCategory cannot be empty");
        return false;
    }
    if (operationType.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("operationType cannot be empty");
        return false;
    }
    if (targetSubsystem.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("targetSubsystem cannot be empty");
        return false;
    }
    return true;
}

QJsonObject TaskSignature::toJson() const
{
    TaskSignature normalized = *this;
    normalized.normalize();

    return QJsonObject{
        {QStringLiteral("taskCategory"), normalized.taskCategory},
        {QStringLiteral("operationType"), normalized.operationType},
        {QStringLiteral("relevantScopes"), QJsonArray::fromStringList(normalized.relevantScopes)},
        {QStringLiteral("referencedFiles"), QJsonArray::fromStringList(normalized.referencedFiles)},
        {QStringLiteral("targetSubsystem"), normalized.targetSubsystem},
        {QStringLiteral("languageFramework"), normalized.languageFramework},
        {QStringLiteral("validationRequirements"), QJsonArray::fromStringList(normalized.validationRequirements)},
        {QStringLiteral("governanceClass"), normalized.governanceClass},
        {QStringLiteral("resourceOwnershipClass"), normalized.resourceOwnershipClass},
        {QStringLiteral("signatureVersion"), normalized.signatureVersion},
        {QStringLiteral("fingerprint"), normalized.fingerprint()}
    };
}

TaskSignature TaskSignature::fromJson(const QJsonObject& value, QString* error)
{
    TaskSignature sig;
    sig.taskCategory = value.value(QStringLiteral("taskCategory")).toString();
    sig.operationType = value.value(QStringLiteral("operationType")).toString();
    sig.targetSubsystem = value.value(QStringLiteral("targetSubsystem")).toString();
    sig.languageFramework = value.value(QStringLiteral("languageFramework")).toString();
    sig.governanceClass = value.value(QStringLiteral("governanceClass")).toString();
    sig.resourceOwnershipClass = value.value(QStringLiteral("resourceOwnershipClass")).toString();
    sig.signatureVersion = value.value(QStringLiteral("signatureVersion")).toString(QStringLiteral("1.0"));

    for (const auto& s : value.value(QStringLiteral("relevantScopes")).toArray()) {
        sig.relevantScopes.append(s.toString());
    }
    for (const auto& f : value.value(QStringLiteral("referencedFiles")).toArray()) {
        sig.referencedFiles.append(f.toString());
    }
    for (const auto& v : value.value(QStringLiteral("validationRequirements")).toArray()) {
        sig.validationRequirements.append(v.toString());
    }

    sig.normalize();
    if (!sig.isValid(error)) {
        return {};
    }
    return sig;
}

TaskSignature TaskSignature::fromWorkerTaskRequest(const WorkerTaskRequest& request, const ProjectModel* model)
{
    Q_UNUSED(model);
    TaskSignature sig;
    sig.taskCategory = request.type.isEmpty() ? QStringLiteral("implementation") : request.type;
    sig.relevantScopes = request.scopes;
    sig.referencedFiles = request.files;
    sig.targetSubsystem = inferSubsystemFromPaths(request.files, request.scopes);

    if (request.destructive) {
        sig.governanceClass = QStringLiteral("destructive");
        sig.operationType = QStringLiteral("delete");
    } else if (request.files.isEmpty()) {
        sig.governanceClass = QStringLiteral("read-only");
        sig.operationType = QStringLiteral("inspect");
    } else {
        sig.governanceClass = QStringLiteral("governed-write");
        sig.operationType = QStringLiteral("modify");
    }

    sig.resourceOwnershipClass = request.files.isEmpty() ? QStringLiteral("none") : QStringLiteral("exclusive-file");
    sig.normalize();
    return sig;
}

TaskSignature TaskSignature::fromTaskAndCategory(const QString& task, const QString& category,
                                                 const QStringList& scopes, const QStringList& files)
{
    TaskSignature sig;
    if (category.isEmpty()) {
        const QString lower = task.toLower();
        if (lower.contains(QStringLiteral("ui")) || lower.contains(QStringLiteral("layout"))) sig.taskCategory = QStringLiteral("ui");
        else if (lower.contains(QStringLiteral("memory"))) sig.taskCategory = QStringLiteral("memory");
        else if (lower.contains(QStringLiteral("release")) || lower.contains(QStringLiteral("version"))) sig.taskCategory = QStringLiteral("release");
        else if (lower.contains(QStringLiteral("recertification")) || lower.contains(QStringLiteral("governance"))) sig.taskCategory = QStringLiteral("governance");
        else if (lower.contains(QStringLiteral("orchestrat")) || lower.contains(QStringLiteral("p2"))) sig.taskCategory = QStringLiteral("orchestration");
        else if (lower.contains(QStringLiteral("worker")) || lower.contains(QStringLiteral("p0"))) sig.taskCategory = QStringLiteral("worker");
        else if (lower.contains(QStringLiteral("context")) || lower.contains(QStringLiteral("p1"))) sig.taskCategory = QStringLiteral("context");
        else sig.taskCategory = QStringLiteral("implementation");
    } else {
        sig.taskCategory = category;
    }
    sig.relevantScopes = scopes;
    sig.referencedFiles = files;
    sig.targetSubsystem = inferSubsystemFromPaths(files, scopes, task);

    const QString lowerTask = task.toLower();
    if (lowerTask.startsWith(QStringLiteral("add")) || lowerTask.startsWith(QStringLiteral("create"))) {
        sig.operationType = QStringLiteral("create");
    } else if (lowerTask.startsWith(QStringLiteral("remove")) || lowerTask.startsWith(QStringLiteral("delete"))) {
        sig.operationType = QStringLiteral("delete");
    } else if (lowerTask.startsWith(QStringLiteral("verify")) || lowerTask.startsWith(QStringLiteral("validate"))
               || lowerTask.startsWith(QStringLiteral("test")) || lowerTask.startsWith(QStringLiteral("audit"))) {
        sig.operationType = QStringLiteral("validate");
        sig.governanceClass = QStringLiteral("read-only");
    } else {
        sig.operationType = QStringLiteral("modify");
    }

    sig.normalize();
    return sig;
}

double TaskSignature::similarity(const TaskSignature& other) const
{
    TaskSignature a = *this;
    TaskSignature b = other;
    a.normalize();
    b.normalize();

    double score = 0.0;
    if (a.taskCategory == b.taskCategory) score += 0.25;
    if (a.targetSubsystem == b.targetSubsystem) score += 0.25;
    if (a.operationType == b.operationType) score += 0.15;
    score += jaccardSimilarity(a.relevantScopes, b.relevantScopes) * 0.20;
    score += jaccardSimilarity(a.referencedFiles, b.referencedFiles) * 0.15;

    return qBound(0.0, score, 1.0);
}

bool TaskSignature::matches(const TaskSignature& other, double threshold, double* score) const
{
    const double s = similarity(other);
    if (score) *score = s;
    return s >= threshold;
}
