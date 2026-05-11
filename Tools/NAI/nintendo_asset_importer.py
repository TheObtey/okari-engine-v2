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
# Canonical layout (SuperBMD / J3D): many early state indices are u8,
# while texture/TEV table references are u16 arrays.
MAT3_KNOWN_FLAG_OFFSET = 0x00
MAT3_KNOWN_CULL_MODE_INDEX_OFFSET = 0x01
MAT3_KNOWN_NUM_COLOR_CHAN_INDEX_OFFSET = 0x02
MAT3_KNOWN_NUM_TEX_GENS_INDEX_OFFSET = 0x03
MAT3_KNOWN_TEV_STAGE_COUNT_INDEX_OFFSET = 0x04
MAT3_KNOWN_Z_COMP_LOC_INDEX_OFFSET = 0x05
MAT3_KNOWN_Z_MODE_INDEX_OFFSET = 0x06
MAT3_KNOWN_DITHER_INDEX_OFFSET = 0x07
MAT3_KNOWN_TEXNO_INDEX_OFFSET = 0x84
MAT3_KNOWN_TEV_KONST_COLOR_INDEX_OFFSET = 0x94
MAT3_KNOWN_TEV_KONST_COLOR_SEL_OFFSET = 0x9C
MAT3_KNOWN_TEV_KONST_ALPHA_SEL_OFFSET = 0xAC
MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET = 0xBC
MAT3_KNOWN_TEV_COLOR_INDEX_OFFSET = 0xDC
MAT3_KNOWN_TEV_STAGE_INDEX_OFFSET = 0xE4
MAT3_KNOWN_TEV_SWAP_MODE_INDEX_OFFSET = 0x104
MAT3_KNOWN_TEV_SWAP_MODE_TABLE_INDEX_OFFSET = 0x124
MAT3_KNOWN_FOG_INDEX_OFFSET = 0x144
MAT3_KNOWN_ALPHA_COMPARE_INDEX_OFFSET = 0x146
MAT3_KNOWN_BLEND_MODE_INDEX_OFFSET = 0x148
MAT3_KNOWN_NBT_SCALE_INDEX_OFFSET = 0x14A

# Real J3D TevStage struct is 20 bytes, byte-per-field, not a packed GX BP bitfield.
MAT3_TEV_STAGE_ENTRY_SIZE = 0x14
MAT3_TEV_ORDER_ENTRY_SIZE = 0x04

# MAT3 header table pointers for J3D2 bmd3/bdl4.
# Order matches canonical Mat3OffsetIndex.
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
    "tex_coord2": 0x3C,
    "tex_mtx": 0x40,
    "tex_mtx2": 0x44,
    "tex_no": 0x48,
    "tev_order": 0x4C,
    "tev_color": 0x50,
    "tev_konst_color": 0x54,
    "tev_stage_count": 0x58,
    "tev_stage": 0x5C,
    "tev_swap_mode": 0x60,
    "tev_swap_mode_table": 0x64,
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
GX_TEV_COLOR_ARGS = {0: "cprev", 1: "aprev", 2: "c0", 3: "a0", 4: "c1", 5: "a1", 6: "c2", 7: "a2", 8: "texc", 9: "texa", 10: "rasc", 11: "rasa", 12: "one", 13: "half", 14: "konst", 15: "zero"}
GX_TEV_ALPHA_ARGS = {0: "aprev", 1: "a0", 2: "a1", 3: "a2", 4: "texa", 5: "rasa", 6: "konst", 7: "zero"}
GX_TEV_OPS = {0: "add", 1: "sub", 8: "comp_r8_gt", 9: "comp_r8_eq", 10: "comp_gr16_gt", 11: "comp_gr16_eq", 12: "comp_bgr24_gt", 13: "comp_bgr24_eq", 14: "comp_rgb8_gt", 15: "comp_rgb8_eq"}
GX_TEV_BIASES = {0: "zero", 1: "add_half", 2: "sub_half"}
GX_TEV_SCALES = {0: "scale_1", 1: "scale_2", 2: "scale_4", 3: "divide_2"}
GX_TEV_REGS = {0: "prev", 1: "reg0", 2: "reg1", 3: "reg2"}


def enum_name(table, value):
    return table.get(value, f"unknown_{value}")


def get_mat3_table_offset(data, mat3_offset, table_name):
    field_offset = MAT3_TABLE_FIELDS.get(table_name)
    if field_offset is None:
        return None
    return get_chunk_relative_offset(data, mat3_offset, field_offset)


def format_hex(value):
    if value is None:
        return None
    return f"0x{value:X}"


