/*********************************************
*
* Copyright © 2025 VEGA Enterprises LTD,.
* Licensed under the MIT License.
*
**********************************************/

#pragma once

#ifndef SHADER_TRANSPILER_H
#define SHADER_TRANSPILER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "../OrcaAPI.h"

namespace Orca
{
	enum class ShaderTarget
	{
		GLSL,
		HLSL,
		Vulkan,
		Metal
	};

	enum class ShaderStage
	{
		Vertex,
		Fragment
	};

	struct UniformBinding
	{
		std::string name;
		std::string type;
		int binding;
		int set;
	};

	struct VertexAttribute
	{
		std::string name;
		std::string type;
		int location;
	};

	struct TranspilationResult
	{
		bool success;
		std::string output;
		std::vector<uint32_t> binary;
		std::string errorMessage;
	};

	class ORCA_API ShaderTranspiler
	{
	public:
		ShaderTranspiler() = default;
		~ShaderTranspiler() = default;

		// Transpile a shader from GLSL to target language
		TranspilationResult Transpile(const std::string& glslSource, ShaderTarget target, ShaderStage stage);

		// Transpile both vertex and fragment shaders
		TranspilationResult TranspileProgram(const std::string& vertexSource, const std::string& fragmentSource, ShaderTarget target);

		// Parse shader metadata
		std::vector<UniformBinding> ExtractUniforms(const std::string& glslSource);
		std::vector<VertexAttribute> ExtractAttributes(const std::string& glslSource);

		// Get the target language version string
		static std::string GetTargetVersionString(ShaderTarget target);

	private:
		// Transpilation methods for each target
		TranspilationResult TranspileToHLSL(const std::string& glslSource, ShaderStage stage);
		TranspilationResult TranspileToVulkan(const std::string& glslSource, ShaderStage stage);
		TranspilationResult TranspileToMetal(const std::string& glslSource, ShaderStage stage);

		// Helper methods
		std::string ReplaceGLSLBuiltins(const std::string& source, ShaderTarget target, ShaderStage stage);
		std::string ConvertUniformDeclarations(const std::string& source, ShaderTarget target);
		std::string ConvertAttributeDeclarations(const std::string& source, ShaderTarget target, ShaderStage stage);
		std::string ConvertVaryingDeclarations(const std::string& source, ShaderTarget target, ShaderStage stage);
		std::string ConvertMatrixOperations(const std::string& source, ShaderTarget target);
		std::string ConvertBuiltinFunctions(const std::string& source, ShaderTarget target);

		// Utility parsing helpers
		std::string ExtractIdentifier(const std::string& source, size_t& pos);
		std::vector<std::string> SplitLines(const std::string& source);
		bool IsBuiltinType(const std::string& type);
		bool IsUniformDeclaration(const std::string& line);
		bool IsAttributeDeclaration(const std::string& line);
	};
}

#endif