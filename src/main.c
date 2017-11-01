#include "linmath.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/vr.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>

// Forward declarations
static void nonVrLoop();
static void vrLoop();

VRDisplayHandle gDisplay = -1;
VREyeParameters gEyeLeft, gEyeRight;

GLuint vertex_buffer, vertex_shader, fragment_shader, program;
GLint mvp_location, vpos_location, vcol_location;

static const struct
{
	float x, y;
	float r, g, b;
} vertices[3] =
{
	{-0.6f, -0.4f, 1.f, 0.f, 0.f},
	{0.6f, -0.4f, 0.f, 1.f, 0.f},
	{0.f, 0.6f, 0.f, 0.f, 1.f}
};
static const char *vertex_shader_text =
	"#version 100\n"
	"uniform mat4 MVP;\n"
	"attribute lowp vec3 vCol;\n"
	"attribute lowp vec2 vPos;\n"
	"varying lowp vec3 i_color;\n"
	"void main()\n"
	"{\n"
	"    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
	"    i_color = vCol;\n"
	"}\n";
static const char *fragment_shader_text =
	"#version 100\n"
	"varying lowp vec3 i_color;\n"
	"void main()\n"
	"{\n"
	"    gl_FragColor = vec4(i_color, 1.0);\n"
	"}\n";

static int checkShaderCompiled(shader)
{
	GLint success = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (success == GL_FALSE)
	{
		GLint max_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_len);

		GLchar err_log[max_len];
		glGetShaderInfoLog(shader, max_len, &max_len, &err_log[0]);
		glDeleteShader(shader);

		fprintf(stderr, "Shader compilation failed: %s\n", err_log);
	}

	return success;
}

static int checkShaderProgramLinked(program)
{
	GLint success = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &success);

	if (success == GL_FALSE)
	{
		GLint max_len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_len);

		GLchar err_log[max_len];
		glGetProgramInfoLog(program, max_len, &max_len, &err_log[0]);

		fprintf(stderr, "Program linking failed: %s\n", err_log);
	}

	return success;
}

// Init GL context and resources
static void initGL()
{
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	attr.alpha = attr.depth = attr.stencil = attr.antialias = attr.preferLowPowerToHighPerformance = attr.failIfMajorPerformanceCaveat = 0;
	attr.preserveDrawingBuffer = 1; // Some platforms need this true to have the external monitor mirror the contents of the VR display
	attr.enableExtensionsByDefault = 1;
	attr.premultipliedAlpha = 0;
	attr.majorVersion = 1;
	attr.minorVersion = 0;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context(0, &attr);
	emscripten_webgl_make_context_current(ctx);

	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
	glCompileShader(vertex_shader);
	checkShaderCompiled(vertex_shader);

	fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
	glCompileShader(fragment_shader);
	checkShaderCompiled(fragment_shader);

	program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	checkShaderProgramLinked(program);

	mvp_location = glGetUniformLocation(program, "MVP");
	vpos_location = glGetAttribLocation(program, "vPos");
	vcol_location = glGetAttribLocation(program, "vCol");
	glEnableVertexAttribArray(vpos_location);
	glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
						  sizeof(float) * 5, (void *)0);
	glEnableVertexAttribArray(vcol_location);
	glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
						  sizeof(float) * 5, (void *)(sizeof(float) * 2));
}

// Render a single view. Called once or twice depending on VR being active
static void drawView(mat4x4 projection, mat4x4 camera)
{
	mat4x4 m, mv, mvp;
	mat4x4_translate(m, 0.0f, 0.0, -1.0f);
	mat4x4_rotate_Z(m, m, (float)emscripten_get_now() / 1000.0f);
	mat4x4_mul(mv, camera, m);
	mat4x4_mul(mvp, projection, mv);
	glUseProgram(program);
	glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat *)mvp);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

// When VR present request is complete, start VR rendering loop
static void requestPresentCallback(void *userData)
{
	if (emscripten_vr_display_presenting(gDisplay))
	{
		emscripten_vr_get_eye_parameters(gDisplay, VREyeLeft, &gEyeLeft);
		emscripten_vr_get_eye_parameters(gDisplay, VREyeRight, &gEyeRight);

		emscripten_set_canvas_element_size("#canvas", gEyeLeft.renderWidth + gEyeRight.renderWidth, gEyeLeft.renderHeight);

		printf("Set canvas size to %lux%lu\n", gEyeLeft.renderWidth + gEyeRight.renderWidth, gEyeLeft.renderHeight);

		if (!emscripten_vr_set_display_render_loop(gDisplay, vrLoop))
		{
			printf("Error: Failed to dereference display while settings display render loop of device %d\n", gDisplay);
		}
	}
}

