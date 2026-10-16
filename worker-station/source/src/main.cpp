#include "StationWindow.h"
#include <cstdio>

using namespace station;

// Deterministic subprocess fixture. It is reachable only from explicit local self-tests.
static int fakeRuntime(const QStringList& args) {
    if(!args.contains("exec"))return 0;
    QFile input;require(input.open(stdin,QIODevice::ReadOnly),"Fixture stdin unavailable");input.readAll();
    const auto mode=qEnvironmentVariable("ARAMF_STATION_TEST_MODE");
    if(mode=="hang") {QThread::sleep(30);return 1;}
    const auto path=args.value(args.indexOf("--cd")+1);
    if(mode!="missing")save(path+"/index.html",QString("<!doctype html><html lang=\"sv\"><meta charset=\"utf-8\"><title>ARAMF Worker</title><h1>ARAMF Worker</h1><p>Agent 01 är redo</p></html>").toUtf8());
    if(mode=="extra")save(path+"/unexpected.txt","out of scope");
    if(mode=="tamper")save(path+"/ARAMF_WORKER/AGENTS.md","changed instructions");
    const auto answer=args.value(args.indexOf("--output-last-message")+1);save(answer,"Deterministic test fixture. No AI service called.\n");
    if(mode=="failure") {std::puts("{\"type\":\"turn.failed\",\"error\":{\"message\":\"fixture failure\"}}");return 1;}
    if(mode!="noevent")std::puts("{\"type\":\"turn.completed\",\"usage\":{\"input_tokens\":0,\"output_tokens\":0}}");
    std::fflush(stdout);return 0;
}

static int selfTest(const QString& directory) {
    require(!QFileInfo::exists(directory),"Test evidence directory must be new.");QDir().mkpath(directory);
    QJsonArray checks;int failed=0;
    auto check=[&](const QString& name,bool ok){checks.append(QJsonObject{{"name",name},{"status",ok?"PASS":"FAIL"}});if(!ok)++failed;};
    const auto demo=Job::demo();check("Valid job roundtrip",Job::parse(demo.bytes).id==demo.id);
    for(const auto& s:QStringList{"../escape.txt","C:/escape.txt","dir/../../escape","dir\\escape","file:stream","NUL.txt","x/COM1","a./b","a /b","/root",".codex/config.toml","ARAMF_WORKER/AGENTS.md","AGENTS.md","a//b","a?b"})check("Reject path "+s,!safeRelative(s));
    check("Accept relative nested output",safeRelative("resultatsidor/index.html"));
    auto reject=[&](const QString& name,const QJsonObject& obj){bool rejected=false;try{Job::parse(QJsonDocument(obj).toJson());}catch(...){rejected=true;}check(name,rejected);};
    auto invalid=demo.value;invalid["schema"]="unknown";reject("Reject unsupported schema",invalid);
    invalid=demo.value;invalid["command"]="arbitrary program";reject("Reject executable job field",invalid);
    invalid=demo.value;invalid["goal"]="";reject("Reject empty goal",invalid);
    invalid=demo.value;invalid["outputs"]=QJsonArray{};reject("Require declared deliverables",invalid);
    invalid=demo.value;invalid["outputs"]=QJsonArray{QJsonObject{{"path","x"},{"mustContain",QJsonArray{}}},QJsonObject{{"path","X"},{"mustContain",QJsonArray{}}}};reject("Reject Windows case aliases",invalid);
    invalid=demo.value;invalid["outputs"]=QJsonArray{QJsonObject{{"path","a"},{"mustContain",QJsonArray{}}},QJsonObject{{"path","a/b"},{"mustContain",QJsonArray{}}}};reject("Reject file-directory overlaps",invalid);
    const auto arguments=Runtime::runArguments("C:/folder with spaces/work","C:/output path/answer.txt","mxc");
    check("Arguments preserve spaces",arguments.contains("C:/folder with spaces/work"));
    check("No bypass flags",!arguments.join(' ').contains("dangerously")&&arguments.contains("workspace-write"));
    check("Approval escalation disabled",arguments.value(arguments.indexOf("-a")+1)=="never");
    qputenv("ARAMF_STATION_TEST_RUNTIME","1");
    for(const auto& mode:QStringList{"success","missing","failure","noevent","extra","tamper","hang"}) {
        qputenv("ARAMF_STATION_TEST_MODE",mode.toUtf8());Runtime runtime;QEventLoop loop;QString status,location;bool timedOut=false;
        runtime.done=[&](const QString& s,const QString& p){status=s;location=p;loop.quit();};
        QTimer watchdog;watchdog.setSingleShot(true);QObject::connect(&watchdog,&QTimer::timeout,&loop,[&]{timedOut=true;runtime.stop();loop.quit();});
        watchdog.start(15000);runtime.run(QCoreApplication::applicationFilePath(),demo,directory,"unelevated");
        if(mode=="hang")QTimer::singleShot(250,&runtime,[&]{runtime.stop();});
        loop.exec();watchdog.stop();
        const QString expected=mode=="success"?"DELIVERED":mode=="hang"?"CANCELLED":(mode=="missing"||mode=="extra"||mode=="tamper")?"CHECKS_FAILED":"FAILED";
        check("Subprocess "+mode+" -> "+expected,!timedOut&&status==expected);
        if(!location.isEmpty()&&QFileInfo::exists(location+"/report.json")) {
            const auto report=QJsonDocument::fromJson(read(location+"/report.json")).object();
            check("Report remains uncertified ("+mode+")",report["certificationStatus"]=="NOT_CERTIFIED"&&report["verificationStatus"]=="NOT_REVIEWED");
            check("Report binds exact job ("+mode+")",report["jobSha256"]==hash(demo.bytes));
        }else check("Report exists ("+mode+")",false);
    }
    qunsetenv("ARAMF_STATION_TEST_RUNTIME");qunsetenv("ARAMF_STATION_TEST_MODE");
    const QJsonObject result{{"suite","ARAMF Worker Station local regressions"},{"status",failed?"FAIL":"PASS"},{"total",checks.size()},{"failed",failed},{"checks",checks},{"liveChatGPTTest","NOT_RUN"}};
    save(directory+"/tests.json",QJsonDocument(result).toJson());return failed?1:0;
}

int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setApplicationName("ARAMF Worker Station");app.setOrganizationName("ARAMF");
    const auto args=app.arguments();
    try {
        if(qEnvironmentVariable("ARAMF_STATION_TEST_RUNTIME")=="1"&&args.contains("exec"))return fakeRuntime(args);
        if(args.contains("--self-test"))return selfTest(args.value(args.indexOf("--self-test")+1));
        Window w;w.show();
        if(args.contains("--ui-smoke")) {
            const auto directory=args.value(args.indexOf("--ui-smoke")+1);
            QTimer::singleShot(350,&w,[&app,&w,directory]{try{w.smoke(directory);app.exit(0);}catch(const std::exception& e){save(directory+"/ui-error.txt",e.what());app.exit(1);}});
        }
        return app.exec();
    }catch(const std::exception& e){
        if(args.contains("--self-test")){std::fprintf(stderr,"%s\n",e.what());return 2;}
        QMessageBox::critical(nullptr,"ARAMF Worker Station",QString::fromUtf8(e.what()));return 2;
    }
}
