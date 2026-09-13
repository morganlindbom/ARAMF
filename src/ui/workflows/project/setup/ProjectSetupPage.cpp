#include "ProjectSetupPage.h"
#include "../../../../core/ProjectRootRebindService.h"

#include "core/AramfPaths.h"

#include <QDir>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>

namespace {
void addOption(QComboBox* combo, const QString& label, const QString& id)
{
    combo->addItem(label, id);
}
void setData(QComboBox* combo, const QString& value)
{
    const QSignalBlocker blocker(combo);
    const int index = combo->findData(value);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}
}

ProjectSetupPage::ProjectSetupPage(ProjectModel* model, TemplateManager* manager,
                                   ProjectPersistence* persistence, QWidget* parent)
    : QWidget(parent),
      model_(model),
      manager_(manager),
      persistence_(persistence),
      name_(new QLineEdit(this)),
      path_(new QLineEdit(this)),
      id_(new QLineEdit(this)),
      projectFilePath_(new QLineEdit(this)),
      type_(new QLineEdit(this)),
      description_(new QTextEdit(this))
{
    name_->setReadOnly(true);
    name_->setObjectName(QStringLiteral("canonicalProjectName"));
    // Description is the flexible field, but it must yield space to the
    // fixed-content controls above it when the page is short.
    description_->setObjectName(QStringLiteral("projectDescription"));
    description_->setMinimumHeight(64);
    description_->setMaximumHeight(96);
    description_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(
        tr("<h2>Project file, path &amp; Worker</h2>Define project identity, storage and the canonical Worker directory."), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    auto* actions = new QHBoxLayout;
    for (const auto& action : {tr("New"), tr("Open"), tr("Save"), tr("Save As")}) {
        auto* button = new QPushButton(action, this);
        actions->addWidget(button);
        if (action == tr("New")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::newProject);
        if (action == tr("Open")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::openProject);
        if (action == tr("Save")) connect(button, &QPushButton::clicked, this, &ProjectSetupPage::saveProject);
        if (action == tr("Save As")) connect(button, &QPushButton::clicked, this, [this] { saveProjectAs(); });
    }
    actions->addStretch();
    layout->addLayout(actions);
    auto* workerNameForm = new QFormLayout;
    workerNameSuffix_ = new QLineEdit(this);
    workerNameSuffix_->setObjectName(QStringLiteral("workerNameSuffix"));
    workerNameSuffix_->setPlaceholderText(tr("Optional suffix, e.g. ANDROID_PICO"));
    workerNamePreview_ = new QLabel(this);
    workerNamePreview_->setObjectName(QStringLiteral("workerNamePreview"));
    workerPath_ = new QLineEdit(this);
    workerPath_->setObjectName(QStringLiteral("workerPath"));
    workerPath_->setReadOnly(true);
    workerNameForm->addRow(tr("Worker name suffix"), workerNameSuffix_);
    workerNameForm->addRow(tr("Worker name"), workerNamePreview_);
    workerNameForm->addRow(tr("Worker path"), workerPath_);
    layout->addLayout(workerNameForm);

    communicationGroup_ = new QGroupBox(tr("Cross-target communication"), this);
    communicationGroup_->setObjectName(QStringLiteral("communicationGroup"));
    auto* communicationForm = new QFormLayout(communicationGroup_);
    communicationTransport_ = new QComboBox(communicationGroup_);
    addOption(communicationTransport_, tr("Wi-Fi"), "wifi");
    addOption(communicationTransport_, tr("Bluetooth"), "bluetooth");
    addOption(communicationTransport_, tr("Serial / UART"), "serial");
    communicationProtocol_ = new QComboBox(communicationGroup_);
    communicationProtocol_->setObjectName(QStringLiteral("communicationProtocol"));
    addOption(communicationProtocol_, tr("Choose protocol"), {});
    addOption(communicationProtocol_, tr("HTTP / REST"), "http-rest");
    addOption(communicationProtocol_, tr("WebSocket"), "websocket");
    addOption(communicationProtocol_, tr("TCP"), "tcp");
    addOption(communicationProtocol_, tr("UDP"), "udp");
    addOption(communicationProtocol_, tr("Serial protocol"), "serial");
    communicationSourceTarget_ = new QComboBox(communicationGroup_);
    communicationDestinationTarget_ = new QComboBox(communicationGroup_);
    communicationSourceTarget_->setObjectName(QStringLiteral("communicationEndpointATarget"));
    communicationDestinationTarget_->setObjectName(QStringLiteral("communicationEndpointBTarget"));
    for (auto* combo : {communicationSourceTarget_, communicationDestinationTarget_}) {
        addOption(combo, tr("Android Application"), "android-application");
        addOption(combo, tr("Raspberry Pi Pico 2 W"), "raspberry-pi-pico-2-w");
        addOption(combo, tr("Arduino-compatible controller"), "arduino-mcu");
        addOption(combo, tr("HM-10 Bluetooth module"), "hm-10-bluetooth");
        addOption(combo, tr("Custom target"), "custom");
    }
    communicationSourceRole_ = new QComboBox(communicationGroup_);
    communicationDestinationRole_ = new QComboBox(communicationGroup_);
    communicationSourceRole_->setObjectName(QStringLiteral("communicationEndpointARole"));
    communicationDestinationRole_->setObjectName(QStringLiteral("communicationEndpointBRole"));
    for (auto* combo : {communicationSourceRole_, communicationDestinationRole_}) {
        addOption(combo, tr("Client"), "client"); addOption(combo, tr("Server"), "server");
        addOption(combo, tr("Controller"), "controller"); addOption(combo, tr("Monitor"), "monitor");
        addOption(combo, tr("Bidirectional peer"), "peer");
    }
    communicationDirection_ = new QComboBox(communicationGroup_);
    communicationDirection_->setObjectName(QStringLiteral("communicationDirection"));
    addOption(communicationDirection_, tr("Bidirectional"), "bidirectional");
    addOption(communicationDirection_, tr("Unidirectional"), "unidirectional");
    addOption(communicationDirection_, tr("Request / response"), "request-response");
    addOption(communicationDirection_, tr("Event-driven"), "event-driven");
    communicationFrameType_ = new QComboBox(communicationGroup_);
    communicationFrameType_->setObjectName(QStringLiteral("communicationFrameType"));
    addOption(communicationFrameType_, tr("Binary"), "binary"); addOption(communicationFrameType_, tr("Text"), "text");
    communicationLogicalModel_ = new QComboBox(communicationGroup_);
    communicationLogicalModel_->setObjectName(QStringLiteral("communicationLogicalModel"));
    addOption(communicationLogicalModel_, tr("Structured messages"), "structured-messages");
    addOption(communicationLogicalModel_, tr("Raw values"), "raw-values");
    communicationWireEncoding_ = new QComboBox(communicationGroup_);
    communicationWireEncoding_->setObjectName(QStringLiteral("communicationWireEncoding"));
    addOption(communicationWireEncoding_, tr("CBOR"), "cbor"); addOption(communicationWireEncoding_, tr("JSON"), "json");
    addOption(communicationWireEncoding_, tr("None / raw"), "raw");
    communicationByteOrder_ = new QComboBox(communicationGroup_);
    communicationByteOrder_->setObjectName(QStringLiteral("communicationByteOrder"));
    addOption(communicationByteOrder_, tr("Little Endian"), "little-endian"); addOption(communicationByteOrder_, tr("Big Endian"), "big-endian");
    communicationEndpointAAddress_ = new QLineEdit(communicationGroup_);
    communicationEndpointAAddress_->setObjectName(QStringLiteral("communicationEndpointAAddress"));
    communicationEndpointAAddress_->setPlaceholderText(tr("Endpoint A address (no secrets)"));
    communicationEndpointBAddress_ = new QLineEdit(communicationGroup_);
    communicationEndpointBAddress_->setObjectName(QStringLiteral("communicationEndpointBAddress"));
    communicationEndpointBAddress_->setPlaceholderText(tr("Endpoint B address / host:port (no secrets)"));
    communicationVersion_ = new QLineEdit(communicationGroup_); communicationVersion_->setPlaceholderText("1");
    communicationAuthentication_ = new QCheckBox(tr("Authentication required"), communicationGroup_);
    communicationEncryption_ = new QCheckBox(tr("Encrypted transport required"), communicationGroup_);
    communicationForm->addRow(tr("Endpoint A target"), communicationSourceTarget_);
    communicationForm->addRow(tr("Endpoint A role"), communicationSourceRole_);
    communicationForm->addRow(tr("Endpoint A address"), communicationEndpointAAddress_);
    communicationForm->addRow(tr("Endpoint B target"), communicationDestinationTarget_);
    communicationForm->addRow(tr("Endpoint B role"), communicationDestinationRole_);
    communicationForm->addRow(tr("Endpoint B address"), communicationEndpointBAddress_);
    communicationForm->addRow(tr("Direction"), communicationDirection_);
    communicationForm->addRow(tr("Transport"), communicationTransport_);
    communicationForm->addRow(tr("Protocol"), communicationProtocol_);
    communicationForm->addRow(tr("Frame type"), communicationFrameType_);
    communicationForm->addRow(tr("Logical data model"), communicationLogicalModel_);
    communicationForm->addRow(tr("Wire encoding"), communicationWireEncoding_);
    communicationForm->addRow(tr("Protocol version"), communicationVersion_);
    communicationForm->addRow(tr("Byte order"), communicationByteOrder_);
    communicationForm->addRow(communicationAuthentication_);
    communicationForm->addRow(communicationEncryption_);
    communicationGroup_->setVisible(false);
    layout->addWidget(communicationGroup_);

    auto* form = new QFormLayout;
    form->addRow(tr("Project name"), name_);
    auto* pathRow = new QWidget(this);
    auto* pathLayout = new QHBoxLayout(pathRow);
    pathLayout->setContentsMargins(0, 0, 0, 0);
    auto* browse = new QPushButton(tr("Browse..."), pathRow);
    path_->setMinimumWidth(0);
    path_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    browse->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    pathLayout->addWidget(path_);
    pathLayout->addWidget(browse);
    form->addRow(tr("Project path"), pathRow);
    projectFilePath_->setObjectName(QStringLiteral("projectFilePath"));
    projectFilePath_->setReadOnly(true);
    form->addRow(tr("Project file"), projectFilePath_);
    form->addRow(tr("Project ID"), id_);
    type_->setObjectName("projectType");
    form->addRow(tr("Project type"), type_);

    form->addRow(tr("Description"), description_);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addLayout(form);
    migrationStatus_ = new QLabel(this);
    migrationStatus_->setObjectName(QStringLiteral("projectMigrationStatus"));
    migrationStatus_->setWordWrap(true);
    migrationDetails_ = new QLabel(this);
    migrationDetails_->setObjectName(QStringLiteral("projectMigrationDetails"));
    migrationDetails_->setWordWrap(true);
    layout->addWidget(migrationStatus_);
    layout->addWidget(migrationDetails_);
    layout->addStretch();

    id_->setReadOnly(true);
    connect(type_, &QLineEdit::textEdited, model_, &ProjectModel::setContext);
    connect(path_, &QLineEdit::textChanged, this, [this](const QString& value) {
        model_->setProjectPath(value);
        syncCanonicalIdentity(true);
    });
    connect(workerNameSuffix_, &QLineEdit::textChanged, this, [this](const QString& raw) {
        workerNameEditing_ = true;
        workerNameRawInput_ = raw;
        const QString normalized = AramfPaths::normalizeWorkerNameSuffix(raw);
        model_->setWorkerNameSuffix(normalized);
        syncCanonicalIdentity(true);
    });
    connect(workerNameSuffix_, &QLineEdit::editingFinished, this, [this] {
        workerNameEditing_ = false;
        const QString normalized = AramfPaths::normalizeWorkerNameSuffix(workerNameSuffix_->text());
        const QSignalBlocker blocker(workerNameSuffix_);
        workerNameSuffix_->setText(normalized);
        model_->setWorkerNameSuffix(normalized);
        workerNameRawInput_ = normalized;
        syncCanonicalIdentity(true);
    });
    connect(browse, &QPushButton::clicked, this, &ProjectSetupPage::browseProjectPath);
    connect(description_, &QTextEdit::textChanged, this, [this] {
        model_->setDescription(description_->toPlainText());
    });
    const auto persistCommunication = [this] {
        auto value = model_->communicationConfiguration();
        value.sourceTarget = communicationSourceTarget_->currentData().toString();
        value.destinationTarget = communicationDestinationTarget_->currentData().toString();
        value.protocol = communicationProtocol_->currentData().toString();
        value.sourceRole = communicationSourceRole_->currentData().toString();
        value.destinationRole = communicationDestinationRole_->currentData().toString();
        value.transport = communicationTransport_->currentData().toString();
        value.protocolVersion = communicationVersion_->text();
        value.authenticationRequired = communicationAuthentication_->isChecked();
        value.encryptionRequired = communicationEncryption_->isChecked();
        if (value.endpoints.size() < 2) {
            value.endpoints = {{QStringLiteral("endpoint-a"), value.sourceTarget, QString(), value.sourceRole, {}, {}},
                               {QStringLiteral("endpoint-b"), value.destinationTarget, QString(), value.destinationRole, {}, {}}};
        }
        value.endpoints[0].targetId = value.sourceTarget; value.endpoints[0].role = value.sourceRole; value.endpoints[0].address = communicationEndpointAAddress_->text();
        value.endpoints[1].targetId = value.destinationTarget; value.endpoints[1].role = value.destinationRole; value.endpoints[1].address = communicationEndpointBAddress_->text();
        if (value.links.isEmpty()) value.links.append({QStringLiteral("communication-link"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("bidirectional"), value.transport, value.protocol, {}, {}, {}, value.protocolVersion, {}, {}, {}, false, 0, {}});
        auto& link = value.links[0]; link.direction = communicationDirection_->currentData().toString(); link.transport = value.transport; link.protocol = value.protocol; link.frameType = communicationFrameType_->currentData().toString(); link.logicalDataModel = communicationLogicalModel_->currentData().toString(); link.wireEncoding = communicationWireEncoding_->currentData().toString(); link.protocolVersion = value.protocolVersion; link.byteOrder = communicationByteOrder_->currentData().toString();
        model_->setCommunicationConfiguration(value);
    };
    connect(communicationProtocol_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationSourceTarget_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationDestinationTarget_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationSourceRole_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationDestinationRole_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationDirection_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationTransport_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationFrameType_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationLogicalModel_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationWireEncoding_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationByteOrder_, &QComboBox::currentIndexChanged, this, persistCommunication);
    connect(communicationEndpointAAddress_, &QLineEdit::textChanged, this, persistCommunication);
    connect(communicationEndpointBAddress_, &QLineEdit::textChanged, this, persistCommunication);
    connect(communicationVersion_, &QLineEdit::textChanged, this, persistCommunication);
    connect(communicationAuthentication_, &QCheckBox::toggled, this, persistCommunication);
    connect(communicationEncryption_, &QCheckBox::toggled, this, persistCommunication);
    connect(model_, &ProjectModel::modelChanged,
            this, &ProjectSetupPage::refreshFromModel);
    refreshFromModel();
}

void ProjectSetupPage::browseProjectPath()
{
    const QString currentPath = model_->projectPath().trimmed();
    const QString initialDirectory = QFileInfo(currentPath).isDir()
        ? currentPath
        : QDir::homePath();
    const QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        tr("Select Project Directory"),
        initialDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selectedDirectory.isEmpty() || selectedDirectory == model_->projectPath()) {
        return;
    }

    // Browse changes only the managed target path. It never saves or changes
    // the separate ARAMF configuration-file path.
    model_->setProjectPath(selectedDirectory);
}

void ProjectSetupPage::newProject()
{
    if (!confirmDiscardOrSave()) return;
    model_->resetForNewProject();
    manager_->applyTemplate(model_, manager_->builtInTemplates().first());
    model_->setModified(true);
}

void ProjectSetupPage::openProject()
{
    if (!confirmDiscardOrSave()) return;
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Open ARAMF Project"), QString(), tr("ARAMF Projects (*.aramf.json *.json);;All files (*)"));
    if (filePath.isEmpty()) return;

    QString error;
    if (!persistence_->load(model_, filePath, &error)) {
        QMessageBox::warning(this, tr("Open Project"), error);
        return;
    }
    const auto rebind = ProjectRootRebindService().rebind(model_, QFileInfo(filePath).absolutePath(), true);
    if (!rebind.success) QMessageBox::warning(this, tr("Open Project"), rebind.error);
}

void ProjectSetupPage::saveProject()
{
    QString error;
    if (!saveCurrentProject(&error)) {
        QMessageBox::warning(this, tr("Save Project"), error);
    }
}

bool ProjectSetupPage::saveProjectAs(QString* error)
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Select Project Directory"), model_->projectPath().isEmpty() ? QDir::homePath() : model_->projectPath());
    if (directory.isEmpty()) {
        if (error) *error = tr("Save As was cancelled.");
        return false;
    }
    const QString filePath = QDir(directory).filePath(AramfPaths::workerDirectoryName(model_->workerNameSuffix()) + QStringLiteral(".aramf.json"));
    QString saveError;
    if (!writeProject(filePath, &saveError)) {
        if (error) {
            *error = saveError;
        } else {
            QMessageBox::warning(this, tr("Save Project"), saveError);
        }
        return false;
    }
    return true;
}

