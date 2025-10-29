#pragma once

namespace sc_mgr {
    inline const char *client_id = "rErS7bNj8KEQnYeCoOf9zjxnhlTP89bL";

    class downloader {
    public:
        bool save_to_file(const std::string &file_name);

        bool parse_audio(const std::string &raw_data, std::string &mp3_buffer);

        bool perform(const std::string &song_url);

        std::string title_name, artist_name, artwork_url;
    protected:
        std::string song_url_, audio_buffer_;
    };
} // namespace sc_mgr