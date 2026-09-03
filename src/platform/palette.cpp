#include "platform/palette.h"

#include <KColorScheme>

#include <QCoreApplication>
#include <QEvent>

using namespace frappe;

namespace
{
/*
 * The alphas.
 *
 * A colour scheme describes opaque colours; translucency is the dock's own
 * chrome, which §6.4 declares non-normative. These are collected here rather
 * than scattered through QML so that the roles arrive ready to use and no view
 * ends up composing half of its own colour.
 *
 * The shelf's figure is the one with any evidence behind it: the reference
 * capture solves to roughly 45–50 % over a heavily blurred backdrop (task
 * 6.4.1). It is a sanity bound, not a target.
 */
constexpr qreal shelfAlpha = 0.45;
constexpr qreal rimAlpha = 0.20;
/// A rim answering a drag says so plainly; the resting rim does not compete.
constexpr qreal activeRimAlpha = 0.85;
/// Denser than the shelf: a plate carries words and does not always have the
/// shelf's backdrop underneath it.
constexpr qreal plateAlpha = 0.80;
constexpr qreal separatorAlpha = 0.30;
constexpr qreal indicatorAlpha = 0.85;
/// A tint, not a fill — the artwork it sits behind has to stay readable.
constexpr qreal dropTintAlpha = 0.25;

QColor withAlpha(const QColor &colour, qreal alpha)
{
    QColor result = colour;
    result.setAlphaF(alpha);
    return result;
}
}

DockPalette::DockPalette(QObject *parent)
    : DockPalette(KSharedConfig::openConfig(), parent)
{
}

DockPalette::DockPalette(const KSharedConfig::Ptr &config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_watcher(KConfigWatcher::create(config))
{
    m_colours = read();

    // Two routes, because neither covers the other. The watcher hears the
    // scheme file being rewritten, which is what a scheme switch does; the
    // palette-change event is what the platform theme raises once it has
    // noticed, and it also fires for a light/dark flip that leaves the file
    // alone. Both land on reload(), which is idempotent.
    connect(m_watcher.get(), &KConfigWatcher::configChanged, this,
            [this](const KConfigGroup &group, const QByteArrayList &) {
                if (group.name().startsWith(QLatin1String("Colors:"))) {
                    reload();
                }
            });

    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
    }
}

DockPalette::~DockPalette() = default;

DockPalette *DockPalette::instance()
{
    static DockPalette s_instance;
    return &s_instance;
}

bool DockPalette::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ApplicationPaletteChange) {
        reload();
    }
    return QObject::eventFilter(watched, event);
}

DockPalette::Colours DockPalette::read() const
{
    // Complementary, not Window: see the class comment. `Active` rather than
    // `Normal` because the dock is never the focused window and must not fade
    // to an inactive treatment on that account.
    const KColorScheme scheme(QPalette::Active, KColorScheme::Complementary, m_config);

    const QColor background = scheme.background(KColorScheme::NormalBackground).color();
    const QColor foreground = scheme.foreground(KColorScheme::NormalText).color();
    const QColor positive = scheme.foreground(KColorScheme::PositiveText).color();
    const QColor negative = scheme.foreground(KColorScheme::NegativeText).color();

    return Colours{
        .shelf = withAlpha(background, shelfAlpha),
        .rim = withAlpha(foreground, rimAlpha),
        .rimAccepted = withAlpha(positive, activeRimAlpha),
        .rimRejected = withAlpha(negative, activeRimAlpha),
        .plate = withAlpha(background, plateAlpha),
        .text = foreground,
        .separator = withAlpha(foreground, separatorAlpha),
        .indicator = withAlpha(foreground, indicatorAlpha),
        .dropAccepted = withAlpha(positive, dropTintAlpha),
        .dropRejected = withAlpha(negative, dropTintAlpha),
        .accent = scheme.decoration(KColorScheme::FocusColor).color(),
    };
}

void DockPalette::reload()
{
    // The scheme lives in a file this process did not write, so the cached copy
    // has to be dropped before the new colours can be seen at all.
    m_config->reparseConfiguration();

    const Colours fresh = read();
    if (fresh == m_colours) {
        return;
    }
    m_colours = fresh;
    Q_EMIT changed();
}

QColor DockPalette::shelf() const
{
    return m_colours.shelf;
}

QColor DockPalette::rim() const
{
    return m_colours.rim;
}

QColor DockPalette::rimAccepted() const
{
    return m_colours.rimAccepted;
}

QColor DockPalette::rimRejected() const
{
    return m_colours.rimRejected;
}

QColor DockPalette::plate() const
{
    return m_colours.plate;
}

QColor DockPalette::text() const
{
    return m_colours.text;
}

QColor DockPalette::separator() const
{
    return m_colours.separator;
}

QColor DockPalette::indicator() const
{
    return m_colours.indicator;
}

QColor DockPalette::dropAccepted() const
{
    return m_colours.dropAccepted;
}

QColor DockPalette::dropRejected() const
{
    return m_colours.dropRejected;
}

QColor DockPalette::accent() const
{
    return m_colours.accent;
}
