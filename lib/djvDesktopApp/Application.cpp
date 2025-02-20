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

#include <djvDesktopApp/Application.h>

#include <djvDesktopApp/EventSystem.h>
#include <djvDesktopApp/GLFWSystem.h>

#include <djvUI/UISystem.h>

#include <djvAV/AVSystem.h>
#include <djvAV/GLFWSystem.h>
#include <djvAV/IO.h>
#include <djvAV/Render2D.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>

using namespace djv::Core;

namespace djv
{
    namespace Desktop
    {
        namespace
        {
            //! \todo Should this be configurable?
            const size_t frameRate = 60;
        
        } // namespace

        struct Application::Private
        {
            bool running = false;
        };

        void Application::_init(const std::vector<std::string>& args)
        {
            Context::_init(args);
            DJV_PRIVATE_PTR();

            auto avSystem = AV::AVSystem::create(shared_from_this());
            auto glfwSystem = GLFWSystem::create(shared_from_this());
            auto uiSystem = UI::UISystem::create(shared_from_this());
            auto avGLFWSystem = getSystemT<AV::GLFW::System>();
            auto glfwWindow = avGLFWSystem->getGLFWWindow();
            auto eventSystem = EventSystem::create(glfwWindow, shared_from_this());
        }
        
        Application::Application() :
            _p(new Private)
        {}
        
        Application::~Application()
        {}

        std::shared_ptr<Application> Application::create(const std::vector<std::string>& args)
        {
            auto out = std::shared_ptr<Application>(new Application);
            out->_init(args);
            return out;
        }

        int Application::run()
        {
            DJV_PRIVATE_PTR();
            auto avGLFWSystem = getSystemT<AV::GLFW::System>();
            if (auto glfwWindow = avGLFWSystem->getGLFWWindow())
            {
                glfwShowWindow(glfwWindow);
                p.running = true;
                auto time = std::chrono::system_clock::now();
                float dt = 0.F;
                while (p.running && glfwWindow && !glfwWindowShouldClose(glfwWindow))
                {
                    glfwPollEvents();
                    tick(dt);

                    auto t = std::chrono::system_clock::now();
                    std::chrono::duration<float> delta = t - time;
                    const float oneOverFrameRate = 1 / static_cast<float>(frameRate);
                    const float sleep = oneOverFrameRate - delta.count();
                    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(sleep * 1000)));

                    t = std::chrono::system_clock::now();
                    delta = t - time;
                    dt = delta.count();
                    //if (dt > oneOverFrameRate)
                    //{
                    //    std::cout << std::fixed << (dt - oneOverFrameRate) << std::endl;
                    //}
                    time = t;
                }
            }
            return 0;
        }

        void Application::exit()
        {
            _p->running = false;
        }

    } // namespace Desktop
} // namespace Gp
