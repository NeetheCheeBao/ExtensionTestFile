import os

# 主目录路径
main_directory = os.path.join(os.getcwd(), "Test")

# 定义各类别对应的扩展名列表
video_extensions = [
    "avi", "wmv", "wmp", "wm", "asf", "mpg", "mpeg", "mpe", "m1v", "m2v",
    "mpv2", "mp2v", "ts", "tp", "tpr", "trp", "vob", "ifo", "ogm", "ogv",
    "mp4", "m4v", "m4p", "m4b", "3gp", "3gpp", "3g2", "3gp2", "mkv", "rm",
    "ram", "rmvb", "rpm", "flv", "mov", "qt", "nsv", "dpg", "m2ts", "m2t",
    "mts", "dvr-ms", "k3g", "skm", "evo", "nsr", "amv", "divx", "webm",
    "wtv", "f4v", "mxf"
]

audio_extensions = [
    "wav", "wma", "mpa", "mp2", "m1a", "m2a", "mp3", "ogg", "m4a", "aac",
    "mka", "ra", "flac", "ape", "mpc", "mod", "ac3", "eac3", "dts", "dtshd",
    "wv", "tak", "cda", "dsf", "tta", "aiff", "aif", "opus", "amr"
]

playlist_extensions = [
    "asx", "m3u", "m3u8", "pls", "wvx", "wax", "wmx", "cue", "mpls", "mpl",
    "dpl", "xspf", "mpd"
]

# 分类信息：文件夹名称与对应扩展名列表的映射
categories = {
    "视频": video_extensions,
    "音频": audio_extensions,
    "播放列表": playlist_extensions
}

def create_files():
    # 创建主目录（如果不存在）
    os.makedirs(main_directory, exist_ok=True)
    
    # 遍历每个分类
    for category_name, exts in categories.items():
        # 创建分类文件夹
        category_path = os.path.join(main_directory, category_name)
        os.makedirs(category_path, exist_ok=True)
        
        # 为每个扩展名创建文件
        for ext in exts:
            # 构建文件名：FILE + 扩展名
            file_name = f"FILE.{ext}"
            file_path = os.path.join(category_path, file_name)
            
            # 写入文件内容"EEEEE"，使用UTF-8编码
            with open(file_path, "w", encoding="utf-8") as f:
                f.write("EEEEE")
        
        print(f"已完成 [{category_name}] 分类文件创建，共 {len(exts)} 个文件")

if __name__ == "__main__":
    create_files()
    print("所有文件创建完成！")