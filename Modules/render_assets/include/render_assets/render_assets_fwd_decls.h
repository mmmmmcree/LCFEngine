#pragma once

#include "PointerDefs.h"

namespace lcf {
    class Geometry;
    LCF_DECLARE_POINTER_DEFS(Geometry, STDPointerDefs);

    class Texture2D;
    LCF_DECLARE_POINTER_DEFS(Texture2D, STDPointerDefs);

    class Material;
    LCF_DECLARE_POINTER_DEFS(Material, STDPointerDefs);

    class RenderPrimitive;

    class Model;

    class ModelLoader;
}