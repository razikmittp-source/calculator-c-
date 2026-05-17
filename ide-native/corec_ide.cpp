// CoreC IDE — Native Linux GUI in C++ using GTK3
// Build: g++ -O2 corec_ide.cpp $(pkg-config --cflags --libs gtk+-3.0) -o corec-ide
// Run:   ./corec-ide

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <chrono>

// ─── Global widgets ───
static GtkWidget *window = nullptr;
static GtkWidget *editor_view = nullptr;
static GtkTextBuffer *editor_buf = nullptr;
static GtkWidget *output_view = nullptr;
static GtkTextBuffer *output_buf = nullptr;
static GtkWidget *status_label = nullptr;
static GtkWidget *file_label = nullptr;
static std::string current_file = "";
static std::string compiler_path = "";

// ─── Syntax highlighting tags ───
static GtkTextTag *tag_keyword = nullptr;
static GtkTextTag *tag_type = nullptr;
static GtkTextTag *tag_string = nullptr;
static GtkTextTag *tag_number = nullptr;
static GtkTextTag *tag_comment = nullptr;
static GtkTextTag *tag_function = nullptr;
static GtkTextTag *tag_operator = nullptr;

// ─── Output color tags ───
static GtkTextTag *tag_out_error = nullptr;
static GtkTextTag *tag_out_success = nullptr;
static GtkTextTag *tag_out_info = nullptr;

// Forward declarations
static void on_run_clicked(GtkWidget *w, gpointer data);
static void on_emit_clicked(GtkWidget *w, gpointer data);
static void on_open_clicked(GtkWidget *w, gpointer data);
static void on_save_clicked(GtkWidget *w, gpointer data);
static void on_new_clicked(GtkWidget *w, gpointer data);
static void apply_syntax_highlighting(GtkTextBuffer *buf);
static void on_buffer_changed(GtkTextBuffer *buf, gpointer data);
static std::string find_compiler();
static void set_status(const std::string &msg, const std::string &type);
static void clear_output();
static void append_output(const std::string &text, GtkTextTag *tag = nullptr);

// ─── Example code ───
static const char *HELLO_CODE =
"// CoreC — Hello World\n"
"fn main() {\n"
"    let name = \"World\"\n"
"    print(\"Hello, {name}!\")\n"
"    print(\"CoreC is blazingly fast\")\n"
"}\n";

// ─── Keywords for syntax highlighting ───
static const std::vector<std::string> KEYWORDS = {
    "fn", "let", "mut", "if", "else", "for", "in", "while",
    "return", "match", "struct", "import", "true", "false"
};
static const std::vector<std::string> TYPES = {
    "i32", "i64", "f32", "f64", "bool", "str", "void"
};

// ─── Helpers ───
static std::string read_file(const std::string &path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool write_file(const std::string &path, const std::string &content) {
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

static std::string get_editor_text() {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(editor_buf, &start, &end);
    char *txt = gtk_text_buffer_get_text(editor_buf, &start, &end, FALSE);
    std::string s(txt);
    g_free(txt);
    return s;
}

static void set_editor_text(const std::string &text) {
    g_signal_handlers_block_by_func(editor_buf, (gpointer)on_buffer_changed, NULL);
    gtk_text_buffer_set_text(editor_buf, text.c_str(), -1);
    g_signal_handlers_unblock_by_func(editor_buf, (gpointer)on_buffer_changed, NULL);
    apply_syntax_highlighting(editor_buf);
}

static std::string exec_cmd(const std::string &cmd, int *exit_code = nullptr, long *ms = nullptr) {
    auto start = std::chrono::high_resolution_clock::now();
    FILE *pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        if (exit_code) *exit_code = -1;
        return "Failed to execute command";
    }
    std::string result;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int status = pclose(pipe);
    if (exit_code) *exit_code = WEXITSTATUS(status);
    auto end = std::chrono::high_resolution_clock::now();
    if (ms) *ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}

// ─── Find compiler ───
static std::string find_compiler() {
    // Try common locations
    std::vector<std::string> candidates = {
        "./compiler/corec.py",
        "../compiler/corec.py",
        "/usr/local/bin/corec",
        "/home/ubuntu/corec/compiler/corec.py"
    };

    // Check next to the binary itself
    char self[1024];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = 0;
        std::string base(self);
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            base = base.substr(0, slash);
            candidates.insert(candidates.begin(), base + "/../compiler/corec.py");
            candidates.insert(candidates.begin(), base + "/compiler/corec.py");
            candidates.insert(candidates.begin(), base + "/corec.py");
        }
    }

    for (const auto &c : candidates) {
        if (access(c.c_str(), R_OK) == 0) {
            return c;
        }
    }
    return "";
}

