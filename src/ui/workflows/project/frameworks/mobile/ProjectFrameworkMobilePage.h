// File: src/ui/workflows/project/frameworks/mobile/ProjectFrameworkMobilePage.h
#pragma once

#include "../ProjectFrameworksPage.h"
#include "core/EnvironmentCatalog.h"
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

class ProjectFrameworkMobilePage final : public ProjectFrameworksPage
{
public:
    explicit ProjectFrameworkMobilePage(ProjectModel* model, QWidget* parent = nullptr)
    // Brief: Offer independent, reviewable profiles alongside the existing mobile framework catalog.
        : ProjectFrameworksPage(model, parent, EnvironmentCatalog::frameworkMobile(), {"flutter", "react-native", "dotnet-maui", "android-sdk", "android-emulator", "android-device-testing", "jetpack-compose", "room", "androidx-viewmodel", "kotlin-coroutines", "retrofit"}), model_(model)
    {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);
        layout->addWidget(new QLabel(tr("<h2>Project profile</h2>Review and edit defaults before saving or generating. Profiles configure this project; they do not modify source repositories or provision credentials."), panel));
        profiles_ = new QComboBox(panel); profiles_->setObjectName("projectProfileSelector");
        for (const auto& option : EnvironmentCatalog::projectProfiles()) profiles_->addItem(option.first, option.second);
        layout->addWidget(profiles_);
        source_ = new QLabel(panel); source_->setWordWrap(true); source_->setTextFormat(Qt::PlainText); layout->addWidget(source_);
        settings_ = new QTableWidget(panel); settings_->setObjectName("projectProfileSettings");
        settings_->setColumnCount(2); settings_->setHorizontalHeaderLabels({tr("Setting"), tr("Configured value")});
        settings_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); settings_->setMinimumHeight(240); layout->addWidget(settings_);
        features_ = new QTableWidget(panel); features_->setObjectName("projectProfileFeatures");
        features_->setColumnCount(4); features_->setHorizontalHeaderLabels({tr("Capability"), tr("Selected"), tr("Required"), tr("Availability")});
        features_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        features_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        features_->setMinimumHeight(240); layout->addWidget(features_);
        error_ = new QLabel(panel); error_->setWordWrap(true); layout->addWidget(error_);
        static_cast<QVBoxLayout*>(this->layout())->insertWidget(0, panel);
        connect(profiles_, &QComboBox::currentIndexChanged, this, [this] {
            QString error; model_->selectProjectProfile(profiles_->currentData().toString(), &error); error_->setText(error); refreshProfile();
        });
        connect(settings_, &QTableWidget::cellChanged, this, [this](int row, int column) {
            if (column != 1 || refreshing_) return;
            QString error;
            model_->setProjectProfileSetting(settings_->item(row, 0)->data(Qt::UserRole).toString(), settings_->item(row, 1)->text(), &error);
            error_->setText(error);
        });
        connect(features_, &QTableWidget::cellChanged, this, [this](int row, int column) {
            if (refreshing_ || column == 0) return;
            const auto key = features_->item(row, 0)->data(Qt::UserRole).toString();
            QString error;
            model_->setProjectProfileFeature(key, features_->item(row, 1)->checkState() == Qt::Checked,
                features_->item(row, 2)->checkState() == Qt::Checked, features_->item(row, 3)->text(), &error);
            error_->setText(error);
            if (!error.isEmpty()) refreshProfile();
        });
        connect(model_, &ProjectModel::modelChanged, this, [this] { refreshProfile(); }, Qt::QueuedConnection);
        refreshProfile();
    }
private:
    void refreshProfile()
    // Brief: Present source defaults separately from configured settings and unsupported capabilities.
    {
        refreshing_ = true;
        const QSignalBlocker profileBlocker(profiles_), settingBlocker(settings_), featureBlocker(features_);
        const auto profile = model_->projectProfile();
        const auto definition = EnvironmentCatalog::projectProfile(profile.value("id").toString());
        profiles_->setCurrentIndex(profiles_->findData(profile.value("id").toString()));
        source_->setText(definition.value("source").toString() + tr("\nAvailable, planned and unsupported describe capability availability. Selection and requirement do not constitute test evidence. Change required to unchecked before deselecting a requirement."));
        const auto settings = profile.value("settings").toObject();
        settings_->setRowCount(settings.size()); int row = 0;
        for (auto it = settings.begin(); it != settings.end(); ++it, ++row) {
            auto* key = new QTableWidgetItem(it.key()); key->setData(Qt::UserRole, it.key()); key->setFlags(key->flags() & ~Qt::ItemIsEditable);
            settings_->setItem(row, 0, key); settings_->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
            settings_->item(row, 1)->setToolTip(tr("Source default: ") + definition.value("settings").toObject().value(it.key()).toString());
        }
        const auto choices = profile.value("features").toObject();
        features_->setRowCount(choices.size()); row = 0;
        for (auto it = choices.begin(); it != choices.end(); ++it, ++row) {
            const auto state = it.value().toObject();
            const auto fact = definition.value("features").toObject().value(it.key()).toObject();
            auto* label = new QTableWidgetItem(fact.value("label").toString()); label->setData(Qt::UserRole, it.key());
            label->setToolTip(fact.value("provenance").toString()); label->setFlags(label->flags() & ~Qt::ItemIsEditable); features_->setItem(row, 0, label);
            for (int column = 1; column <= 2; ++column) {
                auto* item = new QTableWidgetItem;
                item->setFlags((item->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
                item->setCheckState(state.value(column == 1 ? "selected" : "required").toBool() ? Qt::Checked : Qt::Unchecked); features_->setItem(row, column, item);
            }
            auto* availability = new QTableWidgetItem(state.value("availability").toString());
            availability->setToolTip(tr("available, planned or unsupported")); features_->setItem(row, 3, availability);
        }
        settings_->setVisible(!profile.isEmpty()); features_->setVisible(!profile.isEmpty());
        refreshing_ = false;
    }
    ProjectModel* model_;
    QComboBox* profiles_;
    QTableWidget* settings_;
    QTableWidget* features_;
    QLabel* source_;
    QLabel* error_;
    bool refreshing_ = false;
};
