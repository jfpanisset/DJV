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

#include <djvAV/Render2D.h>

#include <djvAV/Color.h>
#include <djvAV/GLFWSystem.h>
#include <djvAV/OpenGLMesh.h>
#include <djvAV/OpenGLShader.h>
#include <djvAV/OpenGLTexture.h>
#include <djvAV/Shader.h>
#include <djvAV/Shape.h>
#include <djvAV/TextureAtlas.h>
#include <djvAV/TriangleMesh.h>

#include <djvCore/Cache.h>
#include <djvCore/Context.h>
#include <djvCore/FileIO.h>
#include <djvCore/LogSystem.h>
#include <djvCore/Range.h>
#include <djvCore/ResourceSystem.h>
#include <djvCore/Timer.h>

#include <OpenColorIO/OpenColorIO.h>

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/perpendicular.hpp>

using namespace djv::Core;
namespace _OCIO = OCIO_NAMESPACE;

namespace djv
{
    namespace AV
    {
        namespace Render
        {
            namespace
            {
                //! \todo Should this be configurable?
                const uint8_t  textureAtlasCount      = 4;
                const uint16_t textureAtlasSize       = 8192;
                const size_t   dynamicTextureIDCount  = 16;
                const size_t   dynamicTextureCacheMax = 16;
#if !defined(DJV_OPENGL_ES2)
                const size_t   lut3DSize              = 32;
                const size_t   colorSpaceCacheMax     = 32;
#endif // DJV_OPENGL_ES2

                // This enumeration provides how the color is used to draw the render primitive.
                enum class ColorMode
                {
                    SolidColor,             // Use the uniform variable "color"
                    ColorWithTextureAlpha,  // Use the uniform variable "color" with the alpha multiplied
                                            // by the red channel from the texture (e.g., used for
                                            // drawing text)
                    ColorWithTextureAlphaR, // Used for drawing text with LCD sub-sampling
                    ColorWithTextureAlphaG,
                    ColorWithTextureAlphaB,
                    ColorAndTexture,        // Use the uniform variable "color" multiplied by the texture     
                    Shadow                  // Use the uniform variable "color" multiplied by the "U" texture coordinate
                };

                //! This struct provides data used to draw the render primitive.
                struct PrimitiveData
                {
                    // Used as an offset to find textures.
                    uint8_t textureAtlasCount = 0;

                    // Shader uniform variable locations.
                    GLint colorModeLoc          = 0;
                    GLint colorLoc              = 0;
                    GLint imageChannelsLoc      = 0;
#if !defined(DJV_OPENGL_ES2)
                    GLint colorSpaceLoc         = 0;
                    GLint colorSpaceSamplerLoc  = 0;
#endif // DJV_OPENGL_ES2
                    GLint colorMatrixLoc        = 0;
                    GLint colorMatrixEnabledLoc = 0;
                    GLint colorInvertLoc        = 0;
                    GLint levelsInLowLoc        = 0;
                    GLint levelsInHighLoc       = 0;
                    GLint levelsGammaLoc        = 0;
                    GLint levelsOutLowLoc       = 0;
                    GLint levelsOutHighLoc      = 0;
                    GLint levelsEnabledLoc      = 0;
                    GLint exposureVLoc          = 0;
                    GLint exposureDLoc          = 0;
                    GLint exposureKLoc          = 0;
                    GLint exposureFLoc          = 0;
                    GLint exposureEnabledLoc    = 0;
                    GLint softClipLoc           = 0;
                    GLint imageChannelLoc       = 0;
                    GLint textureSamplerLoc     = 0;
                };

                //! This class provides the base functionality for render primitives.
                class Primitive
                {
                public:
                    virtual ~Primitive() {}
                    
                    BBox2f      clipRect;
                    float       color[4]    = { 0.F, 0.F, 0.F, 0.F };
                    GLenum      type        = GL_TRIANGLES;
                    size_t      vaoOffset   = 0;
                    size_t      vaoSize     = 0;
                    AlphaBlend  alphaBlend  = AlphaBlend::Straight;
                    bool        lcdText     = false;

                    virtual void bind(const PrimitiveData& data, const std::shared_ptr<OpenGL::Shader>& shader)
                    {
                        shader->setUniform(data.colorModeLoc, static_cast<int>(ColorMode::SolidColor));
                        shader->setUniform(data.colorLoc, reinterpret_cast<const GLfloat*>(color));
                    }
                };

                //! This class provides a text render primitive.
                class TextPrimitive : public Primitive
                {
                public:
                    uint8_t atlasIndex = 0;

                    void bind(const PrimitiveData& data, const std::shared_ptr<OpenGL::Shader>& shader) override
                    {
                        if (!lcdText)
                        {
                            shader->setUniform(data.colorModeLoc, static_cast<int>(ColorMode::ColorWithTextureAlpha));
                        }
                        shader->setUniform(data.colorLoc, reinterpret_cast<const GLfloat*>(color));
                        shader->setUniform(data.textureSamplerLoc, static_cast<int>(atlasIndex));
                    }
                };

                //! This class provides an image render primitive.
                class ImagePrimitive : public Primitive
                {
                public:
                    ColorMode       colorMode           = ColorMode::ColorAndTexture;
                    Image::Channels imageChannels       = Image::Channels::RGBA;
#if !defined(DJV_OPENGL_ES2)
                    uint8_t         colorSpace          = 0;
                    GLuint          colorSpaceTextureID = 0;
#endif // DJV_OPENGL_ES2
                    glm::mat4x4     colorMatrix;
                    bool            colorMatrixEnabled  = false;
                    bool            colorInvert         = false;
                    ImageLevels     levels;
                    bool            levelsEnabled       = false;
                    float           exposureV           = 0.F;
                    float           exposureD           = 0.F;
                    float           exposureK           = 0.F;
                    float           exposureF           = 0.F;
                    bool            exposureEnabled     = false;
                    float           softClip            = 0.F;
                    ImageChannel    imageChannel        = ImageChannel::None;
                    ImageCache      imageCache          = ImageCache::Atlas;
                    uint8_t         atlasIndex          = 0;
                    GLuint          textureID           = 0;

