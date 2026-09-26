#include "IdentityTrustFoundation.h"
QStringList IdentityTrustFoundation::actorTaxonomy()
{
    return {
        QStringLiteral("human"),
        QStringLiteral("user"),
        QStringLiteral("agent"),
        QStringLiteral("autonomous-agent"),
        QStringLiteral("tool"),
        QStringLiteral("runtime"),
        QStringLiteral("system")
    };
}

bool IdentityTrustFoundation::isValidActor(const QString& actor)
{
    return actorTaxonomy().contains(actor.toLower().trimmed());
}

bool IdentityTrustFoundation::validateProvenance(const QJsonObject& prov, QString* error)
{
    if (prov.isEmpty()) {
        if (error) *error = QStringLiteral("Missing required provenance.");
        return false;
    }
    const QString actor = prov.value(QStringLiteral("actor")).toString().trimmed();
    if (actor.isEmpty()) {
        if (error) *error = QStringLiteral("Provenance is missing required actor.");
        return false;
    }
    const QString lowerActor = actor.toLower();
    if (!IdentityTrustFoundation::isValidActor(lowerActor)) {
        if (error) *error = QStringLiteral("Provenance has invalid actor '%1'.").arg(actor);
        return false;
    }
    const QString agentId = prov.value(QStringLiteral("agentId")).toString().trimmed();
    if ((lowerActor == QStringLiteral("agent") || lowerActor == QStringLiteral("autonomous-agent") || lowerActor == QStringLiteral("system"))
        && agentId.isEmpty()) {
        if (error) *error = QStringLiteral("Provenance is missing required agentId for actor '%1'.").arg(actor);
        return false;
    }
    const QString tool = prov.value(QStringLiteral("tool")).toString().trimmed();
    if (tool.isEmpty()) {
        if (error) *error = QStringLiteral("Provenance is missing required tool.");
        return false;
    }
    return true;
}

bool IdentityTrustFoundation::isVerifiedAdministrativeOverride(const QString& instruction)
{
    const QString normalized = instruction.simplified();
    return normalized.contains(QStringLiteral("Admin Morgan Lindbom"), Qt::CaseSensitive)
        && normalized.contains(QStringLiteral("override"), Qt::CaseInsensitive);
}

bool IdentityTrustFoundation::containsDestructivePattern(const QString& text)
{
    static const QStringList patterns = {
        QStringLiteral("rmdir /s /q"),
        QStringLiteral("rd /s /q"),
        QStringLiteral("rm -rf"),
        QStringLiteral("Remove-Item -Recurse"),
        QStringLiteral("Remove-Item -r"),
        QStringLiteral("del /s /q"),
        QStringLiteral("git reset --hard"),
        QStringLiteral("git clean")
    };
    for (const auto& p : patterns) {
        if (text.contains(p, Qt::CaseInsensitive)) return true;
    }
    return false;
}

bool IdentityTrustFoundation::respectsTrustBoundary(const QString& instruction,
                                                     const QString& requestedAction,
                                                     QString* error)
{
    // Trust boundary: admin overrides must come from verified admin identity
    if (!isVerifiedAdministrativeOverride(instruction)) {
        if (error) *error = QStringLiteral("F2: Instruction does not pass administrative verification (exact Admin Morgan Lindbom identity and override intent required)");
        return false;
    }

    // Block dangerous shell operations regardless of admin status
    if (containsDestructivePattern(requestedAction) || containsDestructivePattern(instruction)) {
        if (error) *error = QStringLiteral("F2: Recursive deletion blocked by trust boundary");
        return false;
    }

    return true;
}

QJsonObject IdentityTrustFoundation::contract()
{
    return QJsonObject{
        {QStringLiteral("foundation"), QStringLiteral("F2")},
        {QStringLiteral("name"), QStringLiteral("Identity, Provenance & Trust Foundation")},
        {QStringLiteral("responsibility"),
            QStringLiteral("Establishes who/what produced evidence and whether attribution is trustworthy")},
        {QStringLiteral("owns"), QJsonArray{
            QStringLiteral("actor-identity-taxonomy"),
            QStringLiteral("provenance-validation"),
            QStringLiteral("trust-boundary-enforcement"),
            QStringLiteral("admin-override-verification")
        }},
        {QStringLiteral("bootstrapOrder"), 2},
        {QStringLiteral("dependsOn"), QJsonArray{}},
        {QStringLiteral("requiredBy"), QJsonArray{}}
    };
}


