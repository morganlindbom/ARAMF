// MemoryEvidenceFoundation.cpp
// Implementation of F1: Memory & Evidence Foundation.
// Pure physical evidence storage, append-only ledger integrity, hardened reconstruction, and queries.
// Zero semantic dependency on F2, F3, F4, or P1-P14.

#include "MemoryEvidenceFoundation.h"
#include "AramfPaths.h"
#include "ProjectMemory.h"
#include "ProjectMemoryCompaction.h"
#include "CertificationService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>

namespace {

QByteArray readRawFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

QJsonObject readJsonMap(const QString& path)
{
    const auto data = readRawFile(path);
    if (data.isEmpty()) return {};
    return QJsonDocument::fromJson(data).object();
}

QString computeFileSha256(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!f.atEnd()) {
        hash.addData(f.read(65536));
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool isPersistedEvidenceReference(const QString& reference)
{
    return QFileInfo(reference).isAbsolute()
        || reference.startsWith(QStringLiteral("ARAMF_WORKER/"))
        || reference.startsWith(QStringLiteral("ARAMF_WORKER\\"));
}

} // anonymous namespace

// ─── Physical Evidence Validation ────────────────────────────────────────────

F1EvidenceReport MemoryEvidenceFoundation::validate(const QString& projectRoot, QString* error)
{
    F1EvidenceReport report;
    report.valid = false;
    report.schemaCompatible = true;

    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    QFile eventFile(eventLogPath);
    if (!eventFile.exists()) {
        report.errors.append(QStringLiteral("F1: event-log.jsonl does not exist"));
        if (error) *error = report.errors.join(QStringLiteral("; "));
        return report;
    }

    if (!eventFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        report.errors.append(QStringLiteral("F1: Cannot open event-log.jsonl: %1").arg(eventFile.errorString()));
        if (error) *error = report.errors.join(QStringLiteral("; "));
        return report;
    }

    // 1. Scan ledger line by line for physical/structural integrity
    qint64 lastSeq = 0;
    QString lastId;
    int lineCount = 0;
    int eventCount = 0;
    QSet<QString> seenIds;
    QSet<qint64> seenSeqs;

    // Load acknowledged legacy event ID exceptions if present
    const QString excPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-id-integrity-exceptions.json"));
    QSet<QString> acknowledgedDuplicates;
    const auto excObj = readJsonMap(excPath);
    for (const auto& item : excObj.value(QStringLiteral("exceptions")).toArray()) {
        const QString id = item.toObject().value(QStringLiteral("eventId")).toString().trimmed();
        if (!id.isEmpty()) acknowledgedDuplicates.insert(id);
    }

    bool ledgerOk = true;
    bool sequenceOk = true;

    while (!eventFile.atEnd()) {
        lineCount++;
        const QByteArray rawLine = eventFile.readLine().trimmed();
        if (rawLine.isEmpty()) continue;

        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(rawLine, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            ledgerOk = false;
            report.errors.append(QStringLiteral("F1: Malformed JSON in event-log.jsonl at line %1: %2")
                .arg(lineCount).arg(parseErr.errorString()));
            break;
        }

        if (!doc.isObject()) {
            ledgerOk = false;
            report.errors.append(QStringLiteral("F1: Non-object JSON in event-log.jsonl at line %1").arg(lineCount));
            break;
        }

        const auto obj = doc.object();
        const QString eventId = obj.value(QStringLiteral("eventId")).toString().trimmed();
        if (eventId.isEmpty()) {
            ledgerOk = false;
            report.errors.append(QStringLiteral("F1: Missing eventId at line %1").arg(lineCount));
        } else if (seenIds.contains(eventId) && !acknowledgedDuplicates.contains(eventId)) {
            ledgerOk = false;
            report.errors.append(QStringLiteral("F1: Duplicate eventId '%1' at line %2").arg(eventId).arg(lineCount));
        }
        seenIds.insert(eventId);

        if (!obj.contains(QStringLiteral("sequenceNumber"))) {
            sequenceOk = false;
            report.errors.append(QStringLiteral("F1: Missing sequenceNumber at line %1").arg(lineCount));
        } else {
            const qint64 seq = obj.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
            if (seq <= 0) {
                sequenceOk = false;
                report.errors.append(QStringLiteral("F1: Invalid sequenceNumber %1 at line %2").arg(seq).arg(lineCount));
            } else if (seenSeqs.contains(seq)) {
                sequenceOk = false;
                report.errors.append(QStringLiteral("F1: Duplicate sequenceNumber %1 at line %2").arg(seq).arg(lineCount));
            } else if (seq <= lastSeq) {
                sequenceOk = false;
                report.errors.append(QStringLiteral("F1: Non-monotonic sequenceNumber %1 at line %2 (previous was %3)")
                    .arg(seq).arg(lineCount).arg(lastSeq));
            }
            seenSeqs.insert(seq);
            lastSeq = seq;
        }

        if (obj.contains(QStringLiteral("schemaVersion"))) {
            const int schemaVer = obj.value(QStringLiteral("schemaVersion")).toInt();
            if (schemaVer > 3) {
                report.schemaCompatible = false;
                report.errors.append(QStringLiteral("F1: Unsupported event schema version %1 at line %2").arg(schemaVer).arg(lineCount));
            }
        }

        lastId = eventId;
        eventCount++;
    }
    eventFile.close();

    report.ledgerIntact = ledgerOk;
    report.sequenceMonotonic = sequenceOk;
    report.totalEvents = eventCount;
    report.latestSequenceNumber = lastSeq;
    report.evidenceFingerprint = computeFileSha256(eventLogPath);

    // 2. Manifest physical consistency
    const QString manifestPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.exists()) {
        report.manifestConsistent = false;
        report.recoveryRequired = report.ledgerIntact;
        report.recoveryReason = QStringLiteral("Manifest file memory-manifest.json missing; reconstructible from ledger");
        report.errors.append(QStringLiteral("F1: Manifest file memory-manifest.json missing"));
    } else {
        const auto manifest = readJsonMap(manifestPath);
        if (manifest.isEmpty()) {
            report.manifestConsistent = false;
            report.recoveryRequired = report.ledgerIntact;
            report.recoveryReason = QStringLiteral("Manifest file memory-manifest.json is malformed; reconstructible from ledger");
            report.errors.append(QStringLiteral("F1: Manifest file memory-manifest.json is malformed"));
        } else {
            const QString mVer = manifest.value(QStringLiteral("memoryVersion")).toVariant().toString().trimmed();
            bool isNum = false;
            const double numVer = mVer.toDouble(&isNum);
            if (!mVer.isEmpty() && mVer != QStringLiteral("1") && mVer != QStringLiteral("2") && mVer != QStringLiteral("3") && (!isNum || numVer > 3.0)) {
                report.schemaCompatible = false;
                report.errors.append(QStringLiteral("F1: Unsupported memory version '%1' in manifest").arg(mVer));
            }

            const qint64 nextSeq = manifest.value(QStringLiteral("nextSequenceNumber")).toVariant().toLongLong();
            const int manifestCount = manifest.value(QStringLiteral("eventCount")).toInt();
            const QString manifestLatestId = manifest.value(QStringLiteral("latestEventId")).toString();

            bool manifestOk = true;
            if (nextSeq != lastSeq + 1) {
                manifestOk = false;
                report.recoveryReason = QStringLiteral("Manifest nextSequenceNumber %1 does not match ledger maximum + 1 (%2)")
                    .arg(nextSeq).arg(lastSeq + 1);
                report.errors.append(QStringLiteral("F1: %1").arg(report.recoveryReason));
            }
            if (manifestCount != eventCount) {
                manifestOk = false;
                report.recoveryReason = QStringLiteral("Manifest eventCount %1 does not match ledger event count %2")
                    .arg(manifestCount).arg(eventCount);
                report.errors.append(QStringLiteral("F1: %1").arg(report.recoveryReason));
            }
            if (eventCount > 0 && !manifestLatestId.isEmpty() && manifestLatestId != lastId) {
                manifestOk = false;
                report.recoveryReason = QStringLiteral("Manifest latestEventId '%1' does not match latest ledger event '%2'")
                    .arg(manifestLatestId, lastId);
                report.errors.append(QStringLiteral("F1: %1").arg(report.recoveryReason));
            }

            report.manifestConsistent = manifestOk;
            if (!manifestOk && report.ledgerIntact) {
                report.recoveryRequired = true;
            }
        }
    }

