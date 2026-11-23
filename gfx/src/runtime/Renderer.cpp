#include "Renderer.hpp"


#include "RenderConfig.hpp"

// Core rendering
#include "RenderKernel.hpp"
#include "internal/KernelInternal.hpp"
#include "internal/ResidencyInternal.hpp"

// Heaps
#include "heaps/GeometryHeap.hpp"
#include "heaps/AnimationHeap.hpp"
#include "heaps/DrawHeap.hpp"
#include "heaps/MaterialHeap.hpp"

// Data
#include "data/ShaderViewData.hpp"
#include <vhl/commands/DrawIndirectCommand.hpp>

// Helper
#include "helper/CatchGraphicsError.hpp"

#include "SceneResidency.hpp"

#include "ShadowMap.hpp"

#include <core/PathResolver.hpp>
#include <vhl/utils/SpvBytecode.hpp>

#include <vhl/core/Allocator.hpp>
#include <vhl/core/GraphicsContext.hpp>
#include <vhl/core/Descriptors.hpp>

#include <vhl/graph/HazardTracker.hpp>
#include <vhl/graph/CommandQueue.hpp>
#include <vhl/graph/CommandEncoder.hpp>

#include <vhl/pipeline/GraphicsPipeline.hpp>
#include <vhl/pipeline/ComputePipeline.hpp>

#include <runtime/Texture.hpp>
#include <vhl/core/images/Image.hpp>
#include <vhl/core/images/ImageView.hpp>
#include <vhl/core/images/Sampler.hpp>

#include <vhl/core/buffers/UniformBuffer.hpp>
#include <vhl/core/buffers/StorageBuffer.hpp>
#include <vhl/core/buffers/StagingBuffer.hpp>

namespace tungol::runtime
{
	using namespace vhl;


#pragma region RenderParts
	
	namespace
	{
		static constexpr Format k_DepthFormat = Format::D32Float;

		template<uint32_t FIF>
		struct Camera
		{
			Camera(SegregatedAllocator& allocator, Device& device,
				DescriptorPool& descriptorPool, DescriptorSetLayoutList& layouts,
				HazardTracker& tracker);

			Buffer& UBs(uint32_t slot) const { return *m_ubs[slot]; }
			const DescriptorSetHandle& Sets(uint32_t slot) const { return m_sets[slot]; }

			~Camera();
		private:
			DescriptorPool& m_pool;
			DescriptorSetHandle m_sets[FIF];
			std::unique_ptr<UniformBuffer> m_ubs[FIF];
		};

		struct SkinningParams
		{
			uint32_t totalThreads = 0;
			uint32_t playbackCount = 0;
		};

		struct FrameState
		{
			std::optional<uint32_t> imageIndex;
			bool presentable = true;
			const core::ViewData* viewData = nullptr;

			FramebufferSize fbSize{};

			uint32_t slot = 0;

			const core::Scene* scene = nullptr;

			uint64_t layoutVersion = UINT64_MAX;
			uint64_t clipVersion = UINT64_MAX;
			uint64_t residencyVersion = UINT64_MAX;
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
				m_width = swapchain.width();
				m_height = swapchain.height();

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

			Image& swapImage(uint32_t index) { return m_ctx.swapchain().colorImage(index); }
			ImageView& swapView(uint32_t index) { return *m_swapViews[index]; }

			Format colorFormat() const { return m_colorFormat; }
			uint32_t width() const { return m_width; }
			uint32_t height() const { return m_height; }
			SampleCount samples() const { return m_samples; }
			uint32_t count() const { return m_count; }
		private:
			GraphicsContext& m_ctx;
			SegregatedAllocator& m_allocator;
			HazardTracker& m_tracker;

			Format m_colorFormat = Format::Undefined;
			SampleCount m_samples = SampleCount::One;
			uint32_t m_count = 0;
			uint32_t m_width = 0, m_height = 0;

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
				.pushConstantRanges = { PushConstantRange{ ShaderStageFlags::Compute, 0, sizeof(SkinningParams) }}
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
				.pushConstantRanges = { PushConstantRange{ ShaderStageFlags::Compute, 0, sizeof(SkinningParams) }}
			};

