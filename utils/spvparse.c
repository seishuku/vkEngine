#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "../math/math.h"
#include "../system/system.h"
// Local copy from the Vulkan SDK, because some SDK deployments are dumb and don't include this or are in differing locations.
#include "spirv.h"
#include "spvparse.h"

#define SPV_SPIRV_VERSION_MAJOR_PART(WORD) (((uint32_t)(WORD)>>16)&0xFF)
#define SPV_SPIRV_VERSION_MINOR_PART(WORD) (((uint32_t)(WORD)>>8)&0xFF)

#define SPV_MAX_MEMBERS 32
#define SPV_MAX_MEMBER_DECORATIONS 8
#define SPV_MAX_LITERALS 32

typedef struct
{
	uint32_t targetID;
	struct
	{
		SpvDecoration decoration;
		uint32_t decorationValue;
	} decorations[SPV_MAX_MEMBER_DECORATIONS];
	uint32_t decorationCount;
} SpvDecorate_t;

typedef struct
{
	uint32_t structTypeID;
	struct
	{
		struct
		{
			SpvDecoration decoration;
			uint32_t decorationValue;
		} decorations[SPV_MAX_MEMBER_DECORATIONS];
		uint32_t decorationCount;
		char name[SPV_REFLECT_MAX_NAME];
	} member[SPV_MAX_MEMBERS];
} SpvMemberDecorate_t;

typedef struct
{
	uint32_t resultID;
	union
	{
		struct
		{
			uint32_t intWidth;
			bool intSignedness;
		};
		uint32_t floatWidth;
		struct
		{
			uint32_t vectorComponentTypeID;
			uint32_t vectorComponentCount;
		};
		struct
		{
			uint32_t matrixColumnTypeID;
			uint32_t matrixColumnCount;
		};
		struct
		{
			uint32_t imageSampledTypeID;
			SpvDim imageDim;
			uint8_t imageDepth;
			bool imageArrayed;
			bool imageMultisample;
			uint8_t imageSampled;
			SpvImageFormat imageFormat;
			SpvAccessQualifier imageAccessQualifier;
		};
		uint32_t sampledImageTypeID;
		struct
		{
			uint32_t arrayElementTypeID;
			uint32_t arrayLengthID;
		};
		struct
		{
			uint32_t runtimeArrayElementTypeID;
		};
		struct
		{
			uint32_t structMemberTypeIDs[SPV_MAX_MEMBERS];
			uint32_t structMemberCount;
		};
		const char *opaqueTypeName;
		struct
		{
			SpvStorageClass pointerStorageClass;
			uint32_t pointerTypeID;
		};
		SpvAccessQualifier pipeAccessQualifier;
	};
} SpvType_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	bool value;
} SpvConstantBool_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	union
	{
		double fLiteral;
		int64_t iLiteral;
	} literals[SPV_MAX_LITERALS];
	uint32_t literalCount;
} SpvConstant_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	SpvStorageClass storageClass;
	uint32_t initializerID;
} SpvVariable_t;

typedef struct
{
	SpvOp opCode;
	char name[SPV_REFLECT_MAX_NAME];

	SpvDecorate_t decoration;
	SpvMemberDecorate_t memberDecoration;

	union
	{
		SpvType_t type;
		SpvConstantBool_t constantBool;
		SpvConstant_t constant;
		SpvVariable_t variable;
	};
} SpvID_t;

typedef struct
{
	uint32_t magicNumber;
	uint32_t version;
	uint32_t generator;
	uint32_t bound;
	uint32_t schema;
} SpvHeader_t;