def validate_mat3_header_offsets(data, mat3_offset, mat3_size):
    chunk_end = min(mat3_offset + mat3_size if mat3_size else len(data), len(data))
    warnings = []
    errors = []
    tables = {}

    if mat3_offset < 0 or mat3_offset + 8 > len(data):
        return {
            "ok": False,
            "errors": ["MAT3 offset is outside file bounds."],
            "warnings": warnings,
            "tables": tables,
        }

    tag = data[mat3_offset:mat3_offset + 4].decode("ascii", errors="replace")
    if tag != "MAT3":
        errors.append(f"Expected MAT3 tag, got {tag!r}.")

    if mat3_size <= 0:
        errors.append("MAT3 chunk size is null or invalid.")
    elif mat3_offset + mat3_size > len(data):
        errors.append("MAT3 chunk end is outside file bounds.")

    fields_by_header_offset = {}
    for table_name, header_field_offset in MAT3_TABLE_FIELDS.items():
        fields_by_header_offset.setdefault(header_field_offset, []).append(table_name)

    for header_field_offset, table_names in sorted(fields_by_header_offset.items()):
        if len(table_names) > 1:
            warnings.append(
                "MAT3_TABLE_FIELDS has multiple table names using the same "
                f"header field 0x{header_field_offset:X}: {', '.join(table_names)}"
            )

    for table_name, header_field_offset in sorted(MAT3_TABLE_FIELDS.items(), key=lambda item: item[1]):
        header_absolute_offset = mat3_offset + header_field_offset
        table_info = {
            "header_field_offset": format_hex(header_field_offset),
            "header_absolute_offset": format_hex(header_absolute_offset),
            "relative_offset": None,
            "absolute_offset": None,
            "status": "missing",
        }

        if header_absolute_offset + 4 > len(data):
            table_info["status"] = "header_out_of_file"
            errors.append(f"{table_name}: header field is outside file bounds.")
            tables[table_name] = table_info
            continue

        relative_offset = safe_u32(data, header_absolute_offset, 0)
        table_info["relative_offset"] = format_hex(relative_offset)

        if relative_offset == 0:
            tables[table_name] = table_info
            continue

        absolute_offset = mat3_offset + relative_offset
        table_info["absolute_offset"] = format_hex(absolute_offset)

        if relative_offset < 0x84:
            table_info["status"] = "suspicious_header_area"
            warnings.append(f"{table_name}: relative offset 0x{relative_offset:X} points inside the MAT3 header area.")
        elif absolute_offset < mat3_offset or absolute_offset >= chunk_end:
            table_info["status"] = "out_of_mat3_chunk"
            errors.append(f"{table_name}: relative offset 0x{relative_offset:X} points outside the MAT3 chunk.")
        else:
            table_info["status"] = "ok"

        tables[table_name] = table_info

    offsets_to_tables = {}
    for table_name, table_info in tables.items():
        relative_offset = table_info.get("relative_offset")
        if relative_offset is None or relative_offset == "0x0":
            continue
        offsets_to_tables.setdefault(relative_offset, []).append(table_name)

    for relative_offset, table_names in sorted(offsets_to_tables.items()):
        if len(table_names) > 1:
            warnings.append(f"Multiple MAT3 tables resolve to {relative_offset}: {', '.join(table_names)}")

    return {
        "ok": len(errors) == 0,
        "tag": tag,
        "chunk_offset": format_hex(mat3_offset),
        "chunk_size": format_hex(mat3_size),
        "chunk_end": format_hex(chunk_end),
        "errors": errors,
        "warnings": warnings,
        "tables": tables,
    }


def get_mat3_table_range(data, mat3_offset, mat3_size, table_name):
    table_offset = get_mat3_table_offset(data, mat3_offset, table_name)
    if table_offset is None:
        return None, None

    chunk_end = min(mat3_offset + mat3_size if mat3_size else len(data), len(data))
    relative_offset = table_offset - mat3_offset
    next_relative_offset = None

    seen = set()
    for other_name in MAT3_TABLE_FIELDS.keys():
        other_offset = get_mat3_table_offset(data, mat3_offset, other_name)
        if other_offset is None or other_offset in seen:
            continue
        seen.add(other_offset)
        other_relative_offset = other_offset - mat3_offset
        if other_relative_offset > relative_offset:
            if next_relative_offset is None or other_relative_offset < next_relative_offset:
                next_relative_offset = other_relative_offset

    table_end = mat3_offset + next_relative_offset if next_relative_offset is not None else chunk_end
    table_end = min(table_end, chunk_end, len(data))
    return table_offset, table_end


def get_mat3_table_entry_count(data, mat3_offset, mat3_size, table_name, entry_size):
    table_offset, table_end = get_mat3_table_range(data, mat3_offset, mat3_size, table_name)
    if table_offset is None or table_end is None or entry_size <= 0 or table_end <= table_offset:
        return 0
    return (table_end - table_offset) // entry_size


def clamp_tev_stage_count(stage_count):
    if stage_count is None:
        return 1
    try:
        return max(0, min(int(stage_count), 16))
    except (TypeError, ValueError):
        return 1


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


def parse_tev_stage_count_from_material_entry(data, material_entry_offset, mat3_offset):
    table_offset = get_mat3_table_offset(data, mat3_offset, "tev_stage_count")
    fallback = {
        "index": None,
        "count": 1,
        "raw": None,
        "used_fallback": True,
        "fallback_reason": "missing_or_invalid_stage_count",
        "j3d": {
            "tev_stage_count_index_offset": f"0x{MAT3_KNOWN_TEV_STAGE_COUNT_INDEX_OFFSET:02X}",
            "tev_stage_count_table_relative_offset": f"0x{table_offset - mat3_offset:X}" if table_offset is not None else None,
        },
    }

    if table_offset is None or material_entry_offset is None:
        return fallback

    index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_TEV_STAGE_COUNT_INDEX_OFFSET)
    if index is None:
        return fallback

    raw_offset = table_offset + index
    if raw_offset < 0 or raw_offset >= len(data):
        fallback["index"] = index
        fallback["fallback_reason"] = "stage_count_raw_out_of_bounds"
        return fallback

    raw_count = u8(data, raw_offset)
    if raw_count == 0xFF:
        fallback.update({"index": index, "raw": "FF", "fallback_reason": "invalid_0xFF"})
        return fallback

    count = clamp_tev_stage_count(raw_count)
    return {
        "index": index,
        "count": count,
        "declared_count": raw_count,
        "raw": f"{raw_count:02X}",
        "used_fallback": False,
        "j3d": {
            "tev_stage_count_index_offset": f"0x{MAT3_KNOWN_TEV_STAGE_COUNT_INDEX_OFFSET:02X}",
            "tev_stage_count_table_relative_offset": f"0x{table_offset - mat3_offset:X}",
        },
    }


