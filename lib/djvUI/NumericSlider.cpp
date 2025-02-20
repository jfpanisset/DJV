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

#include <djvUI/NumericSlider.h>

#include <djvUI/DrawUtil.h>
#include <djvUI/Style.h>

#include <djvAV/Render2D.h>

#include <djvCore/Context.h>
#include <djvCore/Timer.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

using namespace djv::Core;

namespace djv
{
    namespace UI
    {
        struct NumericSlider::Private
        {
            Orientation orientation = Orientation::First;
            float handleWidth = 0.F;
            std::chrono::milliseconds delay = std::chrono::milliseconds(0);
            std::shared_ptr<Time::Timer> delayTimer;
            Event::PointerID pressedID = Event::InvalidID;
            glm::vec2 pressedPos = glm::vec2(0.F, 0.F);
            glm::vec2 prevPos = glm::vec2(0.F, 0.F);
        };

        void NumericSlider::_init(Orientation orientation, const std::shared_ptr<Context>& context)
        {
            Widget::_init(context);
            DJV_PRIVATE_PTR();
            setPointerEnabled(true);
            switch (orientation)
            {
                case Orientation::Horizontal: setVAlign(VAlign::Center); break;
                case Orientation::Vertical:   setHAlign(HAlign::Center); break;
                default: break;
            }
            p.orientation = orientation;
            p.delayTimer = Time::Timer::create(context);
        }

        NumericSlider::NumericSlider() :
            _p(new Private)
        {}

        NumericSlider::~NumericSlider()
        {}

        Orientation NumericSlider::getOrientation() const
        {
            return _p->orientation;
        }

        std::chrono::milliseconds NumericSlider::getDelay() const
        {
            return _p->delay;
        }

        void NumericSlider::setDelay(std::chrono::milliseconds value)
        {
            _p->delay = value;
        }

        bool NumericSlider::acceptFocus(TextFocusDirection)
        {
            bool out = false;
            if (isEnabled(true) && isVisible(true) && !isClipped())
            {
                takeTextFocus();
                out = true;
            }
            return out;
        }
        
        float NumericSlider::_getHandleWidth() const
        {
            return _p->handleWidth;
        }
        
        void NumericSlider::_paint(float v, float pos)
        {
            DJV_PRIVATE_PTR();
            const auto& style = _getStyle();
            const BBox2f & g = getMargin().bbox(getGeometry(), style);
            const float m = style->getMetric(MetricsRole::MarginSmall);
            const float b = style->getMetric(MetricsRole::Border);
            auto render = _getRender();
            
            if (hasTextFocus())
            {
                render->setFillColor(style->getColor(ColorRole::TextFocus));
                drawBorder(render, g, b * 2.F);
            }

            const BBox2f g2 = g.margin(-b * 2.F);
            if (_getPointerHover().size())
            {
                render->setFillColor(style->getColor(ColorRole::Hovered));
                render->drawRect(g2);
            }

            const BBox2f g3 = g2.margin(-m);
            float troughHeight = 0.F;
            switch (p.orientation)
            {
            case Orientation::Horizontal:
            {
                troughHeight = g3.h() / 3.F;
                const BBox2f g4 = BBox2f(
                    g3.min.x,
                    floorf(g3.min.y + g3.h() / 2.F - troughHeight / 2.F),
                    g3.w(),
                    troughHeight);
                render->setFillColor(style->getColor(ColorRole::Border));
                drawBorder(render, g4, b);
                render->setFillColor(style->getColor(ColorRole::Trough));
                const BBox2f g5 = g4.margin(-b);
                render->drawRect(g5);
                render->setFillColor(style->getColor(ColorRole::Checked));
                render->drawRect(BBox2f(
                    g5.min.x,
                    g5.min.y,
                    ceilf((g5.w() - p.handleWidth / 2.F) * v),
                    g5.h()));
                break;
            }
            case Orientation::Vertical:
            {
                troughHeight = g3.w() / 3.F;
                const BBox2f g4 = BBox2f(
                    floorf(g3.min.x + g3.w() / 2.F - troughHeight / 2.F),
                    g3.min.y,
                    troughHeight,
                    g3.h());
                render->setFillColor(style->getColor(ColorRole::Border));
                drawBorder(render, g4, b);
                render->setFillColor(style->getColor(ColorRole::Trough));
                const BBox2f g5 = g4.margin(-b);
                render->drawRect(g5);
                render->setFillColor(style->getColor(ColorRole::Checked));
                render->drawRect(BBox2f(
                    g5.min.x,
                    g5.min.y,
                    g5.w(),
                    ceilf((g5.h() - p.handleWidth / 2.F) * v)));
                break;
            }
            default: break;
            }

            BBox2f handleBBox;
            switch (p.orientation)
            {
            case Orientation::Horizontal:
                handleBBox = BBox2f(
                    floorf(pos - p.handleWidth / 2.F),
                    g3.min.y,
                    p.handleWidth,
                    g3.h());
                break;
            case Orientation::Vertical:
                handleBBox = BBox2f(
                    g3.min.x,
                    floorf(pos - p.handleWidth / 2.F),
                    g3.w(),
                    p.handleWidth);
                break;
            default: break;
            }
            render->setFillColor(style->getColor(ColorRole::BorderButton));
            render->drawRect(handleBBox);
            render->setFillColor(style->getColor(ColorRole::Button));
            render->drawRect(handleBBox.margin(-b));
            if (p.pressedID != Event::InvalidID)
            {
                render->setFillColor(style->getColor(ColorRole::Pressed));
                render->drawRect(handleBBox);
            }
            else if (_getPointerHover().size())
            {
                render->setFillColor(style->getColor(ColorRole::Hovered));
                render->drawRect(handleBBox);
            }
        }

