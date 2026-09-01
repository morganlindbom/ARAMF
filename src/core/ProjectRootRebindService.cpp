#include "ProjectRootRebindService.h"

#include "AramfPaths.h"
#include "ProjectModel.h"
#include "ProjectPersistence.h"
#include "Services.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>
#include <QStringList>

namespace {
QString normalized(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString& left, const QString& right)
{
    return normalized(left).compare(normalized(right), Qt::CaseInsensitive) == 0;
}

bool underRoot(const QString& path, const QString& root)
{
    const QString candidate = normalized(path);
    const QString base = normalized(root);
    return candidate.compare(base, Qt::CaseInsensitive) == 0
        || candidate.startsWith(base + QDir::separator(), Qt::CaseInsensitive);
}

QString rebasePath(const QString& value, const QString& oldRoot, const QString& newRoot)
{
    if (value.trimmed().isEmpty()) return value;
    const QString normalizedValue = QDir::fromNativeSeparators(value);
    const QString normalizedOld = QDir::fromNativeSeparators(normalized(oldRoot));
    if (!normalizedValue.startsWith(normalizedOld, Qt::CaseInsensitive)) return value;
    const QString relative = QDir(oldRoot).relativeFilePath(value);
    return QDir(newRoot).filePath(relative);
}

void replaceStrings(QJsonValue* value, const QString& oldRoot, const QString& newRoot)
{
    if (value->isString()) {
        const QString text = value->toString();
        const QString rebased = rebasePath(text, oldRoot, newRoot);
        if (rebased != text) *value = rebased;
    } else if (value->isObject()) {
        QJsonObject object = value->toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            QJsonValue child = it.value();
            replaceStrings(&child, oldRoot, newRoot);
            it.value() = child;
        }
        *value = object;
    } else if (value->isArray()) {
        QJsonArray array = value->toArray();
        for (int i = 0; i < array.size(); ++i) {
            QJsonValue child = array.at(i);
            replaceStrings(&child, oldRoot, newRoot);
            array[i] = child;
        }
        *value = array;
    }
}

bool rewriteFile(const QString& path, const QString& oldRoot, const QString& newRoot, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return true;
    const QByteArray original = file.readAll();
    file.close();
    QString text = QString::fromUtf8(original);
    text.replace(oldRoot, newRoot, Qt::CaseInsensitive);
    QString oldForward = oldRoot; oldForward.replace('\\', '/');
    QString newForward = newRoot; newForward.replace('\\', '/');
    text.replace(oldForward, newForward, Qt::CaseInsensitive);
    if (text.toUtf8() == original) return true;
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)
        || output.write(text.toUtf8()) != text.toUtf8().size() || !output.commit()) {
        if (error) *error = output.errorString();
        return false;
    }
    return true;
}

bool writeJson(const QString& path, const QJsonObject& object, QString* error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }
    const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) { if (error) *error = file.errorString(); return false; }
    return true;
}

bool preserved(const QString& path)
{
    QString relative = path;
    relative.replace('\\', '/');
    return relative.contains(QStringLiteral("/memory/event-log.jsonl"), Qt::CaseInsensitive)
        || relative.contains(QStringLiteral("/memory/decisions.md"), Qt::CaseInsensitive)
        || relative.contains(QStringLiteral("/memory/framework-knowledge.json"), Qt::CaseInsensitive)
        || relative.contains(QStringLiteral("/memory/checkpoints"), Qt::CaseInsensitive)
        || relative.contains(QStringLiteral("/custom/"), Qt::CaseInsensitive)
        || relative.contains(QStringLiteral("/certification/evidence/"), Qt::CaseInsensitive);
}

void downgradeBroadCertificationClaim(const QString& root)
{
    const QString path = QDir(root).filePath(AramfPaths::ProjectStatus);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString text = QString::fromUtf8(file.readAll());
    file.close();
    text.replace(QStringLiteral("Current state: VERIFIED / CERTIFIED"), QStringLiteral("Current state: PARTIAL"), Qt::CaseInsensitive);
    QSaveFile output(path);
    if (output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        const QByteArray data = text.toUtf8();
        if (output.write(data) == data.size()) output.commit();
    }
}
}

