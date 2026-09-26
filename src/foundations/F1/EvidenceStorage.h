#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>
// F1 physical persistence and reconstruction. No semantic Foundation services.
class EvidenceStorage final {
public:
 // Physical append of a prepared record. Semantic provenance/scope policy is
 // enforced by external orchestration, never by another Foundation here.
 bool appendEvidence(const QString&,QJsonObject,QString* = nullptr) const;
 QList<QJsonObject> events(const QString&,QString* = nullptr) const;
 QList<QJsonObject> decisions(const QString&,bool = true,QString* = nullptr) const;
 QList<QJsonObject> currentDecisions(const QString&,QString* = nullptr) const;
 QList<QJsonObject> checkpoints(const QString&,QString* = nullptr) const;
 bool generateCurrentState(const QString&,QString* = nullptr) const;
 static bool requiresControlPlane(const QString&);
 bool generateColdStartValidation(const QString&,QString* = nullptr) const;
 bool generateColdStartValidation(const QString&,QString*,bool) const;
 QJsonObject validateColdStart(const QString&,QString* = nullptr) const;
 bool refreshDerivedState(const QString&,QString* = nullptr) const;
 qint64 managedMemoryUsage(const QString&) const;
 qint64 memoryUsageBytes(const QString&) const;
};
