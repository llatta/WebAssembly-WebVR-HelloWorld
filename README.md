WebAssembly WebVR Hello World
===

This is sort of a Hello World program demonstrating driving WebVR from a WebAssembly program in a browser.
You should see a spinning triangle, the click/tap on the canvas, and VR mode should start.

Run a compiled version of this test at: http://2ld.de/webvrasm/

It has been tested on Firefox 56, Chrome with WebVR Emulation Extension, and the Oculus Browser on GearVR.

The code uses the WebVR 1.1 API with the C wrapper in emscripten 1.37.22. These APIs are still in flux, so future compatibility is uncertain.

# Compilation and Serving

- Follow the steps [here](http://webassembly.org/getting-started/developers-guide/) to get ready to compile WebAssembly
- Clone this directory wherever you want (`git clone https://github.com/llatta/WebAssembly-WebVR-HelloWorld.git`)
- `cd` to the root of the newly cloned directory
- Use the makefile to either build, clean up, or make a distribution version (build directory only) of the repo
    - Build: `make`
    - Clean: `make clean`
    - Build, but remove objects leaving the `build` dir: `make dist`
- You can then use `python -m SimpleHTTPServer 8080` and open your browser to `localhost:8080`.

# Acknowledgments

This sample is based on Harry Gould's WebAssembly-WebGL2 sample: https://github.com/HarryLovesCode/WebAssembly-WebGL-2

The usage of the emscripten WebVR 1.1 API is based on Jonathan Hale's emscripten contribution: https://github.com/kripken/emscripten/pull/5508
