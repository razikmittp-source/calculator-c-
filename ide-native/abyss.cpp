#include <gtk/gtk.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <signal.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>


static const char *kCss =
    "window, .background { background-color: #07090c; }\n"
    "* { color: #8993a0; font-family: \"Iosevka\", \"JetBrains Mono\", \"Source Code Pro\", monospace; }\n"
    "headerbar { background: #0b1015; min-height: 34px; padding: 0 10px; border: none; box-shadow: none; }\n"
    "headerbar label { color: #c7cdd6; letter-spacing: 4px; }\n"
    "button { background: transparent; color: #8993a0; border: 1px solid #161c24; padding: 4px 10px; min-height: 0; }\n"
    "button:hover { color: #c7cdd6; }\n"
    ".title { color: #c7cdd6; letter-spacing: 8px; font-size: 13px; }\n"
    ".sub { color: #4b5360; letter-spacing: 3px; font-size: 11px; }\n"
    ".section-header { color: #4b5360; font-size: 10px; letter-spacing: 4px; padding: 12px 14px 4px 14px; }\n"
    ".panel { background: #0b1015; }\n"
    ".panel-border-right { border-right: 1px solid #161c24; }\n"
    ".panel-border-left { border-left: 1px solid #161c24; }\n"
    "list, list row { background: transparent; }\n"
    "row { padding: 4px 14px; color: #8993a0; }\n"
    "row:selected { background: rgba(255,255,255,0.04); color: #c7cdd6; box-shadow: inset 2px 0 0 #6b3a3a; }\n"
    "row:hover { background: rgba(255,255,255,0.025); color: #c7cdd6; }\n"
    "row label { color: inherit; }\n"
    "textview text, textview { background: #07090c; color: #c7cdd6; caret-color: #6b3a3a; }\n"
    ".sidebar-text text, .sidebar-text { background: #0b1015; color: #8993a0; }\n"
    ".console-text text, .console-text { background: #0f161d; color: #8993a0; }\n"
    ".console-head { background: #0b1015; padding: 6px 14px; color: #4b5360; border-top: 1px solid #161c24; border-bottom: 1px solid #161c24; }\n"
    ".run-button { color: #c7cdd6; border: 1px solid #6b3a3a; padding: 4px 16px; letter-spacing: 4px; }\n"
    ".run-button:hover { background: rgba(107,58,58,0.18); }\n"
    ".statusbar { background: #0b1015; color: #4b5360; padding: 4px 14px; font-size: 11px; border-top: 1px solid #161c24; }\n"
    "entry { background: transparent; color: #c7cdd6; border: 1px solid #161c24; padding: 4px 8px; }\n"
    "entry selection { background: rgba(107,58,58,0.35); color: #c7cdd6; }\n"
    "scrollbar slider { background: #1a2028; min-width: 6px; min-height: 6px; }\n"
    "scrollbar trough { background: transparent; }\n"
    "popover, popover .background { background: #0b1015; }\n";


struct Document {
    std::string name;
    std::string source;
    long saved_at_ms = 0;
    bool dirty = false;
};


static const char *kMarkQuotes[] = {
    "the keyboard is the only thing that doesn't ask how i'm doing.",
    "i kept her name in a variable so it would still exist somewhere.",
    "the language doesn't crash. it just stops. i wish i'd learned that earlier.",
    "i wrote a function called fade. i call it more than i should.",
    "every file ends with a newline. every person ends with one too.",
    "if the program survives the night it's already a victory.",
    "i can't fix the world but i can fix the indentation.",
    "the rain on the window has its own syntax. i'm still learning it.",
    "i deleted the catch block. nothing to catch anymore.",
    "there is no exception handler for missing her.",
    "void isn't empty. void is what's left after the garbage collector takes everything you loved.",
    "perhaps tomorrow. perhaps not. that's the only branch i write now.",
    "drown is honest. it loops until something stops it. usually nothing stops it.",
    "i write voice main() because i'm trying to remember mine.",
    "the cursor blinks slower than my heart now. that's on purpose.",
    "i pushed the first commit at 4:17 a.m. there was no one to review it.",
    "the compiler doesn't judge me. the compiler doesn't even exist. it's just me, parsing.",
    "she used to call me at this hour. now i call functions.",
    "memory is a type for a reason. you can't free it just because you want to.",
    "pain is a float because integers couldn't hold it.",
};

