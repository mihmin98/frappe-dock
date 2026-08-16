#pragma once

#include <QString>
#include <Qt>

#include <vector>

namespace frappe::input
{

/// What a tile interaction asks the dock to do. The controller routes these;
/// nothing here knows how any of them is carried out.
enum class Command {
    LaunchOrActivate,
    RevealInFileManager,
    ActivateAndHidePrevious,
    ActivateAndHideOthers,
    NewInstance,
    ShowContextMenu,
    ShowJumpList,
};

/// How the pointer produced the interaction. Press-and-hold is not a modifier
/// combination, but it belongs in the same table: it is one more row of the
/// interaction vocabulary and the settings page has to list it alongside the
/// rest.
enum class Trigger {
    Click,
    PressAndHold,
};

struct Binding {
    Qt::MouseButton button;
    Qt::KeyboardModifiers modifiers;
    Trigger trigger;
    Command command;
    /// Rendered on the settings shortcuts page, which is the reason it lives on
    /// the binding rather than in a parallel table that could drift from it.
    QString description;
};

/// The interaction matrix, and the only place it exists.
///
/// The dispatcher reads it and the settings page renders it, so documentation
/// cannot disagree with behaviour.
const std::vector<Binding> &bindings();

/// The command for an interaction. Unrecognised combinations fall back to
/// LaunchOrActivate: a click on a dock tile that does nothing at all reads as a
/// broken dock, and clicking to launch is the least surprising thing to do
/// instead.
Command dispatch(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, Trigger trigger = Trigger::Click);

}
