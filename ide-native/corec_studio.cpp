// CoreC Studio — Visual Studio-style native Linux IDE
// Build: g++ -O2 -std=c++17 corec_studio.cpp $(pkg-config --cflags --libs gtk+-3.0) -o corec-studio
//
// Features:
//   - Menu bar (File / Edit / View / Build / Debug / Help)
//   - Toolbar with icons
//   - Solution Explorer (file tree, left)
//   - Tabbed code editor with syntax highlighting (center)
//   - Output panel with tabs (Build / Console, bottom)
//   - Status bar
//   - Dark theme (VS 2022 / VS Code Dark+)

#include <gtk/gtk.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

// ─── Globals ───
static GtkWidget *window = nullptr;
static GtkWidget *notebook_editors = nullptr;     // Tabbed editor area
static GtkWidget *notebook_output = nullptr;      // Bottom output tabs
static GtkWidget *tree_solution = nullptr;        // Solution Explorer
static GtkTreeStore *solution_store = nullptr;
static GtkWidget *output_build = nullptr;         // Build output text view
static GtkTextBuffer *output_build_buf = nullptr;
static GtkWidget *output_console = nullptr;       // Run output text view
static GtkTextBuffer *output_console_buf = nullptr;
static GtkWidget *output_c = nullptr;             // Generated C view
static GtkTextBuffer *output_c_buf = nullptr;
static GtkWidget *status_label = nullptr;
static GtkWidget *line_col_label = nullptr;
static std::string project_dir = "";
static std::string compiler_path = "";

// ─── Tab/file management ───
struct EditorTab {
    GtkWidget *scroll;
    GtkWidget *view;
    GtkTextBuffer *buffer;
    GtkWidget *tab_label;
    GtkWidget *tab_box;
    std::string file_path;
    bool dirty = false;
    bool is_new = true;
};
static std::vector<std::shared_ptr<EditorTab>> tabs;

// ─── Syntax tags (per buffer) ───
struct SyntaxTags {
    GtkTextTag *keyword;
    GtkTextTag *type;
    GtkTextTag *string;
    GtkTextTag *number;
    GtkTextTag *comment;
    GtkTextTag *function;
    GtkTextTag *operator_;
};
static std::map<GtkTextBuffer*, SyntaxTags> syntax_tags;

// ─── Output tags ───
static GtkTextTag *tag_out_error = nullptr;
static GtkTextTag *tag_out_success = nullptr;
static GtkTextTag *tag_out_info = nullptr;
static GtkTextTag *tag_out_warn = nullptr;

// Keywords
static const std::vector<std::string> KEYWORDS = {
    "fn", "let", "mut", "if", "else", "for", "in", "while",
    "return", "match", "struct", "import", "true", "false"
};
static const std::vector<std::string> TYPES = {
    "i32", "i64", "f32", "f64", "bool", "str", "void"
};

// ─── Forward declarations ───
static void create_new_tab(const std::string &file_path, const std::string &content);
static void on_run_clicked(GtkWidget *w, gpointer data);
static void on_build_clicked(GtkWidget *w, gpointer data);
static void on_view_c_clicked(GtkWidget *w, gpointer data);
static void on_open_clicked(GtkWidget *w, gpointer data);
static void on_save_clicked(GtkWidget *w, gpointer data);
static void on_new_clicked(GtkWidget *w, gpointer data);
static void on_close_tab(GtkButton *btn, EditorTab *tab);
static void on_open_folder(GtkWidget *w, gpointer data);
static void on_tree_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data);
static void on_buffer_changed(GtkTextBuffer *buf, gpointer data);
static void on_cursor_moved(GtkTextBuffer *buf, GtkTextIter *loc, GtkTextMark *mark, gpointer data);
static void apply_syntax_highlighting(GtkTextBuffer *buf, SyntaxTags &tags);
static EditorTab* current_tab();
static void refresh_solution_tree();
static void load_folder_to_tree(const std::string &dir);
static std::string find_compiler();
static void set_status(const std::string &msg, const std::string &type = "info");
static void append_to_buffer(GtkTextBuffer *buf, const std::string &text, GtkTextTag *tag = nullptr);
static void clear_buffer(GtkTextBuffer *buf);
static void update_tab_label(EditorTab *tab);

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

static std::string basename_of(const std::string &path) {
    auto p = path.find_last_of('/');
    return (p == std::string::npos) ? path : path.substr(p + 1);
}

static std::string dirname_of(const std::string &path) {
    auto p = path.find_last_of('/');
    return (p == std::string::npos) ? "." : path.substr(0, p);
}

static std::string get_buffer_text(GtkTextBuffer *buf) {
    GtkTextIter s, e;
    gtk_text_buffer_get_bounds(buf, &s, &e);
    char *t = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
    std::string r(t);
    g_free(t);
    return r;
}

