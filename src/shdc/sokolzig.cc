/*
    Generate output header in C for sokol_gfx.h
*/
#include "shdc.h"
#include "fmt/format.h"
#include "pystring.h"
#include <stdio.h>

namespace shdc {

using namespace output;

static std::string file_content;

#if defined(_MSC_VER)
#define L(str, ...) file_content.append(fmt::format(str, __VA_ARGS__))
#else
#define L(str, ...) file_content.append(fmt::format(str, ##__VA_ARGS__))
#endif

static const char* img_type_to_sokol_type_str(image_t::type_t type) {
    switch (type) {
        case image_t::IMAGE_TYPE_2D:    return "._2D";
        case image_t::IMAGE_TYPE_CUBE:  return ".CUBE";
        case image_t::IMAGE_TYPE_3D:    return "._3D";
        case image_t::IMAGE_TYPE_ARRAY: return ".ARRAY";
        default: return "INVALID";
    }
}

static const char* img_basetype_to_sokol_samplertype_str(image_t::basetype_t basetype) {
    switch (basetype) {
        case image_t::IMAGE_BASETYPE_FLOAT: return ".FLOAT";
        case image_t::IMAGE_BASETYPE_SINT:  return ".SINT";
        case image_t::IMAGE_BASETYPE_UINT:  return ".UINT";
        default: return "INVALID";
    }
}

static const char* sokol_backend(slang_t::type_t slang) {
    switch (slang) {
        case slang_t::GLSL330:      return ".GLCORE33";
        case slang_t::GLSL100:      return ".GLES2";
        case slang_t::GLSL300ES:    return ".GLES3";
        case slang_t::HLSL4:        return ".D3D11";
        case slang_t::HLSL5:        return ".D3D11";
        case slang_t::METAL_MACOS:  return ".METAL_MACOS";
        case slang_t::METAL_IOS:    return ".METAL_IOS";
        case slang_t::METAL_SIM:    return ".METAL_SIMULATOR";
        case slang_t::WGPU:         return ".WGPU";
        default: return "<INVALID>";
    }
}

static void write_vertex_attrs(const input_t& inp, const spirvcross_t& spirvcross) {
    for (const spirvcross_source_t& src: spirvcross.sources) {
        if (src.refl.stage == stage_t::VS) {
            const snippet_t& vs_snippet = inp.snippets[src.snippet_index];
            for (const attr_t& attr: src.refl.inputs) {
                if (attr.slot >= 0) {
                    L("pub const ATTR_{}{}_{} = {};\n", mod_prefix(inp), vs_snippet.name, attr.name, attr.slot);
                }
            }
        }
    }
}

static void write_image_bind_slots(const input_t& inp, const spirvcross_t& spirvcross) {
    for (const image_t& img: spirvcross.unique_images) {
        L("pub const SLOT_{}{} = {};\n", mod_prefix(inp), img.name, img.slot);
    }
}

static std::string struct_case_name(const std::string& prefix, const std::string& name) {
    std::vector<std::string> splits;
    std::vector<std::string> parts = { pystring::capitalize(prefix) };
    pystring::split(name, splits, "_");
    for (const auto& part: splits) {
        parts.push_back(pystring::capitalize(part));
    }
    return pystring::join("", parts);
}

static std::string func_case_name(const std::string& prefix, const std::string& name) {
    std::vector<std::string> splits;
    std::vector<std::string> parts = { pystring::capitalize(prefix) };
    pystring::split(name, splits, "_");
    for (const auto& part: splits) {
        parts.push_back(pystring::capitalize(part));
    }
    std::string all = pystring::join("", parts);
    all[0] = tolower(all[0]);
    return all;
}

static void write_uniform_blocks(const input_t& inp, const spirvcross_t& spirvcross, slang_t::type_t slang) {
    for (const uniform_block_t& ub: spirvcross.unique_uniform_blocks) {
        L("pub const SLOT_{}{} = {};\n", mod_prefix(inp), ub.name, ub.slot);
        // FIXME: trying to 16-byte align this struct currently produces a Zig
        // compiler error: https://github.com/ziglang/zig/issues/7780
        L("pub const {} = packed struct {{\n", struct_case_name(mod_prefix(inp), ub.name));
        int cur_offset = 0;
        for (const uniform_t& uniform: ub.uniforms) {
            int next_offset = uniform.offset;
            if (next_offset > cur_offset) {
                L("    _pad_{}: [{}]u8 = undefined,\n", cur_offset, next_offset - cur_offset);
                cur_offset = next_offset;
            }
            if (inp.type_map.count(uniform_type_str(uniform.type)) > 0) {
                // user-provided type names
                if (uniform.array_count == 1) {
                    L("    {}: {}", uniform.name, inp.type_map.at(uniform_type_str(uniform.type)));
                }
                else {
                    L("    {}: [{}]{}", uniform.name, uniform.array_count, inp.type_map.at(uniform_type_str(uniform.type)));
                }
            }
            else {
                // default type names (float)
                if (uniform.array_count == 1) {
                    switch (uniform.type) {
                        case uniform_t::FLOAT:   L("    {}: f32", uniform.name); break;
                        case uniform_t::FLOAT2:  L("    {}: [2]f32", uniform.name); break;
                        case uniform_t::FLOAT3:  L("    {}: [3]f32", uniform.name); break;
                        case uniform_t::FLOAT4:  L("    {}: [4]f32", uniform.name); break;
                        case uniform_t::MAT4:    L("    {}: [16]f32", uniform.name); break;
                        default:                 L("    INVALID_UNIFORM_TYPE"); break;
                    }
                }
                else {
                    switch (uniform.type) {
                        case uniform_t::FLOAT:   L("    {}: [{}]f32", uniform.name, uniform.array_count); break;
                        case uniform_t::FLOAT2:  L("    {}: [{}][2]f32", uniform.name, uniform.array_count); break;
                        case uniform_t::FLOAT3:  L("    {}: [{}][3]f32", uniform.name, uniform.array_count); break;
                        case uniform_t::FLOAT4:  L("    {}: [{}][4]f32", uniform.name, uniform.array_count); break;
                        case uniform_t::MAT4:    L("    {}: [{}][16]f32", uniform.name, uniform.array_count); break;
                        default:                 L("    INVALID_UNIFORM_TYPE,"); break;
                    }
                }
            }
            if (0 == cur_offset) {
                // align the first item
                L(" align(16),\n");
            }
            else {
                L(",\n");
            }
            cur_offset += uniform_type_size(uniform.type) * uniform.array_count;
        }
        /* pad to multiple of 16-bytes struct size */
        const int round16 = roundup(cur_offset, 16);
        if (cur_offset != round16) {
            L("    uint8_t _pad_{}[{}];\n", cur_offset, round16-cur_offset);
        }
        L("}};\n");
    }
}

static void write_shader_sources_and_blobs(const input_t& inp,
                                           const spirvcross_t& spirvcross,
                                           const bytecode_t& bytecode,
                                           slang_t::type_t slang)
{
    for (int snippet_index = 0; snippet_index < (int)inp.snippets.size(); snippet_index++) {
        const snippet_t& snippet = inp.snippets[snippet_index];
        if ((snippet.type != snippet_t::VS) && (snippet.type != snippet_t::FS)) {
            continue;
        }
        int src_index = spirvcross.find_source_by_snippet_index(snippet_index);
        assert(src_index >= 0);
        const spirvcross_source_t& src = spirvcross.sources[src_index];
        int blob_index = bytecode.find_blob_by_snippet_index(snippet_index);
        const bytecode_blob_t* blob = 0;
        if (blob_index != -1) {
            blob = &bytecode.blobs[blob_index];
        }
        std::vector<std::string> lines;
        pystring::splitlines(src.source_code, lines);
        /* first write the source code in a comment block */
        L("//\n");
        for (const std::string& line: lines) {
            L("// {}\n", line);
        }
        L("//\n");
        if (blob) {
            std::string c_name = fmt::format("{}{}_bytecode_{}", mod_prefix(inp), snippet.name, slang_t::to_str(slang));
            L("const {} = [{}]u8 {{\n", c_name.c_str(), blob->data.size());
            const size_t len = blob->data.size();
            for (size_t i = 0; i < len; i++) {
                if ((i & 15) == 0) {
                    L("    ");
                }
                L("{:#04x},", blob->data[i]);
                if ((i & 15) == 15) {
                    L("\n");
                }
            }
            L("\n}};\n");
        }
        else {
            /* if no bytecode exists, write the source code, but also a byte array with a trailing 0 */
            std::string c_name = fmt::format("{}{}_source_{}", mod_prefix(inp), snippet.name, slang_t::to_str(slang));
            const size_t len = src.source_code.length() + 1;
            L("const {} = [{}]u8 {{\n", c_name.c_str(), len);
            for (size_t i = 0; i < len; i++) {
                if ((i & 15) == 0) {
                    L("    ");
                }
                L("{:#04x},", src.source_code[i]);
                if ((i & 15) == 15) {
                    L("\n");
                }
            }
            L("\n}};\n");
        }
    }
}

static void write_stage(const char* indent,
                        const char* stage_name,
                        const spirvcross_source_t& src,
                        const std::string& src_name,
                        const bytecode_blob_t* blob,
                        const std::string& blob_name,
                        slang_t::type_t slang)
{
    if (blob) {
        L("{}desc.{}.bytecode.ptr = &{};\n", indent, stage_name, blob_name);
        L("{}desc.{}.bytecode.size = {};\n", indent, stage_name, blob->data.size());
    }
    else {
        L("{}desc.{}.source = &{};\n", indent, stage_name, src_name);
        const char* d3d11_tgt = nullptr;
        if (slang == slang_t::HLSL4) {
            d3d11_tgt = (0 == strcmp("vs", stage_name)) ? "vs_4_0" : "ps_4_0";
        }
        else if (slang == slang_t::HLSL5) {
            d3d11_tgt = (0 == strcmp("vs", stage_name)) ? "vs_5_0" : "ps_5_0";
        }
        if (d3d11_tgt) {
            L("{}desc.{}.d3d11_target = \"{}\";\n", indent, stage_name, d3d11_tgt);
        }
    }
    L("{}desc.{}.entry = \"{}\";\n", indent, stage_name, src.refl.entry_point);
    for (int ub_index = 0; ub_index < uniform_block_t::NUM; ub_index++) {
        const uniform_block_t* ub = find_uniform_block(src.refl, ub_index);
        if (ub) {
            L("{}desc.{}.uniform_blocks[{}].size = {};\n", indent, stage_name, ub_index, roundup(ub->size, 16));
            if (slang_t::is_glsl(slang) && (ub->uniforms.size() > 0)) {
                L("{}desc.{}.uniform_blocks[{}].uniforms[0].name = \"{}\";\n", indent, stage_name, ub_index, ub->name);
                L("{}desc.{}.uniform_blocks[{}].uniforms[0].type = .FLOAT4;\n", indent, stage_name, ub_index);
                L("{}desc.{}.uniform_blocks[{}].uniforms[0].array_count = {};\n", indent, stage_name, ub_index, roundup(ub->size, 16) / 16);
            }
        }
    }
    for (int img_index = 0; img_index < image_t::NUM; img_index++) {
        const image_t* img = find_image(src.refl, img_index);
        if (img) {
            L("{}desc.{}.images[{}].name = \"{}\";\n", indent, stage_name, img_index, img->name);
            L("{}desc.{}.images[{}].type = {};\n", indent, stage_name, img_index, img_type_to_sokol_type_str(img->type));
            L("{}desc.{}.images[{}].sampler_type = {};\n", indent, stage_name, img_index, img_basetype_to_sokol_samplertype_str(img->base_type));
        }
    }
}

static void write_shader_desc_init(const char* indent, const program_t& prog, const input_t& inp, const spirvcross_t& spirvcross, const bytecode_t& bytecode, slang_t::type_t slang) {
    int vs_snippet_index = inp.snippet_map.at(prog.vs_name);
    int fs_snippet_index = inp.snippet_map.at(prog.fs_name);
    int vs_src_index = spirvcross.find_source_by_snippet_index(vs_snippet_index);
    int fs_src_index = spirvcross.find_source_by_snippet_index(fs_snippet_index);
    assert((vs_src_index >= 0) && (fs_src_index >= 0));
    const spirvcross_source_t& vs_src = spirvcross.sources[vs_src_index];
    const spirvcross_source_t& fs_src = spirvcross.sources[fs_src_index];
    int vs_blob_index = bytecode.find_blob_by_snippet_index(vs_snippet_index);
    int fs_blob_index = bytecode.find_blob_by_snippet_index(fs_snippet_index);
    const bytecode_blob_t* vs_blob = 0;
    const bytecode_blob_t* fs_blob = 0;
    if (vs_blob_index != -1) {
        vs_blob = &bytecode.blobs[vs_blob_index];
    }
    if (fs_blob_index != -1) {
        fs_blob = &bytecode.blobs[fs_blob_index];
    }
    std::string vs_src_name, fs_src_name;
    std::string vs_blob_name, fs_blob_name;
    if (vs_blob_index != -1) {
        vs_blob_name = fmt::format("{}{}_bytecode_{}", mod_prefix(inp), prog.vs_name, slang_t::to_str(slang));
    }
    else {
        vs_src_name = fmt::format("{}{}_source_{}", mod_prefix(inp), prog.vs_name, slang_t::to_str(slang));
    }
    if (fs_blob_index != -1) {
        fs_blob_name = fmt::format("{}{}_bytecode_{}", mod_prefix(inp), prog.fs_name, slang_t::to_str(slang));
    }
    else {
        fs_src_name = fmt::format("{}{}_source_{}", mod_prefix(inp), prog.fs_name, slang_t::to_str(slang));
    }

    /* write shader desc */
    for (int attr_index = 0; attr_index < attr_t::NUM; attr_index++) {
        const attr_t& attr = vs_src.refl.inputs[attr_index];
        if (attr.slot >= 0) {
            if (slang_t::is_glsl(slang)) {
                L("{}desc.attrs[{}].name = \"{}\";\n", indent, attr_index, attr.name);
            }
            else if (slang_t::is_hlsl(slang)) {
                L("{}desc.attrs[{}].sem_name = \"{}\";\n", indent, attr_index, attr.sem_name);
                L("{}desc.attrs[{}].sem_index = {};\n", indent, attr_index, attr.sem_index);
            }
        }
    }
    write_stage(indent, "vs", vs_src, vs_src_name, vs_blob, vs_blob_name, slang);
    write_stage(indent, "fs", fs_src, fs_src_name, fs_blob, fs_blob_name, slang);
    L("{}desc.label = \"{}{}_shader\";\n", indent, mod_prefix(inp), prog.name);
}

errmsg_t sokolzig_t::gen(const args_t& args, const input_t& inp,
                     const std::array<spirvcross_t,slang_t::NUM>& spirvcross,
                     const std::array<bytecode_t,slang_t::NUM>& bytecode)
{
    // first write everything into a string, and only when no errors occur,
    // dump this into a file (so we don't have half-written files lying around)
    file_content.clear();

    L("const sg = @import(\"sokol\").gfx;\n");
    bool comment_header_written = false;
    bool common_decls_written = false;
    for (int i = 0; i < slang_t::NUM; i++) {
        slang_t::type_t slang = (slang_t::type_t) i;
        if (args.slang & slang_t::bit(slang)) {
            errmsg_t err = check_errors(inp, spirvcross[i], slang);
            if (err.valid) {
                return err;
            }
            if (!comment_header_written) {
                // FIXME: write_header(args, inp, spirvcross[i]);
                comment_header_written = true;
            }
            if (!common_decls_written) {
                common_decls_written = true;
                write_vertex_attrs(inp, spirvcross[i]);
                write_image_bind_slots(inp, spirvcross[i]);
                write_uniform_blocks(inp, spirvcross[i], slang);
            }
            write_shader_sources_and_blobs(inp, spirvcross[i], bytecode[i], slang);
        }
    }

    // write access functions which return sg.ShaderDesc structs
    for (const auto& item: inp.programs) {
        const program_t& prog = item.second;
        L("pub fn {}ShaderDesc(backend: sg.Backend) sg.ShaderDesc {{\n", func_case_name(mod_prefix(inp), prog.name));
        L("    var desc: sg.ShaderDesc = .{{}};\n");
        L("    switch (backend) {{\n");
        for (int i = 0; i < slang_t::NUM; i++) {
            slang_t::type_t slang = (slang_t::type_t) i;
            if (args.slang & slang_t::bit(slang)) {
                L("        {} => {{\n", sokol_backend(slang));
                write_shader_desc_init("            ", prog, inp, spirvcross[i], bytecode[i], slang);
                L("        }},\n");
            }
        }
        L("        else => {{}},\n");
        L("    }}\n");
        L("    return desc;\n");
        L("}}\n");
    }

    // write result into output file
    FILE* f = fopen(args.output.c_str(), "w");
    if (!f) {
        return errmsg_t::error(inp.base_path, 0, fmt::format("failed to open output file '{}'", args.output));
    }
    fwrite(file_content.c_str(), file_content.length(), 1, f);
    fclose(f);
    return errmsg_t();
}

} // namespace shdc
