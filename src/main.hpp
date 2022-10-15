#pragma once

#include <filesystem>

#include <fmt/core.h>
#include <sqlite3.h>

#include "ui_mainwindow.h"

#define MINIMUM_MUSIC_DB 82
#define log(msg, ...) output_message(fmt::format(msg __VA_OPT__(,) __VA_ARGS__))

class KMPC;

enum TargetSystem {
    TARGET_UNIX,
    TARGET_WINDOWS
};

enum Extension {
    PRESERVE_EXISTING,
    M3U,
    M3U8
};

struct File {
    std::string desc;
    std::string path;
    File(const std::string_view &desc, const std::string_view &path) : desc(desc), path(path) {}
    File(File &&f) : desc(std::move(f.desc)), path(std::move(f.path)) {}
};

struct Playlist {
    std::string name;
    std::string path;
    std::vector<File> files;
    KMPC *window;
    Playlist(const std::string &path, KMPC *window) : path(path), window(window) {}
    bool parse();
    bool write(const std::string &out_path);
};

class KMPC : public QMainWindow, private Ui::KMPC
{
    Q_OBJECT

public:
    explicit KMPC(QWidget *parent = nullptr);
    void closeEvent(QCloseEvent *event);
    void output_message(std::string &&message);

private slots:
    void on_convert_button_clicked();
    void on_browse_input_clicked();
    void on_browse_output_clicked();
    void on_browse_database_clicked();
    void on_path_replacement_enable_toggled(bool checked);

private:
    sqlite3 *db;
    void set_path_replacement_state(bool enabled);
    void set_convert_button_state();
    void export_playlists();
    int convert(const std::filesystem::path &path, TargetSystem target);
    void read_settings();
    void write_settings();
};