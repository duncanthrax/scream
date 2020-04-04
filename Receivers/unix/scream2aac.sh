./scream -v -o raw | ffmpeg -f s16le -ar 48000 -ac 2 -i - -c:a aac -b:a 128k -y output.m4a
