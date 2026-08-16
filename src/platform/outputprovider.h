#pragma once

#include <QObject>

#include "core/interfaces/ioutputprovider.h"

QT_BEGIN_NAMESPACE
class QScreen;
QT_END_NAMESPACE

namespace frappe
{

/// IOutputProvider over QGuiApplication::screens().
///
/// QObject only so it can hold connections to the screen-change signals; the
/// interface core sees stays free of Qt's meta-object system.
class OutputProvider : public QObject, public IOutputProvider
{
    Q_OBJECT

public:
    explicit OutputProvider(QObject *parent = nullptr);

    std::vector<OutputInfo> outputs() const override;
    /// The output under the pointer — see
    /// docs/decisions/2026-08-16-followactive-definition.md. Falls back to the
    /// primary output when the pointer is not over any known screen.
    OutputInfo activeOutput() const override;
    void setChangeCallback(std::function<void()> cb) override;

private:
    void watchScreen(QScreen *screen);
    void notifyChanged();

    std::function<void()> m_callback;
};

}
