#pragma once

#include <QObject>

namespace frappe
{

/// The one geometry knob that is deliberately *not* a setting.
///
/// The inter-icon gap is S/3 and the shelf's inner padding is the same number —
/// one constant, from the proportion model, and exposing it as a config key
/// would mean the model had been abandoned (see CLAUDE.md). But the tuning
/// harness has to be able to ask whether 1/3 is the right number, and a
/// question cannot be asked of a literal. So it lives here: process-wide,
/// writable, never persisted, and left at its default by everything except the
/// harness.
class Tuning : public QObject
{
    Q_OBJECT

    /// The inter-icon gap as a fraction of the tile edge.
    Q_PROPERTY(qreal spacingRatio READ spacingRatio WRITE setSpacingRatio NOTIFY changed)

public:
    explicit Tuning(QObject *parent = nullptr);

    /// The process-wide instance. QML binds to it as a singleton.
    static Tuning *instance();

    qreal spacingRatio() const;
    void setSpacingRatio(qreal value);

    /// Back to the proportion model's own value, for the harness's reset.
    Q_INVOKABLE void reset();

Q_SIGNALS:
    void changed();

private:
    qreal m_spacingRatio = 1.0 / 3.0;
};

}
