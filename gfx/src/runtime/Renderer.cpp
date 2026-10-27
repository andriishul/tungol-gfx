#include "Renderer.hpp"

#include "RenderConfig.hpp"
#include "helper/ShadowViewProj.hpp"
#include "helper/CatchGraphicsError.hpp"

#include <core/PathResolver.hpp>

#include <asset/ModelFormat.hpp>
#include <vhl/utils/SpvBytecode.hpp>

#include <vhl/core/Allocator.hpp>
#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>
#include <vhl/graph/CommandQueue.hpp>
#include <vhl/graph/CommandEncoder.hpp>

#include <vhl/commands/CopyContext.hpp>

#include <vhl/pipeline/GraphicsPipeline.hpp>
#include <vhl/pipeline/ComputePipeline.hpp>

#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/buffers/UniformBuffer.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

#include <vhl/commands/DrawIndirectCommand.hpp>

namespace tungol::runtime
{
	using namespace vhl;

	namespace 
	{
		static constexpr Format k_DepthFormat = Format::D32Float;

		struct alignas(16) CameraData
		{
			math::mat4 projection;
			math::mat4 view;
			math::vec3 position;
		};

		struct alignas(16) LightData
		{
			math::vec3 direction;
			float intensity;
			math::vec3 color;
		};

		struct alignas(16) ShadowData
		{
			math::mat4 viewProj;
			float bias;
		};

		struct FrameData
		{
			CameraData camera;
			LightData light;
			ShadowData shadow;
		};

		template<uint32_t FIF>
		struct Camera 
		{
			DescriptorSetHandle frameSets[FIF];
			std::unique_ptr<UniformBuffer> frameUBs[FIF];
		};


		template<uint32_t N, uint32_t FIF>
		class Cameras
		{
		public:
			Cameras(const Device& device, SegregatedAllocator& allocator, const DescriptorSetLayoutList& layouts,
				DescriptorPool& descriptorPool, HazardTracker& tracker);
		
			~Cameras();

			Camera<FIF> list[N];
		private:
			DescriptorPool& m_descriptorPool;
		};

		struct Push
		{
			uint32_t totalBoneCount = 0;
			uint32_t instanceCount = 0;
		};

		struct FrameState
		{
			std::optional<uint32_t> imageIndex;
			bool presentable = true;
			const core::ViewData* viewData = nullptr;

			FramebufferSize fbSize{};
		};

		inline vhl::Viewport toViewport(const core::Viewport& v) 
		{
			return vhl::Viewport
			{
				.x = v.x,
				.y = v.y,

				.width = v.width,
				.height = v.height,

				.minDepth = v.minDepth,
				.maxDepth = v.maxDepth				
			}; 
		}

		inline vhl::Scissor toScissor(const core::Scissor& s) 
		{
			return vhl::Scissor
			{
				.offsetX = s.x,
				.offsetY = s.y,

				.width = s.width,
				.height = s.height
			};
		}

		class RenderTargets
		{
		public:
			RenderTargets(const RenderTargets&) = delete;
			inline RenderTargets(GraphicsContext& _ctx, SegregatedAllocator& _allocator, HazardTracker& _tracker)
				: m_ctx(_ctx), m_allocator(_allocator), m_tracker(_tracker)
			{
				const auto& device = m_ctx.activeDevice();

				m_samples = tungol::main::k_MsaaSampleCount;

				bool hasSupport = samplesSupported(device.limits().framebuffer.colorSampleCounts, main::k_MsaaSampleCount) &&
					samplesSupported(device.limits().framebuffer.depthSampleCounts, main::k_MsaaSampleCount);

				// Workaround: AMD's Vulkan driver mishandles sRGB MSAA resolve, resulting in a drastically darker image. Force 1x MSAA on AMD. 
				bool isAmd = device.name().find("AMD") != std::string::npos ||
					device.name().find("Radeon") != std::string::npos;

				if (isAmd || !hasSupport)
				{
					m_samples = SampleCount::One;
				}
			}
			inline ~RenderTargets() noexcept
			{
				m_swapViews.clear();
				m_depthViews.clear();
				m_depthImages.clear();
				m_msaaViews.clear();
				m_msaaImages.clear();
			}

