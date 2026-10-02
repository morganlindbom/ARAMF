#include "ProjectCommunicationPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QVBoxLayout>

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

void addTargetOptions(QComboBox* combo)
{
    addOption(combo, QObject::tr("Android Application"), QStringLiteral("android-application"));
    addOption(combo, QObject::tr("Raspberry Pi Pico 2 W"), QStringLiteral("raspberry-pi-pico-2-w"));
    addOption(combo, QObject::tr("Arduino-compatible controller"), QStringLiteral("arduino-mcu"));
    addOption(combo, QObject::tr("HM-10 Bluetooth module"), QStringLiteral("hm-10-bluetooth"));
    addOption(combo, QObject::tr("Custom target"), QStringLiteral("custom"));
}

void addRoleOptions(QComboBox* combo)
{
    addOption(combo, QObject::tr("Client"), QStringLiteral("client"));
    addOption(combo, QObject::tr("Server"), QStringLiteral("server"));
    addOption(combo, QObject::tr("Controller"), QStringLiteral("controller"));
    addOption(combo, QObject::tr("Monitor"), QStringLiteral("monitor"));
    addOption(combo, QObject::tr("Bidirectional peer"), QStringLiteral("peer"));
}

}

ProjectCommunicationPage::ProjectCommunicationPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent), model_(model)
{
    auto* layout = new QVBoxLayout(this);
    auto* heading = new QLabel(
        tr("<h2>Project communication</h2>Define how the project's targets communicate. Secrets must not be stored here."), this);
    heading->setWordWrap(true);
    layout->addWidget(heading);

    disabledNotice_ = new QLabel(
        tr("Cross-target communication is not enabled by the selected project modules or template."), this);
    disabledNotice_->setWordWrap(true);
    disabledNotice_->setObjectName(QStringLiteral("communicationDisabledNotice"));
    layout->addWidget(disabledNotice_);

    communicationGroup_ = new QGroupBox(tr("Cross-target communication"), this);
    communicationGroup_->setObjectName(QStringLiteral("communicationGroup"));
    auto* form = new QFormLayout(communicationGroup_);

    sourceTarget_ = new QComboBox(communicationGroup_);
    destinationTarget_ = new QComboBox(communicationGroup_);
    sourceTarget_->setObjectName(QStringLiteral("communicationEndpointATarget"));
    destinationTarget_->setObjectName(QStringLiteral("communicationEndpointBTarget"));
    addTargetOptions(sourceTarget_);
    addTargetOptions(destinationTarget_);

    sourceRole_ = new QComboBox(communicationGroup_);
    destinationRole_ = new QComboBox(communicationGroup_);
    sourceRole_->setObjectName(QStringLiteral("communicationEndpointARole"));
    destinationRole_->setObjectName(QStringLiteral("communicationEndpointBRole"));
    addRoleOptions(sourceRole_);
    addRoleOptions(destinationRole_);

    endpointAAddress_ = new QLineEdit(communicationGroup_);
    endpointBAddress_ = new QLineEdit(communicationGroup_);
    endpointAAddress_->setObjectName(QStringLiteral("communicationEndpointAAddress"));
    endpointBAddress_->setObjectName(QStringLiteral("communicationEndpointBAddress"));
    endpointAAddress_->setPlaceholderText(tr("Endpoint A address (no secrets)"));
    endpointBAddress_->setPlaceholderText(tr("Endpoint B address / host:port (no secrets)"));

    direction_ = new QComboBox(communicationGroup_);
    addOption(direction_, tr("Bidirectional"), QStringLiteral("bidirectional"));
    addOption(direction_, tr("Unidirectional"), QStringLiteral("unidirectional"));
    addOption(direction_, tr("Request / response"), QStringLiteral("request-response"));
    addOption(direction_, tr("Event-driven"), QStringLiteral("event-driven"));

    transport_ = new QComboBox(communicationGroup_);
    addOption(transport_, tr("Wi-Fi"), QStringLiteral("wifi"));
    addOption(transport_, tr("Bluetooth"), QStringLiteral("bluetooth"));
    addOption(transport_, tr("Serial / UART"), QStringLiteral("serial"));

    protocol_ = new QComboBox(communicationGroup_);
    addOption(protocol_, tr("Choose protocol"), {});
    addOption(protocol_, tr("HTTP / REST"), QStringLiteral("http-rest"));
    addOption(protocol_, tr("WebSocket"), QStringLiteral("websocket"));
    addOption(protocol_, tr("TCP"), QStringLiteral("tcp"));
    addOption(protocol_, tr("UDP"), QStringLiteral("udp"));
    addOption(protocol_, tr("Serial protocol"), QStringLiteral("serial"));

    frameType_ = new QComboBox(communicationGroup_);
    addOption(frameType_, tr("Binary"), QStringLiteral("binary"));
    addOption(frameType_, tr("Text"), QStringLiteral("text"));
    logicalModel_ = new QComboBox(communicationGroup_);
    addOption(logicalModel_, tr("Structured messages"), QStringLiteral("structured-messages"));
    addOption(logicalModel_, tr("Raw values"), QStringLiteral("raw-values"));
    wireEncoding_ = new QComboBox(communicationGroup_);
    addOption(wireEncoding_, tr("CBOR"), QStringLiteral("cbor"));
    addOption(wireEncoding_, tr("JSON"), QStringLiteral("json"));
    addOption(wireEncoding_, tr("None / raw"), QStringLiteral("raw"));
    byteOrder_ = new QComboBox(communicationGroup_);
    addOption(byteOrder_, tr("Little Endian"), QStringLiteral("little-endian"));
    addOption(byteOrder_, tr("Big Endian"), QStringLiteral("big-endian"));
    version_ = new QLineEdit(communicationGroup_);
    version_->setObjectName(QStringLiteral("communicationProtocolVersion"));
    version_->setPlaceholderText(QStringLiteral("1"));
    authentication_ = new QCheckBox(tr("Authentication required"), communicationGroup_);
    encryption_ = new QCheckBox(tr("Encrypted transport required"), communicationGroup_);

    form->addRow(tr("Endpoint A target"), sourceTarget_);
    form->addRow(tr("Endpoint A role"), sourceRole_);
    form->addRow(tr("Endpoint A address"), endpointAAddress_);
    form->addRow(tr("Endpoint B target"), destinationTarget_);
    form->addRow(tr("Endpoint B role"), destinationRole_);
    form->addRow(tr("Endpoint B address"), endpointBAddress_);
    form->addRow(tr("Direction"), direction_);
    form->addRow(tr("Transport"), transport_);
    form->addRow(tr("Protocol"), protocol_);
    form->addRow(tr("Frame type"), frameType_);
    form->addRow(tr("Logical data model"), logicalModel_);
    form->addRow(tr("Wire encoding"), wireEncoding_);
    form->addRow(tr("Protocol version"), version_);
    form->addRow(tr("Byte order"), byteOrder_);
    form->addRow(authentication_);
    form->addRow(encryption_);
    layout->addWidget(communicationGroup_);
    layout->addStretch();

    const QList<QObject*> controls{
        protocol_, sourceTarget_, destinationTarget_, sourceRole_, destinationRole_,
        direction_, transport_, frameType_, logicalModel_, wireEncoding_, byteOrder_,
        endpointAAddress_, endpointBAddress_, version_, authentication_, encryption_};
    for (QObject* control : controls) {
        if (auto* combo = qobject_cast<QComboBox*>(control))
            connect(combo, &QComboBox::currentIndexChanged, this, [this] { persistCommunication(); });
        else if (auto* edit = qobject_cast<QLineEdit*>(control))
            connect(edit, &QLineEdit::textChanged, this, [this] { persistCommunication(); });
        else if (auto* check = qobject_cast<QCheckBox*>(control))
            connect(check, &QCheckBox::toggled, this, [this] { persistCommunication(); });
    }
    connect(model_, &ProjectModel::modelChanged, this, &ProjectCommunicationPage::refreshFromModel);
    refreshFromModel();
}

