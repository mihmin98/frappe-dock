#include "platform/quickviewsurfacefactory.h"

#include "core/config/configfacade.h"
#include "core/geometry/layout.h"
#include "core/interfaces/iiconprovider.h"
#include "core/model/tilemodel.h"
#include "platform/blurregion.h"
#include "platform/iconprovider.h"

#include <KWindowEffects>

#include <LayerShellQt/Window>

#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QScreen>

#include <utility>

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
    // Zero-interval and single-shot: everything the shelf reports within one
    // turn of the event loop becomes one blur update, and the update still
    // lands before the frame it belongs to is drawn.
    m_blurUpdate.setSingleShot(true);
    m_blurUpdate.setInterval(0);
    connect(&m_blurUpdate, &QTimer::timeout, this, &QuickViewSurfaceFactory::flushBlur);
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

    // Built before the view, because it is passed in as an initial property and
    // Dock.qml binds to it while the component is being completed.
    auto backend = std::make_unique<FolderBackend>();
    auto stackModel = std::make_unique<StackModel>(backend.get());
    StackModel *stackModelRaw = stackModel.get();
    m_folderBackends.insert_or_assign(output.id, std::move(backend));
    m_stackModels.insert_or_assign(output.id, std::move(stackModel));

    auto *view = new QQuickView;
    view->setResizeMode(QQuickView::SizeRootObjectToView);
    view->setColor(Qt::transparent);
    view->engine()->addImageProvider(QStringLiteral("frappeicon"), new QmlIconProvider(m_icons));
    view->setInitialProperties({
        {QStringLiteral("tileModel"), QVariant::fromValue(m_model)},
        {QStringLiteral("controller"), QVariant::fromValue(m_controller)},
        {QStringLiteral("stackModel"), QVariant::fromValue(stackModelRaw)},
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

        // The open stack has to join the input region or it is drawn and dead
        // to the pointer, which looks exactly like a stack that failed to open.
        const QString outputId = output.id;
        if (!connect(root, SIGNAL(stackRegionChanged(QRectF)), this, SLOT(onStackRegionChanged(QRectF)))) {
            qCWarning(FRAPPE_SURFACE) << "Dock.qml has no stackRegionChanged signal; stacks will not accept clicks";
        }
        // Without this the compositor is never told where to blur, and the
        // shelf is a flat translucent slab that looks like the effect is off.
        if (!connect(root, SIGNAL(shelfRegionChanged(QRectF)), this, SLOT(onShelfRegionChanged(QRectF)))) {
            qCWarning(FRAPPE_SURFACE) << "Dock.qml has no shelfRegionChanged signal; the shelf will not be blurred";
        }
        view->setProperty("frappeOutputId", outputId);
    }

    m_views.insert(output.id, view);
    configureSurface(view, output);
    view->show();
}

void QuickViewSurfaceFactory::onStackRegionChanged(const QRectF &region)
{
    // The sender is the QML root; which surface it belongs to is stamped on the
    // view, because one factory serves every output and the signal carries only
    // the rectangle.
    auto *root = qobject_cast<QQuickItem *>(sender());
    QQuickView *view = root ? qobject_cast<QQuickView *>(root->window()) : nullptr;
    if (!view) {
        return;
    }

    setStackRegion(view->property("frappeOutputId").toString(), region.toAlignedRect());
}

void QuickViewSurfaceFactory::onShelfRegionChanged(const QRectF &region)
{
    auto *root = qobject_cast<QQuickItem *>(sender());
    QQuickView *view = root ? qobject_cast<QQuickView *>(root->window()) : nullptr;
    if (!view) {
        return;
    }

    setShelfRegion(view->property("frappeOutputId").toString(), region.toAlignedRect());
}

void QuickViewSurfaceFactory::setShelfRegion(const QString &outputId, const QRect &rect)
{
    if (m_shelfRegions.value(outputId) == rect) {
        return;
    }
    m_shelfRegions.insert(outputId, rect);

    // Coalesced, not applied here. The shelf reports x, y, width and height as
    // four separate changes, so a magnifying dock would otherwise re-send the
    // blur region four times a frame — three of them describing a shelf that
    // was already superseded before the compositor saw it.
    m_blurPending.insert(outputId);
    m_blurUpdate.start();
}

void QuickViewSurfaceFactory::flushBlur()
{
    const QSet<QString> pending = std::exchange(m_blurPending, {});
    for (const QString &outputId : pending) {
        if (QQuickView *view = m_views.value(outputId)) {
            applyBlur(view, outputId);
        }
    }
}

void QuickViewSurfaceFactory::applyBlur(QQuickView *view, const QString &outputId)
{
    const QRect shelf = m_shelfRegions.value(outputId);
    if (shelf.isNull()) {
        return;
    }

    // Explicitly, and per surface. The blur effect matches on window class and
    // type, neither of which a layer surface has, so it does not pick the dock
    // up on its own however dock-like the dock is — see
    // docs/decisions/2026-08-16-blur-route.md. With the effect absent or
    // disabled this is simply ignored and the shelf falls back to flat
    // translucency, which needs no code of its own.
    const qreal tileSize = m_config->tileSize();
    const int radius = qRound(geometry::shelfCornerRadius(
        geometry::shelfThickness(tileSize, tileSize / 3.0)));
    KWindowEffects::enableBlurBehind(view, true, blurRegion(shelf, radius));
    view->requestUpdate();
}