// Click callback, used to requesting entering/exiting VR mode
static EM_BOOL clickCallback(int eventType, const EmscriptenMouseEvent *e, void *userData)
{
	if (!e || eventType != EMSCRIPTEN_EVENT_CLICK)
		return EM_FALSE;

	if (emscripten_vr_display_presenting(gDisplay))
	{
		// Stop presenting, and revert to non-VR loop calls
		emscripten_vr_exit_present(gDisplay);
		emscripten_vr_cancel_display_render_loop(gDisplay);
		emscripten_resume_main_loop();
	}
	else
	{
		// Start presenting, stop our usual non-VR main loop calls
		VRLayerInit init =
		{
			NULL, // Use default #canvas
			VR_LAYER_DEFAULT_LEFT_BOUNDS,
			VR_LAYER_DEFAULT_RIGHT_BOUNDS
		};
		if (!emscripten_vr_request_present(gDisplay, &init, 1, requestPresentCallback, NULL))
		{
			printf("Request present with default canvas failed.\n");
			return EM_FALSE;
		}
		emscripten_pause_main_loop();
	}

	return EM_FALSE;
}

// Regularly called render function while VR is NOT active
static void nonVrLoop()
{
	// Check if VR system has come online and we can look for a device
	if (gDisplay == -1 && emscripten_vr_ready())
	{
		int numDisplays = emscripten_vr_count_displays();
		if (numDisplays > 0)
		{
			printf("%d VR displays found\n", numDisplays);

			for (int i = 0; i < numDisplays; ++i)
			{
				VRDisplayHandle display = emscripten_vr_get_display_handle(i);

				VRDisplayCapabilities caps;
				if (!emscripten_vr_get_display_capabilities(display, &caps))
				{
					fprintf(stderr, "Error: failed to get display capabilities.\n");
					continue;
				}

				if (caps.canPresent) // ... add more checks if needed
				{
					// If we like those caps, use the device
					gDisplay = display;
					char* devName = emscripten_vr_get_display_name(display);
					printf("Using VRDisplay '%s' (displayId '%d')\n", devName, display);

					printf("Display Capabilities:\n"
						"{hasPosition: %d, hasExternalDisplay: %d, canPresent: %d, maxLayers: %lu}\n",
						caps.hasPosition, caps.hasExternalDisplay, caps.canPresent, caps.maxLayers);

					// Set callback for getting present permission for the VR display.
					// Must be a reaction to a user action. We'll use click on the canvas.
					emscripten_set_click_callback("#canvas", 0, EM_TRUE, clickCallback);
				}
			}
		}
	}

	// Draw single view in non-VR mode
	float ratio;
	int width, height;
	emscripten_get_canvas_element_size("#canvas", &width, &height);
	ratio = width / (float)height;
	glViewport(0, 0, width, height);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	mat4x4 c, p;
	mat4x4_identity(c);
	mat4x4_perspective(p, 1.6f, ratio, 0.01f, 100.0f);

	drawView(p, c);
}

// Regularly called render function while VR is active
static void vrLoop()
{
	if (!emscripten_vr_display_presenting(gDisplay))
	{
		emscripten_vr_cancel_display_render_loop(gDisplay);
		emscripten_resume_main_loop();
		return;
	}

	VRFrameData data;
	if (!emscripten_vr_get_frame_data(gDisplay, &data))
	{
		printf("Could not get frame data.\n");
		return;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, gEyeLeft.renderWidth, gEyeLeft.renderHeight);
	drawView(*(mat4x4 *)&data.leftProjectionMatrix, *(mat4x4 *)&data.leftViewMatrix);

	glViewport(gEyeLeft.renderWidth, 0, gEyeRight.renderWidth, gEyeRight.renderHeight);
	drawView(*(mat4x4 *)&data.rightProjectionMatrix, *(mat4x4 *)&data.rightViewMatrix);

	if (!emscripten_vr_submit_frame(gDisplay))
	{
		printf("Error: Failed to submit frame to VR display %d (second iteration)\n", gDisplay);
	}
}

int main()
{
	// Start GL
	initGL();

	// Start VR system
	if (!emscripten_vr_init())
	{
		fprintf(stderr, "Browser does not support WebVR\n");
	}
	else
	{
		printf("Browser is running WebVR version %d.%d\n",
			emscripten_vr_version_major(),
			emscripten_vr_version_minor());
	}

	emscripten_set_main_loop(nonVrLoop, 0, 0);

	return 0;
}
