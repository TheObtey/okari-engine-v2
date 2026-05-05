import json
import struct
import subprocess
import threading
import webbrowser
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk


APP_TITLE = "(Okari) Nintendo Asset Importer"
APP_VERSION = "v0.3.2"
AUTHOR_NAME = "TheObtey"
AUTHOR_URL = "https://github.com/TheObtey"
CONFIG_PATH = Path(__file__).with_suffix(".config.json")


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def u8(data, offset):
    return data[offset]


def u16(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def read_c_string(data, offset):
    end = data.find(b"\0", offset)
    if end == -1:
        end = len(data)
    return data[offset:end].decode("shift_jis", errors="replace")


def read_j3d_string_table(data, base_offset):
    if base_offset <= 0 or base_offset + 4 > len(data):
        return []

    count = u16(data, base_offset)
    strings = []

    for index in range(count):
        entry_offset = base_offset + 4 + index * 4
        if entry_offset + 4 > len(data):
            break

        string_offset = u16(data, entry_offset + 2)
        strings.append(read_c_string(data, base_offset + string_offset))

    return strings


def iter_j3d_chunks(data):
    if len(data) < 0x20:
        return

    chunk_count = u32(data, 0x0C)
    offset = 0x20

    for _ in range(chunk_count):
        if offset + 8 > len(data):
            return

        tag = data[offset:offset + 4].decode("ascii", errors="replace")
        size = u32(data, offset + 4)

        if size <= 0 or offset + size > len(data):
            return

        yield tag, offset, size
        offset += size


def find_j3d_chunks(data):
    return {tag: (offset, size) for tag, offset, size in iter_j3d_chunks(data)}


def parse_tex1(data, tex1_offset):
    texture_count = u16(data, tex1_offset + 0x08)
    name_table_offset = u32(data, tex1_offset + 0x10)
    names = read_j3d_string_table(data, tex1_offset + name_table_offset)

    textures = []
    for index in range(texture_count):
        name = names[index] if index < len(names) else f"texture_{index}"
        textures.append({
            "index": index,
            "name": name
        })

    return textures


J3D_INVALID_INDEX = 0xFFFF
MAT3_MATERIAL_INIT_SIZE = 0x14C
MAT3_MAX_TEXTURE_SLOTS = 8


def safe_u16(data, offset, default=J3D_INVALID_INDEX):
    if offset < 0 or offset + 2 > len(data):
        return default
    return u16(data, offset)


def safe_u32(data, offset, default=0):
    if offset < 0 or offset + 4 > len(data):
        return default
    return u32(data, offset)


def get_chunk_relative_offset(data, chunk_offset, header_field_offset):
    value = safe_u32(data, chunk_offset + header_field_offset, 0)
    if value == 0:
        return None
    return chunk_offset + value



# Offsets inside J3DMaterialInitData (0x14C bytes) for J3D2 bmd3/bdl4.
# Important: most indices near the end of the block are u16 table indices,
# while cull / zCompLoc / dither are byte-sized indices.
MAT3_KNOWN_TEXNO_INDEX_OFFSET = 0x28
MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET = 0xB4
MAT3_KNOWN_ALPHA_COMPARE_INDEX_OFFSET = 0x144
MAT3_KNOWN_BLEND_MODE_INDEX_OFFSET = 0x146
MAT3_KNOWN_Z_MODE_INDEX_OFFSET = 0x148
MAT3_KNOWN_CULL_MODE_INDEX_OFFSET = 0x01
MAT3_KNOWN_Z_COMP_LOC_INDEX_OFFSET = 0x14A
MAT3_KNOWN_DITHER_INDEX_OFFSET = 0x14B

# MAT3 header table pointers for J3D2 bmd3/bdl4.
MAT3_TABLE_FIELDS = {
    "indirect": 0x18,
    "cull_mode": 0x1C,
    "material_color": 0x20,
    "color_channel_count": 0x24,
    "color_channel": 0x28,
    "ambient_color": 0x2C,
    "light": 0x30,
    "tex_gen_count": 0x34,
    "tex_coord": 0x38,
    "tex_mtx": 0x3C,
    "post_tex_gen_count": 0x40,
    "post_tex_coord": 0x44,
    "post_tex_mtx": 0x48,
    "tex_no": 0x4C,
    "tev_order": 0x50,
    "tev_color": 0x54,
    "tev_konst_color": 0x58,
    "tev_stage_count": 0x5C,
    "tev_stage": 0x60,
    "tev_swap_mode": 0x64,
    "tev_swap_mode_table": 0x68,
    "fog": 0x68,
    "alpha_compare": 0x6C,
    "blend_mode": 0x70,
    "z_mode": 0x74,
    "z_comp_loc": 0x78,
    "dither": 0x7C,
    "nbt_scale": 0x80,
}

GX_COMPARE_FUNCS = {0: "never", 1: "less", 2: "equal", 3: "lequal", 4: "greater", 5: "nequal", 6: "gequal", 7: "always"}
GX_ALPHA_OPS = {0: "and", 1: "or", 2: "xor", 3: "xnor"}
GX_BLEND_TYPES = {0: "none", 1: "blend", 2: "logic", 3: "subtract"}
GX_BLEND_FACTORS = {0: "zero", 1: "one", 2: "src_color", 3: "inv_src_color", 4: "src_alpha", 5: "inv_src_alpha", 6: "dst_alpha", 7: "inv_dst_alpha"}
GX_LOGIC_OPS = {0: "clear", 1: "and", 2: "rev_and", 3: "copy", 4: "inv_and", 5: "noop", 6: "xor", 7: "or", 8: "nor", 9: "equiv", 10: "inv", 11: "rev_or", 12: "inv_copy", 13: "inv_or", 14: "nand", 15: "set"}
GX_CULL_MODES = {0: "none", 1: "front", 2: "back", 3: "all"}


def enum_name(table, value):
    return table.get(value, f"unknown_{value}")


def get_mat3_table_offset(data, mat3_offset, table_name):
    field_offset = MAT3_TABLE_FIELDS.get(table_name)
    if field_offset is None:
        return None
    return get_chunk_relative_offset(data, mat3_offset, field_offset)


def read_u8_index(data, offset):
    if offset < 0 or offset >= len(data):
        return None
    value = u8(data, offset)
    if value == 0xFF:
        return None
    return value


def read_u16_index(data, offset):
    value = safe_u16(data, offset)
    if value == J3D_INVALID_INDEX:
        return None
    return value


def parse_cull_mode_value(data, table_offset, index):
    if table_offset is None or index is None:
        return {"index": index, "value": None, "name": "back", "raw": None}

    raw_offset = table_offset + index * 4
    if raw_offset + 4 <= len(data):
        value = safe_u32(data, raw_offset)
        if value in GX_CULL_MODES:
            return {"index": index, "value": value, "name": enum_name(GX_CULL_MODES, value), "raw": f"{value:08X}"}

    raw_offset = table_offset + index
    if raw_offset < len(data):
        value = u8(data, raw_offset)
        return {"index": index, "value": value, "name": enum_name(GX_CULL_MODES, value), "raw": f"{value:02X}"}

    return {"index": index, "value": None, "name": "back", "raw": None}


def parse_alpha_compare_value(data, table_offset, index):
    if table_offset is None or index is None:
        return {"index": index, "comp0": "always", "ref0": 0, "op": "and", "comp1": "always", "ref1": 0, "raw": None}

    raw_offset = table_offset + index * 8
    raw = data[raw_offset:raw_offset + 8] if raw_offset + 8 <= len(data) else b""

    if len(raw) < 5:
        return {"index": index, "comp0": "always", "ref0": 0, "op": "and", "comp1": "always", "ref1": 0, "raw": raw.hex().upper()}

    comp0, ref0, op, comp1, ref1 = raw[0], raw[1], raw[2], raw[3], raw[4]
    return {
        "index": index,
        "comp0_value": comp0,
        "comp0": enum_name(GX_COMPARE_FUNCS, comp0),
        "ref0": ref0,
        "op_value": op,
        "op": enum_name(GX_ALPHA_OPS, op),
        "comp1_value": comp1,
        "comp1": enum_name(GX_COMPARE_FUNCS, comp1),
        "ref1": ref1,
        "raw": raw.hex().upper(),
    }


def parse_blend_mode_value(data, table_offset, index):
    if table_offset is None or index is None:
        return {"index": index, "type": "none", "src": "one", "dst": "zero", "logic": "noop", "raw": None}

    raw_offset = table_offset + index * 4
    raw = data[raw_offset:raw_offset + 4] if raw_offset + 4 <= len(data) else b""

    if len(raw) < 4:
        return {"index": index, "type": "none", "src": "one", "dst": "zero", "logic": "noop", "raw": raw.hex().upper()}

    blend_type, src_factor, dst_factor, logic_op = raw[0], raw[1], raw[2], raw[3]
    return {
        "index": index,
        "type_value": blend_type,
        "type": enum_name(GX_BLEND_TYPES, blend_type),
        "src_value": src_factor,
        "src": enum_name(GX_BLEND_FACTORS, src_factor),
        "dst_value": dst_factor,
        "dst": enum_name(GX_BLEND_FACTORS, dst_factor),
        "logic_value": logic_op,
        "logic": enum_name(GX_LOGIC_OPS, logic_op),
        "raw": raw.hex().upper(),
    }


def parse_z_mode_value(data, table_offset, index):
    if table_offset is None or index is None:
        return {"index": index, "test": True, "func": "lequal", "write": True, "raw": None}

    raw_offset = table_offset + index * 4
    raw = data[raw_offset:raw_offset + 4] if raw_offset + 4 <= len(data) else b""

    if len(raw) < 3:
        return {"index": index, "test": True, "func": "lequal", "write": True, "raw": raw.hex().upper()}

    return {
        "index": index,
        "test": bool(raw[0]),
        "func_value": raw[1],
        "func": enum_name(GX_COMPARE_FUNCS, raw[1]),
        "write": bool(raw[2]),
        "raw": raw.hex().upper(),
    }


def parse_bool_table_value(data, table_offset, index, default=True):
    if table_offset is None or index is None:
        return {"index": index, "value": default, "raw": None}

    raw_offset = table_offset + index
    if raw_offset >= len(data):
        return {"index": index, "value": default, "raw": None}

    value = u8(data, raw_offset)
    return {"index": index, "value": bool(value), "raw": f"{value:02X}"}


def alpha_compare_requires_cutout(alpha_compare):
    comp0 = alpha_compare.get("comp0")
    comp1 = alpha_compare.get("comp1")
    ref0 = alpha_compare.get("ref0", 0)
    ref1 = alpha_compare.get("ref1", 0)
    return not (comp0 == "always" and comp1 == "always" and ref0 == 0 and ref1 == 0)

# GX blend is not the same thing as visual transparency.
# J3D materials use blend as part of the TEV/GX pipeline
# while still needing to be rendered as opaque.
# 
# Was Nintendo vibe coding??
def derive_alpha_mode(alpha_compare, blend_mode):
    if alpha_compare_requires_cutout(alpha_compare):
        return "cutout"
    
    return "opaque"


def resolve_textures_from_material_entry(data, material_entry_offset, mat3_offset, texture_names):
    textures = []
    tex_no_table_offset = get_mat3_table_offset(data, mat3_offset, "tex_no")

    if tex_no_table_offset is None or material_entry_offset is None:
        return textures

    for slot in range(MAT3_MAX_TEXTURE_SLOTS):
        tex_no_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_TEXNO_INDEX_OFFSET + slot * 2)
        if tex_no_index is None:
            continue

        texture_index = safe_u16(data, tex_no_table_offset + tex_no_index * 2)
        if texture_index == J3D_INVALID_INDEX or texture_index >= len(texture_names):
            continue

        textures.append({
            "slot": slot,
            "index": texture_index,
            "name": texture_names[texture_index],
            "j3d": {
                "tex_no_index": tex_no_index,
                "texno_index_offset": f"0x{MAT3_KNOWN_TEXNO_INDEX_OFFSET:02X}",
                "tex_table_relative_offset": f"0x{tex_no_table_offset - mat3_offset:X}"
            }
        })

    return textures