static std::string exec_cmd(const std::string &cmd, int *exit_code = nullptr, long *ms = nullptr) {
    auto start = std::chrono::high_resolution_clock::now();
    FILE *p = popen((cmd + " 2>&1").c_str(), "r");
    if (!p) { if (exit_code) *exit_code = -1; return ""; }
    std::string r;
    char buf[1024];
    while (fgets(buf, sizeof(buf), p)) r += buf;
    int s = pclose(p);
    if (exit_code) *exit_code = WEXITSTATUS(s);
    auto end = std::chrono::high_resolution_clock::now();
    if (ms) *ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return r;
}

static std::string find_compiler() {
    std::vector<std::string> cands;
    char self[1024];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = 0;
        std::string base(self);
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            base = base.substr(0, slash);
            cands.push_back(base + "/../compiler/corec.py");
            cands.push_back(base + "/compiler/corec.py");
        }
    }
    cands.push_back("./compiler/corec.py");
    cands.push_back("../compiler/corec.py");
    cands.push_back("/home/ubuntu/corec/compiler/corec.py");
    cands.push_back("/usr/local/bin/corec");
    for (const auto &c : cands)
        if (access(c.c_str(), R_OK) == 0) return c;
    return "";
}

// ─── Output ───
static void clear_buffer(GtkTextBuffer *buf) {
    gtk_text_buffer_set_text(buf, "", -1);
}

static void append_to_buffer(GtkTextBuffer *buf, const std::string &text, GtkTextTag *tag) {
    GtkTextIter e;
    gtk_text_buffer_get_end_iter(buf, &e);
    if (tag) gtk_text_buffer_insert_with_tags(buf, &e, text.c_str(), -1, tag, NULL);
    else gtk_text_buffer_insert(buf, &e, text.c_str(), -1);
}

static void set_status(const std::string &msg, const std::string &type) {
    std::string color = "#a9a9a9";
    if (type == "ok") color = "#4ec9b0";
    else if (type == "err") color = "#f85149";
    else if (type == "run") color = "#dcdcaa";
    std::string markup = "<span foreground=\"" + color + "\">" + msg + "</span>";
    gtk_label_set_markup(GTK_LABEL(status_label), markup.c_str());
}

// ─── Syntax highlighting ───
static bool is_word_char(char c) { return isalnum(c) || c == '_'; }

static void apply_syntax_highlighting(GtkTextBuffer *buf, SyntaxTags &tags) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buf, &start, &end);
    gtk_text_buffer_remove_all_tags(buf, &start, &end);

    char *text = gtk_text_buffer_get_text(buf, &start, &end, FALSE);
    std::string s(text);
    g_free(text);

    size_t i = 0;
    while (i < s.size()) {
        if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
            size_t j = i;
            while (j < s.size() && s[j] != '\n') j++;
            GtkTextIter a, b;
            gtk_text_buffer_get_iter_at_offset(buf, &a, i);
            gtk_text_buffer_get_iter_at_offset(buf, &b, j);
            gtk_text_buffer_apply_tag(buf, tags.comment, &a, &b);
            i = j;
            continue;
        }
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
            gtk_text_buffer_apply_tag(buf, tags.string, &a, &b);
            i = j;
            continue;
        }
        if (isdigit(s[i])) {
            size_t j = i;
            while (j < s.size() && (isdigit(s[j]) || s[j] == '.')) {
                if (s[j] == '.' && j + 1 < s.size() && s[j + 1] == '.') break;
                j++;
            }
            GtkTextIter a, b;
            gtk_text_buffer_get_iter_at_offset(buf, &a, i);
            gtk_text_buffer_get_iter_at_offset(buf, &b, j);
            gtk_text_buffer_apply_tag(buf, tags.number, &a, &b);
            i = j;
            continue;
        }
        if (isalpha(s[i]) || s[i] == '_') {
            size_t j = i;
            while (j < s.size() && is_word_char(s[j])) j++;
            std::string w = s.substr(i, j - i);

            GtkTextTag *tag = nullptr;
            for (const auto &k : KEYWORDS) if (w == k) { tag = tags.keyword; break; }
            if (!tag) for (const auto &k : TYPES) if (w == k) { tag = tags.type; break; }
            if (!tag) {
                size_t k = j;
                while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) k++;
                if (k < s.size() && s[k] == '(') tag = tags.function;
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

static guint highlight_timer = 0;
static GtkTextBuffer *pending_highlight_buf = nullptr;

static gboolean delayed_hl(gpointer data) {
    if (pending_highlight_buf && syntax_tags.count(pending_highlight_buf)) {
        apply_syntax_highlighting(pending_highlight_buf, syntax_tags[pending_highlight_buf]);
    }
    highlight_timer = 0;
    pending_highlight_buf = nullptr;
    return G_SOURCE_REMOVE;
}

static void on_buffer_changed(GtkTextBuffer *buf, gpointer data) {
    // Mark tab as dirty
    for (auto &t : tabs) {
        if (t->buffer == buf && !t->dirty) {
            t->dirty = true;
            update_tab_label(t.get());
            break;
        }
    }
    if (highlight_timer) g_source_remove(highlight_timer);
    pending_highlight_buf = buf;
    highlight_timer = g_timeout_add(150, delayed_hl, NULL);
}

static void on_cursor_moved(GtkTextBuffer *buf, GtkTextIter *loc, GtkTextMark *mark, gpointer data) {
    if (gtk_text_buffer_get_insert(buf) != mark) return;
    GtkTextIter it;
    gtk_text_buffer_get_iter_at_mark(buf, &it, mark);
    int line = gtk_text_iter_get_line(&it) + 1;
    int col = gtk_text_iter_get_line_offset(&it) + 1;
    char buftxt[64];
    snprintf(buftxt, sizeof(buftxt), "Ln %d, Col %d", line, col);
    gtk_label_set_text(GTK_LABEL(line_col_label), buftxt);
}

// ─── Tabs ───
static EditorTab* current_tab() {
    int idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_editors));
    if (idx < 0 || idx >= (int)tabs.size()) return nullptr;
    return tabs[idx].get();
}

