#include <QQmlEngine>
#include <QStandardPaths>
#include <QtQuickTest>

/// Points the FrappeConfig singleton at the test tree before any QML touches it,
/// so running the suite cannot overwrite the developer's real settings.
class Setup : public QObject
{
    Q_OBJECT

public:
    Setup()
    {
        QStandardPaths::setTestModeEnabled(true);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(tst_qml, Setup)

#include "tst_qml.moc"