			m_boneHierarchy = std::make_unique<ComputePipeline>(m_device);
			m_boneHierarchy->init_layout(layoutDesc).build(pipelineDesc);
		}

		template<uint32_t FIF>
		Camera<FIF>::Camera(SegregatedAllocator& allocator, Device& device,
			DescriptorPool& descriptorPool, DescriptorSetLayoutList& layouts,
			HazardTracker& tracker) : m_pool(descriptorPool)
		{
			for (uint32_t i = 0; i < FIF; ++i)
			{
				m_ubs[i] = std::make_unique<UniformBuffer>(device, allocator, sizeof(ShaderViewData));
				m_sets[i] = descriptorPool.emplace(layouts[0]);
				tracker.emplace(*m_ubs[i]);

				vhl::DescriptorWriter writer(m_sets[i]);
				writer.uniformBuffer(*m_ubs[i], 0, 0, m_ubs[i]->size());
				writer.apply(device);
			}
		}

		template<uint32_t FIF>
		Camera<FIF>::~Camera()
		{
			for (uint32_t i = 0; i < FIF; ++i)
			{
				m_pool.remove(m_sets[i]);
				m_ubs[i].reset();
			}
		}

	}

#pragma endregion

	struct Renderer::Impl
	{
	public:
		Impl(RenderKernel& kern);
		~Impl();

		void begin(const core::ViewData& view);
		void draw(const core::Scene& scene);
		void end();
	private:
		void encodeUpload();
		void encodeCopy();
		void encodeBones();
		void encodeShadow();
		void encodeMainPass();
	private:

		RenderKernel& kernel;
		CommandQueue& direct;

		uint32_t drawPerFrame = 0;

		// Draw counts, recomputed every frame from buildRenderData().
		uint32_t staticDrawCount = 0;
		uint32_t skinnedDrawCount = 0;

		RenderTargets targets;
		Pipelines pipelines;

		Camera<RenderKernel::FramesInFlight> camera;

		const std::array<float, 4> defaultColor = { 0.19f, 0.30f, 0.47f, 1.0f };
		std::unique_ptr<CommandEncoder> encoder;
		ResidencyManager& residencyManager;

		struct Heaps 
		{
			Heaps(ResidencyManager& rm);
			
			GeometryHeap& geom;
			DrawHeap& draw;
			AnimationHeap& anim;
			MaterialHeap& mat;
			TextureList& texture;

		} heaps;

		ShadowMap shadowMap;
		FrameDrawData drawData;
		FrameState frame;
		UploadBatch upload;

		std::optional<asset::AABB> sceneAABB;
		SceneResidency sceneResidency;
	};

