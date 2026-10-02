#pragma once

#include <QWidget>

#include "core/ProjectModel.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;

class ProjectCommunicationPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ProjectCommunicationPage(ProjectModel* model, QWidget* parent = nullptr);

public slots:
    void refreshFromModel();

private:
    void persistCommunication();

    ProjectModel* model_ = nullptr;
    QGroupBox* communicationGroup_ = nullptr;
    QLabel* disabledNotice_ = nullptr;
    QComboBox* protocol_ = nullptr;
    QComboBox* sourceTarget_ = nullptr;
    QComboBox* destinationTarget_ = nullptr;
    QComboBox* sourceRole_ = nullptr;
    QComboBox* destinationRole_ = nullptr;
    QComboBox* direction_ = nullptr;
    QComboBox* transport_ = nullptr;
    QComboBox* frameType_ = nullptr;
    QComboBox* logicalModel_ = nullptr;
    QComboBox* wireEncoding_ = nullptr;
    QComboBox* byteOrder_ = nullptr;
    QLineEdit* endpointAAddress_ = nullptr;
    QLineEdit* endpointBAddress_ = nullptr;
    QLineEdit* version_ = nullptr;
    QCheckBox* authentication_ = nullptr;
    QCheckBox* encryption_ = nullptr;
};
