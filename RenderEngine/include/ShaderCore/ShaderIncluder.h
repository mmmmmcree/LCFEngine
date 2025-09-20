#pragma once

#include "ShaderDefs.h"
#include "PointerDefs.h"
#include <string>
#include <vector>

namespace lcf {
    class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface, public STDPointerDefs<ShaderIncluder>
    {
    public:
        void addIncludeDirectory(const char *path);
        virtual shaderc_include_result* GetInclude(
            const char* requested_source,
            shaderc_include_type type,
            const char* requesting_source,
            size_t include_depth) override;
        virtual void ReleaseInclude(shaderc_include_result* include_result) override;
    private:
        shaderc_include_result * createIncludeResult(const char *path);
    private:
        std::vector<std::string> m_search_paths;
    };
}