    // 3. Metrics consistency
    const QString metricsPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/metrics.json"));
    QFile metricsFile(metricsPath);
    if (metricsFile.exists()) {
        const auto metrics = readJsonMap(metricsPath);
        if (metrics.isEmpty()) {
            report.metricsConsistent = false;
            if (report.ledgerIntact) report.recoveryRequired = true;
            report.errors.append(QStringLiteral("F1: metrics.json is malformed"));
        } else {
            bool metricsOk = true;
            if (metrics.contains(QStringLiteral("totalEvents"))) {
                if (metrics.value(QStringLiteral("totalEvents")).toInt() != eventCount) metricsOk = false;
            }
            if (metrics.contains(QStringLiteral("durableSequence"))) {
                if (metrics.value(QStringLiteral("durableSequence")).toVariant().toLongLong() != lastSeq) metricsOk = false;
            }
            if (!metrics.contains(QStringLiteral("activeEvents"))
                || !metrics.value(QStringLiteral("activeEvents")).isDouble()) {
                metricsOk = false;
                report.errors.append(QStringLiteral("F1: metrics.json is missing canonical activeEvents"));
            } else if (metrics.value(QStringLiteral("activeEvents")).toInt() != eventCount) {
                metricsOk = false;
                report.errors.append(QStringLiteral("F1: metrics.json activeEvents does not match ledger event count"));
            }
            if (!metrics.contains(QStringLiteral("totalEventsCreated"))
                || !metrics.value(QStringLiteral("totalEventsCreated")).isDouble()) {
                metricsOk = false;
                report.errors.append(QStringLiteral("F1: metrics.json is missing canonical totalEventsCreated"));
            }
            report.metricsConsistent = metricsOk;
            if (!metricsOk) {
                if (report.ledgerIntact) report.recoveryRequired = true;
            }
        }
    } else {
        report.metricsConsistent = false;
        if (report.ledgerIntact) report.recoveryRequired = true;
        report.errors.append(QStringLiteral("F1: metrics.json does not exist"));
    }

