#pragma once

#include "rkit/Core/Module.h"

#include <glslang/Include/glslang_c_interface.h>

namespace rkit { namespace buildsystem { namespace vulkan {
	struct GlslCApi
	{
		void (*glslang_get_version)(glslang_version_t *version);

		int (*glslang_initialize_process)(void);
		void (*glslang_finalize_process)(void);

		glslang_shader_t *(*glslang_shader_create)(const glslang_input_t *input);
		void (*glslang_shader_delete)(glslang_shader_t *shader);
		void (*glslang_shader_set_preamble)(glslang_shader_t *shader, const char *s);
		void (*glslang_shader_shift_binding)(glslang_shader_t *shader, glslang_resource_type_t res, unsigned int base);
		void (*glslang_shader_shift_binding_for_set)(glslang_shader_t *shader, glslang_resource_type_t res, unsigned int base, unsigned int set);
		void (*glslang_shader_set_options)(glslang_shader_t *shader, int options); // glslang_shader_options_t
		void (*glslang_shader_set_glsl_version)(glslang_shader_t *shader, int version);
		void (*glslang_shader_set_default_uniform_block_set_and_binding)(glslang_shader_t *shader, unsigned int set, unsigned int binding);
		void (*glslang_shader_set_default_uniform_block_name)(glslang_shader_t *shader, const char *name);
		void (*glslang_shader_set_resource_set_binding)(glslang_shader_t *shader, const char *const *bindings, unsigned int num_bindings);
		int (*glslang_shader_preprocess)(glslang_shader_t *shader, const glslang_input_t *input);
		int (*glslang_shader_parse)(glslang_shader_t *shader, const glslang_input_t *input);
		const char *(*glslang_shader_get_preprocessed_code)(glslang_shader_t *shader);
		void (*glslang_shader_set_preprocessed_code)(glslang_shader_t *shader, const char *code);
		const char *(*glslang_shader_get_info_log)(glslang_shader_t *shader);
		const char *(*glslang_shader_get_info_debug_log)(glslang_shader_t *shader);

		glslang_program_t *(*glslang_program_create)(void);
		void (*glslang_program_delete)(glslang_program_t *program);
		void (*glslang_program_add_shader)(glslang_program_t *program, glslang_shader_t *shader);
		int (*glslang_program_link)(glslang_program_t *program, int messages); // glslang_messages_t
		void (*glslang_program_add_source_text)(glslang_program_t *program, glslang_stage_t stage, const char *text, size_t len);
		void (*glslang_program_set_source_file)(glslang_program_t *program, glslang_stage_t stage, const char *file);
		int (*glslang_program_map_io)(glslang_program_t *program);
		int (*glslang_program_map_io_with_resolver_and_mapper)(glslang_program_t *program, glslang_resolver_t *resolver, glslang_mapper_t *mapper);
		void (*glslang_program_SPIRV_generate)(glslang_program_t *program, glslang_stage_t stage);
		void (*glslang_program_SPIRV_generate_with_options)(glslang_program_t *program, glslang_stage_t stage, glslang_spv_options_t *spv_options);
		size_t(*glslang_program_SPIRV_get_size)(glslang_program_t *program);
		void (*glslang_program_SPIRV_get)(glslang_program_t *program, unsigned int *);
		unsigned int *(*glslang_program_SPIRV_get_ptr)(glslang_program_t *program);
		const char *(*glslang_program_SPIRV_get_messages)(glslang_program_t *program);
		const char *(*glslang_program_get_info_log)(glslang_program_t *program);
		const char *(*glslang_program_get_info_debug_log)(glslang_program_t *program);

		glslang_mapper_t *(*glslang_glsl_mapper_create)();
		void (*glslang_glsl_mapper_delete)(glslang_mapper_t *mapper);

		glslang_resolver_t *(*glslang_glsl_resolver_create)(glslang_program_t *program, glslang_stage_t stage);
		void (*glslang_glsl_resolver_delete)(glslang_resolver_t *resolver);
	};

	struct ShaderCApiGroup
	{
		const GlslCApi *m_glslApi;
	};

	struct ShaderCModuleInitParameters : public ModuleInitParameters
	{
		ShaderCApiGroup *m_outApiGroup;
	};
} } }
