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

#include <djvUIComponents/ISettingsWidget.h>

namespace djv
{
    namespace ViewApp
    {
        //! This class provides the playback settings widget.
        class PlaybackSettingsWidget : public UI::ISettingsWidget
        {
            DJV_NON_COPYABLE(PlaybackSettingsWidget);

        protected:
            void _init(const std::shared_ptr<Core::Context>&);
            PlaybackSettingsWidget();

        public:
            static std::shared_ptr<PlaybackSettingsWidget> create(const std::shared_ptr<Core::Context>&);

            std::string getSettingsName() const override;
            std::string getSettingsGroup() const override;
            std::string getSettingsSortKey() const override;

        protected:
            void _initEvent(Core::Event::Init&) override;

        private:
            DJV_PRIVATE();
        };

        //! This class provides the timeline settings widget.
        class TimelineSettingsWidget : public UI::ISettingsWidget
        {
            DJV_NON_COPYABLE(TimelineSettingsWidget);

        protected:
            void _init(const std::shared_ptr<Core::Context>&);
            TimelineSettingsWidget();

        public:
            static std::shared_ptr<TimelineSettingsWidget> create(const std::shared_ptr<Core::Context>&);

            std::string getSettingsName() const override;
            std::string getSettingsGroup() const override;
            std::string getSettingsSortKey() const override;

        protected:
            void _initEvent(Core::Event::Init&) override;

        private:
            DJV_PRIVATE();
        };

    } // namespace ViewApp
} // namespace djv

