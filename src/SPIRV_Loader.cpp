#include <spirv_msl.hpp>

#include <fstream>
#include <iostream>

std::vector<uint32_t> load_spirv_binary(const std::string& file_name) {
    auto file = std::ifstream(file_name, std::ios::in | std::ios::binary);
    if (!file.is_open()) { return {}; }
    auto out = std::vector<uint32_t>{};

    while (!file.eof()) {
        uint32_t v;
        file.read(reinterpret_cast<char*>(&v), sizeof(uint32_t));
        out.emplace_back(v);
    }
    return out;
}

std::string spirv_shader_data_to_msl_program(const std::vector<uint32_t>& spv_binary) {
    try {
        auto compiler = spirv_cross::CompilerMSL(spv_binary);
        auto options = spirv_cross::CompilerMSL::Options{};
        options.set_msl_version(2U, 4U, 0U);
        compiler.set_msl_options(options);
        return compiler.compile();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return "#include <metal_stdlib>\n";
    }
}
