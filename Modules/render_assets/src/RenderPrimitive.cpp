#include "render_assets/RenderPrimitive.h"
#include "render_assets/DefaultAssetProvider.h"

using namespace lcf;

RenderPrimitive::RenderPrimitive()
{
    m_geometry_sp = DefaultAssetProvider::getInstance().getGeometryResource();
    m_material_sp = DefaultAssetProvider::getInstance().getMaterialResource();
}