static constexpr int kMarkQuoteCount = sizeof(kMarkQuotes) / sizeof(kMarkQuotes[0]);


struct StarterFile {
    const char *name;
    const char *source;
};


static const StarterFile kStarters[] = {
    {"here.crc",
     "voice main() {\n"
     "    keep name: echo = \"i am here\"\n"
     "    whisper(name)\n"
     "}\n"},
    {"rain.crc",
     "voice fall(drops: memory) {\n"
     "    keep i: memory = 0\n"
     "    drown i < drops {\n"
     "        whisper(\".\")\n"
     "        i = i + 1\n"
     "    }\n"
     "}\n"
     "\n"
     "voice main() {\n"
     "    whisper(\"the rain started at three.\")\n"
     "    whisper(\"nobody noticed.\")\n"
     "    fall(7)\n"
     "    whisper(\"the room was quiet again.\")\n"
     "}\n"},
    {"letters.crc",
     "voice main() {\n"
     "    keep letters: echo = [\n"
     "        \"i wrote you a letter today.\",\n"
     "        \"i didn't send it.\",\n"
     "        \"i don't know your address anymore.\",\n"
     "        \"i think you would have liked the rain.\",\n"
     "        \"i'll keep writing anyway.\"\n"
     "    ]\n"
     "    bury line in letters {\n"
     "        whisper(line)\n"
     "    }\n"
     "}\n"},
    {"fade.crc",
     "voice greet(name: echo) -> echo {\n"
     "    perhaps name == \"\" {\n"
     "        fade \"i don't remember who you are.\"\n"
     "    }\n"
     "    fade \"hello, \" + name + \".\"\n"
     "}\n"
     "\n"
     "voice main() {\n"
     "    whisper(greet(\"mark\"))\n"
     "    whisper(greet(\"\"))\n"
     "    whisper(greet(\"anna\"))\n"
     "}\n"},
    {"heartbeat.crc",
     "voice heartbeat(times: memory) {\n"
     "    keep n: memory = 0\n"
     "    drown n < times {\n"
     "        perhaps n % 2 == 0 {\n"
     "            whisper(\"thump.\")\n"
     "        } otherwise {\n"
     "            whisper(\"...\")\n"
     "        }\n"
     "        n = n + 1\n"
     "    }\n"
     "}\n"
     "\n"
     "voice main() {\n"
     "    heartbeat(6)\n"
     "    whisper(\"then nothing.\")\n"
     "}\n"},
};


struct AppState {
    GtkWidget *window = nullptr;
    GtkWidget *editor_view = nullptr;
    GtkTextBuffer *editor_buffer = nullptr;
    GtkWidget *file_list = nullptr;
    GtkWidget *console_view = nullptr;
    GtkTextBuffer *console_buffer = nullptr;
    GtkWidget *thoughts_view = nullptr;
    GtkTextBuffer *thoughts_buffer = nullptr;
    GtkWidget *ai_log_view = nullptr;
    GtkTextBuffer *ai_log_buffer = nullptr;
    GtkWidget *ai_entry = nullptr;
    GtkWidget *status_name = nullptr;
    GtkWidget *status_lines = nullptr;
    GtkWidget *status_saved = nullptr;
    GtkWidget *status_rain = nullptr;
    GtkWidget *run_button = nullptr;
    GtkWidget *run_meta = nullptr;
    GtkTextTag *tomb_tag = nullptr;
    GtkTextTag *him_tag = nullptr;
    GtkTextTag *you_tag = nullptr;

    std::vector<Document> docs;
    int active_index = 0;
    long last_save_ms = 0;
    long forget_after_ms = 90 * 1000;
    bool rain_on = true;
    bool darkened = false;
    std::string interpreter_path;
    std::string thoughts_path;

    std::mt19937 rng{(unsigned)std::chrono::steady_clock::now().time_since_epoch().count()};

    std::atomic<bool> rain_running{false};
    std::thread rain_thread;
    pid_t rain_pid = -1;
};


static long now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}


static std::string read_file(const std::string &path) {
    std::ifstream fh(path);
    if (!fh) return {};
    std::stringstream ss;
    ss << fh.rdbuf();
    return ss.str();
}