// ─── Status bar ───
static void set_status(const std::string &msg, const std::string &type) {
    std::string css_class;
    std::string color;
    if (type == "ok") color = "#3fb950";
    else if (type == "err") color = "#f85149";
    else if (type == "run") color = "#d29922";
    else color = "#8b949e";

    std::string markup = "<span foreground=\"" + color + "\"><b>● </b>" + msg + "</span>";
    gtk_label_set_markup(GTK_LABEL(status_label), markup.c_str());
}

// ─── Output panel ───
static void clear_output() {
    gtk_text_buffer_set_text(output_buf, "", -1);
}

static void append_output(const std::string &text, GtkTextTag *tag) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(output_buf, &end);
    if (tag) {
        gtk_text_buffer_insert_with_tags(output_buf, &end, text.c_str(), -1, tag, NULL);
    } else {
        gtk_text_buffer_insert(output_buf, &end, text.c_str(), -1);
    }
    // Auto-scroll
    gtk_text_buffer_get_end_iter(output_buf, &end);
    GtkTextMark *mark = gtk_text_buffer_create_mark(output_buf, NULL, &end, FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(output_view), mark);
    gtk_text_buffer_delete_mark(output_buf, mark);
}

// ─── Syntax highlighting ───
static bool is_word_char(char c) { return isalnum(c) || c == '_'; }

static void apply_syntax_highlighting(GtkTextBuffer *buf) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    // Remove all existing tags
    gtk_text_buffer_remove_all_tags(buf, &start, &end);

    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    std::string s(text);
    g_free(text);

    size_t i = 0;
    while (i < s.size()) {
        // Comments
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
            size_t j = i;
            while (j < s.size() && s[j] != '\n') j++;
            GtkTextIter a, b;
            gtk_text_buffer_get_iter_at_offset(buf, &a, i);
            gtk_text_buffer_get_iter_at_offset(buf, &b, j);
            gtk_text_buffer_apply_tag(buf, tag_comment, &a, &b);
            i = j;
            continue;
        }
        // Strings
        if (s[i] == '"') {
            size_t j = i + 1;
            while (j < s.size() && s[j] != '"') {
                if (s[j] == '\\' && j + 1 < s.size()) j++;
                j++;
            }
            if (j < s.size()) j++;
            GtkTextIter a, b;
            gtk_text_buffer_get_iter_at_offset(buf, &a, i);
            gtk_text_buffer_get_iter_at_offset(buf, &b, j);
            gtk_text_buffer_apply_tag(buf, tag_string, &a, &b);
            i = j;
            continue;
        }
        // Numbers
        if (isdigit(s[i])) {
            size_t j = i;
            while (j < s.size() && (isdigit(s[j]) || s[j] == '.')) {
                if (s[j] == '.' && j + 1 < s.size() && s[j + 1] == '.') break;
                j++;
            }
            GtkTextIter a, b;
            gtk_text_buffer_get_iter_at_offset(buf, &a, i);
            gtk_text_buffer_get_iter_at_offset(buf, &b, j);
            gtk_text_buffer_apply_tag(buf, tag_number, &a, &b);
            i = j;
            continue;
        }
        // Identifiers/keywords
        if (isalpha(s[i]) || s[i] == '_') {
            size_t j = i;
            while (j < s.size() && is_word_char(s[j])) j++;
            std::string word = s.substr(i, j - i);

            GtkTextTag *tag = nullptr;
            for (const auto &k : KEYWORDS) {
                if (word == k) { tag = tag_keyword; break; }
            }
            if (!tag) {
                for (const auto &k : TYPES) {
                    if (word == k) { tag = tag_type; break; }
                }
            }
            // Function call: identifier followed by '('
            if (!tag) {
                size_t k = j;
                while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) k++;
                if (k < s.size() && s[k] == '(') tag = tag_function;
            }

            if (tag) {
                GtkTextIter a, b;
                gtk_text_buffer_get_iter_at_offset(buf, &a, i);
                gtk_text_buffer_get_iter_at_offset(buf, &b, j);
                gtk_text_buffer_apply_tag(buf, tag, &a, &b);
            }
            i = j;
            continue;
        }
        i++;
    }
}

