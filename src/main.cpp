#include <string>
#include <string_view>
#include <filesystem>
#include <memory>
#include <cstdio>

#include <QApplication>
#include <QWidget>
#include <QFileDialog>
#include <QScrollBar>
#include <QSettings>
#include <QMessageBox>
#include <sqlite3.h>

#include "main.hpp"

extern "C" {
static int table_exists_callback(void *data, int argc, char **argv, char **column_name);
static int song_callback(void *data, int argc, char **argv, char **column_name);
}

static int table_exists_callback(void *data, int argc, char **argv, char **column_name)
{
    bool *table_exists = (bool*) data;
    *table_exists = true;
    return 0;
}

static int song_callback(void *data, int argc, char **argv, char **column_name)
{
    int *song_id = (int*) data;
    *song_id = atoi(argv[0]);
    return 0;
}

std::string str_replace(std::string_view sv, std::string_view in, std::string_view out)
{
    std::string str(sv);
    size_t pos = 0;
    while(pos < str.length() && (pos = str.find(in.data(), pos, in.length())) != std::string::npos) {
        str.replace(pos, in.length(), out.data(), out.length());
        pos += out.length();
    }
    return str;
}

KMPC::KMPC(QWidget *parent): QMainWindow(parent), db(nullptr)
{
    setupUi(this);
    convert_button->setEnabled(false);
    read_settings();
    set_convert_button_state();
}

void KMPC::closeEvent(QCloseEvent *event)
{
    write_settings();
    event->accept();
}

void KMPC::on_browse_input_clicked()
{
    QString current_dir = input_dir->text();
    QString dir = QFileDialog::getExistingDirectory(this,
                      "Select input directory",
                      current_dir
                  );
    if (!dir.isNull()) {
#ifdef _WIN32
        dir = QDir::toNativeSeparators(dir);
#endif
        input_dir->setText(dir);
    }
    set_convert_button_state();
}

void KMPC::on_browse_output_clicked()
{
    QString current_dir = output_dir->text();
    QString dir = QFileDialog::getExistingDirectory(this,
                      "Select output directory",
                      current_dir
                  );
    if (!dir.isNull()) {
#ifdef _WIN32
      dir = QDir::toNativeSeparators(dir);
#endif
        output_dir->setText(dir);
    }
    set_convert_button_state();
}

void KMPC::on_browse_database_clicked()
{
    QString dir;
    QString current_file = database_file->text();
    if (!current_file.isEmpty()) {
        std::filesystem::path path(current_file.toStdString());
        dir = QString::fromStdString(path.parent_path().string());
    }
    QString file = QFileDialog::getOpenFileName(this,
                       "Select the Kodi music database",
                       dir,
                       "Sqlite database (*.db)"
                   );
    if (!file.isNull()) {
#ifdef _WIN32
      file = QDir::toNativeSeparators(file);
#endif
        database_file->setText(file);
    }
    set_convert_button_state();
}

void KMPC::on_convert_button_clicked()
{
    export_playlists();
}

void KMPC::on_path_replacement_enable_toggled(bool checked)
{
    set_path_replacement_state(checked);
}

void KMPC::set_convert_button_state()
{
    if (input_dir->text().isEmpty() ||
    output_dir->text().isEmpty() ||
    database_file->text().isEmpty())
        convert_button->setEnabled(false);
    else
        convert_button->setEnabled(true);
}

void KMPC::set_path_replacement_state(bool enabled)
{
    path_replacement_input->setReadOnly(!enabled);
    path_replacement_output->setReadOnly(!enabled);
    path_replacement_input->setStyleSheet("QLineEdit[readOnly=\"true\"] {color: #808080; background-color: #F0F0F0;}");
    path_replacement_output->setStyleSheet("QLineEdit[readOnly=\"true\"] {color: #808080; background-color: #F0F0F0;}");
}