			inline void resize()
			{
				m_swapViews.clear();
				m_ctx.rebuildSwapchain();

				m_depthViews.clear();
				m_depthImages.clear();
				m_msaaViews.clear();
				m_msaaImages.clear();

				auto& device = m_ctx.activeDevice();
				auto& swapchain = m_ctx.swapchain();

				m_count = swapchain.colorImageCount();
				m_colorFormat = swapchain.colorFormat();

				m_swapViews.reserve(m_count);
				m_depthImages.reserve(m_count);
				m_depthViews.reserve(m_count);
				m_msaaImages.reserve(m_count);
				m_msaaViews.reserve(m_count);

				for (uint32_t i = 0; i < m_count; ++i)
				{
					ImageViewDesc colorViewDesc{ .format = swapchain.colorFormat() };
					m_swapViews.emplace_back(new vhl::ImageView(device, swapchain.colorImage(i), colorViewDesc));

					ImageDesc depthDesc
					{
						.format = k_DepthFormat,
						.width = swapchain.width(),
						.height = swapchain.height(),
						.usage = ImageUsageFlags::DepthStencil,
						.samples = m_samples
					};

					m_depthImages.emplace_back(new vhl::Image(device, m_allocator, depthDesc));
					m_tracker.emplace(*m_depthImages[i]);

					ImageViewDesc depthViewDesc{ .format = k_DepthFormat, .aspect = ImageAspectFlags::Depth };
					m_depthViews.emplace_back(new vhl::ImageView(device, *m_depthImages[i], depthViewDesc));

					if (m_samples != SampleCount::One)
					{
						ImageDesc msaaColorDesc
						{
							.format = swapchain.colorFormat(),
							.width = swapchain.width(),
							.height = swapchain.height(),
							.usage = ImageUsageFlags::Color,
							.samples = m_samples
						};

						m_msaaImages.emplace_back(new vhl::Image(device, m_allocator, msaaColorDesc));
						m_tracker.emplace(*m_msaaImages[i]);

						ImageViewDesc msaaViewDesc{ .format = swapchain.colorFormat() };
						m_msaaViews.emplace_back(new vhl::ImageView(device, *m_msaaImages[i], msaaViewDesc));
					}
				}
			}

			Image& depthImage(uint32_t index) { return *m_depthImages[index]; }
			ImageView& depthView(uint32_t index) { return *m_depthViews[index]; }

			Image* msaaImage(uint32_t index) { return m_msaaImages.size() == m_count ? m_msaaImages[index].get() : nullptr; }
			ImageView* msaaView(uint32_t index) { return m_msaaViews.size() == m_count ? m_msaaViews[index].get() : nullptr; }

			Image& swapImage(uint32_t index) { m_ctx.swapchain().colorImage(index); }
			ImageView& swapView(uint32_t index) { return *m_swapViews[index]; }

			Format colorFormat() const { return m_colorFormat; }
			SampleCount samples() const { return m_samples; }
			uint32_t count() const { return m_count; }
		private:
			GraphicsContext& m_ctx;
			SegregatedAllocator& m_allocator;
			HazardTracker& m_tracker;

			Format m_colorFormat = Format::Undefined;
			SampleCount m_samples = SampleCount::One;
			uint32_t m_count = 0;

			std::vector<std::unique_ptr<Image>> m_depthImages;
			std::vector<std::unique_ptr<ImageView>> m_depthViews;

			std::vector<std::unique_ptr<ImageView>> m_msaaViews;
			std::vector<std::unique_ptr<Image>> m_msaaImages;
			std::vector<std::unique_ptr<ImageView>> m_swapViews;
		};

		class Pipelines
		{
		public:
			Pipelines(DescriptorSetLayoutList& layouts, Device& device);
			~Pipelines();

			void buildStaticPbr(const FramebufferSize& pixelSize, const Format& colorFormat, const SampleCount& samples);
			void buildSkinnedPbr(const FramebufferSize& pixelSize, const Format& colorFormat, const SampleCount& samples);

			void buildStaticShadow();
			void buildSkinnedShadow();

			void buildBoneSample();
			void buildBoneHierarchy();

			GraphicsPipeline& staticPbr() { return *m_staticPbr; }
			GraphicsPipeline& skinnedPbr() { return *m_skinnedPbr; }

			GraphicsPipeline& staticShadow() { return *m_shadowStatic; }
			GraphicsPipeline& skinnedShadow() { return *m_shadowSkinned; }

			ComputePipeline& boneSample() { return *m_boneSample; }
			ComputePipeline& boneHierarchy() { return *m_boneHierarchy; }
		private:
			Device& m_device;
			DescriptorSetLayoutList& m_layouts;

			std::unique_ptr<GraphicsPipeline> m_staticPbr;
			std::unique_ptr<GraphicsPipeline> m_skinnedPbr;
			std::unique_ptr<GraphicsPipeline> m_shadowStatic;
			std::unique_ptr<GraphicsPipeline> m_shadowSkinned;

			std::unique_ptr<ComputePipeline> m_boneSample, m_boneHierarchy;
		};

		Pipelines::Pipelines(DescriptorSetLayoutList& layouts, Device& device) : m_layouts(layouts), m_device(device)
		{
		}

		Pipelines::~Pipelines()
		{
			m_boneHierarchy.reset();
			m_boneSample.reset();

			m_shadowSkinned.reset();
			m_shadowStatic.reset();

			m_skinnedPbr.reset();
			m_staticPbr.reset();
		}

