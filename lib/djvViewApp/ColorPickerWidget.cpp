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

#include <djvViewApp/ColorPickerWidget.h>

#include <djvViewApp/ImageView.h>
#include <djvViewApp/Media.h>
#include <djvViewApp/MediaWidget.h>
#include <djvViewApp/WindowSystem.h>

#include <djvUIComponents/ColorPicker.h>

#include <djvUI/Action.h>
#include <djvUI/ActionGroup.h>
#include <djvUI/ColorSwatch.h>
#include <djvUI/EventSystem.h>
#include <djvUI/IntSlider.h>
#include <djvUI/Label.h>
#include <djvUI/Menu.h>
#include <djvUI/PopupMenu.h>
#include <djvUI/RowLayout.h>
#include <djvUI/ToolButton.h>

#include <djvAV/OCIOSystem.h>
#include <djvAV/ImageUtil.h>
#include <djvAV/OpenGLOffscreenBuffer.h>
#include <djvAV/Render2D.h>
#if defined(DJV_OPENGL_ES2)
#include <djvAV/OpenGLMesh.h>
#include <djvAV/OpenGLShader.h>
#include <djvAV/Shader.h>
#endif // DJV_OPENGL_ES2

#include <djvCore/Context.h>
#if defined(DJV_OPENGL_ES2)
#include <djvCore/ResourceSystem.h>
#endif // DJV_OPENGL_ES2

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_transform_2d.hpp>

using namespace djv::Core;

namespace djv
{
    namespace ViewApp
    {
        namespace
        {
            //! \todo Should this be configurable?
            const size_t sampleSizeMax = 100;

            //! \todo What is this really?
            const size_t bufferSizeMin = 100;
        
        } // namespace

        struct ColorPickerWidget::Private
        {
            int sampleSize = 1;
            AV::Image::Type colorTypeLock = AV::Image::Type::None;
            AV::Image::Color color = AV::Image::Color(0.F, 0.F, 0.F);
            glm::vec2 pickerPos = glm::vec2(0.F, 0.F);
            std::shared_ptr<AV::Image::Image> image;
            AV::Render::ImageOptions imageOptions;
            glm::vec2 imagePos = glm::vec2(0.F, 0.F);
            float imageZoom = 1.F;
            ImageRotate imageRotate = ImageRotate::First;
            UI::ImageAspectRatio imageAspectRatio = UI::ImageAspectRatio::First;
            AV::OCIO::Config ocioConfig;
            std::string outputColorSpace;
            std::shared_ptr<MediaWidget> activeWidget;

            std::map<std::string, std::shared_ptr<UI::Action> > actions;
            std::shared_ptr<UI::ColorSwatch> colorSwatch;
            std::shared_ptr<UI::Label> colorLabel;
            std::shared_ptr<UI::ToolButton> copyButton;
            std::shared_ptr<UI::IntSlider> sampleSizeSlider;
            std::shared_ptr<UI::ColorTypeWidget> typeWidget;
            std::shared_ptr<UI::Menu> menu;
            std::shared_ptr<UI::PopupMenu> popupMenu;
            std::shared_ptr<UI::VerticalLayout> layout;

            std::shared_ptr<AV::OpenGL::OffscreenBuffer> offscreenBuffer;
#if defined(DJV_OPENGL_ES2)
            std::shared_ptr<AV::OpenGL::Shader> shader;
#endif // DJV_OPENGL_ES2

            std::shared_ptr<ValueObserver<bool> > lockObserver;
            std::shared_ptr<ValueObserver<std::shared_ptr<MediaWidget> > > activeWidgetObserver;
            std::shared_ptr<ValueObserver<std::shared_ptr<AV::Image::Image> > > imageObserver;
            std::shared_ptr<ValueObserver<AV::Render::ImageOptions> > imageOptionsObserver;
            std::shared_ptr<ValueObserver<glm::vec2> > imagePosObserver;
            std::shared_ptr<ValueObserver<float> > imageZoomObserver;
            std::shared_ptr<ValueObserver<ImageRotate> > imageRotateObserver;
            std::shared_ptr<ValueObserver<UI::ImageAspectRatio> > imageAspectRatioObserver;
            std::shared_ptr<ValueObserver<AV::OCIO::Config> > ocioConfigObserver;
            std::shared_ptr<ValueObserver<PointerData> > dragObserver;
        };

