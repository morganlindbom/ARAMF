// Compiled four times, each executable links one Foundation and Qt Core only.
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QTextStream>
#if FOUNDATION_NUMBER == 1
#include "foundations/F1/MemoryEvidenceFoundation.h"
#include "foundations/F1/EvidenceStorage.h"
#elif FOUNDATION_NUMBER == 2
#include "foundations/F2/IdentityTrustFoundation.h"
#elif FOUNDATION_NUMBER == 3
#include "foundations/F3/ScopeIntegrityFoundation.h"
#elif FOUNDATION_NUMBER == 4
#include "foundations/F4/LifecycleCertificationFoundation.h"
#include "foundations/F4/ProcessVersion.h"
#endif
static bool check(bool pass,const char* label){QTextStream(stdout)<<(pass?"PASS ":"FAIL ")<<label<<Qt::endl;return pass;}
static bool bytes(const QString& path,const QByteArray& value){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(value)==value.size();}
static bool json(const QString& path,const QJsonObject& value){return bytes(path,QJsonDocument(value).toJson());}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);bool ok=true;QTemporaryDir fixture;fixture.setAutoRemove(false);
 const QString root=fixture.path();const QString w=root+"/ARAMF_WORKER/";QString error;
 const QString source=app.arguments().value(1,QDir::currentPath());
 const QString directory=source+"/src/foundations/F"+QString::number(FOUNDATION_NUMBER);
 const QList<QStringList> classes={{"MemoryEvidenceFoundation","EvidenceStorage"},{"IdentityTrustFoundation"},{"ScopeIntegrityFoundation"},{"LifecycleCertificationFoundation","ProcessVersionLifecycle","ProcessVersionState"}};
 QDirIterator sources(directory,{"*.cpp","*.h"},QDir::Files,QDirIterator::Subdirectories);
 ok &= check(sources.hasNext(),"dedicated Foundation source directory exists and is nonempty");
 while(sources.hasNext()){
  QFile f(sources.next());ok &= check(f.open(QIODevice::ReadOnly),"source readable");QString code=QString::fromUtf8(f.readAll());
  code.remove(QRegularExpression("//[^\\n]*|/\\*.*?\\*/",QRegularExpression::DotMatchesEverythingOption));
  for(int i=0;i<4;++i)if(i+1!=FOUNDATION_NUMBER)for(const auto& type:classes[i])
   ok &= check(!code.contains(QRegularExpression("\\b"+type+"\\b")),"no peer include, type, construction, or service call");
  ok &= check(!code.contains(QRegularExpression("#include\\s+[<\"](?:[^\"\\n]*[/])?(?:ProjectMemory|FoundationServices|FoundationProjectValidation|CertificationService)\\.h")),"no orchestration include");
  ok &= check(!code.contains(QRegularExpression("(?:^|[;{}])\\s*(?:ProjectMemory|CertificationService|FoundationProjectValidation)\\s+\\w+",QRegularExpression::MultilineOption)),"no hidden concrete coordinator");
  for(int i=1;i<=4;++i)if(i!=FOUNDATION_NUMBER)ok &= check(!code.contains("/F"+QString::number(i)+"/"),"no peer source import");
 }
#if FOUNDATION_NUMBER == 1
 const QJsonObject event{{"eventId","independent-event"},{"sequenceNumber",1},{"schemaVersion",1}};
 ok &= bytes(w+"memory/event-log.jsonl",QJsonDocument(event).toJson(QJsonDocument::Compact)+"\n");
 for(const auto& name:QStringList{"framework-knowledge.json","event-id-integrity-exceptions.json","project-knowledge.json","compaction-manifest.json"})ok &= json(w+"memory/"+name,{{"version",1}});
 ok &= bytes(w+"memory/decisions.md","# Independent F1 decisions\n");
 ok &= bytes(w+"memory/current-state.md","# Independent state\n");
 ok &= bytes(w+"AGENTS.md","memory/memory-contract.json append-only narrow mutation\n");
 ok &= bytes(w+"PROJECT_STATUS.md","# Independent F1\n");
 ok &= json(w+"memory/memory-config.json",{{"writerMode","agent-direct"},{"maintenanceOptions",QJsonArray{}}});
 ok &= json(w+"memory/memory-contract.json",{{"supportedOperations",QJsonArray{"task-start"}}});
 ok &= json(w+"memory/checkpoints.json",{{"checkpoints",QJsonArray{}}});
 ok &= json(w+"memory/metrics.json",{{"activeEvents",1},{"totalEventsCreated",1}});
 ok &= check(MemoryEvidenceFoundation::reconstructManifestFromLedger(root,&error),"F1 reconstructs without peers");
 ok &= check(MemoryEvidenceFoundation::validate(root,&error).valid,qPrintable(error));
 ok &= check(MemoryEvidenceFoundation::evidenceRecords(root).size()==1,"F1 reload");
 ok &= check(EvidenceStorage().refreshDerivedState(root,&error),"F1 independent cold start");
 ok &= check(EvidenceStorage().appendEvidence(root,{{"eventId","second-independent-event"},{"eventType","STORAGE_TEST"},{"task","independent physical persistence"}},&error),"F1 writes without F2/F3/F4 or a coordinator");
 const auto stored=EvidenceStorage().events(root,&error);
 ok &= check(stored.size()==2&&stored.last().value("sequenceNumber").toInt()==2,"F1 independently persists sequence and reloads appended evidence");
 ok &= check(EvidenceStorage().validateColdStart(root,&error).value("status")=="PASS","F1 reconstructs after independent append");
 ok &= check(MemoryEvidenceFoundation::validate(root,&error).valid,"F1 physical invariants survive independent append");