		void Pipelines::buildStaticPbr(const FramebufferSize& pixelSize, const Format& colorFormat, const SampleCount& samples)
		{
			// ===================== SHADERS =================================
			Shaders ss
			{
				.vertex = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_StaticVertex)),
				.fragment = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_StaticFragment))
			};

			// ========================== VIEWPORT / SCISSOR

			Viewport viewport
			{
				.width = static_cast<float>(pixelSize.width),
				.height = static_cast<float>(pixelSize.height)
			};

			Scissor scissor
			{
				.width = pixelSize.width,
				.height = pixelSize.height
			};

			// ============================== VERTEX INPUT AND ATTRIBUTES ============

			VertexInputStageInfo vertexInput;
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 0, 0, Format::Rgb32Float, offsetof(asset::StaticVertex, position) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 1, 0, Format::Rgb32Float, offsetof(asset::StaticVertex, normal) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 2, 0, Format::Rg32Float, offsetof(asset::StaticVertex, uv) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 3, 0, Format::Rgba32Float, offsetof(asset::StaticVertex, tangent) });

			vertexInput.bindings.push_back(VertexInputBindingDesc{ 0, sizeof(asset::StaticVertex) });

			// ============================== PIPELINE CREATION


			PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[0], m_layouts[1], m_layouts[2], m_layouts[5] }
			};

			MultisampleStateDesc multisample{};
			multisample.samples = samples;

			GraphicsPipelineDesc pipelineDesc
			{
				.shaders = ss,
				.vertexInput = vertexInput,
				.topology = PrimitiveTopology::TriangleList,

				.defaultViewport = viewport,
				.defaultScissor = scissor,

				.defaultColorFormat = colorFormat,
				.defaultDepthFormat = k_DepthFormat,
				.defaultStencilFormat = Format::Undefined,

				.multisample = multisample
			};

			m_staticPbr = std::make_unique<GraphicsPipeline>(m_device);
			m_staticPbr->init_layout(layoutDesc).build(pipelineDesc);
		}

		void Pipelines::buildSkinnedPbr(const FramebufferSize& pixelSize, const Format& colorFormat, const SampleCount& samples)
		{
			// ===================== SHADERS =================================
			Shaders ss
			{
				.vertex = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_SkinnedVertex)),
				.fragment = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_SkinnedFragment))
			};

			// ========================== VIEWPORT / SCISSOR

			vhl::Viewport viewport
			{
				.width = static_cast<float>(pixelSize.width),
				.height = static_cast<float>(pixelSize.height)
			};

			vhl::Scissor scissor
			{
				.width = pixelSize.width,
				.height = pixelSize.height
			};

			// ============================== VERTEX INPUT AND ATTRIBUTES ============

			vhl::VertexInputStageInfo vertexInput;
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 0, 0, vhl::Format::Rgb32Float, offsetof(asset::SkinnedVertex, position) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 1, 0, vhl::Format::Rgb32Float, offsetof(asset::SkinnedVertex, normal) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 2, 0, vhl::Format::Rg32Float, offsetof(asset::SkinnedVertex, uv) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 3, 0, vhl::Format::Rgba32Float, offsetof(asset::SkinnedVertex, tangent) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 4, 0, vhl::Format::Rgba32Sint, offsetof(asset::SkinnedVertex, boneIndices) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 5, 0, vhl::Format::Rgba32Float, offsetof(asset::SkinnedVertex, boneWeights) });

			vertexInput.bindings.push_back(vhl::VertexInputBindingDesc{ 0, sizeof(asset::SkinnedVertex) });

			// ============================== PIPELINE CREATION

			MultisampleStateDesc multisample{};
			multisample.samples = samples;

			vhl::PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[0], m_layouts[1], m_layouts[2], m_layouts[4], m_layouts[5] }
			};

			vhl::GraphicsPipelineDesc pipelineDesc
			{
				.shaders = ss,
				.vertexInput = vertexInput,
				.topology = vhl::PrimitiveTopology::TriangleList,

				.defaultViewport = viewport,
				.defaultScissor = scissor,

				.defaultColorFormat = colorFormat,
				.defaultDepthFormat = k_DepthFormat,
				.defaultStencilFormat = vhl::Format::Undefined,

				.multisample = multisample
			};

			m_skinnedPbr = std::make_unique<vhl::GraphicsPipeline>(m_device);
			m_skinnedPbr->init_layout(layoutDesc).build(pipelineDesc);
		}

		void Pipelines::buildStaticShadow()
		{
			// ===================== SHADERS =================================
			Shaders ss
			{
				.vertex = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_ShadowStatic)),
				.fragment = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_ShadowEmpty))
			};

			// ========================== VIEWPORT / SCISSOR

			Viewport viewport
			{
				.width = static_cast<float>(main::k_ShadowMapResolution),
				.height = static_cast<float>(main::k_ShadowMapResolution)
			};

			Scissor scissor
			{
				.width = main::k_ShadowMapResolution,
				.height = main::k_ShadowMapResolution
			};

			// ============================== VERTEX INPUT AND ATTRIBUTES ============

			VertexInputStageInfo vertexInput;
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 0, 0, Format::Rgb32Float, offsetof(asset::StaticVertex, position) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 1, 0, Format::Rgb32Float, offsetof(asset::StaticVertex, normal) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 2, 0, Format::Rg32Float, offsetof(asset::StaticVertex, uv) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 3, 0, Format::Rgba32Float, offsetof(asset::StaticVertex, tangent) });

			vertexInput.bindings.push_back(VertexInputBindingDesc{ 0, sizeof(asset::StaticVertex) });

			// ============================== PIPELINE CREATION

			PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[0], m_layouts[1] }
			};


			RasterizationStateDesc rasterization
			{
				.cullMode = CullMode::Front
			};

			GraphicsPipelineDesc pipelineDesc
			{
				.shaders = ss,
				.vertexInput = vertexInput,
				.topology = PrimitiveTopology::TriangleList,

				.defaultViewport = viewport,
				.defaultScissor = scissor,

				.defaultColorFormat = Format::Undefined,
				.defaultDepthFormat = main::k_ShadowMapFormat,
				.defaultStencilFormat = Format::Undefined,

				.rasterization = rasterization
			};

			m_shadowStatic = std::make_unique<GraphicsPipeline>(m_device);
			m_shadowStatic->init_layout(layoutDesc).build(pipelineDesc);
		}
		
		void Pipelines::buildSkinnedShadow()
		{
			// ===================== SHADERS =================================
			Shaders shaders
			{
				.vertex = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_ShadowSkinned)),
				.fragment = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_ShadowEmpty))
			};

			// ========================== VIEWPORT / SCISSOR

			vhl::Viewport viewport
			{
				.width = static_cast<float>(main::k_ShadowMapResolution),
				.height = static_cast<float>(main::k_ShadowMapResolution)
			};

			vhl::Scissor scissor
			{
				.width = main::k_ShadowMapResolution,
				.height = main::k_ShadowMapResolution
			};

			// ============================== VERTEX INPUT AND ATTRIBUTES ============

			vhl::VertexInputStageInfo vertexInput;
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 0, 0, vhl::Format::Rgb32Float, offsetof(asset::SkinnedVertex, position) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 1, 0, vhl::Format::Rgb32Float, offsetof(asset::SkinnedVertex, normal) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 2, 0, vhl::Format::Rg32Float, offsetof(asset::SkinnedVertex, uv) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 3, 0, vhl::Format::Rgba32Float, offsetof(asset::SkinnedVertex, tangent) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 4, 0, vhl::Format::Rgba32Sint, offsetof(asset::SkinnedVertex, boneIndices) });
			vertexInput.attributes.push_back(VertexInputAttributeDesc{ 5, 0, vhl::Format::Rgba32Float, offsetof(asset::SkinnedVertex, boneWeights) });

			vertexInput.bindings.push_back(vhl::VertexInputBindingDesc{ 0, sizeof(asset::SkinnedVertex) });

			// ============================== PIPELINE CREATION

			vhl::PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[0], m_layouts[1], m_layouts[4] }
			};

			RasterizationStateDesc rasterization
			{
				.cullMode = CullMode::Front
			};

			vhl::GraphicsPipelineDesc pipelineDesc
			{
				.shaders = shaders,
				.vertexInput = vertexInput,
				.topology = vhl::PrimitiveTopology::TriangleList,

				.defaultViewport = viewport,
				.defaultScissor = scissor,

				.defaultColorFormat = Format::Undefined,
				.defaultDepthFormat = main::k_ShadowMapFormat,
				.defaultStencilFormat = Format::Undefined,

				.rasterization = rasterization
			};

			m_shadowSkinned = std::make_unique<vhl::GraphicsPipeline>(m_device);
			m_shadowSkinned->init_layout(layoutDesc).build(pipelineDesc);
		}
		
		void Pipelines::buildBoneSample()
		{
			ComputePipelineDesc pipelineDesc
			{
				.compute = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_BoneSample))
			};


			vhl::PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[3] },
				.pushConstantRanges = { PushConstantRange{ ShaderStageFlags::Compute, 0, sizeof(Push) }}
			};

			m_boneSample = std::make_unique<ComputePipeline>(m_device);
			m_boneSample->init_layout(layoutDesc).build(pipelineDesc);
		}
		
		void Pipelines::buildBoneHierarchy()
		{
			ComputePipelineDesc pipelineDesc
			{
				.compute = utils::LoadSpvBytecode(core::PathResolver::GetShaderPath(shaders::k_BoneHierarchy))
			};

			vhl::PipelineLayoutDesc layoutDesc
			{
				.setLayouts = { m_layouts[4] },
				.pushConstantRanges = { PushConstantRange{ ShaderStageFlags::Compute, 0, sizeof(Push) }}
			};

			m_boneHierarchy = std::make_unique<ComputePipeline>(m_device);
			m_boneHierarchy->init_layout(layoutDesc).build(pipelineDesc);
		}
		
		template<uint32_t N, uint32_t FIF>
		Cameras<N, FIF>::Cameras(const Device& device, SegregatedAllocator& allocator, const DescriptorSetLayoutList& layouts, DescriptorPool& descriptorPool, HazardTracker& tracker)
			: m_descriptorPool(descriptorPool)
		{
			for (Camera<FIF>& c : list) 
			{
				for (uint32_t i = 0; i < FIF; ++i)
				{
					c.frameUBs[i] = std::make_unique<UniformBuffer>(device, allocator, sizeof(FrameData));
					tracker.emplace(*c.frameUBs[i]);

					c.frameSets[i] = descriptorPool.emplace(layouts[0]);
					vhl::DescriptorWriter writer(c.frameSets[i]);
					writer.uniformBuffer(*c.frameUBs[i], 0, 0, sizeof(FrameData));
					writer.apply(device);
				}
			}
		}
		template<uint32_t N, uint32_t FIF>
		Cameras<N, FIF>::~Cameras()
		{
			for (Camera<FIF>& c : list)
			{
				for (uint32_t i = 0; i < FIF; ++i)
				{
					c.frameUBs[i].reset();
					m_descriptorPool.remove(c.frameSets[i]);
				}
			}
		}
}

	struct Renderer::Impl 
	{
	private:
		static FrameData buildFrameData(const RenderScene& renderScene, const core::ViewData& data);
	public:
		Impl(RenderKernel& kern);
		~Impl();

		RenderKernel& kernel;
		CommandQueue& mainQueue;
		
		uint32_t drawPerFrame = 0;
		RenderTargets targets;
		Pipelines pipelines;

		Cameras<main::k_MaxScenes, RenderKernel::FramesInFlight> cameras;

		void begin();
		void draw(const RenderScene& renderScene, const core::ViewData& view);
		void end();

		void close() noexcept;
	private:
		void recordCopyData(CommandRecorder& recorder, const RenderScene& renderScene, const core::Scene& scene, uint32_t frameSlot, uint32_t drawId);
		void recordDrawPipelines(CommandRecorder& recorder, const RenderScene& renderScene, uint32_t frameSlot, uint32_t drawId);
		void recordDrawShadows(CommandRecorder& recorder, const RenderScene& renderScene, uint32_t frameSlot, uint32_t drawId);
	private:
		std::unique_ptr<CommandEncoder> encoder;
		FrameState frameState;
	};