                    void bind(const PrimitiveData& data, const std::shared_ptr<OpenGL::Shader>& shader) override
                    {
                        shader->setUniform(data.colorModeLoc, static_cast<int>(colorMode));
                        shader->setUniform(data.colorLoc, reinterpret_cast<const GLfloat*>(color));
                        shader->setUniform(data.imageChannelsLoc, static_cast<int>(imageChannels));
                        if (colorMatrixEnabled)
                        {
                            shader->setUniform(data.colorMatrixLoc, colorMatrix);
                        }
                        shader->setUniform(data.colorMatrixEnabledLoc, colorMatrixEnabled);
                        shader->setUniform(data.colorInvertLoc, colorInvert);
                        if (levelsEnabled)
                        {
                            shader->setUniform(data.levelsInLowLoc, levels.inLow);
                            shader->setUniform(data.levelsInHighLoc, levels.inHigh);
                            shader->setUniform(data.levelsGammaLoc, 1.F / levels.gamma);
                            shader->setUniform(data.levelsOutLowLoc, levels.outLow);
                            shader->setUniform(data.levelsOutHighLoc, levels.outHigh);
                        }
                        shader->setUniform(data.levelsEnabledLoc, levelsEnabled);
                        if (exposureEnabled)
                        {
                            shader->setUniform(data.exposureVLoc, exposureV);
                            shader->setUniform(data.exposureDLoc, exposureD);
                            shader->setUniform(data.exposureKLoc, exposureK);
                            shader->setUniform(data.exposureFLoc, exposureF);
                        }
                        shader->setUniform(data.exposureEnabledLoc, exposureEnabled);
                        shader->setUniform(data.softClipLoc, softClip);
#if !defined(DJV_OPENGL_ES2)
                        shader->setUniform(data.colorSpaceLoc, colorSpace);
                        if (colorSpace > 0)
                        {
                            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + data.textureAtlasCount + 1));
                            glBindTexture(GL_TEXTURE_3D, colorSpaceTextureID);
                            shader->setUniform(data.colorSpaceSamplerLoc, static_cast<int>(data.textureAtlasCount + 1));
                        }
#endif // DJV_OPENGL_ES2
                        shader->setUniform(data.imageChannelLoc, static_cast<int>(imageChannel));
                        switch (imageCache)
                        {
                        case ImageCache::Atlas:
                            shader->setUniform(data.textureSamplerLoc, static_cast<int>(atlasIndex));
                            break;
                        case ImageCache::Dynamic:
                            glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + data.textureAtlasCount));
                            glBindTexture(GL_TEXTURE_2D, textureID);
                            shader->setUniform(data.textureSamplerLoc, static_cast<int>(data.textureAtlasCount));
                            break;
                        default: break;
                        }
                    }
                };

                //! This class provides a shadow render primitive.
                class ShadowPrimitive : public Primitive
                {
                public:
                    void bind(const PrimitiveData& data, const std::shared_ptr<OpenGL::Shader>& shader) override
                    {
                        shader->setUniform(data.colorModeLoc, static_cast<int>(ColorMode::Shadow));
                        shader->setUniform(data.colorLoc, reinterpret_cast<const GLfloat*>(color));
                    }
                };

                //! This struct provides the layout for a VBO vertex.
                struct VBOVertex
                {
                    float    vx;
                    float    vy;
                    uint16_t tx;
                    uint16_t ty;
                };

#if !defined(DJV_OPENGL_ES2)

                //! This class provides a 3D lookup table for color space conversions.
                class LUT3D
                {
                    DJV_NON_COPYABLE(LUT3D);

                public:
                    explicit LUT3D(size_t edgeLen = lut3DSize) :
                        _edgeLen(edgeLen),
                        _size(3 * edgeLen * edgeLen * edgeLen),
                        _data(new float[_size])
                    {
                        glGenTextures(1, &_id);
                        glBindTexture(GL_TEXTURE_3D, _id);
                        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        GLenum internalFormat = GL_RGB;
                        glTexImage3D(
                            GL_TEXTURE_3D,
                            0,
                            internalFormat,
                            edgeLen,
                            edgeLen,
                            edgeLen,
                            0,
                            GL_RGB,
                            GL_FLOAT,
                            0);
                    }

                    ~LUT3D()
                    {
                        if (_id)
                        {
                            glDeleteTextures(1, &_id);
                            _id = 0;
                        }
                        delete[] _data;
                    }

                    size_t getEdgeLen() const { return _edgeLen; }
                    size_t getSize() const { return _size; }
                    float* getData() { return _data; }
                    const float* getData() const { return _data; }
                    GLuint getID() const { return _id; }

                    void copy()
                    {
                        glBindTexture(GL_TEXTURE_3D, _id);
                        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                        glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
                        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
                        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                        glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);
                        glTexSubImage3D(
                            GL_TEXTURE_3D,
                            0,
                            0,
                            0,
                            0,
                            _edgeLen,
                            _edgeLen,
                            _edgeLen,
                            GL_RGB,
                            GL_FLOAT,
                            _data);
                    }

                    void bind()
                    {
                        glBindTexture(GL_TEXTURE_3D, _id);
                    }

                private:
                    size_t _edgeLen = 0;
                    size_t _size = 0;
                    float* _data = nullptr;
                    GLuint _id = 0;
                };

                //! This variable provides color space conversion ids.
                size_t colorSpaceID = 1;

                //! This struct provides data for color space conversions.
                struct ColorSpaceData
                {
                    size_t                  id;
                    std::string             shaderSource;
                    std::shared_ptr<LUT3D>  lut3D;
                };