static guint highlight_timer_id = 0;
static gboolean delayed_highlight(gpointer data) {
    apply_syntax_highlighting(editor_buf);
    highlight_timer_id = 0;
    return G_SOURCE_REMOVE;
}

static void on_buffer_changed(GtkTextBuffer *buf, gpointer data) {
    if (highlight_timer_id) g_source_remove(highlight_timer_id);
    highlight_timer_id = g_timeout_add(150, delayed_highlight, NULL);
}

// ─── Actions ───
static void on_run_clicked(GtkWidget *w, gpointer data) {
    if (compiler_path.empty()) {
        clear_output();
        append_output("Compiler not found. Place corec.py in ../compiler/ relative to this binary,\n", tag_out_error);
        append_output("or install with: sudo ln -sf ./compiler/corec.py /usr/local/bin/corec\n", tag_out_error);
        set_status("Compiler not found", "err");
        return;
    }

    set_status("Compiling...", "run");
    clear_output();
    append_output("Compiling...\n", tag_out_info);
    while (gtk_events_pending()) gtk_main_iteration();

    // Save editor to temp file
    std::string src_path = "/tmp/corec_ide_program.crc";
    std::string bin_path = "/tmp/corec_ide_program";
    write_file(src_path, get_editor_text());

    // Build
    std::string cmd = "python3 \"" + compiler_path + "\" build \"" + src_path + "\" -o \"" + bin_path + "\"";
    int exit_code;
    long ms;
    std::string build_out = exec_cmd(cmd, &exit_code, &ms);

    if (exit_code != 0) {
        clear_output();
        append_output("✗ Compilation error:\n\n", tag_out_error);
        append_output(build_out, tag_out_error);
        set_status("Compilation failed", "err");
        return;
    }

    // Run
    set_status("Running...", "run");
    clear_output();
    long run_ms;
    int run_exit;
    std::string run_out = exec_cmd("\"" + bin_path + "\"", &run_exit, &run_ms);

    if (!run_out.empty()) {
        append_output(run_out, tag_out_success);
        if (run_out.back() != '\n') append_output("\n");
    }
    
    char info[256];
    snprintf(info, sizeof(info), "\n────────────────────────\nFinished in %ldms (compile %ldms) | exit=%d\n",
             run_ms, ms - run_ms, run_exit);
    append_output(info, tag_out_info);

    set_status("Done", "ok");
    unlink(bin_path.c_str());
}

static void on_emit_clicked(GtkWidget *w, gpointer data) {
    if (compiler_path.empty()) {
        set_status("Compiler not found", "err");
        return;
    }
    set_status("Generating C...", "run");

    std::string src_path = "/tmp/corec_ide_program.crc";
    write_file(src_path, get_editor_text());

    std::string cmd = "python3 \"" + compiler_path + "\" emit \"" + src_path + "\"";
    int exit_code;
    std::string out = exec_cmd(cmd, &exit_code);

    clear_output();
    if (exit_code != 0) {
        append_output("✗ Error generating C:\n\n", tag_out_error);
        append_output(out, tag_out_error);
        set_status("Error", "err");
    } else {
        append_output("───── Generated C code ─────\n\n", tag_out_info);
        append_output(out);
        set_status("C generated", "ok");
    }
}

