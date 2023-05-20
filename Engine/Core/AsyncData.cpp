#include "AsyncData.hpp"

namespace Engine
{
    AsyncData::AsyncData(AsyncState state)
        : State(state)
        , m_future()
        , m_progressInfo()
        , m_progressMutex()
        , m_progressTicks(0.0f)
        , m_subProgressTicks(0.0f)
        , m_totalProgressTicks(0.0f)
        , m_totalSubProgressTicks(0.0f)
    {
    }

    AsyncData::AsyncData()
        : AsyncData(AsyncState::NotStarted)
    {
    }

    void AsyncData::SetFuture(std::future<void> future)
    {
        m_future = std::move(future);
    }
        
    void AsyncData::InitProgress(const std::string& text, float totalProgressTicks)
    {
        m_progressTicks = 0;
        m_totalProgressTicks = totalProgressTicks;
        m_subProgressTicks = 0;
        m_totalSubProgressTicks = 0;

        const std::lock_guard<std::mutex> guard(m_progressMutex);
        m_progressInfo.ProgressText = text;
        m_progressInfo.Progress = 0.0f;
        m_progressInfo.SubProgressText = "";
        m_progressInfo.SubProgress = 0.0f;
    }

    void AsyncData::InitSubProgress(const std::string& subText, float totalSubProgressTicks)
    {
        m_subProgressTicks = 0;
        m_totalSubProgressTicks = totalSubProgressTicks;

        const std::lock_guard<std::mutex> guard(m_progressMutex);
        m_progressInfo.SubProgressText = subText;
        m_progressInfo.SubProgress = 0.0f;
    }

    void AsyncData::AddSubProgress(float progressTicks)
    {
        m_progressTicks += progressTicks;
        m_subProgressTicks += progressTicks;

        const std::lock_guard<std::mutex> guard(m_progressMutex);
        m_progressInfo.Progress = m_progressTicks / m_totalProgressTicks;
        m_progressInfo.SubProgress = m_subProgressTicks / m_totalSubProgressTicks;
    }

    ProgressInfo AsyncData::GetProgress()
    {
        const std::lock_guard<std::mutex> guard(m_progressMutex);
        return m_progressInfo;
    }

    void AsyncData::Abort()
    {
        if (State != AsyncState::InProgress)
        {
            return;
        }

        State = AsyncState::Cancelled;
        m_future.wait();
    }
}