static inline const char *printSpvOp(const SpvOp op)
{
	switch(op)
	{
		case SpvOpNop:										return "SpvOpNop";
		case SpvOpUndef:									return "SpvOpUndef";
		case SpvOpSourceContinued:							return "SpvOpSourceContinued";
		case SpvOpSource:									return "SpvOpSource";
		case SpvOpSourceExtension:							return "SpvOpSourceExtension";
		case SpvOpName:										return "SpvOpName";
		case SpvOpMemberName:								return "SpvOpMemberName";
		case SpvOpString:									return "SpvOpString";
		case SpvOpLine:										return "SpvOpLine";
		case SpvOpExtension:								return "SpvOpExtension";
		case SpvOpExtInstImport:							return "SpvOpExtInstImport";
		case SpvOpExtInst:									return "SpvOpExtInst";
		case SpvOpMemoryModel:								return "SpvOpMemoryModel";
		case SpvOpEntryPoint:								return "SpvOpEntryPoint";
		case SpvOpExecutionMode:							return "SpvOpExecutionMode";
		case SpvOpCapability:								return "SpvOpCapability";
		case SpvOpTypeVoid:									return "SpvOpTypeVoid";
		case SpvOpTypeBool:									return "SpvOpTypeBool";
		case SpvOpTypeInt:									return "SpvOpTypeInt";
		case SpvOpTypeFloat:								return "SpvOpTypeFloat";
		case SpvOpTypeVector:								return "SpvOpTypeVector";
		case SpvOpTypeMatrix:								return "SpvOpTypeMatrix";
		case SpvOpTypeImage:								return "SpvOpTypeImage";
		case SpvOpTypeSampler:								return "SpvOpTypeSampler";
		case SpvOpTypeSampledImage:							return "SpvOpTypeSampledImage";
		case SpvOpTypeArray:								return "SpvOpTypeArray";
		case SpvOpTypeRuntimeArray:							return "SpvOpTypeRuntimeArray";
		case SpvOpTypeStruct:								return "SpvOpTypeStruct";
		case SpvOpTypeOpaque:								return "SpvOpTypeOpaque";
		case SpvOpTypePointer:								return "SpvOpTypePointer";
		case SpvOpTypeFunction:								return "SpvOpTypeFunction";
		case SpvOpTypeEvent:								return "SpvOpTypeEvent";
		case SpvOpTypeDeviceEvent:							return "SpvOpTypeDeviceEvent";
		case SpvOpTypeReserveId:							return "SpvOpTypeReserveId";
		case SpvOpTypeQueue:								return "SpvOpTypeQueue";
		case SpvOpTypePipe:									return "SpvOpTypePipe";
		case SpvOpTypeForwardPointer:						return "SpvOpTypeForwardPointer";
		case SpvOpConstantTrue:								return "SpvOpConstantTrue";
		case SpvOpConstantFalse:							return "SpvOpConstantFalse";
		case SpvOpConstant:									return "SpvOpConstant";
		case SpvOpConstantComposite:						return "SpvOpConstantComposite";
		case SpvOpConstantSampler:							return "SpvOpConstantSampler";
		case SpvOpConstantNull:								return "SpvOpConstantNull";
		case SpvOpSpecConstantTrue:							return "SpvOpSpecConstantTrue";
		case SpvOpSpecConstantFalse:						return "SpvOpSpecConstantFalse";
		case SpvOpSpecConstant:								return "SpvOpSpecConstant";
		case SpvOpSpecConstantComposite:					return "SpvOpSpecConstantComposite";
		case SpvOpSpecConstantOp:							return "SpvOpSpecConstantOp";
		case SpvOpFunction:									return "SpvOpFunction";
		case SpvOpFunctionParameter:						return "SpvOpFunctionParameter";
		case SpvOpFunctionEnd:								return "SpvOpFunctionEnd";
		case SpvOpFunctionCall:								return "SpvOpFunctionCall";
		case SpvOpVariable:									return "SpvOpVariable";
		case SpvOpImageTexelPointer:						return "SpvOpImageTexelPointer";
		case SpvOpLoad:										return "SpvOpLoad";
		case SpvOpStore:									return "SpvOpStore";
		case SpvOpCopyMemory:								return "SpvOpCopyMemory";
		case SpvOpCopyMemorySized:							return "SpvOpCopyMemorySized";
		case SpvOpAccessChain:								return "SpvOpAccessChain";
		case SpvOpInBoundsAccessChain:						return "SpvOpInBoundsAccessChain";
		case SpvOpPtrAccessChain:							return "SpvOpPtrAccessChain";
		case SpvOpArrayLength:								return "SpvOpArrayLength";
		case SpvOpGenericPtrMemSemantics:					return "SpvOpGenericPtrMemSemantics";
		case SpvOpInBoundsPtrAccessChain:					return "SpvOpInBoundsPtrAccessChain";
		case SpvOpDecorate:									return "SpvOpDecorate";
		case SpvOpMemberDecorate:							return "SpvOpMemberDecorate";
		case SpvOpDecorationGroup:							return "SpvOpDecorationGroup";
		case SpvOpGroupDecorate:							return "SpvOpGroupDecorate";
		case SpvOpGroupMemberDecorate:						return "SpvOpGroupMemberDecorate";
		case SpvOpVectorExtractDynamic:						return "SpvOpVectorExtractDynamic";
		case SpvOpVectorInsertDynamic:						return "SpvOpVectorInsertDynamic";
		case SpvOpVectorShuffle:							return "SpvOpVectorShuffle";
		case SpvOpCompositeConstruct:						return "SpvOpCompositeConstruct";
		case SpvOpCompositeExtract:							return "SpvOpCompositeExtract";
		case SpvOpCompositeInsert:							return "SpvOpCompositeInsert";
		case SpvOpCopyObject:								return "SpvOpCopyObject";
		case SpvOpTranspose:								return "SpvOpTranspose";
		case SpvOpSampledImage:								return "SpvOpSampledImage";
		case SpvOpImageSampleImplicitLod:					return "SpvOpImageSampleImplicitLod";
		case SpvOpImageSampleExplicitLod:					return "SpvOpImageSampleExplicitLod";
		case SpvOpImageSampleDrefImplicitLod:				return "SpvOpImageSampleDrefImplicitLod";
		case SpvOpImageSampleDrefExplicitLod:				return "SpvOpImageSampleDrefExplicitLod";
		case SpvOpImageSampleProjImplicitLod:				return "SpvOpImageSampleProjImplicitLod";
		case SpvOpImageSampleProjExplicitLod:				return "SpvOpImageSampleProjExplicitLod";
		case SpvOpImageSampleProjDrefImplicitLod:			return "SpvOpImageSampleProjDrefImplicitLod";
		case SpvOpImageSampleProjDrefExplicitLod:			return "SpvOpImageSampleProjDrefExplicitLod";
		case SpvOpImageFetch:								return "SpvOpImageFetch";
		case SpvOpImageGather:								return "SpvOpImageGather";
		case SpvOpImageDrefGather:							return "SpvOpImageDrefGather";
		case SpvOpImageRead:								return "SpvOpImageRead";
		case SpvOpImageWrite:								return "SpvOpImageWrite";
		case SpvOpImage:									return "SpvOpImage";
		case SpvOpImageQueryFormat:							return "SpvOpImageQueryFormat";
		case SpvOpImageQueryOrder:							return "SpvOpImageQueryOrder";
		case SpvOpImageQuerySizeLod:						return "SpvOpImageQuerySizeLod";
		case SpvOpImageQuerySize:							return "SpvOpImageQuerySize";
		case SpvOpImageQueryLod:							return "SpvOpImageQueryLod";
		case SpvOpImageQueryLevels:							return "SpvOpImageQueryLevels";
		case SpvOpImageQuerySamples:						return "SpvOpImageQuerySamples";
		case SpvOpConvertFToU:								return "SpvOpConvertFToU";
		case SpvOpConvertFToS:								return "SpvOpConvertFToS";
		case SpvOpConvertSToF:								return "SpvOpConvertSToF";
		case SpvOpConvertUToF:								return "SpvOpConvertUToF";
		case SpvOpUConvert:									return "SpvOpUConvert";
		case SpvOpSConvert:									return "SpvOpSConvert";
		case SpvOpFConvert:									return "SpvOpFConvert";
		case SpvOpQuantizeToF16:							return "SpvOpQuantizeToF16";
		case SpvOpConvertPtrToU:							return "SpvOpConvertPtrToU";
		case SpvOpSatConvertSToU:							return "SpvOpSatConvertSToU";
		case SpvOpSatConvertUToS:							return "SpvOpSatConvertUToS";
		case SpvOpConvertUToPtr:							return "SpvOpConvertUToPtr";
		case SpvOpPtrCastToGeneric:							return "SpvOpPtrCastToGeneric";
		case SpvOpGenericCastToPtr:							return "SpvOpGenericCastToPtr";
		case SpvOpGenericCastToPtrExplicit:					return "SpvOpGenericCastToPtrExplicit";
		case SpvOpBitcast:									return "SpvOpBitcast";
		case SpvOpSNegate:									return "SpvOpSNegate";
		case SpvOpFNegate:									return "SpvOpFNegate";
		case SpvOpIAdd:										return "SpvOpIAdd";
		case SpvOpFAdd:										return "SpvOpFAdd";
		case SpvOpISub:										return "SpvOpISub";
		case SpvOpFSub:										return "SpvOpFSub";
		case SpvOpIMul:										return "SpvOpIMul";
		case SpvOpFMul:										return "SpvOpFMul";
		case SpvOpUDiv:										return "SpvOpUDiv";
		case SpvOpSDiv:										return "SpvOpSDiv";
		case SpvOpFDiv:										return "SpvOpFDiv";
		case SpvOpUMod:										return "SpvOpUMod";
		case SpvOpSRem:										return "SpvOpSRem";
		case SpvOpSMod:										return "SpvOpSMod";
		case SpvOpFRem:										return "SpvOpFRem";
		case SpvOpFMod:										return "SpvOpFMod";
		case SpvOpVectorTimesScalar:						return "SpvOpVectorTimesScalar";
		case SpvOpMatrixTimesScalar:						return "SpvOpMatrixTimesScalar";
		case SpvOpVectorTimesMatrix:						return "SpvOpVectorTimesMatrix";
		case SpvOpMatrixTimesVector:						return "SpvOpMatrixTimesVector";
		case SpvOpMatrixTimesMatrix:						return "SpvOpMatrixTimesMatrix";
		case SpvOpOuterProduct:								return "SpvOpOuterProduct";
		case SpvOpDot:										return "SpvOpDot";
		case SpvOpIAddCarry:								return "SpvOpIAddCarry";
		case SpvOpISubBorrow:								return "SpvOpISubBorrow";
		case SpvOpUMulExtended:								return "SpvOpUMulExtended";
		case SpvOpSMulExtended:								return "SpvOpSMulExtended";
		case SpvOpAny:										return "SpvOpAny";
		case SpvOpAll:										return "SpvOpAll";
		case SpvOpIsNan:									return "SpvOpIsNan";
		case SpvOpIsInf:									return "SpvOpIsInf";
		case SpvOpIsFinite:									return "SpvOpIsFinite";
		case SpvOpIsNormal:									return "SpvOpIsNormal";
		case SpvOpSignBitSet:								return "SpvOpSignBitSet";
		case SpvOpLessOrGreater:							return "SpvOpLessOrGreater";
		case SpvOpOrdered:									return "SpvOpOrdered";
		case SpvOpUnordered:								return "SpvOpUnordered";
		case SpvOpLogicalEqual:								return "SpvOpLogicalEqual";
		case SpvOpLogicalNotEqual:							return "SpvOpLogicalNotEqual";
		case SpvOpLogicalOr:								return "SpvOpLogicalOr";
		case SpvOpLogicalAnd:								return "SpvOpLogicalAnd";
		case SpvOpLogicalNot:								return "SpvOpLogicalNot";
		case SpvOpSelect:									return "SpvOpSelect";
		case SpvOpIEqual:									return "SpvOpIEqual";
		case SpvOpINotEqual:								return "SpvOpINotEqual";
		case SpvOpUGreaterThan:								return "SpvOpUGreaterThan";
		case SpvOpSGreaterThan:								return "SpvOpSGreaterThan";
		case SpvOpUGreaterThanEqual:						return "SpvOpUGreaterThanEqual";
		case SpvOpSGreaterThanEqual:						return "SpvOpSGreaterThanEqual";
		case SpvOpULessThan:								return "SpvOpULessThan";
		case SpvOpSLessThan:								return "SpvOpSLessThan";
		case SpvOpULessThanEqual:							return "SpvOpULessThanEqual";
		case SpvOpSLessThanEqual:							return "SpvOpSLessThanEqual";
		case SpvOpFOrdEqual:								return "SpvOpFOrdEqual";
		case SpvOpFUnordEqual:								return "SpvOpFUnordEqual";
		case SpvOpFOrdNotEqual:								return "SpvOpFOrdNotEqual";
		case SpvOpFUnordNotEqual:							return "SpvOpFUnordNotEqual";
		case SpvOpFOrdLessThan:								return "SpvOpFOrdLessThan";
		case SpvOpFUnordLessThan:							return "SpvOpFUnordLessThan";
		case SpvOpFOrdGreaterThan:							return "SpvOpFOrdGreaterThan";
		case SpvOpFUnordGreaterThan:						return "SpvOpFUnordGreaterThan";
		case SpvOpFOrdLessThanEqual:						return "SpvOpFOrdLessThanEqual";
		case SpvOpFUnordLessThanEqual:						return "SpvOpFUnordLessThanEqual";
		case SpvOpFOrdGreaterThanEqual:						return "SpvOpFOrdGreaterThanEqual";
		case SpvOpFUnordGreaterThanEqual:					return "SpvOpFUnordGreaterThanEqual";
		case SpvOpShiftRightLogical:						return "SpvOpShiftRightLogical";
		case SpvOpShiftRightArithmetic:						return "SpvOpShiftRightArithmetic";
		case SpvOpShiftLeftLogical:							return "SpvOpShiftLeftLogical";
		case SpvOpBitwiseOr:								return "SpvOpBitwiseOr";
		case SpvOpBitwiseXor:								return "SpvOpBitwiseXor";
		case SpvOpBitwiseAnd:								return "SpvOpBitwiseAnd";
		case SpvOpNot:										return "SpvOpNot";
		case SpvOpBitFieldInsert:							return "SpvOpBitFieldInsert";
		case SpvOpBitFieldSExtract:							return "SpvOpBitFieldSExtract";
		case SpvOpBitFieldUExtract:							return "SpvOpBitFieldUExtract";
		case SpvOpBitReverse:								return "SpvOpBitReverse";
		case SpvOpBitCount:									return "SpvOpBitCount";
		case SpvOpDPdx:										return "SpvOpDPdx";
		case SpvOpDPdy:										return "SpvOpDPdy";
		case SpvOpFwidth:									return "SpvOpFwidth";
		case SpvOpDPdxFine:									return "SpvOpDPdxFine";
		case SpvOpDPdyFine:									return "SpvOpDPdyFine";
		case SpvOpFwidthFine:								return "SpvOpFwidthFine";
		case SpvOpDPdxCoarse:								return "SpvOpDPdxCoarse";
		case SpvOpDPdyCoarse:								return "SpvOpDPdyCoarse";
		case SpvOpFwidthCoarse:								return "SpvOpFwidthCoarse";
		case SpvOpEmitVertex:								return "SpvOpEmitVertex";
		case SpvOpEndPrimitive:								return "SpvOpEndPrimitive";
		case SpvOpEmitStreamVertex:							return "SpvOpEmitStreamVertex";
		case SpvOpEndStreamPrimitive:						return "SpvOpEndStreamPrimitive";
		case SpvOpControlBarrier:							return "SpvOpControlBarrier";
		case SpvOpMemoryBarrier:							return "SpvOpMemoryBarrier";
		case SpvOpAtomicLoad:								return "SpvOpAtomicLoad";
		case SpvOpAtomicStore:								return "SpvOpAtomicStore";
		case SpvOpAtomicExchange:							return "SpvOpAtomicExchange";
		case SpvOpAtomicCompareExchange:					return "SpvOpAtomicCompareExchange";
		case SpvOpAtomicCompareExchangeWeak:				return "SpvOpAtomicCompareExchangeWeak";
		case SpvOpAtomicIIncrement:							return "SpvOpAtomicIIncrement";
		case SpvOpAtomicIDecrement:							return "SpvOpAtomicIDecrement";
		case SpvOpAtomicIAdd:								return "SpvOpAtomicIAdd";
		case SpvOpAtomicISub:								return "SpvOpAtomicISub";
		case SpvOpAtomicSMin:								return "SpvOpAtomicSMin";
		case SpvOpAtomicUMin:								return "SpvOpAtomicUMin";
		case SpvOpAtomicSMax:								return "SpvOpAtomicSMax";
		case SpvOpAtomicUMax:								return "SpvOpAtomicUMax";
		case SpvOpAtomicAnd:								return "SpvOpAtomicAnd";
		case SpvOpAtomicOr:									return "SpvOpAtomicOr";
		case SpvOpAtomicXor:								return "SpvOpAtomicXor";
		case SpvOpPhi:										return "SpvOpPhi";
		case SpvOpLoopMerge:								return "SpvOpLoopMerge";
		case SpvOpSelectionMerge:							return "SpvOpSelectionMerge";
		case SpvOpLabel:									return "SpvOpLabel";
		case SpvOpBranch:									return "SpvOpBranch";
		case SpvOpBranchConditional:						return "SpvOpBranchConditional";
		case SpvOpSwitch:									return "SpvOpSwitch";
		case SpvOpKill:										return "SpvOpKill";
		case SpvOpReturn:									return "SpvOpReturn";
		case SpvOpReturnValue:								return "SpvOpReturnValue";
		case SpvOpUnreachable:								return "SpvOpUnreachable";
		case SpvOpLifetimeStart:							return "SpvOpLifetimeStart";
		case SpvOpLifetimeStop:								return "SpvOpLifetimeStop";
		case SpvOpGroupAsyncCopy:							return "SpvOpGroupAsyncCopy";
		case SpvOpGroupWaitEvents:							return "SpvOpGroupWaitEvents";
		case SpvOpGroupAll:									return "SpvOpGroupAll";
		case SpvOpGroupAny:									return "SpvOpGroupAny";
		case SpvOpGroupBroadcast:							return "SpvOpGroupBroadcast";
		case SpvOpGroupIAdd:								return "SpvOpGroupIAdd";
		case SpvOpGroupFAdd:								return "SpvOpGroupFAdd";
		case SpvOpGroupFMin:								return "SpvOpGroupFMin";
		case SpvOpGroupUMin:								return "SpvOpGroupUMin";
		case SpvOpGroupSMin:								return "SpvOpGroupSMin";
		case SpvOpGroupFMax:								return "SpvOpGroupFMax";
		case SpvOpGroupUMax:								return "SpvOpGroupUMax";
		case SpvOpGroupSMax:								return "SpvOpGroupSMax";
		case SpvOpReadPipe:									return "SpvOpReadPipe";
		case SpvOpWritePipe:								return "SpvOpWritePipe";
		case SpvOpReservedReadPipe:							return "SpvOpReservedReadPipe";
		case SpvOpReservedWritePipe:						return "SpvOpReservedWritePipe";
		case SpvOpReserveReadPipePackets:					return "SpvOpReserveReadPipePackets";
		case SpvOpReserveWritePipePackets:					return "SpvOpReserveWritePipePackets";
		case SpvOpCommitReadPipe:							return "SpvOpCommitReadPipe";
		case SpvOpCommitWritePipe:							return "SpvOpCommitWritePipe";
		case SpvOpIsValidReserveId:							return "SpvOpIsValidReserveId";
		case SpvOpGetNumPipePackets:						return "SpvOpGetNumPipePackets";
		case SpvOpGetMaxPipePackets:						return "SpvOpGetMaxPipePackets";
		case SpvOpGroupReserveReadPipePackets:				return "SpvOpGroupReserveReadPipePackets";
		case SpvOpGroupReserveWritePipePackets:				return "SpvOpGroupReserveWritePipePackets";
		case SpvOpGroupCommitReadPipe:						return "SpvOpGroupCommitReadPipe";
		case SpvOpGroupCommitWritePipe:						return "SpvOpGroupCommitWritePipe";
		case SpvOpEnqueueMarker:							return "SpvOpEnqueueMarker";
		case SpvOpEnqueueKernel:							return "SpvOpEnqueueKernel";
		case SpvOpGetKernelNDrangeSubGroupCount:			return "SpvOpGetKernelNDrangeSubGroupCount";
		case SpvOpGetKernelNDrangeMaxSubGroupSize:			return "SpvOpGetKernelNDrangeMaxSubGroupSize";
		case SpvOpGetKernelWorkGroupSize:					return "SpvOpGetKernelWorkGroupSize";
		case SpvOpGetKernelPreferredWorkGroupSizeMultiple:	return "SpvOpGetKernelPreferredWorkGroupSizeMultiple";
		case SpvOpRetainEvent:								return "SpvOpRetainEvent";
		case SpvOpReleaseEvent:								return "SpvOpReleaseEvent";
		case SpvOpCreateUserEvent:							return "SpvOpCreateUserEvent";
		case SpvOpIsValidEvent:								return "SpvOpIsValidEvent";
		case SpvOpSetUserEventStatus:						return "SpvOpSetUserEventStatus";
		case SpvOpCaptureEventProfilingInfo:				return "SpvOpCaptureEventProfilingInfo";
		case SpvOpGetDefaultQueue:							return "SpvOpGetDefaultQueue";
		case SpvOpBuildNDRange:								return "SpvOpBuildNDRange";
		case SpvOpImageSparseSampleImplicitLod:				return "SpvOpImageSparseSampleImplicitLod";
		case SpvOpImageSparseSampleExplicitLod:				return "SpvOpImageSparseSampleExplicitLod";
		case SpvOpImageSparseSampleDrefImplicitLod:			return "SpvOpImageSparseSampleDrefImplicitLod";
		case SpvOpImageSparseSampleDrefExplicitLod:			return "SpvOpImageSparseSampleDrefExplicitLod";
		case SpvOpImageSparseSampleProjImplicitLod:			return "SpvOpImageSparseSampleProjImplicitLod";
		case SpvOpImageSparseSampleProjExplicitLod:			return "SpvOpImageSparseSampleProjExplicitLod";
		case SpvOpImageSparseSampleProjDrefImplicitLod:		return "SpvOpImageSparseSampleProjDrefImplicitLod";
		case SpvOpImageSparseSampleProjDrefExplicitLod:		return "SpvOpImageSparseSampleProjDrefExplicitLod";
		case SpvOpImageSparseFetch:							return "SpvOpImageSparseFetch";
		case SpvOpImageSparseGather:						return "SpvOpImageSparseGather";
		case SpvOpImageSparseDrefGather:					return "SpvOpImageSparseDrefGather";
		case SpvOpImageSparseTexelsResident:				return "SpvOpImageSparseTexelsResident";
		case SpvOpNoLine:									return "SpvOpNoLine";
		case SpvOpAtomicFlagTestAndSet:						return "SpvOpAtomicFlagTestAndSet";
		case SpvOpAtomicFlagClear:							return "SpvOpAtomicFlagClear";
		case SpvOpImageSparseRead:							return "SpvOpImageSparseRead";
		case SpvOpSizeOf:									return "SpvOpSizeOf";
		case SpvOpTypePipeStorage:							return "SpvOpTypePipeStorage";
		case SpvOpConstantPipeStorage:						return "SpvOpConstantPipeStorage";
		case SpvOpCreatePipeFromPipeStorage:				return "SpvOpCreatePipeFromPipeStorage";
		case SpvOpGetKernelLocalSizeForSubgroupCount:		return "SpvOpGetKernelLocalSizeForSubgroupCount";
		case SpvOpGetKernelMaxNumSubgroups:					return "SpvOpGetKernelMaxNumSubgroups";
		case SpvOpTypeNamedBarrier:							return "SpvOpTypeNamedBarrier";
		case SpvOpNamedBarrierInitialize:					return "SpvOpNamedBarrierInitialize";
		case SpvOpMemoryNamedBarrier:						return "SpvOpMemoryNamedBarrier";
		case SpvOpModuleProcessed:							return "SpvOpModuleProcessed";
		case SpvOpExecutionModeId:							return "SpvOpExecutionModeId";
		case SpvOpDecorateId:								return "SpvOpDecorateId";
		case SpvOpGroupNonUniformElect:						return "SpvOpGroupNonUniformElect";
		case SpvOpGroupNonUniformAll:						return "SpvOpGroupNonUniformAll";
		case SpvOpGroupNonUniformAny:						return "SpvOpGroupNonUniformAny";
		case SpvOpGroupNonUniformAllEqual:					return "SpvOpGroupNonUniformAllEqual";
		case SpvOpGroupNonUniformBroadcast:					return "SpvOpGroupNonUniformBroadcast";
		case SpvOpGroupNonUniformBroadcastFirst:			return "SpvOpGroupNonUniformBroadcastFirst";
		case SpvOpGroupNonUniformBallot:					return "SpvOpGroupNonUniformBallot";
		case SpvOpGroupNonUniformInverseBallot:				return "SpvOpGroupNonUniformInverseBallot";
		case SpvOpGroupNonUniformBallotBitExtract:			return "SpvOpGroupNonUniformBallotBitExtract";
		case SpvOpGroupNonUniformBallotBitCount:			return "SpvOpGroupNonUniformBallotBitCount";
		case SpvOpGroupNonUniformBallotFindLSB:				return "SpvOpGroupNonUniformBallotFindLSB";
		case SpvOpGroupNonUniformBallotFindMSB:				return "SpvOpGroupNonUniformBallotFindMSB";
		case SpvOpGroupNonUniformShuffle:					return "SpvOpGroupNonUniformShuffle";
		case SpvOpGroupNonUniformShuffleXor:				return "SpvOpGroupNonUniformShuffleXor";
		case SpvOpGroupNonUniformShuffleUp:					return "SpvOpGroupNonUniformShuffleUp";
		case SpvOpGroupNonUniformShuffleDown:				return "SpvOpGroupNonUniformShuffleDown";
		case SpvOpGroupNonUniformIAdd:						return "SpvOpGroupNonUniformIAdd";
		case SpvOpGroupNonUniformFAdd:						return "SpvOpGroupNonUniformFAdd";
		case SpvOpGroupNonUniformIMul:						return "SpvOpGroupNonUniformIMul";
		case SpvOpGroupNonUniformFMul:						return "SpvOpGroupNonUniformFMul";
		case SpvOpGroupNonUniformSMin:						return "SpvOpGroupNonUniformSMin";
		case SpvOpGroupNonUniformUMin:						return "SpvOpGroupNonUniformUMin";
		case SpvOpGroupNonUniformFMin:						return "SpvOpGroupNonUniformFMin";
		case SpvOpGroupNonUniformSMax:						return "SpvOpGroupNonUniformSMax";
		case SpvOpGroupNonUniformUMax:						return "SpvOpGroupNonUniformUMax";
		case SpvOpGroupNonUniformFMax:						return "SpvOpGroupNonUniformFMax";
		case SpvOpGroupNonUniformBitwiseAnd:				return "SpvOpGroupNonUniformBitwiseAnd";
		case SpvOpGroupNonUniformBitwiseOr:					return "SpvOpGroupNonUniformBitwiseOr";
		case SpvOpGroupNonUniformBitwiseXor:				return "SpvOpGroupNonUniformBitwiseXor";
		case SpvOpGroupNonUniformLogicalAnd:				return "SpvOpGroupNonUniformLogicalAnd";
		case SpvOpGroupNonUniformLogicalOr:					return "SpvOpGroupNonUniformLogicalOr";
		case SpvOpGroupNonUniformLogicalXor:				return "SpvOpGroupNonUniformLogicalXor";
		case SpvOpGroupNonUniformQuadBroadcast:				return "SpvOpGroupNonUniformQuadBroadcast";
		case SpvOpGroupNonUniformQuadSwap:					return "SpvOpGroupNonUniformQuadSwap";
		case SpvOpCopyLogical:								return "SpvOpCopyLogical";
		case SpvOpPtrEqual:									return "SpvOpPtrEqual";
		case SpvOpPtrNotEqual:								return "SpvOpPtrNotEqual";
		case SpvOpPtrDiff:									return "SpvOpPtrDiff";
		default:											return "unknown";
	}
}

