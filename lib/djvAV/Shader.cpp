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

#include <djvAV/Shader.h>

#include <djvCore/FileIO.h>
#include <djvCore/Path.h>

using namespace djv::Core;

namespace djv
{
    namespace AV
    {
        namespace Render
        {
            ShaderError::ShaderError(const std::string& what) :
                std::runtime_error(what)
            {}
            
            Shader::Shader()
            {}

            Shader::~Shader()
            {}

            std::shared_ptr<Shader> Shader::create(const std::string & vertex, const std::string & fragment)
            {
                auto out = std::shared_ptr<Shader>(new Shader);
                out->_vertex.second = vertex;
                out->_fragment.second = fragment;
                return out;
            }

            std::shared_ptr<Shader> Shader::create(const FileSystem::Path & vertex, const FileSystem::Path & fragment)
            {
                auto out = std::shared_ptr<Shader>(new Shader);
                try
                {
                    FileSystem::FileIO fileIO;
                    fileIO.open(std::string(vertex), FileSystem::FileIO::Mode::Read);
                    out->_vertex.second = FileSystem::FileIO::readContents(fileIO);
                    out->_vertex.first = vertex.get();
                    fileIO.open(std::string(fragment), FileSystem::FileIO::Mode::Read);
                    out->_fragment.second = FileSystem::FileIO::readContents(fileIO);
                    out->_fragment.first = fragment.get();
                }
                catch (const std::exception & e)
                {
                    std::stringstream ss;
                    ss << DJV_TEXT("The shader cannot be created") << ". " << e.what();
                    throw ShaderError(ss.str());
                }
                return out;
            }

            const std::string & Shader::getVertexName() const
            {
                return _vertex.first;
            }

            const std::string & Shader::getVertexSource() const
            {
                return _vertex.second;
            }

            const std::string & Shader::getFragmentName() const
            {
                return _fragment.first;
            }

            const std::string & Shader::getFragmentSource() const
            {
                return _fragment.second;
            }

            void Shader::setVertexName(const std::string& value)
            {
                _vertex.first = value;
            }
            
            void Shader::setFragmentName(const std::string& value)
            {
                _fragment.first = value;
            }

        } // namespace Render
    } // namespace AV
} // namespace djv