def parse_tev_orders_from_material_entry(data, material_entry_offset, mat3_offset, texture_names):
    tev_orders = []
    tev_order_table_offset = get_mat3_table_offset(data, mat3_offset, "tev_order")

    if tev_order_table_offset is None or material_entry_offset is None:
        return tev_orders

    for stage in range(16):
        tev_order_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET + stage * 2)
        if tev_order_index is None:
            continue

        raw_offset = tev_order_table_offset + tev_order_index * 4
        raw = data[raw_offset:raw_offset + 4] if raw_offset + 4 <= len(data) else b""

        if len(raw) < 4:
            continue

        tex_coord = raw[0]
        tex_map = raw[1]
        color_chan = raw[2]
        texture_name = texture_names[tex_map] if tex_map != 0xFF and tex_map < len(texture_names) else None

        tev_orders.append({
            "stage": stage,
            "index": tev_order_index,
            "tex_coord": tex_coord if tex_coord != 0xFF else None,
            "tex_map": tex_map if tex_map != 0xFF else None,
            "texture": texture_name,
            "color_channel": color_chan if color_chan != 0xFF else None,
            "raw": raw.hex().upper(),
            "j3d": {
                "tev_order_index_offset": f"0x{MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET:02X}",
                "tev_order_table_relative_offset": f"0x{tev_order_table_offset - mat3_offset:X}",
            }
        })

    return tev_orders


def parse_material_gx_state(data, mat3_offset, material_entry_offset):
    alpha_compare_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_ALPHA_COMPARE_INDEX_OFFSET)
    blend_mode_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_BLEND_MODE_INDEX_OFFSET)
    z_mode_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_Z_MODE_INDEX_OFFSET)
    cull_mode_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_CULL_MODE_INDEX_OFFSET)
    z_comp_loc_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_Z_COMP_LOC_INDEX_OFFSET)
    dither_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_DITHER_INDEX_OFFSET)

    alpha_compare = parse_alpha_compare_value(data, get_mat3_table_offset(data, mat3_offset, "alpha_compare"), alpha_compare_index)
    blend_mode = parse_blend_mode_value(data, get_mat3_table_offset(data, mat3_offset, "blend_mode"), blend_mode_index)
    z_mode = parse_z_mode_value(data, get_mat3_table_offset(data, mat3_offset, "z_mode"), z_mode_index)
    cull_mode = parse_cull_mode_value(data, get_mat3_table_offset(data, mat3_offset, "cull_mode"), cull_mode_index)
    z_comp_loc = parse_bool_table_value(data, get_mat3_table_offset(data, mat3_offset, "z_comp_loc"), z_comp_loc_index, True)
    dither = parse_bool_table_value(data, get_mat3_table_offset(data, mat3_offset, "dither"), dither_index, True)

    return {
        "indices": {
            "cull_mode": cull_mode_index,
            "alpha_compare": alpha_compare_index,
            "blend_mode": blend_mode_index,
            "z_mode": z_mode_index,
            "z_comp_loc": z_comp_loc_index,
            "dither": dither_index,
        },
        "cull_mode": cull_mode,
        "alpha_compare": alpha_compare,
        "blend_mode": blend_mode,
        "z_mode": z_mode,
        "z_comp_loc": z_comp_loc,
        "dither": dither,
    }