#pragma region Renderer::Impl

	FrameData Renderer::Impl::buildFrameData(const RenderScene& renderScene, const core::ViewData& data)
	{
		CameraData cd
		{
			.projection = data.camera.projection,
			.view = data.camera.view,
			.position = data.camera.position
		};


		LightData ld
		{
			.direction = data.light.direction,
			.intensity = data.light.intensity,
			.color = data.light.color
		};


		auto min = renderScene.sceneMin(); 
		
		auto max = renderScene.sceneMax();


		ShadowData sd
		{
			.viewProj = computeShadowViewProj(min, max, ld.direction),
			.bias = data.shadow.bias
		};

		FrameData fd{};
		fd.camera = cd;
		fd.light = ld;
		fd.shadow = sd;

		return fd;
	}

	void Renderer::Impl::recordDrawPipelines(CommandRecorder& recorder, const RenderScene& renderScene, uint32_t frameSlot, uint32_t drawId)
	{
		if (renderScene.staticDrawCount() > 0)
		{
			recorder.bindVertexBuffer(renderScene.staticVB());
			recorder.bindPipeline(pipelines.staticPbr());
			recorder.bindIndexBuffer(renderScene.indexBuf(), IndexType::Uint32);
			
			recorder.bindDescriptorSet(cameras.list[drawId].frameSets[frameSlot], PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 0);
			recorder.bindDescriptorSet(renderScene.objectSets(frameSlot), PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 1);
			recorder.bindDescriptorSet(renderScene.textures().set(), PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 2);
			recorder.bindDescriptorSet(renderScene.shadows().set(), PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 3);

			recorder.drawIndexedIndirect(renderScene.indirectBuf(), 0, renderScene.staticDrawCount(), sizeof(vhl::DrawIndexedIndirectCommand));
		}

		if (renderScene.skinnedDrawCount() > 0) 
		{
			recorder.bindVertexBuffer(renderScene.skinnedVB());
			recorder.bindPipeline(pipelines.skinnedPbr());
			recorder.bindIndexBuffer(renderScene.indexBuf(), IndexType::Uint32);
			
			
			recorder.bindDescriptorSet(cameras.list[drawId].frameSets[frameSlot], PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 0);
			recorder.bindDescriptorSet(renderScene.objectSets(frameSlot), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 1);
			recorder.bindDescriptorSet(renderScene.textures().set(), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 2);
			
			recorder.bindDescriptorSet(renderScene.animations().hierarchySets(frameSlot), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 3);
			recorder.bindDescriptorSet(renderScene.shadows().set(), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 4);

			recorder.drawIndexedIndirect(renderScene.indirectBuf(), renderScene.staticDrawCount() * sizeof(vhl::DrawIndexedIndirectCommand), renderScene.skinnedDrawCount(), sizeof(vhl::DrawIndexedIndirectCommand));
		}
	}

	void Renderer::Impl::recordDrawShadows(CommandRecorder& recorder, const RenderScene& renderScene, uint32_t frameSlot, uint32_t drawId)
	{
		if (renderScene.staticDrawCount() > 0)
		{
			recorder.bindVertexBuffer(renderScene.staticVB());
			recorder.bindPipeline(pipelines.staticShadow());
			recorder.bindIndexBuffer(renderScene.indexBuf(), IndexType::Uint32);

			recorder.bindDescriptorSet(cameras.list[drawId].frameSets[frameSlot], PipelineType::Graphics, pipelines.staticShadow().layoutHandle(), 0);
			recorder.bindDescriptorSet(renderScene.objectSets(frameSlot), PipelineType::Graphics, pipelines.staticShadow().layoutHandle(), 1);

			recorder.drawIndexedIndirect(renderScene.indirectBuf(), 0, renderScene.staticDrawCount(), sizeof(DrawIndexedIndirectCommand));
		}

		if (renderScene.skinnedDrawCount() > 0)
		{
			recorder.bindVertexBuffer(renderScene.skinnedVB());
			recorder.bindPipeline(pipelines.skinnedShadow());
			recorder.bindIndexBuffer(renderScene.indexBuf(), IndexType::Uint32);

			recorder.bindDescriptorSet(cameras.list[drawId].frameSets[frameSlot], PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 0);
			recorder.bindDescriptorSet(renderScene.objectSets(frameSlot), PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 1);
			recorder.bindDescriptorSet(renderScene.animations().hierarchySets(frameSlot), PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 2);

			recorder.drawIndexedIndirect(renderScene.indirectBuf(), renderScene.staticDrawCount() * sizeof(DrawIndexedIndirectCommand),  renderScene.skinnedDrawCount(), sizeof(DrawIndexedIndirectCommand));
		}
	}

	void Renderer::Impl::recordCopyData(CommandRecorder& recorder, const RenderScene& renderScene, const core::Scene& scene, uint32_t frameSlot, uint32_t drawId)
	{
		if (uint8_t* playbackSrc = renderScene.buildPlayback(scene)) 
		{
			auto& playbackTB = renderScene.animations().playbackTBs(frameSlot);
			memcpy(playbackTB.pMappedData(), playbackSrc, renderScene.animations().playbackSize());
			recorder.copyTo(playbackTB, renderScene.animations().playbackSBs(frameSlot), 0, 0, playbackTB.size());
		}

		if (uint8_t* instanceSrc = renderScene.build(scene)) 
		{
			auto& instanceTB = renderScene.instanceTBs(frameSlot);
			memcpy(instanceTB.pMappedData(), instanceSrc, instanceTB.size());
			recorder.copyTo(instanceTB, renderScene.instanceSBs(frameSlot), 0, 0, instanceTB.size());
		}

		if (frameState.viewData)
		{
			auto& frameUB = *cameras.list[drawId].frameUBs[frameSlot];
			const FrameData fd = buildFrameData(renderScene, *frameState.viewData);
			recorder.updateUniformBuffer(frameUB, &fd, 0, frameUB.size());
		}
	}


	Renderer::Impl::Impl(RenderKernel& kern)
		: kernel(kern), 
		mainQueue(kern.mainQueue()), 
		targets(kernel.context(), kernel.allocator(), kernel.tracker()),
		pipelines(kernel.layouts(), kernel.context().activeDevice()),
		cameras(kernel.context().activeDevice(), kernel.allocator(), 
			kernel.layouts(), kernel.descriptorPool(), kernel.tracker())
	{
		const auto fbSize = kernel.window().pixelSize();

		frameState.fbSize = fbSize;

		targets.resize();
		
		pipelines.buildStaticPbr(fbSize, targets.colorFormat(), targets.samples());
		pipelines.buildSkinnedPbr(fbSize, targets.colorFormat(), targets.samples());
		
		pipelines.buildStaticShadow();
		pipelines.buildSkinnedShadow();

		pipelines.buildBoneHierarchy();
		pipelines.buildBoneSample();
	}

	Renderer::Impl::~Impl()
	{
		try 
		{
			kernel.context().activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR
		{
			cameras.~Cameras();
			pipelines.~Pipelines();
			targets.~RenderTargets();
		}
	}

	void Renderer::Impl::begin()
	{
		auto newFbSize = kernel.window().pixelSize();

		if (newFbSize.width <= 1 || newFbSize.height <= 1)
		{
			frameState.imageIndex = std::nullopt;
			return;
		}

		mainQueue.begin();

		if (!frameState.presentable || frameState.fbSize != newFbSize)
		{
			targets.resize();
			frameState.fbSize = newFbSize;
			frameState.presentable = true;
		}

		auto& swapchain = kernel.context().swapchain();
		
		AcquireResult acquireResult = mainQueue.nextImageIndex(swapchain);	

		frameState.presentable = acquireResult.success;

		if (!frameState.presentable) return;

		if (acquireResult.imageIndex.has_value()) 
		{
			frameState.imageIndex = acquireResult.imageIndex.value();
		}

		encoder = std::make_unique<CommandEncoder>(mainQueue, kernel.tracker());
		
		auto& colorImage = swapchain.colorImage(*frameState.imageIndex);

		encoder->setDrawable(colorImage);
		drawPerFrame = 0;
	}

	void Renderer::Impl::draw(const RenderScene& renderScene, const core::ViewData& view)
	{
		const auto& scene = renderScene.scene();
		
		if (!encoder) return;
		if (!frameState.imageIndex.has_value() || !frameState.presentable) return;
		if (drawPerFrame >= main::k_MaxScenes) throw std::runtime_error("[Renderer::draw] too many scenes drawn in one frame");

		auto& swapchain = kernel.context().swapchain();

		auto& depthImage = targets.depthImage(*frameState.imageIndex);

		frameState.viewData = nullptr;
		frameState.viewData = &view;

		const uint32_t frameSlot = mainQueue.frameSlot();

		const uint32_t drawId = drawPerFrame++;

		auto& colorImage = swapchain.colorImage(*frameState.imageIndex);
		
		encoder->encodePass([&, frameSlot, drawId](vhl::CommandRecorder& recorder)
		{
			recordCopyData(recorder, renderScene, scene, frameSlot, drawId);
		})
		.useBuffer(*cameras.list[drawId].frameUBs[frameSlot], UsageIntent::TransferDst)
		.useBuffer(renderScene.animations().playbackTBs(frameSlot), UsageIntent::TransferSrc)
		.useBuffer(renderScene.animations().playbackSBs(frameSlot), UsageIntent::TransferDst)
		.useBuffer(renderScene.instanceTBs(frameSlot), UsageIntent::TransferSrc)
		.useBuffer(renderScene.instanceSBs(frameSlot), UsageIntent::TransferDst);

		const uint32_t bonesCount = renderScene.animations().boneCount();
		const uint32_t groups = (bonesCount + 63) / 64;

		const Push push{ bonesCount, static_cast<uint32_t>(scene.instances().size()) };

		encoder->encodePass([&, frameSlot, groups, push](CommandRecorder& recorder)
		{
			recorder.bindPipeline(pipelines.boneSample());
			recorder.bindDescriptorSet(renderScene.animations().sampleSets(frameSlot), PipelineType::Compute, pipelines.boneSample().layoutHandle(), 0);
			recorder.pushConstants(pipelines.boneSample().layoutHandle(), ShaderStageFlags::Compute, 0, sizeof(push), &push);
			recorder.dispatch(groups, 1, 1);
		})
		.useBuffer(renderScene.animations().boneMetaSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().playbackSBs(frameSlot), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().animationsSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().channelsSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().keyframesSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().bindPosesSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().boneLocalsSBs(frameSlot), UsageIntent::ShaderWriteCompute);
		
		encoder->encodePass([&, frameSlot, groups, push](CommandRecorder& recorder)
		{
			recorder.bindPipeline(pipelines.boneHierarchy());
			recorder.bindDescriptorSet(renderScene.animations().hierarchySets(frameSlot), PipelineType::Compute, pipelines.boneHierarchy().layoutHandle(), 0);
			recorder.pushConstants(pipelines.boneHierarchy().layoutHandle(), ShaderStageFlags::Compute, 0, sizeof(push), &push);
			recorder.dispatch(groups, 1, 1);
		})
		.useBuffer(renderScene.animations().boneMetaSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().inverseBindsSB(), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().boneLocalsSBs(frameSlot), UsageIntent::ShaderReadCompute)
		.useBuffer(renderScene.animations().bonePaletteSBs(frameSlot), UsageIntent::ShaderWriteCompute);

		auto& shadowView = renderScene.shadows().view();
		auto& shadowImage = renderScene.shadows().image();

		encoder->encodePass([&, frameSlot, drawId](CommandRecorder& recorder)
		{
			RenderingAttachment depthAttachment
			{
				.imageView = &shadowView,
				.layout = ImageLayout::DepthStencilAttachment,
				.loadOp = AttachmentLoadOp::Clear,
				.storeOp = AttachmentStoreOp::Store,
				.clearValue = 1.0f
			};

			RenderingInfo info
			{
				.width = shadowImage.width(),
				.height = shadowImage.height(),
				.depthAttachment = depthAttachment
			};

			Scissor scissor
			{
				.width = shadowImage.width(),
				.height = shadowImage.height()
			};

			Viewport viewport{};
			viewport.width = static_cast<float>(shadowImage.width());
			viewport.height = static_cast<float>(shadowImage.height());
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			recorder.setScissor(scissor);
			recorder.setViewport(viewport);

			recorder.beginRendering(info);

			recordDrawShadows(recorder, renderScene, frameSlot, drawId);

			recorder.endRendering();
		})
		.useImage(shadowImage, UsageIntent::DepthStencilAttachment)
		.useBuffer(renderScene.staticVB(), UsageIntent::VertexAttributeRead)
		.useBuffer(renderScene.skinnedVB(), UsageIntent::VertexAttributeRead)
		.useBuffer(renderScene.indexBuf(), UsageIntent::IndexRead)
		.useBuffer(renderScene.indirectBuf(), UsageIntent::IndirectRead)
		.useBuffer(renderScene.instanceSBs(frameSlot), UsageIntent::ShaderReadVertex)
		.useBuffer(*cameras.list[drawId].frameUBs[frameSlot], UsageIntent::UniformReadVertex)
		.useBuffer(renderScene.animations().bonePaletteSBs(frameSlot), UsageIntent::ShaderReadVertex);

		auto passBuilder = encoder->encodePass([&, frameSlot, drawId](CommandRecorder& recorder)
		{
			RenderingAttachment depthAttachment
			{
				.imageView = &targets.depthView(*frameState.imageIndex),
				.layout = ImageLayout::DepthStencilAttachment,
				.loadOp = drawId == 0 ? AttachmentLoadOp::Clear : AttachmentLoadOp::Load,

				.storeOp = AttachmentStoreOp::Store,
				.clearValue = 1.0f
			};

			RenderingAttachment colorAttachment{};
			colorAttachment.loadOp = AttachmentLoadOp::Load;
			colorAttachment.storeOp = AttachmentStoreOp::Store;
			colorAttachment.clearValue = view.clearColor;
			
			if (targets.samples() != SampleCount::One)
			{
				colorAttachment.imageView = targets.msaaView(*frameState.imageIndex);
				colorAttachment.layout = ImageLayout::ColorAttachment;

				colorAttachment.resolveImageView = &targets.swapView(*frameState.imageIndex);
				colorAttachment.resolveLayout = ImageLayout::ColorAttachment;
			}
			else
			{
				colorAttachment.imageView = &targets.swapView(*frameState.imageIndex);
				colorAttachment.layout = ImageLayout::ColorAttachment;

				colorAttachment.resolveImageView = nullptr; 
				colorAttachment.resolveLayout = ImageLayout::Undefined;
			}

			RenderingInfo info
			{
				.width = colorImage.width(),
				.height = colorImage.height(),
				.colorAttachment = colorAttachment,
				.depthAttachment = depthAttachment
			};

			Scissor scissor{};
			
			if (!view.scissor.has_value())
			{
				scissor.width = info.width;
				scissor.height = info.height;
			}
			else scissor = toScissor(*view.scissor);

			Viewport viewport{};

			if (!view.viewport.has_value())
			{
				viewport.height = static_cast<float>(info.height);
				viewport.width = static_cast<float>(info.width);
			}
			else viewport = toViewport(*view.viewport);
					
			recorder.beginRendering(info);

			recorder.setScissor(scissor);
			recorder.setViewport(viewport);

			recorder.clearAttachment(ImageAspectFlags::Color, scissor, view.clearColor);

			recordDrawPipelines(recorder, renderScene, frameSlot, drawId);
			
			recorder.endRendering();
		});

		passBuilder.useImage(colorImage, UsageIntent::ColorAttachment)
			.useImage(depthImage, UsageIntent::DepthStencilAttachment)
			.useBuffer(*cameras.list[drawId].frameUBs[frameSlot], UsageIntent::UniformReadVertex)
			.useBuffer(renderScene.instanceSBs(frameSlot), UsageIntent::ShaderReadVertex)
			.useBuffer(renderScene.animations().bonePaletteSBs(frameSlot), UsageIntent::ShaderReadVertex)
			.useImage(renderScene.shadows().image(), UsageIntent::ShaderReadFragment)
			.useBuffer(renderScene.materialSB(), UsageIntent::ShaderReadFragment);
			
		
		if (targets.samples() != SampleCount::One)
		{
			if(auto msaaImage = targets.msaaImage(*frameState.imageIndex))
				passBuilder.useImage(*msaaImage, UsageIntent::ColorAttachment);
		}
		
		renderScene.textures().foreachImage([&passBuilder](vhl::Image& img)
		{
			passBuilder.useImage(img, UsageIntent::ShaderReadFragment);
		});
	}

	void Renderer::Impl::end()
	{
		if (frameState.imageIndex.has_value() && frameState.presentable && encoder)
		{
			encoder->commit();
		
			frameState.presentable = mainQueue.end(kernel.context().swapchain(), *frameState.imageIndex);
			encoder.reset();
			frameState.imageIndex = std::nullopt;
			drawPerFrame = 0;
		}

	}

	void Renderer::Impl::close() noexcept
	{
		kernel.context().activeDevice().wait();
		kernel.window().stop();
	}
	
#pragma endregion


	Renderer::~Renderer() = default;

	Renderer::Renderer(RenderKernel& kernel) : m_impl(new Impl(kernel))
	{
	}


	void Renderer::begin() 
	{
		m_impl->begin();
	}


	void Renderer::draw(const RenderScene& renderScene, const core::ViewData& view)
	{
		m_impl->draw(renderScene, view);
	}


	void Renderer::end()
	{
		m_impl->end();
	}
	
	

}