    // 4. Checkpoints physical integrity
    const QString checkpointsPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/checkpoints.json"));
    QFile cpFile(checkpointsPath);
    if (cpFile.exists()) {
        if (!cpFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report.checkpointsIntact = false;
            report.errors.append(QStringLiteral("F1: Cannot read checkpoints.json: %1").arg(cpFile.errorString()));
        } else {
            QJsonParseError cpErr;
            const auto doc = QJsonDocument::fromJson(cpFile.readAll(), &cpErr);
            cpFile.close();
            if (cpErr.error != QJsonParseError::NoError) {
                report.checkpointsIntact = false;
                report.errors.append(QStringLiteral("F1: Checkpoints file is malformed JSON: %1").arg(cpErr.errorString()));
            } else {
                QJsonArray cpArr;
                if (doc.isArray()) cpArr = doc.array();
                else if (doc.isObject()) cpArr = doc.object().value(QStringLiteral("checkpoints")).toArray();
                else report.checkpointsIntact = false;

                if (report.errors.isEmpty() || report.checkpointsIntact) {
                    QSet<QString> cpIds;
                    bool cpValid = true;
                    for (const auto& item : cpArr) {
                        if (!item.isObject()) { cpValid = false; break; }
                        const auto cp = item.toObject();
                        const QString cpId = cp.value(QStringLiteral("id")).toString().trimmed();
                        if (cpId.isEmpty() || cpIds.contains(cpId)) { cpValid = false; break; }
                        cpIds.insert(cpId);
                        const QString createdAt = cp.value(QStringLiteral("createdAt")).toString();
                        if (!QDateTime::fromString(createdAt, Qt::ISODate).isValid()) { cpValid = false; break; }
                        const qint64 prodSeq = cp.value(QStringLiteral("productionSequence")).toVariant().toLongLong();
                        if (prodSeq < 0 || prodSeq > lastSeq) { cpValid = false; break; }
                    }
                    report.checkpointsIntact = cpValid;
                    report.totalCheckpoints = cpIds.size();
                    if (!cpValid) {
                        report.errors.append(QStringLiteral("F1: Checkpoint records contain physically invalid fields"));
                    }
                }
            }
        }
    } else {
        report.checkpointsIntact = true;
    }

