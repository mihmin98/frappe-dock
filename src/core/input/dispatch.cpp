#include "core/input/dispatch.h"

#include <algorithm>

using namespace frappe::input;

namespace
{
/// The modifiers the matrix distinguishes. Anything else the platform reports —
/// keypad state, group switches — is masked off before matching, so an active
/// NumLock cannot turn a known combination into an unknown one.
constexpr Qt::KeyboardModifiers relevantModifiers =
    Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
}

const std::vector<Binding> &frappe::input::bindings()
{
    // Function-local static: built once, on first use, with no static
    // initialisation order to reason about.
    static const std::vector<Binding> table = {
        {Qt::LeftButton, Qt::NoModifier, Trigger::Click, Command::LaunchOrActivate,
         QStringLiteral("Launch the application, or activate and raise it if it is already running")},

        {Qt::LeftButton, Qt::MetaModifier, Trigger::Click, Command::RevealInFileManager,
         QStringLiteral("Show the application in the file manager")},

        {Qt::LeftButton, Qt::AltModifier, Trigger::Click, Command::ActivateAndHidePrevious,
         QStringLiteral("Activate the application and hide the one that was active before it")},

        {Qt::LeftButton, Qt::MetaModifier | Qt::AltModifier, Trigger::Click, Command::ActivateAndHideOthers,
         QStringLiteral("Activate the application and hide every other one")},

        {Qt::MiddleButton, Qt::NoModifier, Trigger::Click, Command::NewInstance,
         QStringLiteral("Open a new instance of the application")},

        {Qt::RightButton, Qt::NoModifier, Trigger::Click, Command::ShowContextMenu,
         QStringLiteral("Show the context menu")},

        {Qt::LeftButton, Qt::NoModifier, Trigger::PressAndHold, Command::ShowJumpList,
         QStringLiteral("Press and hold to show the application's own actions")},
    };
    return table;
}

Command frappe::input::dispatch(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, Trigger trigger)
{
    const Qt::KeyboardModifiers wanted = modifiers & relevantModifiers;

    const auto &table = bindings();
    const auto it = std::find_if(table.begin(), table.end(), [&](const Binding &binding) {
        return binding.button == button && binding.modifiers == wanted && binding.trigger == trigger;
    });

    return it == table.end() ? Command::LaunchOrActivate : it->command;
}