static inline const char *printSpvDecoration(const SpvDecoration decoration)
{
	switch(decoration)
	{
		case SpvDecorationRelaxedPrecision:		return "SpvDecorationRelaxedPrecision";
		case SpvDecorationSpecId:				return "SpvDecorationSpecId";
		case SpvDecorationBlock:				return "SpvDecorationBlock";
		case SpvDecorationBufferBlock:			return "SpvDecorationBufferBlock";
		case SpvDecorationRowMajor:				return "SpvDecorationRowMajor";
		case SpvDecorationColMajor:				return "SpvDecorationColMajor";
		case SpvDecorationArrayStride:			return "SpvDecorationArrayStride";
		case SpvDecorationMatrixStride:			return "SpvDecorationMatrixStride";
		case SpvDecorationGLSLShared:			return "SpvDecorationGLSLShared";
		case SpvDecorationGLSLPacked:			return "SpvDecorationGLSLPacked";
		case SpvDecorationCPacked:				return "SpvDecorationCPacked";
		case SpvDecorationBuiltIn:				return "SpvDecorationBuiltIn";
		case SpvDecorationNoPerspective:		return "SpvDecorationNoPerspective";
		case SpvDecorationFlat:					return "SpvDecorationFlat";
		case SpvDecorationPatch:				return "SpvDecorationPatch";
		case SpvDecorationCentroid:				return "SpvDecorationCentroid";
		case SpvDecorationSample:				return "SpvDecorationSample";
		case SpvDecorationInvariant:			return "SpvDecorationInvariant";
		case SpvDecorationRestrict:				return "SpvDecorationRestrict";
		case SpvDecorationAliased:				return "SpvDecorationAliased";
		case SpvDecorationVolatile:				return "SpvDecorationVolatile";
		case SpvDecorationConstant:				return "SpvDecorationConstant";
		case SpvDecorationCoherent:				return "SpvDecorationCoherent";
		case SpvDecorationNonWritable:			return "SpvDecorationNonWritable";
		case SpvDecorationNonReadable:			return "SpvDecorationNonReadable";
		case SpvDecorationUniform:				return "SpvDecorationUniform";
		case SpvDecorationUniformId:			return "SpvDecorationUniformId";
		case SpvDecorationSaturatedConversion:	return "SpvDecorationSaturatedConversion";
		case SpvDecorationStream:				return "SpvDecorationStream";
		case SpvDecorationLocation:				return "SpvDecorationLocation";
		case SpvDecorationComponent:			return "SpvDecorationComponent";
		case SpvDecorationIndex:				return "SpvDecorationIndex";
		case SpvDecorationBinding:				return "SpvDecorationBinding";
		case SpvDecorationDescriptorSet:		return "SpvDecorationDescriptorSet";
		case SpvDecorationOffset:				return "SpvDecorationOffset";
		case SpvDecorationXfbBuffer:			return "SpvDecorationXfbBuffer";
		case SpvDecorationXfbStride:			return "SpvDecorationXfbStride";
		case SpvDecorationFuncParamAttr:		return "SpvDecorationFuncParamAttr";
		case SpvDecorationFPRoundingMode:		return "SpvDecorationFPRoundingMode";
		case SpvDecorationFPFastMathMode:		return "SpvDecorationFPFastMathMode";
		case SpvDecorationLinkageAttributes:	return "SpvDecorationLinkageAttributes";
		case SpvDecorationNoContraction:		return "SpvDecorationNoContraction";
		case SpvDecorationInputAttachmentIndex:	return "SpvDecorationInputAttachmentIndex";
		case SpvDecorationAlignment:			return "SpvDecorationAlignment";
		case SpvDecorationMaxByteOffset:		return "SpvDecorationMaxByteOffset";
		case SpvDecorationAlignmentId:			return "SpvDecorationAlignmentId";
		case SpvDecorationMaxByteOffsetId:		return "SpvDecorationMaxByteOffsetId";
		default:								return "SpvDecorationUnknown";
	}
}