static bool write_file(const std::string &path, const std::string &data) {
    std::ofstream fh(path);
    if (!fh) return false;
    fh << data;
    return true;
}


static bool path_exists(const std::string &path) {
    return access(path.c_str(), F_OK) == 0;
}


static std::string find_interpreter(const std::string &argv0) {
    const char *env = getenv("COREC_HOME");
    if (env) {
        std::string p = std::string(env) + "/compiler/corec.py";
        if (path_exists(p)) return p;
    }
    std::string self = argv0;
    char real[4096];
    ssize_t n = readlink("/proc/self/exe", real, sizeof(real) - 1);
    if (n > 0) {
        real[n] = '\0';
        self = real;
    }
    auto pos = self.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = self.substr(0, pos);
        std::vector<std::string> candidates = {
            dir + "/../compiler/corec.py",
            dir + "/../../compiler/corec.py",
            dir + "/corec.py",
        };
        for (const auto &c : candidates) {
            if (path_exists(c)) return c;
        }
    }
    return "corec";
}


static std::string ensure_config_dir() {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char *home = getenv("HOME");
        base = home ? std::string(home) + "/.config" : "./.config";
    }
    std::string dir = base + "/abyss";
    mkdir(base.c_str(), 0700);
    mkdir(dir.c_str(), 0700);
    return dir;
}


static std::string text_of(GtkTextBuffer *buffer) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *raw = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    std::string out = raw ? raw : "";
    g_free(raw);
    return out;
}


static int line_count(GtkTextBuffer *buffer) {
    return gtk_text_buffer_get_line_count(buffer);
}


static void set_text(GtkTextBuffer *buffer, const std::string &text) {
    gtk_text_buffer_set_text(buffer, text.c_str(), (int)text.size());
}


static void append_console(AppState *st, const std::string &text, bool is_tomb) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(st->console_buffer, &end);
    if (is_tomb && st->tomb_tag) {
        gtk_text_buffer_insert_with_tags(st->console_buffer, &end, text.c_str(), (int)text.size(), st->tomb_tag, NULL);
    } else {
        gtk_text_buffer_insert(st->console_buffer, &end, text.c_str(), (int)text.size());
    }
    gtk_text_buffer_get_end_iter(st->console_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(st->console_view), &end, 0.0, FALSE, 0, 0);
}


static void clear_console(AppState *st) {
    gtk_text_buffer_set_text(st->console_buffer, "", 0);
}


static void switch_to(AppState *st, int idx);


static void rebuild_file_list(AppState *st) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(st->file_list));
    for (GList *it = children; it != NULL; it = it->next) {
        gtk_widget_destroy(GTK_WIDGET(it->data));
    }
    g_list_free(children);

    for (size_t i = 0; i < st->docs.size(); ++i) {
        GtkWidget *row = gtk_list_box_row_new();
        std::string label_text = st->docs[i].name + (st->docs[i].dirty ? " *" : "");
        GtkWidget *lbl = gtk_label_new(label_text.c_str());
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_container_add(GTK_CONTAINER(row), lbl);
        gtk_list_box_insert(GTK_LIST_BOX(st->file_list), row, -1);
        g_object_set_data(G_OBJECT(row), "doc-index", GINT_TO_POINTER((int)i));
    }
    gtk_widget_show_all(st->file_list);

    GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(st->file_list), st->active_index);
    if (row) {
        gtk_list_box_select_row(GTK_LIST_BOX(st->file_list), row);
    }
}


static void refresh_status(AppState *st) {
    if (st->active_index >= 0 && st->active_index < (int)st->docs.size()) {
        const auto &doc = st->docs[st->active_index];
        std::string name = doc.name + (doc.dirty ? " *" : "");
        gtk_label_set_text(GTK_LABEL(st->status_name), name.c_str());
    }
    int n = line_count(st->editor_buffer);
    bool forgotten = st->last_save_ms > 0 && (now_ms() - st->last_save_ms) > st->forget_after_ms;
    char buf[64];
    if (forgotten) {
        snprintf(buf, sizeof(buf), "%d lines (forgotten)", n);
    } else {
        snprintf(buf, sizeof(buf), "%d lines", n);
    }
    gtk_label_set_text(GTK_LABEL(st->status_lines), buf);

    if (st->last_save_ms == 0) {
        gtk_label_set_text(GTK_LABEL(st->status_saved), "never saved");
    } else {
        long ago = (now_ms() - st->last_save_ms) / 1000;
        char sbuf[64];
        if (ago < 5) snprintf(sbuf, sizeof(sbuf), "just saved");
        else if (ago < 60) snprintf(sbuf, sizeof(sbuf), "%lds ago", ago);
        else if (ago < 3600) snprintf(sbuf, sizeof(sbuf), "%ldm ago", ago / 60);
        else snprintf(sbuf, sizeof(sbuf), "%ldh ago", ago / 3600);
        gtk_label_set_text(GTK_LABEL(st->status_saved), sbuf);
    }
}


