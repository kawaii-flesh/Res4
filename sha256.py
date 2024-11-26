import hashlib
import os
import sys

BLOCK_SIZE = 10 * 1024 * 1024

def calculate_sha256_for_block(data):
    sha256 = hashlib.sha256()
    sha256.update(data)
    return sha256.hexdigest()

def process_file(file_path):
    results = []
    with open(file_path, 'rb') as f:
        offset = 0
        while True:
            data = f.read(BLOCK_SIZE)
            if not data:
                break
            sha256_hash = calculate_sha256_for_block(data)
            results.append(f"{sha256_hash}|{offset}|{file_path}")
            offset += len(data)
    
    return results

def main():
    file_paths = sys.argv[1:]
    if not file_paths:
        print("File paths are required.")
        sys.exit(1)

    all_results = []

    for file_path in file_paths:
        if os.path.isfile(file_path):
            file_results = process_file(file_path)
            all_results.extend(file_results)
        else:
            print(f"Warning: {file_path} is not a file or does not exist.")

    with open("sha256.list", "w") as output_file:
        for line in all_results:
            output_file.write(line + "\n")

    print("sha256.list file successfully created.")

if __name__ == "__main__":
    main()