    // 5. Certification ledger physical integrity
    const QString certsPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/certification/certificates.jsonl"));
    QFile certsFile(certsPath);
    if (!certsFile.exists()) {
        report.certificatesIntact = true;
        report.totalCertificates = 0;
    } else {
        if (!certsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report.certificatesIntact = false;
            report.errors.append(QStringLiteral("F1: Cannot read certificates.jsonl: %1").arg(certsFile.errorString()));
        } else {
            int certLine = 0;
            QSet<QString> certIds;
            bool certsValid = true;
            while (!certsFile.atEnd()) {
                certLine++;
                const QByteArray cLine = certsFile.readLine().trimmed();
                if (cLine.isEmpty()) continue;
                QJsonParseError cParseErr;
                const auto doc = QJsonDocument::fromJson(cLine, &cParseErr);
                if (cParseErr.error != QJsonParseError::NoError || !doc.isObject()) {
                    certsValid = false;
                    report.errors.append(QStringLiteral("F1: Certification ledger error: Malformed certification JSONL at line %1: %2")
                        .arg(certLine).arg(cParseErr.errorString()));
                    break;
                }
                const auto certObj = doc.object();
                const QString cId = certObj.value(QStringLiteral("certificateId")).toString().trimmed();
                if (cId.isEmpty()) {
                    certsValid = false;
                    report.errors.append(QStringLiteral("F1: Certificate at line %1 is missing certificateId").arg(certLine));
                    break;
                }
                if (certIds.contains(cId)) {
                    certsValid = false;
                    report.errors.append(QStringLiteral("F1: Duplicate certificate ID '%1' at line %2").arg(cId).arg(certLine));
                    break;
                }
                certIds.insert(cId);

                // Physical evidence references check
                const auto evRefs = certObj.value(QStringLiteral("evidenceReferences")).toArray();
                for (const auto& evRef : evRefs) {
                    if (!evRef.isObject()) {
                        certsValid = false;
                        report.errors.append(QStringLiteral("F1: Certificate '%1' contains a malformed evidence reference").arg(cId));
                        break;
                    }
                    const auto refObj = evRef.toObject();
                    const QString refStr = refObj.value(QStringLiteral("reference")).toString().trimmed();
                    if (refStr.isEmpty()) {
                        certsValid = false;
                        report.errors.append(QStringLiteral("F1: Certificate '%1' contains an evidence reference without a reference").arg(cId));
                        break;
                    }
                    if (isPersistedEvidenceReference(refStr)) {
                        const QString evidencePath = QFileInfo(refStr).isAbsolute()
                            ? refStr : QDir(projectRoot).filePath(refStr);
                        if (!QFile::exists(evidencePath)) {
                            certsValid = false;
                            report.errors.append(QStringLiteral("F1: Certificate '%1' references missing evidence file '%2'")
                                .arg(cId, refStr));
                            break;
                        }
                        const QString expectedFingerprint = refObj.value(QStringLiteral("fingerprint")).toString().trimmed();
                        if (!expectedFingerprint.isEmpty() && computeFileSha256(evidencePath) != expectedFingerprint) {
                            certsValid = false;
                            report.errors.append(QStringLiteral("F1: Certificate '%1' evidence fingerprint mismatch for '%2'")
                                .arg(cId, refStr));
                            break;
                        }
                    }
                }
                if (!certsValid) break;
            }
            certsFile.close();

            // Derived certification state check
            const QString curCertPath = QDir(projectRoot).filePath(
                QStringLiteral("ARAMF_WORKER/certification/current-certification-state.json"));
            if (certsValid && QFile::exists(curCertPath)) {
                const auto curCertObj = readJsonMap(curCertPath);
                const auto subjects = curCertObj.value(QStringLiteral("subjects"));
                if (!subjects.isObject()) {
                    certsValid = false;
                    report.errors.append(QStringLiteral("F1: Current certification state has malformed subjects"));
                }
                const auto subjectMap = subjects.toObject();
                for (auto it = subjectMap.begin(); it != subjectMap.end() && certsValid; ++it) {
                    const QString refId = it.value().toObject().value(QStringLiteral("certificateId")).toString().trimmed();
                    if (refId.isEmpty() || !certIds.contains(refId)) {
                        certsValid = false;
                        report.errors.append(QStringLiteral("F1: Derived certification state references missing certificate '%1'")
                            .arg(refId.isEmpty() ? QStringLiteral("<empty>") : refId));
                        break;
                    }
                }
            }

            report.certificatesIntact = certsValid;
            report.totalCertificates = certIds.size();
        }
    }