static gboolean status_tick(gpointer data) {
    refresh_status(static_cast<AppState *>(data));
    return G_SOURCE_CONTINUE;
}


static void switch_to(AppState *st, int idx) {
    if (idx < 0 || idx >= (int)st->docs.size()) return;
    if (st->active_index >= 0 && st->active_index < (int)st->docs.size()) {
        st->docs[st->active_index].source = text_of(st->editor_buffer);
    }
    st->active_index = idx;
    set_text(st->editor_buffer, st->docs[idx].source);
    refresh_status(st);
}


static void on_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    if (!row) return;
    AppState *st = static_cast<AppState *>(data);
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "doc-index"));
    if (idx != st->active_index) {
        switch_to(st, idx);
    }
}


static void on_new_doc(GtkButton *btn, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    Document doc;
    char buf[64];
    snprintf(buf, sizeof(buf), "page_%ld.crc", now_ms() % 100000);
    doc.name = buf;
    doc.source = "";
    doc.dirty = true;
    st->docs.push_back(doc);
    st->active_index = (int)st->docs.size() - 1;
    rebuild_file_list(st);
    set_text(st->editor_buffer, "");
    refresh_status(st);
    gtk_widget_grab_focus(st->editor_view);
}


static void on_editor_changed(GtkTextBuffer *buffer, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    if (st->active_index >= 0 && st->active_index < (int)st->docs.size()) {
        st->docs[st->active_index].source = text_of(buffer);
        if (!st->docs[st->active_index].dirty) {
            st->docs[st->active_index].dirty = true;
            rebuild_file_list(st);
        }
    }
    refresh_status(st);
}


static void save_active(AppState *st) {
    if (st->active_index < 0 || st->active_index >= (int)st->docs.size()) return;
    auto &doc = st->docs[st->active_index];
    doc.source = text_of(st->editor_buffer);
    doc.dirty = false;
    doc.saved_at_ms = now_ms();
    st->last_save_ms = doc.saved_at_ms;
    std::string dir = ensure_config_dir();
    std::string path = dir + "/" + doc.name;
    write_file(path, doc.source);
    rebuild_file_list(st);
    refresh_status(st);
}


static gboolean on_editor_key(GtkWidget *w, GdkEventKey *evt, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    if ((evt->state & GDK_CONTROL_MASK) && (evt->keyval == GDK_KEY_s || evt->keyval == GDK_KEY_S)) {
        save_active(st);
        return TRUE;
    }
    return FALSE;
}


static void on_thoughts_changed(GtkTextBuffer *buffer, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    std::string content = text_of(buffer);
    write_file(st->thoughts_path, content);
}


static gboolean unset_darkened(gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    GtkStyleContext *ctx = gtk_widget_get_style_context(st->window);
    gtk_style_context_remove_class(ctx, "darkened");
    st->darkened = false;
    return G_SOURCE_REMOVE;
}


static void flash_dark(AppState *st) {
    if (st->darkened) return;
    GtkStyleContext *ctx = gtk_widget_get_style_context(st->window);
    gtk_style_context_add_class(ctx, "darkened");
    st->darkened = true;
    g_timeout_add(700, unset_darkened, st);
}


struct RunResult {
    std::string output;
    std::vector<std::string> tomb;
    long elapsed_ms = 0;
};


