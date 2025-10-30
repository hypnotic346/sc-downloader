#pragma once
#include <shared_mutex>

namespace sc_mgr {
    inline std::string song_path = "";
    inline std::string client_id = "rErS7bNj8KEQnYeCoOf9zjxnhlTP89bL";
    inline std::shared_mutex progress_mutex;

    class downloader {
    public:
 
        bool save_to_file(const std::string &file_name);

        bool parse_audio(const std::string &raw_data, std::string &mp3_buffer);

        void get_preset(const std::string &preset_name);

        bool get_artwork_texture();

        bool download();

        bool get_track_info(const std::string &song_url);

        void set_progress(float p) {
            std::unique_lock lock(progress_mutex);
            progress = p;
        }

        float get_progress() {
            std::unique_lock lock(progress_mutex);
            return progress;
        }

        std::string title, artist_name, artwork_url;
        std::string song_m3u_link;
        float progress;
        ImVec2 artwork_size;
        unsigned int *artwork_tex;
        std::string artwork_location;
    private:

        // audio format.
        int transcodings_type;

    protected:
        nlohmann::json song_info_json_;
        std::string song_url_, audio_buffer_;
        bool loaded_tex_;
        size_t total_audio_size_;
        std::vector<std::pair<std::string, std::string>> transcodings_;
    };
} // namespace sc_mgr