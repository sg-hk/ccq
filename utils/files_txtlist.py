import os
from pathlib import Path

source_directory = Path.home() / '.local/share/ccq/media'

output_file = Path.home() / '.local/share/ccq/audiolist'

opus_files = [f for f in source_directory.glob('*.opus')]

with output_file.open('w') as file:
    for opus_file in opus_files:
        file.write(f"{opus_file.name}\n")

print(f"List of OPUS files written to {output_file}")