def parse_mat3(data, mat3_offset, textures=None, mat3_size=None):
    if textures is None:
        textures = []

    if mat3_size is None:
        mat3_size = safe_u32(data, mat3_offset + 0x04, 0)

    texture_names = [texture["name"] for texture in textures]

    material_count = u16(data, mat3_offset + 0x08)
    material_init_table = get_chunk_relative_offset(data, mat3_offset, 0x0C)
    material_remap_table = get_chunk_relative_offset(data, mat3_offset, 0x10)
    name_table_offset = u32(data, mat3_offset + 0x14)
    names = read_j3d_string_table(data, mat3_offset + name_table_offset)

    material_init_indices = []
    material_entry_offsets = []

    for index in range(material_count):
        material_init_index = index

        if material_remap_table is not None:
            remapped_index = safe_u16(data, material_remap_table + index * 2, index)
            if remapped_index != J3D_INVALID_INDEX:
                material_init_index = remapped_index

        material_init_indices.append(material_init_index)

        if material_init_table is not None:
            material_entry_offsets.append(material_init_table + material_init_index * MAT3_MATERIAL_INIT_SIZE)
        else:
            material_entry_offsets.append(None)


    materials = []
    for index in range(material_count):
        material_init_index = material_init_indices[index]
        material_entry_offset = material_entry_offsets[index]
        name = names[index] if index < len(names) else f"material_{index}"

        material_textures = []
        tev_orders = []
        gx_state = None

        if material_entry_offset is not None:
            material_textures = resolve_textures_from_material_entry(
                data,
                material_entry_offset,
                mat3_offset,
                texture_names
            )

            tev_orders = parse_tev_orders_from_material_entry(
                data,
                material_entry_offset,
                mat3_offset,
                texture_names
            )

            gx_state = parse_material_gx_state(
                data,
                mat3_offset,
                material_entry_offset
            )

        if gx_state is None:
            gx_state = {
                "indices": {},
                "cull_mode": {"name": "back"},
                "alpha_compare": {"comp0": "always", "ref0": 0, "op": "and", "comp1": "always", "ref1": 0},
                "blend_mode": {"type": "none", "src": "one", "dst": "zero"},
                "z_mode": {"test": True, "func": "lequal", "write": True},
                "z_comp_loc": {"value": True},
                "dither": {"value": True},
            }

        alpha_mode = derive_alpha_mode(gx_state["alpha_compare"], gx_state["blend_mode"])
        alpha_cutoff = gx_state["alpha_compare"].get("ref0", 0) / 255.0 if alpha_mode == "cutout" else 0.5
        render_queue = "transparent" if alpha_mode == "blend" else ("cutout" if alpha_mode == "cutout" else "opaque")

        materials.append({
            "index": index,
            "name": name,
            "textures": material_textures,
            "alpha_mode": alpha_mode,
            "alpha_cutoff": alpha_cutoff,
            "blend": {
                "enabled": gx_state["blend_mode"].get("type") in ("blend", "subtract", "logic"),
                "type": gx_state["blend_mode"].get("type"),
                "src": gx_state["blend_mode"].get("src"),
                "dst": gx_state["blend_mode"].get("dst"),
                "logic": gx_state["blend_mode"].get("logic"),
            },
            "culling": gx_state["cull_mode"].get("name", "back"),
            "depth": {
                "test": gx_state["z_mode"].get("test", True),
                "write": gx_state["z_mode"].get("write", True),
                "func": gx_state["z_mode"].get("func", "lequal")
            },
            "render_queue": render_queue,
            "j3d": {
                "mat3_index": index,
                "material_init_index": material_init_index,
                "material_entry_offset": material_entry_offset - mat3_offset if material_entry_offset is not None else None,
                "gx": gx_state,
                "tev_orders": tev_orders
            }
        })

    return materials, {
        "material_count": material_count,
        "texture_count": len(textures),
        "material_init_table_relative_offset": f"0x{material_init_table - mat3_offset:X}" if material_init_table is not None else None,
        "material_remap_table_relative_offset": f"0x{material_remap_table - mat3_offset:X}" if material_remap_table is not None else None,
        "tex_no_table_relative_offset": f"0x{get_mat3_table_offset(data, mat3_offset, 'tex_no') - mat3_offset:X}" if get_mat3_table_offset(data, mat3_offset, "tex_no") is not None else None,
        "tev_order_table_relative_offset": f"0x{get_mat3_table_offset(data, mat3_offset, 'tev_order') - mat3_offset:X}" if get_mat3_table_offset(data, mat3_offset, "tev_order") is not None else None,
    }

def build_okmat_document(model_path, data):
    chunks = find_j3d_chunks(data)

    if "MAT3" not in chunks:
        raise ValueError("MAT3 chunk not found")

    textures = []
    if "TEX1" in chunks:
        textures = parse_tex1(data, chunks["TEX1"][0])

    materials, mat3_debug = parse_mat3(data, chunks["MAT3"][0], textures, chunks["MAT3"][1])

    return {
        "format": "okmat",
        "version": 1,
        "source": {
            "file": model_path.name,
            "type": data[:8].decode("ascii", errors="replace"),
        },
        "textures": textures,
        "materials": materials,
        "debug": {
            "mat3": mat3_debug
        }
    }


def convert_model_to_okmat(model_path, input_root, output_root, log):
    relative = model_path.relative_to(input_root)
    output_dir = output_root / relative.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    raw = model_path.read_bytes()
    data = yaz0_decompress(raw)
    document = build_okmat_document(model_path, data)

    output_path = output_dir / f"{model_path.stem}.okmat"
    output_path.write_text(json.dumps(document, indent=4, ensure_ascii=False), encoding="utf-8")

    material_count = len(document["materials"])
    texture_count = len(document["textures"])
    log(f"[OKMAT] {model_path} -> {output_path} ({material_count} materials, {texture_count} textures)")
    return True


def yaz0_decompress(data):
    if data[:4] != b"Yaz0":
        return data

    size = u32(data, 4)
    src = 16
    dst = bytearray()

    code = 0
    bits = 0

    while len(dst) < size:
        if bits == 0:
            code = data[src]
            src += 1
            bits = 8

        if code & 0x80:
            dst.append(data[src])
            src += 1
        else:
            b1 = data[src]
            b2 = data[src + 1]
            src += 2

            distance = ((b1 & 0x0F) << 8) | b2
            copy_src = len(dst) - distance - 1

            length = b1 >> 4
            if length == 0:
                length = data[src] + 0x12
                src += 1
            else:
                length += 2

            for _ in range(length):
                dst.append(dst[copy_src])
                copy_src += 1

        code <<= 1
        bits -= 1

    return bytes(dst)


