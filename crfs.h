class CacheRemoteFileSystem : public FileSystem {
    public:
        metacache mcache;
        filecache fcache;
    public:
        RemoteFileSystem();
        ~RemoteFileSystem();
        int init_(std::string config_path) override;
        int getattr_(std::string path, struct stat& stbuf) override;
        int readdir_(std::string path, 
                std::vector<struct direntstat>& dirents) override;
        int open_(std::string path, int flags) override;
        int close_(int fd) override;
        int read_(int fd, char* buffer, int offest, int size) override;
        int write_(int fd, const char* buffer, 
                int offest, int size) override;
};