def parse_tev_orders_from_material_entry(data, material_entry_offset, mat3_offset, mat3_size, texture_names, stage_count, material_textures=None):
    tev_orders = []
    invalid_orders = []
    table_offset, table_end = get_mat3_table_range(data, mat3_offset, mat3_size, "tev_order")

    if table_offset is None or material_entry_offset is None:
        return tev_orders, invalid_orders

    max_order_count = max(0, (table_end - table_offset) // 4) if table_end is not None else 0
    stage_count = clamp_tev_stage_count(stage_count)

    for stage in range(stage_count):
        index_offset = material_entry_offset + MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET + stage * 2
        tev_order_index = read_u16_index(data, index_offset)
        if tev_order_index is None:
            invalid_orders.append({"stage": stage, "index": None, "reason": "invalid_index", "raw": data[index_offset:index_offset + 2].hex().upper() if index_offset + 2 <= len(data) else None})
            continue
        if tev_order_index >= max_order_count:
            invalid_orders.append({"stage": stage, "index": tev_order_index, "reason": "out_of_tev_order_table", "max_valid_index": max_order_count - 1 if max_order_count > 0 else None})
            continue

        raw_offset = table_offset + tev_order_index * 4
        raw = data[raw_offset:raw_offset + 4] if raw_offset + 4 <= len(data) else b""
        if len(raw) < 4:
            invalid_orders.append({"stage": stage, "index": tev_order_index, "reason": "raw_out_of_bounds"})
            continue

        tex_coord = raw[0]
        tex_map = raw[1]
        color_chan = raw[2]

        # tex_map is a per-material texture slot (0..7), not a direct TEX1 index.
        # Resolve the human-readable name through material.textures[slot].
        texture_name = None
        texture_index = None
        if tex_map != 0xFF and material_textures is not None:
            for texture in material_textures:
                if texture.get("slot") == tex_map:
                    texture_name = texture.get("name")
                    texture_index = texture.get("index")
                    break
        elif tex_map != 0xFF and tex_map < len(texture_names):
            texture_name = texture_names[tex_map]
            texture_index = tex_map

        tev_orders.append({
            "stage": stage,
            "index": tev_order_index,
            "tex_coord": tex_coord if tex_coord != 0xFF else None,
            "tex_map": tex_map if tex_map != 0xFF else None,
            "texture_slot": tex_map if tex_map != 0xFF else None,
            "texture_index": texture_index,
            "texture": texture_name,
            "color_channel": color_chan if color_chan != 0xFF else None,
            "raw": raw.hex().upper(),
            "j3d": {
                "tev_order_index_offset": f"0x{MAT3_KNOWN_TEV_ORDER_INDEX_OFFSET:02X}",
                "tev_order_table_relative_offset": f"0x{table_offset - mat3_offset:X}",
                "tev_order_table_entry_count": max_order_count,
            },
        })

    return tev_orders, invalid_orders


def parse_tev_stage_value(raw, stage, tev_stage_index, raw_offset, mat3_offset):
    raw = bytes(raw)
    if len(raw) < MAT3_TEV_STAGE_ENTRY_SIZE:
        return {"stage": stage, "index": tev_stage_index, "raw": raw.hex().upper(), "error": "raw_too_short"}

    unknown0 = raw[0]
    color_in_a, color_in_b, color_in_c, color_in_d = raw[1], raw[2], raw[3], raw[4]
    color_op, color_bias, color_scale, color_clamp, color_reg = raw[5], raw[6], raw[7], raw[8], raw[9]
    alpha_in_a, alpha_in_b, alpha_in_c, alpha_in_d = raw[10], raw[11], raw[12], raw[13]
    alpha_op, alpha_bias, alpha_scale, alpha_clamp, alpha_reg = raw[14], raw[15], raw[16], raw[17], raw[18]
    unknown1 = raw[19]

    color = {
        "a_value": color_in_a, "a": enum_name(GX_TEV_COLOR_ARGS, color_in_a),
        "b_value": color_in_b, "b": enum_name(GX_TEV_COLOR_ARGS, color_in_b),
        "c_value": color_in_c, "c": enum_name(GX_TEV_COLOR_ARGS, color_in_c),
        "d_value": color_in_d, "d": enum_name(GX_TEV_COLOR_ARGS, color_in_d),
        "op_value": color_op, "op": enum_name(GX_TEV_OPS, color_op),
        "bias_value": color_bias, "bias": enum_name(GX_TEV_BIASES, color_bias),
        "scale_value": color_scale, "scale": enum_name(GX_TEV_SCALES, color_scale),
        "clamp_value": color_clamp, "clamp": bool(color_clamp),
        "reg_value": color_reg, "reg": enum_name(GX_TEV_REGS, color_reg),
    }
    alpha = {
        "a_value": alpha_in_a, "a": enum_name(GX_TEV_ALPHA_ARGS, alpha_in_a),
        "b_value": alpha_in_b, "b": enum_name(GX_TEV_ALPHA_ARGS, alpha_in_b),
        "c_value": alpha_in_c, "c": enum_name(GX_TEV_ALPHA_ARGS, alpha_in_c),
        "d_value": alpha_in_d, "d": enum_name(GX_TEV_ALPHA_ARGS, alpha_in_d),
        "op_value": alpha_op, "op": enum_name(GX_TEV_OPS, alpha_op),
        "bias_value": alpha_bias, "bias": enum_name(GX_TEV_BIASES, alpha_bias),
        "scale_value": alpha_scale, "scale": enum_name(GX_TEV_SCALES, alpha_scale),
        "clamp_value": alpha_clamp, "clamp": bool(alpha_clamp),
        "reg_value": alpha_reg, "reg": enum_name(GX_TEV_REGS, alpha_reg),
    }
    sanity = {
        "unknown0": f"0x{unknown0:02X}",
        "unknown1": f"0x{unknown1:02X}",
        "unknown0_is_ff": unknown0 == 0xFF,
        "unknown1_is_ff": unknown1 == 0xFF,
        "color_in_in_range": all(v < 16 for v in (color_in_a, color_in_b, color_in_c, color_in_d)),
        "alpha_in_in_range": all(v < 8 for v in (alpha_in_a, alpha_in_b, alpha_in_c, alpha_in_d)),
        "color_reg_in_range": color_reg < 4,
        "alpha_reg_in_range": alpha_reg < 4,
        "color_clamp_is_bool": color_clamp in (0, 1),
        "alpha_clamp_is_bool": alpha_clamp in (0, 1),
    }
    sanity["all_fields_plausible"] = all(v for v in sanity.values() if isinstance(v, bool))

    return {
        "stage": stage,
        "index": tev_stage_index,
        "raw": raw.hex().upper(),
        "entry_size": MAT3_TEV_STAGE_ENTRY_SIZE,
        "bytes": [f"{value:02X}" for value in raw],
        "decode_status": "j3d_tev_stage_byte_per_field_layout",
        "color": color,
        "alpha": alpha,
        "sanity": sanity,
        "j3d": {
            "tev_stage_index_offset": f"0x{MAT3_KNOWN_TEV_STAGE_INDEX_OFFSET:02X}",
            "tev_stage_table_relative_offset": None,
            "raw_relative_offset": f"0x{raw_offset - mat3_offset:X}",
        },
    }

def parse_tev_stages_from_material_entry(data, material_entry_offset, mat3_offset, mat3_size, stage_count):
    tev_stages = []
    invalid_stages = []
    table_offset, table_end = get_mat3_table_range(data, mat3_offset, mat3_size, "tev_stage")
    if table_offset is None or material_entry_offset is None:
        return tev_stages, invalid_stages

    max_stage_count = max(0, (table_end - table_offset) // MAT3_TEV_STAGE_ENTRY_SIZE) if table_end is not None else 0
    stage_count = clamp_tev_stage_count(stage_count)

    for stage in range(stage_count):
        index_offset = material_entry_offset + MAT3_KNOWN_TEV_STAGE_INDEX_OFFSET + stage * 2
        tev_stage_index = read_u16_index(data, index_offset)
        if tev_stage_index is None:
            invalid_stages.append({"stage": stage, "index": None, "reason": "invalid_index", "raw": data[index_offset:index_offset + 2].hex().upper() if index_offset + 2 <= len(data) else None})
            continue
        if tev_stage_index >= max_stage_count:
            invalid_stages.append({"stage": stage, "index": tev_stage_index, "reason": "out_of_tev_stage_table", "max_valid_index": max_stage_count - 1 if max_stage_count > 0 else None})
            continue

        raw_offset = table_offset + tev_stage_index * MAT3_TEV_STAGE_ENTRY_SIZE
        raw = data[raw_offset:raw_offset + MAT3_TEV_STAGE_ENTRY_SIZE] if raw_offset + MAT3_TEV_STAGE_ENTRY_SIZE <= len(data) else b""
        if len(raw) < MAT3_TEV_STAGE_ENTRY_SIZE:
            invalid_stages.append({"stage": stage, "index": tev_stage_index, "reason": "raw_out_of_bounds"})
            continue

        parsed = parse_tev_stage_value(raw, stage, tev_stage_index, raw_offset, mat3_offset)
        parsed["j3d"]["tev_stage_table_relative_offset"] = f"0x{table_offset - mat3_offset:X}"
        parsed["j3d"]["tev_stage_table_entry_count"] = max_stage_count
        tev_stages.append(parsed)

    return tev_stages, invalid_stages





def parse_color16_rgba(data, offset):
    if offset < 0 or offset + 8 > len(data):
        return None

    r = safe_u16(data, offset)
    g = safe_u16(data, offset + 2)
    b = safe_u16(data, offset + 4)
    a = safe_u16(data, offset + 6)

    return {
        "r": r,
        "g": g,
        "b": b,
        "a": a,
        "normalized": {
            "r": round(r / 255.0, 6),
            "g": round(g / 255.0, 6),
            "b": round(b / 255.0, 6),
            "a": round(a / 255.0, 6),
        },
        "raw": f"{r:04X}{g:04X}{b:04X}{a:04X}",
        "storage": "rgba16",
    }


def parse_color8_rgba(data, offset):
    if offset < 0 or offset + 4 > len(data):
        return None

    r = u8(data, offset)
    g = u8(data, offset + 1)
    b = u8(data, offset + 2)
    a = u8(data, offset + 3)

    return {
        "r": r,
        "g": g,
        "b": b,
        "a": a,
        "normalized": {
            "r": round(r / 255.0, 6),
            "g": round(g / 255.0, 6),
            "b": round(b / 255.0, 6),
            "a": round(a / 255.0, 6),
        },
        "raw": f"{r:02X}{g:02X}{b:02X}{a:02X}",
        "storage": "rgba8",
    }


def parse_tev_colors_from_material_entry(data, material_entry_offset, mat3_offset):
    tev_colors = []
    table_offset = get_mat3_table_offset(data, mat3_offset, "tev_color")

    if table_offset is None or material_entry_offset is None:
        return tev_colors

    for i in range(4):
        index = read_u16_index(
            data,
            material_entry_offset + MAT3_KNOWN_TEV_COLOR_INDEX_OFFSET + i * 2
        )

        if index is None:
            tev_colors.append(None)
            continue

        raw_offset = table_offset + index * 8
        color = parse_color16_rgba(data, raw_offset)

        tev_colors.append({
            "index": index,
            "color": color,
            "j3d": {
                "tev_color_index_offset": f"0x{MAT3_KNOWN_TEV_COLOR_INDEX_OFFSET + i * 2:X}",
                "tev_color_table_relative_offset": f"0x{table_offset - mat3_offset:X}",
            },
        })

    return tev_colors


def parse_tev_konst_colors_from_material_entry(data, material_entry_offset, mat3_offset):
    konst_colors = []
    table_offset = get_mat3_table_offset(data, mat3_offset, "tev_konst_color")

    if table_offset is None or material_entry_offset is None:
        return konst_colors

    for i in range(4):
        index = read_u16_index(
            data,
            material_entry_offset + MAT3_KNOWN_TEV_KONST_COLOR_INDEX_OFFSET + i * 2
        )

        if index is None:
            konst_colors.append(None)
            continue

        # J3D TevKColor / konst color table stores GXColor as 4 bytes RGBA8,
        # unlike tev_color/c0-c2 registers which use 8-byte RGBA16.
        raw_offset = table_offset + index * 4
        color = parse_color8_rgba(data, raw_offset)

        konst_colors.append({
            "index": index,
            "color": color,
            "j3d": {
                "tev_konst_color_index_offset": f"0x{MAT3_KNOWN_TEV_KONST_COLOR_INDEX_OFFSET + i * 2:X}",
                "tev_konst_color_table_relative_offset": f"0x{table_offset - mat3_offset:X}",
                "tev_konst_color_entry_size": 4,
            },
        })

    return konst_colors


def parse_tev_konst_selectors_from_material_entry(data, material_entry_offset):
    color_selectors = []
    alpha_selectors = []

    if material_entry_offset is None:
        return {
            "color": color_selectors,
            "alpha": alpha_selectors,
        }

    for stage in range(16):
        color_sel = u8(data, material_entry_offset + MAT3_KNOWN_TEV_KONST_COLOR_SEL_OFFSET + stage)
        alpha_sel = u8(data, material_entry_offset + MAT3_KNOWN_TEV_KONST_ALPHA_SEL_OFFSET + stage)

        color_selectors.append(color_sel if color_sel != 0xFF else None)
        alpha_selectors.append(alpha_sel if alpha_sel != 0xFF else None)

    return {
        "color": color_selectors,
        "alpha": alpha_selectors,
    }


def parse_material_gx_state(data, mat3_offset, material_entry_offset):
    cull_mode_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_CULL_MODE_INDEX_OFFSET)
    z_comp_loc_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_Z_COMP_LOC_INDEX_OFFSET)
    z_mode_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_Z_MODE_INDEX_OFFSET)
    dither_index = read_u8_index(data, material_entry_offset + MAT3_KNOWN_DITHER_INDEX_OFFSET)
    alpha_compare_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_ALPHA_COMPARE_INDEX_OFFSET)
    blend_mode_index = read_u16_index(data, material_entry_offset + MAT3_KNOWN_BLEND_MODE_INDEX_OFFSET)

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

    mat3_header_validation = validate_mat3_header_offsets(data, mat3_offset, mat3_size)
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
        material_entry_offsets.append(material_init_table + material_init_index * MAT3_MATERIAL_INIT_SIZE if material_init_table is not None else None)

    materials = []
    for index in range(material_count):
        material_init_index = material_init_indices[index]
        material_entry_offset = material_entry_offsets[index]
        name = names[index] if index < len(names) else f"material_{index}"

        material_textures = []
        tev_stage_count = None
        tev_orders = []
        invalid_tev_orders = []
        tev_stages = []
        invalid_tev_stages = []
        tev_colors = []
        tev_konst_colors = []
        tev_konst_selectors = {}
        gx_state = None

        if material_entry_offset is not None:
            material_textures = resolve_textures_from_material_entry(data, material_entry_offset, mat3_offset, texture_names)
            tev_stage_count = parse_tev_stage_count_from_material_entry(data, material_entry_offset, mat3_offset)
            tev_orders, invalid_tev_orders = parse_tev_orders_from_material_entry(
                data, material_entry_offset, mat3_offset, mat3_size, texture_names, tev_stage_count.get("count", 1), material_textures
            )
            effective_tev_stage_count = len(tev_orders)
            if effective_tev_stage_count == 0 and tev_stage_count.get("count", 1) > 0:
                effective_tev_stage_count = tev_stage_count.get("count", 1)
            tev_stages, invalid_tev_stages = parse_tev_stages_from_material_entry(
                data, material_entry_offset, mat3_offset, mat3_size, effective_tev_stage_count
            )

            tev_colors = parse_tev_colors_from_material_entry(
                data, material_entry_offset, mat3_offset
            )

            tev_konst_colors = parse_tev_konst_colors_from_material_entry(
                data, material_entry_offset, mat3_offset
            )

            tev_konst_selectors = parse_tev_konst_selectors_from_material_entry(
                data, material_entry_offset
            )
            if invalid_tev_orders:
                tev_stage_count["invalid_order_count"] = len(invalid_tev_orders)
            if invalid_tev_stages:
                tev_stage_count["invalid_stage_count"] = len(invalid_tev_stages)
            gx_state = parse_material_gx_state(data, mat3_offset, material_entry_offset)

        if tev_stage_count is None:
            tev_stage_count = {"index": None, "count": 1, "raw": None, "used_fallback": True}

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
        declared_stage_count = tev_stage_count.get("count", 1)
        effective_stage_count = len(tev_orders)
        if effective_stage_count == 0 and declared_stage_count > 0:
            effective_stage_count = declared_stage_count

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
                "func": gx_state["z_mode"].get("func", "lequal"),
            },
            "render_queue": render_queue,
            "tev": {
                "stage_count": effective_stage_count,
                "declared_stage_count": declared_stage_count,
                "orders": tev_orders,
                "stages": tev_stages,
                "colors": tev_colors,
                "konst_colors": tev_konst_colors,
                "konst_selectors": tev_konst_selectors,
            },
            "j3d": {
                "mat3_index": index,
                "material_init_index": material_init_index,
                "material_entry_offset": material_entry_offset - mat3_offset if material_entry_offset is not None else None,
                "gx": gx_state,
                "tev_stage_count": tev_stage_count,
                "tev_orders": tev_orders,
                "invalid_tev_orders": invalid_tev_orders,
                "tev_stages": tev_stages,
                "invalid_tev_stages": invalid_tev_stages,
            },
        })

    return materials, {
        "material_count": material_count,
        "texture_count": len(textures),
        "material_init_table_relative_offset": f"0x{material_init_table - mat3_offset:X}" if material_init_table is not None else None,
        "material_remap_table_relative_offset": f"0x{material_remap_table - mat3_offset:X}" if material_remap_table is not None else None,
        "tex_no_table_relative_offset": f"0x{get_mat3_table_offset(data, mat3_offset, 'tex_no') - mat3_offset:X}" if get_mat3_table_offset(data, mat3_offset, "tex_no") is not None else None,
        "tev_order_table_relative_offset": f"0x{get_mat3_table_offset(data, mat3_offset, 'tev_order') - mat3_offset:X}" if get_mat3_table_offset(data, mat3_offset, "tev_order") is not None else None,
        "tev_order_table_entry_count": get_mat3_table_entry_count(data, mat3_offset, mat3_size, "tev_order", 4),
        "tev_stage_count_table_entry_count": get_mat3_table_entry_count(data, mat3_offset, mat3_size, "tev_stage_count", 1),
        "tev_stage_table_relative_offset": f"0x{get_mat3_table_offset(data, mat3_offset, 'tev_stage') - mat3_offset:X}" if get_mat3_table_offset(data, mat3_offset, "tev_stage") is not None else None,
        "tev_stage_table_entry_count": get_mat3_table_entry_count(data, mat3_offset, mat3_size, "tev_stage", MAT3_TEV_STAGE_ENTRY_SIZE),
        "tev_stage_entry_size": f"0x{MAT3_TEV_STAGE_ENTRY_SIZE:X}",
        "tev_stage_index_offset": f"0x{MAT3_KNOWN_TEV_STAGE_INDEX_OFFSET:X}",
        "header_validation": mat3_header_validation,
    }