static inline const char *printSpvExecutionModel(const SpvExecutionModel executionModel)
{
	switch(executionModel)
	{
		case SpvExecutionModelVertex:					return "SpvExecutionModelVertex";
		case SpvExecutionModelTessellationControl:		return "SpvExecutionModelTessellationControl";
		case SpvExecutionModelTessellationEvaluation:	return "SpvExecutionModelTessellationEvaluation";
		case SpvExecutionModelGeometry:					return "SpvExecutionModelGeometry";
		case SpvExecutionModelFragment:					return "SpvExecutionModelFragment";
		case SpvExecutionModelGLCompute:				return "SpvExecutionModelGLCompute";
		case SpvExecutionModelKernel:					return "SpvExecutionModelKernel";
		default:										return "SpvExecutionModelUnknown";
	}
}

static inline const char *printSpvExecutionMode(const SpvExecutionMode executionMode)
{
	switch(executionMode)
	{
		case SpvExecutionModeInvocations:				return "SpvExecutionModeInvocations";
		case SpvExecutionModeSpacingEqual:				return "SpvExecutionModeSpacingEqual";
		case SpvExecutionModeSpacingFractionalEven:		return "SpvExecutionModeSpacingFractionalEven";
		case SpvExecutionModeSpacingFractionalOdd:		return "SpvExecutionModeSpacingFractionalOdd";
		case SpvExecutionModeVertexOrderCw:				return "SpvExecutionModeVertexOrderCw";
		case SpvExecutionModeVertexOrderCcw:			return "SpvExecutionModeVertexOrderCcw";
		case SpvExecutionModePixelCenterInteger:		return "SpvExecutionModePixelCenterInteger";
		case SpvExecutionModeOriginUpperLeft:			return "SpvExecutionModeOriginUpperLeft";
		case SpvExecutionModeOriginLowerLeft:			return "SpvExecutionModeOriginLowerLeft";
		case SpvExecutionModeEarlyFragmentTests:		return "SpvExecutionModeEarlyFragmentTests";
		case SpvExecutionModePointMode:					return "SpvExecutionModePointMode";
		case SpvExecutionModeXfb:						return "SpvExecutionModeXfb";
		case SpvExecutionModeDepthReplacing:			return "SpvExecutionModeDepthReplacing";
		case SpvExecutionModeDepthGreater:				return "SpvExecutionModeDepthGreater";
		case SpvExecutionModeDepthLess:					return "SpvExecutionModeDepthLess";
		case SpvExecutionModeDepthUnchanged:			return "SpvExecutionModeDepthUnchanged";
		case SpvExecutionModeLocalSize:					return "SpvExecutionModeLocalSize";
		case SpvExecutionModeLocalSizeHint:				return "SpvExecutionModeLocalSizeHint";
		case SpvExecutionModeInputPoints:				return "SpvExecutionModeInputPoints";
		case SpvExecutionModeInputLines:				return "SpvExecutionModeInputLines";
		case SpvExecutionModeInputLinesAdjacency:		return "SpvExecutionModeInputLinesAdjacency";
		case SpvExecutionModeTriangles:					return "SpvExecutionModeTriangles";
		case SpvExecutionModeInputTrianglesAdjacency:	return "SpvExecutionModeInputTrianglesAdjacency";
		case SpvExecutionModeQuads:						return "SpvExecutionModeQuads";
		case SpvExecutionModeIsolines:					return "SpvExecutionModeIsolines";
		case SpvExecutionModeOutputVertices:			return "SpvExecutionModeOutputVertices";
		case SpvExecutionModeOutputPoints:				return "SpvExecutionModeOutputPoints";
		case SpvExecutionModeOutputLineStrip:			return "SpvExecutionModeOutputLineStrip";
		case SpvExecutionModeOutputTriangleStrip:		return "SpvExecutionModeOutputTriangleStrip";
		case SpvExecutionModeVecTypeHint:				return "SpvExecutionModeVecTypeHint";
		case SpvExecutionModeContractionOff:			return "SpvExecutionModeContractionOff";
		case SpvExecutionModeInitializer:				return "SpvExecutionModeInitializer";
		case SpvExecutionModeFinalizer:					return "SpvExecutionModeFinalizer";
		case SpvExecutionModeSubgroupSize:				return "SpvExecutionModeSubgroupSize";
		case SpvExecutionModeSubgroupsPerWorkgroup:		return "SpvExecutionModeSubgroupsPerWorkgroup";
		case SpvExecutionModeSubgroupsPerWorkgroupId:	return "SpvExecutionModeSubgroupsPerWorkgroupId";
		case SpvExecutionModeLocalSizeId:				return "SpvExecutionModeLocalSizeId";
		case SpvExecutionModeLocalSizeHintId:			return "SpvExecutionModeLocalSizeHintId";
		default:										return "SpvExecutionModeUnknown";
	}
}

