import base64
import re
import argparse
from pathlib import Path

# Define the mapping of stage types to file extensions
STAGE_EXTENSIONS = {
    "vertex": ".vert.spv",
    "tessellationControl": ".tesc.spv",
    "tessellationEvaluation": ".tese.spv",
    "geometry": ".geom.spv",
    "fragment": ".frag.spv",
    "compute": ".comp.spv",
}

def encode_file_to_base64(file_path):
    """Reads a file and encodes its content in base64."""
    with open(file_path, 'rb') as f:
        return base64.b64encode(f.read()).decode('utf-8')

def replace_stage_files_with_base64(input_file, shader_path=None):
    """Replaces addStage filenames with base64-encoded content derived from the pipeline filename."""
    input_path = Path(input_file)
    if not input_path.exists():
        print(f"Error: File '{input_file}' does not exist.")
        return

    # Determine the shader directory
    shader_directory = Path(shader_path) if shader_path else input_path.parent

    # Read the content of the input file
    with open(input_file, 'r') as f:
        content = f.read()

    # Pattern to match addStage statements
    stage_pattern = re.compile(r'addStage\("([^"]*)",\s*(\w+)\)')

    def replacer(match):
        # Extract the stage type
        stage_type = match.group(2)

        # Get the file extension based on the stage type
        extension = STAGE_EXTENSIONS.get(stage_type)
        if not extension:
            print(f"Warning: Unrecognized stage type '{stage_type}'. Skipping.")
            return match.group(0)

        # Construct the new file path using the shader directory and pipeline filename
        stage_file = shader_directory / f"{input_path.stem}{extension}"

        try:
            # Encode the file content to base64
            encoded_content = encode_file_to_base64(stage_file)
            return f'addStage(base64"{encoded_content}", {stage_type})'
        except FileNotFoundError:
            print(f"Warning: File '{stage_file}' not found. Skipping.")
            return match.group(0)

    # Replace all addStage matches
    updated_content = stage_pattern.sub(replacer, content)

    # Write the updated content to the original file
    with open(input_file, 'w') as f:
        f.write(updated_content)

def main():
    parser = argparse.ArgumentParser(description="Process a pipeline file and replace addStage filenames with base64 content.")
    parser.add_argument("file", help="Path to the pipeline file to process")
    parser.add_argument("--shader-path", help="Path to the directory containing shader binaries", default=None)

    args = parser.parse_args()
    replace_stage_files_with_base64(args.file, args.shader_path)

if __name__ == "__main__":
    main()