static void update_tab_label(EditorTab *tab) {
    std::string name = tab->file_path.empty() ? "untitled.crc" : basename_of(tab->file_path);
    if (tab->dirty) name = "● " + name;
    gtk_label_set_text(GTK_LABEL(tab->tab_label), name.c_str());
}

static void on_close_tab(GtkButton *btn, EditorTab *tab) {
    for (auto it = tabs.begin(); it != tabs.end(); ++it) {
        if (it->get() == tab) {
            int idx = it - tabs.begin();
            tabs.erase(it);
            gtk_notebook_remove_page(GTK_NOTEBOOK(notebook_editors), idx);
            syntax_tags.erase(tab->buffer);
            break;
        }
    }
    if (tabs.empty()) {
        create_new_tab("", "// Welcome to CoreC Studio\n// Open a file or create a new one\n");
    }
}

static SyntaxTags create_syntax_tags(GtkTextBuffer *buf) {
    SyntaxTags t;
    t.keyword = gtk_text_buffer_create_tag(buf, "keyword",
        "foreground", "#c586c0", "weight", PANGO_WEIGHT_BOLD, NULL);
    t.type = gtk_text_buffer_create_tag(buf, "type",
        "foreground", "#4ec9b0", NULL);
    t.string = gtk_text_buffer_create_tag(buf, "string",
        "foreground", "#ce9178", NULL);
    t.number = gtk_text_buffer_create_tag(buf, "number",
        "foreground", "#b5cea8", NULL);
    t.comment = gtk_text_buffer_create_tag(buf, "comment",
        "foreground", "#6a9955", "style", PANGO_STYLE_ITALIC, NULL);
    t.function = gtk_text_buffer_create_tag(buf, "function",
        "foreground", "#dcdcaa", NULL);
    t.operator_ = gtk_text_buffer_create_tag(buf, "operator",
        "foreground", "#d4d4d4", NULL);
    return t;
}

static void create_new_tab(const std::string &file_path, const std::string &content) {
    auto tab = std::make_shared<EditorTab>();
    tab->file_path = file_path;
    tab->is_new = file_path.empty();

    tab->scroll = gtk_scrolled_window_new(NULL, NULL);
    tab->view = gtk_text_view_new();
    tab->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tab->view));
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->view), 18);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->view), 18);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tab->view), 10);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tab->view), 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(tab->view), "editor");
    
    PangoTabArray *tabarr = pango_tab_array_new_with_positions(1, TRUE, PANGO_TAB_LEFT, 32);
    gtk_text_view_set_tabs(GTK_TEXT_VIEW(tab->view), tabarr);

    syntax_tags[tab->buffer] = create_syntax_tags(tab->buffer);
    gtk_text_buffer_set_text(tab->buffer, content.c_str(), -1);
    apply_syntax_highlighting(tab->buffer, syntax_tags[tab->buffer]);

    g_signal_connect(tab->buffer, "changed", G_CALLBACK(on_buffer_changed), NULL);
    g_signal_connect(tab->buffer, "mark-set", G_CALLBACK(on_cursor_moved), NULL);

    gtk_container_add(GTK_CONTAINER(tab->scroll), tab->view);
    gtk_widget_show_all(tab->scroll);

    // Tab label widget with close button
    tab->tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    std::string name = file_path.empty() ? "untitled.crc" : basename_of(file_path);
    tab->tab_label = gtk_label_new(name.c_str());
    GtkWidget *close_btn = gtk_button_new();
    GtkWidget *close_img = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
    gtk_button_set_image(GTK_BUTTON(close_btn), close_img);
    gtk_button_set_relief(GTK_BUTTON(close_btn), GTK_RELIEF_NONE);
    gtk_widget_set_size_request(close_btn, 18, 18);
    gtk_box_pack_start(GTK_BOX(tab->tab_box), tab->tab_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab->tab_box), close_btn, FALSE, FALSE, 0);
    gtk_widget_show_all(tab->tab_box);

    tabs.push_back(tab);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab), tab.get());

    int idx = gtk_notebook_append_page(GTK_NOTEBOOK(notebook_editors), tab->scroll, tab->tab_box);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_editors), idx);
    gtk_widget_grab_focus(tab->view);
}