static RunResult run_program(AppState *st, const std::string &source) {
    RunResult r;
    char tmpl[] = "/tmp/abyss-XXXXXX.crc";
    int fd = mkstemps(tmpl, 4);
    if (fd < 0) {
        r.tomb.push_back("could not open a temporary file.");
        return r;
    }
    if (write(fd, source.c_str(), source.size()) < 0) {
        close(fd);
        unlink(tmpl);
        r.tomb.push_back("could not write source.");
        return r;
    }
    close(fd);

    std::string tomb_path = std::string(tmpl) + ".tomb";
    unlink(tomb_path.c_str());

    long started = now_ms();
    const char *interp = st->interpreter_path.c_str();
    gchar *cmd_argv[] = {
        (gchar *)"python3",
        (gchar *)interp,
        (gchar *)"run",
        (gchar *)tmpl,
        NULL,
    };
    gchar *out = NULL;
    gchar *err = NULL;
    gint status = 0;
    GError *gerr = NULL;
    gboolean ok = g_spawn_sync(NULL, cmd_argv, NULL,
                               G_SPAWN_SEARCH_PATH,
                               NULL, NULL,
                               &out, &err,
                               &status, &gerr);
    r.elapsed_ms = now_ms() - started;
    if (!ok) {
        r.tomb.push_back(gerr ? gerr->message : "could not invoke the interpreter.");
        if (gerr) g_error_free(gerr);
        if (out) g_free(out);
        if (err) g_free(err);
        unlink(tmpl);
        return r;
    }
    if (out) {
        r.output = out;
        g_free(out);
    }
    if (err && *err) {
        r.tomb.push_back(err);
    }
    if (err) g_free(err);

    std::ifstream tfh(tomb_path);
    if (tfh) {
        std::string line;
        while (std::getline(tfh, line)) {
            r.tomb.push_back(line);
        }
    }
    unlink(tmpl);
    unlink(tomb_path.c_str());
    return r;
}


static void on_run_clicked(GtkButton *btn, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    std::string source = text_of(st->editor_buffer);
    clear_console(st);
    gtk_widget_set_sensitive(st->run_button, FALSE);
    gtk_label_set_text(GTK_LABEL(st->run_meta), "thinking…");
    while (gtk_events_pending()) gtk_main_iteration();

    RunResult r = run_program(st, source);

    if (!r.output.empty()) {
        std::string out = r.output;
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        if (!out.empty()) {
            append_console(st, out + "\n", false);
        }
    }
    if (!r.tomb.empty()) {
        append_console(st, "\n— tomb —\n", true);
        for (const auto &line : r.tomb) {
            append_console(st, line + "\n", true);
        }
        flash_dark(st);
    } else if (r.output.empty()) {
        append_console(st, "(silence.)\n", true);
    }

    char meta[64];
    snprintf(meta, sizeof(meta), "%ld ms", r.elapsed_ms);
    gtk_label_set_text(GTK_LABEL(st->run_meta), meta);
    gtk_widget_set_sensitive(st->run_button, TRUE);
}


static void append_him(AppState *st, const std::string &text, GtkTextTag *tag) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(st->ai_log_buffer, &end);
    if (tag) {
        gtk_text_buffer_insert_with_tags(st->ai_log_buffer, &end, text.c_str(), (int)text.size(), tag, NULL);
    } else {
        gtk_text_buffer_insert(st->ai_log_buffer, &end, text.c_str(), (int)text.size());
    }
    gtk_text_buffer_get_end_iter(st->ai_log_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(st->ai_log_view), &end, 0.0, FALSE, 0, 0);
}


static void say_a_quote(AppState *st) {
    std::uniform_int_distribution<int> pick(0, kMarkQuoteCount - 1);
    int i = pick(st->rng);
    std::string s = std::string("— ") + kMarkQuotes[i] + "\n";
    append_him(st, s, st->him_tag);
}


static void on_ai_send(GtkWidget *w, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    const char *raw = gtk_entry_get_text(GTK_ENTRY(st->ai_entry));
    if (!raw || !*raw) return;
    std::string line = std::string("you: ") + raw + "\n";
    append_him(st, line, st->you_tag);
    gtk_entry_set_text(GTK_ENTRY(st->ai_entry), "");
    say_a_quote(st);
}


static void on_theme_clicked(GtkButton *btn, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(st->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_OTHER,
        GTK_BUTTONS_CLOSE,
        "%s",
        "there is no light here.\n\nmark removed the switch. it was the first thing he committed.");
    gtk_window_set_title(GTK_WINDOW(dialog), "abyss");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}


