#include "structure/S3/DependencyInterfaces.h"
#include <QCoreApplication>
#include <iostream>

namespace {
bool check(bool value, const char* name) { if (!value) std::cerr << "FAIL [" << name << "]\n"; else std::cout << "PASS [" << name << "]\n"; return value; }
S1::Configuration ownership() {
    S1::Configuration c; c.rootId="project";
    c.responsibilities.append({"project","project","Project",S1::ResponsibilityKind::Project,{},{},{},{},{}});
    c.responsibilities.append({"a","a","A",S1::ResponsibilityKind::Feature,{},"project",{},{},{}});
    c.responsibilities.append({"b","b","B",S1::ResponsibilityKind::Feature,{},"project",{},{},{}});
    c.artifacts.append({"a-source","src/a.cpp","source","a",S1::OwnershipMode::Exclusive,{},{},false,{},{},{},{},{}});
    return c;
}
S2::Configuration physical() { S2::Configuration c; c.rootId="root"; c.nodes.append({"root","root","Project",{},{},"project",{},"root",{}}); c.boundaries.append({"a-boundary","a","src/a","EXCLUSIVE",{},{},{},{},false,{}}); return c; }
S3::Configuration valid() { S3::Configuration c; c.interfaces.append({"api-a","api-a","A API","b",S3::InterfaceVisibility::Public,S3::InterfaceKind::Api,{},{},"",{}}); c.dependencies.append({"dep-a","dep-a","a","b","api-a",S3::DependencyKind::UsesApi,"FORWARD","DECLARED","a uses B API","a-source",{},"","",{}}); return c; }
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv); bool all=true; const auto s1=ownership(); const auto s2=physical(); auto c=valid(); QString error;
    auto result=S3::audit(c,s1,s2); all&=check(result.valid,"valid directional dependency audits"); all&=check(S3::toJson(c)==S3::toJson(c),"deterministic S3 serialization");
    S3::Configuration roundTrip; all&=check(S3::fromJson(S3::toJson(c),&roundTrip,&error)&&S3::toJson(roundTrip)==S3::toJson(c),"S3 persistence round trip"); all&=check(c.lifecycle.identifier()=="S3.1.0.0.0","default S3 lifecycle");
    ProcessVersionState state=ProcessVersionState::empty(); state.hasNextStructure=true; state.nextStructure=c.lifecycle; all&=check(ProcessVersionLifecycle::startNextStructure(&state,&error)&&state.activeStructure.identifier()=="S3.1.1.0.0","S3 lifecycle start"); all&=check(ProcessVersionLifecycle::certifyCurrentStructureIteration(&state,&error)&&state.activeStructure.identifier()=="S3.1.1.1.0","S3 lifecycle certification"); all&=check(ProcessVersionLifecycle::completeActiveStructure(&state,&error)&&state.structureHistory.last().identifier()=="S3.1.1.1.1","S3 lifecycle completion");
    auto invalid=c; invalid.dependencies[0].targetResponsibilityId="missing"; all&=check(!S3::audit(invalid,s1,s2).valid,"unknown target rejected"); invalid=c; invalid.interfaces[0].visibility=S3::InterfaceVisibility::Private; all&=check(!S3::audit(invalid,s1,s2).valid,"private cross-boundary rejected"); invalid=c; invalid.dependencies.append(c.dependencies[0]); all&=check(!S3::audit(invalid,s1,s2).valid,"duplicate dependency rejected");
    invalid=c; invalid.dependencies[0].interfaceId.clear(); invalid.observations.append({"obs","a","b","api-a","a-source",{},"include","a.cpp",S3::DependencyKind::UsesApi,"HIGH","OBSERVED",{}}); result=S3::audit(invalid,s1,s2); bool undeclared=false; for(const auto& diagnostic:result.diagnostics) undeclared|=diagnostic.ruleCode.contains("UNDECLARED"); all&=check(undeclared,"undeclared observation diagnosed");
    invalid=c; invalid.dependencies.append({"dep-b","dep-b","b","a","api-a",S3::DependencyKind::UsesApi,"FORWARD","DECLARED","cycle",{},{},{},{},{}}); all&=check(!S3::audit(invalid,s1,s2).valid,"dependency cycle detected");
    std::cout << (all ? "S3 tests PASS\n" : "S3 tests FAIL\n"); return all?0:1;
}