#endif // DJV_OPENGL_ES2

                // Utility function to flip the y-coordinate.
                BBox2f flip(const BBox2f& value, const Image::Size& size)
                {
                    BBox2f out;
                    out.min.x = value.min.x;
                    out.min.y = static_cast<float>(size.h) - value.max.y;
                    out.max.x = value.max.x;
                    out.max.y = static_cast<float>(size.h) - value.min.y;
                    return out;
                }

                float knee(float x, float f)
                {
                    return logf(x * f + 1.F) / f;
                }

                float knee2(float x, float y)
                {
                    float f0 = 0.F;
                    float f1 = 1.F;
                    while (knee(x, f1) > y)
                    {
                        f0 = f1;
                        f1 = f1 * 2.F;
                    }
                    for (size_t i = 0; i < 30; ++i)
                    {
                        const float f2 = (f0 + f1) / 2.F;
                        if (knee(x, f2) < y)
                        {
                            f1 = f2;
                        }
                        else
                        {
                            f0 = f2;
                        }
                    }
                    return (f0 + f1) / 2.F;
                }

            } // namespace

            glm::mat4x4 brightnessMatrix(float r, float g, float b)
            {
                return glm::mat4x4(
                      r, 0.F, 0.F, 0.F,
                    0.F,   g, 0.F, 0.F,
                    0.F, 0.F,   b, 0.F,
                    0.F, 0.F, 0.F, 1.F);
            }

            glm::mat4x4 contrastMatrix(float r, float g, float b)
            {
                return
                    glm::mat4x4(
                        1.F, 0.F, 0.F, -.5F,
                        0.F, 1.F, 0.F, -.5F,
                        0.F, 0.F, 1.F, -.5F,
                        0.F, 0.F, 0.F, 1.F) *
                    glm::mat4x4(
                          r, 0.F, 0.F, 0.F,
                        0.F,   g, 0.F, 0.F,
                        0.F, 0.F,   b, 0.F,
                        0.F, 0.F, 0.F, 1.F) *
                    glm::mat4x4(
                        1.F, 0.F, 0.F, .5F,
                        0.F, 1.F, 0.F, .5F,
                        0.F, 0.F, 1.F, .5F,
                        0.F, 0.F, 0.F, 1.F);
            }

            glm::mat4x4 saturationMatrix(float r, float g, float b)
            {
                const float s[] =
                {
                    (1.F - r) * .3086F,
                    (1.F - g) * .6094F,
                    (1.F - b) * .0820F
                };
                return glm::mat4x4(
                    s[0] + r, s[1],     s[2],     0.F,
                    s[0],     s[1] + g, s[2],     0.F,
                    s[0],     s[1],     s[2] + b, 0.F,
                     0.F,      0.F,      0.F,     1.F);
            }

            glm::mat4x4 colorMatrix(const ImageColor & in)
            {
                return
                    brightnessMatrix(in.brightness, in.brightness, in.brightness) *
                    contrastMatrix(in.contrast, in.contrast, in.contrast) *
                    saturationMatrix(in.saturation, in.saturation, in.saturation);
            }


            struct Render2D::Private
            {
                Render2D* system = nullptr;

                std::weak_ptr<Font::System>             fontSystem;
                Font::Info                              currentFont;
                std::shared_ptr<ValueSubject<bool> >    lcdText;

                BBox2f                                              viewport;
                std::vector<Primitive*>                             primitives;
                PrimitiveData                                       primitiveData;
                std::shared_ptr<TextureAtlas>                       textureAtlas;
                std::map<UID, uint64_t>                             textureIDs;
                std::map<UID, uint64_t>                             glyphTextureIDs;
                std::vector<std::shared_ptr<OpenGL::Texture> >      dynamicTextureIDs;
                std::map<UID, std::shared_ptr<OpenGL::Texture> >    dynamicTextureCache;
#if !defined(DJV_OPENGL_ES2)
                std::map<OCIO::Convert, ColorSpaceData>             colorSpaceCache;
#endif // DJV_OPENGL_ES2
                std::vector<uint8_t>                                vboData;
                size_t                                              vboDataSize         = 0;
                std::shared_ptr<OpenGL::VBO>                        vbo;
                std::shared_ptr<OpenGL::VAO>                        vao;
                std::string                                         vertexFileName;
                std::string                                         vertexSource;
                std::string                                         fragmentFileName;
                std::string                                         fragmentSource;
                std::shared_ptr<OpenGL::Shader>                     shader;
                GLint                                               mvpLoc              = 0;

                std::shared_ptr<Time::Timer>                        statsTimer;
                std::vector<float>                                  fpsSamples;
                std::chrono::time_point<std::chrono::system_clock>  fpsTime             = std::chrono::system_clock::now();

                void updateVBODataSize(size_t);

                void drawImage(
                    const std::shared_ptr<Image::Image>&,
                    const glm::vec2& pos,
                    const ImageOptions&,
                    ColorMode,
                    const glm::mat3x3& currentTransform,
                    const BBox2f& currentClipRect,
                    const float finalColor[4]);

                std::string getFragmentSource() const;
            };

            void Render2D::_init(const std::shared_ptr<Core::Context>& context)
            {
                ISystem::_init("djv::AV::Render::Render2D", context);

                DJV_PRIVATE_PTR();
                p.system = this;

                addDependency(context->getSystemT<AV::GLFW::System>());
                auto fontSystem = context->getSystemT<Font::System>();
                p.fontSystem = fontSystem;
                addDependency(fontSystem);

                p.lcdText = ValueSubject<bool>::create(true);

                GLint maxTextureUnits = 0;
                GLint maxTextureSize = 0;
                glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits);
                glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
                {
                    auto logSystem = context->getSystemT<LogSystem>();
                    std::stringstream ss;
                    ss << "Maximum OpenGL texture units: " << maxTextureUnits << "\n";
                    ss << "Maximum OpenGL texture size: " << maxTextureSize;
                    logSystem->log("djv::AV::Render::Render2D", ss.str());
                }
                const uint8_t _textureAtlasCount = std::min(maxTextureUnits, static_cast<GLint>(textureAtlasCount));
                const uint16_t _textureAtlasSize = std::min(maxTextureSize, static_cast<GLint>(textureAtlasSize));
                {
                    auto logSystem = context->getSystemT<LogSystem>();
                    std::stringstream ss;
                    ss << "Texture atlas count: " << _textureAtlasCount << "\n";
                    ss << "Texture atlas size: " << _textureAtlasSize;
                    logSystem->log("djv::AV::Render::Render2D", ss.str());
                }
                p.textureAtlas.reset(new TextureAtlas(
                    _textureAtlasCount,
                    _textureAtlasSize,
                    Image::Type::RGBA_U8,
                    GL_NEAREST,
                    0));
                p.primitiveData.textureAtlasCount = _textureAtlasCount;

                for (size_t i = 0; i < dynamicTextureIDCount; ++i)
                {
                    p.dynamicTextureIDs.push_back(AV::OpenGL::Texture::create(AV::Image::Info(), GL_LINEAR, GL_NEAREST));
                }

                auto resourceSystem = context->getSystemT<ResourceSystem>();
                const FileSystem::Path shaderPath = resourceSystem->getPath(FileSystem::ResourcePath::Shaders);
                try
                {
                    p.vertexFileName = std::string(FileSystem::Path(shaderPath, "djvAVRender2DVertex.glsl"));
                    FileSystem::FileIO io;
                    io.open(p.vertexFileName, FileSystem::FileIO::Mode::Read);
                    p.vertexSource = FileSystem::FileIO::readContents(io);
                    p.fragmentFileName = std::string(FileSystem::Path(shaderPath, "djvAVRender2DFragment.glsl"));
                    io.open(p.fragmentFileName, FileSystem::FileIO::Mode::Read);
                    p.fragmentSource = FileSystem::FileIO::readContents(io);
                }
                catch (const std::exception& e)
                {
                    auto logSystem = context->getSystemT<LogSystem>();
                    logSystem->log("djv::AV::Render::Render2D", e.what(), LogLevel::Error);
                }

                p.statsTimer = Time::Timer::create(context);
                p.statsTimer->setRepeating(true);
                p.statsTimer->start(
                    Time::getMilliseconds(Time::TimerValue::VerySlow),
                    [this](float)
                    {
                        DJV_PRIVATE_PTR();
                        std::stringstream ss;
                        ss << "Texture atlas: " << p.textureAtlas->getPercentageUsed() << "%\n";
                        ss << "Texture IDs: " << p.textureIDs.size() << "%\n";
                        ss << "Glyph texture IDs: " << p.glyphTextureIDs.size() << "\n";
                        ss << "Dynamic texture IDs: " << p.dynamicTextureIDs.size() << "\n";
                        ss << "Dynamic texture cache: " << p.dynamicTextureCache.size() << "\n";
#if !defined(DJV_OPENGL_ES2)
                        ss << "Color space cache: " << p.colorSpaceCache.size() << "\n";
#endif // DJV_OPENGL_ES2
                        ss << "VBO size: " << (p.vbo ? p.vbo->getSize() : 0);
                        _log(ss.str());
                    });

                /*p.fpsTimer = Time::Timer::create(context);
                p.fpsTimer->setRepeating(true);
                p.fpsTimer->start(
                    Time::getMilliseconds(Time::TimerValue::VerySlow),
                    [this](float)
                {
                    DJV_PRIVATE_PTR();
                    float average = 1.F;
                    for (const auto & i : p.fpsSamples)
                    {
                        average += i;
                    }
                    average /= static_cast<float>(p.fpsSamples.size());
                    std::stringstream ss;
                    ss << "FPS: " << average;
                    _log(ss.str());
                });*/
            }

            Render2D::Render2D() :
                _p(new Private)
            {}

            Render2D::~Render2D()
            {}

            std::shared_ptr<Render2D> Render2D::create(const std::shared_ptr<Core::Context>& context)
            {
                auto out = std::shared_ptr<Render2D>(new Render2D);
                out->_init(context);
                return out;
            }

            void Render2D::beginFrame(const Image::Size& size)
            {
                DJV_PRIVATE_PTR();
                _size = size;
                _currentClipRect = BBox2f(0.F, 0.F, static_cast<float>(size.w), static_cast<float>(size.h));
                p.viewport = BBox2f(0.F, 0.F, static_cast<float>(size.w), static_cast<float>(size.h));
            }

            void Render2D::endFrame()
            {
                DJV_PRIVATE_PTR();

                if (!p.shader)
                {
                    auto shader = Shader::create(p.vertexSource, p.getFragmentSource());
                    shader->setVertexName(p.vertexFileName);
                    shader->setFragmentName(p.fragmentFileName);
                    p.shader = OpenGL::Shader::create(shader);
                    const auto program = p.shader->getProgram();
                    p.mvpLoc = glGetUniformLocation(program, "transform.mvp");
                    p.primitiveData.imageChannelsLoc = glGetUniformLocation(program, "imageChannels");
#if !defined(DJV_OPENGL_ES2)
                    p.primitiveData.colorSpaceLoc = glGetUniformLocation(program, "colorSpace");
                    p.primitiveData.colorSpaceSamplerLoc = glGetUniformLocation(program, "colorSpaceSampler");
#endif // DJV_OPENGL_ES2
                    p.primitiveData.imageChannelLoc = glGetUniformLocation(program, "imageChannel");
                    p.primitiveData.colorMatrixLoc = glGetUniformLocation(program, "colorMatrix");
                    p.primitiveData.colorMatrixEnabledLoc = glGetUniformLocation(program, "colorMatrixEnabled");
                    p.primitiveData.colorInvertLoc = glGetUniformLocation(program, "colorInvert");
                    p.primitiveData.levelsInLowLoc = glGetUniformLocation(program, "levels.inLow");
                    p.primitiveData.levelsInHighLoc = glGetUniformLocation(program, "levels.inHigh");
                    p.primitiveData.levelsGammaLoc = glGetUniformLocation(program, "levels.gamma");
                    p.primitiveData.levelsOutLowLoc = glGetUniformLocation(program, "levels.outLow");
                    p.primitiveData.levelsOutHighLoc = glGetUniformLocation(program, "levels.outHigh");
                    p.primitiveData.levelsEnabledLoc = glGetUniformLocation(program, "levelsEnabled");
                    p.primitiveData.exposureVLoc = glGetUniformLocation(program, "exposure.v");
                    p.primitiveData.exposureDLoc = glGetUniformLocation(program, "exposure.d");
                    p.primitiveData.exposureKLoc = glGetUniformLocation(program, "exposure.k");
                    p.primitiveData.exposureFLoc = glGetUniformLocation(program, "exposure.f");
                    p.primitiveData.exposureEnabledLoc = glGetUniformLocation(program, "exposureEnabled");
                    p.primitiveData.softClipLoc = glGetUniformLocation(program, "softClip");
                    p.primitiveData.colorModeLoc = glGetUniformLocation(program, "colorMode");
                    p.primitiveData.colorLoc = glGetUniformLocation(program, "color");
                    p.primitiveData.textureSamplerLoc = glGetUniformLocation(program, "textureSampler");
                }
                p.shader->bind();

#if !defined(DJV_OPENGL_ES2)
                glEnable(GL_MULTISAMPLE);
#endif // DJV_OPENGL_ES2
                glEnable(GL_SCISSOR_TEST);
                glEnable(GL_BLEND);

                glViewport(
                    static_cast<GLint>(p.viewport.min.x),
                    static_cast<GLint>(p.viewport.min.y),
                    static_cast<GLsizei>(p.viewport.w()),
                    static_cast<GLsizei>(p.viewport.h()));
                glScissor(
                    static_cast<GLint>(p.viewport.min.x),
                    static_cast<GLint>(p.viewport.min.y),
                    static_cast<GLsizei>(p.viewport.w()),
                    static_cast<GLsizei>(p.viewport.h()));
                glClearColor(0.F, 0.F, 0.F, 0.F);
                glClear(GL_COLOR_BUFFER_BIT);

                const auto viewMatrix = glm::ortho(
                    p.viewport.min.x,
                    p.viewport.max.x,
                    p.viewport.max.y,
                    p.viewport.min.y,
                    -1.F, 1.F);
                p.shader->setUniform(p.mvpLoc, viewMatrix);

                const auto& atlasTextures = p.textureAtlas->getTextures();
                for (GLuint i = 0; i < static_cast<GLuint>(atlasTextures.size()); ++i)
                {
                    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + i));
                    glBindTexture(GL_TEXTURE_2D, atlasTextures[i]);
                }

                const size_t vertexByteCount = AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                if (!p.vbo || p.vboDataSize / vertexByteCount > p.vbo->getSize())
                {
                    p.vbo = OpenGL::VBO::create(p.vboDataSize / vertexByteCount, OpenGL::VBOType::Pos2_F32_UV_U16);
                    p.vao = OpenGL::VAO::create(p.vbo->getType(), p.vbo->getID());
                }
                p.vbo->copy(p.vboData, 0, p.vboDataSize);
                p.vao->bind();

                AlphaBlend currentAlphaBlend = AlphaBlend::Straight;
                bool currentLCDText = false;
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                for (size_t i = 0; i < p.primitives.size(); ++i)
                {
                    const auto& primitive = p.primitives[i];
                    const BBox2f clipRect = flip(primitive->clipRect, _size);
                    glScissor(
                        static_cast<GLint>(clipRect.min.x),
                        static_cast<GLint>(clipRect.min.y),
                        static_cast<GLsizei>(clipRect.w()),
                        static_cast<GLsizei>(clipRect.h()));
                    if (primitive->alphaBlend != currentAlphaBlend)
                    {
                        currentAlphaBlend = primitive->alphaBlend;
                        switch (currentAlphaBlend)
                        {
                        case AlphaBlend::None:
                            glBlendFunc(GL_ONE, GL_ZERO);
                            break;
                        case AlphaBlend::Straight:
                            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                            break;
                        case AlphaBlend::Premultiplied:
                            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                            break;
                        default: break;
                        }
                    }
                    if (primitive->lcdText != currentLCDText)
                    {
                        currentLCDText = primitive->lcdText;
                        if (!currentLCDText)
                        {
                            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                        }
                    }
                    primitive->bind(p.primitiveData, p.shader);
                    if (currentLCDText)
                    {
                        p.shader->setUniform(p.primitiveData.colorModeLoc, static_cast<int>(ColorMode::ColorWithTextureAlphaR));
                        glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
                        p.vao->draw(primitive->type, primitive->vaoOffset, primitive->vaoSize);
                        p.shader->setUniform(p.primitiveData.colorModeLoc, static_cast<int>(ColorMode::ColorWithTextureAlphaG));
                        glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
                        p.vao->draw(primitive->type, primitive->vaoOffset, primitive->vaoSize);
                        p.shader->setUniform(p.primitiveData.colorModeLoc, static_cast<int>(ColorMode::ColorWithTextureAlphaB));
                        glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
                        p.vao->draw(primitive->type, primitive->vaoOffset, primitive->vaoSize);
                    }
                    else
                    {
                        p.vao->draw(primitive->type, primitive->vaoOffset, primitive->vaoSize);
                    }
                }

                const auto now = std::chrono::system_clock::now();
                const std::chrono::duration<float> delta = now - p.fpsTime;
                p.fpsTime = now;
                p.fpsSamples.push_back(1.F / delta.count());
                while (p.fpsSamples.size() > 10)
                {
                    p.fpsSamples.erase(p.fpsSamples.begin());
                }

                _clipRects.clear();
                for (size_t i = 0; i < p.primitives.size(); ++i)
                {
                    delete p.primitives[i];
                }
                p.primitives.clear();
                p.vboDataSize = 0;
                while (p.dynamicTextureCache.size() > dynamicTextureCacheMax)
                {
                    auto texture = p.dynamicTextureCache.begin();
                    p.dynamicTextureIDs.push_back(texture->second);
                    p.dynamicTextureCache.erase(texture);
                }
                while (p.dynamicTextureIDs.size() > dynamicTextureIDCount)
                {
                    p.dynamicTextureIDs.pop_back();
                }
