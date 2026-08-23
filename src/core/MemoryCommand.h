#pragma once

#include <QStringList>

class QTextStream;

int runMemoryCommand(const QStringList& arguments, QTextStream& output, QTextStream& error);
