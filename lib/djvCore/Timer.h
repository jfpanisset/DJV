//------------------------------------------------------------------------------
// Copyright (c) 2004-2019 Darby Johnston
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions, and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions, and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the names of the copyright holders nor the names of any
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//------------------------------------------------------------------------------

#pragma once

#include <djvCore/Enum.h>
#include <djvCore/ISystem.h>

#include <chrono>
#include <functional>

namespace djv
{
    namespace Core
    {
        namespace Time
        {
            class TimerSystem;

            //! This enumeration provides common timer values.
            enum class TimerValue
            {
                VerySlow,
                Slow,
                Medium,
                Fast,
                VeryFast,

                Count,
                First = VerySlow
            };
            DJV_ENUM_HELPERS(TimerValue);
            size_t getValue(TimerValue);
            std::chrono::milliseconds getMilliseconds(TimerValue);

            //! This class provides a timer.
            class Timer : public std::enable_shared_from_this<Timer>
            {
                DJV_NON_COPYABLE(Timer);
                void _init(const std::shared_ptr<Context>&);
                Timer();

            public:
                //! Create a new time.
                static std::shared_ptr<Timer> create(const std::shared_ptr<Context>&);

                //! \name Timer Options
                ///@{

                bool isRepeating() const;
                void setRepeating(bool);

                ///@}

                //! Is the timer active?
                bool isActive() const;

                //! Start the timer.
                void start(std::chrono::milliseconds, const std::function<void(float)> &);

                //! Stop the timer.
                void stop();

            private:
                void _tick(float dt);

                bool _repeating = false;
                bool _active = false;
                std::chrono::milliseconds _timeout = std::chrono::milliseconds(0);
                std::function<void(float)> _callback;
                std::chrono::time_point<std::chrono::system_clock> _start;

                friend class TimerSystem;
            };

            //! This class provides a timer system.
            class TimerSystem : public ISystemBase
            {
                DJV_NON_COPYABLE(TimerSystem);
                void _init(const std::shared_ptr<Context>&);
                TimerSystem();

            public:
                virtual ~TimerSystem();

                //! Create a new timer system.
                static std::shared_ptr<TimerSystem> create(const std::shared_ptr<Context>&);

                void tick(float dt) override;

            private:
                void _addTimer(const std::weak_ptr<Timer> &);

                DJV_PRIVATE();

                friend class Timer;
            };

        } // namespace Time
    } // namespace Core
    
    DJV_ENUM_SERIALIZE_HELPERS(Core::Time::TimerValue);
    
} // namespace djv

#include <djvCore/TimerInline.h>