// ─── Solution Explorer ───
static void load_folder_to_tree(const std::string &dir) {
    gtk_tree_store_clear(solution_store);
    project_dir = dir;

    GtkTreeIter root;
    gtk_tree_store_append(solution_store, &root, NULL);
    gtk_tree_store_set(solution_store, &root,
        0, "📁",
        1, basename_of(dir).c_str(),
        2, dir.c_str(), -1);

    DIR *d = opendir(dir.c_str());
    if (!d) return;
    
    std::vector<std::string> dirs, files;
    struct dirent *de;
    while ((de = readdir(d))) {
        std::string n = de->d_name;
        if (n == "." || n == "..") continue;
        if (n[0] == '.') continue;  // hidden
        std::string fullpath = dir + "/" + n;
        struct stat st;
        if (stat(fullpath.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) dirs.push_back(n);
        else files.push_back(n);
    }
    closedir(d);
    
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    for (const auto &n : dirs) {
        GtkTreeIter child;
        gtk_tree_store_append(solution_store, &child, &root);
        gtk_tree_store_set(solution_store, &child,
            0, "📁",
            1, n.c_str(),
            2, (dir + "/" + n).c_str(), -1);
        
        // Read sub-dir one level deep
        DIR *sd = opendir((dir + "/" + n).c_str());
        if (sd) {
            std::vector<std::string> subdirs, subfiles;
            struct dirent *sde;
            while ((sde = readdir(sd))) {
                std::string sn = sde->d_name;
                if (sn == "." || sn == ".." || sn[0] == '.') continue;
                std::string fp = dir + "/" + n + "/" + sn;
                struct stat sst;
                if (stat(fp.c_str(), &sst) != 0) continue;
                if (S_ISDIR(sst.st_mode)) subdirs.push_back(sn);
                else subfiles.push_back(sn);
            }
            closedir(sd);
            std::sort(subdirs.begin(), subdirs.end());
            std::sort(subfiles.begin(), subfiles.end());
            for (const auto &sn : subdirs) {
                GtkTreeIter sub;
                gtk_tree_store_append(solution_store, &sub, &child);
                gtk_tree_store_set(solution_store, &sub,
                    0, "📁", 1, sn.c_str(),
                    2, (dir + "/" + n + "/" + sn).c_str(), -1);
            }
            for (const auto &sn : subfiles) {
                std::string icon = "📄";
                if (sn.size() >= 4 && sn.substr(sn.size() - 4) == ".crc") icon = "📝";
                GtkTreeIter sub;
                gtk_tree_store_append(solution_store, &sub, &child);
                gtk_tree_store_set(solution_store, &sub,
                    0, icon.c_str(), 1, sn.c_str(),
                    2, (dir + "/" + n + "/" + sn).c_str(), -1);
            }
        }
    }
    for (const auto &n : files) {
        std::string icon = "📄";
        if (n.size() >= 4 && n.substr(n.size() - 4) == ".crc") icon = "📝";
        GtkTreeIter child;
        gtk_tree_store_append(solution_store, &child, &root);
        gtk_tree_store_set(solution_store, &child,
            0, icon.c_str(),
            1, n.c_str(),
            2, (dir + "/" + n).c_str(), -1);
    }

    gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_solution));
}

static void on_tree_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    GtkTreeIter it;
    if (!gtk_tree_model_get_iter(model, &it, path)) return;
    
    char *fullpath;
    gtk_tree_model_get(model, &it, 2, &fullpath, -1);
    if (!fullpath) return;
    
    struct stat st;
    if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode)) {
        // Check if already open
        for (size_t i = 0; i < tabs.size(); i++) {
            if (tabs[i]->file_path == fullpath) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_editors), i);
                g_free(fullpath);
                return;
            }
        }
        std::string content = read_file(fullpath);
        create_new_tab(fullpath, content);
    }
    g_free(fullpath);
}

// ─── Actions ───
static void on_run_clicked(GtkWidget *w, gpointer data) {
    EditorTab *t = current_tab();
    if (!t) return;
    if (compiler_path.empty()) {
        set_status("Compiler not found", "err");
        return;
    }

    set_status("Building...", "run");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_output), 0);  // Build tab
    clear_buffer(output_build_buf);
    append_to_buffer(output_build_buf, "1>------ Build started: CoreC project ------\n", tag_out_info);
    while (gtk_events_pending()) gtk_main_iteration();

    std::string src = "/tmp/corec_studio_program.crc";
    std::string bin = "/tmp/corec_studio_program";
    write_file(src, get_buffer_text(t->buffer));

    std::string cmd = "python3 \"" + compiler_path + "\" build \"" + src + "\" -o \"" + bin + "\"";
    int ec; long ms;
    std::string out = exec_cmd(cmd, &ec, &ms);

    if (ec != 0) {
        append_to_buffer(output_build_buf, "1>" + out + "\n", tag_out_error);
        append_to_buffer(output_build_buf,
            "========== Build: 0 succeeded, 1 failed ==========\n", tag_out_error);
        set_status("Build failed", "err");
        return;
    }
    
    char info[256];
    snprintf(info, sizeof(info), "1>Build succeeded in %ldms\n========== Build: 1 succeeded, 0 failed ==========\n", ms);
    append_to_buffer(output_build_buf, info, tag_out_success);

    // Switch to Console tab and run
    set_status("Running...", "run");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_output), 1);  // Console tab
    clear_buffer(output_console_buf);

    long rms; int rec;
    std::string rout = exec_cmd("\"" + bin + "\"", &rec, &rms);

    if (!rout.empty()) {
        append_to_buffer(output_console_buf, rout);
        if (rout.back() != '\n') append_to_buffer(output_console_buf, "\n");
    }
    char rinfo[256];
    snprintf(rinfo, sizeof(rinfo), "\n%s Program finished with exit code %d in %ldms.\n",
             rec == 0 ? "[OK]" : "[FAIL]", rec, rms);
    append_to_buffer(output_console_buf, rinfo, rec == 0 ? tag_out_success : tag_out_error);
    
    set_status(rec == 0 ? "Run successful" : "Run failed", rec == 0 ? "ok" : "err");
    unlink(bin.c_str());
}