bool ProjectSetupPage::writeProject(const QString& filePath, QString* error)
{
    QString saveError;
    if (!persistence_->save(*model_, filePath, &saveError)) {
        if (error) *error = saveError;
        return false;
    }
    model_->setProjectFilePath(filePath);
    model_->setModified(false);
    return true;
}

bool ProjectSetupPage::saveCurrentProject(QString* error)
{
    if (model_->projectFilePath().trimmed().isEmpty()) {
        return saveProjectAs(error);
    }
    return writeProject(model_->projectFilePath(), error);
}

bool ProjectSetupPage::saveForGeneration(QString* error)
{
    return saveCurrentProject(error);
}

bool ProjectSetupPage::confirmDiscardOrSave()
{
    if (!model_->isModified()) return true;

    const auto choice = QMessageBox::question(
        this, tr("Unsaved Project"), tr("Save changes before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Cancel) return false;
    if (choice == QMessageBox::Save) {
        saveProject();
        return !model_->isModified();
    }
    return true;
}

void ProjectSetupPage::syncCanonicalIdentity(bool deriveProjectFile)
{
    const QString workerName = AramfPaths::workerDirectoryName(model_->workerNameSuffix());
    if (model_->projectName() != workerName) model_->setProjectName(workerName);
    workerNamePreview_->setText(workerName);
    const QString derivedFileName = workerName + QStringLiteral(".aramf.json");
    if (deriveProjectFile && !model_->projectPath().trimmed().isEmpty() && model_->projectFilePath().trimmed().isEmpty())
        model_->setProjectFilePath(QDir(model_->projectPath()).filePath(derivedFileName));
    projectFilePath_->setText(model_->projectFilePath().isEmpty() ? derivedFileName : model_->projectFilePath());
    const QString workerPath = model_->projectPath().trimmed().isEmpty()
        ? workerName
        : QDir(model_->projectPath()).filePath(workerName);
    workerPath_->setText(workerPath);
}

void ProjectSetupPage::refreshFromModel()
{
    const QSignalBlocker nameBlocker(name_);
    const QSignalBlocker pathBlocker(path_);
    const QSignalBlocker idBlocker(id_);
    const QSignalBlocker projectFileBlocker(projectFilePath_);
    const QSignalBlocker typeBlocker(type_);
    const QSignalBlocker descriptionBlocker(description_);
    const QSignalBlocker suffixBlocker(workerNameSuffix_);

    name_->setText(AramfPaths::workerDirectoryName(model_->workerNameSuffix()));
    path_->setText(model_->projectPath());
    id_->setText(model_->projectId());
    type_->setText(model_->context());
    const auto communication = model_->communicationConfiguration();
    communicationGroup_->setVisible(communication.enabled);
    const auto endpointA = communication.endpoints.size() > 0 ? communication.endpoints.at(0) : CommunicationEndpoint{};
    const auto endpointB = communication.endpoints.size() > 1 ? communication.endpoints.at(1) : CommunicationEndpoint{};
    const auto link = communication.links.isEmpty() ? CommunicationLink{} : communication.links.first();
    setData(communicationProtocol_, link.protocol.isEmpty() ? communication.protocol : link.protocol);
    setData(communicationTransport_, link.transport.isEmpty() ? communication.transport : link.transport);
    setData(communicationSourceTarget_, endpointA.targetId.isEmpty() ? communication.sourceTarget : endpointA.targetId);
    setData(communicationDestinationTarget_, endpointB.targetId.isEmpty() ? communication.destinationTarget : endpointB.targetId);
    setData(communicationSourceRole_, endpointA.role.isEmpty() ? communication.sourceRole : endpointA.role);
    setData(communicationDestinationRole_, endpointB.role.isEmpty() ? communication.destinationRole : endpointB.role);
    setData(communicationDirection_, link.direction);
    setData(communicationFrameType_, link.frameType);
    setData(communicationLogicalModel_, link.logicalDataModel);
    setData(communicationWireEncoding_, link.wireEncoding);
    setData(communicationByteOrder_, link.byteOrder);
    { const QSignalBlocker b1(communicationEndpointAAddress_), b2(communicationEndpointBAddress_), b3(communicationVersion_), b4(communicationAuthentication_), b5(communicationEncryption_);
      communicationEndpointAAddress_->setText(endpointA.address); communicationEndpointBAddress_->setText(endpointB.address); communicationVersion_->setText(link.protocolVersion.isEmpty() ? communication.protocolVersion : link.protocolVersion);
      communicationAuthentication_->setChecked(communication.authenticationRequired); communicationEncryption_->setChecked(communication.encryptionRequired); }
    type_->setReadOnly(model_->projectTypeLocked());
    description_->setPlainText(model_->description());
    if (!workerNameEditing_ && !workerNameSuffix_->hasFocus()) {
        workerNameRawInput_ = model_->workerNameSuffix();
        workerNameSuffix_->setText(workerNameRawInput_);
    }
    syncCanonicalIdentity(false);
    const auto notices = model_->migrationNotices();
    migrationStatus_->setText(notices.isEmpty()
        ? tr("Project compatibility: current schema %1 (no migration review required).").arg(model_->projectSchemaVersion())
        : tr("Project compatibility: schema %1 -> %2 | %3")
              .arg(model_->migratedFromSchemaVersion()).arg(model_->projectSchemaVersion()).arg(model_->migrationStatus()));
    QStringList details;
    for (const auto& value : notices) {
        const auto notice = value.toObject();
        details << QStringLiteral("- %1: %2 Current state: %3")
                       .arg(notice.value(QStringLiteral("legacyPath")).toString(),
                            notice.value(QStringLiteral("reason")).toString(),
                            notice.value(QStringLiteral("resultingState")).toString());
    }
    migrationDetails_->setText(details.join(QLatin1Char('\n')));
    migrationDetails_->setVisible(!details.isEmpty());
}
