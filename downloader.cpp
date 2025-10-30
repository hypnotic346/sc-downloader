#include <gl/GL.h>
#include <pch.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

bool load_texture(char *data, size_t data_size, GLuint *out_texture, int *out_width, int *out_height) {
    int image_width = 0;
    int image_height = 0;

    unsigned char *image_data = stbi_load_from_memory((const unsigned char *) data, (int) data_size, &image_width, &image_height, NULL, 4);

    if (image_data == NULL)
        return false;

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

bool load_texture_from_file(const char *file_name, GLuint *out_texture, int *out_width, int *out_height) {
    FILE *f = fopen(file_name, "rb");

    if (f == NULL)
        return false;

    fseek(f, 0, SEEK_END);

    size_t file_size = (size_t) ftell(f);

    if (file_size == -1)
        return false;

    fseek(f, 0, SEEK_SET);

    void *file_data = IM_ALLOC(file_size);

    fread(file_data, 1, file_size, f);
    fclose(f);

    bool ret = load_texture((char *) file_data, file_size, out_texture, out_width, out_height);
    IM_FREE(file_data);

    return ret;
}

bool sc_mgr::downloader::save_to_file(const std::string &file_name) {
    std::ofstream file(file_name, std::ios::binary | std::ios::out);

    if (this->audio_buffer_.empty())
        return false;

    file << this->audio_buffer_;

    file.close();

    return true;
}

bool sc_mgr::downloader::parse_audio(const std::string &raw_data, std::string &mp3_buffer) {
    std::istringstream iss(raw_data);
    std::string line;

    auto get_file_size = [](const std::string &url) -> int {
        cpr::Response res = cpr::Head(cpr::Url {url});

        if (res.status_code == 200) {
            if (res.header.count("Content-Length"))
                return std::stoi(res.header["Content-Length"]);
        }

        return -1;
    };

    std::deque<std::string> uris;

    spdlog::info("Determining download size...");

    while (std::getline(iss, line)) {
        if (line.starts_with("#EXTINF")) {
            std::getline(iss, line);

            this->total_audio_size_ += get_file_size(line);

            uris.emplace_back(line);
        }
    }

    if (!uris.empty()) {
        for (const auto &url : uris) {
            const cpr::Response res = cpr::Get(cpr::Url(url));

            if (res.status_code != 200) {
                spdlog::set_level(spdlog::level::err);
                spdlog::error("Error while downloading, reason: \"{}\"", res.reason);
                return false;
            }

            mp3_buffer += res.text;

            const size_t size_bytes = mp3_buffer.size();

            this->set_progress(static_cast<float>(size_bytes) / static_cast<float>(this->total_audio_size_));

#ifdef _DEBUG
            spdlog::set_level(spdlog::level::debug);
            spdlog::debug("Download progress: {}/{} ({:.2f}%)", size_bytes, this->total_audio_size_, this->get_progress());
#endif
        }
    }

    if (mp3_buffer.empty()) {
        spdlog::set_level(spdlog::level::err);
        spdlog::error("Failed to combine into audio file!");
        return false;
    }

    return true;
}

void sc_mgr::downloader::get_preset(const std::string &preset_name) {
    this->transcodings_.clear();

    for (const auto &transcoding : this->song_info_json_.at("media").at("transcodings"))
        this->transcodings_.emplace_back(std::make_pair(transcoding["preset"], transcoding["url"]));

    if (!this->transcodings_.empty()) {
        int it = 0;

        for (auto &[preset, url] : this->transcodings_) {
            if (preset == preset_name) {
                this->transcodings_type = it;
                this->song_m3u_link = url;
                break;
            }

            it++;
        }
    }
}

bool sc_mgr::downloader::get_artwork_texture() {
    int width, height;

    bool ret = false;

    if (!this->loaded_tex_) {
        ret = load_texture_from_file(this->artwork_location.c_str(), this->artwork_tex, &width, &height);
        this->loaded_tex_ = true;
    }

    return ret;
}

bool sc_mgr::downloader::download() {
    if (!this->song_m3u_link.empty())
        this->song_m3u_link += fmt::format("?client_id={}", client_id);

    cpr::Response res = cpr::Get(cpr::Url(this->song_m3u_link));

    if (res.status_code == 200) {
        nlohmann::json j = nlohmann::json::parse(res.text);

        std::string url_link = j["url"];

        res = cpr::Get(cpr::Url(url_link));

        if (res.status_code == 200) {
            std::filesystem::create_directory(this->artist_name);
            std::filesystem::create_directory(this->artist_name + "\\" + this->title);

            const std::string path = fmt::format("{}\\{}\\artwork.jpeg", this->artist_name, this->title);

            std::ofstream artwork_path(path, std::ios::binary | std::ios::out);

            {
                cpr::Response artwork_res = cpr::Download(artwork_path, cpr::Url(this->artwork_url));

                if (artwork_res.status_code == 200) {
                    this->artwork_location = path;
                    //get_artwork_texture();
                    artwork_path.close();
                }

                if (this->parse_audio(res.text, this->audio_buffer_)) {
                    std::string relative_path = fmt::format("{}\\{}\\{} - {}.mp3", this->artist_name, this->title, this->artist_name, this->title);
                    std::string track_path = song_path + relative_path;

                    this->save_to_file(track_path);
                }
            }
        }

        return !this->audio_buffer_.empty();
    }
}

bool sc_mgr::downloader::get_track_info(const std::string &song_url) {
    this->song_url_ = song_url;

    const std::string post_url = fmt::format("https://api-v2.soundcloud.com/resolve?url={}&client_id={}", song_url, client_id);

    cpr::Response res = cpr::Get(cpr::Url(post_url));

    if (res.status_code == 200) {
        this->song_info_json_ = nlohmann::json::parse(res.text);

        this->get_preset("mp3_1_0");

        if (!this->song_info_json_.empty()) {
            const bool has_artist_metadata = this->song_info_json_["publisher_metadata"].contains("artist");

            this->title = this->song_info_json_["title"];
            this->artist_name = has_artist_metadata ? this->song_info_json_["publisher_metadata"].at("artist") : this->song_info_json_["user"].at("username");

            std::string artwork_url = this->song_info_json_["artwork_url"];

            const size_t pos = artwork_url.find("-large");

            if (pos != std::string::npos)
                artwork_url.replace(pos, 6, "-original");

            this->artwork_url = artwork_url;

            return true;
        }
    }

    spdlog::set_level(spdlog::level::err);
    spdlog::error("Error downloading song \"{}\". Make sure your song URL is correct OR you provided the correct client ID.", song_url);

    return false;
}