static void on_open_clicked(GtkWidget *w, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open CoreC File", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "CoreC files (*.crc)");
    gtk_file_filter_add_pattern(filter, "*.crc");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    GtkFileFilter *all = gtk_file_filter_new();
    gtk_file_filter_set_name(all, "All files");
    gtk_file_filter_add_pattern(all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), all);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        std::string content = read_file(fname);
        set_editor_text(content);
        current_file = fname;
        std::string label = "📄 " + std::string(strrchr(fname, '/') ? strrchr(fname, '/') + 1 : fname);
        gtk_label_set_text(GTK_LABEL(file_label), label.c_str());
        set_status("Opened", "ok");
        g_free(fname);
    }
    gtk_widget_destroy(dialog);
}

static void on_save_clicked(GtkWidget *w, gpointer data) {
    std::string path = current_file;
    if (path.empty()) {
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Save CoreC File", GTK_WINDOW(window),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "Cancel", GTK_RESPONSE_CANCEL,
            "Save", GTK_RESPONSE_ACCEPT, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "program.crc");

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            path = fname;
            g_free(fname);
        }
        gtk_widget_destroy(dialog);
        if (path.empty()) return;
    }
    if (write_file(path, get_editor_text())) {
        current_file = path;
        std::string label = "📄 " + path.substr(path.find_last_of('/') + 1);
        gtk_label_set_text(GTK_LABEL(file_label), label.c_str());
        set_status("Saved", "ok");
    } else {
        set_status("Save failed", "err");
    }
}

static void on_new_clicked(GtkWidget *w, gpointer data) {
    set_editor_text(HELLO_CODE);
    current_file = "";
    gtk_label_set_text(GTK_LABEL(file_label), "📄 untitled.crc");
    set_status("New file", "ok");
}

static void on_example_changed(GtkComboBoxText *combo, gpointer data) {
    const gchar *name = gtk_combo_box_text_get_active_text(combo);
    if (!name) return;
    std::string n(name);

    // Find example file
    std::vector<std::string> roots = {
        "./examples/", "../examples/", "/home/ubuntu/corec/examples/"
    };
    char self[1024];
    ssize_t l = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (l > 0) {
        self[l] = 0;
        std::string base(self);
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            roots.insert(roots.begin(), base.substr(0, slash) + "/../examples/");
            roots.insert(roots.begin(), base.substr(0, slash) + "/examples/");
        }
    }

    for (const auto &r : roots) {
        std::string p = r + n + ".crc";
        if (access(p.c_str(), R_OK) == 0) {
            std::string content = read_file(p);
            set_editor_text(content);
            current_file = "";
            gtk_label_set_text(GTK_LABEL(file_label), ("📄 " + n + ".crc").c_str());
            set_status("Loaded example", "ok");
            return;
        }
    }
}

// ─── Build UI ───
static void apply_css() {
    GtkCssProvider *prov = gtk_css_provider_new();
    const char *css =
        "window { background: #0d1117; color: #c9d1d9; }"
        ".header { background: #161b22; border-bottom: 1px solid #30363d; padding: 6px 12px; }"
        ".logo { font-size: 18px; font-weight: bold; color: #58a6ff; }"
        ".logo-c { color: #3fb950; }"
        ".btn-run { background: #238636; color: white; border: 1px solid #2ea043; padding: 4px 14px; font-weight: bold; }"
        ".btn-run:hover { background: #2ea043; }"
        ".btn-emit { background: #1f2937; color: #bc8cff; border: 1px solid #30363d; padding: 4px 14px; }"
        ".btn-file { background: #21262d; color: #c9d1d9; border: 1px solid #30363d; padding: 4px 12px; }"
        ".btn-file:hover { border-color: #58a6ff; }"
        ".paned { background: #0d1117; }"
        ".editor textview { background: #0d1117; color: #c9d1d9; font-family: 'JetBrains Mono', monospace; font-size: 13px; padding: 12px; }"
        ".editor textview text { background: #0d1117; }"
        ".output { background: #0d1117; }"
        ".output textview { background: #0d1117; color: #c9d1d9; font-family: 'JetBrains Mono', monospace; font-size: 12px; padding: 12px; }"
        ".output textview text { background: #0d1117; }"
        ".section-header { background: #161b22; color: #8b949e; padding: 6px 12px; font-size: 11px; border-bottom: 1px solid #30363d; }"
        ".statusbar { background: #161b22; border-top: 1px solid #30363d; padding: 4px 12px; color: #8b949e; font-size: 11px; }"
        "combobox { background: #21262d; color: #c9d1d9; }"
        "combobox button { background: #21262d; }";
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(prov),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);
}

