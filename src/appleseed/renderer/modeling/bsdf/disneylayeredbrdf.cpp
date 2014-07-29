
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2014 Esteban Tovagliari, The appleseedhq Organization
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
#include "disneylayeredbrdf.h"

// appleseed.renderer headers
#include "renderer/modeling/bsdf/disneybrdf.h"
#include "renderer/modeling/input/inputevaluator.h"
#include "renderer/modeling/material/disneymaterial.h"

// appleseed.foundation headers.
#include "foundation/image/colorspace.h"
#include "foundation/math/scalar.h"

// Forward declarations.
namespace foundation    { class AbortSwitch; }
namespace renderer      { class Assembly; }
namespace renderer      { class Project; }

using namespace foundation;
using namespace std;

namespace renderer
{

//
// Disney Layered BRDF implementation.
//

namespace
{

    const char* Model = "disney_layered_brdf";

}

DisneyLayeredBRDF::DisneyLayeredBRDF(const DisneyMaterial* parent)
  : BSDF("disney_layered_brdf", Reflective, Diffuse | Glossy, ParamArray())
  , m_parent(parent)
  , m_brdf(DisneyBRDFFactory().create("disney_brdf", ParamArray()))
{
    assert(parent);
}

void DisneyLayeredBRDF::release()
{
    delete this;
}

const char* DisneyLayeredBRDF::get_model() const
{
    return Model;
}

bool DisneyLayeredBRDF::on_frame_begin(
    const Project&              project,
    const Assembly&             assembly,
    foundation::AbortSwitch*    abort_switch)
{
    if (!BSDF::on_frame_begin(project, assembly, abort_switch))
        return false;
    
    if (!m_brdf->on_frame_begin(project, assembly, abort_switch))
        return false;

    return true;
}

void DisneyLayeredBRDF::on_frame_end(
    const Project&              project,
    const Assembly&             assembly)
{
    m_brdf->on_frame_end(project, assembly);
    BSDF::on_frame_end(project, assembly);
}

size_t DisneyLayeredBRDF::compute_input_data_size(
    const Assembly&             assembly) const
{
    return sizeof(DisneyBRDFInputValues);
}

void DisneyLayeredBRDF::evaluate_inputs(
    InputEvaluator&             input_evaluator,
    const ShadingPoint&         shading_point,
    const size_t                offset) const
{
    char* ptr = reinterpret_cast<char*>(input_evaluator.data());
    DisneyBRDFInputValues* values = reinterpret_cast<DisneyBRDFInputValues*>(ptr + offset);
    memset(values, 0, sizeof(DisneyBRDFInputValues));

    Color3d base_color(0.0f);

    for (size_t i = 0, e = m_parent->get_layer_count(); i < e; ++i)
    {
        const DisneyMaterialLayer& layer = m_parent->get_layer(i);
        layer.evaluate_expressions(shading_point, base_color, *values);
    }

    base_color = srgb_to_linear_rgb(base_color);
    linear_rgb_reflectance_to_spectrum(Color3f(base_color), values->m_base_color);
}

BSDF::Mode DisneyLayeredBRDF::sample(
    SamplingContext&    sampling_context,
    const void*         data,
    const bool          adjoint,
    const bool          cosine_mult,
    const Vector3d&     geometric_normal,
    const Basis3d&      shading_basis,
    const Vector3d&     outgoing,
    Vector3d&           incoming,
    Spectrum&           value,
    double&             probability) const
{
    if (m_parent->get_layer_count() == 0)
    {
        value.set(0.0f);
        probability = 0.0;
        return Absorption;
    }

    return m_brdf->sample(
        sampling_context,
        data,
        adjoint,
        cosine_mult,
        geometric_normal,
        shading_basis,
        outgoing,
        incoming,
        value,
        probability);
}

double DisneyLayeredBRDF::evaluate(
    const void*         data,
    const bool          adjoint,
    const bool          cosine_mult,
    const Vector3d&     geometric_normal,
    const Basis3d&      shading_basis,
    const Vector3d&     outgoing,
    const Vector3d&     incoming,
    const int           modes,
    Spectrum&           value) const
{
    if (m_parent->get_layer_count() == 0)
    {
        value.set(0.0f);
        return 0.0;
    }

    return m_brdf->evaluate(
        data,
        adjoint,
        cosine_mult,
        geometric_normal,
        shading_basis,
        outgoing,
        incoming,
        modes,
        value);
}

double DisneyLayeredBRDF::evaluate_pdf(
    const void*         data,
    const Vector3d&     geometric_normal,
    const Basis3d&      shading_basis,
    const Vector3d&     outgoing,
    const Vector3d&     incoming,
    const int           modes) const
{
    if (m_parent->get_layer_count() == 0)
        return 0.0;

    return m_brdf->evaluate_pdf(
        data,
        geometric_normal,
        shading_basis,
        outgoing,
        incoming,
        modes);
}

}   // namespace renderer