ProjectRootRebindResult ProjectRootRebindService::rebind(ProjectModel* model,
                                                         const QString& openedProjectRoot,
                                                         bool regenerate) const
{
    ProjectRootRebindResult result;
    if (!model) { result.error = QStringLiteral("Project model is not available."); return result; }
    result.savedRoot = normalized(model->projectPath());
    result.currentRoot = normalized(openedProjectRoot);
    if (result.currentRoot.isEmpty() || !QDir(result.currentRoot).exists()) {
        result.error = QStringLiteral("Opened project root does not exist: %1").arg(openedProjectRoot);
        return result;
    }
    result.rebound = !samePath(result.savedRoot, result.currentRoot);
    if (result.rebound) {
        auto resources = model->resources();
        for (auto& resource : resources) resource.location = rebasePath(resource.location, result.savedRoot, result.currentRoot);
        model->beginUpdate();
        model->setProjectPath(result.currentRoot);
        model->setResources(resources);
        model->endUpdate();
        if (!model->projectFilePath().isEmpty()) {
            ProjectPersistence persistence;
            if (!persistence.save(*model, model->projectFilePath(), &result.error)) return result;
        }
    }

    const QString worker = QDir(result.currentRoot).filePath(AramfPaths::ControlDirectory);
    if (QDir(worker).exists() && result.rebound) {
        QDirIterator iterator(worker, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString path = iterator.next();
            if (preserved(path)) continue;
            QString error;
            if (!rewriteFile(path, result.savedRoot, result.currentRoot, &error)) { result.error = error; return result; }
            if (!error.isEmpty()) result.warnings << error;
        }
    }

    if (regenerate) {
        GenerationServices generation;
        const auto generated = generation.generate(*model, model->generationOptions());
        result.warnings.append(generated.warnings);
        if (!generated.success) { result.error = generated.error; return result; }
        if (model->certificationConfiguration().enabled) {
            result.warnings << QStringLiteral("Certification control plane regenerated for the active root.");
            downgradeBroadCertificationClaim(result.currentRoot);
        }
    }

    // Always replace any prior result with a fresh, non-destructive verification.
    // On a root change this is what prevents old PASS evidence becoming current.
    const auto verification = VerificationServices().verify(*model, model->generationOptions());
    if (verification.overallStatus == VerificationStatus::Fail) {
        result.warnings << QStringLiteral("Project-root update completed, but fresh verification did not pass.");
    }
    result.success = true;
    return result;
}

int runProjectRootRebindCommand(const QStringList& arguments, QTextStream& output, QTextStream& error)
{
    if (arguments.size() < 5 || arguments.at(0) != QStringLiteral("project") || arguments.at(1) != QStringLiteral("rebind")) {
        error << "Usage: aramf project rebind --file <project-file> --root <project-root>\n";
        return 2;
    }
    QString filePath, root;
    for (int i = 2; i + 1 < arguments.size(); ++i) {
        if (arguments.at(i) == QStringLiteral("--file")) filePath = arguments.at(++i);
        else if (arguments.at(i) == QStringLiteral("--root")) root = arguments.at(++i);
    }
    ProjectModel model;
    ProjectPersistence persistence;
    QString operationError;
    if (!persistence.load(&model, filePath, &operationError)) { error << "error=" << operationError << "\n"; return 2; }
    model.setProjectFilePath(filePath);
    if (arguments.contains(QStringLiteral("--enable-certification")))
        model.setCertificationConfiguration({true, model.certificationConfiguration().defaultVerificationLevel});
    const auto result = ProjectRootRebindService().rebind(&model, root, true);
    if (!result.success) { error << "error=" << result.error << "\n"; return 2; }
    output << "rebound=" << (result.rebound ? "true" : "false") << " root=" << result.currentRoot << "\n";
    return 0;
}
