#ifndef _ML_METAL_H_
#define _ML_METAL_H_

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#include <nezha/gpu_context.hpp>
#include <nezha/compute_pass.hpp>

/* All possible kernels. */
namespace nz
{

struct acc_matrix_descriptor;
struct acc_kernel;

acc_kernel *make_acc_kernel(ml_kernel kernel_type, const ml_kernel_config &cfg);
void encode_matrix_multiplication(VkSemaphore semaphore, acc_matrix_descriptor *a,
                                  acc_matrix_descriptor *b, acc_matrix_descriptor *result);

}

void ml_metal_test();
void ml_metal_buffer_test(VkBuffer);

#endif