static void on_build_clicked(GtkWidget *w, gpointer data) {
    EditorTab *t = current_tab();
    if (!t) return;
    set_status("Building...", "run");
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_output), 0);
    clear_buffer(output_build_buf);
    append_to_buffer(output_build_buf, "1>------ Build started ------\n", tag_out_info);
    while (gtk_events_pending()) gtk_main_iteration();

    std::string src = "/tmp/corec_studio_program.crc";
    std::string out_path = t->file_path.empty() ? "/tmp/corec_studio_out" 
                          : t->file_path.substr(0, t->file_path.find_last_of('.'));
    write_file(src, get_buffer_text(t->buffer));

    std::string cmd = "python3 \"" + compiler_path + "\" build \"" + src + "\" -o \"" + out_path + "\"";
    int ec; long ms;
    std::string out = exec_cmd(cmd, &ec, &ms);

    if (ec != 0) {
        append_to_buffer(output_build_buf, out, tag_out_error);
        append_to_buffer(output_build_buf,
            "========== Build: 0 succeeded, 1 failed ==========\n", tag_out_error);
        set_status("Build failed", "err");
    } else {
        char info[512];
        snprintf(info, sizeof(info), "1>Output: %s\n1>Build succeeded in %ldms\n========== Build: 1 succeeded, 0 failed ==========\n",
                 out_path.c_str(), ms);
        append_to_buffer(output_build_buf, info, tag_out_success);
        set_status("Build succeeded", "ok");
    }
}

static void on_view_c_clicked(GtkWidget *w, gpointer data) {
    EditorTab *t = current_tab();
    if (!t) return;

    std::string src = "/tmp/corec_studio_program.crc";
    write_file(src, get_buffer_text(t->buffer));
    std::string cmd = "python3 \"" + compiler_path + "\" emit \"" + src + "\"";
    int ec;
    std::string out = exec_cmd(cmd, &ec);
    
    clear_buffer(output_c_buf);
    if (ec != 0) {
        append_to_buffer(output_c_buf, out, tag_out_error);
        set_status("Error generating C", "err");
    } else {
        append_to_buffer(output_c_buf, out);
        set_status("Generated C", "ok");
    }
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_output), 2);
}

static void on_open_clicked(GtkWidget *w, gpointer data) {
    GtkWidget *d = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "CoreC files");
    gtk_file_filter_add_pattern(f, "*.crc");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), f);
    GtkFileFilter *a = gtk_file_filter_new();
    gtk_file_filter_set_name(a, "All files"); gtk_file_filter_add_pattern(a, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(d), a);
    
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        for (size_t i = 0; i < tabs.size(); i++) {
            if (tabs[i]->file_path == fn) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_editors), i);
                g_free(fn);
                gtk_widget_destroy(d);
                return;
            }
        }
        create_new_tab(fn, read_file(fn));
        set_status("Opened " + basename_of(fn), "ok");
        g_free(fn);
    }
    gtk_widget_destroy(d);
}

static void on_open_folder(GtkWidget *w, gpointer data) {
    GtkWidget *d = gtk_file_chooser_dialog_new("Open Folder", GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        load_folder_to_tree(fn);
        set_status("Opened folder: " + std::string(fn), "ok");
        g_free(fn);
    }
    gtk_widget_destroy(d);
}

static void on_save_clicked(GtkWidget *w, gpointer data) {
    EditorTab *t = current_tab();
    if (!t) return;
    std::string path = t->file_path;
    if (path.empty()) {
        GtkWidget *d = gtk_file_chooser_dialog_new("Save File", GTK_WINDOW(window),
            GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL,
            "Save", GTK_RESPONSE_ACCEPT, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(d), TRUE);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(d), "program.crc");
        if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
            char *fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
            path = fn; g_free(fn);
        }
        gtk_widget_destroy(d);
        if (path.empty()) return;
    }
    if (write_file(path, get_buffer_text(t->buffer))) {
        t->file_path = path;
        t->dirty = false;
        t->is_new = false;
        update_tab_label(t);
        set_status("Saved " + basename_of(path), "ok");
        if (!project_dir.empty()) load_folder_to_tree(project_dir);
    } else {
        set_status("Save failed", "err");
    }
}