#elif FOUNDATION_NUMBER == 2
 const QJsonObject provenance{{"actor","agent"},{"agentId","independent"},{"tool","test"}};
 const QJsonObject record{{"sequenceNumber",1},{"provenance",provenance}};
 ok &= check(IdentityTrustFoundation::validate(QJsonArray{record}).valid,"F2 standalone provenance");
 ok &= check(!IdentityTrustFoundation::validate(QJsonArray{QJsonObject{{"sequenceNumber",1}}}).valid,"F2 rejects missing provenance");
 ok &= check(!IdentityTrustFoundation::validateProvenance({{"actor","agent"},{"tool","test"}}),"F2 rejects missing agent identity");
 ok &= check(!IdentityTrustFoundation::respectsTrustBoundary("Admin Morgan Lindbom override","rm -rf example"),"F2 trust boundary");
 ok &= json(root+"/identity.json",record);QFile f(root+"/identity.json");ok &= f.open(QIODevice::ReadOnly);
 ok &= check(IdentityTrustFoundation::validate(QJsonArray{QJsonDocument::fromJson(f.readAll()).object()}).valid,"F2 persisted reload");
 ok &= check(IdentityTrustFoundation::contract().value("dependsOn").toArray().isEmpty(),"F2 independent contract");
#elif FOUNDATION_NUMBER == 3
 const QJsonObject project{{"projectId","independent"}};
 const QJsonArray records{QJsonObject{{"scope","tests"},{"affectedFiles",QJsonArray{"tests/example.cpp"}}}};
 ok &= check(ScopeIntegrityFoundation::validate(root,project,records,ScopeIntegrityFoundation::baseReservedScopes()).valid,"F3 standalone validation");
 ok &= check(!ScopeIntegrityFoundation::validateScopeSet({"project","global"}),"F3 rejects contradictory scopes");
 ok &= check(!ScopeIntegrityFoundation::validateProjectIsolation(root,project,QJsonArray{QJsonObject{{"affectedFiles",QJsonArray{root+"-foreign/file"}}}}),"F3 rejects prefix sibling escape");
 ok &= json(root+"/scope.json",project);QFile f(root+"/scope.json");ok &= f.open(QIODevice::ReadOnly);
 ok &= check(ScopeIntegrityFoundation::validate(root,QJsonDocument::fromJson(f.readAll()).object(),records,ScopeIntegrityFoundation::baseReservedScopes()).valid,"F3 persisted reload");
 ok &= check(ScopeIntegrityFoundation::contract().value("dependsOn").toArray().isEmpty(),"F3 independent contract");
#elif FOUNDATION_NUMBER == 4
 auto state=ProcessVersionState::currentCanonicalState();
 ok &= check(!state.isP6Eligible(),"F4 independently enforces governance prerequisites");
 ok &= check(ProcessVersionLifecycle::startNextProcess(&state,&error),"F4 start");
 ok &= check(!ProcessVersionLifecycle::completeActiveProcess(&state,&error),"F4 rejects completion without certification");
 ok &= check(ProcessVersionLifecycle::certifyCurrentIteration(&state,&error),"F4 certify transition");
 ok &= check(ProcessVersionLifecycle::completeActiveProcess(&state,&error),"F4 complete transition");
 ok &= json(w+"project.json",{{"processVersion",processVersionStateToJson(state)}});
 ok &= check(LifecycleCertificationFoundation::validate(root,&error).valid,"F4 persisted reload");
 ok &= check(!LifecycleCertificationFoundation::validateCertificationSemantics({{"certification",0},{"done",1},{"iteration",1}}),"F4 rejects invalid certification");
 ok &= check(LifecycleCertificationFoundation::contract().value("dependsOn").toArray().isEmpty(),"F4 independent contract");
#endif
 QTextStream(stdout)<<"Fixture preserved: "<<root<<Qt::endl;
 return ok?0:1;
}
