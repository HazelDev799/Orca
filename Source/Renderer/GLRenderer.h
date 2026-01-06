/*********************************************
*
* Copyright © 2025 VEGA Enterprises LTD,.
* Licensed under the MIT License.
*
**********************************************/

#pragma once

#include "Renderer.h"
#include <vector>
#include "Mesh.h"

#ifndef GL_RENDERER_H
#define GL_RENDERER_H

namespace Orca
{
	class CameraComponent;

	struct RenderCommand
	{
		const Mesh* mesh;
		Matrix4 transform;
	};

	class GLRenderer : public Renderer
	{
    public:
        GLRenderer() = default;
        virtual ~GLRenderer() override;

        // Implementation of the Renderer Interface
        void Initialize(void* windowHandle) override;
        void Shutdown() override;
        void BeginFrame() override;
        void Render() override;
        void EndFrame() override;

        void DrawMesh(const Mesh& mesh, const Shader& shader, const Matrix4& transform) override;

		void SetActiveCamera(CameraComponent* camera) {activeCamera = camera;}
		void DrawSkybox(const Shader& shader, unsigned int cubemapID);
		bool CompileAndLinkShaders();

        // Shader transpilation methods
        TranspilationResult TranspileShader(const std::string& shaderPath, ShaderTarget target, ShaderStage stage);
        TranspilationResult TranspileProgram(const std::string& vertPath, const std::string& fragPath, ShaderTarget target);

        // GL Specifics
        void SubmitMesh(const Mesh& mesh);

    private:
        bool isInitialized = false;
        unsigned int programID = 0;
        std::vector<RenderCommand> renderQuene;
		CameraComponent* activeCamera = nullptr;
		std::unique_ptr<ShaderTranspiler> m_transpiler

		std::string ReadShaderFiles(const std::string& path);
		std::string GetShaderPath(const std::string& fileName);
		void SaveInternalShaderCache(const std::string& fileName, const std::string& content);
	};
}


#endif
