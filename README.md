# SquashFS Backup Script

This repository contains a script to create a SquashFS backup of a disk, ensuring data integrity through checksum verification.
```diskwriter``` is a good way of writing to disk. it check a block is if it is same or not with data to be written.

## Overview

The script performs the following tasks:

1. Reads a disk and calculates its SHA256 checksum.
2. Uses the output of `dd` to compute the SHA256 checksum without reading the disk again.
3. Creates a SquashFS image of the disk using `mksquashfs`.
4. Verifies the SHA256 checksum of both the original disk and the SquashFS image to ensure data integrity.

## Usage

### Prerequisites

- Clone this repository.
- Install dependencies from `requirements.txt`.
- Ensure you have sufficient permissions to read the disk (`/dev/sda` in this example).

### Instructions

1. Create an empty directory for the backup.
2. Navigate to the directory:
   `cd /path/to/empty_directory`
3. Run the script `squash_fs_bck` with the following options:<br>
   `../PathToSquashBackup/SquashBackup/squash_fs_bck -d /dev/sda -n NameOftheBackup -a`

- `-d`: Specify the name of the disk you want to backup (e.g., `/dev/sda`).
- `-n`: Provide a name for the backup.
- `-a` (optional): Append the current timestamp to the backup name automatically.

### Example

To backup `/dev/sda` with the name "MyBackup" and append a timestamp:
`./PathToSquashBackup/SquashBackup/squash_fs_bck -d /dev/sda -n MyBackup -a`


## Contributing

Feel free to contribute to this project by forking the repository and submitting pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- This script is inspired by [this Unix Stack Exchange answer](https://unix.stackexchange.com/a/75590).
- Thanks to the open source community for their contributions.


