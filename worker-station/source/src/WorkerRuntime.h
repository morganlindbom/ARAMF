#pragma once
#include "WorkerJob.h"
#include <functional>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace station {
class Runtime : public QObject {
    QProcess process;
    QTimer deadline;
    QByteArray pending;
    QFile eventFile;
    Job job;
    QString runDirectory, workspace, startedAt, operation;
    bool stopped=false, completed=false, failedEvent=false, overflow=false, finalized=false;
    qint64 outputBytes=0;
    QMap<QString,QString> baseline;
    QJsonObject auditWorkspace() {
        QJsonArray changed, violations;
        QSet<QString> outputs;
        for(const auto v:job.value["outputs"].toArray())outputs.insert(v.toObject()["path"].toString());
        QSet<QString> seen;int count=0;
        QDirIterator it(workspace,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden,QDirIterator::Subdirectories);
        while(it.hasNext()) {
            it.next();const auto info=it.fileInfo();const auto relative=QDir(workspace).relativeFilePath(info.absoluteFilePath());
            require(++count<=1000,"För många filer i arbetsmappen för pilotens granskning.");
            noLinks(info.absoluteFilePath());if(info.isDir())continue;
            const auto digest=hash(read(info.absoluteFilePath(),4*1024*1024));seen.insert(relative);
            if(baseline.value(relative)!=digest){changed.append(QJsonObject{{"path",relative},{"sha256",digest}});if(!outputs.contains(relative))violations.append(relative);}
        }
        for(auto i=baseline.begin();i!=baseline.end();++i)if(!seen.contains(i.key())){changed.append(QJsonObject{{"path",i.key()},{"deleted",true}});violations.append(i.key());}
        return {{"status",violations.isEmpty()?"PASS":"FAIL"},{"changes",changed},{"unexpectedChanges",violations},
            {"meaning","Post-run workspace diff check; not a per-file operating-system permission boundary."}};
    }
#ifdef Q_OS_WIN
    HANDLE processGroup=nullptr;
#endif
    void releaseGroup() {
#ifdef Q_OS_WIN
        if(processGroup){CloseHandle(processGroup);processGroup=nullptr;}
#endif
    }
    bool containProcess() {
#ifdef Q_OS_WIN
        processGroup=CreateJobObjectW(nullptr,nullptr);
        if(!processGroup)return false;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        HANDLE p=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(process.processId()));
        const bool result=p && SetInformationJobObject(processGroup,JobObjectExtendedLimitInformation,&info,sizeof(info))
            && AssignProcessToJobObject(processGroup,p);
        if(p)CloseHandle(p);
        return result;
#else
        return true;
#endif
    }
    void consume(bool errorChannel) {
        const auto data=errorChannel?process.readAllStandardError():process.readAllStandardOutput();
        outputBytes+=data.size();
        if(outputBytes>16*1024*1024){overflow=true;stop();return;}
        if(operation!="run") {if(log)log(QString::fromUtf8(data));return;}
        if(eventFile.isOpen()) {
            const QJsonObject line{{"at",utc()},{"channel",errorChannel?"stderr":"stdout"},{"text",QString::fromUtf8(data)}};
            const auto encoded=QJsonDocument(line).toJson(QJsonDocument::Compact)+'\n';
            if(eventFile.write(encoded)!=encoded.size()||!eventFile.flush()){overflow=true;stop();return;}
        }
        if(errorChannel){if(log)log(QString::fromUtf8(data));return;}
        pending+=data;
        if(pending.size()>1024*1024){overflow=true;stop();return;}
        while(pending.contains('\n')){
            const int n=pending.indexOf('\n');const auto line=pending.left(n);pending.remove(0,n+1);
            const auto d=QJsonDocument::fromJson(line).object();const auto type=d["type"].toString();
            if(type=="turn.completed")completed=true;
            if(type=="turn.failed"||type=="error")failedEvent=true;
            const auto item=d["item"].toObject();const auto kind=item["type"].toString();
            QString text=item["text"].toString();
            if(kind=="command_execution")text=item["command"].toString()+"\n"+item["aggregated_output"].toString();
            if(type=="error")text=d["message"].toString();
            if(type=="turn.failed")text=QString::fromUtf8(QJsonDocument(d).toJson(QJsonDocument::Compact));
            if(!text.isEmpty() && log)log(text.left(10000));
        }
    }
    void finish(int exitCode,QProcess::ExitStatus exitStatus) {
        if(finalized)return;
        finalized=true;
        deadline.stop();consume(false);consume(true);releaseGroup();eventFile.close();
        if(operation!="run"){
            if(done)done(exitCode==0&&exitStatus==QProcess::NormalExit&&!stopped?"OK":"FAILED",QString());
            return;
        }
        QString status=stopped?"CANCELLED":(exitCode==0&&exitStatus==QProcess::NormalExit&&completed&&!failedEvent&&!overflow?"DELIVERED":"FAILED");
        try {
            const auto checks=inspectOutputs(job,workspace);
            const auto scope=auditWorkspace();
            if(status=="DELIVERED"&&(checks["status"]!="PASS"||scope["status"]!="PASS"))status="CHECKS_FAILED";
            if(overflow)status="FAILED";
            const auto report=QJsonObject{{"schema","aramf-worker-result/v1"},{"jobId",job.id},{"jobSha256",hash(job.bytes)},
                {"member",job.member},{"team",job.team},{"manager",job.manager},{"startedAt",startedAt},{"finishedAt",utc()},
                {"executionStatus",status},{"exitCode",exitCode},{"completionEventObserved",completed},{"checks",checks},{"scopeCheck",scope},
                {"verificationStatus","NOT_REVIEWED"},{"certificationStatus","NOT_CERTIFIED"},{"workspace",workspace},
                {"runtime",process.program()},{"arguments",QJsonArray::fromStringList(process.arguments())},
                {"logSha256",hash(read(runDirectory+"/events.jsonl",20*1024*1024))}};
            save(runDirectory+"/report.json",QJsonDocument(report).toJson());
            save(runDirectory+"/run.json",QJsonDocument(QJsonObject{{"status",status},{"startedAt",startedAt},{"finishedAt",utc()},{"report","report.json"}}).toJson());
            QString summary="ARAMF Worker Station — leverans\n\nUppdrag: "+job.id+"\nAgent: "+job.member+"\nTeam: "+job.team+
                "\nKörresultat: "+status+"\nFilkontroller: "+checks["status"].toString()+
                "\nOberoende granskning: EJ UTFÖRD\nCertifiering: EJ CERTIFIERAD\n\nLeveransfiler: workspace\\\nDetaljer: report.json\nAgentens svar: answer.txt\n";
            save(runDirectory+"/LAS-MIG.txt",summary.toUtf8());
        }catch(const std::exception& e){status="REPORT_FAILED";if(log)log(QString::fromUtf8(e.what()));}
        if(done)done(status,runDirectory);
    }