static inline const char *printSpvStorageClass(const SpvStorageClass storageClass)
{
	switch(storageClass)
	{
		case SpvStorageClassUniformConstant:	return "SpvStorageClassUniformConstant";
		case SpvStorageClassInput:				return "SpvStorageClassInput";
		case SpvStorageClassUniform:			return "SpvStorageClassUniform";
		case SpvStorageClassOutput:				return "SpvStorageClassOutput";
		case SpvStorageClassWorkgroup:			return "SpvStorageClassWorkgroup";
		case SpvStorageClassCrossWorkgroup:		return "SpvStorageClassCrossWorkgroup";
		case SpvStorageClassPrivate:			return "SpvStorageClassPrivate";
		case SpvStorageClassFunction:			return "SpvStorageClassFunction";
		case SpvStorageClassGeneric:			return "SpvStorageClassGeneric";
		case SpvStorageClassPushConstant:		return "SpvStorageClassPushConstant";
		case SpvStorageClassAtomicCounter:		return "SpvStorageClassAtomicCounter";
		case SpvStorageClassImage:				return "SpvStorageClassImage";
		case SpvStorageClassStorageBuffer:		return "SpvStorageClassStorageBuffer";
		default:								return "SpvStorageClassUnknown";
	}
}

static void printSpvHeader(const SpvHeader_t *header)
{
	DBGPRINTF(DEBUG_INFO, "SPIR-V Header:\n");
	DBGPRINTF(DEBUG_INFO, "\tMagic Number: 0x%x\n", header->magicNumber);
	DBGPRINTF(DEBUG_INFO, "\tVersion: %d.%d\n", SPV_SPIRV_VERSION_MAJOR_PART(header->version), SPV_SPIRV_VERSION_MINOR_PART(header->version));
	DBGPRINTF(DEBUG_INFO, "\tGenerator: 0x%x\n", header->generator);
	DBGPRINTF(DEBUG_INFO, "\tBound: %d\n", header->bound);
	DBGPRINTF(DEBUG_INFO, "\tSchema: %d\n", header->schema);
}