static void stop_rain(AppState *st) {
    if (!st->rain_running.load()) return;
    st->rain_running.store(false);
    if (st->rain_pid > 0) {
        kill(st->rain_pid, SIGTERM);
        int status = 0;
        waitpid(st->rain_pid, &status, 0);
        st->rain_pid = -1;
    }
    if (st->rain_thread.joinable()) st->rain_thread.join();
}


static void rain_worker(AppState *st) {
    int fds[2];
    if (pipe(fds) != 0) {
        st->rain_running.store(false);
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]); close(fds[1]);
        st->rain_running.store(false);
        return;
    }
    if (pid == 0) {
        dup2(fds[0], 0);
        close(fds[0]);
        close(fds[1]);
        execlp("paplay", "paplay", "--raw", "--rate=22050", "--format=s16le", "--channels=1", (char *)NULL);
        execlp("aplay", "aplay", "-q", "-f", "S16_LE", "-r", "22050", "-c", "1", (char *)NULL);
        _exit(127);
    }
    close(fds[0]);
    st->rain_pid = pid;

    std::mt19937 gen{(unsigned)now_ms()};
    std::uniform_real_distribution<double> u(-1.0, 1.0);
    double last = 0.0;
    const int chunk = 1024;
    std::vector<int16_t> buf(chunk);
    while (st->rain_running.load()) {
        for (int i = 0; i < chunk; ++i) {
            double white = u(gen);
            last = (last + 0.02 * white) / 1.02;
            double v = last * 12.0;
            if (v > 1.0) v = 1.0;
            if (v < -1.0) v = -1.0;
            buf[i] = (int16_t)(v * 18000.0);
        }
        ssize_t total = (ssize_t)(buf.size() * sizeof(int16_t));
        const char *raw = reinterpret_cast<const char *>(buf.data());
        ssize_t wrote = 0;
        while (wrote < total && st->rain_running.load()) {
            ssize_t n = write(fds[1], raw + wrote, total - wrote);
            if (n <= 0) { st->rain_running.store(false); break; }
            wrote += n;
        }
    }
    close(fds[1]);
}


static void start_rain(AppState *st) {
    if (st->rain_running.load()) return;
    st->rain_running.store(true);
    st->rain_thread = std::thread(rain_worker, st);
}


static void on_rain_clicked(GtkButton *btn, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    if (st->rain_on) {
        st->rain_on = false;
        stop_rain(st);
        gtk_button_set_label(GTK_BUTTON(st->status_rain), "rain: off");
    } else {
        st->rain_on = true;
        start_rain(st);
        gtk_button_set_label(GTK_BUTTON(st->status_rain), "rain: on");
    }
}


static void on_window_destroy(GtkWidget *w, gpointer data) {
    AppState *st = static_cast<AppState *>(data);
    stop_rain(st);
    gtk_main_quit();
}


static void install_css() {
    GtkCssProvider *prov = gtk_css_provider_new();
    gtk_css_provider_load_from_data(prov, kCss, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(prov),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(prov);
}


static GtkWidget *make_label(const char *text, const char *css_class, GtkAlign halign) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_set_halign(lbl, halign);
    if (css_class) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(lbl);
        gtk_style_context_add_class(ctx, css_class);
    }
    return lbl;
}