# ---------------------------------------------------------------------------
# KCL PARSER  (Nintendo GameCube / Twilight Princess collision format)
# ---------------------------------------------------------------------------
#
# Twilight Princess GC KCL triangle entry layout (16 bytes, big-endian):
#   bytes  0-3  : f32  length   (world-space triangle height scalar)
#   bytes  4-5  : u16  vertex_index
#   bytes  6-7  : u16  face_normal_index
#   bytes  8-9  : u16  normal_a_index
#   bytes 10-11 : u16  normal_b_index
#   bytes 12-13 : u16  normal_c_index
#   bytes 14-15 : u16  attribute  (surface material + behaviour flags)
# ---------------------------------------------------------------------------

KCL_HEADER_SIZE         = 0x40
KCL_TRIANGLE_ENTRY_SIZE = 0x10
KCL_VERTEX_ENTRY_SIZE   = 0x0C
KCL_NORMAL_ENTRY_SIZE   = 0x0C

KCL_SURFACE_TYPES = {
    0x00: "default",
    0x01: "stone",
    0x02: "grass",
    0x03: "sand",
    0x04: "water",
    0x05: "lava",
    0x06: "snow",
    0x07: "ice",
    0x08: "dirt",
    0x09: "wood",
    0x0A: "metal",
    0x0B: "climbable",
    0x0C: "trigger",
    0x0D: "void",
    0x0E: "swamp",
    0x0F: "shallow_water",
    0x10: "carpet",
    0x11: "bridge",
    0x12: "fence",
    0x13: "steep_slope",
}