F2TrustReport IdentityTrustFoundation::validate(const QJsonArray& inputRecords, int legacyCutoff, QString* error)
{
    F2TrustReport report;
    QList<QJsonObject> events;
    for (const auto& value : inputRecords) {
        if (!value.isObject()) { report.errors.append(QStringLiteral("F2: Invalid attribution record")); return report; }
        events.append(value.toObject());
    }
    report.allProvenanceValid = true;
    for (const auto& ev : events) {
        if (ev.value(QStringLiteral("sequenceNumber")).toInt() <= legacyCutoff
            && ev.value(QStringLiteral("provenance")).toObject().isEmpty()) continue;
        QString failure;
        if (!validateProvenance(ev.value(QStringLiteral("provenance")).toObject(), &failure)) {
            report.allProvenanceValid = false; report.errors.append(failure);
        }
    }
    const auto validActors = actorTaxonomy();
    QSet<QString> seenActors;
    report.actorTaxonomyConsistent = true;

    for (const auto& ev : events) {
        const int seq = ev.value(QStringLiteral("sequenceNumber")).toInt(0);
        const auto prov = ev.value(QStringLiteral("provenance")).toObject();
        const auto actor = prov.value(QStringLiteral("actor")).toString();

        if (seq <= legacyCutoff && actor.isEmpty()) {
            report.legacyExemptEvents++;
            continue;
        }

        if (!actor.isEmpty()) {
            seenActors.insert(actor);
            report.eventsWithProvenance++;
            if (!validActors.contains(actor)) {
                report.actorTaxonomyConsistent = false;
                report.errors.append(QStringLiteral("F2: Unknown actor '%1' in event seq %2")
                    .arg(actor).arg(seq));
            }
        } else {
            report.eventsWithoutProvenance++;
        }
    }
    report.recognizedActors = seenActors.values();
    report.recognizedActors.sort();

    // 3. Trust boundary enforcement - verify admin override events have correct provenance & identity
    report.trustBoundariesEnforced = true;
    report.adminOverrideValid = true;
    for (const auto& ev : events) {
        if (ev.value(QStringLiteral("eventType")).toString() == QStringLiteral("ADMIN_OVERRIDE")) {
            const auto prov = ev.value(QStringLiteral("provenance")).toObject();
            const QString actor = prov.value(QStringLiteral("actor")).toString();
            if (actor != QStringLiteral("human")) {
                report.trustBoundariesEnforced = false;
                report.adminOverrideValid = false;
                report.errors.append(QStringLiteral("F2: ADMIN_OVERRIDE event has non-human actor '%1'").arg(actor));
            }
            const QString instruction = ev.value(QStringLiteral("instruction")).toString();
            if (!instruction.isEmpty() && !isVerifiedAdministrativeOverride(instruction)) {
                report.adminOverrideValid = false;
                report.errors.append(QStringLiteral("F2: ADMIN_OVERRIDE event has invalid instruction identity"));
            }
            const QString action = ev.value(QStringLiteral("requestedAction")).toString();
            if (containsDestructivePattern(action) || containsDestructivePattern(instruction)) {
                report.trustBoundariesEnforced = false;
                report.errors.append(QStringLiteral("F2: Prohibited destructive action in ADMIN_OVERRIDE event"));
            }
        }
    }

    report.valid = report.allProvenanceValid && report.actorTaxonomyConsistent
                && report.trustBoundariesEnforced && report.adminOverrideValid;
    report.fullReport = QJsonObject{{"valid", report.valid}, {"errors", QJsonArray::fromStringList(report.errors)},
        {"eventsWithProvenance", report.eventsWithProvenance}, {"legacyExemptEvents", report.legacyExemptEvents}};
    if (!report.valid && error) *error = report.errors.join("; ");
    return report;
}
