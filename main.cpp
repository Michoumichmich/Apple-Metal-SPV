#include <MetalLibraryLoader.hpp>

#include <numeric>
#include <vector>

inline void run_example(MTL::Device* device, MetalLibraryLoader& libs, size_t size) {
    auto in_buf = std::vector<float>(size);
    std::iota(in_buf.begin(), in_buf.end(), 0);   // buf == 0, 1, 2, ..., size-1
    auto byte_size = size * sizeof(float);
    auto check_result = [&](const float* res) {
        for (unsigned i = 0U; i < size; ++i) {
            if (std::abs(res[i] - std::sqrt(static_cast<float>(i))) < 0.01) { continue; }
            std::cerr << "Mismatch pos: " << i << ", got: " << res[i] << ", but expected: " << std::sqrt(i) << std::endl;
        }
    };

    //auto fun = libs.get_kernel_function("Shaders/base.metal", "sqrtf");
    MTL::Function* fun = libs.get_kernel_function("sqrtf");
    MTL::Buffer* input = device->newBuffer(in_buf.data(), byte_size, MTL::ResourceCPUCacheModeDefaultCache);
    MTL::Buffer* output = device->newBuffer(byte_size, MTL::ResourceCPUCacheModeDefaultCache);
    {
        MTL::CommandQueue* queue = device->newCommandQueue();
        NS::Error* error = nullptr;
        MTL::ComputePipelineState* pipeline = device->newComputePipelineState(fun, &error);
        if (error) {
            report_error(error);
            error->release();
        }
        size_t sub_group_size = pipeline->threadExecutionWidth();
        MTL::CommandBuffer* command_buffer = queue->commandBuffer();
        {
            MTL::ComputeCommandEncoder* encoder = command_buffer->computeCommandEncoder();
            encoder->setComputePipelineState(pipeline);
            encoder->setBuffer(input, 0U, 0U);
            encoder->setBuffer(output, 0U, 1U);
            encoder->dispatchThreads(MTL::Size(size, 1U, 1U), MTL::Size(sub_group_size, 1U, 1U));
            encoder->endEncoding();
        }
        command_buffer->commit();
        command_buffer->waitUntilCompleted();
        command_buffer->release();
        //std::cout << "Pipeline: " << pipeline->description()->cString(NS::StringEncoding::ASCIIStringEncoding) << std::endl;
        pipeline->release();
        queue->release();
    }
    const auto out = reinterpret_cast<const float*>(output->contents());
    check_result(out);
    input->release();
    output->release();
    fun->release();
}


int main() {
    auto device = MTL::CreateSystemDefaultDevice();
    std::cout << "Running on: " << device->name()->cString(NS::StringEncoding::ASCIIStringEncoding) << std::endl;
    auto libs = MetalLibraryLoader(device);
    libs.import_metal_source("Shaders/base.metal");
    libs.import("Shaders/hlsl_resource_binding.spv");
    libs.import("Shaders/vadd.spv");
    libs.import("Shaders/AAPLShaders.metal");
    std::cout << libs << std::endl;
    for (auto i = 1; i < 10000000; i += 1000) {
        run_example(device, libs, i);
        std::cout << i << std::endl;
    }
    device->release();
    return 0;
}