def get_j3d_category(extension):
    if extension in [".bmd", ".bdl"]:
        return "models"

    if extension == ".bck":
        return "skeletal_animations"

    if extension in [".btk", ".btp"]:
        return "texture_animations"

    if extension in [".brk", ".bpk", ".bva"]:
        return "material_animations"

    return "misc_j3d"


def extract_j3d_from_arc(arc_path, input_root, output_root, log):
    raw = arc_path.read_bytes()
    data = yaz0_decompress(raw)

    signatures = [
        # Models
        (b"J3D2bmd3", ".bmd"),
        (b"J3D2bdl4", ".bdl"),

        # Skeletal animations
        (b"J3D1bck1", ".bck"),

        # Texture / material / visibility animations
        (b"J3D1btk1", ".btk"),
        (b"J3D1btp1", ".btp"),
        (b"J3D1brk1", ".brk"),
        (b"J3D1bpk1", ".bpk"),
        (b"J3D1bva1", ".bva"),
    ]

    relative = arc_path.relative_to(input_root).with_suffix("")
    archive_label = str(relative).replace("\\", "__").replace("/", "__")

    extracted = 0
    counts_by_category = {}

    for signature, ext in signatures:
        offset = 0
        type_index = 0

        while True:
            pos = data.find(signature, offset)

            if pos == -1:
                break

            if pos + 0x10 > len(data):
                break

            file_size = u32(data, pos + 0x08)

            if file_size <= 0 or pos + file_size > len(data):
                offset = pos + 1
                continue

            category = get_j3d_category(ext)
            output_dir = output_root / category / archive_label
            output_dir.mkdir(parents=True, exist_ok=True)

            file_name = f"{archive_label}_{type_index:03d}{ext}"
            output_path = output_dir / file_name

            output_path.write_bytes(data[pos:pos + file_size])
            log(f"[EXTRACT:{category}] {output_path}")

            extracted += 1
            counts_by_category[category] = counts_by_category.get(category, 0) + 1
            type_index += 1
            offset = pos + file_size

    return extracted, counts_by_category


def convert_model(superbmd_path, model_path, input_root, output_root, log):
    relative = model_path.relative_to(input_root)
    output_dir = output_root / relative.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    output_path = output_dir / f"{model_path.stem}.dae"

    command = [
        str(superbmd_path),
        str(model_path),
        str(output_path),
    ]

    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False
    )

    if result.returncode != 0:
        log(f"[FAIL] {model_path}")
        if result.stderr:
            log(result.stderr.strip())
        if result.stdout:
            log(result.stdout.strip())
        return False

    log(f"[CONVERT] {model_path} -> {output_path}")
    return True


def convert_dae_to_fbx(blender_path, dae_path, input_root, output_root, log):
    relative = dae_path.relative_to(input_root)
    output_dir = output_root / relative.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    output_path = output_dir / f"{dae_path.stem}.fbx"

    blender_script = f"""
        import bpy

        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete()

        bpy.ops.wm.collada_import(filepath=r"{dae_path}")

        bpy.ops.object.select_all(action='SELECT')

        bpy.ops.export_scene.fbx(
            filepath=r"{output_path}",
            use_selection=True,
            apply_unit_scale=True,
            bake_space_transform=False,
            object_types={{'MESH', 'ARMATURE'}},
            mesh_smooth_type='OFF',
            add_leaf_bones=False
        )
    """

    command = [
        str(blender_path),
        "--background",
        "--python-expr",
        blender_script
    ]

    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=False
    )

    if result.returncode != 0:
        log(f"[FAIL] {dae_path}")
        if result.stderr:
            log(result.stderr.strip())
        if result.stdout:
            log(result.stdout.strip())
        return False

    log(f"[FBX] {dae_path} -> {output_path}")
    return True