void ProjectCommunicationPage::persistCommunication()
{
    if (!model_) return;
    auto value = model_->communicationConfiguration();
    value.sourceTarget = sourceTarget_->currentData().toString();
    value.destinationTarget = destinationTarget_->currentData().toString();
    value.protocol = protocol_->currentData().toString();
    value.sourceRole = sourceRole_->currentData().toString();
    value.destinationRole = destinationRole_->currentData().toString();
    value.transport = transport_->currentData().toString();
    value.protocolVersion = version_->text();
    value.authenticationRequired = authentication_->isChecked();
    value.encryptionRequired = encryption_->isChecked();
    if (value.endpoints.size() < 2) {
        value.endpoints = {{QStringLiteral("endpoint-a"), value.sourceTarget, {}, value.sourceRole, {}, {}},
                           {QStringLiteral("endpoint-b"), value.destinationTarget, {}, value.destinationRole, {}, {}}};
    }
    value.endpoints[0].targetId = value.sourceTarget;
    value.endpoints[0].role = value.sourceRole;
    value.endpoints[0].address = endpointAAddress_->text();
    value.endpoints[1].targetId = value.destinationTarget;
    value.endpoints[1].role = value.destinationRole;
    value.endpoints[1].address = endpointBAddress_->text();
    if (value.links.isEmpty())
        value.links.append({QStringLiteral("communication-link"), QStringLiteral("endpoint-a"), QStringLiteral("endpoint-b"), QStringLiteral("bidirectional"), value.transport, value.protocol, {}, {}, {}, value.protocolVersion, {}, {}, {}, false, 0, {}});
    auto& link = value.links[0];
    link.direction = direction_->currentData().toString();
    link.transport = value.transport;
    link.protocol = value.protocol;
    link.frameType = frameType_->currentData().toString();
    link.logicalDataModel = logicalModel_->currentData().toString();
    link.wireEncoding = wireEncoding_->currentData().toString();
    link.protocolVersion = value.protocolVersion;
    link.byteOrder = byteOrder_->currentData().toString();
    model_->setCommunicationConfiguration(value);
}