        void NumericSlider::_preLayoutEvent(Event::PreLayout & event)
        {
            DJV_PRIVATE_PTR();
            const auto& style = _getStyle();
            const float s = style->getMetric(MetricsRole::Slider);
            const float m = style->getMetric(MetricsRole::MarginSmall);
            const float b = style->getMetric(MetricsRole::Border);
            const float is = style->getMetric(MetricsRole::IconSmall);
            glm::vec2 size(0.F, 0.F);
            switch (p.orientation)
            {
            case Orientation::Horizontal: size = glm::vec2(s, is); break;
            case Orientation::Vertical:   size = glm::vec2(is, s); break;
            default: break;
            }
            _setMinimumSize(size + m * 2.F + b * 4.F + getMargin().getSize(style));
        }

        void NumericSlider::_pointerEnterEvent(Event::PointerEnter & event)
        {
            if (!event.isRejected())
            {
                event.accept();
                if (isEnabled(true))
                {
                    _redraw();
                }
            }
        }

        void NumericSlider::_pointerLeaveEvent(Event::PointerLeave & event)
        {
            event.accept();
            if (isEnabled(true))
            {
                _redraw();
            }
        }

        void NumericSlider::_pointerMoveEvent(Event::PointerMove & event)
        {
            DJV_PRIVATE_PTR();
            event.accept();
            const auto & pointerInfo = event.getPointerInfo();
            if (pointerInfo.id == p.pressedID)
            {
                float v = 0.F;
                switch (p.orientation)
                {
                case Orientation::Horizontal:
                    v = pointerInfo.projectedPos.x;
                    break;
                case Orientation::Vertical:
                    v = pointerInfo.projectedPos.y;
                    break;
                default: break;
                }
                _pointerMove(v);
                if (p.delay > std::chrono::milliseconds(0) && glm::length(pointerInfo.projectedPos - p.prevPos) > 0.F)
                {
                    _resetTimer();
                }
                p.prevPos = pointerInfo.projectedPos;
                _redraw();
            }
        }

        void NumericSlider::_buttonPressEvent(Event::ButtonPress & event)
        {
            DJV_PRIVATE_PTR();
            if (p.pressedID)
                return;
            event.accept();
            takeTextFocus();
            const auto & pointerInfo = event.getPointerInfo();
            p.pressedID = pointerInfo.id;
            p.pressedPos = pointerInfo.projectedPos;
            p.prevPos = pointerInfo.projectedPos;
            float v = 0.F;
            switch (p.orientation)
            {
            case Orientation::Horizontal:
                v = pointerInfo.projectedPos.x;
                break;
            case Orientation::Vertical:
                v = pointerInfo.projectedPos.y;
                break;
            default: break;
            }
            _buttonPress(v);
            if (p.delay > std::chrono::milliseconds(0))
            {
                _resetTimer();
            }
            _redraw();
        }

        void NumericSlider::_buttonReleaseEvent(Event::ButtonRelease & event)
        {
            DJV_PRIVATE_PTR();
            const auto & pointerInfo = event.getPointerInfo();
            if (pointerInfo.id == p.pressedID)
            {
                event.accept();
                p.pressedID = Event::InvalidID;
                _buttonRelease();
                p.delayTimer->stop();
                _redraw();
            }
        }

        void NumericSlider::_keyPressEvent(Event::KeyPress& event)
        {
            Widget::_keyPressEvent(event);
            DJV_PRIVATE_PTR();
            if (!event.isAccepted() && hasTextFocus())
            {
                if (_keyPress(event.getKey()))
                {
                    event.accept();
                }
                else
                {
                    switch (event.getKey())
                    {
                    case GLFW_KEY_ESCAPE:
                        event.accept();
                        releaseTextFocus();
                        break;
                    default: break;
                    }
                }
            }
        }

        void NumericSlider::_textFocusEvent(Event::TextFocus&)
        {
            _redraw();
        }

        void NumericSlider::_textFocusLostEvent(Event::TextFocusLost&)
        {
            _redraw();
        }

        void NumericSlider::_initEvent(Event::Init& event)
        {
            Widget::_initEvent(event);
            DJV_PRIVATE_PTR();
            if (auto context = getContext().lock())
            {
                const auto& style = _getStyle();
                p.handleWidth = style->getMetric(MetricsRole::Handle);
            }
        }

        void NumericSlider::_resetTimer()
        {
            DJV_PRIVATE_PTR();
            auto weak = std::weak_ptr<NumericSlider>(std::dynamic_pointer_cast<NumericSlider>(shared_from_this()));
            p.delayTimer->start(
                p.delay,
                [weak](float)
            {
                if (auto widget = weak.lock())
                {
                    widget->_valueUpdate();
                }
            });
        }

    } // namespace UI
} // namespace djv