static uint32_t spvTypeSize(const SpvID_t *type, const SpvID_t *IDs)
{
	switch(type->opCode)
	{
		case SpvOpTypeBool:
			return 4;

		case SpvOpTypeInt:
			return type->type.intWidth/8;

		case SpvOpTypeFloat:
			return type->type.floatWidth/8;

		case SpvOpTypeVector:
		{
			const SpvID_t *comp=&IDs[type->type.vectorComponentTypeID];
			return type->type.vectorComponentCount*spvTypeSize(comp, IDs);
		}

		case SpvOpTypeMatrix:
		{
			const SpvID_t *col=&IDs[type->type.matrixColumnTypeID];
			return type->type.matrixColumnCount*spvTypeSize(col, IDs);
		}

		case SpvOpTypeArray:
		{
			const SpvID_t *elem=&IDs[type->type.arrayElementTypeID];
			const SpvID_t *lenID=&IDs[type->type.arrayLengthID];
			uint32_t len=(lenID->opCode==SpvOpConstant)?(uint32_t)lenID->constant.literals[0].iLiteral:1;

			return len*spvTypeSize(elem, IDs);
		}

		case SpvOpTypeStruct:
		{
			uint32_t size=0;

			for(uint32_t i=0;i<type->type.structMemberCount;i++)
			{
				const SpvID_t *memberType=&IDs[type->type.structMemberTypeIDs[i]];
				uint32_t memberSize=spvTypeSize(memberType, IDs);
				uint32_t offset=0;

				for(uint32_t j=0;j<type->memberDecoration.member[i].decorationCount;j++)
				{
					if(type->memberDecoration.member[i].decorations[j].decoration==SpvDecorationOffset)
					{
						offset=type->memberDecoration.member[i].decorations[j].decorationValue;
						break;
					}
				}

				if(offset+memberSize>size)
					size=offset+memberSize;
			}

			return size;
		}

		default:
			return 0;
	}
}

static uint32_t spvFillMembers(const SpvID_t *type, const SpvID_t *IDs, SpvStructMember_t *members, uint32_t maxMembers)
{
	if(type->opCode!=SpvOpTypeStruct)
		return 0;

	uint32_t count=0;

	for(uint32_t i=0;i<type->type.structMemberCount&&count<maxMembers;i++)
	{
		SpvStructMember_t *m=&members[count];
		const SpvID_t *memberType=&IDs[type->type.structMemberTypeIDs[i]];

		strncpy(m->name, type->memberDecoration.member[i].name, SPV_REFLECT_MAX_NAME-1);
		m->name[SPV_REFLECT_MAX_NAME-1]='\0';

		m->typeOp=memberType->opCode;
		m->sizeBytes=spvTypeSize(memberType, IDs);
		m->offset=0;
		m->arrayCount=0;

		// Find SpvDecorationOffset
		for(uint32_t j=0;j<type->memberDecoration.member[i].decorationCount;j++)
		{
			if(type->memberDecoration.member[i].decorations[j].decoration==SpvDecorationOffset)
			{
				m->offset=type->memberDecoration.member[i].decorations[j].decorationValue;
				break;
			}
		}

		// Handle arrays
		if(memberType->opCode==SpvOpTypeArray)
		{
			const SpvID_t *lenID=&IDs[memberType->type.arrayLengthID];

			if(lenID->opCode==SpvOpConstant)
				m->arrayCount=(uint32_t)lenID->constant.literals[0].iLiteral;
		}

		count++;
	}

	return count;
}