int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    install_css();

    AppState st;
    st.interpreter_path = find_interpreter(argv[0]);
    std::string cfg_dir = ensure_config_dir();
    st.thoughts_path = cfg_dir + "/thoughts.txt";

    for (const auto &s : kStarters) {
        Document d;
        d.name = s.name;
        d.source = s.source;
        std::string saved = read_file(cfg_dir + "/" + s.name);
        if (!saved.empty()) d.source = saved;
        st.docs.push_back(d);
    }

    st.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(st.window), "abyss");
    gtk_window_set_default_size(GTK_WINDOW(st.window), 1200, 760);
    g_signal_connect(st.window, "destroy", G_CALLBACK(on_window_destroy), &st);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    GtkWidget *title = gtk_label_new("abyss");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "title");
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header), title);
    GtkWidget *theme_btn = gtk_button_new_with_label("light theme");
    g_signal_connect(theme_btn, "clicked", G_CALLBACK(on_theme_clicked), &st);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), theme_btn);
    gtk_window_set_titlebar(GTK_WINDOW(st.window), header);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(st.window), root);

    GtkWidget *paned_main = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(paned_main), 220);
    gtk_paned_set_wide_handle(GTK_PANED(paned_main), FALSE);
    gtk_box_pack_start(GTK_BOX(root), paned_main, TRUE, TRUE, 0);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(left_panel), "panel");
    gtk_style_context_add_class(gtk_widget_get_style_context(left_panel), "panel-border-right");
    gtk_paned_pack1(GTK_PANED(paned_main), left_panel, FALSE, FALSE);

    GtkWidget *files_header = make_label("documents", "section-header", GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(left_panel), files_header, FALSE, FALSE, 0);

    GtkWidget *files_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(files_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    st.file_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(st.file_list), GTK_SELECTION_BROWSE);
    g_signal_connect(st.file_list, "row-selected", G_CALLBACK(on_row_selected), &st);
    gtk_container_add(GTK_CONTAINER(files_scroll), st.file_list);
    gtk_box_pack_start(GTK_BOX(left_panel), files_scroll, TRUE, TRUE, 0);

    GtkWidget *new_btn = gtk_button_new_with_label("new empty page");
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_new_doc), &st);
    gtk_widget_set_margin_start(new_btn, 14);
    gtk_widget_set_margin_end(new_btn, 14);
    gtk_widget_set_margin_top(new_btn, 6);
    gtk_widget_set_margin_bottom(new_btn, 10);
    gtk_box_pack_start(GTK_BOX(left_panel), new_btn, FALSE, FALSE, 0);

    GtkWidget *paned_right = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(paned_right), 640);
    gtk_paned_pack2(GTK_PANED(paned_main), paned_right, TRUE, FALSE);

    GtkWidget *paned_center = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(paned_center), 460);
    gtk_paned_pack1(GTK_PANED(paned_right), paned_center, TRUE, FALSE);

    GtkWidget *editor_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editor_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    st.editor_view = gtk_text_view_new();
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(st.editor_view), 16);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(st.editor_view), 16);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(st.editor_view), 16);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(st.editor_view), 16);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(st.editor_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(st.editor_view), GTK_WRAP_NONE);
    st.editor_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st.editor_view));
    g_signal_connect(st.editor_buffer, "changed", G_CALLBACK(on_editor_changed), &st);
    g_signal_connect(st.editor_view, "key-press-event", G_CALLBACK(on_editor_key), &st);
    gtk_container_add(GTK_CONTAINER(editor_scroll), st.editor_view);
    gtk_paned_pack1(GTK_PANED(paned_center), editor_scroll, TRUE, FALSE);

    GtkWidget *console_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack2(GTK_PANED(paned_center), console_box, FALSE, FALSE);

    GtkWidget *console_head = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(console_head), "console-head");
    GtkWidget *console_lbl = make_label("console", NULL, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(console_head), console_lbl, TRUE, TRUE, 0);
    st.run_meta = make_label("", NULL, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(console_head), st.run_meta, FALSE, FALSE, 0);
    st.run_button = gtk_button_new_with_label("run");
    gtk_style_context_add_class(gtk_widget_get_style_context(st.run_button), "run-button");
    g_signal_connect(st.run_button, "clicked", G_CALLBACK(on_run_clicked), &st);
    gtk_box_pack_start(GTK_BOX(console_head), st.run_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(console_box), console_head, FALSE, FALSE, 0);

    GtkWidget *console_scroll = gtk_scrolled_window_new(NULL, NULL);
    st.console_view = gtk_text_view_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(st.console_view), "console-text");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(st.console_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(st.console_view), FALSE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(st.console_view), 14);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(st.console_view), 10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(st.console_view), 14);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(st.console_view), 10);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(st.console_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(st.console_view), GTK_WRAP_WORD_CHAR);
    st.console_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st.console_view));
    st.tomb_tag = gtk_text_buffer_create_tag(st.console_buffer, "tomb",
        "foreground", "#6b3a3a",
        "style", PANGO_STYLE_ITALIC,
        NULL);
    gtk_container_add(GTK_CONTAINER(console_scroll), st.console_view);
    gtk_box_pack_start(GTK_BOX(console_box), console_scroll, TRUE, TRUE, 0);

    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(right_panel), "panel");
    gtk_style_context_add_class(gtk_widget_get_style_context(right_panel), "panel-border-left");
    gtk_paned_pack2(GTK_PANED(paned_right), right_panel, FALSE, FALSE);

    GtkWidget *thoughts_header = make_label("thoughts", "section-header", GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(right_panel), thoughts_header, FALSE, FALSE, 0);

    GtkWidget *thoughts_scroll = gtk_scrolled_window_new(NULL, NULL);
    st.thoughts_view = gtk_text_view_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(st.thoughts_view), "sidebar-text");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(st.thoughts_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(st.thoughts_view), 14);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(st.thoughts_view), 14);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(st.thoughts_view), 6);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(st.thoughts_view), 6);
    st.thoughts_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st.thoughts_view));
    std::string saved_thoughts = read_file(st.thoughts_path);
    if (!saved_thoughts.empty()) set_text(st.thoughts_buffer, saved_thoughts);
    g_signal_connect(st.thoughts_buffer, "changed", G_CALLBACK(on_thoughts_changed), &st);
    gtk_container_add(GTK_CONTAINER(thoughts_scroll), st.thoughts_view);
    gtk_box_pack_start(GTK_BOX(right_panel), thoughts_scroll, TRUE, TRUE, 0);

    GtkWidget *him_header = make_label("him", "section-header", GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(right_panel), him_header, FALSE, FALSE, 0);

    GtkWidget *him_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(him_scroll, -1, 200);
    st.ai_log_view = gtk_text_view_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(st.ai_log_view), "sidebar-text");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(st.ai_log_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(st.ai_log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(st.ai_log_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(st.ai_log_view), 14);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(st.ai_log_view), 14);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(st.ai_log_view), 4);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(st.ai_log_view), 6);
    st.ai_log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(st.ai_log_view));
    st.him_tag = gtk_text_buffer_create_tag(st.ai_log_buffer, "him",
        "foreground", "#c7cdd6",
        "style", PANGO_STYLE_ITALIC,
        NULL);
    st.you_tag = gtk_text_buffer_create_tag(st.ai_log_buffer, "you",
        "foreground", "#4b5360",
        NULL);
    gtk_container_add(GTK_CONTAINER(him_scroll), st.ai_log_view);
    gtk_box_pack_start(GTK_BOX(right_panel), him_scroll, FALSE, TRUE, 0);

    GtkWidget *ai_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(ai_row, 14);
    gtk_widget_set_margin_end(ai_row, 14);
    gtk_widget_set_margin_top(ai_row, 6);
    gtk_widget_set_margin_bottom(ai_row, 10);
    st.ai_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(st.ai_entry), "say something");
    g_signal_connect(st.ai_entry, "activate", G_CALLBACK(on_ai_send), &st);
    gtk_box_pack_start(GTK_BOX(ai_row), st.ai_entry, TRUE, TRUE, 0);
    GtkWidget *send_btn = gtk_button_new_with_label("send");
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_ai_send), &st);
    gtk_box_pack_start(GTK_BOX(ai_row), send_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_panel), ai_row, FALSE, FALSE, 0);

    GtkWidget *statusbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_style_context_add_class(gtk_widget_get_style_context(statusbar), "statusbar");
    gtk_box_pack_start(GTK_BOX(root), statusbar, FALSE, FALSE, 0);

    st.status_name = make_label(kStarters[0].name, NULL, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(statusbar), st.status_name, FALSE, FALSE, 0);
    st.status_lines = make_label("0 lines", NULL, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(statusbar), st.status_lines, FALSE, FALSE, 0);
    st.status_saved = make_label("never saved", NULL, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(statusbar), st.status_saved, FALSE, FALSE, 0);
    GtkWidget *spacer = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(statusbar), spacer, TRUE, TRUE, 0);
    st.status_rain = gtk_button_new_with_label("rain: on");
    g_signal_connect(st.status_rain, "clicked", G_CALLBACK(on_rain_clicked), &st);
    gtk_box_pack_start(GTK_BOX(statusbar), st.status_rain, FALSE, FALSE, 0);

    rebuild_file_list(&st);
    set_text(st.editor_buffer, st.docs[0].source);
    refresh_status(&st);
    say_a_quote(&st);

    g_timeout_add(1000, status_tick, &st);

    if (st.rain_on) start_rain(&st);

    gtk_widget_show_all(st.window);
    gtk_main();
    return 0;
}