KCL_ATTRIBUTE_FLAGS = {
    0x0020: "death_barrier",
    0x0040: "no_walk",
    0x0080: "no_collision",
    0x0100: "camera_through",
    0x0200: "hookshot",
    0x0400: "grabbable",
    0x0800: "trigger",
}


def f32(data, offset):
    return struct.unpack_from(">f", data, offset)[0]


def read_kcl_vec3(data, offset):
    if offset < 0 or offset + 12 > len(data):
        return None
    return (f32(data, offset), f32(data, offset + 4), f32(data, offset + 8))


def vec3_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vec3_scale(v, s):
    return (v[0] * s, v[1] * s, v[2] * s)


def vec3_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def vec3_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def reconstruct_triangle_vertices(base_vertex, face_normal, normal_a, normal_b, normal_c, length):
    try:
        edge_a = vec3_cross(normal_a, face_normal)
        edge_b = vec3_cross(normal_b, face_normal)
        denom_a = vec3_dot(edge_a, normal_c)
        denom_b = vec3_dot(edge_b, normal_c)
        if abs(denom_a) < 1e-10 or abs(denom_b) < 1e-10:
            return None
        v0 = base_vertex
        v1 = vec3_add(base_vertex, vec3_scale(edge_b, length / denom_b))
        v2 = vec3_add(base_vertex, vec3_scale(edge_a, length / denom_a))
        return (v0, v1, v2)
    except Exception:
        return None


