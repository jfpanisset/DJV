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

#include <djvViewApp/PlaybackSystem.h>

#include <djvViewApp/Application.h>
#include <djvViewApp/FileSystem.h>
#include <djvViewApp/Media.h>
#include <djvViewApp/MediaWidget.h>
#include <djvViewApp/PlaybackSettings.h>
#include <djvViewApp/WindowSystem.h>

#include <djvUI/Action.h>
#include <djvUI/ActionGroup.h>
#include <djvUI/Menu.h>
#include <djvUI/RowLayout.h>
#include <djvUI/Shortcut.h>
#include <djvUI/Style.h>
#include <djvUI/UISystem.h>

#include <djvCore/Context.h>
#include <djvCore/TextSystem.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace djv::Core;

namespace djv
{
    namespace ViewApp
    {
        struct PlaybackSystem::Private
        {
            std::shared_ptr<PlaybackSettings> settings;
            std::shared_ptr<Media> currentMedia;
            Time::Speed speed;
            Frame::Sequence sequence;
            bool playEveryFrame = false;
            Playback playback = Playback::Forward;
            bool inOutPointsEnabled = false;
            glm::vec2 hoverPos = glm::vec2(0.F, 0.F);
            glm::vec2 dragStart = glm::vec2(0.F, 0.F);
            Frame::Index dragStartFrame = Frame::invalidIndex;
            Playback dragStartPlayback = Playback::Stop;
            std::map<std::string, std::shared_ptr<UI::Action> > actions;
            std::shared_ptr<UI::ActionGroup> playbackActionGroup;
            std::shared_ptr<UI::ActionGroup> playbackModeActionGroup;
            std::shared_ptr<UI::Menu> menu;
            std::map<std::string, std::shared_ptr<ValueObserver<bool> > > actionObservers;
            std::shared_ptr<ValueObserver<std::shared_ptr<Media> > > currentMediaObserver;
            std::shared_ptr<ValueObserver<Time::Speed> > speedObserver;
            std::shared_ptr<ValueObserver<bool> > playEveryFrameObserver;
            std::shared_ptr<ValueObserver<Frame::Sequence> > sequenceObserver;
            std::shared_ptr<ValueObserver<Playback> > playbackObserver;
            std::shared_ptr<ValueObserver<PlaybackMode> > playbackModeObserver;
            std::shared_ptr<ValueObserver<AV::IO::InOutPoints> > inOutPointsEnabledObserver;
            std::shared_ptr<ValueObserver<std::shared_ptr<MediaWidget> > > activeWidgetObserver;
            std::shared_ptr<ValueObserver<PointerData> > hoverObserver;
            std::shared_ptr<ValueObserver<PointerData> > dragObserver;
        };

