#include "DocumentInstruction.h"
#include <QJsonArray>

namespace {
DocumentInstruction make(const QString& id, const QString& type, const QString& purpose, const QStringList& rules)
{
    return {id, type, 1, purpose, rules};
}
}

namespace DocumentInstructions {
DocumentInstruction thesis()
{
    return make(QStringLiteral("aramf-thesis-instruction"), QStringLiteral("thesis"),
        QStringLiteral("Generic academic guidance for writing and governing a thesis without assuming a discipline, technology, institution, or course."), {
        QStringLiteral("Background explains context, not conclusions."),
        QStringLiteral("Problem formulation, purpose, research questions where applicable, objectives, scope, and limitations must be coherent."),
        QStringLiteral("Theoretical foundation, related work, terminology, and concepts must support the stated problem where relevant."),
        QStringLiteral("Method selection and justification must explain how the work can answer its questions and handle data, material, and sources."),
        QStringLiteral("Testing, validation, reliability, validity, ethics, and implementation or investigation evidence must be addressed where relevant."),
        QStringLiteral("Results primarily state findings or outputs; Discussion interprets them and evaluates method, limitations, and evidence."),
        QStringLiteral("Conclusions must be supported by Results and Discussion, and research questions must be answered where applicable."),
        QStringLiteral("Future work, references, and traceability between claims and evidence must be explicit where relevant."),
        QStringLiteral("Unsupported claims must not be presented as facts; references must support externally sourced claims."),
        QStringLiteral("Guidance is authoring assistance, not final prose.")});
}

DocumentInstruction report()
{
    return make(QStringLiteral("aramf-report-instruction"), QStringLiteral("report"),
        QStringLiteral("General-purpose guidance for writing and governing a report without imposing thesis-only academic requirements."), {
        QStringLiteral("Background explains context; Purpose states why the report exists; Objectives state what should be achieved."),
        QStringLiteral("Scope, requirements, assumptions, approach or method, and work process should be stated where relevant."),
        QStringLiteral("Execution explains what was actually done, including implementation, structure, tools, and important decisions."),
        QStringLiteral("Testing or verification should explain how work was checked against appropriate criteria."),
        QStringLiteral("Results present outcomes objectively; Discussion interprets and evaluates them."),
        QStringLiteral("Deviations and limitations must be visible rather than hidden."),
        QStringLiteral("Conclusions must follow from reported results; recommendations and future work must be evidence-based where included."),
        QStringLiteral("References must support externally sourced claims and traceability should be maintained where relevant."),
        QStringLiteral("Research questions, related work, theoretical framework, and formal reliability or validity discussion are not required by this instruction alone."),
        QStringLiteral("Guidance is authoring assistance, not final prose.")});
}

const DocumentInstruction& forType(const QString& documentType)
{
    static const DocumentInstruction thesisInstruction = thesis();
    static const DocumentInstruction reportInstruction = report();
    return documentType.compare(QStringLiteral("report"), Qt::CaseInsensitive) == 0 ? reportInstruction : thesisInstruction;
}

QStringList validate(const DocumentInstruction& instruction)
{
    QStringList errors;
    if (instruction.id.isEmpty() || instruction.documentType.isEmpty()) errors << QStringLiteral("Document instruction identity is missing");
    if (instruction.version < 1) errors << QStringLiteral("Document instruction version is invalid");
    if (instruction.rules.isEmpty()) errors << QStringLiteral("Document instruction rules are missing");
    return errors;
}

QJsonObject toJson(const DocumentInstruction& instruction)
{
    QJsonArray rules;
    for (const auto& rule : instruction.rules) rules.append(rule);
    return {{QStringLiteral("id"), instruction.id}, {QStringLiteral("documentType"), instruction.documentType},
            {QStringLiteral("version"), instruction.version}, {QStringLiteral("purpose"), instruction.purpose},
            {QStringLiteral("rules"), rules}};
}
}
