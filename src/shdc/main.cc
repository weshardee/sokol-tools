/*
    sokol-shdc main source file.

    TODO:

    - [DONE] "@lib name" optional, as prefix for all generated structs and variables
    - no redundant uniform block structs and bind slot variables per
      module (allows to share bind slots and structs across shaders)
    - rename vertex-output and fragment-inputs to their location and
      ignore the names
    - "@include file" simple include mechanism, no header search paths,
      instead paths are relative to current module file.
*/
#include "shdc.h"

using namespace shdc;

int main(int argc, const char** argv) {

    // parse command line args
    args_t args = args_t::parse(argc, argv);
    if (args.debug_dump) {
        args.dump_debug();
    }
    if (!args.valid) {
        return args.exit_code;
    }

    // load the source and parse tagged blocks
    input_t inp = input_t::load_and_parse(args.input);
    if (args.debug_dump) {
        inp.dump_debug(args.error_format);
    }
    if (inp.error.valid) {
        inp.error.print(args.error_format);
        return 10;
    }

    // compile source snippets to SPIRV blobs
    spirv_t::initialize_spirv_tools();
    spirv_t spirv = spirv_t::compile_glsl(inp);
    if (args.debug_dump) {
        spirv.dump_debug(inp, args.error_format);
    }
    if (!spirv.errors.empty()) {
        for (const error_t& err : spirv.errors) {
            err.print(args.error_format);
        }
        return 10;
    }
    spirv_t::finalize_spirv_tools();

    // cross-translate SPIRV to shader dialects
    spirvcross_t spirvcross = spirvcross_t::translate(inp, spirv, args.slang);
    if (args.debug_dump) {
        spirvcross.dump_debug(args.error_format);
    }
    if (spirvcross.error.valid) {
        spirvcross.error.print(args.error_format);
        return 10;
    }

    // compile shader-byte code if requested (HLSL / Metal)
    bytecode_t bytecode = bytecode_t::compile(inp, spirvcross, args.byte_code);
    if (args.debug_dump) {
        bytecode.dump_debug();
    }
    if (bytecode.error.valid) {
        bytecode.error.print(args.error_format);
        return 10;
    }

    // generate the output C header
    error_t err = sokol_t::gen(args, inp, spirvcross, bytecode);
    if (err.valid) {
        err.print(args.error_format);
        return 10;
    }

    // success
    return 0;
}