bool parseSpv(const uint32_t *opCodes, uint32_t codeSize, SpvReflectionInfo_t *reflOut)
{
	if(opCodes==NULL)
		return false;

	uint32_t offset=5;
	const uint32_t codeEnd=codeSize/sizeof(uint32_t);

	if(opCodes[0]!=SpvMagicNumber)
	{
		DBGPRINTF(DEBUG_INFO, "SPIR-V magic number not found.\n");
		return false;
	}

	const SpvHeader_t *header=(const SpvHeader_t *)opCodes;

	// printSpvHeader(header);

	SpvID_t *IDs=(SpvID_t *)Zone_Malloc(zone, sizeof(SpvID_t)*header->bound);

	if(IDs==NULL)
		return false;

	memset(IDs, 0, sizeof(SpvID_t)*header->bound);

	while(offset<codeEnd)
	{
		uint16_t wordCount=(uint16_t)(opCodes[offset]>>16);
		SpvOp opCode=(SpvOp)(opCodes[offset]&0xFFFFu);

		switch(opCode)
		{
			case SpvOpName:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpName word count too small.\n");
					return false;
				}

				uint32_t targetID=opCodes[offset+1];
				const char *str=(const char *)&opCodes[offset+2];

				strncpy(IDs[targetID].name, str, SPV_REFLECT_MAX_NAME-1);
				IDs[targetID].name[SPV_REFLECT_MAX_NAME-1]='\0';
				break;
			}

			case SpvOpMemberName:
		    {
			    if(wordCount<4)
			    {
				    DBGPRINTF(DEBUG_ERROR, "SpvOpMemberName word count too small.\n");
				    return false;
			    }

				uint32_t typeID=opCodes[offset+1];
			    uint32_t memberIdx=opCodes[offset+2];
			    const char *str=(const char *)&opCodes[offset+3];

			    if(memberIdx<SPV_MAX_MEMBERS)
			    {
				    strncpy(IDs[typeID].memberDecoration.member[memberIdx].name, str, SPV_REFLECT_MAX_NAME-1);
				    IDs[typeID].memberDecoration.member[memberIdx].name[SPV_REFLECT_MAX_NAME-1]='\0';
			    }
			    break;
		    }

		    // case SpvOpEntryPoint:
			// {
			// 	if(wordCount<4)
			// 	{
			// 		DBGPRINTF(DEBUG_ERROR, "SpvOpEntryPoint word count too small.\n");
			// 		return false;
			// 	}

			// 	SpvExecutionModel executionModel=(SpvExecutionModel)opCodes[offset+1];
			// 	uint32_t entryPointID=opCodes[offset+2];
			// 	const char *entryPointName=(const char *)&opCodes[offset+3];

			// 	DBGPRINTF(DEBUG_INFO, "SpvOpEntryPoint:\n\tExecution model: %s\n\tEntry point ID: %u\n\tEntry point name: %s\n", printSpvExecutionModel(executionModel), entryPointID, entryPointName);
			// 	break;
			// }

			// case SpvOpExecutionMode:
			// {
			// 	if(wordCount<3)
			// 	{
			// 		DBGPRINTF(DEBUG_ERROR, "SpvOpExecutionMode word count too small.\n");
			// 		return false;
			// 	}

			// 	const uint32_t entryPointID=opCodes[offset+1];
			// 	const SpvExecutionMode executionMode=(SpvExecutionMode)opCodes[offset+2];

			// 	DBGPRINTF(DEBUG_INFO, "SpvOpExecutionMode:\n\tEntry point ID: %u\n\tExecution mode: %s\n", entryPointID, printSpvExecutionMode(executionMode));

			// 	switch(executionMode)
			// 	{
			// 		case SpvExecutionModeLocalSize:
			// 			// Local sizes for compute shaders:
			// 			// localSizeX=opCodes[offset+3];
			// 			// localSizeY=opCodes[offset+4];
			// 			// localSizeZ=opCodes[offset+5];
			// 			break;
			// 	}
			// 	break;
			// }

			case SpvOpDecorate:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpDecorate word count too small.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].decoration.targetID=targetID;
				const uint32_t decorationCount=IDs[targetID].decoration.decorationCount;

				if(decorationCount>SPV_MAX_MEMBER_DECORATIONS)
				{
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more decorations than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBER_DECORATIONS);
					break;
				}

				IDs[targetID].decoration.decorations[decorationCount].decoration=(SpvDecoration)opCodes[offset+2];

				if(wordCount>3)
					IDs[targetID].decoration.decorations[decorationCount].decorationValue=opCodes[offset+3];

				IDs[targetID].decoration.decorationCount=decorationCount+1;
				break;
			}

			case SpvOpMemberDecorate:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpMemberDecorate word count too small.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				const uint32_t member=opCodes[offset+2];
				const uint32_t decorationCount=IDs[targetID].memberDecoration.member[member].decorationCount;

				if(decorationCount>SPV_MAX_MEMBER_DECORATIONS)
				{
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more decorations than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBER_DECORATIONS);
					break;
				}

				IDs[targetID].memberDecoration.structTypeID=targetID;
				IDs[targetID].memberDecoration.member[member].decorations[decorationCount].decoration=(SpvDecoration)opCodes[offset+3];

				if(wordCount>4)
					IDs[targetID].memberDecoration.member[member].decorations[decorationCount].decorationValue=(SpvDecoration)opCodes[offset+4];

				IDs[targetID].memberDecoration.member[member].decorationCount=decorationCount+1;
				break;
			}

			case SpvOpTypeVoid:
			case SpvOpTypeBool:
			case SpvOpTypeSampler:
			case SpvOpTypeOpaque:
			case SpvOpTypeFunction:
			case SpvOpTypeEvent:
			case SpvOpTypeDeviceEvent:
			case SpvOpTypeReserveId:
			case SpvOpTypeQueue:
			case SpvOpTypePipe:
			case SpvOpTypeForwardPointer:
			{
				if(wordCount<2)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				break;
			}

			case SpvOpTypeInt:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.intWidth=opCodes[offset+2];
				IDs[targetID].type.intSignedness=opCodes[offset+3];
				break;
			}

			case SpvOpTypeFloat:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.floatWidth=opCodes[offset+2];
				break;
			}

			case SpvOpTypeVector:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.vectorComponentTypeID=opCodes[offset+2];
				IDs[targetID].type.vectorComponentCount=opCodes[offset+3];
				break;
			}

			case SpvOpTypeMatrix:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.matrixColumnTypeID=opCodes[offset+2];
				IDs[targetID].type.matrixColumnCount=opCodes[offset+3];
				break;
			}

			case SpvOpTypeImage:
			{
				if(wordCount<9)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.imageSampledTypeID=opCodes[offset+2];
				IDs[targetID].type.imageDim=(SpvDim)opCodes[offset+3];
				IDs[targetID].type.imageDepth=opCodes[offset+4];
				IDs[targetID].type.imageMultisample=opCodes[offset+5];
				IDs[targetID].type.imageSampled=opCodes[offset+6];
				IDs[targetID].type.imageFormat=(SpvImageFormat)opCodes[offset+7];

				if(wordCount>9)
					IDs[targetID].type.imageAccessQualifier=(SpvAccessQualifier)opCodes[offset+8];
				break;
			}

			case SpvOpTypeSampledImage:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.sampledImageTypeID=opCodes[offset+2];
				break;
			}

			case SpvOpTypeArray:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.arrayElementTypeID=opCodes[offset+2];
				IDs[targetID].type.arrayLengthID=opCodes[offset+3];
				break;
			}

			case SpvOpTypeRuntimeArray:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.runtimeArrayElementTypeID=opCodes[offset+2];
				break;
			}


			case SpvOpTypeStruct:
			{
				if(wordCount<2)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				const uint32_t memberCount=wordCount-2;

				if(memberCount>SPV_MAX_MEMBERS)
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more members than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBERS);

				IDs[targetID].type.structMemberCount=min(SPV_MAX_MEMBERS, memberCount);

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;

				for(uint32_t i=0;i<IDs[targetID].type.structMemberCount;i++)
					IDs[targetID].type.structMemberTypeIDs[i]=opCodes[offset+i+2];
				break;
			}

			case SpvOpTypePointer:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.pointerStorageClass=(SpvStorageClass)opCodes[offset+2];
				IDs[targetID].type.pointerTypeID=opCodes[offset+3];

				break;
			}

			case SpvOpConstantTrue:
			case SpvOpConstantFalse:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].constantBool.resultTypeID=opCodes[offset+1];
				IDs[targetID].constantBool.resultID=opCodes[offset+1];

				if(opCode==SpvOpConstantTrue)
					IDs[targetID].constantBool.value=true;
				else
					IDs[targetID].constantBool.value=false;

				break;
			}

			case SpvOpConstant:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].constant.resultTypeID=opCodes[offset+1];
				IDs[targetID].constant.resultID=targetID;

				const uint32_t literalCount=wordCount-3;

				if(literalCount>SPV_MAX_MEMBERS)
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more literals than code supports (%d), no more will be added.\n", targetID, SPV_MAX_LITERALS);

				IDs[targetID].constant.literalCount=min(SPV_MAX_LITERALS, literalCount);

				for(uint32_t i=0;i<IDs[targetID].constant.literalCount;i++)
					IDs[targetID].constant.literals[i].iLiteral=opCodes[offset+i+3];
				break;
			}

			case SpvOpVariable:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].variable.resultTypeID=opCodes[offset+1];
				IDs[targetID].variable.resultID=targetID;
				IDs[targetID].variable.storageClass=(SpvStorageClass)opCodes[offset+3];

				if(wordCount==5)
					IDs[targetID].variable.initializerID=opCodes[offset+4];

			    // Reflection capture
			    if(reflOut&&IDs[targetID].variable.storageClass!=SpvStorageClassInput&&IDs[targetID].variable.storageClass!=SpvStorageClassOutput)
			    {
				    uint32_t set=0, binding=0;

					for(uint32_t j=0;j<IDs[targetID].decoration.decorationCount;j++)
				    {
					    switch(IDs[targetID].decoration.decorations[j].decoration)
					    {
							case SpvDecorationDescriptorSet:
								set=IDs[targetID].decoration.decorations[j].decorationValue;
								break;

							case SpvDecorationBinding:
								binding=IDs[targetID].decoration.decorations[j].decorationValue;
								break;

							default:
								break;
					    }
				    }

				    SpvID_t *ptrType=&IDs[IDs[targetID].variable.resultTypeID];
				    SpvID_t *baseType=&IDs[ptrType->type.pointerTypeID];

				    SpvResourceBinding_t *res=&reflOut->bindings[reflOut->numBindings++];

					if(reflOut->numBindings>=SPV_REFLECT_MAX_BINDINGS)
					{
						DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more bindings than code supports (%d), no more will be added.\n", targetID, SPV_REFLECT_MAX_BINDINGS);
						break;
					}

				    res->set=set;
				    res->binding=binding;
					res->typeOp=baseType->opCode;

				    res->sizeBytes=spvTypeSize(baseType, IDs);

				    if(baseType->opCode==SpvOpTypeStruct)
					    res->memberCount=spvFillMembers(baseType, IDs, res->members, SPV_REFLECT_MAX_MEMBERS);

				    switch(IDs[targetID].variable.storageClass)
				    {
						case SpvStorageClassUniform:
							res->type=SPV_RESOURCE_UNIFORM_BUFFER;
							break;

						case SpvStorageClassStorageBuffer:
							res->type=SPV_RESOURCE_STORAGE_BUFFER;
							break;

						case SpvStorageClassUniformConstant:
							if(baseType->opCode==SpvOpTypeImage)
								res->type = SPV_RESOURCE_SAMPLED_IMAGE;
							else if(baseType->opCode==SpvOpTypeSampler)
								res->type=SPV_RESOURCE_SAMPLER;
							else
								res->type=SPV_RESOURCE_UNIFORM_BUFFER;
							break;

						case SpvStorageClassPushConstant:
							res->type=SPV_RESOURCE_PUSH_CONSTANT;
							break;

						default:
							break;
				    }
			    }

				break;
		    }
		}

		offset+=wordCount;
	}

	Zone_Free(zone, IDs);

	return true;
}