def decode_kcl_surface_attribute(attribute):
    material_type = attribute & 0x001F
    flags_raw     = attribute & 0xFFE0
    flags = [name for mask, name in KCL_ATTRIBUTE_FLAGS.items() if flags_raw & mask]
    return {
        "raw":           attribute,
        "raw_hex":       "0x{:04X}".format(attribute),
        "material_type": material_type,
        "material_name": KCL_SURFACE_TYPES.get(material_type, "unknown_0x{:02X}".format(material_type)),
        "flags":         flags,
        "passthrough":   bool(flags_raw & 0x0080),
    }


def parse_kcl(data):
    errors   = []
    warnings = []

    if len(data) < KCL_HEADER_SIZE:
        return None, ["File too small for KCL header: {} bytes".format(len(data))]

    vtx_offset = u32(data, 0x00)
    nrm_offset = u32(data, 0x04)
    tri_offset = u32(data, 0x08)
    oct_offset = u32(data, 0x0C)

    if not (vtx_offset < nrm_offset < tri_offset):
        warnings.append(
            "Unexpected KCL section ordering: vtx=0x{:X} nrm=0x{:X} tri=0x{:X}".format(
                vtx_offset, nrm_offset, tri_offset)
        )

    vtx_count = (nrm_offset - vtx_offset) // KCL_VERTEX_ENTRY_SIZE
    vertices  = []
    for i in range(vtx_count):
        v = read_kcl_vec3(data, vtx_offset + i * KCL_VERTEX_ENTRY_SIZE)
        if v is None:
            warnings.append("Vertex {}: read out of bounds".format(i))
            break
        vertices.append(v)

    nrm_count = (tri_offset - nrm_offset) // KCL_NORMAL_ENTRY_SIZE
    normals   = []
    for i in range(nrm_count):
        n = read_kcl_vec3(data, nrm_offset + i * KCL_NORMAL_ENTRY_SIZE)
        if n is None:
            warnings.append("Normal {}: read out of bounds".format(i))
            break
        normals.append(n)

    tri_pool_end    = oct_offset if oct_offset > tri_offset else len(data)
    tri_entry_count = (tri_pool_end - tri_offset) // KCL_TRIANGLE_ENTRY_SIZE

    triangles         = []
    invalid_triangles = []

    for i in range(1, tri_entry_count):
        entry_off = tri_offset + i * KCL_TRIANGLE_ENTRY_SIZE
        if entry_off + KCL_TRIANGLE_ENTRY_SIZE > len(data):
            break

        raw          = data[entry_off:entry_off + KCL_TRIANGLE_ENTRY_SIZE]
        length_float = struct.unpack_from(">f", raw, 0)[0]
        vtx_idx      = struct.unpack_from(">H", raw, 4)[0]
        fnrm_idx     = struct.unpack_from(">H", raw, 6)[0]
        nrm_a_idx    = struct.unpack_from(">H", raw, 8)[0]
        nrm_b_idx    = struct.unpack_from(">H", raw, 10)[0]
        nrm_c_idx    = struct.unpack_from(">H", raw, 12)[0]
        attribute    = struct.unpack_from(">H", raw, 14)[0]

        ok     = True
        reason = None
        if vtx_idx >= len(vertices):
            ok = False
            reason = "vtx_idx {} >= vtx_count {}".format(vtx_idx, len(vertices))
        elif fnrm_idx >= len(normals):
            ok = False
            reason = "face_normal_idx {} >= nrm_count {}".format(fnrm_idx, len(normals))
        elif nrm_a_idx >= len(normals):
            ok = False
            reason = "normal_a_idx {} >= nrm_count {}".format(nrm_a_idx, len(normals))
        elif nrm_b_idx >= len(normals):
            ok = False
            reason = "normal_b_idx {} >= nrm_count {}".format(nrm_b_idx, len(normals))
        elif nrm_c_idx >= len(normals):
            ok = False
            reason = "normal_c_idx {} >= nrm_count {}".format(nrm_c_idx, len(normals))

        if not ok:
            invalid_triangles.append({"index": i, "reason": reason, "raw": raw.hex().upper()})
            continue

        world_verts = reconstruct_triangle_vertices(
            vertices[vtx_idx], normals[fnrm_idx],
            normals[nrm_a_idx], normals[nrm_b_idx], normals[nrm_c_idx],
            length_float
        )

        surface = decode_kcl_surface_attribute(attribute)

        tri = {
            "index":             i,
            "vertex_index":      vtx_idx,
            "face_normal_index": fnrm_idx,
            "normal_a_index":    nrm_a_idx,
            "normal_b_index":    nrm_b_idx,
            "normal_c_index":    nrm_c_idx,
            "length":            round(length_float, 6),
            "attribute":         surface,
            "raw":               raw.hex().upper(),
            "kcl": {
                "base_vertex": list(vertices[vtx_idx]),
                "face_normal": list(normals[fnrm_idx]),
            },
        }

        if world_verts is not None:
            v0, v1, v2 = world_verts
            tri["vertices"]          = [[round(c, 6) for c in vv] for vv in (v0, v1, v2)]
            tri["face_normal_world"] = [round(c, 6) for c in normals[fnrm_idx]]
        else:
            tri["vertices"]      = None
            tri["vertices_error"] = "reconstruction_failed"

        triangles.append(tri)

    surface_type_counts = {}
    for tri in triangles:
        k = tri["attribute"]["material_name"]
        surface_type_counts[k] = surface_type_counts.get(k, 0) + 1

    return {
        "vertices":          [[round(c, 6) for c in v] for v in vertices],
        "normals":           [[round(c, 6) for c in n] for n in normals],
        "triangles":         triangles,
        "invalid_triangles": invalid_triangles,
        "stats": {
            "vertex_count":           len(vertices),
            "normal_count":           len(normals),
            "triangle_count":         len(triangles),
            "invalid_triangle_count": len(invalid_triangles),
            "surface_types":          surface_type_counts,
            "has_world_vertices":     sum(1 for t in triangles if t.get("vertices") is not None),
        },
        "header": {
            "vertex_pool_offset":    "0x{:X}".format(vtx_offset),
            "normal_pool_offset":    "0x{:X}".format(nrm_offset),
            "triangle_array_offset": "0x{:X}".format(tri_offset),
            "spatial_index_offset":  "0x{:X}".format(oct_offset),
        },
        "errors":   errors,
        "warnings": warnings,
    }, errors


