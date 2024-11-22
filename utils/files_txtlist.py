import os
from pathlib import Path

source_directory = Path.home() / '.local/share/ccq/media'

output_file = Path.home() / '.local/share/ccq/audiolist'

mp3_files = [f for f in source_directory.glob('*.mp3')]

with output_file.open('w') as file:
    for mp3_file in mp3_files:
        file.write(f"{mp3_file.name}\n")

print(f"List of MP3 files written to {output_file}")