static const char *spv_reflect_type_string(SpvOp typeOp, uint32_t sizeBytes)
{
	switch (typeOp)
	{
		case SpvOpTypeBool:          return "bool";
		case SpvOpTypeInt:
			switch (sizeBytes)
			{
				case 1:  return "int8";  case 2:  return "int16";
				case 4:  return "int32"; case 8:  return "int64";
				default: return "int";
			}
		case SpvOpTypeFloat:
			switch (sizeBytes)
			{
				case 2: return "half"; case 4: return "float";
				case 8: return "double";
				default: return "float";
			}
		case SpvOpTypeVector:        return "vecN";
		case SpvOpTypeMatrix:        return "matN";
		case SpvOpTypeStruct:        return "struct";
		case SpvOpTypeSampler:       return "sampler";
		case SpvOpTypeImage:         return "image";
		case SpvOpTypeSampledImage:  return "sampler2D";
		case SpvOpTypeArray:         return "array";
		case SpvOpTypeRuntimeArray:  return "runtime_array";
		default:                     return "(unknown)";
	}
}

// More refined for vectors/matrices if we can infer from sizeBytes
static const char *spv_reflect_guess_compound_type(SpvOp op, uint32_t sizeBytes)
{
	static char buf[32];

	switch (op)
	{
		case SpvOpTypeVector:
			// 4 bytes per component, guess vec2/3/4
			switch (sizeBytes)
			{
				case 8:  return "vec2";
				case 12: return "vec3";
				case 16: return "vec4";
				default: snprintf(buf, sizeof(buf), "vec? (%uB)", sizeBytes); return buf;
			}
		case SpvOpTypeMatrix:
			// 16 bytes per column, guess mat2/3/4
			switch (sizeBytes)
			{
				case 32: return "mat2";
				case 48: return "mat3";
				case 64: return "mat4";
				default: snprintf(buf, sizeof(buf), "mat? (%uB)", sizeBytes); return buf;
			}
		default:
			return spv_reflect_type_string(op, sizeBytes);
	}
}

void spvReflectDump(const SpvReflectionInfo_t *refl)
{
	if (!refl)
		return;

	DBGPRINTF(DEBUG_INFO, "=== SPIR-V Reflection Dump ===\n");
	DBGPRINTF(DEBUG_INFO, "Total bindings: %u\n", refl->numBindings);
	DBGPRINTF(DEBUG_INFO, "------------------------------------------\n");

	for (uint32_t i = 0; i < refl->numBindings; i++)
	{
		const SpvResourceBinding_t *b = &refl->bindings[i];
		const char *bname = (b->name[0]) ? b->name : "(unnamed)";

		const char *typeStr = "(unknown)";
		switch (b->type)
		{
			case SPV_RESOURCE_UNIFORM_BUFFER:  typeStr = "UniformBuffer"; break;
			case SPV_RESOURCE_STORAGE_BUFFER:  typeStr = "StorageBuffer"; break;
			case SPV_RESOURCE_SAMPLED_IMAGE:   typeStr = "SampledImage";  break;
			case SPV_RESOURCE_STORAGE_IMAGE:   typeStr = "StorageImage";  break;
			case SPV_RESOURCE_SAMPLER:         typeStr = "Sampler";       break;
			case SPV_RESOURCE_PUSH_CONSTANT:   typeStr = "PushConstant";  break;
			default: break;
		}

		DBGPRINTF(DEBUG_INFO, "Set %u, Binding %u — %s\n", b->set, b->binding, bname);
		DBGPRINTF(DEBUG_INFO, "\tType: %-15s  SPIR-V Base: %-16s\n", typeStr, spv_reflect_type_string(b->typeOp, b->sizeBytes));
		DBGPRINTF(DEBUG_INFO, "\tTotal Size: %u bytes\n", b->sizeBytes);

		if (b->memberCount > 0)
		{
			DBGPRINTF(DEBUG_INFO, "\tMembers (%u):\n", b->memberCount);
			for (uint32_t m = 0; m < b->memberCount; m++)
			{
				const SpvStructMember_t *mem = &b->members[m];
				const char *mname = (mem->name[0]) ? mem->name : "(unnamed)";

				const char *desc = spv_reflect_guess_compound_type(mem->typeOp, mem->sizeBytes);

				DBGPRINTF(DEBUG_INFO, "\t\t[%2u] %-16s offset=%-4u size=%-4u %-12s", m, mname, mem->offset, mem->sizeBytes, desc);

				if (mem->arrayCount > 0)
					DBGPRINTF(DEBUG_INFO, "\tarray[%u]", mem->arrayCount);

				DBGPRINTF(DEBUG_INFO, "\n");
			}
		}

		DBGPRINTF(DEBUG_INFO, "------------------------------------------\n");
	}

	DBGPRINTF(DEBUG_INFO, "=== End of Reflection ===\n");
}
