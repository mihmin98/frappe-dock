#pragma once

#include <QRect>
#include <QString>

#include <functional>
#include <vector>

namespace frappe
{

struct OutputInfo {
    QString id;
    QRect geometry;
    qreal scale = 1.0;
    bool isPrimary = false;

    bool operator==(const OutputInfo &) const = default;
};

/// The set of connected outputs, and which one is currently active.
///
/// "Active" means the output under the pointer — see
/// docs/decisions/2026-08-16-followactive-definition.md.
class IOutputProvider
{
public:
    virtual ~IOutputProvider() = default;

    virtual std::vector<OutputInfo> outputs() const = 0;
    /// The active output. Falls back to the primary output, then to a default-constructed
    /// OutputInfo when nothing is connected.
    virtual OutputInfo activeOutput() const = 0;
    /// Invoked whenever the output set, their geometry, or the active output changes.
    virtual void setChangeCallback(std::function<void()> cb) = 0;
};

}