        void ColorPickerWidget::_init(const std::shared_ptr<Core::Context>& context)
        {
            MDIWidget::_init(context);

            DJV_PRIVATE_PTR();
            setClassName("djv::ViewApp::ColorPickerWidget");
            
            p.actions["Lock"] = UI::Action::create();
            p.actions["Lock"]->setButtonType(UI::ButtonType::Toggle);

            p.colorSwatch = UI::ColorSwatch::create(context);
            p.colorSwatch->setBorder(false);
            p.colorSwatch->setHAlign(UI::HAlign::Fill);

            p.colorLabel = UI::Label::create(context);
            p.colorLabel->setFont(AV::Font::familyMono);
            p.colorLabel->setHAlign(UI::HAlign::Left);
            p.colorLabel->setMargin(UI::Layout::Margin(UI::MetricsRole::MarginSmall));

            p.copyButton = UI::ToolButton::create(context);
            p.copyButton->setIcon("djvIconShare");

            p.sampleSizeSlider = UI::IntSlider::create(context);
            p.sampleSizeSlider->setRange(IntRange(1, sampleSizeMax));

            p.typeWidget = UI::ColorTypeWidget::create(context);

            p.menu = UI::Menu::create(context);
            p.menu->setIcon("djvIconSettings");
            p.menu->addAction(p.actions["Lock"]);
            p.popupMenu = UI::PopupMenu::create(context);
            p.popupMenu->setMenu(p.menu);

            p.layout = UI::VerticalLayout::create(context);
            p.layout->setSpacing(UI::Layout::Spacing(UI::MetricsRole::None));
            p.layout->setBackgroundRole(UI::ColorRole::Background);
            p.layout->setShadowOverlay({ UI::Side::Top });
            p.layout->addChild(p.colorSwatch);
            p.layout->setStretch(p.colorSwatch, UI::RowStretch::Expand);
            auto hLayout = UI::HorizontalLayout::create(context);
            hLayout->setSpacing(UI::Layout::Spacing(UI::MetricsRole::None));
            hLayout->addChild(p.colorLabel);
            hLayout->setStretch(p.colorLabel, UI::RowStretch::Expand);
            hLayout->addChild(p.copyButton);
            p.layout->addChild(hLayout);
            p.layout->addChild(p.sampleSizeSlider);
            hLayout = UI::HorizontalLayout::create(context);
            hLayout->setSpacing(UI::Layout::Spacing(UI::MetricsRole::None));
            hLayout->addChild(p.typeWidget);
            hLayout->addExpander();
            hLayout->addChild(p.popupMenu);
            p.layout->addChild(hLayout);
            addChild(p.layout);

#if defined(DJV_OPENGL_ES2)
            auto resourceSystem = context->getSystemT<ResourceSystem>();
            const Core::FileSystem::Path shaderPath = resourceSystem->getPath(Core::FileSystem::ResourcePath::Shaders);
            p.shader = AV::OpenGL::Shader::create(AV::Render::Shader::create(
                Core::FileSystem::Path(shaderPath, "djvAVRender2DVertex.glsl"),
                Core::FileSystem::Path(shaderPath, "djvAVRender2DFragment.glsl")));
#endif // DJV_OPENGL_ES2

            _sampleUpdate();
            _widgetUpdate();

            auto weak = std::weak_ptr<ColorPickerWidget>(std::dynamic_pointer_cast<ColorPickerWidget>(shared_from_this()));
            p.copyButton->setClickedCallback(
                [weak]
                {
                    if (auto widget = weak.lock())
                    {
                        if (auto eventSystem = widget->_getEventSystem().lock())
                        {
                            eventSystem->setClipboard(AV::Image::Color::getLabel(widget->_p->color));
                        }
                    }
                });

            p.sampleSizeSlider->setValueCallback(
                [weak](int value)
                {
                    if (auto widget = weak.lock())
                    {
                        widget->_p->sampleSize = value;
                        widget->_sampleUpdate();
                        widget->_widgetUpdate();
                    }
                });

            p.typeWidget->setTypeCallback(
                [weak](AV::Image::Type value)
                {
                    if (auto widget = weak.lock())
                    {
                        widget->_p->color = widget->_p->color.convert(value);
                        if (widget->_p->colorTypeLock != AV::Image::Type::None)
                        {
                            widget->_p->colorTypeLock = value;
                        }
                        widget->_widgetUpdate();
                    }
                });

            p.lockObserver = ValueObserver<bool>::create(
                p.actions["Lock"]->observeChecked(),
                [weak](bool value)
                {
                    if (auto widget = weak.lock())
                    {
                        if (value)
                        {
                            widget->_p->colorTypeLock = widget->_p->typeWidget->getType();
                        }
                        else
                        {
                            widget->_p->colorTypeLock = AV::Image::Type::None;
                        }
                    }
                });
        
            if (auto windowSystem = context->getSystemT<WindowSystem>())
            {
                p.activeWidgetObserver = ValueObserver<std::shared_ptr<MediaWidget> >::create(
                    windowSystem->observeActiveWidget(),
                    [weak](const std::shared_ptr<MediaWidget>& value)
                    {
                        if (auto widget = weak.lock())
                        {
                            widget->_p->activeWidget = value;
                            if (widget->_p->activeWidget)
                            {
                                widget->_p->imageObserver = ValueObserver<std::shared_ptr<AV::Image::Image> >::create(
                                    widget->_p->activeWidget->getImageView()->observeImage(),
                                    [weak](const std::shared_ptr<AV::Image::Image>& value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->image = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->imageOptionsObserver = ValueObserver<AV::Render::ImageOptions>::create(
                                    widget->_p->activeWidget->getImageView()->observeImageOptions(),
                                    [weak](const AV::Render::ImageOptions& value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->imageOptions = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->imagePosObserver = ValueObserver<glm::vec2>::create(
                                    widget->_p->activeWidget->getImageView()->observeImagePos(),
                                    [weak](const glm::vec2& value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->imagePos = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->imageZoomObserver = ValueObserver<float>::create(
                                    widget->_p->activeWidget->getImageView()->observeImageZoom(),
                                    [weak](float value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->imageZoom = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->imageRotateObserver = ValueObserver<ImageRotate>::create(
                                    widget->_p->activeWidget->getImageView()->observeImageRotate(),
                                    [weak](ImageRotate value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->imageRotate = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->imageAspectRatioObserver = ValueObserver<UI::ImageAspectRatio>::create(
                                    widget->_p->activeWidget->getImageView()->observeImageAspectRatio(),
                                    [weak](UI::ImageAspectRatio value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->imageAspectRatio = value;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });

                                widget->_p->dragObserver = ValueObserver<PointerData>::create(
                                    widget->_p->activeWidget->observeDrag(),
                                    [weak](const PointerData& value)
                                    {
                                        if (auto widget = weak.lock())
                                        {
                                            widget->_p->pickerPos = value.pos;
                                            widget->_sampleUpdate();
                                            widget->_widgetUpdate();
                                        }
                                    });
                            }
                            else
                            {
                                widget->_p->imageObserver.reset();
                                widget->_p->imageOptionsObserver.reset();
                                widget->_p->imagePosObserver.reset();
                                widget->_p->imageZoomObserver.reset();
                                widget->_p->imageRotateObserver.reset();
                                widget->_p->imageAspectRatioObserver.reset();
                                widget->_p->dragObserver.reset();
                            }
                        }
                    });
            }

            auto ocioSystem = context->getSystemT<AV::OCIO::System>();
            auto contextWeak = std::weak_ptr<Context>(context);
            p.ocioConfigObserver = ValueObserver<AV::OCIO::Config>::create(
                ocioSystem->observeCurrentConfig(),
                [weak, contextWeak](const AV::OCIO::Config& value)
                {
                    if (auto context = contextWeak.lock())
                    {
                        if (auto widget = weak.lock())
                        {
                            auto ocioSystem = context->getSystemT<AV::OCIO::System>();
                            widget->_p->ocioConfig = value;
                            widget->_p->outputColorSpace = ocioSystem->getColorSpace(value.display, value.view);
                            widget->_sampleUpdate();
                            widget->_widgetUpdate();
                        }
                    }
                });
        }

        ColorPickerWidget::ColorPickerWidget() :
            _p(new Private)
        {}

        ColorPickerWidget::~ColorPickerWidget()
        {}

        std::shared_ptr<ColorPickerWidget> ColorPickerWidget::create(const std::shared_ptr<Core::Context>& context)
        {
            auto out = std::shared_ptr<ColorPickerWidget>(new ColorPickerWidget);
            out->_init(context);
            return out;
        }

        int ColorPickerWidget::getSampleSize() const
        {
            return _p->sampleSize;
        }

        void ColorPickerWidget::setSampleSize(int value)
        {
            DJV_PRIVATE_PTR();
            if (value == p.sampleSize)
                return;
            p.sampleSize = value;
            _widgetUpdate();
            _redraw();
        }

        AV::Image::Type ColorPickerWidget::getColorTypeLock() const
        {
            return _p->colorTypeLock;
        }

        void ColorPickerWidget::setColorTypeLock(AV::Image::Type value)
        {
            DJV_PRIVATE_PTR();
            if (value == p.colorTypeLock)
                return;
            p.colorTypeLock = value;
            if (p.colorTypeLock != AV::Image::Type::None)
            {
                p.color = p.color.convert(p.colorTypeLock);
            }
            _widgetUpdate();
            _redraw();
        }

        const glm::vec2& ColorPickerWidget::getPickerPos() const
        {
            return _p->pickerPos;
        }

        void ColorPickerWidget::setPickerPos(const glm::vec2& value)
        {
            DJV_PRIVATE_PTR();
            if (value == p.pickerPos)
                return;
            p.pickerPos = value;
            _sampleUpdate();
            _widgetUpdate();
            _redraw();
        }

        void ColorPickerWidget::_textUpdateEvent(Event::TextUpdate & event)
        {
            MDIWidget::_textUpdateEvent(event);
            DJV_PRIVATE_PTR();

            setTitle(_getText(DJV_TEXT("Color Picker")));

            p.actions["Lock"]->setText(_getText(DJV_TEXT("Lock color type")));
            p.actions["Lock"]->setTooltip(_getText(DJV_TEXT("Color picker lock color type tooltip")));

            p.sampleSizeSlider->setTooltip(_getText(DJV_TEXT("Color picker sample size tooltip")));

            p.copyButton->setTooltip(_getText(DJV_TEXT("Color picker copy tooltip")));
        }

        void ColorPickerWidget::_sampleUpdate()
        {
            DJV_PRIVATE_PTR();
            if (p.image && p.image->isValid())
            {
                try
                {
                    const AV::Image::Type type = p.colorTypeLock != AV::Image::Type::None ? p.colorTypeLock : p.image->getType();
                    const size_t sampleSize = std::max(static_cast<size_t>(ceilf(p.sampleSize * p.imageZoom)), bufferSizeMin);
                    const AV::Image::Info info(sampleSize, sampleSize, type);
                    if (p.offscreenBuffer)
                    {
                        if (info != p.offscreenBuffer->getInfo())
                        {
                            p.offscreenBuffer = AV::OpenGL::OffscreenBuffer::create(info);
                        }
                    }
                    else
                    {
                        p.offscreenBuffer = AV::OpenGL::OffscreenBuffer::create(info);
                    }
                    p.offscreenBuffer->bind();
                    const auto& render = _getRender();
                    render->beginFrame(info.size);
                    render->setFillColor(AV::Image::Color(1.F, 1.F, 1.F));
                    glm::mat3x3 m(1.F);
                    m = glm::translate(m, glm::vec2(p.sampleSize * p.imageZoom / 2.F, p.sampleSize * p.imageZoom / 2.F) - p.pickerPos);
                    m = glm::translate(m, p.imagePos);
                    m = glm::rotate(m, Math::deg2rad(getImageRotate(p.imageRotate)));
                    m = glm::scale(m, glm::vec2(
                        p.imageZoom * UI::getPixelAspectRatio(p.imageAspectRatio, p.image->getInfo().pixelAspectRatio),
                        p.imageZoom * UI::getAspectRatioScale(p.imageAspectRatio, p.image->getAspectRatio())));
                    render->pushTransform(m);
                    AV::Render::ImageOptions options(p.imageOptions);
                    auto i = p.ocioConfig.colorSpaces.find(p.image->getPluginName());
                    if (i != p.ocioConfig.colorSpaces.end())
                    {
                        options.colorSpace.input = i->second;
                    }
                    else
                    {
                        i = p.ocioConfig.colorSpaces.find(std::string());
                        if (i != p.ocioConfig.colorSpaces.end())
                        {
                            options.colorSpace.input = i->second;
                        }
                    }
                    options.colorSpace.output = p.outputColorSpace;
                    options.cache = AV::Render::ImageCache::Dynamic;
                    render->drawImage(p.image, glm::vec2(0.F, 0.F), options);
                    render->popTransform();
                    render->endFrame();
                    auto data = AV::Image::Data::create(AV::Image::Info(
                        ceilf(p.sampleSize * p.imageZoom),
                        ceilf(p.sampleSize * p.imageZoom),
                        type));
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, p.offscreenBuffer->getID());
                    glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glReadPixels(
                        0,
                        static_cast<int>(sampleSize) - static_cast<int>(data->getHeight()),
                        data->getWidth(),
                        data->getHeight(),
                        info.getGLFormat(),
                        info.getGLType(),
                        data->getData());
                    p.color = AV::Image::getAverageColor(data);
                }
                catch (const std::exception& e)
                {
                    std::stringstream ss;
                    ss << _getText(DJV_TEXT("Cannot sample color")) << ". " << e.what();
                    _log(ss.str(), LogLevel::Error);
                }
            }
            else if (p.offscreenBuffer)
            {
                p.offscreenBuffer.reset();
            }
        }

        void ColorPickerWidget::_widgetUpdate()
        {
            DJV_PRIVATE_PTR();

            const AV::Image::Type type = p.color.getType();
            p.typeWidget->setType(type);

            const bool lock = p.colorTypeLock != AV::Image::Type::None;
            p.actions["Lock"]->setChecked(lock);

            p.colorSwatch->setColor(p.color);
            p.colorLabel->setText(AV::Image::Color::getLabel(p.color));
            p.sampleSizeSlider->setValue(p.sampleSize);
        }

    } // namespace ViewApp
} // namespace djv