    // 6. Cold-start reconstructability
    ProjectMemory memory;
    const auto csReport = memory.validateColdStart(projectRoot, error);
    report.coldStartFresh = (csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"));
    report.coldStartFingerprint = csReport.value(QStringLiteral("fingerprint")).toString();
    if (!report.coldStartFresh) {
        report.errors.append(QStringLiteral("F1: Cold-start reconstruction is stale or failed"));
        if (report.ledgerIntact) report.recoveryRequired = true;
    }

    // 7. Overall validity
    report.valid = report.ledgerIntact
                && report.sequenceMonotonic
                && report.manifestConsistent
                && report.certificatesIntact
                && report.metricsConsistent
                && report.checkpointsIntact
                && report.coldStartFresh
                && report.schemaCompatible;

    QJsonObject memVal{
        {QStringLiteral("status"), report.valid ? QStringLiteral("PASS") : QStringLiteral("FAIL")},
        {QStringLiteral("ledgerIntact"), report.ledgerIntact},
        {QStringLiteral("sequenceMonotonic"), report.sequenceMonotonic},
        {QStringLiteral("manifestConsistent"), report.manifestConsistent},
        {QStringLiteral("metricsConsistent"), report.metricsConsistent},
        {QStringLiteral("checkpointsIntact"), report.checkpointsIntact},
        {QStringLiteral("certificatesIntact"), report.certificatesIntact},
        {QStringLiteral("schemaCompatible"), report.schemaCompatible}
    };

    report.fullReport = QJsonObject{
        {QStringLiteral("valid"), report.valid},
        {QStringLiteral("ledgerIntact"), report.ledgerIntact},
        {QStringLiteral("sequenceMonotonic"), report.sequenceMonotonic},
        {QStringLiteral("manifestConsistent"), report.manifestConsistent},
        {QStringLiteral("certificatesIntact"), report.certificatesIntact},
        {QStringLiteral("metricsConsistent"), report.metricsConsistent},
        {QStringLiteral("checkpointsIntact"), report.checkpointsIntact},
        {QStringLiteral("coldStartFresh"), report.coldStartFresh},
        {QStringLiteral("schemaCompatible"), report.schemaCompatible},
        {QStringLiteral("recoveryRequired"), report.recoveryRequired},
        {QStringLiteral("totalEvents"), report.totalEvents},
        {QStringLiteral("totalCertificates"), report.totalCertificates},
        {QStringLiteral("totalCheckpoints"), report.totalCheckpoints},
        {QStringLiteral("latestSequenceNumber"), report.latestSequenceNumber},
        {QStringLiteral("evidenceFingerprint"), report.evidenceFingerprint},
        {QStringLiteral("coldStartFingerprint"), report.coldStartFingerprint},
        {QStringLiteral("recoveryReason"), report.recoveryReason},
        {QStringLiteral("errors"), QJsonArray::fromStringList(report.errors)},
        {QStringLiteral("memoryValidation"), memVal},
        {QStringLiteral("coldStartValidation"), csReport}
    };

    if (error && !report.errors.isEmpty()) {
        *error = report.errors.join(QStringLiteral("; "));
    }

    return report;
}

// ─── Hardened Manifest Reconstruction ────────────────────────────────────────

bool MemoryEvidenceFoundation::reconstructManifestFromLedger(const QString& projectRoot, QString* error)
{
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    QFile eventFile(eventLogPath);
    if (!eventFile.exists()) {
        if (error) *error = QStringLiteral("Cannot reconstruct manifest: event-log.jsonl does not exist");
        return false;
    }
    if (!eventFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open event-log.jsonl: %1").arg(eventFile.errorString());
        return false;
    }

    // Load acknowledged legacy event ID exceptions
    const QString excPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-id-integrity-exceptions.json"));
    QSet<QString> acknowledgedDuplicates;
    const auto excObj = readJsonMap(excPath);
    for (const auto& item : excObj.value(QStringLiteral("exceptions")).toArray()) {
        const QString id = item.toObject().value(QStringLiteral("eventId")).toString().trimmed();
        if (!id.isEmpty()) acknowledgedDuplicates.insert(id);
    }

    qint64 maxSeq = 0;
    int lineNum = 0;
    int validCount = 0;
    QString latestId;
    QSet<QString> seenIds;
    QSet<qint64> seenSeqs;

    while (!eventFile.atEnd()) {
        lineNum++;
        const QByteArray rawLine = eventFile.readLine().trimmed();
        if (rawLine.isEmpty()) continue;

        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(rawLine, &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: malformed JSON at line %1: %2")
                .arg(lineNum).arg(parseErr.errorString());
            eventFile.close();
            return false;
        }

        if (!doc.isObject()) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: event at line %1 is not a JSON object").arg(lineNum);
            eventFile.close();
            return false;
        }

        const auto obj = doc.object();
        const QString eventId = obj.value(QStringLiteral("eventId")).toString().trimmed();
        if (eventId.isEmpty()) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: event at line %1 is missing eventId").arg(lineNum);
            eventFile.close();
            return false;
        }
        if (seenIds.contains(eventId) && !acknowledgedDuplicates.contains(eventId)) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: duplicate eventId '%1' at line %2").arg(eventId).arg(lineNum);
            eventFile.close();
            return false;
        }
        seenIds.insert(eventId);

        if (!obj.contains(QStringLiteral("sequenceNumber"))) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: event at line %1 is missing sequenceNumber").arg(lineNum);
            eventFile.close();
            return false;
        }
        const qint64 seq = obj.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        if (seq <= 0) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: invalid sequenceNumber %1 at line %2").arg(seq).arg(lineNum);
            eventFile.close();
            return false;
        }
        if (seenSeqs.contains(seq)) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: duplicate sequenceNumber %1 at line %2").arg(seq).arg(lineNum);
            eventFile.close();
            return false;
        }
        if (seq <= maxSeq) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: non-monotonic sequenceNumber %1 at line %2 (previous was %3)")
                .arg(seq).arg(lineNum).arg(maxSeq);
            eventFile.close();
            return false;
        }
        seenSeqs.insert(seq);
        maxSeq = seq;
        latestId = eventId;

        if (obj.contains(QStringLiteral("schemaVersion")) && obj.value(QStringLiteral("schemaVersion")).toInt() > 3) {
            if (error) *error = QStringLiteral("Cannot reconstruct manifest: unsupported event schema version at line %1").arg(lineNum);
            eventFile.close();
            return false;
        }

        validCount++;
    }
    eventFile.close();

    // Preserve original manifest fields if existing
    const QString manifestPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
    QJsonObject manifest = readJsonMap(manifestPath);
    if (manifest.isEmpty()) {
        manifest.insert(QStringLiteral("_file"), QStringLiteral("memory-manifest.json"));
        manifest.insert(QStringLiteral("memoryVersion"), QStringLiteral("3"));
        manifest.insert(QStringLiteral("legacyProvenanceCutoffSequence"), 0);
    }
    manifest.insert(QStringLiteral("nextSequenceNumber"), maxSeq + 1);
    manifest.insert(QStringLiteral("eventCount"), validCount);
    if (!latestId.isEmpty()) {
        manifest.insert(QStringLiteral("latestEventId"), latestId);
    }

    QDir(projectRoot).mkpath(QStringLiteral("ARAMF_WORKER/memory"));
    QSaveFile saveFile(manifestPath);
    if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open memory-manifest.json for write: %1").arg(saveFile.errorString());
        return false;
    }
    saveFile.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        if (error) *error = QStringLiteral("Cannot commit memory-manifest.json: %1").arg(saveFile.errorString());
        return false;
    }
    return true;
}

