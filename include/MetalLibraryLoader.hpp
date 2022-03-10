#pragma once

#include <AppleMetal.hpp>
#include <SPIRV_Loader.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>

static inline std::string gen_random_id() {
    static std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<size_t> distribution{};
    size_t i = distribution(engine);
    return std::to_string(i);
}

static inline void report_error(NS::Error* error) {
    if (auto msg = error->description()->cString(NS::StringEncoding::ASCIIStringEncoding)) { std::cerr << msg << std::endl; }

    if (auto msg = error->localizedFailureReason()->cString(NS::StringEncoding::ASCIIStringEncoding)) { std::cerr << msg << std::endl; }
}

class MetalLibraryLoader {
private:
    static constexpr bool default_value_use_runtime_build = false;
    MTL::Device* device_;
    std::map<std::string, MTL::Library*> cached_libraries;

public:
    explicit MetalLibraryLoader(MTL::Device* device) : device_(device) {}

    ~MetalLibraryLoader() {
        for (auto const& [key, value]: cached_libraries) {
            if (value) { value->release(); }
        }
    }

private:
    MTL::Library* lookup_cache(const std::string& key) {
        auto it = cached_libraries.find(key);
        if (it != cached_libraries.end()) { return it->second; }
        return nullptr;
    }

    MTL::Library* load_library_from_library_file(const std::string& metallib_path) {
        NS::Error* error = nullptr;
        MTL::Library* lib = device_->newLibrary(NS::String::string(metallib_path.c_str(), NS::StringEncoding::ASCIIStringEncoding), &error);
        if (error) {
            report_error(error);
            error->release();
            return nullptr;
        }
        return lib;
    }

    MTL::Library* load_library_from_spirv_shader(const std::string& shader_file, bool use_runtime_build) {
        auto spv_data = load_spirv_binary(shader_file);
        auto msl_program_string = spirv_shader_data_to_msl_program(spv_data);
        return compile_library_from_program_string(msl_program_string, use_runtime_build);
    }

    MTL::Library* compile_library_from_program_string(const std::string& program, bool use_runtime_build) {
        MTL::Library* lib;
        if (use_runtime_build) {
            NS::Error* error = nullptr;
            MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc();
            compile_options->setFastMathEnabled(true);
            compile_options->setLanguageVersion(MTL::LanguageVersion2_4);
            lib = device_->newLibrary(NS::String::string(program.c_str(), NS::StringEncoding::ASCIIStringEncoding), compile_options, &error);
            compile_options->release();
            if (error) {
                report_error(error);
                error->release();
                lib = nullptr;
            }
        } else {
            std::string metallib_path = "/tmp/mtl-" + gen_random_id() + ".metallib";
            std::string build_command = "xcrun -sdk macosx metal -std=macos-metal2.4 -xmetal -Ofast -o " + metallib_path + " -";
            std::string cleanup_command = "rm -r " + metallib_path;
            FILE* pipe = ::popen(build_command.c_str(), "w");
            size_t nNumWritten = fwrite(program.data(), 1, program.size(), pipe);
            if (nNumWritten != program.size()) { return nullptr; }
            ::pclose(pipe);
            lib = load_library_from_library_file(metallib_path);
            system(cleanup_command.c_str());
        }
        return lib;
    }

    MTL::Library* compile_library_from_source(const std::string& source_file, bool use_runtime_build) {
        MTL::Library* lib;
        if (use_runtime_build) {
            std::ifstream t(source_file, std::fstream::in);
            if (!t.is_open()) {
                std::cerr << "Could not open " << source_file << std::endl;
                return nullptr;
            }
            std::stringstream buffer;
            buffer << t.rdbuf();
            lib = compile_library_from_program_string(buffer.str(), true);
        } else {
            std::string metallib_path = "/tmp/mtl-" + gen_random_id() + ".metallib";
            std::string build_command = "xcrun -sdk macosx metal -std=macos-metal2.4 -xmetal -Ofast -o " + metallib_path + ' ' + source_file;
            std::string cleanup_command = "rm -r " + metallib_path;
            system(build_command.c_str());
            lib = load_library_from_library_file(metallib_path);
            system(cleanup_command.c_str());
        }
        return lib;
    }

public:
    MTL::Library* import_metallib(const std::string& name) {
        if (MTL::Library* ptr = lookup_cache(name)) return ptr;
        MTL::Library* loaded = load_library_from_library_file(name);
        if (loaded) { cached_libraries[name] = loaded; }
        return loaded;
    }