void KMPC::export_playlists()
{
    output_box->clear();
    std::string input_dir = this->input_dir->text().toStdString();
    std::string output_dir = this->output_dir->text().toStdString();
    std::string database_file = this->database_file->text().toStdString();

    // Make sure the database is correct
    size_t music = database_file.rfind("MyMusic");
    if (music == std::string::npos) {
        log("Invalid database file");
        return;
    }
    size_t dot = database_file.rfind(".");
    int db_version = atoi(database_file.substr(music + 7, dot - (music + 7)).c_str());
    if (db_version < MINIMUM_MUSIC_DB) {
        log("Database version too old ({} or newer is required)", MINIMUM_MUSIC_DB);
        return;
    }

    // Generate list of playlists in input directory
    std::vector<std::filesystem::path> playlists;
    for (const auto &entry : std::filesystem::directory_iterator(input_dir)) {
        const std::string &ext = entry.path().extension().string();
        if (ext == ".m3u" || ext == ".m3u8")
            playlists.push_back(entry.path());
    }
    if (!playlists.size()) {
        log("No playlists were detected in input directory '{}'", input_dir);
        return;
    }
    size_t nb_playlists = playlists.size();
    log("Found {} playlist{}", nb_playlists, nb_playlists > 1 ? "s" : "");

    // Open and validate sqlite db
    int rc = sqlite3_open(database_file.c_str(), &db);
    if (rc) {
        log("Failed to open sqlite database: {}", sqlite3_errmsg(db));
        return;
    }
    std::unique_ptr<sqlite3, int (*)(sqlite3*)> db_wrapper(db, sqlite3_close);
    bool table_exists = false;
    rc = sqlite3_exec(db, 
             "SELECT name FROM sqlite_master WHERE type='view' AND name='songview';",
             table_exists_callback,
             (void*) &table_exists,
             NULL
         );
    if (rc != SQLITE_OK || !table_exists) {
        log("Failed to get songs from sqlite database (Sqlite returncode {})", rc);
        return;
    }

    // Convert playlists
    int errors = 0;
    TargetSystem target = (TargetSystem) target_system->currentIndex();
    for (auto &playlist : playlists)
        errors += convert(playlist, target);
    if (!errors)
        log("All playlists were successfully converted");
    else {
        log("{} playlist{} failed to convert", errors, errors > 1 ? "s" : "");
        QMessageBox::warning(this,
            "Error", 
            "One or more playlists failed to convert. Check the log for more information"
        );
    }
}

int KMPC::convert(const std::filesystem::path &path, TargetSystem target)
{
    // Parse playlist
    std::filesystem::path output_path = output_dir->text().toStdString() / path.filename();
    Extension extension = (Extension) file_extension->currentIndex();
    if (extension != PRESERVE_EXISTING) 
        output_path.replace_extension(extension == M3U ? ".m3u" : ".m3u8");

    Playlist playlist(path.string(), this);
    if (!playlist.parse())
        return 1;

    // Replace paths in files
    if (path_replacement_enable->isChecked()) {
        std::string in = path_replacement_input->text().toStdString();
        std::string out = path_replacement_output->text().toStdString();
        for (File &file : playlist.files)
            file.path = str_replace(file.path, in, out);
    }

    // Convert paths to database format
    bool error = false;
    for (File &file : playlist.files) {
        int song_id = -1;
        std::filesystem::path file_path(file.path);
        std::filesystem::path dir = file_path.parent_path();
        std::string dir_string = dir.string();
        const char *path_separator;
#ifdef _WIN32
        path_separator = (target == TARGET_UNIX) ? "/" : "\\";
        if (target == TARGET_UNIX)
            dir_string = str_replace(dir_string, "\\", "/");
#else
        path_separator = (target == TARGET_WINDOWS) ? "\\" : "/";
        if (target == TARGET_WINDOWS)
            dir_string = str_replace(dir_string, "/", "\\");
#endif
        if (dir_string.back() != *path_separator);
            dir_string += path_separator;

        std::filesystem::path filename = file_path.filename();
        std::string query = fmt::format("SELECT idSong FROM songview WHERE strPath='{}' AND strFileName='{}';",
                                str_replace(dir_string, "'", "''"),
                                str_replace(filename.string(), "'", "''")
                            );
        char *sqlite_error = NULL;
        int rc = sqlite3_exec(db, 
                    query.c_str(),
                    song_callback,
                    (void*) &song_id,
                    &sqlite_error
                 );
        if (rc != SQLITE_OK || song_id == -1) {
            log("Failed to find song '{}' in database (Sqlite returncode: {})", file.path, rc);
            if (sqlite_error != NULL) {
                log("Sqlite Error: {}", sqlite_error);
                log ("Sqlite Query: {}", query);
                sqlite3_free(sqlite_error);
            }
            error = true;
            break;
        }
        file.path = fmt::format("musicdb://songs/{}{}", song_id, filename.extension().string());
    }

    // Write to file
    if (!error)
        error = !playlist.write(output_path.string());
    if (error)
        log("Failed to convert playlist '{}'", path.filename().string());
    else
        log("Successfully converted playlist '{}'", path.filename().string());
        
    return error ? 1 : 0;
}

