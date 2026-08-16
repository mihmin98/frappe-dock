#pragma once

#include <QObject>

namespace frappe
{

class ILauncherBackend;
class TileModel;

/// Routes tile activations to the launcher.
///
/// Phase 1 has one command; the modifier matrix and window activation arrive in
/// Phase 2, and this is where they will land.
class DockController : public QObject
{
    Q_OBJECT

public:
    DockController(TileModel *model, const ILauncherBackend *launcher, QObject *parent = nullptr);

public Q_SLOTS:
    /// Launches the application behind \a tileId. Unknown or unlaunchable ids are
    /// logged and ignored: a dock must degrade rather than die.
    void activateTile(const QString &tileId);

private:
    TileModel *m_model;
    const ILauncherBackend *m_launcher;
};

}
