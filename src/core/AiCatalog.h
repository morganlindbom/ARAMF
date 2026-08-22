#pragma once

#include <QString>
#include <QList>

struct AiOption {
    QString displayName;
    QString id;
    QString category;
};

namespace AiCatalog {

QList<AiOption> agents();
QList<AiOption> responsibilities();
QList<AiOption> permissions();
QList<AiOption> integrations();

}
