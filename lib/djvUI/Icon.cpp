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

#include <djvUI/Icon.h>

#include <djvUI/IconSystem.h>

#include <djvAV/IO.h>
#include <djvAV/Image.h>
#include <djvAV/Render2D.h>

#include <djvCore/Context.h>

using namespace djv::Core;

namespace djv
{
    namespace UI
    {
        struct Icon::Private
        {
            std::string name;
            ColorRole iconColorRole = ColorRole::Foreground;
            MetricsRole iconSizeRole = MetricsRole::Icon;
            std::future<std::shared_ptr<AV::Image::Image> > imageFuture;
            std::shared_ptr<AV::Image::Image> image;
            std::shared_ptr<IconSystem> iconSystem;
        };

        void Icon::_init(const std::shared_ptr<Context>& context)
        {
            Widget::_init(context);
            setClassName("djv::UI::Icon");
            _p->iconSystem = context->getSystemT<IconSystem>();
        }

        Icon::Icon() :
            _p(new Private)
        {}

        Icon::~Icon()
        {}

        std::shared_ptr<Icon> Icon::create(const std::shared_ptr<Context>& context)
        {
            auto out = std::shared_ptr<Icon>(new Icon);
            out->_init(context);
            return out;
        }

        const std::string & Icon::getIcon() const
        {
            return _p->name;
        }

        void Icon::setIcon(const std::string & value)
        {
            DJV_PRIVATE_PTR();
            if (auto context = getContext().lock())
            {
                if (value == p.name)
                    return;
                p.imageFuture = std::future<std::shared_ptr<AV::Image::Image> >();
                p.image.reset();
                p.name = value;
                if (!p.name.empty())
                {
                    auto iconSystem = context->getSystemT<IconSystem>();
                    const auto& style = _getStyle();
                    p.imageFuture = iconSystem->getIcon(p.name, static_cast<int>(style->getMetric(p.iconSizeRole)));
                }
                _resize();
            }
        }

        ColorRole Icon::getIconColorRole() const
        {
            return _p->iconColorRole;
        }

        void Icon::setIconColorRole(ColorRole value)
        {
            DJV_PRIVATE_PTR();
            if (value == p.iconColorRole)
                return;
            p.iconColorRole = value;
            _redraw();
        }

        MetricsRole Icon::getIconSizeRole() const
        {
            return _p->iconSizeRole;
        }

        void Icon::setIconSizeRole(MetricsRole value)
        {
            DJV_PRIVATE_PTR();
            if (value == p.iconSizeRole)
                return;
            p.iconSizeRole = value;
            _resize();
        }

        void Icon::_preLayoutEvent(Event::PreLayout & event)
        {
            DJV_PRIVATE_PTR();
            const auto& style = _getStyle();
            const float i = style->getMetric(p.iconSizeRole);

            if (p.imageFuture.valid())
            {
                try
                {
                    p.image = p.imageFuture.get();
                }
                catch (const std::exception & e)
                {
                    p.image = nullptr;
                    _log(e.what(), LogLevel::Error);
                }
                _resize();
            }

            glm::vec2 size(0.F, 0.F);
            if (p.image)
            {
                size.x = p.image->getWidth();
                size.y = p.image->getHeight();
            }
            size.x = std::max(size.x, i);
            size.y = std::max(size.y, i);
            _setMinimumSize(size + getMargin().getSize(style));
        }

        void Icon::_paintEvent(Event::Paint & event)
        {
            Widget::_paintEvent(event);
            DJV_PRIVATE_PTR();
            const auto& style = _getStyle();
            const BBox2f & g = getMargin().bbox(getGeometry(), style);
            const glm::vec2 c = g.getCenter();

            // Draw the icon.
            if (p.image && p.image->isValid())
            {
                const uint16_t w = p.image->getWidth();
                const uint16_t h = p.image->getHeight();
                glm::vec2 pos = glm::vec2(0.F, 0.F);
                switch (getHAlign())
                {
                case HAlign::Center:
                case HAlign::Fill:   pos.x = ceilf(c.x - w / 2.F); break;
                case HAlign::Left:   pos.x = g.min.x; break;
                case HAlign::Right:  pos.x = g.max.x - w; break;
                default: break;
                }
                switch (getVAlign())
                {
                case VAlign::Center:
                case VAlign::Fill:   pos.y = ceilf(c.y - h / 2.F); break;
                case VAlign::Top:    pos.y = g.min.y; break;
                case VAlign::Bottom: pos.y = g.max.y - h; break;
                default: break;
                }
                auto render = _getRender();
                if (p.iconColorRole != ColorRole::None)
                {
                    render->setFillColor(style->getColor(p.iconColorRole));
                    render->drawFilledImage(p.image, pos);
                }
                else
                {
                    render->setFillColor(AV::Image::Color(1.F, 1.F, 1.F));
                    render->drawImage(p.image, pos);
                }
            }
        }

        void Icon::_initEvent(Event::Init & event)
        {
            Widget::_initEvent(event);
            DJV_PRIVATE_PTR();
            if (!p.name.empty())
            {
                if (auto context = getContext().lock())
                {
                    auto iconSystem = context->getSystemT<IconSystem>();
                    const auto& style = _getStyle();
                    p.imageFuture = iconSystem->getIcon(p.name, static_cast<int>(style->getMetric(p.iconSizeRole)));
                }
            }
        }

    } // namespace UI
} // namespace djv