def build_okcol_document(kcl_path, data):
    kcl_data, errors = parse_kcl(data)
    if kcl_data is None:
        raise ValueError("KCL parse failed: {}".format("; ".join(errors)))

    triangles_flat = []
    for tri in kcl_data["triangles"]:
        entry = {
            "index":         tri["index"],
            "material":      tri["attribute"]["material_name"],
            "material_type": tri["attribute"]["material_type"],
            "attribute_raw": tri["attribute"]["raw_hex"],
            "flags":         tri["attribute"]["flags"],
            "passthrough":   tri["attribute"]["passthrough"],
        }
        if tri.get("vertices") is not None:
            entry["v0"]     = tri["vertices"][0]
            entry["v1"]     = tri["vertices"][1]
            entry["v2"]     = tri["vertices"][2]
            entry["normal"] = tri["face_normal_world"]
        else:
            entry["error"] = tri.get("vertices_error", "unknown")
        triangles_flat.append(entry)

    return {
        "format":  "okcol",
        "version": 1,
        "source":  {"file": kcl_path.name, "type": "kcl", "size": len(data)},
        "collision": {"triangles": triangles_flat},
        "stats": kcl_data["stats"],
        "debug": {
            "header":            kcl_data["header"],
            "warnings":          kcl_data["warnings"],
            "errors":            kcl_data["errors"],
            "invalid_triangles": kcl_data["invalid_triangles"],
        },
    }


