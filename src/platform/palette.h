#pragma once

#include <QColor>
#include <QObject>

#include <KConfigWatcher>
#include <KSharedConfig>

namespace frappe
{

/// The dock's colours, derived from the active colour scheme.
///
/// Every colour the dock draws comes from here, and every colour here comes
/// from `KColorScheme`. Nothing in QML writes a colour of its own — that is
/// what lets a colour scheme retheme the dock without the dock knowing which
/// scheme it is running under.
///
/// The roles are named for what they are *for* rather than for what they are
/// made of, and they arrive ready to use, alpha included. A view that had to
/// compose `Qt.alpha(palette.text, 0.3)` at the point of use would be deciding
/// half of its own colour, which is the mistake this class exists to prevent.
///
/// **Colour set.** `Complementary`, not `Window`. The dock is a translucent
/// surface floating over the wallpaper rather than a widget inside an
/// application window, and `Complementary` is the set a scheme provides for
/// exactly that — legible over arbitrary content, which the window set is not
/// required to be.
///
/// Alpha values are not scheme data and cannot be: a scheme describes opaque
/// colours. They are chrome, which §6.4 declares non-normative, and they are
/// documented at their definitions in the implementation.
class DockPalette : public QObject
{
    Q_OBJECT

    /// The shelf's fill and its rim stroke.
    Q_PROPERTY(QColor shelf READ shelf NOTIFY changed)
    Q_PROPERTY(QColor rim READ rim NOTIFY changed)
    /// The rim while a drop over the shelf would be taken, and while it
    /// would be refused.
    Q_PROPERTY(QColor rimAccepted READ rimAccepted NOTIFY changed)
    Q_PROPERTY(QColor rimRejected READ rimRejected NOTIFY changed)

    /// The fill behind a label that has to be readable over anything — a
    /// refusal message, the Remove affordance. Denser than the shelf: it
    /// carries words, and it is not always over the shelf's own backdrop.
    Q_PROPERTY(QColor plate READ plate NOTIFY changed)
    /// Text on the shelf or on a plate.
    Q_PROPERTY(QColor text READ text NOTIFY changed)

    /// The separator rule, and the border of anything as quiet as it.
    Q_PROPERTY(QColor separator READ separator NOTIFY changed)
    /// The running-window dot.
    Q_PROPERTY(QColor indicator READ indicator NOTIFY changed)

    /// The tint behind a tile that would take the drop, and one that would
    /// refuse it.
    Q_PROPERTY(QColor dropAccepted READ dropAccepted NOTIFY changed)
    Q_PROPERTY(QColor dropRejected READ dropRejected NOTIFY changed)

    /// The scheme's accent. The spring-load ring is drawn in it, and it is
    /// also what the Tinted appearance mode tints with (§6.3), so that mode
    /// needs no colour of its own in the config schema.
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)

public:
    /// Reads the user's colour scheme.
    explicit DockPalette(QObject *parent = nullptr);
    /// Reads \a config instead. The seam a test uses to run against a fixture
    /// scheme without touching the user's.
    explicit DockPalette(const KSharedConfig::Ptr &config, QObject *parent = nullptr);
    ~DockPalette() override;

    /// The process-wide instance. QML binds to it as a singleton.
    static DockPalette *instance();

    QColor shelf() const;
    QColor rim() const;
    QColor rimAccepted() const;
    QColor rimRejected() const;
    QColor plate() const;
    QColor text() const;
    QColor separator() const;
    QColor indicator() const;
    QColor dropAccepted() const;
    QColor dropRejected() const;
    QColor accent() const;

    /// Re-reads the scheme and emits changed() if anything moved.
    ///
    /// Called for us on the two events that mean the scheme changed — the
    /// config file being rewritten and the application palette being replaced
    /// — and public because a test has neither.
    Q_INVOKABLE void reload();

Q_SIGNALS:
    /// One signal for the whole palette: a scheme change moves every role at
    /// once, and a per-role signal would only invite a view to bind to some of
    /// them and go stale on the rest.
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Every role, together, so that reload() can ask whether *anything*
    /// changed rather than picking a few roles and hoping they stand for the
    /// rest.
    struct Colours {
        QColor shelf;
        QColor rim;
        QColor rimAccepted;
        QColor rimRejected;
        QColor plate;
        QColor text;
        QColor separator;
        QColor indicator;
        QColor dropAccepted;
        QColor dropRejected;
        QColor accent;

        bool operator==(const Colours &) const = default;
    };

    Colours read() const;

    KSharedConfig::Ptr m_config;
    KConfigWatcher::Ptr m_watcher;
    Colours m_colours;
};

}