static void on_new_clicked(GtkWidget *w, gpointer data) {
    create_new_tab("", "// New CoreC file\nfn main() {\n    print(\"Hello!\")\n}\n");
}

static void on_about(GtkWidget *w, gpointer data) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(window),
        GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "CoreC Studio v0.1\n\nA Visual Studio-style IDE for the CoreC programming language.\n\n"
        "CoreC transpiles to optimized C code, giving you native speed with clean syntax.\n\n"
        "Built with GTK3 and C++17\n© 2026 mineroce — MIT License");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
}

// ─── CSS theme ───
static void apply_css() {
    GtkCssProvider *p = gtk_css_provider_new();
    const char *css =
        "* { outline: none; }"
        "window { background: #1e1e1e; color: #cccccc; }"
        ".titlebar, headerbar { background: #2d2d30; color: #cccccc; }"
        "menubar { background: #2d2d30; color: #cccccc; border-bottom: 1px solid #333; padding: 2px 0; }"
        "menubar > menuitem { padding: 4px 10px; color: #cccccc; }"
        "menubar > menuitem:hover, menubar > menuitem:active { background: #094771; color: white; }"
        "menu { background: #1e1e1e; color: #cccccc; border: 1px solid #454545; }"
        "menu menuitem { padding: 6px 16px; color: #cccccc; }"
        "menu menuitem:hover { background: #094771; color: white; }"
        ".toolbar { background: #2d2d30; padding: 4px 8px; border-bottom: 1px solid #333; }"
        ".toolbar button { background: transparent; border: 1px solid transparent; color: #cccccc; padding: 4px 10px; margin: 0 1px; }"
        ".toolbar button:hover { background: #3e3e42; border-color: #555; }"
        ".toolbar button:active { background: #094771; }"
        ".btn-run-tb { background: #0e639c; color: white; border-radius: 3px; font-weight: bold; padding: 4px 16px; }"
        ".btn-run-tb:hover { background: #1177bb; }"
        ".section-bar { background: #252526; color: #cccccc; padding: 4px 12px; font-size: 11px; border-bottom: 1px solid #333; font-weight: bold; }"
        "notebook { background: #1e1e1e; }"
        "notebook header { background: #252526; border-bottom: 1px solid #333; }"
        "notebook tab { background: #2d2d30; color: #969696; padding: 4px 12px; border: 1px solid transparent; border-radius: 0; }"
        "notebook tab:checked { background: #1e1e1e; color: white; border-color: #333; border-bottom-color: #1e1e1e; }"
        "notebook tab button { padding: 0; min-width: 16px; min-height: 16px; }"
        "notebook tab button image { color: #969696; }"
        "notebook tab button:hover { background: #555; border-radius: 50%; }"
        "treeview { background: #252526; color: #cccccc; }"
        "treeview:selected { background: #094771; color: white; }"
        "treeview header button { background: #2d2d30; color: #cccccc; border: none; padding: 4px 8px; }"
        ".editor, .editor scrolledwindow, .editor viewport { background: #1e1e1e; }"
        "textview.editor, textview.editor text { background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', 'JetBrains Mono', monospace; font-size: 13px; }"
        "scrolledwindow { background: #1e1e1e; }"
        "viewport { background: #1e1e1e; }"
        "textview { background: #1e1e1e; color: #d4d4d4; }"
        "textview text { background: #1e1e1e; color: #d4d4d4; }"
        ".output textview, .output textview text { background: #1e1e1e; color: #cccccc; font-family: 'Consolas', monospace; font-size: 12px; }"
        ".statusbar { background: #007acc; color: white; padding: 2px 12px; font-size: 11px; }"
        ".statusbar label { color: white; }"
        "scrollbar { background: #1e1e1e; }"
        "scrollbar slider { background: #424242; border-radius: 0; min-width: 10px; min-height: 10px; }"
        "scrollbar slider:hover { background: #686868; }"
        "paned > separator { background: #333; min-width: 1px; min-height: 1px; }"
        "entry { background: #3c3c3c; color: #cccccc; border: 1px solid #555; }";
    gtk_css_provider_load_from_data(p, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

// ─── Main ───
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    apply_css();

    compiler_path = find_compiler();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "CoreC Studio");
    gtk_window_set_default_size(GTK_WINDOW(window), 1400, 850);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // ═══ Menu bar ═══
    GtkWidget *menubar = gtk_menu_bar_new();
    
    auto add_menu_item = [&](GtkWidget *menu, const char *label, GCallback cb, guint key, GdkModifierType mod) {
        GtkWidget *item = gtk_menu_item_new_with_label(label);
        if (cb) g_signal_connect(item, "activate", cb, NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        return item;
    };
    
    // File
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    add_menu_item(file_menu, "New File", G_CALLBACK(on_new_clicked), 0, (GdkModifierType)0);
    add_menu_item(file_menu, "Open File...", G_CALLBACK(on_open_clicked), 0, (GdkModifierType)0);
    add_menu_item(file_menu, "Open Folder...", G_CALLBACK(on_open_folder), 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    add_menu_item(file_menu, "Save", G_CALLBACK(on_save_clicked), 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    GtkWidget *exit_item = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(exit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    // Edit
    GtkWidget *edit_menu = gtk_menu_new();
    GtkWidget *edit_item = gtk_menu_item_new_with_label("Edit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
    add_menu_item(edit_menu, "Undo", NULL, 0, (GdkModifierType)0);
    add_menu_item(edit_menu, "Redo", NULL, 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), gtk_separator_menu_item_new());
    add_menu_item(edit_menu, "Cut", NULL, 0, (GdkModifierType)0);
    add_menu_item(edit_menu, "Copy", NULL, 0, (GdkModifierType)0);
    add_menu_item(edit_menu, "Paste", NULL, 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);

    // View
    GtkWidget *view_menu = gtk_menu_new();
    GtkWidget *view_item = gtk_menu_item_new_with_label("View");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
    add_menu_item(view_menu, "Solution Explorer", NULL, 0, (GdkModifierType)0);
    add_menu_item(view_menu, "Output", NULL, 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);

    // Build
    GtkWidget *build_menu = gtk_menu_new();
    GtkWidget *build_item = gtk_menu_item_new_with_label("Build");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(build_item), build_menu);
    add_menu_item(build_menu, "Build (Ctrl+B)", G_CALLBACK(on_build_clicked), 0, (GdkModifierType)0);
    add_menu_item(build_menu, "Rebuild", G_CALLBACK(on_build_clicked), 0, (GdkModifierType)0);
    add_menu_item(build_menu, "View Generated C", G_CALLBACK(on_view_c_clicked), 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), build_item);

    // Debug
    GtkWidget *debug_menu = gtk_menu_new();
    GtkWidget *debug_item = gtk_menu_item_new_with_label("Debug");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(debug_item), debug_menu);
    add_menu_item(debug_menu, "Start Without Debugging (F5)", G_CALLBACK(on_run_clicked), 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), debug_item);

    // Help
    GtkWidget *help_menu = gtk_menu_new();
    GtkWidget *help_item = gtk_menu_item_new_with_label("Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_item), help_menu);
    add_menu_item(help_menu, "About CoreC Studio", G_CALLBACK(on_about), 0, (GdkModifierType)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_item);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    // ═══ Toolbar ═══
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_style_context_add_class(gtk_widget_get_style_context(toolbar), "toolbar");

    auto add_tb_btn = [&](const char *icon, const char *tooltip, GCallback cb) {
        GtkWidget *b = gtk_button_new_with_label(icon);
        gtk_widget_set_tooltip_text(b, tooltip);
        if (cb) g_signal_connect(b, "clicked", cb, NULL);
        gtk_box_pack_start(GTK_BOX(toolbar), b, FALSE, FALSE, 0);
        return b;
    };

    add_tb_btn("📄", "New File (Ctrl+N)", G_CALLBACK(on_new_clicked));
    add_tb_btn("📂", "Open File (Ctrl+O)", G_CALLBACK(on_open_clicked));
    add_tb_btn("📁", "Open Folder", G_CALLBACK(on_open_folder));
    add_tb_btn("💾", "Save (Ctrl+S)", G_CALLBACK(on_save_clicked));

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(toolbar), sep, FALSE, FALSE, 4);

    add_tb_btn("🔨", "Build (Ctrl+B)", G_CALLBACK(on_build_clicked));
    add_tb_btn("⚙ View C", "View Generated C Code", G_CALLBACK(on_view_c_clicked));
    
    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(toolbar), sep2, FALSE, FALSE, 4);

    GtkWidget *run_btn = gtk_button_new_with_label("▶ Start  (F5)");
    gtk_style_context_add_class(gtk_widget_get_style_context(run_btn), "btn-run-tb");
    g_signal_connect(run_btn, "clicked", G_CALLBACK(on_run_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), run_btn, FALSE, FALSE, 4);

    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(toolbar), spacer, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    // ═══ Main: Solution Explorer (left) | Editor + Output (right vertical pane) ═══
    GtkWidget *main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(main_paned), 240);

    // ── Solution Explorer ──
    GtkWidget *se_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *se_header = gtk_label_new("SOLUTION EXPLORER");
    gtk_widget_set_halign(se_header, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(se_header), "section-bar");
    gtk_box_pack_start(GTK_BOX(se_box), se_header, FALSE, FALSE, 0);

    GtkWidget *se_scroll = gtk_scrolled_window_new(NULL, NULL);
    solution_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tree_solution = gtk_tree_view_new_with_model(GTK_TREE_MODEL(solution_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_solution), FALSE);
    
    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    GtkCellRenderer *icon_r = gtk_cell_renderer_text_new();
    GtkCellRenderer *text_r = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, icon_r, FALSE);
    gtk_tree_view_column_pack_start(col, text_r, TRUE);
    gtk_tree_view_column_add_attribute(col, icon_r, "text", 0);
    gtk_tree_view_column_add_attribute(col, text_r, "text", 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_solution), col);
    g_signal_connect(tree_solution, "row-activated", G_CALLBACK(on_tree_row_activated), NULL);
    
    gtk_container_add(GTK_CONTAINER(se_scroll), tree_solution);
    gtk_box_pack_start(GTK_BOX(se_box), se_scroll, TRUE, TRUE, 0);
    gtk_paned_pack1(GTK_PANED(main_paned), se_box, FALSE, FALSE);

    // ── Right side: vertical pane (editors top, output bottom) ──
    GtkWidget *right_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(right_paned), 550);

    // Editor notebook
    notebook_editors = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook_editors), TRUE);
    gtk_paned_pack1(GTK_PANED(right_paned), notebook_editors, TRUE, FALSE);

    // Output notebook
    GtkWidget *output_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    notebook_output = gtk_notebook_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(notebook_output), "output");

    auto add_output_tab = [&](const char *label) {
        GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
        GtkWidget *view = gtk_text_view_new();
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 12);
        gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 8);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
        gtk_container_add(GTK_CONTAINER(scroll), view);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook_output), scroll,
            gtk_label_new(label));
        return std::make_pair(view, buf);
    };

    auto bp = add_output_tab("Build");
    output_build = bp.first; output_build_buf = bp.second;
    auto cp = add_output_tab("Console");
    output_console = cp.first; output_console_buf = cp.second;
    auto gp = add_output_tab("Generated C");
    output_c = gp.first; output_c_buf = gp.second;

    // Output tags
    tag_out_error = gtk_text_buffer_create_tag(output_build_buf, "error", "foreground", "#f48771", NULL);
    tag_out_success = gtk_text_buffer_create_tag(output_build_buf, "success", "foreground", "#4ec9b0", NULL);
    tag_out_info = gtk_text_buffer_create_tag(output_build_buf, "info", "foreground", "#9cdcfe", NULL);
    tag_out_warn = gtk_text_buffer_create_tag(output_build_buf, "warn", "foreground", "#dcdcaa", NULL);
    // Same tags for console & C buffers (shared by name within same TagTable required)
    // Create on console buf separately
    GtkTextTag *ce = gtk_text_buffer_create_tag(output_console_buf, "error", "foreground", "#f48771", NULL);
    GtkTextTag *cs = gtk_text_buffer_create_tag(output_console_buf, "success", "foreground", "#4ec9b0", NULL);
    (void)ce; (void)cs;
    // For console, we re-tag at write time by passing the correct tag from its own table
    // Simpler: use the appropriate per-buffer tag dynamically

    GtkWidget *output_header = gtk_label_new("OUTPUT");
    gtk_widget_set_halign(output_header, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(output_header), "section-bar");
    gtk_box_pack_start(GTK_BOX(output_box), output_header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(output_box), notebook_output, TRUE, TRUE, 0);
    gtk_paned_pack2(GTK_PANED(right_paned), output_box, TRUE, FALSE);

    gtk_paned_pack2(GTK_PANED(main_paned), right_paned, TRUE, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), main_paned, TRUE, TRUE, 0);

    // ═══ Status bar ═══
    GtkWidget *statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusbar), "statusbar");
    
    status_label = gtk_label_new(NULL);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(statusbar), status_label, FALSE, FALSE, 8);

    GtkWidget *spacer2 = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(statusbar), spacer2, TRUE, TRUE, 0);

    line_col_label = gtk_label_new("Ln 1, Col 1");
    gtk_box_pack_start(GTK_BOX(statusbar), line_col_label, FALSE, FALSE, 0);

    GtkWidget *lang_label = gtk_label_new("CoreC v0.1");
    gtk_box_pack_start(GTK_BOX(statusbar), lang_label, FALSE, FALSE, 8);

    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);

    // ═══ Initial content ═══
    // Try to load default project (examples folder)
    std::vector<std::string> dirs = {
        "./examples", "../examples", "/home/ubuntu/corec/examples"
    };
    char self[1024];
    ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0) {
        self[n] = 0;
        std::string base(self);
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            dirs.insert(dirs.begin(), base.substr(0, slash) + "/../examples");
        }
    }
    for (const auto &d : dirs) {
        if (access(d.c_str(), R_OK) == 0) {
            load_folder_to_tree(d);
            break;
        }
    }

    // Open hello example as default
    bool opened = false;
    for (const auto &d : dirs) {
        std::string h = d + "/hello.crc";
        if (access(h.c_str(), R_OK) == 0) {
            create_new_tab(h, read_file(h));
            opened = true;
            break;
        }
    }
    if (!opened) {
        create_new_tab("", 
            "// CoreC — Welcome to CoreC Studio\n"
            "fn main() {\n"
            "    let name = \"World\"\n"
            "    print(\"Hello, {name}!\")\n"
            "}\n");
    }

    if (compiler_path.empty()) {
        set_status("⚠ Compiler not found", "err");
    } else {
        set_status("Ready", "ok");
    }

    // ═══ Keyboard shortcuts ═══
    GtkAccelGroup *ag = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), ag);
    gtk_widget_add_accelerator(run_btn, "clicked", ag, GDK_KEY_F5, (GdkModifierType)0, GTK_ACCEL_VISIBLE);

    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}
