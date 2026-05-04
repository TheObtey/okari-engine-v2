import json
import struct
import subprocess
import threading
import webbrowser
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk


APP_TITLE = "(Okari) Nintendo Asset Importer"
APP_VERSION = "v0.3.0"
AUTHOR_NAME = "TheObtey"
AUTHOR_URL = "https://github.com/TheObtey"
CONFIG_PATH = Path(__file__).with_suffix(".config.json")


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


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

    def save_config(self):
        config = {
            "dump_dir": self.dump_dir.get(),
            "extract_dir": self.extract_dir.get(),
            "dae_dir": self.dae_dir.get(),
            "superbmd_path": self.superbmd_path.get(),
            "fbx_dir": self.fbx_dir.get(),
            "blender_path": self.blender_path.get(),
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

        # Bind while the pointer is anywhere inside the tab, including labels,
        # entries and buttons. This fixes wheel events being swallowed by children.
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