// ─── Physical State Recovery ─────────────────────────────────────────────────

bool MemoryEvidenceFoundation::recoverPhysicalState(const QString& projectRoot, QString* error)
{
    // Step 1: Reconstruct manifest from ledger (validates complete ledger first)
    if (!reconstructManifestFromLedger(projectRoot, error)) {
        return false;
    }

    // Step 2: Refresh metrics atomically
    const auto events = MemoryEvidenceFoundation::evidenceRecords(projectRoot);
    if (events.isEmpty()) {
        if (error && error->isEmpty()) *error = QStringLiteral("Cannot recover physical state: evidence ledger could not be read");
        return false;
    }
    qint64 maxSeq = 0;
    for (const auto& ev : events) {
        const qint64 seq = ev.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        if (seq > maxSeq) maxSeq = seq;
    }

    const QString metricsPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/metrics.json"));
    QJsonObject metrics = readJsonMap(metricsPath);
    metrics.insert(QStringLiteral("_file"), QStringLiteral("metrics.json"));
    if (!metrics.contains(QStringLiteral("totalEventsCreated"))) {
        metrics.insert(QStringLiteral("totalEventsCreated"), events.size());
    }
    metrics.insert(QStringLiteral("activeEvents"), events.size());

    QSaveFile metricsSave(metricsPath);
    if (!metricsSave.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open metrics.json for recovery: %1").arg(metricsSave.errorString());
        return false;
    }
    const QByteArray metricsData = QJsonDocument(metrics).toJson(QJsonDocument::Indented);
    if (metricsSave.write(metricsData) != metricsData.size()) {
        if (error) *error = QStringLiteral("Cannot write metrics.json during recovery: %1").arg(metricsSave.errorString());
        return false;
    }
    if (!metricsSave.commit()) {
        if (error) *error = QStringLiteral("Cannot commit metrics.json during recovery: %1").arg(metricsSave.errorString());
        return false;
    }

    const auto postMetricsReport = MemoryEvidenceFoundation::validate(projectRoot);
    if (!postMetricsReport.certificatesIntact) {
        if (error) *error = QStringLiteral("Cannot recover physical state: certification evidence is invalid");
        return false;
    }

    // Step 3: Refresh derived current-state and cold-start validation
    ProjectMemory memory;
    if (!memory.refreshDerivedState(projectRoot, error)) {
        return false;
    }

    return true;
}