public:
    std::function<void(const QString&)> log;
    std::function<void(const QString&,const QString&)> done;
    explicit Runtime(QObject* parent=nullptr):QObject(parent) {
        connect(&process,&QProcess::readyReadStandardOutput,this,[this]{consume(false);});
        connect(&process,&QProcess::readyReadStandardError,this,[this]{consume(true);});
        connect(&process,&QProcess::started,this,[this]{
            if(!containProcess()){if(log)log("Kunde inte skapa processgruppen. Körningen stoppas.");overflow=true;stop();}
        });
        connect(&process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int c,QProcess::ExitStatus s){finish(c,s);});
        connect(&process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){
            if(log)log(process.errorString());
            if(e==QProcess::FailedToStart)finish(-1,QProcess::CrashExit);
        });
        deadline.setSingleShot(true);connect(&deadline,&QTimer::timeout,this,[this]{if(log)log("Tidsgränsen nådd. Körningen stoppas.");stop();});
    }
    ~Runtime() override {releaseGroup();if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(3000);}}
    bool busy()const{return process.state()!=QProcess::NotRunning;}
    QString lastRun()const{return runDirectory;}
    static QString accountHome() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/ARAMFWorkerStation/Codex";
    }
    static QString findCodex() {
        const QString packaged=QCoreApplication::applicationDirPath()+"/codex/bin/codex.exe";
        if(QFileInfo::exists(packaged))return packaged;
        const QString npm=qEnvironmentVariable("APPDATA")+"/npm/node_modules/@openai/codex/node_modules/@openai/codex-win32-x64/vendor/x86_64-pc-windows-msvc/bin/codex.exe";
        if(QFileInfo::exists(npm))return npm;
        return QStandardPaths::findExecutable("codex.exe");
    }
    static QStringList runArguments(const QString& directory,const QString& answer,const QString& sandbox) {
        require(sandbox=="mxc"||sandbox=="unelevated","Okänd sandbox.");
        return {"-a","never","-c","windows.sandbox=\""+sandbox+"\"","-c","web_search=\"disabled\"",
            "-c","sandbox_workspace_write.network_access=false","-c","sandbox_workspace_write.exclude_tmpdir_env_var=true",
            "-c","sandbox_workspace_write.exclude_slash_tmp=true","exec","--ignore-user-config","--ignore-rules",
            "--sandbox","workspace-write","--skip-git-repo-check","--ephemeral","--json","--color","never",
            "--cd",directory,"--output-last-message",answer,"-"};
    }
    void launch(const QString& executable,const QStringList& args,const QString& cwd,int milliseconds) {
        require(!busy(),"En körning pågår redan.");require(QFileInfo(executable).isFile(),"Codex saknas. Välj codex.exe eller använd det kompletta programpaketet.");
        QDir().mkpath(accountHome());
        auto env=QProcessEnvironment::systemEnvironment();
        for(const auto& key:env.keys()) if(key.contains("TOKEN",Qt::CaseInsensitive)||key.contains("SECRET",Qt::CaseInsensitive)
            ||key.contains("API_KEY",Qt::CaseInsensitive)||key.startsWith("CODEX_")||key.startsWith("OPENAI_"))env.remove(key);
        env.insert("CODEX_HOME",accountHome());
        const auto vendor=QFileInfo(executable).dir().absoluteFilePath("..");
        env.insert("PATH",QDir(vendor).filePath("codex-path")+';'+env.value("PATH"));
        process.setProcessEnvironment(env);process.setWorkingDirectory(cwd);process.setProgram(executable);process.setArguments(args);
        stopped=false;completed=false;failedEvent=false;overflow=false;finalized=false;outputBytes=0;pending.clear();
        process.start();deadline.start(milliseconds);
    }
    void account(const QString& executable,bool login) {
        require(!busy(),"En körning pågår redan.");operation=login?"login":"account";
        // A dedicated real Codex account directory; credentials never enter the job or result package.
        launch(executable,login?QStringList{"login"}:QStringList{"login","status"},QStandardPaths::writableLocation(QStandardPaths::HomeLocation),10*60*1000);
    }
    void run(const QString& executable,const Job& input,const QString& destination,const QString& sandbox) {
        require(!busy(),"En körning pågår redan.");noLinks(destination);
        require(QFileInfo(destination).isDir(),"Välj en befintlig resultatmapp.");
        job=input;operation="run";startedAt=utc();
        runDirectory=QDir(destination).filePath("run-"+QUuid::createUuid().toString(QUuid::WithoutBraces));
        require(QDir().mkdir(runDirectory),"Kunde inte skapa en ny körmapp.");
        workspace=runDirectory+"/workspace";require(QDir().mkpath(workspace+"/ARAMF_WORKER"),"Kunde inte skapa arbetsmapp.");
        save(runDirectory+"/job.json",job.bytes);
        save(runDirectory+"/run.json",QJsonDocument(QJsonObject{{"status","RUNNING"},{"startedAt",startedAt},{"jobSha256",hash(job.bytes)}}).toJson());
        for(const auto v:job.value["inputs"].toArray()) {
            const auto o=v.toObject();const auto path=QDir(workspace).filePath(o["path"].toString());
            require(QDir().mkpath(QFileInfo(path).path()),"Kunde inte skapa indatakatalog.");save(path,o["text"].toString().toUtf8());
        }
        save(workspace+"/AGENTS.md","Read and follow ARAMF_WORKER/AGENTS.md.\n");
        save(workspace+"/ARAMF_WORKER/AGENTS.md",("# Assigned pilot job\n\nDerived from reviewed pilot job "+job.id+"; SHA256 "+hash(job.bytes)+
            ". This is a standalone station pilot, not an ARAMF TaskContract or a certificate.\n\n"+job.prompt()).toUtf8());
        baseline.clear();QDirIterator initial(workspace,QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);
        while(initial.hasNext()){const auto path=initial.next();baseline[QDir(workspace).relativeFilePath(path)]=hash(read(path));}
        eventFile.setFileName(runDirectory+"/events.jsonl");require(eventFile.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Kunde inte skapa körlogg.");
        try {
            launch(executable,runArguments(workspace,runDirectory+"/answer.txt",sandbox),workspace,20*60*1000);
            process.write(job.prompt().toUtf8());process.closeWriteChannel();
        } catch(...) {eventFile.close();throw;}
    }
    void stop() {
        stopped=true;releaseGroup();
        if(busy()){process.kill();process.waitForFinished(3000);}
    }
};
}