static void create_syntax_tags(GtkTextBuffer *buf) {
    tag_keyword = gtk_text_buffer_create_tag(buf, "keyword",
        "foreground", "#ff7b72", "weight", PANGO_WEIGHT_BOLD, NULL);
    tag_type = gtk_text_buffer_create_tag(buf, "type",
        "foreground", "#79c0ff", NULL);
    tag_string = gtk_text_buffer_create_tag(buf, "string",
        "foreground", "#a5d6ff", NULL);
    tag_number = gtk_text_buffer_create_tag(buf, "number",
        "foreground", "#f0883e", NULL);
    tag_comment = gtk_text_buffer_create_tag(buf, "comment",
        "foreground", "#6e7681", "style", PANGO_STYLE_ITALIC, NULL);
    tag_function = gtk_text_buffer_create_tag(buf, "function",
        "foreground", "#d2a8ff", NULL);
}

static void create_output_tags(GtkTextBuffer *buf) {
    tag_out_error = gtk_text_buffer_create_tag(buf, "error",
        "foreground", "#f85149", NULL);
    tag_out_success = gtk_text_buffer_create_tag(buf, "success",
        "foreground", "#3fb950", NULL);
    tag_out_info = gtk_text_buffer_create_tag(buf, "info",
        "foreground", "#8b949e", "style", PANGO_STYLE_ITALIC, NULL);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    apply_css();

    compiler_path = find_compiler();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CoreC IDE — Blazingly Fast Language");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 750);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // ─── Header bar ───
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "header");

    GtkWidget *logo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(logo),
        "<span size='large' weight='bold' foreground='#58a6ff'>Core</span>"
        "<span size='large' weight='bold' foreground='#3fb950'>C</span>"
        "<span size='small' foreground='#8b949e'>  v0.1 IDE</span>");
    gtk_box_pack_start(GTK_BOX(header), logo, FALSE, FALSE, 6);

    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(header), sep1, FALSE, FALSE, 4);

    GtkWidget *btn_new = gtk_button_new_with_label("New");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_new), "btn-file");
    g_signal_connect(btn_new, "clicked", G_CALLBACK(on_new_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(header), btn_new, FALSE, FALSE, 0);

    GtkWidget *btn_open = gtk_button_new_with_label("📂 Open");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_open), "btn-file");
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(header), btn_open, FALSE, FALSE, 0);

    GtkWidget *btn_save = gtk_button_new_with_label("💾 Save");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_save), "btn-file");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(header), btn_save, FALSE, FALSE, 0);

    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(header), sep2, FALSE, FALSE, 4);

    GtkWidget *examples_label = gtk_label_new("Example:");
    gtk_box_pack_start(GTK_BOX(header), examples_label, FALSE, FALSE, 4);

    GtkWidget *examples_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(examples_combo), "hello");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(examples_combo), "fibonacci");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(examples_combo), "factorial");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(examples_combo), "arrays");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(examples_combo), "benchmark");
    g_signal_connect(examples_combo, "changed", G_CALLBACK(on_example_changed), NULL);
    gtk_box_pack_start(GTK_BOX(header), examples_combo, FALSE, FALSE, 0);

    // Right side - Run/Emit
    GtkWidget *btn_emit = gtk_button_new_with_label("⚙ View C");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_emit), "btn-emit");
    g_signal_connect(btn_emit, "clicked", G_CALLBACK(on_emit_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(header), btn_emit, FALSE, FALSE, 4);

    GtkWidget *btn_run = gtk_button_new_with_label("▶ Run  (F5)");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_run), "btn-run");
    g_signal_connect(btn_run, "clicked", G_CALLBACK(on_run_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(header), btn_run, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    // ─── Main paned area: editor | output ───
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(gtk_widget_get_style_context(paned), "paned");
    gtk_paned_set_position(GTK_PANED(paned), 720);

    // Editor side
    GtkWidget *editor_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    file_label = gtk_label_new("📄 untitled.crc");
    gtk_widget_set_halign(file_label, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(file_label), "section-header");
    gtk_box_pack_start(GTK_BOX(editor_box), file_label, FALSE, FALSE, 0);

    GtkWidget *editor_scroll = gtk_scrolled_window_new(NULL, NULL);
    editor_view = gtk_text_view_new();
    editor_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor_view));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(editor_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(editor_view), 16);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(editor_view), 16);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(editor_view), 12);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(editor_view), 12);
    gtk_text_view_set_tabs(GTK_TEXT_VIEW(editor_view), pango_tab_array_new_with_positions(1, TRUE, PANGO_TAB_LEFT, 32));
    create_syntax_tags(editor_buf);
    gtk_style_context_add_class(gtk_widget_get_style_context(editor_view), "editor");
    g_signal_connect(editor_buf, "changed", G_CALLBACK(on_buffer_changed), NULL);
    gtk_container_add(GTK_CONTAINER(editor_scroll), editor_view);
    gtk_box_pack_start(GTK_BOX(editor_box), editor_scroll, TRUE, TRUE, 0);
    gtk_paned_pack1(GTK_PANED(paned), editor_box, TRUE, FALSE);

    // Output side
    GtkWidget *output_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(output_box), "output");

    GtkWidget *out_header = gtk_label_new("💻 Output");
    gtk_widget_set_halign(out_header, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(out_header), "section-header");
    gtk_box_pack_start(GTK_BOX(output_box), out_header, FALSE, FALSE, 0);

    GtkWidget *output_scroll = gtk_scrolled_window_new(NULL, NULL);
    output_view = gtk_text_view_new();
    output_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(output_view));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(output_view), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(output_view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(output_view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(output_view), 8);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output_view), GTK_WRAP_WORD_CHAR);
    create_output_tags(output_buf);
    gtk_container_add(GTK_CONTAINER(output_scroll), output_view);
    gtk_box_pack_start(GTK_BOX(output_box), output_scroll, TRUE, TRUE, 0);
    gtk_paned_pack2(GTK_PANED(paned), output_box, TRUE, FALSE);

    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

    // ─── Status bar ───
    GtkWidget *statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusbar), "statusbar");
    status_label = gtk_label_new(NULL);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(statusbar), status_label, TRUE, TRUE, 0);

    GtkWidget *hint = gtk_label_new("F5 = Run  |  Ctrl+S = Save  |  Ctrl+O = Open");
    gtk_widget_set_halign(hint, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(statusbar), hint, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);

    // ─── Initial content ───
    set_editor_text(HELLO_CODE);
    if (compiler_path.empty()) {
        set_status("Compiler not found - place corec.py in ../compiler/", "err");
    } else {
        set_status("Ready  •  Compiler: " + compiler_path, "ok");
    }

    // ─── Keyboard shortcuts ───
    GtkAccelGroup *accel = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel);
    gtk_widget_add_accelerator(btn_run, "clicked", accel, GDK_KEY_F5, (GdkModifierType)0, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(btn_save, "clicked", accel, GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(btn_open, "clicked", accel, GDK_KEY_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(btn_new, "clicked", accel, GDK_KEY_n, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_widget_add_accelerator(btn_emit, "clicked", accel, GDK_KEY_F6, (GdkModifierType)0, GTK_ACCEL_VISIBLE);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
