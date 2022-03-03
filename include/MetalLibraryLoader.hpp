#pragma once

#include <AppleMetal.hpp>
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
    MTL::Library* from_library(const std::string& name) {
        if (MTL::Library* ptr = lookup_cache(name)) return ptr;
        MTL::Library* loaded = load_library_from_library_file(name);
        if (loaded) { cached_libraries[name] = loaded; }
        return loaded;
    }

    MTL::Library* from_program_string(const std::string& program) {
        if (MTL::Library* ptr = lookup_cache(program)) return ptr;
        MTL::Library* loaded = compile_library_from_program_string(program);
        if (loaded) { cached_libraries[program] = loaded; }
        return loaded;
    }

    MTL::Library* from_source_file(const std::string& shader_file) {
        if (MTL::Library* ptr = lookup_cache(shader_file)) return ptr;
        MTL::Library* loaded = compile_library_from_source(shader_file);
        if (loaded) { cached_libraries[shader_file] = loaded; }
        return loaded;
    }


    MTL::Library* from(const std::string& lib_name) {
        if (MTL::Library* ptr = lookup_cache(lib_name)) return ptr;

        auto endsWith = [&](std::string const& ending) {
            if (lib_name.length() >= ending.length()) {
                return (0 == lib_name.compare(lib_name.length() - ending.length(), ending.length(), ending));
            } else {
                return false;
            }
        };

        MTL::Library* ptr;
        if (endsWith(".metal")) {
            ptr = compile_library_from_source(lib_name);
        } else if (endsWith(".metallib") || endsWith(".air")) {
            ptr = load_library_from_library_file(lib_name);
        } else {
            ptr = compile_library_from_program_string(lib_name);
        }
        if (ptr) { cached_libraries[lib_name] = ptr; }
        return ptr;
    }

    MTL::Function* get_function(const std::string& lib_name, const std::string& func_name) {
        MTL::Library* lib = lookup_cache(lib_name);
        if (!lib) {
            lib = from(lib_name);
            if (!lib) { return nullptr; }
        }
        return lib->newFunction(NS::String::string(func_name.c_str(), NS::StringEncoding::ASCIIStringEncoding));
    }

    MetalLibraryLoader(MetalLibraryLoader const& other) = delete;

    MetalLibraryLoader(MetalLibraryLoader&& other) = delete;

    MetalLibraryLoader& operator=(MetalLibraryLoader other) = delete;

    MetalLibraryLoader& operator=(MetalLibraryLoader&& other) = delete;
};