        void PlaybackSystem::_init(const std::shared_ptr<Core::Context>& context)
        {
            IViewSystem::_init("djv::ViewApp::PlaybackSystem", context);

            DJV_PRIVATE_PTR();
            p.settings = PlaybackSettings::create(context);

            p.actions["Forward"] = UI::Action::create();
            p.actions["Forward"]->setIcon("djvIconPlaybackForward");
            p.actions["Forward"]->addShortcut(GLFW_KEY_UP);
            p.actions["Reverse"] = UI::Action::create();
            p.actions["Reverse"]->setIcon("djvIconPlaybackReverse");
            p.actions["Reverse"]->setShortcut(GLFW_KEY_DOWN);
            p.playbackActionGroup = UI::ActionGroup::create(UI::ButtonType::Exclusive);
            p.playbackActionGroup->addAction(p.actions["Forward"]);
            p.playbackActionGroup->addAction(p.actions["Reverse"]);
            p.actions["Playback"] = UI::Action::create();
            p.actions["Playback"]->addShortcut(GLFW_KEY_SPACE);

            p.actions["PlayOnce"] = UI::Action::create();
            p.actions["PlayOnce"]->setEnabled(false);
            p.actions["PlayLoop"] = UI::Action::create();
            p.actions["PlayLoop"]->setEnabled(false);
            p.actions["PlayPingPong"] = UI::Action::create();
            p.actions["PlayPingPong"]->setEnabled(false);
            p.playbackModeActionGroup = UI::ActionGroup::create(UI::ButtonType::Radio);
            p.playbackModeActionGroup->addAction(p.actions["PlayOnce"]);
            p.playbackModeActionGroup->addAction(p.actions["PlayLoop"]);
            p.playbackModeActionGroup->addAction(p.actions["PlayPingPong"]);

            p.actions["PlayEveryFrame"] = UI::Action::create();
            p.actions["PlayEveryFrame"]->setButtonType(UI::ButtonType::Toggle);

            p.actions["InPoint"] = UI::Action::create();
            p.actions["InPoint"]->setIcon("djvIconFrameStart");
            p.actions["InPoint"]->setShortcut(GLFW_KEY_HOME);
            p.actions["OutPoint"] = UI::Action::create();
            p.actions["OutPoint"]->setIcon("djvIconFrameEnd");
            p.actions["OutPoint"]->setShortcut(GLFW_KEY_END);
            p.actions["StartFrame"] = UI::Action::create();
            p.actions["StartFrame"]->setShortcut(GLFW_KEY_HOME, GLFW_MOD_SHIFT);
            p.actions["EndFrame"] = UI::Action::create();
            p.actions["EndFrame"]->setShortcut(GLFW_KEY_END, GLFW_MOD_SHIFT);
            p.actions["NextFrame"] = UI::Action::create();
            p.actions["NextFrame"]->setIcon("djvIconFrameNext");
            p.actions["NextFrame"]->addShortcut(GLFW_KEY_RIGHT);
            p.actions["NextFrame"]->addShortcut(GLFW_KEY_RIGHT_BRACKET);
            p.actions["NextFrame10"] = UI::Action::create();
            p.actions["NextFrame10"]->addShortcut(GLFW_KEY_RIGHT, GLFW_MOD_SHIFT);
            p.actions["NextFrame10"]->addShortcut(GLFW_KEY_RIGHT_BRACKET, GLFW_MOD_SHIFT);
            p.actions["NextFrame100"] = UI::Action::create();
            p.actions["NextFrame100"]->addShortcut(GLFW_KEY_RIGHT, UI::Shortcut::getSystemModifier());
            p.actions["NextFrame100"]->addShortcut(GLFW_KEY_RIGHT_BRACKET, UI::Shortcut::getSystemModifier());
            p.actions["PrevFrame"] = UI::Action::create();
            p.actions["PrevFrame"]->setIcon("djvIconFramePrev");
            p.actions["PrevFrame"]->addShortcut(GLFW_KEY_LEFT);
            p.actions["PrevFrame"]->addShortcut(GLFW_KEY_LEFT_BRACKET);
            p.actions["PrevFrame10"] = UI::Action::create();
            p.actions["PrevFrame10"]->addShortcut(GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
            p.actions["PrevFrame10"]->addShortcut(GLFW_KEY_LEFT_BRACKET, GLFW_MOD_SHIFT);
            p.actions["PrevFrame100"] = UI::Action::create();
            p.actions["PrevFrame100"]->addShortcut(GLFW_KEY_LEFT, UI::Shortcut::getSystemModifier());
            p.actions["PrevFrame100"]->addShortcut(GLFW_KEY_LEFT_BRACKET, UI::Shortcut::getSystemModifier());

            p.actions["InOutPoints"] = UI::Action::create();
            p.actions["InOutPoints"]->setButtonType(UI::ButtonType::Toggle);
            p.actions["InOutPoints"]->setShortcut(GLFW_KEY_P);
            p.actions["SetInPoint"] = UI::Action::create();
            p.actions["SetInPoint"]->setShortcut(GLFW_KEY_I);
            p.actions["SetOutPoint"] = UI::Action::create();
            p.actions["SetOutPoint"]->setShortcut(GLFW_KEY_O);
            p.actions["ResetInPoint"] = UI::Action::create();
            p.actions["ResetInPoint"]->setShortcut(GLFW_KEY_I, GLFW_MOD_SHIFT);
            p.actions["ResetOutPoint"] = UI::Action::create();
            p.actions["ResetOutPoint"]->setShortcut(GLFW_KEY_O, GLFW_MOD_SHIFT);

            p.menu = UI::Menu::create(context);
            p.menu->addAction(p.actions["Forward"]);
            p.menu->addAction(p.actions["Reverse"]);
            p.menu->addSeparator();
            p.menu->addAction(p.actions["InPoint"]);
            p.menu->addAction(p.actions["OutPoint"]);
            p.menu->addAction(p.actions["StartFrame"]);
            p.menu->addAction(p.actions["EndFrame"]);
            p.menu->addSeparator();
            p.menu->addAction(p.actions["NextFrame"]);
            p.menu->addAction(p.actions["NextFrame10"]);
            p.menu->addAction(p.actions["NextFrame100"]);
            p.menu->addAction(p.actions["PrevFrame"]);
            p.menu->addAction(p.actions["PrevFrame10"]);
            p.menu->addAction(p.actions["PrevFrame100"]);
            p.menu->addSeparator();
            p.menu->addAction(p.actions["InOutPoints"]);
            p.menu->addAction(p.actions["SetInPoint"]);
            p.menu->addAction(p.actions["SetOutPoint"]);
            p.menu->addAction(p.actions["ResetInPoint"]);
            p.menu->addAction(p.actions["ResetOutPoint"]);

            _actionsUpdate();

            auto weak = std::weak_ptr<PlaybackSystem>(std::dynamic_pointer_cast<PlaybackSystem>(shared_from_this()));
            p.playbackActionGroup->setExclusiveCallback(
                [weak](int index)
            {
                if (auto system = weak.lock())
                {
                    if (auto media = system->_p->currentMedia)
                    {
                        Playback playback = Playback::Stop;
                        switch (index)
                        {
                        case 0: playback = system->_p->playback = Playback::Forward; break;
                        case 1: playback = system->_p->playback = Playback::Reverse; break;
                        }
                        media->setPlayback(playback);
                    }
                }
            });

            p.playbackModeActionGroup->setRadioCallback(
                [weak](int index)
            {
                if (auto system = weak.lock())
                {
                    if (auto media = system->_p->currentMedia)
                    {
                        media->setPlaybackMode(static_cast<PlaybackMode>(index));
                    }
                }
            });

            p.actionObservers["Playback"] = ValueObserver<bool>::create(
                p.actions["Playback"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->setPlayback(
                                Playback::Stop == media->observePlayback()->get() ?
                                (Playback::Stop == system->_p->playback ? Playback::Forward : system->_p->playback) :
                                Playback::Stop);
                        }
                    }
                }
            });