def convert_kcl_to_okcol(kcl_path, input_root, output_root, log):
    relative   = kcl_path.relative_to(input_root)
    output_dir = output_root / relative.parent
    output_dir.mkdir(parents=True, exist_ok=True)

    raw      = kcl_path.read_bytes()
    data     = yaz0_decompress(raw)
    document = build_okcol_document(kcl_path, data)

    output_path = output_dir / (kcl_path.stem + ".okcol")
    output_path.write_text(json.dumps(document, indent=4, ensure_ascii=False), encoding="utf-8")

    s = document["stats"]
    log("[OKCOL] {} -> {} ({} triangles, {} with vertices, {} invalid)".format(
        kcl_path, output_path,
        s["triangle_count"], s["has_world_vertices"], s["invalid_triangle_count"]))
    return True




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
        self.okcol_dir = tk.StringVar(value="okcol")

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
        self.okcol_dir.set(config.get("okcol_dir", self.okcol_dir.get()))

    def save_config(self):
        config = {
            "dump_dir": self.dump_dir.get(),
            "extract_dir": self.extract_dir.get(),
            "dae_dir": self.dae_dir.get(),
            "superbmd_path": self.superbmd_path.get(),
            "fbx_dir": self.fbx_dir.get(),
            "blender_path": self.blender_path.get(),
            "okmat_dir": self.okmat_dir.get(),
            "okcol_dir": self.okcol_dir.get(),
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
        self.build_kcl_tab()
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


    def build_kcl_tab(self):
        tab = self.register_tab("kcl", "KCL to OKCOL")
        scroll_frame = self.make_scrollable(tab)

        self.create_info_card(
            scroll_frame,
            "Convert Nintendo collision meshes to OKCOL",
            "Parse extracted .kcl files (Nintendo GameCube collision format) and generate .okcol JSON files. "
            "Each .okcol contains reconstructed triangle vertices, face normals, surface material types and "
            "behaviour flags. Use these files for debug rendering in Okari Engine to verify collision geometry "
            "alignment against your .fbx visual meshes."
        )

        self.create_path_row(
            scroll_frame,
            "KCL input folder",
            "Root folder containing extracted .kcl files. The converter searches all subfolders recursively.",
            self.extract_dir,
            self.select_extract_folder,
            1
        )
        self.create_path_row(
            scroll_frame,
            "OKCOL output",
            "Destination folder for generated .okcol collision files.",
            self.okcol_dir,
            self.select_okcol_folder,
            2
        )

        actions = tk.Frame(scroll_frame, bg=self.colors["surface"])
        actions.grid(row=3, column=0, sticky="ew", pady=(16, 0))
        self.kcl_button = ttk.Button(
            actions,
            text="Generate OKCOL",
            style="Primary.TButton",
            command=self.run_kcl_convert
        )
        self.kcl_button.pack(side="left")
        ttk.Label(
            actions,
            text="Reads .kcl files. Outputs runtime-ready collision triangles for Okari Engine.",
            style="SectionText.TLabel"
        ).pack(side="left", padx=(12, 0))
        self.action_buttons.append(self.kcl_button)

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

    def select_okcol_folder(self):
        self.select_directory("Select OKCOL output folder", self.okcol_dir)


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


    def run_kcl_convert(self):
        if self.is_running:
            return

        self.save_config()

        input_root = self.validate_folder(self.extract_dir.get(), "KCL input folder")
        if input_root is None:
            return

        output_root = Path(self.okcol_dir.get())
        output_root.mkdir(parents=True, exist_ok=True)

        thread = threading.Thread(
            target=self.kcl_convert_worker,
            args=(input_root, output_root),
            daemon=True
        )
        thread.start()

    def kcl_convert_worker(self, input_root, output_root):
        self.root.after(0, self.set_running, True)

        kcl_files = list(input_root.rglob("*.kcl"))
        total     = len(kcl_files)
        success   = 0
        failed    = 0

        self.log("[INFO] Found {} .kcl files".format(total))
        self.root.after(0, self.progress.configure, {"maximum": max(total, 1), "value": 0})

        for index, kcl_path in enumerate(kcl_files, start=1):
            self.log("[{}/{}] {}".format(index, total, kcl_path))
            try:
                ok = convert_kcl_to_okcol(kcl_path, input_root, output_root, self.log)
                if ok:
                    success += 1
                else:
                    failed += 1
            except Exception as e:
                failed += 1
                self.log("[FAIL] {} : {}".format(kcl_path, e))
            self.root.after(0, self.progress.configure, {"value": index})

        self.log("")
        self.log("[DONE] OKCOL generated: {}".format(success))
        self.log("[DONE] Failed: {}".format(failed))

        self.root.after(0, self.set_running, False)
        self.root.after(0, self.show_done_popup, "OKCOL generation complete", [
            "KCL conversion finished.",
            "",
            "KCL files found: {}".format(total),
            "OKCOL generated: {}".format(success),
            "Failed: {}".format(failed),
        ])

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