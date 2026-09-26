#include "FoundationProjectValidation.h"
#include "ProjectMemory.h"
#include "ProcessVersion.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
namespace foundation_project_detail {
QJsonObject read(const QString& root, const QString& path) {
 QFile f(QDir(root).filePath(path)); if(!f.open(QIODevice::ReadOnly))return {};
 return QJsonDocument::fromJson(f.readAll()).object();
}
QJsonArray records(const QString& root, QString* error) {
 QJsonArray result; for(const auto& event:ProjectMemory().events(root,error))result.append(event); return result;
}
}
F2TrustReport FoundationProjectValidation::validateF2(const QString& root, QString* error) {
 QString failure; const auto events=foundation_project_detail::records(root,&failure);
 if(!failure.isEmpty()){F2TrustReport r;r.errors.append(failure);if(error)*error=failure;return r;}
 const int cutoff=foundation_project_detail::read(root,"ARAMF_WORKER/memory/memory-manifest.json").value("legacyProvenanceCutoffSequence").toInt();
 auto r=IdentityTrustFoundation::validate(events,cutoff,error);
 // Project-wide persisted provenance checks also cover decisions and checkpoints.
 const auto memory=ProjectMemory().validate(root,&failure,false);
 for(const auto& value:memory.value("checks").toArray()){
  const auto c=value.toObject();if(c.value("name")=="event-provenance-valid"&&c.value("status")!="PASS"){
   r.valid=r.allProvenanceValid=false;r.errors.append("Persisted project provenance failed");
  }
 }
 r.fullReport.insert("projectMemoryValidation",memory);
 return r;
}
bool FoundationProjectValidation::validateProjectIsolation(const QString& root, QString* error) {
 QString failure;const auto records=foundation_project_detail::records(root,&failure);
 if(!failure.isEmpty()){if(error)*error=failure;return false;}
 return ScopeIntegrityFoundation::validateProjectIsolation(root,foundation_project_detail::read(root,"ARAMF_WORKER/project.json"),records,error);
}
F3IntegrityReport FoundationProjectValidation::validateF3(const QString& root,const ProjectModel*,QString* error) {
 QString failure;const auto records=foundation_project_detail::records(root,&failure);
 if(!failure.isEmpty()){F3IntegrityReport r;r.errors.append(failure);if(error)*error=failure;return r;}
 const auto project=foundation_project_detail::read(root,"ARAMF_WORKER/project.json");
 QJsonArray scopeRecords;
 for (const auto& value : records) {
  auto event = value.toObject();
  const auto type = event.value("eventType").toString().trimmed().toUpper();
  // The persisted administrative schema uses scope as a human rule description,
  // not a routing partition (the same distinction used by ProjectMemory).
  // Adapt a copy only; structured scopes and affected-file isolation still apply.
  if (type == "ADMIN_OVERRIDE" || type == "ADMIN_OVERRIDE_VALIDATION") event.remove("scope");
  scopeRecords.append(event);
 }
 auto r=ScopeIntegrityFoundation::validate(root,project,scopeRecords,ScopeIntegrityFoundation::effectiveCanonicalScopeRegistry(root),error);
 const auto memory=ProjectMemory().validate(root,&failure,false);
 for(const auto& value:memory.value("checks").toArray()){
  const auto c=value.toObject();if(c.value("name")=="persisted-scope-validity"&&c.value("status")!="PASS"){
   r.valid=r.scopeTaxonomyValid=false;r.errors.append("Persisted project scope validation failed");
  }
 }
 // Lifecycle semantics are evaluated here, outside F3, by their F4 owner.
 if(project.contains("processVersion")) {
  ProcessVersionState state;
  if(!processVersionStateFromJson(project.value("processVersion"),&state,&failure)||!state.isValid(&failure)){
   r.valid=r.projectStateIntegral=false;r.errors.append(failure);
  }
 }
 r.fullReport.insert("projectMemoryValidation",memory);
 return r;
}
