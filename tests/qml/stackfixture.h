#pragma once

#include <QObject>
#include <QQmlEngine>

#include <map>
#include <memory>

#include "core/model/stackmodel.h"
#include "fakes/fakefolderbackend.h"

/*
 * StackModel instances for the QML tests, over a listing that changes on
 * command rather than whenever the filesystem gets round to it.
 *
 * A QML test cannot build one for itself: StackModel is uncreatable from QML and
 * takes an IFolderBackend, and a plain QML object cannot stand in for it —
 * GridView needs a real QAbstractItemModel for its rows, while the view also
 * asks the same object to navigate. Registered here rather than in the QML
 * module because it exists only for the suite.
 */
class StackFixture : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    /// A model rooted at \a root, with no contents yet. Owned by the fixture,
    /// so a test can drop the reference without leaking.
    Q_INVOKABLE QObject *create(const QString &root)
    {
        auto backend = std::make_unique<frappe::FakeFolderBackend>();
        auto model = std::make_unique<frappe::StackModel>(backend.get());
        model->setRootPath(root);

        frappe::StackModel *raw = model.get();
        QQmlEngine::setObjectOwnership(raw, QQmlEngine::CppOwnership);
        m_backends.emplace(raw, std::move(backend));
        m_models.push_back(std::move(model));
        return raw;
    }

    Q_INVOKABLE void addDir(QObject *model, const QString &name)
    {
        if (auto *backend = backendFor(model)) {
            backend->addDir(name);
        }
    }

    Q_INVOKABLE void addFile(QObject *model, const QString &name)
    {
        if (auto *backend = backendFor(model)) {
            backend->addFile(name);
        }
    }

    Q_INVOKABLE void removeEntry(QObject *model, const QString &name)
    {
        if (auto *backend = backendFor(model)) {
            backend->removeEntry(name);
        }
    }

    /// Adds \a count files named "file-000.txt" upwards, for the cases that are
    /// about what a lot of rows does to the view.
    Q_INVOKABLE void addFiles(QObject *model, int count)
    {
        auto *backend = backendFor(model);
        if (!backend) {
            return;
        }
        for (int i = 0; i < count; ++i) {
            backend->addFile(QStringLiteral("file-%1.txt").arg(i, 3, 10, QLatin1Char('0')));
        }
    }

    /// 0 Idle, 1 Loading, 2 Ready, 3 Failed — mirroring FolderStatus.
    Q_INVOKABLE void setStatus(QObject *model, int status)
    {
        if (auto *backend = backendFor(model)) {
            backend->setStatus(static_cast<frappe::FolderStatus>(status));
        }
    }

    /// Pretend the listing changed, the way the filesystem would.
    Q_INVOKABLE void notifyChange(QObject *model)
    {
        if (auto *backend = backendFor(model)) {
            backend->notifyChange();
        }
    }

private:
    frappe::FakeFolderBackend *backendFor(QObject *model) const
    {
        const auto it = m_backends.find(model);
        return it == m_backends.end() ? nullptr : it->second.get();
    }

    // Backends outlive their models: StackModel holds a raw pointer to one and
    // calls into it from the callback, so the two have to be torn down in that
    // order. Declared second, destroyed first — which is the wrong way round,
    // hence clearing the models explicitly.
    std::vector<std::unique_ptr<frappe::StackModel>> m_models;
    std::map<QObject *, std::unique_ptr<frappe::FakeFolderBackend>> m_backends;

public:
    ~StackFixture() override
    {
        m_models.clear();
    }
};
