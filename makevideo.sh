ffmpeg -i output.mp4 \
       -i output.wav \
       -c:v copy \
       -c:a aac \
       -map 0:v:0 \
       -map 1:a:0 \
       final.mp4
