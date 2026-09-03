#include "core/model/stackmodel.h"
#include "fakes/fakefolderbackend.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTest>

using namespace frappe;

namespace
{

/// A fixed instant to offset every fixture timestamp from, so the expected
/// orders are the same on every machine and in every timezone.
QDateTime epoch()
{
    return QDateTime(QDate(2026, 1, 1), QTime(12, 0), QTimeZone::UTC);
}

QDateTime at(int minutes)
{
    return epoch().addSecs(minutes * 60);
}

FolderEntry file(const QString &name,
                 const QString &mimeType = QStringLiteral("text/plain"),
                 int added = 0,
                 int modified = 0,
                 int created = 0)
{
    FolderEntry entry;
    entry.name = name;
    entry.path = QStringLiteral("/stack/") + name;
    entry.mimeType = mimeType;
    entry.dateAdded = at(added);
    entry.dateModified = at(modified);
    entry.dateCreated = at(created);
    return entry;
}

FolderEntry dir(const QString &name, int added = 0)
{
    FolderEntry entry = file(name, QStringLiteral("inode/directory"), added, added, added);
    entry.isDir = true;
    return entry;
}

}

/// The five sort orders, and the tie-breaking rule they all share.
///
/// Driven through FakeFolderBackend rather than files on disk. The task called
/// for committed fixtures with controlled timestamps, but git does not preserve
/// mtime — a checkout stamps every file with the time of the checkout — so a
/// committed fixture cannot carry a timestamp to sort by. Setting the dates on
/// the entries directly is both more controlled and readable in the test.
class TestSorting : public QObject
{
    Q_OBJECT

private:
    FakeFolderBackend m_backend;

    StackModel *makeModel(std::vector<FolderEntry> entries, StackSortOrder order)
    {
        m_backend.watch(QStringLiteral("/stack"));
        m_backend.setEntries(std::move(entries));

        auto *model = new StackModel(&m_backend, this);
        model->setSortOrder(static_cast<int>(order));
        m_backend.notifyChange();
        return model;
    }