// ─── Query API ───────────────────────────────────────────────────────────────

QList<QJsonObject> MemoryEvidenceFoundation::evidenceRecords(const QString& projectRoot, QString* error)
{
    QList<QJsonObject> records;
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    QFile f(eventLogPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) *error = QStringLiteral("Cannot open event-log.jsonl: %1").arg(f.errorString());
        return records;
    }
    int line = 0;
    while (!f.atEnd()) {
        line++;
        const auto raw = f.readLine().trimmed();
        if (raw.isEmpty()) continue;
        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(raw, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) *error = QStringLiteral("Malformed event at line %1: %2").arg(line).arg(parseErr.errorString());
            records.clear();
            return records;
        }
        records.append(doc.object());
    }
    return records;
}

bool MemoryEvidenceFoundation::evidenceRecordById(const QString& projectRoot,
                                                 const QString& eventId,
                                                 QJsonObject* result,
                                                 QString* error)
{
    const auto records = evidenceRecords(projectRoot, error);
    for (const auto& r : records) {
        if (r.value(QStringLiteral("eventId")).toString() == eventId) {
            if (result) *result = r;
            return true;
        }
    }
    if (error) *error = QStringLiteral("Event ID '%1' not found in ledger").arg(eventId);
    return false;
}

QList<QJsonObject> MemoryEvidenceFoundation::evidenceRecordsBySequenceRange(const QString& projectRoot,
                                                                           qint64 startSeq,
                                                                           qint64 endSeq,
                                                                           QString* error)
{
    QList<QJsonObject> filtered;
    const auto records = evidenceRecords(projectRoot, error);
    for (const auto& r : records) {
        const qint64 seq = r.value(QStringLiteral("sequenceNumber")).toVariant().toLongLong();
        if (seq >= startSeq && seq <= endSeq) {
            filtered.append(r);
        }
    }
    return filtered;
}

QString MemoryEvidenceFoundation::evidenceFingerprint(const QString& projectRoot, QString* error)
{
    const QString eventLogPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/event-log.jsonl"));
    if (!QFile::exists(eventLogPath)) {
        if (error) *error = QStringLiteral("event-log.jsonl does not exist");
        return QString();
    }
    return computeFileSha256(eventLogPath);
}

QJsonObject MemoryEvidenceFoundation::sourceBindingMetadata(const QString& projectRoot, QString* error)
{
    QJsonObject meta;
    const QString projectJsonPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/project.json"));
    const auto pJson = readJsonMap(projectJsonPath);
    meta.insert(QStringLiteral("projectId"), pJson.value(QStringLiteral("projectId")).toString());
    meta.insert(QStringLiteral("projectName"), pJson.value(QStringLiteral("projectName")).toString());
    meta.insert(QStringLiteral("workerIdentity"), pJson.value(QStringLiteral("workerIdentity")).toString());

    const QString manifestPath = QDir(projectRoot).filePath(
        QStringLiteral("ARAMF_WORKER/memory/memory-manifest.json"));
    const auto mJson = readJsonMap(manifestPath);
    meta.insert(QStringLiteral("memoryVersion"), mJson.value(QStringLiteral("memoryVersion")));
    meta.insert(QStringLiteral("nextSequenceNumber"), mJson.value(QStringLiteral("nextSequenceNumber")));
    meta.insert(QStringLiteral("eventCount"), mJson.value(QStringLiteral("eventCount")));
    meta.insert(QStringLiteral("latestEventId"), mJson.value(QStringLiteral("latestEventId")));
    meta.insert(QStringLiteral("evidenceFingerprint"), evidenceFingerprint(projectRoot, error));
    return meta;
}

