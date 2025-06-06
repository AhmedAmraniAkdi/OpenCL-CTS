//
// Copyright (c) 2024 The Khronos Group Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <vulkan_interop_common.hpp>
#include <opencl_vulkan_wrapper.hpp>
#include <vulkan_wrapper.hpp>
#if !defined(__APPLE__)
#include <CL/cl.h>
#include <CL/cl_ext.h>
#else
#include <OpenCL/cl.h>
#include <OpenCL/cl_ext.h>
#endif

#include <assert.h>
#include <vector>
#include <iostream>
#include <string.h>
#include "harness/testHarness.h"
#include "harness/typeWrappers.h"
#include "harness/deviceInfo.h"

#include "vulkan_test_base.h"
#include "opencl_vulkan_wrapper.hpp"

namespace {

struct ConsistencyExternalImage1DTest : public VulkanTestBase
{
    ConsistencyExternalImage1DTest(cl_device_id device, cl_context context,
                                   cl_command_queue queue, cl_int nelems)
        : VulkanTestBase(device, context, queue, nelems)
    {}

    cl_int Run() override
    {
        cl_int errNum = CL_SUCCESS;

#ifdef _WIN32
        if (!is_extension_available(device, "cl_khr_external_memory_win32"))
        {
            throw std::runtime_error(
                "Device does not support"
                "cl_khr_external_memory_win32 extension \n");
        }
#else
        if (!is_extension_available(device, "cl_khr_external_memory_opaque_fd"))
        {
            log_info("Device does not support "
                     "cl_khr_external_memory_opaque_fd extension \n");
            return TEST_SKIPPED_ITSELF;
        }
#endif
        uint32_t width = 256;
        cl_image_desc image_desc;
        memset(&image_desc, 0x0, sizeof(cl_image_desc));
        cl_image_format img_format = { 0 };

        VulkanExternalMemoryHandleType vkExternalMemoryHandleType =
            getSupportedVulkanExternalMemoryHandleTypeList(
                vkDevice->getPhysicalDevice())[0];

        auto vulkanImageTiling = vkClExternalMemoryHandleTilingAssumption(
            device, vkExternalMemoryHandleType, &errNum);
        ASSERT_SUCCESS(errNum, "Failed to query OpenCL tiling mode");
        if (vulkanImageTiling == std::nullopt)
        {
            log_info("No image tiling supported by both Vulkan and OpenCL "
                     "could be found\n");
            return TEST_SKIPPED_ITSELF;
        }


        VulkanImage1D vkImage1D =
            VulkanImage1D(*vkDevice, VULKAN_FORMAT_R8G8B8A8_UNORM, width,
                          *vulkanImageTiling, 1, vkExternalMemoryHandleType);

        const VulkanMemoryTypeList& memoryTypeList =
            vkImage1D.getMemoryTypeList();

        log_info("Memory type index: %u\n", (uint32_t)memoryTypeList[0]);
        log_info("Memory type property: %d\n",
                 memoryTypeList[0].getMemoryTypeProperty());
        log_info("Image size : %lu\n", vkImage1D.getSize());

        VulkanDeviceMemory* vkDeviceMem =
            new VulkanDeviceMemory(*vkDevice, vkImage1D, memoryTypeList[0],
                                   vkExternalMemoryHandleType);
        vkDeviceMem->bindImage(vkImage1D, 0);

        void* handle = NULL;
        int fd;
        std::vector<cl_mem_properties> extMemProperties{
            (cl_mem_properties)CL_MEM_DEVICE_HANDLE_LIST_KHR,
            (cl_mem_properties)device,
            (cl_mem_properties)CL_MEM_DEVICE_HANDLE_LIST_END_KHR,
        };
        switch (vkExternalMemoryHandleType)
        {
#ifdef _WIN32
            case VULKAN_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_NT:
                handle = vkDeviceMem->getHandle(vkExternalMemoryHandleType);
                errNum = check_external_memory_handle_type(
                    device, CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR);
                extMemProperties.push_back(
                    (cl_mem_properties)
                        CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR);
                extMemProperties.push_back((cl_mem_properties)handle);
                break;
            case VULKAN_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT:
                handle = vkDeviceMem->getHandle(vkExternalMemoryHandleType);
                errNum = check_external_memory_handle_type(
                    device, CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR);
                extMemProperties.push_back(
                    (cl_mem_properties)
                        CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KMT_KHR);
                extMemProperties.push_back((cl_mem_properties)handle);
                break;
#else
            case VULKAN_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD:
                fd = (int)vkDeviceMem->getHandle(vkExternalMemoryHandleType);
                errNum = check_external_memory_handle_type(
                    device, CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR);
                extMemProperties.push_back(
                    (cl_mem_properties)CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_FD_KHR);
                extMemProperties.push_back((cl_mem_properties)fd);
                break;
#endif
            default:
                errNum = TEST_FAIL;
                log_error("Unsupported external memory handle type \n");
                break;
        }
        if (errNum != CL_SUCCESS)
        {
            log_error("Checks failed for "
                      "CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR\n");
            return TEST_FAIL;
        }
        extMemProperties.push_back(0);

        const VkImageCreateInfo VulkanImageCreateInfo =
            vkImage1D.getVkImageCreateInfo();

        auto layout = vkImage1D.getSubresourceLayout();
        errNum = getCLImageInfoFromVkImageInfo(
            device, &VulkanImageCreateInfo, &img_format, &image_desc,
            vulkanImageTiling == VULKAN_IMAGE_TILING_LINEAR ? &layout
                                                            : nullptr);
        test_error_fail(errNum, "getCLImageInfoFromVkImageInfo failed!!!");

        clMemWrapper image;

        // Pass valid properties, image_desc and image_format
        image = clCreateImageWithProperties(
            context, extMemProperties.data(), CL_MEM_READ_WRITE, &img_format,
            &image_desc, NULL /* host_ptr */, &errNum);
        test_error(errNum, "Unable to create Image with Properties");
        image.reset();

        // Passing image_format as NULL
        image = clCreateImageWithProperties(context, extMemProperties.data(),
                                            CL_MEM_READ_WRITE, NULL,
                                            &image_desc, NULL, &errNum);
        test_failure_error(errNum, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR,
                           "Image creation must fail with "
                           "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"
                           "when image desc passed as NULL");

        image.reset();

        // Passing image_desc as NULL
        image = clCreateImageWithProperties(context, extMemProperties.data(),
                                            CL_MEM_READ_WRITE, &img_format,
                                            NULL, NULL, &errNum);
        test_failure_error(errNum, CL_INVALID_IMAGE_DESCRIPTOR,
                           "Image creation must fail with "
                           "CL_INVALID_IMAGE_DESCRIPTOR "
                           "when image desc passed as NULL");
        image.reset();

        return TEST_PASS;
    }
};
}

REGISTER_TEST(test_consistency_external_for_1dimage)
{
    return MakeAndRunTest<ConsistencyExternalImage1DTest>(device, context,
                                                          queue, num_elements);
}
