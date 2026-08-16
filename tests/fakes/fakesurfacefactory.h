#pragma once

#include "platform/surfacemanager.h"

#include <algorithm>

namespace frappe
{

/// ISurfaceFactory that records what it was asked to do instead of creating
/// windows, so surface reconciliation can be tested headless.
class FakeSurfaceFactory : public ISurfaceFactory
{
public:
    void createSurface(const OutputInfo &output) override
    {
        m_surfaces.push_back(output.id);
        ++m_createCount;
    }

    void destroySurface(const QString &outputId) override
    {
        std::erase(m_surfaces, outputId);
        ++m_destroyCount;
    }

    void moveSurface(const QString &fromOutputId, const OutputInfo &to) override
    {
        const auto it = std::ranges::find(m_surfaces, fromOutputId);
        if (it == m_surfaces.end()) {
            createSurface(to);
            return;
        }
        *it = to.id;
        ++m_moveCount;
    }

    std::vector<QString> surfaces() const override
    {
        return m_surfaces;
    }

    int surfaceCount() const
    {
        return static_cast<int>(m_surfaces.size());
    }

    int createCount() const
    {
        return m_createCount;
    }

    int destroyCount() const
    {
        return m_destroyCount;
    }

    int moveCount() const
    {
        return m_moveCount;
    }

private:
    std::vector<QString> m_surfaces;
    int m_createCount = 0;
    int m_destroyCount = 0;
    int m_moveCount = 0;
};

}
