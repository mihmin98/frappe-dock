#include "platform/quickviewsurfacefactory.h"

#include "core/config/configfacade.h"
#include "core/geometry/layout.h"
#include "core/interfaces/iiconprovider.h"
#include "core/model/tilemodel.h"
#include "platform/iconprovider.h"

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

using namespace frappe;

Q_LOGGING_CATEGORY(FRAPPE_SURFACE, "frappe.surface")

namespace
{
constexpr auto dockQmlUrl = "qrc:/qt/qml/org/kde/frappedock/Dock.qml";

QScreen *screenForOutput(const QString &outputId)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == outputId) {
            return screen;
        }
    }
    return QGuiApplication::primaryScreen();
}
}

QuickViewSurfaceFactory::QuickViewSurfaceFactory(ConfigFacade *config,
                                                 TileModel *model,
                                                 const IIconProvider *icons,
                                                 QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_model(model)
    , m_icons(icons)
{
}

QuickViewSurfaceFactory::~QuickViewSurfaceFactory()
{
    const QList<QQuickView *> views = m_views.values();
    for (QQuickView *view : views) {
        delete view;
    }
}

void QuickViewSurfaceFactory::setController(QObject *controller)
{
    m_controller = controller;
}

void QuickViewSurfaceFactory::createSurface(const OutputInfo &output)
{
    if (m_views.contains(output.id)) {
        return;
    }

    auto *view = new QQuickView;
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(Qt::transparent);
    view->engine()->addImageProvider(QStringLiteral("frappeicon"), new QmlIconProvider(m_icons));
    view->setInitialProperties({
        {QStringLiteral("tileModel"), QVariant::fromValue(m_model)},
        {QStringLiteral("controller"), QVariant::fromValue(m_controller)},
    });
    view->setSource(QUrl(QString::fromLatin1(dockQmlUrl)));

    // Dock.qml's signal is declared in QML, so this is a by-name connection and
    // the compiler cannot check it. A silent failure here means a dock whose
    // icons do nothing, which is worth a warning rather than a mystery.
    QQuickItem *root = view->rootObject();
    if (!root) {
        qCWarning(FRAPPE_SURFACE) << "Dock.qml failed to load; the surface will be empty";
    } else {
        // SIGNAL/SLOT strings are unavoidable here: the sender is a QML object
        // whose signals have no C++ declaration to take a pointer to.
        if (!connect(root, SIGNAL(tileClicked(QString, int, int)), this, SIGNAL(tileClicked(QString, int, int)))) {
            qCWarning(FRAPPE_SURFACE) << "Dock.qml has no tileClicked signal; clicks will do nothing";
        }
        if (!connect(root, SIGNAL(tileHeld(QString)), this, SIGNAL(tileHeld(QString)))) {
            qCWarning(FRAPPE_SURFACE) << "Dock.qml has no tileHeld signal; press and hold will do nothing";
        }
    }

    m_views.insert(output.id, view);
    configureSurface(view, output);
    view->show();
}

void QuickViewSurfaceFactory::destroySurface(const QString &outputId)
{
    QQuickView *view = m_views.take(outputId);
    if (!view) {
        return;
    }
    // Destroying a view while it is dispatching an event crashes; the surface set
    // is reconciled from event handlers, so this is not hypothetical.
    view->deleteLater();
}

void QuickViewSurfaceFactory::moveSurface(const QString &fromOutputId, const OutputInfo &to)
{
    QQuickView *view = m_views.take(fromOutputId);
    if (!view) {
        createSurface(to);
        return;
    }

    m_views.insert(to.id, view);
    configureSurface(view, to);
}

std::vector<QString> QuickViewSurfaceFactory::surfaces() const
{
    std::vector<QString> ids;
    ids.reserve(m_views.size());
    for (auto it = m_views.cbegin(); it != m_views.cend(); ++it) {
        ids.push_back(it.key());
    }
    return ids;
}

void QuickViewSurfaceFactory::updateGeometry()
{
    for (auto it = m_views.cbegin(); it != m_views.cend(); ++it) {
        OutputInfo output;
        output.id = it.key();
        configureSurface(it.value(), output);
    }
}

void QuickViewSurfaceFactory::configureSurface(QQuickView *view, const OutputInfo &output)
{
    using LayerShellQt::Window;

    // The proportion model, plan.md Part 0. S is the tile edge; nothing here is
    // a literal beyond the ratios themselves.
    const qreal tileSize = m_config->tileSize();
    const qreal gap = tileSize / 3.0;
    // The dock floats: its outer edge sits g/3 = S/9 from the screen edge. This
    // is a layer-shell margin, not QML padding, so the shelf's own geometry stays
    // a pure function of S.
    const int screenGap = qRound(gap / 3.0);

    // Two different thicknesses, and conflating them is what cut magnified
    // icons off at the top. The shelf is what the dock *is* — normative
    // geometry, S + 2g, unaffected by the magnification setting. The surface is
    // what it needs to *draw on*, which at a high peak is several times that,
    // because a magnified tile grows out of the shelf and away from the screen
    // edge. Both come from core so the view and the surface cannot disagree.
    const qreal peak = m_config->magnificationEnabled()
        ? m_config->magnificationFactor() * tileSize : tileSize;
    const int shelfThickness = qRound(geometry::shelfThickness(tileSize, gap));
    const int surfaceThickness = qRound(geometry::surfaceThickness(tileSize, gap, peak));

    QScreen *screen = screenForOutput(output.id);
    if (screen) {
        view->setScreen(screen);
    }

    Window *layer = Window::get(view);
    layer->setScope(QStringLiteral("frappe-dock"));
    layer->setLayer(Window::LayerTop);
    layer->setKeyboardInteractivity(Window::KeyboardInteractivityOnDemand);

    // A strip must anchor to three edges: with a size of 0 on the spanning axis
    // and a single anchor, the compositor has no width to hand out and kills the
    // connection with a protocol error. setExclusiveEdge() then says which of the
    // three the reserved zone is measured from.
    switch (m_config->position()) {
    case ConfigFacade::Left:
        layer->setAnchors(Window::Anchors(Window::AnchorLeft) | Window::AnchorTop | Window::AnchorBottom);
        layer->setExclusiveEdge(Window::AnchorLeft);
        layer->setMargins(QMargins(screenGap, 0, 0, 0));
        // 0 on the spanning axis means "compositor decides that one".
        layer->setDesiredSize(QSize(surfaceThickness, 0));
        view->setWidth(surfaceThickness);
        break;
    case ConfigFacade::Right:
        layer->setAnchors(Window::Anchors(Window::AnchorRight) | Window::AnchorTop | Window::AnchorBottom);
        layer->setExclusiveEdge(Window::AnchorRight);
        layer->setMargins(QMargins(0, 0, screenGap, 0));
        layer->setDesiredSize(QSize(surfaceThickness, 0));
        view->setWidth(surfaceThickness);
        break;
    case ConfigFacade::Bottom:
    default:
        layer->setAnchors(Window::Anchors(Window::AnchorBottom) | Window::AnchorLeft | Window::AnchorRight);
        layer->setExclusiveEdge(Window::AnchorBottom);
        layer->setMargins(QMargins(0, 0, 0, screenGap));
        layer->setDesiredSize(QSize(0, surfaceThickness));
        view->setHeight(surfaceThickness);
        break;
    }

    // The zone is measured from the surface's own edge, and the compositor adds
    // the margin on top of it — so this must be the thickness alone. Adding
    // screenGap here reserves it twice and leaves a visible dead strip above
    // the dock. Measured on KWin 6.7.4: reserved area came back as exactly
    // zone + margin at every tile size tried.
    //
    // The *shelf's* thickness, not the surface's: the headroom above the shelf
    // is drawn into but not occupied, and reserving it would push every window
    // up by space the dock only borrows while the pointer is over it.
    layer->setExclusiveZone(shelfThickness);

    // Input stops at the shelf too. Without this the surface would swallow
    // clicks across the whole screen edge over a band as tall as the largest
    // possible tile — mostly empty space, and at a high peak most of the band.
    // Nothing is lost by it: hover is driven from the shelf, so the headroom
    // was never interactive.
    const QRect span = screen ? screen->geometry() : QRect(0, 0, 8192, 8192);
    switch (m_config->position()) {
    case ConfigFacade::Left:
        view->setMask(QRegion(0, 0, shelfThickness, span.height()));
        break;
    case ConfigFacade::Right:
        view->setMask(QRegion(surfaceThickness - shelfThickness, 0, shelfThickness, span.height()));
        break;
    case ConfigFacade::Bottom:
    default:
        view->setMask(QRegion(0, surfaceThickness - shelfThickness, span.width(), shelfThickness));
        break;
    }

    // Layer-surface state is double-buffered and latches on the next surface
    // commit. With a static scene Qt renders no new frame, so without this the
    // setters above silently never reach the compositor.
    view->requestUpdate();
}
