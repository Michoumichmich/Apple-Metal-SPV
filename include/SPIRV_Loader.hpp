#pragma once

#include <string>
#include <vector>

std::vector<uint32_t> load_spirv_binary(const std::string& file_name);

std::string spirv_shader_data_to_msl_program(const std::vector<uint32_t>& spv_binary);