#if !defined(DJV_OPENGL_ES2)
                while (p.colorSpaceCache.size() > colorSpaceCacheMax)
                {
                    p.colorSpaceCache.erase(p.colorSpaceCache.begin());
                    p.shader.reset();
                }
#endif // DJV_OPENGL_ES2
            }
            
            void Render2D::drawPolyline(const std::vector<glm::vec2>& value)
            {
                DJV_PRIVATE_PTR();
                const size_t size = value.size();
                if (size > 1)
                {
                    std::vector<glm::vec2> pts;
                    for (size_t i = 1; i < size; ++i)
                    {
                        const glm::vec2 v = value[i] - value[i - 1];
                        const glm::vec2 perp = glm::normalize(glm::vec2(-v.y, v.x)) * _lineWidth;
                        const glm::vec2 pt[2] =
                        {
                            value[i] - perp,
                            value[i] + perp
                        };
                        for (size_t j = 0; j < 2; ++j)
                        {
                            const glm::vec3 tmp = _currentTransform * glm::vec3(pt[j].x, pt[j].y, 1.F);
                            pts.push_back(glm::vec2(tmp.x, tmp.y));
                        }
                    }
                    
                    BBox2f bbox;
                    bbox.min = bbox.max = glm::vec2(pts[0].x, pts[0].y);
                    const size_t ptsSize = pts.size();
                    for (size_t i = 1; i < ptsSize; ++i)
                    {
                        bbox.expand(pts[i]);
                    }
                    if (bbox.intersects(_currentClipRect))
                    {
                        auto primitive = new Primitive;
                        p.primitives.push_back(primitive);
                        primitive->clipRect = _currentClipRect;
                        primitive->color[0] = _finalColor[0];
                        primitive->color[1] = _finalColor[1];
                        primitive->color[2] = _finalColor[2];
                        primitive->color[3] = _finalColor[3];
                        primitive->type = GL_TRIANGLE_STRIP;
                        primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                        primitive->vaoSize = ptsSize;

                        const size_t vboDataSize = p.vboDataSize;
                        p.updateVBODataSize(ptsSize);
                        const glm::vec2* pPts = pts.data();
                        VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                        for (size_t i = 0; i < size; ++i, pPts += 2)
                        {
                            pData->vx = pPts[0].x;
                            pData->vy = pPts[0].y;
                            ++pData;
                            pData->vx = pPts[1].x;
                            pData->vy = pPts[1].y;
                            ++pData;
                        }
                    }
                }
            }
            
            void Render2D::drawRect(const BBox2f & value)
            {
                drawRects({ value });
            }

            void Render2D::drawRects(const std::vector<BBox2f>& value)
            {
                DJV_PRIVATE_PTR();
                std::vector<const BBox2f*> clipped;
                for (const auto& i : value)
                {
                    if (i.intersects(_currentClipRect))
                    {
                        clipped.push_back(&i);
                    }
                }
                const size_t clippedSize = clipped.size();
                if (clippedSize > 0)
                {
                    auto primitive = new Primitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = clippedSize * 6;

                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(clippedSize * 6);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                    for (const auto& i : clipped)
                    {
                        pData->vx = i->min.x;
                        pData->vy = i->min.y;
                        ++pData;
                        pData->vx = i->max.x;
                        pData->vy = i->min.y;
                        ++pData;
                        pData->vx = i->max.x;
                        pData->vy = i->max.y;
                        ++pData;
                        pData->vx = i->max.x;
                        pData->vy = i->max.y;
                        ++pData;
                        pData->vx = i->min.x;
                        pData->vy = i->max.y;
                        ++pData;
                        pData->vx = i->min.x;
                        pData->vy = i->min.y;
                        ++pData;
                    }
                }
            }

            void Render2D::drawPill(const Core::BBox2f& rect, size_t facets)
            {
                DJV_PRIVATE_PTR();
                if (rect.intersects(_currentClipRect))
                {
                    auto primitive = new Primitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = 3 * 2 + facets * 2 * 3;

                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(primitive->vaoSize);
                    const float h = rect.h();
                    const float radius = h / 2.F;
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                    pData->vx = rect.min.x + radius;
                    pData->vy = rect.min.y;
                    ++pData;
                    pData->vx = rect.max.x - radius;
                    pData->vy = rect.min.y;
                    ++pData;
                    pData->vx = rect.max.x - radius;
                    pData->vy = rect.max.y;
                    ++pData;
                    pData->vx = rect.max.x - radius;
                    pData->vy = rect.max.y;
                    ++pData;
                    pData->vx = rect.min.x + radius;
                    pData->vy = rect.max.y;
                    ++pData;
                    pData->vx = rect.min.x + radius;
                    pData->vy = rect.min.y;
                    ++pData;

                    for (size_t i = 0; i < facets; ++i)
                    {
                        const float x = rect.min.x + radius;
                        const float y = rect.min.y + radius;
                        pData->vx = x;
                        pData->vy = y;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 180.F + 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 180.F + 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                    }

                    for (size_t i = 0; i < facets; ++i)
                    {
                        const float x = rect.max.x - radius;
                        const float y = rect.min.y + radius;
                        pData->vx = x;
                        pData->vy = y;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 180.F + 270.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 180.F + 270.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                    }
                }
            }

            void Render2D::drawCircle(const glm::vec2 & pos, float radius, size_t facets)
            {
                DJV_PRIVATE_PTR();
                const BBox2f rect(pos.x - radius, pos.y - radius, radius * 2.F, radius * 2.F);
                if (rect.intersects(_currentClipRect))
                {
                    auto primitive = new Primitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    //! \todo Implement me!
                    //primitive->type = GL_TRIANGLE_FAN;
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = 3 * facets;

                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(3 * facets);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                    for (size_t i = 0; i < facets * 3; i += 3)
                    {
                        pData->vx = pos.x;
                        pData->vy = pos.y;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 360.F;
                        pData->vx = pos.x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = pos.y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 360.F;
                        pData->vx = pos.x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = pos.y + sinf(Math::deg2rad(degrees)) * radius;
                        ++pData;
                    }
                }
            }

            void Render2D::drawImage(
                const std::shared_ptr<Image::Image> & image,
                const glm::vec2& pos,
                const ImageOptions& options)
            {
                DJV_PRIVATE_PTR();
                p.drawImage(image, pos, options, ColorMode::ColorAndTexture, _currentTransform, _currentClipRect, _finalColor);
            }

            void Render2D::drawFilledImage(
                const std::shared_ptr<Image::Image> & image,
                const glm::vec2& pos,
                const ImageOptions& options)
            {
                DJV_PRIVATE_PTR();
                p.drawImage(image, pos, options, ColorMode::ColorWithTextureAlpha, _currentTransform, _currentClipRect, _finalColor);
            }

            void Render2D::setCurrentFont(const Font::Info & value)
            {
                _p->currentFont = value;
            }

            std::shared_ptr<IValueSubject<bool> > Render2D::observeLCDText() const
            {
                return _p->lcdText;
            }

            void Render2D::setLCDText(bool value)
            {
                _p->lcdText->setIfChanged(value);
            }

            std::vector<std::shared_ptr<Font::Glyph> > Render2D::drawText(const std::string & value, const glm::vec2 & pos)
            {
                DJV_PRIVATE_PTR();
                std::vector<std::shared_ptr<Font::Glyph> > out;
                try
                {
                    if (auto fontSystem = p.fontSystem.lock())
                    {
                        out = fontSystem->getGlyphs(value, p.currentFont).get();
                    }
                }
                catch (const std::exception& e)
                {
                    _log(e.what());
                }
                drawText(out, pos);
                return out;
            }

            void Render2D::drawText(const std::vector<std::shared_ptr<Font::Glyph> >& glyphs, const glm::vec2 & pos)
            {
                DJV_PRIVATE_PTR();

                float x = 0.F;
                int32_t rsbDeltaPrev = 0;
                struct GlyphData
                {
                    std::shared_ptr<Font::Glyph> glyph;
                    BBox2f bbox;
                    TextureAtlasItem item;
                };
                std::vector<std::vector<GlyphData> > clipped;
                uint8_t textureIndex = 0;
                for (const auto& glyph : glyphs)
                {
                    if (rsbDeltaPrev - glyph->lsbDelta > 32)
                    {
                        x -= 1.F;
                    }
                    else if (rsbDeltaPrev - glyph->lsbDelta < -31)
                    {
                        x += 1.F;
                    }
                    rsbDeltaPrev = glyph->rsbDelta;

                    if (glyph->imageData && glyph->imageData->isValid())
                    {
                        const uint16_t width = glyph->imageData->getWidth();
                        const uint16_t height = glyph->imageData->getHeight();
                        const glm::vec2& offset = glyph->offset;
                        const BBox2f bbox(pos.x + x + offset.x, pos.y - offset.y, width, height);
                        if (bbox.intersects(_currentClipRect))
                        {
                            GlyphData data;
                            data.glyph = glyph;
                            data.bbox = bbox;
                            const auto uid = glyph->imageData->getUID();
                            uint64_t id = 0;
                            const auto i = p.glyphTextureIDs.find(uid);
                            if (i != p.glyphTextureIDs.end())
                            {
                                id = i->second;
                            }
                            if (!p.textureAtlas->getItem(id, data.item))
                            {
                                id = p.textureAtlas->addItem(glyph->imageData, data.item);
                                p.glyphTextureIDs[uid] = id;
                            }
                            
                            if (data.item.textureIndex != textureIndex || 0 == clipped.size())
                            {
                                textureIndex = data.item.textureIndex;
                                clipped.push_back({});
                            }
                            
                            clipped.back().push_back(data);
                        }
                    }

                    x += glyph->advance;
                }
                
                for (const auto& i : clipped)
                {
                    auto primitive = new TextPrimitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    primitive->atlasIndex = i.front().item.textureIndex;
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = i.size() * 6;
                    primitive->lcdText = p.lcdText->get();
                    
                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(i.size() * 6);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                    for (const auto& j : i)
                    {
                        pData->vx = j.bbox.min.x;
                        pData->vy = j.bbox.min.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.min * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.min * 65535.F);
                        ++pData;
                        pData->vx = j.bbox.max.x;
                        pData->vy = j.bbox.min.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.max * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.min * 65535.F);
                        ++pData;
                        pData->vx = j.bbox.max.x;
                        pData->vy = j.bbox.max.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.max * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.max * 65535.F);
                        ++pData;
                        pData->vx = j.bbox.max.x;
                        pData->vy = j.bbox.max.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.max * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.max * 65535.F);
                        ++pData;
                        pData->vx = j.bbox.min.x;
                        pData->vy = j.bbox.max.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.min * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.max * 65535.F);
                        ++pData;
                        pData->vx = j.bbox.min.x;
                        pData->vy = j.bbox.min.y;
                        pData->tx = static_cast<uint16_t>(j.item.textureU.min * 65535.F);
                        pData->ty = static_cast<uint16_t>(j.item.textureV.min * 65535.F);
                        ++pData;
                    }
                }
            }

            void Render2D::drawShadow(const BBox2f& value, Side side)
            {
                DJV_PRIVATE_PTR();
                if (value.intersects(_currentClipRect))
                {
                    auto primitive = new ShadowPrimitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    primitive->type = GL_TRIANGLE_STRIP;
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = 4;

                    static const uint16_t u[][4] =
                    {
                        {     0,     0,     0,     0 },
                        {     0, 65535,     0, 65535 },
                        {     0,     0, 65535, 65535 },
                        { 65535,     0, 65535,     0 },
                        { 65535, 65535,     0,     0 }
                    };

                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(4);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);
                    pData->vx = value.min.x;
                    pData->vy = value.min.y;
                    pData->tx = u[static_cast<size_t>(side)][0];
                    ++pData;
                    pData->vx = value.max.x;
                    pData->vy = value.min.y;
                    pData->tx = u[static_cast<size_t>(side)][1];
                    ++pData;
                    pData->vx = value.min.x;
                    pData->vy = value.max.y;
                    pData->tx = u[static_cast<size_t>(side)][2];
                    ++pData;
                    pData->vx = value.max.x;
                    pData->vy = value.max.y;
                    pData->tx = u[static_cast<size_t>(side)][3];
                }
            }

            void Render2D::drawShadow(const BBox2f& value, float radius, size_t facets)
            {
                DJV_PRIVATE_PTR();
                if (value.intersects(_currentClipRect))
                {
                    auto primitive = new ShadowPrimitive;
                    p.primitives.push_back(primitive);
                    primitive->clipRect = _currentClipRect;
                    primitive->color[0] = _finalColor[0];
                    primitive->color[1] = _finalColor[1];
                    primitive->color[2] = _finalColor[2];
                    primitive->color[3] = _finalColor[3];
                    primitive->vaoOffset = p.vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = 5 * 2 * 3 + 4 * facets * 3;

                    const size_t vboDataSize = p.vboDataSize;
                    p.updateVBODataSize(primitive->vaoSize);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&p.vboData[vboDataSize]);

                    // Center.
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;

                    // Right.
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x;
                    pData->vy = value.min.y + radius;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x;
                    pData->vy = value.max.y - radius;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x;
                    pData->vy = value.max.y - radius;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;

                    // Left.
                    pData->vx = value.min.x;
                    pData->vy = value.min.y + radius;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x;
                    pData->vy = value.max.y - radius;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.min.x;
                    pData->vy = value.min.y + radius;
                    pData->tx = 0;
                    ++pData;

                    // Top.
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y + radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.min.y;
                    pData->tx = 0;
                    ++pData;

                    // Bottom.
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.max.x - radius;
                    pData->vy = value.max.y;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y;
                    pData->tx = 0;
                    ++pData;
                    pData->vx = value.min.x + radius;
                    pData->vy = value.max.y - radius;
                    pData->tx = 65535;
                    ++pData;

                    // Upper left.
                    float x = value.min.x + radius;
                    float y = value.min.y + radius;
                    for (size_t i = 0; i < facets; ++i)
                    {
                        pData->vx = x;
                        pData->vy = y;
                        pData->tx = 65535;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 90.F + 180.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 90.F + 180.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                    }

                    // Upper right.
                    x = value.max.x - radius;
                    y = value.min.y + radius;
                    for (size_t i = 0; i < facets; ++i)
                    {
                        pData->vx = x;
                        pData->vy = y;
                        pData->tx = 65535;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 90.F + 270.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 90.F + 270.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                    }

                    // Lower right.
                    x = value.max.x - radius;
                    y = value.max.y - radius;
                    for (size_t i = 0; i < facets; ++i)
                    {
                        pData->vx = x;
                        pData->vy = y;
                        pData->tx = 65535;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                    }

                    // Lower left.
                    x = value.min.x + radius;
                    y = value.max.y - radius;
                    for (size_t i = 0; i < facets; ++i)
                    {
                        pData->vx = x;
                        pData->vy = y;
                        pData->tx = 65535;
                        ++pData;
                        float degrees = i / static_cast<float>(facets) * 90.F + 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                        degrees = (i + 1) / static_cast<float>(facets) * 90.F + 90.F;
                        pData->vx = x + cosf(Math::deg2rad(degrees)) * radius;
                        pData->vy = y + sinf(Math::deg2rad(degrees)) * radius;
                        pData->tx = 0;
                        ++pData;
                    }
                }
            }

            float Render2D::getTextureAtlasPercentage() const
            {
                return _p->textureAtlas->getPercentageUsed();
            }

            size_t Render2D::getDynamicTextureCount() const
            {
                return _p->dynamicTextureCache.size();
            }

            size_t Render2D::getVBOSize() const
            {
                return _p->vbo ? _p->vbo->getSize() : 0;
            }

            void Render2D::Private::updateVBODataSize(size_t value)
            {
                const size_t vertexByteCount = AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                vboDataSize += value * vertexByteCount;
                if (vboDataSize > vboData.size())
                {
                    vboData.resize(vboDataSize);
                }
            }

            void Render2D::Private::drawImage(
                const std::shared_ptr<Image::Image>& image,
                const glm::vec2& pos,
                const ImageOptions& options,
                ColorMode colorMode,
                const glm::mat3x3& currentTransform,
                const BBox2f& currentClipRect,
                const float finalColor[4])
            {
                const auto& info = image->getInfo();

                static glm::vec3 pts[4];
                pts[0].x = pos.x;
                pts[0].y = pos.y;
                pts[0].z = 1.F;
                pts[1].x = pos.x + info.size.w;
                pts[1].y = pos.y;
                pts[1].z = 1.F;
                pts[2].x = pos.x + info.size.w;
                pts[2].y = pos.y + info.size.h;
                pts[2].z = 1.F;
                pts[3].x = pos.x;
                pts[3].y = pos.y + info.size.h;
                pts[3].z = 1.F;
                for (auto& i : pts)
                {
                    i = currentTransform * i;
                }

                static BBox2f bbox;
                bbox.min = pts[0];
                bbox.max = pts[0];
                for (size_t i = 1; i < 4; ++i)
                {
                    bbox.min.x = std::min(bbox.min.x, pts[i].x);
                    bbox.max.x = std::max(bbox.max.x, pts[i].x);
                    bbox.min.y = std::min(bbox.min.y, pts[i].y);
                    bbox.max.y = std::max(bbox.max.y, pts[i].y);
                }

                if (bbox.intersects(currentClipRect))
                {
                    auto primitive = new ImagePrimitive;
                    primitives.push_back(primitive);
                    primitive->clipRect = currentClipRect;
                    primitive->imageChannels = Image::getChannels(info.type);
                    primitive->colorMode = colorMode;
                    primitive->color[0] = finalColor[0];
                    primitive->color[1] = finalColor[1];
                    primitive->color[2] = finalColor[2];
                    primitive->color[3] = finalColor[3];
                    primitive->imageChannel = options.channel;
                    primitive->alphaBlend = options.alphaBlend;
                    primitive->colorMatrixEnabled = options.colorEnabled;
                    if (primitive->colorMatrixEnabled)
                    {
                        primitive->colorMatrix = colorMatrix(options.color);
                    }
                    primitive->colorInvert = options.color.invert;
                    primitive->levels = options.levels;
                    primitive->levelsEnabled = options.levelsEnabled;
                    primitive->exposureEnabled = options.exposureEnabled;
                    if (primitive->exposureEnabled)
                    {
                        primitive->exposureV = powf(
                            2.F,
                            options.exposure.exposure + 2.47393F);
                        primitive->exposureD = options.exposure.defog;
                        primitive->exposureK = powf(
                            2.F,
                            options.exposure.kneeLow);
                        primitive->exposureF = knee2(
                            powf(2.F, options.exposure.kneeHigh) -
                            primitive->exposureK,
                            powf(2.F, 3.5F) - primitive->exposureK);
                    }
                    primitive->softClip = options.softClip;
                    primitive->imageCache = options.cache;
                    FloatRange textureU;
                    FloatRange textureV;
                    const UID uid = image->getUID();
                    switch (options.cache)
                    {
                    case ImageCache::Atlas:
                    {
                        TextureAtlasItem item;
                        uint64_t id = 0;
                        const auto i = textureIDs.find(uid);
                        if (i != textureIDs.end())
                        {
                            id = i->second;
                        }
                        if (!textureAtlas->getItem(id, item))
                        {
                            textureIDs[uid] = textureAtlas->addItem(image, item);
                        }
                        primitive->atlasIndex = item.textureIndex;
                        if (info.layout.mirror.x)
                        {
                            textureU.min = item.textureU.max;
                            textureU.max = item.textureU.min;
                        }
                        else
                        {
                            textureU = item.textureU;
                        }
                        if (info.layout.mirror.y)
                        {
                            textureV.min = item.textureV.max;
                            textureV.max = item.textureV.min;
                        }
                        else
                        {
                            textureV = item.textureV;
                        }
                        break;
                    }
                    case ImageCache::Dynamic:
                    {
                        const auto i = dynamicTextureCache.find(uid);
                        if (i != dynamicTextureCache.end())
                        {
                            primitive->textureID = i->second->getID();
                        }
                        else
                        {
                            std::shared_ptr<OpenGL::Texture> texture;
                            if (dynamicTextureIDs.size())
                            {
                                texture = dynamicTextureIDs.back();
                                dynamicTextureIDs.pop_back();
                                texture->set(image->getInfo());
                            }
                            else
                            {
                                texture = OpenGL::Texture::create(image->getInfo(), GL_LINEAR, GL_NEAREST);
                            }
                            texture->copy(*image);
                            dynamicTextureCache[uid] = texture;
                            primitive->textureID = texture->getID();
                        }
                        if (info.layout.mirror.x)
                        {
                            textureU.min = 1.F;
                            textureU.max = 0.F;
                        }
                        else
                        {
                            textureU.min = 0.F;
                            textureU.max = 1.F;
                        }
                        if (info.layout.mirror.y)
                        {
                            textureV.min = 1.F;
                            textureV.max = 0.F;
                        }
                        else
                        {
                            textureV.min = 0.F;
                            textureV.max = 1.F;
                        }
                        break;
                    }
                    default: break;
                    }
                    if (options.mirror.x)
                    {
                        textureU.min = 1.F - textureU.min;
                        textureU.max = 1.F - textureU.max;
                    }
                    if (options.mirror.y)
                    {
                        textureV.min = 1.F - textureV.min;
                        textureV.max = 1.F - textureV.max;
                    }
#if !defined(DJV_OPENGL_ES2)
                    if (options.colorSpace.isValid())
                    {
                        ColorSpaceData colorSpaceData;
                        const auto i = colorSpaceCache.find(options.colorSpace);
                        if (i != colorSpaceCache.end())
                        {
                            colorSpaceData = i->second;
                        }
                        else
                        {
                            try
                            {
                                colorSpaceData.id = colorSpaceID++;
                                colorSpaceData.lut3D.reset(new LUT3D);
                                auto config = _OCIO::GetCurrentConfig();
                                auto processor = config->getProcessor(options.colorSpace.input.c_str(), options.colorSpace.output.c_str());
                                _OCIO::GpuShaderDesc shaderDesc;
                                shaderDesc.setLanguage(_OCIO::GPU_LANGUAGE_GLSL_1_3);
                                std::stringstream ss;
                                ss << "colorSpace" << colorSpaceData.id;
                                shaderDesc.setFunctionName(ss.str().c_str());
                                shaderDesc.setLut3DEdgeLen(colorSpaceData.lut3D->getEdgeLen());
                                colorSpaceData.shaderSource = processor->getGpuShaderText(shaderDesc);
                                size_t index = colorSpaceData.shaderSource.find("texture3D");
                                if (index != std::string::npos)
                                {
                                    colorSpaceData.shaderSource.replace(index, std::string("texture3D").size(), "texture");
                                }
                                auto data = colorSpaceData.lut3D->getData();
                                processor->getGpuLut3D(data, shaderDesc);
                                colorSpaceData.lut3D->copy();
                                colorSpaceCache[options.colorSpace] = colorSpaceData;
                                shader.reset();
                            }
                            catch (const std::exception& e)
                            {
                                system->_log(e.what());
                            }
                        }
                        primitive->colorSpace = colorSpaceData.id;
                        primitive->colorSpaceTextureID = colorSpaceData.lut3D ? colorSpaceData.lut3D->getID() : 0;
                    }
#endif // DJV_OPENGL_ES2
                    primitive->type = GL_TRIANGLE_STRIP;
                    primitive->vaoOffset = vboDataSize / AV::OpenGL::getVertexByteCount(OpenGL::VBOType::Pos2_F32_UV_U16);
                    primitive->vaoSize = 4;

                    const size_t vboDataSize = this->vboDataSize;
                    updateVBODataSize(4);
                    VBOVertex* pData = reinterpret_cast<VBOVertex*>(&vboData[vboDataSize]);
                    pData->vx = pts[0].x;
                    pData->vy = pts[0].y;
                    pData->tx = static_cast<uint16_t>(textureU.min * 65535.F);
                    pData->ty = static_cast<uint16_t>(textureV.min * 65535.F);
                    ++pData;
                    pData->vx = pts[1].x;
                    pData->vy = pts[1].y;
                    pData->tx = static_cast<uint16_t>(textureU.max * 65535.F);
                    pData->ty = static_cast<uint16_t>(textureV.min * 65535.F);
                    ++pData;
                    pData->vx = pts[3].x;
                    pData->vy = pts[3].y;
                    pData->tx = static_cast<uint16_t>(textureU.min * 65535.F);
                    pData->ty = static_cast<uint16_t>(textureV.max * 65535.F);
                    ++pData;
                    pData->vx = pts[2].x;
                    pData->vy = pts[2].y;
                    pData->tx = static_cast<uint16_t>(textureU.max * 65535.F);
                    pData->ty = static_cast<uint16_t>(textureV.max * 65535.F);
                }
            }

            std::string Render2D::Private::getFragmentSource() const
            {
                std::string out = fragmentSource;

                std::string functions;
                std::string body;
#if !defined(DJV_OPENGL_ES2)
                size_t i = 0;
                for (const auto& j : colorSpaceCache)
                {
                    functions += j.second.shaderSource;
                    {
                        std::stringstream ss;
                        if (0 == i)
                        {
                            ss << "    if (" << j.second.id << " == colorSpace)\n";
                        }
                        else
                        {
                            ss << "    else if (" << j.second.id << " == colorSpace)\n";
                        }
                        ss << "    {\n";
                        ss << "        t = colorSpace" << j.second.id << "(t, colorSpaceSampler);\n";
                        ss << "    }\n";
                        body += ss.str();
                    }
                    ++i;
                }

                std::string token = "//$colorSpaceFunctions";
                i = out.find(token);
                if (i != std::string::npos)
                {
                    out.replace(i, token.size(), functions);
                }
                token = "//$colorSpaceBody";
                i = out.find(token);
                if (i != std::string::npos)
                {
                    out.replace(i, token.size(), body);
                }
#endif // DJV_OPENGL_ES2

                return out;
            }

        } // namespace Render
    } // namespace AV
    
    DJV_ENUM_SERIALIZE_HELPERS_IMPLEMENTATION(
        AV::Render,
        ImageChannel,
        DJV_TEXT("None"),
        DJV_TEXT("Red"),
        DJV_TEXT("Green"),
        DJV_TEXT("Blue"),
        DJV_TEXT("Alpha"));

    DJV_ENUM_SERIALIZE_HELPERS_IMPLEMENTATION(
        AV::Render,
        ImageCache,
        DJV_TEXT("Atlas"),
        DJV_TEXT("Dynamic"));

} // namespace djv