#pragma region Renderer::Impl

	Renderer::Impl::Heaps::Heaps(ResidencyManager& rm) :
		geom(ResidencyInternal::geomHeap(rm)), anim(ResidencyInternal::animHeap(rm)),
		draw(ResidencyInternal::drawHeap(rm)), mat(ResidencyInternal::matHeap(rm)),
		texture(ResidencyInternal::textures(rm))
	{}

	Renderer::Impl::Impl(RenderKernel& kern) : kernel(kern), 
		camera(KernelInternal::allocator(kern), KernelInternal::context(kern).activeDevice(), KernelInternal::descriptorPool(kern), KernelInternal::layouts(kern), KernelInternal::tracker(kern)),
		targets(KernelInternal::context(kern), KernelInternal::allocator(kern), KernelInternal::tracker(kern)),
		pipelines(KernelInternal::layouts(kern), KernelInternal::context(kern).activeDevice()), residencyManager(KernelInternal::residency(kern)),
		direct(KernelInternal::directQueue(kern)), heaps(KernelInternal::residency(kern)), 
		shadowMap(kern, main::k_ShadowMapFormat, main::k_ShadowMapResolution)
	{
		const auto fbSize = kernel.window().pixelSize();

		frame.fbSize = fbSize;

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
			KernelInternal::context(kernel).activeDevice().wait();
		}
		CATCH_GRAPHICS_ERROR
		{
		
		}
	}
	
	void Renderer::Impl::encodeUpload()
	{
		if (!upload.buffers.empty())
		{
			const auto bufs = upload.buffers;

			for (const auto& b : bufs)
			{
				auto pass = encoder->encodePass([b](vhl::CommandRecorder& recorder)
				{
					recorder.copyTo(*b.src, *b.dst, b.srcOffset, b.dstOffset, b.size);
				});

				pass.useBuffer(*b.src, UsageIntent::TransferSrc, b.srcOffset, b.size)
					.useBuffer(*b.dst, UsageIntent::TransferDst, b.dstOffset, b.size);
			}

			upload.buffers.clear();
		}

		if(!upload.images.empty())
		{
			const auto imgs = upload.images;

			for (const auto& i : imgs)
			{
				auto pass = encoder->encodePass([i](vhl::CommandRecorder& recorder)
				{
					recorder.copyTo(*i.src, *i.dst, i.srcOffset, i.width, i.height, i.depth, 0, 0, 0, i.mip, i.layer);
				});

				pass.useBuffer(*i.src, UsageIntent::TransferSrc, i.srcOffset, i.size)
					.useImage(*i.dst, UsageIntent::TransferDst);
			}

			upload.images.clear();
		}
	}
	
	void Renderer::Impl::encodeCopy()
	{
		const core::Scene* scene = frame.scene;
		
		const bool layoutChanged = scene && (
			scene->layoutVersion() != frame.layoutVersion ||
			residencyManager.version() != frame.residencyVersion);

		const bool clipChanged = scene && (scene->clipVersion() != frame.clipVersion);

		if (layoutChanged)
		{
			sceneAABB = std::nullopt;
			drawData = FrameDrawData{};
			auto animRefs = buildAnimRefs(*scene, kernel.assets(), residencyManager);

			SceneSnapshot snapshot
			{
				.scene = *scene,
				.assets = kernel.assets(),
				.animRefs = animRefs,
			};

			buildDrawData(snapshot, drawData, residencyManager);

			asset::AABB aabb;
			if (computeSceneAABB(*scene, kernel.assets(), aabb))
			{
				sceneAABB = aabb;
			}

			frame.layoutVersion = scene->layoutVersion();
			frame.clipVersion = scene->clipVersion();
			frame.residencyVersion = residencyManager.version();

			staticDrawCount = drawData.staticIndirectCount;
			skinnedDrawCount = drawData.skinnedIndirectCount;
		}
		else if (clipChanged)
		{
			auto animRefs = buildAnimRefs(*scene, kernel.assets(), residencyManager);
			applyAnimRefs(animRefs, *scene, residencyManager, drawData);
			frame.clipVersion = scene->clipVersion();
		}

		if (scene)
		{
			sceneResidency.updateState(residencyManager, *scene, drawData);
		}

		auto pass = encoder->encodePass([&, this](CommandRecorder& recorder)
		{
			if (!drawData.indirectCmds.empty())
			{
				auto& indirectTB = heaps.draw.indirectTBs(frame.slot);
				auto& indirectBuf = heaps.draw.indirectBufs(frame.slot);
				uint64_t size = drawData.indirectCmds.size() * sizeof(vhl::DrawIndexedIndirectCommand);

				memcpy(indirectTB.pMappedData(), drawData.indirectCmds.data(), size);
				recorder.copyTo(indirectTB, indirectBuf, 0, 0, size);
			}
			if (!drawData.playbacks.empty())
			{
				auto& playbackTB = heaps.anim.playbackTBs(frame.slot);

				uint64_t size = drawData.playbacks.size() * sizeof(ShaderPlaybackState);

				memcpy(playbackTB.pMappedData(), drawData.playbacks.data(), size);
				recorder.copyTo(playbackTB, heaps.anim.playbackSBs(frame.slot), 0, 0, size);
			}
				
			if (!drawData.instances.empty())
			{
				auto& instanceTB = heaps.draw.instancesTBs(frame.slot);
				auto& instanceSB = heaps.draw.instancesSBs(frame.slot);

			
				uint64_t size = drawData.instances.size() * sizeof(ShaderInstance);
				memcpy(instanceTB.pMappedData(), drawData.instances.data(), size);
				recorder.copyTo(instanceTB, instanceSB, 0, 0, size);
			}

			if (frame.viewData && frame.scene)
			{
				auto& cameraUB = camera.UBs(frame.slot);
				const ShaderViewData svd = buildShaderViewData(*scene, *frame.viewData, sceneAABB);
				recorder.updateUniformBuffer(cameraUB, &svd, 0, cameraUB.size());
			}
		});

		pass.useBuffer(heaps.anim.playbackTBs(frame.slot), UsageIntent::TransferSrc)
			.useBuffer(heaps.anim.playbackSBs(frame.slot), UsageIntent::TransferDst)
			.useBuffer(heaps.draw.indirectTBs(frame.slot), UsageIntent::TransferSrc)
			.useBuffer(heaps.draw.indirectBufs(frame.slot), UsageIntent::TransferDst)
			.useBuffer(heaps.draw.instancesTBs(frame.slot), UsageIntent::TransferSrc)
			.useBuffer(heaps.draw.instancesSBs(frame.slot), UsageIntent::TransferDst)
			.useBuffer(camera.UBs(frame.slot), UsageIntent::TransferDst);
	}

	void Renderer::Impl::encodeBones()
	{
		uint32_t totalBonesCount = totalBoneCount(drawData);
		uint32_t playbackCount = static_cast<uint32_t>(drawData.playbacks.size());

		if (totalBonesCount == 0 || playbackCount == 0) return;
		
		if (totalBonesCount > main::k_MaxSkinnedInstances * main::k_MaxBonesPerInstance) 
			throw std::runtime_error("[Renderer::Impl::encodeBones] palette overflow");

		const uint32_t groups = (totalBonesCount + 63) / 64;

		const SkinningParams params{ totalBonesCount, playbackCount };

		encoder->encodePass([this, groups, params](CommandRecorder& recorder)
		{
			recorder.bindPipeline(pipelines.boneSample());
			recorder.bindDescriptorSet(heaps.anim.sampleSets(frame.slot), PipelineType::Compute, pipelines.boneSample().layoutHandle(), 0);
			recorder.pushConstants(pipelines.boneSample().layoutHandle(), ShaderStageFlags::Compute, 0, sizeof(params), &params);
			recorder.dispatch(groups, 1, 1);
		})
		.useBuffer(heaps.anim.playbackSBs(frame.slot), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.boneLocalsSBs(frame.slot), UsageIntent::ShaderWriteCompute)
		.useBuffer(heaps.anim.boneMetasPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.animationsPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.channelsPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.keyframesPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.bindPosesPool().buffer(), UsageIntent::ShaderReadCompute);

		encoder->encodePass([this, groups, params](CommandRecorder& recorder)
		{
			recorder.bindPipeline(pipelines.boneHierarchy());
			recorder.bindDescriptorSet(heaps.anim.hierarchySets(frame.slot), PipelineType::Compute, pipelines.boneHierarchy().layoutHandle(), 0);
			recorder.pushConstants(pipelines.boneHierarchy().layoutHandle(), ShaderStageFlags::Compute, 0, sizeof(params), &params);
			recorder.dispatch(groups, 1, 1);
		})
		.useBuffer(heaps.anim.playbackSBs(frame.slot), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.boneMetasPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.inverseBindsPool().buffer(), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.boneLocalsSBs(frame.slot), UsageIntent::ShaderReadCompute)
		.useBuffer(heaps.anim.bonePaletteSBs(frame.slot), UsageIntent::ShaderWriteCompute);
	}

	void Renderer::Impl::encodeShadow()
	{
		auto& shadowView = shadowMap.view();
		auto& shadowImage = shadowMap.image();

		auto& staticVB = heaps.geom.staticPool().buffer();
		auto& skinnedVB = heaps.geom.skinnedPool().buffer();
		auto& indexBuf = heaps.geom.indexedPool().buffer();

		auto& indirectBuf = heaps.draw.indirectBufs(frame.slot);
		auto& instanceSB = heaps.draw.instancesSBs(frame.slot);

		auto& objectSet = heaps.draw.objectSets(frame.slot);
		auto& cameraSet = camera.Sets(frame.slot);

		encoder->encodePass([&, this](CommandRecorder& recorder)
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

			if (staticDrawCount > 0)
			{
				recorder.bindVertexBuffer(staticVB);
				recorder.bindPipeline(pipelines.staticShadow());
				recorder.bindIndexBuffer(indexBuf, IndexType::Uint32);

				recorder.bindDescriptorSet(cameraSet, PipelineType::Graphics, pipelines.staticShadow().layoutHandle(), 0);
				recorder.bindDescriptorSet(objectSet, PipelineType::Graphics, pipelines.staticShadow().layoutHandle(), 1);

				recorder.drawIndexedIndirect(indirectBuf, 0, staticDrawCount, sizeof(DrawIndexedIndirectCommand));
			}

			if (skinnedDrawCount > 0)
			{
				recorder.bindVertexBuffer(skinnedVB);
				recorder.bindPipeline(pipelines.skinnedShadow());
				recorder.bindIndexBuffer(indexBuf, IndexType::Uint32);

				recorder.bindDescriptorSet(cameraSet, PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 0);
				recorder.bindDescriptorSet(objectSet, PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 1);
				recorder.bindDescriptorSet(heaps.anim.hierarchySets(frame.slot), PipelineType::Graphics, pipelines.skinnedShadow().layoutHandle(), 2);

				recorder.drawIndexedIndirect(indirectBuf, staticDrawCount * sizeof(DrawIndexedIndirectCommand), skinnedDrawCount, sizeof(DrawIndexedIndirectCommand));
			}

			recorder.endRendering();
		})
		.useImage(shadowImage, UsageIntent::DepthStencilAttachment)
			.useBuffer(heaps.anim.bonePaletteSBs(frame.slot), UsageIntent::ShaderReadVertex)
			.useBuffer(camera.UBs(frame.slot), UsageIntent::UniformReadVertex)
			.useBuffer(instanceSB, UsageIntent::ShaderReadVertex)
			.useBuffer(staticVB, UsageIntent::VertexAttributeRead)
			.useBuffer(skinnedVB, UsageIntent::VertexAttributeRead)
			.useBuffer(indirectBuf, UsageIntent::IndirectRead)
			.useBuffer(indexBuf, UsageIntent::IndexRead);
	}

	void Renderer::Impl::encodeMainPass()
	{
		auto& shadowView = shadowMap.view();
		auto& shadowImage = shadowMap.image();
		auto& shadowSet = shadowMap.set();

		auto& staticVB = heaps.geom.staticPool().buffer();
		auto& skinnedVB = heaps.geom.skinnedPool().buffer();
		auto& indexBuf = heaps.geom.indexedPool().buffer();
		
		auto& indirectBuf = heaps.draw.indirectBufs(frame.slot);
		auto& instanceSB = heaps.draw.instancesSBs(frame.slot);

		auto& objectSet = heaps.draw.objectSets(frame.slot);
		auto& cameraSet = camera.Sets(frame.slot);

		auto& textures = heaps.texture;

		auto passBuilder = encoder->encodePass([&, this](CommandRecorder& recorder)
		{
			RenderingAttachment depthAttachment
			{
				.imageView = &targets.depthView(*frame.imageIndex),
				.layout = ImageLayout::DepthStencilAttachment,
				.loadOp = AttachmentLoadOp::Clear,

				.storeOp = AttachmentStoreOp::Store,
				.clearValue = 1.0f
			};

			const core::ViewData* view = frame.viewData;

			const auto clearColor = view ? view->clearColor : defaultColor;

			RenderingAttachment colorAttachment{};
			colorAttachment.loadOp = AttachmentLoadOp::Load;
			colorAttachment.storeOp = AttachmentStoreOp::Store;
			colorAttachment.clearValue = view->clearColor;

			if (targets.samples() != SampleCount::One)
			{
				colorAttachment.imageView = targets.msaaView(*frame.imageIndex);
				colorAttachment.layout = ImageLayout::ColorAttachment;

				colorAttachment.resolveImageView = &targets.swapView(*frame.imageIndex);
				colorAttachment.resolveLayout = ImageLayout::ColorAttachment;
			}
			else
			{
				colorAttachment.imageView = &targets.swapView(*frame.imageIndex);
				colorAttachment.layout = ImageLayout::ColorAttachment;

				colorAttachment.resolveImageView = nullptr;
				colorAttachment.resolveLayout = ImageLayout::Undefined;
			}

			RenderingInfo info
			{
				.width = targets.width(),
				.height = targets.height(),
				.colorAttachment = colorAttachment,
				.depthAttachment = depthAttachment
			};

			Scissor scissor{};

			if (!view || !view->scissor.has_value())
			{
				scissor.width = info.width;
				scissor.height = info.height;
			}
			else scissor = toScissor(*view->scissor);

			Viewport viewport{};

			if (!view || !view->viewport.has_value())
			{
				viewport.height = static_cast<float>(info.height);
				viewport.width = static_cast<float>(info.width);
			}
			else viewport = toViewport(*view->viewport);

			recorder.beginRendering(info);

			recorder.setScissor(scissor);
			recorder.setViewport(viewport);

			recorder.clearAttachment(ImageAspectFlags::Color, scissor, clearColor);

			if (staticDrawCount > 0)
			{
				recorder.bindVertexBuffer(staticVB);
				recorder.bindPipeline(pipelines.staticPbr());
				recorder.bindIndexBuffer(indexBuf, IndexType::Uint32);

				recorder.bindDescriptorSet(cameraSet, PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 0);
				recorder.bindDescriptorSet(objectSet, PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 1);
				recorder.bindDescriptorSet(textures.set(), PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 2);
				recorder.bindDescriptorSet(shadowSet, PipelineType::Graphics, pipelines.staticPbr().layoutHandle(), 3);

				recorder.drawIndexedIndirect(indirectBuf, 0, staticDrawCount, sizeof(vhl::DrawIndexedIndirectCommand));
			}

			if (skinnedDrawCount > 0)
			{
				recorder.bindVertexBuffer(skinnedVB);
				recorder.bindPipeline(pipelines.skinnedPbr());
				recorder.bindIndexBuffer(indexBuf, IndexType::Uint32);


				recorder.bindDescriptorSet(cameraSet, PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 0);
				recorder.bindDescriptorSet(objectSet, PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 1);
				recorder.bindDescriptorSet(textures.set(), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 2);

				recorder.bindDescriptorSet(heaps.anim.hierarchySets(frame.slot), PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 3);
				recorder.bindDescriptorSet(shadowSet, PipelineType::Graphics, pipelines.skinnedPbr().layoutHandle(), 4);

				recorder.drawIndexedIndirect(indirectBuf, staticDrawCount * sizeof(vhl::DrawIndexedIndirectCommand), skinnedDrawCount, sizeof(vhl::DrawIndexedIndirectCommand));
			}
			recorder.endRendering();
		});

		passBuilder.useImage(targets.swapImage(*frame.imageIndex), UsageIntent::ColorAttachment)
			.useImage(targets.depthImage(*frame.imageIndex), UsageIntent::DepthStencilAttachment)
			.useBuffer(heaps.anim.bonePaletteSBs(frame.slot), UsageIntent::ShaderReadVertex)
			.useBuffer(heaps.mat.pool().buffer(), UsageIntent::ShaderReadFragment)
			.useBuffer(camera.UBs(frame.slot), UsageIntent::UniformReadVertex)
			.useBuffer(instanceSB, UsageIntent::ShaderReadVertex)
			.useBuffer(staticVB, UsageIntent::VertexAttributeRead)
			.useBuffer(skinnedVB, UsageIntent::VertexAttributeRead)
			.useImage(shadowImage, UsageIntent::ShaderReadFragment)
			.useBuffer(indirectBuf, UsageIntent::IndirectRead)
			.useBuffer(indexBuf, UsageIntent::IndexRead);
			


		if (targets.samples() != SampleCount::One)
		{
			if (auto msaaImage = targets.msaaImage(*frame.imageIndex))
				passBuilder.useImage(*msaaImage, UsageIntent::ColorAttachment);
		}

		textures.foreachImage([&passBuilder](vhl::Image& img)
		{
			passBuilder.useImage(img, UsageIntent::ShaderReadFragment);
		});
	}

	void Renderer::Impl::begin(const core::ViewData& view)
	{
		auto newFbSize = kernel.window().pixelSize();

		if (newFbSize.width <= 1 || newFbSize.height <= 1)
		{
			frame.imageIndex = std::nullopt;
			return;
		}

		direct.begin();

		if (!frame.presentable || frame.fbSize != newFbSize)
		{
			targets.resize();
			frame.fbSize = newFbSize;
			frame.presentable = true;
		}

		auto& swapchain = KernelInternal::context(kernel).swapchain();

		AcquireResult acquireResult = direct.nextImageIndex(swapchain);

		frame.presentable = acquireResult.success;

		if (!frame.presentable) return;

		if (acquireResult.imageIndex.has_value())
		{
			frame.imageIndex = acquireResult.imageIndex.value();
		}

		encoder = std::make_unique<CommandEncoder>(direct, KernelInternal::tracker(kernel));

		uint32_t frameSlot = direct.frameSlot();

		ResidencyInternal::releaseRetired(residencyManager, frameSlot);
		ResidencyInternal::resetStaging(residencyManager, frameSlot);
		upload = ResidencyInternal::collectUploads(residencyManager, frameSlot);

		auto& colorImage = targets.swapImage(*frame.imageIndex);

		encoder->setDrawable(colorImage);
		
		frame.slot = frameSlot;
		frame.viewData = nullptr;
		frame.viewData = &view;

		drawPerFrame = 0;
	}

	void Renderer::Impl::draw(const core::Scene& scene)
	{
		if (!encoder) return;
		if (!frame.imageIndex.has_value() || !frame.presentable) return;

		if (drawPerFrame > 0) throw std::runtime_error("draw() can only be called once per frame — global buffers hold one scene");

		frame.slot = direct.frameSlot();
		frame.scene = &scene;
		
		drawPerFrame++;

		sceneResidency.updateResidency(residencyManager, scene, kernel.assets());
		
		//printf("================\n");

		encodeUpload();
		encodeCopy();
		encodeBones();      
		encodeShadow();   
		encodeMainPass();
	}

	void Renderer::Impl::end()
	{
		if (frame.imageIndex.has_value() && frame.presentable && encoder)
		{
			encoder->commit();

			frame.presentable = direct.end(KernelInternal::context(kernel).swapchain(), *frame.imageIndex);
			encoder.reset();
			frame.imageIndex = std::nullopt;

			drawPerFrame = 0;
		}

		frame.viewData = nullptr;
	}

	
#pragma endregion

	Renderer::~Renderer() = default;

	Renderer::Renderer(RenderKernel& kernel) : m_impl(new Impl(kernel))
	{
	}

	void Renderer::render(const core::ViewData& view, const core::Scene& scene)
	{
		if (m_impl) 
		{
			m_impl->begin(view);
			m_impl->draw(scene);
			m_impl->end();
		}
	}
}