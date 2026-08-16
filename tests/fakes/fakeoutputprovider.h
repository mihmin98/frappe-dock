#pragma once

#include "core/interfaces/ioutputprovider.h"

namespace frappe
{

/// IOutputProvider driven entirely by the test.
class FakeOutputProvider : public IOutputProvider
{
public:
    void setOutputs(const std::vector<OutputInfo> &outputs)
    {
        m_outputs = outputs;
        notify();
    }

    void addOutput(const OutputInfo &output)
    {
        m_outputs.push_back(output);
        notify();
    }

    void removeOutput(const QString &id)
    {
        std::erase_if(m_outputs, [&id](const OutputInfo &o) {
            return o.id == id;
        });
        notify();
    }

    void setActiveOutput(const QString &id)
    {
        m_activeId = id;
        notify();
    }

    std::vector<OutputInfo> outputs() const override
    {
        return m_outputs;
    }

    OutputInfo activeOutput() const override
    {
        for (const OutputInfo &output : m_outputs) {
            if (output.id == m_activeId) {
                return output;
            }
        }
        return m_outputs.empty() ? OutputInfo{} : m_outputs.front();
    }

    void setChangeCallback(std::function<void()> cb) override
    {
        m_callback = std::move(cb);
    }

private:
    void notify()
    {
        if (m_callback) {
            m_callback();
        }
    }

    std::vector<OutputInfo> m_outputs;
    QString m_activeId;
    std::function<void()> m_callback;
};

}