void ProjectCommunicationPage::refreshFromModel()
{
    if (!model_) return;
    const auto communication = model_->communicationConfiguration();
    communicationGroup_->setVisible(communication.enabled);
    disabledNotice_->setVisible(!communication.enabled);
    const auto endpointA = communication.endpoints.size() > 0 ? communication.endpoints.at(0) : CommunicationEndpoint{};
    const auto endpointB = communication.endpoints.size() > 1 ? communication.endpoints.at(1) : CommunicationEndpoint{};
    const auto link = communication.links.isEmpty() ? CommunicationLink{} : communication.links.first();
    setData(protocol_, link.protocol.isEmpty() ? communication.protocol : link.protocol);
    setData(transport_, link.transport.isEmpty() ? communication.transport : link.transport);
    setData(sourceTarget_, endpointA.targetId.isEmpty() ? communication.sourceTarget : endpointA.targetId);
    setData(destinationTarget_, endpointB.targetId.isEmpty() ? communication.destinationTarget : endpointB.targetId);
    setData(sourceRole_, endpointA.role.isEmpty() ? communication.sourceRole : endpointA.role);
    setData(destinationRole_, endpointB.role.isEmpty() ? communication.destinationRole : endpointB.role);
    setData(direction_, link.direction);
    setData(frameType_, link.frameType);
    setData(logicalModel_, link.logicalDataModel);
    setData(wireEncoding_, link.wireEncoding);
    setData(byteOrder_, link.byteOrder);
    const QSignalBlocker b1(endpointAAddress_), b2(endpointBAddress_), b3(version_), b4(authentication_), b5(encryption_);
    endpointAAddress_->setText(endpointA.address);
    endpointBAddress_->setText(endpointB.address);
    version_->setText(link.protocolVersion.isEmpty() ? communication.protocolVersion : link.protocolVersion);
    authentication_->setChecked(communication.authenticationRequired);
    encryption_->setChecked(communication.encryptionRequired);
}
