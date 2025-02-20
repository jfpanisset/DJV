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

#include <djvUI/AVSettings.h>

#include <djvAV/AVSystem.h>
#include <djvAV/IO.h>
#include <djvAV/Render2D.h>

#include <djvCore/Context.h>

// These need to be included last on OSX.
#include <djvCore/PicoJSONTemplates.h>
#include <djvUI/ISettingsTemplates.h>

//#pragma optimize("", off)

using namespace djv::Core;

namespace djv
{
    namespace UI
    {
        namespace Settings
        {
            struct AV::Private
            {
                std::shared_ptr<djv::AV::AVSystem> avSystem;
                std::shared_ptr<djv::AV::Render::Render2D> renderSystem;
                std::shared_ptr<djv::AV::IO::System> ioSystem;
            };

            void AV::_init(const std::shared_ptr<Core::Context>& context)
            {
                ISettings::_init("djv::UI::Settings::AV", context);
                DJV_PRIVATE_PTR();
                p.avSystem = context->getSystemT<djv::AV::AVSystem>();
                p.renderSystem = context->getSystemT<djv::AV::Render::Render2D>();
                p.ioSystem = context->getSystemT<djv::AV::IO::System>();
                _load();
            }

            AV::AV() :
                _p(new Private)
            {}

            AV::~AV()
            {}

            std::shared_ptr<AV> AV::create(const std::shared_ptr<Core::Context>& context)
            {
                auto out = std::shared_ptr<AV>(new AV);
                out->_init(context);
                return out;
            }

            void AV::load(const picojson::value & value)
            {
                DJV_PRIVATE_PTR();
                if (value.is<picojson::object>())
                {
                    const auto & object = value.get<picojson::object>();
                    djv::AV::TimeUnits timeUnits = djv::AV::TimeUnits::First;
                    djv::AV::AlphaBlend alphaBlend = djv::AV::AlphaBlend::Straight;
                    Time::FPS defaultSpeed = Time::getDefaultSpeed();
                    bool lcdText = false;
                    for (const auto & i : object)
                    {
                        if ("TimeUnits" == i.first)
                        {
                            std::stringstream ss(i.second.get<std::string>());
                            ss >> timeUnits;
                        }
                        else if ("AlphaBlend" == i.first)
                        {
                            std::stringstream ss(i.second.get<std::string>());
                            ss >> alphaBlend;
                        }
                        else if ("DefaultSpeed" == i.first)
                        {
                            std::stringstream ss(i.second.get<std::string>());
                            ss >> defaultSpeed;
                        }
                        else if ("LCDText" == i.first)
                        {
                            std::stringstream ss(i.second.get<std::string>());
                            ss >> lcdText;
                        }
                    }
                    p.avSystem->setTimeUnits(timeUnits);
                    p.avSystem->setAlphaBlend(alphaBlend);
                    p.avSystem->setDefaultSpeed(defaultSpeed);
                    p.renderSystem->setLCDText(lcdText);
                    for (const auto & i : p.ioSystem->getPluginNames())
                    {
                        const auto j = object.find(i);
                        if (j != object.end())
                        {
                            p.ioSystem->setOptions(i, j->second);
                        }
                    }
                }
            }

            picojson::value AV::save()
            {
                DJV_PRIVATE_PTR();
                picojson::value out(picojson::object_type, true);
                auto & object = out.get<picojson::object>();
                {
                    std::stringstream ss;
                    ss << p.avSystem->observeTimeUnits()->get();
                    object["TimeUnits"] = picojson::value(ss.str());
                }
                {
                    std::stringstream ss;
                    ss << p.avSystem->observeAlphaBlend()->get();
                    object["AlphaBlend"] = picojson::value(ss.str());
                }
                {
                    std::stringstream ss;
                    ss << p.avSystem->observeDefaultSpeed()->get();
                    object["DefaultSpeed"] = picojson::value(ss.str());
                }
                {
                    std::stringstream ss;
                    ss << p.renderSystem->observeLCDText()->get();
                    object["LCDText"] = picojson::value(ss.str());
                }
                for (const auto & i : p.ioSystem->getPluginNames())
                {
                    object[i] = p.ioSystem->getOptions(i);
                }
                return out;
            }

        } // namespace Settings
    } // namespace UI
} // namespace djv

