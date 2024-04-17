#include "exec/globals.hpp"

namespace ivy::exec {

static VulkanResourceBase prepare_vulkan_resource_base()
{
	// Device extensions
	static const std::vector <const char *> EXTENSIONS {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME
	};

	// Load physical device
	auto predicate = [](vk::PhysicalDevice phdev) {
		return littlevk::physical_device_able(phdev, EXTENSIONS);
	};

	vk::PhysicalDevice phdev = littlevk::pick_physical_device(predicate);

	// Enable features
	vk::PhysicalDeviceFeatures2KHR features {};

	vk::PhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentrics {};
	barycentrics.fragmentShaderBarycentric = vk::True;

	vk::PhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR separation {};
	separation.separateDepthStencilLayouts = vk::True;

	features.pNext = &barycentrics;
	barycentrics.pNext = &separation;

	phdev.getFeatures2(&features);

	// Create the resource base
	return VulkanResourceBase::from(phdev, EXTENSIONS, features);
}

Biome &Globals::active_biome()
{
	return *biome;
}

Globals Globals::from()
{
	return { prepare_vulkan_resource_base(), std::nullopt };
}

}