QJsonObject MemoryEvidenceFoundation::reconstructionStatus(const QString& projectRoot, QString* error)
{
    QJsonObject status;
    ProjectMemory memory;
    const auto csReport = memory.validateColdStart(projectRoot, error);
    status.insert(QStringLiteral("coldStartFresh"), csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS"));
    status.insert(QStringLiteral("coldStartFingerprint"), csReport.value(QStringLiteral("fingerprint")).toString());

    const auto f1Report = validate(projectRoot);
    status.insert(QStringLiteral("manifestConsistent"), f1Report.manifestConsistent);
    status.insert(QStringLiteral("recoveryRequired"), f1Report.recoveryRequired);
    status.insert(QStringLiteral("recoveryReason"), f1Report.recoveryReason);
    return status;
}

QJsonObject MemoryEvidenceFoundation::physicalCorruptionStatus(const QString& projectRoot, QString* error)
{
    const auto f1Report = validate(projectRoot);
    QJsonObject status;
    status.insert(QStringLiteral("ledgerIntact"), f1Report.ledgerIntact);
    status.insert(QStringLiteral("sequenceMonotonic"), f1Report.sequenceMonotonic);
    status.insert(QStringLiteral("manifestConsistent"), f1Report.manifestConsistent);
    status.insert(QStringLiteral("certificatesIntact"), f1Report.certificatesIntact);
    status.insert(QStringLiteral("checkpointsIntact"), f1Report.checkpointsIntact);
    status.insert(QStringLiteral("metricsConsistent"), f1Report.metricsConsistent);
    status.insert(QStringLiteral("schemaCompatible"), f1Report.schemaCompatible);
    status.insert(QStringLiteral("recoveryRequired"), f1Report.recoveryRequired);
    status.insert(QStringLiteral("recoveryReason"), f1Report.recoveryReason);
    status.insert(QStringLiteral("physicallyValid"), f1Report.valid);
    return status;
}

// ─── Cold-start & Summary helpers ───────────────────────────────────────────

bool MemoryEvidenceFoundation::canReconstructFromColdStart(const QString& projectRoot, QString* error)
{
    ProjectMemory memory;
    const auto csReport = memory.validateColdStart(projectRoot, error);
    return csReport.value(QStringLiteral("status")).toString() == QStringLiteral("PASS");
}

QJsonObject MemoryEvidenceFoundation::evidenceSummary(const QString& projectRoot, QString* error)
{
    QJsonObject summary;
    ProjectMemory memory;

    const auto events = memory.events(projectRoot, error);
    summary.insert(QStringLiteral("totalEvents"), events.size());

    const auto decisions = memory.currentDecisions(projectRoot, error);
    summary.insert(QStringLiteral("currentDecisions"), decisions.size());

    const auto checkpoints = memory.checkpoints(projectRoot, error);
    summary.insert(QStringLiteral("checkpoints"), checkpoints.size());

    CertificationService certService;
    QString certError;
    const auto certs = certService.certificates(projectRoot, &certError);
    summary.insert(QStringLiteral("certificates"), certs.size());

    summary.insert(QStringLiteral("memoryUsageBytes"), memory.memoryUsageBytes(projectRoot));
    summary.insert(QStringLiteral("evidenceFingerprint"), evidenceFingerprint(projectRoot));
    summary.insert(QStringLiteral("foundation"), QStringLiteral("F1"));
    summary.insert(QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation"));

    return summary;
}

QJsonObject MemoryEvidenceFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F1")},
        {QStringLiteral("name"), QStringLiteral("Memory & Evidence Foundation")},
        {QStringLiteral("responsibility"), QStringLiteral("Stores and reconstructs evidence")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("append-only-ledger"),
            QStringLiteral("cold-start-reconstruction"),
            QStringLiteral("certification-evidence"),
            QStringLiteral("memory-consistency"),
            QStringLiteral("compaction-governance"),
            QStringLiteral("ledger-manifest-recovery")
        }},
        {QStringLiteral("bootstrapOrder"), 1},
        {QStringLiteral("dependsOn"), QJsonArray{}},
        {QStringLiteral("requiredBy"), QJsonArray{
            QStringLiteral("F2"), QStringLiteral("F3"), QStringLiteral("F4")
        }}
    };
}
