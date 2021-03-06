
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
// Copyright (c) 2014-2016 Francois Beaune, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "disktexture2d.h"

// appleseed.renderer headers.
#include "renderer/global/globallogger.h"
#include "renderer/modeling/texture/texture.h"
#include "renderer/utility/messagecontext.h"
#include "renderer/utility/paramarray.h"

// appleseed.foundation headers.
#include "foundation/image/canvasproperties.h"
#include "foundation/image/colorspace.h"
#include "foundation/image/genericprogressiveimagefilereader.h"
#include "foundation/image/tile.h"
#include "foundation/platform/thread.h"
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/containers/specializedarrays.h"
#include "foundation/utility/makevector.h"
#include "foundation/utility/searchpaths.h"

// Standard headers.
#include <cstddef>
#include <string>

using namespace foundation;
using namespace std;

namespace renderer
{

namespace
{
    //
    // 2D disk texture.
    //

    const char* Model = "disk_texture_2d";

    class DiskTexture2d
      : public Texture
    {
      public:
        DiskTexture2d(
            const char*         name,
            const ParamArray&   params,
            const SearchPaths&  search_paths)
          : Texture(name, params)
          , m_reader(&global_logger())
        {
            extract_parameters(search_paths);
        }

        virtual void release() APPLESEED_OVERRIDE
        {
            delete this;
        }

        virtual const char* get_model() const APPLESEED_OVERRIDE
        {
            return Model;
        }

        virtual ColorSpace get_color_space() const APPLESEED_OVERRIDE
        {
            return m_color_space;
        }

        virtual void collect_asset_paths(StringArray& paths) const APPLESEED_OVERRIDE
        {
            if (m_params.strings().exist("filename"))
                paths.push_back(m_params.get("filename"));
        }

        virtual void update_asset_paths(const StringDictionary& mappings) APPLESEED_OVERRIDE
        {
            m_params.set("filename", mappings.get(m_params.get("filename")));
        }

        virtual const CanvasProperties& properties() APPLESEED_OVERRIDE
        {
            boost::mutex::scoped_lock lock(m_mutex);
            open_image_file();
            return m_props;
        }

        virtual Tile* load_tile(
            const size_t        tile_x,
            const size_t        tile_y) APPLESEED_OVERRIDE
        {
            boost::mutex::scoped_lock lock(m_mutex);
            open_image_file();
            return m_reader.read_tile(tile_x, tile_y);
        }

        virtual void unload_tile(
            const size_t        tile_x,
            const size_t        tile_y,
            const Tile*         tile) APPLESEED_OVERRIDE
        {
            delete tile;
        }

      private:
        string                              m_filepath;
        ColorSpace                          m_color_space;

        mutable boost::mutex                m_mutex;
        GenericProgressiveImageFileReader   m_reader;
        CanvasProperties                    m_props;

        void extract_parameters(const SearchPaths& search_paths)
        {
            const EntityDefMessageContext message_context("texture", this);

            // Establish and store the qualified path to the texture file.
            m_filepath = search_paths.qualify(m_params.get_required<string>("filename", ""));

            // Retrieve the color space.
            const string color_space =
                m_params.get_required<string>(
                    "color_space",
                    "linear_rgb",
                    make_vector("linear_rgb", "srgb", "ciexyz"),
                    message_context);
            if (color_space == "linear_rgb")
                m_color_space = ColorSpaceLinearRGB;
            else if (color_space == "srgb")
                m_color_space = ColorSpaceSRGB;
            else m_color_space = ColorSpaceCIEXYZ;
        }

        void open_image_file()
        {
            if (!m_reader.is_open())
            {
                RENDERER_LOG_INFO(
                    "opening texture file %s and reading metadata...",
                    m_filepath.c_str());

                m_reader.open(m_filepath.c_str());
                m_reader.read_canvas_properties(m_props);
            }
        }
    };
}


//
// DiskTexture2dFactory class implementation.
//

const char* DiskTexture2dFactory::get_model() const
{
    return Model;
}

Dictionary DiskTexture2dFactory::get_model_metadata() const
{
    return
        Dictionary()
            .insert("name", Model)
            .insert("label", "2D Texture File")
            .insert("default_model", "true");
}

DictionaryArray DiskTexture2dFactory::get_input_metadata() const
{
    DictionaryArray metadata;

    metadata.push_back(
        Dictionary()
            .insert("name", "filename")
            .insert("label", "File Path")
            .insert("type", "file")
            .insert("file_picker_mode", "open")
            .insert("file_picker_filter", "Texture Files (*.png;*.exr);;OpenEXR (*.exr);;PNG (*.png);;All Files (*.*)")
            .insert("use", "required"));

    metadata.push_back(
        Dictionary()
            .insert("name", "color_space")
            .insert("label", "Color Space")
            .insert("type", "enumeration")
            .insert("items",
                Dictionary()
                    .insert("Linear RGB", "linear_rgb")
                    .insert("sRGB", "srgb")
                    .insert("CIE XYZ", "ciexyz"))
            .insert("use", "required")
            .insert("default", "srgb"));

    return metadata;
}

auto_release_ptr<Texture> DiskTexture2dFactory::create(
    const char*         name,
    const ParamArray&   params,
    const SearchPaths&  search_paths) const
{
    return auto_release_ptr<Texture>(new DiskTexture2d(name, params, search_paths));
}

auto_release_ptr<Texture> DiskTexture2dFactory::static_create(
    const char*         name,
    const ParamArray&   params,
    const SearchPaths&  search_paths)
{
    return auto_release_ptr<Texture>(new DiskTexture2d(name, params, search_paths));
}

}   // namespace renderer
