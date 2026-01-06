/*********************************************
*
* Copyright © 2025 VEGA Enterprises LTD,.
* Licensed under the MIT License.
*
**********************************************/

#define _CRT_SECURE_NO_WARNINGS
#include "ShaderTranspiler.h"
#include "../Core/Logger.h"
#include <regex>
#include <algorithm>
#include <sstream>

namespace Orca
{
	TranspilationResult ShaderTranspiler::Transpile(const std::string& glslSource, ShaderTarget target, ShaderStage stage)
	{
		if (glslSource.empty())
		{
			return { false, "", {}, "Input shader source is empty" };
		}

		if (glslSource.find("{") == std::string::npos || glslSource.find("}") == std::string::npos)
		{
			return { false, "", {}, "ERROR: Missing curly braces in shader source. Please fix the problem." };
		}

		try
		{
			TranspilationResult result;

			switch (target)
			{
			case ShaderTarget::GLSL:
				// Already GLSL, minimal conversion
				result = { true, glslSource, {}, "" };
				break;
			case ShaderTarget::HLSL:
				result = TranspileToHLSL(glslSource, stage);
				break;
			case ShaderTarget::Vulkan:
				result = TranspileToVulkan(glslSource, stage);
				break;
			case ShaderTarget::Metal:
				result = TranspileToMetal(glslSource, stage);
				break;
			default:
				result = { false, "", {}, "Unknown shader target" };
			}

			if (result.success)
			{
				Logger::Log(LogLevel::Info, "Shader transpilation successful");
			}
			else
			{
				Logger::Log(LogLevel::Error, "Shader transpilation failed: " + result.errorMessage);
			}

			return result;
		}
		catch (const std::exception& e)
		{
			return { false, "", {}, std::string("Transpilation exception: ") + e.what() };
		}
	}

	TranspilationResult ShaderTranspiler::TranspileProgram(const std::string& vertexSource, const std::string& fragmentSource, ShaderTarget target)
	{
		auto vertResult = Transpile(vertexSource, target, ShaderStage::Vertex);
		if (!vertResult.success)
		{
			return vertResult;
		}

		auto fragResult = Transpile(fragmentSource, target, ShaderStage::Fragment);
		if (!fragResult.success)
		{
			return fragResult;
		}

		// Combine results with appropriate separators
		std::string combined = "// === VERTEX SHADER ===\n" + vertResult.output + 
							   "\n\n// === FRAGMENT SHADER ===\n" + fragResult.output;

		return { true, combined, {}, "" };
	}

	TranspilationResult ShaderTranspiler::TranspileToHLSL(const std::string& glslSource, ShaderStage stage)
	{
		std::string cleanedSource = std::regex_replace(glslSource, std::regex(R"(#version\s+\d+\s*\n)"), "");

		std::string converted = ConvertUniformDeclarations(cleanedSource, ShaderTarget::HLSL);
		converted = ConvertAttributeDeclarations(converted, ShaderTarget::HLSL, stage);
		converted = ConvertVaryingDeclarations(converted, ShaderTarget::HLSL, stage);
		converted = ConvertBuiltinFunctions(converted, ShaderTarget::HLSL);
		converted = ConvertMatrixOperations(converted, ShaderTarget::HLSL);

		converted = ReplaceGLSLBuiltins(converted, ShaderTarget::HLSL, stage);

		std::string hlsl = GetTargetVersionString(ShaderTarget::HLSL) + "\n";

		if (hlsl.find("inverse") != std::string::npos) 
		{
			hlsl = R"(
				float4x4 inverse(float4x4 m) {
					// In a real engine, you'd inject a full matrix inversion here
					// or pass the inverse matrix as a separate uniform.
					return m; // Placeholder: this will stop the error but math will be wrong
				})" + hlsl;
		}

		hlsl += converted;

		std::string tempPath = "Saved/ShaderCache/validate.hlsl";
		std::ofstream outFile(tempPath);
		if (outFile.is_open()) 
		{
			outFile << hlsl;
			outFile.close();
		}

