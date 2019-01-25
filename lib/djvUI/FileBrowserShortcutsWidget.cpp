//------------------------------------------------------------------------------
// Copyright (c) 2018 Darby Johnston
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

#include <djvUI/FileBrowserPrivate.h>

#include <djvUI/Action.h>
#include <djvUI/ActionGroup.h>
#include <djvUI/Border.h>
#include <djvUI/Icon.h>
#include <djvUI/Label.h>
#include <djvUI/ListButton.h>
#include <djvUI/ImageWidget.h>
#include <djvUI/Menu.h>
#include <djvUI/MenuButton.h>
#include <djvUI/RowLayout.h>
#include <djvUI/ScrollWidget.h>
#include <djvUI/StackLayout.h>
#include <djvUI/TextBlock.h>

#include <djvAV/IO.h>

#include <djvCore/FileInfo.h>
#include <djvCore/OS.h>

using namespace djv::Core;

namespace djv
{
    namespace UI
    {
        namespace FileBrowser
        {
            struct ShorcutsWidget::Private
            {
                std::vector<FileSystem::Path> shortcuts;
                std::shared_ptr<ActionGroup> actionGroup;
                std::shared_ptr<Layout::Vertical> layout;
                std::function<void(const FileSystem::Path &)> shortcutCallback;
            };

            void ShorcutsWidget::_init(Context * context)
            {
                UI::Widget::_init(context);

                setClassName("djv::UI::FileBrowser::ShorcutsWidget");

                DJV_PRIVATE_PTR();
                p.actionGroup = ActionGroup::create(ButtonType::Push);

                auto itemLayout = Layout::Vertical::create(context);
                itemLayout->setSpacing(Style::MetricsRole::None);
                for (size_t i = 0; i < static_cast<size_t>(OS::DirectoryShortcut::Count); ++i)
                {
                    const auto shortcut = OS::getPath(static_cast<OS::DirectoryShortcut>(i));
                    p.shortcuts.push_back(shortcut);

                    auto action = Action::create();
                    const auto text = shortcut.getFileName();
                    action->setText(text);
                    p.actionGroup->addAction(action);

                    auto button = Button::List::create(context);
                    button->setText(text);
                    button->setTextHAlign(TextHAlign::Left);
                    itemLayout->addWidget(button);

                    button->setClickedCallback(
                        [action]
                    {
                        action->doClicked();
                    });
                }
                auto scrollWidget = ScrollWidget::create(ScrollType::Vertical, context);
                scrollWidget->setBorder(false);
                scrollWidget->addWidget(itemLayout);

                p.layout = Layout::Vertical::create(context);
                p.layout->addWidget(scrollWidget, Layout::RowStretch::Expand);
                p.layout->setParent(shared_from_this());

                auto weak = std::weak_ptr<ShorcutsWidget>(std::dynamic_pointer_cast<ShorcutsWidget>(shared_from_this()));
                p.actionGroup->setClickedCallback(
                    [weak](int value)
                {
                    if (auto widget = weak.lock())
                    {
                        if (value >= 0 && value < widget->_p->shortcuts.size() && widget->_p->shortcutCallback)
                        {
                            widget->_p->shortcutCallback(widget->_p->shortcuts[value]);
                        }
                    }
                });
            }

            ShorcutsWidget::ShorcutsWidget() :
                _p(new Private)
            {}

            ShorcutsWidget::~ShorcutsWidget()
            {}

            std::shared_ptr<ShorcutsWidget> ShorcutsWidget::create(Context * context)
            {
                auto out = std::shared_ptr<ShorcutsWidget>(new ShorcutsWidget);
                out->_init(context);
                return out;
            }

            void ShorcutsWidget::setShortcutCallback(const std::function<void(const FileSystem::Path &)> & value)
            {
                _p->shortcutCallback = value;
            }

            void ShorcutsWidget::_preLayoutEvent(Event::PreLayout& event)
            {
                _setMinimumSize(_p->layout->getMinimumSize());
            }

            void ShorcutsWidget::_layoutEvent(Event::Layout& event)
            {
                _p->layout->setGeometry(getGeometry());
            }

        } // namespace FileBrowser
    } // namespace UI
} // namespace djv
