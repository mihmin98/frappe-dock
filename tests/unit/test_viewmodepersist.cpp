#include "core/model/contextmenu.h"
#include "core/model/stacksettings.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

/// Per-folder view mode: what it defaults to, that it survives a reload, and
/// that the menu offering it says which one is in force.
class TestViewModePersist : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    /// One file per test function. Per-folder preferences are keyed by path and
    /// so do not collide, but the *default* sort order is global to the file:
    /// one test setting it would otherwise decide what the next one reads.
    QString configPath() const
    {
        return m_dir.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral("-dockrc"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void defaultModeIsGrid()
    {
        StackSettings settings(configPath());

        // Grid is the default deliberately: it is the mode that stays usable at
        // a few hundred entries, and the affordance the reference platform
        // removed. A folder nobody has expressed an opinion about gets it.
        QCOMPARE(settings.viewMode(QStringLiteral("/never/configured")), int(StackSettings::Grid));
        QCOMPARE(settings.viewMode(QString()), int(StackSettings::Grid));
    }

    void modePersistsPerFolder()
    {
        const QString one = QStringLiteral("/stack/One");
        const QString two = QStringLiteral("/stack/Two");

        {
            StackSettings settings(configPath());
            settings.setViewMode(one, StackSettings::Fan);
            settings.setViewMode(two, StackSettings::List);

            // Per folder, not global: setting one must not move the other.
            QCOMPARE(settings.viewMode(one), int(StackSettings::Fan));
            QCOMPARE(settings.viewMode(two), int(StackSettings::List));
            QCOMPARE(settings.viewMode(QStringLiteral("/stack/Three")), int(StackSettings::Grid));
        }

        // A second instance over the same file: this is the part that makes it
        // persistence rather than a member variable.
        StackSettings reloaded(configPath());
        QCOMPARE(reloaded.viewMode(one), int(StackSettings::Fan));
        QCOMPARE(reloaded.viewMode(two), int(StackSettings::List));
    }

    void pathsAreNormalisedBeforeStorage()
    {
        StackSettings settings(configPath());
        settings.setViewMode(QStringLiteral("/stack/Norm"), StackSettings::List);

        // The same folder named awkwardly is the same folder. Without this, a
        // tile whose path gained a redundant hop would forget its mode.
        QCOMPARE(settings.viewMode(QStringLiteral("/stack/./Norm")), int(StackSettings::List));
        QCOMPARE(settings.viewMode(QStringLiteral("/stack/Other/../Norm")), int(StackSettings::List));
    }

    void changingModeNotifies()
    {
        StackSettings settings(configPath());
        QSignalSpy spy(&settings, &StackSettings::viewModeChanged);

        settings.setViewMode(QStringLiteral("/stack/Notify"), StackSettings::Fan);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QStringLiteral("/stack/Notify"));

        // Setting the mode it already has is not a change.
        settings.setViewMode(QStringLiteral("/stack/Notify"), StackSettings::Fan);
        QCOMPARE(spy.count(), 1);
    }

    void outOfRangeModesAreRefused()
    {
        StackSettings settings(configPath());
        settings.setViewMode(QStringLiteral("/stack/Range"), StackSettings::List);

        // A value from a later version, or a hand-edited file. Refusing to store
        // it beats storing a mode nothing can render.
        settings.setViewMode(QStringLiteral("/stack/Range"), 99);
        settings.setViewMode(QStringLiteral("/stack/Range"), -1);
        QCOMPARE(settings.viewMode(QStringLiteral("/stack/Range")), int(StackSettings::List));
    }

    void forgettingRestoresTheDefault()
    {
        const QString folder = QStringLiteral("/stack/Forget");

        StackSettings settings(configPath());
        settings.setViewMode(folder, StackSettings::Fan);
        QCOMPARE(settings.viewMode(folder), int(StackSettings::Fan));

        settings.forget(folder);
        QCOMPARE(settings.viewMode(folder), int(StackSettings::Grid));

        // And it stays forgotten, rather than being written back as Grid.
        StackSettings reloaded(configPath());
        QCOMPARE(reloaded.viewMode(folder), int(StackSettings::Grid));
    }

    // --- Sort order, persisted the same way --------------------------------

    void sortOrderPersistsPerFolder()
    {
        const QString one = QStringLiteral("/stack/SortOne");
        const QString two = QStringLiteral("/stack/SortTwo");

        {
            StackSettings settings(configPath());
            settings.setSortOrder(one, StackSettings::Kind);
            settings.setSortOrder(two, StackSettings::DateModified);

            QCOMPARE(settings.sortOrder(one), int(StackSettings::Kind));
            QCOMPARE(settings.sortOrder(two), int(StackSettings::DateModified));
            // And it is a different key from the view mode, not the same one.
            QCOMPARE(settings.viewMode(one), int(StackSettings::Grid));
        }

        StackSettings reloaded(configPath());
        QCOMPARE(reloaded.sortOrder(one), int(StackSettings::Kind));
        QCOMPARE(reloaded.sortOrder(two), int(StackSettings::DateModified));
    }

    void defaultSortOrderIsNameAndIsSettable()
    {
        StackSettings settings(configPath());
        QCOMPARE(settings.defaultSortOrder(), int(StackSettings::Name));
        // A folder with no preference of its own follows the default.
        QCOMPARE(settings.sortOrder(QStringLiteral("/stack/Unconfigured")), int(StackSettings::Name));

        // This is the row the settings page writes (7.6).
        settings.setDefaultSortOrder(StackSettings::DateModified);
        QCOMPARE(settings.defaultSortOrder(), int(StackSettings::DateModified));
        QCOMPARE(settings.sortOrder(QStringLiteral("/stack/Unconfigured")), int(StackSettings::DateModified));
    }

    /// Regression: choosing the order that happened to match the current default
    /// wrote nothing, leaving the folder following the default — so moving the
    /// default later silently moved a folder the user had explicitly set.
    void anExplicitChoiceSurvivesTheDefaultMoving()
    {
        const QString folder = QStringLiteral("/stack/Explicit");

        StackSettings settings(configPath());
        QCOMPARE(settings.defaultSortOrder(), int(StackSettings::Name));

        // Chosen deliberately, and equal to today's default.
        settings.setSortOrder(folder, StackSettings::Name);

        settings.setDefaultSortOrder(StackSettings::Kind);
        QCOMPARE(settings.sortOrder(folder), int(StackSettings::Name));
        QCOMPARE(settings.sortOrder(QStringLiteral("/stack/Untouched")), int(StackSettings::Kind));
    }

    void defaultSortChangeNotifiesWithoutNamingAFolder()
    {
        StackSettings settings(configPath());
        QSignalSpy spy(&settings, &StackSettings::sortOrderChanged);

        settings.setDefaultSortOrder(StackSettings::Kind);
        QCOMPARE(spy.count(), 1);
        // Empty: this moved every folder with no preference of its own, and
        // naming one of them would be naming the wrong thing.
        QVERIFY(spy.first().at(0).toString().isEmpty());
    }

    void outOfRangeSortOrdersAreRefused()
    {
        StackSettings settings(configPath());
        settings.setSortOrder(QStringLiteral("/stack/SortRange"), StackSettings::Kind);

        settings.setSortOrder(QStringLiteral("/stack/SortRange"), 99);
        settings.setSortOrder(QStringLiteral("/stack/SortRange"), -1);
        QCOMPARE(settings.sortOrder(QStringLiteral("/stack/SortRange")), int(StackSettings::Kind));

        settings.setDefaultSortOrder(99);
        QCOMPARE(settings.defaultSortOrder(), int(StackSettings::Name));
    }

    void forgettingClearsBothPreferences()
    {
        const QString folder = QStringLiteral("/stack/ForgetBoth");

        StackSettings settings(configPath());
        settings.setViewMode(folder, StackSettings::List);
        settings.setSortOrder(folder, StackSettings::Kind);

        settings.forget(folder);
        QCOMPARE(settings.viewMode(folder), int(StackSettings::Grid));
        QCOMPARE(settings.sortOrder(folder), settings.defaultSortOrder());
    }

    // --- The menu that offers the modes -----------------------------------

    void stackMenuOffersEveryModeAndChecksTheCurrentOne()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");
        context.folderName = QStringLiteral("Menu");
        context.viewMode = int(StackViewMode::Fan);

        const std::vector<MenuItem> rows = buildStackMenu(context);

        QStringList viewLabels;
        QStringList checkedLabels;
        for (const MenuItem &row : rows) {
            if (row.kind != MenuItemKind::StackView) {
                continue;
            }
            QVERIFY(row.checkable);
            viewLabels.append(row.label);
            if (row.checked) {
                checkedLabels.append(row.label);
            }
        }

        // All three offered, so the menu says what the alternatives are rather
        // than cycling through them one click at a time.
        QCOMPARE(viewLabels, QStringList({QStringLiteral("Grid"), QStringLiteral("Fan"), QStringLiteral("List")}));
        QCOMPARE(checkedLabels, QStringList({QStringLiteral("Fan")}));
    }

    void stackMenuCarriesTheModeAsItsItemId()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");
        context.viewMode = int(StackViewMode::Grid);

        // One kind for all three rows, distinguished by id, so the set of modes
        // can grow without the controller's switch learning about it.
        for (const MenuItem &row : buildStackMenu(context)) {
            if (row.kind != MenuItemKind::StackView) {
                continue;
            }
            bool ok = false;
            const int mode = row.id.toInt(&ok);
            QVERIFY2(ok, qPrintable(QStringLiteral("row '%1' has a non-numeric id").arg(row.label)));
            QVERIFY(mode >= int(StackViewMode::Grid) && mode <= int(StackViewMode::List));
        }
    }

    void stackMenuOffersEverySortOrderAndChecksTheCurrentOne()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");
        context.viewMode = int(StackViewMode::Grid);
        context.sortOrder = int(StackSortOrder::DateCreated);

        QStringList checked;
        int offered = 0;
        for (const MenuItem &row : buildStackMenu(context)) {
            if (row.kind != MenuItemKind::StackSort) {
                continue;
            }
            QVERIFY(row.checkable);
            ++offered;
            bool ok = false;
            const int order = row.id.toInt(&ok);
            QVERIFY(ok);
            QVERIFY(order >= int(StackSortOrder::Name) && order <= int(StackSortOrder::Kind));
            if (row.checked) {
                checked.append(row.label);
            }
        }

        QCOMPARE(offered, 5);
        QCOMPARE(checked, QStringList({QStringLiteral("Sort by Date Created")}));
    }

    void stackMenuSeparatesTheModesFromTheOrders()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");
        const std::vector<MenuItem> rows = buildStackMenu(context);

        // Five rows beside three would read as one list of eight unrelated
        // choices without a rule between them.
        int lastView = -1;
        int firstSort = -1;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].kind == MenuItemKind::StackView) {
                lastView = int(i);
            }
            if (rows[i].kind == MenuItemKind::StackSort && firstSort < 0) {
                firstSort = int(i);
            }
        }
        QVERIFY(lastView >= 0);
        QVERIFY(firstSort > lastView);
        QCOMPARE(firstSort - lastView, 2);
        QCOMPARE(rows[lastView + 1].kind, MenuItemKind::Separator);
    }

    void stackMenuOffersRemovalButNotApplicationRows()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");

        bool hasRemove = false;
        for (const MenuItem &row : buildStackMenu(context)) {
            // Pin, quit and open-at-login are about an application. A folder is
            // not one, and offering them would be offering nothing.
            QVERIFY(row.kind != MenuItemKind::Pin);
            QVERIFY(row.kind != MenuItemKind::Unpin);
            QVERIFY(row.kind != MenuItemKind::Quit);
            QVERIFY(row.kind != MenuItemKind::LaunchAtLogin);
            if (row.kind == MenuItemKind::RemoveFolder) {
                hasRemove = true;
            }
        }
        QVERIFY(hasRemove);
    }

    void stackMenuHasNoLeadingOrDoubledSeparators()
    {
        StackMenuContext context;
        context.folderPath = QStringLiteral("/stack/Menu");
        const std::vector<MenuItem> rows = buildStackMenu(context);

        QVERIFY(!rows.empty());
        QVERIFY(rows.front().kind != MenuItemKind::Separator);
        QVERIFY(rows.back().kind != MenuItemKind::Separator);
        for (std::size_t i = 1; i < rows.size(); ++i) {
            QVERIFY(!(rows[i].kind == MenuItemKind::Separator && rows[i - 1].kind == MenuItemKind::Separator));
        }
    }
};

QTEST_MAIN(TestViewModePersist)
#include "test_viewmodepersist.moc"
