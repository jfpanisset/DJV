//------------------------------------------------------------------------------
// Copyright (c) 2004-2015 Darby Johnston
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions, and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions, and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
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

#include <djvViewLib/AbstractActions.h>

#include <memory>

namespace djv
{
    namespace ViewLib
    {
        //! \class FileActions
        //!
        //! This class provides the file group actions.
        class FileActions : public AbstractActions
        {
            Q_OBJECT
                Q_ENUMS(ACTION)
                Q_ENUMS(GROUP)

        public:
            //! This enumeration provides the actions.
            enum ACTION
            {
                OPEN,
                RELOAD,
                RELOAD_FRAME,
                CLOSE,
                SAVE,
                SAVE_FRAME,
                LAYER_PREV,
                LAYER_NEXT,
                U8_CONVERSION,
                CACHE,
                PRELOAD,
                CLEAR_CACHE,
                MESSAGES,
                PREFS,
                DEBUG_LOG,
                EXIT,

                ACTION_COUNT
            };

            //! This enumeration provides the action groups.
            enum GROUP
            {
                RECENT_GROUP,
                LAYER_GROUP,
                PROXY_GROUP,

                GROUP_COUNT
            };

            explicit FileActions(Context *, QObject * parent = nullptr);

            virtual ~FileActions();

        public Q_SLOTS:
            //! Set the layers.
            void setLayers(const QStringList &);

            //! Set the current layer.
            void setLayer(int);

        Q_SIGNALS:
            //! This signal is emitted when the recently opened action group changes.
            void recentChanged();

        private Q_SLOTS:
            void update();

        private:
            DJV_PRIVATE_COPY(FileActions);

            struct Private;
            std::unique_ptr<Private> _p;
        };

    } // namespace ViewLib
} // namespace djv
