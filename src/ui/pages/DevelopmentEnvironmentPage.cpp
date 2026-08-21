// DevelopmentEnvironmentPage.cpp

#include "DevelopmentEnvironmentPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
QComboBox* makeCombo(QWidget* parent, std::initializer_list<std::pair<const char*, const char*>> values)
{
    /**Create a data-backed combo box.

    Display labels remain user friendly while stable IDs are stored in the project model.
    */
    auto* combo = new QComboBox(parent);
    for (const auto& value : values) {
        combo->addItem(QString::fromUtf8(value.first), QString::fromUtf8(value.second));
    }
    return combo;
}
}

DevelopmentEnvironmentPage::DevelopmentEnvironmentPage(ProjectModel* model, QWidget* parent)
    : QWidget(parent),
      model_(model)
{
    /**Construct the target-project development environment page.

    The choices describe the project ARAMF manages; they do not add those runtimes or build systems as dependencies of the ARAMF application itself.
    */
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("<h2>Environment</h2>Template-derived defaults are shown below. Change only values that are exceptional for this project."),
        this));

    auto* form = new QFormLayout;
    ide_ = makeCombo(this, {{"Visual Studio Code", "vscode"}, {"Qt Creator", "qt-creator"}, {"CLion", "clion"}});
    compiler_ = makeCombo(this, {{"MSYS2 UCRT64 GCC", "msys2-ucrt64-gcc"}, {"Clang", "clang"}, {"MSVC", "msvc"}, {"Python runtime", "python"}});
    os_ = makeCombo(this, {{"Windows", "windows"}, {"Linux", "linux"}, {"macOS", "macos"}});
    target_ = makeCombo(this, {{"Desktop", "desktop"}, {"Web", "web"}, {"Server", "server"}, {"Embedded", "embedded"}, {"Cloud", "cloud"}});
    build_ = makeCombo(this, {{"CMake", "cmake"}, {"Ninja", "ninja"}, {"npm scripts", "npm"}, {"Make", "make"}, {"PyProject", "pyproject"}});
    auto* language = makeCombo(this, {{"C++", "cpp"}, {"Python", "python"}, {"TypeScript / JavaScript", "typescript"}, {"C#", "csharp"}});
    auto* framework = makeCombo(this, {{"None", "none"}, {"Qt 6", "qt6"}, {"Pico SDK", "pico-sdk"}, {"React", "react"}, {"FastAPI", "fastapi"}, {"ASP.NET", "aspnet"}});

    form->addRow(tr("Language"), language);
    form->addRow(tr("Framework"), framework);
    form->addRow(tr("IDE"), ide_);
    form->addRow(tr("Target project compiler/runtime"), compiler_);
    form->addRow(tr("Operating system"), os_);
    form->addRow(tr("Primary target"), target_);
    form->addRow(tr("Target project build system"), build_);
    layout->addLayout(form);

    auto* advanced = new QGroupBox(tr("Advanced Environment Options"), this);
    advanced->setCheckable(true);
    advanced->setChecked(false);
    auto* capabilities = new QGroupBox(tr("Additional targets and capabilities"), advanced);
    auto* capabilitiesLayout = new QVBoxLayout(capabilities);
    for (const auto& text : {
             tr("Cross-platform build"),
             tr("Debug build"),
             tr("Release build"),
             tr("Package installer"),
             tr("Container image"),
             tr("Cross-compilation")}) {
        capabilitiesLayout->addWidget(new QCheckBox(text, capabilities));
    }
    auto* advancedLayout = new QVBoxLayout(advanced);
    advancedLayout->addWidget(capabilities);
    capabilities->setVisible(false);
    connect(advanced, &QGroupBox::toggled, capabilities, &QWidget::setVisible);
    layout->addWidget(advanced);
    layout->addStretch();

    const auto updateModel = [this, language, framework] {
        auto environment = model_->developmentEnvironment();
        environment.language = language->currentData().toString();
        environment.framework = framework->currentData().toString();
        environment.ide = ide_->currentData().toString();
        environment.compiler = compiler_->currentData().toString();
        environment.operatingSystem = os_->currentData().toString();
        environment.targetPlatform = target_->currentData().toString();
        environment.buildSystem = build_->currentData().toString();
        model_->setDevelopmentEnvironment(environment);
    };

    connect(ide_, &QComboBox::currentIndexChanged, this, updateModel);
    connect(compiler_, &QComboBox::currentIndexChanged, this, updateModel);
    connect(os_, &QComboBox::currentIndexChanged, this, updateModel);
    connect(target_, &QComboBox::currentIndexChanged, this, updateModel);
    connect(build_, &QComboBox::currentIndexChanged, this, updateModel);
    connect(language, &QComboBox::currentIndexChanged, this, updateModel);
    connect(framework, &QComboBox::currentIndexChanged, this, updateModel);
    connect(model_, &ProjectModel::developmentEnvironmentChanged, this, &DevelopmentEnvironmentPage::refreshFromModel);
    refreshFromModel();
}

void DevelopmentEnvironmentPage::refreshFromModel()
{
    /**Synchronize environment controls from the project model.

    Signal blockers prevent a refresh from being interpreted as a new user edit.
    */
    const auto environment = model_->developmentEnvironment();
    const QSignalBlocker ideBlocker(ide_);
    const QSignalBlocker compilerBlocker(compiler_);
    const QSignalBlocker osBlocker(os_);
    const QSignalBlocker targetBlocker(target_);
    const QSignalBlocker buildBlocker(build_);
    const auto combos = findChildren<QComboBox*>();
    const QSignalBlocker languageBlocker(combos.at(5));
    const QSignalBlocker frameworkBlocker(combos.at(6));

    ide_->setCurrentIndex(ide_->findData(environment.ide));
    compiler_->setCurrentIndex(compiler_->findData(environment.compiler));
    os_->setCurrentIndex(os_->findData(environment.operatingSystem));
    target_->setCurrentIndex(target_->findData(environment.targetPlatform));
    build_->setCurrentIndex(build_->findData(environment.buildSystem));
    if (combos.size() >= 7) {
        combos.at(5)->setCurrentIndex(combos.at(5)->findData(environment.language));
        combos.at(6)->setCurrentIndex(combos.at(6)->findData(environment.framework));
    }
}
