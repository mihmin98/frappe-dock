#include "app/tuning.h"

#include <QtGlobal>

using namespace frappe;

Tuning::Tuning(QObject *parent)
    : QObject(parent)
{
}

Tuning *Tuning::instance()
{
    static Tuning s_instance;
    return &s_instance;
}

qreal Tuning::spacingRatio() const
{
    return m_spacingRatio;
}

void Tuning::setSpacingRatio(qreal value)
{
    // A non-positive gap is not a layout, it is overlapping tiles.
    if (value <= 0.0 || qFuzzyCompare(m_spacingRatio, value)) {
        return;
    }
    m_spacingRatio = value;
    Q_EMIT changed();
}

void Tuning::reset()
{
    setSpacingRatio(1.0 / 3.0);
}
