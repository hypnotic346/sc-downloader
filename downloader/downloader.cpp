#include <pch.hpp>

bool sc_mgr::downloader::save_to_file(const std::string &file_name) {
    std::ofstream file(file_name, std::ios::app | std::ios::in | std::ios::out);

    file << this->audio_buffer_;

    file.close();

    return true;
}

bool sc_mgr::downloader::parse_audio(const std::string &raw_data, std::string &mp3_buffer) {
    std::istringstream iss(raw_data);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.starts_with("#EXTINF")) {
            std::getline(iss, line);

            const cpr::Response res = cpr::Get(cpr::Url(line));

            if (res.status_code != 200) {
                spdlog::set_level(spdlog::level::err);
                spdlog::error("Error while downloading: {}", res.reason);
                return false;
            }

            mp3_buffer += res.text;
        }
    }

    if (mp3_buffer.empty()) {
        spdlog::set_level(spdlog::level::err);
        spdlog::error("Failed to combine into mp3!");
        return false;
    }

    return true;
}

bool sc_mgr::downloader::perform(const std::string &song_url) {
    this->song_url_ = song_url;

    std::string post_url = fmt::format("https://api-v2.soundcloud.com/resolve?url={}&client_id={}", song_url, client_id);

    cpr::Response res = cpr::Get(cpr::Url(post_url));

    nlohmann::json j;

    if (res.status_code == 200) {
        j = nlohmann::json::parse(res.text);

        if (!j.empty()) {
            const bool has_artist_metadata = j["publisher_metadata"].contains("artist");

            this->title_name = j["title"];       
            this->artist_name = has_artist_metadata ? j["publisher_metadata"].at("artist") : j["user"].at("username");
            this->artwork_url = j["artwork_url"];

#ifdef _DEBUG
            spdlog::info("Track info: {}, {}, {}", this->title_name, this->artist_name, this->artwork_url);
#endif

            std::string song_m3u_link = "";

            for (const auto &transcoding : j.at("media").at("transcodings")) {
                if (transcoding.contains("preset") && transcoding["preset"] == "mp3_1_0") {
                    song_m3u_link = transcoding["url"];
                    break;
                }
            }

            if (!song_m3u_link.empty())
                song_m3u_link += fmt::format("?client_id={}", client_id);

            res = cpr::Get(cpr::Url(song_m3u_link));

            if (res.status_code == 200) {
                j = nlohmann::json::parse(res.text);

                std::string url_link = j["url"];

                res = cpr::Get(cpr::Url(url_link));

                if (res.status_code == 200) {
                    if (this->parse_audio(res.text, this->audio_buffer_))
                        this->save_to_file("test2.mp3");
                }

                return !this->audio_buffer_.empty();
            }
        }
    }

    spdlog::set_level(spdlog::level::err);
    spdlog::error("Error in downloading song \"{}\"", song_url);

    return false;
}