#pragma once

#include <QObject>
#include <QString>

namespace frappe
{

/// How a stack draws its contents.
enum class StackViewMode {
    Grid,
    Fan,
    List,
};

/// The order a stack lists its contents in. Every order breaks ties by name, so
/// each of these is a total order and a folder never reshuffles between reads.
enum class StackSortOrder {
    Name,
    /// When the entry appeared in this folder. See the decision record: no Linux
    /// filesystem records this, and ctime is the closest honest answer.
    DateAdded,
    DateModified,
    DateCreated,
    /// Folders first, then by type. What "kind" means to someone looking for
    /// the images among the documents.
    Kind,
};

/// Per-folder stack preferences, persisted.
///
/// A separate class from ConfigFacade because the shape of the data is
/// different: the settings keys are a fixed schema KConfigXT can generate, and
/// this is a map from an unbounded set of folder paths to a value. KConfigXT has
/// no such entry type, so this writes the group itself — one key per folder, in
/// the same `frappe-dockrc` file so there is still only one thing to back up.
///
/// Folders the user has never expressed an opinion about are simply absent, and
/// answer with the default. That is what keeps the file from growing a line for
/// every folder ever opened.
class StackSettings : public QObject
{
    Q_OBJECT

public:
    /// Mirrors StackViewMode, so QML need not include this twice.
    enum ViewMode { Grid, Fan, List };
    Q_ENUM(ViewMode)

    /// Mirrors StackSortOrder, for the same reason.
    enum SortOrder { Name, DateAdded, DateModified, DateCreated, Kind };
    Q_ENUM(SortOrder)

    explicit StackSettings(QObject *parent = nullptr);

    /// Reads and writes \a configPath instead of the user's settings. For tests.
    explicit StackSettings(const QString &configPath, QObject *parent = nullptr);

    ~StackSettings() override;

    /// The process-wide instance QML and the controller bind to.
    static StackSettings *instance();

    /// Points an existing instance at \a configPath and rereads. Intended for
    /// tests, which otherwise share one persistent file and leak state from one
    /// *run* to the next — the same reason ConfigFacade has redirectTo().
    void redirectTo(const QString &configPath);

    /// The view mode for \a folderPath, or Grid where none was ever chosen.
    ///
    /// Grid is the default deliberately: it is the mode that stays usable at a
    /// few hundred entries, and it is the affordance the reference platform
    /// removed, which is most of the reason this project exists.
    Q_INVOKABLE int viewMode(const QString &folderPath) const;

    /// Records the view mode for \a folderPath. Written through immediately:
    /// choosing it from a menu is direct manipulation, and the gesture is the
    /// only save point there is.
    Q_INVOKABLE void setViewMode(const QString &folderPath, int mode);

    /// The sort order for \a folderPath, or defaultSortOrder() where none was
    /// ever chosen for it.
    Q_INVOKABLE int sortOrder(const QString &folderPath) const;

    /// Records the sort order for \a folderPath.
    Q_INVOKABLE void setSortOrder(const QString &folderPath, int order);

    /// What a folder with no preference of its own sorts by. This is the row
    /// the settings page writes (§7.6); the per-folder values above are what
    /// the context menu writes.
    Q_INVOKABLE int defaultSortOrder() const;
    Q_INVOKABLE void setDefaultSortOrder(int order);

    /// Forgets any preference for \a folderPath, so it answers with the default
    /// again. What removing a folder tile should do — a stale entry for a folder
    /// no longer on the dock is a line in the config file with nothing to apply
    /// it to.
    Q_INVOKABLE void forget(const QString &folderPath);

Q_SIGNALS:
    /// Emitted when any folder's mode changes, carrying the folder.
    void viewModeChanged(const QString &folderPath);
    /// Emitted when any folder's sort order changes, carrying the folder. The
    /// folder is empty when it was the default that moved, which affects every
    /// folder that has no preference of its own.
    void sortOrderChanged(const QString &folderPath);

private:
    class Private;
    Private *d;
};

}
