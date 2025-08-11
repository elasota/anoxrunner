#include "rkit/Core/Module.h"
#include "rkit/Core/ModuleGlue.h"

#include "rkit/ShaderC/ShaderC_DLL.h"

namespace rkit { namespace buildsystem { namespace vulkan {
	class ShaderCModule final
	{
	public:
		static Result Init(const ModuleInitParameters *initParams);
		static void Shutdown();

	private:
		static const GlslCApi ms_glslcApi;
	};

	Result ShaderCModule::Init(const ModuleInitParameters *initParams)
	{
		const ShaderCModuleInitParameters *shaderCParams = static_cast<const ShaderCModuleInitParameters *>(initParams);

		shaderCParams->m_outApiGroup->m_glslApi = &ms_glslcApi;

		return rkit::ResultCode::kOK;
	}

	void ShaderCModule::Shutdown()
	{
	}

	const GlslCApi ShaderCModule::ms_glslcApi =
	{
		glslang_get_version,

		glslang_initialize_process,
		glslang_finalize_process,

		glslang_shader_create,
		glslang_shader_delete,
		glslang_shader_set_preamble,
		glslang_shader_shift_binding,
		glslang_shader_shift_binding_for_set,
		glslang_shader_set_options,
		glslang_shader_set_glsl_version,
		glslang_shader_set_default_uniform_block_set_and_binding,
		glslang_shader_set_default_uniform_block_name,
		glslang_shader_set_resource_set_binding,
		glslang_shader_preprocess,
		glslang_shader_parse,
		glslang_shader_get_preprocessed_code,
		glslang_shader_set_preprocessed_code,
		glslang_shader_get_info_log,
		glslang_shader_get_info_debug_log,

		glslang_program_create,
		glslang_program_delete,
		glslang_program_add_shader,
		glslang_program_link,
		glslang_program_add_source_text,
		glslang_program_set_source_file,
		glslang_program_map_io,
		glslang_program_map_io_with_resolver_and_mapper,
		glslang_program_SPIRV_generate,
		glslang_program_SPIRV_generate_with_options,
		glslang_program_SPIRV_get_size,
		glslang_program_SPIRV_get,
		glslang_program_SPIRV_get_ptr,
		glslang_program_SPIRV_get_messages,
		glslang_program_get_info_log,
		glslang_program_get_info_debug_log,

		glslang_glsl_mapper_create,
		glslang_glsl_mapper_delete,

		glslang_glsl_resolver_create,
		glslang_glsl_resolver_delete,
	};
} } }


RKIT_IMPLEMENT_MODULE("RKit", "ShaderC_DLL", ::rkit::buildsystem::vulkan::ShaderCModule)