    MTL::Library* import_metal_source_string(const std::string& program, bool use_runtime_build = default_value_use_runtime_build) {
        if (MTL::Library* ptr = lookup_cache(program)) return ptr;
        MTL::Library* loaded = compile_library_from_program_string(program, use_runtime_build);
        if (loaded) { cached_libraries[program] = loaded; }
        return loaded;
    }

    MTL::Library* import_metal_source(const std::string& msl_path, bool use_runtime_build = default_value_use_runtime_build) {
        if (MTL::Library* ptr = lookup_cache(msl_path)) return ptr;
        MTL::Library* loaded = compile_library_from_source(msl_path, use_runtime_build);
        if (loaded) { cached_libraries[msl_path] = loaded; }
        return loaded;
    }

    MTL::Library* import_spirv_shader(const std::string& spv_path, bool use_runtime_build = default_value_use_runtime_build) {
        if (MTL::Library* ptr = lookup_cache(spv_path)) return ptr;
        MTL::Library* loaded = load_library_from_spirv_shader(spv_path, use_runtime_build);
        if (loaded) { cached_libraries[spv_path] = loaded; }
        return loaded;
    }

    MTL::Library* import(const std::string& lib_name, bool use_runtime_build = default_value_use_runtime_build) {
        auto endsWith = [&](std::string const& ending) -> bool {
            if (lib_name.length() >= ending.length()) {
                return (0 == lib_name.compare(lib_name.length() - ending.length(), ending.length(), ending));
            } else {
                return false;
            }
        };

        if (endsWith(".metal") || endsWith(".frag") || endsWith(".vert")) {
            return import_metal_source(lib_name, use_runtime_build);
        } else if (endsWith(".metallib") || endsWith(".air")) {
            return import_metallib(lib_name);
        } else if (endsWith(".spv")) {
            return import_spirv_shader(lib_name, use_runtime_build);
        } else {
            return import_metal_source_string(lib_name, use_runtime_build);
        }
    }

    MTL::Function* get_kernel_function(const std::string& lib_name, const std::string& func_name) {
        MTL::Library* lib = lookup_cache(lib_name);
        if (!lib) {
            lib = import(lib_name);
            if (!lib) { return nullptr; }
        }
        return lib->newFunction(NS::String::string(func_name.c_str(), NS::StringEncoding::ASCIIStringEncoding));
    }

    MTL::Function* get_kernel_function(const std::string& func_name) {
        for (const auto& lib: cached_libraries) {
            NS::Array* names = lib.second->functionNames();
            if (!names) continue;
            const unsigned count = names->count();
            for (int i = 0; i < count; ++i) {
                auto str = reinterpret_cast<NS::String*>(names->object(i));
                if (!str) continue;
                if (func_name == (str->cString(NS::StringEncoding::ASCIIStringEncoding))) { return lib.second->newFunction(str); }
            }
        }
        return nullptr;
    }

    friend std::ostream& operator<<(std::ostream& os, const MetalLibraryLoader& loader) {
        for (const auto& lib: loader.cached_libraries) {
            os << "* " << lib.first << ": \n";
            NS::Array* names = lib.second->functionNames();
            if (!names) continue;
            const unsigned count = names->count();
            for (int i = 0; i < count; ++i) {
                auto str = reinterpret_cast<NS::String*>(names->object(i));
                if (!str) continue;
                os << "   * " << str->cString(NS::StringEncoding::ASCIIStringEncoding) << ":   ";
                MTL::Function* func = lib.second->newFunction(str);
                NS::Array* attributes = func->stageInputAttributes();
                const unsigned attribute_count = attributes->count();
                for (int j = 0; j < attribute_count; ++j) {
                    auto attr = reinterpret_cast<MTL::Attribute*>(attributes->object(j));
                    os << attr->name()->cString(NS::StringEncoding::ASCIIStringEncoding) << ", ";
                }
                func->release();
                os << '\n';
            }
        }
        return os;
    }

    MetalLibraryLoader(MetalLibraryLoader const& other) = delete;

    MetalLibraryLoader(MetalLibraryLoader&& other) = delete;

    MetalLibraryLoader& operator=(MetalLibraryLoader other) = delete;

    MetalLibraryLoader& operator=(MetalLibraryLoader&& other) = delete;
};