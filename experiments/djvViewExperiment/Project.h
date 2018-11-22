//------------------------------------------------------------------------------
// Copyright (c) 2004-2018 Darby Johnston
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

#include <IUISystem.h>

#include <QFileInfo>

#include <vector>

namespace djv
{
    namespace ViewExperiment
    {
        class ProjectSystem;

        class Project : public QObject
        {
            Q_OBJECT

        protected:
            Project(const QPointer<Context> &, QObject * parent = nullptr);

        public:
            ~Project() override;

            const QFileInfo & getFileInfo() const;
            bool hasChanges() const;
            
        public Q_SLOTS:
            void open(const QFileInfo &);
            void close();
            void save();
            void saveAs(const QFileInfo &);
            
        Q_SIGNALS:
            void fileInfoChanged(const QFileInfo &);

        private:
            DJV_PRIVATE();

            friend class ProjectSystem;
        };
        
        class ProjectSystem : public IUISystem
        {
            Q_OBJECT

        public:
            ProjectSystem(const QPointer<Context> &, QObject * parent = nullptr);
            ~ProjectSystem() override;

            const std::vector<QPointer<Project> > & getProjects() const;
            QPointer<Project> getProject(int) const;
            const QPointer<Project> & getCurrentProject() const;
            
            QPointer<QMenu> createMenu() override;
            QString getMenuSortKey() const override;

            QPointer<QDockWidget> createDockWidget() override;
            QString getDockWidgetSortKey() const override;
            Qt::DockWidgetArea getDockWidgetArea() const override;

        public Q_SLOTS:
            void newProject();
            void openProject(const QFileInfo &);
            void closeProject(const QPointer<Project> &);
            void closeProject(int);
            void setCurrentProject(const QPointer<Project> &);
            void setCurrentProject(int);

        Q_SIGNALS:
            void projectAdded(const QPointer<Project> &);
            void projectRemoved(const QPointer<Project> &);
            void currentProjectChanged(const QPointer<Project> &);
            
        private:
            DJV_PRIVATE();
        };

    } // namespace ViewExperiment
} // namespace djv

