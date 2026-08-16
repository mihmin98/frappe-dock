#pragma once

#include <QHash>
#include <QObject>
#include <QUrl>

#include "platform/surfacemanager.h"

QT_BEGIN_NAMESPACE
class QQuickView;
QT_END_NAMESPACE

namespace frappe
{

class ConfigFacade;
class IIconProvider;
class TileModel;

/// Makes each dock surface a layer-shell QQuickView loading Dock.qml.
///
/// Owns the views; SurfaceManager only tells it which outputs need one.
class QuickViewSurfaceFactory : public QObject, public ISurfaceFactory
{
    Q_OBJECT

public:
    QuickViewSurfaceFactory(ConfigFacade *config, TileModel *model, const IIconProvider *icons, QObject *parent = nullptr);

    /// The object QML routes interactions through. Set before the first surface
    /// is created; it is passed as an initial property and not re-read after.
    void setController(QObject *controller);
    ~QuickViewSurfaceFactory() override;

    void createSurface(const OutputInfo &output) override;
    void destroySurface(const QString &outputId) override;
    void moveSurface(const QString &fromOutputId, const OutputInfo &to) override;
    std::vector<QString> surfaces() const override;

    /// Re-applies size, margins and exclusive zone to every surface. Call after
    /// tileSize or position changes.
    void updateGeometry();

Q_SIGNALS:
    /// A tile was interacted with. Wired to the controller in main(), which
    /// decides what the interaction means.
    void tileClicked(const QString &tileId, int button, int modifiers);
    void tileHeld(const QString &tileId);

private:
    void configureSurface(QQuickView *view, const OutputInfo &output);

    ConfigFacade *m_config;
    TileModel *m_model;
    const IIconProvider *m_icons;
    QObject *m_controller = nullptr;
    QHash<QString, QQuickView *> m_views;
};

}
