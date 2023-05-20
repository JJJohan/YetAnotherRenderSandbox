#pragma once

#include "Macros.hpp"
#include <future>
#include <atomic>
#include <mutex>

namespace Engine
{
    enum class AsyncState
    {
        NotStarted,
        InProgress,
        Cancelled,
        Failed,
        Completed
    };

    struct ProgressInfo
    {
        float Progress;
        float SubProgress;
        std::string ProgressText;
        std::string SubProgressText;

        ProgressInfo() noexcept
            : Progress(0.0f)
            , SubProgress(0.0f)
            , ProgressText()
            , SubProgressText()
        {
        }

        ProgressInfo(const ProgressInfo& info) noexcept
            : Progress(info.Progress)
            , SubProgress(info.SubProgress)
            , ProgressText(info.ProgressText)
            , SubProgressText(info.SubProgressText)
        {
        }
    };

    class AsyncData
    {
    public:
        EXPORT AsyncData();
        EXPORT AsyncData(AsyncState state);

        void InitProgress(const std::string& text, float totalProgressTicks);
        void InitSubProgress(const std::string& subText, float totalSubProgressTicks);
        void AddSubProgress(float progressTicks);
        EXPORT ProgressInfo GetProgress();

        void SetFuture(std::future<void> future);
        EXPORT void Abort();

        std::atomic<AsyncState> State;

    private:
        float m_totalProgressTicks;
        float m_totalSubProgressTicks;
        float m_progressTicks;
        float m_subProgressTicks;

        std::mutex m_progressMutex;
        ProgressInfo m_progressInfo;
        std::future<void> m_future;
    };

}