class TPAssetImporterGUI:
    def __init__(self, root):
        self.root = root
        self.root.title(f"NAI {APP_VERSION}")
        self.root.configure(bg="#0f1115")
        self.configure_window_size()

        self.dump_dir = tk.StringVar(value="dump_folder")
        self.extract_dir = tk.StringVar(value="extracted_assets")
        self.dae_dir = tk.StringVar(value="dae")
        self.superbmd_path = tk.StringVar(value="")
        self.fbx_dir = tk.StringVar(value="fbx")
        self.blender_path = tk.StringVar(value="")
        self.okmat_dir = tk.StringVar(value="okmat")

        self.is_running = False
        self.action_buttons = []

        self.setup_style()
        self.load_config()
        self.build_ui()

        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def configure_window_size(self):
        screen_w = self.root.winfo_screenwidth()
        screen_h = self.root.winfo_screenheight()

        window_w = min(1180, max(940, screen_w - 120))
        window_h = min(840, max(640, screen_h - 120))

        pos_x = max(0, (screen_w - window_w) // 2)
        pos_y = max(0, (screen_h - window_h) // 2)

        self.root.geometry(f"{window_w}x{window_h}+{pos_x}+{pos_y}")
        self.root.minsize(900, 620)

    def setup_style(self):
        self.colors = {
            "bg": "#0f1115",
            "surface": "#15181e",
            "surface_2": "#1b1f27",
            "surface_3": "#222733",
            "border": "#2a2f3a",
            "border_light": "#3a404c",
            "text": "#ffffff",
            "muted": "#a0a6b0",
            "subtle": "#6b7280",
            "accent": "#d1d5db",
            "accent_hover": "#ffffff",
            "success": "#22c55e",
            "danger": "#ef4444",
            "tab_bg": "#15181e",
            "tab_hover": "#1f2430",
            "tab_selected": "#2c3443",
        }

        style = ttk.Style()
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        c = self.colors

        style.configure("TFrame", background=c["bg"])
        style.configure("App.TFrame", background=c["bg"])
        style.configure("Panel.TFrame", background=c["surface"])
        style.configure("Card.TFrame", background=c["surface_2"], relief="flat", borderwidth=0)
        style.configure("Footer.TFrame", background=c["surface"])

        style.configure("TLabel", background=c["bg"], foreground=c["text"], font=("Segoe UI", 9))
        style.configure("Title.TLabel", background=c["bg"], foreground=c["text"], font=("Segoe UI", 21, "bold"))
        style.configure("Subtitle.TLabel", background=c["bg"], foreground=c["muted"], font=("Segoe UI", 10))
        style.configure("SectionTitle.TLabel", background=c["surface"], foreground=c["text"], font=("Segoe UI", 15, "bold"))
        style.configure("SectionText.TLabel", background=c["surface"], foreground=c["muted"], font=("Segoe UI", 9))
        style.configure("CardTitle.TLabel", background=c["surface_2"], foreground=c["text"], font=("Segoe UI", 11, "bold"))
        style.configure("CardText.TLabel", background=c["surface_2"], foreground=c["muted"], font=("Segoe UI", 9))
        style.configure("PathLabel.TLabel", background=c["surface_2"], foreground=c["text"], font=("Segoe UI", 9, "bold"))
        style.configure("Hint.TLabel", background=c["surface_2"], foreground=c["muted"], font=("Segoe UI", 8))
        style.configure("Footer.TLabel", background=c["surface"], foreground=c["muted"], font=("Segoe UI", 8))
        style.configure("FooterLink.TLabel", background=c["surface"], foreground=c["text"], font=("Segoe UI", 8, "bold"))
        style.configure("Status.TLabel", background=c["surface"], foreground=c["muted"], font=("Segoe UI", 9))

        style.configure(
            "TEntry",
            fieldbackground=c["surface_3"],
            background=c["surface_3"],
            foreground=c["text"],
            bordercolor=c["border"],
            lightcolor=c["border"],
            darkcolor=c["border"],
            insertcolor=c["text"],
            padding=(8, 7),
        )
        style.map("TEntry", fieldbackground=[("disabled", c["surface_3"])], foreground=[("disabled", c["subtle"])])

        style.configure(
            "TButton",
            background=c["surface_3"],
            foreground=c["text"],
            borderwidth=0,
            focusthickness=0,
            focuscolor=c["surface_3"],
            font=("Segoe UI", 9, "bold"),
            padding=(12, 8),
        )
        style.map(
            "TButton",
            background=[("active", c["border"]), ("disabled", c["surface"])],
            foreground=[("active", c["text"]), ("disabled", c["subtle"])],
        )

        style.configure(
            "Primary.TButton",
            background=c["surface_3"],
            foreground=c["text"],
            borderwidth=0,
            focusthickness=0,
            font=("Segoe UI", 10, "bold"),
            padding=(16, 10),
        )
        style.map(
            "Primary.TButton",
            background=[("active", c["border_light"]), ("disabled", c["surface"])],
            foreground=[("disabled", c["subtle"])],
        )

        style.configure(
            "Horizontal.TProgressbar",
            background=c["border_light"],
            troughcolor=c["surface_3"],
            bordercolor=c["surface_3"],
            lightcolor=c["border_light"],
            darkcolor=c["border_light"],
        )

        style.configure(
            "Vertical.TScrollbar",
            background="#ffffff",
            troughcolor=c["surface"],
            bordercolor=c["surface"],
            arrowcolor="#ffffff"
        )
        style.map("Vertical.TScrollbar", background=[("active", "#ffffff"), ("!active", "#ffffff")])

    def load_config(self):
        if not CONFIG_PATH.exists():
            return

        try:
            config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        except Exception:
            return

        self.dump_dir.set(config.get("dump_dir", self.dump_dir.get()))
        self.extract_dir.set(config.get("extract_dir", self.extract_dir.get()))
        self.dae_dir.set(config.get("dae_dir", self.dae_dir.get()))
        self.superbmd_path.set(config.get("superbmd_path", self.superbmd_path.get()))
        self.fbx_dir.set(config.get("fbx_dir", self.fbx_dir.get()))
        self.blender_path.set(config.get("blender_path", self.blender_path.get()))
        self.okmat_dir.set(config.get("okmat_dir", self.okmat_dir.get()))

    def save_config(self):
        config = {
            "dump_dir": self.dump_dir.get(),
            "extract_dir": self.extract_dir.get(),
            "dae_dir": self.dae_dir.get(),
            "superbmd_path": self.superbmd_path.get(),
            "fbx_dir": self.fbx_dir.get(),
            "blender_path": self.blender_path.get(),
            "okmat_dir": self.okmat_dir.get(),
        }

        try:
            CONFIG_PATH.write_text(json.dumps(config, indent=4), encoding="utf-8")
        except Exception as e:
            self.log(f"[WARN] Could not save config: {e}")

    def on_close(self):
        self.save_config()
        self.root.destroy()

    def build_ui(self):
        self.tabs = {}
        self.tab_buttons = {}
        self.active_tab = None

        main = ttk.Frame(self.root, style="App.TFrame", padding=(22, 18, 22, 12))
        main.pack(fill="both", expand=True)
        main.columnconfigure(0, weight=1)
        main.rowconfigure(1, weight=1)

        header = ttk.Frame(main, style="App.TFrame")
        header.grid(row=0, column=0, sticky="ew", pady=(0, 14))
        header.columnconfigure(0, weight=1)

        title_row = ttk.Frame(header, style="App.TFrame")
        title_row.grid(row=0, column=0, sticky="ew")
        ttk.Label(title_row, text="(Okari) Nintendo Asset Importer", style="Title.TLabel").pack(side="left", anchor="w")
        ttk.Label(title_row, text=APP_VERSION, style="Subtitle.TLabel").pack(side="left", anchor="s", padx=(10, 0), pady=(0, 4))

        ttk.Label(
            header,
            text="Extract Nintendo J3D assets step by step, then convert models into engine-ready formats.",
            style="Subtitle.TLabel"
        ).grid(row=1, column=0, sticky="w", pady=(6, 0))

        workspace = ttk.Frame(main, style="Panel.TFrame", padding=(14, 14, 14, 14))
        workspace.grid(row=1, column=0, sticky="nsew")
        workspace.columnconfigure(0, weight=1)
        workspace.rowconfigure(1, weight=1)

        self.tab_bar = tk.Frame(workspace, bg=self.colors["tab_bg"], highlightthickness=0, bd=0)
        self.tab_bar.grid(row=0, column=0, sticky="w", pady=(0, 14))

        self.tab_content = ttk.Frame(workspace, style="Panel.TFrame")
        self.tab_content.grid(row=1, column=0, sticky="nsew")
        self.tab_content.columnconfigure(0, weight=1)
        self.tab_content.rowconfigure(0, weight=1)

        self.build_extract_tab()
        self.build_model_tab()
        self.build_okmat_tab()
        self.build_fbx_tab()
        self.build_settings_tab()
        self.switch_tab("extract")

        bottom = ttk.Frame(main, style="Panel.TFrame", padding=(14, 12))
        bottom.grid(row=2, column=0, sticky="ew", pady=(12, 0))
        bottom.columnconfigure(0, weight=1)

        log_header = ttk.Frame(bottom, style="Panel.TFrame")
        log_header.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        log_header.columnconfigure(1, weight=1)

        ttk.Label(log_header, text="Activity log", style="SectionTitle.TLabel").grid(row=0, column=0, sticky="w")
        self.status_label = ttk.Label(log_header, text="Ready", style="Status.TLabel")
        self.status_label.grid(row=0, column=1, sticky="w", padx=(12, 0), pady=(3, 0))

        self.clear_button = ttk.Button(log_header, text="Clear", command=self.clear_log)
        self.clear_button.grid(row=0, column=2, sticky="e")

        self.progress = ttk.Progressbar(bottom, mode="determinate", style="Horizontal.TProgressbar")
        self.progress.grid(row=1, column=0, sticky="ew", pady=(0, 8))

        log_frame = ttk.Frame(bottom, style="Panel.TFrame")
        log_frame.grid(row=2, column=0, sticky="ew")
        log_frame.columnconfigure(0, weight=1)

        self.log_box = tk.Text(
            log_frame,
            height=7,
            wrap="word",
            bg="#0b0d12",
            fg=self.colors["text"],
            insertbackground=self.colors["text"],
            selectbackground=self.colors["border_light"],
            selectforeground="#ffffff",
            relief="flat",
            padx=12,
            pady=10,
            font=("Cascadia Mono", 9)
        )
        self.log_box.grid(row=0, column=0, sticky="ew")

        scrollbar = ttk.Scrollbar(log_frame, command=self.log_box.yview, style="Vertical.TScrollbar")
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.log_box.configure(yscrollcommand=scrollbar.set)

        footer = ttk.Frame(main, style="Footer.TFrame", padding=(14, 8))
        footer.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        footer.columnconfigure(1, weight=1)

        ttk.Label(footer, text=f"{APP_TITLE} • {APP_VERSION} • made with ❤️ by", style="Footer.TLabel").grid(row=0, column=0, sticky="w")
        author = ttk.Label(footer, text=f" {AUTHOR_NAME}", style="FooterLink.TLabel", cursor="hand2")
        author.grid(row=0, column=1, sticky="w")
        author.bind("<Button-1>", lambda _event: webbrowser.open(AUTHOR_URL))
        ttk.Label(footer, text="Okari Engine asset pipeline", style="Footer.TLabel").grid(row=0, column=2, sticky="e")

    def add_tab_button(self, key, text):
        button = tk.Button(
            self.tab_bar,
            text=text,
            command=lambda: self.switch_tab(key),
            bd=0,
            highlightthickness=0,
            relief="flat",
            padx=16,
            pady=7,
            font=("Segoe UI", 10, "bold"),
            cursor="hand2"
        )
        button.pack(side="left", padx=(0, 2), pady=3)
        self.tab_buttons[key] = button

    def register_tab(self, key, title):
        self.add_tab_button(key, title)
        frame = ttk.Frame(self.tab_content, style="Panel.TFrame", padding=(0, 0, 0, 0))
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        frame.grid(row=0, column=0, sticky="nsew")
        self.tabs[key] = frame
        return frame

    def switch_tab(self, key):
        if key not in self.tabs:
            return

        self.active_tab = key
        self.tabs[key].tkraise()

        for tab_key, button in self.tab_buttons.items():
            selected = tab_key == key
            button.configure(
                bg=self.colors["tab_selected"] if selected else self.colors["tab_bg"],
                fg=self.colors["text"] if selected else self.colors["muted"],
                activebackground=self.colors["tab_selected"] if selected else self.colors["tab_hover"],
                activeforeground=self.colors["text"],
            )

    def make_scrollable(self, parent):
        container = tk.Frame(parent, bg=self.colors["surface"], highlightthickness=0, bd=0)
        container.grid(row=0, column=0, sticky="nsew")
        container.rowconfigure(0, weight=1)
        container.columnconfigure(0, weight=1)

        canvas = tk.Canvas(
            container,
            bg=self.colors["surface"],
            highlightthickness=0,
            bd=0,
            yscrollincrement=24,
        )
        canvas.grid(row=0, column=0, sticky="nsew")

        scrollbar = ttk.Scrollbar(container, orient="vertical", command=canvas.yview, style="Vertical.TScrollbar")
        scrollbar.grid(row=0, column=1, sticky="ns")
        canvas.configure(yscrollcommand=scrollbar.set)

        scroll_frame = tk.Frame(canvas, bg=self.colors["surface"])
        scroll_frame.columnconfigure(0, weight=1)
        window_id = canvas.create_window((0, 0), window=scroll_frame, anchor="nw")

        def update_scrollregion(_event=None):
            canvas.configure(scrollregion=canvas.bbox("all"))

        def resize_frame(event):
            canvas.itemconfig(window_id, width=event.width)
            update_scrollregion()

        scroll_frame.bind("<Configure>", update_scrollregion)
        canvas.bind("<Configure>", resize_frame)

        def _on_mousewheel(event):
            if event.num == 4:
                delta = -3
            elif event.num == 5:
                delta = 3
            else:
                delta = int(-event.delta / 120)
                if delta == 0:
                    delta = -1 if event.delta > 0 else 1
            canvas.yview_scroll(delta, "units")
            return "break"

        def bind_wheel(_event=None):
            canvas.bind_all("<MouseWheel>", _on_mousewheel)
            canvas.bind_all("<Button-4>", _on_mousewheel)
            canvas.bind_all("<Button-5>", _on_mousewheel)

        def unbind_wheel(_event=None):
            canvas.unbind_all("<MouseWheel>")
            canvas.unbind_all("<Button-4>")
            canvas.unbind_all("<Button-5>")

        container.bind("<Enter>", bind_wheel)
        container.bind("<Leave>", unbind_wheel)
        canvas.bind("<Destroy>", unbind_wheel)

        return scroll_frame

    def build_extract_tab(self):
        tab = self.register_tab("extract", "Extract")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Extract J3D assets",
            "Scan your dumped ISO folder recursively, read every .arc archive, and extract supported J3D assets. Output is automatically organized by type: models, skeletal animations, texture animations, and material animations."
        )

        self.create_path_row(
            scroll_frame,
            "Dumped ISO folder",
            "Root folder extracted from your GameCube/Wii ISO. The tool will search for .arc files inside this folder and all subfolders.",
            self.dump_dir,
            self.select_dump_folder,
            1
        )
        self.create_path_row(
            scroll_frame,
            "Extracted assets output",
            "Destination folder for extracted .bmd, .bdl, .bck, .btk, .btp, .brk, .bpk and .bva files.",
            self.extract_dir,
            self.select_extract_folder,
            2
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=3, column=0, sticky="ew", pady=(16, 0))
        self.extract_button = ttk.Button(
            actions,
            text="Extract assets",
            style="Primary.TButton",
            command=self.run_extract
        )
        self.extract_button.pack(side="left")
        ttk.Label(actions, text="Creates typed folders like models/ and skeletal_animations/.", style="SectionText.TLabel").pack(side="left", padx=(12, 0))
        self.action_buttons.append(self.extract_button)

    def build_model_tab(self):
        tab = self.register_tab("models", "Models to DAE")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Convert Nintendo models to Collada",
            "Use SuperBMD to convert extracted .bmd and .bdl files into .dae files. If the extracted assets folder contains a models/ subfolder, it will be used automatically."
        )

        self.create_path_row(
            scroll_frame,
            "Extracted assets input",
            "Folder created by the Extract step. The converter will look inside models/ when available.",
            self.extract_dir,
            self.select_extract_folder,
            1
        )
        self.create_path_row(
            scroll_frame,
            "SuperBMD executable",
            "Path to SuperBMD.exe used for BMD/BDL to DAE conversion.",
            self.superbmd_path,
            self.select_superbmd,
            2,
            file_path=True
        )
        self.create_path_row(
            scroll_frame,
            "DAE output",
            "Destination folder for generated .dae files.",
            self.dae_dir,
            self.select_dae_folder,
            3
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=4, column=0, sticky="ew", pady=(16, 0))
        self.convert_button = ttk.Button(
            actions,
            text="Convert models",
            style="Primary.TButton",
            command=self.run_convert
        )
        self.convert_button.pack(side="left")
        ttk.Label(actions, text="Only converts .bmd and .bdl files.", style="SectionText.TLabel").pack(side="left", padx=(12, 0))
        self.action_buttons.append(self.convert_button)

    def build_okmat_tab(self):
        tab = self.register_tab("okmat", "Materials to OKMAT")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Extract Nintendo materials to OKMAT",
            "Read MAT3 and TEX1 directly from extracted .bmd/.bdl files and generate clean .okmat files containing Nintendo material states: textures, TEV orders, alpha compare, blend, culling and depth."
        )

        self.create_path_row(
            scroll_frame,
            "Extracted assets input",
            "Folder created by the Extract step. The parser will look inside models/ when available.",
            self.extract_dir,
            self.select_extract_folder,
            1
        )
        self.create_path_row(
            scroll_frame,
            "OKMAT output",
            "Destination folder for generated .okmat material files.",
            self.okmat_dir,
            self.select_okmat_folder,
            2
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=3, column=0, sticky="ew", pady=(16, 0))
        self.okmat_button = ttk.Button(
            actions,
            text="Generate OKMAT",
            style="Primary.TButton",
            command=self.run_okmat_convert
        )
        self.okmat_button.pack(side="left")
        ttk.Label(actions, text="Reads .bmd/.bdl.", style="SectionText.TLabel").pack(side="left", padx=(12, 0))
        self.action_buttons.append(self.okmat_button)


    def build_fbx_tab(self):
        tab = self.register_tab("fbx", "DAE to FBX")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Convert Collada models to FBX",
            "Run Blender in background mode to import each .dae file and export an .fbx. This step can be slow, but it does not modify extracted assets or DAE files."
        )

        self.create_path_row(
            scroll_frame,
            "DAE input",
            "Folder created by the Models to DAE step. Every .dae found recursively will be converted.",
            self.dae_dir,
            self.select_dae_folder,
            1
        )
        self.create_path_row(
            scroll_frame,
            "Blender executable",
            "Path to blender.exe used in background mode for DAE to FBX conversion.",
            self.blender_path,
            self.select_blender,
            2,
            file_path=True
        )
        self.create_path_row(
            scroll_frame,
            "FBX output",
            "Final folder containing .fbx files ready for your engine import step.",
            self.fbx_dir,
            self.select_fbx_folder,
            3
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=4, column=0, sticky="ew", pady=(16, 0))
        self.fbx_button = ttk.Button(
            actions,
            text="Convert to FBX",
            style="Primary.TButton",
            command=self.run_fbx_convert
        )
        self.fbx_button.pack(side="left")
        ttk.Label(actions, text="Uses Blender, so large batches can take a long time.", style="SectionText.TLabel").pack(side="left", padx=(12, 0))
        self.action_buttons.append(self.fbx_button)

    def build_settings_tab(self):
        tab = self.register_tab("settings", "Settings")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Saved paths",
            f"All selected paths are saved automatically to: {CONFIG_PATH}. You can also save them manually or open the config folder below."
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=1, column=0, sticky="ew", pady=(12, 0))
        ttk.Button(actions, text="Save paths", style="Primary.TButton", command=self.save_config_with_popup).pack(side="left")
        ttk.Button(actions, text="Open config folder", command=self.open_config_folder).pack(side="left", padx=(10, 0))

    def create_info_card(self, parent, title, text):
        card = ttk.Frame(parent, style="Card.TFrame", padding=16)
        card.grid(row=0, column=0, sticky="ew", pady=(0, 16))
        card.columnconfigure(0, weight=1)

        ttk.Label(card, text=title, style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(card, text=text, style="CardText.TLabel", wraplength=940, justify="left").grid(row=1, column=0, sticky="ew", pady=(7, 0))

    def create_path_row(self, parent, label, hint, variable, command, row, file_path=False):
        card = ttk.Frame(parent, style="Card.TFrame", padding=14)
        card.grid(row=row, column=0, sticky="ew", pady=7)
        card.columnconfigure(0, weight=1)

        ttk.Label(card, text=label, style="PathLabel.TLabel").grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(card, text=hint, style="Hint.TLabel", wraplength=900, justify="left").grid(row=1, column=0, columnspan=3, sticky="ew", pady=(3, 9))

        entry = ttk.Entry(card, textvariable=variable)
        entry.grid(row=2, column=0, sticky="ew", padx=(0, 8))
        entry.bind("<FocusOut>", lambda _event: self.save_config())
        entry.bind("<Return>", lambda _event: self.save_config())

        browse_button = ttk.Button(card, text="Browse", command=command)
        browse_button.grid(row=2, column=1, sticky="e")

        open_button = ttk.Button(card, text="Open", command=lambda: self.open_path(variable.get(), file_path=file_path))
        open_button.grid(row=2, column=2, sticky="e", padx=(6, 0))

    def select_dump_folder(self):
        self.select_directory("Select GCM_DUMP folder", self.dump_dir)

    def select_extract_folder(self):
        self.select_directory("Select extracted assets folder", self.extract_dir)

    def select_dae_folder(self):
        self.select_directory("Select DAE output folder", self.dae_dir)

    def select_fbx_folder(self):
        self.select_directory("Select FBX output folder", self.fbx_dir)

    def select_okmat_folder(self):
        self.select_directory("Select OKMAT output folder", self.okmat_dir)

    def select_directory(self, title, variable):
        path = filedialog.askdirectory(title=title)
        if path:
            variable.set(path)
            self.save_config()

    def select_superbmd(self):
        self.select_file("Select SuperBMD.exe", self.superbmd_path)

    def select_blender(self):
        self.select_file("Select Blender.exe", self.blender_path)

    def select_file(self, title, variable):
        path = filedialog.askopenfilename(
            title=title,
            filetypes=[("Executable", "*.exe"), ("All files", "*.*")]
        )
        if path:
            variable.set(path)
            self.save_config()

    def open_path(self, path, file_path=False):
        if not path:
            messagebox.showwarning("Missing path", "No path selected yet.")
            return

        target = Path(path)
        if file_path:
            target = target.parent

        if not target.exists():
            messagebox.showwarning("Invalid path", f"Path does not exist:\n{target}")
            return

        try:
            import os
            os.startfile(target)
        except Exception as e:
            messagebox.showerror("Open failed", str(e))

    def open_config_folder(self):
        self.open_path(str(CONFIG_PATH.parent))

    def save_config_with_popup(self):
        self.save_config()
        messagebox.showinfo("Saved", "Paths saved successfully.")

    def log(self, message):
        self.root.after(0, self._log_main_thread, message)

    def _log_main_thread(self, message):
        self.log_box.insert("end", message + "\n")
        self.log_box.see("end")

    def clear_log(self):
        self.log_box.delete("1.0", "end")

    def set_running(self, running):
        self.is_running = running
        state = "disabled" if running else "normal"

        for button in self.action_buttons:
            button.configure(state=state)

        self.clear_button.configure(state=state)

    def validate_folder(self, path, label):
        if not path:
            messagebox.showerror("Missing path", f"{label} is empty.")
            return None

        folder = Path(path)

        if not folder.exists():
            messagebox.showerror("Invalid path", f"{label} does not exist:\n{folder}")
            return None

        return folder

    def show_done_popup(self, title, lines):
        messagebox.showinfo(title, "\n".join(lines))

    def run_extract(self):
        if self.is_running:
            return

        self.save_config()

        input_root = self.validate_folder(self.dump_dir.get(), "GCM_DUMP folder")
        if input_root is None:
            return

        output_root = Path(self.extract_dir.get())
        output_root.mkdir(parents=True, exist_ok=True)

        thread = threading.Thread(
            target=self.extract_worker,
            args=(input_root, output_root),
            daemon=True
        )
        thread.start()

    def extract_worker(self, input_root, output_root):
        self.root.after(0, self.set_running, True)

        arc_files = list(input_root.rglob("*.arc"))
        total_files = len(arc_files)
        total_extracted = 0
        skipped = 0
        totals_by_category = {}

        self.log(f"[INFO] Found {total_files} .arc files")
        self.root.after(0, self.progress.configure, {"maximum": max(total_files, 1), "value": 0})

        for index, arc_path in enumerate(arc_files, start=1):
            self.log(f"[{index}/{total_files}] {arc_path}")

            try:
                extracted, counts_by_category = extract_j3d_from_arc(
                    arc_path,
                    input_root,
                    output_root,
                    self.log
                )
                total_extracted += extracted

                for category, count in counts_by_category.items():
                    totals_by_category[category] = totals_by_category.get(category, 0) + count

            except Exception as e:
                skipped += 1
                self.log(f"[SKIP] {arc_path} : {e}")

            self.root.after(0, self.progress.configure, {"value": index})

        self.log("")
        self.log(f"[DONE] Extracted {total_extracted} J3D asset file(s)")
        self.log(f"[DONE] Skipped archives: {skipped}")

        summary_lines = [
            "Extraction finished.",
            "",
            f"Archives scanned: {total_files}",
            f"J3D assets extracted: {total_extracted}",
            f"Skipped archives: {skipped}",
        ]

        if totals_by_category:
            summary_lines.append("")
            summary_lines.append("By category:")
            for category, count in sorted(totals_by_category.items()):
                summary_lines.append(f"- {category}: {count}")

        self.root.after(0, self.set_running, False)
        self.root.after(0, self.show_done_popup, "Extraction complete", summary_lines)

    def run_convert(self):
        if self.is_running:
            return

        self.save_config()

        input_root = self.validate_folder(self.extract_dir.get(), "Extracted assets folder")
        if input_root is None:
            return

        superbmd = Path(self.superbmd_path.get())

        if not superbmd.exists():
            messagebox.showerror("Invalid path", f"SuperBMD.exe does not exist:\n{superbmd}")
            return

        output_root = Path(self.dae_dir.get())
        output_root.mkdir(parents=True, exist_ok=True)

        thread = threading.Thread(
            target=self.convert_worker,
            args=(superbmd, input_root, output_root),
            daemon=True
        )
        thread.start()

    def convert_worker(self, superbmd, input_root, output_root):
        self.root.after(0, self.set_running, True)

        models_root = input_root / "models"
        if models_root.exists():
            models = list(models_root.rglob("*.bmd")) + list(models_root.rglob("*.bdl"))
        else:
            models = list(input_root.rglob("*.bmd")) + list(input_root.rglob("*.bdl"))
        total_models = len(models)

        success = 0
        failed = 0

        self.log(f"[INFO] Found {total_models} model files")
        self.root.after(0, self.progress.configure, {"maximum": max(total_models, 1), "value": 0})

        for index, model_path in enumerate(models, start=1):
            self.log(f"[{index}/{total_models}] {model_path}")

            try:
                ok = convert_model(
                    superbmd,
                    model_path,
                    input_root,
                    output_root,
                    self.log
                )

                if ok:
                    success += 1
                else:
                    failed += 1

            except Exception as e:
                failed += 1
                self.log(f"[FAIL] {model_path} : {e}")

            self.root.after(0, self.progress.configure, {"value": index})

        self.log("")
        self.log(f"[DONE] Converted: {success}")
        self.log(f"[DONE] Failed: {failed}")

        summary_lines = [
            "Model conversion finished.",
            "",
            f"Models found: {total_models}",
            f"Converted to DAE: {success}",
            f"Failed: {failed}",
        ]

        self.root.after(0, self.set_running, False)
        self.root.after(0, self.show_done_popup, "Model conversion complete", summary_lines)

    def run_okmat_convert(self):
        if self.is_running:
            return

        self.save_config()

        input_root = self.validate_folder(self.extract_dir.get(), "Extracted assets folder")
        if input_root is None:
            return

        output_root = Path(self.okmat_dir.get())
        output_root.mkdir(parents=True, exist_ok=True)

        thread = threading.Thread(
            target=self.okmat_convert_worker,
            args=(input_root, output_root),
            daemon=True
        )
        thread.start()

    def okmat_convert_worker(self, input_root, output_root):
        self.root.after(0, self.set_running, True)

        models_root = input_root / "models"
        if models_root.exists():
            models = list(models_root.rglob("*.bmd")) + list(models_root.rglob("*.bdl"))
        else:
            models = list(input_root.rglob("*.bmd")) + list(input_root.rglob("*.bdl"))
        total_models = len(models)

        success = 0
        failed = 0

        self.log(f"[INFO] Found {total_models} model files for OKMAT generation")
        self.root.after(0, self.progress.configure, {"maximum": max(total_models, 1), "value": 0})

        for index, model_path in enumerate(models, start=1):
            self.log(f"[{index}/{total_models}] {model_path}")

            try:
                ok = convert_model_to_okmat(
                    model_path,
                    input_root,
                    output_root,
                    self.log
                )

                if ok:
                    success += 1
                else:
                    failed += 1

            except Exception as e:
                failed += 1
                self.log(f"[FAIL] {model_path} : {e}")

            self.root.after(0, self.progress.configure, {"value": index})

        self.log("")
        self.log(f"[DONE] OKMAT generated: {success}")
        self.log(f"[DONE] Failed: {failed}")

        summary_lines = [
            "OKMAT generation finished.",
            "",
            f"Models found: {total_models}",
            f"OKMAT generated: {success}",
            f"Failed: {failed}",
        ]

        self.root.after(0, self.set_running, False)
        self.root.after(0, self.show_done_popup, "OKMAT generation complete", summary_lines)


    def run_fbx_convert(self):
        if self.is_running:
            return

        self.save_config()

        input_root = self.validate_folder(self.dae_dir.get(), "DAE output folder")
        if input_root is None:
            return

        blender = Path(self.blender_path.get())

        if not blender.exists():
            messagebox.showerror("Invalid path", f"Blender.exe does not exist:\n{blender}")
            return

        output_root = Path(self.fbx_dir.get())
        output_root.mkdir(parents=True, exist_ok=True)

        thread = threading.Thread(
            target=self.fbx_convert_worker,
            args=(blender, input_root, output_root),
            daemon=True
        )
        thread.start()

    def fbx_convert_worker(self, blender, input_root, output_root):
        self.root.after(0, self.set_running, True)

        dae_files = list(input_root.rglob("*.dae"))
        total = len(dae_files)

        success = 0
        failed = 0

        self.log(f"[INFO] Found {total} .dae files")
        self.root.after(0, self.progress.configure, {"maximum": max(total, 1), "value": 0})

        for index, dae_path in enumerate(dae_files, start=1):
            self.log(f"[{index}/{total}] {dae_path}")

            try:
                ok = convert_dae_to_fbx(
                    blender,
                    dae_path,
                    input_root,
                    output_root,
                    self.log
                )

                if ok:
                    success += 1
                else:
                    failed += 1

            except Exception as e:
                failed += 1
                self.log(f"[FAIL] {dae_path} : {e}")

            self.root.after(0, self.progress.configure, {"value": index})

        self.log("")
        self.log(f"[DONE] FBX converted: {success}")
        self.log(f"[DONE] Failed: {failed}")

        summary_lines = [
            "FBX conversion finished.",
            "",
            f"DAE files found: {total}",
            f"Converted to FBX: {success}",
            f"Failed: {failed}",
        ]

        self.root.after(0, self.set_running, False)
        self.root.after(0, self.show_done_popup, "FBX conversion complete", summary_lines)


def main():
    root = tk.Tk()
    TPAssetImporterGUI(root)
    root.mainloop()


if __name__ == "__main__":
    main()