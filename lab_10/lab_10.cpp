#include <iostream>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>
#include <stdlib.h>

using namespace std;

const string DIRECTORY = "randfiles";
const int NFILES = 3;
string file_names[NFILES];
int file_handlers[NFILES];

// generating random string with 16 chars in lowercase
string gen_name() {
    string rand_str;
    
    for (int i = 0; i < 16; i++) {
        int sym = rand() % 62;

        if (sym < 10) {
            rand_str += sym + 0x30; // 0x30 - 0, 0x39 - 9 in ASCII
            continue;
        } sym -= 10;

        if (sym < 26) {
            rand_str += sym + 0x41; // 0x41 - A, 0x5A - Z in ASCII
            continue;
        } sym -= 26;
        
        if (sym < 26) {
            rand_str += sym + 0x61; // 0x61 - a, 0x7A - z in ASCII
        }
    }

    return rand_str;
}

// Check if directory exists. If not exists create
int prepare_directory(string _directory) {
    struct stat eninfo;
    
    if (stat(_directory.c_str(), &eninfo) != 0) {
        if (errno != ENOENT) {
            cout << "[Error] Something went wrong when get stat " << _directory << " directory!" << endl;
            return errno; 
        }

        cout << "[Info] Directory " << _directory << " not exists. Creating new one!" << endl;
        if (mkdir(_directory.c_str(), 0700) != 0) {
            cout << "[Error] Something went wrong while creating " << _directory << " directory!" << endl;
            return errno;
        }

        return 0;
    }

    if ((eninfo.st_mode & S_IFMT) != S_IFDIR) {
        cout << "[Error] Object with name " << _directory << " already exists and it not directory!" << endl;
        return -1;
    }

    return 0;
}

// Int to rights rwxrwxrwx (include SUID, SGID and Sticky Bit)
string itor(int _rights) {
    string rights = "";

    rights += ((_rights >> 6) & 4) ? "r" : "-";
    rights += ((_rights >> 6) & 2) ? "w" : "-";

    if ((_rights >> 9) & 4) {
        if ((_rights >> 6) & 1) {
            rights += "s";
        } else {
            rights += "S";
        }
    } else {
        if ((_rights >> 6) & 1) {
            rights += "x";
        } else {
            rights += "-";
        } 
    }

    rights += ((_rights >> 3) & 4) ? "r" : "-";
    rights += ((_rights >> 3) & 2) ? "w" : "-";

    if ((_rights >> 9) & 2) {
        if ((_rights >> 3) & 1) {
            rights += "s";
        } else {
            rights += "S";
        }
    } else {
        if ((_rights >> 3) & 1) {
            rights += "x";
        } else {
            rights += "-";
        } 
    }

    rights += (_rights & 4) ? "r" : "-";
    rights += (_rights & 2) ? "w" : "-";

    if ((_rights >> 9) & 1) {
        if (_rights & 1) {
            rights += "t";
        } else {
            rights += "T";
        }
    } else {
        if (_rights & 1) {
            rights += "x";
        } else {
            rights += "-";
        } 
    }

    return rights;
}

// Print info about file inode
void print_open_handlers_table(int num) {
    cout << "\n||| HANDLERS TABLE #" << num << "|||" << endl;
    string entry;
    struct stat entry_info;
    char formated_time[200];
    char nano_seconds_s[50];
    for (int i = 0; i < NFILES; i++) {
        if (file_handlers[i] == 0) {continue;}
        
        entry = "==> " + file_names[i] + "\n";
        fstat(file_handlers[i], &entry_info);
        entry += "|- Inode:\t\t\t" + to_string(entry_info.st_ino) + "\n";
        entry += "|- Mode:\t\t\t" + itor(entry_info.st_mode & 07777) + "\n";
        entry += "|- UID:\t\t\t\t" + to_string(entry_info.st_uid) + "\n";
        entry += "|- GID:\t\t\t\t" + to_string(entry_info.st_gid) + "\n";
        entry += "|- Size:\t\t\t" + to_string(entry_info.st_size) + " B\n";
        
        snprintf(nano_seconds_s, sizeof(nano_seconds_s), "%09ld", entry_info.st_atim.tv_nsec);
        string atim = "%b %a %d %T." + string(nano_seconds_s) + " %Z %Y";
        strftime(formated_time, sizeof(formated_time), atim.c_str(), gmtime(&entry_info.st_atim.tv_sec));
        atim = string(formated_time);
        entry += "|- Last access time:\t\t" + atim + "\n";

        snprintf(nano_seconds_s, sizeof(nano_seconds_s), "%09ld", entry_info.st_mtim.tv_nsec);
        atim = "%b %a %d %T." + string(nano_seconds_s) + " %Z %Y";
        strftime(formated_time, sizeof(formated_time), atim.c_str(), gmtime(&entry_info.st_mtim.tv_sec));
        atim = string(formated_time);
        entry += "|- Last modification time:\t" + atim + "\n";

        snprintf(nano_seconds_s, sizeof(nano_seconds_s), "%09ld", entry_info.st_ctim.tv_nsec);
        atim = "%b %a %d %T." + string(nano_seconds_s) + " %Z %Y";
        strftime(formated_time, sizeof(formated_time), atim.c_str(), gmtime(&entry_info.st_ctim.tv_sec));
        atim = string(formated_time);
        entry += "|- Last status change time:\t" + atim + "\n";

        cout << entry << endl;
    }
}

int main() {
    // Create if not exists work directory
    int err = prepare_directory(DIRECTORY);
    if (err != 0) {
        return err;
    }

    // Clear file handlers array for correct build table
    for (int i = 0; i < NFILES; i++) {
        file_handlers[i] = 0;
    }

    print_open_handlers_table(0);

    // Create/Open files
    for (int i = 0; i < NFILES; i++) {
        file_names[i] = gen_name(); // Generate random 16 bytes name
        string file_path = DIRECTORY + "/" + file_names[i];
        file_handlers[i] = open(file_path.c_str(), O_RDWR | O_CREAT, 0600);
        cout << "[Info] File #" << i << " with name " << file_names[i] << endl;
        print_open_handlers_table(i+1);
    }

    
    ftruncate(file_handlers[2], 0); // Set third file size to 0
    lseek(file_handlers[2], 0, SEEK_SET); // Set offset to 0
    print_open_handlers_table(4);

    // Copy third file to second
    char buf[1024];
    int bytes_read;
    do {
        bytes_read = read(file_handlers[1], &buf, sizeof(buf));
        if (bytes_read > 0) {write(file_handlers[2], &buf, bytes_read);}
    } while (bytes_read > 0);
    print_open_handlers_table(5);

    // Close all files
    for (int i = 0; i < NFILES; i++) {
        close(file_handlers[i]);
        file_handlers[i] = 0;
        print_open_handlers_table(i+6);
    }
 
    return 0;
}