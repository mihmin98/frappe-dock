#include "core/config/configfacade.h"
#include "fakes/fakeoutputprovider.h"
#include "fakes/fakesurfacefactory.h"
#include "platform/surfacemanager.h"

#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace frappe;

namespace
{
OutputInfo output(const QString &id, bool primary = false)
{
    OutputInfo info;
    info.id = id;
    info.geometry = QRect(0, 0, 1600, 900);
    info.scale = 1.0;
    info.isPrimary = primary;
    return info;
}
}

class TestSurfaceManager : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString configPath() const
    {
        return m_dir.filePath(QStringLiteral("frappe-dockrc"));
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
    }

    void allScreensCreatesOnePerOutput()
    {
        ConfigFacade config(configPath());
        config.setDisplayMode(ConfigFacade::AllScreens);

        FakeOutputProvider outputs;
        outputs.setOutputs({output(QStringLiteral("WL-0"), true), output(QStringLiteral("WL-1")), output(QStringLiteral("WL-2"))});

        FakeSurfaceFactory factory;
        SurfaceManager manager(&config, &outputs, &factory);
        manager.reconcile();

        QCOMPARE(factory.surfaceCount(), 3);

        // Reconciling again must be a no-op, not three more surfaces.
        manager.reconcile();
        QCOMPARE(factory.surfaceCount(), 3);
        QCOMPARE(factory.createCount(), 3);
    }

    void singleScreenCreatesOnlyOnTarget()
    {
        ConfigFacade config(configPath());
        config.setDisplayMode(ConfigFacade::SingleScreen);
        config.setTargetOutput(QStringLiteral("WL-1"));

        FakeOutputProvider outputs;
        outputs.setOutputs({output(QStringLiteral("WL-0"), true), output(QStringLiteral("WL-1"))});

        FakeSurfaceFactory factory;
        SurfaceManager manager(&config, &outputs, &factory);
        manager.reconcile();

        QCOMPARE(factory.surfaces(), std::vector<QString>({QStringLiteral("WL-1")}));
    }

    void singleScreenFallsBackToPrimaryWhenTargetMissing()
    {
        ConfigFacade config(configPath());
        config.setDisplayMode(ConfigFacade::SingleScreen);
        config.setTargetOutput(QStringLiteral("WL-9"));

        FakeOutputProvider outputs;
        outputs.setOutputs({output(QStringLiteral("WL-0")), output(QStringLiteral("WL-1"), true)});

        FakeSurfaceFactory factory;
        SurfaceManager manager(&config, &outputs, &factory);
        manager.reconcile();

        QCOMPARE(factory.surfaces(), std::vector<QString>({QStringLiteral("WL-1")}));
    }

    void followActiveNeverExceedsOneSurface()
    {
        ConfigFacade config(configPath());
        config.setDisplayMode(ConfigFacade::FollowActive);

        FakeOutputProvider outputs;
        outputs.setOutputs({output(QStringLiteral("WL-0"), true)});

        FakeSurfaceFactory factory;
        SurfaceManager manager(&config, &outputs, &factory);
        outputs.setChangeCallback([&manager] {
            manager.reconcile();
        });
        manager.reconcile();

        // A long random walk over output and focus changes. This mode is the one
        // that invites surface leaks, and a leak shows up as a count above one.
        auto *random = QRandomGenerator::global();
        for (int step = 0; step < 2000; ++step) {
            switch (random->bounded(4)) {
            case 0: {
                const QString id = QStringLiteral("WL-%1").arg(random->bounded(5));
                std::vector<OutputInfo> current = outputs.outputs();
                const bool known = std::ranges::any_of(current, [&id](const OutputInfo &o) {
                    return o.id == id;
                });
                if (!known) {
                    outputs.addOutput(output(id, current.empty()));
                }
                break;
            }
            case 1: {
                const std::vector<OutputInfo> current = outputs.outputs();
                if (current.size() > 1) {
                    outputs.removeOutput(current[random->bounded(int(current.size()))].id);
                }
                break;
            }
            case 2:
                outputs.setActiveOutput(QStringLiteral("WL-%1").arg(random->bounded(5)));
                break;
            case 3:
                manager.reconcile();
                break;
            }

            QVERIFY2(factory.surfaceCount() <= 1,
                     qPrintable(QStringLiteral("step %1 produced %2 surfaces").arg(step).arg(factory.surfaceCount())));
            QVERIFY(!outputs.outputs().empty() ? factory.surfaceCount() == 1 : true);
        }
    }

    void outputAddedCreatesSurfaceOnlyInAllScreens()
    {
        for (const int mode : {int(ConfigFacade::AllScreens), int(ConfigFacade::SingleScreen), int(ConfigFacade::FollowActive)}) {
            ConfigFacade config(configPath());
            config.setDisplayMode(mode);
            config.setTargetOutput(QStringLiteral("WL-0"));

            FakeOutputProvider outputs;
            outputs.setOutputs({output(QStringLiteral("WL-0"), true)});
            outputs.setActiveOutput(QStringLiteral("WL-0"));

            FakeSurfaceFactory factory;
            SurfaceManager manager(&config, &outputs, &factory);
            manager.reconcile();
            QCOMPARE(factory.surfaceCount(), 1);

            outputs.addOutput(output(QStringLiteral("WL-1")));
            manager.reconcile();

            const int expected = mode == int(ConfigFacade::AllScreens) ? 2 : 1;
            QCOMPARE(factory.surfaceCount(), expected);
        }
    }

    void outputRemovedDestroysItsSurface()
    {
        ConfigFacade config(configPath());
        config.setDisplayMode(ConfigFacade::AllScreens);

        FakeOutputProvider outputs;
        outputs.setOutputs({output(QStringLiteral("WL-0"), true), output(QStringLiteral("WL-1"))});

        FakeSurfaceFactory factory;
        SurfaceManager manager(&config, &outputs, &factory);
        manager.reconcile();
        QCOMPARE(factory.surfaceCount(), 2);

        outputs.removeOutput(QStringLiteral("WL-1"));
        manager.reconcile();

        QCOMPARE(factory.surfaces(), std::vector<QString>({QStringLiteral("WL-0")}));
    }

    void modeChangeReconcilesCorrectly()
    {
        const std::vector<int> modes = {
            int(ConfigFacade::AllScreens),
            int(ConfigFacade::SingleScreen),
            int(ConfigFacade::FollowActive),
        };

        // Every ordered pair, including a mode changing to itself.
        for (const int from : modes) {
            for (const int to : modes) {
                ConfigFacade config(configPath());
                config.setTargetOutput(QStringLiteral("WL-1"));

                FakeOutputProvider outputs;
                outputs.setOutputs({output(QStringLiteral("WL-0"), true), output(QStringLiteral("WL-1")), output(QStringLiteral("WL-2"))});
                outputs.setActiveOutput(QStringLiteral("WL-2"));

                FakeSurfaceFactory factory;
                SurfaceManager manager(&config, &outputs, &factory);

                config.setDisplayMode(from);
                manager.reconcile();
                config.setDisplayMode(to);
                manager.reconcile();

                const int expected = to == int(ConfigFacade::AllScreens) ? 3 : 1;
                QCOMPARE(factory.surfaceCount(), expected);

                if (to == int(ConfigFacade::SingleScreen)) {
                    QCOMPARE(factory.surfaces().front(), QStringLiteral("WL-1"));
                } else if (to == int(ConfigFacade::FollowActive)) {
                    QCOMPARE(factory.surfaces().front(), QStringLiteral("WL-2"));
                }
            }
        }
    }
};

QTEST_MAIN(TestSurfaceManager)
#include "test_surfacemanager.moc"