    static QStringList namesOf(const StackModel *model)
    {
        QStringList names;
        for (int row = 0; row < model->rowCount(); ++row) {
            names.append(model->entryAt(row).name);
        }
        return names;
    }

private Q_SLOTS:
    void nameSortCorrect()
    {
        StackModel *model = makeModel({file(QStringLiteral("delta.txt")),
                                       file(QStringLiteral("Alpha.txt")),
                                       file(QStringLiteral("charlie.txt")),
                                       file(QStringLiteral("bravo.txt"))},
                                      StackSortOrder::Name);

        // Case-insensitive: "Alpha" belongs with the a's, not before every
        // lowercase name, which is what a byte comparison would do.
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("Alpha.txt"), QStringLiteral("bravo.txt"),
                              QStringLiteral("charlie.txt"), QStringLiteral("delta.txt")}));
    }

    void nameSortIsNumericWhereNamesAreNumbered()
    {
        StackModel *model = makeModel({file(QStringLiteral("shot-10.png")),
                                       file(QStringLiteral("shot-9.png")),
                                       file(QStringLiteral("shot-100.png")),
                                       file(QStringLiteral("shot-1.png"))},
                                      StackSortOrder::Name);

        // A folder of numbered screenshots is the case this is for.
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("shot-1.png"), QStringLiteral("shot-9.png"),
                              QStringLiteral("shot-10.png"), QStringLiteral("shot-100.png")}));
    }

    void dateAddedSortCorrect()
    {
        StackModel *model = makeModel(
            {file(QStringLiteral("oldest.txt"), QStringLiteral("text/plain"), 0, 500, 500),
             file(QStringLiteral("newest.txt"), QStringLiteral("text/plain"), 300, 0, 0),
             file(QStringLiteral("middle.txt"), QStringLiteral("text/plain"), 100, 200, 200)},
            StackSortOrder::DateAdded);

        // Newest first: the question a date order is asked is "what turned up
        // recently", not "what is oldest". The other two dates are set to
        // contradict this order, so a comparator reading the wrong field fails.
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("newest.txt"), QStringLiteral("middle.txt"),
                              QStringLiteral("oldest.txt")}));
    }

    void dateModifiedSortCorrect()
    {
        StackModel *model = makeModel(
            {file(QStringLiteral("stale.txt"), QStringLiteral("text/plain"), 900, 0, 900),
             file(QStringLiteral("fresh.txt"), QStringLiteral("text/plain"), 0, 400, 0),
             file(QStringLiteral("middling.txt"), QStringLiteral("text/plain"), 500, 200, 500)},
            StackSortOrder::DateModified);

        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("fresh.txt"), QStringLiteral("middling.txt"),
                              QStringLiteral("stale.txt")}));
    }

    void dateCreatedSortCorrect()
    {
        StackModel *model = makeModel(
            {file(QStringLiteral("ancient.txt"), QStringLiteral("text/plain"), 900, 900, 0),
             file(QStringLiteral("recent.txt"), QStringLiteral("text/plain"), 0, 0, 600),
             file(QStringLiteral("between.txt"), QStringLiteral("text/plain"), 400, 400, 300)},
            StackSortOrder::DateCreated);

        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("recent.txt"), QStringLiteral("between.txt"),
                              QStringLiteral("ancient.txt")}));
    }

    void invalidDatesSortLast()
    {
        FolderEntry undated = file(QStringLiteral("undated.txt"));
        undated.dateCreated = QDateTime();

        StackModel *model = makeModel({undated,
                                       file(QStringLiteral("dated.txt"), QStringLiteral("text/plain"), 0, 0, 100)},
                                      StackSortOrder::DateCreated);

        // A filesystem that records no birth time would otherwise shuffle the
        // whole folder, since an invalid QDateTime compares less than every
        // valid one.
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("dated.txt"), QStringLiteral("undated.txt")}));
    }

    void kindSortCorrect()
    {
        StackModel *model = makeModel({file(QStringLiteral("notes.txt"), QStringLiteral("text/plain")),
                                       file(QStringLiteral("photo.png"), QStringLiteral("image/png")),
                                       dir(QStringLiteral("zebra")),
                                       file(QStringLiteral("paper.pdf"), QStringLiteral("application/pdf")),
                                       dir(QStringLiteral("apple"))},
                                      StackSortOrder::Kind);

        // Folders first regardless of name, then types grouped together. That is
        // what "kind" means to someone looking for the images among the
        // documents.
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("apple"), QStringLiteral("zebra"), QStringLiteral("paper.pdf"),
                              QStringLiteral("photo.png"), QStringLiteral("notes.txt")}));
    }

    void tiesBreakByName()
    {
        // Every entry shares every timestamp: only the tie-breaker can order
        // them. A build output directory is nothing but this case.
        StackModel *model = makeModel({file(QStringLiteral("gamma.o")),
                                       file(QStringLiteral("alpha.o")),
                                       file(QStringLiteral("beta.o"))},
                                      StackSortOrder::DateModified);

        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("alpha.o"), QStringLiteral("beta.o"), QStringLiteral("gamma.o")}));

        model->setSortOrder(int(StackSortOrder::DateAdded));
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("alpha.o"), QStringLiteral("beta.o"), QStringLiteral("gamma.o")}));

        model->setSortOrder(int(StackSortOrder::DateCreated));
        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("alpha.o"), QStringLiteral("beta.o"), QStringLiteral("gamma.o")}));
    }

    void kindTiesBreakByName()
    {
        StackModel *model = makeModel({file(QStringLiteral("zulu.txt")),
                                       file(QStringLiteral("alpha.txt")),
                                       file(QStringLiteral("mike.txt"))},
                                      StackSortOrder::Kind);

        QCOMPARE(namesOf(model),
                 QStringList({QStringLiteral("alpha.txt"), QStringLiteral("mike.txt"), QStringLiteral("zulu.txt")}));
    }

    void changingOrderReorders()
    {
        StackModel *model = makeModel(
            {file(QStringLiteral("aaa.txt"), QStringLiteral("text/plain"), 0, 0, 0),
             file(QStringLiteral("zzz.txt"), QStringLiteral("text/plain"), 100, 100, 100)},
            StackSortOrder::Name);
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("aaa.txt"), QStringLiteral("zzz.txt")}));

        QSignalSpy spy(model, &StackModel::sortOrderChanged);
        model->setSortOrder(int(StackSortOrder::DateModified));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(namesOf(model), QStringList({QStringLiteral("zzz.txt"), QStringLiteral("aaa.txt")}));
    }

    void outOfRangeOrdersAreRefused()
    {
        StackModel *model = makeModel({file(QStringLiteral("a.txt"))}, StackSortOrder::Kind);

        // A value from a hand-edited config, or from a later version. Keeping
        // the order it has beats sorting by nothing.
        model->setSortOrder(99);
        QCOMPARE(model->sortOrder(), int(StackSortOrder::Kind));
        model->setSortOrder(-1);
        QCOMPARE(model->sortOrder(), int(StackSortOrder::Kind));
    }

    void everyOrderIsTotal()
    {
        // Two entries alike in every field but name. If any comparator failed to
        // reach the tie-breaker, std::sort would be free to return either order,
        // and the folder would reshuffle between refreshes.
        const std::vector<StackSortOrder> orders = {StackSortOrder::Name, StackSortOrder::DateAdded,
                                                    StackSortOrder::DateModified, StackSortOrder::DateCreated,
                                                    StackSortOrder::Kind};

        for (StackSortOrder order : orders) {
            StackModel *model = makeModel({file(QStringLiteral("twin-b.txt")), file(QStringLiteral("twin-a.txt"))},
                                          order);
            const QStringList first = namesOf(model);
            QCOMPARE(first, QStringList({QStringLiteral("twin-a.txt"), QStringLiteral("twin-b.txt")}));

            // Same input, refreshed: the answer must not move.
            m_backend.notifyChange();
            QCOMPARE(namesOf(model), first);
        }
    }
};

QTEST_MAIN(TestSorting)
#include "test_sorting.moc"
