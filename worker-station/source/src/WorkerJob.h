#pragma once
#include <QtCore>
#include <stdexcept>

namespace station {
inline void require(bool ok, const QString& message) { if (!ok) throw std::runtime_error(message.toUtf8().constData()); }
inline QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
inline QString hash(const QByteArray& data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
inline void save(const QString& path, const QByteArray& data) {
    QSaveFile f(path); require(f.open(QIODevice::WriteOnly), "Kan inte skriva: " + path);
    require(f.write(data) == data.size() && f.commit(), "Kunde inte spara: " + path);
}
inline QByteArray read(const QString& path, qint64 limit = 1024*1024) {
    QFile f(path); require(f.open(QIODevice::ReadOnly), "Kan inte läsa: " + path);
    require(f.size() <= limit, "Filen är för stor: " + path); return f.readAll();
}
inline bool safeRelative(const QString& path) {
    if (path.isEmpty() || path.size()>180 || QDir::isAbsolutePath(path) || path.contains('\\')
        || path.contains(':') || path.contains(QChar::Null) || QDir::cleanPath(path)!=path) return false;
    const auto parts=path.split('/');
    static const QRegularExpression reserved("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\.|$)", QRegularExpression::CaseInsensitiveOption);
    for (const auto& s:parts)
        if (s.isEmpty() || s=="." || s==".." || s.endsWith('.') || s.endsWith(' ') || s.startsWith('.')
            || s.compare("ARAMF_WORKER",Qt::CaseInsensitive)==0 || s.compare("AGENTS.md",Qt::CaseInsensitive)==0
            || s.contains(QRegularExpression("[<>\"|?*\\x00-\\x1f]")) || reserved.match(s).hasMatch()) return false;
    return true;
}
inline void noLinks(const QString& path) {
    QFileInfo f(path);
    while (!f.filePath().isEmpty()) {
        require(!f.isSymLink() && !f.isJunction(), "Länkar/junctions tillåts inte: " + f.filePath());
        const auto parent=f.absolutePath(); if(parent==f.absoluteFilePath()) break;
        f.setFile(parent);
    }
}
struct Job {
    QJsonObject value;
    QByteArray bytes;
    QString id, member, team, manager, goal, instructions;
    static Job parse(const QByteArray& bytes) {
        require(bytes.size()<=256*1024, "Uppdraget får vara högst 256 KiB.");
        QJsonParseError error; const auto doc=QJsonDocument::fromJson(bytes,&error);
        require(error.error==QJsonParseError::NoError && doc.isObject(), "Uppdraget måste vara giltig JSON.");
        Job j; j.value=doc.object();j.bytes=bytes;
        const QSet<QString> fields{"schema","id","member","team","manager","goal","instructions","inputs","outputs"};
        for(auto i=j.value.begin();i!=j.value.end();++i) require(fields.contains(i.key()),"Okänt uppdragsfält: "+i.key());
        require(j.value.value("schema")=="aramf-worker-pilot/v1", "Okänd uppdragsversion.");
        auto string=[&](const QString& key,int max) {
            const auto v=j.value.value(key);require(v.isString() && !v.toString().trimmed().isEmpty() && v.toString().size()<=max,"Ogiltigt fält: "+key);return v.toString();
        };
        j.id=string("id",80);j.member=string("member",100);j.team=string("team",100);j.manager=string("manager",100);
        j.goal=string("goal",4000);j.instructions=string("instructions",16000);
        require(j.value.value("inputs").isArray() && j.value.value("outputs").isArray(), "inputs och outputs ska vara listor.");
        require(j.value["inputs"].toArray().size()<=30 && j.value["outputs"].toArray().size()>0 && j.value["outputs"].toArray().size()<=30,"Ange 1–30 leveransfiler och högst 30 indatafiler.");
        for(const auto& key: {QStringLiteral("inputs"),QStringLiteral("outputs")}) {
            QSet<QString> paths;
            for(const auto v:j.value[key].toArray()) {
                require(v.isObject(),"Filpost måste vara ett objekt."); const auto o=v.toObject();const auto path=o["path"].toString();
                require(safeRelative(path) && !paths.contains(path.toCaseFolded()),"Ogiltig eller dubbel filsökväg: "+path);paths.insert(path.toCaseFolded());
                const QSet<QString> keys = key=="inputs" ? QSet<QString>{"path","text"} : QSet<QString>{"path","mustContain"};
                for(auto it=o.begin();it!=o.end();++it)require(keys.contains(it.key()),"Okänt filfält: "+it.key());
                if(key=="inputs") require(o["text"].isString() && o["text"].toString().size()<=64000,"Indata måste vara text, högst 64 000 tecken.");
                else {
                    require(o["mustContain"].isArray() && o["mustContain"].toArray().size()<=30,"mustContain måste vara en lista (högst 30).");
                    for(const auto term:o["mustContain"].toArray()) require(term.isString() && !term.toString().isEmpty() && term.toString().size()<=1000,"Ogiltigt innehållskrav.");
                }
            }
        }
        // Windows is case-insensitive; reject file/directory and differently-cased aliases across both lists.
        QStringList paths;QMap<QString,QString> spelling;
        for(const auto& key:{QStringLiteral("inputs"),QStringLiteral("outputs")}) for(const auto v:j.value[key].toArray()) {
            const auto s=v.toObject()["path"].toString(), folded=s.toCaseFolded();
            require(!spelling.contains(folded)||spelling[folded]==s,"Olika stavning av samma Windows-fil.");spelling[folded]=s;
            for(const auto& previous:paths) require(!folded.startsWith(previous+'/')&&!previous.startsWith(folded+'/'),"Fil och katalog har överlappande sökväg.");
            paths.append(folded);
        }
        return j;
    }
    QString prompt() const {
        return "Read AGENTS.md and ARAMF_WORKER/AGENTS.md first. Execute only this single assigned pilot job. "
               "Do not contact other agents, publish, commit, push, install tools, change account settings or access unrelated work. "
               "Do not use recursive deletion. Work only in this working directory. "
               "Report what you changed, checks actually run and limitations. Completion is a delivery for human review, never certification.\n\n"
               "Member: "+member+"\nTeam: "+team+"\nManager: "+manager+"\nGoal: "+goal+"\nInstructions:\n"+instructions+
               "\nExpected outputs:\n"+QString::fromUtf8(QJsonDocument(value["outputs"].toArray()).toJson());
    }
    static Job demo() {
        QJsonObject d{{"schema","aramf-worker-pilot/v1"},{"id","pilot-001"},{"member","Agent 01"},{"team","Morgan"},{"manager","Morgan"},
            {"goal","Skapa en liten svensk välkomstsida för agentstationen."},
            {"instructions","Skapa index.html som en fristående HTML-sida utan externa länkar, skript, typsnitt eller nätverksanrop. Visa rubriken ARAMF Worker och texten Agent 01 är redo. Använd enkel inbyggd CSS. Läs brief.txt. Kontrollera HTML-filen och rapportera resultatet på svenska."},
            {"inputs",QJsonArray{QJsonObject{{"path","brief.txt"},{"text","Detta är ett provuppdrag för en separat Windows-agentstation. En tydlig sida med mörkblå rubrik och vit bakgrund räcker. Ingen publicering."}}}},
            {"outputs",QJsonArray{QJsonObject{{"path","index.html"},{"mustContain",QJsonArray{"ARAMF Worker","Agent 01 är redo"}}}}}};
        return parse(QJsonDocument(d).toJson());
    }
};
inline QJsonObject inspectOutputs(const Job& j,const QString& workspace) {
    QJsonArray checks;bool pass=true;
    for(const auto v:j.value["outputs"].toArray()) {
        const auto o=v.toObject();const auto relative=o["path"].toString();const auto absolute=QDir(workspace).filePath(relative);
        QJsonObject c{{"path",relative}};
        try {
            noLinks(absolute);require(QFileInfo(absolute).isFile(),"Leveransfil saknas.");
            const auto bytes=read(absolute,4*1024*1024);const auto text=QString::fromUtf8(bytes);
            for(const auto term:o["mustContain"].toArray())require(text.contains(term.toString()),"Ett innehållskrav saknas: "+term.toString());
            c["status"]="PASS";c["sha256"]=hash(bytes);c["size"]=qint64(bytes.size());
        }catch(const std::exception& e){c["status"]="FAIL";c["reason"]=QString::fromUtf8(e.what());pass=false;}
        checks.append(c);
    }
    return {{"status",pass?"PASS":"FAIL"},{"checks",checks},{"meaning","Only declared file-existence/content checks; not independent semantic verification or certification."}};
}
}