bool Playlist::parse()
{
    std::unique_ptr<FILE, int (*)(FILE*)> file(fopen(path.c_str(), "r"), fclose);
    FILE *fp = file.get();
    if (fp == nullptr) {
        window->log("Failed to open playlist '{}'", path);
        return false;
    }

    // Read file into memory
    fseek(fp, 0L, SEEK_END);
    size_t size = ftell(fp);
    rewind(fp);
    std::vector<char> buffer(size);
    size_t read = fread(buffer.data(), 1, size, fp);
    if (read != size) {
        window->log("Failed to read file '{}' into memory", path);
        return false;
    }
    std::string_view buf(buffer.data(), size);
    std::vector<std::string_view> lines;
    char *p = buffer.data();
    size_t cur = 0;
    size_t end;

    // Split file into lines
    while((end = buf.find("\n", cur)) != std::string::npos) {
        lines.push_back(std::string_view(p + cur, end - cur));
        cur = end + 1;
    }
    if (cur < size)
        lines.push_back(std::string_view(p + cur, size - cur));

    if (!lines.size()) {
        window->log("Failed to parse file '{}'", path);
        return false;
    }
    if (!lines[0].starts_with("#EXTM3U")) {
        window->log("File '{}' is not a valid M3U playlist", path);
        return false;
    }

    // Parse
    std::string_view *desc = nullptr;
    for (int i = 1; i < lines.size(); i++) {
        if (lines[i].starts_with('#')) {
            if (lines[i].starts_with("#EXTINF:"))
                desc = &lines[i];
        } 
        else {
            if (!lines[i].empty())
                files.push_back(File(desc != nullptr ? *desc : std::string_view(), lines[i]));
            desc = nullptr;
        }
    }
    return true;
}

bool Playlist::write(const std::string &out_path)
{
    std::unique_ptr<FILE, int (*)(FILE*)> file(fopen(out_path.c_str(), "wb"), fclose);
    FILE *fp = file.get();
    if (fp == nullptr) {
        window->log("Failed to open output file '{}'", out_path);
        return false;
    }
    fmt::print(fp, "#EXTM3U\n");
    for (const File &file : files) {
        if (!file.desc.empty())
            fmt::print(fp, "{}\n", file.desc);
        fmt::print(fp, "{}\n", file.path);
    }
    return true;
}

void KMPC::output_message(std::string &&message)
{
    output_box->appendPlainText(QString::fromStdString(message));
    output_box->verticalScrollBar()->setValue(output_box->verticalScrollBar()->maximum());
}

void KMPC::read_settings()
{
    QSettings settings("complexlogic", "MKPC");
    QString input_dir = settings.value("input_dir").toString();
    if (!input_dir.isEmpty()) {
        std::filesystem::path path(input_dir.toStdString());
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
            this->input_dir->setText(input_dir);
    }
    QString output_dir = settings.value("output_dir").toString();
    if (!output_dir.isEmpty()) {
        std::filesystem::path path(output_dir.toStdString());
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
            this->output_dir->setText(output_dir);
    }
    QString database_file = settings.value("database_file").toString();
    if (!database_file.isEmpty()) {
        std::filesystem::path path(database_file.toStdString());
        if (std::filesystem::exists(path))
            this->database_file->setText(database_file);
    }
    target_system->setCurrentIndex(settings.value("target_system").toInt());
    file_extension->setCurrentIndex(settings.value("file_extension").toInt());
    path_replacement_enable->setChecked(settings.value("path_replace").toBool());
    path_replacement_input->setText(settings.value("path_replacement_input").toString());
    path_replacement_output->setText(settings.value("path_replacement_output").toString());

    if (!path_replacement_enable->isChecked())
        set_path_replacement_state(false);
}

void KMPC::write_settings()
{
    QSettings settings("complexlogic", "MKPC");
    settings.setValue("input_dir", input_dir->text());
    settings.setValue("output_dir", output_dir->text());
    settings.setValue("database_file", database_file->text());
    settings.setValue("target_system", target_system->currentIndex());
    settings.setValue("file_extension", file_extension->currentIndex());
    settings.setValue("path_replace", path_replacement_enable->isChecked());
    settings.setValue("path_replacement_input", path_replacement_input->text());
    settings.setValue("path_replacement_output", path_replacement_output->text());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KMPC window;
    window.show();
    return app.exec();
}