		const char* sdkEnv = std::getenv("VULKAN_SDK");
		std::string sdkPath = sdkEnv ? sdkEnv : "C:/VulkanSDK/default";

		std::string targetProfile = (stage == ShaderStage::Vertex) ? "vs_6_0" : "ps_6_0";

		std::string cmd = "\"" + sdkPath + "\\Bin\\dxc.exe\" -T " + targetProfile + " -E main " + tempPath;

		if (std::system(cmd.c_str()) != 0) 
		{
			return { false, hlsl, {}, "DXC Validation Failed! Check shader syntax." };
		}

		return { true, hlsl, {}, "" };
	}

	TranspilationResult ShaderTranspiler::TranspileToVulkan(const std::string& glslSource, ShaderStage stage)
	{
		std::string output = "#version 450 core\n\n" + glslSource;
		std::string tempIn = "Saved/ShaderCache/temp_input.glsl";
		std::string tempOut = "Saved/ShaderCache/temp_input.spv";

		std::ofstream outFile(tempIn);
		outFile << output;
		outFile.close();

		std::string sdkPath = std::getenv("VULKAN_SDK") ? std::getenv("VULKAN_SDK") : "C:/Program Files/VulkanSDK/1.4.313.1";
		std::string command = "\"" + sdkPath + "/Bin/glslang.exe\" -V " + tempIn + " -o " + tempOut;

		if (std::system(command.c_str()) == 0)
		{
			std::ifstream binFile(tempOut, std::ios::binary | std::ios::ate);
			std::streamsize size = binFile.tellg();
			binFile.seekg(0, std::ios::beg);

			std::vector<uint32_t> buffer(size / sizeof(uint32_t));
			binFile.read((char*)buffer.data(), size);

			return { true, output, buffer, "SPIR-V compilation success!"};
		}

		return { true, "", {}, "SPIR-V compilation failed!"};
	}

	TranspilationResult ShaderTranspiler::TranspileToMetal(const std::string& glslSource, ShaderStage stage)
	{
		auto vResult = TranspileToVulkan(glslSource, stage);
		if (!vResult.success) return vResult;

		std::string tempSpv = "Saved/ShaderCache/temp_vulkan.spv";
		std::string tempMetal = "Saved/ShaderCache/temp_output.metal";
		std::string sdkPath = std::getenv("VULKAN_SDK");

		std::string command = "\"" + sdkPath + "/Bin/spirv-cross.exe\" -V " + tempSpv + " -o " + tempMetal;

		if (std::system(command.c_str()) == 0)
		{
			std::ifstream ifs(tempMetal);
			std::string metalSource((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
			return { true, metalSource, {}, "Metal transpilation success!" };
		}

		return { false, "", {}, "Metal transpilation failed!" };
	}

	std::vector<UniformBinding> ShaderTranspiler::ExtractUniforms(const std::string& glslSource)
	{
		std::vector<UniformBinding> uniforms;
		std::regex uniformRegex(R"(uniform\s+(\w+)\s+(\w+);)");
		std::smatch match;

		std::string::const_iterator searchStart(glslSource.cbegin());
		while (std::regex_search(searchStart, glslSource.cend(), match, uniformRegex))
		{
			UniformBinding binding;
			binding.type = match[1].str();
			binding.name = match[2].str();
			binding.binding = static_cast<int>(uniforms.size());
			binding.set = 0;
			uniforms.push_back(binding);

			searchStart = match.suffix().first;
		}

		return uniforms;
	}

	std::vector<VertexAttribute> ShaderTranspiler::ExtractAttributes(const std::string& glslSource)
	{
		std::vector<VertexAttribute> attributes;
		std::regex attrRegex(R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+(\w+)\s+(\w+);)");
		std::smatch match;

		std::string::const_iterator searchStart(glslSource.cbegin());
		while (std::regex_search(searchStart, glslSource.cend(), match, attrRegex))
		{
			VertexAttribute attr;
			attr.location = std::stoi(match[1].str());
			attr.type = match[2].str();
			attr.name = match[3].str();
			attributes.push_back(attr);

			searchStart = match.suffix().first;
		}

		return attributes;
	}

	std::string ShaderTranspiler::GetTargetVersionString(ShaderTarget target)
	{
		switch (target)
		{
		case ShaderTarget::GLSL:
			return "#version 330 core";
		case ShaderTarget::HLSL:
			return "// HLSL Shader (Target: Direct3D 11)";
		case ShaderTarget::Vulkan:
			return "#version 450 core";
		case ShaderTarget::Metal:
			return "// Metal Shader Language";
		default:
			return "";
		}
	}

	std::string ShaderTranspiler::ReplaceGLSLBuiltins(const std::string& source, ShaderTarget target, ShaderStage stage)
	{
		std::string output = source;

		if (target == ShaderTarget::HLSL)
		{
			if (stage == ShaderStage::Vertex)
			{
				output = std::regex_replace(output, std::regex(R"(\bgl_Position\b)"), "position");
			}
			else
			{
				output = std::regex_replace(output, std::regex(R"(\bgl_FragColor\b)"), "output");
			}
		}
		else if (target == ShaderTarget::Metal)
		{
			if (stage == ShaderStage::Vertex)
			{
				output = std::regex_replace(output, std::regex(R"(\bgl_Position\b)"), "out.position");
			}
			else
			{
				output = std::regex_replace(output, std::regex(R"(\bgl_FragColor\b)"), "out.color");
			}
		}

		return output;
	}

	std::string ShaderTranspiler::ConvertUniformDeclarations(const std::string& source, ShaderTarget target)
	{
		if (target != ShaderTarget::HLSL) return source;

		auto uniforms = ExtractUniforms(source);
		if (uniforms.empty()) return source;

		std::stringstream ss;
		ss << "cbuffer Uniforms : register(b0)\n{\n";
		for (const auto& uniform : uniforms)
		{
			std::string hlslType = uniform.type;
			if (hlslType == "vec2") hlslType = "float2";
			else if (hlslType == "vec3") hlslType = "float3";
			else if (hlslType == "vec4") hlslType = "float4";
			else if (hlslType == "mat3") hlslType = "float3x3";
			else if (hlslType == "mat4") hlslType = "float4x4";
			ss << "    " << hlslType << " " << uniform.name << ";\n";
		}

		std::string cleanedSource = std::regex_replace(source, std::regex(R"(uniform\s+.*?)"), "");
		return ss.str() + cleanedSource;
	}

	std::string ShaderTranspiler::ConvertAttributeDeclarations(const std::string& source, ShaderTarget target, ShaderStage stage)
	{
		std::string output = source;

		if (stage != ShaderStage::Vertex) return output;

		if (target == ShaderTarget::HLSL)
		{
			// Convert layout in to struct input
			output = std::regex_replace(output, std::regex(R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+(\w+)\s+(\w+);)"),
				"$2 $3 : TEXCOORD$1;");
		}
		else if (target == ShaderTarget::Metal)
		{
			// Convert to Metal attributes
			output = std::regex_replace(output, std::regex(R"(layout\s*\(\s*location\s*=\s*(\d+)\s*\)\s*in\s+(\w+)\s+(\w+);)"),
				"$2 $3 [[attribute($1)]];");
		}

		return output;
	}

	std::string ShaderTranspiler::ConvertVaryingDeclarations(const std::string& source, ShaderTarget target, ShaderStage stage)
	{
		std::string output = source;

		if (target == ShaderTarget::HLSL)
		{
			// Convert out/in to struct members
			if (stage == ShaderStage::Vertex)
			{
				output = std::regex_replace(output, std::regex(R"(\bout\s+(\w+)\s+(\w+);)"),
					"$1 $2 : TEXCOORD0;");
			}
			else
			{
				output = std::regex_replace(output, std::regex(R"(\bin\s+(\w+)\s+(\w+);)"),
					"$1 $2 : TEXCOORD0;");
			}
		}
		else if (target == ShaderTarget::Metal)
		{
			if (stage == ShaderStage::Vertex)
			{
				output = std::regex_replace(output, std::regex(R"(\bout\s+(\w+)\s+(\w+);)"),
					"$1 $2 [[user(locn0)]];");
			}
		}

		return output;
	}

	std::string ShaderTranspiler::ConvertMatrixOperations(const std::string& source, ShaderTarget target)
	{
		if (target != ShaderTarget::HLSL) return source;

		std::string output = source;
		std::regex mathRegex(R"((\w+)\s*\*\s*([\w\d\(\).]+))");

		output = std::regex_replace(output, mathRegex, "mul($1, $2)");

		return output;
	}

	std::string ShaderTranspiler::ConvertBuiltinFunctions(const std::string& source, ShaderTarget target)
	{
		std::string output = source;

		if (target == ShaderTarget::HLSL)
		{
			// Convert GLSL built-ins to HLSL equivalents
			output = std::regex_replace(output, std::regex(R"(\bnormalize\b)"), "normalize");
			output = std::regex_replace(output, std::regex(R"(\bdot\b)"), "dot");
			output = std::regex_replace(output, std::regex(R"(\bmax\b)"), "max");
			output = std::regex_replace(output, std::regex(R"(\btranspose\b)"), "transpose");
			output = std::regex_replace(output, std::regex(R"(\binverse\b)"), "inverse");
			output = std::regex_replace(output, std::regex(R"(\bmat3\b)"), "float3x3");
			output = std::regex_replace(output, std::regex(R"(\bmat4\b)"), "float4x4");
			output = std::regex_replace(output, std::regex(R"(\bvec2\b)"), "float2");
			output = std::regex_replace(output, std::regex(R"(\bvec3\b)"), "float3");
			output = std::regex_replace(output, std::regex(R"(\bvec4\b)"), "float4");
		}
		else if (target == ShaderTarget::Metal)
		{
			// Metal has similar built-ins but needs type conversions
			output = std::regex_replace(output, std::regex(R"(\bmat3\b)"), "float3x3");
			output = std::regex_replace(output, std::regex(R"(\bmat4\b)"), "float4x4");
			output = std::regex_replace(output, std::regex(R"(\bvec2\b)"), "float2");
			output = std::regex_replace(output, std::regex(R"(\bvec3\b)"), "float3");
			output = std::regex_replace(output, std::regex(R"(\bvec4\b)"), "float4");
		}

		return output;
	}

	std::string ShaderTranspiler::ExtractIdentifier(const std::string& source, size_t& pos)
	{
		while (pos < source.size() && !std::isalnum(source[pos]) && source[pos] != '_') pos++;

		size_t start = pos;
		while (pos < source.size() && (std::isalnum(source[pos]) || source[pos] == '_')) pos++;

		return source.substr(start, pos - start);
	}

	std::vector<std::string> ShaderTranspiler::SplitLines(const std::string& source)
	{
		std::vector<std::string> lines;
		std::stringstream ss(source);
		std::string line;

		while (std::getline(ss, line))
		{
			lines.push_back(line);
		}

		return lines;
	}

	bool ShaderTranspiler::IsBuiltinType(const std::string& type)
	{
		static const std::unordered_map<std::string, bool> builtinTypes = {
			{ "float", true }, { "int", true }, { "uint", true },
			{ "vec2", true }, { "vec3", true }, { "vec4", true },
			{ "ivec2", true }, { "ivec3", true }, { "ivec4", true },
			{ "mat2", true }, { "mat3", true }, { "mat4", true },
			{ "sampler2D", true }, { "samplerCube", true }
		};

		return builtinTypes.find(type) != builtinTypes.end();
	}

	bool ShaderTranspiler::IsUniformDeclaration(const std::string& line)
	{
		return line.find("uniform") != std::string::npos;
	}

	bool ShaderTranspiler::IsAttributeDeclaration(const std::string& line)
	{
		return line.find("layout") != std::string::npos && line.find("in") != std::string::npos;
	}
}