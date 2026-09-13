#pragma once

#include <QString>

namespace ProjectSchema {

inline constexpr int CurrentVersion = 1;
inline const QString MigrationOk = QStringLiteral("MIGRATION_OK");
inline const QString MigrationReviewRequired = QStringLiteral("MIGRATION_REVIEW_REQUIRED");
inline const QString MigrationBlocked = QStringLiteral("MIGRATION_BLOCKED");

} // namespace ProjectSchema