            p.actionObservers["PlayEveryFrame"] = ValueObserver<bool>::create(
                p.actions["PlayEveryFrame"]->observeChecked(),
                [weak](bool value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->setPlayEveryFrame(value);
                        }
                    }
                });

            p.actionObservers["InPoint"] = ValueObserver<bool>::create(
                p.actions["InPoint"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->inPoint();
                            }
                        }
                    }
                });

            p.actionObservers["OutPoint"] = ValueObserver<bool>::create(
                p.actions["OutPoint"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->outPoint();
                        }
                    }
                }
            });

            p.actionObservers["StartFrame"] = ValueObserver<bool>::create(
                p.actions["StartFrame"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->start();
                            }
                        }
                    }
                });

            p.actionObservers["EndFrame"] = ValueObserver<bool>::create(
                p.actions["EndFrame"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->end();
                            }
                        }
                    }
                });

            p.actionObservers["NextFrame"] = ValueObserver<bool>::create(
                p.actions["NextFrame"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->nextFrame();
                        }
                    }
                }
            });

            p.actionObservers["NextFrame10"] = ValueObserver<bool>::create(
                p.actions["NextFrame10"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->nextFrame(10);
                        }
                    }
                }
            });

            p.actionObservers["NextFrame100"] = ValueObserver<bool>::create(
                p.actions["NextFrame100"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->nextFrame(100);
                        }
                    }
                }
            });

            p.actionObservers["PrevFrame"] = ValueObserver<bool>::create(
                p.actions["PrevFrame"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->prevFrame();
                        }
                    }
                }
            });

            p.actionObservers["PrevFrame10"] = ValueObserver<bool>::create(
                p.actions["PrevFrame10"]->observeClicked(),
                [weak](bool value)
            {
                if (value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            media->prevFrame(10);
                        }
                    }
                }
            });

            p.actionObservers["PrevFrame100"] = ValueObserver<bool>::create(
                p.actions["PrevFrame100"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->prevFrame(100);
                            }
                        }
                    }
                });

            p.actionObservers["InOutPoints"] = ValueObserver<bool>::create(
                p.actions["InOutPoints"]->observeChecked(),
                [weak](bool value)
                {
                    if (auto system = weak.lock())
                    {
                        if (auto media = system->_p->currentMedia)
                        {
                            const auto& inOutPoints = media->observeInOutPoints()->get();
                            media->setInOutPoints(AV::IO::InOutPoints(value, inOutPoints.getIn(), inOutPoints.getOut()));
                        }
                    }
                });

            p.actionObservers["SetInPoint"] = ValueObserver<bool>::create(
                p.actions["SetInPoint"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->setInPoint();
                            }
                        }
                    }
                });

            p.actionObservers["SetOutPoint"] = ValueObserver<bool>::create(
                p.actions["SetOutPoint"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->setOutPoint();
                            }
                        }
                    }
                });

            p.actionObservers["ResetInPoint"] = ValueObserver<bool>::create(
                p.actions["ResetInPoint"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->resetInPoint();
                            }
                        }
                    }
                });

            p.actionObservers["ResetOutPoint"] = ValueObserver<bool>::create(
                p.actions["ResetOutPoint"]->observeClicked(),
                [weak](bool value)
                {
                    if (value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (auto media = system->_p->currentMedia)
                            {
                                media->resetOutPoint();
                            }
                        }
                    }
                });

            if (auto fileSystem = context->getSystemT<FileSystem>())
            {
                p.currentMediaObserver = ValueObserver<std::shared_ptr<Media> >::create(
                    fileSystem->observeCurrentMedia(),
                    [weak](const std::shared_ptr<Media> & value)
                {
                    if (auto system = weak.lock())
                    {
                        system->_p->currentMedia = value;
                        system->_actionsUpdate();
                        if (value)
                        {
                            system->_p->speedObserver = ValueObserver<Time::Speed>::create(
                                value->observeSpeed(),
                                [weak](const Time::Speed& value)
                                {
                                    if (auto system = weak.lock())
                                    {
                                        system->_p->speed = value;
                                        system->_actionsUpdate();
                                    }
                                });

                            system->_p->playEveryFrameObserver = ValueObserver<bool>::create(
                                value->observePlayEveryFrame(),
                                [weak](bool value)
                                {
                                    if (auto system = weak.lock())
                                    {
                                        system->_p->playEveryFrame = value;
                                        system->_actionsUpdate();
                                    }
                                });

                            system->_p->sequenceObserver = ValueObserver<Frame::Sequence>::create(
                                value->observeSequence(),
                                [weak](const Frame::Sequence& value)
                                {
                                    if (auto system = weak.lock())
                                    {
                                        system->_p->sequence = value;
                                        system->_actionsUpdate();
                                    }
                                });

                            system->_p->playbackObserver = ValueObserver<Playback>::create(
                                value->observePlayback(),
                                [weak](Playback value)
                            {
                                if (auto system = weak.lock())
                                {
                                    int index = -1;
                                    switch (value)
                                    {
                                    case Playback::Forward:
                                        index = 0;
                                        system->_p->playback = value;
                                        break;
                                    case Playback::Reverse:
                                        index = 1;
                                        system->_p->playback = value;
                                        break;
                                    default: break;
                                    }
                                    system->_p->playbackActionGroup->setChecked(index);
                                }
                            });

                            system->_p->playbackModeObserver = ValueObserver<PlaybackMode>::create(
                                value->observePlaybackMode(),
                                [weak](PlaybackMode value)
                            {
                                if (auto system = weak.lock())
                                {
                                    system->_p->playbackModeActionGroup->setChecked(static_cast<int>(value), true);
                                }
                            });

                            system->_p->inOutPointsEnabledObserver = ValueObserver<AV::IO::InOutPoints>::create(
                                value->observeInOutPoints(),
                                [weak](const AV::IO::InOutPoints& value)
                                {
                                    if (auto system = weak.lock())
                                    {
                                        system->_p->inOutPointsEnabled = value.isEnabled();
                                        system->_actionsUpdate();
                                    }
                                });
                        }
                        else
                        {
                            system->_p->speed = Time::Speed();
                            system->_p->sequence = Frame::Sequence();
                            system->_p->playEveryFrame = false;
                            system->_p->playback = Playback::Stop;
                            system->_p->inOutPointsEnabled = false;

                            system->_p->playbackActionGroup->setChecked(0, false);
                            system->_p->playbackActionGroup->setChecked(1, false);

                            system->_p->speedObserver.reset();
                            system->_p->sequenceObserver.reset();
                            system->_p->playEveryFrameObserver.reset();
                            system->_p->playbackObserver.reset();
                            system->_p->playbackModeObserver.reset();
                            system->_p->inOutPointsEnabledObserver.reset();

                            system->_actionsUpdate();
                        }
                    }
                });
            }

            if (auto windowSystem = context->getSystemT<WindowSystem>())
            {
                p.activeWidgetObserver = ValueObserver<std::shared_ptr<MediaWidget> >::create(
                    windowSystem->observeActiveWidget(),
                    [weak](const std::shared_ptr<MediaWidget>& value)
                    {
                        if (auto system = weak.lock())
                        {
                            if (value)
                            {
                                system->_p->hoverObserver = ValueObserver<PointerData>::create(
                                    value->observeHover(),
                                    [weak](const PointerData& value)
                                    {
                                        if (auto system = weak.lock())
                                        {
                                            system->_p->hoverPos = value.pos;
                                        }
                                    });
                                auto widgetWeak = std::weak_ptr<MediaWidget>(std::dynamic_pointer_cast<MediaWidget>(value));
                                system->_p->dragObserver = ValueObserver<PointerData>::create(
                                    value->observeDrag(),
                                    [weak, widgetWeak](const PointerData& value)
                                    {
                                        if (auto system = weak.lock())
                                        {
                                            if (auto widget = widgetWeak.lock())
                                            {
                                                if (auto media = system->_p->currentMedia)
                                                {
                                                    const auto i = value.buttons.find(3);
                                                    if (i != value.buttons.end())
                                                    {
                                                        switch (value.state)
                                                        {
                                                        case PointerState::Start:
                                                            system->_p->dragStart = value.pos;
                                                            system->_p->dragStartFrame = media->observeCurrentFrame()->get();
                                                            system->_p->dragStartPlayback = media->observePlayback()->get();
                                                            break;
                                                        case PointerState::Move:
                                                        {
                                                            if (auto context = system->getContext().lock())
                                                            {
                                                                auto uiSystem = context->getSystemT<UI::UISystem>();
                                                                auto style = uiSystem->getStyle();
                                                                const Frame::Index offset =
                                                                    (value.pos.x - system->_p->dragStart.x) / style->getMetric(UI::MetricsRole::Scrub);
                                                                media->setCurrentFrame(system->_p->dragStartFrame + offset);
                                                            }
                                                            break;
                                                        }
                                                        case PointerState::End:
                                                            media->setPlayback(system->_p->dragStartPlayback);
                                                            break;
                                                        default: break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    });
                            }
                            else
                            {
                                system->_p->hoverObserver.reset();
                                system->_p->dragObserver.reset();
                            }
                        }
                    });
            }
        }

        PlaybackSystem::PlaybackSystem() :
            _p(new Private)
        {}

        PlaybackSystem::~PlaybackSystem()
        {}

        std::shared_ptr<PlaybackSystem> PlaybackSystem::create(const std::shared_ptr<Core::Context>& context)
        {
            auto out = std::shared_ptr<PlaybackSystem>(new PlaybackSystem);
            out->_init(context);
            return out;
        }

        std::map<std::string, std::shared_ptr<UI::Action> > PlaybackSystem::getActions() const
        {
            return _p->actions;
        }

        MenuData PlaybackSystem::getMenu() const
        {
            return
            {
                _p->menu,
                "E"
            };
        }

        void PlaybackSystem::_actionsUpdate()
        {
            DJV_PRIVATE_PTR();
            const bool playable =
                (p.currentMedia ? true : false) &&
                (p.sequence.getSize());
            p.actions["Forward"]->setEnabled(playable);
            p.actions["Reverse"]->setEnabled(playable);
            p.actions["PlayOnce"]->setEnabled(playable);
            p.actions["PlayLoop"]->setEnabled(playable);
            p.actions["PlayPingPong"]->setEnabled(playable);
            p.actions["PlayEveryFrame"]->setEnabled(playable);
            p.actions["PlayEveryFrame"]->setChecked(p.playEveryFrame);
            p.actions["InPoint"]->setEnabled(playable);
            p.actions["OutPoint"]->setEnabled(playable);
            p.actions["StartFrame"]->setEnabled(playable);
            p.actions["EndFrame"]->setEnabled(playable);
            p.actions["NextFrame"]->setEnabled(playable);
            p.actions["NextFrame10"]->setEnabled(playable);
            p.actions["NextFrame100"]->setEnabled(playable);
            p.actions["PrevFrame"]->setEnabled(playable);
            p.actions["PrevFrame10"]->setEnabled(playable);
            p.actions["PrevFrame100"]->setEnabled(playable);
            p.actions["InOutPoints"]->setEnabled(playable);
            p.actions["InOutPoints"]->setChecked(p.inOutPointsEnabled);
            p.actions["SetInPoint"]->setEnabled(playable);
            p.actions["SetOutPoint"]->setEnabled(playable);
            p.actions["ResetInPoint"]->setEnabled(playable);
            p.actions["ResetOutPoint"]->setEnabled(playable);
        }

        void PlaybackSystem::_textUpdate()
        {
            DJV_PRIVATE_PTR();
            if (p.actions.size())
            {
                p.actions["Forward"]->setText(_getText(DJV_TEXT("Forward")));
                p.actions["Forward"]->setTooltip(_getText(DJV_TEXT("Forward tooltip")));
                p.actions["Reverse"]->setText(_getText(DJV_TEXT("Reverse")));
                p.actions["Reverse"]->setTooltip(_getText(DJV_TEXT("Reverse tooltip")));
                p.actions["PlayOnce"]->setText(_getText(DJV_TEXT("Play Once")));
                p.actions["PlayOnce"]->setTooltip(_getText(DJV_TEXT("Play once tooltip")));
                p.actions["PlayLoop"]->setText(_getText(DJV_TEXT("Loop")));
                p.actions["PlayLoop"]->setTooltip(_getText(DJV_TEXT("Loop tooltip")));
                p.actions["PlayPingPong"]->setText(_getText(DJV_TEXT("Ping Pong")));
                p.actions["PlayPingPong"]->setTooltip(_getText(DJV_TEXT("Ping pong tooltip")));
                p.actions["PlayEveryFrame"]->setText(_getText(DJV_TEXT("Play Every Frame")));
                p.actions["PlayEveryFrame"]->setTooltip(_getText(DJV_TEXT("Play every frame tooltip")));
                p.actions["InPoint"]->setText(_getText(DJV_TEXT("Go to In Point")));
                p.actions["InPoint"]->setTooltip(_getText(DJV_TEXT("Go to in point tooltip")));
                p.actions["OutPoint"]->setText(_getText(DJV_TEXT("Go to Out Point")));
                p.actions["OutPoint"]->setTooltip(_getText(DJV_TEXT("Go to out point tooltip")));
                p.actions["StartFrame"]->setText(_getText(DJV_TEXT("Go to Start Frame")));
                p.actions["StartFrame"]->setTooltip(_getText(DJV_TEXT("Go to start frame tooltip")));
                p.actions["EndFrame"]->setText(_getText(DJV_TEXT("Go to End Frame")));
                p.actions["EndFrame"]->setTooltip(_getText(DJV_TEXT("Go to end frame tooltip")));
                p.actions["NextFrame"]->setText(_getText(DJV_TEXT("Next Frame")));
                p.actions["NextFrame"]->setTooltip(_getText(DJV_TEXT("Next frame tooltip")));
                p.actions["NextFrame10"]->setText(_getText(DJV_TEXT("Next Frame X10")));
                p.actions["NextFrame10"]->setTooltip(_getText(DJV_TEXT("Next frame X10 tooltip")));
                p.actions["NextFrame100"]->setText(_getText(DJV_TEXT("Next Frame X100")));
                p.actions["NextFrame100"]->setTooltip(_getText(DJV_TEXT("Next frame X100 tooltip")));
                p.actions["PrevFrame"]->setText(_getText(DJV_TEXT("Previous Frame")));
                p.actions["PrevFrame"]->setTooltip(_getText(DJV_TEXT("Previous frame tooltip")));
                p.actions["PrevFrame10"]->setText(_getText(DJV_TEXT("Previous Frame X10")));
                p.actions["PrevFrame10"]->setTooltip(_getText(DJV_TEXT("Previous frame X10 tooltip")));
                p.actions["PrevFrame100"]->setText(_getText(DJV_TEXT("Previous Frame X100")));
                p.actions["PrevFrame100"]->setTooltip(_getText(DJV_TEXT("Previous frame X100 tooltip")));
                p.actions["InOutPoints"]->setText(_getText(DJV_TEXT("Enable In/Out Points")));
                p.actions["InOutPoints"]->setTooltip(_getText(DJV_TEXT("Enable in/out points tooltip")));
                p.actions["SetInPoint"]->setText(_getText(DJV_TEXT("Set In Point")));
                p.actions["SetInPoint"]->setTooltip(_getText(DJV_TEXT("Set in point tooltip")));
                p.actions["SetOutPoint"]->setText(_getText(DJV_TEXT("Set Out Point")));
                p.actions["SetOutPoint"]->setTooltip(_getText(DJV_TEXT("Set out point tooltip")));
                p.actions["ResetInPoint"]->setText(_getText(DJV_TEXT("Reset In Point")));
                p.actions["ResetInPoint"]->setTooltip(_getText(DJV_TEXT("Reset in point tooltip")));
                p.actions["ResetOutPoint"]->setText(_getText(DJV_TEXT("Reset Out Point")));
                p.actions["ResetOutPoint"]->setTooltip(_getText(DJV_TEXT("Reset out point tooltip")));

                p.menu->setText(_getText(DJV_TEXT("Playback")));
            }
        }

    } // namespace ViewApp
} // namespace djv
