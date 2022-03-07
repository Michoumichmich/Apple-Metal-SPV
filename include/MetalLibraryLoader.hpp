#pragma once

#include <AppleMetal.hpp>
#include <SPIRV_Loader.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>

static inline void report_error(NS::Error* error) {
    if (auto msg = error->description()->cString(NS::StringEncoding::ASCIIStringEncoding)) { std::cerr << msg << std::endl; }

    if (auto msg = error->localizedFailureReason()->cString(NS::StringEncoding::ASCIIStringEncoding)) { std::cerr << msg << std::endl; }
}

class MetalLibraryLoader {
private:
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

    MTL::Library* load_library_from_spirv_shader(const std::string& shader_file) {
        auto spv_data = load_spirv_binary(shader_file);
        auto msl_program_string = spirv_shader_data_to_msl_program(spv_data);
        return compile_library_from_program_string(msl_program_string);
    }


    MTL::Library* compile_library_from_program_string(const std::string& program) {
        NS::Error* error = nullptr;
        MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc();
        compile_options->setFastMathEnabled(true);
        compile_options->setLanguageVersion(MTL::LanguageVersion2_4);
        MTL::Library* lib = device_->newLibrary(NS::String::string(program.c_str(), NS::StringEncoding::ASCIIStringEncoding), compile_options, &error);
        compile_options->release();
        if (error) {
            report_error(error);
            error->release();
            return nullptr;
        }
        return lib;
    }


    MTL::Library* compile_library_from_source(const std::string& source_file) {
        std::ifstream t(source_file, std::fstream::in);
        if (!t.is_open()) {
            std::cerr << "Could not open " << source_file << std::endl;
            return nullptr;
        }
        std::stringstream buffer;
        buffer << t.rdbuf();
        return compile_library_from_program_string(buffer.str());
    }


public:
    MTL::Library* import_metallib(const std::string& name) {
        if (MTL::Library* ptr = lookup_cache(name)) return ptr;
        MTL::Library* loaded = load_library_from_library_file(name);
        if (loaded) { cached_libraries[name] = loaded; }
        return loaded;
    }

    MTL::Library* import_metal_source_string(const std::string& program) {
        if (MTL::Library* ptr = lookup_cache(program)) return ptr;
        MTL::Library* loaded = compile_library_from_program_string(program);
        if (loaded) { cached_libraries[program] = loaded; }
        return loaded;
    }

    MTL::Library* import_metal_source(const std::string& msl_path) {
        if (MTL::Library* ptr = lookup_cache(msl_path)) return ptr;
        MTL::Library* loaded = compile_library_from_source(msl_path);
        if (loaded) { cached_libraries[msl_path] = loaded; }
        return loaded;
    }

    MTL::Library* import_spirv_shader(const std::string& spv_path) {
        if (MTL::Library* ptr = lookup_cache(spv_path)) return ptr;
        MTL::Library* loaded = load_library_from_spirv_shader(spv_path);
        if (loaded) { cached_libraries[spv_path] = loaded; }
        return loaded;
    }


    MTL::Library* import(const std::string& lib_name) {
        auto endsWith = [&](std::string const& ending) {
            if (lib_name.length() >= ending.length()) {
                return (0 == lib_name.compare(lib_name.length() - ending.length(), ending.length(), ending));
            } else {
                return false;
            }
        };

        if (endsWith(".metal")) {
            return import_metal_source(lib_name);
        } else if (endsWith(".metallib") || endsWith(".air")) {
            return import_metallib(lib_name);
        } else if (endsWith(".spv")) {
            return import_spirv_shader(lib_name);
        } else {
            return import_metal_source_string(lib_name);
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