#pragma once
#include "WorkerRuntime.h"
#include <QtWidgets>

namespace station {
class Window:public QWidget {
    Runtime runtime;
    Job job;
    bool loaded=false;
    QLineEdit *runtimePath,*destination;
    QPlainTextEdit *preview,*activity;
    QLabel *state,*identity;
    QPushButton *start,*stop,*open,*demo,*login,*account,*results;
    QComboBox* sandbox;
    QString resultPath;
    void alert(const QString& message){QMessageBox::warning(this,"ARAMF Worker Station",message);}
    template<typename F> void guarded(F f){try{f();}catch(const std::exception& e){alert(QString::fromUtf8(e.what()));}}
    void busy(bool running) {
        start->setEnabled(!running&&loaded);stop->setEnabled(running);open->setEnabled(!running);demo->setEnabled(!running);
        login->setEnabled(!running);account->setEnabled(!running);runtimePath->setEnabled(!running);
        destination->setEnabled(!running);sandbox->setEnabled(!running);
    }
public:
    explicit Window():runtime(this) {
        setWindowTitle("ARAMF Worker Station — pilot 0.1");resize(1000,800);setMinimumSize(760,640);
        setStyleSheet("QWidget{font-family:'Segoe UI';font-size:14px;background:#f5f7fb;color:#18283b;}"
            "QGroupBox{font-weight:600;border:1px solid #d4dfeb;border-radius:8px;margin-top:16px;padding:15px;}"
            "QGroupBox::title{subcontrol-origin:margin;left:12px;}"
            "QPushButton{background:white;border:1px solid #b7c8dc;border-radius:5px;padding:8px 12px;}"
            "QPushButton:hover{background:#e9f1fc;}QPushButton:disabled{color:#8593a3;background:#edf0f4;}"
            "QLineEdit,QPlainTextEdit,QComboBox{background:white;border:1px solid #cbd7e5;border-radius:4px;padding:6px;}"
            "QPushButton#start{background:#175bc2;color:white;font-weight:600;}QPushButton#start:disabled{background:#aebbd0;}");
        auto* outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);outer->addWidget(scroll);
        auto* content=new QWidget;scroll->setWidget(content);
        auto* layout=new QVBoxLayout(content);layout->setContentsMargins(24,20,24,20);layout->setSpacing(12);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        auto* title=new QLabel("ARAMF Worker Station");title->setStyleSheet("font-size:26px;font-weight:700;");layout->addWidget(title);
        auto* intro=new QLabel("En agent · Ett uppdrag · En separat arbetsmapp");intro->setStyleSheet("color:#52708c;");layout->addWidget(intro);
        auto* setup=new QGroupBox("1. Anslut ChatGPT");auto* form=new QFormLayout(setup);
        auto* rrow=new QHBoxLayout;runtimePath=new QLineEdit(Runtime::findCodex());runtimePath->setObjectName("runtimePath");
        auto* browse=new QPushButton("Välj codex.exe");rrow->addWidget(runtimePath,1);rrow->addWidget(browse);form->addRow("AI-motor",rrow);
        auto* auth=new QHBoxLayout;login=new QPushButton("Logga in med ChatGPT");account=new QPushButton("Kontrollera inloggning");auth->addWidget(login);auth->addWidget(account);auth->addStretch();form->addRow(auth);
        auto* authnote=new QLabel("Inloggning öppnas i webbläsaren. Ditt kontos Codex-åtkomst och användningsgränser gäller.");authnote->setWordWrap(true);form->addRow(authnote);layout->addWidget(setup);
        auto* assignment=new QGroupBox("2. Välj uppdrag");auto* assign=new QVBoxLayout(assignment);auto* filebuttons=new QHBoxLayout;
        demo=new QPushButton("Skapa provuppdrag");demo->setObjectName("demoButton");open=new QPushButton("Öppna uppdragsfil…");filebuttons->addWidget(demo);filebuttons->addWidget(open);filebuttons->addStretch();assign->addLayout(filebuttons);
        identity=new QLabel("Inget uppdrag valt.");identity->setTextFormat(Qt::PlainText);identity->setWordWrap(true);assign->addWidget(identity);
        preview=new QPlainTextEdit;preview->setReadOnly(true);preview->setMinimumHeight(100);preview->setMaximumHeight(150);preview->setPlaceholderText("Uppdragets mål och instruktioner visas här före start.");assign->addWidget(preview);layout->addWidget(assignment);
        auto* runbox=new QGroupBox("3. Kör och granska");auto* runform=new QFormLayout(runbox);
        auto* dirrow=new QHBoxLayout;destination=new QLineEdit(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/ARAMF Worker Runs");
        auto* folder=new QPushButton("Välj mapp…");dirrow->addWidget(destination,1);dirrow->addWidget(folder);runform->addRow("Resultat sparas i",dirrow);
        sandbox=new QComboBox;sandbox->addItem("Windows MXC (uppdaterad Windows 11)","mxc");sandbox->addItem("Begränsad Windows-token (pilot)","unelevated");runform->addRow("Körskydd",sandbox);
        auto* note=new QLabel("Varje start skapar en ny arbetsmapp. MXC stoppar om datorn saknar stöd. Tokenläget har svagare nätverksisolering. Använd endast testdata i piloten.");note->setWordWrap(true);runform->addRow(note);
        auto* actions=new QHBoxLayout;start=new QPushButton("Starta uppdrag");start->setObjectName("start");stop=new QPushButton("Stoppa");results=new QPushButton("Öppna senaste resultat");results->setEnabled(false);
        actions->addWidget(start);actions->addWidget(stop);actions->addStretch();actions->addWidget(results);runform->addRow(actions);
        state=new QLabel("Redo för ett första test.");state->setTextFormat(Qt::PlainText);state->setWordWrap(true);runform->addRow(state);layout->addWidget(runbox);
        activity=new QPlainTextEdit;activity->setReadOnly(true);activity->setMinimumHeight(140);activity->setMaximumBlockCount(1500);activity->setPlaceholderText("Här visas inloggning, agentens arbete och eventuella fel.");layout->addWidget(activity,1);
        auto* footer=new QLabel("Pilot: uppdrag öppnas från fil. Koppling till ARAMF:s medlemssida och automatisk certifiering ingår inte.");footer->setWordWrap(true);footer->setStyleSheet("color:#65758a;font-size:12px;");layout->addWidget(footer);
        connect(browse,&QPushButton::clicked,this,[this]{if(runtime.busy())return;const auto f=QFileDialog::getOpenFileName(this,"Välj officiell Codex-motor",runtimePath->text(),"Codex (codex.exe)");if(!f.isEmpty())runtimePath->setText(f);});
        connect(folder,&QPushButton::clicked,this,[this]{if(runtime.busy())return;const auto d=QFileDialog::getExistingDirectory(this,"Välj resultatmapp",destination->text());if(!d.isEmpty())destination->setText(d);});
        connect(demo,&QPushButton::clicked,this,[this]{guarded([this]{
            const auto base=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)+"/ARAMF Worker Jobs";require(QDir().mkpath(base),"Kan inte skapa uppdragsmapp.");
            const auto name=base+"/pilot-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".aramf-job.json";
            const auto example=Job::demo();save(name,example.bytes);loadJob(name);activity->appendPlainText("Provuppdrag sparat: "+name);
        });});
        connect(open,&QPushButton::clicked,this,[this]{const auto f=QFileDialog::getOpenFileName(this,"Öppna uppdrag",QString(),"ARAMF-uppdrag (*.aramf-job.json *.json)");if(!f.isEmpty())guarded([this,f]{loadJob(f);});});
        connect(login,&QPushButton::clicked,this,[this]{guarded([this]{state->setText("Slutför inloggningen i webbläsaren.");busy(true);runtime.account(runtimePath->text(),true);});if(!runtime.busy())busy(false);});
        connect(account,&QPushButton::clicked,this,[this]{guarded([this]{busy(true);runtime.account(runtimePath->text(),false);});if(!runtime.busy())busy(false);});
        connect(start,&QPushButton::clicked,this,[this]{guarded([this]{
            require(QDir().mkpath(destination->text()),"Kan inte skapa resultatmapp.");busy(true);state->setText("Agenten arbetar. Automatisk tidsgräns: 20 minuter.");
            activity->clear();runtime.run(runtimePath->text(),job,destination->text(),sandbox->currentData().toString());
        });if(!runtime.busy())busy(false);});
        connect(stop,&QPushButton::clicked,this,[this]{state->setText("Stoppar agenten och dess underprocesser…");runtime.stop();});
        connect(results,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(resultPath));});
        runtime.log=[this](const QString& s){activity->appendPlainText(s.trimmed());};
        runtime.done=[this](const QString& status,const QString& path){
            busy(false);
            if(path.isEmpty()){state->setText(status=="OK"?"Kontokontrollen slutförd. Se informationen i loggen.":"Kontokontrollen misslyckades eller avbröts. Se loggen.");return;}
            resultPath=path;results->setEnabled(true);
            state->setText(status=="DELIVERED"?"Leverans klar · filkontroller godkända · inväntar din granskning.":"Körresultat: "+status+". Öppna resultatet och granska rapporten.");
            activity->appendPlainText("Resultatmapp: "+path);
        };
        busy(false);
        resize(1000,850);
    }
    void loadJob(const QString& file) {
        // Parse completely before replacing the currently reviewed job.
        Job candidate=Job::parse(read(file,256*1024));job=candidate;loaded=true;
        identity->setText(job.member+"  |  Team: "+job.team+"  |  Ansvarig: "+job.manager+"  |  "+job.id);
        preview->setPlainText(job.goal+"\n\n"+job.instructions+"\n\nLeveranskrav:\n"+QString::fromUtf8(QJsonDocument(job.value["outputs"].toArray()).toJson()));
        state->setText("Uppdrag inläst. Granska instruktionerna före start.");busy(false);
    }
    void smoke(const QString& directory) {
        QDir().mkpath(directory);save(directory+"/demo.aramf-job.json",Job::demo().bytes);loadJob(directory+"/demo.aramf-job.json");
        require(start->isEnabled()&&!stop->isEnabled(),"Fel knappstatus efter inläsning.");
        require(preview->toPlainText().contains("välkomstsida"),"Uppdragsförhandsvisningen saknas.");
        require(grab().save(directory+"/station.png"),"Skärmbild kunde inte sparas.");
    }
protected:
    void closeEvent(QCloseEvent* e) override {
        if(runtime.busy()){
            if(QMessageBox::question(this,"Stoppa pågående arbete?","Agenten arbetar fortfarande. Stoppa och stäng?",QMessageBox::Yes|QMessageBox::No)!=QMessageBox::Yes){e->ignore();return;}
            runtime.stop();
        }
        e->accept();
    }
};
}