void QuickViewSurfaceFactory::destroySurface(const QString &outputId)
{
    m_shelfRegions.remove(outputId);
    // Or the flush would look up a view that has gone.
    m_blurPending.remove(outputId);
    m_stackRegions.remove(outputId);
    // The model holds a raw pointer into the backend and calls it from the
    // change callback, so it has to go first.
    m_stackModels.erase(outputId);
    m_folderBackends.erase(outputId);

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

void QuickViewSurfaceFactory::setStackRegion(const QString &outputId, const QRect &rect)
{
    if (m_stackRegions.value(outputId) == rect) {
        return;
    }

    if (rect.isNull()) {
        m_stackRegions.remove(outputId);
    } else {
        m_stackRegions.insert(outputId, rect);
    }

    if (QQuickView *view = m_views.value(outputId)) {
        applyMask(view, outputId);
        // Double-buffered, like everything else the compositor is told: without
        // a commit the new region never reaches it and the stack stays dead to
        // the pointer.
        view->requestUpdate();
    }
}

void QuickViewSurfaceFactory::applyMask(QQuickView *view, const QString &outputId)
{
    const qreal tileSize = m_config->tileSize();
    const qreal gap = tileSize / 3.0;
    const int screenGap = qRound(gap / 3.0);
    const int shelfThickness = qRound(geometry::shelfThickness(tileSize, gap));

    QScreen *screen = screenForOutput(outputId);

    // Input stops at the shelf. Without this the surface would swallow clicks
    // across the whole screen edge over a band as tall as the largest possible
    // tile — mostly empty space, and at a high peak most of the band. Nothing is
    // lost by it: hover is driven from the shelf, so the headroom was never
    // interactive.
    // Measured from where the shelf actually is, not from the surface's edge:
    // those were the same thing while the surface was shelf-sized and are not
    // now. The shelf sits screenGap in from the edge — Dock.qml's shelfCross.
    const QRect span = screen ? screen->geometry() : QRect(0, 0, 8192, 8192);
    QRegion region;
    switch (m_config->position()) {
    case ConfigFacade::Left:
        region = QRegion(screenGap, 0, shelfThickness, span.height());
        break;
    case ConfigFacade::Right:
        region = QRegion(span.width() - screenGap - shelfThickness, 0, shelfThickness, span.height());
        break;
    case ConfigFacade::Bottom:
    default:
        region = QRegion(0, span.height() - screenGap - shelfThickness, span.width(), shelfThickness);
        break;
    }

    // An open stack is drawn outside the shelf and has to be clickable, so its
    // rectangle joins the region for as long as it is open — and leaves it
    // again on close, or the dock would keep swallowing clicks over a stack
    // that is no longer there.
    const QRect stack = m_stackRegions.value(outputId);
    if (!stack.isNull()) {
        region += stack;
    }

    view->setMask(region);
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

    // The shelf is what the dock *is* — normative geometry, S + 2g, unaffected
    // by the magnification setting. It is the only thickness this file needs:
    // the surface is the whole output, so what the dock draws outside the shelf
    // no longer has to be predicted here. The zone and the input region are
    // both measured from the shelf.
    const int shelfThickness = qRound(geometry::shelfThickness(tileSize, gap));

    QScreen *screen = screenForOutput(output.id);
    if (screen) {
        view->setScreen(screen);
    }

    Window *layer = Window::get(view);
    layer->setScope(QStringLiteral("frappe-dock"));
    layer->setLayer(Window::LayerTop);
    layer->setKeyboardInteractivity(Window::KeyboardInteractivityOnDemand);

    // Anchored to all four edges, so the surface is the whole output and the
    // dock can draw anywhere on it. A shelf-sized surface clips everything that
    // leaves the shelf — the tile being dragged out to be removed, its Remove
    // label, the drop refusal messages — and no headroom figure fixes that,
    // because the gesture has no fixed extent: the user can drag as far as they
    // like. setExclusiveEdge() says which edge the reserved zone is measured
    // from, and both the zone and the input region stay at the shelf, so
    // occupying the screen is only ever about what may be *drawn*.
    //
    // The screen gap moves into the QML with this: the shelf is placed g/3 in
    // from the edge by Dock.qml rather than by a layer-shell margin, since the
    // surface no longer ends where the shelf does.
    layer->setAnchors(Window::Anchors(Window::AnchorLeft) | Window::AnchorRight
                      | Window::AnchorTop | Window::AnchorBottom);
    layer->setMargins(QMargins(0, 0, 0, 0));
    // 0 on both axes: the compositor sizes it to the output it is anchored across.
    layer->setDesiredSize(QSize(0, 0));

    switch (m_config->position()) {
    case ConfigFacade::Left:
        layer->setExclusiveEdge(Window::AnchorLeft);
        break;
    case ConfigFacade::Right:
        layer->setExclusiveEdge(Window::AnchorRight);
        break;
    case ConfigFacade::Bottom:
    default:
        layer->setExclusiveEdge(Window::AnchorBottom);
        break;
    }

    // The zone is measured from the surface's own edge, which is now the screen
    // edge — there is no margin left for the compositor to add on top, because
    // the gap moved into the QML. So the zone has to cover both: what the dock
    // occupies is the shelf plus the gap it floats above.
    //
    // The *shelf's* thickness, not the surface's: the surface is the whole
    // output now, and reserving that would leave no screen at all. Everything
    // outside the shelf is drawn into but not occupied.
    layer->setExclusiveZone(shelfThickness + screenGap);

    applyMask(view, output.id);
    // The shelf's own rectangle is the view's to report and may not have
    // arrived yet; when it has, this re-applies it against the new size.
    applyBlur(view, output.id);

    // Layer-surface state is double-buffered and latches on the next surface
    // commit. With a static scene Qt renders no new frame, so without this the
    // setters above silently never reach the compositor.
    view->requestUpdate();
}
