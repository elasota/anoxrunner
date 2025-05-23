#pragma once
#pragma once

#include "rkit/Data/RenderDataHandler.h"

namespace rkit { namespace data
{
	class RenderDataHandler final : public IRenderDataHandler
	{
	public:
		const RenderRTTIStructType *GetSamplerDescRTTI() const override;
		const RenderRTTIStructType *GetPushConstantDescRTTI() const override;
		const RenderRTTIStructType *GetDescriptorDescRTTI() const override;
		const RenderRTTIEnumType *GetDescriptorTypeRTTI() const override;
		const RenderRTTIStructType *GetGraphicsPipelineDescRTTI() const override;
		const RenderRTTIStructType *GetGraphicsPipelineNameLookupRTTI() const override;
		const RenderRTTIStructType *GetRenderTargetDescRTTI() const override;
		const RenderRTTIStructType *GetRenderOperationDescRTTI() const override;
		const RenderRTTIStructType *GetShaderDescRTTI() const override;
		const RenderRTTIStructType *GetDepthStencilOperationDescRTTI() const override;
		const RenderRTTIEnumType *GetNumericTypeRTTI() const override;
		const RenderRTTIEnumType *GetInputLayoutVertexInputSteppingRTTI() const override;
		const RenderRTTIObjectPtrType *GetVectorNumericTypePtrRTTI() const override;
		const RenderRTTIObjectPtrType *GetCompoundNumericTypePtrRTTI() const override;
		const RenderRTTIObjectPtrType *GetStructureTypePtrRTTI() const override;
		const RenderRTTIStructType *GetRenderPassDescRTTI() const override;
		const RenderRTTIStructType *GetRenderPassNameLookupRTTI() const override;
		const RenderRTTIStructType *GetDepthStencilTargetDescRTTI() const override;

		Result ProcessIndexable(RenderRTTIIndexableStructType indexableStructType, UniquePtr<IRenderRTTIListBase> *outList, UniquePtr<IRenderRTTIObjectPtrList> *outPtrList, const RenderRTTIStructType **outRTTI) const override;

		uint32_t GetPackageVersion() const override;
		uint32_t GetPackageIdentifier() const override;

		Result LoadPackage(IReadStream &stream, bool allowTempStrings, data::IRenderDataConfigurator *configurator, UniquePtr<IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *outBinaryContent) const override;
	};
} } // rkit::data
