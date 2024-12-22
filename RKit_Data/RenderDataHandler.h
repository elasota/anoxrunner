#pragma once
#pragma once

#include "rkit/Data/RenderDataHandler.h"

namespace rkit::data
{
	class RenderDataHandler final : public IRenderDataHandler
	{
	public:
		const RenderRTTIStructType *GetSamplerDescRTTI() const override;
		const RenderRTTIStructType *GetPushConstantDescRTTI() const override;
		const RenderRTTIEnumType *GetInputLayoutVertexInputSteppingRTTI() const override;
		const RenderRTTIStructType *GetDescriptorDescRTTI() const override;
		const RenderRTTIEnumType *GetDescriptorTypeRTTI() const override;
		const RenderRTTIStructType *GetGraphicsPipelineDescRTTI() const override;
		const RenderRTTIStructType *GetGraphicsPipelineNameLookupRTTI() const override;
		const RenderRTTIStructType *GetRenderTargetDescRTTI() const override;
		const RenderRTTIStructType *GetShaderDescRTTI() const override;
		const RenderRTTIStructType *GetDepthStencilDescRTTI() const override;
		const RenderRTTIEnumType *GetNumericTypeRTTI() const override;
		const RenderRTTIObjectPtrType *GetVectorNumericTypePtrRTTI() const override;
		const RenderRTTIObjectPtrType *GetCompoundNumericTypePtrRTTI() const override;
		const RenderRTTIObjectPtrType *GetStructureTypePtrRTTI() const override;

		Result ProcessIndexable(RenderRTTIIndexableStructType indexableStructType, UniquePtr<IRenderRTTIListBase> *outList, UniquePtr<IRenderRTTIObjectPtrList> *outPtrList, const RenderRTTIStructType **outRTTI) const override;

		uint32_t GetPackageVersion() const override;
		uint32_t GetPackageIdentifier() const override;

		Result LoadPackage(IReadStream &stream, bool allowTempStrings, UniquePtr<IRenderDataPackage> &outPackage, Vector<Vector<uint8_t>> *outBinaryContent) const override;
	};
}
